/*
  Include "petsctao.h" so we can use TAO solvers
  Include "petscdmda.h" so that we can use distributed arrays (DMs) for managing
  Include "petscksp.h" so we can set KSP type
  the parallel mesh.
*/

#include <petsctao.h>
#include <petscdmda.h>

static char help[] = "This example demonstrates use of the TAO package to \n\
solve a bound constrained minimization problem.  This example is based on \n\
the problem DPJB from the MINPACK-2 test suite.  This pressure journal \n\
bearing problem is an example of elliptic variational problem defined over \n\
a two dimensional rectangle.  By discretizing the domain into triangular \n\
elements, the pressure surrounding the journal bearing is defined as the \n\
minimum of a quadratic function whose variables are bounded below by zero.\n\
The command line options are:\n\
  -mx <xg>, where <xg> = number of grid points in the 1st coordinate direction\n\
  -my <yg>, where <yg> = number of grid points in the 2nd coordinate direction\n\
 \n";

/*
   User-defined application context - contains data needed by the
   application-provided call-back routines, FormFunctionGradient(),
   FormHessian().
*/
typedef struct {
  /* problem parameters */
  PetscReal ecc;    /* test problem parameter */
  PetscReal b;      /* A dimension of journal bearing */
  PetscInt  nx, ny; /* discretization in x, y directions */

  /* Working space */
  DM  dm; /* distributed array data structure */
  Mat A;  /* Quadratic Objective term */
  Vec B;  /* Linear Objective term */
} AppCtx;

/* User-defined routines */
static PetscReal      p(PetscReal xi, PetscReal ecc);
static PetscErrorCode FormFunctionGradient(Tao, Vec, PetscReal *, Vec, void *);
static PetscErrorCode FormHessian(Tao, Vec, Mat, Mat, void *);
static PetscErrorCode ComputeB(AppCtx *);
static PetscErrorCode Monitor(Tao, void *);
static PetscErrorCode ConvergenceTest(Tao, void *);

int main(int argc, char **argv)
{
  PetscInt  Nx, Ny; /* number of processors in x- and y- directions */
  PetscInt  m;      /* number of local elements in vectors */
  Vec       x;      /* variables vector */
  Vec       xl, xu; /* bounds vectors */
  PetscReal d1000 = 1000;
  PetscBool flg, testgetdiag; /* A return variable when checking for user options */
  Tao       tao;              /* Tao solver context */
  KSP       ksp;
  AppCtx    user;       /* user-defined work context */
  PetscReal zero = 0.0; /* lower bound on all variables */

  /* Initialize PETSC and TAO */
  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));

  /* Set the default values for the problem parameters */
  user.nx     = 50;
  user.ny     = 50;
  user.ecc    = 0.1;
  user.b      = 10.0;
  testgetdiag = PETSC_FALSE;

  /* Check for any command line arguments that override defaults */
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-mx", &user.nx, &flg));
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-my", &user.ny, &flg));
  PetscCall(PetscOptionsGetReal(NULL, NULL, "-ecc", &user.ecc, &flg));
  PetscCall(PetscOptionsGetReal(NULL, NULL, "-b", &user.b, &flg));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-test_getdiagonal", &testgetdiag, NULL));

  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "\n---- Journal Bearing Problem SHB-----\n"));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "mx: %" PetscInt_FMT ",  my: %" PetscInt_FMT ",  ecc: %g \n\n", user.nx, user.ny, (double)user.ecc));

  /* Let Petsc determine the grid division */
  Nx = PETSC_DECIDE;
  Ny = PETSC_DECIDE;

  /*
     A two dimensional distributed array will help define this problem,
     which derives from an elliptic PDE on two dimensional domain.  From
     the distributed array, Create the vectors.
  */
  PetscCall(DMDACreate2d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, DMDA_STENCIL_STAR, user.nx, user.ny, Nx, Ny, 1, 1, NULL, NULL, &user.dm));
  PetscCall(DMSetFromOptions(user.dm));
  PetscCall(DMSetUp(user.dm));

  /*
     Extract global and local vectors from DM; the vector user.B is
     used solely as work space for the evaluation of the function,
     gradient, and Hessian.  Duplicate for remaining vectors that are
     the same types.
  */
  PetscCall(DMCreateGlobalVector(user.dm, &x)); /* Solution */
  PetscCall(VecDuplicate(x, &user.B));          /* Linear objective */

  /*  Create matrix user.A to store quadratic, Create a local ordering scheme. */
  PetscCall(VecGetLocalSize(x, &m));
  PetscCall(DMCreateMatrix(user.dm, &user.A));

  if (testgetdiag) PetscCall(MatSetOperation(user.A, MATOP_GET_DIAGONAL, NULL));

  /* User defined function -- compute linear term of quadratic */
  PetscCall(ComputeB(&user));

  /* The TAO code begins here */

  /*
     Create the optimization solver
     Suitable methods: TAOGPCG, TAOBQPIP, TAOTRON, TAOBLMVM
  */
  PetscCall(TaoCreate(PETSC_COMM_WORLD, &tao));
  PetscCall(TaoSetType(tao, TAOBLMVM));

  /* Set the initial vector */
  PetscCall(VecSet(x, zero));
  PetscCall(TaoSetSolution(tao, x));

  /* Set the user function, gradient, hessian evaluation routines and data structures */
  PetscCall(TaoSetObjectiveAndGradient(tao, NULL, FormFunctionGradient, (void *)&user));

  PetscCall(TaoSetHessian(tao, user.A, user.A, FormHessian, (void *)&user));

  /* Set a routine that defines the bounds */
  PetscCall(VecDuplicate(x, &xl));
  PetscCall(VecDuplicate(x, &xu));
  PetscCall(VecSet(xl, zero));
  PetscCall(VecSet(xu, d1000));
  PetscCall(TaoSetVariableBounds(tao, xl, xu));

  PetscCall(TaoGetKSP(tao, &ksp));
  if (ksp) PetscCall(KSPSetType(ksp, KSPCG));

  PetscCall(PetscOptionsHasName(NULL, NULL, "-testmonitor", &flg));
  if (flg) PetscCall(TaoSetMonitor(tao, Monitor, &user, NULL));
  PetscCall(PetscOptionsHasName(NULL, NULL, "-testconvergence", &flg));
  if (flg) PetscCall(TaoSetConvergenceTest(tao, ConvergenceTest, &user));

  /* Check for any tao command line options */
  PetscCall(TaoSetFromOptions(tao));

  /* Solve the bound constrained problem */
  PetscCall(TaoSolve(tao));

  /* Free PETSc data structures */
  PetscCall(VecDestroy(&x));
  PetscCall(VecDestroy(&xl));
  PetscCall(VecDestroy(&xu));
  PetscCall(MatDestroy(&user.A));
  PetscCall(VecDestroy(&user.B));

  /* Free TAO data structures */
  PetscCall(TaoDestroy(&tao));
  PetscCall(DMDestroy(&user.dm));
  PetscCall(PetscFinalize());
  return 0;
}

static PetscReal p(PetscReal xi, PetscReal ecc)
{
  PetscReal t = 1.0 + ecc * PetscCosScalar(xi);
  return (t * t * t);
}

PetscErrorCode ComputeB(AppCtx *user)
{
  PetscInt  i, j, k;
  PetscInt  nx, ny, xs, xm, gxs, gxm, ys, ym, gys, gym;
  PetscReal two = 2.0, pi = 4.0 * atan(1.0);
  PetscReal hx, hy, ehxhy;
  PetscReal temp, *b;
  PetscReal ecc = user->ecc;

  PetscFunctionBegin;
  nx    = user->nx;
  ny    = user->ny;
  hx    = two * pi / (nx + 1.0);
  hy    = two * user->b / (ny + 1.0);
  ehxhy = ecc * hx * hy;

  /*
     Get local grid boundaries
  */
  PetscCall(DMDAGetCorners(user->dm, &xs, &ys, NULL, &xm, &ym, NULL));
  PetscCall(DMDAGetGhostCorners(user->dm, &gxs, &gys, NULL, &gxm, &gym, NULL));

  /* Compute the linear term in the objective function */
  PetscCall(VecGetArray(user->B, &b));
  for (i = xs; i < xs + xm; i++) {
    temp = PetscSinScalar((i + 1) * hx);
    for (j = ys; j < ys + ym; j++) {
      k    = xm * (j - ys) + (i - xs);
      b[k] = -ehxhy * temp;
    }
  }
  PetscCall(VecRestoreArray(user->B, &b));
  PetscCall(PetscLogFlops(5.0 * xm * ym + 3.0 * xm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode FormFunctionGradient(Tao tao, Vec X, PetscReal *fcn, Vec G, void *ptr)
{
  AppCtx    *user = (AppCtx *)ptr;
  PetscInt   i, j, k, kk;
  PetscInt   col[5], row, nx, ny, xs, xm, gxs, gxm, ys, ym, gys, gym;
  PetscReal  one = 1.0, two = 2.0, six = 6.0, pi = 4.0 * atan(1.0);
  PetscReal  hx, hy, hxhy, hxhx, hyhy;
  PetscReal  xi, v[5];
  PetscReal  ecc = user->ecc, trule1, trule2, trule3, trule4, trule5, trule6;
  PetscReal  vmiddle, vup, vdown, vleft, vright;
  PetscReal  tt, f1, f2;
  PetscReal *x, *g, zero = 0.0;
  Vec        localX;

  PetscFunctionBegin;
  nx   = user->nx;
  ny   = user->ny;
  hx   = two * pi / (nx + 1.0);
  hy   = two * user->b / (ny + 1.0);
  hxhy = hx * hy;
  hxhx = one / (hx * hx);
  hyhy = one / (hy * hy);

  PetscCall(DMGetLocalVector(user->dm, &localX));

  PetscCall(DMGlobalToLocalBegin(user->dm, X, INSERT_VALUES, localX));
  PetscCall(DMGlobalToLocalEnd(user->dm, X, INSERT_VALUES, localX));

  PetscCall(VecSet(G, zero));
  /*
    Get local grid boundaries
  */
  PetscCall(DMDAGetCorners(user->dm, &xs, &ys, NULL, &xm, &ym, NULL));
  PetscCall(DMDAGetGhostCorners(user->dm, &gxs, &gys, NULL, &gxm, &gym, NULL));

  PetscCall(VecGetArray(localX, &x));
  PetscCall(VecGetArray(G, &g));

  for (i = xs; i < xs + xm; i++) {
    xi     = (i + 1) * hx;
    trule1 = hxhy * (p(xi, ecc) + p(xi + hx, ecc) + p(xi, ecc)) / six;      /* L(i,j) */
    trule2 = hxhy * (p(xi, ecc) + p(xi - hx, ecc) + p(xi, ecc)) / six;      /* U(i,j) */
    trule3 = hxhy * (p(xi, ecc) + p(xi + hx, ecc) + p(xi + hx, ecc)) / six; /* U(i+1,j) */
    trule4 = hxhy * (p(xi, ecc) + p(xi - hx, ecc) + p(xi - hx, ecc)) / six; /* L(i-1,j) */
    trule5 = trule1;                                                        /* L(i,j-1) */
    trule6 = trule2;                                                        /* U(i,j+1) */

    vdown   = -(trule5 + trule2) * hyhy;
    vleft   = -hxhx * (trule2 + trule4);
    vright  = -hxhx * (trule1 + trule3);
    vup     = -hyhy * (trule1 + trule6);
    vmiddle = (hxhx) * (trule1 + trule2 + trule3 + trule4) + hyhy * (trule1 + trule2 + trule5 + trule6);

    for (j = ys; j < ys + ym; j++) {
      row  = (j - gys) * gxm + (i - gxs);
      v[0] = 0;
      v[1] = 0;
      v[2] = 0;
      v[3] = 0;
      v[4] = 0;

      k = 0;
      if (j > gys) {
        v[k]   = vdown;
        col[k] = row - gxm;
        k++;
      }

      if (i > gxs) {
        v[k]   = vleft;
        col[k] = row - 1;
        k++;
      }

      v[k]   = vmiddle;
      col[k] = row;
      k++;

      if (i + 1 < gxs + gxm) {
        v[k]   = vright;
        col[k] = row + 1;
        k++;
      }

      if (j + 1 < gys + gym) {
        v[k]   = vup;
        col[k] = row + gxm;
        k++;
      }
      tt = 0;
      for (kk = 0; kk < k; kk++) tt += v[kk] * x[col[kk]];
      row    = (j - ys) * xm + (i - xs);
      g[row] = tt;
    }
  }

  PetscCall(VecRestoreArray(localX, &x));
  PetscCall(VecRestoreArray(G, &g));

  PetscCall(DMRestoreLocalVector(user->dm, &localX));

  PetscCall(VecDot(X, G, &f1));
  PetscCall(VecDot(user->B, X, &f2));
  PetscCall(VecAXPY(G, one, user->B));
  *fcn = f1 / 2.0 + f2;

  PetscCall(PetscLogFlops((91 + 10.0 * ym) * xm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   FormHessian computes the quadratic term in the quadratic objective function
   Notice that the objective function in this problem is quadratic (therefore a constant
   hessian).  If using a nonquadratic solver, then you might want to reconsider this function
*/
PetscErrorCode FormHessian(Tao tao, Vec X, Mat hes, Mat Hpre, void *ptr)
{
  AppCtx   *user = (AppCtx *)ptr;
  PetscInt  i, j, k;
  PetscInt  col[5], row, nx, ny, xs, xm, gxs, gxm, ys, ym, gys, gym;
  PetscReal one = 1.0, two = 2.0, six = 6.0, pi = 4.0 * atan(1.0);
  PetscReal hx, hy, hxhy, hxhx, hyhy;
  PetscReal xi, v[5];
  PetscReal ecc = user->ecc, trule1, trule2, trule3, trule4, trule5, trule6;
  PetscReal vmiddle, vup, vdown, vleft, vright;
  PetscBool assembled;

  PetscFunctionBegin;
  nx   = user->nx;
  ny   = user->ny;
  hx   = two * pi / (nx + 1.0);
  hy   = two * user->b / (ny + 1.0);
  hxhy = hx * hy;
  hxhx = one / (hx * hx);
  hyhy = one / (hy * hy);

  /*
    Get local grid boundaries
  */
  PetscCall(DMDAGetCorners(user->dm, &xs, &ys, NULL, &xm, &ym, NULL));
  PetscCall(DMDAGetGhostCorners(user->dm, &gxs, &gys, NULL, &gxm, &gym, NULL));
  PetscCall(MatAssembled(hes, &assembled));
  if (assembled) PetscCall(MatZeroEntries(hes));

  for (i = xs; i < xs + xm; i++) {
    xi     = (i + 1) * hx;
    trule1 = hxhy * (p(xi, ecc) + p(xi + hx, ecc) + p(xi, ecc)) / six;      /* L(i,j) */
    trule2 = hxhy * (p(xi, ecc) + p(xi - hx, ecc) + p(xi, ecc)) / six;      /* U(i,j) */
    trule3 = hxhy * (p(xi, ecc) + p(xi + hx, ecc) + p(xi + hx, ecc)) / six; /* U(i+1,j) */
    trule4 = hxhy * (p(xi, ecc) + p(xi - hx, ecc) + p(xi - hx, ecc)) / six; /* L(i-1,j) */
    trule5 = trule1;                                                        /* L(i,j-1) */
    trule6 = trule2;                                                        /* U(i,j+1) */

    vdown   = -(trule5 + trule2) * hyhy;
    vleft   = -hxhx * (trule2 + trule4);
    vright  = -hxhx * (trule1 + trule3);
    vup     = -hyhy * (trule1 + trule6);
    vmiddle = (hxhx) * (trule1 + trule2 + trule3 + trule4) + hyhy * (trule1 + trule2 + trule5 + trule6);
    v[0]    = 0;
    v[1]    = 0;
    v[2]    = 0;
    v[3]    = 0;
    v[4]    = 0;

    for (j = ys; j < ys + ym; j++) {
      row = (j - gys) * gxm + (i - gxs);

      k = 0;
      if (j > gys) {
        v[k]   = vdown;
        col[k] = row - gxm;
        k++;
      }

      if (i > gxs) {
        v[k]   = vleft;
        col[k] = row - 1;
        k++;
      }

      v[k]   = vmiddle;
      col[k] = row;
      k++;

      if (i + 1 < gxs + gxm) {
        v[k]   = vright;
        col[k] = row + 1;
        k++;
      }

      if (j + 1 < gys + gym) {
        v[k]   = vup;
        col[k] = row + gxm;
        k++;
      }
      PetscCall(MatSetValuesLocal(hes, 1, &row, k, col, v, INSERT_VALUES));
    }
  }

  /*
     Assemble matrix, using the 2-step process:
     MatAssemblyBegin(), MatAssemblyEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  PetscCall(MatAssemblyBegin(hes, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(hes, MAT_FINAL_ASSEMBLY));

  /*
    Tell the matrix we will never add a new nonzero location to the
    matrix. If we do it will generate an error.
  */
  PetscCall(MatSetOption(hes, MAT_NEW_NONZERO_LOCATION_ERR, PETSC_TRUE));
  PetscCall(MatSetOption(hes, MAT_SYMMETRIC, PETSC_TRUE));

  PetscCall(PetscLogFlops(9.0 * xm * ym + 49.0 * xm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode Monitor(Tao tao, void *ctx)
{
  PetscInt           its;
  PetscReal          f, gnorm, cnorm, xdiff;
  TaoConvergedReason reason;

  PetscFunctionBegin;
  PetscCall(TaoGetSolutionStatus(tao, &its, &f, &gnorm, &cnorm, &xdiff, &reason));
  if (!(its % 5)) PetscCall(PetscPrintf(PETSC_COMM_WORLD, "iteration=%" PetscInt_FMT "\tf=%g\n", its, (double)f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode ConvergenceTest(Tao tao, void *ctx)
{
  PetscInt           its;
  PetscReal          f, gnorm, cnorm, xdiff;
  TaoConvergedReason reason;

  PetscFunctionBegin;
  PetscCall(TaoGetSolutionStatus(tao, &its, &f, &gnorm, &cnorm, &xdiff, &reason));
  if (its == 100) PetscCall(TaoSetConvergedReason(tao, TAO_DIVERGED_MAXITS));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*TEST

   build:
      requires: !complex

   test:
      args: -tao_smonitor -mx 8 -my 12 -tao_type tron -tao_gatol 1.e-5
      requires: !single

   test:
      suffix: 2
      nsize: 2
      args: -tao_smonitor -mx 50 -my 50 -ecc 0.99 -tao_type gpcg -tao_gatol 1.e-5
      requires: !single

   test:
      suffix: 3
      nsize: 2
      args: -tao_smonitor -mx 10 -my 16 -ecc 0.9 -tao_type bqpip -tao_gatol 1.e-4
      requires: !single

   test:
      suffix: 4
      nsize: 2
      args: -tao_smonitor -mx 10 -my 16 -ecc 0.9 -tao_type bqpip -tao_gatol 1.e-4 -test_getdiagonal
      output_file: output/jbearing2_3.out
      requires: !single

   test:
      suffix: 5
      args: -tao_smonitor -mx 8 -my 12 -tao_type bncg -tao_bncg_type gd -tao_gatol 1e-4
      requires: !single

   test:
      suffix: 6
      args: -tao_smonitor -mx 8 -my 12 -tao_type bncg -tao_gatol 1e-4
      requires: !single

   test:
      suffix: 7
      args: -tao_smonitor -mx 8 -my 12 -tao_type bnls -tao_gatol 1e-5
      requires: !single

   test:
      suffix: 8
      args: -tao_smonitor -mx 8 -my 12 -tao_type bntr -tao_gatol 1e-5
      requires: !single

   test:
      suffix: 9
      args: -tao_smonitor -mx 8 -my 12 -tao_type bntl -tao_gatol 1e-5
      requires: !single

   test:
      suffix: 10
      args: -tao_smonitor -mx 8 -my 12 -tao_type bnls -tao_gatol 1e-5 -tao_bnk_max_cg_its 3
      requires: !single

   test:
      suffix: 11
      args: -tao_smonitor -mx 8 -my 12 -tao_type bntr -tao_gatol 1e-5 -tao_bnk_max_cg_its 3
      requires: !single

   test:
      suffix: 12
      args: -tao_smonitor -mx 8 -my 12 -tao_type bntl -tao_gatol 1e-5 -tao_bnk_max_cg_its 3
      requires: !single

   test:
     suffix: 13
     args: -tao_smonitor -mx 8 -my 12 -tao_gatol 1e-4 -tao_type bqnls
     requires: !single

   test:
     suffix: 14
     args: -tao_smonitor -mx 8 -my 12 -tao_gatol 1e-4 -tao_type blmvm
     requires: !single

   test:
     suffix: 15
     args: -tao_smonitor -mx 8 -my 12 -tao_gatol 1e-4 -tao_type bqnkls -tao_bqnk_mat_type lmvmbfgs
     requires: !single

   test:
     suffix: 16
     args: -tao_smonitor -mx 8 -my 12 -tao_gatol 1e-4 -tao_type bqnktr -tao_bqnk_mat_type lmvmsr1
     requires: !single

   test:
     suffix: 17
     args: -tao_smonitor -mx 8 -my 12 -tao_gatol 1e-4 -tao_type bqnls -tao_bqnls_mat_lmvm_scale_type scalar -tao_view
     requires: !single

   test:
     suffix: 18
     args: -tao_smonitor -mx 8 -my 12 -tao_gatol 1e-4 -tao_type bqnls -tao_bqnls_mat_lmvm_scale_type none -tao_view
     requires: !single

   test:
     suffix: 19
     args: -tao_smonitor -mx 8 -my 12 -tao_type bnls -tao_gatol 1e-5 -tao_mf_hessian
     requires: !single

   test:
      suffix: 20
      args: -tao_smonitor -mx 8 -my 12 -tao_type bntr -tao_gatol 1e-5 -tao_mf_hessian
      requires: !single

   test:
      suffix: 21
      args: -tao_smonitor -mx 8 -my 12 -tao_type bntl -tao_gatol 1e-5 -tao_mf_hessian
      requires: !single
TEST*/

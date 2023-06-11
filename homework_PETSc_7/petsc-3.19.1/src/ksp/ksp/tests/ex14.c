
static char help[] = "Solves a nonlinear system in parallel with a user-defined Newton method.\n\
Uses KSP to solve the linearized Newton systems.  This solver\n\
is a very simplistic inexact Newton method.  The intent of this code is to\n\
demonstrate the repeated solution of linear systems with the same nonzero pattern.\n\
\n\
This is NOT the recommended approach for solving nonlinear problems with PETSc!\n\
We urge users to employ the SNES component for solving nonlinear problems whenever\n\
possible, as it offers many advantages over coding nonlinear solvers independently.\n\
\n\
We solve the  Bratu (SFI - solid fuel ignition) problem in a 2D rectangular\n\
domain, using distributed arrays (DMDAs) to partition the parallel grid.\n\
The command line options include:\n\
  -par <parameter>, where <parameter> indicates the problem's nonlinearity\n\
     problem SFI:  <parameter> = Bratu parameter (0 <= par <= 6.81)\n\
  -mx <xg>, where <xg> = number of grid points in the x-direction\n\
  -my <yg>, where <yg> = number of grid points in the y-direction\n\
  -Nx <npx>, where <npx> = number of processors in the x-direction\n\
  -Ny <npy>, where <npy> = number of processors in the y-direction\n\n";

/* ------------------------------------------------------------------------

    Solid Fuel Ignition (SFI) problem.  This problem is modeled by
    the partial differential equation

            -Laplacian u - lambda*exp(u) = 0,  0 < x,y < 1,

    with boundary conditions

             u = 0  for  x = 0, x = 1, y = 0, y = 1.

    A finite difference approximation with the usual 5-point stencil
    is used to discretize the boundary value problem to obtain a nonlinear
    system of equations.

    The SNES version of this problem is:  snes/tutorials/ex5.c
    We urge users to employ the SNES component for solving nonlinear
    problems whenever possible, as it offers many advantages over coding
    nonlinear solvers independently.

  ------------------------------------------------------------------------- */

/*
   Include "petscdmda.h" so that we can use distributed arrays (DMDAs).
   Include "petscksp.h" so that we can use KSP solvers.  Note that this
   file automatically includes:
     petscsys.h       - base PETSc routines   petscvec.h - vectors
     petscmat.h - matrices
     petscis.h     - index sets            petscksp.h - Krylov subspace methods
     petscviewer.h - viewers               petscpc.h  - preconditioners
*/
#include <petscdm.h>
#include <petscdmda.h>
#include <petscksp.h>

/*
   User-defined application context - contains data needed by the
   application-provided call-back routines, ComputeJacobian() and
   ComputeFunction().
*/
typedef struct {
  PetscReal param;  /* test problem parameter */
  PetscInt  mx, my; /* discretization in x,y directions */
  Vec       localX; /* ghosted local vector */
  DM        da;     /* distributed array data structure */
} AppCtx;

/*
   User-defined routines
*/
extern PetscErrorCode ComputeFunction(AppCtx *, Vec, Vec), FormInitialGuess(AppCtx *, Vec);
extern PetscErrorCode ComputeJacobian(AppCtx *, Vec, Mat);

int main(int argc, char **argv)
{
  /* -------------- Data to define application problem ---------------- */
  MPI_Comm    comm;    /* communicator */
  KSP         ksp;     /* linear solver */
  Vec         X, Y, F; /* solution, update, residual vectors */
  Mat         J;       /* Jacobian matrix */
  AppCtx      user;    /* user-defined work context */
  PetscInt    Nx, Ny;  /* number of preocessors in x- and y- directions */
  PetscMPIInt size;    /* number of processors */
  PetscReal   bratu_lambda_max = 6.81, bratu_lambda_min = 0.;
  PetscInt    m, N;

  /* --------------- Data to define nonlinear solver -------------- */
  PetscReal rtol = 1.e-8;            /* relative convergence tolerance */
  PetscReal xtol = 1.e-8;            /* step convergence tolerance */
  PetscReal ttol;                    /* convergence tolerance */
  PetscReal fnorm, ynorm, xnorm;     /* various vector norms */
  PetscInt  max_nonlin_its = 3;      /* maximum number of iterations for nonlinear solver */
  PetscInt  max_functions  = 50;     /* maximum number of function evaluations */
  PetscInt  lin_its;                 /* number of linear solver iterations for each step */
  PetscInt  i;                       /* nonlinear solve iteration number */
  PetscBool no_output = PETSC_FALSE; /* flag indicating whether to suppress output */

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  comm = PETSC_COMM_WORLD;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-no_output", &no_output, NULL));

  /*
     Initialize problem parameters
  */
  user.mx    = 4;
  user.my    = 4;
  user.param = 6.0;

  PetscCall(PetscOptionsGetInt(NULL, NULL, "-mx", &user.mx, NULL));
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-my", &user.my, NULL));
  PetscCall(PetscOptionsGetReal(NULL, NULL, "-par", &user.param, NULL));
  PetscCheck(user.param < bratu_lambda_max && user.param > bratu_lambda_min, PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "Lambda is out of range");
  N = user.mx * user.my;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create linear solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(KSPCreate(comm, &ksp));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create vector data structures
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Create distributed array (DMDA) to manage parallel grid and vectors
  */
  PetscCallMPI(MPI_Comm_size(comm, &size));
  Nx = PETSC_DECIDE;
  Ny = PETSC_DECIDE;
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-Nx", &Nx, NULL));
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-Ny", &Ny, NULL));
  PetscCheck(Nx * Ny == size || (Nx == PETSC_DECIDE && Ny == PETSC_DECIDE), PETSC_COMM_WORLD, PETSC_ERR_ARG_INCOMP, "Incompatible number of processors:  Nx * Ny != size");
  PetscCall(DMDACreate2d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, DMDA_STENCIL_STAR, user.mx, user.my, Nx, Ny, 1, 1, NULL, NULL, &user.da));
  PetscCall(DMSetFromOptions(user.da));
  PetscCall(DMSetUp(user.da));

  /*
     Extract global and local vectors from DMDA; then duplicate for remaining
     vectors that are the same types
  */
  PetscCall(DMCreateGlobalVector(user.da, &X));
  PetscCall(DMCreateLocalVector(user.da, &user.localX));
  PetscCall(VecDuplicate(X, &F));
  PetscCall(VecDuplicate(X, &Y));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create matrix data structure for Jacobian
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Note:  For the parallel case, vectors and matrices MUST be partitioned
     accordingly.  When using distributed arrays (DMDAs) to create vectors,
     the DMDAs determine the problem partitioning.  We must explicitly
     specify the local matrix dimensions upon its creation for compatibility
     with the vector distribution.  Thus, the generic MatCreate() routine
     is NOT sufficient when working with distributed arrays.

     Note: Here we only approximately preallocate storage space for the
     Jacobian.  See the users manual for a discussion of better techniques
     for preallocating matrix memory.
  */
  if (size == 1) {
    PetscCall(MatCreateSeqAIJ(comm, N, N, 5, NULL, &J));
  } else {
    PetscCall(VecGetLocalSize(X, &m));
    PetscCall(MatCreateAIJ(comm, m, m, N, N, 5, NULL, 3, NULL, &J));
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Customize linear solver; set runtime options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Set runtime options (e.g.,-ksp_monitor -ksp_rtol <rtol> -ksp_type <type>)
  */
  PetscCall(KSPSetFromOptions(ksp));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Evaluate initial guess
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(FormInitialGuess(&user, X));
  PetscCall(ComputeFunction(&user, X, F)); /* Compute F(X)    */
  PetscCall(VecNorm(F, NORM_2, &fnorm));   /* fnorm = || F || */
  ttol = fnorm * rtol;
  if (!no_output) PetscCall(PetscPrintf(comm, "Initial function norm = %g\n", (double)fnorm));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system with a user-defined method
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
      This solver is a very simplistic inexact Newton method, with no
      no damping strategies or bells and whistles. The intent of this code
      is  merely to demonstrate the repeated solution with KSP of linear
      systems with the same nonzero structure.

      This is NOT the recommended approach for solving nonlinear problems
      with PETSc!  We urge users to employ the SNES component for solving
      nonlinear problems whenever possible with application codes, as it
      offers many advantages over coding nonlinear solvers independently.
   */

  for (i = 0; i < max_nonlin_its; i++) {
    /*
        Compute the Jacobian matrix.
     */
    PetscCall(ComputeJacobian(&user, X, J));

    /*
        Solve J Y = F, where J is the Jacobian matrix.
          - First, set the KSP linear operators.  Here the matrix that
            defines the linear system also serves as the preconditioning
            matrix.
          - Then solve the Newton system.
     */
    PetscCall(KSPSetOperators(ksp, J, J));
    PetscCall(KSPSolve(ksp, F, Y));
    PetscCall(KSPGetIterationNumber(ksp, &lin_its));

    /*
       Compute updated iterate
     */
    PetscCall(VecNorm(Y, NORM_2, &ynorm)); /* ynorm = || Y || */
    PetscCall(VecAYPX(Y, -1.0, X));        /* Y <- X - Y      */
    PetscCall(VecCopy(Y, X));              /* X <- Y          */
    PetscCall(VecNorm(X, NORM_2, &xnorm)); /* xnorm = || X || */
    if (!no_output) PetscCall(PetscPrintf(comm, "   linear solve iterations = %" PetscInt_FMT ", xnorm=%g, ynorm=%g\n", lin_its, (double)xnorm, (double)ynorm));

    /*
       Evaluate new nonlinear function
     */
    PetscCall(ComputeFunction(&user, X, F)); /* Compute F(X)    */
    PetscCall(VecNorm(F, NORM_2, &fnorm));   /* fnorm = || F || */
    if (!no_output) PetscCall(PetscPrintf(comm, "Iteration %" PetscInt_FMT ", function norm = %g\n", i + 1, (double)fnorm));

    /*
       Test for convergence
     */
    if (fnorm <= ttol) {
      if (!no_output) PetscCall(PetscPrintf(comm, "Converged due to function norm %g < %g (relative tolerance)\n", (double)fnorm, (double)ttol));
      break;
    }
    if (ynorm < xtol * (xnorm)) {
      if (!no_output) PetscCall(PetscPrintf(comm, "Converged due to small update length: %g < %g * %g\n", (double)ynorm, (double)xtol, (double)xnorm));
      break;
    }
    if (i > max_functions) {
      if (!no_output) PetscCall(PetscPrintf(comm, "Exceeded maximum number of function evaluations: %" PetscInt_FMT " > %" PetscInt_FMT "\n", i, max_functions));
      break;
    }
  }
  PetscCall(PetscPrintf(comm, "Number of nonlinear iterations = %" PetscInt_FMT "\n", i));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(MatDestroy(&J));
  PetscCall(VecDestroy(&Y));
  PetscCall(VecDestroy(&user.localX));
  PetscCall(VecDestroy(&X));
  PetscCall(VecDestroy(&F));
  PetscCall(KSPDestroy(&ksp));
  PetscCall(DMDestroy(&user.da));
  PetscCall(PetscFinalize());
  return 0;
}
/* ------------------------------------------------------------------- */
/*
   FormInitialGuess - Forms initial approximation.

   Input Parameters:
   user - user-defined application context
   X - vector

   Output Parameter:
   X - vector
 */
PetscErrorCode FormInitialGuess(AppCtx *user, Vec X)
{
  PetscInt     i, j, row, mx, my, xs, ys, xm, ym, gxm, gym, gxs, gys;
  PetscReal    one = 1.0, lambda, temp1, temp, hx, hy;
  PetscScalar *x;

  mx     = user->mx;
  my     = user->my;
  lambda = user->param;
  hx     = one / (PetscReal)(mx - 1);
  hy     = one / (PetscReal)(my - 1);
  temp1  = lambda / (lambda + one);

  /*
     Get a pointer to vector data.
       - For default PETSc vectors, VecGetArray() returns a pointer to
         the data array.  Otherwise, the routine is implementation dependent.
       - You MUST call VecRestoreArray() when you no longer need access to
         the array.
  */
  PetscCall(VecGetArray(X, &x));

  /*
     Get local grid boundaries (for 2-dimensional DMDA):
       xs, ys   - starting grid indices (no ghost points)
       xm, ym   - widths of local grid (no ghost points)
       gxs, gys - starting grid indices (including ghost points)
       gxm, gym - widths of local grid (including ghost points)
  */
  PetscCall(DMDAGetCorners(user->da, &xs, &ys, NULL, &xm, &ym, NULL));
  PetscCall(DMDAGetGhostCorners(user->da, &gxs, &gys, NULL, &gxm, &gym, NULL));

  /*
     Compute initial guess over the locally owned part of the grid
  */
  for (j = ys; j < ys + ym; j++) {
    temp = (PetscReal)(PetscMin(j, my - j - 1)) * hy;
    for (i = xs; i < xs + xm; i++) {
      row = i - gxs + (j - gys) * gxm;
      if (i == 0 || j == 0 || i == mx - 1 || j == my - 1) {
        x[row] = 0.0;
        continue;
      }
      x[row] = temp1 * PetscSqrtReal(PetscMin((PetscReal)(PetscMin(i, mx - i - 1)) * hx, temp));
    }
  }

  /*
     Restore vector
  */
  PetscCall(VecRestoreArray(X, &x));
  return PETSC_SUCCESS;
}
/* ------------------------------------------------------------------- */
/*
   ComputeFunction - Evaluates nonlinear function, F(x).

   Input Parameters:
.  X - input vector
.  user - user-defined application context

   Output Parameter:
.  F - function vector
 */
PetscErrorCode ComputeFunction(AppCtx *user, Vec X, Vec F)
{
  PetscInt    i, j, row, mx, my, xs, ys, xm, ym, gxs, gys, gxm, gym;
  PetscReal   two = 2.0, one = 1.0, lambda, hx, hy, hxdhy, hydhx, sc;
  PetscScalar u, uxx, uyy, *x, *f;
  Vec         localX = user->localX;

  mx     = user->mx;
  my     = user->my;
  lambda = user->param;
  hx     = one / (PetscReal)(mx - 1);
  hy     = one / (PetscReal)(my - 1);
  sc     = hx * hy * lambda;
  hxdhy  = hx / hy;
  hydhx  = hy / hx;

  /*
     Scatter ghost points to local vector, using the 2-step process
        DMGlobalToLocalBegin(), DMGlobalToLocalEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  PetscCall(DMGlobalToLocalBegin(user->da, X, INSERT_VALUES, localX));
  PetscCall(DMGlobalToLocalEnd(user->da, X, INSERT_VALUES, localX));

  /*
     Get pointers to vector data
  */
  PetscCall(VecGetArray(localX, &x));
  PetscCall(VecGetArray(F, &f));

  /*
     Get local grid boundaries
  */
  PetscCall(DMDAGetCorners(user->da, &xs, &ys, NULL, &xm, &ym, NULL));
  PetscCall(DMDAGetGhostCorners(user->da, &gxs, &gys, NULL, &gxm, &gym, NULL));

  /*
     Compute function over the locally owned part of the grid
  */
  for (j = ys; j < ys + ym; j++) {
    row = (j - gys) * gxm + xs - gxs - 1;
    for (i = xs; i < xs + xm; i++) {
      row++;
      if (i == 0 || j == 0 || i == mx - 1 || j == my - 1) {
        f[row] = x[row];
        continue;
      }
      u      = x[row];
      uxx    = (two * u - x[row - 1] - x[row + 1]) * hydhx;
      uyy    = (two * u - x[row - gxm] - x[row + gxm]) * hxdhy;
      f[row] = uxx + uyy - sc * PetscExpScalar(u);
    }
  }

  /*
     Restore vectors
  */
  PetscCall(VecRestoreArray(localX, &x));
  PetscCall(VecRestoreArray(F, &f));
  PetscCall(PetscLogFlops(11.0 * ym * xm));
  return PETSC_SUCCESS;
}
/* ------------------------------------------------------------------- */
/*
   ComputeJacobian - Evaluates Jacobian matrix.

   Input Parameters:
.  x - input vector
.  user - user-defined application context

   Output Parameters:
+  jac - Jacobian matrix
-  flag - flag indicating matrix structure

   Notes:
   Due to grid point reordering with DMDAs, we must always work
   with the local grid points, and then transform them to the new
   global numbering with the "ltog" mapping
   We cannot work directly with the global numbers for the original
   uniprocessor grid!
*/
PetscErrorCode ComputeJacobian(AppCtx *user, Vec X, Mat jac)
{
  Vec                    localX = user->localX; /* local vector */
  const PetscInt        *ltog;                  /* local-to-global mapping */
  PetscInt               i, j, row, mx, my, col[5];
  PetscInt               xs, ys, xm, ym, gxs, gys, gxm, gym, grow;
  PetscScalar            two = 2.0, one = 1.0, lambda, v[5], hx, hy, hxdhy, hydhx, sc, *x;
  ISLocalToGlobalMapping ltogm;

  mx     = user->mx;
  my     = user->my;
  lambda = user->param;
  hx     = one / (PetscReal)(mx - 1);
  hy     = one / (PetscReal)(my - 1);
  sc     = hx * hy;
  hxdhy  = hx / hy;
  hydhx  = hy / hx;

  /*
     Scatter ghost points to local vector, using the 2-step process
        DMGlobalToLocalBegin(), DMGlobalToLocalEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  PetscCall(DMGlobalToLocalBegin(user->da, X, INSERT_VALUES, localX));
  PetscCall(DMGlobalToLocalEnd(user->da, X, INSERT_VALUES, localX));

  /*
     Get pointer to vector data
  */
  PetscCall(VecGetArray(localX, &x));

  /*
     Get local grid boundaries
  */
  PetscCall(DMDAGetCorners(user->da, &xs, &ys, NULL, &xm, &ym, NULL));
  PetscCall(DMDAGetGhostCorners(user->da, &gxs, &gys, NULL, &gxm, &gym, NULL));

  /*
     Get the global node numbers for all local nodes, including ghost points
  */
  PetscCall(DMGetLocalToGlobalMapping(user->da, &ltogm));
  PetscCall(ISLocalToGlobalMappingGetIndices(ltogm, &ltog));

  /*
     Compute entries for the locally owned part of the Jacobian.
      - Currently, all PETSc parallel matrix formats are partitioned by
        contiguous chunks of rows across the processors. The "grow"
        parameter computed below specifies the global row number
        corresponding to each local grid point.
      - Each processor needs to insert only elements that it owns
        locally (but any non-local elements will be sent to the
        appropriate processor during matrix assembly).
      - Always specify global row and columns of matrix entries.
      - Here, we set all entries for a particular row at once.
  */
  for (j = ys; j < ys + ym; j++) {
    row = (j - gys) * gxm + xs - gxs - 1;
    for (i = xs; i < xs + xm; i++) {
      row++;
      grow = ltog[row];
      /* boundary points */
      if (i == 0 || j == 0 || i == mx - 1 || j == my - 1) {
        PetscCall(MatSetValues(jac, 1, &grow, 1, &grow, &one, INSERT_VALUES));
        continue;
      }
      /* interior grid points */
      v[0]   = -hxdhy;
      col[0] = ltog[row - gxm];
      v[1]   = -hydhx;
      col[1] = ltog[row - 1];
      v[2]   = two * (hydhx + hxdhy) - sc * lambda * PetscExpScalar(x[row]);
      col[2] = grow;
      v[3]   = -hydhx;
      col[3] = ltog[row + 1];
      v[4]   = -hxdhy;
      col[4] = ltog[row + gxm];
      PetscCall(MatSetValues(jac, 1, &grow, 5, col, v, INSERT_VALUES));
    }
  }
  PetscCall(ISLocalToGlobalMappingRestoreIndices(ltogm, &ltog));

  /*
     Assemble matrix, using the 2-step process:
       MatAssemblyBegin(), MatAssemblyEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  PetscCall(MatAssemblyBegin(jac, MAT_FINAL_ASSEMBLY));
  PetscCall(VecRestoreArray(localX, &x));
  PetscCall(MatAssemblyEnd(jac, MAT_FINAL_ASSEMBLY));

  return PETSC_SUCCESS;
}

/*TEST

    test:

TEST*/

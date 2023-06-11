
static char help[] = "Solves a linear system with KSP.  This problem is\n\
intended to test the complex numbers version of various solvers.\n\n";

#include <petscksp.h>

typedef enum {
  TEST_1,
  TEST_2,
  TEST_3,
  HELMHOLTZ_1,
  HELMHOLTZ_2
} TestType;
extern PetscErrorCode FormTestMatrix(Mat, PetscInt, TestType);

int main(int argc, char **args)
{
  Vec         x, b, u; /* approx solution, RHS, exact solution */
  Mat         A;       /* linear system matrix */
  KSP         ksp;     /* KSP context */
  PetscInt    n = 10, its, dim, p = 1, use_random;
  PetscScalar none = -1.0, pfive = 0.5;
  PetscReal   norm;
  PetscRandom rctx;
  TestType    type;
  PetscBool   flg;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &args, (char *)0, help));
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-n", &n, NULL));
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-p", &p, NULL));
  switch (p) {
  case 1:
    type = TEST_1;
    dim  = n;
    break;
  case 2:
    type = TEST_2;
    dim  = n;
    break;
  case 3:
    type = TEST_3;
    dim  = n;
    break;
  case 4:
    type = HELMHOLTZ_1;
    dim  = n * n;
    break;
  case 5:
    type = HELMHOLTZ_2;
    dim  = n * n;
    break;
  default:
    type = TEST_1;
    dim  = n;
  }

  /* Create vectors */
  PetscCall(VecCreate(PETSC_COMM_WORLD, &x));
  PetscCall(VecSetSizes(x, PETSC_DECIDE, dim));
  PetscCall(VecSetFromOptions(x));
  PetscCall(VecDuplicate(x, &b));
  PetscCall(VecDuplicate(x, &u));

  use_random = 1;
  flg        = PETSC_FALSE;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-norandom", &flg, NULL));
  if (flg) {
    use_random = 0;
    PetscCall(VecSet(u, pfive));
  } else {
    PetscCall(PetscRandomCreate(PETSC_COMM_WORLD, &rctx));
    PetscCall(PetscRandomSetFromOptions(rctx));
    PetscCall(VecSetRandom(u, rctx));
  }

  /* Create and assemble matrix */
  PetscCall(MatCreate(PETSC_COMM_WORLD, &A));
  PetscCall(MatSetSizes(A, PETSC_DECIDE, PETSC_DECIDE, dim, dim));
  PetscCall(MatSetFromOptions(A));
  PetscCall(MatSetUp(A));
  PetscCall(FormTestMatrix(A, n, type));
  PetscCall(MatMult(A, u, b));
  flg = PETSC_FALSE;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-printout", &flg, NULL));
  if (flg) {
    PetscCall(MatView(A, PETSC_VIEWER_STDOUT_WORLD));
    PetscCall(VecView(u, PETSC_VIEWER_STDOUT_WORLD));
    PetscCall(VecView(b, PETSC_VIEWER_STDOUT_WORLD));
  }

  /* Create KSP context; set operators and options; solve linear system */
  PetscCall(KSPCreate(PETSC_COMM_WORLD, &ksp));
  PetscCall(KSPSetOperators(ksp, A, A));
  PetscCall(KSPSetFromOptions(ksp));
  PetscCall(KSPSolve(ksp, b, x));
  /* PetscCall(KSPView(ksp,PETSC_VIEWER_STDOUT_WORLD)); */

  /* Check error */
  PetscCall(VecAXPY(x, none, u));
  PetscCall(VecNorm(x, NORM_2, &norm));
  PetscCall(KSPGetIterationNumber(ksp, &its));
  if (norm >= 1.e-12) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Norm of error %g, Iterations %" PetscInt_FMT "\n", (double)norm, its));
  } else {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Norm of error < 1.e-12, Iterations %" PetscInt_FMT "\n", its));
  }

  /* Free work space */
  PetscCall(VecDestroy(&x));
  PetscCall(VecDestroy(&u));
  PetscCall(VecDestroy(&b));
  PetscCall(MatDestroy(&A));
  if (use_random) PetscCall(PetscRandomDestroy(&rctx));
  PetscCall(KSPDestroy(&ksp));
  PetscCall(PetscFinalize());
  return 0;
}

PetscErrorCode FormTestMatrix(Mat A, PetscInt n, TestType type)
{
  PetscScalar val[5];
  PetscInt    i, j, Ii, J, col[5], Istart, Iend;

  PetscFunctionBeginUser;
  PetscCall(MatGetOwnershipRange(A, &Istart, &Iend));
  if (type == TEST_1) {
    val[0] = 1.0;
    val[1] = 4.0;
    val[2] = -2.0;
    for (i = 1; i < n - 1; i++) {
      col[0] = i - 1;
      col[1] = i;
      col[2] = i + 1;
      PetscCall(MatSetValues(A, 1, &i, 3, col, val, INSERT_VALUES));
    }
    i      = n - 1;
    col[0] = n - 2;
    col[1] = n - 1;
    PetscCall(MatSetValues(A, 1, &i, 2, col, val, INSERT_VALUES));
    i      = 0;
    col[0] = 0;
    col[1] = 1;
    val[0] = 4.0;
    val[1] = -2.0;
    PetscCall(MatSetValues(A, 1, &i, 2, col, val, INSERT_VALUES));
  } else if (type == TEST_2) {
    val[0] = 1.0;
    val[1] = 0.0;
    val[2] = 2.0;
    val[3] = 1.0;
    for (i = 2; i < n - 1; i++) {
      col[0] = i - 2;
      col[1] = i - 1;
      col[2] = i;
      col[3] = i + 1;
      PetscCall(MatSetValues(A, 1, &i, 4, col, val, INSERT_VALUES));
    }
    i      = n - 1;
    col[0] = n - 3;
    col[1] = n - 2;
    col[2] = n - 1;
    PetscCall(MatSetValues(A, 1, &i, 3, col, val, INSERT_VALUES));
    i      = 1;
    col[0] = 0;
    col[1] = 1;
    col[2] = 2;
    PetscCall(MatSetValues(A, 1, &i, 3, col, &val[1], INSERT_VALUES));
    i = 0;
    PetscCall(MatSetValues(A, 1, &i, 2, col, &val[2], INSERT_VALUES));
  } else if (type == TEST_3) {
    val[0] = PETSC_i * 2.0;
    val[1] = 4.0;
    val[2] = 0.0;
    val[3] = 1.0;
    val[4] = 0.7;
    for (i = 1; i < n - 3; i++) {
      col[0] = i - 1;
      col[1] = i;
      col[2] = i + 1;
      col[3] = i + 2;
      col[4] = i + 3;
      PetscCall(MatSetValues(A, 1, &i, 5, col, val, INSERT_VALUES));
    }
    i      = n - 3;
    col[0] = n - 4;
    col[1] = n - 3;
    col[2] = n - 2;
    col[3] = n - 1;
    PetscCall(MatSetValues(A, 1, &i, 4, col, val, INSERT_VALUES));
    i      = n - 2;
    col[0] = n - 3;
    col[1] = n - 2;
    col[2] = n - 1;
    PetscCall(MatSetValues(A, 1, &i, 3, col, val, INSERT_VALUES));
    i      = n - 1;
    col[0] = n - 2;
    col[1] = n - 1;
    PetscCall(MatSetValues(A, 1, &i, 2, col, val, INSERT_VALUES));
    i      = 0;
    col[0] = 0;
    col[1] = 1;
    col[2] = 2;
    col[3] = 3;
    PetscCall(MatSetValues(A, 1, &i, 4, col, &val[1], INSERT_VALUES));
  } else if (type == HELMHOLTZ_1) {
    /* Problem domain: unit square: (0,1) x (0,1)
       Solve Helmholtz equation:
          -delta u - sigma1*u + i*sigma2*u = f,
           where delta = Laplace operator
       Dirichlet b.c.'s on all sides
     */
    PetscRandom rctx;
    PetscReal   h2, sigma1 = 5.0;
    PetscScalar sigma2;
    PetscCall(PetscOptionsGetReal(NULL, NULL, "-sigma1", &sigma1, NULL));
    PetscCall(PetscRandomCreate(PETSC_COMM_WORLD, &rctx));
    PetscCall(PetscRandomSetFromOptions(rctx));
    PetscCall(PetscRandomSetInterval(rctx, 0.0, PETSC_i));
    h2 = 1.0 / ((n + 1) * (n + 1));
    for (Ii = Istart; Ii < Iend; Ii++) {
      *val = -1.0;
      i    = Ii / n;
      j    = Ii - i * n;
      if (i > 0) {
        J = Ii - n;
        PetscCall(MatSetValues(A, 1, &Ii, 1, &J, val, ADD_VALUES));
      }
      if (i < n - 1) {
        J = Ii + n;
        PetscCall(MatSetValues(A, 1, &Ii, 1, &J, val, ADD_VALUES));
      }
      if (j > 0) {
        J = Ii - 1;
        PetscCall(MatSetValues(A, 1, &Ii, 1, &J, val, ADD_VALUES));
      }
      if (j < n - 1) {
        J = Ii + 1;
        PetscCall(MatSetValues(A, 1, &Ii, 1, &J, val, ADD_VALUES));
      }
      PetscCall(PetscRandomGetValue(rctx, &sigma2));
      *val = 4.0 - sigma1 * h2 + sigma2 * h2;
      PetscCall(MatSetValues(A, 1, &Ii, 1, &Ii, val, ADD_VALUES));
    }
    PetscCall(PetscRandomDestroy(&rctx));
  } else if (type == HELMHOLTZ_2) {
    /* Problem domain: unit square: (0,1) x (0,1)
       Solve Helmholtz equation:
          -delta u - sigma1*u = f,
           where delta = Laplace operator
       Dirichlet b.c.'s on 3 sides
       du/dn = i*alpha*u on (1,y), 0<y<1
     */
    PetscReal   h2, sigma1 = 200.0;
    PetscScalar alpha_h;
    PetscCall(PetscOptionsGetReal(NULL, NULL, "-sigma1", &sigma1, NULL));
    h2      = 1.0 / ((n + 1) * (n + 1));
    alpha_h = (PETSC_i * 10.0) / (PetscReal)(n + 1); /* alpha_h = alpha * h */
    for (Ii = Istart; Ii < Iend; Ii++) {
      *val = -1.0;
      i    = Ii / n;
      j    = Ii - i * n;
      if (i > 0) {
        J = Ii - n;
        PetscCall(MatSetValues(A, 1, &Ii, 1, &J, val, ADD_VALUES));
      }
      if (i < n - 1) {
        J = Ii + n;
        PetscCall(MatSetValues(A, 1, &Ii, 1, &J, val, ADD_VALUES));
      }
      if (j > 0) {
        J = Ii - 1;
        PetscCall(MatSetValues(A, 1, &Ii, 1, &J, val, ADD_VALUES));
      }
      if (j < n - 1) {
        J = Ii + 1;
        PetscCall(MatSetValues(A, 1, &Ii, 1, &J, val, ADD_VALUES));
      }
      *val = 4.0 - sigma1 * h2;
      if (!((Ii + 1) % n)) *val += alpha_h;
      PetscCall(MatSetValues(A, 1, &Ii, 1, &Ii, val, ADD_VALUES));
    }
  } else SETERRQ(PetscObjectComm((PetscObject)A), PETSC_ERR_USER_INPUT, "FormTestMatrix: unknown test matrix type");

  PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*TEST

    build:
      requires: complex

    test:
      args: -ksp_gmres_cgs_refinement_type refine_always -n 6 -ksp_monitor_short -p 5 -norandom -ksp_type gmres -pc_type jacobi -ksp_max_it 15
      requires: complex

    test:
      suffix: 2
      nsize: 3
      requires: complex
      args: -ksp_gmres_cgs_refinement_type refine_always -n 6 -ksp_monitor_short -p 5 -norandom -ksp_type gmres -pc_type jacobi -ksp_max_it 15
      output_file: output/ex17_1.out

    test:
      suffix: superlu_dist
      requires: superlu_dist complex
      args: -n 6 -p 5 -norandom -pc_type lu -pc_factor_mat_solver_type superlu_dist -mat_superlu_dist_colperm MMD_ATA

    test:
      suffix: superlu_dist_2
      requires: superlu_dist complex
      nsize: 3
      output_file: output/ex17_superlu_dist.out
      args: -n 6 -p 5 -norandom -pc_type lu -pc_factor_mat_solver_type superlu_dist -mat_superlu_dist_colperm MMD_ATA

TEST*/

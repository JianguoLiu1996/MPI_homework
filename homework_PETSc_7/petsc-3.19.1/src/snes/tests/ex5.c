
static char help[] = "Newton method to solve u'' + u^{2} = f, sequentially.\n\
This example tests PCVPBJacobiSetBlocks().\n\n";

/*
   Include "petscsnes.h" so that we can use SNES solvers.  Note that this
   file automatically includes:
     petscsys.h       - base PETSc routines   petscvec.h - vectors
     petscmat.h - matrices
     petscis.h     - index sets            petscksp.h - Krylov subspace methods
     petscviewer.h - viewers               petscpc.h  - preconditioners
     petscksp.h   - linear solvers
*/

#include <petscsnes.h>

/*
   User-defined routines
*/
extern PetscErrorCode FormJacobian(SNES, Vec, Mat, Mat, void *);
extern PetscErrorCode FormFunction(SNES, Vec, Vec, void *);
extern PetscErrorCode FormInitialGuess(Vec);

int main(int argc, char **argv)
{
  SNES        snes;       /* SNES context */
  Vec         x, r, F, U; /* vectors */
  Mat         J;          /* Jacobian matrix */
  PetscInt    its, n = 5, i, maxit, maxf, lens[3] = {1, 2, 2};
  PetscMPIInt size;
  PetscScalar h, xp, v, none = -1.0;
  PetscReal   abstol, rtol, stol, norm;
  KSP         ksp;
  PC          pc;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));
  PetscCheck(size == 1, PETSC_COMM_SELF, PETSC_ERR_WRONG_MPI_SIZE, "This is a uniprocessor example only!");
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-n", &n, NULL));
  h = 1.0 / (n - 1);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create nonlinear solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(SNESCreate(PETSC_COMM_WORLD, &snes));
  PetscCall(SNESGetKSP(snes, &ksp));
  PetscCall(KSPGetPC(ksp, &pc));
  PetscCall(PCSetType(pc, PCVPBJACOBI));
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create vector data structures; set function evaluation routine
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Note that we form 1 vector from scratch and then duplicate as needed.
  */
  PetscCall(VecCreate(PETSC_COMM_WORLD, &x));
  PetscCall(VecSetSizes(x, PETSC_DECIDE, n));
  PetscCall(VecSetFromOptions(x));
  PetscCall(VecDuplicate(x, &r));
  PetscCall(VecDuplicate(x, &F));
  PetscCall(VecDuplicate(x, &U));

  /*
     Set function evaluation routine and vector
  */
  PetscCall(SNESSetFunction(snes, r, FormFunction, (void *)F));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create matrix data structure; set Jacobian evaluation routine
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(MatCreate(PETSC_COMM_WORLD, &J));
  PetscCall(MatSetSizes(J, PETSC_DECIDE, PETSC_DECIDE, n, n));
  PetscCall(MatSetFromOptions(J));
  PetscCall(MatSeqAIJSetPreallocation(J, 3, NULL));
  PetscCall(MatSetVariableBlockSizes(J, 3, lens));

  /*
     Set Jacobian matrix data structure and default Jacobian evaluation
     routine. User can override with:
     -snes_fd : default finite differencing approximation of Jacobian
     -snes_mf : matrix-free Newton-Krylov method with no preconditioning
                (unless user explicitly sets preconditioner)
     -snes_mf_operator : form preconditioning matrix as set by the user,
                         but use matrix-free approx for Jacobian-vector
                         products within Newton-Krylov method
  */

  PetscCall(SNESSetJacobian(snes, J, J, FormJacobian, NULL));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Customize nonlinear solver; set runtime options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Set names for some vectors to facilitate monitoring (optional)
  */
  PetscCall(PetscObjectSetName((PetscObject)x, "Approximate Solution"));
  PetscCall(PetscObjectSetName((PetscObject)U, "Exact Solution"));

  /*
     Set SNES/KSP/KSP/PC runtime options, e.g.,
         -snes_view -snes_monitor -ksp_type <ksp> -pc_type <pc>
  */
  PetscCall(SNESSetFromOptions(snes));

  /*
     Print parameters used for convergence testing (optional) ... just
     to demonstrate this routine; this information is also printed with
     the option -snes_view
  */
  PetscCall(SNESGetTolerances(snes, &abstol, &rtol, &stol, &maxit, &maxf));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "atol=%g, rtol=%g, stol=%g, maxit=%" PetscInt_FMT ", maxf=%" PetscInt_FMT "\n", (double)abstol, (double)rtol, (double)stol, maxit, maxf));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize application:
     Store right-hand-side of PDE and exact solution
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  xp = 0.0;
  for (i = 0; i < n; i++) {
    v = 6.0 * xp + PetscPowScalar(xp + 1.e-12, 6.0); /* +1.e-12 is to prevent 0^6 */
    PetscCall(VecSetValues(F, 1, &i, &v, INSERT_VALUES));
    v = xp * xp * xp;
    PetscCall(VecSetValues(U, 1, &i, &v, INSERT_VALUES));
    xp += h;
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Evaluate initial guess; then solve nonlinear system
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Note: The user should initialize the vector, x, with the initial guess
     for the nonlinear solver prior to calling SNESSolve().  In particular,
     to employ an initial guess of zero, the user should explicitly set
     this vector to zero by calling VecSet().
  */
  PetscCall(FormInitialGuess(x));
  PetscCall(SNESSolve(snes, NULL, x));
  PetscCall(SNESGetIterationNumber(snes, &its));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "number of SNES iterations = %" PetscInt_FMT "\n\n", its));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Check solution and clean up
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Check the error
  */
  PetscCall(VecAXPY(x, none, U));
  PetscCall(VecNorm(x, NORM_2, &norm));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Norm of error %g, Iterations %" PetscInt_FMT "\n", (double)norm, its));

  /*
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
  */
  PetscCall(VecDestroy(&x));
  PetscCall(VecDestroy(&r));
  PetscCall(VecDestroy(&U));
  PetscCall(VecDestroy(&F));
  PetscCall(MatDestroy(&J));
  PetscCall(SNESDestroy(&snes));
  PetscCall(PetscFinalize());
  return 0;
}

/*
   FormInitialGuess - Computes initial guess.

   Input/Output Parameter:
.  x - the solution vector
*/
PetscErrorCode FormInitialGuess(Vec x)
{
  PetscScalar pfive = .50;

  PetscFunctionBeginUser;
  PetscCall(VecSet(x, pfive));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   FormFunction - Evaluates nonlinear function, F(x).

   Input Parameters:
.  snes - the SNES context
.  x - input vector
.  ctx - optional user-defined context, as set by SNESSetFunction()

   Output Parameter:
.  f - function vector

   Note:
   The user-defined context can contain any application-specific data
   needed for the function evaluation (such as various parameters, work
   vectors, and grid information).  In this program the context is just
   a vector containing the right-hand-side of the discretized PDE.
 */

PetscErrorCode FormFunction(SNES snes, Vec x, Vec f, void *ctx)
{
  Vec                g = (Vec)ctx;
  const PetscScalar *xx, *gg;
  PetscScalar       *ff, d;
  PetscInt           i, n;

  PetscFunctionBeginUser;
  /*
     Get pointers to vector data.
       - For default PETSc vectors, VecGetArray() returns a pointer to
         the data array.  Otherwise, the routine is implementation dependent.
       - You MUST call VecRestoreArray() when you no longer need access to
         the array.
  */
  PetscCall(VecGetArrayRead(x, &xx));
  PetscCall(VecGetArray(f, &ff));
  PetscCall(VecGetArrayRead(g, &gg));

  /*
     Compute function
  */
  PetscCall(VecGetSize(x, &n));
  d     = (PetscReal)(n - 1);
  d     = d * d;
  ff[0] = xx[0];
  for (i = 1; i < n - 1; i++) ff[i] = d * (xx[i - 1] - 2.0 * xx[i] + xx[i + 1]) + xx[i] * xx[i] - gg[i];
  ff[n - 1] = xx[n - 1] - 1.0;

  /*
     Restore vectors
  */
  PetscCall(VecRestoreArrayRead(x, &xx));
  PetscCall(VecRestoreArray(f, &ff));
  PetscCall(VecRestoreArrayRead(g, &gg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   FormJacobian - Evaluates Jacobian matrix.

   Input Parameters:
.  snes - the SNES context
.  x - input vector
.  dummy - optional user-defined context (not used here)

   Output Parameters:
.  jac - Jacobian matrix
.  B - optionally different preconditioning matrix

*/

PetscErrorCode FormJacobian(SNES snes, Vec x, Mat jac, Mat B, void *dummy)
{
  const PetscScalar *xx;
  PetscScalar        A[3], d;
  PetscInt           i, n, j[3];

  PetscFunctionBeginUser;
  /*
     Get pointer to vector data
  */
  PetscCall(VecGetArrayRead(x, &xx));

  /*
     Compute Jacobian entries and insert into matrix.
      - Note that in this case we set all elements for a particular
        row at once.
  */
  PetscCall(VecGetSize(x, &n));
  d = (PetscReal)(n - 1);
  d = d * d;

  /*
     Interior grid points
  */
  for (i = 1; i < n - 1; i++) {
    j[0] = i - 1;
    j[1] = i;
    j[2] = i + 1;
    A[0] = d;
    A[1] = -2.0 * d + 2.0 * xx[i];
    A[2] = d;
    PetscCall(MatSetValues(B, 1, &i, 3, j, A, INSERT_VALUES));
  }

  /*
     Boundary points
  */
  i    = 0;
  A[0] = 1.0;

  PetscCall(MatSetValues(B, 1, &i, 1, &i, A, INSERT_VALUES));

  i    = n - 1;
  A[0] = 1.0;

  PetscCall(MatSetValues(B, 1, &i, 1, &i, A, INSERT_VALUES));

  /*
     Restore vector
  */
  PetscCall(VecRestoreArrayRead(x, &xx));

  /*
     Assemble matrix
  */
  PetscCall(MatAssemblyBegin(B, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(B, MAT_FINAL_ASSEMBLY));
  if (jac != B) {
    PetscCall(MatAssemblyBegin(jac, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(jac, MAT_FINAL_ASSEMBLY));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*TEST

   testset:
     args: -snes_monitor_short -snes_view -ksp_monitor
     output_file: output/ex5_1.out
     filter: grep -v "type: seqaij"

     test:
      suffix: 1

     test:
      suffix: cuda
      requires: cuda
      args: -mat_type aijcusparse -vec_type cuda

     test:
      suffix: kok
      requires: kokkos_kernels
      args: -mat_type aijkokkos -vec_type kokkos

   # this is just a test for SNESKSPTRASPOSEONLY and KSPSolveTranspose to behave properly
   # the solution is wrong on purpose
   test:
      requires: !single !complex
      suffix: transpose_only
      args: -snes_monitor_short -snes_view -ksp_monitor -snes_type ksptransposeonly -pc_type ilu -snes_test_jacobian -snes_test_jacobian_view -ksp_view_rhs -ksp_view_solution -ksp_view_mat_explicit -ksp_view_preconditioned_operator_explicit

TEST*/

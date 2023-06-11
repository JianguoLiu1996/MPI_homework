static char help[] = "Small ODE to test TS accuracy.\n";

/*
  The ODE
                  u1_t = cos(t),
                  u2_t = sin(u2)
  with analytical solution
                  u1(t) = sin(t),
                  u2(t) = 2 * atan(exp(t) * tan(0.5))
  is used to test the accuracy of TS schemes.
*/

#include <petscts.h>

/*
     Defines the ODE passed to the ODE solver in explicit form: U_t = F(U)
*/
static PetscErrorCode RHSFunction(TS ts, PetscReal t, Vec U, Vec F, void *s)
{
  PetscScalar       *f;
  const PetscScalar *u;

  PetscFunctionBeginUser;
  PetscCall(VecGetArrayRead(U, &u));
  PetscCall(VecGetArray(F, &f));

  f[0] = PetscCosReal(t);
  f[1] = PetscSinScalar(u[1]);

  PetscCall(VecRestoreArrayRead(U, &u));
  PetscCall(VecRestoreArray(F, &f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
     Defines the exact solution.
*/
static PetscErrorCode ExactSolution(PetscReal t, Vec U)
{
  PetscScalar *u;

  PetscFunctionBeginUser;
  PetscCall(VecGetArray(U, &u));

  u[0] = PetscSinReal(t);
  u[1] = 2 * PetscAtanReal(PetscExpReal(t) * PetscTanReal(0.5));

  PetscCall(VecRestoreArray(U, &u));
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  TS           ts;  /* ODE integrator */
  Vec          U;   /* numerical solution will be stored here */
  Vec          Uex; /* analytical (exact) solution will be stored here */
  PetscMPIInt  size;
  PetscInt     n = 2;
  PetscScalar *u;
  PetscReal    t, final_time = 1.0, dt = 0.25;
  PetscReal    error;
  TSAdapt      adapt;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));
  PetscCheck(size == 1, PETSC_COMM_WORLD, PETSC_ERR_WRONG_MPI_SIZE, "Only for sequential runs");

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSCreate(PETSC_COMM_WORLD, &ts));
  PetscCall(TSSetType(ts, TSROSW));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set ODE routines
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSetProblemType(ts, TS_NONLINEAR));
  PetscCall(TSSetRHSFunction(ts, NULL, RHSFunction, NULL));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set initial conditions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(VecCreate(PETSC_COMM_WORLD, &U));
  PetscCall(VecSetSizes(U, n, PETSC_DETERMINE));
  PetscCall(VecSetUp(U));
  PetscCall(VecGetArray(U, &u));
  u[0] = 0.0;
  u[1] = 1.0;
  PetscCall(VecRestoreArray(U, &u));
  PetscCall(TSSetSolution(ts, U));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set solver options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSetSaveTrajectory(ts));
  PetscCall(TSSetMaxTime(ts, final_time));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_STEPOVER));
  PetscCall(TSSetTimeStep(ts, dt));
  /* The adaptive time step controller is forced to take constant time steps. */
  PetscCall(TSGetAdapt(ts, &adapt));
  PetscCall(TSAdaptSetType(adapt, TSADAPTNONE));

  PetscCall(TSSetFromOptions(ts));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Run timestepping solver and compute error
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSolve(ts, U));
  PetscCall(TSGetTime(ts, &t));

  if (PetscAbsReal(t - final_time) > 100 * PETSC_MACHINE_EPSILON) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Note: There is a difference of %g between the prescribed final time %g and the actual final time.\n", (double)(final_time - t), (double)final_time));
  }
  PetscCall(VecDuplicate(U, &Uex));
  PetscCall(ExactSolution(t, Uex));

  PetscCall(VecAYPX(Uex, -1.0, U));
  PetscCall(VecNorm(Uex, NORM_2, &error));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Error at final time: %.2E\n", (double)error));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they are no longer needed.
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(VecDestroy(&U));
  PetscCall(VecDestroy(&Uex));
  PetscCall(TSDestroy(&ts));

  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

    test:
      suffix: 3bs
      args: -ts_type rk -ts_rk_type 3bs
      requires: !single

    test:
      suffix: 5bs
      args: -ts_type rk -ts_rk_type 5bs
      requires: !single

    test:
      suffix: 5dp
      args: -ts_type rk -ts_rk_type 5dp
      requires: !single

    test:
      suffix: 6vr
      args: -ts_type rk -ts_rk_type 6vr
      requires: !single

    test:
      suffix: 7vr
      args: -ts_type rk -ts_rk_type 7vr
      requires: !single

    test:
      suffix: 8vr
      args: -ts_type rk -ts_rk_type 8vr
      requires: !single

TEST*/

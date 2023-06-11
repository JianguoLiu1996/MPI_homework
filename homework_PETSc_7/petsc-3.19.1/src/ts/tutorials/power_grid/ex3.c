
static char help[] = "Basic equation for generator stability analysis.\n";

/*F

\begin{eqnarray}
                 \frac{d \theta}{dt} = \omega_b (\omega - \omega_s)
                 \frac{2 H}{\omega_s}\frac{d \omega}{dt} & = & P_m - P_max \sin(\theta) -D(\omega - \omega_s)\\
\end{eqnarray}

  Ensemble of initial conditions
   ./ex3 -ensemble -ts_monitor_draw_solution_phase -1,-3,3,3      -ts_adapt_dt_max .01  -ts_monitor -ts_type rosw -pc_type lu -ksp_type preonly

  Fault at .1 seconds
   ./ex3           -ts_monitor_draw_solution_phase .42,.95,.6,1.05 -ts_adapt_dt_max .01  -ts_monitor -ts_type rosw -pc_type lu -ksp_type preonly

  Initial conditions same as when fault is ended
   ./ex3 -u 0.496792,1.00932 -ts_monitor_draw_solution_phase .42,.95,.6,1.05  -ts_adapt_dt_max .01  -ts_monitor -ts_type rosw -pc_type lu -ksp_type preonly

F*/

/*
   Include "petscts.h" so that we can use TS solvers.  Note that this
   file automatically includes:
     petscsys.h       - base PETSc routines   petscvec.h - vectors
     petscmat.h - matrices
     petscis.h     - index sets            petscksp.h - Krylov subspace methods
     petscviewer.h - viewers               petscpc.h  - preconditioners
     petscksp.h   - linear solvers
*/

#include <petscts.h>
#include "ex3.h"

int main(int argc, char **argv)
{
  TS           ts; /* ODE integrator */
  Vec          U;  /* solution will be stored here */
  Mat          A;  /* Jacobian matrix */
  PetscMPIInt  size;
  PetscInt     n = 2;
  AppCtx       ctx;
  PetscScalar *u;
  PetscReal    du[2]    = {0.0, 0.0};
  PetscBool    ensemble = PETSC_FALSE, flg1, flg2;
  PetscInt     direction[2];
  PetscBool    terminate[2];

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));
  PetscCheck(size == 1, PETSC_COMM_WORLD, PETSC_ERR_WRONG_MPI_SIZE, "Only for sequential runs");

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    Create necessary matrix and vectors
    - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(MatCreate(PETSC_COMM_WORLD, &A));
  PetscCall(MatSetSizes(A, n, n, PETSC_DETERMINE, PETSC_DETERMINE));
  PetscCall(MatSetType(A, MATDENSE));
  PetscCall(MatSetFromOptions(A));
  PetscCall(MatSetUp(A));

  PetscCall(MatCreateVecs(A, &U, NULL));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    Set runtime options
    - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscOptionsBegin(PETSC_COMM_WORLD, NULL, "Swing equation options", "");
  {
    ctx.omega_b = 1.0;
    ctx.omega_s = 2.0 * PETSC_PI * 60.0;
    ctx.H       = 5.0;
    PetscCall(PetscOptionsScalar("-Inertia", "", "", ctx.H, &ctx.H, NULL));
    ctx.D = 5.0;
    PetscCall(PetscOptionsScalar("-D", "", "", ctx.D, &ctx.D, NULL));
    ctx.E        = 1.1378;
    ctx.V        = 1.0;
    ctx.X        = 0.545;
    ctx.Pmax     = ctx.E * ctx.V / ctx.X;
    ctx.Pmax_ini = ctx.Pmax;
    PetscCall(PetscOptionsScalar("-Pmax", "", "", ctx.Pmax, &ctx.Pmax, NULL));
    ctx.Pm = 0.9;
    PetscCall(PetscOptionsScalar("-Pm", "", "", ctx.Pm, &ctx.Pm, NULL));
    ctx.tf  = 1.0;
    ctx.tcl = 1.05;
    PetscCall(PetscOptionsReal("-tf", "Time to start fault", "", ctx.tf, &ctx.tf, NULL));
    PetscCall(PetscOptionsReal("-tcl", "Time to end fault", "", ctx.tcl, &ctx.tcl, NULL));
    PetscCall(PetscOptionsBool("-ensemble", "Run ensemble of different initial conditions", "", ensemble, &ensemble, NULL));
    if (ensemble) {
      ctx.tf  = -1;
      ctx.tcl = -1;
    }

    PetscCall(VecGetArray(U, &u));
    u[0] = PetscAsinScalar(ctx.Pm / ctx.Pmax);
    u[1] = 1.0;
    PetscCall(PetscOptionsRealArray("-u", "Initial solution", "", u, &n, &flg1));
    n = 2;
    PetscCall(PetscOptionsRealArray("-du", "Perturbation in initial solution", "", du, &n, &flg2));
    u[0] += du[0];
    u[1] += du[1];
    PetscCall(VecRestoreArray(U, &u));
    if (flg1 || flg2) {
      ctx.tf  = -1;
      ctx.tcl = -1;
    }
  }
  PetscOptionsEnd();

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSCreate(PETSC_COMM_WORLD, &ts));
  PetscCall(TSSetProblemType(ts, TS_NONLINEAR));
  PetscCall(TSSetType(ts, TSTHETA));
  PetscCall(TSSetEquationType(ts, TS_EQ_IMPLICIT));
  PetscCall(TSARKIMEXSetFullyImplicit(ts, PETSC_TRUE));
  PetscCall(TSSetIFunction(ts, NULL, (TSIFunction)IFunction, &ctx));
  PetscCall(TSSetIJacobian(ts, A, A, (TSIJacobian)IJacobian, &ctx));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set initial conditions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSetSolution(ts, U));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set solver options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSetMaxTime(ts, 35.0));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_MATCHSTEP));
  PetscCall(TSSetTimeStep(ts, .1));
  PetscCall(TSSetFromOptions(ts));

  direction[0] = direction[1] = 1;
  terminate[0] = terminate[1] = PETSC_FALSE;

  PetscCall(TSSetEventHandler(ts, 2, direction, terminate, EventFunction, PostEventFunction, (void *)&ctx));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  if (ensemble) {
    for (du[1] = -2.5; du[1] <= .01; du[1] += .1) {
      PetscCall(VecGetArray(U, &u));
      u[0] = PetscAsinScalar(ctx.Pm / ctx.Pmax);
      u[1] = ctx.omega_s;
      u[0] += du[0];
      u[1] += du[1];
      PetscCall(VecRestoreArray(U, &u));
      PetscCall(TSSetTimeStep(ts, .01));
      PetscCall(TSSolve(ts, U));
    }
  } else {
    PetscCall(TSSolve(ts, U));
  }
  PetscCall(VecView(U, PETSC_VIEWER_STDOUT_WORLD));
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they are no longer needed.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(MatDestroy(&A));
  PetscCall(VecDestroy(&U));
  PetscCall(TSDestroy(&ts));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   build:
     requires: !complex !single

   test:
      args: -nox

TEST*/

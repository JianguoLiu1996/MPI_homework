
static char help[] = "Solves a simple time-dependent linear PDE (the heat equation).\n\
Input parameters include:\n\
  -m <points>, where <points> = number of grid points\n\
  -time_dependent_rhs : Treat the problem as having a time-dependent right-hand side\n\
  -use_ifunc          : Use IFunction/IJacobian interface\n\
  -debug              : Activate debugging printouts\n\
  -nox                : Deactivate x-window graphics\n\n";

/* ------------------------------------------------------------------------

   This program solves the one-dimensional heat equation (also called the
   diffusion equation),
       u_t = u_xx,
   on the domain 0 <= x <= 1, with the boundary conditions
       u(t,0) = 0, u(t,1) = 0,
   and the initial condition
       u(0,x) = sin(6*pi*x) + 3*sin(2*pi*x).
   This is a linear, second-order, parabolic equation.

   We discretize the right-hand side using finite differences with
   uniform grid spacing h:
       u_xx = (u_{i+1} - 2u_{i} + u_{i-1})/(h^2)
   We then demonstrate time evolution using the various TS methods by
   running the program via
       ex3 -ts_type <timestepping solver>

   We compare the approximate solution with the exact solution, given by
       u_exact(x,t) = exp(-36*pi*pi*t) * sin(6*pi*x) +
                      3*exp(-4*pi*pi*t) * sin(2*pi*x)

   Notes:
   This code demonstrates the TS solver interface to two variants of
   linear problems, u_t = f(u,t), namely
     - time-dependent f:   f(u,t) is a function of t
     - time-independent f: f(u,t) is simply f(u)

    The parallel version of this code is ts/tutorials/ex4.c

  ------------------------------------------------------------------------- */

/*
   Include "petscts.h" so that we can use TS solvers.  Note that this file
   automatically includes:
     petscsys.h       - base PETSc routines   petscvec.h  - vectors
     petscmat.h  - matrices
     petscis.h     - index sets            petscksp.h  - Krylov subspace methods
     petscviewer.h - viewers               petscpc.h   - preconditioners
     petscksp.h   - linear solvers        petscsnes.h - nonlinear solvers
*/

#include <petscts.h>
#include <petscdraw.h>

/*
   User-defined application context - contains data needed by the
   application-provided call-back routines.
*/
typedef struct {
  Vec         solution;         /* global exact solution vector */
  PetscInt    m;                /* total number of grid points */
  PetscReal   h;                /* mesh width h = 1/(m-1) */
  PetscBool   debug;            /* flag (1 indicates activation of debugging printouts) */
  PetscViewer viewer1, viewer2; /* viewers for the solution and error */
  PetscReal   norm_2, norm_max; /* error norms */
  Mat         A;                /* RHS mat, used with IFunction interface */
  PetscReal   oshift;           /* old shift applied, prevent to recompute the IJacobian */
} AppCtx;

/*
   User-defined routines
*/
extern PetscErrorCode InitialConditions(Vec, AppCtx *);
extern PetscErrorCode RHSMatrixHeat(TS, PetscReal, Vec, Mat, Mat, void *);
extern PetscErrorCode IFunctionHeat(TS, PetscReal, Vec, Vec, Vec, void *);
extern PetscErrorCode IJacobianHeat(TS, PetscReal, Vec, Vec, PetscReal, Mat, Mat, void *);
extern PetscErrorCode Monitor(TS, PetscInt, PetscReal, Vec, void *);
extern PetscErrorCode ExactSolution(PetscReal, Vec, AppCtx *);

int main(int argc, char **argv)
{
  AppCtx      appctx;                 /* user-defined application context */
  TS          ts;                     /* timestepping context */
  Mat         A;                      /* matrix data structure */
  Vec         u;                      /* approximate solution vector */
  PetscReal   time_total_max = 100.0; /* default max total time */
  PetscInt    time_steps_max = 100;   /* default max timesteps */
  PetscDraw   draw;                   /* drawing context */
  PetscInt    steps, m;
  PetscMPIInt size;
  PetscReal   dt;
  PetscBool   flg, flg_string;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program and set problem parameters
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));
  PetscCheck(size == 1, PETSC_COMM_WORLD, PETSC_ERR_WRONG_MPI_SIZE, "This is a uniprocessor example only!");

  m = 60;
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-m", &m, NULL));
  PetscCall(PetscOptionsHasName(NULL, NULL, "-debug", &appctx.debug));
  flg_string = PETSC_FALSE;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-test_string_viewer", &flg_string, NULL));

  appctx.m        = m;
  appctx.h        = 1.0 / (m - 1.0);
  appctx.norm_2   = 0.0;
  appctx.norm_max = 0.0;

  PetscCall(PetscPrintf(PETSC_COMM_SELF, "Solving a linear TS problem on 1 processor\n"));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create vector data structures
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Create vector data structures for approximate and exact solutions
  */
  PetscCall(VecCreateSeq(PETSC_COMM_SELF, m, &u));
  PetscCall(VecDuplicate(u, &appctx.solution));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set up displays to show graphs of the solution and error
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(PetscViewerDrawOpen(PETSC_COMM_SELF, 0, "", 80, 380, 400, 160, &appctx.viewer1));
  PetscCall(PetscViewerDrawGetDraw(appctx.viewer1, 0, &draw));
  PetscCall(PetscDrawSetDoubleBuffer(draw));
  PetscCall(PetscViewerDrawOpen(PETSC_COMM_SELF, 0, "", 80, 0, 400, 160, &appctx.viewer2));
  PetscCall(PetscViewerDrawGetDraw(appctx.viewer2, 0, &draw));
  PetscCall(PetscDrawSetDoubleBuffer(draw));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(TSCreate(PETSC_COMM_SELF, &ts));
  PetscCall(TSSetProblemType(ts, TS_LINEAR));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set optional user-defined monitoring routine
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if (!flg_string) PetscCall(TSMonitorSet(ts, Monitor, &appctx, NULL));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

     Create matrix data structure; set matrix evaluation routine.
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(MatCreate(PETSC_COMM_SELF, &A));
  PetscCall(MatSetSizes(A, PETSC_DECIDE, PETSC_DECIDE, m, m));
  PetscCall(MatSetFromOptions(A));
  PetscCall(MatSetUp(A));

  flg = PETSC_FALSE;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-use_ifunc", &flg, NULL));
  if (!flg) {
    appctx.A = NULL;
    PetscCall(PetscOptionsGetBool(NULL, NULL, "-time_dependent_rhs", &flg, NULL));
    if (flg) {
      /*
         For linear problems with a time-dependent f(u,t) in the equation
         u_t = f(u,t), the user provides the discretized right-hand-side
         as a time-dependent matrix.
      */
      PetscCall(TSSetRHSFunction(ts, NULL, TSComputeRHSFunctionLinear, &appctx));
      PetscCall(TSSetRHSJacobian(ts, A, A, RHSMatrixHeat, &appctx));
    } else {
      /*
         For linear problems with a time-independent f(u) in the equation
         u_t = f(u), the user provides the discretized right-hand-side
         as a matrix only once, and then sets the special Jacobian evaluation
         routine TSComputeRHSJacobianConstant() which will NOT recompute the Jacobian.
      */
      PetscCall(RHSMatrixHeat(ts, 0.0, u, A, A, &appctx));
      PetscCall(TSSetRHSFunction(ts, NULL, TSComputeRHSFunctionLinear, &appctx));
      PetscCall(TSSetRHSJacobian(ts, A, A, TSComputeRHSJacobianConstant, &appctx));
    }
  } else {
    Mat J;

    PetscCall(RHSMatrixHeat(ts, 0.0, u, A, A, &appctx));
    PetscCall(MatDuplicate(A, MAT_DO_NOT_COPY_VALUES, &J));
    PetscCall(TSSetIFunction(ts, NULL, IFunctionHeat, &appctx));
    PetscCall(TSSetIJacobian(ts, J, J, IJacobianHeat, &appctx));
    PetscCall(MatDestroy(&J));

    PetscCall(PetscObjectReference((PetscObject)A));
    appctx.A      = A;
    appctx.oshift = PETSC_MIN_REAL;
  }
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set solution vector and initial timestep
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  dt = appctx.h * appctx.h / 2.0;
  PetscCall(TSSetTimeStep(ts, dt));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Customize timestepping solver:
       - Set the solution method to be the Backward Euler method.
       - Set timestepping duration info
     Then set runtime options, which can override these defaults.
     For example,
          -ts_max_steps <maxsteps> -ts_max_time <maxtime>
     to override the defaults set by TSSetMaxSteps()/TSSetMaxTime().
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(TSSetMaxSteps(ts, time_steps_max));
  PetscCall(TSSetMaxTime(ts, time_total_max));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_STEPOVER));
  PetscCall(TSSetFromOptions(ts));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve the problem
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Evaluate initial conditions
  */
  PetscCall(InitialConditions(u, &appctx));

  /*
     Run the timestepping solver
  */
  PetscCall(TSSolve(ts, u));
  PetscCall(TSGetStepNumber(ts, &steps));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     View timestepping solver info
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(PetscPrintf(PETSC_COMM_SELF, "avg. error (2 norm) = %g, avg. error (max norm) = %g\n", (double)(appctx.norm_2 / steps), (double)(appctx.norm_max / steps)));
  if (!flg_string) {
    PetscCall(TSView(ts, PETSC_VIEWER_STDOUT_SELF));
  } else {
    PetscViewer stringviewer;
    char        string[512];
    const char *outstring;

    PetscCall(PetscViewerStringOpen(PETSC_COMM_WORLD, string, sizeof(string), &stringviewer));
    PetscCall(TSView(ts, stringviewer));
    PetscCall(PetscViewerStringGetStringRead(stringviewer, &outstring, NULL));
    PetscCheck((char *)outstring == (char *)string, PETSC_COMM_WORLD, PETSC_ERR_PLIB, "String returned from viewer does not equal original string");
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Output from string viewer:%s\n", outstring));
    PetscCall(PetscViewerDestroy(&stringviewer));
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscCall(TSDestroy(&ts));
  PetscCall(MatDestroy(&A));
  PetscCall(VecDestroy(&u));
  PetscCall(PetscViewerDestroy(&appctx.viewer1));
  PetscCall(PetscViewerDestroy(&appctx.viewer2));
  PetscCall(VecDestroy(&appctx.solution));
  PetscCall(MatDestroy(&appctx.A));

  /*
     Always call PetscFinalize() before exiting a program.  This routine
       - finalizes the PETSc libraries as well as MPI
       - provides summary and diagnostic information if certain runtime
         options are chosen (e.g., -log_view).
  */
  PetscCall(PetscFinalize());
  return 0;
}
/* --------------------------------------------------------------------- */
/*
   InitialConditions - Computes the solution at the initial time.

   Input Parameter:
   u - uninitialized solution vector (global)
   appctx - user-defined application context

   Output Parameter:
   u - vector with solution at initial time (global)
*/
PetscErrorCode InitialConditions(Vec u, AppCtx *appctx)
{
  PetscScalar *u_localptr, h = appctx->h;
  PetscInt     i;

  PetscFunctionBeginUser;
  /*
    Get a pointer to vector data.
    - For default PETSc vectors, VecGetArray() returns a pointer to
      the data array.  Otherwise, the routine is implementation dependent.
    - You MUST call VecRestoreArray() when you no longer need access to
      the array.
    - Note that the Fortran interface to VecGetArray() differs from the
      C version.  See the users manual for details.
  */
  PetscCall(VecGetArrayWrite(u, &u_localptr));

  /*
     We initialize the solution array by simply writing the solution
     directly into the array locations.  Alternatively, we could use
     VecSetValues() or VecSetValuesLocal().
  */
  for (i = 0; i < appctx->m; i++) u_localptr[i] = PetscSinScalar(PETSC_PI * i * 6. * h) + 3. * PetscSinScalar(PETSC_PI * i * 2. * h);

  /*
     Restore vector
  */
  PetscCall(VecRestoreArrayWrite(u, &u_localptr));

  /*
     Print debugging information if desired
  */
  if (appctx->debug) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Initial guess vector\n"));
    PetscCall(VecView(u, PETSC_VIEWER_STDOUT_SELF));
  }

  PetscFunctionReturn(PETSC_SUCCESS);
}
/* --------------------------------------------------------------------- */
/*
   ExactSolution - Computes the exact solution at a given time.

   Input Parameters:
   t - current time
   solution - vector in which exact solution will be computed
   appctx - user-defined application context

   Output Parameter:
   solution - vector with the newly computed exact solution
*/
PetscErrorCode ExactSolution(PetscReal t, Vec solution, AppCtx *appctx)
{
  PetscScalar *s_localptr, h = appctx->h, ex1, ex2, sc1, sc2, tc = t;
  PetscInt     i;

  PetscFunctionBeginUser;
  /*
     Get a pointer to vector data.
  */
  PetscCall(VecGetArrayWrite(solution, &s_localptr));

  /*
     Simply write the solution directly into the array locations.
     Alternatively, we culd use VecSetValues() or VecSetValuesLocal().
  */
  ex1 = PetscExpScalar(-36. * PETSC_PI * PETSC_PI * tc);
  ex2 = PetscExpScalar(-4. * PETSC_PI * PETSC_PI * tc);
  sc1 = PETSC_PI * 6. * h;
  sc2 = PETSC_PI * 2. * h;
  for (i = 0; i < appctx->m; i++) s_localptr[i] = PetscSinScalar(sc1 * (PetscReal)i) * ex1 + 3. * PetscSinScalar(sc2 * (PetscReal)i) * ex2;

  /*
     Restore vector
  */
  PetscCall(VecRestoreArrayWrite(solution, &s_localptr));
  PetscFunctionReturn(PETSC_SUCCESS);
}
/* --------------------------------------------------------------------- */
/*
   Monitor - User-provided routine to monitor the solution computed at
   each timestep.  This example plots the solution and computes the
   error in two different norms.

   This example also demonstrates changing the timestep via TSSetTimeStep().

   Input Parameters:
   ts     - the timestep context
   step   - the count of the current step (with 0 meaning the
             initial condition)
   time   - the current time
   u      - the solution at this timestep
   ctx    - the user-provided context for this monitoring routine.
            In this case we use the application context which contains
            information about the problem size, workspace and the exact
            solution.
*/
PetscErrorCode Monitor(TS ts, PetscInt step, PetscReal time, Vec u, void *ctx)
{
  AppCtx   *appctx = (AppCtx *)ctx; /* user-defined application context */
  PetscReal norm_2, norm_max, dt, dttol;

  PetscFunctionBeginUser;
  /*
     View a graph of the current iterate
  */
  PetscCall(VecView(u, appctx->viewer2));

  /*
     Compute the exact solution
  */
  PetscCall(ExactSolution(time, appctx->solution, appctx));

  /*
     Print debugging information if desired
  */
  if (appctx->debug) {
    PetscCall(PetscPrintf(PETSC_COMM_SELF, "Computed solution vector\n"));
    PetscCall(VecView(u, PETSC_VIEWER_STDOUT_SELF));
    PetscCall(PetscPrintf(PETSC_COMM_SELF, "Exact solution vector\n"));
    PetscCall(VecView(appctx->solution, PETSC_VIEWER_STDOUT_SELF));
  }

  /*
     Compute the 2-norm and max-norm of the error
  */
  PetscCall(VecAXPY(appctx->solution, -1.0, u));
  PetscCall(VecNorm(appctx->solution, NORM_2, &norm_2));
  norm_2 = PetscSqrtReal(appctx->h) * norm_2;
  PetscCall(VecNorm(appctx->solution, NORM_MAX, &norm_max));

  PetscCall(TSGetTimeStep(ts, &dt));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Timestep %3" PetscInt_FMT ": step size = %g, time = %g, 2-norm error = %g, max norm error = %g\n", step, (double)dt, (double)time, (double)norm_2, (double)norm_max));

  appctx->norm_2 += norm_2;
  appctx->norm_max += norm_max;

  dttol = .0001;
  PetscCall(PetscOptionsGetReal(NULL, NULL, "-dttol", &dttol, NULL));
  if (dt < dttol) {
    dt *= .999;
    PetscCall(TSSetTimeStep(ts, dt));
  }

  /*
     View a graph of the error
  */
  PetscCall(VecView(appctx->solution, appctx->viewer1));

  /*
     Print debugging information if desired
  */
  if (appctx->debug) {
    PetscCall(PetscPrintf(PETSC_COMM_SELF, "Error vector\n"));
    PetscCall(VecView(appctx->solution, PETSC_VIEWER_STDOUT_SELF));
  }

  PetscFunctionReturn(PETSC_SUCCESS);
}
/* --------------------------------------------------------------------- */
/*
   RHSMatrixHeat - User-provided routine to compute the right-hand-side
   matrix for the heat equation.

   Input Parameters:
   ts - the TS context
   t - current time
   global_in - global input vector
   dummy - optional user-defined context, as set by TSetRHSJacobian()

   Output Parameters:
   AA - Jacobian matrix
   BB - optionally different preconditioning matrix
   str - flag indicating matrix structure

   Notes:
   Recall that MatSetValues() uses 0-based row and column numbers
   in Fortran as well as in C.
*/
PetscErrorCode RHSMatrixHeat(TS ts, PetscReal t, Vec X, Mat AA, Mat BB, void *ctx)
{
  Mat         A      = AA;            /* Jacobian matrix */
  AppCtx     *appctx = (AppCtx *)ctx; /* user-defined application context */
  PetscInt    mstart = 0;
  PetscInt    mend   = appctx->m;
  PetscInt    i, idx[3];
  PetscScalar v[3], stwo = -2. / (appctx->h * appctx->h), sone = -.5 * stwo;

  PetscFunctionBeginUser;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Compute entries for the locally owned part of the matrix
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Set matrix rows corresponding to boundary data
  */

  mstart = 0;
  v[0]   = 1.0;
  PetscCall(MatSetValues(A, 1, &mstart, 1, &mstart, v, INSERT_VALUES));
  mstart++;

  mend--;
  v[0] = 1.0;
  PetscCall(MatSetValues(A, 1, &mend, 1, &mend, v, INSERT_VALUES));

  /*
     Set matrix rows corresponding to interior data.  We construct the
     matrix one row at a time.
  */
  v[0] = sone;
  v[1] = stwo;
  v[2] = sone;
  for (i = mstart; i < mend; i++) {
    idx[0] = i - 1;
    idx[1] = i;
    idx[2] = i + 1;
    PetscCall(MatSetValues(A, 1, &i, 3, idx, v, INSERT_VALUES));
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Complete the matrix assembly process and set some options
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /*
     Assemble matrix, using the 2-step process:
       MatAssemblyBegin(), MatAssemblyEnd()
     Computations can be done while messages are in transition
     by placing code between these two statements.
  */
  PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));

  /*
     Set and option to indicate that we will never add a new nonzero location
     to the matrix. If we do, it will generate an error.
  */
  PetscCall(MatSetOption(A, MAT_NEW_NONZERO_LOCATION_ERR, PETSC_TRUE));

  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode IFunctionHeat(TS ts, PetscReal t, Vec X, Vec Xdot, Vec r, void *ctx)
{
  AppCtx *appctx = (AppCtx *)ctx; /* user-defined application context */

  PetscFunctionBeginUser;
  PetscCall(MatMult(appctx->A, X, r));
  PetscCall(VecAYPX(r, -1.0, Xdot));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode IJacobianHeat(TS ts, PetscReal t, Vec X, Vec Xdot, PetscReal s, Mat A, Mat B, void *ctx)
{
  AppCtx *appctx = (AppCtx *)ctx; /* user-defined application context */

  PetscFunctionBeginUser;
  if (appctx->oshift == s) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(MatCopy(appctx->A, A, SAME_NONZERO_PATTERN));
  PetscCall(MatScale(A, -1));
  PetscCall(MatShift(A, s));
  PetscCall(MatCopy(A, B, SAME_NONZERO_PATTERN));
  appctx->oshift = s;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*TEST

    test:
      args: -nox -ts_type ssp -ts_dt 0.0005

    test:
      suffix: 2
      args: -nox -ts_type ssp -ts_dt 0.0005 -time_dependent_rhs 1

    test:
      suffix: 3
      args:  -nox -ts_type rosw -ts_max_steps 3 -ksp_converged_reason
      filter: sed "s/ATOL/RTOL/g"
      requires: !single

    test:
      suffix: 4
      args: -nox -ts_type beuler -ts_max_steps 3 -ksp_converged_reason
      filter: sed "s/ATOL/RTOL/g"

    test:
      suffix: 5
      args: -nox -ts_type beuler -ts_max_steps 3 -ksp_converged_reason -time_dependent_rhs
      filter: sed "s/ATOL/RTOL/g"

    test:
      requires: !single
      suffix: pod_guess
      args: -nox -ts_type beuler -use_ifunc -ts_dt 0.0005 -ksp_guess_type pod -pc_type none -ksp_converged_reason

    test:
      requires: !single
      suffix: pod_guess_Ainner
      args: -nox -ts_type beuler -use_ifunc -ts_dt 0.0005 -ksp_guess_type pod -ksp_guess_pod_Ainner -pc_type none -ksp_converged_reason

    test:
      requires: !single
      suffix: fischer_guess
      args: -nox -ts_type beuler -use_ifunc -ts_dt 0.0005 -ksp_guess_type fischer -pc_type none -ksp_converged_reason

    test:
      requires: !single
      suffix: fischer_guess_2
      args: -nox -ts_type beuler -use_ifunc -ts_dt 0.0005 -ksp_guess_type fischer -ksp_guess_fischer_model 2,10 -pc_type none -ksp_converged_reason

    test:
      requires: !single
      suffix: fischer_guess_3
      args: -nox -ts_type beuler -use_ifunc -ts_dt 0.0005 -ksp_guess_type fischer -ksp_guess_fischer_model 3,10 -pc_type none -ksp_converged_reason

    test:
      requires: !single
      suffix: stringview
      args: -nox -ts_type rosw -test_string_viewer

    test:
      requires: !single
      suffix: stringview_euler
      args: -nox -ts_type euler -test_string_viewer

TEST*/

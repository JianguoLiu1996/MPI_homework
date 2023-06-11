static const char help[] = "Time-dependent advection-reaction PDE in 1d, demonstrates IMEX methods.\n";
/*
   u_t + a1*u_x = -k1*u + k2*v + s1
   v_t + a2*v_x = k1*u - k2*v + s2
   0 < x < 1;
   a1 = 1, k1 = 10^6, s1 = 0,
   a2 = 0, k2 = 2*k1, s2 = 1

   Initial conditions:
   u(x,0) = 1 + s2*x
   v(x,0) = k0/k1*u(x,0) + s1/k1

   Upstream boundary conditions:
   u(0,t) = 1-sin(12*t)^4

   Useful command-line parameters:
   -ts_arkimex_fully_implicit - treats advection implicitly, only relevant with -ts_type arkimex (default)
   -ts_type rosw - use Rosenbrock-W method (linearly implicit IMEX, amortizes assembly and preconditioner setup)
*/

#include <petscdm.h>
#include <petscdmda.h>
#include <petscts.h>

typedef PetscScalar Field[2];

typedef struct _User *User;
struct _User {
  PetscReal a[2]; /* Advection speeds */
  PetscReal k[2]; /* Reaction coefficients */
  PetscReal s[2]; /* Source terms */
};

static PetscErrorCode FormRHSFunction(TS, PetscReal, Vec, Vec, void *);
static PetscErrorCode FormIFunction(TS, PetscReal, Vec, Vec, Vec, void *);
static PetscErrorCode FormIJacobian(TS, PetscReal, Vec, Vec, PetscReal, Mat, Mat, void *);
static PetscErrorCode FormInitialSolution(TS, Vec, void *);

int main(int argc, char **argv)
{
  TS                ts;         /* time integrator */
  SNES              snes;       /* nonlinear solver */
  SNESLineSearch    linesearch; /* line search */
  Vec               X;          /* solution, residual vectors */
  Mat               J;          /* Jacobian matrix */
  PetscInt          steps, mx;
  DM                da;
  PetscReal         ftime, dt;
  struct _User      user; /* user-defined work context */
  TSConvergedReason reason;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create distributed array (DMDA) to manage parallel grid and vectors
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(DMDACreate1d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, 11, 2, 2, NULL, &da));
  PetscCall(DMSetFromOptions(da));
  PetscCall(DMSetUp(da));

  /*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Extract global vectors from DMDA;
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(DMCreateGlobalVector(da, &X));

  /* Initialize user application context */
  PetscOptionsBegin(PETSC_COMM_WORLD, NULL, "Advection-reaction options", "");
  {
    user.a[0] = 1;
    PetscCall(PetscOptionsReal("-a0", "Advection rate 0", "", user.a[0], &user.a[0], NULL));
    user.a[1] = 0;
    PetscCall(PetscOptionsReal("-a1", "Advection rate 1", "", user.a[1], &user.a[1], NULL));
    user.k[0] = 1e6;
    PetscCall(PetscOptionsReal("-k0", "Reaction rate 0", "", user.k[0], &user.k[0], NULL));
    user.k[1] = 2 * user.k[0];
    PetscCall(PetscOptionsReal("-k1", "Reaction rate 1", "", user.k[1], &user.k[1], NULL));
    user.s[0] = 0;
    PetscCall(PetscOptionsReal("-s0", "Source 0", "", user.s[0], &user.s[0], NULL));
    user.s[1] = 1;
    PetscCall(PetscOptionsReal("-s1", "Source 1", "", user.s[1], &user.s[1], NULL));
  }
  PetscOptionsEnd();

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSCreate(PETSC_COMM_WORLD, &ts));
  PetscCall(TSSetDM(ts, da));
  PetscCall(TSSetType(ts, TSARKIMEX));
  PetscCall(TSSetRHSFunction(ts, NULL, FormRHSFunction, &user));
  PetscCall(TSSetIFunction(ts, NULL, FormIFunction, &user));
  PetscCall(DMSetMatType(da, MATAIJ));
  PetscCall(DMCreateMatrix(da, &J));
  PetscCall(TSSetIJacobian(ts, J, J, FormIJacobian, &user));

  /* A line search in the nonlinear solve can fail due to ill-conditioning unless an absolute tolerance is set. Since
   * this problem is linear, we deactivate the line search. For a linear problem, it is usually recommended to also use
   * SNESSetType(snes,SNESKSPONLY). */
  PetscCall(TSGetSNES(ts, &snes));
  PetscCall(SNESGetLineSearch(snes, &linesearch));
  PetscCall(SNESLineSearchSetType(linesearch, SNESLINESEARCHBASIC));

  ftime = .1;
  PetscCall(TSSetMaxTime(ts, ftime));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_STEPOVER));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set initial conditions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(FormInitialSolution(ts, X, &user));
  PetscCall(TSSetSolution(ts, X));
  PetscCall(VecGetSize(X, &mx));
  dt = .1 * PetscMax(user.a[0], user.a[1]) / mx; /* Advective CFL, I don't know why it needs so much safety factor. */
  PetscCall(TSSetTimeStep(ts, dt));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set runtime options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSetFromOptions(ts));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSolve(ts, X));
  PetscCall(TSGetSolveTime(ts, &ftime));
  PetscCall(TSGetStepNumber(ts, &steps));
  PetscCall(TSGetConvergedReason(ts, &reason));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%s at time %g after %" PetscInt_FMT " steps\n", TSConvergedReasons[reason], (double)ftime, steps));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(MatDestroy(&J));
  PetscCall(VecDestroy(&X));
  PetscCall(TSDestroy(&ts));
  PetscCall(DMDestroy(&da));
  PetscCall(PetscFinalize());
  return 0;
}

static PetscErrorCode FormIFunction(TS ts, PetscReal t, Vec X, Vec Xdot, Vec F, void *ptr)
{
  User          user = (User)ptr;
  DM            da;
  DMDALocalInfo info;
  PetscInt      i;
  Field        *f;
  const Field  *x, *xdot;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &da));
  PetscCall(DMDAGetLocalInfo(da, &info));

  /* Get pointers to vector data */
  PetscCall(DMDAVecGetArrayRead(da, X, (void *)&x));
  PetscCall(DMDAVecGetArrayRead(da, Xdot, (void *)&xdot));
  PetscCall(DMDAVecGetArray(da, F, &f));

  /* Compute function over the locally owned part of the grid */
  for (i = info.xs; i < info.xs + info.xm; i++) {
    f[i][0] = xdot[i][0] + user->k[0] * x[i][0] - user->k[1] * x[i][1] - user->s[0];
    f[i][1] = xdot[i][1] - user->k[0] * x[i][0] + user->k[1] * x[i][1] - user->s[1];
  }

  /* Restore vectors */
  PetscCall(DMDAVecRestoreArrayRead(da, X, (void *)&x));
  PetscCall(DMDAVecRestoreArrayRead(da, Xdot, (void *)&xdot));
  PetscCall(DMDAVecRestoreArray(da, F, &f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode FormRHSFunction(TS ts, PetscReal t, Vec X, Vec F, void *ptr)
{
  User          user = (User)ptr;
  DM            da;
  Vec           Xloc;
  DMDALocalInfo info;
  PetscInt      i, j;
  PetscReal     hx;
  Field        *f;
  const Field  *x;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &da));
  PetscCall(DMDAGetLocalInfo(da, &info));
  hx = 1.0 / (PetscReal)info.mx;

  /*
     Scatter ghost points to local vector,using the 2-step process
        DMGlobalToLocalBegin(),DMGlobalToLocalEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  PetscCall(DMGetLocalVector(da, &Xloc));
  PetscCall(DMGlobalToLocalBegin(da, X, INSERT_VALUES, Xloc));
  PetscCall(DMGlobalToLocalEnd(da, X, INSERT_VALUES, Xloc));

  /* Get pointers to vector data */
  PetscCall(DMDAVecGetArrayRead(da, Xloc, (void *)&x));
  PetscCall(DMDAVecGetArray(da, F, &f));

  /* Compute function over the locally owned part of the grid */
  for (i = info.xs; i < info.xs + info.xm; i++) {
    const PetscReal *a = user->a;
    PetscReal        u0t[2];
    u0t[0] = 1.0 - PetscPowRealInt(PetscSinReal(12 * t), 4);
    u0t[1] = 0.0;
    for (j = 0; j < 2; j++) {
      if (i == 0) f[i][j] = a[j] / hx * (1. / 3 * u0t[j] + 0.5 * x[i][j] - x[i + 1][j] + 1. / 6 * x[i + 2][j]);
      else if (i == 1) f[i][j] = a[j] / hx * (-1. / 12 * u0t[j] + 2. / 3 * x[i - 1][j] - 2. / 3 * x[i + 1][j] + 1. / 12 * x[i + 2][j]);
      else if (i == info.mx - 2) f[i][j] = a[j] / hx * (-1. / 6 * x[i - 2][j] + x[i - 1][j] - 0.5 * x[i][j] - 1. / 3 * x[i + 1][j]);
      else if (i == info.mx - 1) f[i][j] = a[j] / hx * (-x[i][j] + x[i - 1][j]);
      else f[i][j] = a[j] / hx * (-1. / 12 * x[i - 2][j] + 2. / 3 * x[i - 1][j] - 2. / 3 * x[i + 1][j] + 1. / 12 * x[i + 2][j]);
    }
  }

  /* Restore vectors */
  PetscCall(DMDAVecRestoreArrayRead(da, Xloc, (void *)&x));
  PetscCall(DMDAVecRestoreArray(da, F, &f));
  PetscCall(DMRestoreLocalVector(da, &Xloc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* --------------------------------------------------------------------- */
/*
  IJacobian - Compute IJacobian = dF/dU + a dF/dUdot
*/
PetscErrorCode FormIJacobian(TS ts, PetscReal t, Vec X, Vec Xdot, PetscReal a, Mat J, Mat Jpre, void *ptr)
{
  User          user = (User)ptr;
  DMDALocalInfo info;
  PetscInt      i;
  DM            da;
  const Field  *x, *xdot;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &da));
  PetscCall(DMDAGetLocalInfo(da, &info));

  /* Get pointers to vector data */
  PetscCall(DMDAVecGetArrayRead(da, X, (void *)&x));
  PetscCall(DMDAVecGetArrayRead(da, Xdot, (void *)&xdot));

  /* Compute function over the locally owned part of the grid */
  for (i = info.xs; i < info.xs + info.xm; i++) {
    const PetscReal *k = user->k;
    PetscScalar      v[2][2];

    v[0][0] = a + k[0];
    v[0][1] = -k[1];
    v[1][0] = -k[0];
    v[1][1] = a + k[1];
    PetscCall(MatSetValuesBlocked(Jpre, 1, &i, 1, &i, &v[0][0], INSERT_VALUES));
  }

  /* Restore vectors */
  PetscCall(DMDAVecRestoreArrayRead(da, X, (void *)&x));
  PetscCall(DMDAVecRestoreArrayRead(da, Xdot, (void *)&xdot));

  PetscCall(MatAssemblyBegin(Jpre, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(Jpre, MAT_FINAL_ASSEMBLY));
  if (J != Jpre) {
    PetscCall(MatAssemblyBegin(J, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(J, MAT_FINAL_ASSEMBLY));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode FormInitialSolution(TS ts, Vec X, void *ctx)
{
  User          user = (User)ctx;
  DM            da;
  PetscInt      i;
  DMDALocalInfo info;
  Field        *x;
  PetscReal     hx;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &da));
  PetscCall(DMDAGetLocalInfo(da, &info));
  hx = 1.0 / (PetscReal)info.mx;

  /* Get pointers to vector data */
  PetscCall(DMDAVecGetArray(da, X, &x));

  /* Compute function over the locally owned part of the grid */
  for (i = info.xs; i < info.xs + info.xm; i++) {
    PetscReal r = (i + 1) * hx, ik = user->k[1] != 0.0 ? 1.0 / user->k[1] : 1.0;
    x[i][0] = 1 + user->s[1] * r;
    x[i][1] = user->k[0] * ik * x[i][0] + user->s[1] * ik;
  }
  PetscCall(DMDAVecRestoreArray(da, X, &x));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*TEST

    test:
      args: -nox -da_grid_x 200 -ts_monitor_draw_solution -ts_arkimex_type 4 -ts_adapt_type none -ts_dt .005 -ts_max_time .1
      requires: !single

    test:
      suffix: 2
      args: -nox -da_grid_x 200 -ts_monitor_draw_solution -ts_type rosw -ts_dt 1e-3 -ts_adapt_type none -ts_dt .005 -ts_max_time .1
      nsize: 2

    test:
      suffix: 3
      args: -nox -da_grid_x 200 -ts_monitor_draw_solution -ts_type rosw -ts_rosw_type ra34pw2 -ts_dt 5e-3 -ts_max_time .1  -ts_adapt_type none
      nsize: 2

    test:
      suffix: 4
      args: -ts_type eimex -da_grid_x 200 -ts_eimex_order_adapt true -ts_dt 0.001 -ts_monitor -ts_max_steps 100
      filter: sed "s/ITS/TIME/g"
      nsize: 2

TEST*/

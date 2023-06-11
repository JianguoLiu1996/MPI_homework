
static char help[] = "Solves the van der Pol DAE.\n\
Input parameters include:\n";

/* ------------------------------------------------------------------------

   This program solves the van der Pol DAE
       y' = -z = f(y,z)        (1)
       0  = y-(z^3/3 - z) = g(y,z)
   on the domain 0 <= x <= 1, with the boundary conditions
       y(0) = -2, y'(0) = -2.355301397608119909925287735864250951918
   This is a nonlinear equation.

   Notes:
   This code demonstrates the TS solver interface with the Van der Pol DAE,
   namely it is the case when there is no RHS (meaning the RHS == 0), and the
   equations are converted to two variants of linear problems, u_t = f(u,t),
   namely turning (1) into a vector equation in terms of u,

   [     y' + z      ] = [ 0 ]
   [ (z^3/3 - z) - y ]   [ 0 ]

   which then we can write as a vector equation

   [      u_1' + u_2       ] = [ 0 ]  (2)
   [ (u_2^3/3 - u_2) - u_1 ]   [ 0 ]

   which is now in the desired form of u_t = f(u,t). As this is a DAE, and
   there is no u_2', there is no need for a split,

   so

   [ F(u',u,t) ] = [ u_1' ] + [         u_2           ]
                   [  0   ]   [ (u_2^3/3 - u_2) - u_1 ]

   Using the definition of the Jacobian of F (from the PETSc user manual),
   in the equation F(u',u,t) = G(u,t),

              dF   dF
   J(F) = a * -- - --
              du'  du

   where d is the partial derivative. In this example,

   dF   [ 1 ; 0 ]
   -- = [       ]
   du'  [ 0 ; 0 ]

   dF   [  0 ;      1     ]
   -- = [                 ]
   du   [ -1 ; 1 - u_2^2  ]

   Hence,

          [ a ;    -1     ]
   J(F) = [               ]
          [ 1 ; u_2^2 - 1 ]

  ------------------------------------------------------------------------- */

#include <petscts.h>

typedef struct _n_User *User;
struct _n_User {
  PetscReal next_output;
};

/*
   User-defined routines
*/

static PetscErrorCode IFunction(TS ts, PetscReal t, Vec X, Vec Xdot, Vec F, void *ctx)
{
  PetscScalar       *f;
  const PetscScalar *x, *xdot;

  PetscFunctionBeginUser;
  PetscCall(VecGetArrayRead(X, &x));
  PetscCall(VecGetArrayRead(Xdot, &xdot));
  PetscCall(VecGetArray(F, &f));
  f[0] = xdot[0] + x[1];
  f[1] = (x[1] * x[1] * x[1] / 3.0 - x[1]) - x[0];
  PetscCall(VecRestoreArrayRead(X, &x));
  PetscCall(VecRestoreArrayRead(Xdot, &xdot));
  PetscCall(VecRestoreArray(F, &f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode IJacobian(TS ts, PetscReal t, Vec X, Vec Xdot, PetscReal a, Mat A, Mat B, void *ctx)
{
  PetscInt           rowcol[] = {0, 1};
  PetscScalar        J[2][2];
  const PetscScalar *x;

  PetscFunctionBeginUser;
  PetscCall(VecGetArrayRead(X, &x));
  J[0][0] = a;
  J[0][1] = -1.;
  J[1][0] = 1.;
  J[1][1] = -1. + x[1] * x[1];
  PetscCall(MatSetValues(B, 2, rowcol, 2, rowcol, &J[0][0], INSERT_VALUES));
  PetscCall(VecRestoreArrayRead(X, &x));

  PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
  if (A != B) {
    PetscCall(MatAssemblyBegin(B, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(B, MAT_FINAL_ASSEMBLY));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode RegisterMyARK2(void)
{
  PetscFunctionBeginUser;
  {
    const PetscReal A[3][3] =
      {
        {0,                      0,    0},
        {0.41421356237309504880, 0,    0},
        {0.75,                   0.25, 0}
    },
                    At[3][3] = {{0, 0, 0}, {0.12132034355964257320, 0.29289321881345247560, 0}, {0.20710678118654752440, 0.50000000000000000000, 0.29289321881345247560}}, *bembedt = NULL, *bembed = NULL;
    PetscCall(TSARKIMEXRegister("myark2", 2, 3, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembed, 0, NULL, NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Monitor timesteps and use interpolation to output at integer multiples of 0.1 */
static PetscErrorCode Monitor(TS ts, PetscInt step, PetscReal t, Vec X, void *ctx)
{
  const PetscScalar *x;
  PetscReal          tfinal, dt;
  User               user = (User)ctx;
  Vec                interpolatedX;

  PetscFunctionBeginUser;
  PetscCall(TSGetTimeStep(ts, &dt));
  PetscCall(TSGetMaxTime(ts, &tfinal));

  while (user->next_output <= t && user->next_output <= tfinal) {
    PetscCall(VecDuplicate(X, &interpolatedX));
    PetscCall(TSInterpolate(ts, user->next_output, interpolatedX));
    PetscCall(VecGetArrayRead(interpolatedX, &x));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "[%.1f] %3" PetscInt_FMT " TS %.6f (dt = %.6f) X % 12.6e % 12.6e\n", (double)user->next_output, step, (double)t, (double)dt, (double)PetscRealPart(x[0]), (double)PetscRealPart(x[1])));
    PetscCall(VecRestoreArrayRead(interpolatedX, &x));
    PetscCall(VecDestroy(&interpolatedX));
    user->next_output += PetscRealConstant(0.1);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  TS             ts; /* nonlinear solver */
  Vec            x;  /* solution, residual vectors */
  Mat            A;  /* Jacobian matrix */
  PetscInt       steps;
  PetscReal      ftime   = 0.5;
  PetscBool      monitor = PETSC_FALSE;
  PetscScalar   *x_ptr;
  PetscMPIInt    size;
  struct _n_User user;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));
  PetscCheck(size == 1, PETSC_COMM_WORLD, PETSC_ERR_WRONG_MPI_SIZE, "This is a uniprocessor example only!");

  PetscCall(RegisterMyARK2());

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    Set runtime options
    - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  user.next_output = 0.0;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-monitor", &monitor, NULL));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    Create necessary matrix and vectors, solve same ODE on every process
    - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(MatCreate(PETSC_COMM_WORLD, &A));
  PetscCall(MatSetSizes(A, PETSC_DECIDE, PETSC_DECIDE, 2, 2));
  PetscCall(MatSetFromOptions(A));
  PetscCall(MatSetUp(A));
  PetscCall(MatCreateVecs(A, &x, NULL));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSCreate(PETSC_COMM_WORLD, &ts));
  PetscCall(TSSetType(ts, TSBEULER));
  PetscCall(TSSetIFunction(ts, NULL, IFunction, &user));
  PetscCall(TSSetIJacobian(ts, A, A, IJacobian, &user));
  PetscCall(TSSetMaxTime(ts, ftime));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_STEPOVER));
  if (monitor) PetscCall(TSMonitorSet(ts, Monitor, &user, NULL));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set initial conditions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(VecGetArray(x, &x_ptr));
  x_ptr[0] = -2;
  x_ptr[1] = -2.355301397608119909925287735864250951918;
  PetscCall(VecRestoreArray(x, &x_ptr));
  PetscCall(TSSetTimeStep(ts, .001));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set runtime options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSetFromOptions(ts));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSolve(ts, x));
  PetscCall(TSGetSolveTime(ts, &ftime));
  PetscCall(TSGetStepNumber(ts, &steps));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "steps %3" PetscInt_FMT ", ftime %g\n", steps, (double)ftime));
  PetscCall(VecView(x, PETSC_VIEWER_STDOUT_WORLD));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(MatDestroy(&A));
  PetscCall(VecDestroy(&x));
  PetscCall(TSDestroy(&ts));

  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   test:
      requires: !single
      suffix: a
      args: -monitor -ts_type bdf -ts_rtol 1e-6 -ts_atol 1e-6 -ts_view -ts_adapt_type dsp
      output_file: output/ex19_pi42.out

   test:
      requires: !single
      suffix: b
      args: -monitor -ts_type bdf -ts_rtol 1e-6 -ts_atol 1e-6 -ts_view -ts_adapt_type dsp -ts_adapt_dsp_filter PI42
      output_file: output/ex19_pi42.out

   test:
      requires: !single
      suffix: c
      args: -monitor -ts_type bdf -ts_rtol 1e-6 -ts_atol 1e-6 -ts_view -ts_adapt_type dsp -ts_adapt_dsp_pid 0.4,0.2
      output_file: output/ex19_pi42.out

   test:
      requires: !single
      suffix: bdf_reject
      args: -ts_type bdf -ts_dt 0.5 -ts_max_steps 1 -ts_max_reject {{0 1 2}separate_output} -ts_error_if_step_fails false -ts_adapt_monitor

TEST*/

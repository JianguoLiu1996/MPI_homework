static char help[] = "Demonstrates adjoint sensitivity analysis for Reaction-Diffusion Equations.\n";

/*
  See ex5.c for details on the equation.
  This code demonestrates the TSAdjoint interface to a system of time-dependent partial differential equations.
  It computes the sensitivity of a component in the final solution, which locates in the center of the 2D domain, w.r.t. the initial conditions.
  The user does not need to provide any additional functions. The required functions in the original simulation are reused in the adjoint run.

  Runtime options:
    -forwardonly  - run the forward simulation without adjoint
    -implicitform - provide IFunction and IJacobian to TS, if not set, RHSFunction and RHSJacobian will be used
    -aijpc        - set the preconditioner matrix to be aij (the Jacobian matrix can be of a different type such as ELL)
*/
#include "reaction_diffusion.h"
#include <petscdm.h>
#include <petscdmda.h>

PetscErrorCode InitialConditions(DM, Vec);

PetscErrorCode InitializeLambda(DM da, Vec lambda, PetscReal x, PetscReal y)
{
  PetscInt i, j, Mx, My, xs, ys, xm, ym;
  Field  **l;
  PetscFunctionBegin;

  PetscCall(DMDAGetInfo(da, PETSC_IGNORE, &Mx, &My, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE));
  /* locate the global i index for x and j index for y */
  i = (PetscInt)(x * (Mx - 1));
  j = (PetscInt)(y * (My - 1));
  PetscCall(DMDAGetCorners(da, &xs, &ys, NULL, &xm, &ym, NULL));

  if (xs <= i && i < xs + xm && ys <= j && j < ys + ym) {
    /* the i,j vertex is on this process */
    PetscCall(DMDAVecGetArray(da, lambda, &l));
    l[j][i].u = 1.0;
    l[j][i].v = 1.0;
    PetscCall(DMDAVecRestoreArray(da, lambda, &l));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  TS        ts; /* ODE integrator */
  Vec       x;  /* solution */
  DM        da;
  AppCtx    appctx;
  Vec       lambda[1];
  PetscBool forwardonly = PETSC_FALSE, implicitform = PETSC_TRUE;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-forwardonly", &forwardonly, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-implicitform", &implicitform, NULL));
  appctx.aijpc = PETSC_FALSE;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-aijpc", &appctx.aijpc, NULL));

  appctx.D1    = 8.0e-5;
  appctx.D2    = 4.0e-5;
  appctx.gamma = .024;
  appctx.kappa = .06;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create distributed array (DMDA) to manage parallel grid and vectors
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(DMDACreate2d(PETSC_COMM_WORLD, DM_BOUNDARY_PERIODIC, DM_BOUNDARY_PERIODIC, DMDA_STENCIL_STAR, 64, 64, PETSC_DECIDE, PETSC_DECIDE, 2, 1, NULL, NULL, &da));
  PetscCall(DMSetFromOptions(da));
  PetscCall(DMSetUp(da));
  PetscCall(DMDASetFieldName(da, 0, "u"));
  PetscCall(DMDASetFieldName(da, 1, "v"));

  /*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Extract global vectors from DMDA; then duplicate for remaining
     vectors that are the same types
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(DMCreateGlobalVector(da, &x));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSCreate(PETSC_COMM_WORLD, &ts));
  PetscCall(TSSetDM(ts, da));
  PetscCall(TSSetProblemType(ts, TS_NONLINEAR));
  PetscCall(TSSetEquationType(ts, TS_EQ_ODE_EXPLICIT)); /* less Jacobian evaluations when adjoint BEuler is used, otherwise no effect */
  if (!implicitform) {
    PetscCall(TSSetType(ts, TSRK));
    PetscCall(TSSetRHSFunction(ts, NULL, RHSFunction, &appctx));
    PetscCall(TSSetRHSJacobian(ts, NULL, NULL, RHSJacobian, &appctx));
  } else {
    PetscCall(TSSetType(ts, TSCN));
    PetscCall(TSSetIFunction(ts, NULL, IFunction, &appctx));
    if (appctx.aijpc) {
      Mat A, B;

      PetscCall(DMSetMatType(da, MATSELL));
      PetscCall(DMCreateMatrix(da, &A));
      PetscCall(MatConvert(A, MATAIJ, MAT_INITIAL_MATRIX, &B));
      /* FIXME do we need to change viewer to display matrix in natural ordering as DMCreateMatrix_DA does? */
      PetscCall(TSSetIJacobian(ts, A, B, IJacobian, &appctx));
      PetscCall(MatDestroy(&A));
      PetscCall(MatDestroy(&B));
    } else {
      PetscCall(TSSetIJacobian(ts, NULL, NULL, IJacobian, &appctx));
    }
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set initial conditions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(InitialConditions(da, x));
  PetscCall(TSSetSolution(ts, x));

  /*
    Have the TS save its trajectory so that TSAdjointSolve() may be used
  */
  if (!forwardonly) PetscCall(TSSetSaveTrajectory(ts));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set solver options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSetMaxTime(ts, 200.0));
  PetscCall(TSSetTimeStep(ts, 0.5));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_MATCHSTEP));
  PetscCall(TSSetFromOptions(ts));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve ODE system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(TSSolve(ts, x));
  if (!forwardonly) {
    /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
       Start the Adjoint model
       - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    PetscCall(VecDuplicate(x, &lambda[0]));
    /*   Reset initial conditions for the adjoint integration */
    PetscCall(InitializeLambda(da, lambda[0], 0.5, 0.5));
    PetscCall(TSSetCostGradients(ts, 1, lambda, NULL));
    PetscCall(TSAdjointSolve(ts));
    PetscCall(VecDestroy(&lambda[0]));
  }
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscCall(VecDestroy(&x));
  PetscCall(TSDestroy(&ts));
  PetscCall(DMDestroy(&da));
  PetscCall(PetscFinalize());
  return 0;
}

/* ------------------------------------------------------------------- */
PetscErrorCode InitialConditions(DM da, Vec U)
{
  PetscInt  i, j, xs, ys, xm, ym, Mx, My;
  Field   **u;
  PetscReal hx, hy, x, y;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(da, PETSC_IGNORE, &Mx, &My, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE));

  hx = 2.5 / (PetscReal)Mx;
  hy = 2.5 / (PetscReal)My;

  /*
     Get pointers to vector data
  */
  PetscCall(DMDAVecGetArray(da, U, &u));

  /*
     Get local grid boundaries
  */
  PetscCall(DMDAGetCorners(da, &xs, &ys, NULL, &xm, &ym, NULL));

  /*
     Compute function over the locally owned part of the grid
  */
  for (j = ys; j < ys + ym; j++) {
    y = j * hy;
    for (i = xs; i < xs + xm; i++) {
      x = i * hx;
      if (PetscApproximateGTE(x, 1.0) && PetscApproximateLTE(x, 1.5) && PetscApproximateGTE(y, 1.0) && PetscApproximateLTE(y, 1.5))
        u[j][i].v = PetscPowReal(PetscSinReal(4.0 * PETSC_PI * x), 2.0) * PetscPowReal(PetscSinReal(4.0 * PETSC_PI * y), 2.0) / 4.0;
      else u[j][i].v = 0.0;

      u[j][i].u = 1.0 - 2.0 * u[j][i].v;
    }
  }

  /*
     Restore vectors
  */
  PetscCall(DMDAVecRestoreArray(da, U, &u));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*TEST

   build:
      depends: reaction_diffusion.c
      requires: !complex !single

   test:
      args: -ts_max_steps 10 -ts_monitor -ts_adjoint_monitor -da_grid_x 20 -da_grid_y 20
      output_file: output/ex5adj_1.out

   test:
      suffix: 2
      nsize: 2
      args: -ts_max_steps 10 -ts_dt 10 -ts_monitor -ts_adjoint_monitor -ksp_monitor_short -da_grid_x 20 -da_grid_y 20 -ts_trajectory_dirname Test-dir -ts_trajectory_file_template test-%06D.cp

   test:
      suffix: 3
      nsize: 2
      args: -ts_max_steps 10 -ts_dt 10 -ts_adjoint_monitor_draw_sensi

   test:
      suffix: 4
      nsize: 2
      args: -ts_max_steps 10 -ts_dt 10 -ts_monitor -ts_adjoint_monitor -ksp_monitor_short -da_grid_x 20 -da_grid_y 20 -snes_fd_color
      output_file: output/ex5adj_2.out

   test:
      suffix: 5
      nsize: 2
      args: -ts_max_steps 10 -implicitform 0 -ts_type rk -ts_rk_type 4 -ts_monitor -ts_adjoint_monitor -da_grid_x 20 -da_grid_y 20 -snes_fd_color
      output_file: output/ex5adj_1.out

   test:
      suffix: knl
      args: -ts_max_steps 10 -ts_monitor -ts_adjoint_monitor -ts_trajectory_type memory -ts_trajectory_solution_only 0 -malloc_hbw -ts_trajectory_use_dram 1
      output_file: output/ex5adj_3.out
      requires: knl

   test:
      suffix: sell
      nsize: 4
      args: -forwardonly -ts_max_steps 10 -ts_monitor -snes_monitor_short -dm_mat_type sell -pc_type none
      output_file: output/ex5adj_sell_1.out

   test:
      suffix: aijsell
      nsize: 4
      args: -forwardonly -ts_max_steps 10 -ts_monitor -snes_monitor_short -dm_mat_type aijsell -pc_type none
      output_file: output/ex5adj_sell_1.out

   test:
      suffix: sell2
      nsize: 4
      args: -forwardonly -ts_max_steps 10 -ts_monitor -snes_monitor_short -dm_mat_type sell -pc_type mg -pc_mg_levels 2 -mg_coarse_pc_type sor
      output_file: output/ex5adj_sell_2.out

   test:
      suffix: aijsell2
      nsize: 4
      args: -forwardonly -ts_max_steps 10 -ts_monitor -snes_monitor_short -dm_mat_type aijsell -pc_type mg -pc_mg_levels 2 -mg_coarse_pc_type sor
      output_file: output/ex5adj_sell_2.out

   test:
      suffix: sell3
      nsize: 4
      args: -forwardonly -ts_max_steps 10 -ts_monitor -snes_monitor_short -dm_mat_type sell -pc_type mg -pc_mg_levels 2 -mg_coarse_pc_type bjacobi -mg_levels_pc_type bjacobi
      output_file: output/ex5adj_sell_3.out

   test:
      suffix: sell4
      nsize: 4
      args: -forwardonly -implicitform -ts_max_steps 10 -ts_monitor -snes_monitor_short -dm_mat_type sell -pc_type mg -pc_mg_levels 2 -mg_coarse_pc_type bjacobi -mg_levels_pc_type bjacobi
      output_file: output/ex5adj_sell_4.out

   test:
      suffix: sell5
      nsize: 4
      args: -forwardonly -ts_max_steps 10 -ts_monitor -snes_monitor_short -dm_mat_type sell -aijpc
      output_file: output/ex5adj_sell_5.out

   test:
      suffix: aijsell5
      nsize: 4
      args: -forwardonly -ts_max_steps 10 -ts_monitor -snes_monitor_short -dm_mat_type aijsell
      output_file: output/ex5adj_sell_5.out

   test:
      suffix: sell6
      args: -ts_max_steps 10 -ts_monitor -ts_adjoint_monitor -ts_trajectory_type memory -ts_trajectory_solution_only 0 -dm_mat_type sell -pc_type jacobi
      output_file: output/ex5adj_sell_6.out

   test:
      suffix: gamg1
      args: -pc_type gamg -pc_gamg_esteig_ksp_type gmres -pc_gamg_esteig_ksp_max_it 10 -pc_mg_levels 2 -ksp_monitor_short -ts_max_steps 2 -ts_monitor -ts_adjoint_monitor -ts_trajectory_type memory -ksp_rtol 1e-2 -pc_gamg_use_sa_esteig 0
      output_file: output/ex5adj_gamg.out

   test:
      suffix: gamg2
      args: -pc_type gamg -pc_gamg_esteig_ksp_type gmres -pc_gamg_esteig_ksp_max_it 10 -pc_mg_levels 2 -ksp_monitor_short -ts_max_steps 2 -ts_monitor -ts_adjoint_monitor -ts_trajectory_type memory -ksp_use_explicittranspose -ksp_rtol 1e-2 -pc_gamg_use_sa_esteig 0
      output_file: output/ex5adj_gamg.out
TEST*/

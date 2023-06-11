static const char help[] = "Simple 2D or 3D finite difference seismic wave propagation, using\n"
                           "a staggered grid represented with DMStag objects.\n"
                           "-dim <2,3>  : dimension of problem\n"
                           "-nsteps <n> : number of timesteps\n"
                           "-dt <dt>    : length of timestep\n"
                           "\n";

/*
Reference:

@article{Virieux1986,
  title     = {{P-SV} Wave Propagation in Heterogeneous Media: {V}elocity-Stress Finite-Difference Method},
  author    = {Virieux, Jean},
  journal   = {Geophysics},
  volume    = {51},
  number    = {4},
  pages     = {889--901},
  publisher = {Society of Exploration Geophysicists},
  year      = {1986},
}

This example uses y in 2 dimensions, where the paper uses z.

This example uses the dual grid of the one pictured in Fig. 1. of the paper, so
that velocities are on face boundaries, shear stresses are defined on
vertices(2D) or edges(3D), and normal stresses are defined on elements.

There is a typo in the paragraph after (5) in the paper: Sigma, Xi, and Tau
represent tau_xx, tau_xz, and tau_zz, respectively (the last two entries are
transposed in the paper).

This example treats the boundaries naively (by leaving ~zero velocity and
stress there).
*/

#include <petscdmstag.h>
#include <petscts.h>

/* A struct defining the parameters of the problem */
typedef struct {
  DM          dm_velocity, dm_stress;
  DM          dm_buoyancy, dm_lame;
  Vec         buoyancy, lame;  /* Global, but for efficiency one could store local vectors */
  PetscInt    dim;             /* 2 or 3 */
  PetscScalar rho, lambda, mu; /* constant material properties */
  PetscReal   xmin, xmax, ymin, ymax, zmin, zmax;
  PetscReal   dt;
  PetscInt    timesteps;
  PetscBool   dump_output;
} Ctx;

static PetscErrorCode CreateLame(Ctx *);
static PetscErrorCode ForceStress(const Ctx *, Vec, PetscReal);
static PetscErrorCode DumpVelocity(const Ctx *, Vec, PetscInt);
static PetscErrorCode DumpStress(const Ctx *, Vec, PetscInt);
static PetscErrorCode UpdateVelocity(const Ctx *, Vec, Vec, Vec);
static PetscErrorCode UpdateStress(const Ctx *, Vec, Vec, Vec);

int main(int argc, char *argv[])
{
  Ctx      ctx;
  Vec      velocity, stress;
  PetscInt timestep;

  /* Initialize PETSc */
  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, 0, help));

  /* Populate application context */
  ctx.dim         = 2;
  ctx.rho         = 1.0;
  ctx.lambda      = 1.0;
  ctx.mu          = 1.0;
  ctx.xmin        = 0.0;
  ctx.xmax        = 1.0;
  ctx.ymin        = 0.0;
  ctx.ymax        = 1.0;
  ctx.zmin        = 0.0;
  ctx.zmax        = 1.0;
  ctx.dt          = 0.001;
  ctx.timesteps   = 100;
  ctx.dump_output = PETSC_TRUE;

  /* Update context from command line options */
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-dim", &ctx.dim, NULL));
  PetscCall(PetscOptionsGetReal(NULL, NULL, "-dt", &ctx.dt, NULL));
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-nsteps", &ctx.timesteps, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-dump_output", &ctx.dump_output, NULL));

  /* Create a DMStag, with uniform coordinates, for the velocities */
  {
    PetscInt       dof0, dof1, dof2, dof3;
    const PetscInt stencilWidth = 1;

    switch (ctx.dim) {
    case 2:
      dof0 = 0;
      dof1 = 1;
      dof2 = 0; /* 1 dof per cell boundary */
      PetscCall(DMStagCreate2d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, 100, 100, PETSC_DECIDE, PETSC_DECIDE, dof0, dof1, dof2, DMSTAG_STENCIL_BOX, stencilWidth, NULL, NULL, &ctx.dm_velocity));
      break;
    case 3:
      dof0 = 0;
      dof1 = 0;
      dof2 = 1;
      dof3 = 0; /* 1 dof per cell boundary */
      PetscCall(DMStagCreate3d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, 30, 30, 30, PETSC_DECIDE, PETSC_DECIDE, PETSC_DECIDE, dof0, dof1, dof2, dof3, DMSTAG_STENCIL_BOX, stencilWidth, NULL, NULL, NULL, &ctx.dm_velocity));
      break;
    default:
      SETERRQ(PETSC_COMM_WORLD, PETSC_ERR_SUP, "Not Implemented for dimension %" PetscInt_FMT, ctx.dim);
    }
  }
  PetscCall(DMSetFromOptions(ctx.dm_velocity)); /* Options control velocity DM */
  PetscCall(DMSetUp(ctx.dm_velocity));
  PetscCall(DMStagSetUniformCoordinatesProduct(ctx.dm_velocity, ctx.xmin, ctx.xmax, ctx.ymin, ctx.ymax, ctx.zmin, ctx.zmax));

  /* Create a second, compatible DMStag for the stresses */
  switch (ctx.dim) {
  case 2:
    /* One shear stress component on element corners, two shear stress components on elements */
    PetscCall(DMStagCreateCompatibleDMStag(ctx.dm_velocity, 1, 0, 2, 0, &ctx.dm_stress));
    break;
  case 3:
    /* One shear stress component on element edges, three shear stress components on elements */
    PetscCall(DMStagCreateCompatibleDMStag(ctx.dm_velocity, 0, 1, 0, 3, &ctx.dm_stress));
    break;
  default:
    SETERRQ(PETSC_COMM_WORLD, PETSC_ERR_SUP, "Not Implemented for dimension %" PetscInt_FMT, ctx.dim);
  }
  PetscCall(DMSetUp(ctx.dm_stress));
  PetscCall(DMStagSetUniformCoordinatesProduct(ctx.dm_stress, ctx.xmin, ctx.xmax, ctx.ymin, ctx.ymax, ctx.zmin, ctx.zmax));

  /* Create two additional DMStag objects for the buoyancy and Lame parameters */
  switch (ctx.dim) {
  case 2:
    /* buoyancy on element boundaries (edges) */
    PetscCall(DMStagCreateCompatibleDMStag(ctx.dm_velocity, 0, 1, 0, 0, &ctx.dm_buoyancy));
    break;
  case 3:
    /* buoyancy on element boundaries (faces) */
    PetscCall(DMStagCreateCompatibleDMStag(ctx.dm_velocity, 0, 0, 1, 0, &ctx.dm_buoyancy));
    break;
  default:
    SETERRQ(PETSC_COMM_WORLD, PETSC_ERR_SUP, "Not Implemented for dimension %" PetscInt_FMT, ctx.dim);
  }
  PetscCall(DMSetUp(ctx.dm_buoyancy));

  switch (ctx.dim) {
  case 2:
    /* mu and lambda + 2*mu on element centers, mu on corners */
    PetscCall(DMStagCreateCompatibleDMStag(ctx.dm_velocity, 1, 0, 2, 0, &ctx.dm_lame));
    break;
  case 3:
    /* mu and lambda + 2*mu on element centers, mu on edges */
    PetscCall(DMStagCreateCompatibleDMStag(ctx.dm_velocity, 0, 1, 0, 2, &ctx.dm_lame));
    break;
  default:
    SETERRQ(PETSC_COMM_WORLD, PETSC_ERR_SUP, "Not Implemented for dimension %" PetscInt_FMT, ctx.dim);
  }
  PetscCall(DMSetUp(ctx.dm_lame));

  /* Print out some info */
  {
    PetscInt    N[3];
    PetscScalar dx, Vp;

    PetscCall(DMStagGetGlobalSizes(ctx.dm_velocity, &N[0], &N[1], &N[2]));
    dx = (ctx.xmax - ctx.xmin) / N[0];
    Vp = PetscSqrtScalar((ctx.lambda + 2 * ctx.mu) / ctx.rho);
    if (ctx.dim == 2) {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Using a %" PetscInt_FMT " x %" PetscInt_FMT " mesh\n", N[0], N[1]));
    } else if (ctx.dim == 3) {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Using a %" PetscInt_FMT " x %" PetscInt_FMT " x %" PetscInt_FMT " mesh\n", N[0], N[1], N[2]));
    }
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "dx: %g\n", (double)PetscRealPart(dx)));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "dt: %g\n", (double)ctx.dt));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "P-wave velocity: %g\n", (double)PetscRealPart(PetscSqrtScalar((ctx.lambda + 2 * ctx.mu) / ctx.rho))));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "V_p dt / dx: %g\n", (double)PetscRealPart(Vp * ctx.dt / dx)));
  }

  /* Populate the coefficient arrays */
  PetscCall(CreateLame(&ctx));
  PetscCall(DMCreateGlobalVector(ctx.dm_buoyancy, &ctx.buoyancy));
  PetscCall(VecSet(ctx.buoyancy, 1.0 / ctx.rho));

  /* Create vectors to store the system state */
  PetscCall(DMCreateGlobalVector(ctx.dm_velocity, &velocity));
  PetscCall(DMCreateGlobalVector(ctx.dm_stress, &stress));

  /* Initial State */
  PetscCall(VecSet(velocity, 0.0));
  PetscCall(VecSet(stress, 0.0));
  PetscCall(ForceStress(&ctx, stress, 0.0));
  if (ctx.dump_output) {
    PetscCall(DumpVelocity(&ctx, velocity, 0));
    PetscCall(DumpStress(&ctx, stress, 0));
  }

  /* Time Loop */
  for (timestep = 1; timestep <= ctx.timesteps; ++timestep) {
    const PetscReal t = timestep * ctx.dt;

    PetscCall(UpdateVelocity(&ctx, velocity, stress, ctx.buoyancy));
    PetscCall(UpdateStress(&ctx, velocity, stress, ctx.lame));
    PetscCall(ForceStress(&ctx, stress, t));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Timestep %" PetscInt_FMT ", t = %g\n", timestep, (double)t));
    if (ctx.dump_output) {
      PetscCall(DumpVelocity(&ctx, velocity, timestep));
      PetscCall(DumpStress(&ctx, stress, timestep));
    }
  }

  /* Clean up and finalize PETSc */
  PetscCall(VecDestroy(&velocity));
  PetscCall(VecDestroy(&stress));
  PetscCall(VecDestroy(&ctx.lame));
  PetscCall(VecDestroy(&ctx.buoyancy));
  PetscCall(DMDestroy(&ctx.dm_velocity));
  PetscCall(DMDestroy(&ctx.dm_stress));
  PetscCall(DMDestroy(&ctx.dm_buoyancy));
  PetscCall(DMDestroy(&ctx.dm_lame));
  PetscCall(PetscFinalize());
  return 0;
}

static PetscErrorCode CreateLame(Ctx *ctx)
{
  PetscInt N[3], ex, ey, ez, startx, starty, startz, nx, ny, nz, extrax, extray, extraz;

  PetscFunctionBeginUser;
  PetscCall(DMCreateGlobalVector(ctx->dm_lame, &ctx->lame));
  PetscCall(DMStagGetGlobalSizes(ctx->dm_lame, &N[0], &N[1], &N[2]));
  PetscCall(DMStagGetCorners(ctx->dm_buoyancy, &startx, &starty, &startz, &nx, &ny, &nz, &extrax, &extray, &extraz));
  if (ctx->dim == 2) {
    /* Element values */
    for (ey = starty; ey < starty + ny; ++ey) {
      for (ex = startx; ex < startx + nx; ++ex) {
        DMStagStencil pos;

        pos.i   = ex;
        pos.j   = ey;
        pos.c   = 0;
        pos.loc = DMSTAG_ELEMENT;
        PetscCall(DMStagVecSetValuesStencil(ctx->dm_lame, ctx->lame, 1, &pos, &ctx->lambda, INSERT_VALUES));
        pos.i   = ex;
        pos.j   = ey;
        pos.c   = 1;
        pos.loc = DMSTAG_ELEMENT;
        PetscCall(DMStagVecSetValuesStencil(ctx->dm_lame, ctx->lame, 1, &pos, &ctx->mu, INSERT_VALUES));
      }
    }
    /* Vertex Values */
    for (ey = starty; ey < starty + ny + extray; ++ey) {
      for (ex = startx; ex < startx + nx + extrax; ++ex) {
        DMStagStencil pos;

        pos.i   = ex;
        pos.j   = ey;
        pos.c   = 0;
        pos.loc = DMSTAG_DOWN_LEFT;
        PetscCall(DMStagVecSetValuesStencil(ctx->dm_lame, ctx->lame, 1, &pos, &ctx->mu, INSERT_VALUES));
      }
    }
  } else if (ctx->dim == 3) {
    /* Element values */
    for (ez = startz; ez < startz + nz; ++ez) {
      for (ey = starty; ey < starty + ny; ++ey) {
        for (ex = startx; ex < startx + nx; ++ex) {
          DMStagStencil pos;

          pos.i   = ex;
          pos.j   = ey;
          pos.k   = ez;
          pos.c   = 0;
          pos.loc = DMSTAG_ELEMENT;
          PetscCall(DMStagVecSetValuesStencil(ctx->dm_lame, ctx->lame, 1, &pos, &ctx->lambda, INSERT_VALUES));
          pos.i   = ex;
          pos.j   = ey;
          pos.k   = ez;
          pos.c   = 1;
          pos.loc = DMSTAG_ELEMENT;
          PetscCall(DMStagVecSetValuesStencil(ctx->dm_lame, ctx->lame, 1, &pos, &ctx->mu, INSERT_VALUES));
        }
      }
    }
    /* Edge Values */
    for (ez = startz; ez < startz + nz + extraz; ++ez) {
      for (ey = starty; ey < starty + ny + extray; ++ey) {
        for (ex = startx; ex < startx + nx + extrax; ++ex) {
          DMStagStencil pos;

          if (ex < N[0]) {
            pos.i   = ex;
            pos.j   = ey;
            pos.k   = ez;
            pos.c   = 0;
            pos.loc = DMSTAG_BACK_DOWN;
            PetscCall(DMStagVecSetValuesStencil(ctx->dm_lame, ctx->lame, 1, &pos, &ctx->mu, INSERT_VALUES));
          }
          if (ey < N[1]) {
            pos.i   = ex;
            pos.j   = ey;
            pos.k   = ez;
            pos.c   = 0;
            pos.loc = DMSTAG_BACK_LEFT;
            PetscCall(DMStagVecSetValuesStencil(ctx->dm_lame, ctx->lame, 1, &pos, &ctx->mu, INSERT_VALUES));
          }
          if (ez < N[2]) {
            pos.i   = ex;
            pos.j   = ey;
            pos.k   = ez;
            pos.c   = 0;
            pos.loc = DMSTAG_DOWN_LEFT;
            PetscCall(DMStagVecSetValuesStencil(ctx->dm_lame, ctx->lame, 1, &pos, &ctx->mu, INSERT_VALUES));
          }
        }
      }
    }
  } else SETERRQ(PetscObjectComm((PetscObject)ctx->dm_velocity), PETSC_ERR_SUP, "Unsupported dim %" PetscInt_FMT, ctx->dim);
  PetscCall(VecAssemblyBegin(ctx->lame));
  PetscCall(VecAssemblyEnd(ctx->lame));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode ForceStress(const Ctx *ctx, Vec stress, PetscReal t)
{
  PetscInt          start[3], n[3], N[3];
  DMStagStencil     pos;
  PetscBool         this_rank;
  const PetscScalar val = PetscExpReal(-100.0 * t);

  PetscFunctionBeginUser;
  PetscCall(DMStagGetGlobalSizes(ctx->dm_stress, &N[0], &N[1], &N[2]));
  PetscCall(DMStagGetCorners(ctx->dm_stress, &start[0], &start[1], &start[2], &n[0], &n[1], &n[2], NULL, NULL, NULL));

  /* Normal stresses at a single point */
  this_rank = (PetscBool)(start[0] <= N[0] / 2 && N[0] / 2 <= start[0] + n[0]);
  this_rank = (PetscBool)(this_rank && start[1] <= N[1] / 2 && N[1] / 2 <= start[1] + n[1]);
  if (ctx->dim == 3) this_rank = (PetscBool)(this_rank && start[2] <= N[2] / 2 && N[2] / 2 <= start[2] + n[2]);
  if (this_rank) {
    /* Note integer division to pick element near the center */
    pos.i   = N[0] / 2;
    pos.j   = N[1] / 2;
    pos.k   = N[2] / 2;
    pos.c   = 0;
    pos.loc = DMSTAG_ELEMENT;
    PetscCall(DMStagVecSetValuesStencil(ctx->dm_stress, stress, 1, &pos, &val, INSERT_VALUES));
    pos.i   = N[0] / 2;
    pos.j   = N[1] / 2;
    pos.k   = N[2] / 2;
    pos.c   = 1;
    pos.loc = DMSTAG_ELEMENT;
    PetscCall(DMStagVecSetValuesStencil(ctx->dm_stress, stress, 1, &pos, &val, INSERT_VALUES));
    if (ctx->dim == 3) {
      pos.i   = N[0] / 2;
      pos.j   = N[1] / 2;
      pos.k   = N[2] / 2;
      pos.c   = 2;
      pos.loc = DMSTAG_ELEMENT;
      PetscCall(DMStagVecSetValuesStencil(ctx->dm_stress, stress, 1, &pos, &val, INSERT_VALUES));
    }
  }

  PetscCall(VecAssemblyBegin(stress));
  PetscCall(VecAssemblyEnd(stress));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode UpdateVelocity_2d(const Ctx *ctx, Vec velocity, Vec stress, Vec buoyancy)
{
  Vec                  velocity_local, stress_local, buoyancy_local;
  PetscInt             ex, ey, startx, starty, nx, ny;
  PetscInt             slot_coord_next, slot_coord_element, slot_coord_prev;
  PetscInt             slot_vx_left, slot_vy_down, slot_buoyancy_down, slot_buoyancy_left;
  PetscInt             slot_txx, slot_tyy, slot_txy_downleft, slot_txy_downright, slot_txy_upleft;
  const PetscScalar  **arr_coord_x, **arr_coord_y;
  const PetscScalar ***arr_stress, ***arr_buoyancy;
  PetscScalar       ***arr_velocity;

  PetscFunctionBeginUser;

  /* Prepare direct access to buoyancy data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_buoyancy, DMSTAG_LEFT, 0, &slot_buoyancy_left));
  PetscCall(DMStagGetLocationSlot(ctx->dm_buoyancy, DMSTAG_DOWN, 0, &slot_buoyancy_down));
  PetscCall(DMGetLocalVector(ctx->dm_buoyancy, &buoyancy_local));
  PetscCall(DMGlobalToLocal(ctx->dm_buoyancy, buoyancy, INSERT_VALUES, buoyancy_local));
  PetscCall(DMStagVecGetArrayRead(ctx->dm_buoyancy, buoyancy_local, (void *)&arr_buoyancy));

  /* Prepare read-only access to stress data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 0, &slot_txx));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 1, &slot_tyy));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_UP_LEFT, 0, &slot_txy_upleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_DOWN_LEFT, 0, &slot_txy_downleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_DOWN_RIGHT, 0, &slot_txy_downright));
  PetscCall(DMGetLocalVector(ctx->dm_stress, &stress_local));
  PetscCall(DMGlobalToLocal(ctx->dm_stress, stress, INSERT_VALUES, stress_local));
  PetscCall(DMStagVecGetArrayRead(ctx->dm_stress, stress_local, (void *)&arr_stress));

  /* Prepare read-write access to velocity data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_LEFT, 0, &slot_vx_left));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_DOWN, 0, &slot_vy_down));
  PetscCall(DMGetLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMGlobalToLocal(ctx->dm_velocity, velocity, INSERT_VALUES, velocity_local));
  PetscCall(DMStagVecGetArray(ctx->dm_velocity, velocity_local, &arr_velocity));

  /* Prepare read-only access to coordinate data */
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_LEFT, &slot_coord_prev));
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_RIGHT, &slot_coord_next));
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_ELEMENT, &slot_coord_element));
  PetscCall(DMStagGetProductCoordinateArrays(ctx->dm_velocity, (void *)&arr_coord_x, (void *)&arr_coord_y, NULL));

  /* Iterate over interior of the domain, updating the velocities */
  PetscCall(DMStagGetCorners(ctx->dm_velocity, &startx, &starty, NULL, &nx, &ny, NULL, NULL, NULL, NULL));
  for (ey = starty; ey < starty + ny; ++ey) {
    for (ex = startx; ex < startx + nx; ++ex) {
      /* Update y-velocity */
      if (ey > 0) {
        const PetscScalar dx = arr_coord_x[ex][slot_coord_next] - arr_coord_x[ex][slot_coord_prev];
        const PetscScalar dy = arr_coord_y[ey][slot_coord_element] - arr_coord_y[ey - 1][slot_coord_element];
        const PetscScalar B  = arr_buoyancy[ey][ex][slot_buoyancy_down];

        arr_velocity[ey][ex][slot_vy_down] += B * ctx->dt * ((arr_stress[ey][ex][slot_txy_downright] - arr_stress[ey][ex][slot_txy_downleft]) / dx + (arr_stress[ey][ex][slot_tyy] - arr_stress[ey - 1][ex][slot_tyy]) / dy);
      }

      /* Update x-velocity */
      if (ex > 0) {
        const PetscScalar dx = arr_coord_x[ex][slot_coord_element] - arr_coord_x[ex - 1][slot_coord_element];
        const PetscScalar dy = arr_coord_y[ey][slot_coord_next] - arr_coord_y[ey][slot_coord_prev];
        const PetscScalar B  = arr_buoyancy[ey][ex][slot_buoyancy_left];

        arr_velocity[ey][ex][slot_vx_left] += B * ctx->dt * ((arr_stress[ey][ex][slot_txx] - arr_stress[ey][ex - 1][slot_txx]) / dx + (arr_stress[ey][ex][slot_txy_upleft] - arr_stress[ey][ex][slot_txy_downleft]) / dy);
      }
    }
  }

  /* Restore all access */
  PetscCall(DMStagVecRestoreArrayRead(ctx->dm_buoyancy, buoyancy_local, (void *)&arr_buoyancy));
  PetscCall(DMRestoreLocalVector(ctx->dm_buoyancy, &buoyancy_local));
  PetscCall(DMStagVecRestoreArrayRead(ctx->dm_stress, stress_local, (void *)&arr_stress));
  PetscCall(DMRestoreLocalVector(ctx->dm_stress, &stress_local));
  PetscCall(DMStagVecRestoreArray(ctx->dm_velocity, velocity_local, &arr_velocity));
  PetscCall(DMLocalToGlobal(ctx->dm_velocity, velocity_local, INSERT_VALUES, velocity));
  PetscCall(DMRestoreLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMStagRestoreProductCoordinateArrays(ctx->dm_velocity, (void *)&arr_coord_x, (void *)&arr_coord_y, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode UpdateVelocity_3d(const Ctx *ctx, Vec velocity, Vec stress, Vec buoyancy)
{
  Vec                   velocity_local, stress_local, buoyancy_local;
  PetscInt              ex, ey, ez, startx, starty, startz, nx, ny, nz;
  PetscInt              slot_coord_next, slot_coord_element, slot_coord_prev;
  PetscInt              slot_vx_left, slot_vy_down, slot_vz_back, slot_buoyancy_down, slot_buoyancy_left, slot_buoyancy_back;
  PetscInt              slot_txx, slot_tyy, slot_tzz;
  PetscInt              slot_txy_downleft, slot_txy_downright, slot_txy_upleft;
  PetscInt              slot_txz_backleft, slot_txz_backright, slot_txz_frontleft;
  PetscInt              slot_tyz_backdown, slot_tyz_frontdown, slot_tyz_backup;
  const PetscScalar   **arr_coord_x, **arr_coord_y, **arr_coord_z;
  const PetscScalar ****arr_stress, ****arr_buoyancy;
  PetscScalar       ****arr_velocity;

  PetscFunctionBeginUser;

  /* Prepare direct access to buoyancy data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_buoyancy, DMSTAG_LEFT, 0, &slot_buoyancy_left));
  PetscCall(DMStagGetLocationSlot(ctx->dm_buoyancy, DMSTAG_DOWN, 0, &slot_buoyancy_down));
  PetscCall(DMStagGetLocationSlot(ctx->dm_buoyancy, DMSTAG_BACK, 0, &slot_buoyancy_back));
  PetscCall(DMGetLocalVector(ctx->dm_buoyancy, &buoyancy_local));
  PetscCall(DMGlobalToLocal(ctx->dm_buoyancy, buoyancy, INSERT_VALUES, buoyancy_local));
  PetscCall(DMStagVecGetArrayRead(ctx->dm_buoyancy, buoyancy_local, (void *)&arr_buoyancy));

  /* Prepare read-only access to stress data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 0, &slot_txx));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 1, &slot_tyy));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 2, &slot_tzz));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_UP_LEFT, 0, &slot_txy_upleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_DOWN_LEFT, 0, &slot_txy_downleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_DOWN_RIGHT, 0, &slot_txy_downright));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_BACK_LEFT, 0, &slot_txz_backleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_BACK_RIGHT, 0, &slot_txz_backright));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_FRONT_LEFT, 0, &slot_txz_frontleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_BACK_DOWN, 0, &slot_tyz_backdown));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_BACK_UP, 0, &slot_tyz_backup));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_FRONT_DOWN, 0, &slot_tyz_frontdown));
  PetscCall(DMGetLocalVector(ctx->dm_stress, &stress_local));
  PetscCall(DMGlobalToLocal(ctx->dm_stress, stress, INSERT_VALUES, stress_local));
  PetscCall(DMStagVecGetArrayRead(ctx->dm_stress, stress_local, (void *)&arr_stress));

  /* Prepare read-write access to velocity data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_LEFT, 0, &slot_vx_left));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_DOWN, 0, &slot_vy_down));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_BACK, 0, &slot_vz_back));
  PetscCall(DMGetLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMGlobalToLocal(ctx->dm_velocity, velocity, INSERT_VALUES, velocity_local));
  PetscCall(DMStagVecGetArray(ctx->dm_velocity, velocity_local, &arr_velocity));

  /* Prepare read-only access to coordinate data */
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_LEFT, &slot_coord_prev));
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_RIGHT, &slot_coord_next));
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_ELEMENT, &slot_coord_element));
  PetscCall(DMStagGetProductCoordinateArrays(ctx->dm_velocity, (void *)&arr_coord_x, (void *)&arr_coord_y, (void *)&arr_coord_z));

  /* Iterate over interior of the domain, updating the velocities */
  PetscCall(DMStagGetCorners(ctx->dm_velocity, &startx, &starty, &startz, &nx, &ny, &nz, NULL, NULL, NULL));
  for (ez = startz; ez < startz + nz; ++ez) {
    for (ey = starty; ey < starty + ny; ++ey) {
      for (ex = startx; ex < startx + nx; ++ex) {
        /* Update y-velocity */
        if (ey > 0) {
          const PetscScalar dx = arr_coord_x[ex][slot_coord_next] - arr_coord_x[ex][slot_coord_prev];
          const PetscScalar dy = arr_coord_y[ey][slot_coord_element] - arr_coord_y[ey - 1][slot_coord_element];
          const PetscScalar dz = arr_coord_z[ez][slot_coord_next] - arr_coord_z[ez][slot_coord_prev];
          const PetscScalar B  = arr_buoyancy[ez][ey][ex][slot_buoyancy_down];

          arr_velocity[ez][ey][ex][slot_vy_down] += B * ctx->dt * ((arr_stress[ez][ey][ex][slot_txy_downright] - arr_stress[ez][ey][ex][slot_txy_downleft]) / dx + (arr_stress[ez][ey][ex][slot_tyy] - arr_stress[ez][ey - 1][ex][slot_tyy]) / dy + (arr_stress[ez][ey][ex][slot_tyz_frontdown] - arr_stress[ez][ey][ex][slot_tyz_backdown]) / dz);
        }

        /* Update x-velocity */
        if (ex > 0) {
          const PetscScalar dx = arr_coord_x[ex][slot_coord_element] - arr_coord_x[ex - 1][slot_coord_element];
          const PetscScalar dy = arr_coord_y[ey][slot_coord_next] - arr_coord_y[ey][slot_coord_prev];
          const PetscScalar dz = arr_coord_z[ez][slot_coord_next] - arr_coord_z[ez][slot_coord_prev];
          const PetscScalar B  = arr_buoyancy[ez][ey][ex][slot_buoyancy_left];

          arr_velocity[ez][ey][ex][slot_vx_left] += B * ctx->dt * ((arr_stress[ez][ey][ex][slot_txx] - arr_stress[ez][ey][ex - 1][slot_txx]) / dx + (arr_stress[ez][ey][ex][slot_txy_upleft] - arr_stress[ez][ey][ex][slot_txy_downleft]) / dy + (arr_stress[ez][ey][ex][slot_txz_frontleft] - arr_stress[ez][ey][ex][slot_txz_backleft]) / dz);
        }

        /* Update z-velocity */
        if (ez > 0) {
          const PetscScalar dx = arr_coord_x[ex][slot_coord_next] - arr_coord_x[ex][slot_coord_prev];
          const PetscScalar dy = arr_coord_y[ey][slot_coord_next] - arr_coord_y[ey][slot_coord_prev];
          const PetscScalar dz = arr_coord_z[ez][slot_coord_element] - arr_coord_z[ez - 1][slot_coord_element];
          const PetscScalar B  = arr_buoyancy[ez][ey][ex][slot_buoyancy_back];

          arr_velocity[ez][ey][ex][slot_vz_back] += B * ctx->dt * ((arr_stress[ez][ey][ex][slot_txz_backright] - arr_stress[ez][ey][ex][slot_txz_backleft]) / dx + (arr_stress[ez][ey][ex][slot_tyz_backup] - arr_stress[ez][ey][ex][slot_tyz_backdown]) / dy + (arr_stress[ez][ey][ex][slot_tzz] - arr_stress[ez - 1][ey][ex][slot_tzz]) / dz);
        }
      }
    }
  }

  /* Restore all access */
  PetscCall(DMStagVecRestoreArrayRead(ctx->dm_buoyancy, buoyancy_local, (void *)&arr_buoyancy));
  PetscCall(DMRestoreLocalVector(ctx->dm_buoyancy, &buoyancy_local));
  PetscCall(DMStagVecRestoreArrayRead(ctx->dm_stress, stress_local, (void *)&arr_stress));
  PetscCall(DMRestoreLocalVector(ctx->dm_stress, &stress_local));
  PetscCall(DMStagVecRestoreArray(ctx->dm_velocity, velocity_local, &arr_velocity));
  PetscCall(DMLocalToGlobal(ctx->dm_velocity, velocity_local, INSERT_VALUES, velocity));
  PetscCall(DMRestoreLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMStagRestoreProductCoordinateArrays(ctx->dm_velocity, (void *)&arr_coord_x, (void *)&arr_coord_y, (void *)&arr_coord_z));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode UpdateVelocity(const Ctx *ctx, Vec velocity, Vec stress, Vec buoyancy)
{
  PetscFunctionBeginUser;
  if (ctx->dim == 2) {
    PetscCall(UpdateVelocity_2d(ctx, velocity, stress, buoyancy));
  } else if (ctx->dim == 3) {
    PetscCall(UpdateVelocity_3d(ctx, velocity, stress, buoyancy));
  } else SETERRQ(PetscObjectComm((PetscObject)ctx->dm_velocity), PETSC_ERR_SUP, "Unsupported dim %" PetscInt_FMT, ctx->dim);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode UpdateStress_2d(const Ctx *ctx, Vec velocity, Vec stress, Vec lame)
{
  Vec                  velocity_local, stress_local, lame_local;
  PetscInt             ex, ey, startx, starty, nx, ny;
  PetscInt             slot_coord_next, slot_coord_element, slot_coord_prev;
  PetscInt             slot_vx_left, slot_vy_down, slot_vx_right, slot_vy_up;
  PetscInt             slot_mu_element, slot_lambda_element, slot_mu_downleft;
  PetscInt             slot_txx, slot_tyy, slot_txy_downleft;
  const PetscScalar  **arr_coord_x, **arr_coord_y;
  const PetscScalar ***arr_velocity, ***arr_lame;
  PetscScalar       ***arr_stress;

  PetscFunctionBeginUser;

  /* Prepare read-write access to stress data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 0, &slot_txx));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 1, &slot_tyy));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_DOWN_LEFT, 0, &slot_txy_downleft));
  PetscCall(DMGetLocalVector(ctx->dm_stress, &stress_local));
  PetscCall(DMGlobalToLocal(ctx->dm_stress, stress, INSERT_VALUES, stress_local));
  PetscCall(DMStagVecGetArray(ctx->dm_stress, stress_local, &arr_stress));

  /* Prepare read-only access to velocity data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_LEFT, 0, &slot_vx_left));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_RIGHT, 0, &slot_vx_right));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_DOWN, 0, &slot_vy_down));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_UP, 0, &slot_vy_up));
  PetscCall(DMGetLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMGlobalToLocal(ctx->dm_velocity, velocity, INSERT_VALUES, velocity_local));
  PetscCall(DMStagVecGetArrayRead(ctx->dm_velocity, velocity_local, (void *)&arr_velocity));

  /* Prepare read-only access to Lame' data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_lame, DMSTAG_ELEMENT, 0, &slot_lambda_element));
  PetscCall(DMStagGetLocationSlot(ctx->dm_lame, DMSTAG_ELEMENT, 1, &slot_mu_element));
  PetscCall(DMStagGetLocationSlot(ctx->dm_lame, DMSTAG_DOWN_LEFT, 0, &slot_mu_downleft));
  PetscCall(DMGetLocalVector(ctx->dm_lame, &lame_local));
  PetscCall(DMGlobalToLocal(ctx->dm_lame, lame, INSERT_VALUES, lame_local));
  PetscCall(DMStagVecGetArrayRead(ctx->dm_lame, lame_local, (void *)&arr_lame));

  /* Prepare read-only access to coordinate data */
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_LEFT, &slot_coord_prev));
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_RIGHT, &slot_coord_next));
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_ELEMENT, &slot_coord_element));
  PetscCall(DMStagGetProductCoordinateArrays(ctx->dm_velocity, (void *)&arr_coord_x, (void *)&arr_coord_y, NULL));

  /* Iterate over the interior of the domain, updating the stresses */
  PetscCall(DMStagGetCorners(ctx->dm_velocity, &startx, &starty, NULL, &nx, &ny, NULL, NULL, NULL, NULL));
  for (ey = starty; ey < starty + ny; ++ey) {
    for (ex = startx; ex < startx + nx; ++ex) {
      /* Update tau_xx and tau_yy*/
      {
        const PetscScalar dx   = arr_coord_x[ex][slot_coord_next] - arr_coord_x[ex][slot_coord_prev];
        const PetscScalar dy   = arr_coord_y[ey][slot_coord_next] - arr_coord_y[ey][slot_coord_prev];
        const PetscScalar L    = arr_lame[ey][ex][slot_lambda_element];
        const PetscScalar Lp2M = arr_lame[ey][ex][slot_lambda_element] + 2 * arr_lame[ey][ex][slot_mu_element];

        arr_stress[ey][ex][slot_txx] += Lp2M * ctx->dt * (arr_velocity[ey][ex][slot_vx_right] - arr_velocity[ey][ex][slot_vx_left]) / dx + L * ctx->dt * (arr_velocity[ey][ex][slot_vy_up] - arr_velocity[ey][ex][slot_vy_down]) / dy;

        arr_stress[ey][ex][slot_tyy] += Lp2M * ctx->dt * (arr_velocity[ey][ex][slot_vy_up] - arr_velocity[ey][ex][slot_vy_down]) / dy + L * ctx->dt * (arr_velocity[ey][ex][slot_vx_right] - arr_velocity[ey][ex][slot_vx_left]) / dx;
      }

      /* Update tau_xy */
      if (ex > 0 && ey > 0) {
        const PetscScalar dx = arr_coord_x[ex][slot_coord_element] - arr_coord_x[ex - 1][slot_coord_element];
        const PetscScalar dy = arr_coord_y[ey][slot_coord_element] - arr_coord_y[ey - 1][slot_coord_element];
        const PetscScalar M  = arr_lame[ey][ex][slot_mu_downleft];

        arr_stress[ey][ex][slot_txy_downleft] += M * ctx->dt * ((arr_velocity[ey][ex][slot_vx_left] - arr_velocity[ey - 1][ex][slot_vx_left]) / dy + (arr_velocity[ey][ex][slot_vy_down] - arr_velocity[ey][ex - 1][slot_vy_down]) / dx);
      }
    }
  }

  /* Restore all access */
  PetscCall(DMStagVecRestoreArray(ctx->dm_stress, stress_local, &arr_stress));
  PetscCall(DMLocalToGlobal(ctx->dm_stress, stress_local, INSERT_VALUES, stress));
  PetscCall(DMRestoreLocalVector(ctx->dm_stress, &stress_local));
  PetscCall(DMStagVecRestoreArrayRead(ctx->dm_velocity, velocity_local, (void *)&arr_velocity));
  PetscCall(DMRestoreLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMStagVecRestoreArrayRead(ctx->dm_lame, lame_local, (void *)&arr_lame));
  PetscCall(DMRestoreLocalVector(ctx->dm_lame, &lame_local));
  PetscCall(DMStagRestoreProductCoordinateArrays(ctx->dm_velocity, (void *)&arr_coord_x, (void *)&arr_coord_y, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode UpdateStress_3d(const Ctx *ctx, Vec velocity, Vec stress, Vec lame)
{
  Vec                   velocity_local, stress_local, lame_local;
  PetscInt              ex, ey, ez, startx, starty, startz, nx, ny, nz;
  PetscInt              slot_coord_next, slot_coord_element, slot_coord_prev;
  PetscInt              slot_vx_left, slot_vy_down, slot_vx_right, slot_vy_up, slot_vz_back, slot_vz_front;
  PetscInt              slot_mu_element, slot_lambda_element, slot_mu_downleft, slot_mu_backdown, slot_mu_backleft;
  PetscInt              slot_txx, slot_tyy, slot_tzz, slot_txy_downleft, slot_txz_backleft, slot_tyz_backdown;
  const PetscScalar   **arr_coord_x, **arr_coord_y, **arr_coord_z;
  const PetscScalar ****arr_velocity, ****arr_lame;
  PetscScalar       ****arr_stress;

  PetscFunctionBeginUser;

  /* Prepare read-write access to stress data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 0, &slot_txx));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 1, &slot_tyy));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_ELEMENT, 2, &slot_tzz));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_DOWN_LEFT, 0, &slot_txy_downleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_BACK_LEFT, 0, &slot_txz_backleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_stress, DMSTAG_BACK_DOWN, 0, &slot_tyz_backdown));
  PetscCall(DMGetLocalVector(ctx->dm_stress, &stress_local));
  PetscCall(DMGlobalToLocal(ctx->dm_stress, stress, INSERT_VALUES, stress_local));
  PetscCall(DMStagVecGetArray(ctx->dm_stress, stress_local, &arr_stress));

  /* Prepare read-only access to velocity data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_LEFT, 0, &slot_vx_left));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_RIGHT, 0, &slot_vx_right));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_DOWN, 0, &slot_vy_down));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_UP, 0, &slot_vy_up));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_BACK, 0, &slot_vz_back));
  PetscCall(DMStagGetLocationSlot(ctx->dm_velocity, DMSTAG_FRONT, 0, &slot_vz_front));
  PetscCall(DMGetLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMGlobalToLocal(ctx->dm_velocity, velocity, INSERT_VALUES, velocity_local));
  PetscCall(DMStagVecGetArrayRead(ctx->dm_velocity, velocity_local, (void *)&arr_velocity));

  /* Prepare read-only access to Lame' data */
  PetscCall(DMStagGetLocationSlot(ctx->dm_lame, DMSTAG_ELEMENT, 0, &slot_lambda_element));
  PetscCall(DMStagGetLocationSlot(ctx->dm_lame, DMSTAG_ELEMENT, 1, &slot_mu_element));
  PetscCall(DMStagGetLocationSlot(ctx->dm_lame, DMSTAG_DOWN_LEFT, 0, &slot_mu_downleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_lame, DMSTAG_BACK_LEFT, 0, &slot_mu_backleft));
  PetscCall(DMStagGetLocationSlot(ctx->dm_lame, DMSTAG_BACK_DOWN, 0, &slot_mu_backdown));
  PetscCall(DMGetLocalVector(ctx->dm_lame, &lame_local));
  PetscCall(DMGlobalToLocal(ctx->dm_lame, lame, INSERT_VALUES, lame_local));
  PetscCall(DMStagVecGetArrayRead(ctx->dm_lame, lame_local, (void *)&arr_lame));

  /* Prepare read-only access to coordinate data */
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_LEFT, &slot_coord_prev));
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_RIGHT, &slot_coord_next));
  PetscCall(DMStagGetProductCoordinateLocationSlot(ctx->dm_velocity, DMSTAG_ELEMENT, &slot_coord_element));
  PetscCall(DMStagGetProductCoordinateArrays(ctx->dm_velocity, (void *)&arr_coord_x, (void *)&arr_coord_y, (void *)&arr_coord_z));

  /* Iterate over the interior of the domain, updating the stresses */
  PetscCall(DMStagGetCorners(ctx->dm_velocity, &startx, &starty, &startz, &nx, &ny, &nz, NULL, NULL, NULL));
  for (ez = startz; ez < startz + nz; ++ez) {
    for (ey = starty; ey < starty + ny; ++ey) {
      for (ex = startx; ex < startx + nx; ++ex) {
        /* Update tau_xx, tau_yy, and tau_zz*/
        {
          const PetscScalar dx   = arr_coord_x[ex][slot_coord_next] - arr_coord_x[ex][slot_coord_prev];
          const PetscScalar dy   = arr_coord_y[ey][slot_coord_next] - arr_coord_y[ey][slot_coord_prev];
          const PetscScalar dz   = arr_coord_z[ez][slot_coord_next] - arr_coord_z[ez][slot_coord_prev];
          const PetscScalar L    = arr_lame[ez][ey][ex][slot_lambda_element];
          const PetscScalar Lp2M = arr_lame[ez][ey][ex][slot_lambda_element] + 2 * arr_lame[ez][ey][ex][slot_mu_element];

          arr_stress[ez][ey][ex][slot_txx] += Lp2M * ctx->dt * (arr_velocity[ez][ey][ex][slot_vx_right] - arr_velocity[ez][ey][ex][slot_vx_left]) / dx + L * ctx->dt * (arr_velocity[ez][ey][ex][slot_vy_up] - arr_velocity[ez][ey][ex][slot_vy_down]) / dy +
                                              L * ctx->dt * (arr_velocity[ez][ey][ex][slot_vz_front] - arr_velocity[ez][ey][ex][slot_vz_back]) / dz;

          arr_stress[ez][ey][ex][slot_tyy] += Lp2M * ctx->dt * (arr_velocity[ez][ey][ex][slot_vy_up] - arr_velocity[ez][ey][ex][slot_vy_down]) / dy + L * ctx->dt * (arr_velocity[ez][ey][ex][slot_vz_front] - arr_velocity[ez][ey][ex][slot_vz_back]) / dz +
                                              L * ctx->dt * (arr_velocity[ez][ey][ex][slot_vx_right] - arr_velocity[ez][ey][ex][slot_vx_left]) / dx;

          arr_stress[ez][ey][ex][slot_tzz] += Lp2M * ctx->dt * (arr_velocity[ez][ey][ex][slot_vz_front] - arr_velocity[ez][ey][ex][slot_vz_back]) / dz + L * ctx->dt * (arr_velocity[ez][ey][ex][slot_vx_right] - arr_velocity[ez][ey][ex][slot_vx_left]) / dx +
                                              L * ctx->dt * (arr_velocity[ez][ey][ex][slot_vy_up] - arr_velocity[ez][ey][ex][slot_vy_down]) / dy;
        }

        /* Update tau_xy, tau_xz, tau_yz */
        {
          PetscScalar dx = 1.0, dy = 1.0, dz = 1.0; /* initialization to prevent incorrect compiler warnings */

          if (ex > 0) dx = arr_coord_x[ex][slot_coord_element] - arr_coord_x[ex - 1][slot_coord_element];
          if (ey > 0) dy = arr_coord_y[ey][slot_coord_element] - arr_coord_y[ey - 1][slot_coord_element];
          if (ez > 0) dz = arr_coord_z[ez][slot_coord_element] - arr_coord_z[ez - 1][slot_coord_element];

          if (ex > 0 && ey > 0) {
            const PetscScalar M = arr_lame[ez][ey][ex][slot_mu_downleft];

            arr_stress[ez][ey][ex][slot_txy_downleft] += M * ctx->dt * ((arr_velocity[ez][ey][ex][slot_vx_left] - arr_velocity[ez][ey - 1][ex][slot_vx_left]) / dy + (arr_velocity[ez][ey][ex][slot_vy_down] - arr_velocity[ez][ey][ex - 1][slot_vy_down]) / dx);
          }

          /* Update tau_xz */
          if (ex > 0 && ez > 0) {
            const PetscScalar M = arr_lame[ez][ey][ex][slot_mu_backleft];

            arr_stress[ez][ey][ex][slot_txz_backleft] += M * ctx->dt * ((arr_velocity[ez][ey][ex][slot_vx_left] - arr_velocity[ez - 1][ey][ex][slot_vx_left]) / dz + (arr_velocity[ez][ey][ex][slot_vz_back] - arr_velocity[ez][ey][ex - 1][slot_vz_back]) / dx);
          }

          /* Update tau_yz */
          if (ey > 0 && ez > 0) {
            const PetscScalar M = arr_lame[ez][ey][ex][slot_mu_backdown];

            arr_stress[ez][ey][ex][slot_tyz_backdown] += M * ctx->dt * ((arr_velocity[ez][ey][ex][slot_vy_down] - arr_velocity[ez - 1][ey][ex][slot_vy_down]) / dz + (arr_velocity[ez][ey][ex][slot_vz_back] - arr_velocity[ez][ey - 1][ex][slot_vz_back]) / dy);
          }
        }
      }
    }
  }

  /* Restore all access */
  PetscCall(DMStagVecRestoreArray(ctx->dm_stress, stress_local, &arr_stress));
  PetscCall(DMLocalToGlobal(ctx->dm_stress, stress_local, INSERT_VALUES, stress));
  PetscCall(DMRestoreLocalVector(ctx->dm_stress, &stress_local));
  PetscCall(DMStagVecRestoreArrayRead(ctx->dm_velocity, velocity_local, (void *)&arr_velocity));
  PetscCall(DMRestoreLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMStagVecRestoreArrayRead(ctx->dm_lame, lame_local, (void *)&arr_lame));
  PetscCall(DMRestoreLocalVector(ctx->dm_lame, &lame_local));
  PetscCall(DMStagRestoreProductCoordinateArrays(ctx->dm_velocity, (void *)&arr_coord_x, (void *)&arr_coord_y, (void *)&arr_coord_z));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode UpdateStress(const Ctx *ctx, Vec velocity, Vec stress, Vec lame)
{
  PetscFunctionBeginUser;
  if (ctx->dim == 2) {
    PetscCall(UpdateStress_2d(ctx, velocity, stress, lame));
  } else if (ctx->dim == 3) {
    PetscCall(UpdateStress_3d(ctx, velocity, stress, lame));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DumpStress(const Ctx *ctx, Vec stress, PetscInt timestep)
{
  DM  da_normal, da_shear   = NULL;
  Vec vec_normal, vec_shear = NULL;

  PetscFunctionBeginUser;

  PetscCall(DMStagVecSplitToDMDA(ctx->dm_stress, stress, DMSTAG_ELEMENT, -ctx->dim, &da_normal, &vec_normal));
  PetscCall(PetscObjectSetName((PetscObject)vec_normal, "normal stresses"));

  /* Dump element-based fields to a .vtr file */
  {
    PetscViewer viewer;
    char        filename[PETSC_MAX_PATH_LEN];

    PetscCall(PetscSNPrintf(filename, sizeof(filename), "ex6_stress_normal_%.4" PetscInt_FMT ".vtr", timestep));
    PetscCall(PetscViewerVTKOpen(PetscObjectComm((PetscObject)da_normal), filename, FILE_MODE_WRITE, &viewer));
    PetscCall(VecView(vec_normal, viewer));
    PetscCall(PetscViewerDestroy(&viewer));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Created %s\n", filename));
  }

  if (ctx->dim == 2) {
    /* (2D only) Dump vertex-based fields to a second .vtr file */
    PetscCall(DMStagVecSplitToDMDA(ctx->dm_stress, stress, DMSTAG_DOWN_LEFT, 0, &da_shear, &vec_shear));
    PetscCall(PetscObjectSetName((PetscObject)vec_shear, "shear stresses"));

    {
      PetscViewer viewer;
      char        filename[PETSC_MAX_PATH_LEN];

      PetscCall(PetscSNPrintf(filename, sizeof(filename), "ex6_stress_shear_%.4" PetscInt_FMT ".vtr", timestep));
      PetscCall(PetscViewerVTKOpen(PetscObjectComm((PetscObject)da_normal), filename, FILE_MODE_WRITE, &viewer));
      PetscCall(VecView(vec_shear, viewer));
      PetscCall(PetscViewerDestroy(&viewer));
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Created %s\n", filename));
    }
  }

  /* Destroy DMDAs and Vecs */
  PetscCall(DMDestroy(&da_normal));
  PetscCall(DMDestroy(&da_shear));
  PetscCall(VecDestroy(&vec_normal));
  PetscCall(VecDestroy(&vec_shear));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DumpVelocity(const Ctx *ctx, Vec velocity, PetscInt timestep)
{
  DM       dmVelAvg;
  Vec      velAvg;
  DM       daVelAvg;
  Vec      vecVelAvg;
  Vec      velocity_local;
  PetscInt ex, ey, ez, startx, starty, startz, nx, ny, nz;

  PetscFunctionBeginUser;

  if (ctx->dim == 2) {
    PetscCall(DMStagCreateCompatibleDMStag(ctx->dm_velocity, 0, 0, 2, 0, &dmVelAvg)); /* 2 dof per element */
  } else if (ctx->dim == 3) {
    PetscCall(DMStagCreateCompatibleDMStag(ctx->dm_velocity, 0, 0, 0, 3, &dmVelAvg)); /* 3 dof per element */
  } else SETERRQ(PetscObjectComm((PetscObject)ctx->dm_velocity), PETSC_ERR_SUP, "Unsupported dim %" PetscInt_FMT, ctx->dim);
  PetscCall(DMSetUp(dmVelAvg));
  PetscCall(DMStagSetUniformCoordinatesProduct(dmVelAvg, ctx->xmin, ctx->xmax, ctx->ymin, ctx->ymax, ctx->zmin, ctx->zmax));
  PetscCall(DMCreateGlobalVector(dmVelAvg, &velAvg));
  PetscCall(DMGetLocalVector(ctx->dm_velocity, &velocity_local));
  PetscCall(DMGlobalToLocal(ctx->dm_velocity, velocity, INSERT_VALUES, velocity_local));
  PetscCall(DMStagGetCorners(dmVelAvg, &startx, &starty, &startz, &nx, &ny, &nz, NULL, NULL, NULL));
  if (ctx->dim == 2) {
    for (ey = starty; ey < starty + ny; ++ey) {
      for (ex = startx; ex < startx + nx; ++ex) {
        DMStagStencil from[4], to[2];
        PetscScalar   valFrom[4], valTo[2];

        from[0].i   = ex;
        from[0].j   = ey;
        from[0].loc = DMSTAG_UP;
        from[0].c   = 0;
        from[1].i   = ex;
        from[1].j   = ey;
        from[1].loc = DMSTAG_DOWN;
        from[1].c   = 0;
        from[2].i   = ex;
        from[2].j   = ey;
        from[2].loc = DMSTAG_LEFT;
        from[2].c   = 0;
        from[3].i   = ex;
        from[3].j   = ey;
        from[3].loc = DMSTAG_RIGHT;
        from[3].c   = 0;
        PetscCall(DMStagVecGetValuesStencil(ctx->dm_velocity, velocity_local, 4, from, valFrom));
        to[0].i   = ex;
        to[0].j   = ey;
        to[0].loc = DMSTAG_ELEMENT;
        to[0].c   = 0;
        valTo[0]  = 0.5 * (valFrom[2] + valFrom[3]);
        to[1].i   = ex;
        to[1].j   = ey;
        to[1].loc = DMSTAG_ELEMENT;
        to[1].c   = 1;
        valTo[1]  = 0.5 * (valFrom[0] + valFrom[1]);
        PetscCall(DMStagVecSetValuesStencil(dmVelAvg, velAvg, 2, to, valTo, INSERT_VALUES));
      }
    }
  } else if (ctx->dim == 3) {
    for (ez = startz; ez < startz + nz; ++ez) {
      for (ey = starty; ey < starty + ny; ++ey) {
        for (ex = startx; ex < startx + nx; ++ex) {
          DMStagStencil from[6], to[3];
          PetscScalar   valFrom[6], valTo[3];

          from[0].i   = ex;
          from[0].j   = ey;
          from[0].k   = ez;
          from[0].loc = DMSTAG_UP;
          from[0].c   = 0;
          from[1].i   = ex;
          from[1].j   = ey;
          from[1].k   = ez;
          from[1].loc = DMSTAG_DOWN;
          from[1].c   = 0;
          from[2].i   = ex;
          from[2].j   = ey;
          from[2].k   = ez;
          from[2].loc = DMSTAG_LEFT;
          from[2].c   = 0;
          from[3].i   = ex;
          from[3].j   = ey;
          from[3].k   = ez;
          from[3].loc = DMSTAG_RIGHT;
          from[3].c   = 0;
          from[4].i   = ex;
          from[4].j   = ey;
          from[4].k   = ez;
          from[4].loc = DMSTAG_FRONT;
          from[4].c   = 0;
          from[5].i   = ex;
          from[5].j   = ey;
          from[5].k   = ez;
          from[5].loc = DMSTAG_BACK;
          from[5].c   = 0;
          PetscCall(DMStagVecGetValuesStencil(ctx->dm_velocity, velocity_local, 6, from, valFrom));
          to[0].i   = ex;
          to[0].j   = ey;
          to[0].k   = ez;
          to[0].loc = DMSTAG_ELEMENT;
          to[0].c   = 0;
          valTo[0]  = 0.5 * (valFrom[2] + valFrom[3]);
          to[1].i   = ex;
          to[1].j   = ey;
          to[1].k   = ez;
          to[1].loc = DMSTAG_ELEMENT;
          to[1].c   = 1;
          valTo[1]  = 0.5 * (valFrom[0] + valFrom[1]);
          to[2].i   = ex;
          to[2].j   = ey;
          to[2].k   = ez;
          to[2].loc = DMSTAG_ELEMENT;
          to[2].c   = 2;
          valTo[2]  = 0.5 * (valFrom[4] + valFrom[5]);
          PetscCall(DMStagVecSetValuesStencil(dmVelAvg, velAvg, 3, to, valTo, INSERT_VALUES));
        }
      }
    }
  } else SETERRQ(PetscObjectComm((PetscObject)ctx->dm_velocity), PETSC_ERR_SUP, "Unsupported dim %" PetscInt_FMT, ctx->dim);
  PetscCall(VecAssemblyBegin(velAvg));
  PetscCall(VecAssemblyEnd(velAvg));
  PetscCall(DMRestoreLocalVector(ctx->dm_velocity, &velocity_local));

  PetscCall(DMStagVecSplitToDMDA(dmVelAvg, velAvg, DMSTAG_ELEMENT, -3, &daVelAvg, &vecVelAvg)); /* note -3 : pad with zero in 2D case */
  PetscCall(PetscObjectSetName((PetscObject)vecVelAvg, "Velocity (Averaged)"));

  /* Dump element-based fields to a .vtr file */
  {
    PetscViewer viewer;
    char        filename[PETSC_MAX_PATH_LEN];

    PetscCall(PetscSNPrintf(filename, sizeof(filename), "ex6_velavg_%.4" PetscInt_FMT ".vtr", timestep));
    PetscCall(PetscViewerVTKOpen(PetscObjectComm((PetscObject)daVelAvg), filename, FILE_MODE_WRITE, &viewer));
    PetscCall(VecView(vecVelAvg, viewer));
    PetscCall(PetscViewerDestroy(&viewer));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Created %s\n", filename));
  }

  /* Destroy DMDAs and Vecs */
  PetscCall(VecDestroy(&vecVelAvg));
  PetscCall(DMDestroy(&daVelAvg));
  PetscCall(VecDestroy(&velAvg));
  PetscCall(DMDestroy(&dmVelAvg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*TEST

   test:
      suffix: 1
      requires: !complex
      nsize: 1
      args: -stag_grid_x 12 -stag_grid_y 7 -nsteps 4 -dump_output 0

   test:
      suffix: 2
      requires: !complex
      nsize: 9
      args: -stag_grid_x 12 -stag_grid_y 15 -nsteps 3 -dump_output 0

   test:
      suffix: 3
      requires: !complex
      nsize: 1
      args: -stag_grid_x 12 -stag_grid_y 7 -stag_grid_z 11 -nsteps 2 -dim 3 -dump_output 0

   test:
      suffix: 4
      requires: !complex
      nsize: 12
      args: -stag_grid_x 12 -stag_grid_y 15 -stag_grid_z 8 -nsteps 3 -dim 3 -dump_output 0

TEST*/

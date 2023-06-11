static char help[] = "Time dependent Navier-Stokes problem in 2d and 3d with finite elements.\n\
We solve the Navier-Stokes in a rectangular\n\
domain, using a parallel unstructured mesh (DMPLEX) to discretize it.\n\
This example supports discretized auxiliary fields (Re) as well as\n\
multilevel nonlinear solvers.\n\
Contributed by: Julian Andrej <juan@tf.uni-kiel.de>\n\n\n";

#include <petscdmplex.h>
#include <petscsnes.h>
#include <petscts.h>
#include <petscds.h>

/*
  Navier-Stokes equation:

  du/dt + u . grad u - \Delta u - grad p = f
  div u  = 0
*/

typedef struct {
  PetscInt mms;
} AppCtx;

#define REYN 400.0

/* MMS1

  u = t + x^2 + y^2;
  v = t + 2*x^2 - 2*x*y;
  p = x + y - 1;

  f_x = -2*t*(x + y) + 2*x*y^2 - 4*x^2*y - 2*x^3 + 4.0/Re - 1.0
  f_y = -2*t*x       + 2*y^3 - 4*x*y^2 - 2*x^2*y + 4.0/Re - 1.0

  so that

    u_t + u \cdot \nabla u - 1/Re \Delta u + \nabla p + f = <1, 1> + <t (2x + 2y) + 2x^3 + 4x^2y - 2xy^2, t 2x + 2x^2y + 4xy^2 - 2y^3> - 1/Re <4, 4> + <1, 1>
                                                    + <-t (2x + 2y) + 2xy^2 - 4x^2y - 2x^3 + 4/Re - 1, -2xt + 2y^3 - 4xy^2 - 2x^2y + 4/Re - 1> = 0
    \nabla \cdot u                                  = 2x - 2x = 0

  where

    <u, v> . <<u_x, v_x>, <u_y, v_y>> = <u u_x + v u_y, u v_x + v v_y>
*/
PetscErrorCode mms1_u_2d(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nf, PetscScalar *u, void *ctx)
{
  u[0] = time + x[0] * x[0] + x[1] * x[1];
  u[1] = time + 2.0 * x[0] * x[0] - 2.0 * x[0] * x[1];
  return PETSC_SUCCESS;
}

PetscErrorCode mms1_p_2d(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nf, PetscScalar *p, void *ctx)
{
  *p = x[0] + x[1] - 1.0;
  return PETSC_SUCCESS;
}

/* MMS 2*/

static PetscErrorCode mms2_u_2d(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nf, PetscScalar *u, void *ctx)
{
  u[0] = PetscSinReal(time + x[0]) * PetscSinReal(time + x[1]);
  u[1] = PetscCosReal(time + x[0]) * PetscCosReal(time + x[1]);
  return PETSC_SUCCESS;
}

static PetscErrorCode mms2_p_2d(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nf, PetscScalar *p, void *ctx)
{
  *p = PetscSinReal(time + x[0] - x[1]);
  return PETSC_SUCCESS;
}

static void f0_mms1_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal Re    = REYN;
  const PetscInt  Ncomp = dim;
  PetscInt        c, d;

  for (c = 0; c < Ncomp; ++c) {
    for (d = 0; d < dim; ++d) f0[c] += u[d] * u_x[c * dim + d];
  }
  f0[0] += u_t[0];
  f0[1] += u_t[1];

  f0[0] += -2.0 * t * (x[0] + x[1]) + 2.0 * x[0] * x[1] * x[1] - 4.0 * x[0] * x[0] * x[1] - 2.0 * x[0] * x[0] * x[0] + 4.0 / Re - 1.0;
  f0[1] += -2.0 * t * x[0] + 2.0 * x[1] * x[1] * x[1] - 4.0 * x[0] * x[1] * x[1] - 2.0 * x[0] * x[0] * x[1] + 4.0 / Re - 1.0;
}

static void f0_mms2_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal Re    = REYN;
  const PetscInt  Ncomp = dim;
  PetscInt        c, d;

  for (c = 0; c < Ncomp; ++c) {
    for (d = 0; d < dim; ++d) f0[c] += u[d] * u_x[c * dim + d];
  }
  f0[0] += u_t[0];
  f0[1] += u_t[1];

  f0[0] -= (Re * ((1.0L / 2.0L) * PetscSinReal(2 * t + 2 * x[0]) + PetscSinReal(2 * t + x[0] + x[1]) + PetscCosReal(t + x[0] - x[1])) + 2.0 * PetscSinReal(t + x[0]) * PetscSinReal(t + x[1])) / Re;
  f0[1] -= (-Re * ((1.0L / 2.0L) * PetscSinReal(2 * t + 2 * x[1]) + PetscSinReal(2 * t + x[0] + x[1]) + PetscCosReal(t + x[0] - x[1])) + 2.0 * PetscCosReal(t + x[0]) * PetscCosReal(t + x[1])) / Re;
}

static void f1_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f1[])
{
  const PetscReal Re    = REYN;
  const PetscInt  Ncomp = dim;
  PetscInt        comp, d;

  for (comp = 0; comp < Ncomp; ++comp) {
    for (d = 0; d < dim; ++d) f1[comp * dim + d] = 1.0 / Re * u_x[comp * dim + d];
    f1[comp * dim + comp] -= u[Ncomp];
  }
}

static void f0_p(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscInt d;
  for (d = 0, f0[0] = 0.0; d < dim; ++d) f0[0] += u_x[d * dim + d];
}

static void f1_p(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f1[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) f1[d] = 0.0;
}

/*
  (psi_i, u_j grad_j u_i) ==> (\psi_i, \phi_j grad_j u_i)
*/
static void g0_uu(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g0[])
{
  PetscInt NcI = dim, NcJ = dim;
  PetscInt fc, gc;
  PetscInt d;

  for (d = 0; d < dim; ++d) g0[d * dim + d] = u_tShift;

  for (fc = 0; fc < NcI; ++fc) {
    for (gc = 0; gc < NcJ; ++gc) g0[fc * NcJ + gc] += u_x[fc * NcJ + gc];
  }
}

/*
  (psi_i, u_j grad_j u_i) ==> (\psi_i, \u_j grad_j \phi_i)
*/
static void g1_uu(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g1[])
{
  PetscInt NcI = dim;
  PetscInt NcJ = dim;
  PetscInt fc, gc, dg;
  for (fc = 0; fc < NcI; ++fc) {
    for (gc = 0; gc < NcJ; ++gc) {
      for (dg = 0; dg < dim; ++dg) {
        /* kronecker delta */
        if (fc == gc) g1[(fc * NcJ + gc) * dim + dg] += u[dg];
      }
    }
  }
}

/* < q, \nabla\cdot u >
   NcompI = 1, NcompJ = dim */
static void g1_pu(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g1[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) g1[d * dim + d] = 1.0; /* \frac{\partial\phi^{u_d}}{\partial x_d} */
}

/* -< \nabla\cdot v, p >
    NcompI = dim, NcompJ = 1 */
static void g2_up(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g2[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) g2[d * dim + d] = -1.0; /* \frac{\partial\psi^{u_d}}{\partial x_d} */
}

/* < \nabla v, \nabla u + {\nabla u}^T >
   This just gives \nabla u, give the perdiagonal for the transpose */
static void g3_uu(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g3[])
{
  const PetscReal Re    = REYN;
  const PetscInt  Ncomp = dim;
  PetscInt        compI, d;

  for (compI = 0; compI < Ncomp; ++compI) {
    for (d = 0; d < dim; ++d) g3[((compI * Ncomp + compI) * dim + d) * dim + d] = 1.0 / Re;
  }
}

static PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscFunctionBeginUser;
  options->mms = 1;

  PetscOptionsBegin(comm, "", "Navier-Stokes Equation Options", "DMPLEX");
  PetscCall(PetscOptionsInt("-mms", "The manufactured solution to use", "ex46.c", options->mms, &options->mms, NULL));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CreateMesh(MPI_Comm comm, DM *dm, AppCtx *ctx)
{
  PetscFunctionBeginUser;
  PetscCall(DMCreate(comm, dm));
  PetscCall(DMSetType(*dm, DMPLEX));
  PetscCall(DMSetFromOptions(*dm));
  PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetupProblem(DM dm, AppCtx *ctx)
{
  PetscDS        ds;
  DMLabel        label;
  const PetscInt id = 1;
  PetscInt       dim;

  PetscFunctionBeginUser;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMGetDS(dm, &ds));
  PetscCall(DMGetLabel(dm, "marker", &label));
  switch (dim) {
  case 2:
    switch (ctx->mms) {
    case 1:
      PetscCall(PetscDSSetResidual(ds, 0, f0_mms1_u, f1_u));
      PetscCall(PetscDSSetResidual(ds, 1, f0_p, f1_p));
      PetscCall(PetscDSSetJacobian(ds, 0, 0, g0_uu, g1_uu, NULL, g3_uu));
      PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_up, NULL));
      PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_pu, NULL, NULL));
      PetscCall(PetscDSSetExactSolution(ds, 0, mms1_u_2d, ctx));
      PetscCall(PetscDSSetExactSolution(ds, 1, mms1_p_2d, ctx));
      PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall", label, 1, &id, 0, 0, NULL, (void (*)(void))mms1_u_2d, NULL, ctx, NULL));
      break;
    case 2:
      PetscCall(PetscDSSetResidual(ds, 0, f0_mms2_u, f1_u));
      PetscCall(PetscDSSetResidual(ds, 1, f0_p, f1_p));
      PetscCall(PetscDSSetJacobian(ds, 0, 0, g0_uu, g1_uu, NULL, g3_uu));
      PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_up, NULL));
      PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_pu, NULL, NULL));
      PetscCall(PetscDSSetExactSolution(ds, 0, mms2_u_2d, ctx));
      PetscCall(PetscDSSetExactSolution(ds, 1, mms2_p_2d, ctx));
      PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall", label, 1, &id, 0, 0, NULL, (void (*)(void))mms2_u_2d, NULL, ctx, NULL));
      break;
    default:
      SETERRQ(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "Invalid MMS %" PetscInt_FMT, ctx->mms);
    }
    break;
  default:
    SETERRQ(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "Invalid dimension %" PetscInt_FMT, dim);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetupDiscretization(DM dm, AppCtx *ctx)
{
  MPI_Comm  comm;
  DM        cdm = dm;
  PetscFE   fe[2];
  PetscInt  dim;
  PetscBool simplex;

  PetscFunctionBeginUser;
  PetscCall(PetscObjectGetComm((PetscObject)dm, &comm));
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMPlexIsSimplex(dm, &simplex));
  PetscCall(PetscFECreateDefault(comm, dim, dim, simplex, "vel_", PETSC_DEFAULT, &fe[0]));
  PetscCall(PetscObjectSetName((PetscObject)fe[0], "velocity"));
  PetscCall(PetscFECreateDefault(comm, dim, 1, simplex, "pres_", PETSC_DEFAULT, &fe[1]));
  PetscCall(PetscFECopyQuadrature(fe[0], fe[1]));
  PetscCall(PetscObjectSetName((PetscObject)fe[1], "pressure"));
  /* Set discretization and boundary conditions for each mesh */
  PetscCall(DMSetField(dm, 0, NULL, (PetscObject)fe[0]));
  PetscCall(DMSetField(dm, 1, NULL, (PetscObject)fe[1]));
  PetscCall(DMCreateDS(dm));
  PetscCall(SetupProblem(dm, ctx));
  while (cdm) {
    PetscObject  pressure;
    MatNullSpace nsp;

    PetscCall(DMGetField(cdm, 1, NULL, &pressure));
    PetscCall(MatNullSpaceCreate(PetscObjectComm(pressure), PETSC_TRUE, 0, NULL, &nsp));
    PetscCall(PetscObjectCompose(pressure, "nullspace", (PetscObject)nsp));
    PetscCall(MatNullSpaceDestroy(&nsp));

    PetscCall(DMCopyDisc(dm, cdm));
    PetscCall(DMGetCoarseDM(cdm, &cdm));
  }
  PetscCall(PetscFEDestroy(&fe[0]));
  PetscCall(PetscFEDestroy(&fe[1]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MonitorError(TS ts, PetscInt step, PetscReal crtime, Vec u, void *ctx)
{
  PetscSimplePointFunc funcs[2];
  void                *ctxs[2];
  DM                   dm;
  PetscDS              ds;
  PetscReal            ferrors[2];

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &dm));
  PetscCall(DMGetDS(dm, &ds));
  PetscCall(PetscDSGetExactSolution(ds, 0, &funcs[0], &ctxs[0]));
  PetscCall(PetscDSGetExactSolution(ds, 1, &funcs[1], &ctxs[1]));
  PetscCall(DMComputeL2FieldDiff(dm, crtime, funcs, ctxs, u, ferrors));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Timestep: %04d time = %-8.4g \t L_2 Error: [%2.3g, %2.3g]\n", (int)step, (double)crtime, (double)ferrors[0], (double)ferrors[1]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  AppCtx ctx;
  DM     dm;
  TS     ts;
  Vec    u, r;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  PetscCall(ProcessOptions(PETSC_COMM_WORLD, &ctx));
  PetscCall(CreateMesh(PETSC_COMM_WORLD, &dm, &ctx));
  PetscCall(DMSetApplicationContext(dm, &ctx));
  PetscCall(SetupDiscretization(dm, &ctx));
  PetscCall(DMPlexCreateClosureIndex(dm, NULL));

  PetscCall(DMCreateGlobalVector(dm, &u));
  PetscCall(VecDuplicate(u, &r));

  PetscCall(TSCreate(PETSC_COMM_WORLD, &ts));
  PetscCall(TSMonitorSet(ts, MonitorError, &ctx, NULL));
  PetscCall(TSSetDM(ts, dm));
  PetscCall(DMTSSetBoundaryLocal(dm, DMPlexTSComputeBoundary, &ctx));
  PetscCall(DMTSSetIFunctionLocal(dm, DMPlexTSComputeIFunctionFEM, &ctx));
  PetscCall(DMTSSetIJacobianLocal(dm, DMPlexTSComputeIJacobianFEM, &ctx));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_STEPOVER));
  PetscCall(TSSetFromOptions(ts));
  PetscCall(DMTSCheckFromOptions(ts, u));

  {
    PetscSimplePointFunc funcs[2];
    void                *ctxs[2];
    PetscDS              ds;

    PetscCall(DMGetDS(dm, &ds));
    PetscCall(PetscDSGetExactSolution(ds, 0, &funcs[0], &ctxs[0]));
    PetscCall(PetscDSGetExactSolution(ds, 1, &funcs[1], &ctxs[1]));
    PetscCall(DMProjectFunction(dm, 0.0, funcs, ctxs, INSERT_ALL_VALUES, u));
  }
  PetscCall(TSSolve(ts, u));
  PetscCall(VecViewFromOptions(u, NULL, "-sol_vec_view"));

  PetscCall(VecDestroy(&u));
  PetscCall(VecDestroy(&r));
  PetscCall(TSDestroy(&ts));
  PetscCall(DMDestroy(&dm));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

  # Full solves
  test:
    suffix: 2d_p2p1_r1
    requires: !single triangle
    filter: sed -e "s~ATOL~RTOL~g" -e "s~ABS~RELATIVE~g"
    args: -dm_refine 1 -vel_petscspace_degree 2 -pres_petscspace_degree 1 \
          -ts_type beuler -ts_max_steps 10 -ts_dt 0.1 -ts_monitor -dmts_check \
          -snes_monitor_short -snes_converged_reason \
          -ksp_monitor_short -ksp_converged_reason \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_fact_type full \
            -fieldsplit_velocity_pc_type lu \
            -fieldsplit_pressure_ksp_rtol 1.0e-10 -fieldsplit_pressure_pc_type jacobi

  test:
    suffix: 2d_q2q1_r1
    requires: !single
    filter: sed -e "s~ATOL~RTOL~g" -e "s~ABS~RELATIVE~g" -e "s~ 0\]~ 0.0\]~g"
    args: -dm_plex_simplex 0 -dm_refine 1 -vel_petscspace_degree 2 -pres_petscspace_degree 1 \
          -ts_type beuler -ts_max_steps 10 -ts_dt 0.1 -ts_monitor -dmts_check \
          -snes_monitor_short -snes_converged_reason \
          -ksp_monitor_short -ksp_converged_reason \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_fact_type full \
            -fieldsplit_velocity_pc_type lu \
            -fieldsplit_pressure_ksp_rtol 1.0e-10 -fieldsplit_pressure_pc_type jacobi

TEST*/

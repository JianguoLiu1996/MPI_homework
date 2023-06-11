static char help[] = "Example application of the Bhatnagar-Gross-Krook (BGK) collision operator.\n\
This example is a 0D-1V setting for the kinetic equation\n\
https://en.wikipedia.org/wiki/Bhatnagar%E2%80%93Gross%E2%80%93Krook_operator\n";

#include <petscdmplex.h>
#include <petscdmswarm.h>
#include <petscts.h>
#include <petscdraw.h>
#include <petscviewer.h>

typedef struct {
  PetscInt    particlesPerCell; /* The number of partices per cell */
  PetscReal   momentTol;        /* Tolerance for checking moment conservation */
  PetscBool   monitorhg;        /* Flag for using the TS histogram monitor */
  PetscBool   monitorsp;        /* Flag for using the TS scatter monitor */
  PetscBool   monitorks;        /* Monitor to perform KS test to the maxwellian */
  PetscBool   error;            /* Flag for printing the error */
  PetscInt    ostep;            /* print the energy at each ostep time steps */
  PetscDraw   draw;             /* The draw object for histogram monitoring */
  PetscDrawHG drawhg;           /* The histogram draw context for monitoring */
  PetscDrawSP drawsp;           /* The scatter plot draw context for the monitor */
  PetscDrawSP drawks;           /* Scatterplot draw context for KS test */
} AppCtx;

static PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscFunctionBeginUser;
  options->monitorhg        = PETSC_FALSE;
  options->monitorsp        = PETSC_FALSE;
  options->monitorks        = PETSC_FALSE;
  options->particlesPerCell = 1;
  options->momentTol        = 100.0 * PETSC_MACHINE_EPSILON;
  options->ostep            = 100;

  PetscOptionsBegin(comm, "", "Collision Options", "DMPLEX");
  PetscCall(PetscOptionsBool("-monitorhg", "Flag to use the TS histogram monitor", "ex28.c", options->monitorhg, &options->monitorhg, NULL));
  PetscCall(PetscOptionsBool("-monitorsp", "Flag to use the TS scatter plot monitor", "ex28.c", options->monitorsp, &options->monitorsp, NULL));
  PetscCall(PetscOptionsBool("-monitorks", "Flag to plot KS test results", "ex28.c", options->monitorks, &options->monitorks, NULL));
  PetscCall(PetscOptionsInt("-particles_per_cell", "Number of particles per cell", "ex28.c", options->particlesPerCell, &options->particlesPerCell, NULL));
  PetscCall(PetscOptionsInt("-output_step", "Number of time steps between output", "ex28.c", options->ostep, &options->ostep, NULL));
  PetscOptionsEnd();

  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Create the mesh for velocity space */
static PetscErrorCode CreateMesh(MPI_Comm comm, DM *dm, AppCtx *user)
{
  PetscFunctionBeginUser;
  PetscCall(DMCreate(comm, dm));
  PetscCall(DMSetType(*dm, DMPLEX));
  PetscCall(DMSetFromOptions(*dm));
  PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Since we are putting the same number of particles in each cell, this amounts to a uniform distribution of v */
static PetscErrorCode SetInitialCoordinates(DM sw)
{
  AppCtx        *user;
  PetscRandom    rnd;
  DM             dm;
  DMPolytopeType ct;
  PetscBool      simplex;
  PetscReal     *centroid, *coords, *xi0, *v0, *J, *invJ, detJ, *vals;
  PetscInt       dim, d, cStart, cEnd, c, Np, p;

  PetscFunctionBeginUser;
  PetscCall(PetscRandomCreate(PetscObjectComm((PetscObject)sw), &rnd));
  PetscCall(PetscRandomSetInterval(rnd, -1.0, 1.0));
  PetscCall(PetscRandomSetFromOptions(rnd));

  PetscCall(DMGetApplicationContext(sw, &user));
  Np = user->particlesPerCell;
  PetscCall(DMGetDimension(sw, &dim));
  PetscCall(DMSwarmGetCellDM(sw, &dm));
  PetscCall(DMGetCoordinatesLocalSetUp(dm));
  PetscCall(DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd));
  PetscCall(DMPlexGetCellType(dm, cStart, &ct));
  simplex = DMPolytopeTypeGetNumVertices(ct) == DMPolytopeTypeGetDim(ct) + 1 ? PETSC_TRUE : PETSC_FALSE;
  PetscCall(PetscMalloc5(dim, &centroid, dim, &xi0, dim, &v0, dim * dim, &J, dim * dim, &invJ));
  for (d = 0; d < dim; ++d) xi0[d] = -1.0;
  PetscCall(DMSwarmGetField(sw, DMSwarmPICField_coor, NULL, NULL, (void **)&coords));
  PetscCall(DMSwarmGetField(sw, "w_q", NULL, NULL, (void **)&vals));
  for (c = cStart; c < cEnd; ++c) {
    if (Np == 1) {
      PetscCall(DMPlexComputeCellGeometryFVM(dm, c, NULL, centroid, NULL));
      for (d = 0; d < dim; ++d) {
        coords[c * dim + d] = centroid[d];
        if ((coords[c * dim + d] >= -1) && (coords[c * dim + d] <= 1)) {
          vals[c] = 1.0;
        } else {
          vals[c] = 0.;
        }
      }
    } else {
      PetscCall(DMPlexComputeCellGeometryFEM(dm, c, NULL, v0, J, invJ, &detJ)); /* affine */
      for (p = 0; p < Np; ++p) {
        const PetscInt n   = c * Np + p;
        PetscReal      sum = 0.0, refcoords[3];

        for (d = 0; d < dim; ++d) {
          PetscCall(PetscRandomGetValueReal(rnd, &refcoords[d]));
          sum += refcoords[d];
        }
        if (simplex && sum > 0.0)
          for (d = 0; d < dim; ++d) refcoords[d] -= PetscSqrtReal(dim) * sum;
        vals[n] = 1.0;
        PetscCall(DMPlexReferenceToCoordinates(dm, c, 1, refcoords, &coords[n * dim]));
      }
    }
  }
  PetscCall(DMSwarmRestoreField(sw, DMSwarmPICField_coor, NULL, NULL, (void **)&coords));
  PetscCall(DMSwarmRestoreField(sw, "w_q", NULL, NULL, (void **)&vals));
  PetscCall(PetscFree5(centroid, xi0, v0, J, invJ));
  PetscCall(PetscRandomDestroy(&rnd));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* The initial conditions are just the initial particle weights */
static PetscErrorCode SetInitialConditions(DM dmSw, Vec u)
{
  DM           dm;
  AppCtx      *user;
  PetscReal   *vals;
  PetscScalar *initialConditions;
  PetscInt     dim, d, cStart, cEnd, c, Np, p, n;

  PetscFunctionBeginUser;
  PetscCall(VecGetLocalSize(u, &n));
  PetscCall(DMGetApplicationContext(dmSw, &user));
  Np = user->particlesPerCell;
  PetscCall(DMSwarmGetCellDM(dmSw, &dm));
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd));
  PetscCheck(n == (cEnd - cStart) * Np, PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "TS solution local size %" PetscInt_FMT " != %" PetscInt_FMT " nm particles", n, (cEnd - cStart) * Np);
  PetscCall(DMSwarmGetField(dmSw, "w_q", NULL, NULL, (void **)&vals));
  PetscCall(VecGetArray(u, &initialConditions));
  for (c = cStart; c < cEnd; ++c) {
    for (p = 0; p < Np; ++p) {
      const PetscInt n = c * Np + p;
      for (d = 0; d < dim; d++) initialConditions[n] = vals[n];
    }
  }
  PetscCall(VecRestoreArray(u, &initialConditions));
  PetscCall(DMSwarmRestoreField(dmSw, "w_q", NULL, NULL, (void **)&vals));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CreateParticles(DM dm, DM *sw, AppCtx *user)
{
  PetscInt *cellid;
  PetscInt  dim, cStart, cEnd, c, Np = user->particlesPerCell, p;

  PetscFunctionBeginUser;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMCreate(PetscObjectComm((PetscObject)dm), sw));
  PetscCall(DMSetType(*sw, DMSWARM));
  PetscCall(DMSetDimension(*sw, dim));
  PetscCall(DMSwarmSetType(*sw, DMSWARM_PIC));
  PetscCall(DMSwarmSetCellDM(*sw, dm));
  PetscCall(DMSwarmRegisterPetscDatatypeField(*sw, "kinematics", dim, PETSC_REAL));
  PetscCall(DMSwarmRegisterPetscDatatypeField(*sw, "w_q", 1, PETSC_SCALAR));
  PetscCall(DMSwarmFinalizeFieldRegister(*sw));
  PetscCall(DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd));
  PetscCall(DMSwarmSetLocalSizes(*sw, (cEnd - cStart) * Np, 0));
  PetscCall(DMSetFromOptions(*sw));
  PetscCall(DMSwarmGetField(*sw, DMSwarmPICField_cellid, NULL, NULL, (void **)&cellid));
  for (c = cStart; c < cEnd; ++c) {
    for (p = 0; p < Np; ++p) {
      const PetscInt n = c * Np + p;
      cellid[n]        = c;
    }
  }
  PetscCall(DMSwarmRestoreField(*sw, DMSwarmPICField_cellid, NULL, NULL, (void **)&cellid));
  PetscCall(PetscObjectSetName((PetscObject)*sw, "Particles"));
  PetscCall(DMViewFromOptions(*sw, NULL, "-sw_view"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  f_t = 1/\tau \left( f_eq - f \right)
  n_t = 1/\tau \left( \int f_eq - \int f \right) = 1/\tau (n - n) = 0
  v_t = 1/\tau \left( \int v f_eq - \int v f \right) = 1/\tau (v - v) = 0
  E_t = 1/\tau \left( \int v^2 f_eq - \int v^2 f \right) = 1/\tau (T - T) = 0

  Let's look at a single cell:

    \int_C f_t             = 1/\tau \left( \int_C f_eq - \int_C f \right)
    \sum_{x_i \in C} w^i_t = 1/\tau \left( F_eq(C) - \sum_{x_i \in C} w_i \right)
*/

/* This computes the 1D Maxwellian distribution for given mass n, velocity v, and temperature T */
static PetscReal ComputePDF(PetscReal m, PetscReal n, PetscReal T, PetscReal v[])
{
  return (n / PetscSqrtReal(2.0 * PETSC_PI * T / m)) * PetscExpReal(-0.5 * m * PetscSqr(v[0]) / T);
}

/*
  erf z = \frac{2}{\sqrt\pi} \int^z_0 dt e^{-t^2} and erf \infty = 1

  We begin with our distribution

    \sqrt{\frac{m}{2 \pi T}} e^{-m v^2/2T}

  Let t = \sqrt{m/2T} v, z = \sqrt{m/2T} w, so that we now have

      \sqrt{\frac{m}{2 \pi T}} \int^w_0 dv e^{-m v^2/2T}
    = \sqrt{\frac{m}{2 \pi T}} \int^{\sqrt{m/2T} w}_0 \sqrt{2T/m} dt e^{-t^2}
    = 1/\sqrt{\pi} \int^{\sqrt{m/2T} w}_0 dt e^{-t^2}
    = 1/2 erf(\sqrt{m/2T} w)
*/
static PetscReal ComputeCDF(PetscReal m, PetscReal n, PetscReal T, PetscReal va, PetscReal vb)
{
  PetscReal alpha = PetscSqrtReal(0.5 * m / T);
  PetscReal za    = alpha * va;
  PetscReal zb    = alpha * vb;
  PetscReal sum   = 0.0;

  sum += zb >= 0 ? erf(zb) : -erf(-zb);
  sum -= za >= 0 ? erf(za) : -erf(-za);
  return 0.5 * n * sum;
}

static PetscErrorCode CheckDistribution(DM dm, PetscReal m, PetscReal n, PetscReal T, PetscReal v[])
{
  PetscSection coordSection;
  Vec          coordsLocal;
  PetscReal   *xq, *wq;
  PetscReal    vmin, vmax, neq, veq, Teq;
  PetscInt     Nq = 100, q, cStart, cEnd, c;

  PetscFunctionBeginUser;
  PetscCall(DMGetBoundingBox(dm, &vmin, &vmax));
  /* Check analytic over entire line */
  neq = ComputeCDF(m, n, T, vmin, vmax);
  PetscCheck(PetscAbsReal(neq - n) <= PETSC_SMALL, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Int f %g != %g mass (%g)", (double)neq, (double)n, (double)(neq - n));
  /* Check analytic over cells */
  PetscCall(DMGetCoordinatesLocal(dm, &coordsLocal));
  PetscCall(DMGetCoordinateSection(dm, &coordSection));
  PetscCall(DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd));
  neq = 0.0;
  for (c = cStart; c < cEnd; ++c) {
    PetscScalar *vcoords = NULL;

    PetscCall(DMPlexVecGetClosure(dm, coordSection, coordsLocal, c, NULL, &vcoords));
    neq += ComputeCDF(m, n, T, vcoords[0], vcoords[1]);
    PetscCall(DMPlexVecRestoreClosure(dm, coordSection, coordsLocal, c, NULL, &vcoords));
  }
  PetscCheck(PetscAbsReal(neq - n) <= PETSC_SMALL, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Cell Int f %g != %g mass (%g)", (double)neq, (double)n, (double)(neq - n));
  /* Check quadrature over entire line */
  PetscCall(PetscMalloc2(Nq, &xq, Nq, &wq));
  PetscCall(PetscDTGaussQuadrature(100, vmin, vmax, xq, wq));
  neq = 0.0;
  for (q = 0; q < Nq; ++q) neq += ComputePDF(m, n, T, &xq[q]) * wq[q];
  PetscCheck(PetscAbsReal(neq - n) <= PETSC_SMALL, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Int f %g != %g mass (%g)", (double)neq, (double)n, (double)(neq - n));
  /* Check omemnts with quadrature */
  veq = 0.0;
  for (q = 0; q < Nq; ++q) veq += xq[q] * ComputePDF(m, n, T, &xq[q]) * wq[q];
  veq /= neq;
  PetscCheck(PetscAbsReal(veq - v[0]) <= PETSC_SMALL, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Int v f %g != %g velocity (%g)", (double)veq, (double)v[0], (double)(veq - v[0]));
  Teq = 0.0;
  for (q = 0; q < Nq; ++q) Teq += PetscSqr(xq[q]) * ComputePDF(m, n, T, &xq[q]) * wq[q];
  Teq = Teq * m / neq - PetscSqr(veq);
  PetscCheck(PetscAbsReal(Teq - T) <= PETSC_SMALL, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Int v^2 f %g != %g temperature (%g)", (double)Teq, (double)T, (double)(Teq - T));
  PetscCall(PetscFree2(xq, wq));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode RHSFunctionParticles(TS ts, PetscReal t, Vec U, Vec R, void *ctx)
{
  const PetscScalar *u;
  PetscSection       coordSection;
  Vec                coordsLocal;
  PetscScalar       *r, *coords;
  PetscReal          n = 0.0, v = 0.0, E = 0.0, T = 0.0, m = 1.0, cn = 0.0, cv = 0.0, cE = 0.0, pE = 0.0, eqE = 0.0;
  PetscInt           dim, d, Np, Ncp, p, cStart, cEnd, c;
  DM                 dmSw, plex;

  PetscFunctionBeginUser;
  PetscCall(VecGetLocalSize(U, &Np));
  PetscCall(VecGetArrayRead(U, &u));
  PetscCall(VecGetArray(R, &r));
  PetscCall(TSGetDM(ts, &dmSw));
  PetscCall(DMSwarmGetCellDM(dmSw, &plex));
  PetscCall(DMGetDimension(dmSw, &dim));
  PetscCall(DMGetCoordinatesLocal(plex, &coordsLocal));
  PetscCall(DMGetCoordinateSection(plex, &coordSection));
  PetscCall(DMPlexGetHeightStratum(plex, 0, &cStart, &cEnd));
  Np /= dim;
  Ncp = Np / (cEnd - cStart);
  /* Calculate moments of particle distribution, note that velocity is in the coordinate */
  PetscCall(DMSwarmGetField(dmSw, DMSwarmPICField_coor, NULL, NULL, (void **)&coords));
  for (p = 0; p < Np; ++p) {
    PetscReal m1 = 0.0, m2 = 0.0;

    for (d = 0; d < dim; ++d) {
      m1 += PetscRealPart(coords[p * dim + d]);
      m2 += PetscSqr(PetscRealPart(coords[p * dim + d]));
    }
    n += u[p];
    v += u[p] * m1;
    E += u[p] * m2;
  }
  v /= n;
  T = E * m / n - v * v;
  PetscCall(PetscInfo(ts, "Time %.2f: mass %.4f velocity: %+.4f temperature: %.4f\n", (double)t, (double)n, (double)v, (double)T));
  PetscCall(CheckDistribution(plex, m, n, T, &v));
  /*
     Begin cellwise evaluation of the collision operator. Essentially, penalize the weights of the particles
     in that cell against the maxwellian for the number of particles expected to be in that cell
  */
  for (c = cStart; c < cEnd; ++c) {
    PetscScalar *vcoords    = NULL;
    PetscReal    relaxation = 1.0, neq;
    PetscInt     sp         = c * Ncp, q;

    /* Calculate equilibrium occupation for this velocity cell */
    PetscCall(DMPlexVecGetClosure(plex, coordSection, coordsLocal, c, NULL, &vcoords));
    neq = ComputeCDF(m, n, T, vcoords[0], vcoords[1]);
    PetscCall(DMPlexVecRestoreClosure(plex, coordSection, coordsLocal, c, NULL, &vcoords));
    for (q = 0; q < Ncp; ++q) r[sp + q] = (1.0 / relaxation) * (neq - u[sp + q]);
  }
  /* Check update */
  for (p = 0; p < Np; ++p) {
    PetscReal    m1 = 0.0, m2 = 0.0;
    PetscScalar *vcoords = NULL;

    for (d = 0; d < dim; ++d) {
      m1 += PetscRealPart(coords[p * dim + d]);
      m2 += PetscSqr(PetscRealPart(coords[p * dim + d]));
    }
    cn += r[p];
    cv += r[p] * m1;
    cE += r[p] * m2;
    pE += u[p] * m2;
    PetscCall(DMPlexVecGetClosure(plex, coordSection, coordsLocal, p, NULL, &vcoords));
    eqE += ComputeCDF(m, n, T, vcoords[0], vcoords[1]) * m2;
    PetscCall(DMPlexVecRestoreClosure(plex, coordSection, coordsLocal, p, NULL, &vcoords));
  }
  PetscCall(PetscInfo(ts, "Time %.2f: mass update %.8f velocity update: %+.8f energy update: %.8f (%.8f, %.8f)\n", (double)t, (double)cn, (double)cv, (double)cE, (double)pE, (double)eqE));
  PetscCall(DMSwarmRestoreField(dmSw, DMSwarmPICField_coor, NULL, NULL, (void **)&coords));
  PetscCall(VecRestoreArrayRead(U, &u));
  PetscCall(VecRestoreArray(R, &r));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode HGMonitor(TS ts, PetscInt step, PetscReal t, Vec U, void *ctx)
{
  AppCtx            *user = (AppCtx *)ctx;
  const PetscScalar *u;
  DM                 sw, dm;
  PetscInt           dim, Np, p;

  PetscFunctionBeginUser;
  if (step < 0) PetscFunctionReturn(PETSC_SUCCESS);
  if (((user->ostep > 0) && (!(step % user->ostep)))) {
    PetscDrawAxis axis;

    PetscCall(TSGetDM(ts, &sw));
    PetscCall(DMSwarmGetCellDM(sw, &dm));
    PetscCall(DMGetDimension(dm, &dim));
    PetscCall(PetscDrawHGReset(user->drawhg));
    PetscCall(PetscDrawHGGetAxis(user->drawhg, &axis));
    PetscCall(PetscDrawAxisSetLabels(axis, "Particles", "V", "f(V)"));
    PetscCall(PetscDrawAxisSetLimits(axis, -3, 3, 0, 100));
    PetscCall(PetscDrawHGSetLimits(user->drawhg, -3.0, 3.0, 0, 10));
    PetscCall(VecGetLocalSize(U, &Np));
    Np /= dim;
    PetscCall(VecGetArrayRead(U, &u));
    /* get points from solution vector */
    for (p = 0; p < Np; ++p) PetscCall(PetscDrawHGAddValue(user->drawhg, u[p]));
    PetscCall(PetscDrawHGDraw(user->drawhg));
    PetscCall(VecRestoreArrayRead(U, &u));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SPMonitor(TS ts, PetscInt step, PetscReal t, Vec U, void *ctx)
{
  AppCtx            *user = (AppCtx *)ctx;
  const PetscScalar *u;
  PetscReal         *v, *coords;
  PetscInt           Np, p;
  DM                 dmSw;

  PetscFunctionBeginUser;

  if (step < 0) PetscFunctionReturn(PETSC_SUCCESS);
  if (((user->ostep > 0) && (!(step % user->ostep)))) {
    PetscDrawAxis axis;

    PetscCall(TSGetDM(ts, &dmSw));
    PetscCall(PetscDrawSPReset(user->drawsp));
    PetscCall(PetscDrawSPGetAxis(user->drawsp, &axis));
    PetscCall(PetscDrawAxisSetLabels(axis, "Particles", "V", "w"));
    PetscCall(VecGetLocalSize(U, &Np));
    PetscCall(VecGetArrayRead(U, &u));
    /* get points from solution vector */
    PetscCall(PetscMalloc1(Np, &v));
    for (p = 0; p < Np; ++p) v[p] = PetscRealPart(u[p]);
    PetscCall(DMSwarmGetField(dmSw, DMSwarmPICField_coor, NULL, NULL, (void **)&coords));
    for (p = 0; p < Np - 1; ++p) PetscCall(PetscDrawSPAddPoint(user->drawsp, &coords[p], &v[p]));
    PetscCall(PetscDrawSPDraw(user->drawsp, PETSC_TRUE));
    PetscCall(VecRestoreArrayRead(U, &u));
    PetscCall(DMSwarmRestoreField(dmSw, DMSwarmPICField_coor, NULL, NULL, (void **)&coords));
    PetscCall(PetscFree(v));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSConv(TS ts, PetscInt step, PetscReal t, Vec U, void *ctx)
{
  AppCtx            *user = (AppCtx *)ctx;
  const PetscScalar *u;
  PetscScalar        sup;
  PetscReal         *v, *coords, T = 0., vel = 0., step_cast, w_sum;
  PetscInt           dim, Np, p, cStart, cEnd;
  DM                 sw, plex;

  PetscFunctionBeginUser;
  if (step < 0) PetscFunctionReturn(PETSC_SUCCESS);
  if (((user->ostep > 0) && (!(step % user->ostep)))) {
    PetscDrawAxis axis;
    PetscCall(PetscDrawSPGetAxis(user->drawks, &axis));
    PetscCall(PetscDrawAxisSetLabels(axis, "Particles", "t", "D_n"));
    PetscCall(PetscDrawSPSetLimits(user->drawks, 0., 100, 0., 3.5));
    PetscCall(TSGetDM(ts, &sw));
    PetscCall(VecGetLocalSize(U, &Np));
    PetscCall(VecGetArrayRead(U, &u));
    /* get points from solution vector */
    PetscCall(PetscMalloc1(Np, &v));
    PetscCall(DMSwarmGetCellDM(sw, &plex));
    PetscCall(DMGetDimension(sw, &dim));
    PetscCall(DMPlexGetHeightStratum(plex, 0, &cStart, &cEnd));
    for (p = 0; p < Np; ++p) v[p] = PetscRealPart(u[p]);
    PetscCall(DMSwarmGetField(sw, DMSwarmPICField_coor, NULL, NULL, (void **)&coords));
    w_sum = 0.;
    for (p = 0; p < Np; ++p) {
      w_sum += u[p];
      T += u[p] * coords[p] * coords[p];
      vel += u[p] * coords[p];
    }
    vel /= w_sum;
    T   = T / w_sum - vel * vel;
    sup = 0.0;
    for (p = 0; p < Np; ++p) {
      PetscReal tmp = 0.;
      tmp           = PetscAbs(u[p] - ComputePDF(1.0, w_sum, T, &coords[p * dim]));
      if (tmp > sup) sup = tmp;
    }
    step_cast = (PetscReal)step;
    PetscCall(PetscDrawSPAddPoint(user->drawks, &step_cast, &sup));
    PetscCall(PetscDrawSPDraw(user->drawks, PETSC_TRUE));
    PetscCall(VecRestoreArrayRead(U, &u));
    PetscCall(DMSwarmRestoreField(sw, DMSwarmPICField_coor, NULL, NULL, (void **)&coords));
    PetscCall(PetscFree(v));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode InitializeSolve(TS ts, Vec u)
{
  DM      dm;
  AppCtx *user;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &dm));
  PetscCall(DMGetApplicationContext(dm, &user));
  PetscCall(SetInitialCoordinates(dm));
  PetscCall(SetInitialConditions(dm, u));
  PetscFunctionReturn(PETSC_SUCCESS);
}
/*
     A single particle is generated for each velocity space cell using the dmswarmpicfield_coor and is used to evaluate collisions in that cell.
     0 weight ghost particles are initialized outside of a small velocity domain to ensure the tails of the amxwellian are resolved.
   */
int main(int argc, char **argv)
{
  TS       ts;     /* nonlinear solver */
  DM       dm, sw; /* Velocity space mesh and Particle Swarm */
  Vec      u, w;   /* swarm vector */
  MPI_Comm comm;
  AppCtx   user;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  comm = PETSC_COMM_WORLD;
  PetscCall(ProcessOptions(comm, &user));
  PetscCall(CreateMesh(comm, &dm, &user));
  PetscCall(CreateParticles(dm, &sw, &user));
  PetscCall(DMSetApplicationContext(sw, &user));
  PetscCall(TSCreate(comm, &ts));
  PetscCall(TSSetDM(ts, sw));
  PetscCall(TSSetMaxTime(ts, 10.0));
  PetscCall(TSSetTimeStep(ts, 0.01));
  PetscCall(TSSetMaxSteps(ts, 100000));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_MATCHSTEP));
  if (user.monitorhg) {
    PetscCall(PetscDrawCreate(comm, NULL, "monitor", 0, 0, 400, 300, &user.draw));
    PetscCall(PetscDrawSetFromOptions(user.draw));
    PetscCall(PetscDrawHGCreate(user.draw, 20, &user.drawhg));
    PetscCall(PetscDrawHGSetColor(user.drawhg, 3));
    PetscCall(TSMonitorSet(ts, HGMonitor, &user, NULL));
  } else if (user.monitorsp) {
    PetscCall(PetscDrawCreate(comm, NULL, "monitor", 0, 0, 400, 300, &user.draw));
    PetscCall(PetscDrawSetFromOptions(user.draw));
    PetscCall(PetscDrawSPCreate(user.draw, 1, &user.drawsp));
    PetscCall(TSMonitorSet(ts, SPMonitor, &user, NULL));
  } else if (user.monitorks) {
    PetscCall(PetscDrawCreate(comm, NULL, "monitor", 0, 0, 400, 300, &user.draw));
    PetscCall(PetscDrawSetFromOptions(user.draw));
    PetscCall(PetscDrawSPCreate(user.draw, 1, &user.drawks));
    PetscCall(TSMonitorSet(ts, KSConv, &user, NULL));
  }
  PetscCall(TSSetRHSFunction(ts, NULL, RHSFunctionParticles, &user));
  PetscCall(TSSetFromOptions(ts));
  PetscCall(TSSetComputeInitialCondition(ts, InitializeSolve));
  PetscCall(DMSwarmCreateGlobalVectorFromField(sw, "w_q", &w));
  PetscCall(VecDuplicate(w, &u));
  PetscCall(VecCopy(w, u));
  PetscCall(DMSwarmDestroyGlobalVectorFromField(sw, "w_q", &w));
  PetscCall(TSComputeInitialCondition(ts, u));
  PetscCall(TSSolve(ts, u));
  if (user.monitorhg) {
    PetscCall(PetscDrawSave(user.draw));
    PetscCall(PetscDrawHGDestroy(&user.drawhg));
    PetscCall(PetscDrawDestroy(&user.draw));
  }
  if (user.monitorsp) {
    PetscCall(PetscDrawSave(user.draw));
    PetscCall(PetscDrawSPDestroy(&user.drawsp));
    PetscCall(PetscDrawDestroy(&user.draw));
  }
  if (user.monitorks) {
    PetscCall(PetscDrawSave(user.draw));
    PetscCall(PetscDrawSPDestroy(&user.drawks));
    PetscCall(PetscDrawDestroy(&user.draw));
  }
  PetscCall(VecDestroy(&u));
  PetscCall(TSDestroy(&ts));
  PetscCall(DMDestroy(&sw));
  PetscCall(DMDestroy(&dm));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST
   build:
     requires: double !complex
   test:
     suffix: 1
     args: -particles_per_cell 1 -output_step 10 -ts_type euler -dm_plex_dim 1 -dm_plex_box_faces 200 -dm_plex_box_lower -10 -dm_plex_box_upper 10 -dm_view -monitorsp
   test:
     suffix: 2
     args: -particles_per_cell 1 -output_step 50 -ts_type euler -dm_plex_dim 1 -dm_plex_box_faces 200 -dm_plex_box_lower -10 -dm_plex_box_upper 10 -dm_view -monitorks
TEST*/

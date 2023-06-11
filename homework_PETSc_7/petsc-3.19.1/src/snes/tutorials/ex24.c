static char help[] = "Poisson Problem in mixed form with 2d and 3d with finite elements.\n\
We solve the Poisson problem in a rectangular\n\
domain, using a parallel unstructured mesh (DMPLEX) to discretize it.\n\
This example supports automatic convergence estimation\n\
and Hdiv elements.\n\n\n";

/*

The mixed form of Poisson's equation, e.g. https://firedrakeproject.org/demos/poisson_mixed.py.html, is given
in the strong form by
\begin{align}
  \vb{q} - \nabla u   &= 0 \\
  \nabla \cdot \vb{q} &= f
\end{align}
where $u$ is the potential, as in the original problem, but we also discretize the gradient of potential $\vb{q}$,
or flux, directly. The weak form is then
\begin{align}
  <t, \vb{q}> + <\nabla \cdot t, u> - <t_n, u>_\Gamma &= 0 \\
  <v, \nabla \cdot \vb{q}> &= <v, f>
\end{align}
For the original Poisson problem, the Dirichlet boundary forces an essential boundary condition on the potential space,
and the Neumann boundary gives a natural boundary condition in the weak form. In the mixed formulation, the Neumann
boundary gives an essential boundary condition on the flux space, $\vb{q} \cdot \vu{n} = h$, and the Dirichlet condition
becomes a natural condition in the weak form, <t_n, g>_\Gamma.
*/

#include <petscdmplex.h>
#include <petscsnes.h>
#include <petscds.h>
#include <petscconvest.h>

typedef enum {
  SOL_LINEAR,
  SOL_QUADRATIC,
  SOL_QUARTIC,
  SOL_QUARTIC_NEUMANN,
  SOL_UNKNOWN,
  NUM_SOLTYPE
} SolType;
const char *SolTypeNames[NUM_SOLTYPE + 3] = {"linear", "quadratic", "quartic", "quartic_neumann", "unknown", "SolType", "SOL_", NULL};

typedef struct {
  SolType solType; /* The type of exact solution */
} AppCtx;

static void f0_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) f0[0] += u_x[uOff_x[0] + d * dim + d];
}

/* 2D Dirichlet potential example

  u = x
  q = <1, 0>
  f = 0

  We will need a boundary integral of u over \Gamma.
*/
static PetscErrorCode linear_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  u[0] = x[0];
  return PETSC_SUCCESS;
}
static PetscErrorCode linear_q(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt c;
  for (c = 0; c < Nc; ++c) u[c] = c ? 0.0 : 1.0;
  return PETSC_SUCCESS;
}

static void f0_linear_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  f0[0] = 0.0;
  f0_u(dim, Nf, NfAux, uOff, uOff_x, u, u_t, u_x, aOff, aOff_x, a, a_t, a_x, t, x, numConstants, constants, f0);
}
static void f0_bd_linear_q(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], const PetscReal n[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscScalar potential;
  PetscInt    d;

  PetscCallAbort(PETSC_COMM_SELF, linear_u(dim, t, x, dim, &potential, NULL));
  for (d = 0; d < dim; ++d) f0[d] = -potential * n[d];
}

/* 2D Dirichlet potential example

  u = x^2 + y^2
  q = <2x, 2y>
  f = 4

  We will need a boundary integral of u over \Gamma.
*/
static PetscErrorCode quadratic_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt d;

  u[0] = 0.0;
  for (d = 0; d < dim; ++d) u[0] += x[d] * x[d];
  return PETSC_SUCCESS;
}
static PetscErrorCode quadratic_q(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt c;
  for (c = 0; c < Nc; ++c) u[c] = 2.0 * x[c];
  return PETSC_SUCCESS;
}

static void f0_quadratic_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  f0[0] = -4.0;
  f0_u(dim, Nf, NfAux, uOff, uOff_x, u, u_t, u_x, aOff, aOff_x, a, a_t, a_x, t, x, numConstants, constants, f0);
}
static void f0_bd_quadratic_q(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], const PetscReal n[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscScalar potential;
  PetscInt    d;

  PetscCallAbort(PETSC_COMM_SELF, quadratic_u(dim, t, x, dim, &potential, NULL));
  for (d = 0; d < dim; ++d) f0[d] = -potential * n[d];
}

/* 2D Dirichlet potential example

  u = x (1-x) y (1-y)
  q = <(1-2x) y (1-y), x (1-x) (1-2y)>
  f = -y (1-y) - x (1-x)

  u|_\Gamma = 0 so that the boundary integral vanishes
*/
static PetscErrorCode quartic_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt d;

  u[0] = 1.0;
  for (d = 0; d < dim; ++d) u[0] *= x[d] * (1.0 - x[d]);
  return PETSC_SUCCESS;
}
static PetscErrorCode quartic_q(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt c, d;

  for (c = 0; c < Nc; ++c) {
    u[c] = 1.0;
    for (d = 0; d < dim; ++d) {
      if (c == d) u[c] *= 1 - 2.0 * x[d];
      else u[c] *= x[d] * (1.0 - x[d]);
    }
  }
  return PETSC_SUCCESS;
}

static void f0_quartic_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscInt d;
  f0[0] = 0.0;
  for (d = 0; d < dim; ++d) f0[0] += 2.0 * x[d] * (1.0 - x[d]);
  f0_u(dim, Nf, NfAux, uOff, uOff_x, u, u_t, u_x, aOff, aOff_x, a, a_t, a_x, t, x, numConstants, constants, f0);
}

/* 2D Dirichlet potential example

  u = x (1-x) y (1-y)
  q = <(1-2x) y (1-y), x (1-x) (1-2y)>
  f = -y (1-y) - x (1-x)

  du/dn_\Gamma = {(1-2x) y (1-y), x (1-x) (1-2y)} . n produces an essential condition on q
*/

static void f0_q(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscInt c;
  for (c = 0; c < dim; ++c) f0[c] = u[uOff[0] + c];
}

/* <\nabla\cdot w, u> = <\nabla w, Iu> */
static void f1_q(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f1[])
{
  PetscInt c;
  for (c = 0; c < dim; ++c) f1[c * dim + c] = u[uOff[1]];
}

/* <t, q> */
static void g0_qq(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g0[])
{
  PetscInt c;
  for (c = 0; c < dim; ++c) g0[c * dim + c] = 1.0;
}

/* <\nabla\cdot t, u> = <\nabla t, Iu> */
static void g2_qu(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g2[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) g2[d * dim + d] = 1.0;
}

/* <v, \nabla\cdot q> */
static void g1_uq(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g1[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) g1[d * dim + d] = 1.0;
}

static PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscFunctionBeginUser;
  options->solType = SOL_LINEAR;
  PetscOptionsBegin(comm, "", "Poisson Problem Options", "DMPLEX");
  PetscCall(PetscOptionsEnum("-sol_type", "Type of exact solution", "ex24.c", SolTypeNames, (PetscEnum)options->solType, (PetscEnum *)&options->solType, NULL));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CreateMesh(MPI_Comm comm, AppCtx *user, DM *dm)
{
  PetscFunctionBeginUser;
  PetscCall(DMCreate(comm, dm));
  PetscCall(DMSetType(*dm, DMPLEX));
  PetscCall(PetscObjectSetName((PetscObject)*dm, "Example Mesh"));
  PetscCall(DMSetApplicationContext(*dm, user));
  PetscCall(DMSetFromOptions(*dm));
  PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetupPrimalProblem(DM dm, AppCtx *user)
{
  PetscDS        ds;
  DMLabel        label;
  PetscWeakForm  wf;
  const PetscInt id = 1;
  PetscInt       bd;

  PetscFunctionBeginUser;
  PetscCall(DMGetLabel(dm, "marker", &label));
  PetscCall(DMGetDS(dm, &ds));
  PetscCall(PetscDSSetResidual(ds, 0, f0_q, f1_q));
  PetscCall(PetscDSSetJacobian(ds, 0, 0, g0_qq, NULL, NULL, NULL));
  PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_qu, NULL));
  PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_uq, NULL, NULL));
  switch (user->solType) {
  case SOL_LINEAR:
    PetscCall(PetscDSSetResidual(ds, 1, f0_linear_u, NULL));
    PetscCall(DMAddBoundary(dm, DM_BC_NATURAL, "Dirichlet Bd Integral", label, 1, &id, 0, 0, NULL, NULL, NULL, user, &bd));
    PetscCall(PetscDSGetBoundary(ds, bd, &wf, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    PetscCall(PetscWeakFormSetIndexBdResidual(wf, label, 1, 0, 0, 0, f0_bd_linear_q, 0, NULL));
    PetscCall(PetscDSSetExactSolution(ds, 0, linear_q, user));
    PetscCall(PetscDSSetExactSolution(ds, 1, linear_u, user));
    break;
  case SOL_QUADRATIC:
    PetscCall(PetscDSSetResidual(ds, 1, f0_quadratic_u, NULL));
    PetscCall(DMAddBoundary(dm, DM_BC_NATURAL, "Dirichlet Bd Integral", label, 1, &id, 0, 0, NULL, NULL, NULL, user, &bd));
    PetscCall(PetscDSGetBoundary(ds, bd, &wf, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    PetscCall(PetscWeakFormSetIndexBdResidual(wf, label, 1, 0, 0, 0, f0_bd_quadratic_q, 0, NULL));
    PetscCall(PetscDSSetExactSolution(ds, 0, quadratic_q, user));
    PetscCall(PetscDSSetExactSolution(ds, 1, quadratic_u, user));
    break;
  case SOL_QUARTIC:
    PetscCall(PetscDSSetResidual(ds, 1, f0_quartic_u, NULL));
    PetscCall(PetscDSSetExactSolution(ds, 0, quartic_q, user));
    PetscCall(PetscDSSetExactSolution(ds, 1, quartic_u, user));
    break;
  case SOL_QUARTIC_NEUMANN:
    PetscCall(PetscDSSetResidual(ds, 1, f0_quartic_u, NULL));
    PetscCall(PetscDSSetExactSolution(ds, 0, quartic_q, user));
    PetscCall(PetscDSSetExactSolution(ds, 1, quartic_u, user));
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "Flux condition", label, 1, &id, 0, 0, NULL, (void (*)(void))quartic_q, NULL, user, NULL));
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONG, "Invalid exact solution type %s", SolTypeNames[PetscMin(user->solType, SOL_UNKNOWN)]);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetupDiscretization(DM dm, PetscErrorCode (*setup)(DM, AppCtx *), AppCtx *user)
{
  DM             cdm = dm;
  PetscFE        feq, feu;
  DMPolytopeType ct;
  PetscBool      simplex;
  PetscInt       dim, cStart;

  PetscFunctionBeginUser;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMPlexGetHeightStratum(dm, 0, &cStart, NULL));
  PetscCall(DMPlexGetCellType(dm, cStart, &ct));
  simplex = DMPolytopeTypeGetNumVertices(ct) == DMPolytopeTypeGetDim(ct) + 1 ? PETSC_TRUE : PETSC_FALSE;
  /* Create finite element */
  PetscCall(PetscFECreateDefault(PETSC_COMM_SELF, dim, dim, simplex, "field_", -1, &feq));
  PetscCall(PetscObjectSetName((PetscObject)feq, "field"));
  PetscCall(PetscFECreateDefault(PETSC_COMM_SELF, dim, 1, simplex, "potential_", -1, &feu));
  PetscCall(PetscObjectSetName((PetscObject)feu, "potential"));
  PetscCall(PetscFECopyQuadrature(feq, feu));
  /* Set discretization and boundary conditions for each mesh */
  PetscCall(DMSetField(dm, 0, NULL, (PetscObject)feq));
  PetscCall(DMSetField(dm, 1, NULL, (PetscObject)feu));
  PetscCall(DMCreateDS(dm));
  PetscCall((*setup)(dm, user));
  while (cdm) {
    PetscCall(DMCopyDisc(dm, cdm));
    PetscCall(DMGetCoarseDM(cdm, &cdm));
  }
  PetscCall(PetscFEDestroy(&feq));
  PetscCall(PetscFEDestroy(&feu));
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  DM     dm;   /* Problem specification */
  SNES   snes; /* Nonlinear solver */
  Vec    u;    /* Solutions */
  AppCtx user; /* User-defined work context */

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  PetscCall(ProcessOptions(PETSC_COMM_WORLD, &user));
  /* Primal system */
  PetscCall(SNESCreate(PETSC_COMM_WORLD, &snes));
  PetscCall(CreateMesh(PETSC_COMM_WORLD, &user, &dm));
  PetscCall(SNESSetDM(snes, dm));
  PetscCall(SetupDiscretization(dm, SetupPrimalProblem, &user));
  PetscCall(DMCreateGlobalVector(dm, &u));
  PetscCall(VecSet(u, 0.0));
  PetscCall(PetscObjectSetName((PetscObject)u, "potential"));
  PetscCall(DMPlexSetSNESLocalFEM(dm, &user, &user, &user));
  PetscCall(SNESSetFromOptions(snes));
  PetscCall(DMSNESCheckFromOptions(snes, u));
  PetscCall(SNESSolve(snes, NULL, u));
  PetscCall(SNESGetSolution(snes, &u));
  PetscCall(VecViewFromOptions(u, NULL, "-potential_view"));
  /* Cleanup */
  PetscCall(VecDestroy(&u));
  PetscCall(SNESDestroy(&snes));
  PetscCall(DMDestroy(&dm));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

  test:
    suffix: 2d_bdm1_p0
    requires: triangle
    args: -sol_type linear \
          -field_petscspace_degree 1 -field_petscdualspace_type bdm -dm_refine 1 \
          -dmsnes_check .001 -snes_error_if_not_converged \
          -ksp_rtol 1e-10 -ksp_error_if_not_converged \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_factorization_type full -pc_fieldsplit_schur_precondition full \
            -fieldsplit_field_pc_type lu \
            -fieldsplit_potential_ksp_rtol 1e-10 -fieldsplit_potential_pc_type lu
  test:
    # Using -dm_refine 2 -convest_num_refine 3 we get L_2 convergence rate: [2.0, 1.0]
    # Using -sol_type quadratic -dm_refine 2 -convest_num_refine 3 we get L_2 convergence rate: [2.9, 1.0]
    suffix: 2d_bdm1_p0_conv
    requires: triangle
    args: -sol_type quartic \
          -field_petscspace_degree 1 -field_petscdualspace_type bdm -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate \
          -snes_error_if_not_converged \
          -ksp_rtol 1e-10 -ksp_error_if_not_converged \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_factorization_type full -pc_fieldsplit_schur_precondition full \
            -fieldsplit_field_pc_type lu \
            -fieldsplit_potential_ksp_rtol 1e-10 -fieldsplit_potential_pc_type lu

  test:
    # HDF5 output: -dm_view hdf5:${PETSC_DIR}/sol.h5 -potential_view hdf5:${PETSC_DIR}/sol.h5::append -exact_vec_view hdf5:${PETSC_DIR}/sol.h5::append
    # VTK output: -potential_view vtk: -exact_vec_view vtk:
    # VTU output: -potential_view vtk:multifield.vtu -exact_vec_view vtk:exact.vtu
    suffix: 2d_q2_p0
    requires: triangle
    args: -sol_type linear -dm_plex_simplex 0 \
          -field_petscspace_degree 2 -dm_refine 0 \
          -dmsnes_check .001 -snes_error_if_not_converged \
          -ksp_rtol 1e-10 -ksp_error_if_not_converged \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_factorization_type full -pc_fieldsplit_schur_precondition full \
            -fieldsplit_field_pc_type lu \
            -fieldsplit_potential_ksp_rtol 1e-10 -fieldsplit_potential_pc_type lu
  test:
    # Using -dm_refine 1 -convest_num_refine 3 we get L_2 convergence rate: [0.0057, 1.0]
    suffix: 2d_q2_p0_conv
    requires: triangle
    args: -sol_type linear -dm_plex_simplex 0 \
          -field_petscspace_degree 2 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate \
          -snes_error_if_not_converged \
          -ksp_rtol 1e-10 -ksp_error_if_not_converged \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_factorization_type full -pc_fieldsplit_schur_precondition full \
            -fieldsplit_field_pc_type lu \
            -fieldsplit_potential_ksp_rtol 1e-10 -fieldsplit_potential_pc_type lu
  test:
    # Using -dm_refine 1 -convest_num_refine 3 we get L_2 convergence rate: [-0.014, 0.0066]
    suffix: 2d_q2_p0_neumann_conv
    requires: triangle
    args: -sol_type quartic_neumann -dm_plex_simplex 0 \
          -field_petscspace_degree 2 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate \
          -snes_error_if_not_converged \
          -ksp_rtol 1e-10 -ksp_error_if_not_converged \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_factorization_type full -pc_fieldsplit_schur_precondition full \
            -fieldsplit_field_pc_type lu \
            -fieldsplit_potential_ksp_rtol 1e-10 -fieldsplit_potential_pc_type svd

TEST*/

/* These tests will be active once tensor P^- is working

  test:
    suffix: 2d_bdmq1_p0_0
    requires: triangle
    args: -dm_plex_simplex 0 -sol_type linear \
          -field_petscspace_poly_type pminus_hdiv -field_petscspace_degree 1 -field_petscdualspace_type bdm -dm_refine 0 -convest_num_refine 3 -snes_convergence_estimate \
          -dmsnes_check .001 -snes_error_if_not_converged \
          -ksp_rtol 1e-10 -ksp_error_if_not_converged \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_factorization_type full -pc_fieldsplit_schur_precondition full \
            -fieldsplit_field_pc_type lu \
            -fieldsplit_potential_ksp_rtol 1e-10 -fieldsplit_potential_pc_type lu
  test:
    suffix: 2d_bdmq1_p0_2
    requires: triangle
    args: -dm_plex_simplex 0 -sol_type quartic \
          -field_petscspace_poly_type_no pminus_hdiv -field_petscspace_degree 1 -field_petscdualspace_type bdm -dm_refine 0 -convest_num_refine 3 -snes_convergence_estimate \
          -dmsnes_check .001 -snes_error_if_not_converged \
          -ksp_rtol 1e-10 -ksp_error_if_not_converged \
          -pc_type fieldsplit -pc_fieldsplit_type schur -pc_fieldsplit_schur_factorization_type full -pc_fieldsplit_schur_precondition full \
            -fieldsplit_field_pc_type lu \
            -fieldsplit_potential_ksp_rtol 1e-10 -fieldsplit_potential_pc_type lu

*/

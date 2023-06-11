static char help[] = "Time dependent Biot Poroelasticity problem with finite elements.\n\
We solve three field, quasi-static poroelasticity in a rectangular\n\
domain, using a parallel unstructured mesh (DMPLEX) to discretize it.\n\
Contributed by: Robert Walker <rwalker6@buffalo.edu>\n\n\n";

#include <petscdmplex.h>
#include <petscsnes.h>
#include <petscts.h>
#include <petscds.h>
#include <petscbag.h>

#include <petsc/private/tsimpl.h>

/* This presentation of poroelasticity is taken from

@book{Cheng2016,
  title={Poroelasticity},
  author={Cheng, Alexander H-D},
  volume={27},
  year={2016},
  publisher={Springer}
}

For visualization, use

  -dm_view hdf5:${PETSC_DIR}/sol.h5 -monitor_solution hdf5:${PETSC_DIR}/sol.h5::append

The weak form would then be, using test function $(v, q, \tau)$,

            (q, \frac{1}{M} \frac{dp}{dt}) + (q, \alpha \frac{d\varepsilon}{dt}) + (\nabla q, \kappa \nabla p) = (q, g)
 -(\nabla v, 2 G \epsilon) - (\nabla\cdot v, \frac{2 G \nu}{1 - 2\nu} \varepsilon) + \alpha (\nabla\cdot v, p) = (v, f)
                                                                          (\tau, \nabla \cdot u - \varepsilon) = 0
*/

typedef enum {
  SOL_QUADRATIC_LINEAR,
  SOL_QUADRATIC_TRIG,
  SOL_TRIG_LINEAR,
  SOL_TERZAGHI,
  SOL_MANDEL,
  SOL_CRYER,
  NUM_SOLUTION_TYPES
} SolutionType;
const char *solutionTypes[NUM_SOLUTION_TYPES + 1] = {"quadratic_linear", "quadratic_trig", "trig_linear", "terzaghi", "mandel", "cryer", "unknown"};

typedef struct {
  PetscScalar mu;    /* shear modulus */
  PetscScalar K_u;   /* undrained bulk modulus */
  PetscScalar alpha; /* Biot effective stress coefficient */
  PetscScalar M;     /* Biot modulus */
  PetscScalar k;     /* (isotropic) permeability */
  PetscScalar mu_f;  /* fluid dynamic viscosity */
  PetscScalar P_0;   /* magnitude of vertical stress */
} Parameter;

typedef struct {
  /* Domain and mesh definition */
  PetscReal xmin[3]; /* Lower left bottom corner of bounding box */
  PetscReal xmax[3]; /* Upper right top corner of bounding box */
  /* Problem definition */
  SolutionType solType;   /* Type of exact solution */
  PetscBag     bag;       /* Problem parameters */
  PetscReal    t_r;       /* Relaxation time: 4 L^2 / c */
  PetscReal    dtInitial; /* Override the choice for first timestep */
  /* Exact solution terms */
  PetscInt   niter;     /* Number of series term iterations in exact solutions */
  PetscReal  eps;       /* Precision value for root finding */
  PetscReal *zeroArray; /* Array of root locations */
} AppCtx;

static PetscErrorCode zero(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt c;
  for (c = 0; c < Nc; ++c) u[c] = 0.0;
  return PETSC_SUCCESS;
}

/* Quadratic space and linear time solution

  2D:
  u = x^2
  v = y^2 - 2xy
  p = (x + y) t
  e = 2y
  f = <2 G, 4 G + 2 \lambda > - <alpha t, alpha t>
  g = 0
  \epsilon = / 2x     -y    \
             \ -y   2y - 2x /
  Tr(\epsilon) = e = div u = 2y
  div \sigma = \partial_i 2 G \epsilon_{ij} + \partial_i \lambda \varepsilon \delta_{ij} - \partial_i \alpha p \delta_{ij}
    = 2 G < 2-1, 2 > + \lambda <0, 2> - alpha <t, t>
    = <2 G, 4 G + 2 \lambda> - <alpha t, alpha t>
  \frac{1}{M} \frac{dp}{dt} + \alpha \frac{d\varepsilon}{dt} - \nabla \cdot \kappa \nabla p
    = \frac{1}{M} \frac{dp}{dt} + \kappa \Delta p
    = (x + y)/M

  3D:
  u = x^2
  v = y^2 - 2xy
  w = z^2 - 2yz
  p = (x + y + z) t
  e = 2z
  f = <2 G, 4 G + 2 \lambda > - <alpha t, alpha t, alpha t>
  g = 0
  \varepsilon = / 2x     -y       0   \
                | -y   2y - 2x   -z   |
                \  0     -z    2z - 2y/
  Tr(\varepsilon) = div u = 2z
  div \sigma = \partial_i 2 G \epsilon_{ij} + \partial_i \lambda \varepsilon \delta_{ij} - \partial_i \alpha p \delta_{ij}
    = 2 G < 2-1, 2-1, 2 > + \lambda <0, 0, 2> - alpha <t, t, t>
    = <2 G, 2G, 4 G + 2 \lambda> - <alpha t, alpha t, alpha t>
*/
static PetscErrorCode quadratic_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt d;

  for (d = 0; d < dim; ++d) u[d] = PetscSqr(x[d]) - (d > 0 ? 2.0 * x[d - 1] * x[d] : 0.0);
  return PETSC_SUCCESS;
}

static PetscErrorCode linear_eps(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  u[0] = 2.0 * x[dim - 1];
  return PETSC_SUCCESS;
}

static PetscErrorCode linear_linear_p(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscReal sum = 0.0;
  PetscInt  d;

  for (d = 0; d < dim; ++d) sum += x[d];
  u[0] = sum * time;
  return PETSC_SUCCESS;
}

static PetscErrorCode linear_linear_p_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscReal sum = 0.0;
  PetscInt  d;

  for (d = 0; d < dim; ++d) sum += x[d];
  u[0] = sum;
  return PETSC_SUCCESS;
}

static void f0_quadratic_linear_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal G      = PetscRealPart(constants[0]);
  const PetscReal K_u    = PetscRealPart(constants[1]);
  const PetscReal alpha  = PetscRealPart(constants[2]);
  const PetscReal M      = PetscRealPart(constants[3]);
  const PetscReal K_d    = K_u - alpha * alpha * M;
  const PetscReal lambda = K_d - (2.0 * G) / 3.0;
  PetscInt        d;

  for (d = 0; d < dim - 1; ++d) f0[d] -= 2.0 * G - alpha * t;
  f0[dim - 1] -= 2.0 * lambda + 4.0 * G - alpha * t;
}

static void f0_quadratic_linear_p(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal alpha = PetscRealPart(constants[2]);
  const PetscReal M     = PetscRealPart(constants[3]);
  PetscReal       sum   = 0.0;
  PetscInt        d;

  for (d = 0; d < dim; ++d) sum += x[d];
  f0[0] += u_t ? alpha * u_t[uOff[1]] : 0.0;
  f0[0] += u_t ? u_t[uOff[2]] / M : 0.0;
  f0[0] -= sum / M;
}

/* Quadratic space and trigonometric time solution

  2D:
  u = x^2
  v = y^2 - 2xy
  p = (x + y) cos(t)
  e = 2y
  f = <2 G, 4 G + 2 \lambda > - <alpha cos(t), alpha cos(t)>
  g = 0
  \epsilon = / 2x     -y    \
             \ -y   2y - 2x /
  Tr(\epsilon) = e = div u = 2y
  div \sigma = \partial_i 2 G \epsilon_{ij} + \partial_i \lambda \varepsilon \delta_{ij} - \partial_i \alpha p \delta_{ij}
    = 2 G < 2-1, 2 > + \lambda <0, 2> - alpha <cos(t), cos(t)>
    = <2 G, 4 G + 2 \lambda> - <alpha cos(t), alpha cos(t)>
  \frac{1}{M} \frac{dp}{dt} + \alpha \frac{d\varepsilon}{dt} - \nabla \cdot \kappa \nabla p
    = \frac{1}{M} \frac{dp}{dt} + \kappa \Delta p
    = -(x + y)/M sin(t)

  3D:
  u = x^2
  v = y^2 - 2xy
  w = z^2 - 2yz
  p = (x + y + z) cos(t)
  e = 2z
  f = <2 G, 4 G + 2 \lambda > - <alpha cos(t), alpha cos(t), alpha cos(t)>
  g = 0
  \varepsilon = / 2x     -y       0   \
                | -y   2y - 2x   -z   |
                \  0     -z    2z - 2y/
  Tr(\varepsilon) = div u = 2z
  div \sigma = \partial_i 2 G \epsilon_{ij} + \partial_i \lambda \varepsilon \delta_{ij} - \partial_i \alpha p \delta_{ij}
    = 2 G < 2-1, 2-1, 2 > + \lambda <0, 0, 2> - alpha <cos(t), cos(t), cos(t)>
    = <2 G, 2G, 4 G + 2 \lambda> - <alpha cos(t), alpha cos(t), alpha cos(t)>
*/
static PetscErrorCode linear_trig_p(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscReal sum = 0.0;
  PetscInt  d;

  for (d = 0; d < dim; ++d) sum += x[d];
  u[0] = sum * PetscCosReal(time);
  return PETSC_SUCCESS;
}

static PetscErrorCode linear_trig_p_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscReal sum = 0.0;
  PetscInt  d;

  for (d = 0; d < dim; ++d) sum += x[d];
  u[0] = -sum * PetscSinReal(time);
  return PETSC_SUCCESS;
}

static void f0_quadratic_trig_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal G      = PetscRealPart(constants[0]);
  const PetscReal K_u    = PetscRealPart(constants[1]);
  const PetscReal alpha  = PetscRealPart(constants[2]);
  const PetscReal M      = PetscRealPart(constants[3]);
  const PetscReal K_d    = K_u - alpha * alpha * M;
  const PetscReal lambda = K_d - (2.0 * G) / 3.0;
  PetscInt        d;

  for (d = 0; d < dim - 1; ++d) f0[d] -= 2.0 * G - alpha * PetscCosReal(t);
  f0[dim - 1] -= 2.0 * lambda + 4.0 * G - alpha * PetscCosReal(t);
}

static void f0_quadratic_trig_p(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal alpha = PetscRealPart(constants[2]);
  const PetscReal M     = PetscRealPart(constants[3]);
  PetscReal       sum   = 0.0;
  PetscInt        d;

  for (d = 0; d < dim; ++d) sum += x[d];

  f0[0] += u_t ? alpha * u_t[uOff[1]] : 0.0;
  f0[0] += u_t ? u_t[uOff[2]] / M : 0.0;
  f0[0] += PetscSinReal(t) * sum / M;
}

/* Trigonometric space and linear time solution

u = sin(2 pi x)
v = sin(2 pi y) - 2xy
\varepsilon = / 2 pi cos(2 pi x)             -y        \
              \      -y          2 pi cos(2 pi y) - 2x /
Tr(\varepsilon) = div u = 2 pi (cos(2 pi x) + cos(2 pi y)) - 2 x
div \sigma = \partial_i \lambda \delta_{ij} \varepsilon_{kk} + \partial_i 2\mu\varepsilon_{ij}
  = \lambda \partial_j 2 pi (cos(2 pi x) + cos(2 pi y)) + 2\mu < -4 pi^2 sin(2 pi x) - 1, -4 pi^2 sin(2 pi y) >
  = \lambda < -4 pi^2 sin(2 pi x) - 2, -4 pi^2 sin(2 pi y) > + \mu < -8 pi^2 sin(2 pi x) - 2, -8 pi^2 sin(2 pi y) >

  2D:
  u = sin(2 pi x)
  v = sin(2 pi y) - 2xy
  p = (cos(2 pi x) + cos(2 pi y)) t
  e = 2 pi (cos(2 pi x) + cos(2 pi y)) - 2 x
  f = < -4 pi^2 sin(2 pi x) (2 G + lambda) - (2 G - 2 lambda), -4 pi^2 sin(2 pi y) (2G + lambda) > + 2 pi alpha t <sin(2 pi x), sin(2 pi y)>
  g = 0
  \varepsilon = / 2 pi cos(2 pi x)             -y        \
                \      -y          2 pi cos(2 pi y) - 2x /
  Tr(\varepsilon) = div u = 2 pi (cos(2 pi x) + cos(2 pi y)) - 2 x
  div \sigma = \partial_i 2 G \epsilon_{ij} + \partial_i \lambda \varepsilon \delta_{ij} - \partial_i \alpha p \delta_{ij}
    = 2 G < -4 pi^2 sin(2 pi x) - 1, -4 pi^2 sin(2 pi y) > + \lambda <-4 pi^2 sin(2 pi x) - 2, -4 pi^2 sin(2 pi y)> - alpha <-2 pi sin(2 pi x) t, -2 pi sin(2 pi y) t>
    = < -4 pi^2 sin(2 pi x) (2 G + lambda) - (2 G + 2 lambda), -4 pi^2 sin(2 pi y) (2G + lambda) > + 2 pi alpha t <sin(2 pi x), sin(2 pi y)>
  \frac{1}{M} \frac{dp}{dt} + \alpha \frac{d\varepsilon}{dt} - \nabla \cdot \kappa \nabla p
    = \frac{1}{M} \frac{dp}{dt} + \kappa \Delta p
    = (cos(2 pi x) + cos(2 pi y))/M - 4 pi^2 \kappa (cos(2 pi x) + cos(2 pi y)) t

  3D:
  u = sin(2 pi x)
  v = sin(2 pi y) - 2xy
  v = sin(2 pi y) - 2yz
  p = (cos(2 pi x) + cos(2 pi y) + cos(2 pi z)) t
  e = 2 pi (cos(2 pi x) + cos(2 pi y) + cos(2 pi z)) - 2 x - 2y
  f = < -4 pi^2 sin(2 pi x) (2 G + lambda) - (2 G + 2 lambda),  -4 pi^2 sin(2 pi y) (2 G + lambda) - (2 G + 2 lambda), -4 pi^2 sin(2 pi z) (2G + lambda) > + 2 pi alpha t <sin(2 pi x), sin(2 pi y), , sin(2 pi z)>
  g = 0
  \varepsilon = / 2 pi cos(2 pi x)            -y                     0         \
                |         -y       2 pi cos(2 pi y) - 2x            -z         |
                \          0                  -z         2 pi cos(2 pi z) - 2y /
  Tr(\varepsilon) = div u = 2 pi (cos(2 pi x) + cos(2 pi y) + cos(2 pi z)) - 2 x - 2 y
  div \sigma = \partial_i 2 G \epsilon_{ij} + \partial_i \lambda \varepsilon \delta_{ij} - \partial_i \alpha p \delta_{ij}
    = 2 G < -4 pi^2 sin(2 pi x) - 1, -4 pi^2 sin(2 pi y) - 1, -4 pi^2 sin(2 pi z) > + \lambda <-4 pi^2 sin(2 pi x) - 2, 4 pi^2 sin(2 pi y) - 2, -4 pi^2 sin(2 pi y)> - alpha <-2 pi sin(2 pi x) t, -2 pi sin(2 pi y) t, -2 pi sin(2 pi z) t>
    = < -4 pi^2 sin(2 pi x) (2 G + lambda) - (2 G + 2 lambda),  -4 pi^2 sin(2 pi y) (2 G + lambda) - (2 G + 2 lambda), -4 pi^2 sin(2 pi z) (2G + lambda) > + 2 pi alpha t <sin(2 pi x), sin(2 pi y), , sin(2 pi z)>
  \frac{1}{M} \frac{dp}{dt} + \alpha \frac{d\varepsilon}{dt} - \nabla \cdot \kappa \nabla p
    = \frac{1}{M} \frac{dp}{dt} + \kappa \Delta p
    = (cos(2 pi x) + cos(2 pi y) + cos(2 pi z))/M - 4 pi^2 \kappa (cos(2 pi x) + cos(2 pi y) + cos(2 pi z)) t
*/
static PetscErrorCode trig_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt d;

  for (d = 0; d < dim; ++d) u[d] = PetscSinReal(2. * PETSC_PI * x[d]) - (d > 0 ? 2.0 * x[d - 1] * x[d] : 0.0);
  return PETSC_SUCCESS;
}

static PetscErrorCode trig_eps(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscReal sum = 0.0;
  PetscInt  d;

  for (d = 0; d < dim; ++d) sum += 2. * PETSC_PI * PetscCosReal(2. * PETSC_PI * x[d]) - (d < dim - 1 ? 2. * x[d] : 0.0);
  u[0] = sum;
  return PETSC_SUCCESS;
}

static PetscErrorCode trig_linear_p(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscReal sum = 0.0;
  PetscInt  d;

  for (d = 0; d < dim; ++d) sum += PetscCosReal(2. * PETSC_PI * x[d]);
  u[0] = sum * time;
  return PETSC_SUCCESS;
}

static PetscErrorCode trig_linear_p_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscReal sum = 0.0;
  PetscInt  d;

  for (d = 0; d < dim; ++d) sum += PetscCosReal(2. * PETSC_PI * x[d]);
  u[0] = sum;
  return PETSC_SUCCESS;
}

static void f0_trig_linear_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal G      = PetscRealPart(constants[0]);
  const PetscReal K_u    = PetscRealPart(constants[1]);
  const PetscReal alpha  = PetscRealPart(constants[2]);
  const PetscReal M      = PetscRealPart(constants[3]);
  const PetscReal K_d    = K_u - alpha * alpha * M;
  const PetscReal lambda = K_d - (2.0 * G) / 3.0;
  PetscInt        d;

  for (d = 0; d < dim - 1; ++d) f0[d] += PetscSqr(2. * PETSC_PI) * PetscSinReal(2. * PETSC_PI * x[d]) * (2. * G + lambda) + 2.0 * (G + lambda) - 2. * PETSC_PI * alpha * PetscSinReal(2. * PETSC_PI * x[d]) * t;
  f0[dim - 1] += PetscSqr(2. * PETSC_PI) * PetscSinReal(2. * PETSC_PI * x[dim - 1]) * (2. * G + lambda) - 2. * PETSC_PI * alpha * PetscSinReal(2. * PETSC_PI * x[dim - 1]) * t;
}

static void f0_trig_linear_p(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal alpha = PetscRealPart(constants[2]);
  const PetscReal M     = PetscRealPart(constants[3]);
  const PetscReal kappa = PetscRealPart(constants[4]);
  PetscReal       sum   = 0.0;
  PetscInt        d;

  for (d = 0; d < dim; ++d) sum += PetscCosReal(2. * PETSC_PI * x[d]);
  f0[0] += u_t ? alpha * u_t[uOff[1]] : 0.0;
  f0[0] += u_t ? u_t[uOff[2]] / M : 0.0;
  f0[0] -= sum / M - 4 * PetscSqr(PETSC_PI) * kappa * sum * t;
}

/* Terzaghi Solutions */
/* The analytical solutions given here are drawn from chapter 7, section 3, */
/* "One-Dimensional Consolidation Problem," from Poroelasticity, by Cheng.  */
static PetscErrorCode terzaghi_drainage_pressure(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscScalar alpha = param->alpha;                                        /* -  */
    PetscScalar K_u   = param->K_u;                                          /* Pa */
    PetscScalar M     = param->M;                                            /* Pa */
    PetscScalar G     = param->mu;                                           /* Pa */
    PetscScalar P_0   = param->P_0;                                          /* Pa */
    PetscScalar K_d   = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar eta   = (3.0 * alpha * G) / (3.0 * K_d + 4.0 * G);           /* -,       Cheng (B.11) */
    PetscScalar S     = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */

    u[0] = ((P_0 * eta) / (G * S));
  } else {
    u[0] = 0.0;
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode terzaghi_initial_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  {
    PetscScalar K_u   = param->K_u;                                      /* Pa */
    PetscScalar G     = param->mu;                                       /* Pa */
    PetscScalar P_0   = param->P_0;                                      /* Pa */
    PetscReal   L     = user->xmax[1] - user->xmin[1];                   /* m */
    PetscScalar nu_u  = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G)); /* -,       Cheng (B.9)  */
    PetscReal   zstar = x[1] / L;                                        /* - */

    u[0] = 0.0;
    u[1] = ((P_0 * L * (1.0 - 2.0 * nu_u)) / (2.0 * G * (1.0 - nu_u))) * (1.0 - zstar);
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode terzaghi_initial_eps(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  {
    PetscScalar K_u  = param->K_u;                                      /* Pa */
    PetscScalar G    = param->mu;                                       /* Pa */
    PetscScalar P_0  = param->P_0;                                      /* Pa */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G)); /* -,       Cheng (B.9)  */

    u[0] = -(P_0 * (1.0 - 2.0 * nu_u)) / (2.0 * G * (1.0 - nu_u));
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode terzaghi_2d_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time < 0.0) {
    PetscCall(terzaghi_initial_u(dim, time, x, Nc, u, ctx));
  } else {
    PetscScalar alpha = param->alpha;                  /* -  */
    PetscScalar K_u   = param->K_u;                    /* Pa */
    PetscScalar M     = param->M;                      /* Pa */
    PetscScalar G     = param->mu;                     /* Pa */
    PetscScalar P_0   = param->P_0;                    /* Pa */
    PetscScalar kappa = param->k / param->mu_f;        /* m^2 / (Pa s) */
    PetscReal   L     = user->xmax[1] - user->xmin[1]; /* m */
    PetscInt    N     = user->niter, m;

    PetscScalar K_d  = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar S    = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c    = kappa / S;                                           /* m^2 / s, Cheng (B.16) */

    PetscReal   zstar = x[1] / L;                                    /* - */
    PetscReal   tstar = PetscRealPart(c * time) / PetscSqr(2.0 * L); /* - */
    PetscScalar F2    = 0.0;

    for (m = 1; m < 2 * N + 1; ++m) {
      if (m % 2 == 1) F2 += (8.0 / PetscSqr(m * PETSC_PI)) * PetscCosReal(0.5 * m * PETSC_PI * zstar) * (1.0 - PetscExpReal(-PetscSqr(m * PETSC_PI) * tstar));
    }
    u[0] = 0.0;
    u[1] = ((P_0 * L * (1.0 - 2.0 * nu_u)) / (2.0 * G * (1.0 - nu_u))) * (1.0 - zstar) + ((P_0 * L * (nu_u - nu)) / (2.0 * G * (1.0 - nu_u) * (1.0 - nu))) * F2; /* m */
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode terzaghi_2d_eps(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time < 0.0) {
    PetscCall(terzaghi_initial_eps(dim, time, x, Nc, u, ctx));
  } else {
    PetscScalar alpha = param->alpha;                  /* -  */
    PetscScalar K_u   = param->K_u;                    /* Pa */
    PetscScalar M     = param->M;                      /* Pa */
    PetscScalar G     = param->mu;                     /* Pa */
    PetscScalar P_0   = param->P_0;                    /* Pa */
    PetscScalar kappa = param->k / param->mu_f;        /* m^2 / (Pa s) */
    PetscReal   L     = user->xmax[1] - user->xmin[1]; /* m */
    PetscInt    N     = user->niter, m;

    PetscScalar K_d  = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar S    = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c    = kappa / S;                                           /* m^2 / s, Cheng (B.16) */

    PetscReal   zstar = x[1] / L;                                    /* - */
    PetscReal   tstar = PetscRealPart(c * time) / PetscSqr(2.0 * L); /* - */
    PetscScalar F2_z  = 0.0;

    for (m = 1; m < 2 * N + 1; ++m) {
      if (m % 2 == 1) F2_z += (-4.0 / (m * PETSC_PI * L)) * PetscSinReal(0.5 * m * PETSC_PI * zstar) * (1.0 - PetscExpReal(-PetscSqr(m * PETSC_PI) * tstar));
    }
    u[0] = -((P_0 * L * (1.0 - 2.0 * nu_u)) / (2.0 * G * (1.0 - nu_u) * L)) + ((P_0 * L * (nu_u - nu)) / (2.0 * G * (1.0 - nu_u) * (1.0 - nu))) * F2_z; /* - */
  }
  return PETSC_SUCCESS;
}

// Pressure
static PetscErrorCode terzaghi_2d_p(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscCall(terzaghi_drainage_pressure(dim, time, x, Nc, u, ctx));
  } else {
    PetscScalar alpha = param->alpha;                  /* -  */
    PetscScalar K_u   = param->K_u;                    /* Pa */
    PetscScalar M     = param->M;                      /* Pa */
    PetscScalar G     = param->mu;                     /* Pa */
    PetscScalar P_0   = param->P_0;                    /* Pa */
    PetscScalar kappa = param->k / param->mu_f;        /* m^2 / (Pa s) */
    PetscReal   L     = user->xmax[1] - user->xmin[1]; /* m */
    PetscInt    N     = user->niter, m;

    PetscScalar K_d = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar eta = (3.0 * alpha * G) / (3.0 * K_d + 4.0 * G);           /* -,       Cheng (B.11) */
    PetscScalar S   = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c   = kappa / S;                                           /* m^2 / s, Cheng (B.16) */

    PetscReal   zstar = x[1] / L;                                    /* - */
    PetscReal   tstar = PetscRealPart(c * time) / PetscSqr(2.0 * L); /* - */
    PetscScalar F1    = 0.0;

    PetscCheck(PetscAbsScalar((1 / M + (alpha * eta) / G) - S) <= 1.0e-10, PETSC_COMM_SELF, PETSC_ERR_PLIB, "S %g != check %g", (double)PetscAbsScalar(S), (double)PetscAbsScalar(1 / M + (alpha * eta) / G));

    for (m = 1; m < 2 * N + 1; ++m) {
      if (m % 2 == 1) F1 += (4.0 / (m * PETSC_PI)) * PetscSinReal(0.5 * m * PETSC_PI * zstar) * PetscExpReal(-PetscSqr(m * PETSC_PI) * tstar);
    }
    u[0] = ((P_0 * eta) / (G * S)) * F1; /* Pa */
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode terzaghi_2d_u_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    u[0] = 0.0;
    u[1] = 0.0;
  } else {
    PetscScalar alpha = param->alpha;                  /* -  */
    PetscScalar K_u   = param->K_u;                    /* Pa */
    PetscScalar M     = param->M;                      /* Pa */
    PetscScalar G     = param->mu;                     /* Pa */
    PetscScalar P_0   = param->P_0;                    /* Pa */
    PetscScalar kappa = param->k / param->mu_f;        /* m^2 / (Pa s) */
    PetscReal   L     = user->xmax[1] - user->xmin[1]; /* m */
    PetscInt    N     = user->niter, m;

    PetscScalar K_d  = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar S    = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c    = kappa / S;                                           /* m^2 / s, Cheng (B.16) */

    PetscReal   zstar = x[1] / L;                                    /* - */
    PetscReal   tstar = PetscRealPart(c * time) / PetscSqr(2.0 * L); /* - */
    PetscScalar F2_t  = 0.0;

    for (m = 1; m < 2 * N + 1; ++m) {
      if (m % 2 == 1) F2_t += (2.0 * c / PetscSqr(L)) * PetscCosReal(0.5 * m * PETSC_PI * zstar) * PetscExpReal(-PetscSqr(m * PETSC_PI) * tstar);
    }
    u[0] = 0.0;
    u[1] = ((P_0 * L * (nu_u - nu)) / (2.0 * G * (1.0 - nu_u) * (1.0 - nu))) * F2_t; /* m / s */
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode terzaghi_2d_eps_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    u[0] = 0.0;
  } else {
    PetscScalar alpha = param->alpha;                  /* -  */
    PetscScalar K_u   = param->K_u;                    /* Pa */
    PetscScalar M     = param->M;                      /* Pa */
    PetscScalar G     = param->mu;                     /* Pa */
    PetscScalar P_0   = param->P_0;                    /* Pa */
    PetscScalar kappa = param->k / param->mu_f;        /* m^2 / (Pa s) */
    PetscReal   L     = user->xmax[1] - user->xmin[1]; /* m */
    PetscInt    N     = user->niter, m;

    PetscScalar K_d  = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar S    = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c    = kappa / S;                                           /* m^2 / s, Cheng (B.16) */

    PetscReal   zstar = x[1] / L;                                    /* - */
    PetscReal   tstar = PetscRealPart(c * time) / PetscSqr(2.0 * L); /* - */
    PetscScalar F2_zt = 0.0;

    for (m = 1; m < 2 * N + 1; ++m) {
      if (m % 2 == 1) F2_zt += ((-m * PETSC_PI * c) / (L * L * L)) * PetscSinReal(0.5 * m * PETSC_PI * zstar) * PetscExpReal(-PetscSqr(m * PETSC_PI) * tstar);
    }
    u[0] = ((P_0 * L * (nu_u - nu)) / (2.0 * G * (1.0 - nu_u) * (1.0 - nu))) * F2_zt; /* 1 / s */
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode terzaghi_2d_p_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscScalar alpha = param->alpha;                  /* -  */
    PetscScalar K_u   = param->K_u;                    /* Pa */
    PetscScalar M     = param->M;                      /* Pa */
    PetscScalar G     = param->mu;                     /* Pa */
    PetscScalar P_0   = param->P_0;                    /* Pa */
    PetscScalar kappa = param->k / param->mu_f;        /* m^2 / (Pa s) */
    PetscReal   L     = user->xmax[1] - user->xmin[1]; /* m */

    PetscScalar K_d = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar eta = (3.0 * alpha * G) / (3.0 * K_d + 4.0 * G);           /* -,       Cheng (B.11) */
    PetscScalar S   = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c   = kappa / S;                                           /* m^2 / s, Cheng (B.16) */

    u[0] = -((P_0 * eta) / (G * S)) * PetscSqr(0 * PETSC_PI) * c / PetscSqr(2.0 * L); /* Pa / s */
  } else {
    PetscScalar alpha = param->alpha;                  /* -  */
    PetscScalar K_u   = param->K_u;                    /* Pa */
    PetscScalar M     = param->M;                      /* Pa */
    PetscScalar G     = param->mu;                     /* Pa */
    PetscScalar P_0   = param->P_0;                    /* Pa */
    PetscScalar kappa = param->k / param->mu_f;        /* m^2 / (Pa s) */
    PetscReal   L     = user->xmax[1] - user->xmin[1]; /* m */
    PetscInt    N     = user->niter, m;

    PetscScalar K_d = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar eta = (3.0 * alpha * G) / (3.0 * K_d + 4.0 * G);           /* -,       Cheng (B.11) */
    PetscScalar S   = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c   = kappa / S;                                           /* m^2 / s, Cheng (B.16) */

    PetscReal   zstar = x[1] / L;                                    /* - */
    PetscReal   tstar = PetscRealPart(c * time) / PetscSqr(2.0 * L); /* - */
    PetscScalar F1_t  = 0.0;

    PetscCheck(PetscAbsScalar((1 / M + (alpha * eta) / G) - S) <= 1.0e-10, PETSC_COMM_SELF, PETSC_ERR_PLIB, "S %g != check %g", (double)PetscAbsScalar(S), (double)PetscAbsScalar(1 / M + (alpha * eta) / G));

    for (m = 1; m < 2 * N + 1; ++m) {
      if (m % 2 == 1) F1_t += ((-m * PETSC_PI * c) / PetscSqr(L)) * PetscSinReal(0.5 * m * PETSC_PI * zstar) * PetscExpReal(-PetscSqr(m * PETSC_PI) * tstar);
    }
    u[0] = ((P_0 * eta) / (G * S)) * F1_t; /* Pa / s */
  }
  return PETSC_SUCCESS;
}

/* Mandel Solutions */
static PetscErrorCode mandel_drainage_pressure(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscScalar alpha = param->alpha;                          /* -  */
    PetscScalar K_u   = param->K_u;                            /* Pa */
    PetscScalar M     = param->M;                              /* Pa */
    PetscScalar G     = param->mu;                             /* Pa */
    PetscScalar P_0   = param->P_0;                            /* Pa */
    PetscScalar kappa = param->k / param->mu_f;                /* m^2 / (Pa s) */
    PetscReal   a     = 0.5 * (user->xmax[0] - user->xmin[0]); /* m */
    PetscInt    N     = user->niter, n;

    PetscScalar K_d  = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar B    = alpha * M / K_u;                                     /* -,       Cheng (B.12) */
    PetscScalar S    = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c    = kappa / S;                                           /* m^2 / s, Cheng (B.16) */

    PetscScalar A1   = 3.0 / (B * (1.0 + nu_u));
    PetscReal   aa   = 0.0;
    PetscReal   p    = 0.0;
    PetscReal   time = 0.0;

    for (n = 1; n < N + 1; ++n) {
      aa = user->zeroArray[n - 1];
      p += (PetscSinReal(aa) / (aa - PetscSinReal(aa) * PetscCosReal(aa))) * (PetscCosReal((aa * x[0]) / a) - PetscCosReal(aa)) * PetscExpReal(-1.0 * (aa * aa * PetscRealPart(c) * time) / (a * a));
    }
    u[0] = ((2.0 * P_0) / (a * A1)) * p;
  } else {
    u[0] = 0.0;
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode mandel_initial_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  {
    PetscScalar alpha = param->alpha;                          /* -  */
    PetscScalar K_u   = param->K_u;                            /* Pa */
    PetscScalar M     = param->M;                              /* Pa */
    PetscScalar G     = param->mu;                             /* Pa */
    PetscScalar P_0   = param->P_0;                            /* Pa */
    PetscScalar kappa = param->k / param->mu_f;                /* m^2 / (Pa s) */
    PetscReal   a     = 0.5 * (user->xmax[0] - user->xmin[0]); /* m */
    PetscInt    N     = user->niter, n;

    PetscScalar K_d  = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar S    = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscReal   c    = PetscRealPart(kappa / S);                            /* m^2 / s, Cheng (B.16) */

    PetscReal A_s = 0.0;
    PetscReal B_s = 0.0;
    for (n = 1; n < N + 1; ++n) {
      PetscReal alpha_n = user->zeroArray[n - 1];
      A_s += ((PetscSinReal(alpha_n) * PetscCosReal(alpha_n)) / (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n))) * PetscExpReal(-1 * (alpha_n * alpha_n * c * time) / (a * a));
      B_s += (PetscCosReal(alpha_n) / (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n))) * PetscSinReal((alpha_n * x[0]) / a) * PetscExpReal(-1 * (alpha_n * alpha_n * c * time) / (a * a));
    }
    u[0] = ((P_0 * nu) / (2.0 * G * a) - (P_0 * nu_u) / (G * a) * A_s) * x[0] + P_0 / G * B_s;
    u[1] = (-1 * (P_0 * (1.0 - nu)) / (2 * G * a) + (P_0 * (1 - nu_u)) / (G * a) * A_s) * x[1];
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode mandel_initial_eps(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  {
    PetscScalar alpha = param->alpha;                          /* -  */
    PetscScalar K_u   = param->K_u;                            /* Pa */
    PetscScalar M     = param->M;                              /* Pa */
    PetscScalar G     = param->mu;                             /* Pa */
    PetscScalar P_0   = param->P_0;                            /* Pa */
    PetscScalar kappa = param->k / param->mu_f;                /* m^2 / (Pa s) */
    PetscReal   a     = 0.5 * (user->xmax[0] - user->xmin[0]); /* m */
    PetscInt    N     = user->niter, n;

    PetscScalar K_d = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu  = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar S   = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscReal   c   = PetscRealPart(kappa / S);                            /* m^2 / s, Cheng (B.16) */

    PetscReal aa    = 0.0;
    PetscReal eps_A = 0.0;
    PetscReal eps_B = 0.0;
    PetscReal eps_C = 0.0;
    PetscReal time  = 0.0;

    for (n = 1; n < N + 1; ++n) {
      aa = user->zeroArray[n - 1];
      eps_A += (aa * PetscExpReal((-1.0 * aa * aa * c * time) / (a * a)) * PetscCosReal(aa) * PetscCosReal((aa * x[0]) / a)) / (a * (aa - PetscSinReal(aa) * PetscCosReal(aa)));
      eps_B += (PetscExpReal((-1.0 * aa * aa * c * time) / (a * a)) * PetscSinReal(aa) * PetscCosReal(aa)) / (aa - PetscSinReal(aa) * PetscCosReal(aa));
      eps_C += (PetscExpReal((-1.0 * aa * aa * c * time) / (aa * aa)) * PetscSinReal(aa) * PetscCosReal(aa)) / (aa - PetscSinReal(aa) * PetscCosReal(aa));
    }
    u[0] = (P_0 / G) * eps_A + ((P_0 * nu) / (2.0 * G * a)) - eps_B / (G * a) - (P_0 * (1 - nu)) / (2 * G * a) + eps_C / (G * a);
  }
  return PETSC_SUCCESS;
}

// Displacement
static PetscErrorCode mandel_2d_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  Parameter *param;

  AppCtx *user = (AppCtx *)ctx;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscCall(mandel_initial_u(dim, time, x, Nc, u, ctx));
  } else {
    PetscInt    NITER = user->niter;
    PetscScalar alpha = param->alpha;
    PetscScalar K_u   = param->K_u;
    PetscScalar M     = param->M;
    PetscScalar G     = param->mu;
    PetscScalar k     = param->k;
    PetscScalar mu_f  = param->mu_f;
    PetscScalar F     = param->P_0;

    PetscScalar K_d   = K_u - alpha * alpha * M;
    PetscScalar nu    = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));
    PetscScalar nu_u  = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));
    PetscScalar kappa = k / mu_f;
    PetscReal   a     = (user->xmax[0] - user->xmin[0]) / 2.0;
    PetscReal   c     = PetscRealPart(((2.0 * kappa * G) * (1.0 - nu) * (nu_u - nu)) / (alpha * alpha * (1.0 - 2.0 * nu) * (1.0 - nu_u)));

    // Series term
    PetscScalar A_x = 0.0;
    PetscScalar B_x = 0.0;

    for (PetscInt n = 1; n < NITER + 1; n++) {
      PetscReal alpha_n = user->zeroArray[n - 1];

      A_x += ((PetscSinReal(alpha_n) * PetscCosReal(alpha_n)) / (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n))) * PetscExpReal(-1 * (alpha_n * alpha_n * c * time) / (a * a));
      B_x += (PetscCosReal(alpha_n) / (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n))) * PetscSinReal((alpha_n * x[0]) / a) * PetscExpReal(-1 * (alpha_n * alpha_n * c * time) / (a * a));
    }
    u[0] = ((F * nu) / (2.0 * G * a) - (F * nu_u) / (G * a) * A_x) * x[0] + F / G * B_x;
    u[1] = (-1 * (F * (1.0 - nu)) / (2 * G * a) + (F * (1 - nu_u)) / (G * a) * A_x) * x[1];
  }
  return PETSC_SUCCESS;
}

// Trace strain
static PetscErrorCode mandel_2d_eps(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  Parameter *param;

  AppCtx *user = (AppCtx *)ctx;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscCall(mandel_initial_eps(dim, time, x, Nc, u, ctx));
  } else {
    PetscInt    NITER = user->niter;
    PetscScalar alpha = param->alpha;
    PetscScalar K_u   = param->K_u;
    PetscScalar M     = param->M;
    PetscScalar G     = param->mu;
    PetscScalar k     = param->k;
    PetscScalar mu_f  = param->mu_f;
    PetscScalar F     = param->P_0;

    PetscScalar K_d   = K_u - alpha * alpha * M;
    PetscScalar nu    = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));
    PetscScalar nu_u  = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));
    PetscScalar kappa = k / mu_f;
    //const PetscScalar B = (alpha*M)/(K_d + alpha*alpha * M);

    //const PetscScalar b = (YMAX - YMIN) / 2.0;
    PetscReal a = (user->xmax[0] - user->xmin[0]) / 2.0;
    PetscReal c = PetscRealPart(((2.0 * kappa * G) * (1.0 - nu) * (nu_u - nu)) / (alpha * alpha * (1.0 - 2.0 * nu) * (1.0 - nu_u)));

    // Series term
    PetscReal eps_A = 0.0;
    PetscReal eps_B = 0.0;
    PetscReal eps_C = 0.0;

    for (PetscInt n = 1; n < NITER + 1; n++) {
      PetscReal aa = user->zeroArray[n - 1];

      eps_A += (aa * PetscExpReal((-1.0 * aa * aa * c * time) / (a * a)) * PetscCosReal(aa) * PetscCosReal((aa * x[0]) / a)) / (a * (aa - PetscSinReal(aa) * PetscCosReal(aa)));

      eps_B += (PetscExpReal((-1.0 * aa * aa * c * time) / (a * a)) * PetscSinReal(aa) * PetscCosReal(aa)) / (aa - PetscSinReal(aa) * PetscCosReal(aa));

      eps_C += (PetscExpReal((-1.0 * aa * aa * c * time) / (aa * aa)) * PetscSinReal(aa) * PetscCosReal(aa)) / (aa - PetscSinReal(aa) * PetscCosReal(aa));
    }

    u[0] = (F / G) * eps_A + ((F * nu) / (2.0 * G * a)) - eps_B / (G * a) - (F * (1 - nu)) / (2 * G * a) + eps_C / (G * a);
  }
  return PETSC_SUCCESS;
}

// Pressure
static PetscErrorCode mandel_2d_p(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  Parameter *param;

  AppCtx *user = (AppCtx *)ctx;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscCall(mandel_drainage_pressure(dim, time, x, Nc, u, ctx));
  } else {
    PetscInt NITER = user->niter;

    PetscScalar alpha = param->alpha;
    PetscScalar K_u   = param->K_u;
    PetscScalar M     = param->M;
    PetscScalar G     = param->mu;
    PetscScalar k     = param->k;
    PetscScalar mu_f  = param->mu_f;
    PetscScalar F     = param->P_0;

    PetscScalar K_d   = K_u - alpha * alpha * M;
    PetscScalar nu    = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));
    PetscScalar nu_u  = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));
    PetscScalar kappa = k / mu_f;
    PetscScalar B     = (alpha * M) / (K_d + alpha * alpha * M);

    PetscReal   a  = (user->xmax[0] - user->xmin[0]) / 2.0;
    PetscReal   c  = PetscRealPart(((2.0 * kappa * G) * (1.0 - nu) * (nu_u - nu)) / (alpha * alpha * (1.0 - 2.0 * nu) * (1.0 - nu_u)));
    PetscScalar A1 = 3.0 / (B * (1.0 + nu_u));
    //PetscScalar A2 = (alpha * (1.0 - 2.0*nu)) / (1.0 - nu);

    // Series term
    PetscReal p = 0.0;

    for (PetscInt n = 1; n < NITER + 1; n++) {
      PetscReal aa = user->zeroArray[n - 1];
      p += (PetscSinReal(aa) / (aa - PetscSinReal(aa) * PetscCosReal(aa))) * (PetscCosReal((aa * x[0]) / a) - PetscCosReal(aa)) * PetscExpReal(-1.0 * (aa * aa * c * time) / (a * a));
    }
    u[0] = ((2.0 * F) / (a * A1)) * p;
  }
  return PETSC_SUCCESS;
}

// Time derivative of displacement
static PetscErrorCode mandel_2d_u_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  Parameter *param;

  AppCtx *user = (AppCtx *)ctx;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));

  PetscInt    NITER = user->niter;
  PetscScalar alpha = param->alpha;
  PetscScalar K_u   = param->K_u;
  PetscScalar M     = param->M;
  PetscScalar G     = param->mu;
  PetscScalar F     = param->P_0;

  PetscScalar K_d   = K_u - alpha * alpha * M;
  PetscScalar nu    = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));
  PetscScalar nu_u  = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));
  PetscScalar kappa = param->k / param->mu_f;
  PetscReal   a     = (user->xmax[0] - user->xmin[0]) / 2.0;
  PetscReal   c     = PetscRealPart(((2.0 * kappa * G) * (1.0 - nu) * (nu_u - nu)) / (alpha * alpha * (1.0 - 2.0 * nu) * (1.0 - nu_u)));

  // Series term
  PetscScalar A_s_t = 0.0;
  PetscScalar B_s_t = 0.0;

  for (PetscInt n = 1; n < NITER + 1; n++) {
    PetscReal alpha_n = user->zeroArray[n - 1];

    A_s_t += (-1.0 * alpha_n * alpha_n * c * PetscExpReal((-1.0 * alpha_n * alpha_n * time) / (a * a)) * PetscSinReal((alpha_n * x[0]) / a) * PetscCosReal(alpha_n)) / (a * a * (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n)));
    B_s_t += (-1.0 * alpha_n * alpha_n * c * PetscExpReal((-1.0 * alpha_n * alpha_n * time) / (a * a)) * PetscSinReal(alpha_n) * PetscCosReal(alpha_n)) / (a * a * (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n)));
  }

  u[0] = (F / G) * A_s_t - ((F * nu_u * x[0]) / (G * a)) * B_s_t;
  u[1] = ((F * x[1] * (1 - nu_u)) / (G * a)) * B_s_t;

  return PETSC_SUCCESS;
}

// Time derivative of trace strain
static PetscErrorCode mandel_2d_eps_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  Parameter *param;

  AppCtx *user = (AppCtx *)ctx;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));

  PetscInt    NITER = user->niter;
  PetscScalar alpha = param->alpha;
  PetscScalar K_u   = param->K_u;
  PetscScalar M     = param->M;
  PetscScalar G     = param->mu;
  PetscScalar k     = param->k;
  PetscScalar mu_f  = param->mu_f;
  PetscScalar F     = param->P_0;

  PetscScalar K_d   = K_u - alpha * alpha * M;
  PetscScalar nu    = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));
  PetscScalar nu_u  = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));
  PetscScalar kappa = k / mu_f;
  //const PetscScalar B = (alpha*M)/(K_d + alpha*alpha * M);

  //const PetscScalar b = (YMAX - YMIN) / 2.0;
  PetscReal a = (user->xmax[0] - user->xmin[0]) / 2.0;
  PetscReal c = PetscRealPart(((2.0 * kappa * G) * (1.0 - nu) * (nu_u - nu)) / (alpha * alpha * (1.0 - 2.0 * nu) * (1.0 - nu_u)));

  // Series term
  PetscScalar eps_As = 0.0;
  PetscScalar eps_Bs = 0.0;
  PetscScalar eps_Cs = 0.0;

  for (PetscInt n = 1; n < NITER + 1; n++) {
    PetscReal alpha_n = user->zeroArray[n - 1];

    eps_As += (-1.0 * alpha_n * alpha_n * alpha_n * c * PetscExpReal((-1.0 * alpha_n * alpha_n * c * time) / (a * a)) * PetscCosReal(alpha_n) * PetscCosReal((alpha_n * x[0]) / a)) / (alpha_n * alpha_n * alpha_n * (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n)));
    eps_Bs += (-1.0 * alpha_n * alpha_n * c * PetscExpReal((-1.0 * alpha_n * alpha_n * c * time) / (a * a)) * PetscSinReal(alpha_n) * PetscCosReal(alpha_n)) / (alpha_n * alpha_n * (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n)));
    eps_Cs += (-1.0 * alpha_n * alpha_n * c * PetscExpReal((-1.0 * alpha_n * alpha_n * c * time) / (a * a)) * PetscSinReal(alpha_n) * PetscCosReal(alpha_n)) / (alpha_n * alpha_n * (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n)));
  }

  u[0] = (F / G) * eps_As - ((F * nu_u) / (G * a)) * eps_Bs + ((F * (1 - nu_u)) / (G * a)) * eps_Cs;
  return PETSC_SUCCESS;
}

// Time derivative of pressure
static PetscErrorCode mandel_2d_p_t(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  Parameter *param;

  AppCtx *user = (AppCtx *)ctx;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));

  PetscScalar alpha = param->alpha;
  PetscScalar K_u   = param->K_u;
  PetscScalar M     = param->M;
  PetscScalar G     = param->mu;
  PetscScalar F     = param->P_0;

  PetscScalar K_d  = K_u - alpha * alpha * M;
  PetscScalar nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));
  PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));

  PetscReal a = (user->xmax[0] - user->xmin[0]) / 2.0;
  //PetscScalar A1 = 3.0 / (B * (1.0 + nu_u));
  //PetscScalar A2 = (alpha * (1.0 - 2.0*nu)) / (1.0 - nu);

  u[0] = ((2.0 * F * (-2.0 * nu + 3.0 * nu_u)) / (3.0 * a * alpha * (1.0 - 2.0 * nu)));

  return PETSC_SUCCESS;
}

/* Cryer Solutions */
static PetscErrorCode cryer_drainage_pressure(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscScalar alpha = param->alpha;    /* -  */
    PetscScalar K_u   = param->K_u;      /* Pa */
    PetscScalar M     = param->M;        /* Pa */
    PetscScalar P_0   = param->P_0;      /* Pa */
    PetscScalar B     = alpha * M / K_u; /* -, Cheng (B.12) */

    u[0] = P_0 * B;
  } else {
    u[0] = 0.0;
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode cryer_initial_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  {
    PetscScalar K_u  = param->K_u;                                      /* Pa */
    PetscScalar G    = param->mu;                                       /* Pa */
    PetscScalar P_0  = param->P_0;                                      /* Pa */
    PetscReal   R_0  = user->xmax[1];                                   /* m */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G)); /* -,       Cheng (B.9)  */

    PetscScalar u_0  = -P_0 * R_0 * (1. - 2. * nu_u) / (2. * G * (1. + nu_u)); /* Cheng (7.407) */
    PetscReal   u_sc = PetscRealPart(u_0) / R_0;

    u[0] = u_sc * x[0];
    u[1] = u_sc * x[1];
    u[2] = u_sc * x[2];
  }
  return PETSC_SUCCESS;
}

static PetscErrorCode cryer_initial_eps(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  {
    PetscScalar K_u  = param->K_u;                                      /* Pa */
    PetscScalar G    = param->mu;                                       /* Pa */
    PetscScalar P_0  = param->P_0;                                      /* Pa */
    PetscReal   R_0  = user->xmax[1];                                   /* m */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G)); /* -,       Cheng (B.9)  */

    PetscScalar u_0  = -P_0 * R_0 * (1. - 2. * nu_u) / (2. * G * (1. + nu_u)); /* Cheng (7.407) */
    PetscReal   u_sc = PetscRealPart(u_0) / R_0;

    /* div R = 1/R^2 d/dR R^2 R = 3 */
    u[0] = 3. * u_sc;
    u[1] = 3. * u_sc;
    u[2] = 3. * u_sc;
  }
  return PETSC_SUCCESS;
}

// Displacement
static PetscErrorCode cryer_3d_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscCall(cryer_initial_u(dim, time, x, Nc, u, ctx));
  } else {
    PetscScalar alpha = param->alpha;           /* -  */
    PetscScalar K_u   = param->K_u;             /* Pa */
    PetscScalar M     = param->M;               /* Pa */
    PetscScalar G     = param->mu;              /* Pa */
    PetscScalar P_0   = param->P_0;             /* Pa */
    PetscScalar kappa = param->k / param->mu_f; /* m^2 / (Pa s) */
    PetscReal   R_0   = user->xmax[1];          /* m */
    PetscInt    N     = user->niter, n;

    PetscScalar K_d   = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu    = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar nu_u  = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar S     = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c     = kappa / S;                                           /* m^2 / s, Cheng (B.16) */
    PetscScalar u_inf = -P_0 * R_0 * (1. - 2. * nu) / (2. * G * (1. + nu));  /* m,       Cheng (7.388) */

    PetscReal   R      = PetscSqrtReal(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    PetscReal   R_star = R / R_0;
    PetscReal   tstar  = PetscRealPart(c * time) / PetscSqr(R_0); /* - */
    PetscReal   A_n    = 0.0;
    PetscScalar u_sc;

    for (n = 1; n < N + 1; ++n) {
      const PetscReal x_n = user->zeroArray[n - 1];
      const PetscReal E_n = PetscRealPart(PetscSqr(1 - nu) * PetscSqr(1 + nu_u) * x_n - 18.0 * (1 + nu) * (nu_u - nu) * (1 - nu_u));

      /* m , Cheng (7.404) */
      A_n += PetscRealPart((12.0 * (1.0 + nu) * (nu_u - nu)) / ((1.0 - 2.0 * nu) * E_n * PetscSqr(R_star) * x_n * PetscSinReal(PetscSqrtReal(x_n))) * (3.0 * (nu_u - nu) * (PetscSinReal(R_star * PetscSqrtReal(x_n)) - R_star * PetscSqrtReal(x_n) * PetscCosReal(R_star * PetscSqrtReal(x_n))) + (1.0 - nu) * (1.0 - 2.0 * nu) * PetscPowRealInt(R_star, 3) * x_n * PetscSinReal(PetscSqrtReal(x_n))) * PetscExpReal(-x_n * tstar));
    }
    u_sc = PetscRealPart(u_inf) * (R_star - A_n);
    u[0] = u_sc * x[0] / R;
    u[1] = u_sc * x[1] / R;
    u[2] = u_sc * x[2] / R;
  }
  return PETSC_SUCCESS;
}

// Volumetric Strain
static PetscErrorCode cryer_3d_eps(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscCall(cryer_initial_eps(dim, time, x, Nc, u, ctx));
  } else {
    PetscScalar alpha = param->alpha;           /* -  */
    PetscScalar K_u   = param->K_u;             /* Pa */
    PetscScalar M     = param->M;               /* Pa */
    PetscScalar G     = param->mu;              /* Pa */
    PetscScalar P_0   = param->P_0;             /* Pa */
    PetscScalar kappa = param->k / param->mu_f; /* m^2 / (Pa s) */
    PetscReal   R_0   = user->xmax[1];          /* m */
    PetscInt    N     = user->niter, n;

    PetscScalar K_d   = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar nu    = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar nu_u  = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar S     = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c     = kappa / S;                                           /* m^2 / s, Cheng (B.16) */
    PetscScalar u_inf = -P_0 * R_0 * (1. - 2. * nu) / (2. * G * (1. + nu));  /* m,       Cheng (7.388) */

    PetscReal R      = PetscSqrtReal(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    PetscReal R_star = R / R_0;
    PetscReal tstar  = PetscRealPart(c * time) / PetscSqr(R_0); /* - */
    PetscReal divA_n = 0.0;

    if (R_star < PETSC_SMALL) {
      for (n = 1; n < N + 1; ++n) {
        const PetscReal x_n = user->zeroArray[n - 1];
        const PetscReal E_n = PetscRealPart(PetscSqr(1 - nu) * PetscSqr(1 + nu_u) * x_n - 18.0 * (1 + nu) * (nu_u - nu) * (1 - nu_u));

        divA_n += PetscRealPart((12.0 * (1.0 + nu) * (nu_u - nu)) / ((1.0 - 2.0 * nu) * E_n * PetscSqr(R_star) * x_n * PetscSinReal(PetscSqrtReal(x_n))) * (3.0 * (nu_u - nu) * PetscSqrtReal(x_n) * ((2.0 + PetscSqr(R_star * PetscSqrtReal(x_n))) - 2.0 * PetscCosReal(R_star * PetscSqrtReal(x_n))) + 5.0 * (1.0 - nu) * (1.0 - 2.0 * nu) * PetscPowRealInt(R_star, 2) * x_n * PetscSinReal(PetscSqrtReal(x_n))) * PetscExpReal(-x_n * tstar));
      }
    } else {
      for (n = 1; n < N + 1; ++n) {
        const PetscReal x_n = user->zeroArray[n - 1];
        const PetscReal E_n = PetscRealPart(PetscSqr(1 - nu) * PetscSqr(1 + nu_u) * x_n - 18.0 * (1 + nu) * (nu_u - nu) * (1 - nu_u));

        divA_n += PetscRealPart((12.0 * (1.0 + nu) * (nu_u - nu)) / ((1.0 - 2.0 * nu) * E_n * PetscSqr(R_star) * x_n * PetscSinReal(PetscSqrtReal(x_n))) * (3.0 * (nu_u - nu) * PetscSqrtReal(x_n) * ((2.0 / (R_star * PetscSqrtReal(x_n)) + R_star * PetscSqrtReal(x_n)) * PetscSinReal(R_star * PetscSqrtReal(x_n)) - 2.0 * PetscCosReal(R_star * PetscSqrtReal(x_n))) + 5.0 * (1.0 - nu) * (1.0 - 2.0 * nu) * PetscPowRealInt(R_star, 2) * x_n * PetscSinReal(PetscSqrtReal(x_n))) * PetscExpReal(-x_n * tstar));
      }
    }
    if (PetscAbsReal(divA_n) > 1e3) PetscCall(PetscPrintf(PETSC_COMM_SELF, "(%g, %g, %g) divA_n: %g\n", (double)x[0], (double)x[1], (double)x[2], (double)divA_n));
    u[0] = PetscRealPart(u_inf) / R_0 * (3.0 - divA_n);
  }
  return PETSC_SUCCESS;
}

// Pressure
static PetscErrorCode cryer_3d_p(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  AppCtx    *user = (AppCtx *)ctx;
  Parameter *param;

  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  if (time <= 0.0) {
    PetscCall(cryer_drainage_pressure(dim, time, x, Nc, u, ctx));
  } else {
    PetscScalar alpha = param->alpha;           /* -  */
    PetscScalar K_u   = param->K_u;             /* Pa */
    PetscScalar M     = param->M;               /* Pa */
    PetscScalar G     = param->mu;              /* Pa */
    PetscScalar P_0   = param->P_0;             /* Pa */
    PetscReal   R_0   = user->xmax[1];          /* m */
    PetscScalar kappa = param->k / param->mu_f; /* m^2 / (Pa s) */
    PetscInt    N     = user->niter, n;

    PetscScalar K_d  = K_u - alpha * alpha * M;                             /* Pa,      Cheng (B.5)  */
    PetscScalar eta  = (3.0 * alpha * G) / (3.0 * K_d + 4.0 * G);           /* -,       Cheng (B.11) */
    PetscScalar nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));     /* -,       Cheng (B.8)  */
    PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));     /* -,       Cheng (B.9)  */
    PetscScalar S    = (3.0 * K_u + 4.0 * G) / (M * (3.0 * K_d + 4.0 * G)); /* Pa^{-1}, Cheng (B.14) */
    PetscScalar c    = kappa / S;                                           /* m^2 / s, Cheng (B.16) */
    PetscReal   R    = PetscSqrtReal(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);

    PetscReal R_star = R / R_0;
    PetscReal t_star = PetscRealPart(c * time) / PetscSqr(R_0);
    PetscReal A_x    = 0.0;

    for (n = 1; n < N + 1; ++n) {
      const PetscReal x_n = user->zeroArray[n - 1];
      const PetscReal E_n = PetscRealPart(PetscSqr(1 - nu) * PetscSqr(1 + nu_u) * x_n - 18.0 * (1 + nu) * (nu_u - nu) * (1 - nu_u));

      A_x += PetscRealPart(((18.0 * PetscSqr(nu_u - nu)) / (eta * E_n)) * (PetscSinReal(R_star * PetscSqrtReal(x_n)) / (R_star * PetscSinReal(PetscSqrtReal(x_n))) - 1.0) * PetscExpReal(-x_n * t_star)); /* Cheng (7.395) */
    }
    u[0] = P_0 * A_x;
  }
  return PETSC_SUCCESS;
}

/* Boundary Kernels */
static void f0_terzaghi_bd_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], const PetscReal n[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal P = PetscRealPart(constants[5]);

  f0[0] = 0.0;
  f0[1] = P;
}

#if 0
static void f0_mandel_bd_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                                    const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                                    const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                                    PetscReal t, const PetscReal x[], const PetscReal n[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  // Uniform stress distribution
  /* PetscScalar xmax =  0.5;
  PetscScalar xmin = -0.5;
  PetscScalar ymax =  0.5;
  PetscScalar ymin = -0.5;
  PetscScalar P = constants[5];
  PetscScalar aL = (xmax - xmin) / 2.0;
  PetscScalar sigma_zz = -1.0*P / aL; */

  // Analytical (parabolic) stress distribution
  PetscReal a1, a2, am;
  PetscReal y1, y2, ym;

  PetscInt NITER = 500;
  PetscReal EPS = 0.000001;
  PetscReal zeroArray[500]; /* NITER */
  PetscReal xmax =  1.0;
  PetscReal xmin =  0.0;
  PetscReal ymax =  0.1;
  PetscReal ymin =  0.0;
  PetscReal lower[2], upper[2];

  lower[0] = xmin - (xmax - xmin) / 2.0;
  lower[1] = ymin - (ymax - ymin) / 2.0;
  upper[0] = xmax - (xmax - xmin) / 2.0;
  upper[1] = ymax - (ymax - ymin) / 2.0;

  xmin = lower[0];
  ymin = lower[1];
  xmax = upper[0];
  ymax = upper[1];

  PetscScalar G     = constants[0];
  PetscScalar K_u   = constants[1];
  PetscScalar alpha = constants[2];
  PetscScalar M     = constants[3];
  PetscScalar kappa = constants[4];
  PetscScalar F     = constants[5];

  PetscScalar K_d = K_u - alpha*alpha*M;
  PetscScalar nu = (3.0*K_d - 2.0*G) / (2.0*(3.0*K_d + G));
  PetscScalar nu_u = (3.0*K_u - 2.0*G) / (2.0*(3.0*K_u + G));
  PetscReal   aL = (xmax - xmin) / 2.0;
  PetscReal   c = PetscRealPart(((2.0*kappa*G) * (1.0 - nu) * (nu_u - nu)) / (alpha*alpha * (1.0 - 2.0*nu) * (1.0 - nu_u)));
  PetscScalar B = (3.0 * (nu_u - nu)) / ( alpha * (1.0 - 2.0*nu) * (1.0 + nu_u));
  PetscScalar A1 = 3.0 / (B * (1.0 + nu_u));
  PetscScalar A2 = (alpha * (1.0 - 2.0*nu)) / (1.0 - nu);

  // Generate zero values
  for (PetscInt i=1; i < NITER+1; i++)
  {
    a1 = ((PetscReal) i - 1.0) * PETSC_PI * PETSC_PI / 4.0 + EPS;
    a2 = a1 + PETSC_PI/2;
    for (PetscInt j=0; j<NITER; j++)
    {
      y1 = PetscTanReal(a1) - PetscRealPart(A1/A2)*a1;
      y2 = PetscTanReal(a2) - PetscRealPart(A1/A2)*a2;
      am = (a1 + a2)/2.0;
      ym = PetscTanReal(am) - PetscRealPart(A1/A2)*am;
      if ((ym*y1) > 0)
      {
        a1 = am;
      } else {
        a2 = am;
      }
      if (PetscAbsReal(y2) < EPS)
      {
        am = a2;
      }
    }
    zeroArray[i-1] = am;
  }

  // Solution for sigma_zz
  PetscScalar A_x = 0.0;
  PetscScalar B_x = 0.0;

  for (PetscInt n=1; n < NITER+1; n++)
  {
    PetscReal alpha_n = zeroArray[n-1];

    A_x += ( PetscSinReal(alpha_n) / (alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n))) * PetscCosReal( (alpha_n * x[0]) / aL) * PetscExpReal( -1.0*( (alpha_n*alpha_n*c*t)/(aL*aL)));
    B_x += ( (PetscSinReal(alpha_n) * PetscCosReal(alpha_n))/(alpha_n - PetscSinReal(alpha_n) * PetscCosReal(alpha_n))) * PetscExpReal( -1.0*( (alpha_n*alpha_n*c*t)/(aL*aL)));
  }

  PetscScalar sigma_zz = -1.0*(F/aL) - ((2.0*F)/aL) * (A2/A1) * A_x + ((2.0*F)/aL) * B_x;

  if (x[1] == ymax) {
    f0[1] += sigma_zz;
  } else if (x[1] == ymin) {
    f0[1] -= sigma_zz;
  }
}
#endif

static void f0_cryer_bd_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], const PetscReal n[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal P_0 = PetscRealPart(constants[5]);
  PetscInt        d;

  for (d = 0; d < dim; ++d) f0[d] = -P_0 * n[d];
}

/* Standard Kernels - Residual */
/* f0_e */
static void f0_epsilon(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscInt d;

  for (d = 0; d < dim; ++d) f0[0] += u_x[d * dim + d];
  f0[0] -= u[uOff[1]];
}

/* f0_p */
static void f0_p(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal alpha = PetscRealPart(constants[2]);
  const PetscReal M     = PetscRealPart(constants[3]);

  f0[0] += alpha * u_t[uOff[1]];
  f0[0] += u_t[uOff[2]] / M;
  if (f0[0] != f0[0]) abort();
}

/* f1_u */
static void f1_u(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f1[])
{
  const PetscInt  Nc     = dim;
  const PetscReal G      = PetscRealPart(constants[0]);
  const PetscReal K_u    = PetscRealPart(constants[1]);
  const PetscReal alpha  = PetscRealPart(constants[2]);
  const PetscReal M      = PetscRealPart(constants[3]);
  const PetscReal K_d    = K_u - alpha * alpha * M;
  const PetscReal lambda = K_d - (2.0 * G) / 3.0;
  PetscInt        c, d;

  for (c = 0; c < Nc; ++c) {
    for (d = 0; d < dim; ++d) f1[c * dim + d] -= G * (u_x[c * dim + d] + u_x[d * dim + c]);
    f1[c * dim + c] -= lambda * u[uOff[1]];
    f1[c * dim + c] += alpha * u[uOff[2]];
  }
}

/* f1_p */
static void f1_p(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f1[])
{
  const PetscReal kappa = PetscRealPart(constants[4]);
  PetscInt        d;

  for (d = 0; d < dim; ++d) f1[d] += kappa * u_x[uOff_x[2] + d];
}

/*
  \partial_df \phi_fc g_{fc,gc,df,dg} \partial_dg \phi_gc

  \partial_df \phi_fc \lambda \delta_{fc,df} \sum_gc \partial_dg \phi_gc \delta_{gc,dg}
  = \partial_fc \phi_fc \sum_gc \partial_gc \phi_gc
*/

/* Standard Kernels - Jacobian */
/* g0_ee */
static void g0_ee(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g0[])
{
  g0[0] = -1.0;
}

/* g0_pe */
static void g0_pe(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g0[])
{
  const PetscReal alpha = PetscRealPart(constants[2]);

  g0[0] = u_tShift * alpha;
}

/* g0_pp */
static void g0_pp(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g0[])
{
  const PetscReal M = PetscRealPart(constants[3]);

  g0[0] = u_tShift / M;
}

/* g1_eu */
static void g1_eu(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g1[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) g1[d * dim + d] = 1.0; /* \frac{\partial\phi^{u_d}}{\partial x_d} */
}

/* g2_ue */
static void g2_ue(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g2[])
{
  const PetscReal G      = PetscRealPart(constants[0]);
  const PetscReal K_u    = PetscRealPart(constants[1]);
  const PetscReal alpha  = PetscRealPart(constants[2]);
  const PetscReal M      = PetscRealPart(constants[3]);
  const PetscReal K_d    = K_u - alpha * alpha * M;
  const PetscReal lambda = K_d - (2.0 * G) / 3.0;
  PetscInt        d;

  for (d = 0; d < dim; ++d) g2[d * dim + d] -= lambda;
}
/* g2_up */
static void g2_up(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g2[])
{
  const PetscReal alpha = PetscRealPart(constants[2]);
  PetscInt        d;

  for (d = 0; d < dim; ++d) g2[d * dim + d] += alpha;
}

/* g3_uu */
static void g3_uu(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g3[])
{
  const PetscInt  Nc = dim;
  const PetscReal G  = PetscRealPart(constants[0]);
  PetscInt        c, d;

  for (c = 0; c < Nc; ++c) {
    for (d = 0; d < dim; ++d) {
      g3[((c * Nc + c) * dim + d) * dim + d] -= G;
      g3[((c * Nc + d) * dim + d) * dim + c] -= G;
    }
  }
}

/* g3_pp */
static void g3_pp(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g3[])
{
  const PetscReal kappa = PetscRealPart(constants[4]);
  PetscInt        d;

  for (d = 0; d < dim; ++d) g3[d * dim + d] += kappa;
}

static PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscInt sol;

  PetscFunctionBeginUser;
  options->solType   = SOL_QUADRATIC_TRIG;
  options->niter     = 500;
  options->eps       = PETSC_SMALL;
  options->dtInitial = -1.0;
  PetscOptionsBegin(comm, "", "Biot Poroelasticity Options", "DMPLEX");
  PetscCall(PetscOptionsInt("-niter", "Number of series term iterations in exact solutions", "ex53.c", options->niter, &options->niter, NULL));
  sol = options->solType;
  PetscCall(PetscOptionsEList("-sol_type", "Type of exact solution", "ex53.c", solutionTypes, NUM_SOLUTION_TYPES, solutionTypes[options->solType], &sol, NULL));
  options->solType = (SolutionType)sol;
  PetscCall(PetscOptionsReal("-eps", "Precision value for root finding", "ex53.c", options->eps, &options->eps, NULL));
  PetscCall(PetscOptionsReal("-dt_initial", "Override the initial timestep", "ex53.c", options->dtInitial, &options->dtInitial, NULL));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode mandelZeros(MPI_Comm comm, AppCtx *ctx, Parameter *param)
{
  //PetscBag       bag;
  PetscReal a1, a2, am;
  PetscReal y1, y2, ym;

  PetscFunctionBeginUser;
  //PetscCall(PetscBagGetData(ctx->bag, (void **) &param));
  PetscInt  NITER = ctx->niter;
  PetscReal EPS   = ctx->eps;
  //const PetscScalar YMAX = param->ymax;
  //const PetscScalar YMIN = param->ymin;
  PetscScalar alpha = param->alpha;
  PetscScalar K_u   = param->K_u;
  PetscScalar M     = param->M;
  PetscScalar G     = param->mu;
  //const PetscScalar k = param->k;
  //const PetscScalar mu_f = param->mu_f;
  //const PetscScalar P_0 = param->P_0;

  PetscScalar K_d  = K_u - alpha * alpha * M;
  PetscScalar nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G));
  PetscScalar nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G));
  //const PetscScalar kappa = k / mu_f;

  // Generate zero values
  for (PetscInt i = 1; i < NITER + 1; i++) {
    a1 = ((PetscReal)i - 1.0) * PETSC_PI * PETSC_PI / 4.0 + EPS;
    a2 = a1 + PETSC_PI / 2;
    am = a1;
    for (PetscInt j = 0; j < NITER; j++) {
      y1 = PetscTanReal(a1) - PetscRealPart((1.0 - nu) / (nu_u - nu)) * a1;
      y2 = PetscTanReal(a2) - PetscRealPart((1.0 - nu) / (nu_u - nu)) * a2;
      am = (a1 + a2) / 2.0;
      ym = PetscTanReal(am) - PetscRealPart((1.0 - nu) / (nu_u - nu)) * am;
      if ((ym * y1) > 0) {
        a1 = am;
      } else {
        a2 = am;
      }
      if (PetscAbsReal(y2) < EPS) am = a2;
    }
    ctx->zeroArray[i - 1] = am;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscReal CryerFunction(PetscReal nu_u, PetscReal nu, PetscReal x)
{
  return PetscTanReal(PetscSqrtReal(x)) * (6.0 * (nu_u - nu) - (1.0 - nu) * (1.0 + nu_u) * x) - (6.0 * (nu_u - nu) * PetscSqrtReal(x));
}

static PetscErrorCode cryerZeros(MPI_Comm comm, AppCtx *ctx, Parameter *param)
{
  PetscReal alpha = PetscRealPart(param->alpha); /* -  */
  PetscReal K_u   = PetscRealPart(param->K_u);   /* Pa */
  PetscReal M     = PetscRealPart(param->M);     /* Pa */
  PetscReal G     = PetscRealPart(param->mu);    /* Pa */
  PetscInt  N     = ctx->niter, n;

  PetscReal K_d  = K_u - alpha * alpha * M;                         /* Pa,      Cheng (B.5)  */
  PetscReal nu   = (3.0 * K_d - 2.0 * G) / (2.0 * (3.0 * K_d + G)); /* -,       Cheng (B.8)  */
  PetscReal nu_u = (3.0 * K_u - 2.0 * G) / (2.0 * (3.0 * K_u + G)); /* -,       Cheng (B.9)  */

  PetscFunctionBeginUser;
  for (n = 1; n < N + 1; ++n) {
    PetscReal tol = PetscPowReal(n, 1.5) * ctx->eps;
    PetscReal a1 = 0., a2 = 0., am = 0.;
    PetscReal y1, y2, ym;
    PetscInt  j, k = n - 1;

    y1 = y2 = 1.;
    while (y1 * y2 > 0) {
      ++k;
      a1 = PetscSqr(n * PETSC_PI) - k * PETSC_PI;
      a2 = PetscSqr(n * PETSC_PI) + k * PETSC_PI;
      y1 = CryerFunction(nu_u, nu, a1);
      y2 = CryerFunction(nu_u, nu, a2);
    }
    for (j = 0; j < 50000; ++j) {
      y1 = CryerFunction(nu_u, nu, a1);
      y2 = CryerFunction(nu_u, nu, a2);
      PetscCheck(y1 * y2 <= 0, comm, PETSC_ERR_PLIB, "Invalid root finding initialization for root %" PetscInt_FMT ", (%g, %g)--(%g, %g)", n, (double)a1, (double)y1, (double)a2, (double)y2);
      am = (a1 + a2) / 2.0;
      ym = CryerFunction(nu_u, nu, am);
      if ((ym * y1) < 0) a2 = am;
      else a1 = am;
      if (PetscAbsReal(ym) < tol) break;
    }
    PetscCheck(PetscAbsReal(ym) < tol, comm, PETSC_ERR_PLIB, "Root finding did not converge for root %" PetscInt_FMT " (%g)", n, (double)PetscAbsReal(ym));
    ctx->zeroArray[n - 1] = am;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetupParameters(MPI_Comm comm, AppCtx *ctx)
{
  PetscBag   bag;
  Parameter *p;

  PetscFunctionBeginUser;
  /* setup PETSc parameter bag */
  PetscCall(PetscBagGetData(ctx->bag, (void **)&p));
  PetscCall(PetscBagSetName(ctx->bag, "par", "Poroelastic Parameters"));
  bag = ctx->bag;
  if (ctx->solType == SOL_TERZAGHI) {
    // Realistic values - Terzaghi
    PetscCall(PetscBagRegisterScalar(bag, &p->mu, 3.0, "mu", "Shear Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->K_u, 9.76, "K_u", "Undrained Bulk Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->alpha, 0.6, "alpha", "Biot Effective Stress Coefficient, -"));
    PetscCall(PetscBagRegisterScalar(bag, &p->M, 16.0, "M", "Biot Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->k, 1.5, "k", "Isotropic Permeability, m**2"));
    PetscCall(PetscBagRegisterScalar(bag, &p->mu_f, 1.0, "mu_f", "Fluid Dynamic Viscosity, Pa*s"));
    PetscCall(PetscBagRegisterScalar(bag, &p->P_0, 1.0, "P_0", "Magnitude of Vertical Stress, Pa"));
  } else if (ctx->solType == SOL_MANDEL) {
    // Realistic values - Mandel
    PetscCall(PetscBagRegisterScalar(bag, &p->mu, 0.75, "mu", "Shear Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->K_u, 2.6941176470588233, "K_u", "Undrained Bulk Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->alpha, 0.6, "alpha", "Biot Effective Stress Coefficient, -"));
    PetscCall(PetscBagRegisterScalar(bag, &p->M, 4.705882352941176, "M", "Biot Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->k, 1.5, "k", "Isotropic Permeability, m**2"));
    PetscCall(PetscBagRegisterScalar(bag, &p->mu_f, 1.0, "mu_f", "Fluid Dynamic Viscosity, Pa*s"));
    PetscCall(PetscBagRegisterScalar(bag, &p->P_0, 1.0, "P_0", "Magnitude of Vertical Stress, Pa"));
  } else if (ctx->solType == SOL_CRYER) {
    // Realistic values - Mandel
    PetscCall(PetscBagRegisterScalar(bag, &p->mu, 0.75, "mu", "Shear Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->K_u, 2.6941176470588233, "K_u", "Undrained Bulk Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->alpha, 0.6, "alpha", "Biot Effective Stress Coefficient, -"));
    PetscCall(PetscBagRegisterScalar(bag, &p->M, 4.705882352941176, "M", "Biot Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->k, 1.5, "k", "Isotropic Permeability, m**2"));
    PetscCall(PetscBagRegisterScalar(bag, &p->mu_f, 1.0, "mu_f", "Fluid Dynamic Viscosity, Pa*s"));
    PetscCall(PetscBagRegisterScalar(bag, &p->P_0, 1.0, "P_0", "Magnitude of Vertical Stress, Pa"));
  } else {
    // Nonsense values
    PetscCall(PetscBagRegisterScalar(bag, &p->mu, 1.0, "mu", "Shear Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->K_u, 1.0, "K_u", "Undrained Bulk Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->alpha, 1.0, "alpha", "Biot Effective Stress Coefficient, -"));
    PetscCall(PetscBagRegisterScalar(bag, &p->M, 1.0, "M", "Biot Modulus, Pa"));
    PetscCall(PetscBagRegisterScalar(bag, &p->k, 1.0, "k", "Isotropic Permeability, m**2"));
    PetscCall(PetscBagRegisterScalar(bag, &p->mu_f, 1.0, "mu_f", "Fluid Dynamic Viscosity, Pa*s"));
    PetscCall(PetscBagRegisterScalar(bag, &p->P_0, 1.0, "P_0", "Magnitude of Vertical Stress, Pa"));
  }
  PetscCall(PetscBagSetFromOptions(bag));
  {
    PetscScalar K_d  = p->K_u - p->alpha * p->alpha * p->M;
    PetscScalar nu_u = (3.0 * p->K_u - 2.0 * p->mu) / (2.0 * (3.0 * p->K_u + p->mu));
    PetscScalar nu   = (3.0 * K_d - 2.0 * p->mu) / (2.0 * (3.0 * K_d + p->mu));
    PetscScalar S    = (3.0 * p->K_u + 4.0 * p->mu) / (p->M * (3.0 * K_d + 4.0 * p->mu));
    PetscReal   c    = PetscRealPart((p->k / p->mu_f) / S);

    PetscViewer       viewer;
    PetscViewerFormat format;
    PetscBool         flg;

    switch (ctx->solType) {
    case SOL_QUADRATIC_LINEAR:
    case SOL_QUADRATIC_TRIG:
    case SOL_TRIG_LINEAR:
      ctx->t_r = PetscSqr(ctx->xmax[0] - ctx->xmin[0]) / c;
      break;
    case SOL_TERZAGHI:
      ctx->t_r = PetscSqr(2.0 * (ctx->xmax[1] - ctx->xmin[1])) / c;
      break;
    case SOL_MANDEL:
      ctx->t_r = PetscSqr(2.0 * (ctx->xmax[1] - ctx->xmin[1])) / c;
      break;
    case SOL_CRYER:
      ctx->t_r = PetscSqr(ctx->xmax[1]) / c;
      break;
    default:
      SETERRQ(comm, PETSC_ERR_ARG_WRONG, "Invalid solution type: %s (%d)", solutionTypes[PetscMin(ctx->solType, NUM_SOLUTION_TYPES)], ctx->solType);
    }
    PetscCall(PetscOptionsGetViewer(comm, NULL, NULL, "-param_view", &viewer, &format, &flg));
    if (flg) {
      PetscCall(PetscViewerPushFormat(viewer, format));
      PetscCall(PetscBagView(bag, viewer));
      PetscCall(PetscViewerFlush(viewer));
      PetscCall(PetscViewerPopFormat(viewer));
      PetscCall(PetscViewerDestroy(&viewer));
      PetscCall(PetscPrintf(comm, "  Max displacement: %g %g\n", (double)PetscRealPart(p->P_0 * (ctx->xmax[1] - ctx->xmin[1]) * (1. - 2. * nu_u) / (2. * p->mu * (1. - nu_u))), (double)PetscRealPart(p->P_0 * (ctx->xmax[1] - ctx->xmin[1]) * (1. - 2. * nu) / (2. * p->mu * (1. - nu)))));
      PetscCall(PetscPrintf(comm, "  Relaxation time: %g\n", (double)ctx->t_r));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CreateMesh(MPI_Comm comm, AppCtx *user, DM *dm)
{
  PetscFunctionBeginUser;
  PetscCall(DMCreate(comm, dm));
  PetscCall(DMSetType(*dm, DMPLEX));
  PetscCall(DMSetFromOptions(*dm));
  PetscCall(DMSetApplicationContext(*dm, user));
  PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
  PetscCall(DMGetBoundingBox(*dm, user->xmin, user->xmax));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetupPrimalProblem(DM dm, AppCtx *user)
{
  PetscErrorCode (*exact[3])(PetscInt, PetscReal, const PetscReal[], PetscInt, PetscScalar *, void *);
  PetscErrorCode (*exact_t[3])(PetscInt, PetscReal, const PetscReal[], PetscInt, PetscScalar *, void *);
  PetscDS       ds;
  DMLabel       label;
  PetscWeakForm wf;
  Parameter    *param;
  PetscInt      id_mandel[2];
  PetscInt      comp[1];
  PetscInt      comp_mandel[2];
  PetscInt      dim, id, bd, f;

  PetscFunctionBeginUser;
  PetscCall(DMGetLabel(dm, "marker", &label));
  PetscCall(DMGetDS(dm, &ds));
  PetscCall(PetscDSGetSpatialDimension(ds, &dim));
  PetscCall(PetscBagGetData(user->bag, (void **)&param));
  exact_t[0] = exact_t[1] = exact_t[2] = zero;

  /* Setup Problem Formulation and Boundary Conditions */
  switch (user->solType) {
  case SOL_QUADRATIC_LINEAR:
    PetscCall(PetscDSSetResidual(ds, 0, f0_quadratic_linear_u, f1_u));
    PetscCall(PetscDSSetResidual(ds, 1, f0_epsilon, NULL));
    PetscCall(PetscDSSetResidual(ds, 2, f0_quadratic_linear_p, f1_p));
    PetscCall(PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_uu));
    PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_ue, NULL));
    PetscCall(PetscDSSetJacobian(ds, 0, 2, NULL, NULL, g2_up, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_eu, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 1, g0_ee, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 1, g0_pe, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 2, g0_pp, NULL, NULL, g3_pp));
    exact[0]   = quadratic_u;
    exact[1]   = linear_eps;
    exact[2]   = linear_linear_p;
    exact_t[2] = linear_linear_p_t;

    id = 1;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall displacement", label, 1, &id, 0, 0, NULL, (void (*)(void))exact[0], NULL, user, NULL));
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall pressure", label, 1, &id, 2, 0, NULL, (void (*)(void))exact[2], (void (*)(void))exact_t[2], user, NULL));
    break;
  case SOL_TRIG_LINEAR:
    PetscCall(PetscDSSetResidual(ds, 0, f0_trig_linear_u, f1_u));
    PetscCall(PetscDSSetResidual(ds, 1, f0_epsilon, NULL));
    PetscCall(PetscDSSetResidual(ds, 2, f0_trig_linear_p, f1_p));
    PetscCall(PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_uu));
    PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_ue, NULL));
    PetscCall(PetscDSSetJacobian(ds, 0, 2, NULL, NULL, g2_up, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_eu, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 1, g0_ee, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 1, g0_pe, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 2, g0_pp, NULL, NULL, g3_pp));
    exact[0]   = trig_u;
    exact[1]   = trig_eps;
    exact[2]   = trig_linear_p;
    exact_t[2] = trig_linear_p_t;

    id = 1;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall displacement", label, 1, &id, 0, 0, NULL, (void (*)(void))exact[0], NULL, user, NULL));
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall pressure", label, 1, &id, 2, 0, NULL, (void (*)(void))exact[2], (void (*)(void))exact_t[2], user, NULL));
    break;
  case SOL_QUADRATIC_TRIG:
    PetscCall(PetscDSSetResidual(ds, 0, f0_quadratic_trig_u, f1_u));
    PetscCall(PetscDSSetResidual(ds, 1, f0_epsilon, NULL));
    PetscCall(PetscDSSetResidual(ds, 2, f0_quadratic_trig_p, f1_p));
    PetscCall(PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_uu));
    PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_ue, NULL));
    PetscCall(PetscDSSetJacobian(ds, 0, 2, NULL, NULL, g2_up, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_eu, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 1, g0_ee, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 1, g0_pe, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 2, g0_pp, NULL, NULL, g3_pp));
    exact[0]   = quadratic_u;
    exact[1]   = linear_eps;
    exact[2]   = linear_trig_p;
    exact_t[2] = linear_trig_p_t;

    id = 1;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall displacement", label, 1, &id, 0, 0, NULL, (void (*)(void))exact[0], NULL, user, NULL));
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall pressure", label, 1, &id, 2, 0, NULL, (void (*)(void))exact[2], (void (*)(void))exact_t[2], user, NULL));
    break;
  case SOL_TERZAGHI:
    PetscCall(PetscDSSetResidual(ds, 0, NULL, f1_u));
    PetscCall(PetscDSSetResidual(ds, 1, f0_epsilon, NULL));
    PetscCall(PetscDSSetResidual(ds, 2, f0_p, f1_p));
    PetscCall(PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_uu));
    PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_ue, NULL));
    PetscCall(PetscDSSetJacobian(ds, 0, 2, NULL, NULL, g2_up, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_eu, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 1, g0_ee, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 1, g0_pe, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 2, g0_pp, NULL, NULL, g3_pp));

    exact[0]   = terzaghi_2d_u;
    exact[1]   = terzaghi_2d_eps;
    exact[2]   = terzaghi_2d_p;
    exact_t[0] = terzaghi_2d_u_t;
    exact_t[1] = terzaghi_2d_eps_t;
    exact_t[2] = terzaghi_2d_p_t;

    id = 1;
    PetscCall(DMAddBoundary(dm, DM_BC_NATURAL, "vertical stress", label, 1, &id, 0, 0, NULL, NULL, NULL, user, &bd));
    PetscCall(PetscDSGetBoundary(ds, bd, &wf, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    PetscCall(PetscWeakFormSetIndexBdResidual(wf, label, id, 0, 0, 0, f0_terzaghi_bd_u, 0, NULL));

    id      = 3;
    comp[0] = 1;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "fixed base", label, 1, &id, 0, 1, comp, (void (*)(void))zero, NULL, user, NULL));
    id      = 2;
    comp[0] = 0;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "fixed side", label, 1, &id, 0, 1, comp, (void (*)(void))zero, NULL, user, NULL));
    id      = 4;
    comp[0] = 0;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "fixed side", label, 1, &id, 0, 1, comp, (void (*)(void))zero, NULL, user, NULL));
    id = 1;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "drained surface", label, 1, &id, 2, 0, NULL, (void (*)(void))terzaghi_drainage_pressure, NULL, user, NULL));
    break;
  case SOL_MANDEL:
    PetscCall(PetscDSSetResidual(ds, 0, NULL, f1_u));
    PetscCall(PetscDSSetResidual(ds, 1, f0_epsilon, NULL));
    PetscCall(PetscDSSetResidual(ds, 2, f0_p, f1_p));
    PetscCall(PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_uu));
    PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_ue, NULL));
    PetscCall(PetscDSSetJacobian(ds, 0, 2, NULL, NULL, g2_up, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_eu, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 1, g0_ee, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 1, g0_pe, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 2, g0_pp, NULL, NULL, g3_pp));

    PetscCall(mandelZeros(PETSC_COMM_WORLD, user, param));

    exact[0]   = mandel_2d_u;
    exact[1]   = mandel_2d_eps;
    exact[2]   = mandel_2d_p;
    exact_t[0] = mandel_2d_u_t;
    exact_t[1] = mandel_2d_eps_t;
    exact_t[2] = mandel_2d_p_t;

    id_mandel[0] = 3;
    id_mandel[1] = 1;
    //comp[0] = 1;
    comp_mandel[0] = 0;
    comp_mandel[1] = 1;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "vertical stress", label, 2, id_mandel, 0, 2, comp_mandel, (void (*)(void))mandel_2d_u, NULL, user, NULL));
    //PetscCall(DMAddBoundary(dm, DM_BC_NATURAL, "vertical stress", "marker", 0, 1, comp, NULL, 2, id_mandel, user));
    //PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "fixed base", "marker", 0, 1, comp, (void (*)(void)) zero, 2, id_mandel, user));
    //PetscCall(PetscDSSetBdResidual(ds, 0, f0_mandel_bd_u, NULL));

    id_mandel[0] = 2;
    id_mandel[1] = 4;
    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "drained surface", label, 2, id_mandel, 2, 0, NULL, (void (*)(void))zero, NULL, user, NULL));
    break;
  case SOL_CRYER:
    PetscCall(PetscDSSetResidual(ds, 0, NULL, f1_u));
    PetscCall(PetscDSSetResidual(ds, 1, f0_epsilon, NULL));
    PetscCall(PetscDSSetResidual(ds, 2, f0_p, f1_p));
    PetscCall(PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_uu));
    PetscCall(PetscDSSetJacobian(ds, 0, 1, NULL, NULL, g2_ue, NULL));
    PetscCall(PetscDSSetJacobian(ds, 0, 2, NULL, NULL, g2_up, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 0, NULL, g1_eu, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 1, 1, g0_ee, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 1, g0_pe, NULL, NULL, NULL));
    PetscCall(PetscDSSetJacobian(ds, 2, 2, g0_pp, NULL, NULL, g3_pp));

    PetscCall(cryerZeros(PETSC_COMM_WORLD, user, param));

    exact[0] = cryer_3d_u;
    exact[1] = cryer_3d_eps;
    exact[2] = cryer_3d_p;

    id = 1;
    PetscCall(DMAddBoundary(dm, DM_BC_NATURAL, "normal stress", label, 1, &id, 0, 0, NULL, NULL, NULL, user, &bd));
    PetscCall(PetscDSGetBoundary(ds, bd, &wf, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    PetscCall(PetscWeakFormSetIndexBdResidual(wf, label, id, 0, 0, 0, f0_cryer_bd_u, 0, NULL));

    PetscCall(DMAddBoundary(dm, DM_BC_ESSENTIAL, "drained surface", label, 1, &id, 2, 0, NULL, (void (*)(void))cryer_drainage_pressure, NULL, user, NULL));
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)ds), PETSC_ERR_ARG_WRONG, "Invalid solution type: %s (%d)", solutionTypes[PetscMin(user->solType, NUM_SOLUTION_TYPES)], user->solType);
  }
  for (f = 0; f < 3; ++f) {
    PetscCall(PetscDSSetExactSolution(ds, f, exact[f], user));
    PetscCall(PetscDSSetExactSolutionTimeDerivative(ds, f, exact_t[f], user));
  }

  /* Setup constants */
  {
    PetscScalar constants[6];
    constants[0] = param->mu;              /* shear modulus, Pa */
    constants[1] = param->K_u;             /* undrained bulk modulus, Pa */
    constants[2] = param->alpha;           /* Biot effective stress coefficient, - */
    constants[3] = param->M;               /* Biot modulus, Pa */
    constants[4] = param->k / param->mu_f; /* Darcy coefficient, m**2 / Pa*s */
    constants[5] = param->P_0;             /* Magnitude of Vertical Stress, Pa */
    PetscCall(PetscDSSetConstants(ds, 6, constants));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CreateElasticityNullSpace(DM dm, PetscInt origField, PetscInt field, MatNullSpace *nullspace)
{
  PetscFunctionBeginUser;
  PetscCall(DMPlexCreateRigidBody(dm, origField, nullspace));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetupFE(DM dm, PetscInt Nf, PetscInt Nc[], const char *name[], PetscErrorCode (*setup)(DM, AppCtx *), void *ctx)
{
  AppCtx         *user = (AppCtx *)ctx;
  DM              cdm  = dm;
  PetscFE         fe;
  PetscQuadrature q = NULL, fq = NULL;
  char            prefix[PETSC_MAX_PATH_LEN];
  PetscInt        dim, f;
  PetscBool       simplex;

  PetscFunctionBeginUser;
  /* Create finite element */
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMPlexIsSimplex(dm, &simplex));
  for (f = 0; f < Nf; ++f) {
    PetscCall(PetscSNPrintf(prefix, PETSC_MAX_PATH_LEN, "%s_", name[f]));
    PetscCall(PetscFECreateDefault(PETSC_COMM_SELF, dim, Nc[f], simplex, name[f] ? prefix : NULL, -1, &fe));
    PetscCall(PetscObjectSetName((PetscObject)fe, name[f]));
    if (!q) PetscCall(PetscFEGetQuadrature(fe, &q));
    if (!fq) PetscCall(PetscFEGetFaceQuadrature(fe, &fq));
    PetscCall(PetscFESetQuadrature(fe, q));
    PetscCall(PetscFESetFaceQuadrature(fe, fq));
    PetscCall(DMSetField(dm, f, NULL, (PetscObject)fe));
    PetscCall(PetscFEDestroy(&fe));
  }
  PetscCall(DMCreateDS(dm));
  PetscCall((*setup)(dm, user));
  while (cdm) {
    PetscCall(DMCopyDisc(dm, cdm));
    if (0) PetscCall(DMSetNearNullSpaceConstructor(cdm, 0, CreateElasticityNullSpace));
    /* TODO: Check whether the boundary of coarse meshes is marked */
    PetscCall(DMGetCoarseDM(cdm, &cdm));
  }
  PetscCall(PetscFEDestroy(&fe));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetInitialConditions(TS ts, Vec u)
{
  DM        dm;
  PetscReal t;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &dm));
  PetscCall(TSGetTime(ts, &t));
  if (t <= 0.0) {
    PetscErrorCode (*funcs[3])(PetscInt, PetscReal, const PetscReal[], PetscInt, PetscScalar *, void *);
    void   *ctxs[3];
    AppCtx *ctx;

    PetscCall(DMGetApplicationContext(dm, &ctx));
    switch (ctx->solType) {
    case SOL_TERZAGHI:
      funcs[0] = terzaghi_initial_u;
      ctxs[0]  = ctx;
      funcs[1] = terzaghi_initial_eps;
      ctxs[1]  = ctx;
      funcs[2] = terzaghi_drainage_pressure;
      ctxs[2]  = ctx;
      PetscCall(DMProjectFunction(dm, t, funcs, ctxs, INSERT_VALUES, u));
      break;
    case SOL_MANDEL:
      funcs[0] = mandel_initial_u;
      ctxs[0]  = ctx;
      funcs[1] = mandel_initial_eps;
      ctxs[1]  = ctx;
      funcs[2] = mandel_drainage_pressure;
      ctxs[2]  = ctx;
      PetscCall(DMProjectFunction(dm, t, funcs, ctxs, INSERT_VALUES, u));
      break;
    case SOL_CRYER:
      funcs[0] = cryer_initial_u;
      ctxs[0]  = ctx;
      funcs[1] = cryer_initial_eps;
      ctxs[1]  = ctx;
      funcs[2] = cryer_drainage_pressure;
      ctxs[2]  = ctx;
      PetscCall(DMProjectFunction(dm, t, funcs, ctxs, INSERT_VALUES, u));
      break;
    default:
      PetscCall(DMComputeExactSolution(dm, t, u, NULL));
    }
  } else {
    PetscCall(DMComputeExactSolution(dm, t, u, NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Need to create Viewer each time because HDF5 can get corrupted */
static PetscErrorCode SolutionMonitor(TS ts, PetscInt steps, PetscReal time, Vec u, void *mctx)
{
  DM                dm;
  Vec               exact;
  PetscViewer       viewer;
  PetscViewerFormat format;
  PetscOptions      options;
  const char       *prefix;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &dm));
  PetscCall(PetscObjectGetOptions((PetscObject)ts, &options));
  PetscCall(PetscObjectGetOptionsPrefix((PetscObject)ts, &prefix));
  PetscCall(PetscOptionsGetViewer(PetscObjectComm((PetscObject)ts), options, prefix, "-monitor_solution", &viewer, &format, NULL));
  PetscCall(DMGetGlobalVector(dm, &exact));
  PetscCall(DMComputeExactSolution(dm, time, exact, NULL));
  PetscCall(DMSetOutputSequenceNumber(dm, steps, time));
  PetscCall(VecView(exact, viewer));
  PetscCall(VecView(u, viewer));
  PetscCall(DMRestoreGlobalVector(dm, &exact));
  {
    PetscErrorCode (**exacts)(PetscInt, PetscReal, const PetscReal x[], PetscInt, PetscScalar *u, void *ctx);
    void     **ectxs;
    PetscReal *err;
    PetscInt   Nf, f;

    PetscCall(DMGetNumFields(dm, &Nf));
    PetscCall(PetscCalloc3(Nf, &exacts, Nf, &ectxs, PetscMax(1, Nf), &err));
    {
      PetscInt Nds, s;

      PetscCall(DMGetNumDS(dm, &Nds));
      for (s = 0; s < Nds; ++s) {
        PetscDS         ds;
        DMLabel         label;
        IS              fieldIS;
        const PetscInt *fields;
        PetscInt        dsNf, f;

        PetscCall(DMGetRegionNumDS(dm, s, &label, &fieldIS, &ds, NULL));
        PetscCall(PetscDSGetNumFields(ds, &dsNf));
        PetscCall(ISGetIndices(fieldIS, &fields));
        for (f = 0; f < dsNf; ++f) {
          const PetscInt field = fields[f];
          PetscCall(PetscDSGetExactSolution(ds, field, &exacts[field], &ectxs[field]));
        }
        PetscCall(ISRestoreIndices(fieldIS, &fields));
      }
    }
    PetscCall(DMComputeL2FieldDiff(dm, time, exacts, ectxs, u, err));
    PetscCall(PetscPrintf(PetscObjectComm((PetscObject)ts), "Time: %g L_2 Error: [", (double)time));
    for (f = 0; f < Nf; ++f) {
      if (f) PetscCall(PetscPrintf(PetscObjectComm((PetscObject)ts), ", "));
      PetscCall(PetscPrintf(PetscObjectComm((PetscObject)ts), "%g", (double)err[f]));
    }
    PetscCall(PetscPrintf(PetscObjectComm((PetscObject)ts), "]\n"));
    PetscCall(PetscFree3(exacts, ectxs, err));
  }
  PetscCall(PetscViewerDestroy(&viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SetupMonitor(TS ts, AppCtx *ctx)
{
  PetscViewer       viewer;
  PetscViewerFormat format;
  PetscOptions      options;
  const char       *prefix;
  PetscBool         flg;

  PetscFunctionBeginUser;
  PetscCall(PetscObjectGetOptions((PetscObject)ts, &options));
  PetscCall(PetscObjectGetOptionsPrefix((PetscObject)ts, &prefix));
  PetscCall(PetscOptionsGetViewer(PetscObjectComm((PetscObject)ts), options, prefix, "-monitor_solution", &viewer, &format, &flg));
  if (flg) PetscCall(TSMonitorSet(ts, SolutionMonitor, ctx, NULL));
  PetscCall(PetscViewerDestroy(&viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSAdaptChoose_Terzaghi(TSAdapt adapt, TS ts, PetscReal h, PetscInt *next_sc, PetscReal *next_h, PetscBool *accept, PetscReal *wlte, PetscReal *wltea, PetscReal *wlter)
{
  static PetscReal dtTarget = -1.0;
  PetscReal        dtInitial;
  DM               dm;
  AppCtx          *ctx;
  PetscInt         step;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &dm));
  PetscCall(DMGetApplicationContext(dm, &ctx));
  PetscCall(TSGetStepNumber(ts, &step));
  dtInitial = ctx->dtInitial < 0.0 ? 1.0e-4 * ctx->t_r : ctx->dtInitial;
  if (!step) {
    if (PetscAbsReal(dtInitial - h) > PETSC_SMALL) {
      *accept  = PETSC_FALSE;
      *next_h  = dtInitial;
      dtTarget = h;
    } else {
      *accept  = PETSC_TRUE;
      *next_h  = dtTarget < 0.0 ? dtInitial : dtTarget;
      dtTarget = -1.0;
    }
  } else {
    *accept = PETSC_TRUE;
    *next_h = h;
  }
  *next_sc = 0;  /* Reuse the same order scheme */
  *wlte    = -1; /* Weighted local truncation error was not evaluated */
  *wltea   = -1; /* Weighted absolute local truncation error was not evaluated */
  *wlter   = -1; /* Weighted relative local truncation error was not evaluated */
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  AppCtx      ctx; /* User-defined work context */
  DM          dm;  /* Problem specification */
  TS          ts;  /* Time Series / Nonlinear solver */
  Vec         u;   /* Solutions */
  const char *name[3] = {"displacement", "tracestrain", "pressure"};
  PetscReal   t;
  PetscInt    dim, Nc[3];

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  PetscCall(ProcessOptions(PETSC_COMM_WORLD, &ctx));
  PetscCall(PetscBagCreate(PETSC_COMM_SELF, sizeof(Parameter), &ctx.bag));
  PetscCall(PetscMalloc1(ctx.niter, &ctx.zeroArray));
  PetscCall(CreateMesh(PETSC_COMM_WORLD, &ctx, &dm));
  PetscCall(SetupParameters(PETSC_COMM_WORLD, &ctx));
  /* Primal System */
  PetscCall(TSCreate(PETSC_COMM_WORLD, &ts));
  PetscCall(DMSetApplicationContext(dm, &ctx));
  PetscCall(TSSetDM(ts, dm));

  PetscCall(DMGetDimension(dm, &dim));
  Nc[0] = dim;
  Nc[1] = 1;
  Nc[2] = 1;

  PetscCall(SetupFE(dm, 3, Nc, name, SetupPrimalProblem, &ctx));
  PetscCall(DMCreateGlobalVector(dm, &u));
  PetscCall(DMTSSetBoundaryLocal(dm, DMPlexTSComputeBoundary, &ctx));
  PetscCall(DMTSSetIFunctionLocal(dm, DMPlexTSComputeIFunctionFEM, &ctx));
  PetscCall(DMTSSetIJacobianLocal(dm, DMPlexTSComputeIJacobianFEM, &ctx));
  PetscCall(TSSetExactFinalTime(ts, TS_EXACTFINALTIME_MATCHSTEP));
  PetscCall(TSSetFromOptions(ts));
  PetscCall(TSSetComputeInitialCondition(ts, SetInitialConditions));
  PetscCall(SetupMonitor(ts, &ctx));

  if (ctx.solType != SOL_QUADRATIC_TRIG) {
    TSAdapt adapt;

    PetscCall(TSGetAdapt(ts, &adapt));
    adapt->ops->choose = TSAdaptChoose_Terzaghi;
  }
  if (ctx.solType == SOL_CRYER) {
    Mat          J;
    MatNullSpace sp;

    PetscCall(TSSetUp(ts));
    PetscCall(TSGetIJacobian(ts, &J, NULL, NULL, NULL));
    PetscCall(DMPlexCreateRigidBody(dm, 0, &sp));
    PetscCall(MatSetNullSpace(J, sp));
    PetscCall(MatNullSpaceDestroy(&sp));
  }
  PetscCall(TSGetTime(ts, &t));
  PetscCall(DMSetOutputSequenceNumber(dm, 0, t));
  PetscCall(DMTSCheckFromOptions(ts, u));
  PetscCall(SetInitialConditions(ts, u));
  PetscCall(PetscObjectSetName((PetscObject)u, "solution"));
  PetscCall(TSSolve(ts, u));
  PetscCall(DMTSCheckFromOptions(ts, u));
  PetscCall(TSGetSolution(ts, &u));
  PetscCall(VecViewFromOptions(u, NULL, "-sol_vec_view"));

  /* Cleanup */
  PetscCall(VecDestroy(&u));
  PetscCall(TSDestroy(&ts));
  PetscCall(DMDestroy(&dm));
  PetscCall(PetscBagDestroy(&ctx.bag));
  PetscCall(PetscFree(ctx.zeroArray));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

  test:
    suffix: 2d_quad_linear
    requires: triangle
    args: -sol_type quadratic_linear -dm_refine 2 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -dmts_check .0001 -ts_max_steps 5 -ts_monitor_extreme

  test:
    suffix: 3d_quad_linear
    requires: ctetgen
    args: -dm_plex_dim 3 -sol_type quadratic_linear -dm_refine 1 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -dmts_check .0001 -ts_max_steps 5 -ts_monitor_extreme

  test:
    suffix: 2d_trig_linear
    requires: triangle
    args: -sol_type trig_linear -dm_refine 1 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -dmts_check .0001 -ts_max_steps 5 -ts_dt 0.00001 -ts_monitor_extreme

  test:
    # -dm_refine 2 -convest_num_refine 3 get L_2 convergence rate: [1.9, 2.1, 1.8]
    suffix: 2d_trig_linear_sconv
    requires: triangle
    args: -sol_type trig_linear -dm_refine 1 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -convest_num_refine 1 -ts_convergence_estimate -ts_convergence_temporal 0 -ts_max_steps 1 -ts_dt 0.00001 -pc_type lu

  test:
    suffix: 3d_trig_linear
    requires: ctetgen
    args: -dm_plex_dim 3 -sol_type trig_linear -dm_refine 1 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -dmts_check .0001 -ts_max_steps 2 -ts_monitor_extreme

  test:
    # -dm_refine 1 -convest_num_refine 2 gets L_2 convergence rate: [2.0, 2.1, 1.9]
    suffix: 3d_trig_linear_sconv
    requires: ctetgen
    args: -dm_plex_dim 3 -sol_type trig_linear -dm_refine 1 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -convest_num_refine 1 -ts_convergence_estimate -ts_convergence_temporal 0 -ts_max_steps 1 -pc_type lu

  test:
    suffix: 2d_quad_trig
    requires: triangle
    args: -sol_type quadratic_trig -dm_refine 2 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -dmts_check .0001 -ts_max_steps 5 -ts_monitor_extreme

  test:
    # Using -dm_refine 4 gets the convergence rates to [0.95, 0.97, 0.90]
    suffix: 2d_quad_trig_tconv
    requires: triangle
    args: -sol_type quadratic_trig -dm_refine 1 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -convest_num_refine 3 -ts_convergence_estimate -ts_max_steps 5 -pc_type lu

  test:
    suffix: 3d_quad_trig
    requires: ctetgen
    args: -dm_plex_dim 3 -sol_type quadratic_trig -dm_refine 1 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -dmts_check .0001 -ts_max_steps 5 -ts_monitor_extreme

  test:
    # Using -dm_refine 2 -convest_num_refine 3 gets the convergence rates to [1.0, 1.0, 1.0]
    suffix: 3d_quad_trig_tconv
    requires: ctetgen
    args: -dm_plex_dim 3 -sol_type quadratic_trig -dm_refine 1 \
      -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
      -convest_num_refine 1 -ts_convergence_estimate -ts_max_steps 5 -pc_type lu

  testset:
    args: -sol_type terzaghi -dm_plex_simplex 0 -dm_plex_box_faces 1,8 -dm_plex_box_lower 0,0 -dm_plex_box_upper 10,10 -dm_plex_separate_marker \
          -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 -niter 16000 \
          -pc_type lu

    test:
      suffix: 2d_terzaghi
      requires: double
      args: -ts_dt 0.0028666667 -ts_max_steps 2 -ts_monitor -dmts_check .0001

    test:
      # -dm_plex_box_faces 1,64 -ts_max_steps 4 -convest_num_refine 3 gives L_2 convergence rate: [1.1, 1.1, 1.1]
      suffix: 2d_terzaghi_tconv
      args: -ts_dt 0.023 -ts_max_steps 2 -ts_convergence_estimate -convest_num_refine 1

    test:
      # -dm_plex_box_faces 1,16 -convest_num_refine 4 gives L_2 convergence rate: [1.7, 1.2, 1.1]
      # if we add -displacement_petscspace_degree 3 -tracestrain_petscspace_degree 2 -pressure_petscspace_degree 2, we get [2.1, 1.6, 1.5]
      suffix: 2d_terzaghi_sconv
      args: -ts_dt 1e-5 -dt_initial 1e-5 -ts_max_steps 2 -ts_convergence_estimate -ts_convergence_temporal 0 -convest_num_refine 1

  testset:
    args: -sol_type mandel -dm_plex_simplex 0 -dm_plex_box_lower -0.5,-0.125 -dm_plex_box_upper 0.5,0.125 -dm_plex_separate_marker -dm_refine 1 \
          -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1 \
          -pc_type lu

    test:
      suffix: 2d_mandel
      requires: double
      args: -ts_dt 0.0028666667 -ts_max_steps 2 -ts_monitor -dmts_check .0001

    test:
      # -dm_refine 3 -ts_max_steps 4 -convest_num_refine 3 gives L_2 convergence rate: [1.6, 0.93, 1.2]
      suffix: 2d_mandel_sconv
      args: -ts_dt 1e-5 -dt_initial 1e-5 -ts_max_steps 2 -ts_convergence_estimate -ts_convergence_temporal 0 -convest_num_refine 1

    test:
      # -dm_refine 5 -ts_max_steps 4 -convest_num_refine 3 gives L_2 convergence rate: [0.26, -0.0058, 0.26]
      suffix: 2d_mandel_tconv
      args: -ts_dt 0.023 -ts_max_steps 2 -ts_convergence_estimate -convest_num_refine 1

  testset:
    requires: ctetgen !complex
    args: -sol_type cryer -dm_plex_dim 3 -dm_plex_shape ball \
          -displacement_petscspace_degree 2 -tracestrain_petscspace_degree 1 -pressure_petscspace_degree 1

    test:
      suffix: 3d_cryer
      args: -ts_dt 0.0028666667 -ts_max_time 0.014333 -ts_max_steps 2 -dmts_check .0001 \
            -pc_type svd

    test:
      # -bd_dm_refine 3 -dm_refine_volume_limit_pre 0.004 -convest_num_refine 2 gives L_2 convergence rate: []
      suffix: 3d_cryer_sconv
      args: -bd_dm_refine 1 -dm_refine_volume_limit_pre 0.00666667 \
            -ts_dt 1e-5 -dt_initial 1e-5 -ts_max_steps 2 \
            -ts_convergence_estimate -ts_convergence_temporal 0 -convest_num_refine 1 \
            -pc_type lu -pc_factor_shift_type nonzero

    test:
      # Displacement and Pressure converge. The analytic expression for trace strain is inaccurate at the origin
      # -bd_dm_refine 3 -ref_limit 0.00666667 -ts_max_steps 5 -convest_num_refine 2 gives L_2 convergence rate: [0.47, -0.43, 1.5]
      suffix: 3d_cryer_tconv
      args: -bd_dm_refine 1 -dm_refine_volume_limit_pre 0.00666667 \
            -ts_dt 0.023 -ts_max_time 0.092 -ts_max_steps 2 -ts_convergence_estimate -convest_num_refine 1 \
            -pc_type lu -pc_factor_shift_type nonzero

TEST*/

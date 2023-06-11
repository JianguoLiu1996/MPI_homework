/* Discretization tools */

#include <petscdt.h> /*I "petscdt.h" I*/
#include <petscblaslapack.h>
#include <petsc/private/petscimpl.h>
#include <petsc/private/dtimpl.h>
#include <petsc/private/petscfeimpl.h> /* For CoordinatesRefToReal() */
#include <petscviewer.h>
#include <petscdmplex.h>
#include <petscdmshell.h>

#if defined(PETSC_HAVE_MPFR)
  #include <mpfr.h>
#endif

const char *const        PetscDTNodeTypes_shifted[] = {"default", "gaussjacobi", "equispaced", "tanhsinh", "PETSCDTNODES_", NULL};
const char *const *const PetscDTNodeTypes           = PetscDTNodeTypes_shifted + 1;

const char *const        PetscDTSimplexQuadratureTypes_shifted[] = {"default", "conic", "minsym", "PETSCDTSIMPLEXQUAD_", NULL};
const char *const *const PetscDTSimplexQuadratureTypes           = PetscDTSimplexQuadratureTypes_shifted + 1;

static PetscBool GolubWelschCite       = PETSC_FALSE;
const char       GolubWelschCitation[] = "@article{GolubWelsch1969,\n"
                                         "  author  = {Golub and Welsch},\n"
                                         "  title   = {Calculation of Quadrature Rules},\n"
                                         "  journal = {Math. Comp.},\n"
                                         "  volume  = {23},\n"
                                         "  number  = {106},\n"
                                         "  pages   = {221--230},\n"
                                         "  year    = {1969}\n}\n";

/* Numerical tests in src/dm/dt/tests/ex1.c show that when computing the nodes and weights of Gauss-Jacobi
   quadrature rules:

   - in double precision, Newton's method and Golub & Welsch both work for moderate degrees (< 100),
   - in single precision, Newton's method starts producing incorrect roots around n = 15, but
     the weights from Golub & Welsch become a problem before then: they produces errors
     in computing the Jacobi-polynomial Gram matrix around n = 6.

   So we default to Newton's method (required fewer dependencies) */
PetscBool PetscDTGaussQuadratureNewton_Internal = PETSC_TRUE;

PetscClassId PETSCQUADRATURE_CLASSID = 0;

/*@
  PetscQuadratureCreate - Create a `PetscQuadrature` object

  Collective

  Input Parameter:
. comm - The communicator for the `PetscQuadrature` object

  Output Parameter:
. q  - The `PetscQuadrature` object

  Level: beginner

.seealso: `PetscQuadrature`, `Petscquadraturedestroy()`, `PetscQuadratureGetData()`
@*/
PetscErrorCode PetscQuadratureCreate(MPI_Comm comm, PetscQuadrature *q)
{
  PetscFunctionBegin;
  PetscValidPointer(q, 2);
  PetscCall(DMInitializePackage());
  PetscCall(PetscHeaderCreate(*q, PETSCQUADRATURE_CLASSID, "PetscQuadrature", "Quadrature", "DT", comm, PetscQuadratureDestroy, PetscQuadratureView));
  (*q)->ct        = DM_POLYTOPE_UNKNOWN;
  (*q)->dim       = -1;
  (*q)->Nc        = 1;
  (*q)->order     = -1;
  (*q)->numPoints = 0;
  (*q)->points    = NULL;
  (*q)->weights   = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureDuplicate - Create a deep copy of the `PetscQuadrature` object

  Collective

  Input Parameter:
. q  - The `PetscQuadrature` object

  Output Parameter:
. r  - The new `PetscQuadrature` object

  Level: beginner

.seealso: `PetscQuadrature`, `PetscQuadratureCreate()`, `PetscQuadratureDestroy()`, `PetscQuadratureGetData()`
@*/
PetscErrorCode PetscQuadratureDuplicate(PetscQuadrature q, PetscQuadrature *r)
{
  DMPolytopeType   ct;
  PetscInt         order, dim, Nc, Nq;
  const PetscReal *points, *weights;
  PetscReal       *p, *w;

  PetscFunctionBegin;
  PetscValidPointer(q, 1);
  PetscCall(PetscQuadratureCreate(PetscObjectComm((PetscObject)q), r));
  PetscCall(PetscQuadratureGetCellType(q, &ct));
  PetscCall(PetscQuadratureSetCellType(*r, ct));
  PetscCall(PetscQuadratureGetOrder(q, &order));
  PetscCall(PetscQuadratureSetOrder(*r, order));
  PetscCall(PetscQuadratureGetData(q, &dim, &Nc, &Nq, &points, &weights));
  PetscCall(PetscMalloc1(Nq * dim, &p));
  PetscCall(PetscMalloc1(Nq * Nc, &w));
  PetscCall(PetscArraycpy(p, points, Nq * dim));
  PetscCall(PetscArraycpy(w, weights, Nc * Nq));
  PetscCall(PetscQuadratureSetData(*r, dim, Nc, Nq, p, w));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureDestroy - Destroys a `PetscQuadrature` object

  Collective

  Input Parameter:
. q  - The `PetscQuadrature` object

  Level: beginner

.seealso: `PetscQuadrature`, `PetscQuadratureCreate()`, `PetscQuadratureGetData()`
@*/
PetscErrorCode PetscQuadratureDestroy(PetscQuadrature *q)
{
  PetscFunctionBegin;
  if (!*q) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific((*q), PETSCQUADRATURE_CLASSID, 1);
  if (--((PetscObject)(*q))->refct > 0) {
    *q = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscFree((*q)->points));
  PetscCall(PetscFree((*q)->weights));
  PetscCall(PetscHeaderDestroy(q));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureGetCellType - Return the cell type of the integration domain

  Not Collective

  Input Parameter:
. q - The `PetscQuadrature` object

  Output Parameter:
. ct - The cell type of the integration domain

  Level: intermediate

.seealso: `PetscQuadrature`, `PetscQuadratureSetCellType()`, `PetscQuadratureGetData()`, `PetscQuadratureSetData()`
@*/
PetscErrorCode PetscQuadratureGetCellType(PetscQuadrature q, DMPolytopeType *ct)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  PetscValidPointer(ct, 2);
  *ct = q->ct;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureSetCellType - Set the cell type of the integration domain

  Not Collective

  Input Parameters:
+ q - The `PetscQuadrature` object
- ct - The cell type of the integration domain

  Level: intermediate

.seealso: `PetscQuadrature`, `PetscQuadratureGetCellType()`, `PetscQuadratureGetData()`, `PetscQuadratureSetData()`
@*/
PetscErrorCode PetscQuadratureSetCellType(PetscQuadrature q, DMPolytopeType ct)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  q->ct = ct;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureGetOrder - Return the order of the method in the `PetscQuadrature`

  Not Collective

  Input Parameter:
. q - The `PetscQuadrature` object

  Output Parameter:
. order - The order of the quadrature, i.e. the highest degree polynomial that is exactly integrated

  Level: intermediate

.seealso: `PetscQuadrature`, `PetscQuadratureSetOrder()`, `PetscQuadratureGetData()`, `PetscQuadratureSetData()`
@*/
PetscErrorCode PetscQuadratureGetOrder(PetscQuadrature q, PetscInt *order)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  PetscValidIntPointer(order, 2);
  *order = q->order;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureSetOrder - Set the order of the method in the `PetscQuadrature`

  Not Collective

  Input Parameters:
+ q - The `PetscQuadrature` object
- order - The order of the quadrature, i.e. the highest degree polynomial that is exactly integrated

  Level: intermediate

.seealso: `PetscQuadrature`, `PetscQuadratureGetOrder()`, `PetscQuadratureGetData()`, `PetscQuadratureSetData()`
@*/
PetscErrorCode PetscQuadratureSetOrder(PetscQuadrature q, PetscInt order)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  q->order = order;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureGetNumComponents - Return the number of components for functions to be integrated

  Not Collective

  Input Parameter:
. q - The `PetscQuadrature` object

  Output Parameter:
. Nc - The number of components

  Level: intermediate

  Note:
  We are performing an integral int f(x) . w(x) dx, where both f and w (the weight) have Nc components.

.seealso: `PetscQuadrature`, `PetscQuadratureSetNumComponents()`, `PetscQuadratureGetData()`, `PetscQuadratureSetData()`
@*/
PetscErrorCode PetscQuadratureGetNumComponents(PetscQuadrature q, PetscInt *Nc)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  PetscValidIntPointer(Nc, 2);
  *Nc = q->Nc;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureSetNumComponents - Return the number of components for functions to be integrated

  Not Collective

  Input Parameters:
+ q  - The `PetscQuadrature` object
- Nc - The number of components

  Level: intermediate

  Note:
  We are performing an integral int f(x) . w(x) dx, where both f and w (the weight) have Nc components.

.seealso: `PetscQuadrature`, `PetscQuadratureGetNumComponents()`, `PetscQuadratureGetData()`, `PetscQuadratureSetData()`
@*/
PetscErrorCode PetscQuadratureSetNumComponents(PetscQuadrature q, PetscInt Nc)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  q->Nc = Nc;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscQuadratureGetData - Returns the data defining the `PetscQuadrature`

  Not Collective

  Input Parameter:
. q  - The `PetscQuadrature` object

  Output Parameters:
+ dim - The spatial dimension
. Nc - The number of components
. npoints - The number of quadrature points
. points - The coordinates of each quadrature point
- weights - The weight of each quadrature point

  Level: intermediate

  Fortran Note:
  From Fortran you must call `PetscQuadratureRestoreData()` when you are done with the data

.seealso: `PetscQuadrature`, `PetscQuadratureCreate()`, `PetscQuadratureSetData()`
@*/
PetscErrorCode PetscQuadratureGetData(PetscQuadrature q, PetscInt *dim, PetscInt *Nc, PetscInt *npoints, const PetscReal *points[], const PetscReal *weights[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  if (dim) {
    PetscValidIntPointer(dim, 2);
    *dim = q->dim;
  }
  if (Nc) {
    PetscValidIntPointer(Nc, 3);
    *Nc = q->Nc;
  }
  if (npoints) {
    PetscValidIntPointer(npoints, 4);
    *npoints = q->numPoints;
  }
  if (points) {
    PetscValidPointer(points, 5);
    *points = q->points;
  }
  if (weights) {
    PetscValidPointer(weights, 6);
    *weights = q->weights;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureEqual - determine whether two quadratures are equivalent

  Input Parameters:
+ A - A `PetscQuadrature` object
- B - Another `PetscQuadrature` object

  Output Parameter:
. equal - `PETSC_TRUE` if the quadratures are the same

  Level: intermediate

.seealso: `PetscQuadrature`, `PetscQuadratureCreate()`
@*/
PetscErrorCode PetscQuadratureEqual(PetscQuadrature A, PetscQuadrature B, PetscBool *equal)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, PETSCQUADRATURE_CLASSID, 1);
  PetscValidHeaderSpecific(B, PETSCQUADRATURE_CLASSID, 2);
  PetscValidBoolPointer(equal, 3);
  *equal = PETSC_FALSE;
  if (A->ct != B->ct || A->dim != B->dim || A->Nc != B->Nc || A->order != B->order || A->numPoints != B->numPoints) PetscFunctionReturn(PETSC_SUCCESS);
  for (PetscInt i = 0; i < A->numPoints * A->dim; i++) {
    if (PetscAbsReal(A->points[i] - B->points[i]) > PETSC_SMALL) PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (!A->weights && !B->weights) {
    *equal = PETSC_TRUE;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (A->weights && B->weights) {
    for (PetscInt i = 0; i < A->numPoints; i++) {
      if (PetscAbsReal(A->weights[i] - B->weights[i]) > PETSC_SMALL) PetscFunctionReturn(PETSC_SUCCESS);
    }
    *equal = PETSC_TRUE;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDTJacobianInverse_Internal(PetscInt m, PetscInt n, const PetscReal J[], PetscReal Jinv[])
{
  PetscScalar *Js, *Jinvs;
  PetscInt     i, j, k;
  PetscBLASInt bm, bn, info;

  PetscFunctionBegin;
  if (!m || !n) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscBLASIntCast(m, &bm));
  PetscCall(PetscBLASIntCast(n, &bn));
#if defined(PETSC_USE_COMPLEX)
  PetscCall(PetscMalloc2(m * n, &Js, m * n, &Jinvs));
  for (i = 0; i < m * n; i++) Js[i] = J[i];
#else
  Js    = (PetscReal *)J;
  Jinvs = Jinv;
#endif
  if (m == n) {
    PetscBLASInt *pivots;
    PetscScalar  *W;

    PetscCall(PetscMalloc2(m, &pivots, m, &W));

    PetscCall(PetscArraycpy(Jinvs, Js, m * m));
    PetscCallBLAS("LAPACKgetrf", LAPACKgetrf_(&bm, &bm, Jinvs, &bm, pivots, &info));
    PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_LIB, "Error returned from LAPACKgetrf %" PetscInt_FMT, (PetscInt)info);
    PetscCallBLAS("LAPACKgetri", LAPACKgetri_(&bm, Jinvs, &bm, pivots, W, &bm, &info));
    PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_LIB, "Error returned from LAPACKgetri %" PetscInt_FMT, (PetscInt)info);
    PetscCall(PetscFree2(pivots, W));
  } else if (m < n) {
    PetscScalar  *JJT;
    PetscBLASInt *pivots;
    PetscScalar  *W;

    PetscCall(PetscMalloc1(m * m, &JJT));
    PetscCall(PetscMalloc2(m, &pivots, m, &W));
    for (i = 0; i < m; i++) {
      for (j = 0; j < m; j++) {
        PetscScalar val = 0.;

        for (k = 0; k < n; k++) val += Js[i * n + k] * Js[j * n + k];
        JJT[i * m + j] = val;
      }
    }

    PetscCallBLAS("LAPACKgetrf", LAPACKgetrf_(&bm, &bm, JJT, &bm, pivots, &info));
    PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_LIB, "Error returned from LAPACKgetrf %" PetscInt_FMT, (PetscInt)info);
    PetscCallBLAS("LAPACKgetri", LAPACKgetri_(&bm, JJT, &bm, pivots, W, &bm, &info));
    PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_LIB, "Error returned from LAPACKgetri %" PetscInt_FMT, (PetscInt)info);
    for (i = 0; i < n; i++) {
      for (j = 0; j < m; j++) {
        PetscScalar val = 0.;

        for (k = 0; k < m; k++) val += Js[k * n + i] * JJT[k * m + j];
        Jinvs[i * m + j] = val;
      }
    }
    PetscCall(PetscFree2(pivots, W));
    PetscCall(PetscFree(JJT));
  } else {
    PetscScalar  *JTJ;
    PetscBLASInt *pivots;
    PetscScalar  *W;

    PetscCall(PetscMalloc1(n * n, &JTJ));
    PetscCall(PetscMalloc2(n, &pivots, n, &W));
    for (i = 0; i < n; i++) {
      for (j = 0; j < n; j++) {
        PetscScalar val = 0.;

        for (k = 0; k < m; k++) val += Js[k * n + i] * Js[k * n + j];
        JTJ[i * n + j] = val;
      }
    }

    PetscCallBLAS("LAPACKgetrf", LAPACKgetrf_(&bn, &bn, JTJ, &bn, pivots, &info));
    PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_LIB, "Error returned from LAPACKgetrf %" PetscInt_FMT, (PetscInt)info);
    PetscCallBLAS("LAPACKgetri", LAPACKgetri_(&bn, JTJ, &bn, pivots, W, &bn, &info));
    PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_LIB, "Error returned from LAPACKgetri %" PetscInt_FMT, (PetscInt)info);
    for (i = 0; i < n; i++) {
      for (j = 0; j < m; j++) {
        PetscScalar val = 0.;

        for (k = 0; k < n; k++) val += JTJ[i * n + k] * Js[j * n + k];
        Jinvs[i * m + j] = val;
      }
    }
    PetscCall(PetscFree2(pivots, W));
    PetscCall(PetscFree(JTJ));
  }
#if defined(PETSC_USE_COMPLEX)
  for (i = 0; i < m * n; i++) Jinv[i] = PetscRealPart(Jinvs[i]);
  PetscCall(PetscFree2(Js, Jinvs));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscQuadraturePushForward - Push forward a quadrature functional under an affine transformation.

   Collective

   Input Parameters:
+  q - the quadrature functional
.  imageDim - the dimension of the image of the transformation
.  origin - a point in the original space
.  originImage - the image of the origin under the transformation
.  J - the Jacobian of the image: an [imageDim x dim] matrix in row major order
-  formDegree - transform the quadrature weights as k-forms of this form degree (if the number of components is a multiple of (dim choose formDegree), it is assumed that they represent multiple k-forms) [see `PetscDTAltVPullback()` for interpretation of formDegree]

   Output Parameter:
.  Jinvstarq - a quadrature rule where each point is the image of a point in the original quadrature rule, and where the k-form weights have been pulled-back by the pseudoinverse of `J` to the k-form weights in the image space.

   Level: intermediate

   Note:
   The new quadrature rule will have a different number of components if spaces have different dimensions.  For example, pushing a 2-form forward from a two dimensional space to a three dimensional space changes the number of components from 1 to 3.

.seealso: `PetscQuadrature`, `PetscDTAltVPullback()`, `PetscDTAltVPullbackMatrix()`
@*/
PetscErrorCode PetscQuadraturePushForward(PetscQuadrature q, PetscInt imageDim, const PetscReal origin[], const PetscReal originImage[], const PetscReal J[], PetscInt formDegree, PetscQuadrature *Jinvstarq)
{
  PetscInt         dim, Nc, imageNc, formSize, Ncopies, imageFormSize, Npoints, pt, i, j, c;
  const PetscReal *points;
  const PetscReal *weights;
  PetscReal       *imagePoints, *imageWeights;
  PetscReal       *Jinv;
  PetscReal       *Jinvstar;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  PetscCheck(imageDim >= PetscAbsInt(formDegree), PetscObjectComm((PetscObject)q), PETSC_ERR_ARG_INCOMP, "Cannot represent a %" PetscInt_FMT "-form in %" PetscInt_FMT " dimensions", PetscAbsInt(formDegree), imageDim);
  PetscCall(PetscQuadratureGetData(q, &dim, &Nc, &Npoints, &points, &weights));
  PetscCall(PetscDTBinomialInt(dim, PetscAbsInt(formDegree), &formSize));
  PetscCheck(Nc % formSize == 0, PetscObjectComm((PetscObject)q), PETSC_ERR_ARG_INCOMP, "Number of components %" PetscInt_FMT " is not a multiple of formSize %" PetscInt_FMT, Nc, formSize);
  Ncopies = Nc / formSize;
  PetscCall(PetscDTBinomialInt(imageDim, PetscAbsInt(formDegree), &imageFormSize));
  imageNc = Ncopies * imageFormSize;
  PetscCall(PetscMalloc1(Npoints * imageDim, &imagePoints));
  PetscCall(PetscMalloc1(Npoints * imageNc, &imageWeights));
  PetscCall(PetscMalloc2(imageDim * dim, &Jinv, formSize * imageFormSize, &Jinvstar));
  PetscCall(PetscDTJacobianInverse_Internal(imageDim, dim, J, Jinv));
  PetscCall(PetscDTAltVPullbackMatrix(imageDim, dim, Jinv, formDegree, Jinvstar));
  for (pt = 0; pt < Npoints; pt++) {
    const PetscReal *point      = &points[pt * dim];
    PetscReal       *imagePoint = &imagePoints[pt * imageDim];

    for (i = 0; i < imageDim; i++) {
      PetscReal val = originImage[i];

      for (j = 0; j < dim; j++) val += J[i * dim + j] * (point[j] - origin[j]);
      imagePoint[i] = val;
    }
    for (c = 0; c < Ncopies; c++) {
      const PetscReal *form      = &weights[pt * Nc + c * formSize];
      PetscReal       *imageForm = &imageWeights[pt * imageNc + c * imageFormSize];

      for (i = 0; i < imageFormSize; i++) {
        PetscReal val = 0.;

        for (j = 0; j < formSize; j++) val += Jinvstar[i * formSize + j] * form[j];
        imageForm[i] = val;
      }
    }
  }
  PetscCall(PetscQuadratureCreate(PetscObjectComm((PetscObject)q), Jinvstarq));
  PetscCall(PetscQuadratureSetData(*Jinvstarq, imageDim, imageNc, Npoints, imagePoints, imageWeights));
  PetscCall(PetscFree2(Jinv, Jinvstar));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscQuadratureSetData - Sets the data defining the quadrature

  Not Collective

  Input Parameters:
+ q  - The `PetscQuadrature` object
. dim - The spatial dimension
. Nc - The number of components
. npoints - The number of quadrature points
. points - The coordinates of each quadrature point
- weights - The weight of each quadrature point

  Level: intermediate

  Note:
  This routine owns the references to points and weights, so they must be allocated using `PetscMalloc()` and the user should not free them.

.seealso: `PetscQuadrature`, `PetscQuadratureCreate()`, `PetscQuadratureGetData()`
@*/
PetscErrorCode PetscQuadratureSetData(PetscQuadrature q, PetscInt dim, PetscInt Nc, PetscInt npoints, const PetscReal points[], const PetscReal weights[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  if (dim >= 0) q->dim = dim;
  if (Nc >= 0) q->Nc = Nc;
  if (npoints >= 0) q->numPoints = npoints;
  if (points) {
    PetscValidRealPointer(points, 5);
    q->points = points;
  }
  if (weights) {
    PetscValidRealPointer(weights, 6);
    q->weights = weights;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscQuadratureView_Ascii(PetscQuadrature quad, PetscViewer v)
{
  PetscInt          q, d, c;
  PetscViewerFormat format;

  PetscFunctionBegin;
  if (quad->Nc > 1)
    PetscCall(PetscViewerASCIIPrintf(v, "Quadrature on a %s of order %" PetscInt_FMT " on %" PetscInt_FMT " points (dim %" PetscInt_FMT ") with %" PetscInt_FMT " components\n", DMPolytopeTypes[quad->ct], quad->order, quad->numPoints, quad->dim, quad->Nc));
  else PetscCall(PetscViewerASCIIPrintf(v, "Quadrature on a %s of order %" PetscInt_FMT " on %" PetscInt_FMT " points (dim %" PetscInt_FMT ")\n", DMPolytopeTypes[quad->ct], quad->order, quad->numPoints, quad->dim));
  PetscCall(PetscViewerGetFormat(v, &format));
  if (format != PETSC_VIEWER_ASCII_INFO_DETAIL) PetscFunctionReturn(PETSC_SUCCESS);
  for (q = 0; q < quad->numPoints; ++q) {
    PetscCall(PetscViewerASCIIPrintf(v, "p%" PetscInt_FMT " (", q));
    PetscCall(PetscViewerASCIIUseTabs(v, PETSC_FALSE));
    for (d = 0; d < quad->dim; ++d) {
      if (d) PetscCall(PetscViewerASCIIPrintf(v, ", "));
      PetscCall(PetscViewerASCIIPrintf(v, "%+g", (double)quad->points[q * quad->dim + d]));
    }
    PetscCall(PetscViewerASCIIPrintf(v, ") "));
    if (quad->Nc > 1) PetscCall(PetscViewerASCIIPrintf(v, "w%" PetscInt_FMT " (", q));
    for (c = 0; c < quad->Nc; ++c) {
      if (c) PetscCall(PetscViewerASCIIPrintf(v, ", "));
      PetscCall(PetscViewerASCIIPrintf(v, "%+g", (double)quad->weights[q * quad->Nc + c]));
    }
    if (quad->Nc > 1) PetscCall(PetscViewerASCIIPrintf(v, ")"));
    PetscCall(PetscViewerASCIIPrintf(v, "\n"));
    PetscCall(PetscViewerASCIIUseTabs(v, PETSC_TRUE));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscQuadratureView - View a `PetscQuadrature` object

  Collective

  Input Parameters:
+ quad  - The `PetscQuadrature` object
- viewer - The `PetscViewer` object

  Level: beginner

.seealso: `PetscQuadrature`, `PetscViewer`, `PetscQuadratureCreate()`, `PetscQuadratureGetData()`
@*/
PetscErrorCode PetscQuadratureView(PetscQuadrature quad, PetscViewer viewer)
{
  PetscBool iascii;

  PetscFunctionBegin;
  PetscValidHeader(quad, 1);
  if (viewer) PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  if (!viewer) PetscCall(PetscViewerASCIIGetStdout(PetscObjectComm((PetscObject)quad), &viewer));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscViewerASCIIPushTab(viewer));
  if (iascii) PetscCall(PetscQuadratureView_Ascii(quad, viewer));
  PetscCall(PetscViewerASCIIPopTab(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscQuadratureExpandComposite - Return a quadrature over the composite element, which has the original quadrature in each subelement

  Not Collective; No Fortran Support

  Input Parameters:
+ q - The original `PetscQuadrature`
. numSubelements - The number of subelements the original element is divided into
. v0 - An array of the initial points for each subelement
- jac - An array of the Jacobian mappings from the reference to each subelement

  Output Parameter:
. dim - The dimension

  Level: intermediate

  Note:
  Together v0 and jac define an affine mapping from the original reference element to each subelement

.seealso: `PetscQuadrature`, `PetscFECreate()`, `PetscSpaceGetDimension()`, `PetscDualSpaceGetDimension()`
@*/
PetscErrorCode PetscQuadratureExpandComposite(PetscQuadrature q, PetscInt numSubelements, const PetscReal v0[], const PetscReal jac[], PetscQuadrature *qref)
{
  DMPolytopeType   ct;
  const PetscReal *points, *weights;
  PetscReal       *pointsRef, *weightsRef;
  PetscInt         dim, Nc, order, npoints, npointsRef, c, p, cp, d, e;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(q, PETSCQUADRATURE_CLASSID, 1);
  PetscValidRealPointer(v0, 3);
  PetscValidRealPointer(jac, 4);
  PetscValidPointer(qref, 5);
  PetscCall(PetscQuadratureCreate(PETSC_COMM_SELF, qref));
  PetscCall(PetscQuadratureGetCellType(q, &ct));
  PetscCall(PetscQuadratureGetOrder(q, &order));
  PetscCall(PetscQuadratureGetData(q, &dim, &Nc, &npoints, &points, &weights));
  npointsRef = npoints * numSubelements;
  PetscCall(PetscMalloc1(npointsRef * dim, &pointsRef));
  PetscCall(PetscMalloc1(npointsRef * Nc, &weightsRef));
  for (c = 0; c < numSubelements; ++c) {
    for (p = 0; p < npoints; ++p) {
      for (d = 0; d < dim; ++d) {
        pointsRef[(c * npoints + p) * dim + d] = v0[c * dim + d];
        for (e = 0; e < dim; ++e) pointsRef[(c * npoints + p) * dim + d] += jac[(c * dim + d) * dim + e] * (points[p * dim + e] + 1.0);
      }
      /* Could also use detJ here */
      for (cp = 0; cp < Nc; ++cp) weightsRef[(c * npoints + p) * Nc + cp] = weights[p * Nc + cp] / numSubelements;
    }
  }
  PetscCall(PetscQuadratureSetCellType(*qref, ct));
  PetscCall(PetscQuadratureSetOrder(*qref, order));
  PetscCall(PetscQuadratureSetData(*qref, dim, Nc, npointsRef, pointsRef, weightsRef));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Compute the coefficients for the Jacobi polynomial recurrence,
 *
 * J^{a,b}_n(x) = (cnm1 + cnm1x * x) * J^{a,b}_{n-1}(x) - cnm2 * J^{a,b}_{n-2}(x).
 */
#define PetscDTJacobiRecurrence_Internal(n, a, b, cnm1, cnm1x, cnm2) \
  do { \
    PetscReal _a = (a); \
    PetscReal _b = (b); \
    PetscReal _n = (n); \
    if (n == 1) { \
      (cnm1)  = (_a - _b) * 0.5; \
      (cnm1x) = (_a + _b + 2.) * 0.5; \
      (cnm2)  = 0.; \
    } else { \
      PetscReal _2n  = _n + _n; \
      PetscReal _d   = (_2n * (_n + _a + _b) * (_2n + _a + _b - 2)); \
      PetscReal _n1  = (_2n + _a + _b - 1.) * (_a * _a - _b * _b); \
      PetscReal _n1x = (_2n + _a + _b - 1.) * (_2n + _a + _b) * (_2n + _a + _b - 2); \
      PetscReal _n2  = 2. * ((_n + _a - 1.) * (_n + _b - 1.) * (_2n + _a + _b)); \
      (cnm1)         = _n1 / _d; \
      (cnm1x)        = _n1x / _d; \
      (cnm2)         = _n2 / _d; \
    } \
  } while (0)

/*@
  PetscDTJacobiNorm - Compute the weighted L2 norm of a Jacobi polynomial.

  $\| P^{\alpha,\beta}_n \|_{\alpha,\beta}^2 = \int_{-1}^1 (1 + x)^{\alpha} (1 - x)^{\beta} P^{\alpha,\beta}_n (x)^2 dx.$

  Input Parameters:
- alpha - the left exponent > -1
. beta - the right exponent > -1
+ n - the polynomial degree

  Output Parameter:
. norm - the weighted L2 norm

  Level: beginner

.seealso: `PetscQuadrature`, `PetscDTJacobiEval()`
@*/
PetscErrorCode PetscDTJacobiNorm(PetscReal alpha, PetscReal beta, PetscInt n, PetscReal *norm)
{
  PetscReal twoab1;
  PetscReal gr;

  PetscFunctionBegin;
  PetscCheck(alpha > -1., PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Exponent alpha %g <= -1. invalid", (double)alpha);
  PetscCheck(beta > -1., PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Exponent beta %g <= -1. invalid", (double)beta);
  PetscCheck(n >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "n %" PetscInt_FMT " < 0 invalid", n);
  twoab1 = PetscPowReal(2., alpha + beta + 1.);
#if defined(PETSC_HAVE_LGAMMA)
  if (!n) {
    gr = PetscExpReal(PetscLGamma(alpha + 1.) + PetscLGamma(beta + 1.) - PetscLGamma(alpha + beta + 2.));
  } else {
    gr = PetscExpReal(PetscLGamma(n + alpha + 1.) + PetscLGamma(n + beta + 1.) - (PetscLGamma(n + 1.) + PetscLGamma(n + alpha + beta + 1.))) / (n + n + alpha + beta + 1.);
  }
#else
  {
    PetscInt alphai = (PetscInt)alpha;
    PetscInt betai  = (PetscInt)beta;
    PetscInt i;

    gr = n ? (1. / (n + n + alpha + beta + 1.)) : 1.;
    if ((PetscReal)alphai == alpha) {
      if (!n) {
        for (i = 0; i < alphai; i++) gr *= (i + 1.) / (beta + i + 1.);
        gr /= (alpha + beta + 1.);
      } else {
        for (i = 0; i < alphai; i++) gr *= (n + i + 1.) / (n + beta + i + 1.);
      }
    } else if ((PetscReal)betai == beta) {
      if (!n) {
        for (i = 0; i < betai; i++) gr *= (i + 1.) / (alpha + i + 2.);
        gr /= (alpha + beta + 1.);
      } else {
        for (i = 0; i < betai; i++) gr *= (n + i + 1.) / (n + alpha + i + 1.);
      }
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "lgamma() - math routine is unavailable.");
  }
#endif
  *norm = PetscSqrtReal(twoab1 * gr);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDTJacobiEval_Internal(PetscInt npoints, PetscReal a, PetscReal b, PetscInt k, const PetscReal *points, PetscInt ndegree, const PetscInt *degrees, PetscReal *p)
{
  PetscReal ak, bk;
  PetscReal abk1;
  PetscInt  i, l, maxdegree;

  PetscFunctionBegin;
  maxdegree = degrees[ndegree - 1] - k;
  ak        = a + k;
  bk        = b + k;
  abk1      = a + b + k + 1.;
  if (maxdegree < 0) {
    for (i = 0; i < npoints; i++)
      for (l = 0; l < ndegree; l++) p[i * ndegree + l] = 0.;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  for (i = 0; i < npoints; i++) {
    PetscReal pm1, pm2, x;
    PetscReal cnm1, cnm1x, cnm2;
    PetscInt  j, m;

    x   = points[i];
    pm2 = 1.;
    PetscDTJacobiRecurrence_Internal(1, ak, bk, cnm1, cnm1x, cnm2);
    pm1 = (cnm1 + cnm1x * x);
    l   = 0;
    while (l < ndegree && degrees[l] - k < 0) p[l++] = 0.;
    while (l < ndegree && degrees[l] - k == 0) {
      p[l] = pm2;
      for (m = 0; m < k; m++) p[l] *= (abk1 + m) * 0.5;
      l++;
    }
    while (l < ndegree && degrees[l] - k == 1) {
      p[l] = pm1;
      for (m = 0; m < k; m++) p[l] *= (abk1 + 1 + m) * 0.5;
      l++;
    }
    for (j = 2; j <= maxdegree; j++) {
      PetscReal pp;

      PetscDTJacobiRecurrence_Internal(j, ak, bk, cnm1, cnm1x, cnm2);
      pp  = (cnm1 + cnm1x * x) * pm1 - cnm2 * pm2;
      pm2 = pm1;
      pm1 = pp;
      while (l < ndegree && degrees[l] - k == j) {
        p[l] = pp;
        for (m = 0; m < k; m++) p[l] *= (abk1 + j + m) * 0.5;
        l++;
      }
    }
    p += ndegree;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTJacobiEvalJet - Evaluate the jet (function and derivatives) of the Jacobi polynomials basis up to a given degree.
  The Jacobi polynomials with indices $\alpha$ and $\beta$ are orthogonal with respect to the weighted inner product
  $\langle f, g \rangle = \int_{-1}^1 (1+x)^{\alpha} (1-x)^{\beta} f(x) g(x) dx$.

  Input Parameters:
+ alpha - the left exponent of the weight
. beta - the right exponetn of the weight
. npoints - the number of points to evaluate the polynomials at
. points - [npoints] array of point coordinates
. degree - the maximm degree polynomial space to evaluate, (degree + 1) will be evaluated total.
- k - the maximum derivative to evaluate in the jet, (k + 1) will be evaluated total.

  Output Parameter:
. p - an array containing the evaluations of the Jacobi polynomials's jets on the points.  the size is (degree + 1) x
  (k + 1) x npoints, which also describes the order of the dimensions of this three-dimensional array: the first
  (slowest varying) dimension is polynomial degree; the second dimension is derivative order; the third (fastest
  varying) dimension is the index of the evaluation point.

  Level: advanced

.seealso: `PetscDTJacobiEval()`, `PetscDTPKDEvalJet()`
@*/
PetscErrorCode PetscDTJacobiEvalJet(PetscReal alpha, PetscReal beta, PetscInt npoints, const PetscReal points[], PetscInt degree, PetscInt k, PetscReal p[])
{
  PetscInt   i, j, l;
  PetscInt  *degrees;
  PetscReal *psingle;

  PetscFunctionBegin;
  if (degree == 0) {
    PetscInt zero = 0;

    for (i = 0; i <= k; i++) PetscCall(PetscDTJacobiEval_Internal(npoints, alpha, beta, i, points, 1, &zero, &p[i * npoints]));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscMalloc1(degree + 1, &degrees));
  PetscCall(PetscMalloc1((degree + 1) * npoints, &psingle));
  for (i = 0; i <= degree; i++) degrees[i] = i;
  for (i = 0; i <= k; i++) {
    PetscCall(PetscDTJacobiEval_Internal(npoints, alpha, beta, i, points, degree + 1, degrees, psingle));
    for (j = 0; j <= degree; j++) {
      for (l = 0; l < npoints; l++) p[(j * (k + 1) + i) * npoints + l] = psingle[l * (degree + 1) + j];
    }
  }
  PetscCall(PetscFree(psingle));
  PetscCall(PetscFree(degrees));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDTJacobiEval - evaluate Jacobi polynomials for the weight function $(1.+x)^{\alpha} (1.-x)^{\beta}$ at a set of points
                       at points

   Not Collective

   Input Parameters:
+  npoints - number of spatial points to evaluate at
.  alpha - the left exponent > -1
.  beta - the right exponent > -1
.  points - array of locations to evaluate at
.  ndegree - number of basis degrees to evaluate
-  degrees - sorted array of degrees to evaluate

   Output Parameters:
+  B - row-oriented basis evaluation matrix B[point*ndegree + degree] (dimension npoints*ndegrees, allocated by caller) (or NULL)
.  D - row-oriented derivative evaluation matrix (or NULL)
-  D2 - row-oriented second derivative evaluation matrix (or NULL)

   Level: intermediate

.seealso: `PetscDTGaussQuadrature()`, `PetscDTLegendreEval()`
@*/
PetscErrorCode PetscDTJacobiEval(PetscInt npoints, PetscReal alpha, PetscReal beta, const PetscReal *points, PetscInt ndegree, const PetscInt *degrees, PetscReal *B, PetscReal *D, PetscReal *D2)
{
  PetscFunctionBegin;
  PetscCheck(alpha > -1., PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "alpha must be > -1.");
  PetscCheck(beta > -1., PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "beta must be > -1.");
  if (!npoints || !ndegree) PetscFunctionReturn(PETSC_SUCCESS);
  if (B) PetscCall(PetscDTJacobiEval_Internal(npoints, alpha, beta, 0, points, ndegree, degrees, B));
  if (D) PetscCall(PetscDTJacobiEval_Internal(npoints, alpha, beta, 1, points, ndegree, degrees, D));
  if (D2) PetscCall(PetscDTJacobiEval_Internal(npoints, alpha, beta, 2, points, ndegree, degrees, D2));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDTLegendreEval - evaluate Legendre polynomials at points

   Not Collective

   Input Parameters:
+  npoints - number of spatial points to evaluate at
.  points - array of locations to evaluate at
.  ndegree - number of basis degrees to evaluate
-  degrees - sorted array of degrees to evaluate

   Output Parameters:
+  B - row-oriented basis evaluation matrix B[point*ndegree + degree] (dimension npoints*ndegrees, allocated by caller) (or NULL)
.  D - row-oriented derivative evaluation matrix (or NULL)
-  D2 - row-oriented second derivative evaluation matrix (or NULL)

   Level: intermediate

.seealso: `PetscDTGaussQuadrature()`
@*/
PetscErrorCode PetscDTLegendreEval(PetscInt npoints, const PetscReal *points, PetscInt ndegree, const PetscInt *degrees, PetscReal *B, PetscReal *D, PetscReal *D2)
{
  PetscFunctionBegin;
  PetscCall(PetscDTJacobiEval(npoints, 0., 0., points, ndegree, degrees, B, D, D2));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTIndexToGradedOrder - convert an index into a tuple of monomial degrees in a graded order (that is, if the degree sum of tuple x is less than the degree sum of tuple y, then the index of x is smaller than the index of y)

  Input Parameters:
+ len - the desired length of the degree tuple
- index - the index to convert: should be >= 0

  Output Parameter:
. degtup - will be filled with a tuple of degrees

  Level: beginner

  Note:
  For two tuples x and y with the same degree sum, partial degree sums over the final elements of the tuples
  acts as a tiebreaker.  For example, (2, 1, 1) and (1, 2, 1) have the same degree sum, but the degree sum over the
  last two elements is smaller for the former, so (2, 1, 1) < (1, 2, 1).

.seealso: `PetscDTGradedOrderToIndex()`
@*/
PetscErrorCode PetscDTIndexToGradedOrder(PetscInt len, PetscInt index, PetscInt degtup[])
{
  PetscInt i, total;
  PetscInt sum;

  PetscFunctionBeginHot;
  PetscCheck(len >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "length must be non-negative");
  PetscCheck(index >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "index must be non-negative");
  total = 1;
  sum   = 0;
  while (index >= total) {
    index -= total;
    total = (total * (len + sum)) / (sum + 1);
    sum++;
  }
  for (i = 0; i < len; i++) {
    PetscInt c;

    degtup[i] = sum;
    for (c = 0, total = 1; c < sum; c++) {
      /* going into the loop, total is the number of way to have a tuple of sum exactly c with length len - 1 - i */
      if (index < total) break;
      index -= total;
      total = (total * (len - 1 - i + c)) / (c + 1);
      degtup[i]--;
    }
    sum -= degtup[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTGradedOrderToIndex - convert a tuple into an index in a graded order, the inverse of `PetscDTIndexToGradedOrder()`.

  Input Parameters:
+ len - the length of the degree tuple
- degtup - tuple with this length

  Output Parameter:
. index - index in graded order: >= 0

  Level: Beginner

  Note:
  For two tuples x and y with the same degree sum, partial degree sums over the final elements of the tuples
  acts as a tiebreaker.  For example, (2, 1, 1) and (1, 2, 1) have the same degree sum, but the degree sum over the
  last two elements is smaller for the former, so (2, 1, 1) < (1, 2, 1).

.seealso: `PetscDTIndexToGradedOrder()`
@*/
PetscErrorCode PetscDTGradedOrderToIndex(PetscInt len, const PetscInt degtup[], PetscInt *index)
{
  PetscInt i, idx, sum, total;

  PetscFunctionBeginHot;
  PetscCheck(len >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "length must be non-negative");
  for (i = 0, sum = 0; i < len; i++) sum += degtup[i];
  idx   = 0;
  total = 1;
  for (i = 0; i < sum; i++) {
    idx += total;
    total = (total * (len + i)) / (i + 1);
  }
  for (i = 0; i < len - 1; i++) {
    PetscInt c;

    total = 1;
    sum -= degtup[i];
    for (c = 0; c < sum; c++) {
      idx += total;
      total = (total * (len - 1 - i + c)) / (c + 1);
    }
  }
  *index = idx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscBool PKDCite       = PETSC_FALSE;
const char       PKDCitation[] = "@article{Kirby2010,\n"
                                 "  title={Singularity-free evaluation of collapsed-coordinate orthogonal polynomials},\n"
                                 "  author={Kirby, Robert C},\n"
                                 "  journal={ACM Transactions on Mathematical Software (TOMS)},\n"
                                 "  volume={37},\n"
                                 "  number={1},\n"
                                 "  pages={1--16},\n"
                                 "  year={2010},\n"
                                 "  publisher={ACM New York, NY, USA}\n}\n";

/*@
  PetscDTPKDEvalJet - Evaluate the jet (function and derivatives) of the Proriol-Koornwinder-Dubiner (PKD) basis for
  the space of polynomials up to a given degree.  The PKD basis is L2-orthonormal on the biunit simplex (which is used
  as the reference element for finite elements in PETSc), which makes it a stable basis to use for evaluating
  polynomials in that domain.

  Input Parameters:
+ dim - the number of variables in the multivariate polynomials
. npoints - the number of points to evaluate the polynomials at
. points - [npoints x dim] array of point coordinates
. degree - the degree (sum of degrees on the variables in a monomial) of the polynomial space to evaluate.  There are ((dim + degree) choose dim) polynomials in this space.
- k - the maximum order partial derivative to evaluate in the jet.  There are (dim + k choose dim) partial derivatives
  in the jet.  Choosing k = 0 means to evaluate just the function and no derivatives

  Output Parameter:
. p - an array containing the evaluations of the PKD polynomials' jets on the points.  The size is ((dim + degree)
  choose dim) x ((dim + k) choose dim) x npoints, which also describes the order of the dimensions of this
  three-dimensional array: the first (slowest varying) dimension is basis function index; the second dimension is jet
  index; the third (fastest varying) dimension is the index of the evaluation point.

  Level: advanced

  Notes:
  The ordering of the basis functions, and the ordering of the derivatives in the jet, both follow the graded
  ordering of `PetscDTIndexToGradedOrder()` and `PetscDTGradedOrderToIndex()`.  For example, in 3D, the polynomial with
  leading monomial x^2,y^0,z^1, which has degree tuple (2,0,1), which by `PetscDTGradedOrderToIndex()` has index 12 (it is the 13th basis function in the space);
  the partial derivative $\partial_x \partial_z$ has order tuple (1,0,1), appears at index 6 in the jet (it is the 7th partial derivative in the jet).

  The implementation uses Kirby's singularity-free evaluation algorithm, https://doi.org/10.1145/1644001.1644006.

.seealso: `PetscDTGradedOrderToIndex()`, `PetscDTIndexToGradedOrder()`, `PetscDTJacobiEvalJet()`
@*/
PetscErrorCode PetscDTPKDEvalJet(PetscInt dim, PetscInt npoints, const PetscReal points[], PetscInt degree, PetscInt k, PetscReal p[])
{
  PetscInt   degidx, kidx, d, pt;
  PetscInt   Nk, Ndeg;
  PetscInt  *ktup, *degtup;
  PetscReal *scales, initscale, scaleexp;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(PKDCitation, &PKDCite));
  PetscCall(PetscDTBinomialInt(dim + k, k, &Nk));
  PetscCall(PetscDTBinomialInt(degree + dim, degree, &Ndeg));
  PetscCall(PetscMalloc2(dim, &degtup, dim, &ktup));
  PetscCall(PetscMalloc1(Ndeg, &scales));
  initscale = 1.;
  if (dim > 1) {
    PetscCall(PetscDTBinomial(dim, 2, &scaleexp));
    initscale = PetscPowReal(2., scaleexp * 0.5);
  }
  for (degidx = 0; degidx < Ndeg; degidx++) {
    PetscInt  e, i;
    PetscInt  m1idx = -1, m2idx = -1;
    PetscInt  n;
    PetscInt  degsum;
    PetscReal alpha;
    PetscReal cnm1, cnm1x, cnm2;
    PetscReal norm;

    PetscCall(PetscDTIndexToGradedOrder(dim, degidx, degtup));
    for (d = dim - 1; d >= 0; d--)
      if (degtup[d]) break;
    if (d < 0) { /* constant is 1 everywhere, all derivatives are zero */
      scales[degidx] = initscale;
      for (e = 0; e < dim; e++) {
        PetscCall(PetscDTJacobiNorm(e, 0., 0, &norm));
        scales[degidx] /= norm;
      }
      for (i = 0; i < npoints; i++) p[degidx * Nk * npoints + i] = 1.;
      for (i = 0; i < (Nk - 1) * npoints; i++) p[(degidx * Nk + 1) * npoints + i] = 0.;
      continue;
    }
    n = degtup[d];
    degtup[d]--;
    PetscCall(PetscDTGradedOrderToIndex(dim, degtup, &m1idx));
    if (degtup[d] > 0) {
      degtup[d]--;
      PetscCall(PetscDTGradedOrderToIndex(dim, degtup, &m2idx));
      degtup[d]++;
    }
    degtup[d]++;
    for (e = 0, degsum = 0; e < d; e++) degsum += degtup[e];
    alpha = 2 * degsum + d;
    PetscDTJacobiRecurrence_Internal(n, alpha, 0., cnm1, cnm1x, cnm2);

    scales[degidx] = initscale;
    for (e = 0, degsum = 0; e < dim; e++) {
      PetscInt  f;
      PetscReal ealpha;
      PetscReal enorm;

      ealpha = 2 * degsum + e;
      for (f = 0; f < degsum; f++) scales[degidx] *= 2.;
      PetscCall(PetscDTJacobiNorm(ealpha, 0., degtup[e], &enorm));
      scales[degidx] /= enorm;
      degsum += degtup[e];
    }

    for (pt = 0; pt < npoints; pt++) {
      /* compute the multipliers */
      PetscReal thetanm1, thetanm1x, thetanm2;

      thetanm1x = dim - (d + 1) + 2. * points[pt * dim + d];
      for (e = d + 1; e < dim; e++) thetanm1x += points[pt * dim + e];
      thetanm1x *= 0.5;
      thetanm1 = (2. - (dim - (d + 1)));
      for (e = d + 1; e < dim; e++) thetanm1 -= points[pt * dim + e];
      thetanm1 *= 0.5;
      thetanm2 = thetanm1 * thetanm1;

      for (kidx = 0; kidx < Nk; kidx++) {
        PetscInt f;

        PetscCall(PetscDTIndexToGradedOrder(dim, kidx, ktup));
        /* first sum in the same derivative terms */
        p[(degidx * Nk + kidx) * npoints + pt] = (cnm1 * thetanm1 + cnm1x * thetanm1x) * p[(m1idx * Nk + kidx) * npoints + pt];
        if (m2idx >= 0) p[(degidx * Nk + kidx) * npoints + pt] -= cnm2 * thetanm2 * p[(m2idx * Nk + kidx) * npoints + pt];

        for (f = d; f < dim; f++) {
          PetscInt km1idx, mplty = ktup[f];

          if (!mplty) continue;
          ktup[f]--;
          PetscCall(PetscDTGradedOrderToIndex(dim, ktup, &km1idx));

          /* the derivative of  cnm1x * thetanm1x  wrt x variable f is 0.5 * cnm1x if f > d otherwise it is cnm1x */
          /* the derivative of  cnm1  * thetanm1   wrt x variable f is 0 if f == d, otherwise it is -0.5 * cnm1 */
          /* the derivative of -cnm2  * thetanm2   wrt x variable f is 0 if f == d, otherwise it is cnm2 * thetanm1 */
          if (f > d) {
            PetscInt f2;

            p[(degidx * Nk + kidx) * npoints + pt] += mplty * 0.5 * (cnm1x - cnm1) * p[(m1idx * Nk + km1idx) * npoints + pt];
            if (m2idx >= 0) {
              p[(degidx * Nk + kidx) * npoints + pt] += mplty * cnm2 * thetanm1 * p[(m2idx * Nk + km1idx) * npoints + pt];
              /* second derivatives of -cnm2  * thetanm2   wrt x variable f,f2 is like - 0.5 * cnm2 */
              for (f2 = f; f2 < dim; f2++) {
                PetscInt km2idx, mplty2 = ktup[f2];
                PetscInt factor;

                if (!mplty2) continue;
                ktup[f2]--;
                PetscCall(PetscDTGradedOrderToIndex(dim, ktup, &km2idx));

                factor = mplty * mplty2;
                if (f == f2) factor /= 2;
                p[(degidx * Nk + kidx) * npoints + pt] -= 0.5 * factor * cnm2 * p[(m2idx * Nk + km2idx) * npoints + pt];
                ktup[f2]++;
              }
            }
          } else {
            p[(degidx * Nk + kidx) * npoints + pt] += mplty * cnm1x * p[(m1idx * Nk + km1idx) * npoints + pt];
          }
          ktup[f]++;
        }
      }
    }
  }
  for (degidx = 0; degidx < Ndeg; degidx++) {
    PetscReal scale = scales[degidx];
    PetscInt  i;

    for (i = 0; i < Nk * npoints; i++) p[degidx * Nk * npoints + i] *= scale;
  }
  PetscCall(PetscFree(scales));
  PetscCall(PetscFree2(degtup, ktup));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTPTrimmedSize - The size of the trimmed polynomial space of k-forms with a given degree and form degree,
  which can be evaluated in `PetscDTPTrimmedEvalJet()`.

  Input Parameters:
+ dim - the number of variables in the multivariate polynomials
. degree - the degree (sum of degrees on the variables in a monomial) of the trimmed polynomial space.
- formDegree - the degree of the form

  Output Parameter:
- size - The number ((`dim` + `degree`) choose (`dim` + `formDegree`)) x ((`degree` + `formDegree` - 1) choose (`formDegree`))

  Level: advanced

.seealso: `PetscDTPTrimmedEvalJet()`
@*/
PetscErrorCode PetscDTPTrimmedSize(PetscInt dim, PetscInt degree, PetscInt formDegree, PetscInt *size)
{
  PetscInt Nrk, Nbpt; // number of trimmed polynomials

  PetscFunctionBegin;
  formDegree = PetscAbsInt(formDegree);
  PetscCall(PetscDTBinomialInt(degree + dim, degree + formDegree, &Nbpt));
  PetscCall(PetscDTBinomialInt(degree + formDegree - 1, formDegree, &Nrk));
  Nbpt *= Nrk;
  *size = Nbpt;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* there was a reference implementation based on section 4.4 of Arnold, Falk & Winther (acta numerica, 2006), but it
 * was inferior to this implementation */
static PetscErrorCode PetscDTPTrimmedEvalJet_Internal(PetscInt dim, PetscInt npoints, const PetscReal points[], PetscInt degree, PetscInt formDegree, PetscInt jetDegree, PetscReal p[])
{
  PetscInt  formDegreeOrig = formDegree;
  PetscBool formNegative   = (formDegreeOrig < 0) ? PETSC_TRUE : PETSC_FALSE;

  PetscFunctionBegin;
  formDegree = PetscAbsInt(formDegreeOrig);
  if (formDegree == 0) {
    PetscCall(PetscDTPKDEvalJet(dim, npoints, points, degree, jetDegree, p));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (formDegree == dim) {
    PetscCall(PetscDTPKDEvalJet(dim, npoints, points, degree - 1, jetDegree, p));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscInt Nbpt;
  PetscCall(PetscDTPTrimmedSize(dim, degree, formDegree, &Nbpt));
  PetscInt Nf;
  PetscCall(PetscDTBinomialInt(dim, formDegree, &Nf));
  PetscInt Nk;
  PetscCall(PetscDTBinomialInt(dim + jetDegree, dim, &Nk));
  PetscCall(PetscArrayzero(p, Nbpt * Nf * Nk * npoints));

  PetscInt Nbpm1; // number of scalar polynomials up to degree - 1;
  PetscCall(PetscDTBinomialInt(dim + degree - 1, dim, &Nbpm1));
  PetscReal *p_scalar;
  PetscCall(PetscMalloc1(Nbpm1 * Nk * npoints, &p_scalar));
  PetscCall(PetscDTPKDEvalJet(dim, npoints, points, degree - 1, jetDegree, p_scalar));
  PetscInt total = 0;
  // First add the full polynomials up to degree - 1 into the basis: take the scalar
  // and copy one for each form component
  for (PetscInt i = 0; i < Nbpm1; i++) {
    const PetscReal *src = &p_scalar[i * Nk * npoints];
    for (PetscInt f = 0; f < Nf; f++) {
      PetscReal *dest = &p[(total++ * Nf + f) * Nk * npoints];
      PetscCall(PetscArraycpy(dest, src, Nk * npoints));
    }
  }
  PetscInt *form_atoms;
  PetscCall(PetscMalloc1(formDegree + 1, &form_atoms));
  // construct the interior product pattern
  PetscInt(*pattern)[3];
  PetscInt Nf1; // number of formDegree + 1 forms
  PetscCall(PetscDTBinomialInt(dim, formDegree + 1, &Nf1));
  PetscInt nnz = Nf1 * (formDegree + 1);
  PetscCall(PetscMalloc1(Nf1 * (formDegree + 1), &pattern));
  PetscCall(PetscDTAltVInteriorPattern(dim, formDegree + 1, pattern));
  PetscReal centroid = (1. - dim) / (dim + 1.);
  PetscInt *deriv;
  PetscCall(PetscMalloc1(dim, &deriv));
  for (PetscInt d = dim; d >= formDegree + 1; d--) {
    PetscInt Nfd1; // number of formDegree + 1 forms in dimension d that include dx_0
                   // (equal to the number of formDegree forms in dimension d-1)
    PetscCall(PetscDTBinomialInt(d - 1, formDegree, &Nfd1));
    // The number of homogeneous (degree-1) scalar polynomials in d variables
    PetscInt Nh;
    PetscCall(PetscDTBinomialInt(d - 1 + degree - 1, d - 1, &Nh));
    const PetscReal *h_scalar = &p_scalar[(Nbpm1 - Nh) * Nk * npoints];
    for (PetscInt b = 0; b < Nh; b++) {
      const PetscReal *h_s = &h_scalar[b * Nk * npoints];
      for (PetscInt f = 0; f < Nfd1; f++) {
        // construct all formDegree+1 forms that start with dx_(dim - d) /\ ...
        form_atoms[0] = dim - d;
        PetscCall(PetscDTEnumSubset(d - 1, formDegree, f, &form_atoms[1]));
        for (PetscInt i = 0; i < formDegree; i++) form_atoms[1 + i] += form_atoms[0] + 1;
        PetscInt f_ind; // index of the resulting form
        PetscCall(PetscDTSubsetIndex(dim, formDegree + 1, form_atoms, &f_ind));
        PetscReal *p_f = &p[total++ * Nf * Nk * npoints];
        for (PetscInt nz = 0; nz < nnz; nz++) {
          PetscInt  i     = pattern[nz][0]; // formDegree component
          PetscInt  j     = pattern[nz][1]; // (formDegree + 1) component
          PetscInt  v     = pattern[nz][2]; // coordinate component
          PetscReal scale = v < 0 ? -1. : 1.;

          i     = formNegative ? (Nf - 1 - i) : i;
          scale = (formNegative && (i & 1)) ? -scale : scale;
          v     = v < 0 ? -(v + 1) : v;
          if (j != f_ind) continue;
          PetscReal *p_i = &p_f[i * Nk * npoints];
          for (PetscInt jet = 0; jet < Nk; jet++) {
            const PetscReal *h_jet = &h_s[jet * npoints];
            PetscReal       *p_jet = &p_i[jet * npoints];

            for (PetscInt pt = 0; pt < npoints; pt++) p_jet[pt] += scale * h_jet[pt] * (points[pt * dim + v] - centroid);
            PetscCall(PetscDTIndexToGradedOrder(dim, jet, deriv));
            deriv[v]++;
            PetscReal mult = deriv[v];
            PetscInt  l;
            PetscCall(PetscDTGradedOrderToIndex(dim, deriv, &l));
            if (l >= Nk) continue;
            p_jet = &p_i[l * npoints];
            for (PetscInt pt = 0; pt < npoints; pt++) p_jet[pt] += scale * mult * h_jet[pt];
            deriv[v]--;
          }
        }
      }
    }
  }
  PetscCheck(total == Nbpt, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Incorrectly counted P trimmed polynomials");
  PetscCall(PetscFree(deriv));
  PetscCall(PetscFree(pattern));
  PetscCall(PetscFree(form_atoms));
  PetscCall(PetscFree(p_scalar));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTPTrimmedEvalJet - Evaluate the jet (function and derivatives) of a basis of the trimmed polynomial k-forms up to
  a given degree.

  Input Parameters:
+ dim - the number of variables in the multivariate polynomials
. npoints - the number of points to evaluate the polynomials at
. points - [npoints x dim] array of point coordinates
. degree - the degree (sum of degrees on the variables in a monomial) of the trimmed polynomial space to evaluate.
           There are ((dim + degree) choose (dim + formDegree)) x ((degree + formDegree - 1) choose (formDegree)) polynomials in this space.
           (You can use `PetscDTPTrimmedSize()` to compute this size.)
. formDegree - the degree of the form
- jetDegree - the maximum order partial derivative to evaluate in the jet.  There are ((dim + jetDegree) choose dim) partial derivatives
              in the jet.  Choosing jetDegree = 0 means to evaluate just the function and no derivatives

  Output Parameter:
. p - an array containing the evaluations of the PKD polynomials' jets on the points.  The size is
      `PetscDTPTrimmedSize()` x ((dim + formDegree) choose dim) x ((dim + k) choose dim) x npoints,
      which also describes the order of the dimensions of this
      four-dimensional array:
        the first (slowest varying) dimension is basis function index;
        the second dimension is component of the form;
        the third dimension is jet index;
        the fourth (fastest varying) dimension is the index of the evaluation point.

  Level: advanced

  Notes:
  The ordering of the basis functions is not graded, so the basis functions are not nested by degree like `PetscDTPKDEvalJet()`.
  The basis functions are not an L2-orthonormal basis on any particular domain.

  The implementation is based on the description of the trimmed polynomials up to degree r as
  the direct sum of polynomials up to degree (r-1) and the Koszul differential applied to
  homogeneous polynomials of degree (r-1).

.seealso: `PetscDTPKDEvalJet()`, `PetscDTPTrimmedSize()`
@*/
PetscErrorCode PetscDTPTrimmedEvalJet(PetscInt dim, PetscInt npoints, const PetscReal points[], PetscInt degree, PetscInt formDegree, PetscInt jetDegree, PetscReal p[])
{
  PetscFunctionBegin;
  PetscCall(PetscDTPTrimmedEvalJet_Internal(dim, npoints, points, degree, formDegree, jetDegree, p));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* solve the symmetric tridiagonal eigenvalue system, writing the eigenvalues into eigs and the eigenvectors into V
 * with lds n; diag and subdiag are overwritten */
static PetscErrorCode PetscDTSymmetricTridiagonalEigensolve(PetscInt n, PetscReal diag[], PetscReal subdiag[], PetscReal eigs[], PetscScalar V[])
{
  char          jobz   = 'V'; /* eigenvalues and eigenvectors */
  char          range  = 'A'; /* all eigenvalues will be found */
  PetscReal     VL     = 0.;  /* ignored because range is 'A' */
  PetscReal     VU     = 0.;  /* ignored because range is 'A' */
  PetscBLASInt  IL     = 0;   /* ignored because range is 'A' */
  PetscBLASInt  IU     = 0;   /* ignored because range is 'A' */
  PetscReal     abstol = 0.;  /* unused */
  PetscBLASInt  bn, bm, ldz;  /* bm will equal bn on exit */
  PetscBLASInt *isuppz;
  PetscBLASInt  lwork, liwork;
  PetscReal     workquery;
  PetscBLASInt  iworkquery;
  PetscBLASInt *iwork;
  PetscBLASInt  info;
  PetscReal    *work = NULL;

  PetscFunctionBegin;
#if !defined(PETSCDTGAUSSIANQUADRATURE_EIG)
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "A LAPACK symmetric tridiagonal eigensolver could not be found");
#endif
  PetscCall(PetscBLASIntCast(n, &bn));
  PetscCall(PetscBLASIntCast(n, &ldz));
#if !defined(PETSC_MISSING_LAPACK_STEGR)
  PetscCall(PetscMalloc1(2 * n, &isuppz));
  lwork  = -1;
  liwork = -1;
  PetscCallBLAS("LAPACKstegr", LAPACKstegr_(&jobz, &range, &bn, diag, subdiag, &VL, &VU, &IL, &IU, &abstol, &bm, eigs, V, &ldz, isuppz, &workquery, &lwork, &iworkquery, &liwork, &info));
  PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_PLIB, "xSTEGR error");
  lwork  = (PetscBLASInt)workquery;
  liwork = (PetscBLASInt)iworkquery;
  PetscCall(PetscMalloc2(lwork, &work, liwork, &iwork));
  PetscCall(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
  PetscCallBLAS("LAPACKstegr", LAPACKstegr_(&jobz, &range, &bn, diag, subdiag, &VL, &VU, &IL, &IU, &abstol, &bm, eigs, V, &ldz, isuppz, work, &lwork, iwork, &liwork, &info));
  PetscCall(PetscFPTrapPop());
  PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_PLIB, "xSTEGR error");
  PetscCall(PetscFree2(work, iwork));
  PetscCall(PetscFree(isuppz));
#elif !defined(PETSC_MISSING_LAPACK_STEQR)
  jobz = 'I'; /* Compute eigenvalues and eigenvectors of the
                 tridiagonal matrix.  Z is initialized to the identity
                 matrix. */
  PetscCall(PetscMalloc1(PetscMax(1, 2 * n - 2), &work));
  PetscCallBLAS("LAPACKsteqr", LAPACKsteqr_("I", &bn, diag, subdiag, V, &ldz, work, &info));
  PetscCall(PetscFPTrapPop());
  PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_PLIB, "xSTEQR error");
  PetscCall(PetscFree(work));
  PetscCall(PetscArraycpy(eigs, diag, n));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Formula for the weights at the endpoints (-1 and 1) of Gauss-Lobatto-Jacobi
 * quadrature rules on the interval [-1, 1] */
static PetscErrorCode PetscDTGaussLobattoJacobiEndweights_Internal(PetscInt n, PetscReal alpha, PetscReal beta, PetscReal *leftw, PetscReal *rightw)
{
  PetscReal twoab1;
  PetscInt  m = n - 2;
  PetscReal a = alpha + 1.;
  PetscReal b = beta + 1.;
  PetscReal gra, grb;

  PetscFunctionBegin;
  twoab1 = PetscPowReal(2., a + b - 1.);
#if defined(PETSC_HAVE_LGAMMA)
  grb = PetscExpReal(2. * PetscLGamma(b + 1.) + PetscLGamma(m + 1.) + PetscLGamma(m + a + 1.) - (PetscLGamma(m + b + 1) + PetscLGamma(m + a + b + 1.)));
  gra = PetscExpReal(2. * PetscLGamma(a + 1.) + PetscLGamma(m + 1.) + PetscLGamma(m + b + 1.) - (PetscLGamma(m + a + 1) + PetscLGamma(m + a + b + 1.)));
#else
  {
    PetscInt alphai = (PetscInt)alpha;
    PetscInt betai  = (PetscInt)beta;

    if ((PetscReal)alphai == alpha && (PetscReal)betai == beta) {
      PetscReal binom1, binom2;

      PetscCall(PetscDTBinomial(m + b, b, &binom1));
      PetscCall(PetscDTBinomial(m + a + b, b, &binom2));
      grb = 1. / (binom1 * binom2);
      PetscCall(PetscDTBinomial(m + a, a, &binom1));
      PetscCall(PetscDTBinomial(m + a + b, a, &binom2));
      gra = 1. / (binom1 * binom2);
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "lgamma() - math routine is unavailable.");
  }
#endif
  *leftw  = twoab1 * grb / b;
  *rightw = twoab1 * gra / a;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Evaluates the nth jacobi polynomial with weight parameters a,b at a point x.
   Recurrence relations implemented from the pseudocode given in Karniadakis and Sherwin, Appendix B */
static inline PetscErrorCode PetscDTComputeJacobi(PetscReal a, PetscReal b, PetscInt n, PetscReal x, PetscReal *P)
{
  PetscReal pn1, pn2;
  PetscReal cnm1, cnm1x, cnm2;
  PetscInt  k;

  PetscFunctionBegin;
  if (!n) {
    *P = 1.0;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscDTJacobiRecurrence_Internal(1, a, b, cnm1, cnm1x, cnm2);
  pn2 = 1.;
  pn1 = cnm1 + cnm1x * x;
  if (n == 1) {
    *P = pn1;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  *P = 0.0;
  for (k = 2; k < n + 1; ++k) {
    PetscDTJacobiRecurrence_Internal(k, a, b, cnm1, cnm1x, cnm2);

    *P  = (cnm1 + cnm1x * x) * pn1 - cnm2 * pn2;
    pn2 = pn1;
    pn1 = *P;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Evaluates the first derivative of P_{n}^{a,b} at a point x. */
static inline PetscErrorCode PetscDTComputeJacobiDerivative(PetscReal a, PetscReal b, PetscInt n, PetscReal x, PetscInt k, PetscReal *P)
{
  PetscReal nP;
  PetscInt  i;

  PetscFunctionBegin;
  *P = 0.0;
  if (k > n) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscDTComputeJacobi(a + k, b + k, n - k, x, &nP));
  for (i = 0; i < k; i++) nP *= (a + b + n + 1. + i) * 0.5;
  *P = nP;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDTGaussJacobiQuadrature_Newton_Internal(PetscInt npoints, PetscReal a, PetscReal b, PetscReal x[], PetscReal w[])
{
  PetscInt  maxIter = 100;
  PetscReal eps     = PetscExpReal(0.75 * PetscLogReal(PETSC_MACHINE_EPSILON));
  PetscReal a1, a6, gf;
  PetscInt  k;

  PetscFunctionBegin;

  a1 = PetscPowReal(2.0, a + b + 1);
#if defined(PETSC_HAVE_LGAMMA)
  {
    PetscReal a2, a3, a4, a5;
    a2 = PetscLGamma(a + npoints + 1);
    a3 = PetscLGamma(b + npoints + 1);
    a4 = PetscLGamma(a + b + npoints + 1);
    a5 = PetscLGamma(npoints + 1);
    gf = PetscExpReal(a2 + a3 - (a4 + a5));
  }
#else
  {
    PetscInt ia, ib;

    ia = (PetscInt)a;
    ib = (PetscInt)b;
    gf = 1.;
    if (ia == a && ia >= 0) { /* compute ratio of rising factorals wrt a */
      for (k = 0; k < ia; k++) gf *= (npoints + 1. + k) / (npoints + b + 1. + k);
    } else if (b == b && ib >= 0) { /* compute ratio of rising factorials wrt b */
      for (k = 0; k < ib; k++) gf *= (npoints + 1. + k) / (npoints + a + 1. + k);
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "lgamma() - math routine is unavailable.");
  }
#endif

  a6 = a1 * gf;
  /* Computes the m roots of P_{m}^{a,b} on [-1,1] by Newton's method with Chebyshev points as initial guesses.
   Algorithm implemented from the pseudocode given by Karniadakis and Sherwin and Python in FIAT */
  for (k = 0; k < npoints; ++k) {
    PetscReal r = PetscCosReal(PETSC_PI * (1. - (4. * k + 3. + 2. * b) / (4. * npoints + 2. * (a + b + 1.)))), dP;
    PetscInt  j;

    if (k > 0) r = 0.5 * (r + x[k - 1]);
    for (j = 0; j < maxIter; ++j) {
      PetscReal s = 0.0, delta, f, fp;
      PetscInt  i;

      for (i = 0; i < k; ++i) s = s + 1.0 / (r - x[i]);
      PetscCall(PetscDTComputeJacobi(a, b, npoints, r, &f));
      PetscCall(PetscDTComputeJacobiDerivative(a, b, npoints, r, 1, &fp));
      delta = f / (fp - f * s);
      r     = r - delta;
      if (PetscAbsReal(delta) < eps) break;
    }
    x[k] = r;
    PetscCall(PetscDTComputeJacobiDerivative(a, b, npoints, x[k], 1, &dP));
    w[k] = a6 / (1.0 - PetscSqr(x[k])) / PetscSqr(dP);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Compute the diagonals of the Jacobi matrix used in Golub & Welsch algorithms for Gauss-Jacobi
 * quadrature weight calculations on [-1,1] for exponents (1. + x)^a (1.-x)^b */
static PetscErrorCode PetscDTJacobiMatrix_Internal(PetscInt nPoints, PetscReal a, PetscReal b, PetscReal *d, PetscReal *s)
{
  PetscInt i;

  PetscFunctionBegin;
  for (i = 0; i < nPoints; i++) {
    PetscReal A, B, C;

    PetscDTJacobiRecurrence_Internal(i + 1, a, b, A, B, C);
    d[i] = -A / B;
    if (i) s[i - 1] *= C / B;
    if (i < nPoints - 1) s[i] = 1. / B;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDTGaussJacobiQuadrature_GolubWelsch_Internal(PetscInt npoints, PetscReal a, PetscReal b, PetscReal *x, PetscReal *w)
{
  PetscReal mu0;
  PetscReal ga, gb, gab;
  PetscInt  i;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(GolubWelschCitation, &GolubWelschCite));

#if defined(PETSC_HAVE_TGAMMA)
  ga  = PetscTGamma(a + 1);
  gb  = PetscTGamma(b + 1);
  gab = PetscTGamma(a + b + 2);
#else
  {
    PetscInt ia, ib;

    ia = (PetscInt)a;
    ib = (PetscInt)b;
    if (ia == a && ib == b && ia + 1 > 0 && ib + 1 > 0 && ia + ib + 2 > 0) { /* All gamma(x) terms are (x-1)! terms */
      PetscCall(PetscDTFactorial(ia, &ga));
      PetscCall(PetscDTFactorial(ib, &gb));
      PetscCall(PetscDTFactorial(ia + ib + 1, &gb));
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "tgamma() - math routine is unavailable.");
  }
#endif
  mu0 = PetscPowReal(2., a + b + 1.) * ga * gb / gab;

#if defined(PETSCDTGAUSSIANQUADRATURE_EIG)
  {
    PetscReal   *diag, *subdiag;
    PetscScalar *V;

    PetscCall(PetscMalloc2(npoints, &diag, npoints, &subdiag));
    PetscCall(PetscMalloc1(npoints * npoints, &V));
    PetscCall(PetscDTJacobiMatrix_Internal(npoints, a, b, diag, subdiag));
    for (i = 0; i < npoints - 1; i++) subdiag[i] = PetscSqrtReal(subdiag[i]);
    PetscCall(PetscDTSymmetricTridiagonalEigensolve(npoints, diag, subdiag, x, V));
    for (i = 0; i < npoints; i++) w[i] = PetscSqr(PetscRealPart(V[i * npoints])) * mu0;
    PetscCall(PetscFree(V));
    PetscCall(PetscFree2(diag, subdiag));
  }
#else
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "A LAPACK symmetric tridiagonal eigensolver could not be found");
#endif
  { /* As of March 2, 2020, The Sun Performance Library breaks the LAPACK contract for xstegr and xsteqr: the
       eigenvalues are not guaranteed to be in ascending order.  So we heave a passive aggressive sigh and check that
       the eigenvalues are sorted */
    PetscBool sorted;

    PetscCall(PetscSortedReal(npoints, x, &sorted));
    if (!sorted) {
      PetscInt  *order, i;
      PetscReal *tmp;

      PetscCall(PetscMalloc2(npoints, &order, npoints, &tmp));
      for (i = 0; i < npoints; i++) order[i] = i;
      PetscCall(PetscSortRealWithPermutation(npoints, x, order));
      PetscCall(PetscArraycpy(tmp, x, npoints));
      for (i = 0; i < npoints; i++) x[i] = tmp[order[i]];
      PetscCall(PetscArraycpy(tmp, w, npoints));
      for (i = 0; i < npoints; i++) w[i] = tmp[order[i]];
      PetscCall(PetscFree2(order, tmp));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDTGaussJacobiQuadrature_Internal(PetscInt npoints, PetscReal alpha, PetscReal beta, PetscReal x[], PetscReal w[], PetscBool newton)
{
  PetscFunctionBegin;
  PetscCheck(npoints >= 1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Number of points must be positive");
  /* If asking for a 1D Lobatto point, just return the non-Lobatto 1D point */
  PetscCheck(alpha > -1., PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "alpha must be > -1.");
  PetscCheck(beta > -1., PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "beta must be > -1.");

  if (newton) PetscCall(PetscDTGaussJacobiQuadrature_Newton_Internal(npoints, alpha, beta, x, w));
  else PetscCall(PetscDTGaussJacobiQuadrature_GolubWelsch_Internal(npoints, alpha, beta, x, w));
  if (alpha == beta) { /* symmetrize */
    PetscInt i;
    for (i = 0; i < (npoints + 1) / 2; i++) {
      PetscInt  j  = npoints - 1 - i;
      PetscReal xi = x[i];
      PetscReal xj = x[j];
      PetscReal wi = w[i];
      PetscReal wj = w[j];

      x[i] = (xi - xj) / 2.;
      x[j] = (xj - xi) / 2.;
      w[i] = w[j] = (wi + wj) / 2.;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTGaussJacobiQuadrature - quadrature for the interval [a, b] with the weight function
  $(x-a)^\alpha (x-b)^\beta$.

  Not Collective

  Input Parameters:
+ npoints - the number of points in the quadrature rule
. a - the left endpoint of the interval
. b - the right endpoint of the interval
. alpha - the left exponent
- beta - the right exponent

  Output Parameters:
+ x - array of length `npoints`, the locations of the quadrature points
- w - array of length `npoints`, the weights of the quadrature points

  Level: intermediate

  Note:
  This quadrature rule is exact for polynomials up to degree 2*npoints - 1.

.seealso: `PetscDTGaussQuadrature()`
@*/
PetscErrorCode PetscDTGaussJacobiQuadrature(PetscInt npoints, PetscReal a, PetscReal b, PetscReal alpha, PetscReal beta, PetscReal x[], PetscReal w[])
{
  PetscInt i;

  PetscFunctionBegin;
  PetscCall(PetscDTGaussJacobiQuadrature_Internal(npoints, alpha, beta, x, w, PetscDTGaussQuadratureNewton_Internal));
  if (a != -1. || b != 1.) { /* shift */
    for (i = 0; i < npoints; i++) {
      x[i] = (x[i] + 1.) * ((b - a) / 2.) + a;
      w[i] *= (b - a) / 2.;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDTGaussLobattoJacobiQuadrature_Internal(PetscInt npoints, PetscReal alpha, PetscReal beta, PetscReal x[], PetscReal w[], PetscBool newton)
{
  PetscInt i;

  PetscFunctionBegin;
  PetscCheck(npoints >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Number of points must be positive");
  /* If asking for a 1D Lobatto point, just return the non-Lobatto 1D point */
  PetscCheck(alpha > -1., PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "alpha must be > -1.");
  PetscCheck(beta > -1., PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "beta must be > -1.");

  x[0]           = -1.;
  x[npoints - 1] = 1.;
  if (npoints > 2) PetscCall(PetscDTGaussJacobiQuadrature_Internal(npoints - 2, alpha + 1., beta + 1., &x[1], &w[1], newton));
  for (i = 1; i < npoints - 1; i++) w[i] /= (1. - x[i] * x[i]);
  PetscCall(PetscDTGaussLobattoJacobiEndweights_Internal(npoints, alpha, beta, &w[0], &w[npoints - 1]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTGaussLobattoJacobiQuadrature - quadrature for the interval [a, b] with the weight function
  $(x-a)^\alpha (x-b)^\beta$, with endpoints a and b included as quadrature points.

  Not Collective

  Input Parameters:
+ npoints - the number of points in the quadrature rule
. a - the left endpoint of the interval
. b - the right endpoint of the interval
. alpha - the left exponent
- beta - the right exponent

  Output Parameters:
+ x - array of length `npoints`, the locations of the quadrature points
- w - array of length `npoints`, the weights of the quadrature points

  Level: intermediate

  Note:
  This quadrature rule is exact for polynomials up to degree 2*npoints - 3.

.seealso: `PetscDTGaussJacobiQuadrature()`
@*/
PetscErrorCode PetscDTGaussLobattoJacobiQuadrature(PetscInt npoints, PetscReal a, PetscReal b, PetscReal alpha, PetscReal beta, PetscReal x[], PetscReal w[])
{
  PetscInt i;

  PetscFunctionBegin;
  PetscCall(PetscDTGaussLobattoJacobiQuadrature_Internal(npoints, alpha, beta, x, w, PetscDTGaussQuadratureNewton_Internal));
  if (a != -1. || b != 1.) { /* shift */
    for (i = 0; i < npoints; i++) {
      x[i] = (x[i] + 1.) * ((b - a) / 2.) + a;
      w[i] *= (b - a) / 2.;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDTGaussQuadrature - create Gauss-Legendre quadrature

   Not Collective

   Input Parameters:
+  npoints - number of points
.  a - left end of interval (often-1)
-  b - right end of interval (often +1)

   Output Parameters:
+  x - quadrature points
-  w - quadrature weights

   Level: intermediate

   References:
.  * - Golub and Welsch, Calculation of Quadrature Rules, Math. Comp. 23(106), 1969.

.seealso: `PetscDTLegendreEval()`, `PetscDTGaussJacobiQuadrature()`
@*/
PetscErrorCode PetscDTGaussQuadrature(PetscInt npoints, PetscReal a, PetscReal b, PetscReal *x, PetscReal *w)
{
  PetscInt i;

  PetscFunctionBegin;
  PetscCall(PetscDTGaussJacobiQuadrature_Internal(npoints, 0., 0., x, w, PetscDTGaussQuadratureNewton_Internal));
  if (a != -1. || b != 1.) { /* shift */
    for (i = 0; i < npoints; i++) {
      x[i] = (x[i] + 1.) * ((b - a) / 2.) + a;
      w[i] *= (b - a) / 2.;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDTGaussLobattoLegendreQuadrature - creates a set of the locations and weights of the Gauss-Lobatto-Legendre
                      nodes of a given size on the domain [-1,1]

   Not Collective

   Input Parameters:
+  n - number of grid nodes
-  type - `PETSCGAUSSLOBATTOLEGENDRE_VIA_LINEAR_ALGEBRA` or `PETSCGAUSSLOBATTOLEGENDRE_VIA_NEWTON`

   Output Parameters:
+  x - quadrature points
-  w - quadrature weights

   Level: intermediate

   Notes:
    For n > 30  the Newton approach computes duplicate (incorrect) values for some nodes because the initial guess is apparently not
          close enough to the desired solution

   These are useful for implementing spectral methods based on Gauss-Lobatto-Legendre (GLL) nodes

   See  https://epubs.siam.org/doi/abs/10.1137/110855442  https://epubs.siam.org/doi/abs/10.1137/120889873 for better ways to compute GLL nodes

.seealso: `PetscDTGaussQuadrature()`, `PetscGaussLobattoLegendreCreateType`

@*/
PetscErrorCode PetscDTGaussLobattoLegendreQuadrature(PetscInt npoints, PetscGaussLobattoLegendreCreateType type, PetscReal *x, PetscReal *w)
{
  PetscBool newton;

  PetscFunctionBegin;
  PetscCheck(npoints >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Must provide at least 2 grid points per element");
  newton = (PetscBool)(type == PETSCGAUSSLOBATTOLEGENDRE_VIA_NEWTON);
  PetscCall(PetscDTGaussLobattoJacobiQuadrature_Internal(npoints, 0., 0., x, w, newton));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTGaussTensorQuadrature - creates a tensor-product Gauss quadrature

  Not Collective

  Input Parameters:
+ dim     - The spatial dimension
. Nc      - The number of components
. npoints - number of points in one dimension
. a       - left end of interval (often-1)
- b       - right end of interval (often +1)

  Output Parameter:
. q - A `PetscQuadrature` object

  Level: intermediate

.seealso: `PetscDTGaussQuadrature()`, `PetscDTLegendreEval()`
@*/
PetscErrorCode PetscDTGaussTensorQuadrature(PetscInt dim, PetscInt Nc, PetscInt npoints, PetscReal a, PetscReal b, PetscQuadrature *q)
{
  DMPolytopeType ct;
  PetscInt       totpoints = dim > 1 ? dim > 2 ? npoints * PetscSqr(npoints) : PetscSqr(npoints) : npoints;
  PetscReal     *x, *w, *xw, *ww;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(totpoints * dim, &x));
  PetscCall(PetscMalloc1(totpoints * Nc, &w));
  /* Set up the Golub-Welsch system */
  switch (dim) {
  case 0:
    ct = DM_POLYTOPE_POINT;
    PetscCall(PetscFree(x));
    PetscCall(PetscFree(w));
    PetscCall(PetscMalloc1(1, &x));
    PetscCall(PetscMalloc1(Nc, &w));
    x[0] = 0.0;
    for (PetscInt c = 0; c < Nc; ++c) w[c] = 1.0;
    break;
  case 1:
    ct = DM_POLYTOPE_SEGMENT;
    PetscCall(PetscMalloc1(npoints, &ww));
    PetscCall(PetscDTGaussQuadrature(npoints, a, b, x, ww));
    for (PetscInt i = 0; i < npoints; ++i)
      for (PetscInt c = 0; c < Nc; ++c) w[i * Nc + c] = ww[i];
    PetscCall(PetscFree(ww));
    break;
  case 2:
    ct = DM_POLYTOPE_QUADRILATERAL;
    PetscCall(PetscMalloc2(npoints, &xw, npoints, &ww));
    PetscCall(PetscDTGaussQuadrature(npoints, a, b, xw, ww));
    for (PetscInt i = 0; i < npoints; ++i) {
      for (PetscInt j = 0; j < npoints; ++j) {
        x[(i * npoints + j) * dim + 0] = xw[i];
        x[(i * npoints + j) * dim + 1] = xw[j];
        for (PetscInt c = 0; c < Nc; ++c) w[(i * npoints + j) * Nc + c] = ww[i] * ww[j];
      }
    }
    PetscCall(PetscFree2(xw, ww));
    break;
  case 3:
    ct = DM_POLYTOPE_HEXAHEDRON;
    PetscCall(PetscMalloc2(npoints, &xw, npoints, &ww));
    PetscCall(PetscDTGaussQuadrature(npoints, a, b, xw, ww));
    for (PetscInt i = 0; i < npoints; ++i) {
      for (PetscInt j = 0; j < npoints; ++j) {
        for (PetscInt k = 0; k < npoints; ++k) {
          x[((i * npoints + j) * npoints + k) * dim + 0] = xw[i];
          x[((i * npoints + j) * npoints + k) * dim + 1] = xw[j];
          x[((i * npoints + j) * npoints + k) * dim + 2] = xw[k];
          for (PetscInt c = 0; c < Nc; ++c) w[((i * npoints + j) * npoints + k) * Nc + c] = ww[i] * ww[j] * ww[k];
        }
      }
    }
    PetscCall(PetscFree2(xw, ww));
    break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Cannot construct quadrature rule for dimension %" PetscInt_FMT, dim);
  }
  PetscCall(PetscQuadratureCreate(PETSC_COMM_SELF, q));
  PetscCall(PetscQuadratureSetCellType(*q, ct));
  PetscCall(PetscQuadratureSetOrder(*q, 2 * npoints - 1));
  PetscCall(PetscQuadratureSetData(*q, dim, Nc, totpoints, x, w));
  PetscCall(PetscObjectChangeTypeName((PetscObject)*q, "GaussTensor"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTStroudConicalQuadrature - create Stroud conical quadrature for a simplex

  Not Collective

  Input Parameters:
+ dim     - The simplex dimension
. Nc      - The number of components
. npoints - The number of points in one dimension
. a       - left end of interval (often-1)
- b       - right end of interval (often +1)

  Output Parameter:
. q - A `PetscQuadrature` object

  Level: intermediate

  Note:
  For `dim` == 1, this is Gauss-Legendre quadrature

  References:
. * - Karniadakis and Sherwin.  FIAT

.seealso: `PetscDTGaussTensorQuadrature()`, `PetscDTGaussQuadrature()`
@*/
PetscErrorCode PetscDTStroudConicalQuadrature(PetscInt dim, PetscInt Nc, PetscInt npoints, PetscReal a, PetscReal b, PetscQuadrature *q)
{
  DMPolytopeType ct;
  PetscInt       totpoints;
  PetscReal     *p1, *w1;
  PetscReal     *x, *w;

  PetscFunctionBegin;
  PetscCheck(!(a != -1.0) && !(b != 1.0), PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Must use default internal right now");
  switch (dim) {
  case 0:
    ct = DM_POLYTOPE_POINT;
    break;
  case 1:
    ct = DM_POLYTOPE_SEGMENT;
    break;
  case 2:
    ct = DM_POLYTOPE_TRIANGLE;
    break;
  case 3:
    ct = DM_POLYTOPE_TETRAHEDRON;
    break;
  default:
    ct = DM_POLYTOPE_UNKNOWN;
  }
  totpoints = 1;
  for (PetscInt i = 0; i < dim; ++i) totpoints *= npoints;
  PetscCall(PetscMalloc1(totpoints * dim, &x));
  PetscCall(PetscMalloc1(totpoints * Nc, &w));
  PetscCall(PetscMalloc2(npoints, &p1, npoints, &w1));
  for (PetscInt i = 0; i < totpoints * Nc; ++i) w[i] = 1.;
  for (PetscInt i = 0, totprev = 1, totrem = totpoints / npoints; i < dim; ++i) {
    PetscReal mul;

    mul = PetscPowReal(2., -i);
    PetscCall(PetscDTGaussJacobiQuadrature(npoints, -1., 1., i, 0.0, p1, w1));
    for (PetscInt pt = 0, l = 0; l < totprev; l++) {
      for (PetscInt j = 0; j < npoints; j++) {
        for (PetscInt m = 0; m < totrem; m++, pt++) {
          for (PetscInt k = 0; k < i; k++) x[pt * dim + k] = (x[pt * dim + k] + 1.) * (1. - p1[j]) * 0.5 - 1.;
          x[pt * dim + i] = p1[j];
          for (PetscInt c = 0; c < Nc; c++) w[pt * Nc + c] *= mul * w1[j];
        }
      }
    }
    totprev *= npoints;
    totrem /= npoints;
  }
  PetscCall(PetscFree2(p1, w1));
  PetscCall(PetscQuadratureCreate(PETSC_COMM_SELF, q));
  PetscCall(PetscQuadratureSetCellType(*q, ct));
  PetscCall(PetscQuadratureSetOrder(*q, 2 * npoints - 1));
  PetscCall(PetscQuadratureSetData(*q, dim, Nc, totpoints, x, w));
  PetscCall(PetscObjectChangeTypeName((PetscObject)*q, "StroudConical"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscBool MinSymTriQuadCite       = PETSC_FALSE;
const char       MinSymTriQuadCitation[] = "@article{WitherdenVincent2015,\n"
                                           "  title = {On the identification of symmetric quadrature rules for finite element methods},\n"
                                           "  journal = {Computers & Mathematics with Applications},\n"
                                           "  volume = {69},\n"
                                           "  number = {10},\n"
                                           "  pages = {1232-1241},\n"
                                           "  year = {2015},\n"
                                           "  issn = {0898-1221},\n"
                                           "  doi = {10.1016/j.camwa.2015.03.017},\n"
                                           "  url = {https://www.sciencedirect.com/science/article/pii/S0898122115001224},\n"
                                           "  author = {F.D. Witherden and P.E. Vincent},\n"
                                           "}\n";

#include "petscdttriquadrules.h"

static PetscBool MinSymTetQuadCite       = PETSC_FALSE;
const char       MinSymTetQuadCitation[] = "@article{JaskowiecSukumar2021\n"
                                           "  author = {Jaskowiec, Jan and Sukumar, N.},\n"
                                           "  title = {High-order symmetric cubature rules for tetrahedra and pyramids},\n"
                                           "  journal = {International Journal for Numerical Methods in Engineering},\n"
                                           "  volume = {122},\n"
                                           "  number = {1},\n"
                                           "  pages = {148-171},\n"
                                           "  doi = {10.1002/nme.6528},\n"
                                           "  url = {https://onlinelibrary.wiley.com/doi/abs/10.1002/nme.6528},\n"
                                           "  eprint = {https://onlinelibrary.wiley.com/doi/pdf/10.1002/nme.6528},\n"
                                           "  year = {2021}\n"
                                           "}\n";

#include "petscdttetquadrules.h"

// https://en.wikipedia.org/wiki/Partition_(number_theory)
static PetscErrorCode PetscDTPartitionNumber(PetscInt n, PetscInt *p)
{
  // sequence A000041 in the OEIS
  const PetscInt partition[]   = {1, 1, 2, 3, 5, 7, 11, 15, 22, 30, 42, 56, 77, 101, 135, 176, 231, 297, 385, 490, 627, 792, 1002, 1255, 1575, 1958, 2436, 3010, 3718, 4565, 5604};
  PetscInt       tabulated_max = PETSC_STATIC_ARRAY_LENGTH(partition) - 1;

  PetscFunctionBegin;
  PetscCheck(n >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Partition number not defined for negative number %" PetscInt_FMT, n);
  // not implementing the pentagonal number recurrence, we don't need partition numbers for n that high
  PetscCheck(n <= tabulated_max, PETSC_COMM_SELF, PETSC_ERR_SUP, "Partition numbers only tabulated up to %" PetscInt_FMT ", not computed for %" PetscInt_FMT, tabulated_max, n);
  *p = partition[n];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTSimplexQuadrature - Create a quadrature rule for a simplex that exactly integrates polynomials up to a given degree.

  Not Collective

  Input Parameters:
+ dim     - The spatial dimension of the simplex (1 = segment, 2 = triangle, 3 = tetrahedron)
. degree  - The largest polynomial degree that is required to be integrated exactly
- type    - left end of interval (often-1)

  Output Parameter:
. quad    - A `PetscQuadrature` object for integration over the biunit simplex
            (defined by the bounds $x_i >= -1$ and $\sum_i x_i <= 2 - d$) that is exact for
            polynomials up to the given degree

  Level: intermediate

.seealso: `PetscDTSimplexQuadratureType`, `PetscDTGaussQuadrature()`, `PetscDTStroudCononicalQuadrature()`, `PetscQuadrature`
@*/
PetscErrorCode PetscDTSimplexQuadrature(PetscInt dim, PetscInt degree, PetscDTSimplexQuadratureType type, PetscQuadrature *quad)
{
  PetscDTSimplexQuadratureType orig_type = type;

  PetscFunctionBegin;
  PetscCheck(dim >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Negative dimension %" PetscInt_FMT, dim);
  PetscCheck(degree >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Negative degree %" PetscInt_FMT, degree);
  if (type == PETSCDTSIMPLEXQUAD_DEFAULT) type = PETSCDTSIMPLEXQUAD_MINSYM;
  if (type == PETSCDTSIMPLEXQUAD_CONIC || dim < 2) {
    PetscInt points_per_dim = (degree + 2) / 2; // ceil((degree + 1) / 2);
    PetscCall(PetscDTStroudConicalQuadrature(dim, 1, points_per_dim, -1, 1, quad));
  } else {
    DMPolytopeType    ct;
    PetscInt          n    = dim + 1;
    PetscInt          fact = 1;
    PetscInt         *part, *perm;
    PetscInt          p = 0;
    PetscInt          max_degree;
    const PetscInt   *nodes_per_type     = NULL;
    const PetscInt   *all_num_full_nodes = NULL;
    const PetscReal **weights_list       = NULL;
    const PetscReal **compact_nodes_list = NULL;
    const char       *citation           = NULL;
    PetscBool        *cited              = NULL;

    switch (dim) {
    case 0:
      ct = DM_POLYTOPE_POINT;
      break;
    case 1:
      ct = DM_POLYTOPE_SEGMENT;
      break;
    case 2:
      ct = DM_POLYTOPE_TRIANGLE;
      break;
    case 3:
      ct = DM_POLYTOPE_TETRAHEDRON;
      break;
    default:
      ct = DM_POLYTOPE_UNKNOWN;
    }
    switch (dim) {
    case 2:
      cited              = &MinSymTriQuadCite;
      citation           = MinSymTriQuadCitation;
      max_degree         = PetscDTWVTriQuad_max_degree;
      nodes_per_type     = PetscDTWVTriQuad_num_orbits;
      all_num_full_nodes = PetscDTWVTriQuad_num_nodes;
      weights_list       = PetscDTWVTriQuad_weights;
      compact_nodes_list = PetscDTWVTriQuad_orbits;
      break;
    case 3:
      cited              = &MinSymTetQuadCite;
      citation           = MinSymTetQuadCitation;
      max_degree         = PetscDTJSTetQuad_max_degree;
      nodes_per_type     = PetscDTJSTetQuad_num_orbits;
      all_num_full_nodes = PetscDTJSTetQuad_num_nodes;
      weights_list       = PetscDTJSTetQuad_weights;
      compact_nodes_list = PetscDTJSTetQuad_orbits;
      break;
    default:
      max_degree = -1;
      break;
    }

    if (degree > max_degree) {
      if (orig_type == PETSCDTSIMPLEXQUAD_DEFAULT) {
        // fall back to conic
        PetscCall(PetscDTSimplexQuadrature(dim, degree, PETSCDTSIMPLEXQUAD_CONIC, quad));
        PetscFunctionReturn(PETSC_SUCCESS);
      } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Minimal symmetric quadrature for dim %" PetscInt_FMT ", degree %" PetscInt_FMT " unsupported", dim, degree);
    }

    PetscCall(PetscCitationsRegister(citation, cited));

    PetscCall(PetscDTPartitionNumber(n, &p));
    for (PetscInt d = 2; d <= n; d++) fact *= d;

    PetscInt         num_full_nodes      = all_num_full_nodes[degree];
    const PetscReal *all_compact_nodes   = compact_nodes_list[degree];
    const PetscReal *all_compact_weights = weights_list[degree];
    nodes_per_type                       = &nodes_per_type[p * degree];

    PetscReal      *points;
    PetscReal      *counts;
    PetscReal      *weights;
    PetscReal      *bary_to_biunit; // row-major transformation of barycentric coordinate to biunit
    PetscQuadrature q;

    // compute the transformation
    PetscCall(PetscMalloc1(n * dim, &bary_to_biunit));
    for (PetscInt d = 0; d < dim; d++) {
      for (PetscInt b = 0; b < n; b++) bary_to_biunit[d * n + b] = (d == b) ? 1.0 : -1.0;
    }

    PetscCall(PetscMalloc3(n, &part, n, &perm, n, &counts));
    PetscCall(PetscCalloc1(num_full_nodes * dim, &points));
    PetscCall(PetscMalloc1(num_full_nodes, &weights));

    // (0, 0, ...) is the first partition lexicographically
    PetscCall(PetscArrayzero(part, n));
    PetscCall(PetscArrayzero(counts, n));
    counts[0] = n;

    // for each partition
    for (PetscInt s = 0, node_offset = 0; s < p; s++) {
      PetscInt num_compact_coords = part[n - 1] + 1;

      const PetscReal *compact_nodes   = all_compact_nodes;
      const PetscReal *compact_weights = all_compact_weights;
      all_compact_nodes += num_compact_coords * nodes_per_type[s];
      all_compact_weights += nodes_per_type[s];

      // for every permutation of the vertices
      for (PetscInt f = 0; f < fact; f++) {
        PetscCall(PetscDTEnumPerm(n, f, perm, NULL));

        // check if it is a valid permutation
        PetscInt digit;
        for (digit = 1; digit < n; digit++) {
          // skip permutations that would duplicate a node because it has a smaller symmetry group
          if (part[digit - 1] == part[digit] && perm[digit - 1] > perm[digit]) break;
        }
        if (digit < n) continue;

        // create full nodes from this permutation of the compact nodes
        PetscReal *full_nodes   = &points[node_offset * dim];
        PetscReal *full_weights = &weights[node_offset];

        PetscCall(PetscArraycpy(full_weights, compact_weights, nodes_per_type[s]));
        for (PetscInt b = 0; b < n; b++) {
          for (PetscInt d = 0; d < dim; d++) {
            for (PetscInt node = 0; node < nodes_per_type[s]; node++) full_nodes[node * dim + d] += bary_to_biunit[d * n + perm[b]] * compact_nodes[node * num_compact_coords + part[b]];
          }
        }
        node_offset += nodes_per_type[s];
      }

      if (s < p - 1) { // Generate the next partition
        /* A partition is described by the number of coordinates that are in
         * each set of duplicates (counts) and redundantly by mapping each
         * index to its set of duplicates (part)
         *
         * Counts should always be in nonincreasing order
         *
         * We want to generate the partitions lexically by part, which means
         * finding the last index where count > 1 and reducing by 1.
         *
         * For the new counts beyond that index, we eagerly assign the remaining
         * capacity of the partition to smaller indices (ensures lexical ordering),
         * while respecting the nonincreasing invariant of the counts
         */
        PetscInt last_digit            = part[n - 1];
        PetscInt last_digit_with_extra = last_digit;
        while (counts[last_digit_with_extra] == 1) last_digit_with_extra--;
        PetscInt limit               = --counts[last_digit_with_extra];
        PetscInt total_to_distribute = last_digit - last_digit_with_extra + 1;
        for (PetscInt digit = last_digit_with_extra + 1; digit < n; digit++) {
          counts[digit] = PetscMin(limit, total_to_distribute);
          total_to_distribute -= PetscMin(limit, total_to_distribute);
        }
        for (PetscInt digit = 0, offset = 0; digit < n; digit++) {
          PetscInt count = counts[digit];
          for (PetscInt c = 0; c < count; c++) part[offset++] = digit;
        }
      }
    }
    PetscCall(PetscFree3(part, perm, counts));
    PetscCall(PetscFree(bary_to_biunit));
    PetscCall(PetscQuadratureCreate(PETSC_COMM_SELF, &q));
    PetscCall(PetscQuadratureSetCellType(q, ct));
    PetscCall(PetscQuadratureSetOrder(q, degree));
    PetscCall(PetscQuadratureSetData(q, dim, 1, num_full_nodes, points, weights));
    *quad = q;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTTanhSinhTensorQuadrature - create tanh-sinh quadrature for a tensor product cell

  Not Collective

  Input Parameters:
+ dim   - The cell dimension
. level - The number of points in one dimension, 2^l
. a     - left end of interval (often-1)
- b     - right end of interval (often +1)

  Output Parameter:
. q - A `PetscQuadrature` object

  Level: intermediate

.seealso: `PetscDTGaussTensorQuadrature()`, `PetscQuadrature`
@*/
PetscErrorCode PetscDTTanhSinhTensorQuadrature(PetscInt dim, PetscInt level, PetscReal a, PetscReal b, PetscQuadrature *q)
{
  DMPolytopeType  ct;
  const PetscInt  p     = 16;                        /* Digits of precision in the evaluation */
  const PetscReal alpha = (b - a) / 2.;              /* Half-width of the integration interval */
  const PetscReal beta  = (b + a) / 2.;              /* Center of the integration interval */
  const PetscReal h     = PetscPowReal(2.0, -level); /* Step size, length between x_k */
  PetscReal       xk;                                /* Quadrature point x_k on reference domain [-1, 1] */
  PetscReal       wk = 0.5 * PETSC_PI;               /* Quadrature weight at x_k */
  PetscReal      *x, *w;
  PetscInt        K, k, npoints;

  PetscFunctionBegin;
  PetscCheck(dim <= 1, PETSC_COMM_SELF, PETSC_ERR_SUP, "Dimension %" PetscInt_FMT " not yet implemented", dim);
  PetscCheck(level, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Must give a number of significant digits");
  switch (dim) {
  case 0:
    ct = DM_POLYTOPE_POINT;
    break;
  case 1:
    ct = DM_POLYTOPE_SEGMENT;
    break;
  case 2:
    ct = DM_POLYTOPE_QUADRILATERAL;
    break;
  case 3:
    ct = DM_POLYTOPE_HEXAHEDRON;
    break;
  default:
    ct = DM_POLYTOPE_UNKNOWN;
  }
  /* Find K such that the weights are < 32 digits of precision */
  for (K = 1; PetscAbsReal(PetscLog10Real(wk)) < 2 * p; ++K) wk = 0.5 * h * PETSC_PI * PetscCoshReal(K * h) / PetscSqr(PetscCoshReal(0.5 * PETSC_PI * PetscSinhReal(K * h)));
  PetscCall(PetscQuadratureCreate(PETSC_COMM_SELF, q));
  PetscCall(PetscQuadratureSetCellType(*q, ct));
  PetscCall(PetscQuadratureSetOrder(*q, 2 * K + 1));
  npoints = 2 * K - 1;
  PetscCall(PetscMalloc1(npoints * dim, &x));
  PetscCall(PetscMalloc1(npoints, &w));
  /* Center term */
  x[0] = beta;
  w[0] = 0.5 * alpha * PETSC_PI;
  for (k = 1; k < K; ++k) {
    wk           = 0.5 * alpha * h * PETSC_PI * PetscCoshReal(k * h) / PetscSqr(PetscCoshReal(0.5 * PETSC_PI * PetscSinhReal(k * h)));
    xk           = PetscTanhReal(0.5 * PETSC_PI * PetscSinhReal(k * h));
    x[2 * k - 1] = -alpha * xk + beta;
    w[2 * k - 1] = wk;
    x[2 * k + 0] = alpha * xk + beta;
    w[2 * k + 0] = wk;
  }
  PetscCall(PetscQuadratureSetData(*q, dim, 1, npoints, x, w));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscDTTanhSinhIntegrate(void (*func)(const PetscReal[], void *, PetscReal *), PetscReal a, PetscReal b, PetscInt digits, void *ctx, PetscReal *sol)
{
  const PetscInt  p     = 16;           /* Digits of precision in the evaluation */
  const PetscReal alpha = (b - a) / 2.; /* Half-width of the integration interval */
  const PetscReal beta  = (b + a) / 2.; /* Center of the integration interval */
  PetscReal       h     = 1.0;          /* Step size, length between x_k */
  PetscInt        l     = 0;            /* Level of refinement, h = 2^{-l} */
  PetscReal       osum  = 0.0;          /* Integral on last level */
  PetscReal       psum  = 0.0;          /* Integral on the level before the last level */
  PetscReal       sum;                  /* Integral on current level */
  PetscReal       yk;                   /* Quadrature point 1 - x_k on reference domain [-1, 1] */
  PetscReal       lx, rx;               /* Quadrature points to the left and right of 0 on the real domain [a, b] */
  PetscReal       wk;                   /* Quadrature weight at x_k */
  PetscReal       lval, rval;           /* Terms in the quadature sum to the left and right of 0 */
  PetscInt        d;                    /* Digits of precision in the integral */

  PetscFunctionBegin;
  PetscCheck(digits > 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Must give a positive number of significant digits");
  /* Center term */
  func(&beta, ctx, &lval);
  sum = 0.5 * alpha * PETSC_PI * lval;
  /* */
  do {
    PetscReal lterm, rterm, maxTerm = 0.0, d1, d2, d3, d4;
    PetscInt  k = 1;

    ++l;
    /* PetscPrintf(PETSC_COMM_SELF, "LEVEL %" PetscInt_FMT " sum: %15.15f\n", l, sum); */
    /* At each level of refinement, h --> h/2 and sum --> sum/2 */
    psum = osum;
    osum = sum;
    h *= 0.5;
    sum *= 0.5;
    do {
      wk = 0.5 * h * PETSC_PI * PetscCoshReal(k * h) / PetscSqr(PetscCoshReal(0.5 * PETSC_PI * PetscSinhReal(k * h)));
      yk = 1.0 / (PetscExpReal(0.5 * PETSC_PI * PetscSinhReal(k * h)) * PetscCoshReal(0.5 * PETSC_PI * PetscSinhReal(k * h)));
      lx = -alpha * (1.0 - yk) + beta;
      rx = alpha * (1.0 - yk) + beta;
      func(&lx, ctx, &lval);
      func(&rx, ctx, &rval);
      lterm   = alpha * wk * lval;
      maxTerm = PetscMax(PetscAbsReal(lterm), maxTerm);
      sum += lterm;
      rterm   = alpha * wk * rval;
      maxTerm = PetscMax(PetscAbsReal(rterm), maxTerm);
      sum += rterm;
      ++k;
      /* Only need to evaluate every other point on refined levels */
      if (l != 1) ++k;
    } while (PetscAbsReal(PetscLog10Real(wk)) < p); /* Only need to evaluate sum until weights are < 32 digits of precision */

    d1 = PetscLog10Real(PetscAbsReal(sum - osum));
    d2 = PetscLog10Real(PetscAbsReal(sum - psum));
    d3 = PetscLog10Real(maxTerm) - p;
    if (PetscMax(PetscAbsReal(lterm), PetscAbsReal(rterm)) == 0.0) d4 = 0.0;
    else d4 = PetscLog10Real(PetscMax(PetscAbsReal(lterm), PetscAbsReal(rterm)));
    d = PetscAbsInt(PetscMin(0, PetscMax(PetscMax(PetscMax(PetscSqr(d1) / d2, 2 * d1), d3), d4)));
  } while (d < digits && l < 12);
  *sol = sum;

  PetscFunctionReturn(PETSC_SUCCESS);
}

#if defined(PETSC_HAVE_MPFR)
PetscErrorCode PetscDTTanhSinhIntegrateMPFR(void (*func)(const PetscReal[], void *, PetscReal *), PetscReal a, PetscReal b, PetscInt digits, void *ctx, PetscReal *sol)
{
  const PetscInt safetyFactor = 2; /* Calculate abcissa until 2*p digits */
  PetscInt       l            = 0; /* Level of refinement, h = 2^{-l} */
  mpfr_t         alpha;            /* Half-width of the integration interval */
  mpfr_t         beta;             /* Center of the integration interval */
  mpfr_t         h;                /* Step size, length between x_k */
  mpfr_t         osum;             /* Integral on last level */
  mpfr_t         psum;             /* Integral on the level before the last level */
  mpfr_t         sum;              /* Integral on current level */
  mpfr_t         yk;               /* Quadrature point 1 - x_k on reference domain [-1, 1] */
  mpfr_t         lx, rx;           /* Quadrature points to the left and right of 0 on the real domain [a, b] */
  mpfr_t         wk;               /* Quadrature weight at x_k */
  PetscReal      lval, rval, rtmp; /* Terms in the quadature sum to the left and right of 0 */
  PetscInt       d;                /* Digits of precision in the integral */
  mpfr_t         pi2, kh, msinh, mcosh, maxTerm, curTerm, tmp;

  PetscFunctionBegin;
  PetscCheck(digits > 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Must give a positive number of significant digits");
  /* Create high precision storage */
  mpfr_inits2(PetscCeilReal(safetyFactor * digits * PetscLogReal(10.) / PetscLogReal(2.)), alpha, beta, h, sum, osum, psum, yk, wk, lx, rx, tmp, maxTerm, curTerm, pi2, kh, msinh, mcosh, NULL);
  /* Initialization */
  mpfr_set_d(alpha, 0.5 * (b - a), MPFR_RNDN);
  mpfr_set_d(beta, 0.5 * (b + a), MPFR_RNDN);
  mpfr_set_d(osum, 0.0, MPFR_RNDN);
  mpfr_set_d(psum, 0.0, MPFR_RNDN);
  mpfr_set_d(h, 1.0, MPFR_RNDN);
  mpfr_const_pi(pi2, MPFR_RNDN);
  mpfr_mul_d(pi2, pi2, 0.5, MPFR_RNDN);
  /* Center term */
  rtmp = 0.5 * (b + a);
  func(&rtmp, ctx, &lval);
  mpfr_set(sum, pi2, MPFR_RNDN);
  mpfr_mul(sum, sum, alpha, MPFR_RNDN);
  mpfr_mul_d(sum, sum, lval, MPFR_RNDN);
  /* */
  do {
    PetscReal d1, d2, d3, d4;
    PetscInt  k = 1;

    ++l;
    mpfr_set_d(maxTerm, 0.0, MPFR_RNDN);
    /* PetscPrintf(PETSC_COMM_SELF, "LEVEL %" PetscInt_FMT " sum: %15.15f\n", l, sum); */
    /* At each level of refinement, h --> h/2 and sum --> sum/2 */
    mpfr_set(psum, osum, MPFR_RNDN);
    mpfr_set(osum, sum, MPFR_RNDN);
    mpfr_mul_d(h, h, 0.5, MPFR_RNDN);
    mpfr_mul_d(sum, sum, 0.5, MPFR_RNDN);
    do {
      mpfr_set_si(kh, k, MPFR_RNDN);
      mpfr_mul(kh, kh, h, MPFR_RNDN);
      /* Weight */
      mpfr_set(wk, h, MPFR_RNDN);
      mpfr_sinh_cosh(msinh, mcosh, kh, MPFR_RNDN);
      mpfr_mul(msinh, msinh, pi2, MPFR_RNDN);
      mpfr_mul(mcosh, mcosh, pi2, MPFR_RNDN);
      mpfr_cosh(tmp, msinh, MPFR_RNDN);
      mpfr_sqr(tmp, tmp, MPFR_RNDN);
      mpfr_mul(wk, wk, mcosh, MPFR_RNDN);
      mpfr_div(wk, wk, tmp, MPFR_RNDN);
      /* Abscissa */
      mpfr_set_d(yk, 1.0, MPFR_RNDZ);
      mpfr_cosh(tmp, msinh, MPFR_RNDN);
      mpfr_div(yk, yk, tmp, MPFR_RNDZ);
      mpfr_exp(tmp, msinh, MPFR_RNDN);
      mpfr_div(yk, yk, tmp, MPFR_RNDZ);
      /* Quadrature points */
      mpfr_sub_d(lx, yk, 1.0, MPFR_RNDZ);
      mpfr_mul(lx, lx, alpha, MPFR_RNDU);
      mpfr_add(lx, lx, beta, MPFR_RNDU);
      mpfr_d_sub(rx, 1.0, yk, MPFR_RNDZ);
      mpfr_mul(rx, rx, alpha, MPFR_RNDD);
      mpfr_add(rx, rx, beta, MPFR_RNDD);
      /* Evaluation */
      rtmp = mpfr_get_d(lx, MPFR_RNDU);
      func(&rtmp, ctx, &lval);
      rtmp = mpfr_get_d(rx, MPFR_RNDD);
      func(&rtmp, ctx, &rval);
      /* Update */
      mpfr_mul(tmp, wk, alpha, MPFR_RNDN);
      mpfr_mul_d(tmp, tmp, lval, MPFR_RNDN);
      mpfr_add(sum, sum, tmp, MPFR_RNDN);
      mpfr_abs(tmp, tmp, MPFR_RNDN);
      mpfr_max(maxTerm, maxTerm, tmp, MPFR_RNDN);
      mpfr_set(curTerm, tmp, MPFR_RNDN);
      mpfr_mul(tmp, wk, alpha, MPFR_RNDN);
      mpfr_mul_d(tmp, tmp, rval, MPFR_RNDN);
      mpfr_add(sum, sum, tmp, MPFR_RNDN);
      mpfr_abs(tmp, tmp, MPFR_RNDN);
      mpfr_max(maxTerm, maxTerm, tmp, MPFR_RNDN);
      mpfr_max(curTerm, curTerm, tmp, MPFR_RNDN);
      ++k;
      /* Only need to evaluate every other point on refined levels */
      if (l != 1) ++k;
      mpfr_log10(tmp, wk, MPFR_RNDN);
      mpfr_abs(tmp, tmp, MPFR_RNDN);
    } while (mpfr_get_d(tmp, MPFR_RNDN) < safetyFactor * digits); /* Only need to evaluate sum until weights are < 32 digits of precision */
    mpfr_sub(tmp, sum, osum, MPFR_RNDN);
    mpfr_abs(tmp, tmp, MPFR_RNDN);
    mpfr_log10(tmp, tmp, MPFR_RNDN);
    d1 = mpfr_get_d(tmp, MPFR_RNDN);
    mpfr_sub(tmp, sum, psum, MPFR_RNDN);
    mpfr_abs(tmp, tmp, MPFR_RNDN);
    mpfr_log10(tmp, tmp, MPFR_RNDN);
    d2 = mpfr_get_d(tmp, MPFR_RNDN);
    mpfr_log10(tmp, maxTerm, MPFR_RNDN);
    d3 = mpfr_get_d(tmp, MPFR_RNDN) - digits;
    mpfr_log10(tmp, curTerm, MPFR_RNDN);
    d4 = mpfr_get_d(tmp, MPFR_RNDN);
    d  = PetscAbsInt(PetscMin(0, PetscMax(PetscMax(PetscMax(PetscSqr(d1) / d2, 2 * d1), d3), d4)));
  } while (d < digits && l < 8);
  *sol = mpfr_get_d(sum, MPFR_RNDN);
  /* Cleanup */
  mpfr_clears(alpha, beta, h, sum, osum, psum, yk, wk, lx, rx, tmp, maxTerm, curTerm, pi2, kh, msinh, mcosh, NULL);
  PetscFunctionReturn(PETSC_SUCCESS);
}
#else

PetscErrorCode PetscDTTanhSinhIntegrateMPFR(void (*func)(const PetscReal[], void *, PetscReal *), PetscReal a, PetscReal b, PetscInt digits, void *ctx, PetscReal *sol)
{
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "This method will not work without MPFR. Reconfigure using --download-mpfr --download-gmp");
}
#endif

/*@
  PetscDTTensorQuadratureCreate - create the tensor product quadrature from two lower-dimensional quadratures

  Not Collective

  Input Parameters:
+ q1 - The first quadrature
- q2 - The second quadrature

  Output Parameter:
. q - A `PetscQuadrature` object

  Level: intermediate

.seealso: `PetscQuadrature`, `PetscDTGaussTensorQuadrature()`
@*/
PetscErrorCode PetscDTTensorQuadratureCreate(PetscQuadrature q1, PetscQuadrature q2, PetscQuadrature *q)
{
  DMPolytopeType   ct1, ct2, ct;
  const PetscReal *x1, *w1, *x2, *w2;
  PetscReal       *x, *w;
  PetscInt         dim1, Nc1, Np1, order1, qa, d1;
  PetscInt         dim2, Nc2, Np2, order2, qb, d2;
  PetscInt         dim, Nc, Np, order, qc, d;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(q1, PETSCQUADRATURE_CLASSID, 1);
  PetscValidHeaderSpecific(q2, PETSCQUADRATURE_CLASSID, 2);
  PetscValidPointer(q, 3);
  PetscCall(PetscQuadratureGetOrder(q1, &order1));
  PetscCall(PetscQuadratureGetOrder(q2, &order2));
  PetscCheck(order1 == order2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Order1 %" PetscInt_FMT " != %" PetscInt_FMT " Order2", order1, order2);
  PetscCall(PetscQuadratureGetData(q1, &dim1, &Nc1, &Np1, &x1, &w1));
  PetscCall(PetscQuadratureGetCellType(q1, &ct1));
  PetscCall(PetscQuadratureGetData(q2, &dim2, &Nc2, &Np2, &x2, &w2));
  PetscCall(PetscQuadratureGetCellType(q2, &ct2));
  PetscCheck(Nc1 == Nc2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "NumComp1 %" PetscInt_FMT " != %" PetscInt_FMT " NumComp2", Nc1, Nc2);

  switch (ct1) {
  case DM_POLYTOPE_POINT:
    ct = ct2;
    break;
  case DM_POLYTOPE_SEGMENT:
    switch (ct2) {
    case DM_POLYTOPE_POINT:
      ct = ct1;
      break;
    case DM_POLYTOPE_SEGMENT:
      ct = DM_POLYTOPE_QUADRILATERAL;
      break;
    case DM_POLYTOPE_TRIANGLE:
      ct = DM_POLYTOPE_TRI_PRISM;
      break;
    case DM_POLYTOPE_QUADRILATERAL:
      ct = DM_POLYTOPE_HEXAHEDRON;
      break;
    case DM_POLYTOPE_TETRAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_HEXAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    default:
      ct = DM_POLYTOPE_UNKNOWN;
    }
    break;
  case DM_POLYTOPE_TRIANGLE:
    switch (ct2) {
    case DM_POLYTOPE_POINT:
      ct = ct1;
      break;
    case DM_POLYTOPE_SEGMENT:
      ct = DM_POLYTOPE_TRI_PRISM;
      break;
    case DM_POLYTOPE_TRIANGLE:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_QUADRILATERAL:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_TETRAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_HEXAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    default:
      ct = DM_POLYTOPE_UNKNOWN;
    }
    break;
  case DM_POLYTOPE_QUADRILATERAL:
    switch (ct2) {
    case DM_POLYTOPE_POINT:
      ct = ct1;
      break;
    case DM_POLYTOPE_SEGMENT:
      ct = DM_POLYTOPE_HEXAHEDRON;
      break;
    case DM_POLYTOPE_TRIANGLE:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_QUADRILATERAL:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_TETRAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_HEXAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    default:
      ct = DM_POLYTOPE_UNKNOWN;
    }
    break;
  case DM_POLYTOPE_TETRAHEDRON:
    switch (ct2) {
    case DM_POLYTOPE_POINT:
      ct = ct1;
      break;
    case DM_POLYTOPE_SEGMENT:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_TRIANGLE:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_QUADRILATERAL:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_TETRAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_HEXAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    default:
      ct = DM_POLYTOPE_UNKNOWN;
    }
    break;
  case DM_POLYTOPE_HEXAHEDRON:
    switch (ct2) {
    case DM_POLYTOPE_POINT:
      ct = ct1;
      break;
    case DM_POLYTOPE_SEGMENT:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_TRIANGLE:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_QUADRILATERAL:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_TETRAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    case DM_POLYTOPE_HEXAHEDRON:
      ct = DM_POLYTOPE_UNKNOWN;
      break;
    default:
      ct = DM_POLYTOPE_UNKNOWN;
    }
    break;
  default:
    ct = DM_POLYTOPE_UNKNOWN;
  }
  dim   = dim1 + dim2;
  Nc    = Nc1;
  Np    = Np1 * Np2;
  order = order1;
  PetscCall(PetscQuadratureCreate(PETSC_COMM_SELF, q));
  PetscCall(PetscQuadratureSetCellType(*q, ct));
  PetscCall(PetscQuadratureSetOrder(*q, order));
  PetscCall(PetscMalloc1(Np * dim, &x));
  PetscCall(PetscMalloc1(Np, &w));
  for (qa = 0, qc = 0; qa < Np1; ++qa) {
    for (qb = 0; qb < Np2; ++qb, ++qc) {
      for (d1 = 0, d = 0; d1 < dim1; ++d1, ++d) x[qc * dim + d] = x1[qa * dim1 + d1];
      for (d2 = 0; d2 < dim2; ++d2, ++d) x[qc * dim + d] = x2[qb * dim2 + d2];
      w[qc] = w1[qa] * w2[qb];
    }
  }
  PetscCall(PetscQuadratureSetData(*q, dim, Nc, Np, x, w));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Overwrites A. Can only handle full-rank problems with m>=n
   A in column-major format
   Ainv in row-major format
   tau has length m
   worksize must be >= max(1,n)
 */
static PetscErrorCode PetscDTPseudoInverseQR(PetscInt m, PetscInt mstride, PetscInt n, PetscReal *A_in, PetscReal *Ainv_out, PetscScalar *tau, PetscInt worksize, PetscScalar *work)
{
  PetscBLASInt M, N, K, lda, ldb, ldwork, info;
  PetscScalar *A, *Ainv, *R, *Q, Alpha;

  PetscFunctionBegin;
#if defined(PETSC_USE_COMPLEX)
  {
    PetscInt i, j;
    PetscCall(PetscMalloc2(m * n, &A, m * n, &Ainv));
    for (j = 0; j < n; j++) {
      for (i = 0; i < m; i++) A[i + m * j] = A_in[i + mstride * j];
    }
    mstride = m;
  }
#else
  A    = A_in;
  Ainv = Ainv_out;
#endif

  PetscCall(PetscBLASIntCast(m, &M));
  PetscCall(PetscBLASIntCast(n, &N));
  PetscCall(PetscBLASIntCast(mstride, &lda));
  PetscCall(PetscBLASIntCast(worksize, &ldwork));
  PetscCall(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
  PetscCallBLAS("LAPACKgeqrf", LAPACKgeqrf_(&M, &N, A, &lda, tau, work, &ldwork, &info));
  PetscCall(PetscFPTrapPop());
  PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_LIB, "xGEQRF error");
  R = A; /* Upper triangular part of A now contains R, the rest contains the elementary reflectors */

  /* Extract an explicit representation of Q */
  Q = Ainv;
  PetscCall(PetscArraycpy(Q, A, mstride * n));
  K = N; /* full rank */
  PetscCallBLAS("LAPACKorgqr", LAPACKorgqr_(&M, &N, &K, Q, &lda, tau, work, &ldwork, &info));
  PetscCheck(!info, PETSC_COMM_SELF, PETSC_ERR_LIB, "xORGQR/xUNGQR error");

  /* Compute A^{-T} = (R^{-1} Q^T)^T = Q R^{-T} */
  Alpha = 1.0;
  ldb   = lda;
  PetscCallBLAS("BLAStrsm", BLAStrsm_("Right", "Upper", "ConjugateTranspose", "NotUnitTriangular", &M, &N, &Alpha, R, &lda, Q, &ldb));
  /* Ainv is Q, overwritten with inverse */

#if defined(PETSC_USE_COMPLEX)
  {
    PetscInt i;
    for (i = 0; i < m * n; i++) Ainv_out[i] = PetscRealPart(Ainv[i]);
    PetscCall(PetscFree2(A, Ainv));
  }
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Computes integral of L_p' over intervals {(x0,x1),(x1,x2),...} */
static PetscErrorCode PetscDTLegendreIntegrate(PetscInt ninterval, const PetscReal *x, PetscInt ndegree, const PetscInt *degrees, PetscBool Transpose, PetscReal *B)
{
  PetscReal *Bv;
  PetscInt   i, j;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1((ninterval + 1) * ndegree, &Bv));
  /* Point evaluation of L_p on all the source vertices */
  PetscCall(PetscDTLegendreEval(ninterval + 1, x, ndegree, degrees, Bv, NULL, NULL));
  /* Integral over each interval: \int_a^b L_p' = L_p(b)-L_p(a) */
  for (i = 0; i < ninterval; i++) {
    for (j = 0; j < ndegree; j++) {
      if (Transpose) B[i + ninterval * j] = Bv[(i + 1) * ndegree + j] - Bv[i * ndegree + j];
      else B[i * ndegree + j] = Bv[(i + 1) * ndegree + j] - Bv[i * ndegree + j];
    }
  }
  PetscCall(PetscFree(Bv));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDTReconstructPoly - create matrix representing polynomial reconstruction using cell intervals and evaluation at target intervals

   Not Collective

   Input Parameters:
+  degree - degree of reconstruction polynomial
.  nsource - number of source intervals
.  sourcex - sorted coordinates of source cell boundaries (length nsource+1)
.  ntarget - number of target intervals
-  targetx - sorted coordinates of target cell boundaries (length ntarget+1)

   Output Parameter:
.  R - reconstruction matrix, utarget = sum_s R[t*nsource+s] * usource[s]

   Level: advanced

.seealso: `PetscDTLegendreEval()`
@*/
PetscErrorCode PetscDTReconstructPoly(PetscInt degree, PetscInt nsource, const PetscReal *sourcex, PetscInt ntarget, const PetscReal *targetx, PetscReal *R)
{
  PetscInt     i, j, k, *bdegrees, worksize;
  PetscReal    xmin, xmax, center, hscale, *sourcey, *targety, *Bsource, *Bsinv, *Btarget;
  PetscScalar *tau, *work;

  PetscFunctionBegin;
  PetscValidRealPointer(sourcex, 3);
  PetscValidRealPointer(targetx, 5);
  PetscValidRealPointer(R, 6);
  PetscCheck(degree < nsource, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Reconstruction degree %" PetscInt_FMT " must be less than number of source intervals %" PetscInt_FMT, degree, nsource);
  if (PetscDefined(USE_DEBUG)) {
    for (i = 0; i < nsource; i++) PetscCheck(sourcex[i] < sourcex[i + 1], PETSC_COMM_SELF, PETSC_ERR_ARG_CORRUPT, "Source interval %" PetscInt_FMT " has negative orientation (%g,%g)", i, (double)sourcex[i], (double)sourcex[i + 1]);
    for (i = 0; i < ntarget; i++) PetscCheck(targetx[i] < targetx[i + 1], PETSC_COMM_SELF, PETSC_ERR_ARG_CORRUPT, "Target interval %" PetscInt_FMT " has negative orientation (%g,%g)", i, (double)targetx[i], (double)targetx[i + 1]);
  }
  xmin     = PetscMin(sourcex[0], targetx[0]);
  xmax     = PetscMax(sourcex[nsource], targetx[ntarget]);
  center   = (xmin + xmax) / 2;
  hscale   = (xmax - xmin) / 2;
  worksize = nsource;
  PetscCall(PetscMalloc4(degree + 1, &bdegrees, nsource + 1, &sourcey, nsource * (degree + 1), &Bsource, worksize, &work));
  PetscCall(PetscMalloc4(nsource, &tau, nsource * (degree + 1), &Bsinv, ntarget + 1, &targety, ntarget * (degree + 1), &Btarget));
  for (i = 0; i <= nsource; i++) sourcey[i] = (sourcex[i] - center) / hscale;
  for (i = 0; i <= degree; i++) bdegrees[i] = i + 1;
  PetscCall(PetscDTLegendreIntegrate(nsource, sourcey, degree + 1, bdegrees, PETSC_TRUE, Bsource));
  PetscCall(PetscDTPseudoInverseQR(nsource, nsource, degree + 1, Bsource, Bsinv, tau, nsource, work));
  for (i = 0; i <= ntarget; i++) targety[i] = (targetx[i] - center) / hscale;
  PetscCall(PetscDTLegendreIntegrate(ntarget, targety, degree + 1, bdegrees, PETSC_FALSE, Btarget));
  for (i = 0; i < ntarget; i++) {
    PetscReal rowsum = 0;
    for (j = 0; j < nsource; j++) {
      PetscReal sum = 0;
      for (k = 0; k < degree + 1; k++) sum += Btarget[i * (degree + 1) + k] * Bsinv[k * nsource + j];
      R[i * nsource + j] = sum;
      rowsum += sum;
    }
    for (j = 0; j < nsource; j++) R[i * nsource + j] /= rowsum; /* normalize each row */
  }
  PetscCall(PetscFree4(bdegrees, sourcey, Bsource, work));
  PetscCall(PetscFree4(tau, Bsinv, targety, Btarget));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscGaussLobattoLegendreIntegrate - Compute the L2 integral of a function on the GLL points

   Not Collective

   Input Parameters:
+  n - the number of GLL nodes
.  nodes - the GLL nodes
.  weights - the GLL weights
-  f - the function values at the nodes

   Output Parameter:
.  in - the value of the integral

   Level: beginner

.seealso: `PetscDTGaussLobattoLegendreQuadrature()`
@*/
PetscErrorCode PetscGaussLobattoLegendreIntegrate(PetscInt n, PetscReal *nodes, PetscReal *weights, const PetscReal *f, PetscReal *in)
{
  PetscInt i;

  PetscFunctionBegin;
  *in = 0.;
  for (i = 0; i < n; i++) *in += f[i] * f[i] * weights[i];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscGaussLobattoLegendreElementLaplacianCreate - computes the Laplacian for a single 1d GLL element

   Not Collective

   Input Parameters:
+  n - the number of GLL nodes
.  nodes - the GLL nodes
-  weights - the GLL weights

   Output Parameter:
.  A - the stiffness element

   Level: beginner

   Notes:
   Destroy this with `PetscGaussLobattoLegendreElementLaplacianDestroy()`

   You can access entries in this array with AA[i][j] but in memory it is stored in contiguous memory, row oriented (the array is symmetric)

.seealso: `PetscDTGaussLobattoLegendreQuadrature()`, `PetscGaussLobattoLegendreElementLaplacianDestroy()`
@*/
PetscErrorCode PetscGaussLobattoLegendreElementLaplacianCreate(PetscInt n, PetscReal *nodes, PetscReal *weights, PetscReal ***AA)
{
  PetscReal      **A;
  const PetscReal *gllnodes = nodes;
  const PetscInt   p        = n - 1;
  PetscReal        z0, z1, z2 = -1, x, Lpj, Lpr;
  PetscInt         i, j, nn, r;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(n, &A));
  PetscCall(PetscMalloc1(n * n, &A[0]));
  for (i = 1; i < n; i++) A[i] = A[i - 1] + n;

  for (j = 1; j < p; j++) {
    x  = gllnodes[j];
    z0 = 1.;
    z1 = x;
    for (nn = 1; nn < p; nn++) {
      z2 = x * z1 * (2. * ((PetscReal)nn) + 1.) / (((PetscReal)nn) + 1.) - z0 * (((PetscReal)nn) / (((PetscReal)nn) + 1.));
      z0 = z1;
      z1 = z2;
    }
    Lpj = z2;
    for (r = 1; r < p; r++) {
      if (r == j) {
        A[j][j] = 2. / (3. * (1. - gllnodes[j] * gllnodes[j]) * Lpj * Lpj);
      } else {
        x  = gllnodes[r];
        z0 = 1.;
        z1 = x;
        for (nn = 1; nn < p; nn++) {
          z2 = x * z1 * (2. * ((PetscReal)nn) + 1.) / (((PetscReal)nn) + 1.) - z0 * (((PetscReal)nn) / (((PetscReal)nn) + 1.));
          z0 = z1;
          z1 = z2;
        }
        Lpr     = z2;
        A[r][j] = 4. / (((PetscReal)p) * (((PetscReal)p) + 1.) * Lpj * Lpr * (gllnodes[j] - gllnodes[r]) * (gllnodes[j] - gllnodes[r]));
      }
    }
  }
  for (j = 1; j < p + 1; j++) {
    x  = gllnodes[j];
    z0 = 1.;
    z1 = x;
    for (nn = 1; nn < p; nn++) {
      z2 = x * z1 * (2. * ((PetscReal)nn) + 1.) / (((PetscReal)nn) + 1.) - z0 * (((PetscReal)nn) / (((PetscReal)nn) + 1.));
      z0 = z1;
      z1 = z2;
    }
    Lpj     = z2;
    A[j][0] = 4. * PetscPowRealInt(-1., p) / (((PetscReal)p) * (((PetscReal)p) + 1.) * Lpj * (1. + gllnodes[j]) * (1. + gllnodes[j]));
    A[0][j] = A[j][0];
  }
  for (j = 0; j < p; j++) {
    x  = gllnodes[j];
    z0 = 1.;
    z1 = x;
    for (nn = 1; nn < p; nn++) {
      z2 = x * z1 * (2. * ((PetscReal)nn) + 1.) / (((PetscReal)nn) + 1.) - z0 * (((PetscReal)nn) / (((PetscReal)nn) + 1.));
      z0 = z1;
      z1 = z2;
    }
    Lpj = z2;

    A[p][j] = 4. / (((PetscReal)p) * (((PetscReal)p) + 1.) * Lpj * (1. - gllnodes[j]) * (1. - gllnodes[j]));
    A[j][p] = A[p][j];
  }
  A[0][0] = 0.5 + (((PetscReal)p) * (((PetscReal)p) + 1.) - 2.) / 6.;
  A[p][p] = A[0][0];
  *AA     = A;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscGaussLobattoLegendreElementLaplacianDestroy - frees the Laplacian for a single 1d GLL element created with `PetscGaussLobattoLegendreElementLaplacianCreate()`

   Not Collective

   Input Parameters:
+  n - the number of GLL nodes
.  nodes - the GLL nodes
.  weights - the GLL weightss
-  A - the stiffness element

   Level: beginner

.seealso: `PetscDTGaussLobattoLegendreQuadrature()`, `PetscGaussLobattoLegendreElementLaplacianCreate()`
@*/
PetscErrorCode PetscGaussLobattoLegendreElementLaplacianDestroy(PetscInt n, PetscReal *nodes, PetscReal *weights, PetscReal ***AA)
{
  PetscFunctionBegin;
  PetscCall(PetscFree((*AA)[0]));
  PetscCall(PetscFree(*AA));
  *AA = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscGaussLobattoLegendreElementGradientCreate - computes the gradient for a single 1d GLL element

   Not Collective

   Input Parameter:
+  n - the number of GLL nodes
.  nodes - the GLL nodes
.  weights - the GLL weights

   Output Parameters:
.  AA - the stiffness element
-  AAT - the transpose of AA (pass in `NULL` if you do not need this array)

   Level: beginner

   Notes:
   Destroy this with `PetscGaussLobattoLegendreElementGradientDestroy()`

   You can access entries in these arrays with AA[i][j] but in memory it is stored in contiguous memory, row oriented

.seealso: `PetscDTGaussLobattoLegendreQuadrature()`, `PetscGaussLobattoLegendreElementLaplacianDestroy()`, `PetscGaussLobattoLegendreElementGradientDestroy()`
@*/
PetscErrorCode PetscGaussLobattoLegendreElementGradientCreate(PetscInt n, PetscReal *nodes, PetscReal *weights, PetscReal ***AA, PetscReal ***AAT)
{
  PetscReal      **A, **AT = NULL;
  const PetscReal *gllnodes = nodes;
  const PetscInt   p        = n - 1;
  PetscReal        Li, Lj, d0;
  PetscInt         i, j;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(n, &A));
  PetscCall(PetscMalloc1(n * n, &A[0]));
  for (i = 1; i < n; i++) A[i] = A[i - 1] + n;

  if (AAT) {
    PetscCall(PetscMalloc1(n, &AT));
    PetscCall(PetscMalloc1(n * n, &AT[0]));
    for (i = 1; i < n; i++) AT[i] = AT[i - 1] + n;
  }

  if (n == 1) A[0][0] = 0.;
  d0 = (PetscReal)p * ((PetscReal)p + 1.) / 4.;
  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      A[i][j] = 0.;
      PetscCall(PetscDTComputeJacobi(0., 0., p, gllnodes[i], &Li));
      PetscCall(PetscDTComputeJacobi(0., 0., p, gllnodes[j], &Lj));
      if (i != j) A[i][j] = Li / (Lj * (gllnodes[i] - gllnodes[j]));
      if ((j == i) && (i == 0)) A[i][j] = -d0;
      if (j == i && i == p) A[i][j] = d0;
      if (AT) AT[j][i] = A[i][j];
    }
  }
  if (AAT) *AAT = AT;
  *AA = A;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscGaussLobattoLegendreElementGradientDestroy - frees the gradient for a single 1d GLL element obtained with `PetscGaussLobattoLegendreElementGradientCreate()`

   Not Collective

   Input Parameters:
+  n - the number of GLL nodes
.  nodes - the GLL nodes
.  weights - the GLL weights
.  AA - the stiffness element
-  AAT - the transpose of the element

   Level: beginner

.seealso: `PetscDTGaussLobattoLegendreQuadrature()`, `PetscGaussLobattoLegendreElementLaplacianCreate()`, `PetscGaussLobattoLegendreElementAdvectionCreate()`
@*/
PetscErrorCode PetscGaussLobattoLegendreElementGradientDestroy(PetscInt n, PetscReal *nodes, PetscReal *weights, PetscReal ***AA, PetscReal ***AAT)
{
  PetscFunctionBegin;
  PetscCall(PetscFree((*AA)[0]));
  PetscCall(PetscFree(*AA));
  *AA = NULL;
  if (*AAT) {
    PetscCall(PetscFree((*AAT)[0]));
    PetscCall(PetscFree(*AAT));
    *AAT = NULL;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscGaussLobattoLegendreElementAdvectionCreate - computes the advection operator for a single 1d GLL element

   Not Collective

   Input Parameters:
+  n - the number of GLL nodes
.  nodes - the GLL nodes
-  weights - the GLL weightss

   Output Parameter:
.  AA - the stiffness element

   Level: beginner

   Notes:
   Destroy this with `PetscGaussLobattoLegendreElementAdvectionDestroy()`

   This is the same as the Gradient operator multiplied by the diagonal mass matrix

   You can access entries in this array with AA[i][j] but in memory it is stored in contiguous memory, row oriented

.seealso: `PetscDTGaussLobattoLegendreQuadrature()`, `PetscGaussLobattoLegendreElementLaplacianCreate()`, `PetscGaussLobattoLegendreElementAdvectionDestroy()`
@*/
PetscErrorCode PetscGaussLobattoLegendreElementAdvectionCreate(PetscInt n, PetscReal *nodes, PetscReal *weights, PetscReal ***AA)
{
  PetscReal      **D;
  const PetscReal *gllweights = weights;
  const PetscInt   glln       = n;
  PetscInt         i, j;

  PetscFunctionBegin;
  PetscCall(PetscGaussLobattoLegendreElementGradientCreate(n, nodes, weights, &D, NULL));
  for (i = 0; i < glln; i++) {
    for (j = 0; j < glln; j++) D[i][j] = gllweights[i] * D[i][j];
  }
  *AA = D;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscGaussLobattoLegendreElementAdvectionDestroy - frees the advection stiffness for a single 1d GLL element created with `PetscGaussLobattoLegendreElementAdvectionCreate()`

   Not Collective

   Input Parameters:
+  n - the number of GLL nodes
.  nodes - the GLL nodes
.  weights - the GLL weights
-  A - advection

   Level: beginner

.seealso: `PetscDTGaussLobattoLegendreQuadrature()`, `PetscGaussLobattoLegendreElementAdvectionCreate()`
@*/
PetscErrorCode PetscGaussLobattoLegendreElementAdvectionDestroy(PetscInt n, PetscReal *nodes, PetscReal *weights, PetscReal ***AA)
{
  PetscFunctionBegin;
  PetscCall(PetscFree((*AA)[0]));
  PetscCall(PetscFree(*AA));
  *AA = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscGaussLobattoLegendreElementMassCreate(PetscInt n, PetscReal *nodes, PetscReal *weights, PetscReal ***AA)
{
  PetscReal      **A;
  const PetscReal *gllweights = weights;
  const PetscInt   glln       = n;
  PetscInt         i, j;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(glln, &A));
  PetscCall(PetscMalloc1(glln * glln, &A[0]));
  for (i = 1; i < glln; i++) A[i] = A[i - 1] + glln;
  if (glln == 1) A[0][0] = 0.;
  for (i = 0; i < glln; i++) {
    for (j = 0; j < glln; j++) {
      A[i][j] = 0.;
      if (j == i) A[i][j] = gllweights[i];
    }
  }
  *AA = A;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscGaussLobattoLegendreElementMassDestroy(PetscInt n, PetscReal *nodes, PetscReal *weights, PetscReal ***AA)
{
  PetscFunctionBegin;
  PetscCall(PetscFree((*AA)[0]));
  PetscCall(PetscFree(*AA));
  *AA = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTIndexToBary - convert an index into a barycentric coordinate.

  Input Parameters:
+ len - the desired length of the barycentric tuple (usually 1 more than the dimension it represents, so a barycentric coordinate in a triangle has length 3)
. sum - the value that the sum of the barycentric coordinates (which will be non-negative integers) should sum to
- index - the index to convert: should be >= 0 and < Binomial(len - 1 + sum, sum)

  Output Parameter:
. coord - will be filled with the barycentric coordinate

  Level: beginner

  Note:
  The indices map to barycentric coordinates in lexicographic order, where the first index is the
  least significant and the last index is the most significant.

.seealso: `PetscDTBaryToIndex()`
@*/
PetscErrorCode PetscDTIndexToBary(PetscInt len, PetscInt sum, PetscInt index, PetscInt coord[])
{
  PetscInt c, d, s, total, subtotal, nexttotal;

  PetscFunctionBeginHot;
  PetscCheck(len >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "length must be non-negative");
  PetscCheck(index >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "index must be non-negative");
  if (!len) {
    if (!sum && !index) PetscFunctionReturn(PETSC_SUCCESS);
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid index or sum for length 0 barycentric coordinate");
  }
  for (c = 1, total = 1; c <= len; c++) {
    /* total is the number of ways to have a tuple of length c with sum */
    if (index < total) break;
    total = (total * (sum + c)) / c;
  }
  PetscCheck(c <= len, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "index out of range");
  for (d = c; d < len; d++) coord[d] = 0;
  for (s = 0, subtotal = 1, nexttotal = 1; c > 0;) {
    /* subtotal is the number of ways to have a tuple of length c with sum s */
    /* nexttotal is the number of ways to have a tuple of length c-1 with sum s */
    if ((index + subtotal) >= total) {
      coord[--c] = sum - s;
      index -= (total - subtotal);
      sum       = s;
      total     = nexttotal;
      subtotal  = 1;
      nexttotal = 1;
      s         = 0;
    } else {
      subtotal  = (subtotal * (c + s)) / (s + 1);
      nexttotal = (nexttotal * (c - 1 + s)) / (s + 1);
      s++;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTBaryToIndex - convert a barycentric coordinate to an index

  Input Parameters:
+ len - the desired length of the barycentric tuple (usually 1 more than the dimension it represents, so a barycentric coordinate in a triangle has length 3)
. sum - the value that the sum of the barycentric coordinates (which will be non-negative integers) should sum to
- coord - a barycentric coordinate with the given length and sum

  Output Parameter:
. index - the unique index for the coordinate, >= 0 and < Binomial(len - 1 + sum, sum)

  Level: beginner

  Note:
  The indices map to barycentric coordinates in lexicographic order, where the first index is the
  least significant and the last index is the most significant.

.seealso: `PetscDTIndexToBary`
@*/
PetscErrorCode PetscDTBaryToIndex(PetscInt len, PetscInt sum, const PetscInt coord[], PetscInt *index)
{
  PetscInt c;
  PetscInt i;
  PetscInt total;

  PetscFunctionBeginHot;
  PetscCheck(len >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "length must be non-negative");
  if (!len) {
    if (!sum) {
      *index = 0;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid index or sum for length 0 barycentric coordinate");
  }
  for (c = 1, total = 1; c < len; c++) total = (total * (sum + c)) / c;
  i = total - 1;
  c = len - 1;
  sum -= coord[c];
  while (sum > 0) {
    PetscInt subtotal;
    PetscInt s;

    for (s = 1, subtotal = 1; s < sum; s++) subtotal = (subtotal * (c + s)) / s;
    i -= subtotal;
    sum -= coord[--c];
  }
  *index = i;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscQuadratureComputePermutations - Compute permutations of quadrature points corresponding to domain orientations

  Input Parameter:
. quad - The `PetscQuadrature`

  Output Parameters:
+ Np   - The number of domain orientations
- perm - An array of `IS` permutations, one for ech orientation,

  Level: developer

.seealso: `PetscQuadratureSetCellType()`, `PetscQuadrature`
@*/
PetscErrorCode PetscQuadratureComputePermutations(PetscQuadrature quad, PetscInt *Np, IS *perm[])
{
  DMPolytopeType   ct;
  const PetscReal *xq, *wq;
  PetscInt         dim, qdim, d, Na, o, Nq, q, qp;

  PetscFunctionBegin;
  PetscCall(PetscQuadratureGetData(quad, &qdim, NULL, &Nq, &xq, &wq));
  PetscCall(PetscQuadratureGetCellType(quad, &ct));
  dim = DMPolytopeTypeGetDim(ct);
  Na  = DMPolytopeTypeGetNumArrangments(ct);
  PetscCall(PetscMalloc1(Na, perm));
  if (Np) *Np = Na;
  Na /= 2;
  for (o = -Na; o < Na; ++o) {
    DM        refdm;
    PetscInt *idx;
    PetscReal xi0[3] = {-1., -1., -1.}, v0[3], J[9], detJ, txq[3];
    PetscBool flg;

    PetscCall(DMPlexCreateReferenceCell(PETSC_COMM_SELF, ct, &refdm));
    PetscCall(DMPlexOrientPoint(refdm, 0, o));
    PetscCall(DMPlexComputeCellGeometryFEM(refdm, 0, NULL, v0, J, NULL, &detJ));
    PetscCall(PetscMalloc1(Nq, &idx));
    for (q = 0; q < Nq; ++q) {
      CoordinatesRefToReal(dim, dim, xi0, v0, J, &xq[q * dim], txq);
      for (qp = 0; qp < Nq; ++qp) {
        PetscReal diff = 0.;

        for (d = 0; d < dim; ++d) diff += PetscAbsReal(txq[d] - xq[qp * dim + d]);
        if (diff < PETSC_SMALL) break;
      }
      PetscCheck(qp < Nq, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Transformed quad point %" PetscInt_FMT " does not match another quad point", q);
      idx[q] = qp;
    }
    PetscCall(DMDestroy(&refdm));
    PetscCall(ISCreateGeneral(PETSC_COMM_SELF, Nq, idx, PETSC_OWN_POINTER, &(*perm)[o + Na]));
    PetscCall(ISGetInfo((*perm)[o + Na], IS_PERMUTATION, IS_LOCAL, PETSC_TRUE, &flg));
    PetscCheck(flg, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Ordering for orientation %" PetscInt_FMT " was not a permutation", o);
    PetscCall(ISSetPermutation((*perm)[o + Na]));
  }
  if (!Na) (*perm)[0] = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDTCreateDefaultQuadrature - Create default quadrature for a given cell

  Not collective

  Input Parameters:
+ ct     - The integration domain
- qorder - The desired quadrature order

  Output Parameters:
+ q  - The cell quadrature
- fq - The face quadrature

  Level: developer

.seealso: `PetscFECreateDefault()`, `PetscDTGaussTensorQuadrature()`, `PetscDTSimplexQuadrature()`, `PetscDTTensorQuadratureCreate()`
@*/
PetscErrorCode PetscDTCreateDefaultQuadrature(DMPolytopeType ct, PetscInt qorder, PetscQuadrature *q, PetscQuadrature *fq)
{
  const PetscInt quadPointsPerEdge = PetscMax(qorder + 1, 1);
  const PetscInt dim               = DMPolytopeTypeGetDim(ct);

  PetscFunctionBegin;
  switch (ct) {
  case DM_POLYTOPE_SEGMENT:
  case DM_POLYTOPE_POINT_PRISM_TENSOR:
  case DM_POLYTOPE_QUADRILATERAL:
  case DM_POLYTOPE_SEG_PRISM_TENSOR:
  case DM_POLYTOPE_HEXAHEDRON:
  case DM_POLYTOPE_QUAD_PRISM_TENSOR:
    PetscCall(PetscDTGaussTensorQuadrature(dim, 1, quadPointsPerEdge, -1.0, 1.0, q));
    PetscCall(PetscDTGaussTensorQuadrature(dim - 1, 1, quadPointsPerEdge, -1.0, 1.0, fq));
    break;
  case DM_POLYTOPE_TRIANGLE:
  case DM_POLYTOPE_TETRAHEDRON:
    PetscCall(PetscDTSimplexQuadrature(dim, 2 * qorder, PETSCDTSIMPLEXQUAD_DEFAULT, q));
    PetscCall(PetscDTSimplexQuadrature(dim - 1, 2 * qorder, PETSCDTSIMPLEXQUAD_DEFAULT, fq));
    break;
  case DM_POLYTOPE_TRI_PRISM:
  case DM_POLYTOPE_TRI_PRISM_TENSOR: {
    PetscQuadrature q1, q2;

    // TODO: this should be able to use symmetric rules, but doing so causes tests to fail
    PetscCall(PetscDTSimplexQuadrature(2, 2 * qorder, PETSCDTSIMPLEXQUAD_CONIC, &q1));
    PetscCall(PetscDTGaussTensorQuadrature(1, 1, quadPointsPerEdge, -1.0, 1.0, &q2));
    PetscCall(PetscDTTensorQuadratureCreate(q1, q2, q));
    PetscCall(PetscQuadratureDestroy(&q2));
    *fq = q1;
    /* TODO Need separate quadratures for each face */
  } break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "No quadrature for celltype %s", DMPolytopeTypes[PetscMin(ct, DM_POLYTOPE_UNKNOWN)]);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

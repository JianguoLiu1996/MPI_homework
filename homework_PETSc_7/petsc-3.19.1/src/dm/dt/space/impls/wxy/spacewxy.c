#include <petsc/private/petscfeimpl.h> /*I "petscfe.h" I*/

static PetscErrorCode PetscSpaceSetFromOptions_WXY(PetscSpace sp, PetscOptionItems *PetscOptionsObject)
{
  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "PetscSpace WXY options");
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpacePolynomialView_Ascii(PetscSpace sp, PetscViewer v)
{
  PetscFunctionBegin;
  PetscCall(PetscViewerASCIIPrintf(v, "WXY space of degree %" PetscInt_FMT "\n", sp->degree));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceView_WXY(PetscSpace sp, PetscViewer viewer)
{
  PetscBool iascii;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) PetscCall(PetscSpacePolynomialView_Ascii(sp, viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceDestroy_WXY(PetscSpace sp)
{
  PetscSpace_WXY *wxy = (PetscSpace_WXY *)sp->data;

  PetscFunctionBegin;
  PetscCall(PetscFree(wxy));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceSetUp_WXY(PetscSpace sp)
{
  PetscSpace_WXY *wxy = (PetscSpace_WXY *)sp->data;

  PetscFunctionBegin;
  if (wxy->setupCalled) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCheck(sp->degree >= 0, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_OUTOFRANGE, "Negative degree %" PetscInt_FMT " invalid\n", sp->degree);
  sp->maxDegree    = sp->degree;
  wxy->setupCalled = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceGetDimension_WXY(PetscSpace sp, PetscInt *dim)
{
  PetscFunctionBegin;
  *dim = 6;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceEvaluate_WXY(PetscSpace sp, PetscInt npoints, const PetscReal points[], PetscReal B[], PetscReal D[], PetscReal H[])
{
  PetscSpace_WXY *wxy = (PetscSpace_WXY *)sp->data;
  PetscInt        dim = sp->Nv;
  PetscInt        Nb  = 6;
  PetscInt        Nc  = 3;

  PetscFunctionBegin;
  if (!wxy->setupCalled) {
    PetscCall(PetscSpaceSetUp(sp));
    PetscCall(PetscSpaceEvaluate(sp, npoints, points, B, D, H));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCheck((sp->Nc == 3) && (sp->Nv == 3), PETSC_COMM_SELF, PETSC_ERR_PLIB, "WXY space must have 3 variables and 3 components");
  if (B) {
    PetscInt p_inc = Nb * Nc;
    PetscInt b_inc = Nc;
    PetscInt c_inc = 1;

    for (PetscInt p = 0; p < npoints; p++) {
      const PetscReal x = points[p * dim + 0];
      const PetscReal y = points[p * dim + 1];
      const PetscReal z = points[p * dim + 2];

      /* {2 y z, 0, 0} */
      B[p * p_inc + 0 * b_inc + 0 * c_inc] = 2. * y * z;
      B[p * p_inc + 0 * b_inc + 1 * c_inc] = 0.;
      B[p * p_inc + 0 * b_inc + 2 * c_inc] = 0.;
      /* {0, 2 x z, 0} */
      B[p * p_inc + 1 * b_inc + 0 * c_inc] = 0.;
      B[p * p_inc + 1 * b_inc + 1 * c_inc] = 2. * x * z;
      B[p * p_inc + 1 * b_inc + 2 * c_inc] = 0.;
      /* {0, 2 y z, -z^2} */
      B[p * p_inc + 2 * b_inc + 0 * c_inc] = 0.;
      B[p * p_inc + 2 * b_inc + 1 * c_inc] = 2. * y * z;
      B[p * p_inc + 2 * b_inc + 2 * c_inc] = -z * z;
      /* {2 x z, 0, -z^2} */
      B[p * p_inc + 3 * b_inc + 0 * c_inc] = 2. * x * z;
      B[p * p_inc + 3 * b_inc + 1 * c_inc] = 0.;
      B[p * p_inc + 3 * b_inc + 2 * c_inc] = -z * z;
      /* {x^2, x y, -3 x z} */
      B[p * p_inc + 4 * b_inc + 0 * c_inc] = x * x;
      B[p * p_inc + 4 * b_inc + 1 * c_inc] = x * y;
      B[p * p_inc + 4 * b_inc + 2 * c_inc] = -3. * x * z;
      /* {x y, y^2, -3 y z} */
      B[p * p_inc + 5 * b_inc + 0 * c_inc] = x * y;
      B[p * p_inc + 5 * b_inc + 1 * c_inc] = y * y;
      B[p * p_inc + 5 * b_inc + 2 * c_inc] = -3. * y * z;
    }
  }
  if (D) {
    PetscInt p_inc = Nb * Nc * dim;
    PetscInt b_inc = Nc * dim;
    PetscInt c_inc = dim;

    for (PetscInt p = 0; p < npoints; p++) {
      const PetscReal x = points[p * dim + 0];
      const PetscReal y = points[p * dim + 1];
      const PetscReal z = points[p * dim + 2];

      /* {2 y z, 0, 0} */
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 0] = 0.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 1] = 2. * z;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 2] = 2. * y;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 1] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 2] = 0.;
      /* {0, 2 x z, 0} */
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 0] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 0] = 2. * z;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 1] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 2] = 2. * x;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 2] = 0.;
      /* {0, 2 y z, -z^2} */
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 0] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 1] = 2. * z;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 2] = 2. * y;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 2] = -2. * z;
      /* {2 x z, 0, -z^2} */
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 0] = 2. * z;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 2] = 2. * x;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 1] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 2] = -2. * z;
      /* {x^2, x y, -3 x z} */
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 0] = 2. * x;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 0] = y;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 1] = x;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 0] = -3. * z;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 2] = -3. * x;
      /* {x y, y^2, -3 y z} */
      D[p * p_inc + 5 * b_inc + 0 * c_inc + 0] = y;
      D[p * p_inc + 5 * b_inc + 0 * c_inc + 1] = x;
      D[p * p_inc + 5 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 5 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 5 * b_inc + 1 * c_inc + 1] = 2. * y;
      D[p * p_inc + 5 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 5 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 5 * b_inc + 2 * c_inc + 1] = -3. * z;
      D[p * p_inc + 5 * b_inc + 2 * c_inc + 2] = -3. * y;
    }
  }
  if (H) {
    PetscInt p_inc = Nb * Nc * dim * dim;
    PetscInt b_inc = Nc * dim * dim;
    PetscInt c_inc = dim * dim;

    for (PetscInt p = 0; p < npoints; p++) {
      /* {2 y z, 0, 0} */
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 0] = 0.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 3] = 0.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 4] = 0.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 5] = 2.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 6] = 0.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 7] = 2.;
      D[p * p_inc + 0 * b_inc + 0 * c_inc + 8] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 1] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 3] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 4] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 5] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 6] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 7] = 0.;
      D[p * p_inc + 0 * b_inc + 1 * c_inc + 8] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 2] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 3] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 4] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 5] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 6] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 7] = 0.;
      D[p * p_inc + 0 * b_inc + 2 * c_inc + 8] = 0.;
      /* {0, 2 x z, 0} */
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 0] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 3] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 4] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 5] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 6] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 7] = 0.;
      D[p * p_inc + 1 * b_inc + 0 * c_inc + 8] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 1] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 2] = 2.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 3] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 4] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 5] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 6] = 2.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 7] = 0.;
      D[p * p_inc + 1 * b_inc + 1 * c_inc + 8] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 2] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 3] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 4] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 5] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 6] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 7] = 0.;
      D[p * p_inc + 1 * b_inc + 2 * c_inc + 8] = 0.;
      /* {0, 2 y z, -z^2} */
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 0] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 3] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 4] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 5] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 6] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 7] = 0.;
      D[p * p_inc + 2 * b_inc + 0 * c_inc + 8] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 1] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 3] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 4] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 5] = 2.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 6] = 0.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 7] = 2.;
      D[p * p_inc + 2 * b_inc + 1 * c_inc + 8] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 2] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 3] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 4] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 5] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 6] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 7] = 0.;
      D[p * p_inc + 2 * b_inc + 2 * c_inc + 8] = -2.;
      /* {2 x z, 0, -z^2} */
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 0] = 0.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 2] = 2.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 3] = 0.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 4] = 0.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 5] = 0.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 6] = 2.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 7] = 0.;
      D[p * p_inc + 3 * b_inc + 0 * c_inc + 8] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 1] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 3] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 4] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 5] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 6] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 7] = 0.;
      D[p * p_inc + 3 * b_inc + 1 * c_inc + 8] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 2] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 3] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 4] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 5] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 6] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 7] = 0.;
      D[p * p_inc + 3 * b_inc + 2 * c_inc + 8] = -2.;
      /* {x^2, x y, -3 x z} */
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 0] = 2.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 3] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 4] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 5] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 6] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 7] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 8] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 1] = 1.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 3] = 1.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 4] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 5] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 6] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 7] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 8] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 2] = -3.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 3] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 4] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 5] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 6] = -3.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 7] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 8] = 0.;
      /* {x y, y^2, -3 y z} */
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 0] = 2.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 1] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 2] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 3] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 4] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 5] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 6] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 7] = 0.;
      D[p * p_inc + 4 * b_inc + 0 * c_inc + 8] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 0] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 1] = 1.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 2] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 3] = 1.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 4] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 5] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 6] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 7] = 0.;
      D[p * p_inc + 4 * b_inc + 1 * c_inc + 8] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 0] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 1] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 2] = -3.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 3] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 4] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 5] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 6] = -3.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 7] = 0.;
      D[p * p_inc + 4 * b_inc + 2 * c_inc + 8] = 0.;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceGetHeightSubspace_WXY(PetscSpace sp, PetscInt height, PetscSpace *subsp)
{
  SETERRQ(PetscObjectComm((PetscObject)sp), PETSC_ERR_SUP, "Do not know how to do this");
}

static PetscErrorCode PetscSpaceInitialize_WXY(PetscSpace sp)
{
  PetscFunctionBegin;
  sp->ops->setfromoptions    = PetscSpaceSetFromOptions_WXY;
  sp->ops->setup             = PetscSpaceSetUp_WXY;
  sp->ops->view              = PetscSpaceView_WXY;
  sp->ops->destroy           = PetscSpaceDestroy_WXY;
  sp->ops->getdimension      = PetscSpaceGetDimension_WXY;
  sp->ops->evaluate          = PetscSpaceEvaluate_WXY;
  sp->ops->getheightsubspace = PetscSpaceGetHeightSubspace_WXY;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
  PETSCSPACEWXY = "wxy" - A `PetscSpace` object that encapsulates the Wheeler-Xu-Yotov enrichments.
$ curl {{0, 0, y^2 z}, {x z^2, 0, 0}, {y z^2, 0, 0}, {0, -x z^2, 0}, {0, -3/2 x^2 z, -1/2 x^2 y}, {3/2 y^2 z, 0, 1/2 y^2 x}}
$ = {{2 y z, 0, 0}, {0, 2 x z, 0}, {0, 2 y z, -z^2}, {2 x z, 0, -z^2}, {x^2, x y, -3 x z}, {x y, y^2, -3 y z}}

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceType`, `PetscSpaceCreate()`, `PetscSpaceSetType()`
M*/

PETSC_EXTERN PetscErrorCode PetscSpaceCreate_WXY(PetscSpace sp)
{
  PetscSpace_WXY *wxy;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscCall(PetscNew(&wxy));
  sp->data   = wxy;
  sp->degree = 2;

  PetscCall(PetscSpaceInitialize_WXY(sp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

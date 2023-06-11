
#include <petsc/private/dmdaimpl.h> /*I  "petscdmda.h"   I*/

static PetscErrorCode DMDAGetElements_1D(DM dm, PetscInt *nel, PetscInt *nen, const PetscInt *e[])
{
  DM_DA   *da = (DM_DA *)dm->data;
  PetscInt i, xs, xe, Xs, Xe;
  PetscInt cnt = 0;

  PetscFunctionBegin;
  if (!da->e) {
    PetscInt corners[2];

    PetscCheck(da->s, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Cannot get elements for DMDA with zero stencil width");
    PetscCall(DMDAGetCorners(dm, &xs, NULL, NULL, &xe, NULL, NULL));
    PetscCall(DMDAGetGhostCorners(dm, &Xs, NULL, NULL, &Xe, NULL, NULL));
    xe += xs;
    Xe += Xs;
    if (xs != Xs) xs -= 1;
    da->ne = 1 * (xe - xs - 1);
    PetscCall(PetscMalloc1(1 + 2 * da->ne, &da->e));
    for (i = xs; i < xe - 1; i++) {
      da->e[cnt++] = (i - Xs);
      da->e[cnt++] = (i - Xs + 1);
    }
    da->nen = 2;

    corners[0] = (xs - Xs);
    corners[1] = (xe - 1 - Xs);
    PetscCall(ISCreateGeneral(PETSC_COMM_SELF, 2, corners, PETSC_COPY_VALUES, &da->ecorners));
  }
  *nel = da->ne;
  *nen = da->nen;
  *e   = da->e;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMDAGetElements_2D(DM dm, PetscInt *nel, PetscInt *nen, const PetscInt *e[])
{
  DM_DA   *da = (DM_DA *)dm->data;
  PetscInt i, xs, xe, Xs, Xe;
  PetscInt j, ys, ye, Ys, Ye;
  PetscInt cnt = 0, cell[4], ns = 2;
  PetscInt c, split[] = {0, 1, 3, 2, 3, 1};

  PetscFunctionBegin;
  if (!da->e) {
    PetscInt corners[4], nn = 0;

    PetscCheck(da->s, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Cannot get elements for DMDA with zero stencil width");

    switch (da->elementtype) {
    case DMDA_ELEMENT_Q1:
      da->nen = 4;
      break;
    case DMDA_ELEMENT_P1:
      da->nen = 3;
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unknown element type %d", da->elementtype);
    }
    nn = da->nen;

    if (da->elementtype == DMDA_ELEMENT_P1) ns = 2;
    if (da->elementtype == DMDA_ELEMENT_Q1) ns = 1;
    PetscCall(DMDAGetCorners(dm, &xs, &ys, NULL, &xe, &ye, NULL));
    PetscCall(DMDAGetGhostCorners(dm, &Xs, &Ys, NULL, &Xe, &Ye, NULL));
    xe += xs;
    Xe += Xs;
    if (xs != Xs) xs -= 1;
    ye += ys;
    Ye += Ys;
    if (ys != Ys) ys -= 1;
    da->ne = ns * (xe - xs - 1) * (ye - ys - 1);
    PetscCall(PetscMalloc1(1 + nn * da->ne, &da->e));
    for (j = ys; j < ye - 1; j++) {
      for (i = xs; i < xe - 1; i++) {
        cell[0] = (i - Xs) + (j - Ys) * (Xe - Xs);
        cell[1] = (i - Xs + 1) + (j - Ys) * (Xe - Xs);
        cell[2] = (i - Xs + 1) + (j - Ys + 1) * (Xe - Xs);
        cell[3] = (i - Xs) + (j - Ys + 1) * (Xe - Xs);
        if (da->elementtype == DMDA_ELEMENT_P1) {
          for (c = 0; c < ns * nn; c++) da->e[cnt++] = cell[split[c]];
        }
        if (da->elementtype == DMDA_ELEMENT_Q1) {
          for (c = 0; c < ns * nn; c++) da->e[cnt++] = cell[c];
        }
      }
    }

    corners[0] = (xs - Xs) + (ys - Ys) * (Xe - Xs);
    corners[1] = (xe - 1 - Xs) + (ys - Ys) * (Xe - Xs);
    corners[2] = (xs - Xs) + (ye - 1 - Ys) * (Xe - Xs);
    corners[3] = (xe - 1 - Xs) + (ye - 1 - Ys) * (Xe - Xs);
    PetscCall(ISCreateGeneral(PETSC_COMM_SELF, 4, corners, PETSC_COPY_VALUES, &da->ecorners));
  }
  *nel = da->ne;
  *nen = da->nen;
  *e   = da->e;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMDAGetElements_3D(DM dm, PetscInt *nel, PetscInt *nen, const PetscInt *e[])
{
  DM_DA   *da = (DM_DA *)dm->data;
  PetscInt i, xs, xe, Xs, Xe;
  PetscInt j, ys, ye, Ys, Ye;
  PetscInt k, zs, ze, Zs, Ze;
  PetscInt cnt = 0, cell[8], ns = 6;
  PetscInt c, split[] = {0, 1, 3, 7, 0, 1, 7, 4, 1, 2, 3, 7, 1, 2, 7, 6, 1, 4, 5, 7, 1, 5, 6, 7};

  PetscFunctionBegin;
  if (!da->e) {
    PetscInt corners[8], nn = 0;

    PetscCheck(da->s, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Cannot get elements for DMDA with zero stencil width");

    switch (da->elementtype) {
    case DMDA_ELEMENT_Q1:
      da->nen = 8;
      break;
    case DMDA_ELEMENT_P1:
      da->nen = 4;
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unknown element type %d", da->elementtype);
    }
    nn = da->nen;

    if (da->elementtype == DMDA_ELEMENT_P1) ns = 6;
    if (da->elementtype == DMDA_ELEMENT_Q1) ns = 1;
    PetscCall(DMDAGetCorners(dm, &xs, &ys, &zs, &xe, &ye, &ze));
    PetscCall(DMDAGetGhostCorners(dm, &Xs, &Ys, &Zs, &Xe, &Ye, &Ze));
    xe += xs;
    Xe += Xs;
    if (xs != Xs) xs -= 1;
    ye += ys;
    Ye += Ys;
    if (ys != Ys) ys -= 1;
    ze += zs;
    Ze += Zs;
    if (zs != Zs) zs -= 1;
    da->ne = ns * (xe - xs - 1) * (ye - ys - 1) * (ze - zs - 1);
    PetscCall(PetscMalloc1(1 + nn * da->ne, &da->e));
    for (k = zs; k < ze - 1; k++) {
      for (j = ys; j < ye - 1; j++) {
        for (i = xs; i < xe - 1; i++) {
          cell[0] = (i - Xs) + (j - Ys) * (Xe - Xs) + (k - Zs) * (Xe - Xs) * (Ye - Ys);
          cell[1] = (i + 1 - Xs) + (j - Ys) * (Xe - Xs) + (k - Zs) * (Xe - Xs) * (Ye - Ys);
          cell[2] = (i + 1 - Xs) + (j + 1 - Ys) * (Xe - Xs) + (k - Zs) * (Xe - Xs) * (Ye - Ys);
          cell[3] = (i - Xs) + (j + 1 - Ys) * (Xe - Xs) + (k - Zs) * (Xe - Xs) * (Ye - Ys);
          cell[4] = (i - Xs) + (j - Ys) * (Xe - Xs) + (k + 1 - Zs) * (Xe - Xs) * (Ye - Ys);
          cell[5] = (i + 1 - Xs) + (j - Ys) * (Xe - Xs) + (k + 1 - Zs) * (Xe - Xs) * (Ye - Ys);
          cell[6] = (i + 1 - Xs) + (j + 1 - Ys) * (Xe - Xs) + (k + 1 - Zs) * (Xe - Xs) * (Ye - Ys);
          cell[7] = (i - Xs) + (j + 1 - Ys) * (Xe - Xs) + (k + 1 - Zs) * (Xe - Xs) * (Ye - Ys);
          if (da->elementtype == DMDA_ELEMENT_P1) {
            for (c = 0; c < ns * nn; c++) da->e[cnt++] = cell[split[c]];
          }
          if (da->elementtype == DMDA_ELEMENT_Q1) {
            for (c = 0; c < ns * nn; c++) da->e[cnt++] = cell[c];
          }
        }
      }
    }

    corners[0] = (xs - Xs) + (ys - Ys) * (Xe - Xs) + (zs - Zs) * (Xe - Xs) * (Ye - Ys);
    corners[1] = (xe - 1 - Xs) + (ys - Ys) * (Xe - Xs) + (zs - Zs) * (Xe - Xs) * (Ye - Ys);
    corners[2] = (xs - Xs) + (ye - 1 - Ys) * (Xe - Xs) + (zs - Zs) * (Xe - Xs) * (Ye - Ys);
    corners[3] = (xe - 1 - Xs) + (ye - 1 - Ys) * (Xe - Xs) + (zs - Zs) * (Xe - Xs) * (Ye - Ys);
    corners[4] = (xs - Xs) + (ys - Ys) * (Xe - Xs) + (ze - 1 - Zs) * (Xe - Xs) * (Ye - Ys);
    corners[5] = (xe - 1 - Xs) + (ys - Ys) * (Xe - Xs) + (ze - 1 - Zs) * (Xe - Xs) * (Ye - Ys);
    corners[6] = (xs - Xs) + (ye - 1 - Ys) * (Xe - Xs) + (ze - 1 - Zs) * (Xe - Xs) * (Ye - Ys);
    corners[7] = (xe - 1 - Xs) + (ye - 1 - Ys) * (Xe - Xs) + (ze - 1 - Zs) * (Xe - Xs) * (Ye - Ys);
    PetscCall(ISCreateGeneral(PETSC_COMM_SELF, 8, corners, PETSC_COPY_VALUES, &da->ecorners));
  }
  *nel = da->ne;
  *nen = da->nen;
  *e   = da->e;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   DMDAGetElementsCorners - Returns the global (x,y,z) indices of the lower left
   corner of the non-overlapping decomposition identified by `DMDAGetElements()`

    Not Collective

   Input Parameter:
.     da - the `DMDA` object

   Output Parameters:
+     gx - the x index
.     gy - the y index
-     gz - the z index

   Level: intermediate

.seealso: `DM`, `DMDA`, `DMDAElementType`, `DMDASetElementType()`, `DMDAGetElements()`
@*/
PetscErrorCode DMDAGetElementsCorners(DM da, PetscInt *gx, PetscInt *gy, PetscInt *gz)
{
  PetscInt  xs, Xs;
  PetscInt  ys, Ys;
  PetscInt  zs, Zs;
  PetscBool isda;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  if (gx) PetscValidIntPointer(gx, 2);
  if (gy) PetscValidIntPointer(gy, 3);
  if (gz) PetscValidIntPointer(gz, 4);
  PetscCall(PetscObjectTypeCompare((PetscObject)da, DMDA, &isda));
  PetscCheck(isda, PetscObjectComm((PetscObject)da), PETSC_ERR_USER, "Not for DM type %s", ((PetscObject)da)->type_name);
  PetscCall(DMDAGetCorners(da, &xs, &ys, &zs, NULL, NULL, NULL));
  PetscCall(DMDAGetGhostCorners(da, &Xs, &Ys, &Zs, NULL, NULL, NULL));
  if (xs != Xs) xs -= 1;
  if (ys != Ys) ys -= 1;
  if (zs != Zs) zs -= 1;
  if (gx) *gx = xs;
  if (gy) *gy = ys;
  if (gz) *gz = zs;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
      DMDAGetElementsSizes - Gets the local number of elements per direction for the non-overlapping decomposition identified by `DMDAGetElements()`

    Not Collective

   Input Parameter:
.     da - the `DMDA` object

   Output Parameters:
+     mx - number of local elements in x-direction
.     my - number of local elements in y-direction
-     mz - number of local elements in z-direction

   Level: intermediate

   Note:
    It returns the same number of elements, irrespective of the `DMDAElementType`

.seealso: `DM`, `DMDA`, `DMDAElementType`, `DMDASetElementType()`, `DMDAGetElements`
@*/
PetscErrorCode DMDAGetElementsSizes(DM da, PetscInt *mx, PetscInt *my, PetscInt *mz)
{
  PetscInt  xs, xe, Xs;
  PetscInt  ys, ye, Ys;
  PetscInt  zs, ze, Zs;
  PetscInt  dim;
  PetscBool isda;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  if (mx) PetscValidIntPointer(mx, 2);
  if (my) PetscValidIntPointer(my, 3);
  if (mz) PetscValidIntPointer(mz, 4);
  PetscCall(PetscObjectTypeCompare((PetscObject)da, DMDA, &isda));
  PetscCheck(isda, PetscObjectComm((PetscObject)da), PETSC_ERR_USER, "Not for DM type %s", ((PetscObject)da)->type_name);
  PetscCall(DMDAGetCorners(da, &xs, &ys, &zs, &xe, &ye, &ze));
  PetscCall(DMDAGetGhostCorners(da, &Xs, &Ys, &Zs, NULL, NULL, NULL));
  xe += xs;
  if (xs != Xs) xs -= 1;
  ye += ys;
  if (ys != Ys) ys -= 1;
  ze += zs;
  if (zs != Zs) zs -= 1;
  if (mx) *mx = 0;
  if (my) *my = 0;
  if (mz) *mz = 0;
  PetscCall(DMGetDimension(da, &dim));
  switch (dim) {
  case 3:
    if (mz) *mz = ze - zs - 1; /* fall through */
  case 2:
    if (my) *my = ye - ys - 1; /* fall through */
  case 1:
    if (mx) *mx = xe - xs - 1;
    break;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
      DMDASetElementType - Sets the element type to be returned by `DMDAGetElements()`

    Not Collective

   Input Parameter:
.     da - the `DMDA` object

   Output Parameter:
.     etype - the element type, currently either `DMDA_ELEMENT_P1` or `DMDA_ELEMENT_Q1`

   Level: intermediate

.seealso: `DM`, `DMDA`, `DMDAElementType`, `DMDAGetElementType()`, `DMDAGetElements()`, `DMDARestoreElements()`
@*/
PetscErrorCode DMDASetElementType(DM da, DMDAElementType etype)
{
  DM_DA    *dd = (DM_DA *)da->data;
  PetscBool isda;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  PetscValidLogicalCollectiveEnum(da, etype, 2);
  PetscCall(PetscObjectTypeCompare((PetscObject)da, DMDA, &isda));
  if (!isda) PetscFunctionReturn(PETSC_SUCCESS);
  if (dd->elementtype != etype) {
    PetscCall(PetscFree(dd->e));
    PetscCall(ISDestroy(&dd->ecorners));

    dd->elementtype = etype;
    dd->ne          = 0;
    dd->nen         = 0;
    dd->e           = NULL;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
      DMDAGetElementType - Gets the element type to be returned by `DMDAGetElements()`

    Not Collective

   Input Parameter:
.     da - the `DMDA` object

   Output Parameter:
.     etype - the element type, currently either `DMDA_ELEMENT_P1` or `DMDA_ELEMENT_Q1`

   Level: intermediate

.seealso: `DM`, `DMDA`, `DMDAElementType`, `DMDASetElementType()`, `DMDAGetElements()`, `DMDARestoreElements()`
@*/
PetscErrorCode DMDAGetElementType(DM da, DMDAElementType *etype)
{
  DM_DA    *dd = (DM_DA *)da->data;
  PetscBool isda;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  PetscValidPointer(etype, 2);
  PetscCall(PetscObjectTypeCompare((PetscObject)da, DMDA, &isda));
  PetscCheck(isda, PetscObjectComm((PetscObject)da), PETSC_ERR_USER, "Not for DM type %s", ((PetscObject)da)->type_name);
  *etype = dd->elementtype;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
      DMDAGetElements - Gets an array containing the indices (in local coordinates)
                 of all the local elements

    Not Collective; No Fortran Support

   Input Parameter:
.     dm - the `DMDA` object

   Output Parameters:
+     nel - number of local elements
.     nen - number of element nodes
-     e - the local indices of the elements' vertices

   Level: intermediate

   Notes:
     Call `DMDARestoreElements()` once you have finished accessing the elements.

     Each process uniquely owns a subset of the elements. That is no element is owned by two or more processes.

     If on each process you integrate over its owned elements and use `ADD_VALUES` in `Vec`/`MatSetValuesLocal()` then you'll obtain the correct result.

.seealso: `DM`, `DMDA`, `DMDAElementType`, `DMDASetElementType()`, `VecSetValuesLocal()`, `MatSetValuesLocal()`, `DMGlobalToLocalBegin()`, `DMLocalToGlobalBegin()`
@*/
PetscErrorCode DMDAGetElements(DM dm, PetscInt *nel, PetscInt *nen, const PetscInt *e[])
{
  PetscInt  dim;
  DM_DA    *dd = (DM_DA *)dm->data;
  PetscBool isda;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(dm, DM_CLASSID, 1, DMDA);
  PetscValidIntPointer(nel, 2);
  PetscValidIntPointer(nen, 3);
  PetscValidPointer(e, 4);
  PetscCall(PetscObjectTypeCompare((PetscObject)dm, DMDA, &isda));
  PetscCheck(isda, PetscObjectComm((PetscObject)dm), PETSC_ERR_USER, "Not for DM type %s", ((PetscObject)dm)->type_name);
  PetscCheck(dd->stencil_type != DMDA_STENCIL_STAR, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "DMDAGetElements() requires you use a stencil type of DMDA_STENCIL_BOX");
  PetscCall(DMGetDimension(dm, &dim));
  if (dd->e) {
    *nel = dd->ne;
    *nen = dd->nen;
    *e   = dd->e;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (dim == -1) {
    *nel = 0;
    *nen = 0;
    *e   = NULL;
  } else if (dim == 1) {
    PetscCall(DMDAGetElements_1D(dm, nel, nen, e));
  } else if (dim == 2) {
    PetscCall(DMDAGetElements_2D(dm, nel, nen, e));
  } else if (dim == 3) {
    PetscCall(DMDAGetElements_3D(dm, nel, nen, e));
  } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_CORRUPT, "DMDA dimension not 1, 2, or 3, it is %" PetscInt_FMT, dim);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
      DMDAGetSubdomainCornersIS - Gets an index set containing the corner indices (in local coordinates)
                                 of the non-overlapping decomposition identified by `DMDAGetElements()`

    Not Collective

   Input Parameter:
.     dm - the `DMDA` object

   Output Parameter:
.     is - the index set

   Level: intermediate

   Note:
    Call `DMDARestoreSubdomainCornersIS()` once you have finished accessing the index set.

.seealso: `DM`, `DMDA`, `DMDAElementType`, `DMDASetElementType()`, `DMDAGetElements()`, `DMDARestoreElementsCornersIS()`
@*/
PetscErrorCode DMDAGetSubdomainCornersIS(DM dm, IS *is)
{
  DM_DA    *dd = (DM_DA *)dm->data;
  PetscBool isda;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(dm, DM_CLASSID, 1, DMDA);
  PetscValidPointer(is, 2);
  PetscCall(PetscObjectTypeCompare((PetscObject)dm, DMDA, &isda));
  PetscCheck(isda, PetscObjectComm((PetscObject)dm), PETSC_ERR_USER, "Not for DM type %s", ((PetscObject)dm)->type_name);
  PetscCheck(dd->stencil_type != DMDA_STENCIL_STAR, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "DMDAGetElement() requires you use a stencil type of DMDA_STENCIL_BOX");
  if (!dd->ecorners) { /* compute elements if not yet done */
    const PetscInt *e;
    PetscInt        nel, nen;

    PetscCall(DMDAGetElements(dm, &nel, &nen, &e));
    PetscCall(DMDARestoreElements(dm, &nel, &nen, &e));
  }
  *is = dd->ecorners;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
      DMDARestoreElements - Restores the array obtained with `DMDAGetElements()`

    Not Collective; No Fortran Support

   Input Parameters:
+     dm - the `DM` object
.     nel - number of local elements
.     nen - number of element nodes
-     e - the local indices of the elements' vertices

   Level: intermediate

   Note:
   This restore signals the `DMDA` object that you no longer need access to the array information.

.seealso: `DM`, `DMDA`, `DMDAElementType`, `DMDASetElementType()`, `DMDAGetElements()`
@*/
PetscErrorCode DMDARestoreElements(DM dm, PetscInt *nel, PetscInt *nen, const PetscInt *e[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecificType(dm, DM_CLASSID, 1, DMDA);
  PetscValidIntPointer(nel, 2);
  PetscValidIntPointer(nen, 3);
  PetscValidPointer(e, 4);
  *nel = 0;
  *nen = -1;
  *e   = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
      DMDARestoreSubdomainCornersIS - Restores the `IS` obtained with `DMDAGetSubdomainCornersIS()`

    Not Collective

   Input Parameters:
+     dm - the `DM` object
-     is - the index set

   Level: intermediate

.seealso: `DM`, `DMDA`, `DMDAElementType`, `DMDASetElementType()`, `DMDAGetSubdomainCornersIS()`
@*/
PetscErrorCode DMDARestoreSubdomainCornersIS(DM dm, IS *is)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecificType(dm, DM_CLASSID, 1, DMDA);
  PetscValidHeaderSpecific(*is, IS_CLASSID, 2);
  *is = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

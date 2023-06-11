/* Functions concerning getting and setting Vec and Mat values with DMStagStencil */
#include <petsc/private/dmstagimpl.h>

/* Strings corresponding the the types defined in $PETSC_DIR/include/petscdmstag.h */
const char *const DMStagStencilTypes[] = {"NONE", "STAR", "BOX", "DMStagStencilType", "DM_STAG_STENCIL_", NULL};

/* Strings corresponding the positions in $PETSC_DIR/include/petscdmstag.h */
const char *const DMStagStencilLocations[] = {"NONE", "BACK_DOWN_LEFT", "BACK_DOWN", "BACK_DOWN_RIGHT", "BACK_LEFT", "BACK", "BACK_RIGHT", "BACK_UP_LEFT", "BACK_UP", "BACK_UP_RIGHT", "DOWN_LEFT", "DOWN", "DOWN_RIGHT", "LEFT", "ELEMENT", "RIGHT", "UP_LEFT", "UP", "UP_RIGHT", "FRONT_DOWN_LEFT", "FRONT_DOWN", "FRONT_DOWN_RIGHT", "FRONT_LEFT", "FRONT", "FRONT_RIGHT", "FRONT_UP_LEFT", "FRONT_UP", "FRONT_UP_RIGHT", "DMStagStencilLocation", "", NULL};

/*@C
  DMStagCreateISFromStencils - Create an `IS`, using global numberings, for a subset of DOF in a `DMSTAG` object

  Collective

  Input Parameters:
+ dm - the `DMStag` object
. n_stencil - the number of stencils provided
- stencils - an array of `DMStagStencil` objects (`i`, `j`, and `k` are ignored)

  Output Parameter:
. is - the global IS

  Note:
  Redundant entries in the stencils argument are ignored

  Level: advanced

.seealso: [](chapter_stag), `DMSTAG`, `IS`, `DMStagStencil`, `DMCreateGlobalVector`
@*/
PetscErrorCode DMStagCreateISFromStencils(DM dm, PetscInt n_stencil, DMStagStencil *stencils, IS *is)
{
  PetscInt              *stencil_active;
  DMStagStencil         *stencils_ordered_unique;
  PetscInt              *idx, *idxLocal;
  const PetscInt        *ltogidx;
  PetscInt               n_stencil_unique, dim, count, nidx, nc_max;
  ISLocalToGlobalMapping ltog;
  PetscInt               start[DMSTAG_MAX_DIM], n[DMSTAG_MAX_DIM], extraPoint[DMSTAG_MAX_DIM];

  PetscFunctionBegin;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCheck(dim >= 1 && dim <= 3, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unsupported dimension %" PetscInt_FMT, dim);

  /* To assert that the resulting IS has unique, sorted, entries, we perform
     a bucket sort, taking advantage of the fact that DMStagStencilLocation
     enum values are integers starting with 1, in canonical order */
  nc_max           = 1; // maximum number of components to represent these stencils
  n_stencil_unique = 0;
  for (PetscInt p = 0; p < n_stencil; ++p) nc_max = PetscMax(nc_max, (stencils[p].c + 1));
  PetscCall(PetscCalloc1(DMSTAG_NUMBER_LOCATIONS * nc_max, &stencil_active));
  for (PetscInt p = 0; p < n_stencil; ++p) {
    DMStagStencilLocation loc_canonical;
    PetscInt              slot;

    PetscCall(DMStagStencilLocationCanonicalize(stencils[p].loc, &loc_canonical));
    slot = nc_max * ((PetscInt)loc_canonical) + stencils[p].c;
    if (stencil_active[slot] == 0) {
      stencil_active[slot] = 1;
      ++n_stencil_unique;
    }
  }
  PetscCall(PetscMalloc1(n_stencil_unique, &stencils_ordered_unique));
  {
    PetscInt p = 0;

    for (PetscInt i = 1; i < DMSTAG_NUMBER_LOCATIONS; ++i) {
      for (PetscInt c = 0; c < nc_max; ++c) {
        if (stencil_active[nc_max * i + c] != 0) {
          stencils_ordered_unique[p].loc = (DMStagStencilLocation)i;
          stencils_ordered_unique[p].c   = c;
          ++p;
        }
      }
    }
  }
  PetscCall(PetscFree(stencil_active));

  PetscCall(PetscMalloc1(n_stencil_unique, &idxLocal));
  PetscCall(DMGetLocalToGlobalMapping(dm, &ltog));
  PetscCall(ISLocalToGlobalMappingGetIndices(ltog, &ltogidx));
  PetscCall(DMStagGetCorners(dm, &start[0], &start[1], &start[2], &n[0], &n[1], &n[2], &extraPoint[0], &extraPoint[1], &extraPoint[2]));
  for (PetscInt d = dim; d < DMSTAG_MAX_DIM; ++d) {
    start[d]      = 0;
    n[d]          = 1; /* To allow for a single loop nest below */
    extraPoint[d] = 0;
  }
  nidx = n_stencil_unique;
  for (PetscInt d = 0; d < dim; ++d) nidx *= (n[d] + 1); /* Overestimate (always assumes extraPoint) */
  PetscCall(PetscMalloc1(nidx, &idx));
  count = 0;
  /* Note that unused loop variables are not accessed, for lower dimensions */
  for (PetscInt k = start[2]; k < start[2] + n[2] + extraPoint[2]; ++k) {
    for (PetscInt j = start[1]; j < start[1] + n[1] + extraPoint[1]; ++j) {
      for (PetscInt i = start[0]; i < start[0] + n[0] + extraPoint[0]; ++i) {
        for (PetscInt p = 0; p < n_stencil_unique; ++p) {
          stencils_ordered_unique[p].i = i;
          stencils_ordered_unique[p].j = j;
          stencils_ordered_unique[p].k = k;
        }
        PetscCall(DMStagStencilToIndexLocal(dm, dim, n_stencil_unique, stencils_ordered_unique, idxLocal));
        for (PetscInt p = 0; p < n_stencil_unique; ++p) {
          const PetscInt gidx = ltogidx[idxLocal[p]];
          if (gidx >= 0) {
            idx[count] = gidx;
            ++count;
          }
        }
      }
    }
  }

  PetscCall(ISLocalToGlobalMappingRestoreIndices(ltog, &ltogidx));
  PetscCall(PetscFree(stencils_ordered_unique));
  PetscCall(PetscFree(idxLocal));

  PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)dm), count, idx, PETSC_OWN_POINTER, is));
  PetscCall(ISSetInfo(*is, IS_SORTED, IS_GLOBAL, PETSC_TRUE, PETSC_TRUE));
  PetscCall(ISSetInfo(*is, IS_UNIQUE, IS_GLOBAL, PETSC_TRUE, PETSC_TRUE));

  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMStagGetLocationDOF - Get number of DOF associated with a given point in a `DMSTAG` grid

  Not Collective

  Input Parameters:
+ dm - the `DMSTAG` object
- loc - grid point (see `DMStagStencilLocation`)

  Output Parameter:
. dof - the number of DOF (components) living at `loc` in `dm`

  Level: intermediate

.seealso: [](chapter_stag), `DMSTAG`, `DMStagStencilLocation`, `DMStagStencil`, `DMDAGetDof()`
@*/
PetscErrorCode DMStagGetLocationDOF(DM dm, DMStagStencilLocation loc, PetscInt *dof)
{
  const DM_Stag *const stag = (DM_Stag *)dm->data;
  PetscInt             dim;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(dm, DM_CLASSID, 1, DMSTAG);
  PetscCall(DMGetDimension(dm, &dim));
  switch (dim) {
  case 1:
    switch (loc) {
    case DMSTAG_LEFT:
    case DMSTAG_RIGHT:
      *dof = stag->dof[0];
      break;
    case DMSTAG_ELEMENT:
      *dof = stag->dof[1];
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Not implemented for location %s", DMStagStencilLocations[loc]);
    }
    break;
  case 2:
    switch (loc) {
    case DMSTAG_DOWN_LEFT:
    case DMSTAG_DOWN_RIGHT:
    case DMSTAG_UP_LEFT:
    case DMSTAG_UP_RIGHT:
      *dof = stag->dof[0];
      break;
    case DMSTAG_LEFT:
    case DMSTAG_RIGHT:
    case DMSTAG_UP:
    case DMSTAG_DOWN:
      *dof = stag->dof[1];
      break;
    case DMSTAG_ELEMENT:
      *dof = stag->dof[2];
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Not implemented for location %s", DMStagStencilLocations[loc]);
    }
    break;
  case 3:
    switch (loc) {
    case DMSTAG_BACK_DOWN_LEFT:
    case DMSTAG_BACK_DOWN_RIGHT:
    case DMSTAG_BACK_UP_LEFT:
    case DMSTAG_BACK_UP_RIGHT:
    case DMSTAG_FRONT_DOWN_LEFT:
    case DMSTAG_FRONT_DOWN_RIGHT:
    case DMSTAG_FRONT_UP_LEFT:
    case DMSTAG_FRONT_UP_RIGHT:
      *dof = stag->dof[0];
      break;
    case DMSTAG_BACK_DOWN:
    case DMSTAG_BACK_LEFT:
    case DMSTAG_BACK_RIGHT:
    case DMSTAG_BACK_UP:
    case DMSTAG_DOWN_LEFT:
    case DMSTAG_DOWN_RIGHT:
    case DMSTAG_UP_LEFT:
    case DMSTAG_UP_RIGHT:
    case DMSTAG_FRONT_DOWN:
    case DMSTAG_FRONT_LEFT:
    case DMSTAG_FRONT_RIGHT:
    case DMSTAG_FRONT_UP:
      *dof = stag->dof[1];
      break;
    case DMSTAG_LEFT:
    case DMSTAG_RIGHT:
    case DMSTAG_DOWN:
    case DMSTAG_UP:
    case DMSTAG_BACK:
    case DMSTAG_FRONT:
      *dof = stag->dof[2];
      break;
    case DMSTAG_ELEMENT:
      *dof = stag->dof[3];
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Not implemented for location %s", DMStagStencilLocations[loc]);
    }
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unsupported dimension %" PetscInt_FMT, dim);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
Convert to a location value with only BACK, DOWN, LEFT, and ELEMENT involved
*/
PETSC_INTERN PetscErrorCode DMStagStencilLocationCanonicalize(DMStagStencilLocation loc, DMStagStencilLocation *locCanonical)
{
  PetscFunctionBegin;
  switch (loc) {
  case DMSTAG_ELEMENT:
    *locCanonical = DMSTAG_ELEMENT;
    break;
  case DMSTAG_LEFT:
  case DMSTAG_RIGHT:
    *locCanonical = DMSTAG_LEFT;
    break;
  case DMSTAG_DOWN:
  case DMSTAG_UP:
    *locCanonical = DMSTAG_DOWN;
    break;
  case DMSTAG_BACK:
  case DMSTAG_FRONT:
    *locCanonical = DMSTAG_BACK;
    break;
  case DMSTAG_DOWN_LEFT:
  case DMSTAG_DOWN_RIGHT:
  case DMSTAG_UP_LEFT:
  case DMSTAG_UP_RIGHT:
    *locCanonical = DMSTAG_DOWN_LEFT;
    break;
  case DMSTAG_BACK_LEFT:
  case DMSTAG_BACK_RIGHT:
  case DMSTAG_FRONT_LEFT:
  case DMSTAG_FRONT_RIGHT:
    *locCanonical = DMSTAG_BACK_LEFT;
    break;
  case DMSTAG_BACK_DOWN:
  case DMSTAG_BACK_UP:
  case DMSTAG_FRONT_DOWN:
  case DMSTAG_FRONT_UP:
    *locCanonical = DMSTAG_BACK_DOWN;
    break;
  case DMSTAG_BACK_DOWN_LEFT:
  case DMSTAG_BACK_DOWN_RIGHT:
  case DMSTAG_BACK_UP_LEFT:
  case DMSTAG_BACK_UP_RIGHT:
  case DMSTAG_FRONT_DOWN_LEFT:
  case DMSTAG_FRONT_DOWN_RIGHT:
  case DMSTAG_FRONT_UP_LEFT:
  case DMSTAG_FRONT_UP_RIGHT:
    *locCanonical = DMSTAG_BACK_DOWN_LEFT;
    break;
  default:
    *locCanonical = DMSTAG_NULL_LOCATION;
    break;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMStagMatGetValuesStencil - retrieve local matrix entries using grid indexing

  Not Collective

  Input Parameters:
+ dm - the `DMSTAG` object
. mat - the matrix
. nRow - number of rows
. posRow - grid locations (including components) of rows
. nCol - number of columns
- posCol - grid locations (including components) of columns

  Output Parameter:
. val - logically two-dimensional array of values

  Level: advanced

.seealso: [](chapter_stag), `DMSTAG`, `DMStagStencil`, `DMStagStencilLocation`, `DMStagVecGetValuesStencil()`, `DMStagVecSetValuesStencil()`, `DMStagMatSetValuesStencil()`, `MatSetValuesStencil()`, `MatAssemblyBegin()`, `MatAssemblyEnd()`, `DMCreateMatrix()`
@*/
PetscErrorCode DMStagMatGetValuesStencil(DM dm, Mat mat, PetscInt nRow, const DMStagStencil *posRow, PetscInt nCol, const DMStagStencil *posCol, PetscScalar *val)
{
  PetscInt  dim;
  PetscInt *ir, *ic;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 2);
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(PetscMalloc2(nRow, &ir, nCol, &ic));
  PetscCall(DMStagStencilToIndexLocal(dm, dim, nRow, posRow, ir));
  PetscCall(DMStagStencilToIndexLocal(dm, dim, nCol, posCol, ic));
  PetscCall(MatGetValuesLocal(mat, nRow, ir, nCol, ic, val));
  PetscCall(PetscFree2(ir, ic));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMStagMatSetValuesStencil - insert or add matrix entries using grid indexing

  Not Collective

  Input Parameters:
+ dm - the `DMSTAG` object
. mat - the matrix
. nRow - number of rows
. posRow - grid locations (including components) of rows
. nCol - number of columns
. posCol - grid locations (including components) of columns
. val - logically two-dimensional array of values
- insertmode - `INSERT_VALUES` or `ADD_VALUES`

  Notes:
  See notes for `MatSetValuesStencil()`

  Level: intermediate

.seealso: [](chapter_stag), `DMSTAG`, `DMStagStencil`, `DMStagStencilLocation`, `DMStagVecGetValuesStencil()`, `DMStagVecSetValuesStencil()`, `DMStagMatGetValuesStencil()`, `MatSetValuesStencil()`, `MatAssemblyBegin()`, `MatAssemblyEnd()`, `DMCreateMatrix()`
@*/
PetscErrorCode DMStagMatSetValuesStencil(DM dm, Mat mat, PetscInt nRow, const DMStagStencil *posRow, PetscInt nCol, const DMStagStencil *posCol, const PetscScalar *val, InsertMode insertMode)
{
  PetscInt *ir, *ic;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 2);
  PetscCall(PetscMalloc2(nRow, &ir, nCol, &ic));
  PetscCall(DMStagStencilToIndexLocal(dm, dm->dim, nRow, posRow, ir));
  PetscCall(DMStagStencilToIndexLocal(dm, dm->dim, nCol, posCol, ic));
  PetscCall(MatSetValuesLocal(mat, nRow, ir, nCol, ic, val, insertMode));
  PetscCall(PetscFree2(ir, ic));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMStagStencilToIndexLocal - Convert an array of `DMStagStenci`l objects to an array of indices into a local vector.

  Not Collective

  Input Parameters:
+ dm - the `DMSTAG` object
. dim - the dimension of the `DMSTAG` object
. n - the number of `DMStagStencil` objects
- pos - an array of `n` `DMStagStencil` objects

  Output Parameter:
. ix - output array of `n` indices

  Notes:
  The `DMStagStencil` objects in `pos` use global element indices.

  The `.c` fields in `pos` must always be set (even if to `0`).

  Developer Notes:
  This is a "hot" function, and accepts the dimension redundantly to avoid having to perform any error checking inside the function.

  Level: developer

.seealso: [](chapter_stag), `DMSTAG`, `DMStagStencilLocation`, `DMStagStencil`, `DMGetLocalVector`, `DMCreateLocalVector`
@*/
PetscErrorCode DMStagStencilToIndexLocal(DM dm, PetscInt dim, PetscInt n, const DMStagStencil *pos, PetscInt *ix)
{
  const DM_Stag *const stag = (DM_Stag *)dm->data;
  const PetscInt       epe  = stag->entriesPerElement;

  PetscFunctionBeginHot;
  if (dim == 1) {
    for (PetscInt idx = 0; idx < n; ++idx) {
      const PetscInt eLocal = pos[idx].i - stag->startGhost[0];

      ix[idx] = eLocal * epe + stag->locationOffsets[pos[idx].loc] + pos[idx].c;
    }
  } else if (dim == 2) {
    const PetscInt epr = stag->nGhost[0];

    for (PetscInt idx = 0; idx < n; ++idx) {
      const PetscInt eLocalx = pos[idx].i - stag->startGhost[0];
      const PetscInt eLocaly = pos[idx].j - stag->startGhost[1];
      const PetscInt eLocal  = eLocalx + epr * eLocaly;

      ix[idx] = eLocal * epe + stag->locationOffsets[pos[idx].loc] + pos[idx].c;
    }
  } else if (dim == 3) {
    const PetscInt epr = stag->nGhost[0];
    const PetscInt epl = stag->nGhost[0] * stag->nGhost[1];

    for (PetscInt idx = 0; idx < n; ++idx) {
      const PetscInt eLocalx = pos[idx].i - stag->startGhost[0];
      const PetscInt eLocaly = pos[idx].j - stag->startGhost[1];
      const PetscInt eLocalz = pos[idx].k - stag->startGhost[2];
      const PetscInt eLocal  = epl * eLocalz + epr * eLocaly + eLocalx;

      ix[idx] = eLocal * epe + stag->locationOffsets[pos[idx].loc] + pos[idx].c;
    }
  } else SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Unsupported dimension %" PetscInt_FMT, dim);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMStagVecGetValuesStencil - get vector values using grid indexing

  Not Collective

  Input Parameters:
+ dm - the `DMSTAG` object
. vec - the vector object
. n - the number of values to obtain
- pos - locations to obtain values from (as an array of `DMStagStencil` values)

  Output Parameter:
. val - value at the point

  Notes:
  Accepts stencils which refer to global element numbers, but
  only allows access to entries in the local representation (including ghosts).

  This approach is not as efficient as getting values directly with `DMStagVecGetArray()`,
  which is recommended for matrix free operators.

  Level: advanced

.seealso: [](chapter_stag), `DMSTAG`, `DMStagStencil`, `DMStagStencilLocation`, `DMStagVecSetValuesStencil()`, `DMStagMatSetValuesStencil()`, `DMStagVecGetArray()`
@*/
PetscErrorCode DMStagVecGetValuesStencil(DM dm, Vec vec, PetscInt n, const DMStagStencil *pos, PetscScalar *val)
{
  DM_Stag *const     stag = (DM_Stag *)dm->data;
  PetscInt           nLocal, idx;
  PetscInt          *ix;
  PetscScalar const *arr;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(dm, DM_CLASSID, 1, DMSTAG);
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 2);
  PetscCall(VecGetLocalSize(vec, &nLocal));
  PetscCheck(nLocal == stag->entriesGhost, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Vector should be a local vector. Local size %" PetscInt_FMT " does not match expected %" PetscInt_FMT, nLocal, stag->entriesGhost);
  PetscCall(PetscMalloc1(n, &ix));
  PetscCall(DMStagStencilToIndexLocal(dm, dm->dim, n, pos, ix));
  PetscCall(VecGetArrayRead(vec, &arr));
  for (idx = 0; idx < n; ++idx) val[idx] = arr[ix[idx]];
  PetscCall(VecRestoreArrayRead(vec, &arr));
  PetscCall(PetscFree(ix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMStagVecSetValuesStencil - Set `Vec` values using global grid indexing

  Not Collective

  Input Parameters:
+ dm - the `DMSTAG` object
. vec - the `Vec`
. n - the number of values to set
. pos - the locations to set values, as an array of `DMStagStencil` structs
. val - the values to set
- insertMode - `INSERT_VALUES` or `ADD_VALUES`

  Notes:
  The vector is expected to be a global vector compatible with the DM (usually obtained by `DMGetGlobalVector()` or `DMCreateGlobalVector()`).

  This approach is not as efficient as setting values directly with `DMStagVecGetArray()`, which is recommended for matrix-free operators.
  For assembling systems, where overhead may be less important than convenience, this routine could be helpful in assembling a righthand side and a matrix (using `DMStagMatSetValuesStencil()`).

  Level: advanced

.seealso: [](chapter_stag), `DMSTAG`, `Vec`, `DMStagStencil`, `DMStagStencilLocation`, `DMStagVecGetValuesStencil()`, `DMStagMatSetValuesStencil()`, `DMCreateGlobalVector()`, `DMGetLocalVector()`, `DMStagVecGetArray()`
@*/
PetscErrorCode DMStagVecSetValuesStencil(DM dm, Vec vec, PetscInt n, const DMStagStencil *pos, const PetscScalar *val, InsertMode insertMode)
{
  DM_Stag *const stag = (DM_Stag *)dm->data;
  PetscInt       nLocal;
  PetscInt      *ix;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(dm, DM_CLASSID, 1, DMSTAG);
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 2);
  PetscCall(VecGetLocalSize(vec, &nLocal));
  PetscCheck(nLocal == stag->entries, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONG, "Provided vec has a different number of local entries (%" PetscInt_FMT ") than expected (%" PetscInt_FMT "). It should be a global vector", nLocal, stag->entries);
  PetscCall(PetscMalloc1(n, &ix));
  PetscCall(DMStagStencilToIndexLocal(dm, dm->dim, n, pos, ix));
  PetscCall(VecSetValuesLocal(vec, n, ix, val, insertMode));
  PetscCall(PetscFree(ix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

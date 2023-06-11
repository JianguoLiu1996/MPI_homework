#include <petsc/private/dmmbimpl.h> /*I  "petscdmmoab.h"   I*/

#include <petscdmmoab.h>

/*@C
  DMMoabSetFieldVector - Sets the vector reference that represents the solution associated
  with a particular field component.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. ifield - the index of the field as set before via DMMoabSetFieldName.
- fvec - the Vector solution corresponding to the field (component)

  Level: intermediate

.seealso: `DMMoabGetFieldName()`, `DMMoabSetGlobalFieldVector()`
@*/
PetscErrorCode DMMoabSetFieldVector(DM dm, PetscInt ifield, Vec fvec)
{
  DM_Moab           *dmmoab;
  moab::Tag          vtag, ntag;
  const PetscScalar *varray;
  PetscScalar       *farray;
  moab::ErrorCode    merr;
  std::string        tag_name;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  dmmoab = (DM_Moab *)(dm)->data;

  PetscCheck(!(ifield < 0) && !(ifield >= dmmoab->numFields), PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "The field %d should be positive and less than %d.", ifield, dmmoab->numFields);

  /* Create a tag in MOAB mesh to index and keep track of number of Petsc vec tags */
  merr = dmmoab->mbiface->tag_get_handle(dmmoab->fieldNames[ifield], 1, moab::MB_TYPE_DOUBLE, ntag, moab::MB_TAG_DENSE | moab::MB_TAG_CREAT);
  MBERRNM(merr);

  PetscCall(DMMoabGetVecTag(fvec, &vtag));

  merr = dmmoab->mbiface->tag_get_name(vtag, tag_name);
  if (!tag_name.length() && merr != moab::MB_SUCCESS) {
    PetscCall(VecGetArrayRead(fvec, &varray));
    /* use the entity handle and the Dof index to set the right value */
    merr = dmmoab->mbiface->tag_set_data(ntag, *dmmoab->vowned, (const void *)varray);
    MBERRNM(merr);
    PetscCall(VecRestoreArrayRead(fvec, &varray));
  } else {
    PetscCall(PetscMalloc1(dmmoab->nloc, &farray));
    /* we are using a MOAB Vec - directly copy the tag data to new one */
    merr = dmmoab->mbiface->tag_get_data(vtag, *dmmoab->vowned, (void *)farray);
    MBERRNM(merr);
    merr = dmmoab->mbiface->tag_set_data(ntag, *dmmoab->vowned, (const void *)farray);
    MBERRNM(merr);
    /* make sure the parallel exchange for ghosts are done appropriately */
    PetscCall(PetscFree(farray));
  }
#ifdef MOAB_HAVE_MPI
  merr = dmmoab->pcomm->exchange_tags(ntag, *dmmoab->vowned);
  MBERRNM(merr);
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabSetGlobalFieldVector - Sets the vector reference that represents the global solution associated
  with all fields (components) managed by DM.
  A useful utility when updating the DM solution after a solve, to be serialized with the mesh for
  checkpointing purposes.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
- fvec - the global Vector solution corresponding to all the fields managed by DM

  Level: intermediate

.seealso: `DMMoabGetFieldName()`, `DMMoabSetFieldVector()`
@*/
PetscErrorCode DMMoabSetGlobalFieldVector(DM dm, Vec fvec)
{
  DM_Moab              *dmmoab;
  moab::Tag             vtag, ntag;
  const PetscScalar    *rarray;
  PetscScalar          *varray, *farray;
  moab::ErrorCode       merr;
  PetscInt              i, ifield;
  std::string           tag_name;
  moab::Range::iterator iter;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  dmmoab = (DM_Moab *)(dm)->data;

  /* get the Tag corresponding to the global vector - possible that there is no tag associated.. */
  PetscCall(DMMoabGetVecTag(fvec, &vtag));
  merr = dmmoab->mbiface->tag_get_name(vtag, tag_name);
  PetscCall(PetscMalloc1(dmmoab->nloc, &farray));
  if (!tag_name.length() && merr != moab::MB_SUCCESS) {
    /* not a MOAB vector - use VecGetSubVector to get the parts as needed */
    PetscCall(VecGetArrayRead(fvec, &rarray));
    for (ifield = 0; ifield < dmmoab->numFields; ++ifield) {
      /* Create a tag in MOAB mesh to index and keep track of number of Petsc vec tags */
      merr = dmmoab->mbiface->tag_get_handle(dmmoab->fieldNames[ifield], 1, moab::MB_TYPE_DOUBLE, ntag, moab::MB_TAG_DENSE | moab::MB_TAG_CREAT);
      MBERRNM(merr);

      for (i = 0; i < dmmoab->nloc; i++) farray[i] = (dmmoab->bs == 1 ? rarray[ifield * dmmoab->nloc + i] : rarray[i * dmmoab->numFields + ifield]);

      /* use the entity handle and the Dof index to set the right value */
      merr = dmmoab->mbiface->tag_set_data(ntag, *dmmoab->vowned, (const void *)farray);
      MBERRNM(merr);
    }
    PetscCall(VecRestoreArrayRead(fvec, &rarray));
  } else {
    PetscCall(PetscMalloc1(dmmoab->nloc * dmmoab->numFields, &varray));

    /* we are using a MOAB Vec - directly copy the tag data to new one */
    merr = dmmoab->mbiface->tag_get_data(vtag, *dmmoab->vowned, (void *)varray);
    MBERRNM(merr);
    for (ifield = 0; ifield < dmmoab->numFields; ++ifield) {
      /* Create a tag in MOAB mesh to index and keep track of number of Petsc vec tags */
      merr = dmmoab->mbiface->tag_get_handle(dmmoab->fieldNames[ifield], 1, moab::MB_TYPE_DOUBLE, ntag, moab::MB_TAG_DENSE | moab::MB_TAG_CREAT);
      MBERRNM(merr);

      /* we are using a MOAB Vec - directly copy the tag data to new one */
      for (i = 0; i < dmmoab->nloc; i++) farray[i] = (dmmoab->bs == 1 ? varray[ifield * dmmoab->nloc + i] : varray[i * dmmoab->numFields + ifield]);

      merr = dmmoab->mbiface->tag_set_data(ntag, *dmmoab->vowned, (const void *)farray);
      MBERRNM(merr);

#ifdef MOAB_HAVE_MPI
      /* make sure the parallel exchange for ghosts are done appropriately */
      merr = dmmoab->pcomm->exchange_tags(ntag, *dmmoab->vlocal);
      MBERRNM(merr);
#endif
    }
    PetscCall(PetscFree(varray));
  }
  PetscCall(PetscFree(farray));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabSetFieldNames - Sets the number of fields and their names to be managed by the DM

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. numFields - the total number of fields
- fields - the array containing the names of each field (component); Can be NULL.

  Level: intermediate

.seealso: `DMMoabGetFieldName()`, `DMMoabSetFieldName()`
@*/
PetscErrorCode DMMoabSetFieldNames(DM dm, PetscInt numFields, const char *fields[])
{
  PetscInt i;
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  dmmoab = (DM_Moab *)(dm)->data;

  /* first deallocate the existing field structure */
  if (dmmoab->fieldNames) {
    for (i = 0; i < dmmoab->numFields; i++) PetscCall(PetscFree(dmmoab->fieldNames[i]));
    PetscCall(PetscFree(dmmoab->fieldNames));
  }

  /* now re-allocate and assign field names  */
  dmmoab->numFields = numFields;
  PetscCall(PetscMalloc1(numFields, &dmmoab->fieldNames));
  if (fields) {
    for (i = 0; i < dmmoab->numFields; i++) PetscCall(PetscStrallocpy(fields[i], (char **)&dmmoab->fieldNames[i]));
  }
  PetscCall(DMSetNumFields(dm, numFields));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetFieldName - Gets the names of individual field components in multicomponent
  vectors associated with a DMDA.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
- field - field number for the DMMoab (0, 1, ... dof-1), where dof indicates the
        number of degrees of freedom per node within the DMMoab

  Output Parameter:
. fieldName - the name of the field (component)

  Level: intermediate

.seealso: `DMMoabSetFieldName()`, `DMMoabSetFields()`
@*/
PetscErrorCode DMMoabGetFieldName(DM dm, PetscInt field, const char **fieldName)
{
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  dmmoab = (DM_Moab *)(dm)->data;
  PetscCheck(!(field < 0) && !(field >= dmmoab->numFields), PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "DM field %d should be in [%d, %d)", field, 0, dmmoab->numFields);

  *fieldName = dmmoab->fieldNames[field];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabSetFieldName - Sets the name of a field (component) managed by the DM

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. field - the field number
- fieldName - the field (component) name

  Level: intermediate
  Notes:
    Can only be called after DMMoabSetFields supplied with correct numFields

.seealso: `DMMoabGetFieldName()`, `DMMoabSetFields()`
@*/
PetscErrorCode DMMoabSetFieldName(DM dm, PetscInt field, const char *fieldName)
{
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(fieldName, 3);

  dmmoab = (DM_Moab *)(dm)->data;
  PetscCheck(!(field < 0) && !(field >= dmmoab->numFields), PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "DM field %d should be in [%d, %d)", field, 0, dmmoab->numFields);

  if (dmmoab->fieldNames[field]) PetscCall(PetscFree(dmmoab->fieldNames[field]));
  PetscCall(PetscStrallocpy(fieldName, (char **)&dmmoab->fieldNames[field]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetFieldDof - Gets the global degree-of-freedom of a field (component) defined on a
  particular MOAB EntityHandle.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. point - the MOAB EntityHandle container which holds the field degree-of-freedom values
- field - the field (component) index

  Output Parameter:
. dof - the global degree-of-freedom index corresponding to the field in the discrete representation (Vec, Mat)

  Level: beginner

.seealso: `DMMoabGetFieldDofs()`, `DMMoabGetFieldDofsLocal()`
@*/
PetscErrorCode DMMoabGetFieldDof(DM dm, moab::EntityHandle point, PetscInt field, PetscInt *dof)
{
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  dmmoab = (DM_Moab *)(dm)->data;

  *dof = (dmmoab->bs == 1 ? dmmoab->gidmap[dmmoab->mbiface->id_from_handle(point) - dmmoab->seqstart] + field * dmmoab->n : dmmoab->gidmap[dmmoab->mbiface->id_from_handle(point) - dmmoab->seqstart] * dmmoab->numFields + field);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetFieldDofs - Gets the global degree-of-freedom of a field (component) defined on an
  array of MOAB EntityHandles.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. npoints - the total number of Entities in the points array
. points - the MOAB EntityHandle container array which holds the field degree-of-freedom values
- field - the field (component) index

  Output Parameter:
. dof - the global degree-of-freedom index array corresponding to the field in the discrete representation (Vec, Mat)

  Level: intermediate

.seealso: `DMMoabGetFieldDof()`, `DMMoabGetFieldDofsLocal()`
@*/
PetscErrorCode DMMoabGetFieldDofs(DM dm, PetscInt npoints, const moab::EntityHandle *points, PetscInt field, PetscInt *dof)
{
  PetscInt i;
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(points, 3);
  dmmoab = (DM_Moab *)(dm)->data;

  if (!dof) PetscCall(PetscMalloc1(npoints, &dof));

  /* compute the DOF based on local blocking in the fields */
  /* We also assume all fields have equal distribution; i.e., all fields are either defined on vertices or elements and not on a mixture */
  /* TODO: eliminate the limitation using PetscSection to manage DOFs */
  for (i = 0; i < npoints; ++i)
    dof[i] = (dmmoab->bs == 1 ? dmmoab->gidmap[dmmoab->mbiface->id_from_handle(points[i]) - dmmoab->seqstart] + field * dmmoab->n : dmmoab->gidmap[dmmoab->mbiface->id_from_handle(points[i]) - dmmoab->seqstart] * dmmoab->numFields + field);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetFieldDofsLocal - Gets the local degrees-of-freedom of a field (component) defined on an
  array of MOAB EntityHandles.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. npoints - the total number of Entities in the points array
. points - the MOAB EntityHandle container array which holds the field degree-of-freedom values
- field - the field (component) index

  Output Parameter:
. dof - the local degree-of-freedom index array corresponding to the field in the discrete representation (Vec, Mat)

  Level: intermediate

.seealso: `DMMoabGetFieldDof()`, `DMMoabGetFieldDofs()`
@*/
PetscErrorCode DMMoabGetFieldDofsLocal(DM dm, PetscInt npoints, const moab::EntityHandle *points, PetscInt field, PetscInt *dof)
{
  PetscInt i;
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(points, 3);
  dmmoab = (DM_Moab *)(dm)->data;

  if (!dof) PetscCall(PetscMalloc1(npoints, &dof));

  /* compute the DOF based on local blocking in the fields */
  /* assume all fields have equal distribution; i.e., all fields are either defined on vertices or elements and not on a mixture */
  /* TODO: eliminate the limitation using PetscSection to manage DOFs */
  for (i = 0; i < npoints; ++i) {
    dof[i] = (dmmoab->bs > 1 ? dmmoab->lidmap[dmmoab->mbiface->id_from_handle(points[i]) - dmmoab->seqstart] * dmmoab->numFields + field : dmmoab->lidmap[dmmoab->mbiface->id_from_handle(points[i]) - dmmoab->seqstart] + field * dmmoab->n);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetDofs - Gets the global degree-of-freedom for all fields (components) defined on an
  array of MOAB EntityHandles.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. npoints - the total number of Entities in the points array
- points - the MOAB EntityHandle container array which holds the field degree-of-freedom values

  Output Parameter:
. dof - the global degree-of-freedom index array corresponding to the field in the discrete representation (Vec, Mat)

  Level: intermediate

.seealso: `DMMoabGetFieldDofs()`, `DMMoabGetDofsLocal()`, `DMMoabGetDofsBlocked()`
@*/
PetscErrorCode DMMoabGetDofs(DM dm, PetscInt npoints, const moab::EntityHandle *points, PetscInt *dof)
{
  PetscInt i, field, offset;
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(points, 3);
  dmmoab = (DM_Moab *)(dm)->data;

  if (!dof) PetscCall(PetscMalloc1(dmmoab->numFields * npoints, &dof));

  /* compute the DOF based on local blocking in the fields */
  /* assume all fields have equal distribution; i.e., all fields are either defined on vertices or elements and not on a mixture */
  /* TODO: eliminate the limitation using PetscSection to manage DOFs */
  for (field = 0; field < dmmoab->numFields; ++field) {
    offset = field * dmmoab->n;
    for (i = 0; i < npoints; ++i)
      dof[i * dmmoab->numFields + field] = (dmmoab->bs > 1 ? dmmoab->gidmap[dmmoab->mbiface->id_from_handle(points[i]) - dmmoab->seqstart] * dmmoab->numFields + field : dmmoab->gidmap[dmmoab->mbiface->id_from_handle(points[i]) - dmmoab->seqstart] + offset);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetDofsLocal - Gets the local degree-of-freedom for all fields (components) defined on an
  array of MOAB EntityHandles.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. npoints - the total number of Entities in the points array
- points - the MOAB EntityHandle container array which holds the field degree-of-freedom values

  Output Parameter:
. dof - the local degree-of-freedom index array corresponding to the field in the discrete representation (Vec, Mat)

  Level: intermediate

.seealso: `DMMoabGetFieldDofs()`, `DMMoabGetDofs()`, `DMMoabGetDofsBlocked()`
@*/
PetscErrorCode DMMoabGetDofsLocal(DM dm, PetscInt npoints, const moab::EntityHandle *points, PetscInt *dof)
{
  PetscInt i, field, offset;
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(points, 3);
  dmmoab = (DM_Moab *)(dm)->data;

  if (!dof) PetscCall(PetscMalloc1(dmmoab->numFields * npoints, &dof));

  /* compute the DOF based on local blocking in the fields */
  /* assume all fields have equal distribution; i.e., all fields are either defined on vertices or elements and not on a mixture */
  /* TODO: eliminate the limitation using PetscSection to manage DOFs */
  for (field = 0; field < dmmoab->numFields; ++field) {
    offset = field * dmmoab->n;
    for (i = 0; i < npoints; ++i)
      dof[i * dmmoab->numFields + field] = (dmmoab->bs > 1 ? dmmoab->lidmap[dmmoab->mbiface->id_from_handle(points[i]) - dmmoab->seqstart] * dmmoab->numFields + field : dmmoab->lidmap[dmmoab->mbiface->id_from_handle(points[i]) - dmmoab->seqstart] + offset);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetDofsBlocked - Gets the global degree-of-freedom for the first field (component) defined on an
  array of MOAB EntityHandles. It is useful when performing Blocked(Get/Set) methods in computation
  of element residuals and assembly of the discrete systems when all fields are co-located.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. npoints - the total number of Entities in the points array
- points - the MOAB EntityHandle container array which holds the field degree-of-freedom values

  Output Parameter:
. dof - the blocked global degree-of-freedom index array in the discrete representation (Vec, Mat)

  Level: intermediate

.seealso: `DMMoabGetDofsLocal()`, `DMMoabGetDofs()`, `DMMoabGetDofsBlockedLocal()`
@*/
PetscErrorCode DMMoabGetDofsBlocked(DM dm, PetscInt npoints, const moab::EntityHandle *points, PetscInt *dof)
{
  PetscInt i;
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(points, 3);
  dmmoab = (DM_Moab *)(dm)->data;

  if (!dof) PetscCall(PetscMalloc1(npoints, &dof));

  for (i = 0; i < npoints; ++i) dof[i] = dmmoab->gidmap[(PetscInt)points[i] - dmmoab->seqstart];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetDofsBlockedLocal - Gets the local degree-of-freedom for the first field (component) defined on an
  array of MOAB EntityHandles. It is useful when performing local Blocked(Get/Set) methods in computation
  of element residuals and assembly of the discrete systems when all fields are co-located.

  Not Collective

  Input Parameters:
+ dm     - the discretization manager object
. npoints - the total number of Entities in the points array
- points - the MOAB EntityHandle container array which holds the field degree-of-freedom values

  Output Parameter:
. dof - the blocked local degree-of-freedom index array in the discrete representation (Vec, Mat)

  Level: intermediate

.seealso: `DMMoabGetDofsLocal()`, `DMMoabGetDofs()`, `DMMoabGetDofsBlockedLocal()`
@*/
PetscErrorCode DMMoabGetDofsBlockedLocal(DM dm, PetscInt npoints, const moab::EntityHandle *points, PetscInt *dof)
{
  PetscInt i;
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(points, 3);
  dmmoab = (DM_Moab *)(dm)->data;

  if (!dof) PetscCall(PetscMalloc1(npoints, &dof));

  for (i = 0; i < npoints; ++i) dof[i] = dmmoab->lidmap[(PetscInt)points[i] - dmmoab->seqstart];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetVertexDofsBlocked - Gets the global degree-of-freedom for the first field (component) defined on an
  array of locally owned MOAB mesh vertices. It's utility is when performing Finite-Difference type calculations
  where vertex traversal is faster than element-wise assembly that is typically done in FEM calculations.

  Not Collective

  Input Parameters:
. dm     - the discretization manager object

  Output Parameter:
. dof - the blocked global degree-of-freedom index array in the discrete representation (Vec, Mat) that is vertex-based based on local numbering

  Level: intermediate

.seealso: `DMMoabGetVertexDofsBlockedLocal()`, `DMMoabGetDofsBlocked()`, `DMMoabGetDofsBlockedLocal()`
@*/
PetscErrorCode DMMoabGetVertexDofsBlocked(DM dm, PetscInt **dof)
{
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  dmmoab = (DM_Moab *)(dm)->data;

  *dof = dmmoab->gidmap;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMMoabGetVertexDofsBlockedLocal - Gets the local degree-of-freedom for the first field (component) defined on an
  array of locally owned MOAB mesh vertices. It's utility is when performing Finite-Difference type calculations
  where vertex traversal is faster than element-wise assembly that is typically done in FEM calculations.

  Not Collective

  Input Parameters:
. dm     - the discretization manager object

  Output Parameter:
. dof - the blocked local degree-of-freedom index array in the discrete representation (Vec, Mat) that is vertex-based based on local numbering

  Level: intermediate

.seealso: `DMMoabGetVertexDofsBlocked()`, `DMMoabGetDofsBlocked()`, `DMMoabGetDofsBlockedLocal()`
@*/
PetscErrorCode DMMoabGetVertexDofsBlockedLocal(DM dm, PetscInt **dof)
{
  DM_Moab *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(dof, 2);
  dmmoab = (DM_Moab *)(dm)->data;

  *dof = dmmoab->lidmap;
  PetscFunctionReturn(PETSC_SUCCESS);
}

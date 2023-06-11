#include "../src/dm/impls/swarm/data_bucket.h"

/* string helpers */
PetscErrorCode DMSwarmDataFieldStringInList(const char name[], const PetscInt N, const DMSwarmDataField gfield[], PetscBool *val)
{
  PetscInt i;

  PetscFunctionBegin;
  *val = PETSC_FALSE;
  for (i = 0; i < N; ++i) {
    PetscBool flg;
    PetscCall(PetscStrcmp(name, gfield[i]->name, &flg));
    if (flg) {
      *val = PETSC_TRUE;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldStringFindInList(const char name[], const PetscInt N, const DMSwarmDataField gfield[], PetscInt *index)
{
  PetscInt i;

  PetscFunctionBegin;
  *index = -1;
  for (i = 0; i < N; ++i) {
    PetscBool flg;
    PetscCall(PetscStrcmp(name, gfield[i]->name, &flg));
    if (flg) {
      *index = i;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldCreate(const char registration_function[], const char name[], const size_t size, const PetscInt L, DMSwarmDataField *DF)
{
  DMSwarmDataField df;

  PetscFunctionBegin;
  PetscCall(PetscNew(&df));
  PetscCall(PetscStrallocpy(registration_function, &df->registration_function));
  PetscCall(PetscStrallocpy(name, &df->name));
  df->atomic_size = size;
  df->L           = L;
  df->bs          = 1;
  /* allocate something so we don't have to reallocate */
  PetscCall(PetscMalloc(size * L, &df->data));
  PetscCall(PetscMemzero(df->data, size * L));
  *DF = df;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldDestroy(DMSwarmDataField *DF)
{
  DMSwarmDataField df = *DF;

  PetscFunctionBegin;
  PetscCall(PetscFree(df->registration_function));
  PetscCall(PetscFree(df->name));
  PetscCall(PetscFree(df->data));
  PetscCall(PetscFree(df));
  *DF = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* data bucket */
PetscErrorCode DMSwarmDataBucketCreate(DMSwarmDataBucket *DB)
{
  DMSwarmDataBucket db;

  PetscFunctionBegin;
  PetscCall(PetscNew(&db));

  db->finalised = PETSC_FALSE;
  /* create empty spaces for fields */
  db->L         = -1;
  db->buffer    = 1;
  db->allocated = 1;
  db->nfields   = 0;
  PetscCall(PetscMalloc1(1, &db->field));
  *DB = db;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketDestroy(DMSwarmDataBucket *DB)
{
  DMSwarmDataBucket db = *DB;
  PetscInt          f;

  PetscFunctionBegin;
  /* release fields */
  for (f = 0; f < db->nfields; ++f) PetscCall(DMSwarmDataFieldDestroy(&db->field[f]));
  /* this will catch the initially allocated objects in the event that no fields are registered */
  if (db->field != NULL) PetscCall(PetscFree(db->field));
  PetscCall(PetscFree(db));
  *DB = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketQueryForActiveFields(DMSwarmDataBucket db, PetscBool *any_active_fields)
{
  PetscInt f;

  PetscFunctionBegin;
  *any_active_fields = PETSC_FALSE;
  for (f = 0; f < db->nfields; ++f) {
    if (db->field[f]->active) {
      *any_active_fields = PETSC_TRUE;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketRegisterField(DMSwarmDataBucket db, const char registration_function[], const char field_name[], size_t atomic_size, DMSwarmDataField *_gfield)
{
  PetscBool        val;
  DMSwarmDataField fp;

  PetscFunctionBegin;
  /* check we haven't finalised the registration of fields */
  /*
   if (db->finalised==PETSC_TRUE) {
   printf("ERROR: DMSwarmDataBucketFinalize() has been called. Cannot register more fields\n");
   ERROR();
   }
  */
  /* check for repeated name */
  PetscCall(DMSwarmDataFieldStringInList(field_name, db->nfields, (const DMSwarmDataField *)db->field, &val));
  PetscCheck(val != PETSC_TRUE, PETSC_COMM_SELF, PETSC_ERR_USER, "Field %s already exists. Cannot add same field twice", field_name);
  /* create new space for data */
  PetscCall(PetscRealloc(sizeof(DMSwarmDataField) * (db->nfields + 1), &db->field));
  /* add field */
  PetscCall(DMSwarmDataFieldCreate(registration_function, field_name, atomic_size, db->allocated, &fp));
  db->field[db->nfields] = fp;
  db->nfields++;
  if (_gfield != NULL) *_gfield = fp;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
 #define DMSwarmDataBucketRegisterField(db,name,size,k) {\
 char *location;\
 asprintf(&location,"Registered by %s() at line %d within file %s", __FUNCTION__, __LINE__, __FILE__);\
 _DMSwarmDataBucketRegisterField( (db), location, (name), (size), (k));\
 ierr = PetscFree(location);\
 }
 */

PetscErrorCode DMSwarmDataBucketGetDMSwarmDataFieldIdByName(DMSwarmDataBucket db, const char name[], PetscInt *idx)
{
  PetscBool found;

  PetscFunctionBegin;
  *idx = -1;
  PetscCall(DMSwarmDataFieldStringInList(name, db->nfields, (const DMSwarmDataField *)db->field, &found));
  PetscCheck(found, PETSC_COMM_SELF, PETSC_ERR_USER, "Cannot find DMSwarmDataField with name %s", name);
  PetscCall(DMSwarmDataFieldStringFindInList(name, db->nfields, (const DMSwarmDataField *)db->field, idx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketGetDMSwarmDataFieldByName(DMSwarmDataBucket db, const char name[], DMSwarmDataField *gfield)
{
  PetscInt  idx;
  PetscBool found;

  PetscFunctionBegin;
  PetscCall(DMSwarmDataFieldStringInList(name, db->nfields, (const DMSwarmDataField *)db->field, &found));
  PetscCheck(found, PETSC_COMM_SELF, PETSC_ERR_USER, "Cannot find DMSwarmDataField with name %s", name);
  PetscCall(DMSwarmDataFieldStringFindInList(name, db->nfields, (const DMSwarmDataField *)db->field, &idx));
  *gfield = db->field[idx];
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketQueryDMSwarmDataFieldByName(DMSwarmDataBucket db, const char name[], PetscBool *found)
{
  PetscFunctionBegin;
  *found = PETSC_FALSE;
  PetscCall(DMSwarmDataFieldStringInList(name, db->nfields, (const DMSwarmDataField *)db->field, found));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketFinalize(DMSwarmDataBucket db)
{
  PetscFunctionBegin;
  db->finalised = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldGetNumEntries(DMSwarmDataField df, PetscInt *sum)
{
  PetscFunctionBegin;
  *sum = df->L;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldSetBlockSize(DMSwarmDataField df, PetscInt blocksize)
{
  PetscFunctionBegin;
  df->bs = blocksize;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldSetSize(DMSwarmDataField df, const PetscInt new_L)
{
  PetscFunctionBegin;
  PetscCheck(new_L >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "Cannot set size of DMSwarmDataField to be < 0");
  if (new_L == df->L) PetscFunctionReturn(PETSC_SUCCESS);
  if (new_L > df->L) {
    PetscCall(PetscRealloc(df->atomic_size * (new_L), &df->data));
    /* init new contents */
    PetscCall(PetscMemzero((((char *)df->data) + df->L * df->atomic_size), (new_L - df->L) * df->atomic_size));
  } else {
    /* reallocate pointer list, add +1 in case new_L = 0 */
    PetscCall(PetscRealloc(df->atomic_size * (new_L + 1), &df->data));
  }
  df->L = new_L;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldZeroBlock(DMSwarmDataField df, const PetscInt start, const PetscInt end)
{
  PetscFunctionBegin;
  PetscCheck(start <= end, PETSC_COMM_SELF, PETSC_ERR_USER, "Cannot zero a block of entries if start(%" PetscInt_FMT ") > end(%" PetscInt_FMT ")", start, end);
  PetscCheck(start >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "Cannot zero a block of entries if start(%" PetscInt_FMT ") < 0", start);
  PetscCheck(end <= df->L, PETSC_COMM_SELF, PETSC_ERR_USER, "Cannot zero a block of entries if end(%" PetscInt_FMT ") >= array size(%" PetscInt_FMT ")", end, df->L);
  PetscCall(PetscMemzero((((char *)df->data) + start * df->atomic_size), (end - start) * df->atomic_size));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
 A negative buffer value will simply be ignored and the old buffer value will be used.
 */
PetscErrorCode DMSwarmDataBucketSetSizes(DMSwarmDataBucket db, const PetscInt L, const PetscInt buffer)
{
  PetscInt  current_allocated, new_used, new_unused, new_buffer, new_allocated, f;
  PetscBool any_active_fields;

  PetscFunctionBegin;
  PetscCheck(db->finalised != PETSC_FALSE, PETSC_COMM_SELF, PETSC_ERR_USER, "You must call DMSwarmDataBucketFinalize() before DMSwarmDataBucketSetSizes()");
  PetscCall(DMSwarmDataBucketQueryForActiveFields(db, &any_active_fields));
  PetscCheck(!any_active_fields, PETSC_COMM_SELF, PETSC_ERR_USER, "Cannot safely re-size as at least one DMSwarmDataField is currently being accessed");

  current_allocated = db->allocated;
  new_used          = L;
  new_unused        = current_allocated - new_used;
  new_buffer        = db->buffer;
  if (buffer >= 0) { /* update the buffer value */
    new_buffer = buffer;
  }
  new_allocated = new_used + new_buffer;
  /* action */
  if (new_allocated > current_allocated) {
    /* increase size to new_used + new_buffer */
    for (f = 0; f < db->nfields; f++) PetscCall(DMSwarmDataFieldSetSize(db->field[f], new_allocated));
    db->L         = new_used;
    db->buffer    = new_buffer;
    db->allocated = new_used + new_buffer;
  } else {
    if (new_unused > 2 * new_buffer) {
      /* shrink array to new_used + new_buffer */
      for (f = 0; f < db->nfields; ++f) PetscCall(DMSwarmDataFieldSetSize(db->field[f], new_allocated));
      db->L         = new_used;
      db->buffer    = new_buffer;
      db->allocated = new_used + new_buffer;
    } else {
      db->L      = new_used;
      db->buffer = new_buffer;
    }
  }
  /* zero all entries from db->L to db->allocated */
  for (f = 0; f < db->nfields; ++f) {
    DMSwarmDataField field = db->field[f];
    PetscCall(DMSwarmDataFieldZeroBlock(field, db->L, db->allocated));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketSetInitialSizes(DMSwarmDataBucket db, const PetscInt L, const PetscInt buffer)
{
  PetscInt f;

  PetscFunctionBegin;
  PetscCall(DMSwarmDataBucketSetSizes(db, L, buffer));
  for (f = 0; f < db->nfields; ++f) {
    DMSwarmDataField field = db->field[f];
    PetscCall(DMSwarmDataFieldZeroBlock(field, 0, db->allocated));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketGetSizes(DMSwarmDataBucket db, PetscInt *L, PetscInt *buffer, PetscInt *allocated)
{
  PetscFunctionBegin;
  if (L) *L = db->L;
  if (buffer) *buffer = db->buffer;
  if (allocated) *allocated = db->allocated;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketGetGlobalSizes(MPI_Comm comm, DMSwarmDataBucket db, PetscInt *L, PetscInt *buffer, PetscInt *allocated)
{
  PetscFunctionBegin;
  if (L) PetscCall(MPIU_Allreduce(&db->L, L, 1, MPIU_INT, MPI_SUM, comm));
  if (buffer) PetscCall(MPIU_Allreduce(&db->buffer, buffer, 1, MPIU_INT, MPI_SUM, comm));
  if (allocated) PetscCall(MPIU_Allreduce(&db->allocated, allocated, 1, MPIU_INT, MPI_SUM, comm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketGetDMSwarmDataFields(DMSwarmDataBucket db, PetscInt *L, DMSwarmDataField *fields[])
{
  PetscFunctionBegin;
  if (L) *L = db->nfields;
  if (fields) *fields = db->field;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldGetAccess(const DMSwarmDataField gfield)
{
  PetscFunctionBegin;
  PetscCheck(!gfield->active, PETSC_COMM_SELF, PETSC_ERR_USER, "Field \"%s\" is already active. You must call DMSwarmDataFieldRestoreAccess()", gfield->name);
  gfield->active = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldAccessPoint(const DMSwarmDataField gfield, const PetscInt pid, void **ctx_p)
{
  PetscFunctionBegin;
  *ctx_p = NULL;
#if defined(DMSWARM_DATAFIELD_POINT_ACCESS_GUARD)
  /* debug mode */
  /* check point is valid */
  PetscCheck(pid >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be >= 0");
  PetscCheck(pid < gfield->L, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be < %" PetscInt_FMT, gfield->L);
  PetscCheck(gfield->active != PETSC_FALSE, PETSC_COMM_SELF, PETSC_ERR_USER, "Field \"%s\" is not active. You must call DMSwarmDataFieldGetAccess() before point data can be retrivied", gfield->name);
#endif
  *ctx_p = DMSWARM_DATAFIELD_point_access(gfield->data, pid, gfield->atomic_size);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldAccessPointOffset(const DMSwarmDataField gfield, const size_t offset, const PetscInt pid, void **ctx_p)
{
  PetscFunctionBegin;
#if defined(DMSWARM_DATAFIELD_POINT_ACCESS_GUARD)
  /* debug mode */
  /* check point is valid */
  /* PetscCheck(offset >= 0,PETSC_COMM_SELF,PETSC_ERR_USER,"offset must be >= 0");*/
  /* Note compiler realizes this can never happen with an unsigned PetscInt */
  PetscCheck(offset < gfield->atomic_size, PETSC_COMM_SELF, PETSC_ERR_USER, "offset must be < %zu", gfield->atomic_size);
  /* check point is valid */
  PetscCheck(pid >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be >= 0");
  PetscCheck(pid < gfield->L, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be < %" PetscInt_FMT, gfield->L);
  PetscCheck(gfield->active != PETSC_FALSE, PETSC_COMM_SELF, PETSC_ERR_USER, "Field \"%s\" is not active. You must call DMSwarmDataFieldGetAccess() before point data can be retrivied", gfield->name);
#endif
  *ctx_p = DMSWARM_DATAFIELD_point_access_offset(gfield->data, pid, gfield->atomic_size, offset);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldRestoreAccess(DMSwarmDataField gfield)
{
  PetscFunctionBegin;
  PetscCheck(gfield->active != PETSC_FALSE, PETSC_COMM_SELF, PETSC_ERR_USER, "Field \"%s\" is not active. You must call DMSwarmDataFieldGetAccess()", gfield->name);
  gfield->active = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldVerifyAccess(const DMSwarmDataField gfield, const size_t size)
{
  PetscFunctionBegin;
#if defined(DMSWARM_DATAFIELD_POINT_ACCESS_GUARD)
  PetscCheck(gfield->atomic_size == size, PETSC_COMM_SELF, PETSC_ERR_USER, "Field \"%s\" must be mapped to %zu bytes, your intended structure is %zu bytes in length.", gfield->name, gfield->atomic_size, size);
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldGetAtomicSize(const DMSwarmDataField gfield, size_t *size)
{
  PetscFunctionBegin;
  if (size) *size = gfield->atomic_size;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldGetEntries(const DMSwarmDataField gfield, void **data)
{
  PetscFunctionBegin;
  if (data) *data = gfield->data;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataFieldRestoreEntries(const DMSwarmDataField gfield, void **data)
{
  PetscFunctionBegin;
  if (data) *data = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* y = x */
PetscErrorCode DMSwarmDataBucketCopyPoint(const DMSwarmDataBucket xb, const PetscInt pid_x, const DMSwarmDataBucket yb, const PetscInt pid_y)
{
  PetscInt f;

  PetscFunctionBegin;
  for (f = 0; f < xb->nfields; ++f) {
    void *dest;
    void *src;

    PetscCall(DMSwarmDataFieldGetAccess(xb->field[f]));
    if (xb != yb) PetscCall(DMSwarmDataFieldGetAccess(yb->field[f]));
    PetscCall(DMSwarmDataFieldAccessPoint(xb->field[f], pid_x, &src));
    PetscCall(DMSwarmDataFieldAccessPoint(yb->field[f], pid_y, &dest));
    PetscCall(PetscMemcpy(dest, src, xb->field[f]->atomic_size));
    PetscCall(DMSwarmDataFieldRestoreAccess(xb->field[f]));
    if (xb != yb) PetscCall(DMSwarmDataFieldRestoreAccess(yb->field[f]));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketCreateFromSubset(DMSwarmDataBucket DBIn, const PetscInt N, const PetscInt list[], DMSwarmDataBucket *DB)
{
  PetscInt          nfields;
  DMSwarmDataField *fields;
  PetscInt          f, L, buffer, allocated, p;

  PetscFunctionBegin;
  PetscCall(DMSwarmDataBucketCreate(DB));
  /* copy contents of DBIn */
  PetscCall(DMSwarmDataBucketGetDMSwarmDataFields(DBIn, &nfields, &fields));
  PetscCall(DMSwarmDataBucketGetSizes(DBIn, &L, &buffer, &allocated));
  for (f = 0; f < nfields; ++f) PetscCall(DMSwarmDataBucketRegisterField(*DB, "DMSwarmDataBucketCreateFromSubset", fields[f]->name, fields[f]->atomic_size, NULL));
  PetscCall(DMSwarmDataBucketFinalize(*DB));
  PetscCall(DMSwarmDataBucketSetSizes(*DB, L, buffer));
  /* now copy the desired guys from DBIn => DB */
  for (p = 0; p < N; ++p) PetscCall(DMSwarmDataBucketCopyPoint(DBIn, list[p], *DB, list[p]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* insert into an existing location */
PetscErrorCode DMSwarmDataFieldInsertPoint(const DMSwarmDataField field, const PetscInt index, const void *ctx)
{
  PetscFunctionBegin;
#if defined(DMSWARM_DATAFIELD_POINT_ACCESS_GUARD)
  /* check point is valid */
  PetscCheck(index >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be >= 0");
  PetscCheck(index < field->L, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be < %" PetscInt_FMT, field->L);
#endif
  PetscCall(PetscMemcpy(DMSWARM_DATAFIELD_point_access(field->data, index, field->atomic_size), ctx, field->atomic_size));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* remove data at index - replace with last point */
PetscErrorCode DMSwarmDataBucketRemovePointAtIndex(const DMSwarmDataBucket db, const PetscInt index)
{
  PetscInt  f;
  PetscBool any_active_fields;

  PetscFunctionBegin;
#if defined(DMSWARM_DATAFIELD_POINT_ACCESS_GUARD)
  /* check point is valid */
  PetscCheck(index >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be >= 0");
  PetscCheck(index < db->allocated, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be < %" PetscInt_FMT, db->L + db->buffer);
#endif
  PetscCall(DMSwarmDataBucketQueryForActiveFields(db, &any_active_fields));
  PetscCheck(!any_active_fields, PETSC_COMM_SELF, PETSC_ERR_USER, "Cannot safely remove point as at least one DMSwarmDataField is currently being accessed");
  if (index >= db->L) { /* this point is not in the list - no need to error, but I will anyway */
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_USER, "You should not be trying to remove point at index=%" PetscInt_FMT " since it's < db->L = %" PetscInt_FMT, index, db->L);
  }
  if (index != db->L - 1) { /* not last point in list */
    for (f = 0; f < db->nfields; ++f) {
      DMSwarmDataField field = db->field[f];

      /* copy then remove */
      PetscCall(DMSwarmDataFieldCopyPoint(db->L - 1, field, index, field));
      /* DMSwarmDataFieldZeroPoint(field,index); */
    }
  }
  /* decrement size */
  /* this will zero out an crap at the end of the list */
  PetscCall(DMSwarmDataBucketRemovePoint(db));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* copy x into y */
PetscErrorCode DMSwarmDataFieldCopyPoint(const PetscInt pid_x, const DMSwarmDataField field_x, const PetscInt pid_y, const DMSwarmDataField field_y)
{
  PetscFunctionBegin;
#if defined(DMSWARM_DATAFIELD_POINT_ACCESS_GUARD)
  /* check point is valid */
  PetscCheck(pid_x >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "(IN) index must be >= 0");
  PetscCheck(pid_x < field_x->L, PETSC_COMM_SELF, PETSC_ERR_USER, "(IN) index must be < %" PetscInt_FMT, field_x->L);
  PetscCheck(pid_y >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "(OUT) index must be >= 0");
  PetscCheck(pid_y < field_y->L, PETSC_COMM_SELF, PETSC_ERR_USER, "(OUT) index must be < %" PetscInt_FMT, field_y->L);
  PetscCheck(field_y->atomic_size == field_x->atomic_size, PETSC_COMM_SELF, PETSC_ERR_USER, "atomic size must match");
#endif
  PetscCall(PetscMemcpy(DMSWARM_DATAFIELD_point_access(field_y->data, pid_y, field_y->atomic_size), DMSWARM_DATAFIELD_point_access(field_x->data, pid_x, field_x->atomic_size), field_y->atomic_size));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* zero only the datafield at this point */
PetscErrorCode DMSwarmDataFieldZeroPoint(const DMSwarmDataField field, const PetscInt index)
{
  PetscFunctionBegin;
#if defined(DMSWARM_DATAFIELD_POINT_ACCESS_GUARD)
  /* check point is valid */
  PetscCheck(index >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be >= 0");
  PetscCheck(index < field->L, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be < %" PetscInt_FMT, field->L);
#endif
  PetscCall(PetscMemzero(DMSWARM_DATAFIELD_point_access(field->data, index, field->atomic_size), field->atomic_size));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* zero ALL data for this point */
PetscErrorCode DMSwarmDataBucketZeroPoint(const DMSwarmDataBucket db, const PetscInt index)
{
  PetscInt f;

  PetscFunctionBegin;
  /* check point is valid */
  PetscCheck(index >= 0, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be >= 0");
  PetscCheck(index < db->allocated, PETSC_COMM_SELF, PETSC_ERR_USER, "index must be < %" PetscInt_FMT, db->allocated);
  for (f = 0; f < db->nfields; ++f) {
    DMSwarmDataField field = db->field[f];
    PetscCall(DMSwarmDataFieldZeroPoint(field, index));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* increment */
PetscErrorCode DMSwarmDataBucketAddPoint(DMSwarmDataBucket db)
{
  PetscFunctionBegin;
  PetscCall(DMSwarmDataBucketSetSizes(db, db->L + 1, DMSWARM_DATA_BUCKET_BUFFER_DEFAULT));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* decrement */
PetscErrorCode DMSwarmDataBucketRemovePoint(DMSwarmDataBucket db)
{
  PetscFunctionBegin;
  PetscCall(DMSwarmDataBucketSetSizes(db, db->L - 1, DMSWARM_DATA_BUCKET_BUFFER_DEFAULT));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*  Should be redone to user PetscViewer */
PetscErrorCode DMSwarmDataBucketView_stdout(MPI_Comm comm, DMSwarmDataBucket db)
{
  PetscInt f;
  double   memory_usage_total, memory_usage_total_local = 0.0;

  PetscFunctionBegin;
  PetscCall(PetscPrintf(comm, "DMSwarmDataBucketView: \n"));
  PetscCall(PetscPrintf(comm, "  L                  = %" PetscInt_FMT " \n", db->L));
  PetscCall(PetscPrintf(comm, "  buffer             = %" PetscInt_FMT " \n", db->buffer));
  PetscCall(PetscPrintf(comm, "  allocated          = %" PetscInt_FMT " \n", db->allocated));
  PetscCall(PetscPrintf(comm, "  nfields registered = %" PetscInt_FMT " \n", db->nfields));

  for (f = 0; f < db->nfields; ++f) {
    double memory_usage_f = (double)(db->field[f]->atomic_size * db->allocated) * 1.0e-6;
    memory_usage_total_local += memory_usage_f;
  }
  PetscCallMPI(MPI_Allreduce(&memory_usage_total_local, &memory_usage_total, 1, MPI_DOUBLE, MPI_SUM, comm));

  for (f = 0; f < db->nfields; ++f) {
    double memory_usage_f = (double)(db->field[f]->atomic_size * db->allocated) * 1.0e-6;
    PetscCall(PetscPrintf(comm, "    [%3" PetscInt_FMT "] %15s : Mem. usage       = %1.2e (MB) [rank0]\n", f, db->field[f]->name, memory_usage_f));
    PetscCall(PetscPrintf(comm, "                            blocksize        = %" PetscInt_FMT " \n", db->field[f]->bs));
    if (db->field[f]->bs != 1) {
      PetscCall(PetscPrintf(comm, "                            atomic size      = %zu [full block, bs=%" PetscInt_FMT "]\n", db->field[f]->atomic_size, db->field[f]->bs));
      PetscCall(PetscPrintf(comm, "                            atomic size/item = %zu \n", (size_t)(db->field[f]->atomic_size / db->field[f]->bs)));
    } else {
      PetscCall(PetscPrintf(comm, "                            atomic size      = %zu \n", db->field[f]->atomic_size));
    }
  }
  PetscCall(PetscPrintf(comm, "  Total mem. usage                           = %1.2e (MB) (collective)\n", memory_usage_total));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketView_Seq(MPI_Comm comm, DMSwarmDataBucket db, const char filename[], DMSwarmDataBucketViewType type)
{
  PetscFunctionBegin;
  switch (type) {
  case DATABUCKET_VIEW_STDOUT:
    PetscCall(DMSwarmDataBucketView_stdout(PETSC_COMM_SELF, db));
    break;
  case DATABUCKET_VIEW_ASCII:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for ascii output");
  case DATABUCKET_VIEW_BINARY:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for binary output");
  case DATABUCKET_VIEW_HDF5:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for HDF5 output");
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Unknown viewer method requested");
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketView_MPI(MPI_Comm comm, DMSwarmDataBucket db, const char filename[], DMSwarmDataBucketViewType type)
{
  PetscFunctionBegin;
  switch (type) {
  case DATABUCKET_VIEW_STDOUT:
    PetscCall(DMSwarmDataBucketView_stdout(comm, db));
    break;
  case DATABUCKET_VIEW_ASCII:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for ascii output");
  case DATABUCKET_VIEW_BINARY:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for binary output");
  case DATABUCKET_VIEW_HDF5:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for HDF5 output");
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Unknown viewer method requested");
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketView(MPI_Comm comm, DMSwarmDataBucket db, const char filename[], DMSwarmDataBucketViewType type)
{
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(comm, &size));
  if (size == 1) {
    PetscCall(DMSwarmDataBucketView_Seq(comm, db, filename, type));
  } else {
    PetscCall(DMSwarmDataBucketView_MPI(comm, db, filename, type));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketDuplicateFields(DMSwarmDataBucket dbA, DMSwarmDataBucket *dbB)
{
  DMSwarmDataBucket db2;
  PetscInt          f;

  PetscFunctionBegin;
  PetscCall(DMSwarmDataBucketCreate(&db2));
  /* copy contents from dbA into db2 */
  for (f = 0; f < dbA->nfields; ++f) {
    DMSwarmDataField field;
    size_t           atomic_size;
    char            *name;

    field       = dbA->field[f];
    atomic_size = field->atomic_size;
    name        = field->name;
    PetscCall(DMSwarmDataBucketRegisterField(db2, "DMSwarmDataBucketDuplicateFields", name, atomic_size, NULL));
  }
  PetscCall(DMSwarmDataBucketFinalize(db2));
  PetscCall(DMSwarmDataBucketSetInitialSizes(db2, 0, 1000));
  *dbB = db2;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
 Insert points from db2 into db1
 db1 <<== db2
 */
PetscErrorCode DMSwarmDataBucketInsertValues(DMSwarmDataBucket db1, DMSwarmDataBucket db2)
{
  PetscInt n_mp_points1, n_mp_points2;
  PetscInt n_mp_points1_new, p;

  PetscFunctionBegin;
  PetscCall(DMSwarmDataBucketGetSizes(db1, &n_mp_points1, NULL, NULL));
  PetscCall(DMSwarmDataBucketGetSizes(db2, &n_mp_points2, NULL, NULL));
  n_mp_points1_new = n_mp_points1 + n_mp_points2;
  PetscCall(DMSwarmDataBucketSetSizes(db1, n_mp_points1_new, DMSWARM_DATA_BUCKET_BUFFER_DEFAULT));
  for (p = 0; p < n_mp_points2; ++p) {
    /* db1 <<== db2 */
    PetscCall(DMSwarmDataBucketCopyPoint(db2, p, db1, (n_mp_points1 + p)));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* helpers for parallel send/recv */
PetscErrorCode DMSwarmDataBucketCreatePackedArray(DMSwarmDataBucket db, size_t *bytes, void **buf)
{
  PetscInt f;
  size_t   sizeof_marker_contents;
  void    *buffer;

  PetscFunctionBegin;
  sizeof_marker_contents = 0;
  for (f = 0; f < db->nfields; ++f) {
    DMSwarmDataField df = db->field[f];
    sizeof_marker_contents += df->atomic_size;
  }
  PetscCall(PetscMalloc(sizeof_marker_contents, &buffer));
  PetscCall(PetscMemzero(buffer, sizeof_marker_contents));
  if (bytes) *bytes = sizeof_marker_contents;
  if (buf) *buf = buffer;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketDestroyPackedArray(DMSwarmDataBucket db, void **buf)
{
  PetscFunctionBegin;
  if (buf) {
    PetscCall(PetscFree(*buf));
    *buf = NULL;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketFillPackedArray(DMSwarmDataBucket db, const PetscInt index, void *buf)
{
  PetscInt f;
  void    *data, *data_p;
  size_t   asize, offset;

  PetscFunctionBegin;
  offset = 0;
  for (f = 0; f < db->nfields; ++f) {
    DMSwarmDataField df = db->field[f];

    asize  = df->atomic_size;
    data   = (void *)(df->data);
    data_p = (void *)((char *)data + index * asize);
    PetscCall(PetscMemcpy((void *)((char *)buf + offset), data_p, asize));
    offset = offset + asize;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataBucketInsertPackedArray(DMSwarmDataBucket db, const PetscInt idx, void *data)
{
  PetscInt f;
  void    *data_p;
  size_t   offset;

  PetscFunctionBegin;
  offset = 0;
  for (f = 0; f < db->nfields; ++f) {
    DMSwarmDataField df = db->field[f];

    data_p = (void *)((char *)data + offset);
    PetscCall(DMSwarmDataFieldInsertPoint(df, idx, (void *)data_p));
    offset = offset + df->atomic_size;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
     Provides the interface functions for vector operations that do NOT have PetscScalar/PetscReal in the signature
   These are the vector functions the user calls.
*/
#include <petsc/private/vecimpl.h> /*I  "petscvec.h"   I*/
#include <petsc/private/deviceimpl.h>

/* Logging support */
PetscClassId  VEC_CLASSID;
PetscLogEvent VEC_View, VEC_Max, VEC_Min, VEC_Dot, VEC_MDot, VEC_TDot;
PetscLogEvent VEC_Norm, VEC_Normalize, VEC_Scale, VEC_Copy, VEC_Set, VEC_AXPY, VEC_AYPX, VEC_WAXPY;
PetscLogEvent VEC_MTDot, VEC_MAXPY, VEC_Swap, VEC_AssemblyBegin, VEC_ScatterBegin, VEC_ScatterEnd;
PetscLogEvent VEC_AssemblyEnd, VEC_PointwiseMult, VEC_SetValues, VEC_Load, VEC_SetPreallocateCOO, VEC_SetValuesCOO;
PetscLogEvent VEC_SetRandom, VEC_ReduceArithmetic, VEC_ReduceCommunication, VEC_ReduceBegin, VEC_ReduceEnd, VEC_Ops;
PetscLogEvent VEC_DotNorm2, VEC_AXPBYPCZ;
PetscLogEvent VEC_ViennaCLCopyFromGPU, VEC_ViennaCLCopyToGPU;
PetscLogEvent VEC_CUDACopyFromGPU, VEC_CUDACopyToGPU;
PetscLogEvent VEC_HIPCopyFromGPU, VEC_HIPCopyToGPU;

/*@
   VecStashGetInfo - Gets how many values are currently in the vector stash, i.e. need
       to be communicated to other processors during the `VecAssemblyBegin()`/`VecAssemblyEnd()` process

    Not Collective

   Input Parameter:
.   vec - the vector

   Output Parameters:
+   nstash   - the size of the stash
.   reallocs - the number of additional mallocs incurred in building the stash
.   bnstash   - the size of the block stash
-   breallocs - the number of additional mallocs incurred in building the block stash (from `VecSetValuesBlocked()`)

   Level: advanced

.seealso: [](chapter_vectors), `Vec`, `VecAssemblyBegin()`, `VecAssemblyEnd()`, `Vec`, `VecStashSetInitialSize()`, `VecStashView()`
@*/
PetscErrorCode VecStashGetInfo(Vec vec, PetscInt *nstash, PetscInt *reallocs, PetscInt *bnstash, PetscInt *breallocs)
{
  PetscFunctionBegin;
  PetscCall(VecStashGetInfo_Private(&vec->stash, nstash, reallocs));
  PetscCall(VecStashGetInfo_Private(&vec->bstash, bnstash, breallocs));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetLocalToGlobalMapping - Sets a local numbering to global numbering used
   by the routine `VecSetValuesLocal()` to allow users to insert vector entries
   using a local (per-processor) numbering.

   Logically Collective

   Input Parameters:
+  x - vector
-  mapping - mapping created with `ISLocalToGlobalMappingCreate()` or `ISLocalToGlobalMappingCreateIS()`

   Level: intermediate

   Notes:
   All vectors obtained with `VecDuplicate()` from this vector inherit the same mapping.

   Vectors obtained with `DMCreateGlobaVector()` will often have this attribute attached to the vector so this call is not needed

.seealso: [](chapter_vectors), `Vec`, `VecAssemblyBegin()`, `VecAssemblyEnd()`, `VecSetValues()`, `VecSetValuesLocal()`,
           `VecGetLocalToGlobalMapping()`, `VecSetValuesBlockedLocal()`
@*/
PetscErrorCode VecSetLocalToGlobalMapping(Vec x, ISLocalToGlobalMapping mapping)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  if (mapping) PetscValidHeaderSpecific(mapping, IS_LTOGM_CLASSID, 2);
  if (x->ops->setlocaltoglobalmapping) PetscUseTypeMethod(x, setlocaltoglobalmapping, mapping);
  else PetscCall(PetscLayoutSetISLocalToGlobalMapping(x->map, mapping));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecGetLocalToGlobalMapping - Gets the local-to-global numbering set by `VecSetLocalToGlobalMapping()`

   Not Collective

   Input Parameter:
.  X - the vector

   Output Parameter:
.  mapping - the mapping

   Level: advanced

.seealso: [](chapter_vectors), `Vec`, `VecSetValuesLocal()`, `VecSetLocalToGlobalMapping()`
@*/
PetscErrorCode VecGetLocalToGlobalMapping(Vec X, ISLocalToGlobalMapping *mapping)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(X, VEC_CLASSID, 1);
  PetscValidType(X, 1);
  PetscValidPointer(mapping, 2);
  *mapping = X->map->mapping;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecAssemblyBegin - Begins assembling the vector; that is ensuring all the vector's entries are stored on the correct MPI process. This routine should
   be called after completing all calls to `VecSetValues()`.

   Collective

   Input Parameter:
.  vec - the vector

   Level: beginner

.seealso: [](chapter_vectors), `Vec`, `VecAssemblyEnd()`, `VecSetValues()`
@*/
PetscErrorCode VecAssemblyBegin(Vec vec)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  PetscValidType(vec, 1);
  PetscCall(VecStashViewFromOptions(vec, NULL, "-vec_view_stash"));
  PetscCall(PetscLogEventBegin(VEC_AssemblyBegin, vec, 0, 0, 0));
  PetscTryTypeMethod(vec, assemblybegin);
  PetscCall(PetscLogEventEnd(VEC_AssemblyBegin, vec, 0, 0, 0));
  PetscCall(PetscObjectStateIncrease((PetscObject)vec));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecAssemblyEnd - Completes assembling the vector.  This routine should be called after `VecAssemblyBegin()`.

   Collective

   Input Parameter:
.  vec - the vector

   Options Database Keys:
+  -vec_view - Prints vector in `PETSC_VIEWER_DEFAULT` format
.  -vec_view ::ascii_matlab - Prints vector in `PETSC_VIEWER_ASCII_MATLAB` format to stdout
.  -vec_view matlab:filename - Prints vector in MATLAB .mat file to filename (requires PETSc configured with --with-matlab)
.  -vec_view draw - Activates vector viewing using drawing tools
.  -display <name> - Sets display name (default is host)
.  -draw_pause <sec> - Sets number of seconds to pause after display
-  -vec_view socket - Activates vector viewing using a socket

   Level: beginner

.seealso: [](chapter_vectors), `Vec`, `VecAssemblyBegin()`, `VecSetValues()`
@*/
PetscErrorCode VecAssemblyEnd(Vec vec)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  PetscCall(PetscLogEventBegin(VEC_AssemblyEnd, vec, 0, 0, 0));
  PetscValidType(vec, 1);
  PetscTryTypeMethod(vec, assemblyend);
  PetscCall(PetscLogEventEnd(VEC_AssemblyEnd, vec, 0, 0, 0));
  PetscCall(VecViewFromOptions(vec, NULL, "-vec_view"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetPreallocationCOO - set preallocation for a vector using a coordinate format of the entries with global indices

   Collective

   Input Parameters:
+  x - vector being preallocated
.  ncoo - number of entries
-  coo_i - entry indices

   Level: beginner

   Notes:
   This and `VecSetValuesCOO()` provide an alernative API to using `VecSetValues()` to provide vector values.

   This API is particularly efficient for use on GPUs.

   Entries can be repeated, see `VecSetValuesCOO()`. Negative indices are not allowed unless vector option `VEC_IGNORE_NEGATIVE_INDICES` is set,
   in which case they, along with the corresponding entries in `VecSetValuesCOO()`, are ignored. If vector option `VEC_NO_OFF_PROC_ENTRIES` is set,
   remote entries are ignored, otherwise, they will be properly added or inserted to the vector.

   The array coo_i[] may be freed immediately after calling this function.

.seealso: [](chapter_vectors), `Vec`, `VecSetValuesCOO()`, `VecSetPreallocationCOOLocal()`
@*/
PetscErrorCode VecSetPreallocationCOO(Vec x, PetscCount ncoo, const PetscInt coo_i[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidType(x, 1);
  if (ncoo) PetscValidIntPointer(coo_i, 3);
  PetscCall(PetscLogEventBegin(VEC_SetPreallocateCOO, x, 0, 0, 0));
  PetscCall(PetscLayoutSetUp(x->map));
  if (x->ops->setpreallocationcoo) {
    PetscUseTypeMethod(x, setpreallocationcoo, ncoo, coo_i);
  } else {
    IS is_coo_i;
    /* The default implementation only supports ncoo within limit of PetscInt */
    PetscCheck(ncoo <= PETSC_MAX_INT, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "ncoo %" PetscCount_FMT " overflowed PetscInt; configure --with-64-bit-indices or request support", ncoo);
    PetscCall(ISCreateGeneral(PETSC_COMM_SELF, ncoo, coo_i, PETSC_COPY_VALUES, &is_coo_i));
    PetscCall(PetscObjectCompose((PetscObject)x, "__PETSc_coo_i", (PetscObject)is_coo_i));
    PetscCall(ISDestroy(&is_coo_i));
  }
  PetscCall(PetscLogEventEnd(VEC_SetPreallocateCOO, x, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetPreallocationCOOLocal - set preallocation for vectors using a coordinate format of the entries with local indices

   Collective

   Input Parameters:
+  x - vector being preallocated
.  ncoo - number of entries
-  coo_i - row indices (local numbering; may be modified)

   Level: beginner

   Notes:
   This and `VecSetValuesCOO()` provide an alernative API to using `VecSetValuesLocal()` to provide vector values.

  This API is particularly efficient for use on GPUs.

   The local indices are translated using the local to global mapping, thus `VecSetLocalToGlobalMapping()` must have been
   called prior to this function.

   The indices coo_i may be modified within this function. They might be translated to corresponding global
   indices, but the caller should not rely on them having any specific value after this function returns. The arrays
   can be freed or reused immediately after this function returns.

   Entries can be repeated. Negative indices and remote indices might be allowed. see `VecSetPreallocationCOO()`.

.seealso: [](chapter_vectors), `Vec`, `VecSetPreallocationCOO()`, `VecSetValuesCOO()`
@*/
PetscErrorCode VecSetPreallocationCOOLocal(Vec x, PetscCount ncoo, PetscInt coo_i[])
{
  ISLocalToGlobalMapping ltog;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidType(x, 1);
  if (ncoo) PetscValidIntPointer(coo_i, 3);
  PetscCheck(ncoo <= PETSC_MAX_INT, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "ncoo %" PetscCount_FMT " overflowed PetscInt; configure --with-64-bit-indices or request support", ncoo);
  PetscCall(PetscLayoutSetUp(x->map));
  PetscCall(VecGetLocalToGlobalMapping(x, &ltog));
  if (ltog) PetscCall(ISLocalToGlobalMappingApply(ltog, ncoo, coo_i, coo_i));
  PetscCall(VecSetPreallocationCOO(x, ncoo, coo_i));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetValuesCOO - set values at once in a vector preallocated using `VecSetPreallocationCOO()`

   Collective

   Input Parameters:
+  x - vector being set
.  coo_v - the value array
-  imode - the insert mode

   Level: beginner

   Note:
   This and `VecSetPreallocationCOO() or ``VecSetPreallocationCOOLocal()` provide an alernative API to using `VecSetValues()` to provide vector values.

   This API is particularly efficient for use on GPUs.

   The values must follow the order of the indices prescribed with `VecSetPreallocationCOO()` or `VecSetPreallocationCOOLocal()`.
   When repeated entries are specified in the COO indices the `coo_v` values are first properly summed, regardless of the value of `imode`.
   The imode flag indicates if `coo_v` must be added to the current values of the vector (`ADD_VALUES`) or overwritten (`INSERT_VALUES`).
   `VecAssemblyBegin()` and `VecAssemblyEnd()` do not need to be called after this routine. It automatically handles the assembly process.

.seealso: [](chapter_vectors), `Vec`, `VecSetPreallocationCOO()`, `VecSetPreallocationCOOLocal()`, `VecSetValues()`
@*/
PetscErrorCode VecSetValuesCOO(Vec x, const PetscScalar coo_v[], InsertMode imode)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidType(x, 1);
  PetscValidLogicalCollectiveEnum(x, imode, 3);
  PetscCall(PetscLogEventBegin(VEC_SetValuesCOO, x, 0, 0, 0));
  if (x->ops->setvaluescoo) {
    PetscUseTypeMethod(x, setvaluescoo, coo_v, imode);
    PetscCall(PetscObjectStateIncrease((PetscObject)x));
  } else {
    IS              is_coo_i;
    const PetscInt *coo_i;
    PetscInt        ncoo;
    PetscMemType    mtype;

    PetscCall(PetscGetMemType(coo_v, &mtype));
    PetscCheck(mtype == PETSC_MEMTYPE_HOST, PetscObjectComm((PetscObject)x), PETSC_ERR_ARG_WRONG, "The basic VecSetValuesCOO() only supports v[] on host");
    PetscCall(PetscObjectQuery((PetscObject)x, "__PETSc_coo_i", (PetscObject *)&is_coo_i));
    PetscCheck(is_coo_i, PetscObjectComm((PetscObject)x), PETSC_ERR_COR, "Missing coo_i IS");
    PetscCall(ISGetLocalSize(is_coo_i, &ncoo));
    PetscCall(ISGetIndices(is_coo_i, &coo_i));
    if (imode != ADD_VALUES) PetscCall(VecZeroEntries(x));
    PetscCall(VecSetValues(x, ncoo, coo_i, coo_v, ADD_VALUES));
    PetscCall(ISRestoreIndices(is_coo_i, &coo_i));
    PetscCall(VecAssemblyBegin(x));
    PetscCall(VecAssemblyEnd(x));
  }
  PetscCall(PetscLogEventEnd(VEC_SetValuesCOO, x, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode VecPointwiseApply_Private(Vec w, Vec x, Vec y, PetscLogEvent event, PetscErrorCode (*const pointwise_op)(Vec, Vec, Vec))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(w, VEC_CLASSID, 1);
  PetscValidHeaderSpecific(x, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(y, VEC_CLASSID, 3);
  PetscValidType(w, 1);
  PetscValidType(x, 2);
  PetscValidType(y, 3);
  PetscCheckSameTypeAndComm(x, 2, y, 3);
  PetscCheckSameTypeAndComm(y, 3, w, 1);
  VecCheckSameSize(w, 1, x, 2);
  VecCheckSameSize(w, 1, y, 3);
  VecCheckAssembled(x);
  VecCheckAssembled(y);
  PetscCall(VecSetErrorIfLocked(w, 1));
  PetscValidFunction(pointwise_op, 5);

  if (event) PetscCall(PetscLogEventBegin(event, x, y, w, 0));
  PetscCall((*pointwise_op)(w, x, y));
  if (event) PetscCall(PetscLogEventEnd(event, x, y, w, 0));
  PetscCall(PetscObjectStateIncrease((PetscObject)w));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecPointwiseMax - Computes the component-wise maximum `w[i] = max(x[i], y[i])`.

   Logically Collective

   Input Parameters:
+  x - the first input vector
-  y - the second input vector

   Output Parameter:
.  w - the result

   Level: advanced

   Notes:
   Any subset of the `x`, `y`, and `w` may be the same vector.

   For complex numbers compares only the real part

.seealso: [](chapter_vectors), `Vec`, `VecPointwiseDivide()`, `VecPointwiseMult()`, `VecPointwiseMin()`, `VecPointwiseMaxAbs()`, `VecMaxPointwiseDivide()`
@*/
PetscErrorCode VecPointwiseMax(Vec w, Vec x, Vec y)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(w, VEC_CLASSID, 1);
  // REVIEW ME: no log event?
  PetscCall(VecPointwiseApply_Private(w, x, y, 0, w->ops->pointwisemax));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecPointwiseMin - Computes the component-wise minimum `w[i] = min(x[i], y[i])`.

   Logically Collective

   Input Parameters:
+  x - the first input vector
-  y - the second input vector

   Output Parameter:
.  w - the result

   Level: advanced

   Notes:
   Any subset of the `x`, `y`, and `w` may be the same vector.

   For complex numbers compares only the real part

.seealso: [](chapter_vectors), `Vec`, `VecPointwiseDivide()`, `VecPointwiseMult()`, `VecPointwiseMin()`, `VecPointwiseMaxAbs()`, `VecMaxPointwiseDivide()`
@*/
PetscErrorCode VecPointwiseMin(Vec w, Vec x, Vec y)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(w, VEC_CLASSID, 1);
  VecCheckAssembled(x);
  // REVIEW ME: no log event?
  PetscCall(VecPointwiseApply_Private(w, x, y, 0, w->ops->pointwisemin));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecPointwiseMaxAbs - Computes the component-wise maximum of the absolute values `w[i] = max(abs(x[i]), abs(y[i]))`.

   Logically Collective

   Input Parameters:
+  x - the first input vector
-  y - the second input vector

   Output Parameter:
.  w - the result

   Level: advanced

   Notes:
   Any subset of the `x`, `y`, and `w` may be the same vector.

.seealso: [](chapter_vectors), `Vec`, `VecPointwiseDivide()`, `VecPointwiseMult()`, `VecPointwiseMin()`, `VecPointwiseMax()`, `VecMaxPointwiseDivide()`
@*/
PetscErrorCode VecPointwiseMaxAbs(Vec w, Vec x, Vec y)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(w, VEC_CLASSID, 1);
  // REVIEW ME: no log event?
  PetscCall(VecPointwiseApply_Private(w, x, y, 0, w->ops->pointwisemaxabs));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecPointwiseDivide - Computes the component-wise division `w[i] = x[i] / y[i]`.

   Logically Collective

   Input Parameters:
+  x - the numerator vector
-  y - the denominator vector

   Output Parameter:
.  w - the result

   Level: advanced

   Note:
   Any subset of the `x`, `y`, and `w` may be the same vector.

.seealso: [](chapter_vectors), `Vec`, `VecPointwiseMult()`, `VecPointwiseMax()`, `VecPointwiseMin()`, `VecPointwiseMaxAbs()`, `VecMaxPointwiseDivide()`
@*/
PetscErrorCode VecPointwiseDivide(Vec w, Vec x, Vec y)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(w, VEC_CLASSID, 1);
  // REVIEW ME: no log event?
  PetscCall(VecPointwiseApply_Private(w, x, y, 0, w->ops->pointwisedivide));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecPointwiseMult - Computes the component-wise multiplication `w[i] = x[i] * y[i]`.

   Logically Collective

   Input Parameters:
+  x - the first vector
-  y - the second vector

   Output Parameter:
.  w - the result

   Level: advanced

   Note:
   Any subset of the `x`, `y`, and `w` may be the same vector.

.seealso: [](chapter_vectors), `Vec`, `VecPointwiseDivide()`, `VecPointwiseMax()`, `VecPointwiseMin()`, `VecPointwiseMaxAbs()`, `VecMaxPointwiseDivide()`
@*/
PetscErrorCode VecPointwiseMult(Vec w, Vec x, Vec y)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(w, VEC_CLASSID, 1);
  PetscCall(VecPointwiseApply_Private(w, x, y, VEC_PointwiseMult, w->ops->pointwisemult));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecDuplicate - Creates a new vector of the same type as an existing vector.

   Collective

   Input Parameter:
.  v - a vector to mimic

   Output Parameter:
.  newv - location to put new vector

   Level: beginner

   Notes:
   `VecDuplicate()` DOES NOT COPY the vector entries, but rather allocates storage
   for the new vector.  Use `VecCopy()` to copy a vector.

   Use `VecDestroy()` to free the space. Use `VecDuplicateVecs()` to get several
   vectors.

.seealso: [](chapter_vectors), `Vec`, `VecDestroy()`, `VecDuplicateVecs()`, `VecCreate()`, `VecCopy()`
@*/
PetscErrorCode VecDuplicate(Vec v, Vec *newv)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidPointer(newv, 2);
  PetscValidType(v, 1);
  PetscUseTypeMethod(v, duplicate, newv);
#if PetscDefined(HAVE_DEVICE)
  if (v->boundtocpu && v->bindingpropagates) {
    PetscCall(VecSetBindingPropagates(*newv, PETSC_TRUE));
    PetscCall(VecBindToCPU(*newv, PETSC_TRUE));
  }
#endif
  PetscCall(PetscObjectStateIncrease((PetscObject)(*newv)));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecDestroy - Destroys a vector.

   Collective

   Input Parameter:
.  v  - the vector

   Level: beginner

.seealso: [](chapter_vectors), `Vec`, `VecCreate()`, `VecDuplicate()`, `VecDestroyVecs()`
@*/
PetscErrorCode VecDestroy(Vec *v)
{
  PetscFunctionBegin;
  PetscValidPointer(v, 1);
  if (!*v) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific((*v), VEC_CLASSID, 1);
  if (--((PetscObject)(*v))->refct > 0) {
    *v = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  PetscCall(PetscObjectSAWsViewOff((PetscObject)*v));
  /* destroy the internal part */
  PetscTryTypeMethod(*v, destroy);
  PetscCall(PetscFree((*v)->defaultrandtype));
  /* destroy the external/common part */
  PetscCall(PetscLayoutDestroy(&(*v)->map));
  PetscCall(PetscHeaderDestroy(v));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecDuplicateVecs - Creates several vectors of the same type as an existing vector.

   Collective

   Input Parameters:
+  m - the number of vectors to obtain
-  v - a vector to mimic

   Output Parameter:
.  V - location to put pointer to array of vectors

   Level: intermediate

   Note:
   Use `VecDestroyVecs()` to free the space. Use `VecDuplicate()` to form a single
   vector.

   Fortran Note:
   The Fortran interface is slightly different from that given below, it
   requires one to pass in `V` a `Vec` array of size at least `m`.
   See the [](chapter_fortran) for details.

.seealso: [](chapter_vectors), `Vec`, [](chapter_fortran), `VecDestroyVecs()`, `VecDuplicate()`, `VecCreate()`, `VecDuplicateVecsF90()`
@*/
PetscErrorCode VecDuplicateVecs(Vec v, PetscInt m, Vec *V[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidPointer(V, 3);
  PetscValidType(v, 1);
  PetscUseTypeMethod(v, duplicatevecs, m, V);
#if defined(PETSC_HAVE_VIENNACL) || defined(PETSC_HAVE_CUDA) || defined(PETSC_HAVE_HIP)
  if (v->boundtocpu && v->bindingpropagates) {
    PetscInt i;

    for (i = 0; i < m; i++) {
      /* Since ops->duplicatevecs might itself propagate the value of boundtocpu,
       * avoid unnecessary overhead by only calling VecBindToCPU() if the vector isn't already bound. */
      if (!(*V)[i]->boundtocpu) {
        PetscCall(VecSetBindingPropagates((*V)[i], PETSC_TRUE));
        PetscCall(VecBindToCPU((*V)[i], PETSC_TRUE));
      }
    }
  }
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecDestroyVecs - Frees a block of vectors obtained with `VecDuplicateVecs()`.

   Collective

   Input Parameters:
+  m - the number of vectors previously obtained, if zero no vectors are destroyed
-  vv - pointer to pointer to array of vector pointers, if `NULL` no vectors are destroyed

   Level: intermediate

   Fortran Note:
   The Fortran interface is slightly different from that given below.
   See the [](chapter_fortran) for details.

.seealso: [](chapter_vectors), `Vec`, [](chapter_fortran), `VecDuplicateVecs()`, `VecDestroyVecsf90()`
@*/
PetscErrorCode VecDestroyVecs(PetscInt m, Vec *vv[])
{
  PetscFunctionBegin;
  PetscValidPointer(vv, 2);
  PetscCheck(m >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Trying to destroy negative number of vectors %" PetscInt_FMT, m);
  if (!m || !*vv) {
    *vv = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscValidHeaderSpecific(**vv, VEC_CLASSID, 2);
  PetscValidType(**vv, 2);
  PetscCall((*(**vv)->ops->destroyvecs)(m, *vv));
  *vv = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecViewFromOptions - View a vector based on values in the options database

   Collective

   Input Parameters:
+  A - the vector
.  obj - Optional object that provides the options prefix for this viewing
-  name - command line option

   Level: intermediate

   Note:
   See `PetscObjectViewFromOptions()` to see the `PetscViewer` and PetscViewerFormat` available

.seealso: [](chapter_vectors), `Vec`, `VecView`, `PetscObjectViewFromOptions()`, `VecCreate()`
@*/
PetscErrorCode VecViewFromOptions(Vec A, PetscObject obj, const char name[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, VEC_CLASSID, 1);
  PetscCall(PetscObjectViewFromOptions((PetscObject)A, obj, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecView - Views a vector object.

   Collective

   Input Parameters:
+  vec - the vector
-  viewer - an optional `PetscViewer` visualization context

   Level: beginner

   Notes:
   The available visualization contexts include
+     `PETSC_VIEWER_STDOUT_SELF` - for sequential vectors
.     `PETSC_VIEWER_STDOUT_WORLD` - for parallel vectors created on `PETSC_COMM_WORLD`
-     `PETSC_VIEWER_STDOUT`_(comm) - for parallel vectors created on MPI communicator comm

   You can change the format the vector is printed using the
   option `PetscViewerPushFormat()`.

   The user can open alternative viewers with
+    `PetscViewerASCIIOpen()` - Outputs vector to a specified file
.    `PetscViewerBinaryOpen()` - Outputs vector in binary to a
         specified file; corresponding input uses `VecLoad()`
.    `PetscViewerDrawOpen()` - Outputs vector to an X window display
.    `PetscViewerSocketOpen()` - Outputs vector to Socket viewer
-    `PetscViewerHDF5Open()` - Outputs vector to HDF5 file viewer

   The user can call `PetscViewerPushFormat()` to specify the output
   format of ASCII printed objects (when using `PETSC_VIEWER_STDOUT_SELF`,
   `PETSC_VIEWER_STDOUT_WORLD` and `PetscViewerASCIIOpen()`).  Available formats include
+    `PETSC_VIEWER_DEFAULT` - default, prints vector contents
.    `PETSC_VIEWER_ASCII_MATLAB` - prints vector contents in MATLAB format
.    `PETSC_VIEWER_ASCII_INDEX` - prints vector contents, including indices of vector elements
-    `PETSC_VIEWER_ASCII_COMMON` - prints vector contents, using a
         format common among all vector types

    You can pass any number of vector objects, or other PETSc objects to the same viewer.

    In the debugger you can do call `VecView`(v,0) to display the vector. (The same holds for any PETSc object viewer).

   Notes for binary viewer:
     If you pass multiple vectors to a binary viewer you can read them back in in the same order
     with `VecLoad()`.

     If the blocksize of the vector is greater than one then you must provide a unique prefix to
     the vector with `PetscObjectSetOptionsPrefix`((`PetscObject`)vec,"uniqueprefix"); BEFORE calling `VecView()` on the
     vector to be stored and then set that same unique prefix on the vector that you pass to `VecLoad()`. The blocksize
     information is stored in an ASCII file with the same name as the binary file plus a ".info" appended to the
     filename. If you copy the binary file, make sure you copy the associated .info file with it.

     See the manual page for `VecLoad()` on the exact format the binary viewer stores
     the values in the file.

   Notes for HDF5 Viewer:
     The name of the `Vec` (given with `PetscObjectSetName()` is the name that is used
     for the object in the HDF5 file. If you wish to store the same Vec into multiple
     datasets in the same file (typically with different values), you must change its
     name each time before calling the `VecView()`. To load the same vector,
     the name of the Vec object passed to `VecLoad()` must be the same.

     If the block size of the vector is greater than 1 then it is used as the first dimension in the HDF5 array.
     If the function `PetscViewerHDF5SetBaseDimension2()`is called then even if the block size is one it will
     be used as the first dimension in the HDF5 array (that is the HDF5 array will always be two dimensional)
     See also `PetscViewerHDF5SetTimestep()` which adds an additional complication to reading and writing `Vec`
     with the HDF5 viewer.

.seealso: [](chapter_vectors), `Vec`, `VecViewFromOptions()`, `PetscViewerASCIIOpen()`, `PetscViewerDrawOpen()`, `PetscDrawLGCreate()`,
          `PetscViewerSocketOpen()`, `PetscViewerBinaryOpen()`, `VecLoad()`, `PetscViewerCreate()`,
          `PetscRealView()`, `PetscScalarView()`, `PetscIntView()`, `PetscViewerHDF5SetTimestep()`
@*/
PetscErrorCode VecView(Vec vec, PetscViewer viewer)
{
  PetscBool         iascii;
  PetscViewerFormat format;
  PetscMPIInt       size;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  PetscValidType(vec, 1);
  VecCheckAssembled(vec);
  if (!viewer) PetscCall(PetscViewerASCIIGetStdout(PetscObjectComm((PetscObject)vec), &viewer));
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  PetscCall(PetscViewerGetFormat(viewer, &format));
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)vec), &size));
  if (size == 1 && format == PETSC_VIEWER_LOAD_BALANCE) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCheck(!vec->stash.n && !vec->bstash.n, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Must call VecAssemblyBegin/End() before viewing this vector");

  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscInt rows, bs;

    PetscCall(PetscObjectPrintClassNamePrefixType((PetscObject)vec, viewer));
    if (format == PETSC_VIEWER_ASCII_INFO || format == PETSC_VIEWER_ASCII_INFO_DETAIL) {
      PetscCall(PetscViewerASCIIPushTab(viewer));
      PetscCall(VecGetSize(vec, &rows));
      PetscCall(VecGetBlockSize(vec, &bs));
      if (bs != 1) {
        PetscCall(PetscViewerASCIIPrintf(viewer, "length=%" PetscInt_FMT ", bs=%" PetscInt_FMT "\n", rows, bs));
      } else {
        PetscCall(PetscViewerASCIIPrintf(viewer, "length=%" PetscInt_FMT "\n", rows));
      }
      PetscCall(PetscViewerASCIIPopTab(viewer));
    }
  }
  PetscCall(VecLockReadPush(vec));
  PetscCall(PetscLogEventBegin(VEC_View, vec, viewer, 0, 0));
  if ((format == PETSC_VIEWER_NATIVE || format == PETSC_VIEWER_LOAD_BALANCE) && vec->ops->viewnative) {
    PetscUseTypeMethod(vec, viewnative, viewer);
  } else {
    PetscUseTypeMethod(vec, view, viewer);
  }
  PetscCall(VecLockReadPop(vec));
  PetscCall(PetscLogEventEnd(VEC_View, vec, viewer, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#if defined(PETSC_USE_DEBUG)
  #include <../src/sys/totalview/tv_data_display.h>
PETSC_UNUSED static int TV_display_type(const struct _p_Vec *v)
{
  const PetscScalar *values;
  char               type[32];

  TV_add_row("Local rows", "int", &v->map->n);
  TV_add_row("Global rows", "int", &v->map->N);
  TV_add_row("Typename", TV_ascii_string_type, ((PetscObject)v)->type_name);
  PetscCall(VecGetArrayRead((Vec)v, &values));
  PetscCall(PetscSNPrintf(type, 32, "double[%" PetscInt_FMT "]", v->map->n));
  TV_add_row("values", type, values);
  PetscCall(VecRestoreArrayRead((Vec)v, &values));
  return TV_format_OK;
}
#endif

/*@C
   VecViewNative - Views a vector object with the original type specific viewer

   Collective

   Input Parameters:
+  vec - the vector
-  viewer - an optional `PetscViewer` visualization context

   Level: developer

   Note:
   This can be used with, for example, vectors obtained with `DMCreateGlobalVector()` for a `DMDA` to display the vector
   in the PETSc storage format (each MPI process values follow the previous MPI processes) instead of the "natural" grid
   ordering.

.seealso: [](chapter_vectors), `Vec`, `PetscViewerASCIIOpen()`, `PetscViewerDrawOpen()`, `PetscDrawLGCreate()`, `VecView()`
          `PetscViewerSocketOpen()`, `PetscViewerBinaryOpen()`, `VecLoad()`, `PetscViewerCreate()`,
          `PetscRealView()`, `PetscScalarView()`, `PetscIntView()`, `PetscViewerHDF5SetTimestep()`
@*/
PetscErrorCode VecViewNative(Vec vec, PetscViewer viewer)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  PetscValidType(vec, 1);
  if (!viewer) PetscCall(PetscViewerASCIIGetStdout(PetscObjectComm((PetscObject)vec), &viewer));
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  PetscUseTypeMethod(vec, viewnative, viewer);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecGetSize - Returns the global number of elements of the vector.

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameter:
.  size - the global length of the vector

   Level: beginner

.seealso: [](chapter_vectors), `Vec`, `VecGetLocalSize()`
@*/
PetscErrorCode VecGetSize(Vec x, PetscInt *size)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidIntPointer(size, 2);
  PetscValidType(x, 1);
  PetscUseTypeMethod(x, getsize, size);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecGetLocalSize - Returns the number of elements of the vector stored
   in local memory (that is on this MPI process)

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameter:
.  size - the length of the local piece of the vector

   Level: beginner

.seealso: [](chapter_vectors), `Vec`, `VecGetSize()`
@*/
PetscErrorCode VecGetLocalSize(Vec x, PetscInt *size)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidIntPointer(size, 2);
  PetscValidType(x, 1);
  PetscUseTypeMethod(x, getlocalsize, size);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecGetOwnershipRange - Returns the range of indices owned by
   this process. The vector is laid out with the
   first n1 elements on the first processor, next n2 elements on the
   second, etc.  For certain parallel layouts this range may not be
   well defined.

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameters:
+  low - the first local element, pass in `NULL` if not interested
-  high - one more than the last local element, pass in `NULL` if not interested

   Level: beginner

   Note:
   The high argument is one more than the last element stored locally.

   Fortran Note:
   `PETSC_NULL_INTEGER` should be used instead of NULL

.seealso: [](chapter_vectors), `Vec`, `MatGetOwnershipRange()`, `MatGetOwnershipRanges()`, `VecGetOwnershipRanges()`
@*/
PetscErrorCode VecGetOwnershipRange(Vec x, PetscInt *low, PetscInt *high)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidType(x, 1);
  if (low) PetscValidIntPointer(low, 2);
  if (high) PetscValidIntPointer(high, 3);
  if (low) *low = x->map->rstart;
  if (high) *high = x->map->rend;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecGetOwnershipRanges - Returns the range of indices owned by EACH processor,
   The vector is laid out with the
   first n1 elements on the first processor, next n2 elements on the
   second, etc.  For certain parallel layouts this range may not be
   well defined.

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameter:
.  range - array of length size+1 with the start and end+1 for each process

   Level: beginner

   Notes:
   The high argument is one more than the last element stored locally.

   If the ranges are used after all vectors that share the ranges has been destroyed then the program will crash accessing ranges[].

   Fortran Note:
   You must PASS in an array of length size+1

.seealso: [](chapter_vectors), `Vec`, `MatGetOwnershipRange()`, `MatGetOwnershipRanges()`, `VecGetOwnershipRange()`
@*/
PetscErrorCode VecGetOwnershipRanges(Vec x, const PetscInt *ranges[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidType(x, 1);
  PetscCall(PetscLayoutGetRanges(x->map, ranges));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetOption - Sets an option for controlling a vector's behavior.

   Collective

   Input Parameters:
+  x - the vector
.  op - the option
-  flag - turn the option on or off

   Supported Options:
+     `VEC_IGNORE_OFF_PROC_ENTRIES`, which causes `VecSetValues()` to ignore
          entries destined to be stored on a separate processor. This can be used
          to eliminate the global reduction in the `VecAssemblyBegin()` if you know
          that you have only used `VecSetValues()` to set local elements
.     `VEC_IGNORE_NEGATIVE_INDICES`, which means you can pass negative indices
          in ix in calls to `VecSetValues()` or `VecGetValues()`. These rows are simply
          ignored.
-     `VEC_SUBSET_OFF_PROC_ENTRIES`, which causes `VecAssemblyBegin()` to assume that the off-process
          entries will always be a subset (possibly equal) of the off-process entries set on the
          first assembly which had a true `VEC_SUBSET_OFF_PROC_ENTRIES` and the vector has not
          changed this flag afterwards. If this assembly is not such first assembly, then this
          assembly can reuse the communication pattern setup in that first assembly, thus avoiding
          a global reduction. Subsequent assemblies setting off-process values should use the same
          InsertMode as the first assembly.

   Level: intermediate

   Developer Note:
   The `InsertMode` restriction could be removed by packing the stash messages out of place.

.seealso: [](chapter_vectors), `Vec`, `VecSetValues()`
@*/
PetscErrorCode VecSetOption(Vec x, VecOption op, PetscBool flag)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidType(x, 1);
  PetscTryTypeMethod(x, setoption, op, flag);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Default routines for obtaining and releasing; */
/* may be used by any implementation */
PetscErrorCode VecDuplicateVecs_Default(Vec w, PetscInt m, Vec *V[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(w, VEC_CLASSID, 1);
  PetscValidPointer(V, 3);
  PetscCheck(m > 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "m must be > 0: m = %" PetscInt_FMT, m);
  PetscCall(PetscMalloc1(m, V));
  for (PetscInt i = 0; i < m; i++) PetscCall(VecDuplicate(w, *V + i));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode VecDestroyVecs_Default(PetscInt m, Vec v[])
{
  PetscInt i;

  PetscFunctionBegin;
  PetscValidPointer(v, 2);
  for (i = 0; i < m; i++) PetscCall(VecDestroy(&v[i]));
  PetscCall(PetscFree(v));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecResetArray - Resets a vector to use its default memory. Call this
   after the use of `VecPlaceArray()`.

   Not Collective

   Input Parameter:
.  vec - the vector

   Level: developer

.seealso: [](chapter_vectors), `Vec`, `VecGetArray()`, `VecRestoreArray()`, `VecReplaceArray()`, `VecPlaceArray()`
@*/
PetscErrorCode VecResetArray(Vec vec)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  PetscValidType(vec, 1);
  PetscUseTypeMethod(vec, resetarray);
  PetscCall(PetscObjectStateIncrease((PetscObject)vec));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  VecLoad - Loads a vector that has been stored in binary or HDF5 format
  with `VecView()`.

  Collective

  Input Parameters:
+ vec - the newly loaded vector, this needs to have been created with `VecCreate()` or
           some related function before the call to `VecLoad()`.
- viewer - binary file viewer, obtained from `PetscViewerBinaryOpen()` or
           HDF5 file viewer, obtained from `PetscViewerHDF5Open()`

   Level: intermediate

  Notes:
  Defaults to the standard `VECSEQ` or `VECMPI`, if you want some other type of `Vec` call `VecSetFromOptions()`
  before calling this.

  The input file must contain the full global vector, as
  written by the routine `VecView()`.

  If the type or size of `vec` is not set before a call to `VecLoad()`, PETSc
  sets the type and the local and global sizes based on the vector it is reading in. If type and/or
  sizes are already set, then the same are used.

  If using the binary viewer and the blocksize of the vector is greater than one then you must provide a unique prefix to
  the vector with `PetscObjectSetOptionsPrefix`((`PetscObject`)vec,"uniqueprefix"); BEFORE calling `VecView()` on the
  vector to be stored and then set that same unique prefix on the vector that you pass to VecLoad(). The blocksize
  information is stored in an ASCII file with the same name as the binary file plus a ".info" appended to the
  filename. If you copy the binary file, make sure you copy the associated .info file with it.

  If using HDF5, you must assign the `Vec` the same name as was used in the Vec
  that was stored in the file using `PetscObjectSetName(). Otherwise you will
  get the error message: "Cannot H5DOpen2() with `Vec` name NAMEOFOBJECT".

  If the HDF5 file contains a two dimensional array the first dimension is treated as the block size
  in loading the vector. Hence, for example, using MATLAB notation h5create('vector.dat','/Test_Vec',[27 1]);
  will load a vector of size 27 and block size 27 thus resulting in all 27 entries being on the first process of
  vectors communicator and the rest of the processes having zero entries

  Notes for advanced users when using the binary viewer:
  Most users should not need to know the details of the binary storage
  format, since `VecLoad()` and `VecView()` completely hide these details.
  But for anyone who's interested, the standard binary vector storage
  format is
.vb
     PetscInt    VEC_FILE_CLASSID
     PetscInt    number of rows
     PetscScalar *values of all entries
.ve

   In addition, PETSc automatically uses byte swapping to work on all machines; the files
   are written ALWAYS using big-endian ordering. On small-endian machines the numbers
   are converted to the small-endian format when they are read in from the file.
   See PetscBinaryRead() and PetscBinaryWrite() to see how this may be done.

.seealso: [](chapter_vectors), `Vec`, `PetscViewerBinaryOpen()`, `VecView()`, `MatLoad()`, `VecLoad()`
@*/
PetscErrorCode VecLoad(Vec vec, PetscViewer viewer)
{
  PetscBool         isbinary, ishdf5, isadios, isexodusii;
  PetscViewerFormat format;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  PetscCheckSameComm(vec, 1, viewer, 2);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERBINARY, &isbinary));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERHDF5, &ishdf5));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERADIOS, &isadios));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWEREXODUSII, &isexodusii));
  PetscCheck(isbinary || ishdf5 || isadios || isexodusii, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Invalid viewer; open viewer with PetscViewerBinaryOpen()");

  PetscCall(VecSetErrorIfLocked(vec, 1));
  if (!((PetscObject)vec)->type_name && !vec->ops->create) PetscCall(VecSetType(vec, VECSTANDARD));
  PetscCall(PetscLogEventBegin(VEC_Load, viewer, 0, 0, 0));
  PetscCall(PetscViewerGetFormat(viewer, &format));
  if (format == PETSC_VIEWER_NATIVE && vec->ops->loadnative) {
    PetscUseTypeMethod(vec, loadnative, viewer);
  } else {
    PetscUseTypeMethod(vec, load, viewer);
  }
  PetscCall(PetscLogEventEnd(VEC_Load, viewer, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecReciprocal - Replaces each component of a vector by its reciprocal.

   Logically Collective

   Input Parameter:
.  vec - the vector

   Output Parameter:
.  vec - the vector reciprocal

   Level: intermediate

   Note:
   Vector entries with value 0.0 are not changed

.seealso: [](chapter_vectors), `Vec`, `VecLog()`, `VecExp()`, `VecSqrtAbs()`
@*/
PetscErrorCode VecReciprocal(Vec vec)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  PetscValidType(vec, 1);
  VecCheckAssembled(vec);
  PetscCall(VecSetErrorIfLocked(vec, 1));
  PetscUseTypeMethod(vec, reciprocal);
  PetscCall(PetscObjectStateIncrease((PetscObject)vec));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  VecSetOperation - Allows the user to override a particular vector operation.

   Logically Collective; No Fortran Support

  Input Parameters:
+ vec - The vector to modify
. op  - The name of the operation
- f   - The function that provides the operation.

  Notes:
  `f` may be `NULL` to remove the operation from `vec`. Depending on the operation this may be
  allowed, however some always expect a valid function. In these cases an error will be raised
  when calling the interface routine in question.

  See `VecOperation` for an up-to-date list of override-able operations. The operations listed
  there have the form `VECOP_<OPERATION>`, where `<OPERATION>` is the suffix (in all capital
  letters) of the public interface routine (e.g., `VecView()` -> `VECOP_VIEW`).

  Overriding a particular `Vec`'s operation has no affect on any other `Vec`s past, present,
  or future. The user should also note that overriding a method is "destructive"; the previous
  method is not retained in any way.

  Level: advanced

  Example Usage:
.vb
  // some new VecView() implementation, must have the same signature as the function it seeks
  // to replace
  PetscErrorCode UserVecView(Vec x, PetscViewer viewer)
  {
    PetscFunctionBeginUser;
    // ...
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  // Create a VECMPI which has a pre-defined VecView() implementation
  VecCreateMPI(comm, n, N, &x);
  // Calls the VECMPI implementation for VecView()
  VecView(x, viewer);

  VecSetOperation(x, VECOP_VIEW, (void (*)(void))UserVecView);
  // Now calls UserVecView()
  VecView(x, viewer);
.ve

.seealso: [](chapter_vectors), `Vec`, `VecCreate()`, `MatShellSetOperation()`
@*/
PetscErrorCode VecSetOperation(Vec vec, VecOperation op, void (*f)(void))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  if (op == VECOP_VIEW && !vec->ops->viewnative) {
    vec->ops->viewnative = vec->ops->view;
  } else if (op == VECOP_LOAD && !vec->ops->loadnative) {
    vec->ops->loadnative = vec->ops->load;
  }
  ((void (**)(void))vec->ops)[(int)op] = f;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecStashSetInitialSize - sets the sizes of the vec-stash, that is
   used during the assembly process to store values that belong to
   other processors.

   Not Collective, different processes can have different size stashes

   Input Parameters:
+  vec   - the vector
.  size  - the initial size of the stash.
-  bsize - the initial size of the block-stash(if used).

   Options Database Keys:
+   -vecstash_initial_size <size> or <size0,size1,...sizep-1>
-   -vecstash_block_initial_size <bsize> or <bsize0,bsize1,...bsizep-1>

   Level: intermediate

   Notes:
     The block-stash is used for values set with `VecSetValuesBlocked()` while
     the stash is used for values set with `VecSetValues()`

     Run with the option -info and look for output of the form
     VecAssemblyBegin_MPIXXX:Stash has MM entries, uses nn mallocs.
     to determine the appropriate value, MM, to use for size and
     VecAssemblyBegin_MPIXXX:Block-Stash has BMM entries, uses nn mallocs.
     to determine the value, BMM to use for bsize

   PETSc attempts to smartly manage the stash size so there is little likelihood setting a
   a specific value here will affect performance

.seealso: [](chapter_vectors), `Vec`, `VecSetBlockSize()`, `VecSetValues()`, `VecSetValuesBlocked()`, `VecStashView()`
@*/
PetscErrorCode VecStashSetInitialSize(Vec vec, PetscInt size, PetscInt bsize)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);
  PetscCall(VecStashSetInitialSize_Private(&vec->stash, size));
  PetscCall(VecStashSetInitialSize_Private(&vec->bstash, bsize));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecConjugate - Conjugates a vector. That is, replace every entry in a vector with its complex conjugate

   Logically Collective

   Input Parameter:
.  x - the vector

   Level: intermediate

.seealso: [](chapter_vectors), `Vec`, `VecSet()`
@*/
PetscErrorCode VecConjugate(Vec x)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidType(x, 1);
  VecCheckAssembled(x);
  PetscCall(VecSetErrorIfLocked(x, 1));
  if (PetscDefined(USE_COMPLEX)) {
    PetscUseTypeMethod(x, conjugate);
    /* we need to copy norms here */
    PetscCall(PetscObjectStateIncrease((PetscObject)x));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetRandom - Sets all components of a vector to random numbers.

   Logically Collective

   Input Parameters:
+  x  - the vector
-  rctx - the random number context, formed by `PetscRandomCreate()`, or use `NULL` and it will create one internally.

   Output Parameter:
.  x  - the vector

   Example of Usage:
.vb
     PetscRandomCreate(PETSC_COMM_WORLD,&rctx);
     VecSetRandom(x,rctx);
     PetscRandomDestroy(&rctx);
.ve

   Level: intermediate

.seealso: [](chapter_vectors), `Vec`, `VecSet()`, `VecSetValues()`, `PetscRandomCreate()`, `PetscRandomDestroy()`
@*/
PetscErrorCode VecSetRandom(Vec x, PetscRandom rctx)
{
  PetscRandom randObj = NULL;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  if (rctx) PetscValidHeaderSpecific(rctx, PETSC_RANDOM_CLASSID, 2);
  PetscValidType(x, 1);
  VecCheckAssembled(x);
  PetscCall(VecSetErrorIfLocked(x, 1));

  if (!rctx) {
    PetscCall(PetscRandomCreate(PetscObjectComm((PetscObject)x), &randObj));
    PetscCall(PetscRandomSetType(randObj, x->defaultrandtype));
    PetscCall(PetscRandomSetFromOptions(randObj));
    rctx = randObj;
  }

  PetscCall(PetscLogEventBegin(VEC_SetRandom, x, rctx, 0, 0));
  PetscUseTypeMethod(x, setrandom, rctx);
  PetscCall(PetscLogEventEnd(VEC_SetRandom, x, rctx, 0, 0));

  PetscCall(PetscRandomDestroy(&randObj));
  PetscCall(PetscObjectStateIncrease((PetscObject)x));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  VecZeroEntries - puts a `0.0` in each element of a vector

  Logically Collective

  Input Parameter:
. vec - The vector

  Level: beginner

.seealso: [](chapter_vectors), `Vec`, `VecCreate()`, `VecSetOptionsPrefix()`, `VecSet()`, `VecSetValues()`
@*/
PetscErrorCode VecZeroEntries(Vec vec)
{
  PetscFunctionBegin;
  PetscCall(VecSet(vec, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  VecSetTypeFromOptions_Private - Sets the type of vector from user options. Defaults to a PETSc sequential vector on one
  processor and a PETSc MPI vector on more than one processor.

  Collective

  Input Parameter:
. vec - The vector

  Level: intermediate

.seealso: [](chapter_vectors), `Vec`, `VecSetFromOptions()`, `VecSetType()`
*/
static PetscErrorCode VecSetTypeFromOptions_Private(Vec vec, PetscOptionItems *PetscOptionsObject)
{
  PetscBool   opt;
  VecType     defaultType;
  char        typeName[256];
  PetscMPIInt size;

  PetscFunctionBegin;
  if (((PetscObject)vec)->type_name) defaultType = ((PetscObject)vec)->type_name;
  else {
    PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)vec), &size));
    if (size > 1) defaultType = VECMPI;
    else defaultType = VECSEQ;
  }

  PetscCall(VecRegisterAll());
  PetscCall(PetscOptionsFList("-vec_type", "Vector type", "VecSetType", VecList, defaultType, typeName, 256, &opt));
  if (opt) {
    PetscCall(VecSetType(vec, typeName));
  } else {
    PetscCall(VecSetType(vec, defaultType));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  VecSetFromOptions - Configures the vector from the options database.

  Collective

  Input Parameter:
. vec - The vector

  Level: beginner

  Notes:
  To see all options, run your program with the -help option.

  Must be called after `VecCreate()` but before the vector is used.

.seealso: [](chapter_vectors), `Vec`, `VecCreate()`, `VecSetOptionsPrefix()`
@*/
PetscErrorCode VecSetFromOptions(Vec vec)
{
  PetscBool flg;
  PetscInt  bind_below = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec, VEC_CLASSID, 1);

  PetscObjectOptionsBegin((PetscObject)vec);
  /* Handle vector type options */
  PetscCall(VecSetTypeFromOptions_Private(vec, PetscOptionsObject));

  /* Handle specific vector options */
  PetscTryTypeMethod(vec, setfromoptions, PetscOptionsObject);

  /* Bind to CPU if below a user-specified size threshold.
   * This perhaps belongs in the options for the GPU Vec types, but VecBindToCPU() does nothing when called on non-GPU types,
   * and putting it here makes is more maintainable than duplicating this for all. */
  PetscCall(PetscOptionsInt("-vec_bind_below", "Set the size threshold (in local entries) below which the Vec is bound to the CPU", "VecBindToCPU", bind_below, &bind_below, &flg));
  if (flg && vec->map->n < bind_below) PetscCall(VecBindToCPU(vec, PETSC_TRUE));

  /* process any options handlers added with PetscObjectAddOptionsHandler() */
  PetscCall(PetscObjectProcessOptionsHandlers((PetscObject)vec, PetscOptionsObject));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  VecSetSizes - Sets the local and global sizes, and checks to determine compatibility of the sizes

  Collective

  Input Parameters:
+ v - the vector
. n - the local size (or `PETSC_DECIDE` to have it set)
- N - the global size (or `PETSC_DETERMINE` to have it set)

  Level: intermediate

  Notes:
  `N` cannot be `PETSC_DETERMINE` if `n` is `PETSC_DECIDE`

  If one processor calls this with `N` of `PETSC_DETERMINE` then all processors must, otherwise the program will hang.

.seealso: [](chapter_vectors), `Vec`, `VecGetSize()`, `PetscSplitOwnership()`
@*/
PetscErrorCode VecSetSizes(Vec v, PetscInt n, PetscInt N)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  if (N >= 0) {
    PetscValidLogicalCollectiveInt(v, N, 3);
    PetscCheck(n <= N, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Local size %" PetscInt_FMT " cannot be larger than global size %" PetscInt_FMT, n, N);
  }
  PetscCheck(!(v->map->n >= 0 || v->map->N >= 0) || !(v->map->n != n || v->map->N != N), PETSC_COMM_SELF, PETSC_ERR_SUP, "Cannot change/reset vector sizes to %" PetscInt_FMT " local %" PetscInt_FMT " global after previously setting them to %" PetscInt_FMT " local %" PetscInt_FMT " global", n, N,
             v->map->n, v->map->N);
  v->map->n = n;
  v->map->N = N;
  PetscTryTypeMethod(v, create);
  v->ops->create = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetBlockSize - Sets the block size for future calls to `VecSetValuesBlocked()`
   and `VecSetValuesBlockedLocal()`.

   Logically Collective

   Input Parameters:
+  v - the vector
-  bs - the blocksize

   Level: advanced

   Note:
   All vectors obtained by `VecDuplicate()` inherit the same blocksize.

   Vectors obtained with `DMCreateGlobalVector()` and `DMCreateLocalVector()` generally already have a blocksize set based on the state of the `DM`

.seealso: [](chapter_vectors), `Vec`, `VecSetValuesBlocked()`, `VecSetLocalToGlobalMapping()`, `VecGetBlockSize()`
@*/
PetscErrorCode VecSetBlockSize(Vec v, PetscInt bs)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidLogicalCollectiveInt(v, bs, 2);
  PetscCall(PetscLayoutSetBlockSize(v->map, bs));
  v->bstash.bs = bs; /* use the same blocksize for the vec's block-stash */
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecGetBlockSize - Gets the blocksize for the vector, i.e. what is used for `VecSetValuesBlocked()`
   and `VecSetValuesBlockedLocal()`.

   Not Collective

   Input Parameter:
.  v - the vector

   Output Parameter:
.  bs - the blocksize

   Level: advanced

   Note:
   All vectors obtained by `VecDuplicate()` inherit the same blocksize.

.seealso: [](chapter_vectors), `Vec`, `VecSetValuesBlocked()`, `VecSetLocalToGlobalMapping()`, `VecSetBlockSize()`
@*/
PetscErrorCode VecGetBlockSize(Vec v, PetscInt *bs)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidIntPointer(bs, 2);
  PetscCall(PetscLayoutGetBlockSize(v->map, bs));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecSetOptionsPrefix - Sets the prefix used for searching for all
   `Vec` options in the database.

   Logically Collective

   Input Parameters:
+  v - the `Vec` context
-  prefix - the prefix to prepend to all option names

   Level: advanced

   Note:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the hyphen.

.seealso: [](chapter_vectors), `Vec`, `VecSetFromOptions()`
@*/
PetscErrorCode VecSetOptionsPrefix(Vec v, const char prefix[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscCall(PetscObjectSetOptionsPrefix((PetscObject)v, prefix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecAppendOptionsPrefix - Appends to the prefix used for searching for all
   `Vec` options in the database.

   Logically Collective

   Input Parameters:
+  v - the `Vec` context
-  prefix - the prefix to prepend to all option names

   Level: advanced

   Note:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the hyphen.

.seealso: [](chapter_vectors), `Vec`, `VecGetOptionsPrefix()`
@*/
PetscErrorCode VecAppendOptionsPrefix(Vec v, const char prefix[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscCall(PetscObjectAppendOptionsPrefix((PetscObject)v, prefix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   VecGetOptionsPrefix - Sets the prefix used for searching for all
   Vec options in the database.

   Not Collective

   Input Parameter:
.  v - the `Vec` context

   Output Parameter:
.  prefix - pointer to the prefix string used

   Level: advanced

   Fortran Note:
   The user must pass in a string `prefix` of
   sufficient length to hold the prefix.

.seealso: [](chapter_vectors), `Vec`, `VecAppendOptionsPrefix()`
@*/
PetscErrorCode VecGetOptionsPrefix(Vec v, const char *prefix[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscCall(PetscObjectGetOptionsPrefix((PetscObject)v, prefix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetUp - Sets up the internal vector data structures for the later use.

   Collective

   Input Parameter:
.  v - the `Vec` context

   Level: advanced

   Notes:
   For basic use of the `Vec` classes the user need not explicitly call
   `VecSetUp()`, since these actions will happen automatically.

.seealso: [](chapter_vectors), `Vec`, `VecCreate()`, `VecDestroy()`
@*/
PetscErrorCode VecSetUp(Vec v)
{
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscCheck(v->map->n >= 0 || v->map->N >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Sizes not set");
  if (!((PetscObject)v)->type_name) {
    PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)v), &size));
    if (size == 1) {
      PetscCall(VecSetType(v, VECSEQ));
    } else {
      PetscCall(VecSetType(v, VECMPI));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    These currently expose the PetscScalar/PetscReal in updating the
    cached norm. If we push those down into the implementation these
    will become independent of PetscScalar/PetscReal
*/

/*@
   VecCopy - Copies a vector `y = x`

   Logically Collective

   Input Parameter:
.  x - the vector

   Output Parameter:
.  y - the copy

   Level: beginner

   Note:
   For default parallel PETSc vectors, both `x` and `y` must be distributed in
   the same manner; local copies are done.

   Developer Note:
   `PetscCheckSameTypeAndComm`(x,1,y,2) is not used on these vectors because we allow one
   of the vectors to be sequential and one to be parallel so long as both have the same
   local sizes. This is used in some internal functions in PETSc.

.seealso: [](chapter_vectors), `Vec`, `VecDuplicate()`
@*/
PetscErrorCode VecCopy(Vec x, Vec y)
{
  PetscBool flgs[4];
  PetscReal norms[4] = {0.0, 0.0, 0.0, 0.0};

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidHeaderSpecific(y, VEC_CLASSID, 2);
  PetscValidType(x, 1);
  PetscValidType(y, 2);
  if (x == y) PetscFunctionReturn(PETSC_SUCCESS);
  VecCheckSameLocalSize(x, 1, y, 2);
  VecCheckAssembled(x);
  PetscCall(VecSetErrorIfLocked(y, 2));

#if !defined(PETSC_USE_MIXED_PRECISION)
  for (PetscInt i = 0; i < 4; i++) PetscCall(PetscObjectComposedDataGetReal((PetscObject)x, NormIds[i], norms[i], flgs[i]));
#endif

  PetscCall(PetscLogEventBegin(VEC_Copy, x, y, 0, 0));
#if defined(PETSC_USE_MIXED_PRECISION)
  extern PetscErrorCode VecGetArray(Vec, double **);
  extern PetscErrorCode VecRestoreArray(Vec, double **);
  extern PetscErrorCode VecGetArray(Vec, float **);
  extern PetscErrorCode VecRestoreArray(Vec, float **);
  extern PetscErrorCode VecGetArrayRead(Vec, const double **);
  extern PetscErrorCode VecRestoreArrayRead(Vec, const double **);
  extern PetscErrorCode VecGetArrayRead(Vec, const float **);
  extern PetscErrorCode VecRestoreArrayRead(Vec, const float **);
  if ((((PetscObject)x)->precision == PETSC_PRECISION_SINGLE) && (((PetscObject)y)->precision == PETSC_PRECISION_DOUBLE)) {
    PetscInt     i, n;
    const float *xx;
    double      *yy;
    PetscCall(VecGetArrayRead(x, &xx));
    PetscCall(VecGetArray(y, &yy));
    PetscCall(VecGetLocalSize(x, &n));
    for (i = 0; i < n; i++) yy[i] = xx[i];
    PetscCall(VecRestoreArrayRead(x, &xx));
    PetscCall(VecRestoreArray(y, &yy));
  } else if ((((PetscObject)x)->precision == PETSC_PRECISION_DOUBLE) && (((PetscObject)y)->precision == PETSC_PRECISION_SINGLE)) {
    PetscInt      i, n;
    float        *yy;
    const double *xx;
    PetscCall(VecGetArrayRead(x, &xx));
    PetscCall(VecGetArray(y, &yy));
    PetscCall(VecGetLocalSize(x, &n));
    for (i = 0; i < n; i++) yy[i] = (float)xx[i];
    PetscCall(VecRestoreArrayRead(x, &xx));
    PetscCall(VecRestoreArray(y, &yy));
  } else PetscUseTypeMethod(x, copy, y);
#else
  PetscUseTypeMethod(x, copy, y);
#endif

  PetscCall(PetscObjectStateIncrease((PetscObject)y));
#if !defined(PETSC_USE_MIXED_PRECISION)
  for (PetscInt i = 0; i < 4; i++) {
    if (flgs[i]) PetscCall(PetscObjectComposedDataSetReal((PetscObject)y, NormIds[i], norms[i]));
  }
#endif

  PetscCall(PetscLogEventEnd(VEC_Copy, x, y, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSwap - Swaps the values between two vectors,  `x` and `y`.

   Logically Collective

   Input Parameters:
+  x  - the first vector
-  y  - the second vector

   Level: advanced

.seealso: [](chapter_vectors), `Vec`, `VecSet()`
@*/
PetscErrorCode VecSwap(Vec x, Vec y)
{
  PetscReal normxs[4], normys[4];
  PetscBool flgxs[4], flgys[4];

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidHeaderSpecific(y, VEC_CLASSID, 2);
  PetscValidType(x, 1);
  PetscValidType(y, 2);
  PetscCheckSameTypeAndComm(x, 1, y, 2);
  VecCheckSameSize(x, 1, y, 2);
  VecCheckAssembled(x);
  VecCheckAssembled(y);
  PetscCall(VecSetErrorIfLocked(x, 1));
  PetscCall(VecSetErrorIfLocked(y, 2));

  for (PetscInt i = 0; i < 4; i++) {
    PetscCall(PetscObjectComposedDataGetReal((PetscObject)x, NormIds[i], normxs[i], flgxs[i]));
    PetscCall(PetscObjectComposedDataGetReal((PetscObject)y, NormIds[i], normys[i], flgys[i]));
  }

  PetscCall(PetscLogEventBegin(VEC_Swap, x, y, 0, 0));
  PetscUseTypeMethod(x, swap, y);
  PetscCall(PetscLogEventEnd(VEC_Swap, x, y, 0, 0));

  PetscCall(PetscObjectStateIncrease((PetscObject)x));
  PetscCall(PetscObjectStateIncrease((PetscObject)y));
  for (PetscInt i = 0; i < 4; i++) {
    if (flgxs[i]) PetscCall(PetscObjectComposedDataSetReal((PetscObject)y, NormIds[i], normxs[i]));
    if (flgys[i]) PetscCall(PetscObjectComposedDataSetReal((PetscObject)x, NormIds[i], normys[i]));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  VecStashViewFromOptions - Processes command line options to determine if/how a `VecStash` object is to be viewed.

  Collective

  Input Parameters:
+ obj   - the `Vec` containing a stash
. bobj - optional other object that provides the prefix
- optionname - option to activate viewing

  Level: intermediate

  Developer Note:
  This cannot use `PetscObjectViewFromOptions()` because it takes a `Vec` as an argument but does not use `VecView()`

.seealso: [](chapter_vectors), `Vec`, `VecStashSetInitialSize()`
@*/
PetscErrorCode VecStashViewFromOptions(Vec obj, PetscObject bobj, const char optionname[])
{
  PetscViewer       viewer;
  PetscBool         flg;
  PetscViewerFormat format;
  char             *prefix;

  PetscFunctionBegin;
  prefix = bobj ? bobj->prefix : ((PetscObject)obj)->prefix;
  PetscCall(PetscOptionsGetViewer(PetscObjectComm((PetscObject)obj), ((PetscObject)obj)->options, prefix, optionname, &viewer, &format, &flg));
  if (flg) {
    PetscCall(PetscViewerPushFormat(viewer, format));
    PetscCall(VecStashView(obj, viewer));
    PetscCall(PetscViewerPopFormat(viewer));
    PetscCall(PetscViewerDestroy(&viewer));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecStashView - Prints the entries in the vector stash and block stash.

   Collective

   Input Parameters:
+  v - the vector
-  viewer - the viewer

   Level: advanced

.seealso: [](chapter_vectors), `Vec`, `VecSetBlockSize()`, `VecSetValues()`, `VecSetValuesBlocked()`
@*/
PetscErrorCode VecStashView(Vec v, PetscViewer viewer)
{
  PetscMPIInt rank;
  PetscInt    i, j;
  PetscBool   match;
  VecStash   *s;
  PetscScalar val;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  PetscCheckSameComm(v, 1, viewer, 2);

  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &match));
  PetscCheck(match, PETSC_COMM_SELF, PETSC_ERR_SUP, "Stash viewer only works with ASCII viewer not %s", ((PetscObject)v)->type_name);
  PetscCall(PetscViewerASCIIUseTabs(viewer, PETSC_FALSE));
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)v), &rank));
  s = &v->bstash;

  /* print block stash */
  PetscCall(PetscViewerASCIIPushSynchronized(viewer));
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d]Vector Block stash size %" PetscInt_FMT " block size %" PetscInt_FMT "\n", rank, s->n, s->bs));
  for (i = 0; i < s->n; i++) {
    PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] Element %" PetscInt_FMT " ", rank, s->idx[i]));
    for (j = 0; j < s->bs; j++) {
      val = s->array[i * s->bs + j];
#if defined(PETSC_USE_COMPLEX)
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "(%18.16e %18.16e) ", (double)PetscRealPart(val), (double)PetscImaginaryPart(val)));
#else
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%18.16e ", (double)val));
#endif
    }
    PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "\n"));
  }
  PetscCall(PetscViewerFlush(viewer));

  s = &v->stash;

  /* print basic stash */
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d]Vector stash size %" PetscInt_FMT "\n", rank, s->n));
  for (i = 0; i < s->n; i++) {
    val = s->array[i];
#if defined(PETSC_USE_COMPLEX)
    PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] Element %" PetscInt_FMT " (%18.16e %18.16e) ", rank, s->idx[i], (double)PetscRealPart(val), (double)PetscImaginaryPart(val)));
#else
    PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] Element %" PetscInt_FMT " %18.16e\n", rank, s->idx[i], (double)val));
#endif
  }
  PetscCall(PetscViewerFlush(viewer));
  PetscCall(PetscViewerASCIIPopSynchronized(viewer));
  PetscCall(PetscViewerASCIIUseTabs(viewer, PETSC_TRUE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscOptionsGetVec(PetscOptions options, const char prefix[], const char key[], Vec v, PetscBool *set)
{
  PetscInt     i, N, rstart, rend;
  PetscScalar *xx;
  PetscReal   *xreal;
  PetscBool    iset;

  PetscFunctionBegin;
  PetscCall(VecGetOwnershipRange(v, &rstart, &rend));
  PetscCall(VecGetSize(v, &N));
  PetscCall(PetscCalloc1(N, &xreal));
  PetscCall(PetscOptionsGetRealArray(options, prefix, key, xreal, &N, &iset));
  if (iset) {
    PetscCall(VecGetArray(v, &xx));
    for (i = rstart; i < rend; i++) xx[i - rstart] = xreal[i];
    PetscCall(VecRestoreArray(v, &xx));
  }
  PetscCall(PetscFree(xreal));
  if (set) *set = iset;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecGetLayout - get `PetscLayout` describing a vector layout

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameter:
.  map - the layout

   Level: developer

   Note:
   The layout determines what vector elements are contained on each MPI process

.seealso: [](chapter_vectors), `PetscLayout`, `Vec`, `VecGetSizes()`, `VecGetOwnershipRange()`, `VecGetOwnershipRanges()`
@*/
PetscErrorCode VecGetLayout(Vec x, PetscLayout *map)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscValidPointer(map, 2);
  *map = x->map;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetLayout - set `PetscLayout` describing vector layout

   Not Collective

   Input Parameters:
+  x - the vector
-  map - the layout

   Level: developer

   Note:
   It is normally only valid to replace the layout with a layout known to be equivalent.

.seealso: [](chapter_vectors), `Vec`, `PetscLayout`, `VecGetLayout()`, `VecGetSizes()`, `VecGetOwnershipRange()`, `VecGetOwnershipRanges()`
@*/
PetscErrorCode VecSetLayout(Vec x, PetscLayout map)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x, VEC_CLASSID, 1);
  PetscCall(PetscLayoutReference(map, &x->map));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode VecSetInf(Vec xin)
{
  // use of variables one and zero over just doing 1.0/0.0 is deliberate. MSVC complains that
  // we are dividing by zero in the latter case (ostensibly because dividing by 0 is UB, but
  // only for *integers* not floats).
  const PetscScalar one = 1.0, zero = 0.0, inf = one / zero;

  PetscFunctionBegin;
  if (xin->ops->set) {
    PetscUseTypeMethod(xin, set, inf);
  } else {
    PetscInt     n;
    PetscScalar *xx;

    PetscCall(VecGetLocalSize(xin, &n));
    PetscCall(VecGetArrayWrite(xin, &xx));
    for (PetscInt i = 0; i < n; ++i) xx[i] = inf;
    PetscCall(VecRestoreArrayWrite(xin, &xx));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     VecBindToCPU - marks a vector to temporarily stay on the CPU and perform computations on the CPU

  Logically collective

   Input Parameters:
+   v - the vector
-   flg - bind to the CPU if value of `PETSC_TRUE`

   Level: intermediate

.seelaso: [](chapter_vectors), `Vec`, `VecBoundToCPU()`
@*/
PetscErrorCode VecBindToCPU(Vec v, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidLogicalCollectiveBool(v, flg, 2);
#if defined(PETSC_HAVE_DEVICE)
  if (v->boundtocpu == flg) PetscFunctionReturn(PETSC_SUCCESS);
  v->boundtocpu = flg;
  PetscTryTypeMethod(v, bindtocpu, flg);
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     VecBoundToCPU - query if a vector is bound to the CPU

  Not collective

   Input Parameter:
.   v - the vector

   Output Parameter:
.   flg - the logical flag

   Level: intermediate

.seealso: [](chapter_vectors), `Vec`, `VecBindToCPU()`
@*/
PetscErrorCode VecBoundToCPU(Vec v, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidBoolPointer(flg, 2);
#if defined(PETSC_HAVE_DEVICE)
  *flg = v->boundtocpu;
#else
  *flg = PETSC_TRUE;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecSetBindingPropagates - Sets whether the state of being bound to the CPU for a GPU vector type propagates to child and some other associated objects

   Input Parameters:
+  v - the vector
-  flg - flag indicating whether the boundtocpu flag should be propagated

   Level: developer

   Notes:
   If the value of flg is set to true, then `VecDuplicate()` and `VecDuplicateVecs()` will bind created vectors to GPU if the input vector is bound to the CPU.
   The created vectors will also have their bindingpropagates flag set to true.

   Developer Note:
   If a `DMDA` has the `-dm_bind_below option` set to true, then vectors created by `DMCreateGlobalVector()` will have `VecSetBindingPropagates()` called on them to
   set their bindingpropagates flag to true.

.seealso: [](chapter_vectors), `Vec`, `MatSetBindingPropagates()`, `VecGetBindingPropagates()`
@*/
PetscErrorCode VecSetBindingPropagates(Vec v, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
#if defined(PETSC_HAVE_VIENNACL) || defined(PETSC_HAVE_CUDA) || defined(PETSC_HAVE_HIP)
  v->bindingpropagates = flg;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   VecGetBindingPropagates - Gets whether the state of being bound to the CPU for a GPU vector type propagates to child and some other associated objects

   Input Parameter:
.  v - the vector

   Output Parameter:
.  flg - flag indicating whether the boundtocpu flag will be propagated

   Level: developer

.seealso: [](chapter_vectors), `Vec`, `VecSetBindingPropagates()`
@*/
PetscErrorCode VecGetBindingPropagates(Vec v, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidBoolPointer(flg, 2);
#if defined(PETSC_HAVE_VIENNACL) || defined(PETSC_HAVE_CUDA) || defined(PETSC_HAVE_HIP)
  *flg = v->bindingpropagates;
#else
  *flg = PETSC_FALSE;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  VecSetPinnedMemoryMin - Set the minimum data size for which pinned memory will be used for host (CPU) allocations.

  Logically Collective

  Input Parameters:
+  v    - the vector
-  mbytes - minimum data size in bytes

  Options Database Key:
. -vec_pinned_memory_min <size> - minimum size (in bytes) for an allocation to use pinned memory on host.

  Level: developer

  Note:
  Specifying -1 ensures that pinned memory will never be used.

.seealso: [](chapter_vectors), `Vec`, `VecGetPinnedMemoryMin()`
@*/
PetscErrorCode VecSetPinnedMemoryMin(Vec v, size_t mbytes)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
#if PetscDefined(HAVE_DEVICE)
  v->minimum_bytes_pinned_memory = mbytes;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  VecGetPinnedMemoryMin - Get the minimum data size for which pinned memory will be used for host (CPU) allocations.

  Logically Collective

  Input Parameter:
.  v    - the vector

  Output Parameter:
.  mbytes - minimum data size in bytes

  Level: developer

.seealso: [](chapter_vectors), `Vec`, `VecSetPinnedMemoryMin()`
@*/
PetscErrorCode VecGetPinnedMemoryMin(Vec v, size_t *mbytes)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidPointer(mbytes, 2);
#if PetscDefined(HAVE_DEVICE)
  *mbytes = v->minimum_bytes_pinned_memory;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  VecGetOffloadMask - Get the offload mask of a `Vec`

  Not Collective

  Input Parameter:
.   v - the vector

  Output Parameter:
.   mask - corresponding `PetscOffloadMask` enum value.

   Level: intermediate

.seealso: [](chapter_vectors), `Vec`, `VecCreateSeqCUDA()`, `VecCreateSeqViennaCL()`, `VecGetArray()`, `VecGetType()`
@*/
PetscErrorCode VecGetOffloadMask(Vec v, PetscOffloadMask *mask)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, VEC_CLASSID, 1);
  PetscValidPointer(mask, 2);
  *mask = v->offloadmask;
  PetscFunctionReturn(PETSC_SUCCESS);
}

#if !defined(PETSC_HAVE_VIENNACL)
PETSC_EXTERN PetscErrorCode VecViennaCLGetCLContext(Vec v, PETSC_UINTPTR_T *ctx)
{
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "PETSc must be configured with --with-opencl to get a Vec's cl_context");
}

PETSC_EXTERN PetscErrorCode VecViennaCLGetCLQueue(Vec v, PETSC_UINTPTR_T *queue)
{
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "PETSc must be configured with --with-opencl to get a Vec's cl_command_queue");
}

PETSC_EXTERN PetscErrorCode VecViennaCLGetCLMem(Vec v, PETSC_UINTPTR_T *queue)
{
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "PETSc must be configured with --with-opencl to get a Vec's cl_mem");
}

PETSC_EXTERN PetscErrorCode VecViennaCLGetCLMemRead(Vec v, PETSC_UINTPTR_T *queue)
{
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "PETSc must be configured with --with-opencl to get a Vec's cl_mem");
}

PETSC_EXTERN PetscErrorCode VecViennaCLGetCLMemWrite(Vec v, PETSC_UINTPTR_T *queue)
{
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "PETSc must be configured with --with-opencl to get a Vec's cl_mem");
}

PETSC_EXTERN PetscErrorCode VecViennaCLRestoreCLMemWrite(Vec v)
{
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "PETSc must be configured with --with-opencl to restore a Vec's cl_mem");
}
#endif

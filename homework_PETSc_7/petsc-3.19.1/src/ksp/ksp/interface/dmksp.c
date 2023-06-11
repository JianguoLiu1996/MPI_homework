#include <petsc/private/dmimpl.h>
#include <petsc/private/kspimpl.h> /*I "petscksp.h" I*/
#include <petscdm.h>

static PetscErrorCode DMKSPDestroy(DMKSP *kdm)
{
  PetscFunctionBegin;
  if (!*kdm) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific((*kdm), DMKSP_CLASSID, 1);
  if (--((PetscObject)(*kdm))->refct > 0) {
    *kdm = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if ((*kdm)->ops->destroy) PetscCall(((*kdm)->ops->destroy)(kdm));
  PetscCall(PetscHeaderDestroy(kdm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMKSPCreate(MPI_Comm comm, DMKSP *kdm)
{
  PetscFunctionBegin;
  PetscCall(KSPInitializePackage());
  PetscCall(PetscHeaderCreate(*kdm, DMKSP_CLASSID, "DMKSP", "DMKSP", "DMKSP", comm, DMKSPDestroy, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Attaches the DMKSP to the coarse level.
 * Under what conditions should we copy versus duplicate?
 */
static PetscErrorCode DMCoarsenHook_DMKSP(DM dm, DM dmc, void *ctx)
{
  PetscFunctionBegin;
  PetscCall(DMCopyDMKSP(dm, dmc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Attaches the DMKSP to the coarse level.
 * Under what conditions should we copy versus duplicate?
 */
static PetscErrorCode DMRefineHook_DMKSP(DM dm, DM dmc, void *ctx)
{
  PetscFunctionBegin;
  PetscCall(DMCopyDMKSP(dm, dmc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMKSPCopy - copies the information in a `DMKSP` to another `DMKSP`

   Not Collective

   Input Parameters:
+  kdm - Original `DMKSP`
-  nkdm - `DMKSP` to receive the data, created with `DMKSPCreate()`

   Level: developer

.seealso: [](chapter_ksp), `DMKSPCreate()`, `DMKSPDestroy()`
@*/
PetscErrorCode DMKSPCopy(DMKSP kdm, DMKSP nkdm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(kdm, DMKSP_CLASSID, 1);
  PetscValidHeaderSpecific(nkdm, DMKSP_CLASSID, 2);
  nkdm->ops->computeoperators    = kdm->ops->computeoperators;
  nkdm->ops->computerhs          = kdm->ops->computerhs;
  nkdm->ops->computeinitialguess = kdm->ops->computeinitialguess;
  nkdm->ops->destroy             = kdm->ops->destroy;
  nkdm->ops->duplicate           = kdm->ops->duplicate;

  nkdm->operatorsctx    = kdm->operatorsctx;
  nkdm->rhsctx          = kdm->rhsctx;
  nkdm->initialguessctx = kdm->initialguessctx;
  nkdm->data            = kdm->data;
  /* nkdm->originaldm   = kdm->originaldm; */ /* No need since nkdm->originaldm will be immediately updated in caller DMGetDMKSPWrite */

  nkdm->fortran_func_pointers[0] = kdm->fortran_func_pointers[0];
  nkdm->fortran_func_pointers[1] = kdm->fortran_func_pointers[1];
  nkdm->fortran_func_pointers[2] = kdm->fortran_func_pointers[2];

  /* implementation specific copy hooks */
  PetscTryTypeMethod(kdm, duplicate, nkdm);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMGetDMKSP - get read-only private `DMKSP` context from a `DM`

   Logically Collective

   Input Parameter:
.  dm - `DM` used with a `KSP`

   Output Parameter:
.  snesdm - private `DMKSP` context

   Level: developer

   Note:
   Use `DMGetDMKSPWrite()` if write access is needed. The DMKSPSetXXX API should be used wherever possible.

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMGetDMKSPWrite()`
@*/
PetscErrorCode DMGetDMKSP(DM dm, DMKSP *kspdm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  *kspdm = (DMKSP)dm->dmksp;
  if (!*kspdm) {
    PetscCall(PetscInfo(dm, "Creating new DMKSP\n"));
    PetscCall(DMKSPCreate(PetscObjectComm((PetscObject)dm), kspdm));
    dm->dmksp            = (PetscObject)*kspdm;
    (*kspdm)->originaldm = dm;
    PetscCall(DMCoarsenHookAdd(dm, DMCoarsenHook_DMKSP, NULL, NULL));
    PetscCall(DMRefineHookAdd(dm, DMRefineHook_DMKSP, NULL, NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMGetDMKSPWrite - get write access to private `DMKSP` context from a `DM`

   Logically Collective

   Input Parameter:
.  dm - `DM` used with a `KSP`

   Output Parameter:
.  kspdm - private `DMKSP` context

   Level: developer

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMGetDMKSP()`
@*/
PetscErrorCode DMGetDMKSPWrite(DM dm, DMKSP *kspdm)
{
  DMKSP kdm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMGetDMKSP(dm, &kdm));
  PetscCheck(kdm->originaldm, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "DMKSP has a NULL originaldm");
  if (kdm->originaldm != dm) { /* Copy on write */
    DMKSP oldkdm = kdm;
    PetscCall(PetscInfo(dm, "Copying DMKSP due to write\n"));
    PetscCall(DMKSPCreate(PetscObjectComm((PetscObject)dm), &kdm));
    PetscCall(DMKSPCopy(oldkdm, kdm));
    PetscCall(DMKSPDestroy((DMKSP *)&dm->dmksp));
    dm->dmksp       = (PetscObject)kdm;
    kdm->originaldm = dm;
  }
  *kspdm = kdm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMCopyDMKSP - copies a `DM` `DMKSP` context to a new `DM`

   Logically Collective

   Input Parameters:
+  dmsrc - `DM` to obtain context from
-  dmdest - `DM` to add context to

   Level: developer

   Note:
   The context is copied by reference. This function does not ensure that a context exists.

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMGetDMKSP()`, `KSPSetDM()`
@*/
PetscErrorCode DMCopyDMKSP(DM dmsrc, DM dmdest)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(dmsrc, DM_CLASSID, 1);
  PetscValidHeaderSpecific(dmdest, DM_CLASSID, 2);
  PetscCall(DMKSPDestroy((DMKSP *)&dmdest->dmksp));
  dmdest->dmksp = dmsrc->dmksp;
  PetscCall(PetscObjectReference(dmdest->dmksp));
  PetscCall(DMCoarsenHookAdd(dmdest, DMCoarsenHook_DMKSP, NULL, NULL));
  PetscCall(DMRefineHookAdd(dmdest, DMRefineHook_DMKSP, NULL, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMKSPSetComputeOperators - set `KSP` matrix evaluation function

   Not Collective

   Input Parameters:
+  dm - `DM` to be used with `KSP`
.  func - matrix evaluation function,  for calling sequence see `KSPSetComputeOperators()`
-  ctx - context for matrix evaluation

   Level: developer

   Note:
   `KSPSetComputeOperators()` is normally used, but it calls this function internally because the user context is actually
   associated with the `DM`.  This makes the interface consistent regardless of whether the user interacts with a `DM` or
   not.

   Developer Note:
   If `DM` took a more central role at some later date, this could become the primary method of setting the matrix.

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMKSPSetContext()`, `DMKSPGetComputeOperators()`, `KSPSetOperators()`
@*/
PetscErrorCode DMKSPSetComputeOperators(DM dm, PetscErrorCode (*func)(KSP, Mat, Mat, void *), void *ctx)
{
  DMKSP kdm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMGetDMKSPWrite(dm, &kdm));
  if (func) kdm->ops->computeoperators = func;
  if (ctx) kdm->operatorsctx = ctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMKSPGetComputeOperators - get `KSP` matrix evaluation function

   Not Collective

   Input Parameter:
.  dm - `DM` used with a `KSP`

   Output Parameters:
+  func - matrix evaluation function,  for calling sequence see `KSPSetComputeOperators()`
-  ctx - context for matrix evaluation

   Level: developer

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMKSPSetContext()`, `KSPSetComputeOperators()`, `DMKSPSetComputeOperators()`
@*/
PetscErrorCode DMKSPGetComputeOperators(DM dm, PetscErrorCode (**func)(KSP, Mat, Mat, void *), void *ctx)
{
  DMKSP kdm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMGetDMKSP(dm, &kdm));
  if (func) *func = kdm->ops->computeoperators;
  if (ctx) *(void **)ctx = kdm->operatorsctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMKSPSetComputeRHS - set `KSP` right hand side evaluation function

   Not Collective

   Input Parameters:
+  dm - `DM` used with a `KSP`
.  func - right hand side evaluation function,  for calling sequence see `KSPSetComputeRHS()`
-  ctx - context for right hand side evaluation

   Level: developer

   Note:
   `KSPSetComputeRHS()` is normally used, but it calls this function internally because the user context is actually
   associated with the `DM`.  This makes the interface consistent regardless of whether the user interacts with a `DM` or
   not.

   Developer Note:
   If `DM` took a more central role at some later date, this could become the primary method of setting the matrix.

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMKSPSetContext()`, `DMKSPGetComputeRHS()`, `KSPSetRHS()`
@*/
PetscErrorCode DMKSPSetComputeRHS(DM dm, PetscErrorCode (*func)(KSP, Vec, void *), void *ctx)
{
  DMKSP kdm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMGetDMKSPWrite(dm, &kdm));
  if (func) kdm->ops->computerhs = func;
  if (ctx) kdm->rhsctx = ctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMKSPSetComputeInitialGuess - set `KSP` initial guess evaluation function

   Not Collective

   Input Parameters:
+  dm - `DM` to be used with `KSP`
.  func - initial guess evaluation function,  for calling sequence see `KSPSetComputeInitialGuess()`
-  ctx - context for right hand side evaluation

   Level: developer

   Note:
   `KSPSetComputeInitialGuess()` is normally used, but it calls this function internally because the user context is actually
   associated with the `DM`.

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMKSPSetContext()`, `DMKSPGetComputeRHS()`, `KSPSetRHS()`
@*/
PetscErrorCode DMKSPSetComputeInitialGuess(DM dm, PetscErrorCode (*func)(KSP, Vec, void *), void *ctx)
{
  DMKSP kdm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMGetDMKSPWrite(dm, &kdm));
  if (func) kdm->ops->computeinitialguess = func;
  if (ctx) kdm->initialguessctx = ctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMKSPGetComputeRHS - get `KSP` right hand side evaluation function

   Not Collective

   Input Parameter:
.  dm - `DM` to be used with `KSP`

   Output Parameters:
+  func - right hand side evaluation function,  for calling sequence see `KSPSetComputeRHS()`
-  ctx - context for right hand side evaluation

   Level: advanced

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMKSPSetContext()`, `KSPSetComputeRHS()`, `DMKSPSetComputeRHS()`
@*/
PetscErrorCode DMKSPGetComputeRHS(DM dm, PetscErrorCode (**func)(KSP, Vec, void *), void *ctx)
{
  DMKSP kdm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMGetDMKSP(dm, &kdm));
  if (func) *func = kdm->ops->computerhs;
  if (ctx) *(void **)ctx = kdm->rhsctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMKSPGetComputeInitialGuess - get `KSP` initial guess evaluation function

   Not Collective

   Input Parameter:
.  dm - `DM` used with a `KSP`

   Output Parameters:
+  func - initial guess evaluation function,  for calling sequence see `KSPSetComputeInitialGuess()`
-  ctx - context for right hand side evaluation

   Level: advanced

.seealso: [](chapter_ksp), `DMKSP`, `DM`, `KSP`, `DMKSPSetContext()`, `KSPSetComputeRHS()`, `DMKSPSetComputeRHS()`
@*/
PetscErrorCode DMKSPGetComputeInitialGuess(DM dm, PetscErrorCode (**func)(KSP, Vec, void *), void *ctx)
{
  DMKSP kdm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMGetDMKSP(dm, &kdm));
  if (func) *func = kdm->ops->computeinitialguess;
  if (ctx) *(void **)ctx = kdm->initialguessctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

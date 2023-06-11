
#include <../src/ts/impls/implicit/glle/glle.h> /*I  "petscts.h" I*/

static PetscFunctionList TSGLLEAdaptList;
static PetscBool         TSGLLEAdaptPackageInitialized;
static PetscBool         TSGLLEAdaptRegisterAllCalled;
static PetscClassId      TSGLLEADAPT_CLASSID;

struct _TSGLLEAdaptOps {
  PetscErrorCode (*choose)(TSGLLEAdapt, PetscInt, const PetscInt[], const PetscReal[], const PetscReal[], PetscInt, PetscReal, PetscReal, PetscInt *, PetscReal *, PetscBool *);
  PetscErrorCode (*destroy)(TSGLLEAdapt);
  PetscErrorCode (*view)(TSGLLEAdapt, PetscViewer);
  PetscErrorCode (*setfromoptions)(TSGLLEAdapt, PetscOptionItems *);
};

struct _p_TSGLLEAdapt {
  PETSCHEADER(struct _TSGLLEAdaptOps);
  void *data;
};

PETSC_EXTERN PetscErrorCode TSGLLEAdaptCreate_None(TSGLLEAdapt);
PETSC_EXTERN PetscErrorCode TSGLLEAdaptCreate_Size(TSGLLEAdapt);
PETSC_EXTERN PetscErrorCode TSGLLEAdaptCreate_Both(TSGLLEAdapt);

/*@C
   TSGLLEAdaptRegister -  adds a `TSGLLEAdapt` implementation

   Not Collective

   Input Parameters:
+  sname - name of user-defined adaptivity scheme
-  function - routine to create method context

   Level: advanced

   Note:
   `TSGLLEAdaptRegister()` may be called multiple times to add several user-defined families.

   Sample usage:
.vb
   TSGLLEAdaptRegister("my_scheme",MySchemeCreate);
.ve

   Then, your scheme can be chosen with the procedural interface via
$     TSGLLEAdaptSetType(ts,"my_scheme")
   or at runtime via the option
$     -ts_adapt_type my_scheme

.seealso: [](chapter_ts), `TSGLLE`, `TSGLLEAdapt`, `TSGLLEAdaptRegisterAll()`
@*/
PetscErrorCode TSGLLEAdaptRegister(const char sname[], PetscErrorCode (*function)(TSGLLEAdapt))
{
  PetscFunctionBegin;
  PetscCall(TSGLLEAdaptInitializePackage());
  PetscCall(PetscFunctionListAdd(&TSGLLEAdaptList, sname, function));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  TSGLLEAdaptRegisterAll - Registers all of the adaptivity schemes in `TSGLLEAdapt`

  Not Collective

  Level: advanced

.seealso: [](chapter_ts), `TSGLLEAdapt`, `TSGLLE`, `TSGLLEAdaptRegisterDestroy()`
@*/
PetscErrorCode TSGLLEAdaptRegisterAll(void)
{
  PetscFunctionBegin;
  if (TSGLLEAdaptRegisterAllCalled) PetscFunctionReturn(PETSC_SUCCESS);
  TSGLLEAdaptRegisterAllCalled = PETSC_TRUE;
  PetscCall(TSGLLEAdaptRegister(TSGLLEADAPT_NONE, TSGLLEAdaptCreate_None));
  PetscCall(TSGLLEAdaptRegister(TSGLLEADAPT_SIZE, TSGLLEAdaptCreate_Size));
  PetscCall(TSGLLEAdaptRegister(TSGLLEADAPT_BOTH, TSGLLEAdaptCreate_Both));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  TSGLLEFinalizePackage - This function destroys everything in the `TSGLLE` package. It is
  called from `PetscFinalize()`.

  Level: developer

.seealso: [](chapter_ts), `PetscFinalize()`, `TSGLLEAdapt`, `TSGLLEAdaptInitializePackage()`
@*/
PetscErrorCode TSGLLEAdaptFinalizePackage(void)
{
  PetscFunctionBegin;
  PetscCall(PetscFunctionListDestroy(&TSGLLEAdaptList));
  TSGLLEAdaptPackageInitialized = PETSC_FALSE;
  TSGLLEAdaptRegisterAllCalled  = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  TSGLLEAdaptInitializePackage - This function initializes everything in the `TSGLLEAdapt` package. It is
  called from `TSInitializePackage()`.

  Level: developer

.seealso: [](chapter_ts), `PetscInitialize()`, `TSGLLEAdapt`, `TSGLLEAdaptFinalizePackage()`
@*/
PetscErrorCode TSGLLEAdaptInitializePackage(void)
{
  PetscFunctionBegin;
  if (TSGLLEAdaptPackageInitialized) PetscFunctionReturn(PETSC_SUCCESS);
  TSGLLEAdaptPackageInitialized = PETSC_TRUE;
  PetscCall(PetscClassIdRegister("TSGLLEAdapt", &TSGLLEADAPT_CLASSID));
  PetscCall(TSGLLEAdaptRegisterAll());
  PetscCall(PetscRegisterFinalize(TSGLLEAdaptFinalizePackage));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptSetType(TSGLLEAdapt adapt, TSGLLEAdaptType type)
{
  PetscErrorCode (*r)(TSGLLEAdapt);

  PetscFunctionBegin;
  PetscCall(PetscFunctionListFind(TSGLLEAdaptList, type, &r));
  PetscCheck(r, PETSC_COMM_SELF, PETSC_ERR_ARG_UNKNOWN_TYPE, "Unknown TSGLLEAdapt type \"%s\" given", type);
  if (((PetscObject)adapt)->type_name) PetscUseTypeMethod(adapt, destroy);
  PetscCall((*r)(adapt));
  PetscCall(PetscObjectChangeTypeName((PetscObject)adapt, type));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptSetOptionsPrefix(TSGLLEAdapt adapt, const char prefix[])
{
  PetscFunctionBegin;
  PetscCall(PetscObjectSetOptionsPrefix((PetscObject)adapt, prefix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptView(TSGLLEAdapt adapt, PetscViewer viewer)
{
  PetscBool iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscObjectPrintClassNamePrefixType((PetscObject)adapt, viewer));
    if (adapt->ops->view) {
      PetscCall(PetscViewerASCIIPushTab(viewer));
      PetscUseTypeMethod(adapt, view, viewer);
      PetscCall(PetscViewerASCIIPopTab(viewer));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptDestroy(TSGLLEAdapt *adapt)
{
  PetscFunctionBegin;
  if (!*adapt) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific(*adapt, TSGLLEADAPT_CLASSID, 1);
  if (--((PetscObject)(*adapt))->refct > 0) {
    *adapt = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscTryTypeMethod((*adapt), destroy);
  PetscCall(PetscHeaderDestroy(adapt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptSetFromOptions(TSGLLEAdapt adapt, PetscOptionItems *PetscOptionsObject)
{
  char      type[256] = TSGLLEADAPT_BOTH;
  PetscBool flg;

  PetscFunctionBegin;
  /* This should use PetscOptionsBegin() if/when this becomes an object used outside of TSGLLE, but currently this
  * function can only be called from inside TSSetFromOptions_GLLE()  */
  PetscOptionsHeadBegin(PetscOptionsObject, "TSGLLE Adaptivity options");
  PetscCall(PetscOptionsFList("-ts_adapt_type", "Algorithm to use for adaptivity", "TSGLLEAdaptSetType", TSGLLEAdaptList, ((PetscObject)adapt)->type_name ? ((PetscObject)adapt)->type_name : type, type, sizeof(type), &flg));
  if (flg || !((PetscObject)adapt)->type_name) PetscCall(TSGLLEAdaptSetType(adapt, type));
  PetscTryTypeMethod(adapt, setfromoptions, PetscOptionsObject);
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptChoose(TSGLLEAdapt adapt, PetscInt n, const PetscInt orders[], const PetscReal errors[], const PetscReal cost[], PetscInt cur, PetscReal h, PetscReal tleft, PetscInt *next_sc, PetscReal *next_h, PetscBool *finish)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(adapt, TSGLLEADAPT_CLASSID, 1);
  PetscValidIntPointer(orders, 3);
  PetscValidRealPointer(errors, 4);
  PetscValidRealPointer(cost, 5);
  PetscValidIntPointer(next_sc, 9);
  PetscValidRealPointer(next_h, 10);
  PetscValidBoolPointer(finish, 11);
  PetscUseTypeMethod(adapt, choose, n, orders, errors, cost, cur, h, tleft, next_sc, next_h, finish);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptCreate(MPI_Comm comm, TSGLLEAdapt *inadapt)
{
  TSGLLEAdapt adapt;

  PetscFunctionBegin;
  *inadapt = NULL;
  PetscCall(PetscHeaderCreate(adapt, TSGLLEADAPT_CLASSID, "TSGLLEAdapt", "General Linear adaptivity", "TS", comm, TSGLLEAdaptDestroy, TSGLLEAdaptView));
  *inadapt = adapt;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   Implementations
*/

static PetscErrorCode TSGLLEAdaptDestroy_JustFree(TSGLLEAdapt adapt)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(adapt->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* -------------------------------- None ----------------------------------- */
typedef struct {
  PetscInt  scheme;
  PetscReal h;
} TSGLLEAdapt_None;

static PetscErrorCode TSGLLEAdaptChoose_None(TSGLLEAdapt adapt, PetscInt n, const PetscInt orders[], const PetscReal errors[], const PetscReal cost[], PetscInt cur, PetscReal h, PetscReal tleft, PetscInt *next_sc, PetscReal *next_h, PetscBool *finish)
{
  PetscFunctionBegin;
  *next_sc = cur;
  *next_h  = h;
  if (*next_h > tleft) {
    *finish = PETSC_TRUE;
    *next_h = tleft;
  } else *finish = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptCreate_None(TSGLLEAdapt adapt)
{
  TSGLLEAdapt_None *a;

  PetscFunctionBegin;
  PetscCall(PetscNew(&a));
  adapt->data         = (void *)a;
  adapt->ops->choose  = TSGLLEAdaptChoose_None;
  adapt->ops->destroy = TSGLLEAdaptDestroy_JustFree;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* -------------------------------- Size ----------------------------------- */
typedef struct {
  PetscReal desired_h;
} TSGLLEAdapt_Size;

static PetscErrorCode TSGLLEAdaptChoose_Size(TSGLLEAdapt adapt, PetscInt n, const PetscInt orders[], const PetscReal errors[], const PetscReal cost[], PetscInt cur, PetscReal h, PetscReal tleft, PetscInt *next_sc, PetscReal *next_h, PetscBool *finish)
{
  TSGLLEAdapt_Size *sz  = (TSGLLEAdapt_Size *)adapt->data;
  PetscReal         dec = 0.2, inc = 5.0, safe = 0.9, optimal, last_desired_h;

  PetscFunctionBegin;
  *next_sc = cur;
  optimal  = PetscPowReal((PetscReal)errors[cur], (PetscReal)-1. / (safe * orders[cur]));
  /* Step sizes oscillate when there is no smoothing.  Here we use a geometric mean of the current step size and the
  * one that would have been taken (without smoothing) on the last step. */
  last_desired_h = sz->desired_h;
  sz->desired_h  = h * PetscMax(dec, PetscMin(inc, optimal)); /* Trim to [dec,inc] */

  /* Normally only happens on the first step */
  if (last_desired_h > 1e-14) *next_h = PetscSqrtReal(last_desired_h * sz->desired_h);
  else *next_h = sz->desired_h;

  if (*next_h > tleft) {
    *finish = PETSC_TRUE;
    *next_h = tleft;
  } else *finish = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptCreate_Size(TSGLLEAdapt adapt)
{
  TSGLLEAdapt_Size *a;

  PetscFunctionBegin;
  PetscCall(PetscNew(&a));
  adapt->data         = (void *)a;
  adapt->ops->choose  = TSGLLEAdaptChoose_Size;
  adapt->ops->destroy = TSGLLEAdaptDestroy_JustFree;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* -------------------------------- Both ----------------------------------- */
typedef struct {
  PetscInt  count_at_order;
  PetscReal desired_h;
} TSGLLEAdapt_Both;

static PetscErrorCode TSGLLEAdaptChoose_Both(TSGLLEAdapt adapt, PetscInt n, const PetscInt orders[], const PetscReal errors[], const PetscReal cost[], PetscInt cur, PetscReal h, PetscReal tleft, PetscInt *next_sc, PetscReal *next_h, PetscBool *finish)
{
  TSGLLEAdapt_Both *both = (TSGLLEAdapt_Both *)adapt->data;
  PetscReal         dec = 0.2, inc = 5.0, safe = 0.9;
  struct {
    PetscInt  id;
    PetscReal h, eff;
  } best = {-1, 0, 0}, trial = {-1, 0, 0}, current = {-1, 0, 0};
  PetscInt i;

  PetscFunctionBegin;
  for (i = 0; i < n; i++) {
    PetscReal optimal;
    trial.id  = i;
    optimal   = PetscPowReal((PetscReal)errors[i], (PetscReal)-1. / (safe * orders[i]));
    trial.h   = h * optimal;
    trial.eff = trial.h / cost[i];
    if (trial.eff > best.eff) PetscCall(PetscArraycpy(&best, &trial, 1));
    if (i == cur) PetscCall(PetscArraycpy(&current, &trial, 1));
  }
  /* Only switch orders if the scheme offers significant benefits over the current one.
  When the scheme is not changing, only change step size if it offers significant benefits. */
  if (best.eff < 1.2 * current.eff || both->count_at_order < orders[cur] + 2) {
    PetscReal last_desired_h;
    *next_sc        = current.id;
    last_desired_h  = both->desired_h;
    both->desired_h = PetscMax(h * dec, PetscMin(h * inc, current.h));
    *next_h         = (both->count_at_order > 0) ? PetscSqrtReal(last_desired_h * both->desired_h) : both->desired_h;
    both->count_at_order++;
  } else {
    PetscReal rat        = cost[best.id] / cost[cur];
    *next_sc             = best.id;
    *next_h              = PetscMax(h * rat * dec, PetscMin(h * rat * inc, best.h));
    both->count_at_order = 0;
    both->desired_h      = best.h;
  }

  if (*next_h > tleft) {
    *finish = PETSC_TRUE;
    *next_h = tleft;
  } else *finish = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSGLLEAdaptCreate_Both(TSGLLEAdapt adapt)
{
  TSGLLEAdapt_Both *a;

  PetscFunctionBegin;
  PetscCall(PetscNew(&a));
  adapt->data         = (void *)a;
  adapt->ops->choose  = TSGLLEAdaptChoose_Both;
  adapt->ops->destroy = TSGLLEAdaptDestroy_JustFree;
  PetscFunctionReturn(PETSC_SUCCESS);
}

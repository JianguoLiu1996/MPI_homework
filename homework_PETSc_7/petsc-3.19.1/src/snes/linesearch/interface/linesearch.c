#include <petsc/private/linesearchimpl.h> /*I "petscsnes.h" I*/

PetscBool         SNESLineSearchRegisterAllCalled = PETSC_FALSE;
PetscFunctionList SNESLineSearchList              = NULL;

PetscClassId  SNESLINESEARCH_CLASSID;
PetscLogEvent SNESLINESEARCH_Apply;

/*@
   SNESLineSearchMonitorCancel - Clears all the monitor functions for a `SNESLineSearch` object.

   Logically Collective

   Input Parameter:
.  ls - the `SNESLineSearch` context

   Options Database Key:
.  -snes_linesearch_monitor_cancel - cancels all monitors that have been hardwired
    into a code by calls to `SNESLineSearchMonitorSet()`, but does not cancel those
    set via the options database

   Level: advanced

   Notes:
   There is no way to clear one specific monitor from a `SNESLineSearch` object.

   This does not clear the monitor set with `SNESLineSearchSetDefaultMonitor()` use `SNESLineSearchSetDefaultMonitor`(ls,NULL) to cancel
   that one.

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchMonitorDefault()`, `SNESLineSearchMonitorSet()`
@*/
PetscErrorCode SNESLineSearchMonitorCancel(SNESLineSearch ls)
{
  PetscInt i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ls, SNESLINESEARCH_CLASSID, 1);
  for (i = 0; i < ls->numbermonitors; i++) {
    if (ls->monitordestroy[i]) PetscCall((*ls->monitordestroy[i])(&ls->monitorcontext[i]));
  }
  ls->numbermonitors = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchMonitor - runs the user provided monitor routines, if they exist

   Collective

   Input Parameter:
.  ls - the linesearch object

   Level: developer

   Note:
   This routine is called by the `SNES` implementations.
   It does not typically need to be called by the user.

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchMonitorSet()`
@*/
PetscErrorCode SNESLineSearchMonitor(SNESLineSearch ls)
{
  PetscInt i, n = ls->numbermonitors;

  PetscFunctionBegin;
  for (i = 0; i < n; i++) PetscCall((*ls->monitorftns[i])(ls, ls->monitorcontext[i]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchMonitorSet - Sets an ADDITIONAL function that is to be used at every
   iteration of the nonlinear solver to display the iteration's
   progress.

   Logically Collective

   Input Parameters:
+  ls - the `SNESLineSearch` context
.  f - the monitor function
.  mctx - [optional] user-defined context for private data for the
          monitor routine (use `NULL` if no context is desired)
-  monitordestroy - [optional] routine that frees monitor context
          (may be `NULL`)

   Level: intermediate

   Note:
   Several different monitoring routines may be set by calling
   `SNESLineSearchMonitorSet()` multiple times; all will be called in the
   order in which they were set.

   Fortran Note:
   Only a single monitor function can be set for each `SNESLineSearch` object

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchMonitorDefault()`, `SNESLineSearchMonitorCancel()`
@*/
PetscErrorCode SNESLineSearchMonitorSet(SNESLineSearch ls, PetscErrorCode (*f)(SNESLineSearch, void *), void *mctx, PetscErrorCode (*monitordestroy)(void **))
{
  PetscInt  i;
  PetscBool identical;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ls, SNESLINESEARCH_CLASSID, 1);
  for (i = 0; i < ls->numbermonitors; i++) {
    PetscCall(PetscMonitorCompare((PetscErrorCode(*)(void))f, mctx, monitordestroy, (PetscErrorCode(*)(void))ls->monitorftns[i], ls->monitorcontext[i], ls->monitordestroy[i], &identical));
    if (identical) PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCheck(ls->numbermonitors < MAXSNESLSMONITORS, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Too many monitors set");
  ls->monitorftns[ls->numbermonitors]      = f;
  ls->monitordestroy[ls->numbermonitors]   = monitordestroy;
  ls->monitorcontext[ls->numbermonitors++] = (void *)mctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchMonitorSolutionUpdate - Monitors each update of the function value the linesearch tries

   Collective

   Input Parameters:
+  ls - the `SNES` linesearch object
-  vf - the context for the monitor, in this case it is an `PetscViewerAndFormat`

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESMonitorSet()`, `SNESMonitorSolution()`
@*/
PetscErrorCode SNESLineSearchMonitorSolutionUpdate(SNESLineSearch ls, PetscViewerAndFormat *vf)
{
  PetscViewer viewer = vf->viewer;
  Vec         Y, W, G;

  PetscFunctionBegin;
  PetscCall(SNESLineSearchGetVecs(ls, NULL, NULL, &Y, &W, &G));
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LineSearch attempted update to solution \n"));
  PetscCall(VecView(Y, viewer));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LineSearch attempted new solution \n"));
  PetscCall(VecView(W, viewer));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LineSearch attempted updated function value\n"));
  PetscCall(VecView(G, viewer));
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchCreate - Creates the line search context.

   Logically Collective

   Input Parameter:
.  comm - MPI communicator for the line search (typically from the associated `SNES` context).

   Output Parameter:
.  outlinesearch - the new linesearch context

   Level: developer

   Note:
   The preferred calling sequence for users is to use `SNESGetLineSearch()` to acquire the `SNESLineSearch` instance
   already associated with the `SNES`.

.seealso: `SNESLineSearch`, `LineSearchDestroy()`, `SNESGetLineSearch()`
@*/
PetscErrorCode SNESLineSearchCreate(MPI_Comm comm, SNESLineSearch *outlinesearch)
{
  SNESLineSearch linesearch;

  PetscFunctionBegin;
  PetscValidPointer(outlinesearch, 2);
  PetscCall(SNESInitializePackage());
  *outlinesearch = NULL;

  PetscCall(PetscHeaderCreate(linesearch, SNESLINESEARCH_CLASSID, "SNESLineSearch", "Linesearch", "SNESLineSearch", comm, SNESLineSearchDestroy, SNESLineSearchView));

  linesearch->vec_sol_new  = NULL;
  linesearch->vec_func_new = NULL;
  linesearch->vec_sol      = NULL;
  linesearch->vec_func     = NULL;
  linesearch->vec_update   = NULL;

  linesearch->lambda       = 1.0;
  linesearch->fnorm        = 1.0;
  linesearch->ynorm        = 1.0;
  linesearch->xnorm        = 1.0;
  linesearch->result       = SNES_LINESEARCH_SUCCEEDED;
  linesearch->norms        = PETSC_TRUE;
  linesearch->keeplambda   = PETSC_FALSE;
  linesearch->damping      = 1.0;
  linesearch->maxstep      = 1e8;
  linesearch->steptol      = 1e-12;
  linesearch->rtol         = 1e-8;
  linesearch->atol         = 1e-15;
  linesearch->ltol         = 1e-8;
  linesearch->precheckctx  = NULL;
  linesearch->postcheckctx = NULL;
  linesearch->max_its      = 1;
  linesearch->setupcalled  = PETSC_FALSE;
  linesearch->monitor      = NULL;
  *outlinesearch           = linesearch;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetUp - Prepares the line search for being applied by allocating
   any required vectors.

   Collective

   Input Parameter:
.  linesearch - The `SNESLineSearch` instance.

   Level: advanced

   Note:
   For most cases, this needn't be called by users or outside of `SNESLineSearchApply()`.
   The only current case where this is called outside of this is for the VI
   solvers, which modify the solution and work vectors before the first call
   of `SNESLineSearchApply()`, requiring the `SNESLineSearch` work vectors to be
   allocated upfront.

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchReset()`
@*/

PetscErrorCode SNESLineSearchSetUp(SNESLineSearch linesearch)
{
  PetscFunctionBegin;
  if (!((PetscObject)linesearch)->type_name) PetscCall(SNESLineSearchSetType(linesearch, SNESLINESEARCHBASIC));
  if (!linesearch->setupcalled) {
    if (!linesearch->vec_sol_new) PetscCall(VecDuplicate(linesearch->vec_sol, &linesearch->vec_sol_new));
    if (!linesearch->vec_func_new) PetscCall(VecDuplicate(linesearch->vec_sol, &linesearch->vec_func_new));
    if (linesearch->ops->setup) PetscUseTypeMethod(linesearch, setup);
    if (!linesearch->ops->snesfunc) PetscCall(SNESLineSearchSetFunction(linesearch, SNESComputeFunction));
    linesearch->lambda      = linesearch->damping;
    linesearch->setupcalled = PETSC_TRUE;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchReset - Undoes the `SNESLineSearchSetUp()` and deletes any `Vec`s or `Mat`s allocated by the line search.

   Collective

   Input Parameter:
.  linesearch - The `SNESLineSearch` instance.

   Level: developer

   Note:
    Usually only called by `SNESReset()`

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchSetUp()`
@*/

PetscErrorCode SNESLineSearchReset(SNESLineSearch linesearch)
{
  PetscFunctionBegin;
  if (linesearch->ops->reset) PetscUseTypeMethod(linesearch, reset);

  PetscCall(VecDestroy(&linesearch->vec_sol_new));
  PetscCall(VecDestroy(&linesearch->vec_func_new));

  PetscCall(VecDestroyVecs(linesearch->nwork, &linesearch->work));

  linesearch->nwork       = 0;
  linesearch->setupcalled = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchSetFunction - Sets the function evaluation used by the `SNES` line search
`
   Input Parameters:
.  linesearch - the `SNESLineSearch` context
+  func       - function evaluation routine, this is usually the function provided with `SNESSetFunction()`

   Level: developer

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESSetFunction()`
@*/
PetscErrorCode SNESLineSearchSetFunction(SNESLineSearch linesearch, PetscErrorCode (*func)(SNES, Vec, Vec))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  linesearch->ops->snesfunc = func;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchSetPreCheck - Sets a user function that is called after the initial search direction has been computed but
         before the line search routine has been applied. Allows the user to adjust the result of (usually a linear solve) that
         determined the search direction.

   Logically Collective

   Input Parameters:
+  linesearch - the `SNESLineSearch` context
.  func - [optional] function evaluation routine,  for the calling sequence see `SNESLineSearchPreCheck()`
-  ctx        - [optional] user-defined context for private data for the function evaluation routine (may be `NULL`)

   Level: intermediate

   Note:
   Use `SNESLineSearchSetPostCheck()` to change the step after the line search.
   search is complete.

   Use `SNESVISetVariableBounds()` and `SNESVISetComputeVariableBounds()` to cause `SNES` to automatically control the ranges of variables allowed.

.seealso: `SNESGetLineSearch()`, `SNESLineSearchPreCheck()`, `SNESLineSearchSetPostCheck()`, `SNESLineSearchGetPostCheck()`, `SNESLineSearchGetPreCheck()`,
          `SNESVISetVariableBounds()`, `SNESVISetComputeVariableBounds()`, `SNESSetFunctionDomainError()`, `SNESSetJacobianDomainError()

@*/
PetscErrorCode SNESLineSearchSetPreCheck(SNESLineSearch linesearch, PetscErrorCode (*func)(SNESLineSearch, Vec, Vec, PetscBool *, void *), void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (func) linesearch->ops->precheck = func;
  if (ctx) linesearch->precheckctx = ctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchGetPreCheck - Gets the pre-check function for the line search routine.

   Input Parameter:
.  linesearch - the `SNESLineSearch` context

   Output Parameters:
+  func       - [optional] function evaluation routine,  for calling sequence see `SNESLineSearchPreCheck`
-  ctx        - [optional] user-defined context for private data for the function evaluation routine (may be `NULL`)

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESGetLineSearch()`, `SNESLineSearchPreCheck()`, `SNESLineSearchGetPostCheck()`, `SNESLineSearchSetPreCheck()`, `SNESLineSearchSetPostCheck()`
@*/
PetscErrorCode SNESLineSearchGetPreCheck(SNESLineSearch linesearch, PetscErrorCode (**func)(SNESLineSearch, Vec, Vec, PetscBool *, void *), void **ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (func) *func = linesearch->ops->precheck;
  if (ctx) *ctx = linesearch->precheckctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchSetPostCheck - Sets a user function that is called after the line search has been applied to determine the step
       direction and length. Allows the user a chance to change or override the decision of the line search routine

   Logically Collective

   Input Parameters:
+  linesearch - the `SNESLineSearch` context
.  func - [optional] function evaluation routine,   for the calling sequence see `SNESLineSearchPostCheck`
-  ctx        - [optional] user-defined context for private data for the function evaluation routine (may be `NULL`)

   Level: intermediate

   Notes:
   Use `SNESLineSearchSetPreCheck()` to change the step before the line search.
   search is complete.

   Use `SNESVISetVariableBounds()` and `SNESVISetComputeVariableBounds()` to cause `SNES` to automatically control the ranges of variables allowed.

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchPostCheck()`, `SNESLineSearchSetPreCheck()`, `SNESLineSearchGetPreCheck()`, `SNESLineSearchGetPostCheck()`,
          `SNESVISetVariableBounds()`, `SNESVISetComputeVariableBounds()`, `SNESSetFunctionDomainError()`, `SNESSetJacobianDomainError()
@*/
PetscErrorCode SNESLineSearchSetPostCheck(SNESLineSearch linesearch, PetscErrorCode (*func)(SNESLineSearch, Vec, Vec, Vec, PetscBool *, PetscBool *, void *), void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (func) linesearch->ops->postcheck = func;
  if (ctx) linesearch->postcheckctx = ctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchGetPostCheck - Gets the post-check function for the line search routine.

   Input Parameter:
.  linesearch - the `SNESLineSearch` context

   Output Parameters:
+  func - [optional] function evaluation routine, see for the calling sequence `SNESLineSearchPostCheck`
-  ctx        - [optional] user-defined context for private data for the function evaluation routine (may be `NULL`)

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchGetPreCheck()`, `SNESLineSearchSetPostCheck()`, `SNESLineSearchPostCheck()`, `SNESLineSearchSetPreCheck()`
@*/
PetscErrorCode SNESLineSearchGetPostCheck(SNESLineSearch linesearch, PetscErrorCode (**func)(SNESLineSearch, Vec, Vec, Vec, PetscBool *, PetscBool *, void *), void **ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (func) *func = linesearch->ops->postcheck;
  if (ctx) *ctx = linesearch->postcheckctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchPreCheck - Prepares the line search for being applied.

   Logically Collective

   Input Parameters:
+  linesearch - The linesearch instance.
.  X - The current solution
-  Y - The step direction

   Output Parameter:
.  changed - Indicator that the precheck routine has changed anything

   Level: advanced

   Note:
   This calls any function provided with `SNESLineSearchSetPreCheck()`

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchPostCheck()`, `SNESLineSearchSetPreCheck()`, `SNESLineSearchGetPreCheck()`, `SNESLineSearchSetPostCheck()`,
          `SNESLineSearchGetPostCheck()``
@*/
PetscErrorCode SNESLineSearchPreCheck(SNESLineSearch linesearch, Vec X, Vec Y, PetscBool *changed)
{
  PetscFunctionBegin;
  *changed = PETSC_FALSE;
  if (linesearch->ops->precheck) {
    PetscUseTypeMethod(linesearch, precheck, X, Y, changed, linesearch->precheckctx);
    PetscValidLogicalCollectiveBool(linesearch, *changed, 4);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchPostCheck - Hook to modify step direction or updated solution after a successful linesearch

   Logically Collective

   Input Parameters:
+  linesearch - The linesearch context
.  X - The last solution
.  Y - The step direction
-  W - The updated solution, W = X + lambda*Y for some lambda

   Output Parameters:
+  changed_Y - Indicator if the direction Y has been changed.
-  changed_W - Indicator if the new candidate solution W has been changed.

   Level: developer

   Note:
   This calls any function provided with `SNESLineSearchSetPreCheck()`

.seealso: `SNESGetLineSearch()`, `SNESLineSearchPreCheck()`, `SNESLineSearchSetPostCheck()`, `SNESLineSearchGetPostCheck()`, `SNESLineSearchSetPrecheck()`, `SNESLineSearchGetPrecheck()`
@*/
PetscErrorCode SNESLineSearchPostCheck(SNESLineSearch linesearch, Vec X, Vec Y, Vec W, PetscBool *changed_Y, PetscBool *changed_W)
{
  PetscFunctionBegin;
  *changed_Y = PETSC_FALSE;
  *changed_W = PETSC_FALSE;
  if (linesearch->ops->postcheck) {
    PetscUseTypeMethod(linesearch, postcheck, X, Y, W, changed_Y, changed_W, linesearch->postcheckctx);
    PetscValidLogicalCollectiveBool(linesearch, *changed_Y, 5);
    PetscValidLogicalCollectiveBool(linesearch, *changed_W, 6);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchPreCheckPicard - Implements a correction that is sometimes useful to improve the convergence rate of Picard iteration

   Logically Collective

   Input Parameters:
+  linesearch - linesearch context
.  X - base state for this step
-  ctx - context for this function

   Input/Output Parameter:
.  Y - correction, possibly modified

   Output Parameter:
.  changed - flag indicating that Y was modified

   Options Database Key:
+  -snes_linesearch_precheck_picard - activate this routine
-  -snes_linesearch_precheck_picard_angle - angle

   Level: advanced

   Notes:
   This function should be passed to `SNESLineSearchSetPreCheck()`

   The justification for this method involves the linear convergence of a Picard iteration
   so the Picard linearization should be provided in place of the "Jacobian". This correction
   is generally not useful when using a Newton linearization.

   Reference:
 . - * - Hindmarsh and Payne (1996) Time step limits for stable solutions of the ice sheet equation, Annals of Glaciology.

.seealso: `SNESLineSearch`, `SNESSetPicard()`, `SNESGetLineSearch()`, `SNESLineSearchSetPreCheck()`
@*/
PetscErrorCode SNESLineSearchPreCheckPicard(SNESLineSearch linesearch, Vec X, Vec Y, PetscBool *changed, void *ctx)
{
  PetscReal   angle = *(PetscReal *)linesearch->precheckctx;
  Vec         Ylast;
  PetscScalar dot;
  PetscInt    iter;
  PetscReal   ynorm, ylastnorm, theta, angle_radians;
  SNES        snes;

  PetscFunctionBegin;
  PetscCall(SNESLineSearchGetSNES(linesearch, &snes));
  PetscCall(PetscObjectQuery((PetscObject)snes, "SNESLineSearchPreCheckPicard_Ylast", (PetscObject *)&Ylast));
  if (!Ylast) {
    PetscCall(VecDuplicate(Y, &Ylast));
    PetscCall(PetscObjectCompose((PetscObject)snes, "SNESLineSearchPreCheckPicard_Ylast", (PetscObject)Ylast));
    PetscCall(PetscObjectDereference((PetscObject)Ylast));
  }
  PetscCall(SNESGetIterationNumber(snes, &iter));
  if (iter < 2) {
    PetscCall(VecCopy(Y, Ylast));
    *changed = PETSC_FALSE;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  PetscCall(VecDot(Y, Ylast, &dot));
  PetscCall(VecNorm(Y, NORM_2, &ynorm));
  PetscCall(VecNorm(Ylast, NORM_2, &ylastnorm));
  /* Compute the angle between the vectors Y and Ylast, clip to keep inside the domain of acos() */
  theta         = PetscAcosReal((PetscReal)PetscClipInterval(PetscAbsScalar(dot) / (ynorm * ylastnorm), -1.0, 1.0));
  angle_radians = angle * PETSC_PI / 180.;
  if (PetscAbsReal(theta) < angle_radians || PetscAbsReal(theta - PETSC_PI) < angle_radians) {
    /* Modify the step Y */
    PetscReal alpha, ydiffnorm;
    PetscCall(VecAXPY(Ylast, -1.0, Y));
    PetscCall(VecNorm(Ylast, NORM_2, &ydiffnorm));
    alpha = (ydiffnorm > .001 * ylastnorm) ? ylastnorm / ydiffnorm : 1000.0;
    PetscCall(VecCopy(Y, Ylast));
    PetscCall(VecScale(Y, alpha));
    PetscCall(PetscInfo(snes, "Angle %14.12e degrees less than threshold %14.12e, corrected step by alpha=%14.12e\n", (double)(theta * 180. / PETSC_PI), (double)angle, (double)alpha));
    *changed = PETSC_TRUE;
  } else {
    PetscCall(PetscInfo(snes, "Angle %14.12e degrees exceeds threshold %14.12e, no correction applied\n", (double)(theta * 180. / PETSC_PI), (double)angle));
    PetscCall(VecCopy(Y, Ylast));
    *changed = PETSC_FALSE;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchApply - Computes the line-search update.

   Collective

   Input Parameters:
+  linesearch - The linesearch context
-  Y - The search direction

   Input/Output Parameters:
+  X - The current solution, on output the new solution
.  F - The current function, on output the new function
-  fnorm - The current norm, on output the new function norm

   Options Database Keys:
+ -snes_linesearch_type - basic (or equivalently none), bt, l2, cp, nleqerr, shell
. -snes_linesearch_monitor [:filename] - Print progress of line searches
. -snes_linesearch_damping - The linesearch damping parameter, default is 1.0 (no damping)
. -snes_linesearch_norms   - Turn on/off the linesearch norms computation (SNESLineSearchSetComputeNorms())
. -snes_linesearch_keeplambda - Keep the previous search length as the initial guess
- -snes_linesearch_max_it - The number of iterations for iterative line searches

   Level: Intermediate

   Notes:
   This is typically called from within a `SNESSolve()` implementation in order to
   help with convergence of the nonlinear method.  Various `SNES` types use line searches
   in different ways, but the overarching theme is that a line search is used to determine
   an optimal damping parameter of a step at each iteration of the method.  Each
   application of the line search may invoke `SNESComputeFunction()` several times, and
   therefore may be fairly expensive.

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchCreate()`, `SNESLineSearchPreCheck()`, `SNESLineSearchPostCheck()`, `SNESSolve()`, `SNESComputeFunction()`, `SNESLineSearchSetComputeNorms()`,
          `SNESLineSearchType`, `SNESLineSearchSetType()`
@*/
PetscErrorCode SNESLineSearchApply(SNESLineSearch linesearch, Vec X, Vec F, PetscReal *fnorm, Vec Y)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidHeaderSpecific(X, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(F, VEC_CLASSID, 3);
  PetscValidHeaderSpecific(Y, VEC_CLASSID, 5);

  linesearch->result = SNES_LINESEARCH_SUCCEEDED;

  linesearch->vec_sol    = X;
  linesearch->vec_update = Y;
  linesearch->vec_func   = F;

  PetscCall(SNESLineSearchSetUp(linesearch));

  if (!linesearch->keeplambda) linesearch->lambda = linesearch->damping; /* set the initial guess to lambda */

  if (fnorm) linesearch->fnorm = *fnorm;
  else PetscCall(VecNorm(F, NORM_2, &linesearch->fnorm));

  PetscCall(PetscLogEventBegin(SNESLINESEARCH_Apply, linesearch, X, F, Y));

  PetscUseTypeMethod(linesearch, apply);

  PetscCall(PetscLogEventEnd(SNESLINESEARCH_Apply, linesearch, X, F, Y));

  if (fnorm) *fnorm = linesearch->fnorm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchDestroy - Destroys the line search instance.

   Collective

   Input Parameter:
.  linesearch - The linesearch context

   Level: developer

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchCreate()`, `SNESLineSearchReset()`, `SNESDestroy()`
@*/
PetscErrorCode SNESLineSearchDestroy(SNESLineSearch *linesearch)
{
  PetscFunctionBegin;
  if (!*linesearch) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific((*linesearch), SNESLINESEARCH_CLASSID, 1);
  if (--((PetscObject)(*linesearch))->refct > 0) {
    *linesearch = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscObjectSAWsViewOff((PetscObject)*linesearch));
  PetscCall(SNESLineSearchReset(*linesearch));
  PetscTryTypeMethod(*linesearch, destroy);
  PetscCall(PetscViewerDestroy(&(*linesearch)->monitor));
  PetscCall(SNESLineSearchMonitorCancel((*linesearch)));
  PetscCall(PetscHeaderDestroy(linesearch));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetDefaultMonitor - Turns on/off printing useful information and debugging output about the line search.

   Logically Collective

   Input Parameters:
+  linesearch - the linesearch object
-  viewer - an `PETSCVIEWERASCII` `PetscViewer` or `NULL` to turn off monitor

   Options Database Key:
.   -snes_linesearch_monitor [:filename] - enables the monitor

   Level: intermediate

   Developer Note:
   This monitor is implemented differently than the other line search monitors that are set with
   `SNESLineSearchMonitorSet()` since it is called in many locations of the line search routines to display aspects of the
   line search that are not visible to the other monitors.

.seealso: `SNESLineSearch`, `PETSCVIEWERASCII`, `SNESGetLineSearch()`, `SNESLineSearchGetDefaultMonitor()`, `PetscViewer`, `SNESLineSearchSetMonitor()`,
          `SNESLineSearchMonitorSetFromOptions()`
@*/
PetscErrorCode SNESLineSearchSetDefaultMonitor(SNESLineSearch linesearch, PetscViewer viewer)
{
  PetscFunctionBegin;
  if (viewer) PetscCall(PetscObjectReference((PetscObject)viewer));
  PetscCall(PetscViewerDestroy(&linesearch->monitor));
  linesearch->monitor = viewer;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetDefaultMonitor - Gets the `PetscViewer` instance for the line search monitor.

   Logically Collective

   Input Parameter:
.  linesearch - linesearch context

   Output Parameter:
.  monitor - monitor context

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESGetLineSearch()`, `SNESLineSearchSetDefaultMonitor()`, `PetscViewer`
@*/
PetscErrorCode SNESLineSearchGetDefaultMonitor(SNESLineSearch linesearch, PetscViewer *monitor)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  *monitor = linesearch->monitor;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchMonitorSetFromOptions - Sets a monitor function and viewer appropriate for the type indicated by the user

   Collective

   Input Parameters:
+  ls - `SNESLineSearch` object you wish to monitor
.  name - the monitor type one is seeking
.  help - message indicating what monitoring is done
.  manual - manual page for the monitor
.  monitor - the monitor function
-  monitorsetup - a function that is called once ONLY if the user selected this monitor that may set additional features of the `SNESLineSearch` or `PetscViewer`

   Level: developer

.seealso: `SNESLineSearch`, `SNESLineSearchSetMonitor()`, `PetscOptionsGetViewer()`, `PetscOptionsGetReal()`, `PetscOptionsHasName()`, `PetscOptionsGetString()`,
          `PetscOptionsGetIntArray()`, `PetscOptionsGetRealArray()`, `PetscOptionsBool()`
          `PetscOptionsInt()`, `PetscOptionsString()`, `PetscOptionsReal()`, `PetscOptionsBool()`,
          `PetscOptionsName()`, `PetscOptionsBegin()`, `PetscOptionsEnd()`, `PetscOptionsHeadBegin()`,
          `PetscOptionsStringArray()`, `PetscOptionsRealArray()`, `PetscOptionsScalar()`,
          `PetscOptionsBoolGroupBegin()`, `PetscOptionsBoolGroup()`, `PetscOptionsBoolGroupEnd()`,
          `PetscOptionsFList()`, `PetscOptionsEList()`
@*/
PetscErrorCode SNESLineSearchMonitorSetFromOptions(SNESLineSearch ls, const char name[], const char help[], const char manual[], PetscErrorCode (*monitor)(SNESLineSearch, PetscViewerAndFormat *), PetscErrorCode (*monitorsetup)(SNESLineSearch, PetscViewerAndFormat *))
{
  PetscViewer       viewer;
  PetscViewerFormat format;
  PetscBool         flg;

  PetscFunctionBegin;
  PetscCall(PetscOptionsGetViewer(PetscObjectComm((PetscObject)ls), ((PetscObject)ls)->options, ((PetscObject)ls)->prefix, name, &viewer, &format, &flg));
  if (flg) {
    PetscViewerAndFormat *vf;
    PetscCall(PetscViewerAndFormatCreate(viewer, format, &vf));
    PetscCall(PetscObjectDereference((PetscObject)viewer));
    if (monitorsetup) PetscCall((*monitorsetup)(ls, vf));
    PetscCall(SNESLineSearchMonitorSet(ls, (PetscErrorCode(*)(SNESLineSearch, void *))monitor, vf, (PetscErrorCode(*)(void **))PetscViewerAndFormatDestroy));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetFromOptions - Sets options for the line search

   Logically Collective

   Input Parameter:
.  linesearch - linesearch context

   Options Database Keys:
+ -snes_linesearch_type <type> - basic (or equivalently none), bt, l2, cp, nleqerr, shell
. -snes_linesearch_order <order> - 1, 2, 3.  Most types only support certain orders (bt supports 2 or 3)
. -snes_linesearch_norms   - Turn on/off the linesearch norms for the basic linesearch typem (`SNESLineSearchSetComputeNorms()`)
. -snes_linesearch_minlambda - The minimum step length
. -snes_linesearch_maxstep - The maximum step size
. -snes_linesearch_rtol - Relative tolerance for iterative line searches
. -snes_linesearch_atol - Absolute tolerance for iterative line searches
. -snes_linesearch_ltol - Change in lambda tolerance for iterative line searches
. -snes_linesearch_max_it - The number of iterations for iterative line searches
. -snes_linesearch_monitor [:filename] - Print progress of line searches
. -snes_linesearch_monitor_solution_update [viewer:filename:format] - view each update tried by line search routine
. -snes_linesearch_damping - The linesearch damping parameter
. -snes_linesearch_keeplambda - Keep the previous search length as the initial guess.
. -snes_linesearch_precheck_picard - Use precheck that speeds up convergence of picard method
- -snes_linesearch_precheck_picard_angle - Angle used in Picard precheck method

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESLineSearchCreate()`, `SNESLineSearchSetOrder()`, `SNESLineSearchSetType()`, `SNESLineSearchSetTolerances()`, `SNESLineSearchSetDamping()`, `SNESLineSearchPreCheckPicard()`,
          `SNESLineSearchType`, `SNESLineSearchSetComputeNorms()`
@*/
PetscErrorCode SNESLineSearchSetFromOptions(SNESLineSearch linesearch)
{
  const char *deft = SNESLINESEARCHBASIC;
  char        type[256];
  PetscBool   flg, set;
  PetscViewer viewer;

  PetscFunctionBegin;
  PetscCall(SNESLineSearchRegisterAll());

  PetscObjectOptionsBegin((PetscObject)linesearch);
  if (((PetscObject)linesearch)->type_name) deft = ((PetscObject)linesearch)->type_name;
  PetscCall(PetscOptionsFList("-snes_linesearch_type", "Linesearch type", "SNESLineSearchSetType", SNESLineSearchList, deft, type, 256, &flg));
  if (flg) {
    PetscCall(SNESLineSearchSetType(linesearch, type));
  } else if (!((PetscObject)linesearch)->type_name) {
    PetscCall(SNESLineSearchSetType(linesearch, deft));
  }

  PetscCall(PetscOptionsGetViewer(PetscObjectComm((PetscObject)linesearch), ((PetscObject)linesearch)->options, ((PetscObject)linesearch)->prefix, "-snes_linesearch_monitor", &viewer, NULL, &set));
  if (set) {
    PetscCall(SNESLineSearchSetDefaultMonitor(linesearch, viewer));
    PetscCall(PetscViewerDestroy(&viewer));
  }
  PetscCall(SNESLineSearchMonitorSetFromOptions(linesearch, "-snes_linesearch_monitor_solution_update", "View correction at each iteration", "SNESLineSearchMonitorSolutionUpdate", SNESLineSearchMonitorSolutionUpdate, NULL));

  /* tolerances */
  PetscCall(PetscOptionsReal("-snes_linesearch_minlambda", "Minimum step length", "SNESLineSearchSetTolerances", linesearch->steptol, &linesearch->steptol, NULL));
  PetscCall(PetscOptionsReal("-snes_linesearch_maxstep", "Maximum step size", "SNESLineSearchSetTolerances", linesearch->maxstep, &linesearch->maxstep, NULL));
  PetscCall(PetscOptionsReal("-snes_linesearch_rtol", "Relative tolerance for iterative line search", "SNESLineSearchSetTolerances", linesearch->rtol, &linesearch->rtol, NULL));
  PetscCall(PetscOptionsReal("-snes_linesearch_atol", "Absolute tolerance for iterative line search", "SNESLineSearchSetTolerances", linesearch->atol, &linesearch->atol, NULL));
  PetscCall(PetscOptionsReal("-snes_linesearch_ltol", "Change in lambda tolerance for iterative line search", "SNESLineSearchSetTolerances", linesearch->ltol, &linesearch->ltol, NULL));
  PetscCall(PetscOptionsInt("-snes_linesearch_max_it", "Maximum iterations for iterative line searches", "SNESLineSearchSetTolerances", linesearch->max_its, &linesearch->max_its, NULL));

  /* damping parameters */
  PetscCall(PetscOptionsReal("-snes_linesearch_damping", "Line search damping and initial step guess", "SNESLineSearchSetDamping", linesearch->damping, &linesearch->damping, NULL));

  PetscCall(PetscOptionsBool("-snes_linesearch_keeplambda", "Use previous lambda as damping", "SNESLineSearchSetKeepLambda", linesearch->keeplambda, &linesearch->keeplambda, NULL));

  /* precheck */
  PetscCall(PetscOptionsBool("-snes_linesearch_precheck_picard", "Use a correction that sometimes improves convergence of Picard iteration", "SNESLineSearchPreCheckPicard", flg, &flg, &set));
  if (set) {
    if (flg) {
      linesearch->precheck_picard_angle = 10.; /* correction only active if angle is less than 10 degrees */

      PetscCall(PetscOptionsReal("-snes_linesearch_precheck_picard_angle", "Maximum angle at which to activate the correction", "none", linesearch->precheck_picard_angle, &linesearch->precheck_picard_angle, NULL));
      PetscCall(SNESLineSearchSetPreCheck(linesearch, SNESLineSearchPreCheckPicard, &linesearch->precheck_picard_angle));
    } else {
      PetscCall(SNESLineSearchSetPreCheck(linesearch, NULL, NULL));
    }
  }
  PetscCall(PetscOptionsInt("-snes_linesearch_order", "Order of approximation used in the line search", "SNESLineSearchSetOrder", linesearch->order, &linesearch->order, NULL));
  PetscCall(PetscOptionsBool("-snes_linesearch_norms", "Compute final norms in line search", "SNESLineSearchSetComputeNorms", linesearch->norms, &linesearch->norms, NULL));

  PetscTryTypeMethod(linesearch, setfromoptions, PetscOptionsObject);

  PetscCall(PetscObjectProcessOptionsHandlers((PetscObject)linesearch, PetscOptionsObject));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchView - Prints useful information about the line search

   Logically Collective

   Input Parameters:
+  linesearch - linesearch context
-  viewer - the viewer to display the line search information

   Level: intermediate

.seealso: `SNESLineSearch`, `PetscViewer`, `SNESLineSearchCreate()`
@*/
PetscErrorCode SNESLineSearchView(SNESLineSearch linesearch, PetscViewer viewer)
{
  PetscBool iascii;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (!viewer) PetscCall(PetscViewerASCIIGetStdout(PetscObjectComm((PetscObject)linesearch), &viewer));
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  PetscCheckSameComm(linesearch, 1, viewer, 2);

  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscObjectPrintClassNamePrefixType((PetscObject)linesearch, viewer));
    PetscCall(PetscViewerASCIIPushTab(viewer));
    PetscTryTypeMethod(linesearch, view, viewer);
    PetscCall(PetscViewerASCIIPopTab(viewer));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  maxstep=%e, minlambda=%e\n", (double)linesearch->maxstep, (double)linesearch->steptol));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  tolerances: relative=%e, absolute=%e, lambda=%e\n", (double)linesearch->rtol, (double)linesearch->atol, (double)linesearch->ltol));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  maximum iterations=%" PetscInt_FMT "\n", linesearch->max_its));
    if (linesearch->ops->precheck) {
      if (linesearch->ops->precheck == SNESLineSearchPreCheckPicard) {
        PetscCall(PetscViewerASCIIPrintf(viewer, "  using precheck step to speed up Picard convergence\n"));
      } else {
        PetscCall(PetscViewerASCIIPrintf(viewer, "  using user-defined precheck step\n"));
      }
    }
    if (linesearch->ops->postcheck) PetscCall(PetscViewerASCIIPrintf(viewer, "  using user-defined postcheck step\n"));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchGetType - Gets the linesearch type

   Logically Collective

   Input Parameter:
.  linesearch - linesearch context

   Output Parameter:
.  type - The type of line search, or `NULL` if not set

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESLineSearchType`, `SNESLineSearchCreate()`, `SNESLineSearchType`, `SNESLineSearchSetFromOptions()`, `SNESLineSearchSetType()`
@*/
PetscErrorCode SNESLineSearchGetType(SNESLineSearch linesearch, SNESLineSearchType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidPointer(type, 2);
  *type = ((PetscObject)linesearch)->type_name;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchSetType - Sets the linesearch type

   Logically Collective

   Input Parameters:
+  linesearch - linesearch context
-  type - The type of line search to be used

   Available Types:
+  `SNESLINESEARCHBASIC` - (or equivalently `SNESLINESEARCHNONE`) Simple damping line search, defaults to using the full Newton step
.  `SNESLINESEARCHBT` - Backtracking line search over the L2 norm of the function
.  `SNESLINESEARCHL2` - Secant line search over the L2 norm of the function
.  `SNESLINESEARCHCP` - Critical point secant line search assuming F(x) = grad G(x) for some unknown G(x)
.  `SNESLINESEARCHNLEQERR` - Affine-covariant error-oriented linesearch
-  `SNESLINESEARCHSHELL` - User provided `SNESLineSearch` implementation

   Options Database Key:
.  -snes_linesearch_type <type> - basic (or equivalently none), bt, l2, cp, nleqerr, shell

   Level: intermediate

.seealso:  `SNESLineSearch`, `SNESLineSearchType`, `SNESLineSearchCreate()`, `SNESLineSearchType`, `SNESLineSearchSetFromOptions()`, `SNESLineSearchGetType()`
@*/
PetscErrorCode SNESLineSearchSetType(SNESLineSearch linesearch, SNESLineSearchType type)
{
  PetscBool match;
  PetscErrorCode (*r)(SNESLineSearch);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidCharPointer(type, 2);

  PetscCall(PetscObjectTypeCompare((PetscObject)linesearch, type, &match));
  if (match) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCall(PetscFunctionListFind(SNESLineSearchList, type, &r));
  PetscCheck(r, PETSC_COMM_SELF, PETSC_ERR_ARG_UNKNOWN_TYPE, "Unable to find requested Line Search type %s", type);
  /* Destroy the previous private linesearch context */
  if (linesearch->ops->destroy) {
    PetscCall((*(linesearch)->ops->destroy)(linesearch));
    linesearch->ops->destroy = NULL;
  }
  /* Reinitialize function pointers in SNESLineSearchOps structure */
  linesearch->ops->apply          = NULL;
  linesearch->ops->view           = NULL;
  linesearch->ops->setfromoptions = NULL;
  linesearch->ops->destroy        = NULL;

  PetscCall(PetscObjectChangeTypeName((PetscObject)linesearch, type));
  PetscCall((*r)(linesearch));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetSNES - Sets the `SNES` for the linesearch for function evaluation.

   Input Parameters:
+  linesearch - linesearch context
-  snes - The snes instance

   Level: developer

   Note:
   This happens automatically when the line search is obtained/created with
   `SNESGetLineSearch()`.  This routine is therefore mainly called within `SNES`
   implementations.

.seealso: `SNESLineSearch`, `SNESLineSearchGetSNES()`, `SNESLineSearchSetVecs()`, `SNES`
@*/
PetscErrorCode SNESLineSearchSetSNES(SNESLineSearch linesearch, SNES snes)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 2);
  linesearch->snes = snes;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetSNES - Gets the `SNES` instance associated with the line search.
   Having an associated `SNES` is necessary because most line search implementations must be able to
   evaluate the function using `SNESComputeFunction()` for the associated `SNES`.  This routine
   is used in the line search implementations when one must get this associated `SNES` instance.

   Not Collective

   Input Parameter:
.  linesearch - linesearch context

   Output Parameter:
.  snes - The `SNES` instance

   Level: developer

.seealso: `SNESLineSearch`, `SNESType`, `SNESLineSearchGetSNES()`, `SNESLineSearchSetVecs()`, `SNES`
@*/
PetscErrorCode SNESLineSearchGetSNES(SNESLineSearch linesearch, SNES *snes)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidPointer(snes, 2);
  *snes = linesearch->snes;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetLambda - Gets the last linesearch steplength discovered.

   Not Collective

   Input Parameter:
.  linesearch - linesearch context

   Output Parameter:
.  lambda - The last steplength computed during `SNESLineSearchApply()`

   Level: advanced

   Note:
   This is useful in methods where the solver is ill-scaled and
   requires some adaptive notion of the difference in scale between the
   solution and the function.  For instance, `SNESQN` may be scaled by the
   line search lambda using the argument -snes_qn_scaling ls.

.seealso: `SNESLineSearch`, `SNESLineSearchSetLambda()`, `SNESLineSearchGetDamping()`, `SNESLineSearchApply()`
@*/
PetscErrorCode SNESLineSearchGetLambda(SNESLineSearch linesearch, PetscReal *lambda)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidRealPointer(lambda, 2);
  *lambda = linesearch->lambda;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetLambda - Sets the linesearch steplength

   Input Parameters:
+  linesearch - linesearch context
-  lambda - The last steplength.

   Level: advanced

   Note:
   This routine is typically used within implementations of `SNESLineSearchApply()`
   to set the final steplength.  This routine (and `SNESLineSearchGetLambda()`) were
   added in order to facilitate Quasi-Newton methods that use the previous steplength
   as an inner scaling parameter.

.seealso: `SNESLineSearch`, `SNESLineSearchGetLambda()`
@*/
PetscErrorCode SNESLineSearchSetLambda(SNESLineSearch linesearch, PetscReal lambda)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  linesearch->lambda = lambda;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetTolerances - Gets the tolerances for the linesearch.  These include
   tolerances for the relative and absolute change in the function norm, the change
   in lambda for iterative line searches, the minimum steplength, the maximum steplength,
   and the maximum number of iterations the line search procedure may take.

   Not Collective

   Input Parameter:
.  linesearch - linesearch context

   Output Parameters:
+  steptol - The minimum steplength, this is the value set by `SNESLineSearchGetTolerances()` scaled by the current norm of the function value
.  maxstep - The maximum steplength
.  rtol    - The relative tolerance for iterative line searches
.  atol    - The absolute tolerance for iterative line searches
.  ltol    - The change in lambda tolerance for iterative line searches
-  max_it  - The maximum number of iterations of the line search

   Level: intermediate

   Note:
   Different line searches may implement these parameters slightly differently as
   the type requires.

.seealso: `SNESLineSearch`, `SNESLineSearchSetTolerances()`
@*/
PetscErrorCode SNESLineSearchGetTolerances(SNESLineSearch linesearch, PetscReal *steptol, PetscReal *maxstep, PetscReal *rtol, PetscReal *atol, PetscReal *ltol, PetscInt *max_its)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (steptol) {
    PetscValidRealPointer(steptol, 2);
    *steptol = linesearch->steptol;
  }
  if (maxstep) {
    PetscValidRealPointer(maxstep, 3);
    *maxstep = linesearch->maxstep * linesearch->fnorm;
  }
  if (rtol) {
    PetscValidRealPointer(rtol, 4);
    *rtol = linesearch->rtol;
  }
  if (atol) {
    PetscValidRealPointer(atol, 5);
    *atol = linesearch->atol;
  }
  if (ltol) {
    PetscValidRealPointer(ltol, 6);
    *ltol = linesearch->ltol;
  }
  if (max_its) {
    PetscValidIntPointer(max_its, 7);
    *max_its = linesearch->max_its;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetTolerances -  Gets the tolerances for the linesearch.  These include
   tolerances for the relative and absolute change in the function norm, the change
   in lambda for iterative line searches, the minimum steplength, the maximum steplength,
   and the maximum number of iterations the line search procedure may take.

   Collective

   Input Parameters:
+  linesearch - linesearch context
.  steptol - The minimum steplength
.  maxstep - The maximum steplength
.  rtol    - The relative tolerance for iterative line searches
.  atol    - The absolute tolerance for iterative line searches
.  ltol    - The change in lambda tolerance for iterative line searches
-  max_it  - The maximum number of iterations of the line search

   Level: intermediate

   Note:
   The user may choose to not set any of the tolerances using `PETSC_DEFAULT` in
   place of an argument.

.seealso: `SNESLineSearch`, `SNESLineSearchGetTolerances()`
@*/
PetscErrorCode SNESLineSearchSetTolerances(SNESLineSearch linesearch, PetscReal steptol, PetscReal maxstep, PetscReal rtol, PetscReal atol, PetscReal ltol, PetscInt max_its)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidLogicalCollectiveReal(linesearch, steptol, 2);
  PetscValidLogicalCollectiveReal(linesearch, maxstep, 3);
  PetscValidLogicalCollectiveReal(linesearch, rtol, 4);
  PetscValidLogicalCollectiveReal(linesearch, atol, 5);
  PetscValidLogicalCollectiveReal(linesearch, ltol, 6);
  PetscValidLogicalCollectiveInt(linesearch, max_its, 7);

  if (steptol != (PetscReal)PETSC_DEFAULT) {
    PetscCheck(steptol >= 0.0, PetscObjectComm((PetscObject)linesearch), PETSC_ERR_ARG_OUTOFRANGE, "Minimum step length %14.12e must be non-negative", (double)steptol);
    linesearch->steptol = steptol;
  }

  if (maxstep != (PetscReal)PETSC_DEFAULT) {
    PetscCheck(maxstep >= 0.0, PetscObjectComm((PetscObject)linesearch), PETSC_ERR_ARG_OUTOFRANGE, "Maximum step length %14.12e must be non-negative", (double)maxstep);
    linesearch->maxstep = maxstep;
  }

  if (rtol != (PetscReal)PETSC_DEFAULT) {
    PetscCheck(rtol >= 0.0 && rtol < 1.0, PetscObjectComm((PetscObject)linesearch), PETSC_ERR_ARG_OUTOFRANGE, "Relative tolerance %14.12e must be non-negative and less than 1.0", (double)rtol);
    linesearch->rtol = rtol;
  }

  if (atol != (PetscReal)PETSC_DEFAULT) {
    PetscCheck(atol >= 0.0, PetscObjectComm((PetscObject)linesearch), PETSC_ERR_ARG_OUTOFRANGE, "Absolute tolerance %14.12e must be non-negative", (double)atol);
    linesearch->atol = atol;
  }

  if (ltol != (PetscReal)PETSC_DEFAULT) {
    PetscCheck(ltol >= 0.0, PetscObjectComm((PetscObject)linesearch), PETSC_ERR_ARG_OUTOFRANGE, "Lambda tolerance %14.12e must be non-negative", (double)ltol);
    linesearch->ltol = ltol;
  }

  if (max_its != PETSC_DEFAULT) {
    PetscCheck(max_its >= 0, PetscObjectComm((PetscObject)linesearch), PETSC_ERR_ARG_OUTOFRANGE, "Maximum number of iterations %" PetscInt_FMT " must be non-negative", max_its);
    linesearch->max_its = max_its;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetDamping - Gets the line search damping parameter.

   Input Parameter:
.  linesearch - linesearch context

   Output Parameter:
.  damping - The damping parameter

   Level: advanced

.seealso: `SNESLineSearchGetStepTolerance()`, `SNESQN`
@*/

PetscErrorCode SNESLineSearchGetDamping(SNESLineSearch linesearch, PetscReal *damping)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidRealPointer(damping, 2);
  *damping = linesearch->damping;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetDamping - Sets the line search damping parameter.

   Input Parameters:
+  linesearch - linesearch context
-  damping - The damping parameter

   Options Database Key:
.   -snes_linesearch_damping <damping> - the damping value

   Level: intermediate

   Note:
   The `SNESLINESEARCHNONE` line search merely takes the update step scaled by the damping parameter.
   The use of the damping parameter in the l2 and cp line searches is much more subtle;
   it is used as a starting point in calculating the secant step. However, the eventual
   step may be of greater length than the damping parameter.  In the bt line search it is
   used as the maximum possible step length, as the bt line search only backtracks.

.seealso: `SNESLineSearch`, `SNESLineSearchGetDamping()`
@*/
PetscErrorCode SNESLineSearchSetDamping(SNESLineSearch linesearch, PetscReal damping)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  linesearch->damping = damping;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetOrder - Gets the line search approximation order.

   Input Parameter:
.  linesearch - linesearch context

   Output Parameter:
.  order - The order

   Possible Values for order:
+  1 or `SNES_LINESEARCH_ORDER_LINEAR` - linear order
.  2 or `SNES_LINESEARCH_ORDER_QUADRATIC` - quadratic order
-  3 or `SNES_LINESEARCH_ORDER_CUBIC` - cubic order

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESLineSearchSetOrder()`
@*/

PetscErrorCode SNESLineSearchGetOrder(SNESLineSearch linesearch, PetscInt *order)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidIntPointer(order, 2);
  *order = linesearch->order;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetOrder - Sets the maximum order of the polynomial fit used in the line search

   Input Parameters:
+  linesearch - linesearch context
-  order - The damping parameter

   Level: intermediate

   Possible Values for order:
+  1 or `SNES_LINESEARCH_ORDER_LINEAR` - linear order
.  2 or `SNES_LINESEARCH_ORDER_QUADRATIC` - quadratic order
-  3 or `SNES_LINESEARCH_ORDER_CUBIC` - cubic order

   Notes:
   Variable orders are supported by the following line searches:
+  bt - cubic and quadratic
-  cp - linear and quadratic

.seealso: `SNESLineSearch`, `SNESLineSearchGetOrder()`, `SNESLineSearchSetDamping()`
@*/
PetscErrorCode SNESLineSearchSetOrder(SNESLineSearch linesearch, PetscInt order)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  linesearch->order = order;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetNorms - Gets the norms for for X, Y, and F.

   Not Collective

   Input Parameter:
.  linesearch - linesearch context

   Output Parameters:
+  xnorm - The norm of the current solution
.  fnorm - The norm of the current function, this is the `norm(function(X))` where `X` is the current solution.
-  ynorm - The norm of the current update

   Level: developer

   Note:
   Some values may not be up to date at particular points in the code.

   This, in combination with `SNESLineSearchSetNorms()`, allow the line search and the `SNESSolve_XXX()` to share
   computed values.

.seealso: `SNESLineSearch`, `SNESLineSearchSetNorms()` `SNESLineSearchGetVecs()`
@*/
PetscErrorCode SNESLineSearchGetNorms(SNESLineSearch linesearch, PetscReal *xnorm, PetscReal *fnorm, PetscReal *ynorm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (xnorm) *xnorm = linesearch->xnorm;
  if (fnorm) *fnorm = linesearch->fnorm;
  if (ynorm) *ynorm = linesearch->ynorm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetNorms - Gets the computed norms for for X, Y, and F.

   Collective

   Input Parameters:
+  linesearch - linesearch context
.  xnorm - The norm of the current solution
.  fnorm - The norm of the current function, this is the `norm(function(X))` where `X` is the current solution
-  ynorm - The norm of the current update

   Level: developer

.seealso: `SNESLineSearch`, `SNESLineSearchGetNorms()`, `SNESLineSearchSetVecs()`
@*/
PetscErrorCode SNESLineSearchSetNorms(SNESLineSearch linesearch, PetscReal xnorm, PetscReal fnorm, PetscReal ynorm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  linesearch->xnorm = xnorm;
  linesearch->fnorm = fnorm;
  linesearch->ynorm = ynorm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchComputeNorms - Computes the norms of X, F, and Y.

   Input Parameter:
.  linesearch - linesearch context

   Options Database Key:
.   -snes_linesearch_norms - turn norm computation on or off

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESLineSearchGetNorms`, `SNESLineSearchSetNorms()`, `SNESLineSearchSetComputeNorms()`
@*/
PetscErrorCode SNESLineSearchComputeNorms(SNESLineSearch linesearch)
{
  SNES snes;

  PetscFunctionBegin;
  if (linesearch->norms) {
    if (linesearch->ops->vinorm) {
      PetscCall(SNESLineSearchGetSNES(linesearch, &snes));
      PetscCall(VecNorm(linesearch->vec_sol, NORM_2, &linesearch->xnorm));
      PetscCall(VecNorm(linesearch->vec_update, NORM_2, &linesearch->ynorm));
      PetscCall((*linesearch->ops->vinorm)(snes, linesearch->vec_func, linesearch->vec_sol, &linesearch->fnorm));
    } else {
      PetscCall(VecNormBegin(linesearch->vec_func, NORM_2, &linesearch->fnorm));
      PetscCall(VecNormBegin(linesearch->vec_sol, NORM_2, &linesearch->xnorm));
      PetscCall(VecNormBegin(linesearch->vec_update, NORM_2, &linesearch->ynorm));
      PetscCall(VecNormEnd(linesearch->vec_func, NORM_2, &linesearch->fnorm));
      PetscCall(VecNormEnd(linesearch->vec_sol, NORM_2, &linesearch->xnorm));
      PetscCall(VecNormEnd(linesearch->vec_update, NORM_2, &linesearch->ynorm));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetComputeNorms - Turns on or off the computation of final norms in the line search.

   Input Parameters:
+  linesearch  - linesearch context
-  flg  - indicates whether or not to compute norms

   Options Database Key:
.   -snes_linesearch_norms <true> - Turns on/off computation of the norms for basic (none) linesearch

   Level: intermediate

   Note:
   This is most relevant to the `SNESLINESEARCHBASIC` (or equivalently `SNESLINESEARCHNONE`) line search type since most line searches have a stopping criteria involving the norm.

.seealso: `SNESLineSearch`, `SNESLineSearchGetNorms()`, `SNESLineSearchSetNorms()`, `SNESLineSearchComputeNorms()`, `SNESLINESEARCHBASIC`
@*/
PetscErrorCode SNESLineSearchSetComputeNorms(SNESLineSearch linesearch, PetscBool flg)
{
  PetscFunctionBegin;
  linesearch->norms = flg;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetVecs - Gets the vectors from the `SNESLineSearch` context

   Not Collective on but the vectors are parallel

   Input Parameter:
.  linesearch - linesearch context

   Output Parameters:
+  X - Solution vector
.  F - Function vector
.  Y - Search direction vector
.  W - Solution work vector
-  G - Function work vector

   Level: advanced

   Notes:
   At the beginning of a line search application, `X` should contain a
   solution and the vector `F` the function computed at `X`.  At the end of the
   line search application, `X` should contain the new solution, and `F` the
   function evaluated at the new solution.

   These vectors are owned by the `SNESLineSearch` and should not be destroyed by the caller

.seealso: `SNESLineSearch`, `SNESLineSearchGetNorms()`, `SNESLineSearchSetVecs()`
@*/
PetscErrorCode SNESLineSearchGetVecs(SNESLineSearch linesearch, Vec *X, Vec *F, Vec *Y, Vec *W, Vec *G)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (X) {
    PetscValidPointer(X, 2);
    *X = linesearch->vec_sol;
  }
  if (F) {
    PetscValidPointer(F, 3);
    *F = linesearch->vec_func;
  }
  if (Y) {
    PetscValidPointer(Y, 4);
    *Y = linesearch->vec_update;
  }
  if (W) {
    PetscValidPointer(W, 5);
    *W = linesearch->vec_sol_new;
  }
  if (G) {
    PetscValidPointer(G, 6);
    *G = linesearch->vec_func_new;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetVecs - Sets the vectors on the `SNESLineSearch` context

   Logically Collective

   Input Parameters:
+  linesearch - linesearch context
.  X - Solution vector
.  F - Function vector
.  Y - Search direction vector
.  W - Solution work vector
-  G - Function work vector

   Level: advanced

.seealso: `SNESLineSearch`, `SNESLineSearchSetNorms()`, `SNESLineSearchGetVecs()`
@*/
PetscErrorCode SNESLineSearchSetVecs(SNESLineSearch linesearch, Vec X, Vec F, Vec Y, Vec W, Vec G)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (X) {
    PetscValidHeaderSpecific(X, VEC_CLASSID, 2);
    linesearch->vec_sol = X;
  }
  if (F) {
    PetscValidHeaderSpecific(F, VEC_CLASSID, 3);
    linesearch->vec_func = F;
  }
  if (Y) {
    PetscValidHeaderSpecific(Y, VEC_CLASSID, 4);
    linesearch->vec_update = Y;
  }
  if (W) {
    PetscValidHeaderSpecific(W, VEC_CLASSID, 5);
    linesearch->vec_sol_new = W;
  }
  if (G) {
    PetscValidHeaderSpecific(G, VEC_CLASSID, 6);
    linesearch->vec_func_new = G;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchAppendOptionsPrefix - Appends to the prefix used for searching for all
   `SNESLineSearch` options in the database.

   Logically Collective

   Input Parameters:
+  linesearch - the `SNESLineSearch` context
-  prefix - the prefix to prepend to all option names

   Level: advanced

   Note:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the hyphen.

.seealso: `SNESLineSearch()`, `SNESLineSearchSetFromOptions()`, `SNESGetOptionsPrefix()`
@*/
PetscErrorCode SNESLineSearchAppendOptionsPrefix(SNESLineSearch linesearch, const char prefix[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscCall(PetscObjectAppendOptionsPrefix((PetscObject)linesearch, prefix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchGetOptionsPrefix - Gets the prefix used for searching for all
   SNESLineSearch options in the database.

   Not Collective

   Input Parameter:
.  linesearch - the `SNESLineSearch` context

   Output Parameter:
.  prefix - pointer to the prefix string used

   Level: advanced

   Fortran Note:
   The user should pass in a string 'prefix' of
   sufficient length to hold the prefix.

.seealso: `SNESLineSearch`, `SNESAppendOptionsPrefix()`
@*/
PetscErrorCode SNESLineSearchGetOptionsPrefix(SNESLineSearch linesearch, const char *prefix[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscCall(PetscObjectGetOptionsPrefix((PetscObject)linesearch, prefix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchSetWorkVecs - Sets work vectors for the line search.

   Input Parameters:
+  linesearch - the `SNESLineSearch` context
-  nwork - the number of work vectors

   Level: developer

.seealso: `SNESLineSearch`, `SNESSetWorkVecs()`
@*/
PetscErrorCode SNESLineSearchSetWorkVecs(SNESLineSearch linesearch, PetscInt nwork)
{
  PetscFunctionBegin;
  PetscCheck(linesearch->vec_sol, PetscObjectComm((PetscObject)linesearch), PETSC_ERR_USER, "Cannot get linesearch work-vectors without setting a solution vec!");
  PetscCall(VecDuplicateVecs(linesearch->vec_sol, nwork, &linesearch->work));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchGetReason - Gets the success/failure status of the last line search application

   Input Parameter:
.  linesearch - linesearch context

   Output Parameter:
.  result - The success or failure status

   Level: developer

   Note:
   This is typically called after `SNESLineSearchApply()` in order to determine if the line-search failed
   (and set the SNES convergence accordingly).

.seealso: `SNESLineSearch`, `SNESLineSearchSetReason()`, `SNESLineSearchReason`
@*/
PetscErrorCode SNESLineSearchGetReason(SNESLineSearch linesearch, SNESLineSearchReason *result)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  PetscValidPointer(result, 2);
  *result = linesearch->result;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchSetReason - Sets the success/failure status of the last line search application

   Input Parameters:
+  linesearch - linesearch context
-  result - The success or failure status

   Level: developer

   Note:
   This is typically called in a `SNESLineSearchApply()` or a `SNESLINESEARCHSHELL` implementation to set
   the success or failure of the line search method.

.seealso: `SNESLineSearch`, `SNESLineSearchReason`, `SNESLineSearchGetSResult()`
@*/
PetscErrorCode SNESLineSearchSetReason(SNESLineSearch linesearch, SNESLineSearchReason result)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  linesearch->result = result;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchSetVIFunctions - Sets VI-specific functions for line search computation.

   Logically Collective

   Input Parameters:
+  snes - nonlinear context obtained from `SNESCreate()`
.  projectfunc - function for projecting the function to the bounds
-  normfunc - function for computing the norm of an active set

   Calling sequence of `projectfunc`:
.vb
   PetscErrorCode projectfunc(SNES snes, Vec X)
.ve
+   snes - nonlinear context
-   X - current solution, store the projected solution here

   Calling sequence of `normfunc`:
.vb
   PetscErrorCode normfunc(SNES snes, Vec X, Vec F, PetscScalar *fnorm)
.ve
+   snes - nonlinear context
.   X - current solution
.   F - current residual
-   fnorm - VI-specific norm of the function

    Level: advanced

    Note:
    The VI solvers require projection of the solution to the feasible set.  `projectfunc` should implement this.

    The VI solvers require special evaluation of the function norm such that the norm is only calculated
    on the inactive set.  This should be implemented by `normfunc`.

.seealso: `SNESLineSearch`, `SNESLineSearchGetVIFunctions()`, `SNESLineSearchSetPostCheck()`, `SNESLineSearchSetPreCheck()`
@*/
PetscErrorCode SNESLineSearchSetVIFunctions(SNESLineSearch linesearch, SNESLineSearchVIProjectFunc projectfunc, SNESLineSearchVINormFunc normfunc)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  if (projectfunc) linesearch->ops->viproject = projectfunc;
  if (normfunc) linesearch->ops->vinorm = normfunc;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESLineSearchGetVIFunctions - Sets VI-specific functions for line search computation.

   Not Collective

   Input Parameter:
.  linesearch - the line search context, obtain with `SNESGetLineSearch()`

   Output Parameters:
+  projectfunc - function for projecting the function to the bounds
-  normfunc - function for computing the norm of an active set

    Level: advanced

.seealso: `SNESLineSearch`, `SNESLineSearchSetVIFunctions()`, `SNESLineSearchGetPostCheck()`, `SNESLineSearchGetPreCheck()`
@*/
PetscErrorCode SNESLineSearchGetVIFunctions(SNESLineSearch linesearch, SNESLineSearchVIProjectFunc *projectfunc, SNESLineSearchVINormFunc *normfunc)
{
  PetscFunctionBegin;
  if (projectfunc) *projectfunc = linesearch->ops->viproject;
  if (normfunc) *normfunc = linesearch->ops->vinorm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  SNESLineSearchRegister - register a line search method

  Level: advanced

.seealso: `SNESLineSearch`, `SNESLineSearchType`, `SNESLineSearchSetType()`
@*/
PetscErrorCode SNESLineSearchRegister(const char sname[], PetscErrorCode (*function)(SNESLineSearch))
{
  PetscFunctionBegin;
  PetscCall(SNESInitializePackage());
  PetscCall(PetscFunctionListAdd(&SNESLineSearchList, sname, function));
  PetscFunctionReturn(PETSC_SUCCESS);
}

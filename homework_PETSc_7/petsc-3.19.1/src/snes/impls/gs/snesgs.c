#include <../src/snes/impls/gs/gsimpl.h> /*I "petscsnes.h"  I*/

/*@
   SNESNGSSetTolerances - Sets various parameters used in convergence tests for nonlinear Gauss-Seidel `SNESNCG`

   Logically Collective

   Input Parameters:
+  snes - the `SNES` context
.  abstol - absolute convergence tolerance
.  rtol - relative convergence tolerance
.  stol -  convergence tolerance in terms of the norm of the change in the solution between steps,  || delta x || < stol*|| x ||
-  maxit - maximum number of iterations

   Options Database Keys:
+    -snes_ngs_atol <abstol> - Sets abstol
.    -snes_ngs_rtol <rtol> - Sets rtol
.    -snes_ngs_stol <stol> - Sets stol
-    -snes_max_it <maxit> - Sets maxit

   Level: intermediate

.seealso: `SNESNCG`, `SNESSetTrustRegionTolerance()`
@*/
PetscErrorCode SNESNGSSetTolerances(SNES snes, PetscReal abstol, PetscReal rtol, PetscReal stol, PetscInt maxit)
{
  SNES_NGS *gs = (SNES_NGS *)snes->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);

  if (abstol != (PetscReal)PETSC_DEFAULT) {
    PetscCheck(abstol >= 0.0, PetscObjectComm((PetscObject)snes), PETSC_ERR_ARG_OUTOFRANGE, "Absolute tolerance %g must be non-negative", (double)abstol);
    gs->abstol = abstol;
  }
  if (rtol != (PetscReal)PETSC_DEFAULT) {
    PetscCheck(rtol >= 0.0 && rtol < 1.0, PetscObjectComm((PetscObject)snes), PETSC_ERR_ARG_OUTOFRANGE, "Relative tolerance %g must be non-negative and less than 1.0", (double)rtol);
    gs->rtol = rtol;
  }
  if (stol != (PetscReal)PETSC_DEFAULT) {
    PetscCheck(stol >= 0.0, PetscObjectComm((PetscObject)snes), PETSC_ERR_ARG_OUTOFRANGE, "Step tolerance %g must be non-negative", (double)stol);
    gs->stol = stol;
  }
  if (maxit != PETSC_DEFAULT) {
    PetscCheck(maxit >= 0, PetscObjectComm((PetscObject)snes), PETSC_ERR_ARG_OUTOFRANGE, "Maximum number of iterations %" PetscInt_FMT " must be non-negative", maxit);
    gs->max_its = maxit;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESNGSGetTolerances - Gets various parameters used in convergence tests for nonlinear Gauss-Seidel `SNESNCG`

   Not Collective

   Input Parameters:
+  snes - the `SNES` context
.  atol - absolute convergence tolerance
.  rtol - relative convergence tolerance
.  stol -  convergence tolerance in terms of the norm
           of the change in the solution between steps
-  maxit - maximum number of iterations

   Level: intermediate

   Note:
   The user can specify NULL for any parameter that is not needed.

.seealso: `SNESNCG`, `SNESSetTolerances()`
@*/
PetscErrorCode SNESNGSGetTolerances(SNES snes, PetscReal *atol, PetscReal *rtol, PetscReal *stol, PetscInt *maxit)
{
  SNES_NGS *gs = (SNES_NGS *)snes->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);
  if (atol) *atol = gs->abstol;
  if (rtol) *rtol = gs->rtol;
  if (stol) *stol = gs->stol;
  if (maxit) *maxit = gs->max_its;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESNGSSetSweeps - Sets the number of sweeps of nonlinear GS to use in `SNESNCG`

   Input Parameters:
+  snes   - the `SNES` context
-  sweeps  - the number of sweeps of nonlinear GS to perform.

  Options Database Key:
.   -snes_ngs_sweeps <n> - Number of sweeps of nonlinear GS to apply

   Level: intermediate

.seealso: `SNESNCG`, `SNESSetNGS()`, `SNESGetNGS()`, `SNESSetNPC()`, `SNESNGSGetSweeps()`
@*/

PetscErrorCode SNESNGSSetSweeps(SNES snes, PetscInt sweeps)
{
  SNES_NGS *gs = (SNES_NGS *)snes->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);
  gs->sweeps = sweeps;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESNGSGetSweeps - Gets the number of sweeps nonlinear GS will use in `SNESNCG`

   Input Parameter:
.  snes   - the `SNES` context

   Output Parameter:
.  sweeps  - the number of sweeps of nonlinear GS to perform.

   Level: intermediate

.seealso: `SNESNCG`, `SNESSetNGS()`, `SNESGetNGS()`, `SNESSetNPC()`, `SNESNGSSetSweeps()`
@*/
PetscErrorCode SNESNGSGetSweeps(SNES snes, PetscInt *sweeps)
{
  SNES_NGS *gs = (SNES_NGS *)snes->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);
  *sweeps = gs->sweeps;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SNESReset_NGS(SNES snes)
{
  SNES_NGS *gs = (SNES_NGS *)snes->data;

  PetscFunctionBegin;
  PetscCall(ISColoringDestroy(&gs->coloring));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SNESDestroy_NGS(SNES snes)
{
  PetscFunctionBegin;
  PetscCall(SNESReset_NGS(snes));
  PetscCall(PetscFree(snes->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SNESSetUp_NGS(SNES snes)
{
  PetscErrorCode (*f)(SNES, Vec, Vec, void *);

  PetscFunctionBegin;
  PetscCall(SNESGetNGS(snes, &f, NULL));
  if (!f) PetscCall(SNESSetNGS(snes, SNESComputeNGSDefaultSecant, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SNESSetFromOptions_NGS(SNES snes, PetscOptionItems *PetscOptionsObject)
{
  SNES_NGS *gs = (SNES_NGS *)snes->data;
  PetscInt  sweeps, max_its = PETSC_DEFAULT;
  PetscReal rtol = PETSC_DEFAULT, atol = PETSC_DEFAULT, stol = PETSC_DEFAULT;
  PetscBool flg, flg1, flg2, flg3;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "SNES GS options");
  /* GS Options */
  PetscCall(PetscOptionsInt("-snes_ngs_sweeps", "Number of sweeps of GS to apply", "SNESComputeGS", gs->sweeps, &sweeps, &flg));
  if (flg) PetscCall(SNESNGSSetSweeps(snes, sweeps));
  PetscCall(PetscOptionsReal("-snes_ngs_atol", "Absolute residual tolerance for GS iteration", "SNESComputeGS", gs->abstol, &atol, &flg));
  PetscCall(PetscOptionsReal("-snes_ngs_rtol", "Relative residual tolerance for GS iteration", "SNESComputeGS", gs->rtol, &rtol, &flg1));
  PetscCall(PetscOptionsReal("-snes_ngs_stol", "Absolute update tolerance for GS iteration", "SNESComputeGS", gs->stol, &stol, &flg2));
  PetscCall(PetscOptionsInt("-snes_ngs_max_it", "Maximum number of sweeps of GS to apply", "SNESComputeGS", gs->max_its, &max_its, &flg3));
  if (flg || flg1 || flg2 || flg3) PetscCall(SNESNGSSetTolerances(snes, atol, rtol, stol, max_its));
  flg = PETSC_FALSE;
  PetscCall(PetscOptionsBool("-snes_ngs_secant", "Use finite difference secant approximation with coloring", "", flg, &flg, NULL));
  if (flg) {
    PetscCall(SNESSetNGS(snes, SNESComputeNGSDefaultSecant, NULL));
    PetscCall(PetscInfo(snes, "Setting default finite difference secant approximation with coloring\n"));
  }
  PetscCall(PetscOptionsReal("-snes_ngs_secant_h", "Differencing parameter for secant search", "", gs->h, &gs->h, NULL));
  PetscCall(PetscOptionsBool("-snes_ngs_secant_mat_coloring", "Use the graph coloring of the Jacobian for the secant GS", "", gs->secant_mat, &gs->secant_mat, &flg));

  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SNESView_NGS(SNES snes, PetscViewer viewer)
{
  PetscErrorCode (*f)(SNES, Vec, Vec, void *);
  SNES_NGS *gs = (SNES_NGS *)snes->data;
  PetscBool iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(DMSNESGetNGS(snes->dm, &f, NULL));
    if (f == SNESComputeNGSDefaultSecant) PetscCall(PetscViewerASCIIPrintf(viewer, "  Use finite difference secant approximation with coloring with h = %g \n", (double)gs->h));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SNESSolve_NGS(SNES snes)
{
  Vec              F;
  Vec              X;
  Vec              B;
  PetscInt         i;
  PetscReal        fnorm;
  SNESNormSchedule normschedule;

  PetscFunctionBegin;

  PetscCheck(!snes->xl && !snes->xu && !snes->ops->computevariablebounds, PetscObjectComm((PetscObject)snes), PETSC_ERR_ARG_WRONGSTATE, "SNES solver %s does not support bounds", ((PetscObject)snes)->type_name);

  PetscCall(PetscCitationsRegister(SNESCitation, &SNEScite));
  X = snes->vec_sol;
  F = snes->vec_func;
  B = snes->vec_rhs;

  PetscCall(PetscObjectSAWsTakeAccess((PetscObject)snes));
  snes->iter = 0;
  snes->norm = 0.;
  PetscCall(PetscObjectSAWsGrantAccess((PetscObject)snes));
  snes->reason = SNES_CONVERGED_ITERATING;

  PetscCall(SNESGetNormSchedule(snes, &normschedule));
  if (normschedule == SNES_NORM_ALWAYS || normschedule == SNES_NORM_INITIAL_ONLY || normschedule == SNES_NORM_INITIAL_FINAL_ONLY) {
    /* compute the initial function and preconditioned update delX */
    if (!snes->vec_func_init_set) {
      PetscCall(SNESComputeFunction(snes, X, F));
    } else snes->vec_func_init_set = PETSC_FALSE;

    PetscCall(VecNorm(F, NORM_2, &fnorm)); /* fnorm <- ||F||  */
    SNESCheckFunctionNorm(snes, fnorm);
    PetscCall(PetscObjectSAWsTakeAccess((PetscObject)snes));
    snes->iter = 0;
    snes->norm = fnorm;
    PetscCall(PetscObjectSAWsGrantAccess((PetscObject)snes));
    PetscCall(SNESLogConvergenceHistory(snes, snes->norm, 0));
    PetscCall(SNESMonitor(snes, 0, snes->norm));

    /* test convergence */
    PetscUseTypeMethod(snes, converged, 0, 0.0, 0.0, fnorm, &snes->reason, snes->cnvP);
    if (snes->reason) PetscFunctionReturn(PETSC_SUCCESS);
  } else {
    PetscCall(PetscObjectSAWsGrantAccess((PetscObject)snes));
    PetscCall(SNESLogConvergenceHistory(snes, snes->norm, 0));
  }

  /* Call general purpose update function */
  PetscTryTypeMethod(snes, update, snes->iter);

  for (i = 0; i < snes->max_its; i++) {
    PetscCall(SNESComputeNGS(snes, B, X));
    /* only compute norms if requested or about to exit due to maximum iterations */
    if (normschedule == SNES_NORM_ALWAYS || ((i == snes->max_its - 1) && (normschedule == SNES_NORM_INITIAL_FINAL_ONLY || normschedule == SNES_NORM_FINAL_ONLY))) {
      PetscCall(SNESComputeFunction(snes, X, F));
      PetscCall(VecNorm(F, NORM_2, &fnorm)); /* fnorm <- ||F||  */
      SNESCheckFunctionNorm(snes, fnorm);
      /* Monitor convergence */
      PetscCall(PetscObjectSAWsTakeAccess((PetscObject)snes));
      snes->iter = i + 1;
      snes->norm = fnorm;
      PetscCall(PetscObjectSAWsGrantAccess((PetscObject)snes));
      PetscCall(SNESLogConvergenceHistory(snes, snes->norm, 0));
      PetscCall(SNESMonitor(snes, snes->iter, snes->norm));
    }
    /* Test for convergence */
    if (normschedule == SNES_NORM_ALWAYS) PetscUseTypeMethod(snes, converged, snes->iter, 0.0, 0.0, fnorm, &snes->reason, snes->cnvP);
    if (snes->reason) PetscFunctionReturn(PETSC_SUCCESS);
    /* Call general purpose update function */
    PetscTryTypeMethod(snes, update, snes->iter);
  }
  if (normschedule == SNES_NORM_ALWAYS) {
    if (i == snes->max_its) {
      PetscCall(PetscInfo(snes, "Maximum number of iterations has been reached: %" PetscInt_FMT "\n", snes->max_its));
      if (!snes->reason) snes->reason = SNES_DIVERGED_MAX_IT;
    }
  } else if (!snes->reason) snes->reason = SNES_CONVERGED_ITS; /* GS is meant to be used as a preconditioner */
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
  SNESNGS - Either calls the user-provided solution routine provided with `SNESSetNGS()` or does a finite difference secant approximation
            using coloring.

   Level: advanced

  Options Database Keys:
+   -snes_ngs_sweeps <n> - Number of sweeps of nonlinear GS to apply
.    -snes_ngs_atol <atol> - Absolute residual tolerance for nonlinear GS iteration
.    -snes_ngs_rtol <rtol> - Relative residual tolerance for nonlinear GS iteration
.    -snes_ngs_stol <stol> - Absolute update tolerance for nonlinear GS iteration
.    -snes_ngs_max_it <maxit> - Maximum number of sweeps of nonlinea GS to apply
.    -snes_ngs_secant - Use pointwise secant local Jacobian approximation with coloring instead of user provided Gauss-Seidel routine, this is
                        used by default if no user provided Gauss-Seidel routine is available. Requires either that a `DM` that can compute a coloring
                        is available or a Jacobian sparse matrix is provided (from which to get the coloring).
.    -snes_ngs_secant_h <h> - Differencing parameter for secant approximation
.    -snes_ngs_secant_mat_coloring - Use the graph coloring of the Jacobian for the secant GS even if a DM is available.
-    -snes_norm_schedule <none, always, initialonly, finalonly, initialfinalonly> - how often the residual norms are computed

  Notes:
  the Gauss-Seidel smoother is inherited through composition.  If a solver has been created with `SNESGetNPC()`, it will have
  its parent's Gauss-Seidel routine associated with it.

  By default this routine computes the solution norm at each iteration, this can be time consuming, you can turn this off with `SNESSetNormSchedule()`
  or -snes_norm_schedule none

   References:
.  * - Peter R. Brune, Matthew G. Knepley, Barry F. Smith, and Xuemin Tu, "Composing Scalable Nonlinear Algebraic Solvers",
   SIAM Review, 57(4), 2015

.seealso: `SNESNCG`, `SNESCreate()`, `SNES`, `SNESSetType()`, `SNESSetNGS()`, `SNESType`, `SNESNGSSetSweeps()`, `SNESNGSSetTolerances()`,
          `SNESSetNormSchedule()`
M*/

PETSC_EXTERN PetscErrorCode SNESCreate_NGS(SNES snes)
{
  SNES_NGS *gs;

  PetscFunctionBegin;
  snes->ops->destroy        = SNESDestroy_NGS;
  snes->ops->setup          = SNESSetUp_NGS;
  snes->ops->setfromoptions = SNESSetFromOptions_NGS;
  snes->ops->view           = SNESView_NGS;
  snes->ops->solve          = SNESSolve_NGS;
  snes->ops->reset          = SNESReset_NGS;

  snes->usesksp = PETSC_FALSE;
  snes->usesnpc = PETSC_FALSE;

  snes->alwayscomputesfinalresidual = PETSC_FALSE;

  if (!snes->tolerancesset) {
    snes->max_its   = 10000;
    snes->max_funcs = 10000;
  }

  PetscCall(PetscNew(&gs));

  gs->sweeps  = 1;
  gs->rtol    = 1e-5;
  gs->abstol  = PETSC_MACHINE_EPSILON;
  gs->stol    = 1000 * PETSC_MACHINE_EPSILON;
  gs->max_its = 50;
  gs->h       = PETSC_SQRT_MACHINE_EPSILON;

  snes->data = (void *)gs;
  PetscFunctionReturn(PETSC_SUCCESS);
}

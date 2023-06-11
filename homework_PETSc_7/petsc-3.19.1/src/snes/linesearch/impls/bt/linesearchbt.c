#include <petsc/private/linesearchimpl.h> /*I  "petscsnes.h"  I*/
#include <petsc/private/snesimpl.h>

typedef struct {
  PetscReal alpha; /* sufficient decrease parameter */
} SNESLineSearch_BT;

/*@
   SNESLineSearchBTSetAlpha - Sets the descent parameter, alpha, in the `SNESLINESEARCHBT` variant.

   Input Parameters:
+  linesearch - linesearch context
-  alpha - The descent parameter

   Level: intermediate

.seealso: `SNESLineSearch`, `SNESLineSearchSetLambda()`, `SNESLineSearchGetTolerances()`, `SNESLINESEARCHBT`, `SNESLineSearchBTGetAlpha()`
@*/
PetscErrorCode SNESLineSearchBTSetAlpha(SNESLineSearch linesearch, PetscReal alpha)
{
  SNESLineSearch_BT *bt = (SNESLineSearch_BT *)linesearch->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  bt->alpha = alpha;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   SNESLineSearchBTGetAlpha - Gets the descent parameter, alpha, in the `SNESLINESEARCHBT` variant.

   Input Parameter:
.  linesearch - linesearch context

   Output Parameter:
.  alpha - The descent parameter

   Level: intermediate

.seealso: `SNESLineSearchGetLambda()`, `SNESLineSearchGetTolerances()` `SNESLINESEARCHBT`, `SNESLineSearchBTGetAlpha()`
@*/
PetscErrorCode SNESLineSearchBTGetAlpha(SNESLineSearch linesearch, PetscReal *alpha)
{
  SNESLineSearch_BT *bt = (SNESLineSearch_BT *)linesearch->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(linesearch, SNESLINESEARCH_CLASSID, 1);
  *alpha = bt->alpha;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SNESLineSearchApply_BT(SNESLineSearch linesearch)
{
  PetscBool          changed_y, changed_w;
  Vec                X, F, Y, W, G;
  SNES               snes;
  PetscReal          fnorm, xnorm, ynorm, gnorm;
  PetscReal          lambda, lambdatemp, lambdaprev, minlambda, maxstep, initslope, alpha, stol;
  PetscReal          t1, t2, a, b, d;
  PetscReal          f;
  PetscReal          g, gprev;
  PetscViewer        monitor;
  PetscInt           max_its, count;
  SNESLineSearch_BT *bt = (SNESLineSearch_BT *)linesearch->data;
  Mat                jac;
  const char *const  ordStr[] = {"Linear", "Quadratic", "Cubic"};
  PetscErrorCode (*objective)(SNES, Vec, PetscReal *, void *);

  PetscFunctionBegin;
  PetscCall(SNESLineSearchGetVecs(linesearch, &X, &F, &Y, &W, &G));
  PetscCall(SNESLineSearchGetNorms(linesearch, NULL, &fnorm, NULL));
  PetscCall(SNESLineSearchGetLambda(linesearch, &lambda));
  PetscCall(SNESLineSearchGetSNES(linesearch, &snes));
  PetscCall(SNESLineSearchGetDefaultMonitor(linesearch, &monitor));
  PetscCall(SNESLineSearchGetTolerances(linesearch, &minlambda, &maxstep, NULL, NULL, NULL, &max_its));
  PetscCall(SNESGetTolerances(snes, NULL, NULL, &stol, NULL, NULL));
  PetscCall(SNESGetObjective(snes, &objective, NULL));
  alpha = bt->alpha;

  PetscCall(SNESGetJacobian(snes, &jac, NULL, NULL, NULL));
  PetscCheck(jac || objective, PetscObjectComm((PetscObject)linesearch), PETSC_ERR_USER, "SNESLineSearchBT requires a Jacobian matrix");

  PetscCall(SNESLineSearchPreCheck(linesearch, X, Y, &changed_y));
  PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_SUCCEEDED));

  PetscCall(VecNormBegin(Y, NORM_2, &ynorm));
  PetscCall(VecNormBegin(X, NORM_2, &xnorm));
  PetscCall(VecNormEnd(Y, NORM_2, &ynorm));
  PetscCall(VecNormEnd(X, NORM_2, &xnorm));

  if (ynorm == 0.0) {
    if (monitor) {
      PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
      PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: Initial direction and size is 0\n"));
      PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
    }
    PetscCall(VecCopy(X, W));
    PetscCall(VecCopy(F, G));
    PetscCall(SNESLineSearchSetNorms(linesearch, xnorm, fnorm, ynorm));
    PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_FAILED_REDUCT));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (ynorm > maxstep) { /* Step too big, so scale back */
    if (monitor) {
      PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
      PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: Scaling step by %14.12e old ynorm %14.12e\n", (double)(maxstep / ynorm), (double)ynorm));
      PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
    }
    PetscCall(VecScale(Y, maxstep / (ynorm)));
    ynorm = maxstep;
  }

  /* if the SNES has an objective set, use that instead of the function value */
  if (objective) {
    PetscCall(SNESComputeObjective(snes, X, &f));
  } else {
    f = PetscSqr(fnorm);
  }

  /* compute the initial slope */
  if (objective) {
    /* slope comes from the function (assumed to be the gradient of the objective) */
    PetscCall(VecDotRealPart(Y, F, &initslope));
    initslope *= 2.0; /* all the fitting assumes f = ||grad||^2, not f = 0.5 ||grad||^2 */
  } else {
    /* slope comes from the normal equations */
    PetscCall(MatMult(jac, Y, W));
    PetscCall(VecDotRealPart(F, W, &initslope));
    if (initslope > 0.0) initslope = -initslope;
    if (initslope == 0.0) initslope = -1.0;
  }

  while (PETSC_TRUE) {
    PetscCall(VecWAXPY(W, -lambda, Y, X));
    if (linesearch->ops->viproject) PetscCall((*linesearch->ops->viproject)(snes, W));
    if (snes->nfuncs >= snes->max_funcs && snes->max_funcs >= 0) {
      PetscCall(PetscInfo(snes, "Exceeded maximum function evaluations, while checking full step length!\n"));
      snes->reason = SNES_DIVERGED_FUNCTION_COUNT;
      PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_FAILED_FUNCTION));
      PetscFunctionReturn(PETSC_SUCCESS);
    }

    if (objective) {
      PetscCall(SNESComputeObjective(snes, W, &g));
    } else {
      PetscCall((*linesearch->ops->snesfunc)(snes, W, G));
      if (linesearch->ops->vinorm) {
        gnorm = fnorm;
        PetscCall((*linesearch->ops->vinorm)(snes, G, W, &gnorm));
      } else {
        PetscCall(VecNorm(G, NORM_2, &gnorm));
      }
      g = PetscSqr(gnorm);
    }
    PetscCall(SNESLineSearchMonitor(linesearch));

    if (!PetscIsInfOrNanReal(g)) break;
    if (monitor) {
      PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
      PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: objective function at lambdas = %g is Inf or Nan, cutting lambda\n", (double)lambda));
      PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
    }
    if (lambda <= minlambda) SNESCheckFunctionNorm(snes, g);
    lambda *= .5;
  }

  if (!objective) PetscCall(PetscInfo(snes, "Initial fnorm %14.12e gnorm %14.12e\n", (double)fnorm, (double)gnorm));
  if (.5 * g <= .5 * f + lambda * alpha * initslope) { /* Sufficient reduction or step tolerance convergence */
    if (monitor) {
      PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
      if (!objective) {
        PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: Using full step: fnorm %14.12e gnorm %14.12e\n", (double)fnorm, (double)gnorm));
      } else {
        PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: Using full step: obj %14.12e obj %14.12e\n", (double)f, (double)g));
      }
      PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
    }
  } else {
    if (stol * xnorm > ynorm) {
      /* Since the full step didn't give sufficient decrease and the step is tiny, exit */
      PetscCall(SNESLineSearchSetNorms(linesearch, xnorm, fnorm, ynorm));
      PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_SUCCEEDED));
      if (monitor) {
        PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
        PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: Ended due to ynorm < stol*xnorm (%14.12e < %14.12e).\n", (double)ynorm, (double)(stol * xnorm)));
        PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
      }
      PetscFunctionReturn(PETSC_SUCCESS);
    }
    /* Here to avoid -Wmaybe-uninitiliazed warnings */
    lambdaprev = lambda;
    gprev      = g;
    if (linesearch->order != SNES_LINESEARCH_ORDER_LINEAR) {
      /* Fit points with quadratic */
      lambdatemp = -initslope / (g - f - 2.0 * lambda * initslope);
      if (lambdatemp > .5 * lambda) lambdatemp = .5 * lambda;
      if (lambdatemp <= .1 * lambda) lambda = .1 * lambda;
      else lambda = lambdatemp;

      PetscCall(VecWAXPY(W, -lambda, Y, X));
      if (linesearch->ops->viproject) PetscCall((*linesearch->ops->viproject)(snes, W));
      if (snes->nfuncs >= snes->max_funcs && snes->max_funcs >= 0) {
        PetscCall(PetscInfo(snes, "Exceeded maximum function evaluations, while attempting quadratic backtracking! %" PetscInt_FMT " \n", snes->nfuncs));
        snes->reason = SNES_DIVERGED_FUNCTION_COUNT;
        PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_FAILED_FUNCTION));
        PetscFunctionReturn(PETSC_SUCCESS);
      }
      if (objective) {
        PetscCall(SNESComputeObjective(snes, W, &g));
      } else {
        PetscCall((*linesearch->ops->snesfunc)(snes, W, G));
        if (linesearch->ops->vinorm) {
          gnorm = fnorm;
          PetscCall((*linesearch->ops->vinorm)(snes, G, W, &gnorm));
        } else {
          PetscCall(VecNorm(G, NORM_2, &gnorm));
        }
        g = PetscSqr(gnorm);
      }
      if (PetscIsInfOrNanReal(g)) {
        PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_FAILED_NANORINF));
        PetscCall(PetscInfo(snes, "Aborted due to Nan or Inf in function evaluation\n"));
        PetscFunctionReturn(PETSC_SUCCESS);
      }
      if (monitor) {
        PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
        if (!objective) {
          PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: gnorm after quadratic fit %14.12e\n", (double)gnorm));
        } else {
          PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: obj after quadratic fit %14.12e\n", (double)g));
        }
        PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
      }
    }
    if (linesearch->order != SNES_LINESEARCH_ORDER_LINEAR && .5 * g < .5 * f + lambda * alpha * initslope) { /* sufficient reduction */
      if (monitor) {
        PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
        PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: Quadratically determined step, lambda=%18.16e\n", (double)lambda));
        PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
      }
    } else {
      /* Fit points with cubic */
      for (count = 0; count < max_its; count++) {
        if (lambda <= minlambda) {
          if (monitor) {
            PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
            PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: unable to find good step length! After %" PetscInt_FMT " tries \n", count));
            if (!objective) {
              PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: fnorm=%18.16e, gnorm=%18.16e, ynorm=%18.16e, minlambda=%18.16e, lambda=%18.16e, initial slope=%18.16e\n", (double)fnorm, (double)gnorm, (double)ynorm, (double)minlambda, (double)lambda, (double)initslope));
            } else {
              PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: obj(0)=%18.16e, obj=%18.16e, ynorm=%18.16e, minlambda=%18.16e, lambda=%18.16e, initial slope=%18.16e\n", (double)f, (double)g, (double)ynorm, (double)minlambda, (double)lambda, (double)initslope / 2.0));
            }
            PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
          }
          PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_FAILED_REDUCT));
          PetscFunctionReturn(PETSC_SUCCESS);
        }
        if (linesearch->order == SNES_LINESEARCH_ORDER_CUBIC) {
          t1 = .5 * (g - f) - lambda * initslope;
          t2 = .5 * (gprev - f) - lambdaprev * initslope;
          a  = (t1 / (lambda * lambda) - t2 / (lambdaprev * lambdaprev)) / (lambda - lambdaprev);
          b  = (-lambdaprev * t1 / (lambda * lambda) + lambda * t2 / (lambdaprev * lambdaprev)) / (lambda - lambdaprev);
          d  = b * b - 3 * a * initslope;
          if (d < 0.0) d = 0.0;
          if (a == 0.0) lambdatemp = -initslope / (2.0 * b);
          else lambdatemp = (-b + PetscSqrtReal(d)) / (3.0 * a);
        } else if (linesearch->order == SNES_LINESEARCH_ORDER_QUADRATIC) {
          lambdatemp = -initslope / (g - f - 2.0 * initslope);
        } else if (linesearch->order == SNES_LINESEARCH_ORDER_LINEAR) { /* Just backtrack */
          lambdatemp = .5 * lambda;
        } else SETERRQ(PetscObjectComm((PetscObject)linesearch), PETSC_ERR_SUP, "Line search order %" PetscInt_FMT " for type bt", linesearch->order);
        lambdaprev = lambda;
        gprev      = g;
        if (lambdatemp > .5 * lambda) lambdatemp = .5 * lambda;
        if (lambdatemp <= .1 * lambda) lambda = .1 * lambda;
        else lambda = lambdatemp;
        PetscCall(VecWAXPY(W, -lambda, Y, X));
        if (linesearch->ops->viproject) PetscCall((*linesearch->ops->viproject)(snes, W));
        if (snes->nfuncs >= snes->max_funcs && snes->max_funcs >= 0) {
          PetscCall(PetscInfo(snes, "Exceeded maximum function evaluations, while looking for good step length! %" PetscInt_FMT " \n", count));
          if (!objective) PetscCall(PetscInfo(snes, "fnorm=%18.16e, gnorm=%18.16e, ynorm=%18.16e, lambda=%18.16e, initial slope=%18.16e\n", (double)fnorm, (double)gnorm, (double)ynorm, (double)lambda, (double)initslope));
          PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_FAILED_FUNCTION));
          snes->reason = SNES_DIVERGED_FUNCTION_COUNT;
          PetscFunctionReturn(PETSC_SUCCESS);
        }
        if (objective) {
          PetscCall(SNESComputeObjective(snes, W, &g));
        } else {
          PetscCall((*linesearch->ops->snesfunc)(snes, W, G));
          if (linesearch->ops->vinorm) {
            gnorm = fnorm;
            PetscCall((*linesearch->ops->vinorm)(snes, G, W, &gnorm));
          } else {
            PetscCall(VecNorm(G, NORM_2, &gnorm));
          }
          g = PetscSqr(gnorm);
        }
        if (PetscIsInfOrNanReal(g)) {
          PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_FAILED_NANORINF));
          PetscCall(PetscInfo(snes, "Aborted due to Nan or Inf in function evaluation\n"));
          PetscFunctionReturn(PETSC_SUCCESS);
        }
        if (.5 * g < .5 * f + lambda * alpha * initslope) { /* is reduction enough? */
          if (monitor) {
            PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
            if (!objective) {
              PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: %s step, current gnorm %14.12e lambda=%18.16e\n", ordStr[linesearch->order - 1], (double)gnorm, (double)lambda));
              PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
            } else {
              PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: %s step, obj %14.12e lambda=%18.16e\n", ordStr[linesearch->order - 1], (double)g, (double)lambda));
              PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
            }
          }
          break;
        } else if (monitor) {
          PetscCall(PetscViewerASCIIAddTab(monitor, ((PetscObject)linesearch)->tablevel));
          if (!objective) {
            PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: %s step no good, shrinking lambda, current gnorm %12.12e lambda=%18.16e\n", ordStr[linesearch->order - 1], (double)gnorm, (double)lambda));
            PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
          } else {
            PetscCall(PetscViewerASCIIPrintf(monitor, "    Line search: %s step no good, shrinking lambda, obj %12.12e lambda=%18.16e\n", ordStr[linesearch->order - 1], (double)g, (double)lambda));
            PetscCall(PetscViewerASCIISubtractTab(monitor, ((PetscObject)linesearch)->tablevel));
          }
        }
      }
    }
  }

  /* postcheck */
  /* update Y to lambda*Y so that W is consistent with  X - lambda*Y */
  PetscCall(VecScale(Y, lambda));
  PetscCall(SNESLineSearchPostCheck(linesearch, X, Y, W, &changed_y, &changed_w));
  if (changed_y) {
    PetscCall(VecWAXPY(W, -1.0, Y, X));
    if (linesearch->ops->viproject) PetscCall((*linesearch->ops->viproject)(snes, W));
  }
  if (changed_y || changed_w || objective) { /* recompute the function norm if the step has changed or the objective isn't the norm */
    PetscCall((*linesearch->ops->snesfunc)(snes, W, G));
    if (linesearch->ops->vinorm) {
      gnorm = fnorm;
      PetscCall((*linesearch->ops->vinorm)(snes, G, W, &gnorm));
    } else {
      PetscCall(VecNorm(G, NORM_2, &gnorm));
    }
    PetscCall(VecNorm(Y, NORM_2, &ynorm));
    if (PetscIsInfOrNanReal(gnorm)) {
      PetscCall(SNESLineSearchSetReason(linesearch, SNES_LINESEARCH_FAILED_NANORINF));
      PetscCall(PetscInfo(snes, "Aborted due to Nan or Inf in function evaluation\n"));
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }

  /* copy the solution over */
  PetscCall(VecCopy(W, X));
  PetscCall(VecCopy(G, F));
  PetscCall(VecNorm(X, NORM_2, &xnorm));
  PetscCall(SNESLineSearchSetLambda(linesearch, lambda));
  PetscCall(SNESLineSearchSetNorms(linesearch, xnorm, gnorm, ynorm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SNESLineSearchView_BT(SNESLineSearch linesearch, PetscViewer viewer)
{
  PetscBool          iascii;
  SNESLineSearch_BT *bt = (SNESLineSearch_BT *)linesearch->data;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    if (linesearch->order == SNES_LINESEARCH_ORDER_CUBIC) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "  interpolation: cubic\n"));
    } else if (linesearch->order == SNES_LINESEARCH_ORDER_QUADRATIC) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "  interpolation: quadratic\n"));
    }
    PetscCall(PetscViewerASCIIPrintf(viewer, "  alpha=%e\n", (double)bt->alpha));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SNESLineSearchDestroy_BT(SNESLineSearch linesearch)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(linesearch->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SNESLineSearchSetFromOptions_BT(SNESLineSearch linesearch, PetscOptionItems *PetscOptionsObject)
{
  SNESLineSearch_BT *bt = (SNESLineSearch_BT *)linesearch->data;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "SNESLineSearch BT options");
  PetscCall(PetscOptionsReal("-snes_linesearch_alpha", "Descent tolerance", "SNESLineSearchBT", bt->alpha, &bt->alpha, NULL));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   SNESLINESEARCHBT - Backtracking line search.

   This line search finds the minimum of a polynomial fitting of the L2 norm of the
   function or the objective function if it is provided with `SNESSetObjective()`.
   If this fit does not satisfy the conditions for progress, the interval shrinks
   and the fit is reattempted at most max_it times or until lambda is below minlambda.

   Options Database Keys:
+  -snes_linesearch_alpha <1e\-4> - slope descent parameter
.  -snes_linesearch_damping <1.0> - initial step length
.  -snes_linesearch_maxstep <length> - if the length the initial step is larger than this then the
                                       step is scaled back to be of this length at the beginning of the line search
.  -snes_linesearch_max_it <40> - maximum number of shrinking step
.  -snes_linesearch_minlambda <1e\-12> - minimum step length allowed
-  -snes_linesearch_order <1,2,3> - order of the approximation. With order 1, it performs a simple backtracking without any curve fitting

   Level: advanced

   Note:
   This line search will always produce a step that is less than or equal to, in length, the full step size.

   Reference:
.  - * - "Numerical Methods for Unconstrained Optimization and Nonlinear Equations" by Dennis and Schnabel, page 325.

.seealso: `SNESLineSearch`, `SNESLineSearchType`, `SNESLineSearchCreate()`, `SNESLineSearchSetType()`
M*/
PETSC_EXTERN PetscErrorCode SNESLineSearchCreate_BT(SNESLineSearch linesearch)
{
  SNESLineSearch_BT *bt;

  PetscFunctionBegin;
  linesearch->ops->apply          = SNESLineSearchApply_BT;
  linesearch->ops->destroy        = SNESLineSearchDestroy_BT;
  linesearch->ops->setfromoptions = SNESLineSearchSetFromOptions_BT;
  linesearch->ops->reset          = NULL;
  linesearch->ops->view           = SNESLineSearchView_BT;
  linesearch->ops->setup          = NULL;

  PetscCall(PetscNew(&bt));

  linesearch->data    = (void *)bt;
  linesearch->max_its = 40;
  linesearch->order   = SNES_LINESEARCH_ORDER_CUBIC;
  bt->alpha           = 1e-4;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
       Code for Timestepping with explicit Euler.
*/
#include <petsc/private/tsimpl.h> /*I   "petscts.h"   I*/

typedef struct {
  Vec update; /* work vector where new solution is formed  */
} TS_Euler;

static PetscErrorCode TSStep_Euler(TS ts)
{
  TS_Euler *euler    = (TS_Euler *)ts->data;
  Vec       solution = ts->vec_sol, update = euler->update;
  PetscBool stageok, accept                = PETSC_TRUE;
  PetscReal next_time_step = ts->time_step;

  PetscFunctionBegin;
  PetscCall(TSPreStage(ts, ts->ptime));
  PetscCall(TSComputeRHSFunction(ts, ts->ptime, solution, update));
  PetscCall(VecAYPX(update, ts->time_step, solution));
  PetscCall(TSPostStage(ts, ts->ptime, 0, &solution));
  PetscCall(TSAdaptCheckStage(ts->adapt, ts, ts->ptime, solution, &stageok));
  if (!stageok) {
    ts->reason = TS_DIVERGED_STEP_REJECTED;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(TSFunctionDomainError(ts, ts->ptime + ts->time_step, update, &stageok));
  if (!stageok) {
    ts->reason = TS_DIVERGED_STEP_REJECTED;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  PetscCall(TSAdaptChoose(ts->adapt, ts, ts->time_step, NULL, &next_time_step, &accept));
  if (!accept) {
    ts->reason = TS_DIVERGED_STEP_REJECTED;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(VecCopy(update, solution));

  ts->ptime += ts->time_step;
  ts->time_step = next_time_step;
  PetscFunctionReturn(PETSC_SUCCESS);
}
/*------------------------------------------------------------*/

static PetscErrorCode TSSetUp_Euler(TS ts)
{
  TS_Euler *euler = (TS_Euler *)ts->data;

  PetscFunctionBegin;
  PetscCall(TSCheckImplicitTerm(ts));
  PetscCall(VecDuplicate(ts->vec_sol, &euler->update));
  PetscCall(TSGetAdapt(ts, &ts->adapt));
  PetscCall(TSAdaptCandidatesClear(ts->adapt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSReset_Euler(TS ts)
{
  TS_Euler *euler = (TS_Euler *)ts->data;

  PetscFunctionBegin;
  PetscCall(VecDestroy(&euler->update));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSDestroy_Euler(TS ts)
{
  PetscFunctionBegin;
  PetscCall(TSReset_Euler(ts));
  PetscCall(PetscFree(ts->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}
/*------------------------------------------------------------*/

static PetscErrorCode TSSetFromOptions_Euler(TS ts, PetscOptionItems *PetscOptionsObject)
{
  PetscFunctionBegin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSView_Euler(TS ts, PetscViewer viewer)
{
  PetscFunctionBegin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSInterpolate_Euler(TS ts, PetscReal t, Vec X)
{
  TS_Euler *euler  = (TS_Euler *)ts->data;
  Vec       update = euler->update;
  PetscReal alpha  = (ts->ptime - t) / ts->time_step;

  PetscFunctionBegin;
  PetscCall(VecWAXPY(X, -ts->time_step, update, ts->vec_sol));
  PetscCall(VecAXPBY(X, 1.0 - alpha, alpha, ts->vec_sol));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSComputeLinearStability_Euler(TS ts, PetscReal xr, PetscReal xi, PetscReal *yr, PetscReal *yi)
{
  PetscFunctionBegin;
  *yr = 1.0 + xr;
  *yi = xi;
  PetscFunctionReturn(PETSC_SUCCESS);
}
/* ------------------------------------------------------------ */

/*MC
      TSEULER - ODE solver using the explicit forward Euler method

  Level: beginner

.seealso: [](chapter_ts), `TSCreate()`, `TS`, `TSSetType()`, `TSBEULER`, `TSType`
M*/
PETSC_EXTERN PetscErrorCode TSCreate_Euler(TS ts)
{
  TS_Euler *euler;

  PetscFunctionBegin;
  PetscCall(PetscNew(&euler));
  ts->data = (void *)euler;

  ts->ops->setup           = TSSetUp_Euler;
  ts->ops->step            = TSStep_Euler;
  ts->ops->reset           = TSReset_Euler;
  ts->ops->destroy         = TSDestroy_Euler;
  ts->ops->setfromoptions  = TSSetFromOptions_Euler;
  ts->ops->view            = TSView_Euler;
  ts->ops->interpolate     = TSInterpolate_Euler;
  ts->ops->linearstability = TSComputeLinearStability_Euler;
  ts->default_adapt_type   = TSADAPTNONE;
  ts->usessnes             = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

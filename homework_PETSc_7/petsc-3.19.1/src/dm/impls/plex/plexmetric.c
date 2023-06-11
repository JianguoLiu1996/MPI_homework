#include <petsc/private/dmpleximpl.h> /*I      "petscdmplex.h"   I*/
#include <petscblaslapack.h>

PetscLogEvent DMPLEX_MetricEnforceSPD, DMPLEX_MetricNormalize, DMPLEX_MetricAverage, DMPLEX_MetricIntersection;

PetscErrorCode DMPlexMetricSetFromOptions(DM dm)
{
  DM_Plex  *plex = (DM_Plex *)dm->data;
  MPI_Comm  comm;
  PetscBool isotropic = PETSC_FALSE, uniform = PETSC_FALSE, restrictAnisotropyFirst = PETSC_FALSE;
  PetscBool noInsert = PETSC_FALSE, noSwap = PETSC_FALSE, noMove = PETSC_FALSE, noSurf = PETSC_FALSE;
  PetscInt  verbosity = -1, numIter = 3;
  PetscReal h_min = 1.0e-30, h_max = 1.0e+30, a_max = 1.0e+05, p = 1.0, target = 1000.0, beta = 1.3, hausd = 0.01;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(PetscNew(&plex->metricCtx));
  PetscCall(PetscObjectGetComm((PetscObject)dm, &comm));
  PetscOptionsBegin(comm, "", "Riemannian metric options", "DMPlexMetric");
  PetscCall(PetscOptionsBool("-dm_plex_metric_isotropic", "Is the metric isotropic?", "DMPlexMetricCreateIsotropic", isotropic, &isotropic, NULL));
  PetscCall(DMPlexMetricSetIsotropic(dm, isotropic));
  PetscCall(PetscOptionsBool("-dm_plex_metric_uniform", "Is the metric uniform?", "DMPlexMetricCreateUniform", uniform, &uniform, NULL));
  PetscCall(DMPlexMetricSetUniform(dm, uniform));
  PetscCall(PetscOptionsBool("-dm_plex_metric_restrict_anisotropy_first", "Should anisotropy be restricted before normalization?", "DMPlexNormalize", restrictAnisotropyFirst, &restrictAnisotropyFirst, NULL));
  PetscCall(DMPlexMetricSetRestrictAnisotropyFirst(dm, restrictAnisotropyFirst));
  PetscCall(PetscOptionsBool("-dm_plex_metric_no_insert", "Turn off node insertion and deletion", "DMAdaptMetric", noInsert, &noInsert, NULL));
  PetscCall(DMPlexMetricSetNoInsertion(dm, noInsert));
  PetscCall(PetscOptionsBool("-dm_plex_metric_no_swap", "Turn off facet swapping", "DMAdaptMetric", noSwap, &noSwap, NULL));
  PetscCall(DMPlexMetricSetNoSwapping(dm, noSwap));
  PetscCall(PetscOptionsBool("-dm_plex_metric_no_move", "Turn off facet node movement", "DMAdaptMetric", noMove, &noMove, NULL));
  PetscCall(DMPlexMetricSetNoMovement(dm, noMove));
  PetscCall(PetscOptionsBool("-dm_plex_metric_no_surf", "Turn off surface modification", "DMAdaptMetric", noSurf, &noSurf, NULL));
  PetscCall(DMPlexMetricSetNoSurf(dm, noSurf));
  PetscCall(PetscOptionsBoundedInt("-dm_plex_metric_num_iterations", "Number of ParMmg adaptation iterations", "DMAdaptMetric", numIter, &numIter, NULL, 0));
  PetscCall(DMPlexMetricSetNumIterations(dm, numIter));
  PetscCall(PetscOptionsRangeInt("-dm_plex_metric_verbosity", "Verbosity of metric-based mesh adaptation package (-1 = silent, 10 = maximum)", "DMAdaptMetric", verbosity, &verbosity, NULL, -1, 10));
  PetscCall(DMPlexMetricSetVerbosity(dm, verbosity));
  PetscCall(PetscOptionsReal("-dm_plex_metric_h_min", "Minimum tolerated metric magnitude", "DMPlexMetricEnforceSPD", h_min, &h_min, NULL));
  PetscCall(DMPlexMetricSetMinimumMagnitude(dm, h_min));
  PetscCall(PetscOptionsReal("-dm_plex_metric_h_max", "Maximum tolerated metric magnitude", "DMPlexMetricEnforceSPD", h_max, &h_max, NULL));
  PetscCall(DMPlexMetricSetMaximumMagnitude(dm, h_max));
  PetscCall(PetscOptionsReal("-dm_plex_metric_a_max", "Maximum tolerated anisotropy", "DMPlexMetricEnforceSPD", a_max, &a_max, NULL));
  PetscCall(DMPlexMetricSetMaximumAnisotropy(dm, a_max));
  PetscCall(PetscOptionsReal("-dm_plex_metric_p", "L-p normalization order", "DMPlexMetricNormalize", p, &p, NULL));
  PetscCall(DMPlexMetricSetNormalizationOrder(dm, p));
  PetscCall(PetscOptionsReal("-dm_plex_metric_target_complexity", "Target metric complexity", "DMPlexMetricNormalize", target, &target, NULL));
  PetscCall(DMPlexMetricSetTargetComplexity(dm, target));
  PetscCall(PetscOptionsReal("-dm_plex_metric_gradation_factor", "Metric gradation factor", "DMAdaptMetric", beta, &beta, NULL));
  PetscCall(DMPlexMetricSetGradationFactor(dm, beta));
  PetscCall(PetscOptionsReal("-dm_plex_metric_hausdorff_number", "Metric Hausdorff number", "DMAdaptMetric", hausd, &hausd, NULL));
  PetscCall(DMPlexMetricSetHausdorffNumber(dm, hausd));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetIsotropic - Record whether a metric is isotropic

  Input parameters:
+ dm        - The `DM`
- isotropic - Is the metric isotropic?

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricIsIsotropic()`, `DMPlexMetricSetUniform()`, `DMPlexMetricSetRestrictAnisotropyFirst()`
@*/
PetscErrorCode DMPlexMetricSetIsotropic(DM dm, PetscBool isotropic)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->isotropic = isotropic;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricIsIsotropic - Is a metric isotropic?

  Input parameters:
. dm        - The `DM`

  Output parameters:
. isotropic - Is the metric isotropic?

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricSetIsotropic()`, `DMPlexMetricIsUniform()`, `DMPlexMetricRestrictAnisotropyFirst()`
@*/
PetscErrorCode DMPlexMetricIsIsotropic(DM dm, PetscBool *isotropic)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *isotropic = plex->metricCtx->isotropic;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetUniform - Record whether a metric is uniform

  Input parameters:
+ dm      - The `DM`
- uniform - Is the metric uniform?

  Level: beginner

  Note:
  If the metric is specified as uniform then it is assumed to be isotropic, too.

.seealso: `DMPLEX`, `DMPlexMetricIsUniform()`, `DMPlexMetricSetIsotropic()`, `DMPlexMetricSetRestrictAnisotropyFirst()`
@*/
PetscErrorCode DMPlexMetricSetUniform(DM dm, PetscBool uniform)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->uniform = uniform;
  if (uniform) plex->metricCtx->isotropic = uniform;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricIsUniform - Is a metric uniform?

  Input parameters:
. dm      - The `DM`

  Output parameters:
. uniform - Is the metric uniform?

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricSetUniform()`, `DMPlexMetricIsIsotropic()`, `DMPlexMetricRestrictAnisotropyFirst()`
@*/
PetscErrorCode DMPlexMetricIsUniform(DM dm, PetscBool *uniform)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *uniform = plex->metricCtx->uniform;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetRestrictAnisotropyFirst - Record whether anisotropy should be restricted before normalization

  Input parameters:
+ dm                      - The `DM`
- restrictAnisotropyFirst - Should anisotropy be normalized first?

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricSetIsotropic()`, `DMPlexMetricRestrictAnisotropyFirst()`
@*/
PetscErrorCode DMPlexMetricSetRestrictAnisotropyFirst(DM dm, PetscBool restrictAnisotropyFirst)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->restrictAnisotropyFirst = restrictAnisotropyFirst;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricRestrictAnisotropyFirst - Is anisotropy restricted before normalization or after?

  Input parameters:
. dm                      - The `DM`

  Output parameters:
. restrictAnisotropyFirst - Is anisotropy be normalized first?

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricIsIsotropic()`, `DMPlexMetricSetRestrictAnisotropyFirst()`
@*/
PetscErrorCode DMPlexMetricRestrictAnisotropyFirst(DM dm, PetscBool *restrictAnisotropyFirst)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *restrictAnisotropyFirst = plex->metricCtx->restrictAnisotropyFirst;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetNoInsertion - Should node insertion and deletion be turned off?

  Input parameters:
+ dm       - The `DM`
- noInsert - Should node insertion and deletion be turned off?

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricNoInsertion()`, `DMPlexMetricSetNoSwapping()`, `DMPlexMetricSetNoMovement()`, `DMPlexMetricSetNoSurf()`
@*/
PetscErrorCode DMPlexMetricSetNoInsertion(DM dm, PetscBool noInsert)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->noInsert = noInsert;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricNoInsertion - Are node insertion and deletion turned off?

  Input parameters:
. dm       - The `DM`

  Output parameters:
. noInsert - Are node insertion and deletion turned off?

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricSetNoInsertion()`, `DMPlexMetricNoSwapping()`, `DMPlexMetricNoMovement()`, `DMPlexMetricNoSurf()`
@*/
PetscErrorCode DMPlexMetricNoInsertion(DM dm, PetscBool *noInsert)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *noInsert = plex->metricCtx->noInsert;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetNoSwapping - Should facet swapping be turned off?

  Input parameters:
+ dm     - The `DM`
- noSwap - Should facet swapping be turned off?

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricNoSwapping()`, `DMPlexMetricSetNoInsertion()`, `DMPlexMetricSetNoMovement()`, `DMPlexMetricSetNoSurf()`
@*/
PetscErrorCode DMPlexMetricSetNoSwapping(DM dm, PetscBool noSwap)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->noSwap = noSwap;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricNoSwapping - Is facet swapping turned off?

  Input parameters:
. dm     - The `DM`

  Output parameters:
. noSwap - Is facet swapping turned off?

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricSetNoSwapping()`, `DMPlexMetricNoInsertion()`, `DMPlexMetricNoMovement()`, `DMPlexMetricNoSurf()`
@*/
PetscErrorCode DMPlexMetricNoSwapping(DM dm, PetscBool *noSwap)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *noSwap = plex->metricCtx->noSwap;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetNoMovement - Should node movement be turned off?

  Input parameters:
+ dm     - The `DM`
- noMove - Should node movement be turned off?

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricNoMovement()`, `DMPlexMetricSetNoInsertion()`, `DMPlexMetricSetNoSwapping()`, `DMPlexMetricSetNoSurf()`
@*/
PetscErrorCode DMPlexMetricSetNoMovement(DM dm, PetscBool noMove)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->noMove = noMove;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricNoMovement - Is node movement turned off?

  Input parameters:
. dm     - The `DM`

  Output parameters:
. noMove - Is node movement turned off?

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricSetNoMovement()`, `DMPlexMetricNoInsertion()`, `DMPlexMetricNoSwapping()`, `DMPlexMetricNoSurf()`
@*/
PetscErrorCode DMPlexMetricNoMovement(DM dm, PetscBool *noMove)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *noMove = plex->metricCtx->noMove;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetNoSurf - Should surface modification be turned off?

  Input parameters:
+ dm     - The `DM`
- noSurf - Should surface modification be turned off?

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricNoSurf()`, `DMPlexMetricSetNoMovement()`, `DMPlexMetricSetNoInsertion()`, `DMPlexMetricSetNoSwapping()`
@*/
PetscErrorCode DMPlexMetricSetNoSurf(DM dm, PetscBool noSurf)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->noSurf = noSurf;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricNoSurf - Is surface modification turned off?

  Input parameters:
. dm     - The `DM`

  Output parameters:
. noSurf - Is surface modification turned off?

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricSetNoSurf()`, `DMPlexMetricNoMovement()`, `DMPlexMetricNoInsertion()`, `DMPlexMetricNoSwapping()`
@*/
PetscErrorCode DMPlexMetricNoSurf(DM dm, PetscBool *noSurf)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *noSurf = plex->metricCtx->noSurf;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetMinimumMagnitude - Set the minimum tolerated metric magnitude

  Input parameters:
+ dm    - The `DM`
- h_min - The minimum tolerated metric magnitude

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricGetMinimumMagnitude()`, `DMPlexMetricSetMaximumMagnitude()`
@*/
PetscErrorCode DMPlexMetricSetMinimumMagnitude(DM dm, PetscReal h_min)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  PetscCheck(h_min > 0.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Metric magnitudes must be in (0, inf)");
  plex->metricCtx->h_min = h_min;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetMinimumMagnitude - Get the minimum tolerated metric magnitude

  Input parameters:
. dm    - The `DM`

  Output parameters:
. h_min - The minimum tolerated metric magnitude

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricSetMinimumMagnitude()`, `DMPlexMetricGetMaximumMagnitude()`
@*/
PetscErrorCode DMPlexMetricGetMinimumMagnitude(DM dm, PetscReal *h_min)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *h_min = plex->metricCtx->h_min;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetMaximumMagnitude - Set the maximum tolerated metric magnitude

  Input parameters:
+ dm    - The `DM`
- h_max - The maximum tolerated metric magnitude

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricGetMaximumMagnitude()`, `DMPlexMetricSetMinimumMagnitude()`
@*/
PetscErrorCode DMPlexMetricSetMaximumMagnitude(DM dm, PetscReal h_max)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  PetscCheck(h_max > 0.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Metric magnitudes must be in (0, inf)");
  plex->metricCtx->h_max = h_max;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetMaximumMagnitude - Get the maximum tolerated metric magnitude

  Input parameters:
. dm    - The `DM`

  Output parameters:
. h_max - The maximum tolerated metric magnitude

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricSetMaximumMagnitude()`, `DMPlexMetricGetMinimumMagnitude()`
@*/
PetscErrorCode DMPlexMetricGetMaximumMagnitude(DM dm, PetscReal *h_max)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *h_max = plex->metricCtx->h_max;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetMaximumAnisotropy - Set the maximum tolerated metric anisotropy

  Input parameters:
+ dm    - The `DM`
- a_max - The maximum tolerated metric anisotropy

  Level: beginner

  Note:
  If the value zero is given then anisotropy will not be restricted. Otherwise, it should be at least one.

.seealso: `DMPLEX`, `DMPlexMetricGetMaximumAnisotropy()`, `DMPlexMetricSetMaximumMagnitude()`
@*/
PetscErrorCode DMPlexMetricSetMaximumAnisotropy(DM dm, PetscReal a_max)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  PetscCheck(a_max >= 1.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Anisotropy must be in [1, inf)");
  plex->metricCtx->a_max = a_max;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetMaximumAnisotropy - Get the maximum tolerated metric anisotropy

  Input parameters:
. dm    - The `DM`

  Output parameters:
. a_max - The maximum tolerated metric anisotropy

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricSetMaximumAnisotropy()`, `DMPlexMetricGetMaximumMagnitude()`
@*/
PetscErrorCode DMPlexMetricGetMaximumAnisotropy(DM dm, PetscReal *a_max)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *a_max = plex->metricCtx->a_max;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetTargetComplexity - Set the target metric complexity

  Input parameters:
+ dm               - The `DM`
- targetComplexity - The target metric complexity

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricGetTargetComplexity()`, `DMPlexMetricSetNormalizationOrder()`
@*/
PetscErrorCode DMPlexMetricSetTargetComplexity(DM dm, PetscReal targetComplexity)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  PetscCheck(targetComplexity > 0.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Metric complexity must be in (0, inf)");
  plex->metricCtx->targetComplexity = targetComplexity;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetTargetComplexity - Get the target metric complexity

  Input parameters:
. dm               - The `DM`

  Output parameters:
. targetComplexity - The target metric complexity

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricSetTargetComplexity()`, `DMPlexMetricGetNormalizationOrder()`
@*/
PetscErrorCode DMPlexMetricGetTargetComplexity(DM dm, PetscReal *targetComplexity)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *targetComplexity = plex->metricCtx->targetComplexity;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetNormalizationOrder - Set the order p for L-p normalization

  Input parameters:
+ dm - The `DM`
- p  - The normalization order

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricGetNormalizationOrder()`, `DMPlexMetricSetTargetComplexity()`
@*/
PetscErrorCode DMPlexMetricSetNormalizationOrder(DM dm, PetscReal p)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  PetscCheck(p >= 1.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Normalization order must be in [1, inf)");
  plex->metricCtx->p = p;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetNormalizationOrder - Get the order p for L-p normalization

  Input parameters:
. dm - The `DM`

  Output parameters:
. p - The normalization order

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricSetNormalizationOrder()`, `DMPlexMetricGetTargetComplexity()`
@*/
PetscErrorCode DMPlexMetricGetNormalizationOrder(DM dm, PetscReal *p)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *p = plex->metricCtx->p;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetGradationFactor - Set the metric gradation factor

  Input parameters:
+ dm   - The `DM`
- beta - The metric gradation factor

  Level: beginner

  Notes:
  The gradation factor is the maximum tolerated length ratio between adjacent edges.

  Turn off gradation by passing the value -1. Otherwise, pass a positive value.

  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricGetGradationFactor()`, `DMPlexMetricSetHausdorffNumber()`
@*/
PetscErrorCode DMPlexMetricSetGradationFactor(DM dm, PetscReal beta)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  PetscCheck(beta > 0.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Metric gradation factor must be in (0, inf)");
  plex->metricCtx->gradationFactor = beta;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetGradationFactor - Get the metric gradation factor

  Input parameters:
. dm   - The `DM`

  Output parameters:
. beta - The metric gradation factor

  Level: beginner

  Notes:
  The gradation factor is the maximum tolerated length ratio between adjacent edges.

  The value -1 implies that gradation is turned off.

  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricSetGradationFactor()`, `DMPlexMetricGetHausdorffNumber()`
@*/
PetscErrorCode DMPlexMetricGetGradationFactor(DM dm, PetscReal *beta)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *beta = plex->metricCtx->gradationFactor;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetHausdorffNumber - Set the metric Hausdorff number

  Input parameters:
+ dm    - The `DM`
- hausd - The metric Hausdorff number

  Level: beginner

  Notes:
  The Hausdorff number imposes the maximal distance between the piecewise linear approximation of the
  boundary and the reconstructed ideal boundary. Thus, a low Hausdorff parameter leads to refine the
  high curvature areas. By default, the Hausdorff value is set to 0.01, which is a suitable value for
  an object of size 1 in each direction. For smaller (resp. larger) objects, you may need to decrease
  (resp. increase) the Hausdorff parameter. (Taken from
  https://www.mmgtools.org/mmg-remesher-try-mmg/mmg-remesher-options/mmg-remesher-option-hausd).

  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricSetGradationFactor()`, `DMPlexMetricGetHausdorffNumber()`
@*/
PetscErrorCode DMPlexMetricSetHausdorffNumber(DM dm, PetscReal hausd)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  PetscCheck(hausd > 0.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Metric Hausdorff number must be in (0, inf)");
  plex->metricCtx->hausdorffNumber = hausd;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetHausdorffNumber - Get the metric Hausdorff number

  Input parameters:
. dm    - The `DM`

  Output parameters:
. hausd - The metric Hausdorff number

  Level: beginner

  Notes:
  The Hausdorff number imposes the maximal distance between the piecewise linear approximation of the
  boundary and the reconstructed ideal boundary. Thus, a low Hausdorff parameter leads to refine the
  high curvature areas. By default, the Hausdorff value is set to 0.01, which is a suitable value for
  an object of size 1 in each direction. For smaller (resp. larger) objects, you may need to decrease
  (resp. increase) the Hausdorff parameter. (Taken from
  https://www.mmgtools.org/mmg-remesher-try-mmg/mmg-remesher-options/mmg-remesher-option-hausd).

  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricGetGradationFactor()`, `DMPlexMetricSetHausdorffNumber()`
@*/
PetscErrorCode DMPlexMetricGetHausdorffNumber(DM dm, PetscReal *hausd)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *hausd = plex->metricCtx->hausdorffNumber;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetVerbosity - Set the verbosity of the mesh adaptation package

  Input parameters:
+ dm        - The `DM`
- verbosity - The verbosity, where -1 is silent and 10 is maximum

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricGetVerbosity()`, `DMPlexMetricSetNumIterations()`
@*/
PetscErrorCode DMPlexMetricSetVerbosity(DM dm, PetscInt verbosity)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->verbosity = verbosity;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetVerbosity - Get the verbosity of the mesh adaptation package

  Input parameters:
. dm        - The `DM`

  Output parameters:
. verbosity - The verbosity, where -1 is silent and 10 is maximum

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic).

.seealso: `DMPLEX`, `DMPlexMetricSetVerbosity()`, `DMPlexMetricGetNumIterations()`
@*/
PetscErrorCode DMPlexMetricGetVerbosity(DM dm, PetscInt *verbosity)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *verbosity = plex->metricCtx->verbosity;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricSetNumIterations - Set the number of parallel adaptation iterations

  Input parameters:
+ dm      - The `DM`
- numIter - the number of parallel adaptation iterations

  Level: beginner

  Note:
  This is only used by ParMmg (not Pragmatic or Mmg).

.seealso: `DMPLEX`, `DMPlexMetricSetVerbosity()`, `DMPlexMetricGetNumIterations()`
@*/
PetscErrorCode DMPlexMetricSetNumIterations(DM dm, PetscInt numIter)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  plex->metricCtx->numIter = numIter;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricGetNumIterations - Get the number of parallel adaptation iterations

  Input parameters:
. dm      - The `DM`

  Output parameters:
. numIter - the number of parallel adaptation iterations

  Level: beginner

  Note:
  This is only used by Mmg and ParMmg (not Pragmatic or Mmg).

.seealso: `DMPLEX`, `DMPlexMetricSetNumIterations()`, `DMPlexMetricGetVerbosity()`
@*/
PetscErrorCode DMPlexMetricGetNumIterations(DM dm, PetscInt *numIter)
{
  DM_Plex *plex = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  if (!plex->metricCtx) PetscCall(DMPlexMetricSetFromOptions(dm));
  *numIter = plex->metricCtx->numIter;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexP1FieldCreate_Private(DM dm, PetscInt f, PetscInt size, Vec *metric)
{
  MPI_Comm comm;
  PetscFE  fe;
  PetscInt dim;

  PetscFunctionBegin;

  /* Extract metadata from dm */
  PetscCall(PetscObjectGetComm((PetscObject)dm, &comm));
  PetscCall(DMGetDimension(dm, &dim));

  /* Create a P1 field of the requested size */
  PetscCall(PetscFECreateLagrange(comm, dim, size, PETSC_TRUE, 1, PETSC_DETERMINE, &fe));
  PetscCall(DMSetField(dm, f, NULL, (PetscObject)fe));
  PetscCall(DMCreateDS(dm));
  PetscCall(PetscFEDestroy(&fe));
  PetscCall(DMCreateLocalVector(dm, metric));

  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricCreate - Create a Riemannian metric field

  Input parameters:
+ dm     - The `DM`
- f      - The field number to use

  Output parameter:
. metric - The metric

  Options Database Key:
. -dm_adaptor <pragmatic/mmg/parmmg>        - specify dm adaptor to use

  Options Database Keys for Mmg and ParMmg:
+ -dm_plex_metric_gradation_factor          - Maximum ratio by which edge lengths may grow during gradation
. -dm_plex_metric_num_iterations            - Number of parallel mesh adaptation iterations for ParMmg
. -dm_plex_metric_no_insert                 - Should node insertion/deletion be turned off?
. -dm_plex_metric_no_swap                   - Should facet swapping be turned off?
. -dm_plex_metric_no_move                   - Should node movement be turned off?
- -dm_plex_metric_verbosity                 - Choose a verbosity level from -1 (silent) to 10 (maximum).

  Options Database Keys for Riemannian metrics:
+ -dm_plex_metric_isotropic                 - Is the metric isotropic?
. -dm_plex_metric_uniform                   - Is the metric uniform?
. -dm_plex_metric_restrict_anisotropy_first - Should anisotropy be restricted before normalization?
. -dm_plex_metric_h_min                     - Minimum tolerated metric magnitude
. -dm_plex_metric_h_max                     - Maximum tolerated metric magnitude
. -dm_plex_metric_a_max                     - Maximum tolerated anisotropy
. -dm_plex_metric_p                         - L-p normalization order
- -dm_plex_metric_target_complexity         - Target metric complexity

  Level: beginner

  Note:
  It is assumed that the `DM` is comprised of simplices.

.seealso: `DMPLEX`, `DMPlexMetricCreateUniform()`, `DMPlexMetricCreateIsotropic()`
@*/
PetscErrorCode DMPlexMetricCreate(DM dm, PetscInt f, Vec *metric)
{
  PetscBool isotropic, uniform;
  PetscInt  coordDim, Nd;

  PetscFunctionBegin;
  PetscCall(DMGetCoordinateDim(dm, &coordDim));
  Nd = coordDim * coordDim;
  PetscCall(DMPlexMetricIsUniform(dm, &uniform));
  PetscCall(DMPlexMetricIsIsotropic(dm, &isotropic));
  if (uniform) {
    MPI_Comm comm;

    PetscCall(PetscObjectGetComm((PetscObject)dm, &comm));
    PetscCheck(isotropic, comm, PETSC_ERR_SUP, "Uniform anisotropic metrics not supported");
    PetscCall(VecCreate(comm, metric));
    PetscCall(VecSetSizes(*metric, 1, PETSC_DECIDE));
    PetscCall(VecSetFromOptions(*metric));
  } else if (isotropic) PetscCall(DMPlexP1FieldCreate_Private(dm, f, 1, metric));
  else PetscCall(DMPlexP1FieldCreate_Private(dm, f, Nd, metric));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricCreateUniform - Construct a uniform isotropic metric

  Input parameters:
+ dm     - The `DM`
. f      - The field number to use
- alpha  - Scaling parameter for the diagonal

  Output parameter:
. metric - The uniform metric

  Level: beginner

  Note:
  In this case, the metric is constant in space and so is only specified for a single vertex.

.seealso: `DMPLEX`, `DMPlexMetricCreate()`, `DMPlexMetricCreateIsotropic()`
@*/
PetscErrorCode DMPlexMetricCreateUniform(DM dm, PetscInt f, PetscReal alpha, Vec *metric)
{
  PetscFunctionBegin;
  PetscCall(DMPlexMetricSetUniform(dm, PETSC_TRUE));
  PetscCall(DMPlexMetricCreate(dm, f, metric));
  PetscCheck(alpha, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Uniform metric scaling is undefined");
  PetscCheck(alpha > 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Uniform metric scaling should be in (0, inf)");
  PetscCall(VecSet(*metric, alpha));
  PetscCall(VecAssemblyBegin(*metric));
  PetscCall(VecAssemblyEnd(*metric));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static void identity(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  f0[0] = u[0];
}

/*@
  DMPlexMetricCreateIsotropic - Construct an isotropic metric from an error indicator

  Input parameters:
+ dm        - The `DM`
. f         - The field number to use
- indicator - The error indicator

  Output parameter:
. metric    - The isotropic metric

  Level: beginner

  Notes:
  It is assumed that the `DM` is comprised of simplices.

  The indicator needs to be a scalar field. If it is not defined vertex-wise, then it is projected appropriately.

.seealso: `DMPLEX`, `DMPlexMetricCreate()`, `DMPlexMetricCreateUniform()`
@*/
PetscErrorCode DMPlexMetricCreateIsotropic(DM dm, PetscInt f, Vec indicator, Vec *metric)
{
  PetscInt m, n;

  PetscFunctionBegin;
  PetscCall(DMPlexMetricSetIsotropic(dm, PETSC_TRUE));
  PetscCall(DMPlexMetricCreate(dm, f, metric));
  PetscCall(VecGetSize(indicator, &m));
  PetscCall(VecGetSize(*metric, &n));
  if (m == n) PetscCall(VecCopy(indicator, *metric));
  else {
    DM dmIndi;
    void (*funcs[1])(PetscInt, PetscInt, PetscInt, const PetscInt[], const PetscInt[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscInt[], const PetscInt[], const PetscScalar[], const PetscScalar[], const PetscScalar[], PetscReal, const PetscReal[], PetscInt, const PetscScalar[], PetscScalar[]);

    PetscCall(VecGetDM(indicator, &dmIndi));
    funcs[0] = identity;
    PetscCall(DMProjectFieldLocal(dmIndi, 0.0, indicator, funcs, INSERT_VALUES, *metric));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricDeterminantCreate - Create the determinant field for a Riemannian metric

  Input parameters:
+ dm          - The `DM` of the metric field
- f           - The field number to use

  Output parameter:
+ determinant - The determinant field
- dmDet       - The corresponding `DM`

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricCreateUniform()`, `DMPlexMetricCreateIsotropic()`, `DMPlexMetricCreate()`
@*/
PetscErrorCode DMPlexMetricDeterminantCreate(DM dm, PetscInt f, Vec *determinant, DM *dmDet)
{
  PetscBool uniform;

  PetscFunctionBegin;
  PetscCall(DMPlexMetricIsUniform(dm, &uniform));
  PetscCall(DMClone(dm, dmDet));
  if (uniform) {
    MPI_Comm comm;

    PetscCall(PetscObjectGetComm((PetscObject)*dmDet, &comm));
    PetscCall(VecCreate(comm, determinant));
    PetscCall(VecSetSizes(*determinant, 1, PETSC_DECIDE));
    PetscCall(VecSetFromOptions(*determinant));
  } else PetscCall(DMPlexP1FieldCreate_Private(*dmDet, f, 1, determinant));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode LAPACKsyevFail(PetscInt dim, PetscScalar Mpos[])
{
  PetscInt i, j;

  PetscFunctionBegin;
  PetscCall(PetscPrintf(PETSC_COMM_SELF, "Failed to apply LAPACKsyev to the matrix\n"));
  for (i = 0; i < dim; ++i) {
    if (i == 0) PetscCall(PetscPrintf(PETSC_COMM_SELF, "    [["));
    else PetscCall(PetscPrintf(PETSC_COMM_SELF, "     ["));
    for (j = 0; j < dim; ++j) {
      if (j < dim - 1) PetscCall(PetscPrintf(PETSC_COMM_SELF, "%15.8e, ", (double)PetscAbsScalar(Mpos[i * dim + j])));
      else PetscCall(PetscPrintf(PETSC_COMM_SELF, "%15.8e", (double)PetscAbsScalar(Mpos[i * dim + j])));
    }
    if (i < dim - 1) PetscCall(PetscPrintf(PETSC_COMM_SELF, "]\n"));
    else PetscCall(PetscPrintf(PETSC_COMM_SELF, "]]\n"));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexMetricModify_Private(PetscInt dim, PetscReal h_min, PetscReal h_max, PetscReal a_max, PetscScalar Mp[], PetscScalar *detMp)
{
  PetscInt     i, j, k;
  PetscReal   *eigs, max_eig, l_min = 1.0 / (h_max * h_max), l_max = 1.0 / (h_min * h_min), la_min = 1.0 / (a_max * a_max);
  PetscScalar *Mpos;

  PetscFunctionBegin;
  PetscCall(PetscMalloc2(dim * dim, &Mpos, dim, &eigs));

  /* Symmetrize */
  for (i = 0; i < dim; ++i) {
    Mpos[i * dim + i] = Mp[i * dim + i];
    for (j = i + 1; j < dim; ++j) {
      Mpos[i * dim + j] = 0.5 * (Mp[i * dim + j] + Mp[j * dim + i]);
      Mpos[j * dim + i] = Mpos[i * dim + j];
    }
  }

  /* Compute eigendecomposition */
  if (dim == 1) {
    /* Isotropic case */
    eigs[0] = PetscRealPart(Mpos[0]);
    Mpos[0] = 1.0;
  } else {
    /* Anisotropic case */
    PetscScalar *work;
    PetscBLASInt lwork;

    lwork = 5 * dim;
    PetscCall(PetscMalloc1(5 * dim, &work));
    {
      PetscBLASInt lierr;
      PetscBLASInt nb;

      PetscCall(PetscBLASIntCast(dim, &nb));
      PetscCall(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
#if defined(PETSC_USE_COMPLEX)
      {
        PetscReal *rwork;
        PetscCall(PetscMalloc1(3 * dim, &rwork));
        PetscCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, Mpos, &nb, eigs, work, &lwork, rwork, &lierr));
        PetscCall(PetscFree(rwork));
      }
#else
      PetscCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, Mpos, &nb, eigs, work, &lwork, &lierr));
#endif
      if (lierr) {
        for (i = 0; i < dim; ++i) {
          Mpos[i * dim + i] = Mp[i * dim + i];
          for (j = i + 1; j < dim; ++j) {
            Mpos[i * dim + j] = 0.5 * (Mp[i * dim + j] + Mp[j * dim + i]);
            Mpos[j * dim + i] = Mpos[i * dim + j];
          }
        }
        PetscCall(LAPACKsyevFail(dim, Mpos));
        SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in LAPACK routine %d", (int)lierr);
      }
      PetscCall(PetscFPTrapPop());
    }
    PetscCall(PetscFree(work));
  }

  /* Reflect to positive orthant and enforce maximum and minimum size */
  max_eig = 0.0;
  for (i = 0; i < dim; ++i) {
    eigs[i] = PetscMin(l_max, PetscMax(l_min, PetscAbsReal(eigs[i])));
    max_eig = PetscMax(eigs[i], max_eig);
  }

  /* Enforce maximum anisotropy and compute determinant */
  *detMp = 1.0;
  for (i = 0; i < dim; ++i) {
    if (a_max > 1.0) eigs[i] = PetscMax(eigs[i], max_eig * la_min);
    *detMp *= eigs[i];
  }

  /* Reconstruct Hessian */
  for (i = 0; i < dim; ++i) {
    for (j = 0; j < dim; ++j) {
      Mp[i * dim + j] = 0.0;
      for (k = 0; k < dim; ++k) Mp[i * dim + j] += Mpos[k * dim + i] * eigs[k] * Mpos[k * dim + j];
    }
  }
  PetscCall(PetscFree2(Mpos, eigs));

  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricEnforceSPD - Enforce symmetric positive-definiteness of a metric

  Input parameters:
+ dm                 - The `DM`
. metricIn           - The metric
. restrictSizes      - Should maximum/minimum metric magnitudes be enforced?
- restrictAnisotropy - Should maximum anisotropy be enforced?

  Output parameter:
+ metricOut          - The metric
- determinant        - Its determinant

  Options Database Keys:
+ -dm_plex_metric_isotropic - Is the metric isotropic?
. -dm_plex_metric_uniform   - Is the metric uniform?
. -dm_plex_metric_h_min     - Minimum tolerated metric magnitude
. -dm_plex_metric_h_max     - Maximum tolerated metric magnitude
- -dm_plex_metric_a_max     - Maximum tolerated anisotropy

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricNormalize()`, `DMPlexMetricIntersection()`
@*/
PetscErrorCode DMPlexMetricEnforceSPD(DM dm, Vec metricIn, PetscBool restrictSizes, PetscBool restrictAnisotropy, Vec metricOut, Vec determinant)
{
  DM           dmDet;
  PetscBool    isotropic, uniform;
  PetscInt     dim, vStart, vEnd, v;
  PetscScalar *met, *det;
  PetscReal    h_min = 1.0e-30, h_max = 1.0e+30, a_max = 0.0;

  PetscFunctionBegin;
  PetscCall(PetscLogEventBegin(DMPLEX_MetricEnforceSPD, 0, 0, 0, 0));

  /* Extract metadata from dm */
  PetscCall(DMGetDimension(dm, &dim));
  if (restrictSizes) {
    PetscCall(DMPlexMetricGetMinimumMagnitude(dm, &h_min));
    PetscCall(DMPlexMetricGetMaximumMagnitude(dm, &h_max));
    h_min = PetscMax(h_min, 1.0e-30);
    h_max = PetscMin(h_max, 1.0e+30);
    PetscCheck(h_min < h_max, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Minimum metric magnitude should be smaller than maximum metric magnitude");
  }
  if (restrictAnisotropy) {
    PetscCall(DMPlexMetricGetMaximumAnisotropy(dm, &a_max));
    a_max = PetscMin(a_max, 1.0e+30);
  }

  /* Setup output metric */
  PetscCall(VecCopy(metricIn, metricOut));

  /* Enforce SPD and extract determinant */
  PetscCall(VecGetArray(metricOut, &met));
  PetscCall(DMPlexMetricIsUniform(dm, &uniform));
  PetscCall(DMPlexMetricIsIsotropic(dm, &isotropic));
  if (uniform) {
    PetscCheck(isotropic, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Uniform anisotropic metrics cannot exist");

    /* Uniform case */
    PetscCall(VecGetArray(determinant, &det));
    PetscCall(DMPlexMetricModify_Private(1, h_min, h_max, a_max, met, det));
    PetscCall(VecRestoreArray(determinant, &det));
  } else {
    /* Spatially varying case */
    PetscInt nrow;

    if (isotropic) nrow = 1;
    else nrow = dim;
    PetscCall(VecGetDM(determinant, &dmDet));
    PetscCall(DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd));
    PetscCall(VecGetArray(determinant, &det));
    for (v = vStart; v < vEnd; ++v) {
      PetscScalar *vmet, *vdet;
      PetscCall(DMPlexPointLocalRef(dm, v, met, &vmet));
      PetscCall(DMPlexPointLocalRef(dmDet, v, det, &vdet));
      PetscCall(DMPlexMetricModify_Private(nrow, h_min, h_max, a_max, vmet, vdet));
    }
    PetscCall(VecRestoreArray(determinant, &det));
  }
  PetscCall(VecRestoreArray(metricOut, &met));

  PetscCall(PetscLogEventEnd(DMPLEX_MetricEnforceSPD, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static void detMFunc(PetscInt dim, PetscInt Nf, PetscInt NfAux, const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscScalar p = constants[0];

  f0[0] = PetscPowScalar(u[0], p / (2.0 * p + dim));
}

/*@
  DMPlexMetricNormalize - Apply L-p normalization to a metric

  Input parameters:
+ dm                 - The `DM`
. metricIn           - The unnormalized metric
. restrictSizes      - Should maximum/minimum metric magnitudes be enforced?
- restrictAnisotropy - Should maximum metric anisotropy be enforced?

  Output parameters:
+ metricOut          - The normalized metric
- determinant - computed determinant

  Options Database Keys:
+ -dm_plex_metric_isotropic                 - Is the metric isotropic?
. -dm_plex_metric_uniform                   - Is the metric uniform?
. -dm_plex_metric_restrict_anisotropy_first - Should anisotropy be restricted before normalization?
. -dm_plex_metric_h_min                     - Minimum tolerated metric magnitude
. -dm_plex_metric_h_max                     - Maximum tolerated metric magnitude
. -dm_plex_metric_a_max                     - Maximum tolerated anisotropy
. -dm_plex_metric_p                         - L-p normalization order
- -dm_plex_metric_target_complexity         - Target metric complexity

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricEnforceSPD()`, `DMPlexMetricIntersection()`
@*/
PetscErrorCode DMPlexMetricNormalize(DM dm, Vec metricIn, PetscBool restrictSizes, PetscBool restrictAnisotropy, Vec metricOut, Vec determinant)
{
  DM           dmDet;
  MPI_Comm     comm;
  PetscBool    restrictAnisotropyFirst, isotropic, uniform;
  PetscDS      ds;
  PetscInt     dim, Nd, vStart, vEnd, v, i;
  PetscScalar *met, *det, integral, constants[1];
  PetscReal    p, h_min = 1.0e-30, h_max = 1.0e+30, a_max = 0.0, factGlob, fact, target, realIntegral;

  PetscFunctionBegin;
  PetscCall(PetscLogEventBegin(DMPLEX_MetricNormalize, 0, 0, 0, 0));

  /* Extract metadata from dm */
  PetscCall(PetscObjectGetComm((PetscObject)dm, &comm));
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMPlexMetricIsUniform(dm, &uniform));
  PetscCall(DMPlexMetricIsIsotropic(dm, &isotropic));
  if (isotropic) Nd = 1;
  else Nd = dim * dim;

  /* Set up metric and ensure it is SPD */
  PetscCall(DMPlexMetricRestrictAnisotropyFirst(dm, &restrictAnisotropyFirst));
  PetscCall(DMPlexMetricEnforceSPD(dm, metricIn, PETSC_FALSE, (PetscBool)(restrictAnisotropy && restrictAnisotropyFirst), metricOut, determinant));

  /* Compute global normalization factor */
  PetscCall(DMPlexMetricGetTargetComplexity(dm, &target));
  PetscCall(DMPlexMetricGetNormalizationOrder(dm, &p));
  constants[0] = p;
  if (uniform) {
    PetscCheck(isotropic, comm, PETSC_ERR_SUP, "Uniform anisotropic metrics not supported");
    DM  dmTmp;
    Vec tmp;

    PetscCall(DMClone(dm, &dmTmp));
    PetscCall(DMPlexP1FieldCreate_Private(dmTmp, 0, 1, &tmp));
    PetscCall(VecGetArray(determinant, &det));
    PetscCall(VecSet(tmp, det[0]));
    PetscCall(VecRestoreArray(determinant, &det));
    PetscCall(DMGetDS(dmTmp, &ds));
    PetscCall(PetscDSSetConstants(ds, 1, constants));
    PetscCall(PetscDSSetObjective(ds, 0, detMFunc));
    PetscCall(DMPlexComputeIntegralFEM(dmTmp, tmp, &integral, NULL));
    PetscCall(VecDestroy(&tmp));
    PetscCall(DMDestroy(&dmTmp));
  } else {
    PetscCall(VecGetDM(determinant, &dmDet));
    PetscCall(DMGetDS(dmDet, &ds));
    PetscCall(PetscDSSetConstants(ds, 1, constants));
    PetscCall(PetscDSSetObjective(ds, 0, detMFunc));
    PetscCall(DMPlexComputeIntegralFEM(dmDet, determinant, &integral, NULL));
  }
  realIntegral = PetscRealPart(integral);
  PetscCheck(realIntegral > 1.0e-30, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Global metric normalization factor must be in (0, inf). Is the input metric positive-definite?");
  factGlob = PetscPowReal(target / realIntegral, 2.0 / dim);

  /* Apply local scaling */
  if (restrictSizes) {
    PetscCall(DMPlexMetricGetMinimumMagnitude(dm, &h_min));
    PetscCall(DMPlexMetricGetMaximumMagnitude(dm, &h_max));
    h_min = PetscMax(h_min, 1.0e-30);
    h_max = PetscMin(h_max, 1.0e+30);
    PetscCheck(h_min < h_max, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Minimum metric magnitude should be smaller than maximum metric magnitude");
  }
  if (restrictAnisotropy && !restrictAnisotropyFirst) {
    PetscCall(DMPlexMetricGetMaximumAnisotropy(dm, &a_max));
    a_max = PetscMin(a_max, 1.0e+30);
  }
  PetscCall(VecGetArray(metricOut, &met));
  PetscCall(VecGetArray(determinant, &det));
  if (uniform) {
    /* Uniform case */
    met[0] *= factGlob * PetscPowReal(PetscAbsScalar(det[0]), -1.0 / (2 * p + dim));
    if (restrictSizes) PetscCall(DMPlexMetricModify_Private(1, h_min, h_max, a_max, met, det));
  } else {
    /* Spatially varying case */
    PetscInt nrow;

    if (isotropic) nrow = 1;
    else nrow = dim;
    PetscCall(DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd));
    PetscCall(VecGetDM(determinant, &dmDet));
    for (v = vStart; v < vEnd; ++v) {
      PetscScalar *Mp, *detM;

      PetscCall(DMPlexPointLocalRef(dm, v, met, &Mp));
      PetscCall(DMPlexPointLocalRef(dmDet, v, det, &detM));
      fact = factGlob * PetscPowReal(PetscAbsScalar(detM[0]), -1.0 / (2 * p + dim));
      for (i = 0; i < Nd; ++i) Mp[i] *= fact;
      if (restrictSizes) PetscCall(DMPlexMetricModify_Private(nrow, h_min, h_max, a_max, Mp, detM));
    }
  }
  PetscCall(VecRestoreArray(determinant, &det));
  PetscCall(VecRestoreArray(metricOut, &met));

  PetscCall(PetscLogEventEnd(DMPLEX_MetricNormalize, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricAverage - Compute the average of a list of metrics

  Input Parameters:
+ dm         - The `DM`
. numMetrics - The number of metrics to be averaged
. weights    - Weights for the average
- metrics    - The metrics to be averaged

  Output Parameter:
. metricAvg  - The averaged metric

  Level: beginner

  Notes:
  The weights should sum to unity.

  If weights are not provided then an unweighted average is used.

.seealso: `DMPLEX`, `DMPlexMetricAverage2()`, `DMPlexMetricAverage3()`, `DMPlexMetricIntersection()`
@*/
PetscErrorCode DMPlexMetricAverage(DM dm, PetscInt numMetrics, PetscReal weights[], Vec metrics[], Vec metricAvg)
{
  PetscBool haveWeights = PETSC_TRUE;
  PetscInt  i, m, n;
  PetscReal sum = 0.0, tol = 1.0e-10;

  PetscFunctionBegin;
  PetscCall(PetscLogEventBegin(DMPLEX_MetricAverage, 0, 0, 0, 0));
  PetscCheck(numMetrics >= 1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Cannot average %" PetscInt_FMT " < 1 metrics", numMetrics);
  PetscCall(VecSet(metricAvg, 0.0));
  PetscCall(VecGetSize(metricAvg, &m));
  for (i = 0; i < numMetrics; ++i) {
    PetscCall(VecGetSize(metrics[i], &n));
    PetscCheck(m == n, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Averaging different metric types not implemented");
  }

  /* Default to the unweighted case */
  if (!weights) {
    PetscCall(PetscMalloc1(numMetrics, &weights));
    haveWeights = PETSC_FALSE;
    for (i = 0; i < numMetrics; ++i) weights[i] = 1.0 / numMetrics;
  }

  /* Check weights sum to unity */
  for (i = 0; i < numMetrics; ++i) sum += weights[i];
  PetscCheck(PetscAbsReal(sum - 1) <= tol, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Weights do not sum to unity");

  /* Compute metric average */
  for (i = 0; i < numMetrics; ++i) PetscCall(VecAXPY(metricAvg, weights[i], metrics[i]));
  if (!haveWeights) PetscCall(PetscFree(weights));

  PetscCall(PetscLogEventEnd(DMPLEX_MetricAverage, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricAverage2 - Compute the unweighted average of two metrics

  Input Parameters:
+ dm         - The `DM`
. metric1    - The first metric to be averaged
- metric2    - The second metric to be averaged

  Output Parameter:
. metricAvg  - The averaged metric

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricAverage()`, `DMPlexMetricAverage3()`
@*/
PetscErrorCode DMPlexMetricAverage2(DM dm, Vec metric1, Vec metric2, Vec metricAvg)
{
  PetscReal weights[2] = {0.5, 0.5};
  Vec       metrics[2] = {metric1, metric2};

  PetscFunctionBegin;
  PetscCall(DMPlexMetricAverage(dm, 2, weights, metrics, metricAvg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricAverage3 - Compute the unweighted average of three metrics

  Input Parameters:
+ dm         - The `DM`
. metric1    - The first metric to be averaged
. metric2    - The second metric to be averaged
- metric3    - The third metric to be averaged

  Output Parameter:
. metricAvg  - The averaged metric

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricAverage()`, `DMPlexMetricAverage2()`
@*/
PetscErrorCode DMPlexMetricAverage3(DM dm, Vec metric1, Vec metric2, Vec metric3, Vec metricAvg)
{
  PetscReal weights[3] = {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0};
  Vec       metrics[3] = {metric1, metric2, metric3};

  PetscFunctionBegin;
  PetscCall(DMPlexMetricAverage(dm, 3, weights, metrics, metricAvg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexMetricIntersection_Private(PetscInt dim, PetscScalar M1[], PetscScalar M2[])
{
  PetscInt     i, j, k, l, m;
  PetscReal   *evals;
  PetscScalar *evecs, *sqrtM1, *isqrtM1;

  PetscFunctionBegin;

  /* Isotropic case */
  if (dim == 1) {
    M2[0] = (PetscScalar)PetscMax(PetscRealPart(M1[0]), PetscRealPart(M2[0]));
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  /* Anisotropic case */
  PetscCall(PetscMalloc4(dim * dim, &evecs, dim * dim, &sqrtM1, dim * dim, &isqrtM1, dim, &evals));
  for (i = 0; i < dim; ++i) {
    for (j = 0; j < dim; ++j) evecs[i * dim + j] = M1[i * dim + j];
  }
  {
    PetscScalar *work;
    PetscBLASInt lwork;

    lwork = 5 * dim;
    PetscCall(PetscMalloc1(5 * dim, &work));
    {
      PetscBLASInt lierr, nb;
      PetscReal    sqrtj;

      /* Compute eigendecomposition of M1 */
      PetscCall(PetscBLASIntCast(dim, &nb));
      PetscCall(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
#if defined(PETSC_USE_COMPLEX)
      {
        PetscReal *rwork;
        PetscCall(PetscMalloc1(3 * dim, &rwork));
        PetscCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, evecs, &nb, evals, work, &lwork, rwork, &lierr));
        PetscCall(PetscFree(rwork));
      }
#else
      PetscCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, evecs, &nb, evals, work, &lwork, &lierr));
#endif
      if (lierr) {
        PetscCall(LAPACKsyevFail(dim, M1));
        SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in LAPACK routine %d", (int)lierr);
      }
      PetscCall(PetscFPTrapPop());

      /* Compute square root and the reciprocal thereof */
      for (i = 0; i < dim; ++i) {
        for (k = 0; k < dim; ++k) {
          sqrtM1[i * dim + k]  = 0.0;
          isqrtM1[i * dim + k] = 0.0;
          for (j = 0; j < dim; ++j) {
            sqrtj = PetscSqrtReal(evals[j]);
            sqrtM1[i * dim + k] += evecs[j * dim + i] * sqrtj * evecs[j * dim + k];
            isqrtM1[i * dim + k] += evecs[j * dim + i] * (1.0 / sqrtj) * evecs[j * dim + k];
          }
        }
      }

      /* Map M2 into the space spanned by the eigenvectors of M1 */
      for (i = 0; i < dim; ++i) {
        for (l = 0; l < dim; ++l) {
          evecs[i * dim + l] = 0.0;
          for (j = 0; j < dim; ++j) {
            for (k = 0; k < dim; ++k) evecs[i * dim + l] += isqrtM1[j * dim + i] * M2[j * dim + k] * isqrtM1[k * dim + l];
          }
        }
      }

      /* Compute eigendecomposition */
      PetscCall(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
#if defined(PETSC_USE_COMPLEX)
      {
        PetscReal *rwork;
        PetscCall(PetscMalloc1(3 * dim, &rwork));
        PetscCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, evecs, &nb, evals, work, &lwork, rwork, &lierr));
        PetscCall(PetscFree(rwork));
      }
#else
      PetscCallBLAS("LAPACKsyev", LAPACKsyev_("V", "U", &nb, evecs, &nb, evals, work, &lwork, &lierr));
#endif
      if (lierr) {
        for (i = 0; i < dim; ++i) {
          for (l = 0; l < dim; ++l) {
            evecs[i * dim + l] = 0.0;
            for (j = 0; j < dim; ++j) {
              for (k = 0; k < dim; ++k) evecs[i * dim + l] += isqrtM1[j * dim + i] * M2[j * dim + k] * isqrtM1[k * dim + l];
            }
          }
        }
        PetscCall(LAPACKsyevFail(dim, evecs));
        SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in LAPACK routine %d", (int)lierr);
      }
      PetscCall(PetscFPTrapPop());

      /* Modify eigenvalues */
      for (i = 0; i < dim; ++i) evals[i] = PetscMax(evals[i], 1.0);

      /* Map back to get the intersection */
      for (i = 0; i < dim; ++i) {
        for (m = 0; m < dim; ++m) {
          M2[i * dim + m] = 0.0;
          for (j = 0; j < dim; ++j) {
            for (k = 0; k < dim; ++k) {
              for (l = 0; l < dim; ++l) M2[i * dim + m] += sqrtM1[j * dim + i] * evecs[j * dim + k] * evals[k] * evecs[l * dim + k] * sqrtM1[l * dim + m];
            }
          }
        }
      }
    }
    PetscCall(PetscFree(work));
  }
  PetscCall(PetscFree4(evecs, sqrtM1, isqrtM1, evals));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricIntersection - Compute the intersection of a list of metrics

  Input Parameters:
+ dm         - The `DM`
. numMetrics - The number of metrics to be intersected
- metrics    - The metrics to be intersected

  Output Parameter:
. metricInt  - The intersected metric

  Level: beginner

  Notes:
  The intersection of a list of metrics has the minimal ellipsoid which fits within the ellipsoids of the component metrics.

  The implementation used here is only consistent with the minimal ellipsoid definition in the case numMetrics = 2.

.seealso: `DMPLEX`, `DMPlexMetricIntersection2()`, `DMPlexMetricIntersection3()`, `DMPlexMetricAverage()`
@*/
PetscErrorCode DMPlexMetricIntersection(DM dm, PetscInt numMetrics, Vec metrics[], Vec metricInt)
{
  PetscBool    isotropic, uniform;
  PetscInt     v, i, m, n;
  PetscScalar *met, *meti;

  PetscFunctionBegin;
  PetscCall(PetscLogEventBegin(DMPLEX_MetricIntersection, 0, 0, 0, 0));
  PetscCheck(numMetrics >= 1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Cannot intersect %" PetscInt_FMT " < 1 metrics", numMetrics);

  /* Copy over the first metric */
  PetscCall(VecCopy(metrics[0], metricInt));
  if (numMetrics == 1) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(VecGetSize(metricInt, &m));
  for (i = 0; i < numMetrics; ++i) {
    PetscCall(VecGetSize(metrics[i], &n));
    PetscCheck(m == n, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Intersecting different metric types not implemented");
  }

  /* Intersect subsequent metrics in turn */
  PetscCall(DMPlexMetricIsUniform(dm, &uniform));
  PetscCall(DMPlexMetricIsIsotropic(dm, &isotropic));
  if (uniform) {
    /* Uniform case */
    PetscCall(VecGetArray(metricInt, &met));
    for (i = 1; i < numMetrics; ++i) {
      PetscCall(VecGetArray(metrics[i], &meti));
      PetscCall(DMPlexMetricIntersection_Private(1, meti, met));
      PetscCall(VecRestoreArray(metrics[i], &meti));
    }
    PetscCall(VecRestoreArray(metricInt, &met));
  } else {
    /* Spatially varying case */
    PetscInt     dim, vStart, vEnd, nrow;
    PetscScalar *M, *Mi;

    PetscCall(DMGetDimension(dm, &dim));
    if (isotropic) nrow = 1;
    else nrow = dim;
    PetscCall(DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd));
    PetscCall(VecGetArray(metricInt, &met));
    for (i = 1; i < numMetrics; ++i) {
      PetscCall(VecGetArray(metrics[i], &meti));
      for (v = vStart; v < vEnd; ++v) {
        PetscCall(DMPlexPointLocalRef(dm, v, met, &M));
        PetscCall(DMPlexPointLocalRef(dm, v, meti, &Mi));
        PetscCall(DMPlexMetricIntersection_Private(nrow, Mi, M));
      }
      PetscCall(VecRestoreArray(metrics[i], &meti));
    }
    PetscCall(VecRestoreArray(metricInt, &met));
  }

  PetscCall(PetscLogEventEnd(DMPLEX_MetricIntersection, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricIntersection2 - Compute the intersection of two metrics

  Input Parameters:
+ dm        - The `DM`
. metric1   - The first metric to be intersected
- metric2   - The second metric to be intersected

  Output Parameter:
. metricInt - The intersected metric

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricIntersection()`, `DMPlexMetricIntersection3()`
@*/
PetscErrorCode DMPlexMetricIntersection2(DM dm, Vec metric1, Vec metric2, Vec metricInt)
{
  Vec metrics[2] = {metric1, metric2};

  PetscFunctionBegin;
  PetscCall(DMPlexMetricIntersection(dm, 2, metrics, metricInt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexMetricIntersection3 - Compute the intersection of three metrics

  Input Parameters:
+ dm        - The `DM`
. metric1   - The first metric to be intersected
. metric2   - The second metric to be intersected
- metric3   - The third metric to be intersected

  Output Parameter:
. metricInt - The intersected metric

  Level: beginner

.seealso: `DMPLEX`, `DMPlexMetricIntersection()`, `DMPlexMetricIntersection2()`
@*/
PetscErrorCode DMPlexMetricIntersection3(DM dm, Vec metric1, Vec metric2, Vec metric3, Vec metricInt)
{
  Vec metrics[3] = {metric1, metric2, metric3};

  PetscFunctionBegin;
  PetscCall(DMPlexMetricIntersection(dm, 3, metrics, metricInt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

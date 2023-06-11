#include <petsc/private/dmforestimpl.h> /*I "petscdmforest.h" I*/
#include <petsc/private/dmimpl.h>       /*I "petscdm.h" I*/
#include <petsc/private/dmlabelimpl.h>  /*I "petscdmlabel.h" I*/
#include <petscsf.h>

PetscBool DMForestPackageInitialized = PETSC_FALSE;

typedef struct _DMForestTypeLink *DMForestTypeLink;

struct _DMForestTypeLink {
  char            *name;
  DMForestTypeLink next;
};

DMForestTypeLink DMForestTypeList;

static PetscErrorCode DMForestPackageFinalize(void)
{
  DMForestTypeLink oldLink, link = DMForestTypeList;

  PetscFunctionBegin;
  while (link) {
    oldLink = link;
    PetscCall(PetscFree(oldLink->name));
    link = oldLink->next;
    PetscCall(PetscFree(oldLink));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMForestPackageInitialize(void)
{
  PetscFunctionBegin;
  if (DMForestPackageInitialized) PetscFunctionReturn(PETSC_SUCCESS);
  DMForestPackageInitialized = PETSC_TRUE;

  PetscCall(DMForestRegisterType(DMFOREST));
  PetscCall(PetscRegisterFinalize(DMForestPackageFinalize));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMForestRegisterType - Registers a `DMType` as a subtype of `DMFOREST` (so that `DMIsForest()` will be correct)

  Not Collective

  Input parameter:
. name - the name of the type

  Level: advanced

.seealso: `DMFOREST`, `DMIsForest()`
@*/
PetscErrorCode DMForestRegisterType(DMType name)
{
  DMForestTypeLink link;

  PetscFunctionBegin;
  PetscCall(DMForestPackageInitialize());
  PetscCall(PetscNew(&link));
  PetscCall(PetscStrallocpy(name, &link->name));
  link->next       = DMForestTypeList;
  DMForestTypeList = link;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMIsForest - Check whether a DM uses the DMFOREST interface for hierarchically-refined meshes

  Not Collective

  Input parameter:
. dm - the DM object

  Output parameter:
. isForest - whether dm is a subtype of DMFOREST

  Level: intermediate

.seealso: `DMFOREST`, `DMForestRegisterType()`
@*/
PetscErrorCode DMIsForest(DM dm, PetscBool *isForest)
{
  DMForestTypeLink link = DMForestTypeList;

  PetscFunctionBegin;
  while (link) {
    PetscBool sameType;
    PetscCall(PetscObjectTypeCompare((PetscObject)dm, link->name, &sameType));
    if (sameType) {
      *isForest = PETSC_TRUE;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
    link = link->next;
  }
  *isForest = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestTemplate - Create a new `DM` that will be adapted from a source `DM`.  The new `DM` reproduces the configuration
  of the source, but is not yet setup, so that the user can then define only the ways that the new `DM` should differ
  (by, e.g., refinement or repartitioning).  The source `DM` is also set as the adaptivity source `DM` of the new `DM` (see
  `DMForestSetAdaptivityForest()`).

  Collective

  Input Parameters:
+ dm - the source `DM` object
- comm - the communicator for the new `DM` (this communicator is currently ignored, but is present so that `DMForestTemplate()` can be used within `DMCoarsen()`)

  Output Parameter:
. tdm - the new `DM` object

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetAdaptivityForest()`
@*/
PetscErrorCode DMForestTemplate(DM dm, MPI_Comm comm, DM *tdm)
{
  DM_Forest                 *forest = (DM_Forest *)dm->data;
  DMType                     type;
  DM                         base;
  DMForestTopology           topology;
  MatType                    mtype;
  PetscInt                   dim, overlap, ref, factor;
  DMForestAdaptivityStrategy strat;
  void                      *ctx;
  PetscErrorCode (*map)(DM, PetscInt, PetscInt, const PetscReal[], PetscReal[], void *);
  void *mapCtx;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMCreate(PetscObjectComm((PetscObject)dm), tdm));
  PetscCall(DMGetType(dm, &type));
  PetscCall(DMSetType(*tdm, type));
  PetscCall(DMForestGetBaseDM(dm, &base));
  PetscCall(DMForestSetBaseDM(*tdm, base));
  PetscCall(DMForestGetTopology(dm, &topology));
  PetscCall(DMForestSetTopology(*tdm, topology));
  PetscCall(DMForestGetAdjacencyDimension(dm, &dim));
  PetscCall(DMForestSetAdjacencyDimension(*tdm, dim));
  PetscCall(DMForestGetPartitionOverlap(dm, &overlap));
  PetscCall(DMForestSetPartitionOverlap(*tdm, overlap));
  PetscCall(DMForestGetMinimumRefinement(dm, &ref));
  PetscCall(DMForestSetMinimumRefinement(*tdm, ref));
  PetscCall(DMForestGetMaximumRefinement(dm, &ref));
  PetscCall(DMForestSetMaximumRefinement(*tdm, ref));
  PetscCall(DMForestGetAdaptivityStrategy(dm, &strat));
  PetscCall(DMForestSetAdaptivityStrategy(*tdm, strat));
  PetscCall(DMForestGetGradeFactor(dm, &factor));
  PetscCall(DMForestSetGradeFactor(*tdm, factor));
  PetscCall(DMForestGetBaseCoordinateMapping(dm, &map, &mapCtx));
  PetscCall(DMForestSetBaseCoordinateMapping(*tdm, map, mapCtx));
  if (forest->ftemplate) PetscCall((*forest->ftemplate)(dm, *tdm));
  PetscCall(DMForestSetAdaptivityForest(*tdm, dm));
  PetscCall(DMCopyDisc(dm, *tdm));
  PetscCall(DMGetApplicationContext(dm, &ctx));
  PetscCall(DMSetApplicationContext(*tdm, &ctx));
  {
    const PetscReal *maxCell, *L, *Lstart;

    PetscCall(DMGetPeriodicity(dm, &maxCell, &Lstart, &L));
    PetscCall(DMSetPeriodicity(*tdm, maxCell, Lstart, L));
  }
  PetscCall(DMGetMatType(dm, &mtype));
  PetscCall(DMSetMatType(*tdm, mtype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMInitialize_Forest(DM dm);

PETSC_EXTERN PetscErrorCode DMClone_Forest(DM dm, DM *newdm)
{
  DM_Forest  *forest = (DM_Forest *)dm->data;
  const char *type;

  PetscFunctionBegin;
  forest->refct++;
  (*newdm)->data = forest;
  PetscCall(PetscObjectGetType((PetscObject)dm, &type));
  PetscCall(PetscObjectChangeTypeName((PetscObject)*newdm, type));
  PetscCall(DMInitialize_Forest(*newdm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMDestroy_Forest(DM dm)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  if (--forest->refct > 0) PetscFunctionReturn(PETSC_SUCCESS);
  if (forest->destroy) PetscCall((*forest->destroy)(dm));
  PetscCall(PetscSFDestroy(&forest->cellSF));
  PetscCall(PetscSFDestroy(&forest->preCoarseToFine));
  PetscCall(PetscSFDestroy(&forest->coarseToPreFine));
  PetscCall(DMLabelDestroy(&forest->adaptLabel));
  PetscCall(PetscFree(forest->adaptStrategy));
  PetscCall(DMDestroy(&forest->base));
  PetscCall(DMDestroy(&forest->adapt));
  PetscCall(PetscFree(forest->topology));
  PetscCall(PetscFree(forest));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMForestSetTopology - Set the topology of a `DMFOREST` during the pre-setup phase.  The topology is a string (e.g.
  "cube", "shell") and can be interpreted by subtypes of `DMFOREST`) to construct the base DM of a forest during
  `DMSetUp()`.

  Logically collectiv

  Input parameters:
+ dm - the forest
- topology - the topology of the forest

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetTopology()`, `DMForestSetBaseDM()`
@*/
PetscErrorCode DMForestSetTopology(DM dm, DMForestTopology topology)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the topology after setup");
  PetscCall(PetscFree(forest->topology));
  PetscCall(PetscStrallocpy((const char *)topology, (char **)&forest->topology));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMForestGetTopology - Get a string describing the topology of a `DMFOREST`.

  Not Collective

  Input parameter:
. dm - the forest

  Output parameter:
. topology - the topology of the forest (e.g., 'cube', 'shell')

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetTopology()`
@*/
PetscErrorCode DMForestGetTopology(DM dm, DMForestTopology *topology)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(topology, 2);
  *topology = forest->topology;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetBaseDM - During the pre-setup phase, set the `DM` that defines the base mesh of a `DMFOREST` forest.  The
  forest will be hierarchically refined from the base, and all refinements/coarsenings of the forest will share its
  base.  In general, two forest must share a base to be comparable, to do things like construct interpolators.

  Logically Collective

  Input Parameters:
+ dm - the forest
- base - the base `DM` of the forest

  Level: intermediate

  Note:
    Currently the base `DM` must be a `DMPLEX`

.seealso: `DM`, `DMFOREST`, `DMForestGetBaseDM()`
@*/
PetscErrorCode DMForestSetBaseDM(DM dm, DM base)
{
  DM_Forest *forest = (DM_Forest *)dm->data;
  PetscInt   dim, dimEmbed;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the base after setup");
  PetscCall(PetscObjectReference((PetscObject)base));
  PetscCall(DMDestroy(&forest->base));
  forest->base = base;
  if (base) {
    const PetscReal *maxCell, *Lstart, *L;

    PetscValidHeaderSpecific(base, DM_CLASSID, 2);
    PetscCall(DMGetDimension(base, &dim));
    PetscCall(DMSetDimension(dm, dim));
    PetscCall(DMGetCoordinateDim(base, &dimEmbed));
    PetscCall(DMSetCoordinateDim(dm, dimEmbed));
    PetscCall(DMGetPeriodicity(base, &maxCell, &Lstart, &L));
    PetscCall(DMSetPeriodicity(dm, maxCell, Lstart, L));
  } else PetscCall(DMSetPeriodicity(dm, NULL, NULL, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetBaseDM - Get the base DM of a DMForest forest.  The forest will be hierarchically refined from the base,
  and all refinements/coarsenings of the forest will share its base.  In general, two forest must share a base to be
  comparable, to do things like construct interpolators.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. base - the base DM of the forest

  Notes:
    After DMSetUp(), the base DM will be redundantly distributed across MPI processes

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetBaseDM()`
@*/
PetscErrorCode DMForestGetBaseDM(DM dm, DM *base)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(base, 2);
  *base = forest->base;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMForestSetBaseCoordinateMapping(DM dm, PetscErrorCode (*func)(DM, PetscInt, PetscInt, const PetscReal[], PetscReal[], void *), void *ctx)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  forest->mapcoordinates    = func;
  forest->mapcoordinatesctx = ctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMForestGetBaseCoordinateMapping(DM dm, PetscErrorCode (**func)(DM, PetscInt, PetscInt, const PetscReal[], PetscReal[], void *), void *ctx)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  if (func) *func = forest->mapcoordinates;
  if (ctx) *((void **)ctx) = forest->mapcoordinatesctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetAdaptivityForest - During the pre-setup phase, set the forest from which the current forest will be
  adapted (e.g., the current forest will be refined/coarsened/repartitioned from it) in `DMSetUp()`.  Usually not needed
  by users directly: `DMForestTemplate()` constructs a new forest to be adapted from an old forest and calls this
  routine.

  Logically Collective

  Input Parameters:
+ dm - the new forest, which will be constructed from adapt
- adapt - the old forest

  Level: intermediate

  Note:
  This can be called after setup with `adapt` = `NULL`, which will clear all internal data related to the
  adaptivity forest from `dm`.  This way, repeatedly adapting does not leave stale `DM` objects in memory.

.seealso: `DM`, `DMFOREST`, `DMForestGetAdaptivityForest()`, `DMForestSetAdaptivityPurpose()`
@*/
PetscErrorCode DMForestSetAdaptivityForest(DM dm, DM adapt)
{
  DM_Forest *forest, *adaptForest, *oldAdaptForest;
  DM         oldAdapt;
  PetscBool  isForest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  if (adapt) PetscValidHeaderSpecific(adapt, DM_CLASSID, 2);
  PetscCall(DMIsForest(dm, &isForest));
  if (!isForest) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCheck(adapt == NULL || !dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the adaptation forest after setup");
  forest = (DM_Forest *)dm->data;
  PetscCall(DMForestGetAdaptivityForest(dm, &oldAdapt));
  adaptForest    = (DM_Forest *)(adapt ? adapt->data : NULL);
  oldAdaptForest = (DM_Forest *)(oldAdapt ? oldAdapt->data : NULL);
  if (adaptForest != oldAdaptForest) {
    PetscCall(PetscSFDestroy(&forest->preCoarseToFine));
    PetscCall(PetscSFDestroy(&forest->coarseToPreFine));
    if (forest->clearadaptivityforest) PetscCall((*forest->clearadaptivityforest)(dm));
  }
  switch (forest->adaptPurpose) {
  case DM_ADAPT_DETERMINE:
    PetscCall(PetscObjectReference((PetscObject)adapt));
    PetscCall(DMDestroy(&(forest->adapt)));
    forest->adapt = adapt;
    break;
  case DM_ADAPT_REFINE:
    PetscCall(DMSetCoarseDM(dm, adapt));
    break;
  case DM_ADAPT_COARSEN:
  case DM_ADAPT_COARSEN_LAST:
    PetscCall(DMSetFineDM(dm, adapt));
    break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "invalid adaptivity purpose");
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetAdaptivityForest - Get the forest from which the current forest is adapted.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. adapt - the forest from which `dm` is/was adapted

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetAdaptivityForest()`, `DMForestSetAdaptivityPurpose()`
@*/
PetscErrorCode DMForestGetAdaptivityForest(DM dm, DM *adapt)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  forest = (DM_Forest *)dm->data;
  switch (forest->adaptPurpose) {
  case DM_ADAPT_DETERMINE:
    *adapt = forest->adapt;
    break;
  case DM_ADAPT_REFINE:
    PetscCall(DMGetCoarseDM(dm, adapt));
    break;
  case DM_ADAPT_COARSEN:
  case DM_ADAPT_COARSEN_LAST:
    PetscCall(DMGetFineDM(dm, adapt));
    break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "invalid adaptivity purpose");
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetAdaptivityPurpose - During the pre-setup phase, set whether the current `DM` is being adapted from its
  source (set with `DMForestSetAdaptivityForest()`) for the purpose of refinement (`DM_ADAPT_REFINE`), coarsening
  (`DM_ADAPT_COARSEN`), or undefined (`DM_ADAPT_DETERMINE`).  This only matters for the purposes of reference counting:
  during `DMDestroy()`, cyclic references can be found between `DM`s only if the cyclic reference is due to a fine/coarse
  relationship (see `DMSetFineDM()`/`DMSetCoarseDM()`).  If the purpose is not refinement or coarsening, and the user does
  not maintain a reference to the post-adaptation forest (i.e., the one created by `DMForestTemplate()`), then this can
  cause a memory leak.  This method is used by subtypes of `DMFOREST` when automatically constructing mesh hierarchies.

  Logically Collective

  Input Parameters:
+ dm - the forest
- purpose - the adaptivity purpose

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestTemplate()`, `DMForestSetAdaptivityForest()`, `DMForestGetAdaptivityForest()`, `DMAdaptFlag`
@*/
PetscErrorCode DMForestSetAdaptivityPurpose(DM dm, DMAdaptFlag purpose)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  forest = (DM_Forest *)dm->data;
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the adaptation forest after setup");
  if (purpose != forest->adaptPurpose) {
    DM adapt;

    PetscCall(DMForestGetAdaptivityForest(dm, &adapt));
    PetscCall(PetscObjectReference((PetscObject)adapt));
    PetscCall(DMForestSetAdaptivityForest(dm, NULL));

    forest->adaptPurpose = purpose;

    PetscCall(DMForestSetAdaptivityForest(dm, adapt));
    PetscCall(DMDestroy(&adapt));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetAdaptivityPurpose - Get whether the current `DM` is being adapted from its source (set with
  `DMForestSetAdaptivityForest()`) for the purpose of refinement (`DM_ADAPT_REFINE`), coarsening (`DM_ADAPT_COARSEN`),
  coarsening only the last level (`DM_ADAPT_COARSEN_LAST`) or undefined (`DM_ADAPT_DETERMINE`).
  This only matters for the purposes of reference counting: during `DMDestroy()`, cyclic
  references can be found between `DM`s only if the cyclic reference is due to a fine/coarse relationship (see
  `DMSetFineDM()`/`DMSetCoarseDM()`).  If the purpose is not refinement or coarsening, and the user does not maintain a
  reference to the post-adaptation forest (i.e., the one created by `DMForestTemplate()`), then this can cause a memory
  leak.  This method is used by subtypes of `DMFOREST` when automatically constructing mesh hierarchies.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. purpose - the adaptivity purpose

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestTemplate()`, `DMForestSetAdaptivityForest()`, `DMForestGetAdaptivityForest()`, `DMAdaptFlag`
@*/
PetscErrorCode DMForestGetAdaptivityPurpose(DM dm, DMAdaptFlag *purpose)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  forest   = (DM_Forest *)dm->data;
  *purpose = forest->adaptPurpose;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetAdjacencyDimension - During the pre-setup phase, set the dimension of interface points that determine
  cell adjacency (for the purposes of partitioning and overlap).

  Logically Collective

  Input Parameters:
+ dm - the forest
- adjDim - default 0 (i.e., vertices determine adjacency)

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetAdjacencyDimension()`, `DMForestSetAdjacencyCodimension()`, `DMForestSetPartitionOverlap()`
@*/
PetscErrorCode DMForestSetAdjacencyDimension(DM dm, PetscInt adjDim)
{
  PetscInt   dim;
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the adjacency dimension after setup");
  PetscCheck(adjDim >= 0, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "adjacency dim cannot be < 0: %" PetscInt_FMT, adjDim);
  PetscCall(DMGetDimension(dm, &dim));
  PetscCheck(adjDim <= dim, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "adjacency dim cannot be > %" PetscInt_FMT ": %" PetscInt_FMT, dim, adjDim);
  forest->adjDim = adjDim;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetAdjacencyCodimension - Like `DMForestSetAdjacencyDimension()`, but specified as a co-dimension (so that,
  e.g., adjacency based on facets can be specified by codimension 1 in all cases)

  Logically Collective

  Input Parameters:
+ dm - the forest
- adjCodim - default is the dimension of the forest (see `DMGetDimension()`), since this is the codimension of vertices

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetAdjacencyCodimension()`, `DMForestSetAdjacencyDimension()`
@*/
PetscErrorCode DMForestSetAdjacencyCodimension(DM dm, PetscInt adjCodim)
{
  PetscInt dim;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMForestSetAdjacencyDimension(dm, dim - adjCodim));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetAdjacencyDimension - Get the dimension of interface points that determine cell adjacency (for the
  purposes of partitioning and overlap).

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. adjDim - default 0 (i.e., vertices determine adjacency)

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetAdjacencyDimension()`, `DMForestGetAdjacencyCodimension()`, `DMForestSetPartitionOverlap()`
@*/
PetscErrorCode DMForestGetAdjacencyDimension(DM dm, PetscInt *adjDim)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidIntPointer(adjDim, 2);
  *adjDim = forest->adjDim;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetAdjacencyCodimension - Like `DMForestGetAdjacencyDimension()`, but specified as a co-dimension (so that,
  e.g., adjacency based on facets can be specified by codimension 1 in all cases)

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. adjCodim - default isthe dimension of the forest (see `DMGetDimension()`), since this is the codimension of vertices

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetAdjacencyCodimension()`, `DMForestGetAdjacencyDimension()`
@*/
PetscErrorCode DMForestGetAdjacencyCodimension(DM dm, PetscInt *adjCodim)
{
  DM_Forest *forest = (DM_Forest *)dm->data;
  PetscInt   dim;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidIntPointer(adjCodim, 2);
  PetscCall(DMGetDimension(dm, &dim));
  *adjCodim = dim - forest->adjDim;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetPartitionOverlap - During the pre-setup phase, set the amount of cell-overlap present in parallel
  partitions of a forest, with values > 0 indicating subdomains that are expanded by that many iterations of adding
  adjacent cells

  Logically Collective

  Input Parameters:
+ dm - the forest
- overlap - default 0

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetPartitionOverlap()`, `DMForestSetAdjacencyDimension()`, `DMForestSetAdjacencyCodimension()`
@*/
PetscErrorCode DMForestSetPartitionOverlap(DM dm, PetscInt overlap)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the overlap after setup");
  PetscCheck(overlap >= 0, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "overlap cannot be < 0: %" PetscInt_FMT, overlap);
  forest->overlap = overlap;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetPartitionOverlap - Get the amount of cell-overlap present in parallel partitions of a forest, with values
  > 0 indicating subdomains that are expanded by that many iterations of adding adjacent cells

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. overlap - default 0

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetPartitionOverlap()`, `DMForestSetAdjacencyDimension()`, `DMForestSetAdjacencyCodimension()`
@*/
PetscErrorCode DMForestGetPartitionOverlap(DM dm, PetscInt *overlap)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidIntPointer(overlap, 2);
  *overlap = forest->overlap;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetMinimumRefinement - During the pre-setup phase, set the minimum level of refinement (relative to the base
  `DM`, see `DMForestGetBaseDM()`) allowed in the forest.  If the forest is being created by coarsening a previous forest
  (see `DMForestGetAdaptivityForest()`) this limits the amount of coarsening.

  Logically Collective

  Input Parameters:
+ dm - the forest
- minRefinement - default `PETSC_DEFAULT` (interpreted by the subtype of `DMFOREST`)

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetMinimumRefinement()`, `DMForestSetMaximumRefinement()`, `DMForestSetInitialRefinement()`, `DMForestGetBaseDM()`, `DMForestGetAdaptivityForest()`
@*/
PetscErrorCode DMForestSetMinimumRefinement(DM dm, PetscInt minRefinement)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the minimum refinement after setup");
  forest->minRefinement = minRefinement;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetMinimumRefinement - Get the minimum level of refinement (relative to the base `DM`, see
  `DMForestGetBaseDM()`) allowed in the forest.  If the forest is being created by coarsening a previous forest (see
  `DMForestGetAdaptivityForest()`), this limits the amount of coarsening.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. minRefinement - default `PETSC_DEFAULT` (interpreted by the subtype of `DMFOREST`)

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetMinimumRefinement()`, `DMForestGetMaximumRefinement()`, `DMForestGetInitialRefinement()`, `DMForestGetBaseDM()`, `DMForestGetAdaptivityForest()`
@*/
PetscErrorCode DMForestGetMinimumRefinement(DM dm, PetscInt *minRefinement)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidIntPointer(minRefinement, 2);
  *minRefinement = forest->minRefinement;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetInitialRefinement - During the pre-setup phase, set the initial level of refinement (relative to the base
  `DM`, see `DMForestGetBaseDM()`) allowed in the forest.

  Logically Collective

  Input Parameters:
+ dm - the forest
- initefinement - default `PETSC_DEFAULT` (interpreted by the subtype of `DMFOREST`)

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetMinimumRefinement()`, `DMForestSetMaximumRefinement()`, `DMForestGetBaseDM()`
@*/
PetscErrorCode DMForestSetInitialRefinement(DM dm, PetscInt initRefinement)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the initial refinement after setup");
  forest->initRefinement = initRefinement;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetInitialRefinement - Get the initial level of refinement (relative to the base `DM`, see
  `DMForestGetBaseDM()`) allowed in the forest.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. initRefinement - default `PETSC_DEFAULT` (interpreted by the subtype of `DMFOREST`)

  Level: intermediate

.seealso: `DM`, `DMFOREST`,  `DMForestSetMinimumRefinement()`, `DMForestSetMaximumRefinement()`, `DMForestGetBaseDM()`
@*/
PetscErrorCode DMForestGetInitialRefinement(DM dm, PetscInt *initRefinement)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidIntPointer(initRefinement, 2);
  *initRefinement = forest->initRefinement;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetMaximumRefinement - During the pre-setup phase, set the maximum level of refinement (relative to the base
  `DM`, see `DMForestGetBaseDM()`) allowed in the forest.  If the forest is being created by refining a previous forest
  (see `DMForestGetAdaptivityForest()`), this limits the amount of refinement.

  Logically Collective

  Input Parameters:
+ dm - the forest
- maxRefinement - default `PETSC_DEFAULT` (interpreted by the subtype of `DMFOREST`)

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetMinimumRefinement()`, `DMForestSetMaximumRefinement()`, `DMForestSetInitialRefinement()`, `DMForestGetBaseDM()`, `DMForestGetAdaptivityDM()`
@*/
PetscErrorCode DMForestSetMaximumRefinement(DM dm, PetscInt maxRefinement)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the maximum refinement after setup");
  forest->maxRefinement = maxRefinement;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetMaximumRefinement - Get the maximum level of refinement (relative to the base `DM`, see
  `DMForestGetBaseDM()`) allowed in the forest.  If the forest is being created by refining a previous forest (see
  `DMForestGetAdaptivityForest`()), this limits the amount of refinement.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. maxRefinement - default `PETSC_DEFAULT` (interpreted by the subtype of `DMFOREST`)

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetMaximumRefinement()`, `DMForestGetMinimumRefinement()`, `DMForestGetInitialRefinement()`, `DMForestGetBaseDM()`, `DMForestGetAdaptivityForest()`
@*/
PetscErrorCode DMForestGetMaximumRefinement(DM dm, PetscInt *maxRefinement)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidIntPointer(maxRefinement, 2);
  *maxRefinement = forest->maxRefinement;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMForestSetAdaptivityStrategy - During the pre-setup phase, set the strategy for combining adaptivity labels from multiple processes.

  Logically Collective

  Input Parameters:
+ dm - the forest
- adaptStrategy - default `DMFORESTADAPTALL`

  Level: advanced

  Notes:
  Subtypes of `DMFOREST` may define their own strategies.  Two default strategies are `DMFORESTADAPTALL`, which indicates that all processes must agree
  for a refinement/coarsening flag to be valid, and `DMFORESTADAPTANY`, which indicates that only one process needs to
  specify refinement/coarsening.

.seealso: `DM`, `DMFOREST`, `DMForestGetAdaptivityStrategy()`, `DMFORESTADAPTALL`, `DMFORESTADAPTANY`
@*/
PetscErrorCode DMForestSetAdaptivityStrategy(DM dm, DMForestAdaptivityStrategy adaptStrategy)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(PetscFree(forest->adaptStrategy));
  PetscCall(PetscStrallocpy((const char *)adaptStrategy, (char **)&forest->adaptStrategy));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMForestSetAdaptivityStrategy - Get the strategy for combining adaptivity labels from multiple processes.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. adaptStrategy - the adaptivity strategy (default `DMFORESTADAPTALL`)

  Level: advanced

  Note:
  Subtypes
  of `DMFOREST` may define their own strategies.  Two default strategies are `DMFORESTADAPTALL`, which indicates that all
  processes must agree for a refinement/coarsening flag to be valid, and `DMFORESTADAPTANY`, which indicates that only
  one process needs to specify refinement/coarsening.

.seealso: `DM`, `DMFOREST`, `DMFORESTADAPTALL`, `DMFORESTADAPTANY`, `DMForestSetAdaptivityStrategy()`
@*/
PetscErrorCode DMForestGetAdaptivityStrategy(DM dm, DMForestAdaptivityStrategy *adaptStrategy)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(adaptStrategy, 2);
  *adaptStrategy = forest->adaptStrategy;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetAdaptivitySuccess - Return whether the requested adaptation (refinement, coarsening, repartitioning,
  etc.) was successful.

  Collective

  Input Parameter:
. dm - the post-adaptation forest

  Output Parameter:
. success - `PETSC_TRUE` if the post-adaptation forest is different from the pre-adaptation forest.

  Level: intermediate

  Notes:
  `PETSC_FALSE` indicates that the post-adaptation forest is the same as the pre-adpatation
  forest.  A requested adaptation may have been unsuccessful if, for example, the requested refinement would have
  exceeded the maximum refinement level.

.seealso: `DM`, `DMFOREST`
@*/
PetscErrorCode DMForestGetAdaptivitySuccess(DM dm, PetscBool *success)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "DMSetUp() has not been called yet.");
  forest = (DM_Forest *)dm->data;
  PetscCall((forest->getadaptivitysuccess)(dm, success));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetComputeAdaptivitySF - During the pre-setup phase, set whether transfer `PetscSF`s should be computed
  relating the cells of the pre-adaptation forest to the post-adaptiation forest.

  Logically Collective

  Input Parameters:
+ dm - the post-adaptation forest
- computeSF - default `PETSC_TRUE`

  Level: advanced

  Note:
  After `DMSetUp()` is called, the transfer `PetscSF`s can be accessed with `DMForestGetAdaptivitySF()`.

.seealso: `DM`, `DMFOREST`, `DMForestGetComputeAdaptivitySF()`, `DMForestGetAdaptivitySF()`
@*/
PetscErrorCode DMForestSetComputeAdaptivitySF(DM dm, PetscBool computeSF)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot compute adaptivity PetscSFs after setup is called");
  forest                 = (DM_Forest *)dm->data;
  forest->computeAdaptSF = computeSF;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMForestTransferVec(DM dmIn, Vec vecIn, DM dmOut, Vec vecOut, PetscBool useBCs, PetscReal time)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dmIn, DM_CLASSID, 1);
  PetscValidHeaderSpecific(vecIn, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(dmOut, DM_CLASSID, 3);
  PetscValidHeaderSpecific(vecOut, VEC_CLASSID, 4);
  forest = (DM_Forest *)dmIn->data;
  PetscCheck(forest->transfervec, PetscObjectComm((PetscObject)dmIn), PETSC_ERR_SUP, "DMForestTransferVec() not implemented");
  PetscCall((forest->transfervec)(dmIn, vecIn, dmOut, vecOut, useBCs, time));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMForestTransferVecFromBase(DM dm, Vec vecIn, Vec vecOut)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidHeaderSpecific(vecIn, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(vecOut, VEC_CLASSID, 3);
  forest = (DM_Forest *)dm->data;
  PetscCheck(forest->transfervecfrombase, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "DMForestTransferVecFromBase() not implemented");
  PetscCall((forest->transfervecfrombase)(dm, vecIn, vecOut));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetComputeAdaptivitySF - Get whether transfer `PetscSF`s should be computed relating the cells of the
  pre-adaptation forest to the post-adaptiation forest.  After `DMSetUp()` is called, these transfer PetscSFs can be
  accessed with `DMForestGetAdaptivitySF()`.

  Not Collective

  Input Parameter:
. dm - the post-adaptation forest

  Output Parameter:
. computeSF - default `PETSC_TRUE`

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestSetComputeAdaptivitySF()`, `DMForestGetAdaptivitySF()`
@*/
PetscErrorCode DMForestGetComputeAdaptivitySF(DM dm, PetscBool *computeSF)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  forest     = (DM_Forest *)dm->data;
  *computeSF = forest->computeAdaptSF;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetAdaptivitySF - Get `PetscSF`s that relate the pre-adaptation forest to the post-adaptation forest.
  Adaptation can be any combination of refinement, coarsening, repartition, and change of overlap, so there may be
  some cells of the pre-adaptation that are parents of post-adaptation cells, and vice versa.  Therefore there are two
  `PetscSF`s: one that relates pre-adaptation coarse cells to post-adaptation fine cells, and one that relates
  pre-adaptation fine cells to post-adaptation coarse cells.

  Not Collective

  Input Parameter:
.  dm - the post-adaptation forest

  Output Parameters:
+  preCoarseToFine - pre-adaptation coarse cells to post-adaptation fine cells: BCast goes from pre- to post-
-  coarseToPreFine - post-adaptation coarse cells to pre-adaptation fine cells: BCast goes from post- to pre-

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestGetComputeAdaptivitySF()`, `DMForestSetComputeAdaptivitySF()`
@*/
PetscErrorCode DMForestGetAdaptivitySF(DM dm, PetscSF *preCoarseToFine, PetscSF *coarseToPreFine)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMSetUp(dm));
  forest = (DM_Forest *)dm->data;
  if (preCoarseToFine) *preCoarseToFine = forest->preCoarseToFine;
  if (coarseToPreFine) *coarseToPreFine = forest->coarseToPreFine;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetGradeFactor - During the pre-setup phase, set the desired amount of grading in the mesh, e.g. give 2 to
  indicate that the diameter of neighboring cells should differ by at most a factor of 2.  Subtypes of `DMFOREST` may
  only support one particular choice of grading factor.

  Logically Collective

  Input Parameters:
+ dm - the forest
- grade - the grading factor

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestGetGradeFactor()`
@*/
PetscErrorCode DMForestSetGradeFactor(DM dm, PetscInt grade)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the grade factor after setup");
  forest->gradeFactor = grade;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetGradeFactor - Get the desired amount of grading in the mesh, e.g. give 2 to indicate that the diameter of
  neighboring cells should differ by at most a factor of 2.  Subtypes of `DMFOREST` may only support one particular
  choice of grading factor.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. grade - the grading factor

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestSetGradeFactor()`
@*/
PetscErrorCode DMForestGetGradeFactor(DM dm, PetscInt *grade)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidIntPointer(grade, 2);
  *grade = forest->gradeFactor;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetCellWeightFactor - During the pre-setup phase, set the factor by which the level of refinement changes
  the cell weight (see `DMForestSetCellWeights()`) when calculating partitions.  The final weight of a cell will be
  (cellWeight) * (weightFactor^refinementLevel).  A factor of 1 indicates that the weight of a cell does not depend on
  its level; a factor of 2, for example, might be appropriate for sub-cycling time-stepping methods, when the
  computation associated with a cell is multiplied by a factor of 2 for each additional level of refinement.

  Logically Collective

  Input Parameters:
+ dm - the forest
- weightsFactors - default 1.

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestGetCellWeightFactor()`, `DMForestSetCellWeights()`
@*/
PetscErrorCode DMForestSetCellWeightFactor(DM dm, PetscReal weightsFactor)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the weights factor after setup");
  forest->weightsFactor = weightsFactor;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetCellWeightFactor - Get the factor by which the level of refinement changes the cell weight (see
  `DMForestSetCellWeights()`) when calculating partitions.  The final weight of a cell will be (cellWeight) *
  (weightFactor^refinementLevel).  A factor of 1 indicates that the weight of a cell does not depend on its level; a
  factor of 2, for example, might be appropriate for sub-cycling time-stepping methods, when the computation
  associated with a cell is multiplied by a factor of 2 for each additional level of refinement.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. weightsFactors - default 1.

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestSetCellWeightFactor()`, `DMForestSetCellWeights()`
@*/
PetscErrorCode DMForestGetCellWeightFactor(DM dm, PetscReal *weightsFactor)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidRealPointer(weightsFactor, 2);
  *weightsFactor = forest->weightsFactor;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetCellChart - After the setup phase, get the local half-open interval of the chart of cells on this process

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameters:
+ cStart - the first cell on this process
- cEnd - one after the final cell on this process

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetCellSF()`
@*/
PetscErrorCode DMForestGetCellChart(DM dm, PetscInt *cStart, PetscInt *cEnd)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidIntPointer(cStart, 2);
  PetscValidIntPointer(cEnd, 3);
  if (((forest->cStart == PETSC_DETERMINE) || (forest->cEnd == PETSC_DETERMINE)) && forest->createcellchart) PetscCall(forest->createcellchart(dm, &forest->cStart, &forest->cEnd));
  *cStart = forest->cStart;
  *cEnd   = forest->cEnd;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetCellSF - After the setup phase, get the `PetscSF` for overlapping cells between processes

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. cellSF - the `PetscSF`

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetCellChart()`
@*/
PetscErrorCode DMForestGetCellSF(DM dm, PetscSF *cellSF)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(cellSF, 2);
  if ((!forest->cellSF) && forest->createcellsf) PetscCall(forest->createcellsf(dm, &forest->cellSF));
  *cellSF = forest->cellSF;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMForestSetAdaptivityLabel - During the pre-setup phase, set the label of the pre-adaptation forest (see
  `DMForestGetAdaptivityForest()`) that holds the adaptation flags (refinement, coarsening, or some combination).  The
  interpretation of the label values is up to the subtype of `DMFOREST`, but `DM_ADAPT_DETERMINE`, `DM_ADAPT_KEEP`,
  `DM_ADAPT_REFINE`, and `DM_ADAPT_COARSEN` have been reserved as choices that should be accepted by all subtypes.

  Logically Collective

  Input Parameters:
- dm - the forest
+ adaptLabel - the label in the pre-adaptation forest

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestGetAdaptivityLabel()`
@*/
PetscErrorCode DMForestSetAdaptivityLabel(DM dm, DMLabel adaptLabel)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  if (adaptLabel) PetscValidHeaderSpecific(adaptLabel, DMLABEL_CLASSID, 2);
  PetscCall(PetscObjectReference((PetscObject)adaptLabel));
  PetscCall(DMLabelDestroy(&forest->adaptLabel));
  forest->adaptLabel = adaptLabel;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMForestGetAdaptivityLabel - Get the label of the pre-adaptation forest (see `DMForestGetAdaptivityForest()`) that
  holds the adaptation flags (refinement, coarsening, or some combination).  The interpretation of the label values is
  up to the subtype of `DMFOREST`, but `DM_ADAPT_DETERMINE`, `DM_ADAPT_KEEP`, `DM_ADAPT_REFINE`, and `DM_ADAPT_COARSEN` have
  been reserved as choices that should be accepted by all subtypes.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. adaptLabel - the name of the label in the pre-adaptation forest

  Level: intermediate

.seealso: `DM`, `DMFOREST`, `DMForestSetAdaptivityLabel()`
@*/
PetscErrorCode DMForestGetAdaptivityLabel(DM dm, DMLabel *adaptLabel)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  *adaptLabel = forest->adaptLabel;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetCellWeights - Set the weights assigned to each of the cells (see `DMForestGetCellChart()`) of the current
  process: weights are used to determine parallel partitioning.

  Logically Collective

  Input Parameters:
+ dm - the forest
. weights - the array of weights (see `DMForestSetWeightCapacity()`) for all cells, or `NULL` to indicate each cell has weight 1.
- copyMode - how weights should reference weights

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestGetCellWeights()`, `DMForestSetWeightCapacity()`
@*/
PetscErrorCode DMForestSetCellWeights(DM dm, PetscReal weights[], PetscCopyMode copyMode)
{
  DM_Forest *forest = (DM_Forest *)dm->data;
  PetscInt   cStart, cEnd;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMForestGetCellChart(dm, &cStart, &cEnd));
  PetscCheck(cEnd >= cStart, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "cell chart [%" PetscInt_FMT ",%" PetscInt_FMT ") is not valid", cStart, cEnd);
  if (copyMode == PETSC_COPY_VALUES) {
    if (forest->cellWeightsCopyMode != PETSC_OWN_POINTER || forest->cellWeights == weights) PetscCall(PetscMalloc1(cEnd - cStart, &forest->cellWeights));
    PetscCall(PetscArraycpy(forest->cellWeights, weights, cEnd - cStart));
    forest->cellWeightsCopyMode = PETSC_OWN_POINTER;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (forest->cellWeightsCopyMode == PETSC_OWN_POINTER) PetscCall(PetscFree(forest->cellWeights));
  forest->cellWeights         = weights;
  forest->cellWeightsCopyMode = copyMode;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetCellWeights - Get the weights assigned to each of the cells (see `DMForestGetCellChart()`) of the current
  process: weights are used to determine parallel partitioning.

  Not Collective

  Input Parameter:
. dm - the forest

  Output Parameter:
. weights - the array of weights for all cells, or `NULL` to indicate each cell has weight 1.

  Level: advanced

.seealso: `DM`, `DMFOREST`, `DMForestSetCellWeights()`, `DMForestSetWeightCapacity()`
@*/
PetscErrorCode DMForestGetCellWeights(DM dm, PetscReal **weights)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(weights, 2);
  *weights = forest->cellWeights;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestSetWeightCapacity - During the pre-setup phase, set the capacity of the current process when repartitioning
  a pre-adaptation forest (see `DMForestGetAdaptivityForest()`).  After partitioning, the ratio of the weight of each
  process's cells to the process's capacity will be roughly equal for all processes.  A capacity of 0 indicates that
  the current process should not have any cells after repartitioning.

  Logically Collective

  Input parameters:
+ dm - the forest
- capacity - this process's capacity

  Level: advanced

.seealso `DM`, `DMFOREST`, `DMForestGetWeightCapacity()`, `DMForestSetCellWeights()`, `DMForestSetCellWeightFactor()`
@*/
PetscErrorCode DMForestSetWeightCapacity(DM dm, PetscReal capacity)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCheck(!dm->setupcalled, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONGSTATE, "Cannot change the weight capacity after setup");
  PetscCheck(capacity >= 0., PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Cannot have negative weight capacity; %g", (double)capacity);
  forest->weightCapacity = capacity;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMForestGetWeightCapacity - Set the capacity of the current process when repartitioning a pre-adaptation forest (see
  `DMForestGetAdaptivityForest()`).  After partitioning, the ratio of the weight of each process's cells to the
  process's capacity will be roughly equal for all processes.  A capacity of 0 indicates that the current process
  should not have any cells after repartitioning.

  Not Collective

  Input parameter:
. dm - the forest

  Output parameter:
. capacity - this process's capacity

  Level: advanced

.seealso `DM`, `DMFOREST`, `DMForestSetWeightCapacity()`, `DMForestSetCellWeights()`, `DMForestSetCellWeightFactor()`
@*/
PetscErrorCode DMForestGetWeightCapacity(DM dm, PetscReal *capacity)
{
  DM_Forest *forest = (DM_Forest *)dm->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidRealPointer(capacity, 2);
  *capacity = forest->weightCapacity;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN PetscErrorCode DMSetFromOptions_Forest(DM dm, PetscOptionItems *PetscOptionsObject)
{
  PetscBool                  flg, flg1, flg2, flg3, flg4;
  DMForestTopology           oldTopo;
  char                       stringBuffer[256];
  PetscViewer                viewer;
  PetscViewerFormat          format;
  PetscInt                   adjDim, adjCodim, overlap, minRefinement, initRefinement, maxRefinement, grade;
  PetscReal                  weightsFactor;
  DMForestAdaptivityStrategy adaptStrategy;

  PetscFunctionBegin;
  PetscCall(DMForestGetTopology(dm, &oldTopo));
  PetscOptionsHeadBegin(PetscOptionsObject, "DMForest Options");
  PetscCall(PetscOptionsString("-dm_forest_topology", "the topology of the forest's base mesh", "DMForestSetTopology", oldTopo, stringBuffer, sizeof(stringBuffer), &flg1));
  PetscCall(PetscOptionsViewer("-dm_forest_base_dm", "load the base DM from a viewer specification", "DMForestSetBaseDM", &viewer, &format, &flg2));
  PetscCall(PetscOptionsViewer("-dm_forest_coarse_forest", "load the coarse forest from a viewer specification", "DMForestSetCoarseForest", &viewer, &format, &flg3));
  PetscCall(PetscOptionsViewer("-dm_forest_fine_forest", "load the fine forest from a viewer specification", "DMForestSetFineForest", &viewer, &format, &flg4));
  PetscCheck((PetscInt)flg1 + (PetscInt)flg2 + (PetscInt)flg3 + (PetscInt)flg4 <= 1, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_INCOMP, "Specify only one of -dm_forest_{topology,base_dm,coarse_forest,fine_forest}");
  if (flg1) {
    PetscCall(DMForestSetTopology(dm, (DMForestTopology)stringBuffer));
    PetscCall(DMForestSetBaseDM(dm, NULL));
    PetscCall(DMForestSetAdaptivityForest(dm, NULL));
  }
  if (flg2) {
    DM base;

    PetscCall(DMCreate(PetscObjectComm((PetscObject)dm), &base));
    PetscCall(PetscViewerPushFormat(viewer, format));
    PetscCall(DMLoad(base, viewer));
    PetscCall(PetscViewerDestroy(&viewer));
    PetscCall(DMForestSetBaseDM(dm, base));
    PetscCall(DMDestroy(&base));
    PetscCall(DMForestSetTopology(dm, NULL));
    PetscCall(DMForestSetAdaptivityForest(dm, NULL));
  }
  if (flg3) {
    DM coarse;

    PetscCall(DMCreate(PetscObjectComm((PetscObject)dm), &coarse));
    PetscCall(PetscViewerPushFormat(viewer, format));
    PetscCall(DMLoad(coarse, viewer));
    PetscCall(PetscViewerDestroy(&viewer));
    PetscCall(DMForestSetAdaptivityForest(dm, coarse));
    PetscCall(DMDestroy(&coarse));
    PetscCall(DMForestSetTopology(dm, NULL));
    PetscCall(DMForestSetBaseDM(dm, NULL));
  }
  if (flg4) {
    DM fine;

    PetscCall(DMCreate(PetscObjectComm((PetscObject)dm), &fine));
    PetscCall(PetscViewerPushFormat(viewer, format));
    PetscCall(DMLoad(fine, viewer));
    PetscCall(PetscViewerDestroy(&viewer));
    PetscCall(DMForestSetAdaptivityForest(dm, fine));
    PetscCall(DMDestroy(&fine));
    PetscCall(DMForestSetTopology(dm, NULL));
    PetscCall(DMForestSetBaseDM(dm, NULL));
  }
  PetscCall(DMForestGetAdjacencyDimension(dm, &adjDim));
  PetscCall(PetscOptionsBoundedInt("-dm_forest_adjacency_dimension", "set the dimension of points that define adjacency in the forest", "DMForestSetAdjacencyDimension", adjDim, &adjDim, &flg, 0));
  if (flg) {
    PetscCall(DMForestSetAdjacencyDimension(dm, adjDim));
  } else {
    PetscCall(DMForestGetAdjacencyCodimension(dm, &adjCodim));
    PetscCall(PetscOptionsBoundedInt("-dm_forest_adjacency_codimension", "set the codimension of points that define adjacency in the forest", "DMForestSetAdjacencyCodimension", adjCodim, &adjCodim, &flg, 1));
    if (flg) PetscCall(DMForestSetAdjacencyCodimension(dm, adjCodim));
  }
  PetscCall(DMForestGetPartitionOverlap(dm, &overlap));
  PetscCall(PetscOptionsBoundedInt("-dm_forest_partition_overlap", "set the degree of partition overlap", "DMForestSetPartitionOverlap", overlap, &overlap, &flg, 0));
  if (flg) PetscCall(DMForestSetPartitionOverlap(dm, overlap));
#if 0
  PetscCall(PetscOptionsBoundedInt("-dm_refine","equivalent to -dm_forest_set_minimum_refinement and -dm_forest_set_initial_refinement with the same value",NULL,minRefinement,&minRefinement,&flg,0));
  if (flg) {
    PetscCall(DMForestSetMinimumRefinement(dm,minRefinement));
    PetscCall(DMForestSetInitialRefinement(dm,minRefinement));
  }
  PetscCall(PetscOptionsBoundedInt("-dm_refine_hierarchy","equivalent to -dm_forest_set_minimum_refinement 0 and -dm_forest_set_initial_refinement",NULL,initRefinement,&initRefinement,&flg,0));
  if (flg) {
    PetscCall(DMForestSetMinimumRefinement(dm,0));
    PetscCall(DMForestSetInitialRefinement(dm,initRefinement));
  }
#endif
  PetscCall(DMForestGetMinimumRefinement(dm, &minRefinement));
  PetscCall(PetscOptionsBoundedInt("-dm_forest_minimum_refinement", "set the minimum level of refinement in the forest", "DMForestSetMinimumRefinement", minRefinement, &minRefinement, &flg, 0));
  if (flg) PetscCall(DMForestSetMinimumRefinement(dm, minRefinement));
  PetscCall(DMForestGetInitialRefinement(dm, &initRefinement));
  PetscCall(PetscOptionsBoundedInt("-dm_forest_initial_refinement", "set the initial level of refinement in the forest", "DMForestSetInitialRefinement", initRefinement, &initRefinement, &flg, 0));
  if (flg) PetscCall(DMForestSetInitialRefinement(dm, initRefinement));
  PetscCall(DMForestGetMaximumRefinement(dm, &maxRefinement));
  PetscCall(PetscOptionsBoundedInt("-dm_forest_maximum_refinement", "set the maximum level of refinement in the forest", "DMForestSetMaximumRefinement", maxRefinement, &maxRefinement, &flg, 0));
  if (flg) PetscCall(DMForestSetMaximumRefinement(dm, maxRefinement));
  PetscCall(DMForestGetAdaptivityStrategy(dm, &adaptStrategy));
  PetscCall(PetscOptionsString("-dm_forest_adaptivity_strategy", "the forest's adaptivity-flag resolution strategy", "DMForestSetAdaptivityStrategy", adaptStrategy, stringBuffer, sizeof(stringBuffer), &flg));
  if (flg) PetscCall(DMForestSetAdaptivityStrategy(dm, (DMForestAdaptivityStrategy)stringBuffer));
  PetscCall(DMForestGetGradeFactor(dm, &grade));
  PetscCall(PetscOptionsBoundedInt("-dm_forest_grade_factor", "grade factor between neighboring cells", "DMForestSetGradeFactor", grade, &grade, &flg, 0));
  if (flg) PetscCall(DMForestSetGradeFactor(dm, grade));
  PetscCall(DMForestGetCellWeightFactor(dm, &weightsFactor));
  PetscCall(PetscOptionsReal("-dm_forest_cell_weight_factor", "multiplying weight factor for cell refinement", "DMForestSetCellWeightFactor", weightsFactor, &weightsFactor, &flg));
  if (flg) PetscCall(DMForestSetCellWeightFactor(dm, weightsFactor));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateSubDM_Forest(DM dm, PetscInt numFields, const PetscInt fields[], IS *is, DM *subdm)
{
  PetscFunctionBegin;
  if (subdm) PetscCall(DMClone(dm, subdm));
  PetscCall(DMCreateSectionSubDM(dm, numFields, fields, is, subdm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMRefine_Forest(DM dm, MPI_Comm comm, DM *dmRefined)
{
  DMLabel refine;
  DM      fineDM;

  PetscFunctionBegin;
  PetscCall(DMGetFineDM(dm, &fineDM));
  if (fineDM) {
    PetscCall(PetscObjectReference((PetscObject)fineDM));
    *dmRefined = fineDM;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(DMForestTemplate(dm, comm, dmRefined));
  PetscCall(DMGetLabel(dm, "refine", &refine));
  if (!refine) {
    PetscCall(DMLabelCreate(PETSC_COMM_SELF, "refine", &refine));
    PetscCall(DMLabelSetDefaultValue(refine, DM_ADAPT_REFINE));
  } else PetscCall(PetscObjectReference((PetscObject)refine));
  PetscCall(DMForestSetAdaptivityLabel(*dmRefined, refine));
  PetscCall(DMLabelDestroy(&refine));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCoarsen_Forest(DM dm, MPI_Comm comm, DM *dmCoarsened)
{
  DMLabel coarsen;
  DM      coarseDM;

  PetscFunctionBegin;
  {
    PetscMPIInt mpiComparison;
    MPI_Comm    dmcomm = PetscObjectComm((PetscObject)dm);

    PetscCallMPI(MPI_Comm_compare(comm, dmcomm, &mpiComparison));
    PetscCheck(mpiComparison == MPI_IDENT || mpiComparison == MPI_CONGRUENT, dmcomm, PETSC_ERR_SUP, "No support for different communicators yet");
  }
  PetscCall(DMGetCoarseDM(dm, &coarseDM));
  if (coarseDM) {
    PetscCall(PetscObjectReference((PetscObject)coarseDM));
    *dmCoarsened = coarseDM;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(DMForestTemplate(dm, comm, dmCoarsened));
  PetscCall(DMForestSetAdaptivityPurpose(*dmCoarsened, DM_ADAPT_COARSEN));
  PetscCall(DMGetLabel(dm, "coarsen", &coarsen));
  if (!coarsen) {
    PetscCall(DMLabelCreate(PETSC_COMM_SELF, "coarsen", &coarsen));
    PetscCall(DMLabelSetDefaultValue(coarsen, DM_ADAPT_COARSEN));
  } else PetscCall(PetscObjectReference((PetscObject)coarsen));
  PetscCall(DMForestSetAdaptivityLabel(*dmCoarsened, coarsen));
  PetscCall(DMLabelDestroy(&coarsen));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMAdaptLabel_Forest(DM dm, PETSC_UNUSED Vec metric, DMLabel label, PETSC_UNUSED DMLabel rgLabel, DM *adaptedDM)
{
  PetscBool success;

  PetscFunctionBegin;
  PetscCall(DMForestTemplate(dm, PetscObjectComm((PetscObject)dm), adaptedDM));
  PetscCall(DMForestSetAdaptivityLabel(*adaptedDM, label));
  PetscCall(DMSetUp(*adaptedDM));
  PetscCall(DMForestGetAdaptivitySuccess(*adaptedDM, &success));
  if (!success) {
    PetscCall(DMDestroy(adaptedDM));
    *adaptedDM = NULL;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMInitialize_Forest(DM dm)
{
  PetscFunctionBegin;
  PetscCall(PetscMemzero(dm->ops, sizeof(*(dm->ops))));

  dm->ops->clone          = DMClone_Forest;
  dm->ops->setfromoptions = DMSetFromOptions_Forest;
  dm->ops->destroy        = DMDestroy_Forest;
  dm->ops->createsubdm    = DMCreateSubDM_Forest;
  dm->ops->refine         = DMRefine_Forest;
  dm->ops->coarsen        = DMCoarsen_Forest;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC

     DMFOREST = "forest" - A DM object that encapsulates a hierarchically refined mesh.  Forests usually have a base `DM`
  (see `DMForestGetBaseDM()`), from which it is refined.  The refinement and partitioning of forests is considered
  immutable after `DMSetUp()` is called.  To adapt a mesh, one should call `DMForestTemplate()` to create a new mesh that
  will default to being identical to it, specify how that mesh should differ, and then calling `DMSetUp()` on the new
  mesh.

  To specify that a mesh should be refined or coarsened from the previous mesh, a label should be defined on the
  previous mesh whose values indicate which cells should be refined (`DM_ADAPT_REFINE`) or coarsened (`DM_ADAPT_COARSEN`)
  and how (subtypes are free to allow additional values for things like anisotropic refinement).  The label should be
  given to the *new* mesh with `DMForestSetAdaptivityLabel()`.

  Level: advanced

.seealso: `DMType`, `DM`, `DMCreate()`, `DMSetType()`, `DMForestGetBaseDM()`, `DMForestSetBaseDM()`, `DMForestTemplate()`, `DMForestSetAdaptivityLabel()`
M*/

PETSC_EXTERN PetscErrorCode DMCreate_Forest(DM dm)
{
  DM_Forest *forest;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(PetscNew(&forest));
  dm->dim                     = 0;
  dm->data                    = forest;
  forest->refct               = 1;
  forest->data                = NULL;
  forest->topology            = NULL;
  forest->adapt               = NULL;
  forest->base                = NULL;
  forest->adaptPurpose        = DM_ADAPT_DETERMINE;
  forest->adjDim              = PETSC_DEFAULT;
  forest->overlap             = PETSC_DEFAULT;
  forest->minRefinement       = PETSC_DEFAULT;
  forest->maxRefinement       = PETSC_DEFAULT;
  forest->initRefinement      = PETSC_DEFAULT;
  forest->cStart              = PETSC_DETERMINE;
  forest->cEnd                = PETSC_DETERMINE;
  forest->cellSF              = NULL;
  forest->adaptLabel          = NULL;
  forest->gradeFactor         = 2;
  forest->cellWeights         = NULL;
  forest->cellWeightsCopyMode = PETSC_USE_POINTER;
  forest->weightsFactor       = 1.;
  forest->weightCapacity      = 1.;
  PetscCall(DMForestSetAdaptivityStrategy(dm, DMFORESTADAPTALL));
  PetscCall(DMInitialize_Forest(dm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

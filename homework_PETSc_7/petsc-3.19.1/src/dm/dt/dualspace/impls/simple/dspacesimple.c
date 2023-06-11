#include <petsc/private/petscfeimpl.h> /*I "petscfe.h" I*/
#include <petscdmplex.h>

static PetscErrorCode PetscDualSpaceSetUp_Simple(PetscDualSpace sp)
{
  PetscDualSpace_Simple *s  = (PetscDualSpace_Simple *)sp->data;
  DM                     dm = sp->dm;
  PetscInt               dim, pStart, pEnd;
  PetscSection           section;

  PetscFunctionBegin;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  PetscCall(PetscSectionCreate(PETSC_COMM_SELF, &section));
  PetscCall(PetscSectionSetChart(section, pStart, pEnd));
  PetscCall(PetscSectionSetDof(section, pStart, s->dim));
  PetscCall(PetscSectionSetUp(section));
  sp->pointSection = section;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDualSpaceDestroy_Simple(PetscDualSpace sp)
{
  PetscDualSpace_Simple *s = (PetscDualSpace_Simple *)sp->data;

  PetscFunctionBegin;
  PetscCall(PetscFree(s->numDof));
  PetscCall(PetscFree(s));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscDualSpaceSimpleSetDimension_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscDualSpaceSimpleSetFunctional_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDualSpaceDuplicate_Simple(PetscDualSpace sp, PetscDualSpace spNew)
{
  PetscInt dim, d;

  PetscFunctionBegin;
  PetscCall(PetscDualSpaceGetDimension(sp, &dim));
  PetscCall(PetscDualSpaceSimpleSetDimension(spNew, dim));
  for (d = 0; d < dim; ++d) {
    PetscQuadrature q;

    PetscCall(PetscDualSpaceGetFunctional(sp, d, &q));
    PetscCall(PetscDualSpaceSimpleSetFunctional(spNew, d, q));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDualSpaceSetFromOptions_Simple(PetscDualSpace sp, PetscOptionItems *PetscOptionsObject)
{
  PetscFunctionBegin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDualSpaceSimpleSetDimension_Simple(PetscDualSpace sp, const PetscInt dim)
{
  PetscDualSpace_Simple *s = (PetscDualSpace_Simple *)sp->data;
  DM                     dm;
  PetscInt               spatialDim, f;

  PetscFunctionBegin;
  for (f = 0; f < s->dim; ++f) PetscCall(PetscQuadratureDestroy(&sp->functional[f]));
  PetscCall(PetscFree(sp->functional));
  s->dim = dim;
  PetscCall(PetscCalloc1(s->dim, &sp->functional));
  PetscCall(PetscFree(s->numDof));
  PetscCall(PetscDualSpaceGetDM(sp, &dm));
  PetscCall(DMGetCoordinateDim(dm, &spatialDim));
  PetscCall(PetscCalloc1(spatialDim + 1, &s->numDof));
  s->numDof[spatialDim] = dim;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDualSpaceSimpleSetFunctional_Simple(PetscDualSpace sp, PetscInt f, PetscQuadrature q)
{
  PetscDualSpace_Simple *s = (PetscDualSpace_Simple *)sp->data;
  PetscReal             *weights;
  PetscInt               Nc, c, Nq, p;

  PetscFunctionBegin;
  PetscCheck(!(f < 0) && !(f >= s->dim), PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_OUTOFRANGE, "Basis index %" PetscInt_FMT " not in [0, %" PetscInt_FMT ")", f, s->dim);
  PetscCall(PetscQuadratureDuplicate(q, &sp->functional[f]));
  /* Reweight so that it has unit volume: Do we want to do this for Nc > 1? */
  PetscCall(PetscQuadratureGetData(sp->functional[f], NULL, &Nc, &Nq, NULL, (const PetscReal **)&weights));
  for (c = 0; c < Nc; ++c) {
    PetscReal vol = 0.0;

    for (p = 0; p < Nq; ++p) vol += weights[p * Nc + c];
    for (p = 0; p < Nq; ++p) weights[p * Nc + c] /= (vol == 0.0 ? 1.0 : vol);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDualSpaceSimpleSetDimension - Set the number of functionals in the dual space basis

  Logically Collective

  Input Parameters:
+ sp  - the `PetscDualSpace`
- dim - the basis dimension

  Level: intermediate

.seealso: `PETSCDUALSPACESIMPLE`, `PetscDualSpace`, `PetscDualSpaceSimpleSetFunctional()`
@*/
PetscErrorCode PetscDualSpaceSimpleSetDimension(PetscDualSpace sp, PetscInt dim)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCDUALSPACE_CLASSID, 1);
  PetscValidLogicalCollectiveInt(sp, dim, 2);
  PetscCheck(!sp->setupcalled, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_WRONGSTATE, "Cannot change dimension after dual space is set up");
  PetscTryMethod(sp, "PetscDualSpaceSimpleSetDimension_C", (PetscDualSpace, PetscInt), (sp, dim));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscDualSpaceSimpleSetFunctional - Set the given basis functional for this dual space

  Not Collective

  Input Parameters:
+ sp  - the `PetscDualSpace`
. func - the basis index
- q - the basis functional

  Level: intermediate

  Note:
  The quadrature will be reweighted so that it has unit volume.

.seealso: `PETSCDUALSPACESIMPLE`, `PetscDualSpace`, `PetscDualSpaceSimpleSetDimension()`
@*/
PetscErrorCode PetscDualSpaceSimpleSetFunctional(PetscDualSpace sp, PetscInt func, PetscQuadrature q)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCDUALSPACE_CLASSID, 1);
  PetscTryMethod(sp, "PetscDualSpaceSimpleSetFunctional_C", (PetscDualSpace, PetscInt, PetscQuadrature), (sp, func, q));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDualSpaceInitialize_Simple(PetscDualSpace sp)
{
  PetscFunctionBegin;
  sp->ops->setfromoptions       = PetscDualSpaceSetFromOptions_Simple;
  sp->ops->setup                = PetscDualSpaceSetUp_Simple;
  sp->ops->view                 = NULL;
  sp->ops->destroy              = PetscDualSpaceDestroy_Simple;
  sp->ops->duplicate            = PetscDualSpaceDuplicate_Simple;
  sp->ops->createheightsubspace = NULL;
  sp->ops->createpointsubspace  = NULL;
  sp->ops->getsymmetries        = NULL;
  sp->ops->apply                = PetscDualSpaceApplyDefault;
  sp->ops->applyall             = PetscDualSpaceApplyAllDefault;
  sp->ops->applyint             = PetscDualSpaceApplyInteriorDefault;
  sp->ops->createalldata        = PetscDualSpaceCreateAllDataDefault;
  sp->ops->createintdata        = PetscDualSpaceCreateInteriorDataDefault;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
  PETSCDUALSPACESIMPLE = "simple" - A `PetscDualSpaceType` that encapsulates a dual space of functionals provided with `PetscDualSpaceSimpleSetFunctional()`

  Level: intermediate

  Developer Note:
  It is not clear this has a good name

.seealso: `PetscDualSpace`, `PetscDualSpaceSimpleSetFunctional()`, `PetscDualSpaceType`, `PetscDualSpaceCreate()`, `PetscDualSpaceSetType()`
M*/

PETSC_EXTERN PetscErrorCode PetscDualSpaceCreate_Simple(PetscDualSpace sp)
{
  PetscDualSpace_Simple *s;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCDUALSPACE_CLASSID, 1);
  PetscCall(PetscNew(&s));
  sp->data = s;

  s->dim    = 0;
  s->numDof = NULL;

  PetscCall(PetscDualSpaceInitialize_Simple(sp));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscDualSpaceSimpleSetDimension_C", PetscDualSpaceSimpleSetDimension_Simple));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscDualSpaceSimpleSetFunctional_C", PetscDualSpaceSimpleSetFunctional_Simple));
  PetscFunctionReturn(PETSC_SUCCESS);
}

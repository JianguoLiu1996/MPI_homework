#include <petsc/private/dmimpl.h> /*I      "petscdm.h"          I*/

PETSC_EXTERN PetscErrorCode DMIsForest(DM, PetscBool *);

DMGeneratorFunctionList DMGenerateList              = NULL;
PetscBool               DMGenerateRegisterAllCalled = PETSC_FALSE;

#if defined(PETSC_HAVE_TRIANGLE)
PETSC_EXTERN PetscErrorCode DMPlexGenerate_Triangle(DM, PetscBool, DM *);
PETSC_EXTERN PetscErrorCode DMPlexRefine_Triangle(DM, double *, DM *);
#endif
#if defined(PETSC_HAVE_TETGEN)
PETSC_EXTERN PetscErrorCode DMPlexGenerate_Tetgen(DM, PetscBool, DM *);
PETSC_EXTERN PetscErrorCode DMPlexRefine_Tetgen(DM, double *, DM *);
#endif
#if defined(PETSC_HAVE_CTETGEN)
PETSC_EXTERN PetscErrorCode DMPlexGenerate_CTetgen(DM, PetscBool, DM *);
PETSC_EXTERN PetscErrorCode DMPlexRefine_CTetgen(DM, double *, DM *);
#endif
#if defined(PETSC_HAVE_PRAGMATIC)
PETSC_EXTERN PetscErrorCode DMAdaptMetric_Pragmatic_Plex(DM, Vec, DMLabel, DMLabel, DM *);
#endif
#if defined(PETSC_HAVE_MMG)
PETSC_EXTERN PetscErrorCode DMAdaptMetric_Mmg_Plex(DM, Vec, DMLabel, DMLabel, DM *);
#endif
#if defined(PETSC_HAVE_PARMMG)
PETSC_EXTERN PetscErrorCode DMAdaptMetric_ParMmg_Plex(DM, Vec, DMLabel, DMLabel, DM *);
#endif
PETSC_EXTERN PetscErrorCode DMPlexTransformAdaptLabel(DM, Vec, DMLabel, DMLabel, DM *);
PETSC_EXTERN PetscErrorCode DMAdaptLabel_Forest(DM, Vec, DMLabel, DMLabel, DM *);

/*@C
  DMGenerateRegisterAll - Registers all of the mesh generation methods in the `DM` package.

  Not Collective

  Level: advanced

.seealso: `DM`, `DMGenerateRegisterDestroy()`
@*/
PetscErrorCode DMGenerateRegisterAll(void)
{
  PetscFunctionBegin;
  if (DMGenerateRegisterAllCalled) PetscFunctionReturn(PETSC_SUCCESS);
  DMGenerateRegisterAllCalled = PETSC_TRUE;
#if defined(PETSC_HAVE_TRIANGLE)
  PetscCall(DMGenerateRegister("triangle", DMPlexGenerate_Triangle, DMPlexRefine_Triangle, NULL, 1));
#endif
#if defined(PETSC_HAVE_CTETGEN)
  PetscCall(DMGenerateRegister("ctetgen", DMPlexGenerate_CTetgen, DMPlexRefine_CTetgen, NULL, 2));
#endif
#if defined(PETSC_HAVE_TETGEN)
  PetscCall(DMGenerateRegister("tetgen", DMPlexGenerate_Tetgen, DMPlexRefine_Tetgen, NULL, 2));
#endif
#if defined(PETSC_HAVE_PRAGMATIC)
  PetscCall(DMGenerateRegister("pragmatic", NULL, NULL, DMAdaptMetric_Pragmatic_Plex, -1));
#endif
#if defined(PETSC_HAVE_MMG)
  PetscCall(DMGenerateRegister("mmg", NULL, NULL, DMAdaptMetric_Mmg_Plex, -1));
#endif
#if defined(PETSC_HAVE_PARMMG)
  PetscCall(DMGenerateRegister("parmmg", NULL, NULL, DMAdaptMetric_ParMmg_Plex, -1));
#endif
  PetscCall(DMGenerateRegister("cellrefiner", NULL, NULL, DMPlexTransformAdaptLabel, -1));
  PetscCall(DMGenerateRegister("forest", NULL, NULL, DMAdaptLabel_Forest, -1));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMGenerateRegister -  Adds a grid generator to `DM`

   Not Collective

   Input Parameters:
+  sname - name of a new user-defined grid generator
.  fnc - generator function
.  rfnc - refinement function
.  alfnc - adapt by label function
-  dim - dimension of boundary of domain

   Sample usage:
.vb
   DMGenerateRegister("my_generator",MyGeneratorCreate,MyGeneratorRefiner,MyGeneratorAdaptor,dim);
.ve

   Then, your generator can be chosen with the procedural interface via
$     DMGenerate(dm,"my_generator",...)
   or at runtime via the option
$     -dm_generator my_generator

   Level: advanced

   Note:
   `DMGenerateRegister()` may be called multiple times to add several user-defined generators

.seealso: `DM`, `DMGenerateRegisterAll()`, `DMPlexGenerate()`, `DMGenerateRegisterDestroy()`
@*/
PetscErrorCode DMGenerateRegister(const char sname[], PetscErrorCode (*fnc)(DM, PetscBool, DM *), PetscErrorCode (*rfnc)(DM, PetscReal *, DM *), PetscErrorCode (*alfnc)(DM, Vec, DMLabel, DMLabel, DM *), PetscInt dim)
{
  DMGeneratorFunctionList entry;

  PetscFunctionBegin;
  PetscCall(PetscNew(&entry));
  PetscCall(PetscStrallocpy(sname, &entry->name));
  entry->generate = fnc;
  entry->refine   = rfnc;
  entry->adapt    = alfnc;
  entry->dim      = dim;
  entry->next     = NULL;
  if (!DMGenerateList) DMGenerateList = entry;
  else {
    DMGeneratorFunctionList fl = DMGenerateList;
    while (fl->next) fl = fl->next;
    fl->next = entry;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

extern PetscBool DMGenerateRegisterAllCalled;

PetscErrorCode DMGenerateRegisterDestroy(void)
{
  DMGeneratorFunctionList next, fl;

  PetscFunctionBegin;
  next = fl = DMGenerateList;
  while (next) {
    next = fl ? fl->next : NULL;
    if (fl) PetscCall(PetscFree(fl->name));
    PetscCall(PetscFree(fl));
    fl = next;
  }
  DMGenerateList              = NULL;
  DMGenerateRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMAdaptLabel - Adapt a `DM` based on a `DMLabel` with values interpreted as coarsening and refining flags.  Specific implementations of `DM` maybe have
                 specialized flags, but all implementations should accept flag values `DM_ADAPT_DETERMINE`, `DM_ADAPT_KEEP`, `DM_ADAPT_REFINE`, and,
                 `DM_ADAPT_COARSEN`.

  Collective

  Input parameters:
+ dm - the pre-adaptation `DM` object
- label - label with the flags

  Output parameters:
. dmAdapt - the adapted `DM` object: may be `NULL` if an adapted `DM` could not be produced.

  Level: intermediate

.seealso: `DM`, `DMAdaptMetric()`, `DMCoarsen()`, `DMRefine()`
@*/
PetscErrorCode DMAdaptLabel(DM dm, DMLabel label, DM *dmAdapt)
{
  DMGeneratorFunctionList fl;
  char                    adaptname[PETSC_MAX_PATH_LEN];
  const char             *name;
  PetscInt                dim;
  PetscBool               flg, isForest, found = PETSC_FALSE;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  if (label) PetscValidPointer(label, 2);
  PetscValidPointer(dmAdapt, 3);
  *dmAdapt = NULL;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMIsForest(dm, &isForest));
  name = isForest ? "forest" : "cellrefiner";
  PetscCall(PetscOptionsGetString(((PetscObject)dm)->options, ((PetscObject)dm)->prefix, "-dm_adaptor", adaptname, sizeof(adaptname), &flg));
  if (flg) name = adaptname;

  fl = DMGenerateList;
  while (fl) {
    PetscCall(PetscStrcmp(fl->name, name, &flg));
    if (flg) {
      PetscCall((*fl->adapt)(dm, NULL, label, NULL, dmAdapt));
      found = PETSC_TRUE;
    }
    fl = fl->next;
  }
  PetscCheck(found, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Grid adaptor %s not registered; you may need to add --download-%s to your ./configure options", name, name);
  if (*dmAdapt) {
    (*dmAdapt)->prealloc_only = dm->prealloc_only; /* maybe this should go .... */
    PetscCall(PetscFree((*dmAdapt)->vectype));
    PetscCall(PetscStrallocpy(dm->vectype, (char **)&(*dmAdapt)->vectype));
    PetscCall(PetscFree((*dmAdapt)->mattype));
    PetscCall(PetscStrallocpy(dm->mattype, (char **)&(*dmAdapt)->mattype));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMAdaptMetric - Generates a mesh adapted to the specified metric field.

  Input Parameters:
+ dm - The DM object
. metric - The metric to which the mesh is adapted, defined vertex-wise.
. bdLabel - Label for boundary tags, which will be preserved in the output mesh. bdLabel should be NULL if there is no such label, and should be different from "_boundary_".
- rgLabel - Label for cell tags, which will be preserved in the output mesh. rgLabel should be NULL if there is no such label, and should be different from "_regions_".

  Output Parameter:
. dmAdapt  - Pointer to the DM object containing the adapted mesh

  Note: The label in the adapted mesh will be registered under the name of the input DMLabel object

  Level: advanced

.seealso: `DMAdaptLabel()`, `DMCoarsen()`, `DMRefine()`
@*/
PetscErrorCode DMAdaptMetric(DM dm, Vec metric, DMLabel bdLabel, DMLabel rgLabel, DM *dmAdapt)
{
  DMGeneratorFunctionList fl;
  char                    adaptname[PETSC_MAX_PATH_LEN];
  const char             *name;
  const char *const       adaptors[3] = {"pragmatic", "mmg", "parmmg"};
  PetscInt                dim;
  PetscBool               flg, found = PETSC_FALSE;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidHeaderSpecific(metric, VEC_CLASSID, 2);
  if (bdLabel) PetscValidPointer(bdLabel, 3);
  if (rgLabel) PetscValidPointer(rgLabel, 4);
  PetscValidPointer(dmAdapt, 5);
  *dmAdapt = NULL;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(PetscOptionsGetString(((PetscObject)dm)->options, ((PetscObject)dm)->prefix, "-dm_adaptor", adaptname, sizeof(adaptname), &flg));

  /* Default to Mmg in serial and ParMmg in parallel */
  if (flg) name = adaptname;
  else {
    MPI_Comm    comm;
    PetscMPIInt size;

    PetscCall(PetscObjectGetComm((PetscObject)dm, &comm));
    PetscCallMPI(MPI_Comm_size(comm, &size));
    if (size == 1) name = adaptors[1];
    else name = adaptors[2];
  }

  fl = DMGenerateList;
  while (fl) {
    PetscCall(PetscStrcmp(fl->name, name, &flg));
    if (flg) {
      PetscCall((*fl->adapt)(dm, metric, bdLabel, rgLabel, dmAdapt));
      found = PETSC_TRUE;
    }
    fl = fl->next;
  }
  PetscCheck(found, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Grid adaptor %s not registered; you may need to add --download-%s to your ./configure options", name, name);
  if (*dmAdapt) {
    (*dmAdapt)->prealloc_only = dm->prealloc_only; /* maybe this should go .... */
    PetscCall(PetscFree((*dmAdapt)->vectype));
    PetscCall(PetscStrallocpy(dm->vectype, (char **)&(*dmAdapt)->vectype));
    PetscCall(PetscFree((*dmAdapt)->mattype));
    PetscCall(PetscStrallocpy(dm->mattype, (char **)&(*dmAdapt)->mattype));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

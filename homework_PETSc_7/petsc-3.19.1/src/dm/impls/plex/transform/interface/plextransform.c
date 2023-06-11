#include <petsc/private/dmplextransformimpl.h> /*I "petscdmplextransform.h" I*/

#include <petsc/private/petscfeimpl.h> /* For PetscFEInterpolate_Static() */

PetscClassId DMPLEXTRANSFORM_CLASSID;

PetscFunctionList DMPlexTransformList              = NULL;
PetscBool         DMPlexTransformRegisterAllCalled = PETSC_FALSE;

/* Construct cell type order since we must loop over cell types in depth order */
static PetscErrorCode DMPlexCreateCellTypeOrder_Internal(PetscInt dim, PetscInt *ctOrder[], PetscInt *ctOrderInv[])
{
  PetscInt *ctO, *ctOInv;
  PetscInt  c, d, off = 0;

  PetscFunctionBegin;
  PetscCall(PetscCalloc2(DM_NUM_POLYTOPES + 1, &ctO, DM_NUM_POLYTOPES + 1, &ctOInv));
  for (d = 3; d >= dim; --d) {
    for (c = 0; c <= DM_NUM_POLYTOPES; ++c) {
      if (DMPolytopeTypeGetDim((DMPolytopeType)c) != d) continue;
      ctO[off++] = c;
    }
  }
  if (dim != 0) {
    for (c = 0; c <= DM_NUM_POLYTOPES; ++c) {
      if (DMPolytopeTypeGetDim((DMPolytopeType)c) != 0) continue;
      ctO[off++] = c;
    }
  }
  for (d = dim - 1; d > 0; --d) {
    for (c = 0; c <= DM_NUM_POLYTOPES; ++c) {
      if (DMPolytopeTypeGetDim((DMPolytopeType)c) != d) continue;
      ctO[off++] = c;
    }
  }
  for (c = 0; c <= DM_NUM_POLYTOPES; ++c) {
    if (DMPolytopeTypeGetDim((DMPolytopeType)c) >= 0) continue;
    ctO[off++] = c;
  }
  PetscCheck(off == DM_NUM_POLYTOPES + 1, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Invalid offset %" PetscInt_FMT " for cell type order", off);
  for (c = 0; c <= DM_NUM_POLYTOPES; ++c) ctOInv[ctO[c]] = c;
  *ctOrder    = ctO;
  *ctOrderInv = ctOInv;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexTransformRegister - Adds a new transform component implementation

  Not Collective

  Input Parameters:
+ name        - The name of a new user-defined creation routine
- create_func - The creation routine

  Sample usage:
.vb
  DMPlexTransformRegister("my_transform", MyTransformCreate);
.ve

  Then, your transform type can be chosen with the procedural interface via
.vb
  DMPlexTransformCreate(MPI_Comm, DMPlexTransform *);
  DMPlexTransformSetType(DMPlexTransform, "my_transform");
.ve
  or at runtime via the option
.vb
  -dm_plex_transform_type my_transform
.ve

  Level: advanced

  Note:
  `DMPlexTransformRegister()` may be called multiple times to add several user-defined transforms

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPlexTransformRegisterAll()`, `DMPlexTransformRegisterDestroy()`
@*/
PetscErrorCode DMPlexTransformRegister(const char name[], PetscErrorCode (*create_func)(DMPlexTransform))
{
  PetscFunctionBegin;
  PetscCall(DMInitializePackage());
  PetscCall(PetscFunctionListAdd(&DMPlexTransformList, name, create_func));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN PetscErrorCode DMPlexTransformCreate_Filter(DMPlexTransform);
PETSC_EXTERN PetscErrorCode DMPlexTransformCreate_Regular(DMPlexTransform);
PETSC_EXTERN PetscErrorCode DMPlexTransformCreate_ToBox(DMPlexTransform);
PETSC_EXTERN PetscErrorCode DMPlexTransformCreate_Alfeld(DMPlexTransform);
PETSC_EXTERN PetscErrorCode DMPlexTransformCreate_SBR(DMPlexTransform);
PETSC_EXTERN PetscErrorCode DMPlexTransformCreate_BL(DMPlexTransform);
PETSC_EXTERN PetscErrorCode DMPlexTransformCreate_1D(DMPlexTransform);
PETSC_EXTERN PetscErrorCode DMPlexTransformCreate_Extrude(DMPlexTransform);

/*@C
  DMPlexTransformRegisterAll - Registers all of the transform components in the `DM` package.

  Not Collective

  Level: advanced

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransformType`, `DMRegisterAll()`, `DMPlexTransformRegisterDestroy()`
@*/
PetscErrorCode DMPlexTransformRegisterAll(void)
{
  PetscFunctionBegin;
  if (DMPlexTransformRegisterAllCalled) PetscFunctionReturn(PETSC_SUCCESS);
  DMPlexTransformRegisterAllCalled = PETSC_TRUE;

  PetscCall(DMPlexTransformRegister(DMPLEXTRANSFORMFILTER, DMPlexTransformCreate_Filter));
  PetscCall(DMPlexTransformRegister(DMPLEXREFINEREGULAR, DMPlexTransformCreate_Regular));
  PetscCall(DMPlexTransformRegister(DMPLEXREFINETOBOX, DMPlexTransformCreate_ToBox));
  PetscCall(DMPlexTransformRegister(DMPLEXREFINEALFELD, DMPlexTransformCreate_Alfeld));
  PetscCall(DMPlexTransformRegister(DMPLEXREFINEBOUNDARYLAYER, DMPlexTransformCreate_BL));
  PetscCall(DMPlexTransformRegister(DMPLEXREFINESBR, DMPlexTransformCreate_SBR));
  PetscCall(DMPlexTransformRegister(DMPLEXREFINE1D, DMPlexTransformCreate_1D));
  PetscCall(DMPlexTransformRegister(DMPLEXEXTRUDE, DMPlexTransformCreate_Extrude));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexTransformRegisterDestroy - This function destroys the registered `DMPlexTransformType`. It is called from `PetscFinalize()`.

  Level: developer

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMRegisterAll()`, `DMPlexTransformType`, `PetscInitialize()`
@*/
PetscErrorCode DMPlexTransformRegisterDestroy(void)
{
  PetscFunctionBegin;
  PetscCall(PetscFunctionListDestroy(&DMPlexTransformList));
  DMPlexTransformRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexTransformCreate - Creates an empty transform object. The type can then be set with `DMPlexTransformSetType()`.

  Collective

  Input Parameter:
. comm - The communicator for the transform object

  Output Parameter:
. dm - The transform object

  Level: beginner

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPlexTransformType`, `DMPlexTransformSetType()`, `DMPLEXREFINEREGULAR`, `DMPLEXTRANSFORMFILTER`
@*/
PetscErrorCode DMPlexTransformCreate(MPI_Comm comm, DMPlexTransform *tr)
{
  DMPlexTransform t;

  PetscFunctionBegin;
  PetscValidPointer(tr, 2);
  *tr = NULL;
  PetscCall(DMInitializePackage());

  PetscCall(PetscHeaderCreate(t, DMPLEXTRANSFORM_CLASSID, "DMPlexTransform", "Mesh Transform", "DMPlexTransform", comm, DMPlexTransformDestroy, DMPlexTransformView));
  t->setupcalled = PETSC_FALSE;
  PetscCall(PetscCalloc2(DM_NUM_POLYTOPES, &t->coordFE, DM_NUM_POLYTOPES, &t->refGeom));
  *tr = t;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexTransformSetType - Sets the particular implementation for a transform.

  Collective

  Input Parameters:
+ tr     - The transform
- method - The name of the transform type

  Options Database Key:
. -dm_plex_transform_type <type> - Sets the transform type; see `DMPlexTransformType`

  Level: intermediate

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPlexTransformType`, `DMPlexTransformGetType()`, `DMPlexTransformCreate()`
@*/
PetscErrorCode DMPlexTransformSetType(DMPlexTransform tr, DMPlexTransformType method)
{
  PetscErrorCode (*r)(DMPlexTransform);
  PetscBool match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)tr, method, &match));
  if (match) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCall(DMPlexTransformRegisterAll());
  PetscCall(PetscFunctionListFind(DMPlexTransformList, method, &r));
  PetscCheck(r, PetscObjectComm((PetscObject)tr), PETSC_ERR_ARG_UNKNOWN_TYPE, "Unknown DMPlexTransform type: %s", method);

  PetscTryTypeMethod(tr, destroy);
  PetscCall(PetscMemzero(tr->ops, sizeof(*tr->ops)));
  PetscCall(PetscObjectChangeTypeName((PetscObject)tr, method));
  PetscCall((*r)(tr));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexTransformGetType - Gets the type name (as a string) from the transform.

  Not Collective

  Input Parameter:
. tr  - The `DMPlexTransform`

  Output Parameter:
. type - The `DMPlexTransformType` name

  Level: intermediate

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPlexTransformType`, `DMPlexTransformSetType()`, `DMPlexTransformCreate()`
@*/
PetscErrorCode DMPlexTransformGetType(DMPlexTransform tr, DMPlexTransformType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscValidPointer(type, 2);
  PetscCall(DMPlexTransformRegisterAll());
  *type = ((PetscObject)tr)->type_name;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformView_Ascii(DMPlexTransform tr, PetscViewer v)
{
  PetscViewerFormat format;

  PetscFunctionBegin;
  PetscCall(PetscViewerGetFormat(v, &format));
  if (format == PETSC_VIEWER_ASCII_INFO_DETAIL) {
    const PetscInt *trTypes = NULL;
    IS              trIS;
    PetscInt        cols = 8;
    PetscInt        Nrt  = 8, f, g;

    if (tr->trType) PetscCall(DMLabelView(tr->trType, v));
    PetscCall(PetscViewerASCIIPrintf(v, "Source Starts\n"));
    for (g = 0; g <= cols; ++g) PetscCall(PetscViewerASCIIPrintf(v, " %14s", DMPolytopeTypes[g]));
    PetscCall(PetscViewerASCIIPrintf(v, "\n"));
    for (f = 0; f <= cols; ++f) PetscCall(PetscViewerASCIIPrintf(v, " %14" PetscInt_FMT, tr->ctStart[f]));
    PetscCall(PetscViewerASCIIPrintf(v, "\n"));
    PetscCall(PetscViewerASCIIPrintf(v, "Target Starts\n"));
    for (g = 0; g <= cols; ++g) PetscCall(PetscViewerASCIIPrintf(v, " %14s", DMPolytopeTypes[g]));
    PetscCall(PetscViewerASCIIPrintf(v, "\n"));
    for (f = 0; f <= cols; ++f) PetscCall(PetscViewerASCIIPrintf(v, " %14" PetscInt_FMT, tr->ctStartNew[f]));
    PetscCall(PetscViewerASCIIPrintf(v, "\n"));

    if (tr->trType) {
      PetscCall(DMLabelGetNumValues(tr->trType, &Nrt));
      PetscCall(DMLabelGetValueIS(tr->trType, &trIS));
      PetscCall(ISGetIndices(trIS, &trTypes));
    }
    PetscCall(PetscViewerASCIIPrintf(v, "Offsets\n"));
    PetscCall(PetscViewerASCIIPrintf(v, "     "));
    for (g = 0; g < cols; ++g) PetscCall(PetscViewerASCIIPrintf(v, " %14s", DMPolytopeTypes[g]));
    PetscCall(PetscViewerASCIIPrintf(v, "\n"));
    for (f = 0; f < Nrt; ++f) {
      PetscCall(PetscViewerASCIIPrintf(v, "%2" PetscInt_FMT "  |", trTypes ? trTypes[f] : f));
      for (g = 0; g < cols; ++g) PetscCall(PetscViewerASCIIPrintf(v, " %14" PetscInt_FMT, tr->offset[f * DM_NUM_POLYTOPES + g]));
      PetscCall(PetscViewerASCIIPrintf(v, " |\n"));
    }
    if (tr->trType) {
      PetscCall(ISRestoreIndices(trIS, &trTypes));
      PetscCall(ISDestroy(&trIS));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexTransformView - Views a `DMPlexTransform`

  Collective

  Input Parameters:
+ tr - the `DMPlexTransform` object to view
- v  - the viewer

  Level: beginner

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPlexTransformType`, `PetscViewer`, `DMPlexTransformDestroy()`, `DMPlexTransformCreate()`
@*/
PetscErrorCode DMPlexTransformView(DMPlexTransform tr, PetscViewer v)
{
  PetscBool isascii;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  if (!v) PetscCall(PetscViewerASCIIGetStdout(PetscObjectComm((PetscObject)tr), &v));
  PetscValidHeaderSpecific(v, PETSC_VIEWER_CLASSID, 2);
  PetscCheckSameComm(tr, 1, v, 2);
  PetscCall(PetscViewerCheckWritable(v));
  PetscCall(PetscObjectPrintClassNamePrefixType((PetscObject)tr, v));
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERASCII, &isascii));
  if (isascii) PetscCall(DMPlexTransformView_Ascii(tr, v));
  PetscTryTypeMethod(tr, view, v);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexTransformSetFromOptions - Sets parameters in a transform from values in the options database

  Collective

  Input Parameter:
. tr - the `DMPlexTransform` object to set options for

  Options Database Key:
. -dm_plex_transform_type - Set the transform type, e.g. refine_regular

  Level: intermediate

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPlexTransformView()`, `DMPlexTransformCreate()`
@*/
PetscErrorCode DMPlexTransformSetFromOptions(DMPlexTransform tr)
{
  char        typeName[1024];
  const char *defName = DMPLEXREFINEREGULAR;
  PetscBool   flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscObjectOptionsBegin((PetscObject)tr);
  PetscCall(PetscOptionsFList("-dm_plex_transform_type", "DMPlexTransform", "DMPlexTransformSetType", DMPlexTransformList, defName, typeName, 1024, &flg));
  if (flg) PetscCall(DMPlexTransformSetType(tr, typeName));
  else if (!((PetscObject)tr)->type_name) PetscCall(DMPlexTransformSetType(tr, defName));
  PetscTryTypeMethod(tr, setfromoptions, PetscOptionsObject);
  /* process any options handlers added with PetscObjectAddOptionsHandler() */
  PetscCall(PetscObjectProcessOptionsHandlers((PetscObject)tr, PetscOptionsObject));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexTransformDestroy - Destroys a `DMPlexTransform`

  Collective

  Input Parameter:
. tr - the transform object to destroy

  Level: beginner

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPlexTransformView()`, `DMPlexTransformCreate()`
@*/
PetscErrorCode DMPlexTransformDestroy(DMPlexTransform *tr)
{
  PetscInt c;

  PetscFunctionBegin;
  if (!*tr) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific((*tr), DMPLEXTRANSFORM_CLASSID, 1);
  if (--((PetscObject)(*tr))->refct > 0) {
    *tr = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  if ((*tr)->ops->destroy) PetscCall((*(*tr)->ops->destroy)(*tr));
  PetscCall(DMDestroy(&(*tr)->dm));
  PetscCall(DMLabelDestroy(&(*tr)->active));
  PetscCall(DMLabelDestroy(&(*tr)->trType));
  PetscCall(PetscFree2((*tr)->ctOrderOld, (*tr)->ctOrderInvOld));
  PetscCall(PetscFree2((*tr)->ctOrderNew, (*tr)->ctOrderInvNew));
  PetscCall(PetscFree2((*tr)->ctStart, (*tr)->ctStartNew));
  PetscCall(PetscFree((*tr)->offset));
  PetscCall(PetscFree2((*tr)->depthStart, (*tr)->depthEnd));
  for (c = 0; c < DM_NUM_POLYTOPES; ++c) {
    PetscCall(PetscFEDestroy(&(*tr)->coordFE[c]));
    PetscCall(PetscFEGeomDestroy(&(*tr)->refGeom[c]));
  }
  if ((*tr)->trVerts) {
    for (c = 0; c < DM_NUM_POLYTOPES; ++c) {
      DMPolytopeType *rct;
      PetscInt       *rsize, *rcone, *rornt, Nct, n, r;

      if (DMPolytopeTypeGetDim((DMPolytopeType)c) > 0) {
        PetscCall(DMPlexTransformCellTransform((*tr), (DMPolytopeType)c, 0, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
        for (n = 0; n < Nct; ++n) {
          if (rct[n] == DM_POLYTOPE_POINT) continue;
          for (r = 0; r < rsize[n]; ++r) PetscCall(PetscFree((*tr)->trSubVerts[c][rct[n]][r]));
          PetscCall(PetscFree((*tr)->trSubVerts[c][rct[n]]));
        }
      }
      PetscCall(PetscFree((*tr)->trSubVerts[c]));
      PetscCall(PetscFree((*tr)->trVerts[c]));
    }
  }
  PetscCall(PetscFree3((*tr)->trNv, (*tr)->trVerts, (*tr)->trSubVerts));
  PetscCall(PetscFree2((*tr)->coordFE, (*tr)->refGeom));
  /* We do not destroy (*dm)->data here so that we can reference count backend objects */
  PetscCall(PetscHeaderDestroy(tr));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformCreateOffset_Internal(DMPlexTransform tr, PetscInt ctOrderOld[], PetscInt ctStart[], PetscInt **offset)
{
  DMLabel  trType = tr->trType;
  PetscInt c, cN, *off;

  PetscFunctionBegin;
  if (trType) {
    DM              dm;
    IS              rtIS;
    const PetscInt *reftypes;
    PetscInt        Nrt, r;

    PetscCall(DMPlexTransformGetDM(tr, &dm));
    PetscCall(DMLabelGetNumValues(trType, &Nrt));
    PetscCall(DMLabelGetValueIS(trType, &rtIS));
    PetscCall(ISGetIndices(rtIS, &reftypes));
    PetscCall(PetscCalloc1(Nrt * DM_NUM_POLYTOPES, &off));
    for (r = 0; r < Nrt; ++r) {
      const PetscInt  rt = reftypes[r];
      IS              rtIS;
      const PetscInt *points;
      DMPolytopeType  ct;
      PetscInt        np, p;

      PetscCall(DMLabelGetStratumIS(trType, rt, &rtIS));
      PetscCall(ISGetLocalSize(rtIS, &np));
      PetscCall(ISGetIndices(rtIS, &points));
      if (!np) continue;
      p = points[0];
      PetscCall(ISRestoreIndices(rtIS, &points));
      PetscCall(ISDestroy(&rtIS));
      PetscCall(DMPlexGetCellType(dm, p, &ct));
      for (cN = DM_POLYTOPE_POINT; cN < DM_NUM_POLYTOPES; ++cN) {
        const DMPolytopeType ctNew = (DMPolytopeType)cN;
        DMPolytopeType      *rct;
        PetscInt            *rsize, *cone, *ornt;
        PetscInt             Nct, n, s;

        if (DMPolytopeTypeGetDim(ct) < 0 || DMPolytopeTypeGetDim(ctNew) < 0) {
          off[r * DM_NUM_POLYTOPES + ctNew] = -1;
          break;
        }
        off[r * DM_NUM_POLYTOPES + ctNew] = 0;
        for (s = 0; s <= r; ++s) {
          const PetscInt st = reftypes[s];
          DMPolytopeType sct;
          PetscInt       q, qrt;

          PetscCall(DMLabelGetStratumIS(trType, st, &rtIS));
          PetscCall(ISGetLocalSize(rtIS, &np));
          PetscCall(ISGetIndices(rtIS, &points));
          if (!np) continue;
          q = points[0];
          PetscCall(ISRestoreIndices(rtIS, &points));
          PetscCall(ISDestroy(&rtIS));
          PetscCall(DMPlexGetCellType(dm, q, &sct));
          PetscCall(DMPlexTransformCellTransform(tr, sct, q, &qrt, &Nct, &rct, &rsize, &cone, &ornt));
          PetscCheck(st == qrt, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Refine type %" PetscInt_FMT " of point %" PetscInt_FMT " does not match predicted type %" PetscInt_FMT, qrt, q, st);
          if (st == rt) {
            for (n = 0; n < Nct; ++n)
              if (rct[n] == ctNew) break;
            if (n == Nct) off[r * DM_NUM_POLYTOPES + ctNew] = -1;
            break;
          }
          for (n = 0; n < Nct; ++n) {
            if (rct[n] == ctNew) {
              PetscInt sn;

              PetscCall(DMLabelGetStratumSize(trType, st, &sn));
              off[r * DM_NUM_POLYTOPES + ctNew] += sn * rsize[n];
            }
          }
        }
      }
    }
    PetscCall(ISRestoreIndices(rtIS, &reftypes));
    PetscCall(ISDestroy(&rtIS));
  } else {
    PetscCall(PetscCalloc1(DM_NUM_POLYTOPES * DM_NUM_POLYTOPES, &off));
    for (c = DM_POLYTOPE_POINT; c < DM_NUM_POLYTOPES; ++c) {
      const DMPolytopeType ct = (DMPolytopeType)c;
      for (cN = DM_POLYTOPE_POINT; cN < DM_NUM_POLYTOPES; ++cN) {
        const DMPolytopeType ctNew = (DMPolytopeType)cN;
        DMPolytopeType      *rct;
        PetscInt            *rsize, *cone, *ornt;
        PetscInt             Nct, n, i;

        if (DMPolytopeTypeGetDim(ct) < 0 || DMPolytopeTypeGetDim(ctNew) < 0) {
          off[ct * DM_NUM_POLYTOPES + ctNew] = -1;
          continue;
        }
        off[ct * DM_NUM_POLYTOPES + ctNew] = 0;
        for (i = DM_POLYTOPE_POINT; i < DM_NUM_POLYTOPES; ++i) {
          const DMPolytopeType ict  = (DMPolytopeType)ctOrderOld[i];
          const DMPolytopeType ictn = (DMPolytopeType)ctOrderOld[i + 1];

          PetscCall(DMPlexTransformCellTransform(tr, ict, PETSC_DETERMINE, NULL, &Nct, &rct, &rsize, &cone, &ornt));
          if (ict == ct) {
            for (n = 0; n < Nct; ++n)
              if (rct[n] == ctNew) break;
            if (n == Nct) off[ct * DM_NUM_POLYTOPES + ctNew] = -1;
            break;
          }
          for (n = 0; n < Nct; ++n)
            if (rct[n] == ctNew) off[ct * DM_NUM_POLYTOPES + ctNew] += (ctStart[ictn] - ctStart[ict]) * rsize[n];
        }
      }
    }
  }
  *offset = off;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformSetUp(DMPlexTransform tr)
{
  DM             dm;
  DMPolytopeType ctCell;
  PetscInt       pStart, pEnd, p, c, celldim = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  if (tr->setupcalled) PetscFunctionReturn(PETSC_SUCCESS);
  PetscTryTypeMethod(tr, setup);
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  if (pEnd > pStart) {
    PetscCall(DMPlexGetCellType(dm, 0, &ctCell));
  } else {
    PetscInt dim;

    PetscCall(DMGetDimension(dm, &dim));
    switch (dim) {
    case 0:
      ctCell = DM_POLYTOPE_POINT;
      break;
    case 1:
      ctCell = DM_POLYTOPE_SEGMENT;
      break;
    case 2:
      ctCell = DM_POLYTOPE_TRIANGLE;
      break;
    case 3:
      ctCell = DM_POLYTOPE_TETRAHEDRON;
      break;
    default:
      ctCell = DM_POLYTOPE_UNKNOWN;
    }
  }
  PetscCall(DMPlexCreateCellTypeOrder_Internal(DMPolytopeTypeGetDim(ctCell), &tr->ctOrderOld, &tr->ctOrderInvOld));
  for (p = pStart; p < pEnd; ++p) {
    DMPolytopeType  ct;
    DMPolytopeType *rct;
    PetscInt       *rsize, *cone, *ornt;
    PetscInt        Nct, n;

    PetscCall(DMPlexGetCellType(dm, p, &ct));
    PetscCheck(ct != DM_POLYTOPE_UNKNOWN, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "No cell type for point %" PetscInt_FMT, p);
    PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &cone, &ornt));
    for (n = 0; n < Nct; ++n) celldim = PetscMax(celldim, DMPolytopeTypeGetDim(rct[n]));
  }
  PetscCall(DMPlexCreateCellTypeOrder_Internal(celldim, &tr->ctOrderNew, &tr->ctOrderInvNew));
  /* Construct sizes and offsets for each cell type */
  if (!tr->ctStart) {
    PetscInt *ctS, *ctSN, *ctC, *ctCN;

    PetscCall(PetscCalloc2(DM_NUM_POLYTOPES + 1, &ctS, DM_NUM_POLYTOPES + 1, &ctSN));
    PetscCall(PetscCalloc2(DM_NUM_POLYTOPES + 1, &ctC, DM_NUM_POLYTOPES + 1, &ctCN));
    for (p = pStart; p < pEnd; ++p) {
      DMPolytopeType  ct;
      DMPolytopeType *rct;
      PetscInt       *rsize, *cone, *ornt;
      PetscInt        Nct, n;

      PetscCall(DMPlexGetCellType(dm, p, &ct));
      PetscCheck(ct != DM_POLYTOPE_UNKNOWN, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "No cell type for point %" PetscInt_FMT, p);
      ++ctC[ct];
      PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &cone, &ornt));
      for (n = 0; n < Nct; ++n) ctCN[rct[n]] += rsize[n];
    }
    for (c = 0; c < DM_NUM_POLYTOPES; ++c) {
      const PetscInt cto  = tr->ctOrderOld[c];
      const PetscInt cton = tr->ctOrderOld[c + 1];
      const PetscInt ctn  = tr->ctOrderNew[c];
      const PetscInt ctnn = tr->ctOrderNew[c + 1];

      ctS[cton]  = ctS[cto] + ctC[cto];
      ctSN[ctnn] = ctSN[ctn] + ctCN[ctn];
    }
    PetscCall(PetscFree2(ctC, ctCN));
    tr->ctStart    = ctS;
    tr->ctStartNew = ctSN;
  }
  PetscCall(DMPlexTransformCreateOffset_Internal(tr, tr->ctOrderOld, tr->ctStart, &tr->offset));
  // Compute depth information
  tr->depth = -1;
  for (c = 0; c < DM_NUM_POLYTOPES; ++c)
    if (tr->ctStartNew[tr->ctOrderNew[c + 1]] > tr->ctStartNew[tr->ctOrderNew[c]]) tr->depth = PetscMax(tr->depth, DMPolytopeTypeGetDim((DMPolytopeType)tr->ctOrderNew[c]));
  PetscCall(PetscMalloc2(tr->depth + 1, &tr->depthStart, tr->depth + 1, &tr->depthEnd));
  for (PetscInt d = 0; d <= tr->depth; ++d) {
    tr->depthStart[d] = PETSC_MAX_INT;
    tr->depthEnd[d]   = -1;
  }
  for (c = 0; c < DM_NUM_POLYTOPES; ++c) {
    const PetscInt dep = DMPolytopeTypeGetDim((DMPolytopeType)tr->ctOrderNew[c]);

    if (tr->ctStartNew[tr->ctOrderNew[c + 1]] <= tr->ctStartNew[tr->ctOrderNew[c]]) continue;
    tr->depthStart[dep] = PetscMin(tr->depthStart[dep], tr->ctStartNew[tr->ctOrderNew[c]]);
    tr->depthEnd[dep]   = PetscMax(tr->depthEnd[dep], tr->ctStartNew[tr->ctOrderNew[c + 1]]);
  }
  tr->setupcalled = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetDM(DMPlexTransform tr, DM *dm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscValidPointer(dm, 2);
  *dm = tr->dm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformSetDM(DMPlexTransform tr, DM dm)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscValidHeaderSpecific(dm, DM_CLASSID, 2);
  PetscCall(PetscObjectReference((PetscObject)dm));
  PetscCall(DMDestroy(&tr->dm));
  tr->dm = dm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetActive(DMPlexTransform tr, DMLabel *active)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscValidPointer(active, 2);
  *active = tr->active;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformSetActive(DMPlexTransform tr, DMLabel active)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  if (active) PetscValidHeaderSpecific(active, DMLABEL_CLASSID, 2);
  PetscCall(PetscObjectReference((PetscObject)active));
  PetscCall(DMLabelDestroy(&tr->active));
  tr->active = active;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformGetCoordinateFE(DMPlexTransform tr, DMPolytopeType ct, PetscFE *fe)
{
  PetscFunctionBegin;
  if (!tr->coordFE[ct]) {
    PetscInt dim, cdim;

    dim = DMPolytopeTypeGetDim(ct);
    PetscCall(DMGetCoordinateDim(tr->dm, &cdim));
    PetscCall(PetscFECreateLagrangeByCell(PETSC_COMM_SELF, dim, cdim, ct, 1, PETSC_DETERMINE, &tr->coordFE[ct]));
    {
      PetscDualSpace  dsp;
      PetscQuadrature quad;
      DM              K;
      PetscFEGeom    *cg;
      PetscScalar    *Xq;
      PetscReal      *xq, *wq;
      PetscInt        Nq, q;

      PetscCall(DMPlexTransformGetCellVertices(tr, ct, &Nq, &Xq));
      PetscCall(PetscMalloc1(Nq * cdim, &xq));
      for (q = 0; q < Nq * cdim; ++q) xq[q] = PetscRealPart(Xq[q]);
      PetscCall(PetscMalloc1(Nq, &wq));
      for (q = 0; q < Nq; ++q) wq[q] = 1.0;
      PetscCall(PetscQuadratureCreate(PETSC_COMM_SELF, &quad));
      PetscCall(PetscQuadratureSetData(quad, dim, 1, Nq, xq, wq));
      PetscCall(PetscFESetQuadrature(tr->coordFE[ct], quad));

      PetscCall(PetscFEGetDualSpace(tr->coordFE[ct], &dsp));
      PetscCall(PetscDualSpaceGetDM(dsp, &K));
      PetscCall(PetscFEGeomCreate(quad, 1, cdim, PETSC_FALSE, &tr->refGeom[ct]));
      cg = tr->refGeom[ct];
      PetscCall(DMPlexComputeCellGeometryFEM(K, 0, NULL, cg->v, cg->J, cg->invJ, cg->detJ));
      PetscCall(PetscQuadratureDestroy(&quad));
    }
  }
  *fe = tr->coordFE[ct];
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformSetDimensions_Internal(DMPlexTransform tr, DM dm, DM tdm)
{
  PetscInt dim, cdim;

  PetscFunctionBegin;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMSetDimension(tdm, dim));
  PetscCall(DMGetCoordinateDim(dm, &cdim));
  PetscCall(DMSetCoordinateDim(tdm, cdim));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexTransformSetDimensions - Set the dimensions for the transformed `DM`

  Input Parameters:
+ tr - The `DMPlexTransform` object
- dm - The original `DM`

  Output Parameter:
. tdm - The transformed `DM`

  Level: advanced

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPlexTransformApply()`, `DMPlexTransformCreate()`
@*/
PetscErrorCode DMPlexTransformSetDimensions(DMPlexTransform tr, DM dm, DM tdm)
{
  PetscFunctionBegin;
  PetscUseTypeMethod(tr, setdimensions, dm, tdm);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetChart(DMPlexTransform tr, PetscInt *pStart, PetscInt *pEnd)
{
  PetscFunctionBegin;
  if (pStart) *pStart = 0;
  if (pEnd) *pEnd = tr->ctStartNew[tr->ctOrderNew[DM_NUM_POLYTOPES]];
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetCellType(DMPlexTransform tr, PetscInt cell, DMPolytopeType *celltype)
{
  PetscInt ctNew;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscValidPointer(celltype, 3);
  /* TODO Can do bisection since everything is sorted */
  for (ctNew = DM_POLYTOPE_POINT; ctNew < DM_NUM_POLYTOPES; ++ctNew) {
    PetscInt ctSN = tr->ctStartNew[ctNew], ctEN = tr->ctStartNew[tr->ctOrderNew[tr->ctOrderInvNew[ctNew] + 1]];

    if (cell >= ctSN && cell < ctEN) break;
  }
  PetscCheck(ctNew < DM_NUM_POLYTOPES, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Point %" PetscInt_FMT " cannot be located in the transformed mesh", cell);
  *celltype = (DMPolytopeType)ctNew;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetDepth(DMPlexTransform tr, PetscInt *depth)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  *depth = tr->depth;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetDepthStratum(DMPlexTransform tr, PetscInt depth, PetscInt *start, PetscInt *end)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  if (start) *start = tr->depthStart[depth];
  if (end) *end = tr->depthEnd[depth];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexTransformGetTargetPoint - Get the number of a point in the transformed mesh based on information from the original mesh.

  Not Collective

  Input Parameters:
+ tr    - The `DMPlexTransform`
. ct    - The type of the original point which produces the new point
. ctNew - The type of the new point
. p     - The original point which produces the new point
- r     - The replica number of the new point, meaning it is the rth point of type `ctNew` produced from `p`

  Output Parameter:
. pNew  - The new point number

  Level: developer

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPolytopeType`, `DMPlexTransformGetSourcePoint()`, `DMPlexTransformCellTransform()`
@*/
PetscErrorCode DMPlexTransformGetTargetPoint(DMPlexTransform tr, DMPolytopeType ct, DMPolytopeType ctNew, PetscInt p, PetscInt r, PetscInt *pNew)
{
  DMPolytopeType *rct;
  PetscInt       *rsize, *cone, *ornt;
  PetscInt        rt, Nct, n, off, rp;
  DMLabel         trType = tr->trType;
  PetscInt        ctS = tr->ctStart[ct], ctE = tr->ctStart[tr->ctOrderOld[tr->ctOrderInvOld[ct] + 1]];
  PetscInt        ctSN = tr->ctStartNew[ctNew], ctEN = tr->ctStartNew[tr->ctOrderNew[tr->ctOrderInvNew[ctNew] + 1]];
  PetscInt        newp = ctSN, cind;

  PetscFunctionBeginHot;
  PetscCheck(!(p < ctS) && !(p >= ctE), PETSC_COMM_SELF, PETSC_ERR_PLIB, "Point %" PetscInt_FMT " is not a %s [%" PetscInt_FMT ", %" PetscInt_FMT ")", p, DMPolytopeTypes[ct], ctS, ctE);
  PetscCall(DMPlexTransformCellTransform(tr, ct, p, &rt, &Nct, &rct, &rsize, &cone, &ornt));
  if (trType) {
    PetscCall(DMLabelGetValueIndex(trType, rt, &cind));
    PetscCall(DMLabelGetStratumPointIndex(trType, rt, p, &rp));
    PetscCheck(rp >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Cell type %s point %" PetscInt_FMT " does not have refine type %" PetscInt_FMT, DMPolytopeTypes[ct], p, rt);
  } else {
    cind = ct;
    rp   = p - ctS;
  }
  off = tr->offset[cind * DM_NUM_POLYTOPES + ctNew];
  PetscCheck(off >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Cell type %s (%" PetscInt_FMT ") of point %" PetscInt_FMT " does not produce type %s for transform %s", DMPolytopeTypes[ct], rt, p, DMPolytopeTypes[ctNew], tr->hdr.type_name);
  newp += off;
  for (n = 0; n < Nct; ++n) {
    if (rct[n] == ctNew) {
      if (rsize[n] && r >= rsize[n])
        SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Replica number %" PetscInt_FMT " should be in [0, %" PetscInt_FMT ") for subcell type %s in cell type %s", r, rsize[n], DMPolytopeTypes[rct[n]], DMPolytopeTypes[ct]);
      newp += rp * rsize[n] + r;
      break;
    }
  }

  PetscCheck(!(newp < ctSN) && !(newp >= ctEN), PETSC_COMM_SELF, PETSC_ERR_PLIB, "New point %" PetscInt_FMT " is not a %s [%" PetscInt_FMT ", %" PetscInt_FMT ")", newp, DMPolytopeTypes[ctNew], ctSN, ctEN);
  *pNew = newp;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexTransformGetSourcePoint - Get the number of a point in the original mesh based on information from the transformed mesh.

  Not Collective

  Input Parameters:
+ tr    - The `DMPlexTransform`
- pNew  - The new point number

  Output Parameters:
+ ct    - The type of the original point which produces the new point
. ctNew - The type of the new point
. p     - The original point which produces the new point
- r     - The replica number of the new point, meaning it is the rth point of type ctNew produced from p

  Level: developer

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPolytopeType`, `DMPlexTransformGetTargetPoint()`, `DMPlexTransformCellTransform()`
@*/
PetscErrorCode DMPlexTransformGetSourcePoint(DMPlexTransform tr, PetscInt pNew, DMPolytopeType *ct, DMPolytopeType *ctNew, PetscInt *p, PetscInt *r)
{
  DMLabel         trType = tr->trType;
  DMPolytopeType *rct, ctN;
  PetscInt       *rsize, *cone, *ornt;
  PetscInt        rt = -1, rtTmp, Nct, n, rp = 0, rO = 0, pO;
  PetscInt        offset = -1, ctS, ctE, ctO = 0, ctTmp, rtS;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetCellType(tr, pNew, &ctN));
  if (trType) {
    DM              dm;
    IS              rtIS;
    const PetscInt *reftypes;
    PetscInt        Nrt, r, rtStart;

    PetscCall(DMPlexTransformGetDM(tr, &dm));
    PetscCall(DMLabelGetNumValues(trType, &Nrt));
    PetscCall(DMLabelGetValueIS(trType, &rtIS));
    PetscCall(ISGetIndices(rtIS, &reftypes));
    for (r = 0; r < Nrt; ++r) {
      const PetscInt off = tr->offset[r * DM_NUM_POLYTOPES + ctN];

      if (tr->ctStartNew[ctN] + off > pNew) continue;
      /* Check that any of this refinement type exist */
      /* TODO Actually keep track of the number produced here instead */
      if (off > offset) {
        rt     = reftypes[r];
        offset = off;
      }
    }
    PetscCall(ISRestoreIndices(rtIS, &reftypes));
    PetscCall(ISDestroy(&rtIS));
    PetscCheck(offset >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Source cell type for target point %" PetscInt_FMT " could be not found", pNew);
    /* TODO Map refinement types to cell types */
    PetscCall(DMLabelGetStratumBounds(trType, rt, &rtStart, NULL));
    PetscCheck(rtStart >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Refinement type %" PetscInt_FMT " has no source points", rt);
    for (ctO = 0; ctO < DM_NUM_POLYTOPES; ++ctO) {
      PetscInt ctS = tr->ctStart[ctO], ctE = tr->ctStart[tr->ctOrderOld[tr->ctOrderInvOld[ctO] + 1]];

      if ((rtStart >= ctS) && (rtStart < ctE)) break;
    }
    PetscCheck(ctO != DM_NUM_POLYTOPES, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Could not determine a cell type for refinement type %" PetscInt_FMT, rt);
  } else {
    for (ctTmp = 0; ctTmp < DM_NUM_POLYTOPES; ++ctTmp) {
      const PetscInt off = tr->offset[ctTmp * DM_NUM_POLYTOPES + ctN];

      if (tr->ctStartNew[ctN] + off > pNew) continue;
      if (tr->ctStart[tr->ctOrderOld[tr->ctOrderInvOld[ctTmp] + 1]] <= tr->ctStart[ctTmp]) continue;
      /* TODO Actually keep track of the number produced here instead */
      if (off > offset) {
        ctO    = ctTmp;
        offset = off;
      }
    }
    rt = -1;
    PetscCheck(offset >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Source cell type for target point %" PetscInt_FMT " could be not found", pNew);
  }
  ctS = tr->ctStart[ctO];
  ctE = tr->ctStart[tr->ctOrderOld[tr->ctOrderInvOld[ctO] + 1]];
  if (trType) {
    for (rtS = ctS; rtS < ctE; ++rtS) {
      PetscInt val;
      PetscCall(DMLabelGetValue(trType, rtS, &val));
      if (val == rt) break;
    }
    PetscCheck(rtS < ctE, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Could not find point of type %s with refine type %" PetscInt_FMT, DMPolytopeTypes[ctO], rt);
  } else rtS = ctS;
  PetscCall(DMPlexTransformCellTransform(tr, (DMPolytopeType)ctO, rtS, &rtTmp, &Nct, &rct, &rsize, &cone, &ornt));
  PetscCheck(!trType || rt == rtTmp, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Point %" PetscInt_FMT " has refine type %" PetscInt_FMT " != %" PetscInt_FMT " refine type which produced point %" PetscInt_FMT, rtS, rtTmp, rt, pNew);
  for (n = 0; n < Nct; ++n) {
    if (rct[n] == ctN) {
      PetscInt tmp = pNew - tr->ctStartNew[ctN] - offset, val, c;

      if (trType) {
        for (c = ctS; c < ctE; ++c) {
          PetscCall(DMLabelGetValue(trType, c, &val));
          if (val == rt) {
            if (tmp < rsize[n]) break;
            tmp -= rsize[n];
          }
        }
        PetscCheck(c < ctE, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Parent point for target point %" PetscInt_FMT " could be not found", pNew);
        rp = c - ctS;
        rO = tmp;
      } else {
        // This assumes that all points of type ctO transform the same way
        rp = tmp / rsize[n];
        rO = tmp % rsize[n];
      }
      break;
    }
  }
  PetscCheck(n != Nct, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Replica number for target point %" PetscInt_FMT " could be not found", pNew);
  pO = rp + ctS;
  PetscCheck(!(pO < ctS) && !(pO >= ctE), PETSC_COMM_SELF, PETSC_ERR_PLIB, "Source point %" PetscInt_FMT " is not a %s [%" PetscInt_FMT ", %" PetscInt_FMT ")", pO, DMPolytopeTypes[ctO], ctS, ctE);
  if (ct) *ct = (DMPolytopeType)ctO;
  if (ctNew) *ctNew = (DMPolytopeType)ctN;
  if (p) *p = pO;
  if (r) *r = rO;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexTransformCellTransform - Describes the transform of a given source cell into a set of other target cells. These produced cells become the new mesh.

  Input Parameters:
+ tr     - The `DMPlexTransform` object
. source - The source cell type
- p      - The source point, which can also determine the refine type

  Output Parameters:
+ rt     - The refine type for this point
. Nt     - The number of types produced by this point
. target - An array of length `Nt` giving the types produced
. size   - An array of length `Nt` giving the number of cells of each type produced
. cone   - An array of length `Nt`*size[t]*coneSize[t] giving the cell type for each point in the cone of each produced point
- ornt   - An array of length `Nt`*size[t]*coneSize[t] giving the orientation for each point in the cone of each produced point

  Level: advanced

  Notes:
  The cone array gives the cone of each subcell listed by the first three outputs. For each cone point, we
  need the cell type, point identifier, and orientation within the subcell. The orientation is with respect to the canonical
  division (described in these outputs) of the cell in the original mesh. The point identifier is given by
.vb
   the number of cones to be taken, or 0 for the current cell
   the cell cone point number at each level from which it is subdivided
   the replica number r of the subdivision.
.ve
The orientation is with respect to the canonical cone orientation. For example, the prescription for edge division is
.vb
   Nt     = 2
   target = {DM_POLYTOPE_POINT, DM_POLYTOPE_SEGMENT}
   size   = {1, 2}
   cone   = {DM_POLYTOPE_POINT, 1, 0, 0, DM_POLYTOPE_POINT, 0, 0,  DM_POLYTOPE_POINT, 0, 0, DM_POLYTOPE_POINT, 1, 1, 0}
   ornt   = {                         0,                       0,                        0,                          0}
.ve

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPolytopeType`, `DMPlexTransformApply()`, `DMPlexTransformCreate()`
@*/
PetscErrorCode DMPlexTransformCellTransform(DMPlexTransform tr, DMPolytopeType source, PetscInt p, PetscInt *rt, PetscInt *Nt, DMPolytopeType *target[], PetscInt *size[], PetscInt *cone[], PetscInt *ornt[])
{
  PetscFunctionBegin;
  PetscUseTypeMethod(tr, celltransform, source, p, rt, Nt, target, size, cone, ornt);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetSubcellOrientationIdentity(DMPlexTransform tr, DMPolytopeType sct, PetscInt sp, PetscInt so, DMPolytopeType tct, PetscInt r, PetscInt o, PetscInt *rnew, PetscInt *onew)
{
  PetscFunctionBegin;
  *rnew = r;
  *onew = DMPolytopeTypeComposeOrientation(tct, o, so);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Returns the same thing */
PetscErrorCode DMPlexTransformCellTransformIdentity(DMPlexTransform tr, DMPolytopeType source, PetscInt p, PetscInt *rt, PetscInt *Nt, DMPolytopeType *target[], PetscInt *size[], PetscInt *cone[], PetscInt *ornt[])
{
  static DMPolytopeType vertexT[] = {DM_POLYTOPE_POINT};
  static PetscInt       vertexS[] = {1};
  static PetscInt       vertexC[] = {0};
  static PetscInt       vertexO[] = {0};
  static DMPolytopeType edgeT[]   = {DM_POLYTOPE_SEGMENT};
  static PetscInt       edgeS[]   = {1};
  static PetscInt       edgeC[]   = {DM_POLYTOPE_POINT, 1, 0, 0, DM_POLYTOPE_POINT, 1, 1, 0};
  static PetscInt       edgeO[]   = {0, 0};
  static DMPolytopeType tedgeT[]  = {DM_POLYTOPE_POINT_PRISM_TENSOR};
  static PetscInt       tedgeS[]  = {1};
  static PetscInt       tedgeC[]  = {DM_POLYTOPE_POINT, 1, 0, 0, DM_POLYTOPE_POINT, 1, 1, 0};
  static PetscInt       tedgeO[]  = {0, 0};
  static DMPolytopeType triT[]    = {DM_POLYTOPE_TRIANGLE};
  static PetscInt       triS[]    = {1};
  static PetscInt       triC[]    = {DM_POLYTOPE_SEGMENT, 1, 0, 0, DM_POLYTOPE_SEGMENT, 1, 1, 0, DM_POLYTOPE_SEGMENT, 1, 2, 0};
  static PetscInt       triO[]    = {0, 0, 0};
  static DMPolytopeType quadT[]   = {DM_POLYTOPE_QUADRILATERAL};
  static PetscInt       quadS[]   = {1};
  static PetscInt       quadC[]   = {DM_POLYTOPE_SEGMENT, 1, 0, 0, DM_POLYTOPE_SEGMENT, 1, 1, 0, DM_POLYTOPE_SEGMENT, 1, 2, 0, DM_POLYTOPE_SEGMENT, 1, 3, 0};
  static PetscInt       quadO[]   = {0, 0, 0, 0};
  static DMPolytopeType tquadT[]  = {DM_POLYTOPE_SEG_PRISM_TENSOR};
  static PetscInt       tquadS[]  = {1};
  static PetscInt       tquadC[]  = {DM_POLYTOPE_SEGMENT, 1, 0, 0, DM_POLYTOPE_SEGMENT, 1, 1, 0, DM_POLYTOPE_POINT_PRISM_TENSOR, 1, 2, 0, DM_POLYTOPE_POINT_PRISM_TENSOR, 1, 3, 0};
  static PetscInt       tquadO[]  = {0, 0, 0, 0};
  static DMPolytopeType tetT[]    = {DM_POLYTOPE_TETRAHEDRON};
  static PetscInt       tetS[]    = {1};
  static PetscInt       tetC[]    = {DM_POLYTOPE_TRIANGLE, 1, 0, 0, DM_POLYTOPE_TRIANGLE, 1, 1, 0, DM_POLYTOPE_TRIANGLE, 1, 2, 0, DM_POLYTOPE_TRIANGLE, 1, 3, 0};
  static PetscInt       tetO[]    = {0, 0, 0, 0};
  static DMPolytopeType hexT[]    = {DM_POLYTOPE_HEXAHEDRON};
  static PetscInt       hexS[]    = {1};
  static PetscInt       hexC[] = {DM_POLYTOPE_QUADRILATERAL, 1, 0, 0, DM_POLYTOPE_QUADRILATERAL, 1, 1, 0, DM_POLYTOPE_QUADRILATERAL, 1, 2, 0, DM_POLYTOPE_QUADRILATERAL, 1, 3, 0, DM_POLYTOPE_QUADRILATERAL, 1, 4, 0, DM_POLYTOPE_QUADRILATERAL, 1, 5, 0};
  static PetscInt       hexO[] = {0, 0, 0, 0, 0, 0};
  static DMPolytopeType tripT[]   = {DM_POLYTOPE_TRI_PRISM};
  static PetscInt       tripS[]   = {1};
  static PetscInt       tripC[]   = {DM_POLYTOPE_TRIANGLE, 1, 0, 0, DM_POLYTOPE_TRIANGLE, 1, 1, 0, DM_POLYTOPE_QUADRILATERAL, 1, 2, 0, DM_POLYTOPE_QUADRILATERAL, 1, 3, 0, DM_POLYTOPE_QUADRILATERAL, 1, 4, 0};
  static PetscInt       tripO[]   = {0, 0, 0, 0, 0};
  static DMPolytopeType ttripT[]  = {DM_POLYTOPE_TRI_PRISM_TENSOR};
  static PetscInt       ttripS[]  = {1};
  static PetscInt       ttripC[]  = {DM_POLYTOPE_TRIANGLE, 1, 0, 0, DM_POLYTOPE_TRIANGLE, 1, 1, 0, DM_POLYTOPE_SEG_PRISM_TENSOR, 1, 2, 0, DM_POLYTOPE_SEG_PRISM_TENSOR, 1, 3, 0, DM_POLYTOPE_SEG_PRISM_TENSOR, 1, 4, 0};
  static PetscInt       ttripO[]  = {0, 0, 0, 0, 0};
  static DMPolytopeType tquadpT[] = {DM_POLYTOPE_QUAD_PRISM_TENSOR};
  static PetscInt       tquadpS[] = {1};
  static PetscInt       tquadpC[] = {DM_POLYTOPE_QUADRILATERAL,    1, 0, 0, DM_POLYTOPE_QUADRILATERAL,    1, 1, 0, DM_POLYTOPE_SEG_PRISM_TENSOR, 1, 2, 0,
                                     DM_POLYTOPE_SEG_PRISM_TENSOR, 1, 3, 0, DM_POLYTOPE_SEG_PRISM_TENSOR, 1, 4, 0, DM_POLYTOPE_SEG_PRISM_TENSOR, 1, 5, 0};
  static PetscInt       tquadpO[] = {0, 0, 0, 0, 0, 0};
  static DMPolytopeType pyrT[]    = {DM_POLYTOPE_PYRAMID};
  static PetscInt       pyrS[]    = {1};
  static PetscInt       pyrC[]    = {DM_POLYTOPE_QUADRILATERAL, 1, 0, 0, DM_POLYTOPE_TRIANGLE, 1, 1, 0, DM_POLYTOPE_TRIANGLE, 1, 2, 0, DM_POLYTOPE_TRIANGLE, 1, 3, 0, DM_POLYTOPE_TRIANGLE, 1, 4, 0};
  static PetscInt       pyrO[]    = {0, 0, 0, 0, 0};

  PetscFunctionBegin;
  if (rt) *rt = 0;
  switch (source) {
  case DM_POLYTOPE_POINT:
    *Nt     = 1;
    *target = vertexT;
    *size   = vertexS;
    *cone   = vertexC;
    *ornt   = vertexO;
    break;
  case DM_POLYTOPE_SEGMENT:
    *Nt     = 1;
    *target = edgeT;
    *size   = edgeS;
    *cone   = edgeC;
    *ornt   = edgeO;
    break;
  case DM_POLYTOPE_POINT_PRISM_TENSOR:
    *Nt     = 1;
    *target = tedgeT;
    *size   = tedgeS;
    *cone   = tedgeC;
    *ornt   = tedgeO;
    break;
  case DM_POLYTOPE_TRIANGLE:
    *Nt     = 1;
    *target = triT;
    *size   = triS;
    *cone   = triC;
    *ornt   = triO;
    break;
  case DM_POLYTOPE_QUADRILATERAL:
    *Nt     = 1;
    *target = quadT;
    *size   = quadS;
    *cone   = quadC;
    *ornt   = quadO;
    break;
  case DM_POLYTOPE_SEG_PRISM_TENSOR:
    *Nt     = 1;
    *target = tquadT;
    *size   = tquadS;
    *cone   = tquadC;
    *ornt   = tquadO;
    break;
  case DM_POLYTOPE_TETRAHEDRON:
    *Nt     = 1;
    *target = tetT;
    *size   = tetS;
    *cone   = tetC;
    *ornt   = tetO;
    break;
  case DM_POLYTOPE_HEXAHEDRON:
    *Nt     = 1;
    *target = hexT;
    *size   = hexS;
    *cone   = hexC;
    *ornt   = hexO;
    break;
  case DM_POLYTOPE_TRI_PRISM:
    *Nt     = 1;
    *target = tripT;
    *size   = tripS;
    *cone   = tripC;
    *ornt   = tripO;
    break;
  case DM_POLYTOPE_TRI_PRISM_TENSOR:
    *Nt     = 1;
    *target = ttripT;
    *size   = ttripS;
    *cone   = ttripC;
    *ornt   = ttripO;
    break;
  case DM_POLYTOPE_QUAD_PRISM_TENSOR:
    *Nt     = 1;
    *target = tquadpT;
    *size   = tquadpS;
    *cone   = tquadpC;
    *ornt   = tquadpO;
    break;
  case DM_POLYTOPE_PYRAMID:
    *Nt     = 1;
    *target = pyrT;
    *size   = pyrS;
    *cone   = pyrC;
    *ornt   = pyrO;
    break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "No refinement strategy for %s", DMPolytopeTypes[source]);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexTransformGetSubcellOrientation - Transform the replica number and orientation for a target point according to the group action for the source point

  Not Collective

  Input Parameters:
+ tr  - The `DMPlexTransform`
. sct - The source point cell type, from whom the new cell is being produced
. sp  - The source point
. so  - The orientation of the source point in its enclosing parent
. tct - The target point cell type
. r   - The replica number requested for the produced cell type
- o   - The orientation of the replica

  Output Parameters:
+ rnew - The replica number, given the orientation of the parent
- onew - The replica orientation, given the orientation of the parent

  Level: advanced

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPolytopeType`, `DMPlexTransformCellTransform()`, `DMPlexTransformApply()`
@*/
PetscErrorCode DMPlexTransformGetSubcellOrientation(DMPlexTransform tr, DMPolytopeType sct, PetscInt sp, PetscInt so, DMPolytopeType tct, PetscInt r, PetscInt o, PetscInt *rnew, PetscInt *onew)
{
  PetscFunctionBeginHot;
  PetscUseTypeMethod(tr, getsubcellorientation, sct, sp, so, tct, r, o, rnew, onew);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformSetConeSizes(DMPlexTransform tr, DM rdm)
{
  DM       dm;
  PetscInt pStart, pEnd, p, pNew;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  /* Must create the celltype label here so that we do not automatically try to compute the types */
  PetscCall(DMCreateLabel(rdm, "celltype"));
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  for (p = pStart; p < pEnd; ++p) {
    DMPolytopeType  ct;
    DMPolytopeType *rct;
    PetscInt       *rsize, *rcone, *rornt;
    PetscInt        Nct, n, r;

    PetscCall(DMPlexGetCellType(dm, p, &ct));
    PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
    for (n = 0; n < Nct; ++n) {
      for (r = 0; r < rsize[n]; ++r) {
        PetscCall(DMPlexTransformGetTargetPoint(tr, ct, rct[n], p, r, &pNew));
        PetscCall(DMPlexSetConeSize(rdm, pNew, DMPolytopeTypeGetConeSize(rct[n])));
        PetscCall(DMPlexSetCellType(rdm, pNew, rct[n]));
      }
    }
  }
  /* Let the DM know we have set all the cell types */
  {
    DMLabel  ctLabel;
    DM_Plex *plex = (DM_Plex *)rdm->data;

    PetscCall(DMPlexGetCellTypeLabel(rdm, &ctLabel));
    PetscCall(PetscObjectStateGet((PetscObject)ctLabel, &plex->celltypeState));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetConeSize(DMPlexTransform tr, PetscInt q, PetscInt *coneSize)
{
  DMPolytopeType ctNew;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscValidIntPointer(coneSize, 3);
  PetscCall(DMPlexTransformGetCellType(tr, q, &ctNew));
  *coneSize = DMPolytopeTypeGetConeSize((DMPolytopeType)ctNew);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* The orientation o is for the interior of the cell p */
static PetscErrorCode DMPlexTransformGetCone_Internal(DMPlexTransform tr, PetscInt p, PetscInt o, DMPolytopeType ct, DMPolytopeType ctNew, const PetscInt rcone[], PetscInt *coneoff, const PetscInt rornt[], PetscInt *orntoff, PetscInt coneNew[], PetscInt orntNew[])
{
  DM              dm;
  const PetscInt  csizeNew = DMPolytopeTypeGetConeSize(ctNew);
  const PetscInt *cone;
  PetscInt        c, coff = *coneoff, ooff = *orntoff;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMPlexGetOrientedCone(dm, p, &cone, NULL));
  for (c = 0; c < csizeNew; ++c) {
    PetscInt             ppp   = -1;                            /* Parent Parent point: Parent of point pp */
    PetscInt             pp    = p;                             /* Parent point: Point in the original mesh producing new cone point */
    PetscInt             po    = 0;                             /* Orientation of parent point pp in parent parent point ppp */
    DMPolytopeType       pct   = ct;                            /* Parent type: Cell type for parent of new cone point */
    const PetscInt      *pcone = cone;                          /* Parent cone: Cone of parent point pp */
    PetscInt             pr    = -1;                            /* Replica number of pp that produces new cone point  */
    const DMPolytopeType ft    = (DMPolytopeType)rcone[coff++]; /* Cell type for new cone point of pNew */
    const PetscInt       fn    = rcone[coff++];                 /* Number of cones of p that need to be taken when producing new cone point */
    PetscInt             fo    = rornt[ooff++];                 /* Orientation of new cone point in pNew */
    PetscInt             lc;

    /* Get the type (pct) and point number (pp) of the parent point in the original mesh which produces this cone point */
    for (lc = 0; lc < fn; ++lc) {
      const PetscInt *parr = DMPolytopeTypeGetArrangment(pct, po);
      const PetscInt  acp  = rcone[coff++];
      const PetscInt  pcp  = parr[acp * 2];
      const PetscInt  pco  = parr[acp * 2 + 1];
      const PetscInt *ppornt;

      ppp = pp;
      pp  = pcone[pcp];
      PetscCall(DMPlexGetCellType(dm, pp, &pct));
      // Restore the parent cone from the last iterate
      if (lc) PetscCall(DMPlexRestoreOrientedCone(dm, ppp, &pcone, NULL));
      PetscCall(DMPlexGetOrientedCone(dm, pp, &pcone, NULL));
      PetscCall(DMPlexGetOrientedCone(dm, ppp, NULL, &ppornt));
      po = DMPolytopeTypeComposeOrientation(pct, ppornt[pcp], pco);
      PetscCall(DMPlexRestoreOrientedCone(dm, ppp, NULL, &ppornt));
    }
    if (lc) PetscCall(DMPlexRestoreOrientedCone(dm, pp, &pcone, NULL));
    pr = rcone[coff++];
    /* Orientation po of pp maps (pr, fo) -> (pr', fo') */
    PetscCall(DMPlexTransformGetSubcellOrientation(tr, pct, pp, fn ? po : o, ft, pr, fo, &pr, &fo));
    PetscCall(DMPlexTransformGetTargetPoint(tr, pct, ft, pp, pr, &coneNew[c]));
    orntNew[c] = fo;
  }
  PetscCall(DMPlexRestoreOrientedCone(dm, p, &cone, NULL));
  *coneoff = coff;
  *orntoff = ooff;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformSetCones(DMPlexTransform tr, DM rdm)
{
  DM             dm;
  DMPolytopeType ct;
  PetscInt      *coneNew, *orntNew;
  PetscInt       maxConeSize = 0, pStart, pEnd, p, pNew;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  for (p = 0; p < DM_NUM_POLYTOPES; ++p) maxConeSize = PetscMax(maxConeSize, DMPolytopeTypeGetConeSize((DMPolytopeType)p));
  PetscCall(DMGetWorkArray(rdm, maxConeSize, MPIU_INT, &coneNew));
  PetscCall(DMGetWorkArray(rdm, maxConeSize, MPIU_INT, &orntNew));
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  for (p = pStart; p < pEnd; ++p) {
    PetscInt        coff, ooff;
    DMPolytopeType *rct;
    PetscInt       *rsize, *rcone, *rornt;
    PetscInt        Nct, n, r;

    PetscCall(DMPlexGetCellType(dm, p, &ct));
    PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
    for (n = 0, coff = 0, ooff = 0; n < Nct; ++n) {
      const DMPolytopeType ctNew = rct[n];

      for (r = 0; r < rsize[n]; ++r) {
        PetscCall(DMPlexTransformGetTargetPoint(tr, ct, rct[n], p, r, &pNew));
        PetscCall(DMPlexTransformGetCone_Internal(tr, p, 0, ct, ctNew, rcone, &coff, rornt, &ooff, coneNew, orntNew));
        PetscCall(DMPlexSetCone(rdm, pNew, coneNew));
        PetscCall(DMPlexSetConeOrientation(rdm, pNew, orntNew));
      }
    }
  }
  PetscCall(DMRestoreWorkArray(rdm, maxConeSize, MPIU_INT, &coneNew));
  PetscCall(DMRestoreWorkArray(rdm, maxConeSize, MPIU_INT, &orntNew));
  PetscCall(DMViewFromOptions(rdm, NULL, "-rdm_view"));
  PetscCall(DMPlexSymmetrize(rdm));
  PetscCall(DMPlexStratify(rdm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetConeOriented(DMPlexTransform tr, PetscInt q, PetscInt po, const PetscInt *cone[], const PetscInt *ornt[])
{
  DM              dm;
  DMPolytopeType  ct, qct;
  DMPolytopeType *rct;
  PetscInt       *rsize, *rcone, *rornt, *qcone, *qornt;
  PetscInt        maxConeSize = 0, Nct, p, r, n, nr, coff = 0, ooff = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscValidPointer(cone, 4);
  PetscValidPointer(ornt, 5);
  for (p = 0; p < DM_NUM_POLYTOPES; ++p) maxConeSize = PetscMax(maxConeSize, DMPolytopeTypeGetConeSize((DMPolytopeType)p));
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMGetWorkArray(dm, maxConeSize, MPIU_INT, &qcone));
  PetscCall(DMGetWorkArray(dm, maxConeSize, MPIU_INT, &qornt));
  PetscCall(DMPlexTransformGetSourcePoint(tr, q, &ct, &qct, &p, &r));
  PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
  for (n = 0; n < Nct; ++n) {
    const DMPolytopeType ctNew    = rct[n];
    const PetscInt       csizeNew = DMPolytopeTypeGetConeSize(ctNew);
    PetscInt             Nr       = rsize[n], fn, c;

    if (ctNew == qct) Nr = r;
    for (nr = 0; nr < Nr; ++nr) {
      for (c = 0; c < csizeNew; ++c) {
        ++coff;             /* Cell type of new cone point */
        fn = rcone[coff++]; /* Number of cones of p that need to be taken when producing new cone point */
        coff += fn;
        ++coff; /* Replica number of new cone point */
        ++ooff; /* Orientation of new cone point */
      }
    }
    if (ctNew == qct) break;
  }
  PetscCall(DMPlexTransformGetCone_Internal(tr, p, po, ct, qct, rcone, &coff, rornt, &ooff, qcone, qornt));
  *cone = qcone;
  *ornt = qornt;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformGetCone(DMPlexTransform tr, PetscInt q, const PetscInt *cone[], const PetscInt *ornt[])
{
  DM              dm;
  DMPolytopeType  ct, qct;
  DMPolytopeType *rct;
  PetscInt       *rsize, *rcone, *rornt, *qcone, *qornt;
  PetscInt        maxConeSize = 0, Nct, p, r, n, nr, coff = 0, ooff = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  if (cone) PetscValidPointer(cone, 3);
  if (ornt) PetscValidPointer(ornt, 4);
  for (p = 0; p < DM_NUM_POLYTOPES; ++p) maxConeSize = PetscMax(maxConeSize, DMPolytopeTypeGetConeSize((DMPolytopeType)p));
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMGetWorkArray(dm, maxConeSize, MPIU_INT, &qcone));
  PetscCall(DMGetWorkArray(dm, maxConeSize, MPIU_INT, &qornt));
  PetscCall(DMPlexTransformGetSourcePoint(tr, q, &ct, &qct, &p, &r));
  PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
  for (n = 0; n < Nct; ++n) {
    const DMPolytopeType ctNew    = rct[n];
    const PetscInt       csizeNew = DMPolytopeTypeGetConeSize(ctNew);
    PetscInt             Nr       = rsize[n], fn, c;

    if (ctNew == qct) Nr = r;
    for (nr = 0; nr < Nr; ++nr) {
      for (c = 0; c < csizeNew; ++c) {
        ++coff;             /* Cell type of new cone point */
        fn = rcone[coff++]; /* Number of cones of p that need to be taken when producing new cone point */
        coff += fn;
        ++coff; /* Replica number of new cone point */
        ++ooff; /* Orientation of new cone point */
      }
    }
    if (ctNew == qct) break;
  }
  PetscCall(DMPlexTransformGetCone_Internal(tr, p, 0, ct, qct, rcone, &coff, rornt, &ooff, qcone, qornt));
  if (cone) *cone = qcone;
  else PetscCall(DMRestoreWorkArray(dm, maxConeSize, MPIU_INT, &qcone));
  if (ornt) *ornt = qornt;
  else PetscCall(DMRestoreWorkArray(dm, maxConeSize, MPIU_INT, &qornt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformRestoreCone(DMPlexTransform tr, PetscInt q, const PetscInt *cone[], const PetscInt *ornt[])
{
  DM dm;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  if (cone) PetscCall(DMRestoreWorkArray(dm, 0, MPIU_INT, cone));
  if (ornt) PetscCall(DMRestoreWorkArray(dm, 0, MPIU_INT, ornt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformCreateCellVertices_Internal(DMPlexTransform tr)
{
  PetscInt ict;

  PetscFunctionBegin;
  PetscCall(PetscCalloc3(DM_NUM_POLYTOPES, &tr->trNv, DM_NUM_POLYTOPES, &tr->trVerts, DM_NUM_POLYTOPES, &tr->trSubVerts));
  for (ict = DM_POLYTOPE_POINT; ict < DM_NUM_POLYTOPES; ++ict) {
    const DMPolytopeType ct = (DMPolytopeType)ict;
    DMPlexTransform      reftr;
    DM                   refdm, trdm;
    Vec                  coordinates;
    const PetscScalar   *coords;
    DMPolytopeType      *rct;
    PetscInt            *rsize, *rcone, *rornt;
    PetscInt             Nct, n, r, pNew;
    PetscInt             trdim, vStart, vEnd, Nc;
    const PetscInt       debug = 0;
    const char          *typeName;

    /* Since points are 0-dimensional, coordinates make no sense */
    if (DMPolytopeTypeGetDim(ct) <= 0) continue;
    PetscCall(DMPlexCreateReferenceCell(PETSC_COMM_SELF, ct, &refdm));
    PetscCall(DMPlexTransformCreate(PETSC_COMM_SELF, &reftr));
    PetscCall(DMPlexTransformSetDM(reftr, refdm));
    PetscCall(DMPlexTransformGetType(tr, &typeName));
    PetscCall(DMPlexTransformSetType(reftr, typeName));
    PetscCall(DMPlexTransformSetUp(reftr));
    PetscCall(DMPlexTransformApply(reftr, refdm, &trdm));

    PetscCall(DMGetDimension(trdm, &trdim));
    PetscCall(DMPlexGetDepthStratum(trdm, 0, &vStart, &vEnd));
    tr->trNv[ct] = vEnd - vStart;
    PetscCall(DMGetCoordinatesLocal(trdm, &coordinates));
    PetscCall(VecGetLocalSize(coordinates, &Nc));
    PetscCheck(tr->trNv[ct] * trdim == Nc, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Cell type %s, transformed coordinate size %" PetscInt_FMT " != %" PetscInt_FMT " size of coordinate storage", DMPolytopeTypes[ct], tr->trNv[ct] * trdim, Nc);
    PetscCall(PetscCalloc1(Nc, &tr->trVerts[ct]));
    PetscCall(VecGetArrayRead(coordinates, &coords));
    PetscCall(PetscArraycpy(tr->trVerts[ct], coords, Nc));
    PetscCall(VecRestoreArrayRead(coordinates, &coords));

    PetscCall(PetscCalloc1(DM_NUM_POLYTOPES, &tr->trSubVerts[ct]));
    PetscCall(DMPlexTransformCellTransform(reftr, ct, 0, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
    for (n = 0; n < Nct; ++n) {
      /* Since points are 0-dimensional, coordinates make no sense */
      if (rct[n] == DM_POLYTOPE_POINT) continue;
      PetscCall(PetscCalloc1(rsize[n], &tr->trSubVerts[ct][rct[n]]));
      for (r = 0; r < rsize[n]; ++r) {
        PetscInt *closure = NULL;
        PetscInt  clSize, cl, Nv = 0;

        PetscCall(PetscCalloc1(DMPolytopeTypeGetNumVertices(rct[n]), &tr->trSubVerts[ct][rct[n]][r]));
        PetscCall(DMPlexTransformGetTargetPoint(reftr, ct, rct[n], 0, r, &pNew));
        PetscCall(DMPlexGetTransitiveClosure(trdm, pNew, PETSC_TRUE, &clSize, &closure));
        for (cl = 0; cl < clSize * 2; cl += 2) {
          const PetscInt sv = closure[cl];

          if ((sv >= vStart) && (sv < vEnd)) tr->trSubVerts[ct][rct[n]][r][Nv++] = sv - vStart;
        }
        PetscCall(DMPlexRestoreTransitiveClosure(trdm, pNew, PETSC_TRUE, &clSize, &closure));
        PetscCheck(Nv == DMPolytopeTypeGetNumVertices(rct[n]), PETSC_COMM_SELF, PETSC_ERR_PLIB, "Number of vertices %" PetscInt_FMT " != %" PetscInt_FMT " for %s subcell %" PetscInt_FMT " from cell %s", Nv, DMPolytopeTypeGetNumVertices(rct[n]), DMPolytopeTypes[rct[n]], r, DMPolytopeTypes[ct]);
      }
    }
    if (debug) {
      DMPolytopeType *rct;
      PetscInt       *rsize, *rcone, *rornt;
      PetscInt        v, dE = trdim, d, off = 0;

      PetscCall(PetscPrintf(PETSC_COMM_SELF, "%s: %" PetscInt_FMT " vertices\n", DMPolytopeTypes[ct], tr->trNv[ct]));
      for (v = 0; v < tr->trNv[ct]; ++v) {
        PetscCall(PetscPrintf(PETSC_COMM_SELF, "  "));
        for (d = 0; d < dE; ++d) PetscCall(PetscPrintf(PETSC_COMM_SELF, "%g ", (double)PetscRealPart(tr->trVerts[ct][off++])));
        PetscCall(PetscPrintf(PETSC_COMM_SELF, "\n"));
      }

      PetscCall(DMPlexTransformCellTransform(reftr, ct, 0, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
      for (n = 0; n < Nct; ++n) {
        if (rct[n] == DM_POLYTOPE_POINT) continue;
        PetscCall(PetscPrintf(PETSC_COMM_SELF, "%s: %s subvertices %" PetscInt_FMT "\n", DMPolytopeTypes[ct], DMPolytopeTypes[rct[n]], tr->trNv[ct]));
        for (r = 0; r < rsize[n]; ++r) {
          PetscCall(PetscPrintf(PETSC_COMM_SELF, "  "));
          for (v = 0; v < DMPolytopeTypeGetNumVertices(rct[n]); ++v) PetscCall(PetscPrintf(PETSC_COMM_SELF, "%" PetscInt_FMT " ", tr->trSubVerts[ct][rct[n]][r][v]));
          PetscCall(PetscPrintf(PETSC_COMM_SELF, "\n"));
        }
      }
    }
    PetscCall(DMDestroy(&refdm));
    PetscCall(DMDestroy(&trdm));
    PetscCall(DMPlexTransformDestroy(&reftr));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexTransformGetCellVertices - Get the set of transformed vertices lying in the closure of a reference cell of given type

  Input Parameters:
+ tr - The `DMPlexTransform` object
- ct - The cell type

  Output Parameters:
+ Nv      - The number of transformed vertices in the closure of the reference cell of given type
- trVerts - The coordinates of these vertices in the reference cell

  Level: developer

.seealso: `DMPLEX`, `DMPlexTransform`, `DMPolytopeType`, `DMPlexTransformGetSubcellVertices()`
@*/
PetscErrorCode DMPlexTransformGetCellVertices(DMPlexTransform tr, DMPolytopeType ct, PetscInt *Nv, PetscScalar *trVerts[])
{
  PetscFunctionBegin;
  if (!tr->trNv) PetscCall(DMPlexTransformCreateCellVertices_Internal(tr));
  if (Nv) *Nv = tr->trNv[ct];
  if (trVerts) *trVerts = tr->trVerts[ct];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexTransformGetSubcellVertices - Get the set of transformed vertices defining a subcell in the reference cell of given type

  Input Parameters:
+ tr  - The `DMPlexTransform` object
. ct  - The cell type
. rct - The subcell type
- r   - The subcell index

  Output Parameter:
. subVerts - The indices of these vertices in the set of vertices returned by `DMPlexTransformGetCellVertices()`

  Level: developer

.seealso:  `DMPLEX`, `DMPlexTransform`, `DMPolytopeType`, `DMPlexTransformGetCellVertices()`
@*/
PetscErrorCode DMPlexTransformGetSubcellVertices(DMPlexTransform tr, DMPolytopeType ct, DMPolytopeType rct, PetscInt r, PetscInt *subVerts[])
{
  PetscFunctionBegin;
  if (!tr->trNv) PetscCall(DMPlexTransformCreateCellVertices_Internal(tr));
  PetscCheck(tr->trSubVerts[ct][rct], PetscObjectComm((PetscObject)tr), PETSC_ERR_ARG_WRONG, "Cell type %s does not produce %s", DMPolytopeTypes[ct], DMPolytopeTypes[rct]);
  if (subVerts) *subVerts = tr->trSubVerts[ct][rct][r];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Computes new vertex as the barycenter, or centroid */
PetscErrorCode DMPlexTransformMapCoordinatesBarycenter_Internal(DMPlexTransform tr, DMPolytopeType pct, DMPolytopeType ct, PetscInt p, PetscInt r, PetscInt Nv, PetscInt dE, const PetscScalar in[], PetscScalar out[])
{
  PetscInt v, d;

  PetscFunctionBeginHot;
  PetscCheck(ct == DM_POLYTOPE_POINT, PETSC_COMM_SELF, PETSC_ERR_SUP, "Not for refined point type %s", DMPolytopeTypes[ct]);
  for (d = 0; d < dE; ++d) out[d] = 0.0;
  for (v = 0; v < Nv; ++v)
    for (d = 0; d < dE; ++d) out[d] += in[v * dE + d];
  for (d = 0; d < dE; ++d) out[d] /= Nv;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexTransformMapCoordinates - Calculate new coordinates for produced points

  Not collective

  Input Parameters:
+ tr   - The `DMPlexTransform`
. pct  - The cell type of the parent, from whom the new cell is being produced
. ct   - The type being produced
. p    - The original point
. r    - The replica number requested for the produced cell type
. Nv   - Number of vertices in the closure of the parent cell
. dE   - Spatial dimension
- in   - array of size Nv*dE, holding coordinates of the vertices in the closure of the parent cell

  Output Parameter:
. out - The coordinates of the new vertices

  Level: intermediate

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPolytopeType`, `DMPlexTransform`, `DMPlexTransformApply()`
@*/
PetscErrorCode DMPlexTransformMapCoordinates(DMPlexTransform tr, DMPolytopeType pct, DMPolytopeType ct, PetscInt p, PetscInt r, PetscInt Nv, PetscInt dE, const PetscScalar in[], PetscScalar out[])
{
  PetscFunctionBeginHot;
  PetscUseTypeMethod(tr, mapcoordinates, pct, ct, p, r, Nv, dE, in, out);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode RefineLabel_Internal(DMPlexTransform tr, DMLabel label, DMLabel labelNew)
{
  DM              dm;
  IS              valueIS;
  const PetscInt *values;
  PetscInt        defVal, Nv, val;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMLabelGetDefaultValue(label, &defVal));
  PetscCall(DMLabelSetDefaultValue(labelNew, defVal));
  PetscCall(DMLabelGetValueIS(label, &valueIS));
  PetscCall(ISGetLocalSize(valueIS, &Nv));
  PetscCall(ISGetIndices(valueIS, &values));
  for (val = 0; val < Nv; ++val) {
    IS              pointIS;
    const PetscInt *points;
    PetscInt        numPoints, p;

    /* Ensure refined label is created with same number of strata as
     * original (even if no entries here). */
    PetscCall(DMLabelAddStratum(labelNew, values[val]));
    PetscCall(DMLabelGetStratumIS(label, values[val], &pointIS));
    PetscCall(ISGetLocalSize(pointIS, &numPoints));
    PetscCall(ISGetIndices(pointIS, &points));
    for (p = 0; p < numPoints; ++p) {
      const PetscInt  point = points[p];
      DMPolytopeType  ct;
      DMPolytopeType *rct;
      PetscInt       *rsize, *rcone, *rornt;
      PetscInt        Nct, n, r, pNew = 0;

      PetscCall(DMPlexGetCellType(dm, point, &ct));
      PetscCall(DMPlexTransformCellTransform(tr, ct, point, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
      for (n = 0; n < Nct; ++n) {
        for (r = 0; r < rsize[n]; ++r) {
          PetscCall(DMPlexTransformGetTargetPoint(tr, ct, rct[n], point, r, &pNew));
          PetscCall(DMLabelSetValue(labelNew, pNew, values[val]));
        }
      }
    }
    PetscCall(ISRestoreIndices(pointIS, &points));
    PetscCall(ISDestroy(&pointIS));
  }
  PetscCall(ISRestoreIndices(valueIS, &values));
  PetscCall(ISDestroy(&valueIS));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformCreateLabels(DMPlexTransform tr, DM rdm)
{
  DM       dm;
  PetscInt numLabels, l;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMGetNumLabels(dm, &numLabels));
  for (l = 0; l < numLabels; ++l) {
    DMLabel     label, labelNew;
    const char *lname;
    PetscBool   isDepth, isCellType;

    PetscCall(DMGetLabelName(dm, l, &lname));
    PetscCall(PetscStrcmp(lname, "depth", &isDepth));
    if (isDepth) continue;
    PetscCall(PetscStrcmp(lname, "celltype", &isCellType));
    if (isCellType) continue;
    PetscCall(DMCreateLabel(rdm, lname));
    PetscCall(DMGetLabel(dm, lname, &label));
    PetscCall(DMGetLabel(rdm, lname, &labelNew));
    PetscCall(RefineLabel_Internal(tr, label, labelNew));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* This refines the labels which define regions for fields and DSes since they are not in the list of labels for the DM */
PetscErrorCode DMPlexTransformCreateDiscLabels(DMPlexTransform tr, DM rdm)
{
  DM       dm;
  PetscInt Nf, f, Nds, s;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMGetNumFields(dm, &Nf));
  for (f = 0; f < Nf; ++f) {
    DMLabel     label, labelNew;
    PetscObject obj;
    const char *lname;

    PetscCall(DMGetField(rdm, f, &label, &obj));
    if (!label) continue;
    PetscCall(PetscObjectGetName((PetscObject)label, &lname));
    PetscCall(DMLabelCreate(PETSC_COMM_SELF, lname, &labelNew));
    PetscCall(RefineLabel_Internal(tr, label, labelNew));
    PetscCall(DMSetField_Internal(rdm, f, labelNew, obj));
    PetscCall(DMLabelDestroy(&labelNew));
  }
  PetscCall(DMGetNumDS(dm, &Nds));
  for (s = 0; s < Nds; ++s) {
    DMLabel     label, labelNew;
    const char *lname;

    PetscCall(DMGetRegionNumDS(rdm, s, &label, NULL, NULL, NULL));
    if (!label) continue;
    PetscCall(PetscObjectGetName((PetscObject)label, &lname));
    PetscCall(DMLabelCreate(PETSC_COMM_SELF, lname, &labelNew));
    PetscCall(RefineLabel_Internal(tr, label, labelNew));
    PetscCall(DMSetRegionNumDS(rdm, s, labelNew, NULL, NULL, NULL));
    PetscCall(DMLabelDestroy(&labelNew));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformCreateSF(DMPlexTransform tr, DM rdm)
{
  DM                 dm;
  PetscSF            sf, sfNew;
  PetscInt           numRoots, numLeaves, numLeavesNew = 0, l, m;
  const PetscInt    *localPoints;
  const PetscSFNode *remotePoints;
  PetscInt          *localPointsNew;
  PetscSFNode       *remotePointsNew;
  PetscInt           pStartNew, pEndNew, pNew;
  /* Brute force algorithm */
  PetscSF         rsf;
  PetscSection    s;
  const PetscInt *rootdegree;
  PetscInt       *rootPointsNew, *remoteOffsets;
  PetscInt        numPointsNew, pStart, pEnd, p;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMPlexGetChart(rdm, &pStartNew, &pEndNew));
  PetscCall(DMGetPointSF(dm, &sf));
  PetscCall(DMGetPointSF(rdm, &sfNew));
  /* Calculate size of new SF */
  PetscCall(PetscSFGetGraph(sf, &numRoots, &numLeaves, &localPoints, &remotePoints));
  if (numRoots < 0) PetscFunctionReturn(PETSC_SUCCESS);
  for (l = 0; l < numLeaves; ++l) {
    const PetscInt  p = localPoints[l];
    DMPolytopeType  ct;
    DMPolytopeType *rct;
    PetscInt       *rsize, *rcone, *rornt;
    PetscInt        Nct, n;

    PetscCall(DMPlexGetCellType(dm, p, &ct));
    PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
    for (n = 0; n < Nct; ++n) numLeavesNew += rsize[n];
  }
  /* Send new root point numbers
       It is possible to optimize for regular transforms by sending only the cell type offsets, but it seems a needless complication
  */
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  PetscCall(PetscSectionCreate(PetscObjectComm((PetscObject)dm), &s));
  PetscCall(PetscSectionSetChart(s, pStart, pEnd));
  for (p = pStart; p < pEnd; ++p) {
    DMPolytopeType  ct;
    DMPolytopeType *rct;
    PetscInt       *rsize, *rcone, *rornt;
    PetscInt        Nct, n;

    PetscCall(DMPlexGetCellType(dm, p, &ct));
    PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
    for (n = 0; n < Nct; ++n) PetscCall(PetscSectionAddDof(s, p, rsize[n]));
  }
  PetscCall(PetscSectionSetUp(s));
  PetscCall(PetscSectionGetStorageSize(s, &numPointsNew));
  PetscCall(PetscSFCreateRemoteOffsets(sf, s, s, &remoteOffsets));
  PetscCall(PetscSFCreateSectionSF(sf, s, remoteOffsets, s, &rsf));
  PetscCall(PetscFree(remoteOffsets));
  PetscCall(PetscSFComputeDegreeBegin(sf, &rootdegree));
  PetscCall(PetscSFComputeDegreeEnd(sf, &rootdegree));
  PetscCall(PetscMalloc1(numPointsNew, &rootPointsNew));
  for (p = 0; p < numPointsNew; ++p) rootPointsNew[p] = -1;
  for (p = pStart; p < pEnd; ++p) {
    DMPolytopeType  ct;
    DMPolytopeType *rct;
    PetscInt       *rsize, *rcone, *rornt;
    PetscInt        Nct, n, r, off;

    if (!rootdegree[p - pStart]) continue;
    PetscCall(PetscSectionGetOffset(s, p, &off));
    PetscCall(DMPlexGetCellType(dm, p, &ct));
    PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
    for (n = 0, m = 0; n < Nct; ++n) {
      for (r = 0; r < rsize[n]; ++r, ++m) {
        PetscCall(DMPlexTransformGetTargetPoint(tr, ct, rct[n], p, r, &pNew));
        rootPointsNew[off + m] = pNew;
      }
    }
  }
  PetscCall(PetscSFBcastBegin(rsf, MPIU_INT, rootPointsNew, rootPointsNew, MPI_REPLACE));
  PetscCall(PetscSFBcastEnd(rsf, MPIU_INT, rootPointsNew, rootPointsNew, MPI_REPLACE));
  PetscCall(PetscSFDestroy(&rsf));
  PetscCall(PetscMalloc1(numLeavesNew, &localPointsNew));
  PetscCall(PetscMalloc1(numLeavesNew, &remotePointsNew));
  for (l = 0, m = 0; l < numLeaves; ++l) {
    const PetscInt  p = localPoints[l];
    DMPolytopeType  ct;
    DMPolytopeType *rct;
    PetscInt       *rsize, *rcone, *rornt;
    PetscInt        Nct, n, r, q, off;

    PetscCall(PetscSectionGetOffset(s, p, &off));
    PetscCall(DMPlexGetCellType(dm, p, &ct));
    PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
    for (n = 0, q = 0; n < Nct; ++n) {
      for (r = 0; r < rsize[n]; ++r, ++m, ++q) {
        PetscCall(DMPlexTransformGetTargetPoint(tr, ct, rct[n], p, r, &pNew));
        localPointsNew[m]        = pNew;
        remotePointsNew[m].index = rootPointsNew[off + q];
        remotePointsNew[m].rank  = remotePoints[l].rank;
      }
    }
  }
  PetscCall(PetscSectionDestroy(&s));
  PetscCall(PetscFree(rootPointsNew));
  /* SF needs sorted leaves to correctly calculate Gather */
  {
    PetscSFNode *rp, *rtmp;
    PetscInt    *lp, *idx, *ltmp, i;

    PetscCall(PetscMalloc1(numLeavesNew, &idx));
    PetscCall(PetscMalloc1(numLeavesNew, &lp));
    PetscCall(PetscMalloc1(numLeavesNew, &rp));
    for (i = 0; i < numLeavesNew; ++i) {
      PetscCheck(!(localPointsNew[i] < pStartNew) && !(localPointsNew[i] >= pEndNew), PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Local SF point %" PetscInt_FMT " (%" PetscInt_FMT ") not in [%" PetscInt_FMT ", %" PetscInt_FMT ")", localPointsNew[i], i, pStartNew, pEndNew);
      idx[i] = i;
    }
    PetscCall(PetscSortIntWithPermutation(numLeavesNew, localPointsNew, idx));
    for (i = 0; i < numLeavesNew; ++i) {
      lp[i] = localPointsNew[idx[i]];
      rp[i] = remotePointsNew[idx[i]];
    }
    ltmp            = localPointsNew;
    localPointsNew  = lp;
    rtmp            = remotePointsNew;
    remotePointsNew = rp;
    PetscCall(PetscFree(idx));
    PetscCall(PetscFree(ltmp));
    PetscCall(PetscFree(rtmp));
  }
  PetscCall(PetscSFSetGraph(sfNew, pEndNew - pStartNew, numLeavesNew, localPointsNew, PETSC_OWN_POINTER, remotePointsNew, PETSC_OWN_POINTER));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMPlexCellRefinerMapLocalizedCoordinates - Given a cell of `DMPolytopeType` ct with localized coordinates x, generate localized coordinates xr for subcell r of type rct.

  Not Collective

  Input Parameters:
+ tr  - The `DMPlexTransform`
. ct  - The type of the parent cell
. rct - The type of the produced cell
. r   - The index of the produced cell
- x   - The localized coordinates for the parent cell

  Output Parameter:
. xr  - The localized coordinates for the produced cell

  Level: developer

.seealso: [](chapter_unstructured), `DM`, `DMPLEX`, `DMPlexTransform`, `DMPolytopeType`, `DMPlexCellRefinerSetCoordinates()`
@*/
static PetscErrorCode DMPlexTransformMapLocalizedCoordinates(DMPlexTransform tr, DMPolytopeType ct, DMPolytopeType rct, PetscInt r, const PetscScalar x[], PetscScalar xr[])
{
  PetscFE  fe = NULL;
  PetscInt cdim, v, *subcellV;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetCoordinateFE(tr, ct, &fe));
  PetscCall(DMPlexTransformGetSubcellVertices(tr, ct, rct, r, &subcellV));
  PetscCall(PetscFEGetNumComponents(fe, &cdim));
  for (v = 0; v < DMPolytopeTypeGetNumVertices(rct); ++v) PetscCall(PetscFEInterpolate_Static(fe, x, tr->refGeom[ct], subcellV[v], &xr[v * cdim]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexTransformSetCoordinates(DMPlexTransform tr, DM rdm)
{
  DM                 dm, cdm, cdmCell, cdmNew, cdmCellNew;
  PetscSection       coordSection, coordSectionNew, coordSectionCell, coordSectionCellNew;
  Vec                coordsLocal, coordsLocalNew, coordsLocalCell = NULL, coordsLocalCellNew;
  const PetscScalar *coords;
  PetscScalar       *coordsNew;
  const PetscReal   *maxCell, *Lstart, *L;
  PetscBool          localized, localizeVertices = PETSC_FALSE, localizeCells = PETSC_FALSE;
  PetscInt           dE, dEo, d, cStart, cEnd, c, cStartNew, cEndNew, vStartNew, vEndNew, v, pStart, pEnd, p;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformGetDM(tr, &dm));
  PetscCall(DMGetCoordinateDM(dm, &cdm));
  PetscCall(DMGetCellCoordinateDM(dm, &cdmCell));
  PetscCall(DMGetCoordinatesLocalized(dm, &localized));
  PetscCall(DMGetPeriodicity(dm, &maxCell, &Lstart, &L));
  if (localized) {
    /* Localize coordinates of new vertices */
    localizeVertices = PETSC_TRUE;
    /* If we do not have a mechanism for automatically localizing cell coordinates, we need to compute them explicitly for every divided cell */
    if (!maxCell) localizeCells = PETSC_TRUE;
  }
  PetscCall(DMGetCoordinateSection(dm, &coordSection));
  PetscCall(PetscSectionGetFieldComponents(coordSection, 0, &dEo));
  if (maxCell) {
    PetscReal maxCellNew[3];

    for (d = 0; d < dEo; ++d) maxCellNew[d] = maxCell[d] / 2.0;
    PetscCall(DMSetPeriodicity(rdm, maxCellNew, Lstart, L));
  }
  PetscCall(DMGetCoordinateDim(rdm, &dE));
  PetscCall(PetscSectionCreate(PetscObjectComm((PetscObject)rdm), &coordSectionNew));
  PetscCall(PetscSectionSetNumFields(coordSectionNew, 1));
  PetscCall(PetscSectionSetFieldComponents(coordSectionNew, 0, dE));
  PetscCall(DMPlexGetDepthStratum(rdm, 0, &vStartNew, &vEndNew));
  PetscCall(PetscSectionSetChart(coordSectionNew, vStartNew, vEndNew));
  /* Localization should be inherited */
  /*   Stefano calculates parent cells for each new cell for localization */
  /*   Localized cells need coordinates of closure */
  for (v = vStartNew; v < vEndNew; ++v) {
    PetscCall(PetscSectionSetDof(coordSectionNew, v, dE));
    PetscCall(PetscSectionSetFieldDof(coordSectionNew, v, 0, dE));
  }
  PetscCall(PetscSectionSetUp(coordSectionNew));
  PetscCall(DMSetCoordinateSection(rdm, PETSC_DETERMINE, coordSectionNew));

  if (localizeCells) {
    PetscCall(DMGetCoordinateDM(rdm, &cdmNew));
    PetscCall(DMClone(cdmNew, &cdmCellNew));
    PetscCall(DMSetCellCoordinateDM(rdm, cdmCellNew));
    PetscCall(DMDestroy(&cdmCellNew));

    PetscCall(PetscSectionCreate(PetscObjectComm((PetscObject)rdm), &coordSectionCellNew));
    PetscCall(PetscSectionSetNumFields(coordSectionCellNew, 1));
    PetscCall(PetscSectionSetFieldComponents(coordSectionCellNew, 0, dE));
    PetscCall(DMPlexGetHeightStratum(rdm, 0, &cStartNew, &cEndNew));
    PetscCall(PetscSectionSetChart(coordSectionCellNew, cStartNew, cEndNew));

    PetscCall(DMGetCellCoordinateSection(dm, &coordSectionCell));
    PetscCall(DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd));
    for (c = cStart; c < cEnd; ++c) {
      PetscInt dof;

      PetscCall(PetscSectionGetDof(coordSectionCell, c, &dof));
      if (dof) {
        DMPolytopeType  ct;
        DMPolytopeType *rct;
        PetscInt       *rsize, *rcone, *rornt;
        PetscInt        dim, cNew, Nct, n, r;

        PetscCall(DMPlexGetCellType(dm, c, &ct));
        dim = DMPolytopeTypeGetDim(ct);
        PetscCall(DMPlexTransformCellTransform(tr, ct, c, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
        /* This allows for different cell types */
        for (n = 0; n < Nct; ++n) {
          if (dim != DMPolytopeTypeGetDim(rct[n])) continue;
          for (r = 0; r < rsize[n]; ++r) {
            PetscInt *closure = NULL;
            PetscInt  clSize, cl, Nv = 0;

            PetscCall(DMPlexTransformGetTargetPoint(tr, ct, rct[n], c, r, &cNew));
            PetscCall(DMPlexGetTransitiveClosure(rdm, cNew, PETSC_TRUE, &clSize, &closure));
            for (cl = 0; cl < clSize * 2; cl += 2) {
              if ((closure[cl] >= vStartNew) && (closure[cl] < vEndNew)) ++Nv;
            }
            PetscCall(DMPlexRestoreTransitiveClosure(rdm, cNew, PETSC_TRUE, &clSize, &closure));
            PetscCall(PetscSectionSetDof(coordSectionCellNew, cNew, Nv * dE));
            PetscCall(PetscSectionSetFieldDof(coordSectionCellNew, cNew, 0, Nv * dE));
          }
        }
      }
    }
    PetscCall(PetscSectionSetUp(coordSectionCellNew));
    PetscCall(DMSetCellCoordinateSection(rdm, PETSC_DETERMINE, coordSectionCellNew));
  }
  PetscCall(DMViewFromOptions(dm, NULL, "-coarse_dm_view"));
  {
    VecType     vtype;
    PetscInt    coordSizeNew, bs;
    const char *name;

    PetscCall(DMGetCoordinatesLocal(dm, &coordsLocal));
    PetscCall(VecCreate(PETSC_COMM_SELF, &coordsLocalNew));
    PetscCall(PetscSectionGetStorageSize(coordSectionNew, &coordSizeNew));
    PetscCall(VecSetSizes(coordsLocalNew, coordSizeNew, PETSC_DETERMINE));
    PetscCall(PetscObjectGetName((PetscObject)coordsLocal, &name));
    PetscCall(PetscObjectSetName((PetscObject)coordsLocalNew, name));
    PetscCall(VecGetBlockSize(coordsLocal, &bs));
    PetscCall(VecSetBlockSize(coordsLocalNew, dEo == dE ? bs : dE));
    PetscCall(VecGetType(coordsLocal, &vtype));
    PetscCall(VecSetType(coordsLocalNew, vtype));
  }
  PetscCall(VecGetArrayRead(coordsLocal, &coords));
  PetscCall(VecGetArray(coordsLocalNew, &coordsNew));
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  /* First set coordinates for vertices */
  for (p = pStart; p < pEnd; ++p) {
    DMPolytopeType  ct;
    DMPolytopeType *rct;
    PetscInt       *rsize, *rcone, *rornt;
    PetscInt        Nct, n, r;
    PetscBool       hasVertex = PETSC_FALSE;

    PetscCall(DMPlexGetCellType(dm, p, &ct));
    PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
    for (n = 0; n < Nct; ++n) {
      if (rct[n] == DM_POLYTOPE_POINT) {
        hasVertex = PETSC_TRUE;
        break;
      }
    }
    if (hasVertex) {
      const PetscScalar *icoords = NULL;
      const PetscScalar *array   = NULL;
      PetscScalar       *pcoords = NULL;
      PetscBool          isDG;
      PetscInt           Nc, Nv, v, d;

      PetscCall(DMPlexGetCellCoordinates(dm, p, &isDG, &Nc, &array, &pcoords));

      icoords = pcoords;
      Nv      = Nc / dEo;
      if (ct != DM_POLYTOPE_POINT) {
        if (localizeVertices && maxCell) {
          PetscScalar anchor[3];

          for (d = 0; d < dEo; ++d) anchor[d] = pcoords[d];
          for (v = 0; v < Nv; ++v) PetscCall(DMLocalizeCoordinate_Internal(dm, dEo, anchor, &pcoords[v * dEo], &pcoords[v * dEo]));
        }
      }
      for (n = 0; n < Nct; ++n) {
        if (rct[n] != DM_POLYTOPE_POINT) continue;
        for (r = 0; r < rsize[n]; ++r) {
          PetscScalar vcoords[3];
          PetscInt    vNew, off;

          PetscCall(DMPlexTransformGetTargetPoint(tr, ct, rct[n], p, r, &vNew));
          PetscCall(PetscSectionGetOffset(coordSectionNew, vNew, &off));
          PetscCall(DMPlexTransformMapCoordinates(tr, ct, rct[n], p, r, Nv, dEo, icoords, vcoords));
          PetscCall(DMPlexSnapToGeomModel(dm, p, dE, vcoords, &coordsNew[off]));
        }
      }
      PetscCall(DMPlexRestoreCellCoordinates(dm, p, &isDG, &Nc, &array, &pcoords));
    }
  }
  PetscCall(VecRestoreArrayRead(coordsLocal, &coords));
  PetscCall(VecRestoreArray(coordsLocalNew, &coordsNew));
  PetscCall(DMSetCoordinatesLocal(rdm, coordsLocalNew));
  PetscCall(VecDestroy(&coordsLocalNew));
  PetscCall(PetscSectionDestroy(&coordSectionNew));
  /* Then set coordinates for cells by localizing */
  if (!localizeCells) PetscCall(DMLocalizeCoordinates(rdm));
  else {
    VecType     vtype;
    PetscInt    coordSizeNew, bs;
    const char *name;

    PetscCall(DMGetCellCoordinatesLocal(dm, &coordsLocalCell));
    PetscCall(VecCreate(PETSC_COMM_SELF, &coordsLocalCellNew));
    PetscCall(PetscSectionGetStorageSize(coordSectionCellNew, &coordSizeNew));
    PetscCall(VecSetSizes(coordsLocalCellNew, coordSizeNew, PETSC_DETERMINE));
    PetscCall(PetscObjectGetName((PetscObject)coordsLocalCell, &name));
    PetscCall(PetscObjectSetName((PetscObject)coordsLocalCellNew, name));
    PetscCall(VecGetBlockSize(coordsLocalCell, &bs));
    PetscCall(VecSetBlockSize(coordsLocalCellNew, dEo == dE ? bs : dE));
    PetscCall(VecGetType(coordsLocalCell, &vtype));
    PetscCall(VecSetType(coordsLocalCellNew, vtype));
    PetscCall(VecGetArrayRead(coordsLocalCell, &coords));
    PetscCall(VecGetArray(coordsLocalCellNew, &coordsNew));

    for (p = pStart; p < pEnd; ++p) {
      DMPolytopeType  ct;
      DMPolytopeType *rct;
      PetscInt       *rsize, *rcone, *rornt;
      PetscInt        dof = 0, Nct, n, r;

      PetscCall(DMPlexGetCellType(dm, p, &ct));
      PetscCall(DMPlexTransformCellTransform(tr, ct, p, NULL, &Nct, &rct, &rsize, &rcone, &rornt));
      if (p >= cStart && p < cEnd) PetscCall(PetscSectionGetDof(coordSectionCell, p, &dof));
      if (dof) {
        const PetscScalar *pcoords;

        PetscCall(DMPlexPointLocalRead(cdmCell, p, coords, &pcoords));
        for (n = 0; n < Nct; ++n) {
          const PetscInt Nr = rsize[n];

          if (DMPolytopeTypeGetDim(ct) != DMPolytopeTypeGetDim(rct[n])) continue;
          for (r = 0; r < Nr; ++r) {
            PetscInt pNew, offNew;

            /* It looks like Stefano and Lisandro are allowing localized coordinates without defining the periodic boundary, which means that
               DMLocalizeCoordinate_Internal() will not work. Localized coordinates will have to have obtained by the affine map of the larger
               cell to the ones it produces. */
            PetscCall(DMPlexTransformGetTargetPoint(tr, ct, rct[n], p, r, &pNew));
            PetscCall(PetscSectionGetOffset(coordSectionCellNew, pNew, &offNew));
            PetscCall(DMPlexTransformMapLocalizedCoordinates(tr, ct, rct[n], r, pcoords, &coordsNew[offNew]));
          }
        }
      }
    }
    PetscCall(VecRestoreArrayRead(coordsLocalCell, &coords));
    PetscCall(VecRestoreArray(coordsLocalCellNew, &coordsNew));
    PetscCall(DMSetCellCoordinatesLocal(rdm, coordsLocalCellNew));
    PetscCall(VecDestroy(&coordsLocalCellNew));
    PetscCall(PetscSectionDestroy(&coordSectionCellNew));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformApply(DMPlexTransform tr, DM dm, DM *tdm)
{
  DM                     rdm;
  DMPlexInterpolatedFlag interp;
  PetscInt               pStart, pEnd;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tr, DMPLEXTRANSFORM_CLASSID, 1);
  PetscValidHeaderSpecific(dm, DM_CLASSID, 2);
  PetscValidPointer(tdm, 3);
  PetscCall(DMPlexTransformSetDM(tr, dm));

  PetscCall(DMCreate(PetscObjectComm((PetscObject)dm), &rdm));
  PetscCall(DMSetType(rdm, DMPLEX));
  PetscCall(DMPlexTransformSetDimensions(tr, dm, rdm));
  /* Calculate number of new points of each depth */
  PetscCall(DMPlexIsInterpolatedCollective(dm, &interp));
  PetscCheck(interp == DMPLEX_INTERPOLATED_FULL, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONG, "Mesh must be fully interpolated for regular refinement");
  /* Step 1: Set chart */
  PetscCall(DMPlexTransformGetChart(tr, &pStart, &pEnd));
  PetscCall(DMPlexSetChart(rdm, pStart, pEnd));
  /* Step 2: Set cone/support sizes (automatically stratifies) */
  PetscCall(DMPlexTransformSetConeSizes(tr, rdm));
  /* Step 3: Setup refined DM */
  PetscCall(DMSetUp(rdm));
  /* Step 4: Set cones and supports (automatically symmetrizes) */
  PetscCall(DMPlexTransformSetCones(tr, rdm));
  /* Step 5: Create pointSF */
  PetscCall(DMPlexTransformCreateSF(tr, rdm));
  /* Step 6: Create labels */
  PetscCall(DMPlexTransformCreateLabels(tr, rdm));
  /* Step 7: Set coordinates */
  PetscCall(DMPlexTransformSetCoordinates(tr, rdm));
  PetscCall(DMPlexCopy_Internal(dm, PETSC_TRUE, PETSC_TRUE, rdm));
  // If the original DM was configured from options, the transformed DM should be as well
  rdm->setfromoptionscalled = dm->setfromoptionscalled;

  *tdm = rdm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexTransformAdaptLabel(DM dm, PETSC_UNUSED Vec metric, DMLabel adaptLabel, PETSC_UNUSED DMLabel rgLabel, DM *rdm)
{
  DMPlexTransform tr;
  DM              cdm, rcdm;
  const char     *prefix;

  PetscFunctionBegin;
  PetscCall(DMPlexTransformCreate(PetscObjectComm((PetscObject)dm), &tr));
  PetscCall(PetscObjectGetOptionsPrefix((PetscObject)dm, &prefix));
  PetscCall(PetscObjectSetOptionsPrefix((PetscObject)tr, prefix));
  PetscCall(DMPlexTransformSetDM(tr, dm));
  PetscCall(DMPlexTransformSetFromOptions(tr));
  PetscCall(DMPlexTransformSetActive(tr, adaptLabel));
  PetscCall(DMPlexTransformSetUp(tr));
  PetscCall(PetscObjectViewFromOptions((PetscObject)tr, NULL, "-dm_plex_transform_view"));
  PetscCall(DMPlexTransformApply(tr, dm, rdm));
  PetscCall(DMCopyDisc(dm, *rdm));
  PetscCall(DMGetCoordinateDM(dm, &cdm));
  PetscCall(DMGetCoordinateDM(*rdm, &rcdm));
  PetscCall(DMCopyDisc(cdm, rcdm));
  PetscCall(DMPlexTransformCreateDiscLabels(tr, *rdm));
  PetscCall(DMCopyDisc(dm, *rdm));
  PetscCall(DMPlexTransformDestroy(&tr));
  ((DM_Plex *)(*rdm)->data)->useHashLocation = ((DM_Plex *)dm->data)->useHashLocation;
  PetscFunctionReturn(PETSC_SUCCESS);
}

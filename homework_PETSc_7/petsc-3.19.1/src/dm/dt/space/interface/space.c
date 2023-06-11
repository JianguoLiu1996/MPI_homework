#include <petsc/private/petscfeimpl.h> /*I  "petscfe.h"  I*/
#include <petscdmshell.h>

PetscClassId PETSCSPACE_CLASSID = 0;

PetscFunctionList PetscSpaceList              = NULL;
PetscBool         PetscSpaceRegisterAllCalled = PETSC_FALSE;

/*@C
  PetscSpaceRegister - Adds a new `PetscSpace` implementation

  Not Collective

  Input Parameters:
+ name        - The name of a new user-defined creation routine
- create_func - The creation routine for the implementation type

  Sample usage:
.vb
    PetscSpaceRegister("my_space", MyPetscSpaceCreate);
.ve

  Then, your PetscSpace type can be chosen with the procedural interface via
.vb
    PetscSpaceCreate(MPI_Comm, PetscSpace *);
    PetscSpaceSetType(PetscSpace, "my_space");
.ve
   or at runtime via the option
.vb
    -petscspace_type my_space
.ve

  Level: advanced

  Note:
  `PetscSpaceRegister()` may be called multiple times to add several user-defined types of `PetscSpace`.  The creation function is called
  when the type is set to 'name'.

.seealso: `PetscSpace`, `PetscSpaceRegisterAll()`, `PetscSpaceRegisterDestroy()`
@*/
PetscErrorCode PetscSpaceRegister(const char sname[], PetscErrorCode (*function)(PetscSpace))
{
  PetscFunctionBegin;
  PetscCall(PetscFunctionListAdd(&PetscSpaceList, sname, function));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscSpaceSetType - Builds a particular `PetscSpace`

  Collective

  Input Parameters:
+ sp   - The `PetscSpace` object
- name - The kind of space

  Options Database Key:
. -petscspace_type <type> - Sets the `PetscSpace` type; use -help for a list of available types

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceType`, `PetscSpaceGetType()`, `PetscSpaceCreate()`
@*/
PetscErrorCode PetscSpaceSetType(PetscSpace sp, PetscSpaceType name)
{
  PetscErrorCode (*r)(PetscSpace);
  PetscBool match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)sp, name, &match));
  if (match) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCall(PetscSpaceRegisterAll());
  PetscCall(PetscFunctionListFind(PetscSpaceList, name, &r));
  PetscCheck(r, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_UNKNOWN_TYPE, "Unknown PetscSpace type: %s", name);

  PetscTryTypeMethod(sp, destroy);
  sp->ops->destroy = NULL;

  sp->dim = PETSC_DETERMINE;
  PetscCall((*r)(sp));
  PetscCall(PetscObjectChangeTypeName((PetscObject)sp, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscSpaceGetType - Gets the `PetscSpaceType` (as a string) from the object.

  Not Collective

  Input Parameter:
. sp  - The `PetscSpace`

  Output Parameter:
. name - The `PetscSpace` type name

  Level: intermediate

.seealso: `PetscSpaceType`, `PetscSpace`, `PetscSpaceSetType()`, `PetscSpaceCreate()`
@*/
PetscErrorCode PetscSpaceGetType(PetscSpace sp, PetscSpaceType *name)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscValidPointer(name, 2);
  if (!PetscSpaceRegisterAllCalled) PetscCall(PetscSpaceRegisterAll());
  *name = ((PetscObject)sp)->type_name;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSpaceViewFromOptions - View a `PetscSpace` based on values in the options database

   Collective

   Input Parameters:
+  A - the `PetscSpace` object
.  obj - Optional object that provides the options name prefix
-  name - command line option name

   Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceView()`, `PetscObjectViewFromOptions()`, `PetscSpaceCreate()`
@*/
PetscErrorCode PetscSpaceViewFromOptions(PetscSpace A, PetscObject obj, const char name[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, PETSCSPACE_CLASSID, 1);
  PetscCall(PetscObjectViewFromOptions((PetscObject)A, obj, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscSpaceView - Views a `PetscSpace`

  Collective

  Input Parameters:
+ sp - the `PetscSpace` object to view
- v  - the viewer

  Level: beginner

.seealso: `PetscSpace`, `PetscViewer`, `PetscSpaceViewFromOptions()`, `PetscSpaceDestroy()`
@*/
PetscErrorCode PetscSpaceView(PetscSpace sp, PetscViewer v)
{
  PetscInt  pdim;
  PetscBool iascii;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  if (v) PetscValidHeaderSpecific(v, PETSC_VIEWER_CLASSID, 2);
  if (!v) PetscCall(PetscViewerASCIIGetStdout(PetscObjectComm((PetscObject)sp), &v));
  PetscCall(PetscSpaceGetDimension(sp, &pdim));
  PetscCall(PetscObjectPrintClassNamePrefixType((PetscObject)sp, v));
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscViewerASCIIPushTab(v));
  if (iascii) PetscCall(PetscViewerASCIIPrintf(v, "Space in %" PetscInt_FMT " variables with %" PetscInt_FMT " components, size %" PetscInt_FMT "\n", sp->Nv, sp->Nc, pdim));
  PetscTryTypeMethod(sp, view, v);
  PetscCall(PetscViewerASCIIPopTab(v));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceSetFromOptions - sets parameters in a `PetscSpace` from the options database

  Collective

  Input Parameter:
. sp - the `PetscSpace` object to set options for

  Options Database Keys:
+ -petscspace_degree <deg> - the approximation order of the space
. -petscspace_variables <n> - the number of different variables, e.g. x and y
- -petscspace_components <c> - the number of components, say d for a vector field

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceView()`
@*/
PetscErrorCode PetscSpaceSetFromOptions(PetscSpace sp)
{
  const char *defaultType;
  char        name[256];
  PetscBool   flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  if (!((PetscObject)sp)->type_name) {
    defaultType = PETSCSPACEPOLYNOMIAL;
  } else {
    defaultType = ((PetscObject)sp)->type_name;
  }
  if (!PetscSpaceRegisterAllCalled) PetscCall(PetscSpaceRegisterAll());

  PetscObjectOptionsBegin((PetscObject)sp);
  PetscCall(PetscOptionsFList("-petscspace_type", "Linear space", "PetscSpaceSetType", PetscSpaceList, defaultType, name, 256, &flg));
  if (flg) {
    PetscCall(PetscSpaceSetType(sp, name));
  } else if (!((PetscObject)sp)->type_name) {
    PetscCall(PetscSpaceSetType(sp, defaultType));
  }
  {
    PetscCall(PetscOptionsDeprecated("-petscspace_order", "-petscspace_degree", "3.11", NULL));
  }
  PetscCall(PetscOptionsBoundedInt("-petscspace_degree", "The (maximally included) polynomial degree", "PetscSpaceSetDegree", sp->degree, &sp->degree, NULL, 0));
  PetscCall(PetscOptionsBoundedInt("-petscspace_variables", "The number of different variables, e.g. x and y", "PetscSpaceSetNumVariables", sp->Nv, &sp->Nv, NULL, 0));
  PetscCall(PetscOptionsBoundedInt("-petscspace_components", "The number of components", "PetscSpaceSetNumComponents", sp->Nc, &sp->Nc, NULL, 0));
  PetscTryTypeMethod(sp, setfromoptions, PetscOptionsObject);
  /* process any options handlers added with PetscObjectAddOptionsHandler() */
  PetscCall(PetscObjectProcessOptionsHandlers((PetscObject)sp, PetscOptionsObject));
  PetscOptionsEnd();
  PetscCall(PetscSpaceViewFromOptions(sp, NULL, "-petscspace_view"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscSpaceSetUp - Construct data structures for the `PetscSpace`

  Collective

  Input Parameter:
. sp - the `PetscSpace` object to setup

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceView()`, `PetscSpaceDestroy()`
@*/
PetscErrorCode PetscSpaceSetUp(PetscSpace sp)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscTryTypeMethod(sp, setup);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceDestroy - Destroys a `PetscSpace` object

  Collective

  Input Parameter:
. sp - the `PetscSpace` object to destroy

  Level: beginner

.seealso: `PetscSpace`, `PetscSpaceCreate()`
@*/
PetscErrorCode PetscSpaceDestroy(PetscSpace *sp)
{
  PetscFunctionBegin;
  if (!*sp) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific((*sp), PETSCSPACE_CLASSID, 1);

  if (--((PetscObject)(*sp))->refct > 0) {
    *sp = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  ((PetscObject)(*sp))->refct = 0;
  PetscCall(DMDestroy(&(*sp)->dm));

  PetscCall((*(*sp)->ops->destroy)(*sp));
  PetscCall(PetscHeaderDestroy(sp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceCreate - Creates an empty `PetscSpace` object. The type can then be set with `PetscSpaceSetType()`.

  Collective

  Input Parameter:
. comm - The communicator for the `PetscSpace` object

  Output Parameter:
. sp - The `PetscSpace` object

  Level: beginner

.seealso: `PetscSpace`, `PetscSpaceSetType()`, `PETSCSPACEPOLYNOMIAL`
@*/
PetscErrorCode PetscSpaceCreate(MPI_Comm comm, PetscSpace *sp)
{
  PetscSpace s;

  PetscFunctionBegin;
  PetscValidPointer(sp, 2);
  PetscCall(PetscCitationsRegister(FECitation, &FEcite));
  *sp = NULL;
  PetscCall(PetscFEInitializePackage());

  PetscCall(PetscHeaderCreate(s, PETSCSPACE_CLASSID, "PetscSpace", "Linear Space", "PetscSpace", comm, PetscSpaceDestroy, PetscSpaceView));

  s->degree    = 0;
  s->maxDegree = PETSC_DETERMINE;
  s->Nc        = 1;
  s->Nv        = 0;
  s->dim       = PETSC_DETERMINE;
  PetscCall(DMShellCreate(comm, &s->dm));
  PetscCall(PetscSpaceSetType(s, PETSCSPACEPOLYNOMIAL));

  *sp = s;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceGetDimension - Return the dimension of this space, i.e. the number of basis vectors

  Input Parameter:
. sp - The `PetscSpace`

  Output Parameter:
. dim - The dimension

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceGetDegree()`, `PetscSpaceCreate()`, `PetscSpace`
@*/
PetscErrorCode PetscSpaceGetDimension(PetscSpace sp, PetscInt *dim)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscValidIntPointer(dim, 2);
  if (sp->dim == PETSC_DETERMINE) PetscTryTypeMethod(sp, getdimension, &sp->dim);
  *dim = sp->dim;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceGetDegree - Return the polynomial degrees that characterize this space

  Input Parameter:
. sp - The `PetscSpace`

  Output Parameters:
+ minDegree - The degree of the largest polynomial space contained in the space
- maxDegree - The degree of the smallest polynomial space containing the space

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceSetDegree()`, `PetscSpaceGetDimension()`, `PetscSpaceCreate()`, `PetscSpace`
@*/
PetscErrorCode PetscSpaceGetDegree(PetscSpace sp, PetscInt *minDegree, PetscInt *maxDegree)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  if (minDegree) PetscValidIntPointer(minDegree, 2);
  if (maxDegree) PetscValidIntPointer(maxDegree, 3);
  if (minDegree) *minDegree = sp->degree;
  if (maxDegree) *maxDegree = sp->maxDegree;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceSetDegree - Set the degree of approximation for this space.

  Input Parameters:
+ sp - The `PetscSpace`
. degree - The degree of the largest polynomial space contained in the space
- maxDegree - The degree of the largest polynomial space containing the space.  One of degree and maxDegree can be `PETSC_DETERMINE`.

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceGetDegree()`, `PetscSpaceCreate()`, `PetscSpace`
@*/
PetscErrorCode PetscSpaceSetDegree(PetscSpace sp, PetscInt degree, PetscInt maxDegree)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  sp->degree    = degree;
  sp->maxDegree = maxDegree;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceGetNumComponents - Return the number of components for this space

  Input Parameter:
. sp - The `PetscSpace`

  Output Parameter:
. Nc - The number of components

  Level: intermediate

  Note:
  A vector space, for example, will have d components, where d is the spatial dimension

.seealso: `PetscSpace`, `PetscSpaceSetNumComponents()`, `PetscSpaceGetNumVariables()`, `PetscSpaceGetDimension()`, `PetscSpaceCreate()`, `PetscSpace`
@*/
PetscErrorCode PetscSpaceGetNumComponents(PetscSpace sp, PetscInt *Nc)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscValidIntPointer(Nc, 2);
  *Nc = sp->Nc;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceSetNumComponents - Set the number of components for this space

  Input Parameters:
+ sp - The `PetscSpace`
- order - The number of components

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceGetNumComponents()`, `PetscSpaceSetNumVariables()`, `PetscSpaceCreate()`, `PetscSpace`
@*/
PetscErrorCode PetscSpaceSetNumComponents(PetscSpace sp, PetscInt Nc)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  sp->Nc = Nc;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceSetNumVariables - Set the number of variables for this space

  Input Parameters:
+ sp - The `PetscSpace`
- n - The number of variables, e.g. x, y, z...

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceGetNumVariables()`, `PetscSpaceSetNumComponents()`, `PetscSpaceCreate()`, `PetscSpace`
@*/
PetscErrorCode PetscSpaceSetNumVariables(PetscSpace sp, PetscInt n)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  sp->Nv = n;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceGetNumVariables - Return the number of variables for this space

  Input Parameter:
. sp - The `PetscSpace`

  Output Parameter:
. Nc - The number of variables, e.g. x, y, z...

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceSetNumVariables()`, `PetscSpaceGetNumComponents()`, `PetscSpaceGetDimension()`, `PetscSpaceCreate()`, `PetscSpace`
@*/
PetscErrorCode PetscSpaceGetNumVariables(PetscSpace sp, PetscInt *n)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscValidIntPointer(n, 2);
  *n = sp->Nv;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscSpaceEvaluate - Evaluate the basis functions and their derivatives (jet) at each point

  Input Parameters:
+ sp      - The `PetscSpace`
. npoints - The number of evaluation points, in reference coordinates
- points  - The point coordinates

  Output Parameters:
+ B - The function evaluations in a npoints x nfuncs array
. D - The derivative evaluations in a npoints x nfuncs x dim array
- H - The second derivative evaluations in a npoints x nfuncs x dim x dim array

  Level: beginner

  Note:
  Above nfuncs is the dimension of the space, and dim is the spatial dimension. The coordinates are given
  on the reference cell, not in real space.

.seealso: `PetscSpace`, `PetscFECreateTabulation()`, `PetscFEGetCellTabulation()`, `PetscSpaceCreate()`
@*/
PetscErrorCode PetscSpaceEvaluate(PetscSpace sp, PetscInt npoints, const PetscReal points[], PetscReal B[], PetscReal D[], PetscReal H[])
{
  PetscFunctionBegin;
  if (!npoints) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  if (sp->Nv) PetscValidRealPointer(points, 3);
  if (B) PetscValidRealPointer(B, 4);
  if (D) PetscValidRealPointer(D, 5);
  if (H) PetscValidRealPointer(H, 6);
  PetscTryTypeMethod(sp, evaluate, npoints, points, B, D, H);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceGetHeightSubspace - Get the subset of the primal space basis that is supported on a mesh point of a given height.

  Not Collective

  Input Parameters:
+ sp - the `PetscSpace` object
- height - the height of the mesh point for which the subspace is desired

  Output Parameter:
. subsp - the subspace

  Level: advanced

  Notes:
  If the space is not defined on mesh points of the given height (e.g. if the space is discontinuous and
  pointwise values are not defined on the element boundaries), or if the implementation of `PetscSpace` does not
  support extracting subspaces, then NULL is returned.

  This does not increment the reference count on the returned space, and the user should not destroy it.

.seealso: `PetscDualSpaceGetHeightSubspace()`, `PetscSpace`
@*/
PetscErrorCode PetscSpaceGetHeightSubspace(PetscSpace sp, PetscInt height, PetscSpace *subsp)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscValidPointer(subsp, 3);
  *subsp = NULL;
  PetscTryTypeMethod(sp, getheightsubspace, height, subsp);
  PetscFunctionReturn(PETSC_SUCCESS);
}


#include <petsc/private/isimpl.h> /*I "petscis.h"  I*/

PetscFunctionList ISList              = NULL;
PetscBool         ISRegisterAllCalled = PETSC_FALSE;

/*@
   ISCreate - Creates an index set object. `IS` are objects used to do efficient indexing into other data structures such as `Vec` and `Mat`

   Collective

   Input Parameter:
.  comm - the MPI communicator

   Output Parameter:
.  is - the new index set

   Level: beginner

   Notes:
   When the communicator is not `MPI_COMM_SELF`, the operations on `is` are NOT
   conceptually the same as `MPI_Group` operations. The `IS` are then
   distributed sets of indices and thus certain operations on them are
   collective.

.seealso: [](sec_scatter), `IS`, `ISType()`, `ISSetType()`, `ISCreateGeneral()`, `ISCreateStride()`, `ISCreateBlock()`, `ISAllGather()`
@*/
PetscErrorCode ISCreate(MPI_Comm comm, IS *is)
{
  PetscFunctionBegin;
  PetscValidPointer(is, 2);
  PetscCall(ISInitializePackage());

  PetscCall(PetscHeaderCreate(*is, IS_CLASSID, "IS", "Index Set", "IS", comm, ISDestroy, ISView));
  PetscCall(PetscLayoutCreate(comm, &(*is)->map));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  ISSetType - Builds a index set, for a particular `ISType`

  Collective

  Input Parameters:
+ is    - The index set object
- method - The name of the index set type

  Options Database Key:
. -is_type <type> - Sets the index set type; use `-help` for a list of available types

  Level: intermediate

  Notes:
  See `ISType` for available types (for instance, `ISGENERAL`, `ISSTRIDE`, or `ISBLOCK`).

  Often convenience constructors such as `ISCreateGeneral()`, `ISCreateStride()` or  `ISCreateBlock()` can be used to construct the desired `IS` in one step

  Use `ISDuplicate()` to make a duplicate

.seealso: [](sec_scatter), `IS`, `ISGENERAL`, `ISBLOCK`, `ISGetType()`, `ISCreate()`, `ISCreateGeneral()`, `ISCreateStride()`, `ISCreateBlock()`
@*/
PetscErrorCode ISSetType(IS is, ISType method)
{
  PetscErrorCode (*r)(IS);
  PetscBool match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is, IS_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)is, method, &match));
  if (match) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCall(ISRegisterAll());
  PetscCall(PetscFunctionListFind(ISList, method, &r));
  PetscCheck(r, PETSC_COMM_SELF, PETSC_ERR_ARG_UNKNOWN_TYPE, "Unknown IS type: %s", method);
  PetscTryTypeMethod(is, destroy);
  is->ops->destroy = NULL;

  PetscCall((*r)(is));
  PetscCall(PetscObjectChangeTypeName((PetscObject)is, method));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  ISGetType - Gets the index set type name, `ISType`, (as a string) from the `IS`.

  Not Collective

  Input Parameter:
. is  - The index set

  Output Parameter:
. type - The index set type name

  Level: intermediate

.seealso: [](sec_scatter), `IS`, `ISType`, `ISSetType()`, `ISCreate()`
@*/
PetscErrorCode ISGetType(IS is, ISType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(is, IS_CLASSID, 1);
  PetscValidPointer(type, 2);
  if (!ISRegisterAllCalled) PetscCall(ISRegisterAll());
  *type = ((PetscObject)is)->type_name;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*@C
  ISRegister - Adds a new index set implementation

  Not Collective

  Input Parameters:
+ sname        - The name of a new user-defined creation routine
- function - The creation routine itself

  Sample usage:
.vb
    ISRegister("my_is_name",  MyISCreate);
.ve

  Then, your vector type can be chosen with the procedural interface via
.vb
    ISCreate(MPI_Comm, IS *);
    ISSetType(IS,"my_is_name");
.ve
   or at runtime via the option
.vb
    -is_type my_is_name
.ve

  Level: developer

  Notes:
  `ISRegister()` may be called multiple times to add several user-defined vectors

  This is no `ISSetFromOptions()` and the current implementations do not have a way to dynamically determine type, so
  dynamic registration of custom `IS` types will be of limited use to users.

.seealso: [](sec_scatter), `IS`, `ISType`, `ISSetType()`, `ISRegisterAll()`, `ISRegisterDestroy()`, `ISRegister()`
@*/
PetscErrorCode ISRegister(const char sname[], PetscErrorCode (*function)(IS))
{
  PetscFunctionBegin;
  PetscCall(ISInitializePackage());
  PetscCall(PetscFunctionListAdd(&ISList, sname, function));
  PetscFunctionReturn(PETSC_SUCCESS);
}

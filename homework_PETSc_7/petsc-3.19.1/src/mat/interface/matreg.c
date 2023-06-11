
/*
     Mechanism for register PETSc matrix types
*/
#include <petsc/private/matimpl.h> /*I "petscmat.h" I*/

PetscBool MatRegisterAllCalled = PETSC_FALSE;

/*
   Contains the list of registered Mat routines
*/
PetscFunctionList MatList = NULL;

/* MatGetRootType_Private - Gets the root type of the input matrix's type (e.g., MATAIJ for MATSEQAIJ)

   Not Collective

   Input Parameter:
.  mat      - the input matrix, could be sequential or MPI

   Output Parameter:
.  rootType  - the root matrix type

   Level: developer

.seealso: `MatGetType()`, `MatSetType()`, `MatType`, `Mat`
*/
PetscErrorCode MatGetRootType_Private(Mat mat, MatType *rootType)
{
  PetscBool   found = PETSC_FALSE;
  MatRootName names = MatRootNameList;
  MatType     inType;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscCall(MatGetType(mat, &inType));
  while (names) {
    PetscCall(PetscStrcmp(inType, names->mname, &found));
    if (!found) PetscCall(PetscStrcmp(inType, names->sname, &found));
    if (found) {
      found     = PETSC_TRUE;
      *rootType = names->rname;
      break;
    }
    names = names->next;
  }
  if (!found) *rootType = inType;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* MatGetMPIMatType_Private - Gets the MPI type corresponding to the input matrix's type (e.g., MATMPIAIJ for MATSEQAIJ)

   Not Collective

   Input Parameter:
.  mat      - the input matrix, could be sequential or MPI

   Output Parameter:
.  MPIType  - the parallel (MPI) matrix type

   Level: developer

.seealso: `MatGetType()`, `MatSetType()`, `MatType`, `Mat`
*/
PetscErrorCode MatGetMPIMatType_Private(Mat mat, MatType *MPIType)
{
  PetscBool   found = PETSC_FALSE;
  MatRootName names = MatRootNameList;
  MatType     inType;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscCall(MatGetType(mat, &inType));
  while (names) {
    PetscCall(PetscStrcmp(inType, names->sname, &found));
    if (!found) PetscCall(PetscStrcmp(inType, names->mname, &found));
    if (!found) PetscCall(PetscStrcmp(inType, names->rname, &found));
    if (found) {
      found    = PETSC_TRUE;
      *MPIType = names->mname;
      break;
    }
    names = names->next;
  }
  PetscCheck(found, PETSC_COMM_SELF, PETSC_ERR_SUP, "No corresponding parallel (MPI) type for this matrix");
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatSetType - Builds matrix object for a particular matrix type

   Collective

   Input Parameters:
+  mat      - the matrix object
-  matype   - matrix type

   Options Database Key:
.  -mat_type  <method> - Sets the type; see `MatType`

  Level: intermediate

   Note:
   See `MatType` for possible values

.seealso: [](chapter_matrices), `Mat`, `PCSetType()`, `VecSetType()`, `MatCreate()`, `MatType`, `Mat`
@*/
PetscErrorCode MatSetType(Mat mat, MatType matype)
{
  PetscBool   sametype, found, subclass = PETSC_FALSE, matMPI = PETSC_FALSE, requestSeq = PETSC_FALSE;
  MatRootName names = MatRootNameList;
  PetscErrorCode (*r)(Mat);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);

  /* make this special case fast */
  PetscCall(PetscObjectTypeCompare((PetscObject)mat, matype, &sametype));
  if (sametype) PetscFunctionReturn(PETSC_SUCCESS);

  /* suppose with one MPI process, one created an MPIAIJ (mpiaij) matrix with MatCreateMPIAIJWithArrays(), and later tried
     to change its type via '-mat_type aijcusparse'. Even there is only one MPI rank, we need to adapt matype to
     'mpiaijcusparse' so that it will be treated as a subclass of MPIAIJ and proper MatCovert() will be called.
  */
  if (((PetscObject)mat)->type_name) PetscCall(PetscStrbeginswith(((PetscObject)mat)->type_name, "mpi", &matMPI)); /* mat claims itself is an 'mpi' matrix */
  if (matype) PetscCall(PetscStrbeginswith(matype, "seq", &requestSeq));                                           /* user is requesting a 'seq' matrix */
  PetscCheck(!(matMPI && requestSeq), PetscObjectComm((PetscObject)mat), PETSC_ERR_SUP, "Changing an MPI matrix (%s) to a sequential one (%s) is not allowed. Please remove the 'seq' prefix from your matrix type.", ((PetscObject)mat)->type_name, matype);

  while (names) {
    PetscCall(PetscStrcmp(matype, names->rname, &found));
    if (found) {
      PetscMPIInt size;
      PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)mat), &size));
      if (size == 1 && !matMPI) matype = names->sname; /* try to align the requested type (matype) with the existing type per seq/mpi */
      else matype = names->mname;
      break;
    }
    names = names->next;
  }

  PetscCall(PetscObjectTypeCompare((PetscObject)mat, matype, &sametype));
  if (sametype) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCall(PetscFunctionListFind(MatList, matype, &r));
  PetscCheck(r, PETSC_COMM_SELF, PETSC_ERR_ARG_UNKNOWN_TYPE, "Unknown Mat type given: %s", matype);

  if (mat->assembled && ((PetscObject)mat)->type_name) PetscCall(PetscStrbeginswith(matype, ((PetscObject)mat)->type_name, &subclass));
  if (subclass) { /* mat is a subclass of the requested 'matype'? */
    PetscCall(MatConvert(mat, matype, MAT_INPLACE_MATRIX, &mat));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscTryTypeMethod(mat, destroy);
  mat->ops->destroy = NULL;

  /* should these null spaces be removed? */
  PetscCall(MatNullSpaceDestroy(&mat->nullsp));
  PetscCall(MatNullSpaceDestroy(&mat->nearnullsp));

  PetscCall(PetscMemzero(mat->ops, sizeof(struct _MatOps)));
  mat->preallocated  = PETSC_FALSE;
  mat->assembled     = PETSC_FALSE;
  mat->was_assembled = PETSC_FALSE;

  /*
   Increment, rather than reset these: the object is logically the same, so its logging and
   state is inherited.  Furthermore, resetting makes it possible for the same state to be
   obtained with a different structure, confusing the PC.
  */
  mat->nonzerostate++;
  PetscCall(PetscObjectStateIncrease((PetscObject)mat));

  /* create the new data structure */
  PetscCall((*r)(mat));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatGetType - Gets the matrix type as a string from the matrix object.

   Not Collective

   Input Parameter:
.  mat - the matrix

   Output Parameter:
.  name - name of matrix type

   Level: intermediate

.seealso: [](chapter_matrices), `Mat`, `MatType`, `MatSetType()`
@*/
PetscErrorCode MatGetType(Mat mat, MatType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscValidPointer(type, 2);
  *type = ((PetscObject)mat)->type_name;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatGetVecType - Gets the vector type the matrix will return with `MatCreateVecs()`

   Not Collective

   Input Parameter:
.  mat - the matrix

   Output Parameter:
.  name - name of vector type

   Level: intermediate

.seealso: [](chapter_matrices), `Mat`, `MatType`, `Mat`, `MatSetVecType()`, `VecType`
@*/
PetscErrorCode MatGetVecType(Mat mat, VecType *vtype)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscValidPointer(vtype, 2);
  *vtype = mat->defaultvectype;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatSetVecType - Set the vector type the matrix will return with `MatCreateVecs()`

   Collective

   Input Parameters:
+  mat   - the matrix object
-  vtype - vector type

  Level: advanced

   Note:
     This is rarely needed in practice since each matrix object internally sets the proper vector type.

.seealso: [](chapter_matrices), `Mat`, `VecType`, `VecSetType()`, `MatGetVecType()`
@*/
PetscErrorCode MatSetVecType(Mat mat, VecType vtype)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscCall(PetscFree(mat->defaultvectype));
  PetscCall(PetscStrallocpy(vtype, &mat->defaultvectype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  MatRegister -  - Adds a new matrix type implementation

   Not Collective

   Input Parameters:
+  sname - name of a new user-defined matrix type
-  function - routine to create method context

   Level: advanced

   Note:
   `MatRegister()` may be called multiple times to add several user-defined solvers.

   Sample usage:
.vb
   MatRegister("my_mat",MyMatCreate);
.ve

   Then, your solver can be chosen with the procedural interface via
$     MatSetType(Mat,"my_mat")
   or at runtime via the option
$     -mat_type my_mat

.seealso: [](chapter_matrices), `Mat`, `MatType`, `MatSetType()`, `MatRegisterAll()`
@*/
PetscErrorCode MatRegister(const char sname[], PetscErrorCode (*function)(Mat))
{
  PetscFunctionBegin;
  PetscCall(MatInitializePackage());
  PetscCall(PetscFunctionListAdd(&MatList, sname, function));
  PetscFunctionReturn(PETSC_SUCCESS);
}

MatRootName MatRootNameList = NULL;

/*@C
      MatRegisterRootName - Registers a name that can be used for either a sequential or its corresponding parallel matrix type. `MatSetType()`
        and `-mat_type name` will automatically use the sequential or parallel version based on the size of the MPI communicator associated with the
        matrix.

  Input Parameters:
+     rname - the rootname, for example, `MATAIJ`
.     sname - the name of the sequential matrix type, for example, `MATSEQAIJ`
-     mname - the name of the parallel matrix type, for example, `MATMPIAIJ`

  Level: developer

  Note:
  The matrix rootname should not be confused with the base type of the function `PetscObjectBaseTypeCompare()`

  Developer Note:
  PETSc vectors have a similar rootname that indicates PETSc should automatically select the appropriate `VecType` based on the
      size of the communicator but it is implemented by simply having additional `VecCreate_RootName()` registerer routines that dispatch to the
      appropriate creation routine. Why have two different ways of implementing the same functionality for different types of objects? It is
      confusing.

.seealso: [](chapter_matrices), `Mat`, `MatType`, `PetscObjectBaseTypeCompare()`
@*/
PetscErrorCode MatRegisterRootName(const char rname[], const char sname[], const char mname[])
{
  MatRootName names;

  PetscFunctionBegin;
  PetscCall(PetscNew(&names));
  PetscCall(PetscStrallocpy(rname, &names->rname));
  PetscCall(PetscStrallocpy(sname, &names->sname));
  PetscCall(PetscStrallocpy(mname, &names->mname));
  if (!MatRootNameList) {
    MatRootNameList = names;
  } else {
    MatRootName next = MatRootNameList;
    while (next->next) next = next->next;
    next->next = names;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

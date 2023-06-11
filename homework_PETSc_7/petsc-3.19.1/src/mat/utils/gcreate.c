#include <petsc/private/matimpl.h> /*I "petscmat.h"  I*/

PETSC_INTERN PetscErrorCode MatSetBlockSizes_Default(Mat mat, PetscInt rbs, PetscInt cbs)
{
  PetscFunctionBegin;
  if (!mat->preallocated) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCheck(mat->rmap->bs <= 0 || mat->rmap->bs == rbs, PetscObjectComm((PetscObject)mat), PETSC_ERR_SUP, "Cannot change row block size %" PetscInt_FMT " to %" PetscInt_FMT, mat->rmap->bs, rbs);
  PetscCheck(mat->cmap->bs <= 0 || mat->cmap->bs == cbs, PetscObjectComm((PetscObject)mat), PETSC_ERR_SUP, "Cannot change column block size %" PetscInt_FMT " to %" PetscInt_FMT, mat->cmap->bs, cbs);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode MatShift_Basic(Mat Y, PetscScalar a)
{
  PetscInt    i, start, end;
  PetscScalar alpha = a;
  PetscBool   prevoption;

  PetscFunctionBegin;
  PetscCall(MatGetOption(Y, MAT_NO_OFF_PROC_ENTRIES, &prevoption));
  PetscCall(MatSetOption(Y, MAT_NO_OFF_PROC_ENTRIES, PETSC_TRUE));
  PetscCall(MatGetOwnershipRange(Y, &start, &end));
  for (i = start; i < end; i++) {
    if (i < Y->cmap->N) PetscCall(MatSetValues(Y, 1, &i, 1, &i, &alpha, ADD_VALUES));
  }
  PetscCall(MatAssemblyBegin(Y, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(Y, MAT_FINAL_ASSEMBLY));
  PetscCall(MatSetOption(Y, MAT_NO_OFF_PROC_ENTRIES, prevoption));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatCreate - Creates a matrix where the type is determined
   from either a call to `MatSetType()` or from the options database
   with a call to `MatSetFromOptions()`. The default matrix type is
   `MATAIJ`, using the routines `MatCreateSeqAIJ()` or `MatCreateAIJ()`
   if you do not set a type in the options database. If you never
   call `MatSetType()` or `MatSetFromOptions()` it will generate an
   error when you try to use the matrix.

   Collective

   Input Parameter:
.  comm - MPI communicator

   Output Parameter:
.  A - the matrix

   Options Database Keys:
+    -mat_type seqaij   - `MATSEQAIJ` type, uses `MatCreateSeqAIJ()`
.    -mat_type mpiaij   - `MATMPIAIJ` type, uses `MatCreateAIJ()`
.    -mat_type seqdense - `MATSEQDENSE`, uses `MatCreateSeqDense()`
.    -mat_type mpidense - `MATMPIDENSE` type, uses `MatCreateDense()`
.    -mat_type seqbaij  - `MATSEQBAIJ` type, uses `MatCreateSeqBAIJ()`
-    -mat_type mpibaij  - `MATMPIBAIJ` type, uses `MatCreateBAIJ()`

   See the manpages for particular formats (e.g., `MATSEQAIJ`)
   for additional format-specific options.

   Level: beginner

.seealso: [](chapter_matrices), `Mat`, `MatCreateSeqAIJ()`, `MatCreateAIJ()`,
          `MatCreateSeqDense()`, `MatCreateDense()`,
          `MatCreateSeqBAIJ()`, `MatCreateBAIJ()`,
          `MatCreateSeqSBAIJ()`, `MatCreateSBAIJ()`,
          `MatConvert()`
@*/
PetscErrorCode MatCreate(MPI_Comm comm, Mat *A)
{
  Mat B;

  PetscFunctionBegin;
  PetscValidPointer(A, 2);

  *A = NULL;
  PetscCall(MatInitializePackage());

  PetscCall(PetscHeaderCreate(B, MAT_CLASSID, "Mat", "Matrix", "Mat", comm, MatDestroy, MatView));
  PetscCall(PetscLayoutCreate(comm, &B->rmap));
  PetscCall(PetscLayoutCreate(comm, &B->cmap));
  PetscCall(PetscStrallocpy(VECSTANDARD, &B->defaultvectype));
  PetscCall(PetscStrallocpy(PETSCRANDER48, &B->defaultrandtype));

  B->symmetric                   = PETSC_BOOL3_UNKNOWN;
  B->hermitian                   = PETSC_BOOL3_UNKNOWN;
  B->structurally_symmetric      = PETSC_BOOL3_UNKNOWN;
  B->spd                         = PETSC_BOOL3_UNKNOWN;
  B->symmetry_eternal            = PETSC_FALSE;
  B->structural_symmetry_eternal = PETSC_FALSE;

  B->congruentlayouts = PETSC_DECIDE;
  B->preallocated     = PETSC_FALSE;
#if defined(PETSC_HAVE_DEVICE)
  B->boundtocpu = PETSC_TRUE;
#endif
  *A = B;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatSetErrorIfFailure - Causes `Mat` to generate an immediate error, for example a zero pivot, is detected.

   Logically Collective

   Input Parameters:
+  mat -  matrix obtained from `MatCreate()`
-  flg - `PETSC_TRUE` indicates you want the error generated

   Level: advanced

   Note:
   If this flag is not set then the matrix operation will note the error and continue. The error may cause a later `PC` or `KSP` error
   or result in a `KSPConvergedReason` indicating the method did not converge.

.seealso: [](chapter_matrices), `Mat`, `PCSetErrorIfFailure()`, `KSPConvergedReason`, `SNESConvergedReason`
@*/
PetscErrorCode MatSetErrorIfFailure(Mat mat, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscValidLogicalCollectiveBool(mat, flg, 2);
  mat->erroriffailure = flg;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  MatSetSizes - Sets the local and global sizes, and checks to determine compatibility

  Collective

  Input Parameters:
+  A - the matrix
.  m - number of local rows (or `PETSC_DECIDE`)
.  n - number of local columns (or `PETSC_DECIDE`)
.  M - number of global rows (or `PETSC_DETERMINE`)
-  N - number of global columns (or `PETSC_DETERMINE`)

  Level: beginner

   Notes:
   `m` (`n`) and `M` (`N`) cannot be both `PETSC_DECIDE`
   If one processor calls this with `M` (`N`) of `PETSC_DECIDE` then all processors must, otherwise the program will hang.

   If `PETSC_DECIDE` is not used for the arguments 'm' and 'n', then the
   user must ensure that they are chosen to be compatible with the
   vectors. To do this, one first considers the matrix-vector product
   'y = A x'. The `m` that is used in the above routine must match the
   local size used in the vector creation routine `VecCreateMPI()` for 'y'.
   Likewise, the `n` used must match that used as the local size in
   `VecCreateMPI()` for 'x'.

   You cannot change the sizes once they have been set.

   The sizes must be set before `MatSetUp()` or MatXXXSetPreallocation() is called.

.seealso: [](chapter_matrices), `Mat`, `MatGetSize()`, `PetscSplitOwnership()`
@*/
PetscErrorCode MatSetSizes(Mat A, PetscInt m, PetscInt n, PetscInt M, PetscInt N)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidLogicalCollectiveInt(A, M, 4);
  PetscValidLogicalCollectiveInt(A, N, 5);
  PetscCheck(M <= 0 || m <= M, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Local row size %" PetscInt_FMT " cannot be larger than global row size %" PetscInt_FMT, m, M);
  PetscCheck(N <= 0 || n <= N, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Local column size %" PetscInt_FMT " cannot be larger than global column size %" PetscInt_FMT, n, N);
  PetscCheck((A->rmap->n < 0 || A->rmap->N < 0) || (A->rmap->n == m && (M <= 0 || A->rmap->N == M)), PETSC_COMM_SELF, PETSC_ERR_SUP, "Cannot change/reset row sizes to %" PetscInt_FMT " local %" PetscInt_FMT " global after previously setting them to %" PetscInt_FMT " local %" PetscInt_FMT " global", m, M,
             A->rmap->n, A->rmap->N);
  PetscCheck((A->cmap->n < 0 || A->cmap->N < 0) || (A->cmap->n == n && (N <= 0 || A->cmap->N == N)), PETSC_COMM_SELF, PETSC_ERR_SUP, "Cannot change/reset column sizes to %" PetscInt_FMT " local %" PetscInt_FMT " global after previously setting them to %" PetscInt_FMT " local %" PetscInt_FMT " global", n, N,
             A->cmap->n, A->cmap->N);
  A->rmap->n = m;
  A->cmap->n = n;
  A->rmap->N = M > -1 ? M : A->rmap->N;
  A->cmap->N = N > -1 ? N : A->cmap->N;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatSetFromOptions - Creates a matrix where the type is determined
   from the options database. Generates a parallel MPI matrix if the
   communicator has more than one processor.  The default matrix type is
   `MATAIJ`, using the routines `MatCreateSeqAIJ()` and `MatCreateAIJ()` if
   you do not select a type in the options database.

   Collective

   Input Parameter:
.  A - the matrix

   Options Database Keys:
+    -mat_type seqaij   - `MATSEQAIJ` type, uses `MatCreateSeqAIJ()`
.    -mat_type mpiaij   - `MATMPIAIJ` type, uses `MatCreateAIJ()`
.    -mat_type seqdense - `MATSEQDENSE` type, uses `MatCreateSeqDense()`
.    -mat_type mpidense - `MATMPIDENSE`, uses `MatCreateDense()`
.    -mat_type seqbaij  - `MATSEQBAIJ`, uses `MatCreateSeqBAIJ()`
-    -mat_type mpibaij  - `MATMPIBAIJ`, uses `MatCreateBAIJ()`

   See the manpages for particular formats (e.g., `MATSEQAIJ`)
   for additional format-specific options.

   Level: beginner

.seealso: [](chapter_matrices), `Mat`, `MatCreateSeqAIJ()`, `MatCreateAIJ()`,
          `MatCreateSeqDense()`, `MatCreateDense()`,
          `MatCreateSeqBAIJ()`, `MatCreateBAIJ()`,
          `MatCreateSeqSBAIJ()`, `MatCreateSBAIJ()`,
          `MatConvert()`
@*/
PetscErrorCode MatSetFromOptions(Mat B)
{
  const char *deft = MATAIJ;
  char        type[256];
  PetscBool   flg, set;
  PetscInt    bind_below = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(B, MAT_CLASSID, 1);

  PetscObjectOptionsBegin((PetscObject)B);

  if (B->rmap->bs < 0) {
    PetscInt newbs = -1;
    PetscCall(PetscOptionsInt("-mat_block_size", "Set the blocksize used to store the matrix", "MatSetBlockSize", newbs, &newbs, &flg));
    if (flg) {
      PetscCall(PetscLayoutSetBlockSize(B->rmap, newbs));
      PetscCall(PetscLayoutSetBlockSize(B->cmap, newbs));
    }
  }

  PetscCall(PetscOptionsFList("-mat_type", "Matrix type", "MatSetType", MatList, deft, type, 256, &flg));
  if (flg) {
    PetscCall(MatSetType(B, type));
  } else if (!((PetscObject)B)->type_name) {
    PetscCall(MatSetType(B, deft));
  }

  PetscCall(PetscOptionsName("-mat_is_symmetric", "Checks if mat is symmetric on MatAssemblyEnd()", "MatIsSymmetric", &B->checksymmetryonassembly));
  PetscCall(PetscOptionsReal("-mat_is_symmetric", "Checks if mat is symmetric on MatAssemblyEnd()", "MatIsSymmetric", B->checksymmetrytol, &B->checksymmetrytol, NULL));
  PetscCall(PetscOptionsBool("-mat_null_space_test", "Checks if provided null space is correct in MatAssemblyEnd()", "MatSetNullSpaceTest", B->checknullspaceonassembly, &B->checknullspaceonassembly, NULL));
  PetscCall(PetscOptionsBool("-mat_error_if_failure", "Generate an error if an error occurs when factoring the matrix", "MatSetErrorIfFailure", B->erroriffailure, &B->erroriffailure, NULL));

  PetscTryTypeMethod(B, setfromoptions, PetscOptionsObject);

  flg = PETSC_FALSE;
  PetscCall(PetscOptionsBool("-mat_new_nonzero_location_err", "Generate an error if new nonzeros are created in the matrix structure (useful to test preallocation)", "MatSetOption", flg, &flg, &set));
  if (set) PetscCall(MatSetOption(B, MAT_NEW_NONZERO_LOCATION_ERR, flg));
  flg = PETSC_FALSE;
  PetscCall(PetscOptionsBool("-mat_new_nonzero_allocation_err", "Generate an error if new nonzeros are allocated in the matrix structure (useful to test preallocation)", "MatSetOption", flg, &flg, &set));
  if (set) PetscCall(MatSetOption(B, MAT_NEW_NONZERO_ALLOCATION_ERR, flg));
  flg = PETSC_FALSE;
  PetscCall(PetscOptionsBool("-mat_ignore_zero_entries", "For AIJ/IS matrices this will stop zero values from creating a zero location in the matrix", "MatSetOption", flg, &flg, &set));
  if (set) PetscCall(MatSetOption(B, MAT_IGNORE_ZERO_ENTRIES, flg));

  flg = PETSC_FALSE;
  PetscCall(PetscOptionsBool("-mat_form_explicit_transpose", "Hint to form an explicit transpose for operations like MatMultTranspose", "MatSetOption", flg, &flg, &set));
  if (set) PetscCall(MatSetOption(B, MAT_FORM_EXPLICIT_TRANSPOSE, flg));

  /* Bind to CPU if below a user-specified size threshold.
   * This perhaps belongs in the options for the GPU Mat types, but MatBindToCPU() does nothing when called on non-GPU types,
   * and putting it here makes is more maintainable than duplicating this for all. */
  PetscCall(PetscOptionsInt("-mat_bind_below", "Set the size threshold (in local rows) below which the Mat is bound to the CPU", "MatBindToCPU", bind_below, &bind_below, &flg));
  if (flg && B->rmap->n < bind_below) PetscCall(MatBindToCPU(B, PETSC_TRUE));

  /* process any options handlers added with PetscObjectAddOptionsHandler() */
  PetscCall(PetscObjectProcessOptionsHandlers((PetscObject)B, PetscOptionsObject));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatXAIJSetPreallocation - set preallocation for serial and parallel `MATAIJ`, `MATBAIJ`, and `MATSBAIJ` matrices and their unassembled versions.

   Collective

   Input Parameters:
+  A - matrix being preallocated
.  bs - block size
.  dnnz - number of nonzero column blocks per block row of diagonal part of parallel matrix
.  onnz - number of nonzero column blocks per block row of off-diagonal part of parallel matrix
.  dnnzu - number of nonzero column blocks per block row of upper-triangular part of diagonal part of parallel matrix
-  onnzu - number of nonzero column blocks per block row of upper-triangular part of off-diagonal part of parallel matrix

   Level: beginner

.seealso: [](chapter_matrices), `Mat`, `MatSeqAIJSetPreallocation()`, `MatMPIAIJSetPreallocation()`, `MatSeqBAIJSetPreallocation()`, `MatMPIBAIJSetPreallocation()`,
          `MatSeqSBAIJSetPreallocation()`, `MatMPISBAIJSetPreallocation()`,
          `PetscSplitOwnership()`
@*/
PetscErrorCode MatXAIJSetPreallocation(Mat A, PetscInt bs, const PetscInt dnnz[], const PetscInt onnz[], const PetscInt dnnzu[], const PetscInt onnzu[])
{
  PetscInt cbs;
  void (*aij)(void);
  void (*is)(void);
  void (*hyp)(void) = NULL;

  PetscFunctionBegin;
  if (bs != PETSC_DECIDE) { /* don't mess with an already set block size */
    PetscCall(MatSetBlockSize(A, bs));
  }
  PetscCall(PetscLayoutSetUp(A->rmap));
  PetscCall(PetscLayoutSetUp(A->cmap));
  PetscCall(MatGetBlockSizes(A, &bs, &cbs));
  /* these routines assumes bs == cbs, this should be checked somehow */
  PetscCall(MatSeqBAIJSetPreallocation(A, bs, 0, dnnz));
  PetscCall(MatMPIBAIJSetPreallocation(A, bs, 0, dnnz, 0, onnz));
  PetscCall(MatSeqSBAIJSetPreallocation(A, bs, 0, dnnzu));
  PetscCall(MatMPISBAIJSetPreallocation(A, bs, 0, dnnzu, 0, onnzu));
  /*
    In general, we have to do extra work to preallocate for scalar (AIJ) or unassembled (IS) matrices so we check whether it will do any
    good before going on with it.
  */
  PetscCall(PetscObjectQueryFunction((PetscObject)A, "MatMPIAIJSetPreallocation_C", &aij));
  PetscCall(PetscObjectQueryFunction((PetscObject)A, "MatISSetPreallocation_C", &is));
#if defined(PETSC_HAVE_HYPRE)
  PetscCall(PetscObjectQueryFunction((PetscObject)A, "MatHYPRESetPreallocation_C", &hyp));
#endif
  if (!aij && !is && !hyp) PetscCall(PetscObjectQueryFunction((PetscObject)A, "MatSeqAIJSetPreallocation_C", &aij));
  if (aij || is || hyp) {
    if (bs == cbs && bs == 1) {
      PetscCall(MatSeqAIJSetPreallocation(A, 0, dnnz));
      PetscCall(MatMPIAIJSetPreallocation(A, 0, dnnz, 0, onnz));
      PetscCall(MatISSetPreallocation(A, 0, dnnz, 0, onnz));
#if defined(PETSC_HAVE_HYPRE)
      PetscCall(MatHYPRESetPreallocation(A, 0, dnnz, 0, onnz));
#endif
    } else { /* Convert block-row precallocation to scalar-row */
      PetscInt i, m, *sdnnz, *sonnz;
      PetscCall(MatGetLocalSize(A, &m, NULL));
      PetscCall(PetscMalloc2((!!dnnz) * m, &sdnnz, (!!onnz) * m, &sonnz));
      for (i = 0; i < m; i++) {
        if (dnnz) sdnnz[i] = dnnz[i / bs] * cbs;
        if (onnz) sonnz[i] = onnz[i / bs] * cbs;
      }
      PetscCall(MatSeqAIJSetPreallocation(A, 0, dnnz ? sdnnz : NULL));
      PetscCall(MatMPIAIJSetPreallocation(A, 0, dnnz ? sdnnz : NULL, 0, onnz ? sonnz : NULL));
      PetscCall(MatISSetPreallocation(A, 0, dnnz ? sdnnz : NULL, 0, onnz ? sonnz : NULL));
#if defined(PETSC_HAVE_HYPRE)
      PetscCall(MatHYPRESetPreallocation(A, 0, dnnz ? sdnnz : NULL, 0, onnz ? sonnz : NULL));
#endif
      PetscCall(PetscFree2(sdnnz, sonnz));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
        Merges some information from Cs header to A; the C object is then destroyed

        This is somewhat different from MatHeaderReplace() it would be nice to merge the code
*/
PetscErrorCode MatHeaderMerge(Mat A, Mat *C)
{
  PetscInt         refct;
  PetscOps         Abops;
  struct _MatOps   Aops;
  char            *mtype, *mname, *mprefix;
  Mat_Product     *product;
  Mat_Redundant   *redundant;
  PetscObjectState state;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidHeaderSpecific(*C, MAT_CLASSID, 2);
  if (A == *C) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCheckSameComm(A, 1, *C, 2);
  /* save the parts of A we need */
  Abops     = ((PetscObject)A)->bops[0];
  Aops      = A->ops[0];
  refct     = ((PetscObject)A)->refct;
  mtype     = ((PetscObject)A)->type_name;
  mname     = ((PetscObject)A)->name;
  state     = ((PetscObject)A)->state;
  mprefix   = ((PetscObject)A)->prefix;
  product   = A->product;
  redundant = A->redundant;

  /* zero these so the destroy below does not free them */
  ((PetscObject)A)->type_name = NULL;
  ((PetscObject)A)->name      = NULL;

  /*
     free all the interior data structures from mat
     cannot use PetscUseTypeMethod(A,destroy); because compiler
     thinks it may print NULL type_name and name
  */
  PetscTryTypeMethod(A, destroy);

  PetscCall(PetscFree(A->defaultvectype));
  PetscCall(PetscFree(A->defaultrandtype));
  PetscCall(PetscLayoutDestroy(&A->rmap));
  PetscCall(PetscLayoutDestroy(&A->cmap));
  PetscCall(PetscFunctionListDestroy(&((PetscObject)A)->qlist));
  PetscCall(PetscObjectListDestroy(&((PetscObject)A)->olist));
  PetscCall(PetscComposedQuantitiesDestroy((PetscObject)A));

  /* copy C over to A */
  PetscCall(PetscFree(A->factorprefix));
  PetscCall(PetscMemcpy(A, *C, sizeof(struct _p_Mat)));

  /* return the parts of A we saved */
  ((PetscObject)A)->bops[0]   = Abops;
  A->ops[0]                   = Aops;
  ((PetscObject)A)->refct     = refct;
  ((PetscObject)A)->type_name = mtype;
  ((PetscObject)A)->name      = mname;
  ((PetscObject)A)->prefix    = mprefix;
  ((PetscObject)A)->state     = state + 1;
  A->product                  = product;
  A->redundant                = redundant;

  /* since these two are copied into A we do not want them destroyed in C */
  ((PetscObject)*C)->qlist = NULL;
  ((PetscObject)*C)->olist = NULL;

  PetscCall(PetscHeaderDestroy(C));
  PetscFunctionReturn(PETSC_SUCCESS);
}
/*
        Replace A's header with that of C; the C object is then destroyed

        This is essentially code moved from MatDestroy()

        This is somewhat different from MatHeaderMerge() it would be nice to merge the code

        Used in DM hence is declared PETSC_EXTERN
*/
PETSC_EXTERN PetscErrorCode MatHeaderReplace(Mat A, Mat *C)
{
  PetscInt         refct;
  PetscObjectState state;
  struct _p_Mat    buffer;
  MatStencilInfo   stencil;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidHeaderSpecific(*C, MAT_CLASSID, 2);
  if (A == *C) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCheckSameComm(A, 1, *C, 2);
  PetscCheck(((PetscObject)*C)->refct == 1, PetscObjectComm((PetscObject)C), PETSC_ERR_ARG_WRONGSTATE, "Object C has refct %" PetscInt_FMT " > 1, would leave hanging reference", ((PetscObject)*C)->refct);

  /* swap C and A */
  refct   = ((PetscObject)A)->refct;
  state   = ((PetscObject)A)->state;
  stencil = A->stencil;
  PetscCall(PetscMemcpy(&buffer, A, sizeof(struct _p_Mat)));
  PetscCall(PetscMemcpy(A, *C, sizeof(struct _p_Mat)));
  PetscCall(PetscMemcpy(*C, &buffer, sizeof(struct _p_Mat)));
  ((PetscObject)A)->refct = refct;
  ((PetscObject)A)->state = state + 1;
  A->stencil              = stencil;

  ((PetscObject)*C)->refct = 1;
  PetscCall(MatShellSetOperation(*C, MATOP_DESTROY, (void (*)(void))NULL));
  PetscCall(MatDestroy(C));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     MatBindToCPU - marks a matrix to temporarily stay on the CPU and perform computations on the CPU

   Logically Collective

   Input Parameters:
+   A - the matrix
-   flg - bind to the CPU if value of `PETSC_TRUE`

   Level: intermediate

.seealso: [](chapter_matrices), `Mat`, `MatBoundToCPU()`
@*/
PetscErrorCode MatBindToCPU(Mat A, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidLogicalCollectiveBool(A, flg, 2);
#if defined(PETSC_HAVE_DEVICE)
  if (A->boundtocpu == flg) PetscFunctionReturn(PETSC_SUCCESS);
  A->boundtocpu = flg;
  PetscTryTypeMethod(A, bindtocpu, flg);
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     MatBoundToCPU - query if a matrix is bound to the CPU

   Input Parameter:
.   A - the matrix

   Output Parameter:
.   flg - the logical flag

   Level: intermediate

.seealso: [](chapter_matrices), `Mat`, `MatBindToCPU()`
@*/
PetscErrorCode MatBoundToCPU(Mat A, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidBoolPointer(flg, 2);
#if defined(PETSC_HAVE_DEVICE)
  *flg = A->boundtocpu;
#else
  *flg = PETSC_TRUE;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSetValuesCOO_Basic(Mat A, const PetscScalar coo_v[], InsertMode imode)
{
  IS              is_coo_i, is_coo_j;
  const PetscInt *coo_i, *coo_j;
  PetscInt        n, n_i, n_j;
  PetscScalar     zero = 0.;

  PetscFunctionBegin;
  PetscCall(PetscObjectQuery((PetscObject)A, "__PETSc_coo_i", (PetscObject *)&is_coo_i));
  PetscCall(PetscObjectQuery((PetscObject)A, "__PETSc_coo_j", (PetscObject *)&is_coo_j));
  PetscCheck(is_coo_i, PetscObjectComm((PetscObject)A), PETSC_ERR_COR, "Missing coo_i IS");
  PetscCheck(is_coo_j, PetscObjectComm((PetscObject)A), PETSC_ERR_COR, "Missing coo_j IS");
  PetscCall(ISGetLocalSize(is_coo_i, &n_i));
  PetscCall(ISGetLocalSize(is_coo_j, &n_j));
  PetscCheck(n_i == n_j, PETSC_COMM_SELF, PETSC_ERR_COR, "Wrong local size %" PetscInt_FMT " != %" PetscInt_FMT, n_i, n_j);
  PetscCall(ISGetIndices(is_coo_i, &coo_i));
  PetscCall(ISGetIndices(is_coo_j, &coo_j));
  if (imode != ADD_VALUES) PetscCall(MatZeroEntries(A));
  for (n = 0; n < n_i; n++) PetscCall(MatSetValue(A, coo_i[n], coo_j[n], coo_v ? coo_v[n] : zero, ADD_VALUES));
  PetscCall(ISRestoreIndices(is_coo_i, &coo_i));
  PetscCall(ISRestoreIndices(is_coo_j, &coo_j));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSetPreallocationCOO_Basic(Mat A, PetscCount ncoo, const PetscInt coo_i[], const PetscInt coo_j[])
{
  Mat         preallocator;
  IS          is_coo_i, is_coo_j;
  PetscScalar zero = 0.0;

  PetscFunctionBegin;
  PetscCall(PetscLayoutSetUp(A->rmap));
  PetscCall(PetscLayoutSetUp(A->cmap));
  PetscCall(MatCreate(PetscObjectComm((PetscObject)A), &preallocator));
  PetscCall(MatSetType(preallocator, MATPREALLOCATOR));
  PetscCall(MatSetSizes(preallocator, A->rmap->n, A->cmap->n, A->rmap->N, A->cmap->N));
  PetscCall(MatSetLayouts(preallocator, A->rmap, A->cmap));
  PetscCall(MatSetUp(preallocator));
  for (PetscCount n = 0; n < ncoo; n++) PetscCall(MatSetValue(preallocator, coo_i[n], coo_j[n], zero, INSERT_VALUES));
  PetscCall(MatAssemblyBegin(preallocator, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(preallocator, MAT_FINAL_ASSEMBLY));
  PetscCall(MatPreallocatorPreallocate(preallocator, PETSC_TRUE, A));
  PetscCall(MatDestroy(&preallocator));
  PetscCheck(ncoo <= PETSC_MAX_INT, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "ncoo %" PetscCount_FMT " overflowed PetscInt; configure --with-64-bit-indices or request support", ncoo);
  PetscCall(ISCreateGeneral(PETSC_COMM_SELF, ncoo, coo_i, PETSC_COPY_VALUES, &is_coo_i));
  PetscCall(ISCreateGeneral(PETSC_COMM_SELF, ncoo, coo_j, PETSC_COPY_VALUES, &is_coo_j));
  PetscCall(PetscObjectCompose((PetscObject)A, "__PETSc_coo_i", (PetscObject)is_coo_i));
  PetscCall(PetscObjectCompose((PetscObject)A, "__PETSc_coo_j", (PetscObject)is_coo_j));
  PetscCall(ISDestroy(&is_coo_i));
  PetscCall(ISDestroy(&is_coo_j));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatSetPreallocationCOO - set preallocation for matrices using a coordinate format of the entries with global indices

   Collective

   Input Parameters:
+  A - matrix being preallocated
.  ncoo - number of entries
.  coo_i - row indices
-  coo_j - column indices

   Level: beginner

   Notes:
   The indices `coo_i` and `coo_j` may be modified within this function. The caller should not rely on them
   having any specific value after this function returns. The arrays can be freed or reused immediately
   after this function returns.

   Entries can be repeated, see `MatSetValuesCOO()`. Entries with negative row or column indices are allowed
   but will be ignored. The corresponding entries in `MatSetValuesCOO()` will be ignored too. Remote entries
   are allowed and will be properly added or inserted to the matrix, unless the matrix option `MAT_IGNORE_OFF_PROC_ENTRIES`
   is set, in which case remote entries are ignored, or `MAT_NO_OFF_PROC_ENTRIES` is set, in which case an error will be generated.

   If you just want to create a sequential AIJ matrix (`MATSEQAIJ`), and your matrix entries in COO format are unique, you can also use
   `MatCreateSeqAIJFromTriple()`. But that is not recommended for iterative applications.

.seealso: [](chapter_matrices), `Mat`, `MatSetValuesCOO()`, `MatSeqAIJSetPreallocation()`, `MatMPIAIJSetPreallocation()`, `MatSeqBAIJSetPreallocation()`,
          `MatMPIBAIJSetPreallocation()`, `MatSeqSBAIJSetPreallocation()`, `MatMPISBAIJSetPreallocation()`, `MatSetPreallocationCOOLocal()`,
          `DMSetMatrixPreallocateSkip()`, `MatCreateSeqAIJFromTriple()`
@*/
PetscErrorCode MatSetPreallocationCOO(Mat A, PetscCount ncoo, PetscInt coo_i[], PetscInt coo_j[])
{
  PetscErrorCode (*f)(Mat, PetscCount, const PetscInt[], const PetscInt[]) = NULL;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidType(A, 1);
  if (ncoo) PetscValidIntPointer(coo_i, 3);
  if (ncoo) PetscValidIntPointer(coo_j, 4);
  PetscCall(PetscLayoutSetUp(A->rmap));
  PetscCall(PetscLayoutSetUp(A->cmap));
  PetscCall(PetscObjectQueryFunction((PetscObject)A, "MatSetPreallocationCOO_C", &f));

  PetscCall(PetscLogEventBegin(MAT_PreallCOO, A, 0, 0, 0));
  if (f) {
    PetscCall((*f)(A, ncoo, coo_i, coo_j));
  } else { /* allow fallback, very slow */
    PetscCall(MatSetPreallocationCOO_Basic(A, ncoo, coo_i, coo_j));
  }
  PetscCall(PetscLogEventEnd(MAT_PreallCOO, A, 0, 0, 0));
  A->preallocated = PETSC_TRUE;
  A->nonzerostate++;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatSetPreallocationCOOLocal - set preallocation for matrices using a coordinate format of the entries with local indices

   Collective

   Input Parameters:
+  A - matrix being preallocated
.  ncoo - number of entries
.  coo_i - row indices (local numbering; may be modified)
-  coo_j - column indices (local numbering; may be modified)

   Level: beginner

   Notes:
   The local indices are translated using the local to global mapping, thus `MatSetLocalToGlobalMapping()` must have been
   called prior to this function. For matrices created with `DMCreateMatrix()` the local to global mapping is often already provided.

   The indices `coo_i` and `coo_j` may be modified within this function. They might be translated to corresponding global
   indices, but the caller should not rely on them having any specific value after this function returns. The arrays
   can be freed or reused immediately after this function returns.

   Entries can be repeated, see `MatSetValuesCOO()`. Entries with negative row or column indices are allowed
   but will be ignored. The corresponding entries in `MatSetValuesCOO()` will be ignored too. Remote entries
   are allowed and will be properly added or inserted to the matrix.

.seealso: [](chapter_matrices), `Mat`, `MatSetValuesCOO()`, `MatSeqAIJSetPreallocation()`, `MatMPIAIJSetPreallocation()`, `MatSeqBAIJSetPreallocation()`,
          `MatMPIBAIJSetPreallocation()`, `MatSeqSBAIJSetPreallocation()`, `MatMPISBAIJSetPreallocation()`, `MatSetPreallocationCOO()`,
          `DMSetMatrixPreallocateSkip()`
@*/
PetscErrorCode MatSetPreallocationCOOLocal(Mat A, PetscCount ncoo, PetscInt coo_i[], PetscInt coo_j[])
{
  PetscErrorCode (*f)(Mat, PetscCount, PetscInt[], PetscInt[]) = NULL;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidType(A, 1);
  if (ncoo) PetscValidIntPointer(coo_i, 3);
  if (ncoo) PetscValidIntPointer(coo_j, 4);
  PetscCheck(ncoo <= PETSC_MAX_INT, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "ncoo %" PetscCount_FMT " overflowed PetscInt; configure --with-64-bit-indices or request support", ncoo);
  PetscCall(PetscLayoutSetUp(A->rmap));
  PetscCall(PetscLayoutSetUp(A->cmap));

  PetscCall(PetscObjectQueryFunction((PetscObject)A, "MatSetPreallocationCOOLocal_C", &f));
  if (f) {
    PetscCall((*f)(A, ncoo, coo_i, coo_j));
    A->nonzerostate++;
  } else {
    ISLocalToGlobalMapping ltog_row, ltog_col;
    PetscCall(MatGetLocalToGlobalMapping(A, &ltog_row, &ltog_col));
    if (ltog_row) PetscCall(ISLocalToGlobalMappingApply(ltog_row, ncoo, coo_i, coo_i));
    if (ltog_col) PetscCall(ISLocalToGlobalMappingApply(ltog_col, ncoo, coo_j, coo_j));
    PetscCall(MatSetPreallocationCOO(A, ncoo, coo_i, coo_j));
  }
  A->preallocated = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatSetValuesCOO - set values at once in a matrix preallocated using `MatSetPreallocationCOO()`

   Collective

   Input Parameters:
+  A - matrix being preallocated
.  coo_v - the matrix values (can be `NULL`)
-  imode - the insert mode

   Level: beginner

   Notes:
   The values must follow the order of the indices prescribed with `MatSetPreallocationCOO()` or `MatSetPreallocationCOOLocal()`.

          When repeated entries are specified in the COO indices the `coo_v` values are first properly summed, regardless of the value of imode.
          The imode flag indicates if coo_v must be added to the current values of the matrix (`ADD_VALUES`) or overwritten (`INSERT_VALUES`).

          `MatAssemblyBegin()` and `MatAssemblyEnd()` do not need to be called after this routine. It automatically handles the assembly process.

.seealso: [](chapter_matrices), `Mat`, `MatSetPreallocationCOO()`, `MatSetPreallocationCOOLocal()`, `InsertMode`, `INSERT_VALUES`, `ADD_VALUES`
@*/
PetscErrorCode MatSetValuesCOO(Mat A, const PetscScalar coo_v[], InsertMode imode)
{
  PetscErrorCode (*f)(Mat, const PetscScalar[], InsertMode) = NULL;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidType(A, 1);
  MatCheckPreallocated(A, 1);
  PetscValidLogicalCollectiveEnum(A, imode, 3);
  PetscCall(PetscObjectQueryFunction((PetscObject)A, "MatSetValuesCOO_C", &f));
  PetscCall(PetscLogEventBegin(MAT_SetVCOO, A, 0, 0, 0));
  if (f) {
    PetscCall((*f)(A, coo_v, imode));
  } else { /* allow fallback */
    PetscCall(MatSetValuesCOO_Basic(A, coo_v, imode));
  }
  PetscCall(PetscLogEventEnd(MAT_SetVCOO, A, 0, 0, 0));
  PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatSetBindingPropagates - Sets whether the state of being bound to the CPU for a GPU matrix type propagates to child and some other associated objects

   Input Parameters:
+  A - the matrix
-  flg - flag indicating whether the boundtocpu flag should be propagated

   Level: developer

   Notes:
   If the value of flg is set to true, the following will occur
+   `MatCreateSubMatrices()` and `MatCreateRedundantMatrix()` - bind created matrices to CPU if the input matrix is bound to the CPU.
-   `MatCreateVecs()` - bind created vectors to CPU if the input matrix is bound to the CPU.

   The bindingpropagates flag itself is also propagated by the above routines.

   Developer Note:
   If the fine-scale `DMDA` has the `-dm_bind_below` option set to true, then `DMCreateInterpolationScale()` calls `MatSetBindingPropagates()`
   on the restriction/interpolation operator to set the bindingpropagates flag to true.

.seealso: [](chapter_matrices), `Mat`, `VecSetBindingPropagates()`, `MatGetBindingPropagates()`
@*/
PetscErrorCode MatSetBindingPropagates(Mat A, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
#if defined(PETSC_HAVE_VIENNACL) || defined(PETSC_HAVE_CUDA) || defined(PETSC_HAVE_HIP)
  A->bindingpropagates = flg;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatGetBindingPropagates - Gets whether the state of being bound to the CPU for a GPU matrix type propagates to child and some other associated objects

   Input Parameter:
.  A - the matrix

   Output Parameter:
.  flg - flag indicating whether the boundtocpu flag will be propagated

   Level: developer

.seealso: [](chapter_matrices), `Mat`, `MatSetBindingPropagates()`
@*/
PetscErrorCode MatGetBindingPropagates(Mat A, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidBoolPointer(flg, 2);
#if defined(PETSC_HAVE_VIENNACL) || defined(PETSC_HAVE_CUDA) || defined(PETSC_HAVE_HIP)
  *flg = A->bindingpropagates;
#else
  *flg = PETSC_FALSE;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

#include <../src/mat/impls/aij/mpi/mpiaij.h>
/*@C
   MatCreateMPIAIJSELL - Creates a sparse parallel matrix whose local
   portions are stored as `MATSEQAIJSELL` matrices (a matrix class that inherits
   from SEQAIJ but performs some operations in SELL format).

      Collective

   Input Parameters:
+  comm - MPI communicator
.  m - number of local rows (or `PETSC_DECIDE` to have calculated if `M` is given)
           This value should be the same as the local size used in creating the
           y vector for the matrix-vector product y = Ax.
.  n - This value should be the same as the local size used in creating the
       x vector for the matrix-vector product y = Ax. (or `PETSC_DECIDE` to have
       calculated if `N` is given) For square matrices `n` is almost always `m`.
.  M - number of global rows (or `PETSC_DETERMINE` to have calculated if `m` is given)
.  N - number of global columns (or `PETSC_DETERMINE` to have calculated if `n` is given)
.  d_nz  - number of nonzeros per row in DIAGONAL portion of local submatrix
           (same value is used for all local rows)
.  d_nnz - array containing the number of nonzeros in the various rows of the
           DIAGONAL portion of the local submatrix (possibly different for each row)
           or `NULL`, if `d_nz` is used to specify the nonzero structure.
           The size of this array is equal to the number of local rows, i.e `m`.
           For matrices you plan to factor you must leave room for the diagonal entry and
           put in the entry even if it is zero.
.  o_nz  - number of nonzeros per row in the OFF-DIAGONAL portion of local
           submatrix (same value is used for all local rows).
-  o_nnz - array containing the number of nonzeros in the various rows of the
           OFF-DIAGONAL portion of the local submatrix (possibly different for
           each row) or `NULL`, if `o_nz` is used to specify the nonzero
           structure. The size of this array is equal to the number
           of local rows, i.e `m`.

   Output Parameter:
.  A - the matrix

   Options Database Key:
.  -mat_aijsell_eager_shadow - Construct shadow matrix upon matrix assembly; default is to take a "lazy" approach, performing this step the first
                               time the matrix is applied

   Level: intermediate

   Notes:
   If the *_nnz parameter is given then the *_nz parameter is ignored

   `m`,`n`,`M`,`N` parameters specify the size of the matrix, and its partitioning across
   processors, while `d_nz`,`d_nnz`,`o_nz`,`o_nnz` parameters specify the approximate
   storage requirements for this matrix.

   If `PETSC_DECIDE` or `PETSC_DETERMINE` is used for a particular argument on one
   processor than it must be used on all processors that share the object for
   that argument.

   The user MUST specify either the local or global matrix dimensions
   (possibly both).

   The parallel matrix is partitioned such that the first m0 rows belong to
   process 0, the next m1 rows belong to process 1, the next m2 rows belong
   to process 2 etc.. where m0,m1,m2... are the input parameter `m`.

   The DIAGONAL portion of the local submatrix of a processor can be defined
   as the submatrix which is obtained by extraction the part corresponding
   to the rows r1-r2 and columns r1-r2 of the global matrix, where r1 is the
   first row that belongs to the processor, and r2 is the last row belonging
   to the this processor. This is a square mxm matrix. The remaining portion
   of the local submatrix (mxN) constitute the OFF-DIAGONAL portion.

   If `o_nnz`, `d_nnz` are specified, then `o_nz`, and `d_nz` are ignored.

   When calling this routine with a single process communicator, a matrix of
   type `MATSEQAIJSELL` is returned.  If a matrix of type `MATMPIAIJSELL` is desired
   for this type of communicator, use the construction mechanism
.vb
   MatCreate(...,&A);
   MatSetType(A,MPIAIJSELL);
   MatMPIAIJSetPreallocation(A,...);
.ve

.seealso: [](chapter_matrices), `Mat`, [Sparse Matrix Creation](sec_matsparse), `MATSEQAIJSELL`, `MATMPIAIJSELL`, `MATAIJSELL`, `MatCreate()`, `MatCreateSeqAIJSELL()`, `MatSetValues()`
@*/
PetscErrorCode MatCreateMPIAIJSELL(MPI_Comm comm, PetscInt m, PetscInt n, PetscInt M, PetscInt N, PetscInt d_nz, const PetscInt d_nnz[], PetscInt o_nz, const PetscInt o_nnz[], Mat *A)
{
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscCall(MatCreate(comm, A));
  PetscCall(MatSetSizes(*A, m, n, M, N));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  if (size > 1) {
    PetscCall(MatSetType(*A, MATMPIAIJSELL));
    PetscCall(MatMPIAIJSetPreallocation(*A, d_nz, d_nnz, o_nz, o_nnz));
  } else {
    PetscCall(MatSetType(*A, MATSEQAIJSELL));
    PetscCall(MatSeqAIJSetPreallocation(*A, d_nz, d_nnz));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode MatConvert_SeqAIJ_SeqAIJSELL(Mat, MatType, MatReuse, Mat *);

PetscErrorCode MatMPIAIJSetPreallocation_MPIAIJSELL(Mat B, PetscInt d_nz, const PetscInt d_nnz[], PetscInt o_nz, const PetscInt o_nnz[])
{
  Mat_MPIAIJ *b = (Mat_MPIAIJ *)B->data;

  PetscFunctionBegin;
  PetscCall(MatMPIAIJSetPreallocation_MPIAIJ(B, d_nz, d_nnz, o_nz, o_nnz));
  PetscCall(MatConvert_SeqAIJ_SeqAIJSELL(b->A, MATSEQAIJSELL, MAT_INPLACE_MATRIX, &b->A));
  PetscCall(MatConvert_SeqAIJ_SeqAIJSELL(b->B, MATSEQAIJSELL, MAT_INPLACE_MATRIX, &b->B));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode MatConvert_MPIAIJ_MPIAIJSELL(Mat A, MatType type, MatReuse reuse, Mat *newmat)
{
  Mat B = *newmat;

  PetscFunctionBegin;
  if (reuse == MAT_INITIAL_MATRIX) PetscCall(MatDuplicate(A, MAT_COPY_VALUES, &B));

  PetscCall(PetscObjectChangeTypeName((PetscObject)B, MATMPIAIJSELL));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatMPIAIJSetPreallocation_C", MatMPIAIJSetPreallocation_MPIAIJSELL));
  *newmat = B;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN PetscErrorCode MatCreate_MPIAIJSELL(Mat A)
{
  PetscFunctionBegin;
  PetscCall(MatSetType(A, MATMPIAIJ));
  PetscCall(MatConvert_MPIAIJ_MPIAIJSELL(A, MATMPIAIJSELL, MAT_INPLACE_MATRIX, &A));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   MATAIJSELL - "AIJSELL" - A matrix type to be used for sparse matrices.

   This matrix type is identical to `MATSEQAIJSELL` when constructed with a single process communicator,
   and `MATMPIAIJSELL` otherwise.  As a result, for single process communicators,
   MatSeqAIJSetPreallocation() is supported, and similarly `MatMPIAIJSetPreallocation()` is supported
   for communicators controlling multiple processes.  It is recommended that you call both of
   the above preallocation routines for simplicity.

   Options Database Key:
. -mat_type aijsell - sets the matrix type to `MATAIJSELL`

  Level: beginner

.seealso: [](chapter_matrices), `Mat`, `MatCreateMPIAIJSELL()`, `MATSEQAIJSELL`, `MATMPIAIJSELL`, `MATSEQAIJ`, `MATMPIAIJ`, `MATSEQAIJPERM`, `MATMPIAIJPERM`, `MATSEQAIJMKL`, `MATMPIAIJMKL`
M*/


#include <petsc/private/matimpl.h> /*I   "petscmat.h"  I*/

/*@
   MatGetColumnVector - Gets the values from a given column of a matrix.

   Not Collective

   Input Parameters:
+  A - the matrix
.  yy - the vector
-  col - the column requested (in global numbering)

   Level: advanced

   Notes:
   If a `MatType` does not implement the operation, each processor for which this is called
   gets the values for its rows using `MatGetRow()`.

   The vector must have the same parallel row layout as the matrix.

   Contributed by: Denis Vanderstraeten

.seealso: [](chapter_matrices), `Mat`, `MatGetRow()`, `MatGetDiagonal()`, `MatMult()`
@*/
PetscErrorCode MatGetColumnVector(Mat A, Vec yy, PetscInt col)
{
  PetscScalar       *y;
  const PetscScalar *v;
  PetscInt           i, j, nz, N, Rs, Re, rs, re;
  const PetscInt    *idx;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidHeaderSpecific(yy, VEC_CLASSID, 2);
  PetscValidLogicalCollectiveInt(A, col, 3);
  PetscCheck(col >= 0, PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_OUTOFRANGE, "Requested negative column: %" PetscInt_FMT, col);
  PetscCall(MatGetSize(A, NULL, &N));
  PetscCheck(col < N, PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_OUTOFRANGE, "Requested column %" PetscInt_FMT " larger than number columns in matrix %" PetscInt_FMT, col, N);
  PetscCall(MatGetOwnershipRange(A, &Rs, &Re));
  PetscCall(VecGetOwnershipRange(yy, &rs, &re));
  PetscCheck(Rs == rs && Re == re, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Matrix %" PetscInt_FMT " %" PetscInt_FMT " does not have same ownership range (size) as vector %" PetscInt_FMT " %" PetscInt_FMT, Rs, Re, rs, re);

  if (A->ops->getcolumnvector) PetscUseTypeMethod(A, getcolumnvector, yy, col);
  else {
    PetscCall(VecSet(yy, 0.0));
    PetscCall(VecGetArray(yy, &y));
    /* TODO for general matrices */
    for (i = Rs; i < Re; i++) {
      PetscCall(MatGetRow(A, i, &nz, &idx, &v));
      if (nz && idx[0] <= col) {
        /*
          Should use faster search here
        */
        for (j = 0; j < nz; j++) {
          if (idx[j] >= col) {
            if (idx[j] == col) y[i - rs] = v[j];
            break;
          }
        }
      }
      PetscCall(MatRestoreRow(A, i, &nz, &idx, &v));
    }
    PetscCall(VecRestoreArray(yy, &y));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatGetColumnNorms - Gets the norms of each column of a sparse or dense matrix.

   Input Parameters:
+  A - the matrix
-  type - `NORM_2`, `NORM_1` or `NORM_INFINITY`

   Output Parameter:
.  norms - an array as large as the TOTAL number of columns in the matrix

   Level: intermediate

   Note:
    Each process has ALL the column norms after the call. Because of the way this is computed each process gets all the values,
    if each process wants only some of the values it should extract the ones it wants from the array.

.seealso: [](chapter_matrices), `Mat`, `NormType`, `MatNorm()`
@*/
PetscErrorCode MatGetColumnNorms(Mat A, NormType type, PetscReal norms[])
{
  /* NOTE: MatGetColumnNorms() could simply be a macro that calls MatGetColumnReductions().
   * I've kept this as a function because it allows slightly more in the way of error checking,
   * erroring out if MatGetColumnNorms() is not called with a valid NormType. */

  PetscFunctionBegin;
  if (type == NORM_2 || type == NORM_1 || type == NORM_FROBENIUS || type == NORM_INFINITY || type == NORM_1_AND_2) {
    PetscCall(MatGetColumnReductions(A, type, norms));
  } else SETERRQ(PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_WRONG, "Unknown NormType");
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatGetColumnSumsRealPart - Gets the sums of the real part of each column of a sparse or dense matrix.

   Input Parameter:
.  A - the matrix

   Output Parameter:
.  sums - an array as large as the TOTAL number of columns in the matrix

   Level: intermediate

   Note:
    Each process has ALL the column sums after the call. Because of the way this is computed each process gets all the values,
    if each process wants only some of the values it should extract the ones it wants from the array.

.seealso: [](chapter_matrices), `Mat`, `MatGetColumnSumsImaginaryPart()`, `VecSum()`, `MatGetColumnMeans()`, `MatGetColumnNorms()`, `MatGetColumnReductions()`
@*/
PetscErrorCode MatGetColumnSumsRealPart(Mat A, PetscReal sums[])
{
  PetscFunctionBegin;
  PetscCall(MatGetColumnReductions(A, REDUCTION_SUM_REALPART, sums));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatGetColumnSumsImaginaryPart - Gets the sums of the imaginary part of each column of a sparse or dense matrix.

   Input Parameter:
.  A - the matrix

   Output Parameter:
.  sums - an array as large as the TOTAL number of columns in the matrix

   Level: intermediate

   Note:
    Each process has ALL the column sums after the call. Because of the way this is computed each process gets all the values,
    if each process wants only some of the values it should extract the ones it wants from the array.

.seealso: [](chapter_matrices), `Mat`, `MatGetColumnSumsRealPart()`, `VecSum()`, `MatGetColumnMeans()`, `MatGetColumnNorms()`, `MatGetColumnReductions()`
@*/
PetscErrorCode MatGetColumnSumsImaginaryPart(Mat A, PetscReal sums[])
{
  PetscFunctionBegin;
  PetscCall(MatGetColumnReductions(A, REDUCTION_SUM_IMAGINARYPART, sums));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatGetColumnSums - Gets the sums of each column of a sparse or dense matrix.

   Input Parameter:
.  A - the matrix

   Output Parameter:
.  sums - an array as large as the TOTAL number of columns in the matrix

   Level: intermediate

   Note:
    Each process has ALL the column sums after the call. Because of the way this is computed each process gets all the values,
    if each process wants only some of the values it should extract the ones it wants from the array.

.seealso: [](chapter_matrices), `Mat`, `VecSum()`, `MatGetColumnMeans()`, `MatGetColumnNorms()`, `MatGetColumnReductions()`
@*/
PetscErrorCode MatGetColumnSums(Mat A, PetscScalar sums[])
{
#if defined(PETSC_USE_COMPLEX)
  PetscInt   i, n;
  PetscReal *work;
#endif

  PetscFunctionBegin;

#if !defined(PETSC_USE_COMPLEX)
  PetscCall(MatGetColumnSumsRealPart(A, sums));
#else
  PetscCall(MatGetSize(A, NULL, &n));
  PetscCall(PetscArrayzero(sums, n));
  PetscCall(PetscCalloc1(n, &work));
  PetscCall(MatGetColumnSumsRealPart(A, work));
  for (i = 0; i < n; i++) sums[i] = work[i];
  PetscCall(MatGetColumnSumsImaginaryPart(A, work));
  for (i = 0; i < n; i++) sums[i] += work[i] * PETSC_i;
  PetscCall(PetscFree(work));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatGetColumnMeansRealPart - Gets the arithmetic means of the real part of each column of a sparse or dense matrix.

   Input Parameter:
.  A - the matrix

   Output Parameter:
.  sums - an array as large as the TOTAL number of columns in the matrix

   Level: intermediate

   Note:
    Each process has ALL the column means after the call. Because of the way this is computed each process gets all the values,
    if each process wants only some of the values it should extract the ones it wants from the array.

.seealso: [](chapter_matrices), `Mat`, `MatGetColumnMeansImaginaryPart()`, `VecSum()`, `MatGetColumnSums()`, `MatGetColumnNorms()`, `MatGetColumnReductions()`
@*/
PetscErrorCode MatGetColumnMeansRealPart(Mat A, PetscReal means[])
{
  PetscFunctionBegin;
  PetscCall(MatGetColumnReductions(A, REDUCTION_MEAN_REALPART, means));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatGetColumnMeansImaginaryPart - Gets the arithmetic means of the imaginary part of each column of a sparse or dense matrix.

   Input Parameter:
.  A - the matrix

   Output Parameter:
.  sums - an array as large as the TOTAL number of columns in the matrix

   Level: intermediate

   Note:
    Each process has ALL the column means after the call. Because of the way this is computed each process gets all the values,
    if each process wants only some of the values it should extract the ones it wants from the array.

.seealso: [](chapter_matrices), `Mat`, `MatGetColumnMeansRealPart()`, `VecSum()`, `MatGetColumnSums()`, `MatGetColumnNorms()`, `MatGetColumnReductions()`
@*/
PetscErrorCode MatGetColumnMeansImaginaryPart(Mat A, PetscReal means[])
{
  PetscFunctionBegin;
  PetscCall(MatGetColumnReductions(A, REDUCTION_MEAN_IMAGINARYPART, means));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatGetColumnMeans - Gets the arithmetic means of each column of a sparse or dense matrix.

   Input Parameter:
.  A - the matrix

   Output Parameter:
.  means - an array as large as the TOTAL number of columns in the matrix

   Level: intermediate

   Note:
    Each process has ALL the column means after the call. Because of the way this is computed each process gets all the values,
    if each process wants only some of the values it should extract the ones it wants from the array.

.seealso: [](chapter_matrices), `Mat`, `VecSum()`, `MatGetColumnSums()`, `MatGetColumnNorms()`, `MatGetColumnReductions()`
@*/
PetscErrorCode MatGetColumnMeans(Mat A, PetscScalar means[])
{
#if defined(PETSC_USE_COMPLEX)
  PetscInt   i, n;
  PetscReal *work;
#endif

  PetscFunctionBegin;

#if !defined(PETSC_USE_COMPLEX)
  PetscCall(MatGetColumnMeansRealPart(A, means));
#else
  PetscCall(MatGetSize(A, NULL, &n));
  PetscCall(PetscArrayzero(means, n));
  PetscCall(PetscCalloc1(n, &work));
  PetscCall(MatGetColumnMeansRealPart(A, work));
  for (i = 0; i < n; i++) means[i] = work[i];
  PetscCall(MatGetColumnMeansImaginaryPart(A, work));
  for (i = 0; i < n; i++) means[i] += work[i] * PETSC_i;
  PetscCall(PetscFree(work));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    MatGetColumnReductions - Gets the reductions of each column of a sparse or dense matrix.

  Input Parameters:
+  A - the matrix
-  type - A constant defined in `NormType` or `ReductionType`: `NORM_2`, `NORM_1`, `NORM_INFINITY`, `REDUCTION_SUM_REALPART`,
          `REDUCTION_SUM_IMAGINARYPART`, `REDUCTION_MEAN_REALPART`, `REDUCTION_MEAN_IMAGINARYPART`

  Output Parameter:
.  reductions - an array as large as the TOTAL number of columns in the matrix

   Level: developer

   Note:
    Each process has ALL the column reductions after the call. Because of the way this is computed each process gets all the values,
    if each process wants only some of the values it should extract the ones it wants from the array.

  Developer Note:
    This routine is primarily intended as a back-end.
    `MatGetColumnNorms()`, `MatGetColumnSums()`, and `MatGetColumnMeans()` are implemented using this routine.

.seealso: [](chapter_matrices), `Mat`, `ReductionType`, `NormType`, `MatGetColumnNorms()`, `MatGetColumnSums()`, `MatGetColumnMeans()`
@*/
PetscErrorCode MatGetColumnReductions(Mat A, PetscInt type, PetscReal reductions[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscUseTypeMethod(A, getcolumnreductions, type, reductions);
  PetscFunctionReturn(PETSC_SUCCESS);
}

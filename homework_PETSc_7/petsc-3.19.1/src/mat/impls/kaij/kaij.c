
/*
  Defines the basic matrix operations for the KAIJ  matrix storage format.
  This format is used to evaluate matrices of the form:

    [I \otimes S + A \otimes T]

  where
    S is a dense (p \times q) matrix
    T is a dense (p \times q) matrix
    A is an AIJ  (n \times n) matrix
    I is the identity matrix

  The resulting matrix is (np \times nq)

  We provide:
     MatMult()
     MatMultAdd()
     MatInvertBlockDiagonal()
  and
     MatCreateKAIJ(Mat,PetscInt,PetscInt,const PetscScalar[],const PetscScalar[],Mat*)

  This single directory handles both the sequential and parallel codes
*/

#include <../src/mat/impls/kaij/kaij.h> /*I "petscmat.h" I*/
#include <../src/mat/utils/freespace.h>
#include <petsc/private/vecimpl.h>

/*@C
   MatKAIJGetAIJ - Get the `MATAIJ` matrix describing the blockwise action of the `MATKAIJ` matrix

   Not Collective, but if the `MATKAIJ` matrix is parallel, the `MATAIJ` matrix is also parallel

   Input Parameter:
.  A - the `MATKAIJ` matrix

   Output Parameter:
.  B - the `MATAIJ` matrix

   Level: advanced

   Note:
   The reference count on the `MATAIJ` matrix is not increased so you should not destroy it.

.seealso: [](chapter_matrices), `Mat`, `MatCreateKAIJ()`, `MATKAIJ`, `MATAIJ`
@*/
PetscErrorCode MatKAIJGetAIJ(Mat A, Mat *B)
{
  PetscBool ismpikaij, isseqkaij;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)A, MATMPIKAIJ, &ismpikaij));
  PetscCall(PetscObjectTypeCompare((PetscObject)A, MATSEQKAIJ, &isseqkaij));
  if (ismpikaij) {
    Mat_MPIKAIJ *b = (Mat_MPIKAIJ *)A->data;

    *B = b->A;
  } else if (isseqkaij) {
    Mat_SeqKAIJ *b = (Mat_SeqKAIJ *)A->data;

    *B = b->AIJ;
  } else SETERRQ(PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_WRONG, "Matrix passed in is not of type KAIJ");
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatKAIJGetS - Get the `S` matrix describing the shift action of the `MATKAIJ` matrix

   Not Collective; the entire `S` is stored and returned independently on all processes.

   Input Parameter:
.  A - the `MATKAIJ` matrix

   Output Parameters:
+  m - the number of rows in `S`
.  n - the number of columns in `S`
-  S - the S matrix, in form of a scalar array in column-major format

   Level: advanced

   Note:
   All output parameters are optional (pass `NULL` if not desired)

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatCreateKAIJ()`, `MatGetBlockSizes()`
@*/
PetscErrorCode MatKAIJGetS(Mat A, PetscInt *m, PetscInt *n, PetscScalar **S)
{
  Mat_SeqKAIJ *b = (Mat_SeqKAIJ *)A->data;
  PetscFunctionBegin;
  if (m) *m = b->p;
  if (n) *n = b->q;
  if (S) *S = b->S;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatKAIJGetSRead - Get a read-only pointer to the `S` matrix describing the shift action of the `MATKAIJ` matrix

   Not Collective; the entire `S` is stored and returned independently on all processes.

   Input Parameter:
.  A - the `MATKAIJ` matrix

   Output Parameters:
+  m - the number of rows in `S`
.  n - the number of columns in `S`
-  S - the S matrix, in form of a scalar array in column-major format

   Level: advanced

   Note:
   All output parameters are optional (pass `NULL` if not desired)

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatCreateKAIJ()`, `MatGetBlockSizes()`
@*/
PetscErrorCode MatKAIJGetSRead(Mat A, PetscInt *m, PetscInt *n, const PetscScalar **S)
{
  Mat_SeqKAIJ *b = (Mat_SeqKAIJ *)A->data;
  PetscFunctionBegin;
  if (m) *m = b->p;
  if (n) *n = b->q;
  if (S) *S = b->S;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  MatKAIJRestoreS - Restore array obtained with `MatKAIJGetS()`

  Not Collective

  Input Parameters:
+ A - the `MATKAIJ` matrix
- S - location of pointer to array obtained with `MatKAIJGetS()`

  Level: advanced

  Note:
  This routine zeros the array pointer to prevent accidental reuse after it has been restored.
  If `NULL` is passed, it will not attempt to zero the array pointer.

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatKAIJGetS()`, `MatKAIJGetSRead()`, `MatKAIJRestoreSRead()`
@*/
PetscErrorCode MatKAIJRestoreS(Mat A, PetscScalar **S)
{
  PetscFunctionBegin;
  if (S) *S = NULL;
  PetscCall(PetscObjectStateIncrease((PetscObject)A));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  MatKAIJRestoreSRead - Restore array obtained with `MatKAIJGetSRead()`

  Not Collective

  Input Parameters:
+ A - the `MATKAIJ` matrix
- S - location of pointer to array obtained with `MatKAIJGetS()`

  Level: advanced

  Note:
  This routine zeros the array pointer to prevent accidental reuse after it has been restored.
  If `NULL` is passed, it will not attempt to zero the array pointer.

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatKAIJGetS()`, `MatKAIJGetSRead()`, `MatKAIJRestoreSRead()`
@*/
PetscErrorCode MatKAIJRestoreSRead(Mat A, const PetscScalar **S)
{
  PetscFunctionBegin;
  if (S) *S = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatKAIJGetT - Get the transformation matrix `T` associated with the `MATKAIJ` matrix

   Not Collective; the entire `T` is stored and returned independently on all processes

   Input Parameter:
.  A - the `MATKAIJ` matrix

   Output Parameters:
+  m - the number of rows in `T`
.  n - the number of columns in `T`
-  T - the T matrix, in form of a scalar array in column-major format

   Level: advanced

   Note:
   All output parameters are optional (pass `NULL` if not desired)

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatCreateKAIJ()`, `MatGetBlockSizes()`
@*/
PetscErrorCode MatKAIJGetT(Mat A, PetscInt *m, PetscInt *n, PetscScalar **T)
{
  Mat_SeqKAIJ *b = (Mat_SeqKAIJ *)A->data;
  PetscFunctionBegin;
  if (m) *m = b->p;
  if (n) *n = b->q;
  if (T) *T = b->T;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatKAIJGetTRead - Get a read-only pointer to the transformation matrix `T` associated with the `MATKAIJ` matrix

   Not Collective; the entire `T` is stored and returned independently on all processes

   Input Parameter:
.  A - the `MATKAIJ` matrix

   Output Parameters:
+  m - the number of rows in `T`
.  n - the number of columns in `T`
-  T - the T matrix, in form of a scalar array in column-major format

   Level: advanced

   Note:
   All output parameters are optional (pass `NULL` if not desired)

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatCreateKAIJ()`, `MatGetBlockSizes()`
@*/
PetscErrorCode MatKAIJGetTRead(Mat A, PetscInt *m, PetscInt *n, const PetscScalar **T)
{
  Mat_SeqKAIJ *b = (Mat_SeqKAIJ *)A->data;
  PetscFunctionBegin;
  if (m) *m = b->p;
  if (n) *n = b->q;
  if (T) *T = b->T;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  MatKAIJRestoreT - Restore array obtained with `MatKAIJGetT()`

  Not Collective

  Input Parameters:
+ A - the `MATKAIJ` matrix
- T - location of pointer to array obtained with `MatKAIJGetS()`

  Level: advanced

  Note:
  This routine zeros the array pointer to prevent accidental reuse after it has been restored.
  If `NULL` is passed, it will not attempt to zero the array pointer.

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatKAIJGetT()`, `MatKAIJGetTRead()`, `MatKAIJRestoreTRead()`
@*/
PetscErrorCode MatKAIJRestoreT(Mat A, PetscScalar **T)
{
  PetscFunctionBegin;
  if (T) *T = NULL;
  PetscCall(PetscObjectStateIncrease((PetscObject)A));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  MatKAIJRestoreTRead - Restore array obtained with `MatKAIJGetTRead()`

  Not Collective

  Input Parameters:
+ A - the `MATKAIJ` matrix
- T - location of pointer to array obtained with `MatKAIJGetS()`

  Level: advanced

  Note:
  This routine zeros the array pointer to prevent accidental reuse after it has been restored.
  If `NULL` is passed, it will not attempt to zero the array pointer.

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatKAIJGetT()`, `MatKAIJGetTRead()`, `MatKAIJRestoreTRead()`
@*/
PetscErrorCode MatKAIJRestoreTRead(Mat A, const PetscScalar **T)
{
  PetscFunctionBegin;
  if (T) *T = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatKAIJSetAIJ - Set the `MATAIJ` matrix describing the blockwise action of the `MATKAIJ` matrix

   Logically Collective; if the `MATAIJ` matrix is parallel, the `MATKAIJ` matrix is also parallel

   Input Parameters:
+  A - the `MATKAIJ` matrix
-  B - the `MATAIJ` matrix

   Level: advanced

   Notes:
   This function increases the reference count on the `MATAIJ` matrix, so the user is free to destroy the matrix if it is not needed.

   Changes to the entries of the `MATAIJ` matrix will immediately affect the `MATKAIJ` matrix.

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatKAIJGetAIJ()`, `MatKAIJSetS()`, `MatKAIJSetT()`
@*/
PetscErrorCode MatKAIJSetAIJ(Mat A, Mat B)
{
  PetscMPIInt size;
  PetscBool   flg;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)A), &size));
  if (size == 1) {
    PetscCall(PetscObjectTypeCompare((PetscObject)B, MATSEQAIJ, &flg));
    PetscCheck(flg, PetscObjectComm((PetscObject)B), PETSC_ERR_SUP, "MatKAIJSetAIJ() with MATSEQKAIJ does not support %s as the AIJ mat", ((PetscObject)B)->type_name);
    Mat_SeqKAIJ *a = (Mat_SeqKAIJ *)A->data;
    a->AIJ         = B;
  } else {
    Mat_MPIKAIJ *a = (Mat_MPIKAIJ *)A->data;
    a->A           = B;
  }
  PetscCall(PetscObjectReference((PetscObject)B));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatKAIJSetS - Set the `S` matrix describing the shift action of the `MATKAIJ` matrix

   Logically Collective; the entire `S` is stored independently on all processes.

   Input Parameters:
+  A - the `MATKAIJ` matrix
.  p - the number of rows in `S`
.  q - the number of columns in `S`
-  S - the S matrix, in form of a scalar array in column-major format

   Level: advanced

   Notes:
   The dimensions `p` and `q` must match those of the transformation matrix `T` associated with the `MATKAIJ` matrix.

   The `S` matrix is copied, so the user can destroy this array.

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatKAIJGetS()`, `MatKAIJSetT()`, `MatKAIJSetAIJ()`
@*/
PetscErrorCode MatKAIJSetS(Mat A, PetscInt p, PetscInt q, const PetscScalar S[])
{
  Mat_SeqKAIJ *a = (Mat_SeqKAIJ *)A->data;

  PetscFunctionBegin;
  PetscCall(PetscFree(a->S));
  if (S) {
    PetscCall(PetscMalloc1(p * q, &a->S));
    PetscCall(PetscMemcpy(a->S, S, p * q * sizeof(PetscScalar)));
  } else a->S = NULL;

  a->p = p;
  a->q = q;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatKAIJGetScaledIdentity - Check if both `S` and `T` are scaled identities.

   Logically Collective.

   Input Parameter:
.  A - the `MATKAIJ` matrix

  Output Parameter:
.  identity - the Boolean value

   Level: Advanced

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatKAIJGetS()`, `MatKAIJGetT()`
@*/
PetscErrorCode MatKAIJGetScaledIdentity(Mat A, PetscBool *identity)
{
  Mat_SeqKAIJ *a = (Mat_SeqKAIJ *)A->data;
  PetscInt     i, j;

  PetscFunctionBegin;
  if (a->p != a->q) {
    *identity = PETSC_FALSE;
    PetscFunctionReturn(PETSC_SUCCESS);
  } else *identity = PETSC_TRUE;
  if (!a->isTI || a->S) {
    for (i = 0; i < a->p && *identity; i++) {
      for (j = 0; j < a->p && *identity; j++) {
        if (i != j) {
          if (a->S && PetscAbsScalar(a->S[i + j * a->p]) > PETSC_SMALL) *identity = PETSC_FALSE;
          if (a->T && PetscAbsScalar(a->T[i + j * a->p]) > PETSC_SMALL) *identity = PETSC_FALSE;
        } else {
          if (a->S && PetscAbsScalar(a->S[i * (a->p + 1)] - a->S[0]) > PETSC_SMALL) *identity = PETSC_FALSE;
          if (a->T && PetscAbsScalar(a->T[i * (a->p + 1)] - a->T[0]) > PETSC_SMALL) *identity = PETSC_FALSE;
        }
      }
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatKAIJSetT - Set the transformation matrix `T` associated with the `MATKAIJ` matrix

   Logically Collective; the entire `T` is stored independently on all processes.

   Input Parameters:
+  A - the `MATKAIJ` matrix
.  p - the number of rows in `S`
.  q - the number of columns in `S`
-  T - the `T` matrix, in form of a scalar array in column-major format

   Level: Advanced

   Notes:
   The dimensions `p` and `q` must match those of the shift matrix `S` associated with the `MATKAIJ` matrix.

   The `T` matrix is copied, so the user can destroy this array.

.seealso: [](chapter_matrices), `Mat`, `MATKAIJ`, `MatKAIJGetT()`, `MatKAIJSetS()`, `MatKAIJSetAIJ()`
@*/
PetscErrorCode MatKAIJSetT(Mat A, PetscInt p, PetscInt q, const PetscScalar T[])
{
  PetscInt     i, j;
  Mat_SeqKAIJ *a    = (Mat_SeqKAIJ *)A->data;
  PetscBool    isTI = PETSC_FALSE;

  PetscFunctionBegin;
  /* check if T is an identity matrix */
  if (T && (p == q)) {
    isTI = PETSC_TRUE;
    for (i = 0; i < p; i++) {
      for (j = 0; j < q; j++) {
        if (i == j) {
          /* diagonal term must be 1 */
          if (T[i + j * p] != 1.0) isTI = PETSC_FALSE;
        } else {
          /* off-diagonal term must be 0 */
          if (T[i + j * p] != 0.0) isTI = PETSC_FALSE;
        }
      }
    }
  }
  a->isTI = isTI;

  PetscCall(PetscFree(a->T));
  if (T && (!isTI)) {
    PetscCall(PetscMalloc1(p * q, &a->T));
    PetscCall(PetscMemcpy(a->T, T, p * q * sizeof(PetscScalar)));
  } else a->T = NULL;

  a->p = p;
  a->q = q;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatDestroy_SeqKAIJ(Mat A)
{
  Mat_SeqKAIJ *b = (Mat_SeqKAIJ *)A->data;

  PetscFunctionBegin;
  PetscCall(MatDestroy(&b->AIJ));
  PetscCall(PetscFree(b->S));
  PetscCall(PetscFree(b->T));
  PetscCall(PetscFree(b->ibdiag));
  PetscCall(PetscFree5(b->sor.w, b->sor.y, b->sor.work, b->sor.t, b->sor.arr));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_seqkaij_seqaij_C", NULL));
  PetscCall(PetscFree(A->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatKAIJ_build_AIJ_OAIJ(Mat A)
{
  Mat_MPIKAIJ     *a;
  Mat_MPIAIJ      *mpiaij;
  PetscScalar     *T;
  PetscInt         i, j;
  PetscObjectState state;

  PetscFunctionBegin;
  a      = (Mat_MPIKAIJ *)A->data;
  mpiaij = (Mat_MPIAIJ *)a->A->data;

  PetscCall(PetscObjectStateGet((PetscObject)a->A, &state));
  if (state == a->state) {
    /* The existing AIJ and KAIJ members are up-to-date, so simply exit. */
    PetscFunctionReturn(PETSC_SUCCESS);
  } else {
    PetscCall(MatDestroy(&a->AIJ));
    PetscCall(MatDestroy(&a->OAIJ));
    if (a->isTI) {
      /* If the transformation matrix associated with the parallel matrix A is the identity matrix, then a->T will be NULL.
       * In this case, if we pass a->T directly to the MatCreateKAIJ() calls to create the sequential submatrices, the routine will
       * not be able to tell that transformation matrix should be set to the identity; thus we create a temporary identity matrix
       * to pass in. */
      PetscCall(PetscMalloc1(a->p * a->q, &T));
      for (i = 0; i < a->p; i++) {
        for (j = 0; j < a->q; j++) {
          if (i == j) T[i + j * a->p] = 1.0;
          else T[i + j * a->p] = 0.0;
        }
      }
    } else T = a->T;
    PetscCall(MatCreateKAIJ(mpiaij->A, a->p, a->q, a->S, T, &a->AIJ));
    PetscCall(MatCreateKAIJ(mpiaij->B, a->p, a->q, NULL, T, &a->OAIJ));
    if (a->isTI) PetscCall(PetscFree(T));
    a->state = state;
  }

  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSetUp_KAIJ(Mat A)
{
  PetscInt     n;
  PetscMPIInt  size;
  Mat_SeqKAIJ *seqkaij = (Mat_SeqKAIJ *)A->data;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)A), &size));
  if (size == 1) {
    PetscCall(MatSetSizes(A, seqkaij->p * seqkaij->AIJ->rmap->n, seqkaij->q * seqkaij->AIJ->cmap->n, seqkaij->p * seqkaij->AIJ->rmap->N, seqkaij->q * seqkaij->AIJ->cmap->N));
    PetscCall(PetscLayoutSetBlockSize(A->rmap, seqkaij->p));
    PetscCall(PetscLayoutSetBlockSize(A->cmap, seqkaij->q));
    PetscCall(PetscLayoutSetUp(A->rmap));
    PetscCall(PetscLayoutSetUp(A->cmap));
  } else {
    Mat_MPIKAIJ *a;
    Mat_MPIAIJ  *mpiaij;
    IS           from, to;
    Vec          gvec;

    a      = (Mat_MPIKAIJ *)A->data;
    mpiaij = (Mat_MPIAIJ *)a->A->data;
    PetscCall(MatSetSizes(A, a->p * a->A->rmap->n, a->q * a->A->cmap->n, a->p * a->A->rmap->N, a->q * a->A->cmap->N));
    PetscCall(PetscLayoutSetBlockSize(A->rmap, seqkaij->p));
    PetscCall(PetscLayoutSetBlockSize(A->cmap, seqkaij->q));
    PetscCall(PetscLayoutSetUp(A->rmap));
    PetscCall(PetscLayoutSetUp(A->cmap));

    PetscCall(MatKAIJ_build_AIJ_OAIJ(A));

    PetscCall(VecGetSize(mpiaij->lvec, &n));
    PetscCall(VecCreate(PETSC_COMM_SELF, &a->w));
    PetscCall(VecSetSizes(a->w, n * a->q, n * a->q));
    PetscCall(VecSetBlockSize(a->w, a->q));
    PetscCall(VecSetType(a->w, VECSEQ));

    /* create two temporary Index sets for build scatter gather */
    PetscCall(ISCreateBlock(PetscObjectComm((PetscObject)a->A), a->q, n, mpiaij->garray, PETSC_COPY_VALUES, &from));
    PetscCall(ISCreateStride(PETSC_COMM_SELF, n * a->q, 0, 1, &to));

    /* create temporary global vector to generate scatter context */
    PetscCall(VecCreateMPIWithArray(PetscObjectComm((PetscObject)a->A), a->q, a->q * a->A->cmap->n, a->q * a->A->cmap->N, NULL, &gvec));

    /* generate the scatter context */
    PetscCall(VecScatterCreate(gvec, from, a->w, to, &a->ctx));

    PetscCall(ISDestroy(&from));
    PetscCall(ISDestroy(&to));
    PetscCall(VecDestroy(&gvec));
  }

  A->assembled = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatView_KAIJ(Mat A, PetscViewer viewer)
{
  PetscViewerFormat format;
  Mat_SeqKAIJ      *a = (Mat_SeqKAIJ *)A->data;
  Mat               B;
  PetscInt          i;
  PetscBool         ismpikaij;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)A, MATMPIKAIJ, &ismpikaij));
  PetscCall(PetscViewerGetFormat(viewer, &format));
  if (format == PETSC_VIEWER_ASCII_INFO || format == PETSC_VIEWER_ASCII_INFO_DETAIL || format == PETSC_VIEWER_ASCII_IMPL) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "S and T have %" PetscInt_FMT " rows and %" PetscInt_FMT " columns\n", a->p, a->q));

    /* Print appropriate details for S. */
    if (!a->S) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "S is NULL\n"));
    } else if (format == PETSC_VIEWER_ASCII_IMPL) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "Entries of S are "));
      for (i = 0; i < (a->p * a->q); i++) {
#if defined(PETSC_USE_COMPLEX)
        PetscCall(PetscViewerASCIIPrintf(viewer, "%18.16e %18.16e ", (double)PetscRealPart(a->S[i]), (double)PetscImaginaryPart(a->S[i])));
#else
        PetscCall(PetscViewerASCIIPrintf(viewer, "%18.16e ", (double)PetscRealPart(a->S[i])));
#endif
      }
      PetscCall(PetscViewerASCIIPrintf(viewer, "\n"));
    }

    /* Print appropriate details for T. */
    if (a->isTI) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "T is the identity matrix\n"));
    } else if (!a->T) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "T is NULL\n"));
    } else if (format == PETSC_VIEWER_ASCII_IMPL) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "Entries of T are "));
      for (i = 0; i < (a->p * a->q); i++) {
#if defined(PETSC_USE_COMPLEX)
        PetscCall(PetscViewerASCIIPrintf(viewer, "%18.16e %18.16e ", (double)PetscRealPart(a->T[i]), (double)PetscImaginaryPart(a->T[i])));
#else
        PetscCall(PetscViewerASCIIPrintf(viewer, "%18.16e ", (double)PetscRealPart(a->T[i])));
#endif
      }
      PetscCall(PetscViewerASCIIPrintf(viewer, "\n"));
    }

    /* Now print details for the AIJ matrix, using the AIJ viewer. */
    PetscCall(PetscViewerASCIIPrintf(viewer, "Now viewing the associated AIJ matrix:\n"));
    if (ismpikaij) {
      Mat_MPIKAIJ *b = (Mat_MPIKAIJ *)A->data;
      PetscCall(MatView(b->A, viewer));
    } else {
      PetscCall(MatView(a->AIJ, viewer));
    }

  } else {
    /* For all other matrix viewer output formats, simply convert to an AIJ matrix and call MatView() on that. */
    PetscCall(MatConvert(A, MATAIJ, MAT_INITIAL_MATRIX, &B));
    PetscCall(MatView(B, viewer));
    PetscCall(MatDestroy(&B));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatDestroy_MPIKAIJ(Mat A)
{
  Mat_MPIKAIJ *b = (Mat_MPIKAIJ *)A->data;

  PetscFunctionBegin;
  PetscCall(MatDestroy(&b->AIJ));
  PetscCall(MatDestroy(&b->OAIJ));
  PetscCall(MatDestroy(&b->A));
  PetscCall(VecScatterDestroy(&b->ctx));
  PetscCall(VecDestroy(&b->w));
  PetscCall(PetscFree(b->S));
  PetscCall(PetscFree(b->T));
  PetscCall(PetscFree(b->ibdiag));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatGetDiagonalBlock_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_mpikaij_mpiaij_C", NULL));
  PetscCall(PetscFree(A->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* zz = yy + Axx */
static PetscErrorCode MatMultAdd_SeqKAIJ(Mat A, Vec xx, Vec yy, Vec zz)
{
  Mat_SeqKAIJ       *b = (Mat_SeqKAIJ *)A->data;
  Mat_SeqAIJ        *a = (Mat_SeqAIJ *)b->AIJ->data;
  const PetscScalar *s = b->S, *t = b->T;
  const PetscScalar *x, *v, *bx;
  PetscScalar       *y, *sums;
  const PetscInt     m = b->AIJ->rmap->n, *idx, *ii;
  PetscInt           n, i, jrow, j, l, p = b->p, q = b->q, k;

  PetscFunctionBegin;
  if (!yy) {
    PetscCall(VecSet(zz, 0.0));
  } else {
    PetscCall(VecCopy(yy, zz));
  }
  if ((!s) && (!t) && (!b->isTI)) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCall(VecGetArrayRead(xx, &x));
  PetscCall(VecGetArray(zz, &y));
  idx = a->j;
  v   = a->a;
  ii  = a->i;

  if (b->isTI) {
    for (i = 0; i < m; i++) {
      jrow = ii[i];
      n    = ii[i + 1] - jrow;
      sums = y + p * i;
      for (j = 0; j < n; j++) {
        for (k = 0; k < p; k++) sums[k] += v[jrow + j] * x[q * idx[jrow + j] + k];
      }
    }
    PetscCall(PetscLogFlops(3.0 * (a->nz) * p));
  } else if (t) {
    for (i = 0; i < m; i++) {
      jrow = ii[i];
      n    = ii[i + 1] - jrow;
      sums = y + p * i;
      for (j = 0; j < n; j++) {
        for (k = 0; k < p; k++) {
          for (l = 0; l < q; l++) sums[k] += v[jrow + j] * t[k + l * p] * x[q * idx[jrow + j] + l];
        }
      }
    }
    /* The flop count below assumes that v[jrow+j] is hoisted out (which an optimizing compiler is likely to do),
     * and also that T part is hoisted outside this loop (in exchange for temporary storage) as (A \otimes I) (I \otimes T),
     * so that this multiply doesn't have to be redone for each matrix entry, but just once per column. The latter
     * transformation is much less likely to be applied, but we nonetheless count the minimum flops required. */
    PetscCall(PetscLogFlops((2.0 * p * q - p) * m + 2.0 * p * a->nz));
  }
  if (s) {
    for (i = 0; i < m; i++) {
      sums = y + p * i;
      bx   = x + q * i;
      if (i < b->AIJ->cmap->n) {
        for (j = 0; j < q; j++) {
          for (k = 0; k < p; k++) sums[k] += s[k + j * p] * bx[j];
        }
      }
    }
    PetscCall(PetscLogFlops(2.0 * m * p * q));
  }

  PetscCall(VecRestoreArrayRead(xx, &x));
  PetscCall(VecRestoreArray(zz, &y));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMult_SeqKAIJ(Mat A, Vec xx, Vec yy)
{
  PetscFunctionBegin;
  PetscCall(MatMultAdd_SeqKAIJ(A, xx, NULL, yy));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#include <petsc/private/kernels/blockinvert.h>

static PetscErrorCode MatInvertBlockDiagonal_SeqKAIJ(Mat A, const PetscScalar **values)
{
  Mat_SeqKAIJ       *b = (Mat_SeqKAIJ *)A->data;
  Mat_SeqAIJ        *a = (Mat_SeqAIJ *)b->AIJ->data;
  const PetscScalar *S = b->S;
  const PetscScalar *T = b->T;
  const PetscScalar *v = a->a;
  const PetscInt     p = b->p, q = b->q, m = b->AIJ->rmap->n, *idx = a->j, *ii = a->i;
  PetscInt           i, j, *v_pivots, dof, dof2;
  PetscScalar       *diag, aval, *v_work;

  PetscFunctionBegin;
  PetscCheck(p == q, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "MATKAIJ: Block size must be square to calculate inverse.");
  PetscCheck(S || T || b->isTI, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "MATKAIJ: Cannot invert a zero matrix.");

  dof  = p;
  dof2 = dof * dof;

  if (b->ibdiagvalid) {
    if (values) *values = b->ibdiag;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (!b->ibdiag) PetscCall(PetscMalloc1(dof2 * m, &b->ibdiag));
  if (values) *values = b->ibdiag;
  diag = b->ibdiag;

  PetscCall(PetscMalloc2(dof, &v_work, dof, &v_pivots));
  for (i = 0; i < m; i++) {
    if (S) {
      PetscCall(PetscMemcpy(diag, S, dof2 * sizeof(PetscScalar)));
    } else {
      PetscCall(PetscMemzero(diag, dof2 * sizeof(PetscScalar)));
    }
    if (b->isTI) {
      aval = 0;
      for (j = ii[i]; j < ii[i + 1]; j++)
        if (idx[j] == i) aval = v[j];
      for (j = 0; j < dof; j++) diag[j + dof * j] += aval;
    } else if (T) {
      aval = 0;
      for (j = ii[i]; j < ii[i + 1]; j++)
        if (idx[j] == i) aval = v[j];
      for (j = 0; j < dof2; j++) diag[j] += aval * T[j];
    }
    PetscCall(PetscKernel_A_gets_inverse_A(dof, diag, v_pivots, v_work, PETSC_FALSE, NULL));
    diag += dof2;
  }
  PetscCall(PetscFree2(v_work, v_pivots));

  b->ibdiagvalid = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatGetDiagonalBlock_MPIKAIJ(Mat A, Mat *B)
{
  Mat_MPIKAIJ *kaij = (Mat_MPIKAIJ *)A->data;

  PetscFunctionBegin;
  *B = kaij->AIJ;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatConvert_KAIJ_AIJ(Mat A, MatType newtype, MatReuse reuse, Mat *newmat)
{
  Mat_SeqKAIJ   *a = (Mat_SeqKAIJ *)A->data;
  Mat            AIJ, OAIJ, B;
  PetscInt      *d_nnz, *o_nnz = NULL, nz, i, j, m, d;
  const PetscInt p = a->p, q = a->q;
  PetscBool      ismpikaij, missing;

  PetscFunctionBegin;
  if (reuse != MAT_REUSE_MATRIX) {
    PetscCall(PetscObjectTypeCompare((PetscObject)A, MATMPIKAIJ, &ismpikaij));
    if (ismpikaij) {
      Mat_MPIKAIJ *b = (Mat_MPIKAIJ *)A->data;
      AIJ            = ((Mat_SeqKAIJ *)b->AIJ->data)->AIJ;
      OAIJ           = ((Mat_SeqKAIJ *)b->OAIJ->data)->AIJ;
    } else {
      AIJ  = a->AIJ;
      OAIJ = NULL;
    }
    PetscCall(MatCreate(PetscObjectComm((PetscObject)A), &B));
    PetscCall(MatSetSizes(B, A->rmap->n, A->cmap->n, A->rmap->N, A->cmap->N));
    PetscCall(MatSetType(B, MATAIJ));
    PetscCall(MatGetSize(AIJ, &m, NULL));
    PetscCall(MatMissingDiagonal(AIJ, &missing, &d)); /* assumption that all successive rows will have a missing diagonal */
    if (!missing || !a->S) d = m;
    PetscCall(PetscMalloc1(m * p, &d_nnz));
    for (i = 0; i < m; ++i) {
      PetscCall(MatGetRow_SeqAIJ(AIJ, i, &nz, NULL, NULL));
      for (j = 0; j < p; ++j) d_nnz[i * p + j] = nz * q + (i >= d) * q;
      PetscCall(MatRestoreRow_SeqAIJ(AIJ, i, &nz, NULL, NULL));
    }
    if (OAIJ) {
      PetscCall(PetscMalloc1(m * p, &o_nnz));
      for (i = 0; i < m; ++i) {
        PetscCall(MatGetRow_SeqAIJ(OAIJ, i, &nz, NULL, NULL));
        for (j = 0; j < p; ++j) o_nnz[i * p + j] = nz * q;
        PetscCall(MatRestoreRow_SeqAIJ(OAIJ, i, &nz, NULL, NULL));
      }
      PetscCall(MatMPIAIJSetPreallocation(B, 0, d_nnz, 0, o_nnz));
    } else {
      PetscCall(MatSeqAIJSetPreallocation(B, 0, d_nnz));
    }
    PetscCall(PetscFree(d_nnz));
    PetscCall(PetscFree(o_nnz));
  } else B = *newmat;
  PetscCall(MatConvert_Basic(A, newtype, MAT_REUSE_MATRIX, &B));
  if (reuse == MAT_INPLACE_MATRIX) {
    PetscCall(MatHeaderReplace(A, &B));
  } else *newmat = B;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSOR_SeqKAIJ(Mat A, Vec bb, PetscReal omega, MatSORType flag, PetscReal fshift, PetscInt its, PetscInt lits, Vec xx)
{
  Mat_SeqKAIJ       *kaij = (Mat_SeqKAIJ *)A->data;
  Mat_SeqAIJ        *a    = (Mat_SeqAIJ *)kaij->AIJ->data;
  const PetscScalar *aa = a->a, *T = kaij->T, *v;
  const PetscInt     m = kaij->AIJ->rmap->n, *ai = a->i, *aj = a->j, p = kaij->p, q = kaij->q, *diag, *vi;
  const PetscScalar *b, *xb, *idiag;
  PetscScalar       *x, *work, *workt, *w, *y, *arr, *t, *arrt;
  PetscInt           i, j, k, i2, bs, bs2, nz;

  PetscFunctionBegin;
  its = its * lits;
  PetscCheck(!(flag & SOR_EISENSTAT), PETSC_COMM_SELF, PETSC_ERR_SUP, "No support yet for Eisenstat");
  PetscCheck(its > 0, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Relaxation requires global its %" PetscInt_FMT " and local its %" PetscInt_FMT " both positive", its, lits);
  PetscCheck(!fshift, PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for diagonal shift");
  PetscCheck(!(flag & SOR_APPLY_UPPER) && !(flag & SOR_APPLY_LOWER), PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for applying upper or lower triangular parts");
  PetscCheck(p == q, PETSC_COMM_SELF, PETSC_ERR_SUP, "MatSOR for KAIJ: No support for non-square dense blocks");
  bs  = p;
  bs2 = bs * bs;

  if (!m) PetscFunctionReturn(PETSC_SUCCESS);

  if (!kaij->ibdiagvalid) PetscCall(MatInvertBlockDiagonal_SeqKAIJ(A, NULL));
  idiag = kaij->ibdiag;
  diag  = a->diag;

  if (!kaij->sor.setup) {
    PetscCall(PetscMalloc5(bs, &kaij->sor.w, bs, &kaij->sor.y, m * bs, &kaij->sor.work, m * bs, &kaij->sor.t, m * bs2, &kaij->sor.arr));
    kaij->sor.setup = PETSC_TRUE;
  }
  y    = kaij->sor.y;
  w    = kaij->sor.w;
  work = kaij->sor.work;
  t    = kaij->sor.t;
  arr  = kaij->sor.arr;

  PetscCall(VecGetArray(xx, &x));
  PetscCall(VecGetArrayRead(bb, &b));

  if (flag & SOR_ZERO_INITIAL_GUESS) {
    if (flag & SOR_FORWARD_SWEEP || flag & SOR_LOCAL_FORWARD_SWEEP) {
      PetscKernel_w_gets_Ar_times_v(bs, bs, b, idiag, x); /* x[0:bs] <- D^{-1} b[0:bs] */
      PetscCall(PetscMemcpy(t, b, bs * sizeof(PetscScalar)));
      i2 = bs;
      idiag += bs2;
      for (i = 1; i < m; i++) {
        v  = aa + ai[i];
        vi = aj + ai[i];
        nz = diag[i] - ai[i];

        if (T) { /* b - T (Arow * x) */
          PetscCall(PetscMemzero(w, bs * sizeof(PetscScalar)));
          for (j = 0; j < nz; j++) {
            for (k = 0; k < bs; k++) w[k] -= v[j] * x[vi[j] * bs + k];
          }
          PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs, w, T, &t[i2]);
          for (k = 0; k < bs; k++) t[i2 + k] += b[i2 + k];
        } else if (kaij->isTI) {
          PetscCall(PetscMemcpy(t + i2, b + i2, bs * sizeof(PetscScalar)));
          for (j = 0; j < nz; j++) {
            for (k = 0; k < bs; k++) t[i2 + k] -= v[j] * x[vi[j] * bs + k];
          }
        } else {
          PetscCall(PetscMemcpy(t + i2, b + i2, bs * sizeof(PetscScalar)));
        }

        PetscKernel_w_gets_Ar_times_v(bs, bs, t + i2, idiag, y);
        for (j = 0; j < bs; j++) x[i2 + j] = omega * y[j];

        idiag += bs2;
        i2 += bs;
      }
      /* for logging purposes assume number of nonzero in lower half is 1/2 of total */
      PetscCall(PetscLogFlops(1.0 * bs2 * a->nz));
      xb = t;
    } else xb = b;
    if (flag & SOR_BACKWARD_SWEEP || flag & SOR_LOCAL_BACKWARD_SWEEP) {
      idiag = kaij->ibdiag + bs2 * (m - 1);
      i2    = bs * (m - 1);
      PetscCall(PetscMemcpy(w, xb + i2, bs * sizeof(PetscScalar)));
      PetscKernel_w_gets_Ar_times_v(bs, bs, w, idiag, x + i2);
      i2 -= bs;
      idiag -= bs2;
      for (i = m - 2; i >= 0; i--) {
        v  = aa + diag[i] + 1;
        vi = aj + diag[i] + 1;
        nz = ai[i + 1] - diag[i] - 1;

        if (T) { /* FIXME: This branch untested */
          PetscCall(PetscMemcpy(w, xb + i2, bs * sizeof(PetscScalar)));
          /* copy all rows of x that are needed into contiguous space */
          workt = work;
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemcpy(workt, x + bs * (*vi++), bs * sizeof(PetscScalar)));
            workt += bs;
          }
          arrt = arr;
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemcpy(arrt, T, bs2 * sizeof(PetscScalar)));
            for (k = 0; k < bs2; k++) arrt[k] *= v[j];
            arrt += bs2;
          }
          PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
        } else if (kaij->isTI) {
          PetscCall(PetscMemcpy(w, t + i2, bs * sizeof(PetscScalar)));
          for (j = 0; j < nz; j++) {
            for (k = 0; k < bs; k++) w[k] -= v[j] * x[vi[j] * bs + k];
          }
        }

        PetscKernel_w_gets_Ar_times_v(bs, bs, w, idiag, y); /* RHS incorrect for omega != 1.0 */
        for (j = 0; j < bs; j++) x[i2 + j] = (1.0 - omega) * x[i2 + j] + omega * y[j];

        idiag -= bs2;
        i2 -= bs;
      }
      PetscCall(PetscLogFlops(1.0 * bs2 * (a->nz)));
    }
    its--;
  }
  while (its--) { /* FIXME: This branch not updated */
    if (flag & SOR_FORWARD_SWEEP || flag & SOR_LOCAL_FORWARD_SWEEP) {
      i2    = 0;
      idiag = kaij->ibdiag;
      for (i = 0; i < m; i++) {
        PetscCall(PetscMemcpy(w, b + i2, bs * sizeof(PetscScalar)));

        v     = aa + ai[i];
        vi    = aj + ai[i];
        nz    = diag[i] - ai[i];
        workt = work;
        for (j = 0; j < nz; j++) {
          PetscCall(PetscMemcpy(workt, x + bs * (*vi++), bs * sizeof(PetscScalar)));
          workt += bs;
        }
        arrt = arr;
        if (T) {
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemcpy(arrt, T, bs2 * sizeof(PetscScalar)));
            for (k = 0; k < bs2; k++) arrt[k] *= v[j];
            arrt += bs2;
          }
          PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
        } else if (kaij->isTI) {
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemzero(arrt, bs2 * sizeof(PetscScalar)));
            for (k = 0; k < bs; k++) arrt[k + bs * k] = v[j];
            arrt += bs2;
          }
          PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
        }
        PetscCall(PetscMemcpy(t + i2, w, bs * sizeof(PetscScalar)));

        v     = aa + diag[i] + 1;
        vi    = aj + diag[i] + 1;
        nz    = ai[i + 1] - diag[i] - 1;
        workt = work;
        for (j = 0; j < nz; j++) {
          PetscCall(PetscMemcpy(workt, x + bs * (*vi++), bs * sizeof(PetscScalar)));
          workt += bs;
        }
        arrt = arr;
        if (T) {
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemcpy(arrt, T, bs2 * sizeof(PetscScalar)));
            for (k = 0; k < bs2; k++) arrt[k] *= v[j];
            arrt += bs2;
          }
          PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
        } else if (kaij->isTI) {
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemzero(arrt, bs2 * sizeof(PetscScalar)));
            for (k = 0; k < bs; k++) arrt[k + bs * k] = v[j];
            arrt += bs2;
          }
          PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
        }

        PetscKernel_w_gets_Ar_times_v(bs, bs, w, idiag, y);
        for (j = 0; j < bs; j++) *(x + i2 + j) = (1.0 - omega) * *(x + i2 + j) + omega * *(y + j);

        idiag += bs2;
        i2 += bs;
      }
      xb = t;
    } else xb = b;
    if (flag & SOR_BACKWARD_SWEEP || flag & SOR_LOCAL_BACKWARD_SWEEP) {
      idiag = kaij->ibdiag + bs2 * (m - 1);
      i2    = bs * (m - 1);
      if (xb == b) {
        for (i = m - 1; i >= 0; i--) {
          PetscCall(PetscMemcpy(w, b + i2, bs * sizeof(PetscScalar)));

          v     = aa + ai[i];
          vi    = aj + ai[i];
          nz    = diag[i] - ai[i];
          workt = work;
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemcpy(workt, x + bs * (*vi++), bs * sizeof(PetscScalar)));
            workt += bs;
          }
          arrt = arr;
          if (T) {
            for (j = 0; j < nz; j++) {
              PetscCall(PetscMemcpy(arrt, T, bs2 * sizeof(PetscScalar)));
              for (k = 0; k < bs2; k++) arrt[k] *= v[j];
              arrt += bs2;
            }
            PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
          } else if (kaij->isTI) {
            for (j = 0; j < nz; j++) {
              PetscCall(PetscMemzero(arrt, bs2 * sizeof(PetscScalar)));
              for (k = 0; k < bs; k++) arrt[k + bs * k] = v[j];
              arrt += bs2;
            }
            PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
          }

          v     = aa + diag[i] + 1;
          vi    = aj + diag[i] + 1;
          nz    = ai[i + 1] - diag[i] - 1;
          workt = work;
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemcpy(workt, x + bs * (*vi++), bs * sizeof(PetscScalar)));
            workt += bs;
          }
          arrt = arr;
          if (T) {
            for (j = 0; j < nz; j++) {
              PetscCall(PetscMemcpy(arrt, T, bs2 * sizeof(PetscScalar)));
              for (k = 0; k < bs2; k++) arrt[k] *= v[j];
              arrt += bs2;
            }
            PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
          } else if (kaij->isTI) {
            for (j = 0; j < nz; j++) {
              PetscCall(PetscMemzero(arrt, bs2 * sizeof(PetscScalar)));
              for (k = 0; k < bs; k++) arrt[k + bs * k] = v[j];
              arrt += bs2;
            }
            PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
          }

          PetscKernel_w_gets_Ar_times_v(bs, bs, w, idiag, y);
          for (j = 0; j < bs; j++) *(x + i2 + j) = (1.0 - omega) * *(x + i2 + j) + omega * *(y + j);
        }
      } else {
        for (i = m - 1; i >= 0; i--) {
          PetscCall(PetscMemcpy(w, xb + i2, bs * sizeof(PetscScalar)));
          v     = aa + diag[i] + 1;
          vi    = aj + diag[i] + 1;
          nz    = ai[i + 1] - diag[i] - 1;
          workt = work;
          for (j = 0; j < nz; j++) {
            PetscCall(PetscMemcpy(workt, x + bs * (*vi++), bs * sizeof(PetscScalar)));
            workt += bs;
          }
          arrt = arr;
          if (T) {
            for (j = 0; j < nz; j++) {
              PetscCall(PetscMemcpy(arrt, T, bs2 * sizeof(PetscScalar)));
              for (k = 0; k < bs2; k++) arrt[k] *= v[j];
              arrt += bs2;
            }
            PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
          } else if (kaij->isTI) {
            for (j = 0; j < nz; j++) {
              PetscCall(PetscMemzero(arrt, bs2 * sizeof(PetscScalar)));
              for (k = 0; k < bs; k++) arrt[k + bs * k] = v[j];
              arrt += bs2;
            }
            PetscKernel_w_gets_w_minus_Ar_times_v(bs, bs * nz, w, arr, work);
          }
          PetscKernel_w_gets_Ar_times_v(bs, bs, w, idiag, y);
          for (j = 0; j < bs; j++) *(x + i2 + j) = (1.0 - omega) * *(x + i2 + j) + omega * *(y + j);
        }
      }
      PetscCall(PetscLogFlops(1.0 * bs2 * (a->nz)));
    }
  }

  PetscCall(VecRestoreArray(xx, &x));
  PetscCall(VecRestoreArrayRead(bb, &b));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*===================================================================================*/

static PetscErrorCode MatMultAdd_MPIKAIJ(Mat A, Vec xx, Vec yy, Vec zz)
{
  Mat_MPIKAIJ *b = (Mat_MPIKAIJ *)A->data;

  PetscFunctionBegin;
  if (!yy) {
    PetscCall(VecSet(zz, 0.0));
  } else {
    PetscCall(VecCopy(yy, zz));
  }
  PetscCall(MatKAIJ_build_AIJ_OAIJ(A)); /* Ensure b->AIJ and b->OAIJ are up to date. */
  /* start the scatter */
  PetscCall(VecScatterBegin(b->ctx, xx, b->w, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall((*b->AIJ->ops->multadd)(b->AIJ, xx, zz, zz));
  PetscCall(VecScatterEnd(b->ctx, xx, b->w, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall((*b->OAIJ->ops->multadd)(b->OAIJ, b->w, zz, zz));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMult_MPIKAIJ(Mat A, Vec xx, Vec yy)
{
  PetscFunctionBegin;
  PetscCall(MatMultAdd_MPIKAIJ(A, xx, NULL, yy));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatInvertBlockDiagonal_MPIKAIJ(Mat A, const PetscScalar **values)
{
  Mat_MPIKAIJ *b = (Mat_MPIKAIJ *)A->data;

  PetscFunctionBegin;
  PetscCall(MatKAIJ_build_AIJ_OAIJ(A)); /* Ensure b->AIJ is up to date. */
  PetscCall((*b->AIJ->ops->invertblockdiagonal)(b->AIJ, values));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatGetRow_SeqKAIJ(Mat A, PetscInt row, PetscInt *ncols, PetscInt **cols, PetscScalar **values)
{
  Mat_SeqKAIJ *b    = (Mat_SeqKAIJ *)A->data;
  PetscBool    diag = PETSC_FALSE;
  PetscInt     nzaij, nz, *colsaij, *idx, i, j, p = b->p, q = b->q, r = row / p, s = row % p, c;
  PetscScalar *vaij, *v, *S = b->S, *T = b->T;

  PetscFunctionBegin;
  PetscCheck(!b->getrowactive, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Already active");
  b->getrowactive = PETSC_TRUE;
  PetscCheck(row >= 0 && row < A->rmap->n, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Row %" PetscInt_FMT " out of range", row);

  if ((!S) && (!T) && (!b->isTI)) {
    if (ncols) *ncols = 0;
    if (cols) *cols = NULL;
    if (values) *values = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  if (T || b->isTI) {
    PetscCall(MatGetRow_SeqAIJ(b->AIJ, r, &nzaij, &colsaij, &vaij));
    c = nzaij;
    for (i = 0; i < nzaij; i++) {
      /* check if this row contains a diagonal entry */
      if (colsaij[i] == r) {
        diag = PETSC_TRUE;
        c    = i;
      }
    }
  } else nzaij = c = 0;

  /* calculate size of row */
  nz = 0;
  if (S) nz += q;
  if (T || b->isTI) nz += (diag && S ? (nzaij - 1) * q : nzaij * q);

  if (cols || values) {
    PetscCall(PetscMalloc2(nz, &idx, nz, &v));
    for (i = 0; i < q; i++) {
      /* We need to initialize the v[i] to zero to handle the case in which T is NULL (not the identity matrix). */
      v[i] = 0.0;
    }
    if (b->isTI) {
      for (i = 0; i < nzaij; i++) {
        for (j = 0; j < q; j++) {
          idx[i * q + j] = colsaij[i] * q + j;
          v[i * q + j]   = (j == s ? vaij[i] : 0);
        }
      }
    } else if (T) {
      for (i = 0; i < nzaij; i++) {
        for (j = 0; j < q; j++) {
          idx[i * q + j] = colsaij[i] * q + j;
          v[i * q + j]   = vaij[i] * T[s + j * p];
        }
      }
    }
    if (S) {
      for (j = 0; j < q; j++) {
        idx[c * q + j] = r * q + j;
        v[c * q + j] += S[s + j * p];
      }
    }
  }

  if (ncols) *ncols = nz;
  if (cols) *cols = idx;
  if (values) *values = v;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatRestoreRow_SeqKAIJ(Mat A, PetscInt row, PetscInt *nz, PetscInt **idx, PetscScalar **v)
{
  PetscFunctionBegin;
  if (nz) *nz = 0;
  PetscCall(PetscFree2(*idx, *v));
  ((Mat_SeqKAIJ *)A->data)->getrowactive = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatGetRow_MPIKAIJ(Mat A, PetscInt row, PetscInt *ncols, PetscInt **cols, PetscScalar **values)
{
  Mat_MPIKAIJ   *b    = (Mat_MPIKAIJ *)A->data;
  Mat            AIJ  = b->A;
  PetscBool      diag = PETSC_FALSE;
  Mat            MatAIJ, MatOAIJ;
  const PetscInt rstart = A->rmap->rstart, rend = A->rmap->rend, p = b->p, q = b->q, *garray;
  PetscInt       nz, *idx, ncolsaij = 0, ncolsoaij = 0, *colsaij, *colsoaij, r, s, c, i, j, lrow;
  PetscScalar   *v, *vals, *ovals, *S = b->S, *T = b->T;

  PetscFunctionBegin;
  PetscCall(MatKAIJ_build_AIJ_OAIJ(A)); /* Ensure b->AIJ and b->OAIJ are up to date. */
  MatAIJ  = ((Mat_SeqKAIJ *)b->AIJ->data)->AIJ;
  MatOAIJ = ((Mat_SeqKAIJ *)b->OAIJ->data)->AIJ;
  PetscCheck(!b->getrowactive, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Already active");
  b->getrowactive = PETSC_TRUE;
  PetscCheck(row >= rstart && row < rend, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Only local rows");
  lrow = row - rstart;

  if ((!S) && (!T) && (!b->isTI)) {
    if (ncols) *ncols = 0;
    if (cols) *cols = NULL;
    if (values) *values = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  r = lrow / p;
  s = lrow % p;

  if (T || b->isTI) {
    PetscCall(MatMPIAIJGetSeqAIJ(AIJ, NULL, NULL, &garray));
    PetscCall(MatGetRow_SeqAIJ(MatAIJ, lrow / p, &ncolsaij, &colsaij, &vals));
    PetscCall(MatGetRow_SeqAIJ(MatOAIJ, lrow / p, &ncolsoaij, &colsoaij, &ovals));

    c = ncolsaij + ncolsoaij;
    for (i = 0; i < ncolsaij; i++) {
      /* check if this row contains a diagonal entry */
      if (colsaij[i] == r) {
        diag = PETSC_TRUE;
        c    = i;
      }
    }
  } else c = 0;

  /* calculate size of row */
  nz = 0;
  if (S) nz += q;
  if (T || b->isTI) nz += (diag && S ? (ncolsaij + ncolsoaij - 1) * q : (ncolsaij + ncolsoaij) * q);

  if (cols || values) {
    PetscCall(PetscMalloc2(nz, &idx, nz, &v));
    for (i = 0; i < q; i++) {
      /* We need to initialize the v[i] to zero to handle the case in which T is NULL (not the identity matrix). */
      v[i] = 0.0;
    }
    if (b->isTI) {
      for (i = 0; i < ncolsaij; i++) {
        for (j = 0; j < q; j++) {
          idx[i * q + j] = (colsaij[i] + rstart / p) * q + j;
          v[i * q + j]   = (j == s ? vals[i] : 0.0);
        }
      }
      for (i = 0; i < ncolsoaij; i++) {
        for (j = 0; j < q; j++) {
          idx[(i + ncolsaij) * q + j] = garray[colsoaij[i]] * q + j;
          v[(i + ncolsaij) * q + j]   = (j == s ? ovals[i] : 0.0);
        }
      }
    } else if (T) {
      for (i = 0; i < ncolsaij; i++) {
        for (j = 0; j < q; j++) {
          idx[i * q + j] = (colsaij[i] + rstart / p) * q + j;
          v[i * q + j]   = vals[i] * T[s + j * p];
        }
      }
      for (i = 0; i < ncolsoaij; i++) {
        for (j = 0; j < q; j++) {
          idx[(i + ncolsaij) * q + j] = garray[colsoaij[i]] * q + j;
          v[(i + ncolsaij) * q + j]   = ovals[i] * T[s + j * p];
        }
      }
    }
    if (S) {
      for (j = 0; j < q; j++) {
        idx[c * q + j] = (r + rstart / p) * q + j;
        v[c * q + j] += S[s + j * p];
      }
    }
  }

  if (ncols) *ncols = nz;
  if (cols) *cols = idx;
  if (values) *values = v;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatRestoreRow_MPIKAIJ(Mat A, PetscInt row, PetscInt *nz, PetscInt **idx, PetscScalar **v)
{
  PetscFunctionBegin;
  PetscCall(PetscFree2(*idx, *v));
  ((Mat_SeqKAIJ *)A->data)->getrowactive = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatCreateSubMatrix_KAIJ(Mat mat, IS isrow, IS iscol, MatReuse cll, Mat *newmat)
{
  Mat A;

  PetscFunctionBegin;
  PetscCall(MatConvert(mat, MATAIJ, MAT_INITIAL_MATRIX, &A));
  PetscCall(MatCreateSubMatrix(A, isrow, iscol, cll, newmat));
  PetscCall(MatDestroy(&A));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  MatCreateKAIJ - Creates a matrix of type `MATKAIJ` to be used for matrices of the following form
.vb
    [I \otimes S + A \otimes T]
.ve
  where
.vb
    S is a dense (p \times q) matrix
    T is a dense (p \times q) matrix
    A is a `MATAIJ`  (n \times n) matrix
    I is the identity matrix
.ve
  The resulting matrix is (np \times nq)

  `S` and `T` are always stored independently on all processes as `PetscScalar` arrays in column-major format.

  Collective

  Input Parameters:
+ A - the `MATAIJ` matrix
. p - number of rows in `S` and `T`
. q - number of columns in `S` and `T`
. S - the `S` matrix (can be `NULL`), stored as a `PetscScalar` array (column-major)
- T - the `T` matrix (can be `NULL`), stored as a `PetscScalar` array (column-major)

  Output Parameter:
. kaij - the new `MATKAIJ` matrix

  Level: advanced

  Notes:
  This function increases the reference count on the `MATAIJ` matrix, so the user is free to destroy the matrix if it is not needed.

  Changes to the entries of the `MATAIJ` matrix will immediately affect the `MATKAIJ` matrix.

  Developer Note:
  In the `MATMPIKAIJ` case, the internal 'AIJ' and 'OAIJ' sequential KAIJ matrices are kept up to date by tracking the object state
  of the AIJ matrix 'A' that describes the blockwise action of the `MATMPIKAIJ` matrix and, if the object state has changed, lazily
  rebuilding 'AIJ' and 'OAIJ' just before executing operations with the `MATMPIKAIJ` matrix. If new types of operations are added,
  routines implementing those must also ensure these are rebuilt when needed (by calling the internal MatKAIJ_build_AIJ_OAIJ() routine).

.seealso: [](chapter_matrices), `Mat`, `MatKAIJSetAIJ()`, `MatKAIJSetS()`, `MatKAIJSetT()`, `MatKAIJGetAIJ()`, `MatKAIJGetS()`, `MatKAIJGetT()`, `MATKAIJ`
@*/
PetscErrorCode MatCreateKAIJ(Mat A, PetscInt p, PetscInt q, const PetscScalar S[], const PetscScalar T[], Mat *kaij)
{
  PetscFunctionBegin;
  PetscCall(MatCreate(PetscObjectComm((PetscObject)A), kaij));
  PetscCall(MatSetType(*kaij, MATKAIJ));
  PetscCall(MatKAIJSetAIJ(*kaij, A));
  PetscCall(MatKAIJSetS(*kaij, p, q, S));
  PetscCall(MatKAIJSetT(*kaij, p, q, T));
  PetscCall(MatSetUp(*kaij));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
  MATKAIJ - MATKAIJ = "kaij" - A matrix type to be used to evaluate matrices of form
    [I \otimes S + A \otimes T],
  where
.vb
    S is a dense (p \times q) matrix,
    T is a dense (p \times q) matrix,
    A is an AIJ  (n \times n) matrix,
    and I is the identity matrix.
.ve
  The resulting matrix is (np \times nq).

  S and T are always stored independently on all processes as `PetscScalar` arrays in column-major format.

  Level: advanced

  Note:
  A linear system with multiple right-hand sides, AX = B, can be expressed in the KAIJ-friendly form of (A \otimes I) x = b,
  where x and b are column vectors containing the row-major representations of X and B.

.seealso: [](chapter_matrices), `Mat`, `MatKAIJSetAIJ()`, `MatKAIJSetS()`, `MatKAIJSetT()`, `MatKAIJGetAIJ()`, `MatKAIJGetS()`, `MatKAIJGetT()`, `MatCreateKAIJ()`
M*/

PETSC_EXTERN PetscErrorCode MatCreate_KAIJ(Mat A)
{
  Mat_MPIKAIJ *b;
  PetscMPIInt  size;

  PetscFunctionBegin;
  PetscCall(PetscNew(&b));
  A->data = (void *)b;

  PetscCall(PetscMemzero(A->ops, sizeof(struct _MatOps)));

  b->w = NULL;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)A), &size));
  if (size == 1) {
    PetscCall(PetscObjectChangeTypeName((PetscObject)A, MATSEQKAIJ));
    A->ops->destroy             = MatDestroy_SeqKAIJ;
    A->ops->mult                = MatMult_SeqKAIJ;
    A->ops->multadd             = MatMultAdd_SeqKAIJ;
    A->ops->invertblockdiagonal = MatInvertBlockDiagonal_SeqKAIJ;
    A->ops->getrow              = MatGetRow_SeqKAIJ;
    A->ops->restorerow          = MatRestoreRow_SeqKAIJ;
    A->ops->sor                 = MatSOR_SeqKAIJ;
    PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_seqkaij_seqaij_C", MatConvert_KAIJ_AIJ));
  } else {
    PetscCall(PetscObjectChangeTypeName((PetscObject)A, MATMPIKAIJ));
    A->ops->destroy             = MatDestroy_MPIKAIJ;
    A->ops->mult                = MatMult_MPIKAIJ;
    A->ops->multadd             = MatMultAdd_MPIKAIJ;
    A->ops->invertblockdiagonal = MatInvertBlockDiagonal_MPIKAIJ;
    A->ops->getrow              = MatGetRow_MPIKAIJ;
    A->ops->restorerow          = MatRestoreRow_MPIKAIJ;
    PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatGetDiagonalBlock_C", MatGetDiagonalBlock_MPIKAIJ));
    PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_mpikaij_mpiaij_C", MatConvert_KAIJ_AIJ));
  }
  A->ops->setup           = MatSetUp_KAIJ;
  A->ops->view            = MatView_KAIJ;
  A->ops->createsubmatrix = MatCreateSubMatrix_KAIJ;
  PetscFunctionReturn(PETSC_SUCCESS);
}

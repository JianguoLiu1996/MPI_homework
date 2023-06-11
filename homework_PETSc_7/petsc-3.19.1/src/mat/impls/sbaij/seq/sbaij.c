
/*
    Defines the basic matrix operations for the SBAIJ (compressed row)
  matrix storage format.
*/
#include <../src/mat/impls/baij/seq/baij.h> /*I "petscmat.h" I*/
#include <../src/mat/impls/sbaij/seq/sbaij.h>
#include <petscblaslapack.h>

#include <../src/mat/impls/sbaij/seq/relax.h>
#define USESHORT
#include <../src/mat/impls/sbaij/seq/relax.h>

/* defines MatSetValues_Seq_Hash(), MatAssemblyEnd_Seq_Hash(), MatSetUp_Seq_Hash() */
#define TYPE SBAIJ
#define TYPE_SBAIJ
#define TYPE_BS
#include "../src/mat/impls/aij/seq/seqhashmatsetvalues.h"
#undef TYPE_BS
#define TYPE_BS _BS
#define TYPE_BS_ON
#include "../src/mat/impls/aij/seq/seqhashmatsetvalues.h"
#undef TYPE_BS
#undef TYPE_SBAIJ
#include "../src/mat/impls/aij/seq/seqhashmat.h"
#undef TYPE
#undef TYPE_BS_ON

#if defined(PETSC_HAVE_ELEMENTAL)
PETSC_INTERN PetscErrorCode MatConvert_SeqSBAIJ_Elemental(Mat, MatType, MatReuse, Mat *);
#endif
#if defined(PETSC_HAVE_SCALAPACK)
PETSC_INTERN PetscErrorCode MatConvert_SBAIJ_ScaLAPACK(Mat, MatType, MatReuse, Mat *);
#endif
PETSC_INTERN PetscErrorCode MatConvert_MPISBAIJ_Basic(Mat, MatType, MatReuse, Mat *);

/*
     Checks for missing diagonals
*/
PetscErrorCode MatMissingDiagonal_SeqSBAIJ(Mat A, PetscBool *missing, PetscInt *dd)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt     *diag, *ii = a->i, i;

  PetscFunctionBegin;
  PetscCall(MatMarkDiagonal_SeqSBAIJ(A));
  *missing = PETSC_FALSE;
  if (A->rmap->n > 0 && !ii) {
    *missing = PETSC_TRUE;
    if (dd) *dd = 0;
    PetscCall(PetscInfo(A, "Matrix has no entries therefore is missing diagonal\n"));
  } else {
    diag = a->diag;
    for (i = 0; i < a->mbs; i++) {
      if (diag[i] >= ii[i + 1]) {
        *missing = PETSC_TRUE;
        if (dd) *dd = i;
        break;
      }
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatMarkDiagonal_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt      i, j;

  PetscFunctionBegin;
  if (!a->diag) {
    PetscCall(PetscMalloc1(a->mbs, &a->diag));
    a->free_diag = PETSC_TRUE;
  }
  for (i = 0; i < a->mbs; i++) {
    a->diag[i] = a->i[i + 1];
    for (j = a->i[i]; j < a->i[i + 1]; j++) {
      if (a->j[j] == i) {
        a->diag[i] = j;
        break;
      }
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatGetRowIJ_SeqSBAIJ(Mat A, PetscInt oshift, PetscBool symmetric, PetscBool blockcompressed, PetscInt *nn, const PetscInt *inia[], const PetscInt *inja[], PetscBool *done)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt      i, j, n = a->mbs, nz = a->i[n], *tia, *tja, bs = A->rmap->bs, k, l, cnt;
  PetscInt    **ia = (PetscInt **)inia, **ja = (PetscInt **)inja;

  PetscFunctionBegin;
  *nn = n;
  if (!ia) PetscFunctionReturn(PETSC_SUCCESS);
  if (symmetric) {
    PetscCall(MatToSymmetricIJ_SeqAIJ(n, a->i, a->j, PETSC_FALSE, 0, 0, &tia, &tja));
    nz = tia[n];
  } else {
    tia = a->i;
    tja = a->j;
  }

  if (!blockcompressed && bs > 1) {
    (*nn) *= bs;
    /* malloc & create the natural set of indices */
    PetscCall(PetscMalloc1((n + 1) * bs, ia));
    if (n) {
      (*ia)[0] = oshift;
      for (j = 1; j < bs; j++) (*ia)[j] = (tia[1] - tia[0]) * bs + (*ia)[j - 1];
    }

    for (i = 1; i < n; i++) {
      (*ia)[i * bs] = (tia[i] - tia[i - 1]) * bs + (*ia)[i * bs - 1];
      for (j = 1; j < bs; j++) (*ia)[i * bs + j] = (tia[i + 1] - tia[i]) * bs + (*ia)[i * bs + j - 1];
    }
    if (n) (*ia)[n * bs] = (tia[n] - tia[n - 1]) * bs + (*ia)[n * bs - 1];

    if (inja) {
      PetscCall(PetscMalloc1(nz * bs * bs, ja));
      cnt = 0;
      for (i = 0; i < n; i++) {
        for (j = 0; j < bs; j++) {
          for (k = tia[i]; k < tia[i + 1]; k++) {
            for (l = 0; l < bs; l++) (*ja)[cnt++] = bs * tja[k] + l;
          }
        }
      }
    }

    if (symmetric) { /* deallocate memory allocated in MatToSymmetricIJ_SeqAIJ() */
      PetscCall(PetscFree(tia));
      PetscCall(PetscFree(tja));
    }
  } else if (oshift == 1) {
    if (symmetric) {
      nz = tia[A->rmap->n / bs];
      /*  add 1 to i and j indices */
      for (i = 0; i < A->rmap->n / bs + 1; i++) tia[i] = tia[i] + 1;
      *ia = tia;
      if (ja) {
        for (i = 0; i < nz; i++) tja[i] = tja[i] + 1;
        *ja = tja;
      }
    } else {
      nz = a->i[A->rmap->n / bs];
      /* malloc space and  add 1 to i and j indices */
      PetscCall(PetscMalloc1(A->rmap->n / bs + 1, ia));
      for (i = 0; i < A->rmap->n / bs + 1; i++) (*ia)[i] = a->i[i] + 1;
      if (ja) {
        PetscCall(PetscMalloc1(nz, ja));
        for (i = 0; i < nz; i++) (*ja)[i] = a->j[i] + 1;
      }
    }
  } else {
    *ia = tia;
    if (ja) *ja = tja;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatRestoreRowIJ_SeqSBAIJ(Mat A, PetscInt oshift, PetscBool symmetric, PetscBool blockcompressed, PetscInt *nn, const PetscInt *ia[], const PetscInt *ja[], PetscBool *done)
{
  PetscFunctionBegin;
  if (!ia) PetscFunctionReturn(PETSC_SUCCESS);
  if ((!blockcompressed && A->rmap->bs > 1) || (symmetric || oshift == 1)) {
    PetscCall(PetscFree(*ia));
    if (ja) PetscCall(PetscFree(*ja));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatDestroy_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;

  PetscFunctionBegin;
#if defined(PETSC_USE_LOG)
  PetscCall(PetscLogObjectState((PetscObject)A, "Rows=%" PetscInt_FMT ", NZ=%" PetscInt_FMT, A->rmap->N, a->nz));
#endif
  PetscCall(MatSeqXAIJFreeAIJ(A, &a->a, &a->j, &a->i));
  if (a->free_diag) PetscCall(PetscFree(a->diag));
  PetscCall(ISDestroy(&a->row));
  PetscCall(ISDestroy(&a->col));
  PetscCall(ISDestroy(&a->icol));
  PetscCall(PetscFree(a->idiag));
  PetscCall(PetscFree(a->inode.size));
  if (a->free_imax_ilen) PetscCall(PetscFree2(a->imax, a->ilen));
  PetscCall(PetscFree(a->solve_work));
  PetscCall(PetscFree(a->sor_work));
  PetscCall(PetscFree(a->solves_work));
  PetscCall(PetscFree(a->mult_work));
  PetscCall(PetscFree(a->saved_values));
  if (a->free_jshort) PetscCall(PetscFree(a->jshort));
  PetscCall(PetscFree(a->inew));
  PetscCall(MatDestroy(&a->parent));
  PetscCall(PetscFree(A->data));

  PetscCall(PetscObjectChangeTypeName((PetscObject)A, NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSeqSBAIJGetArray_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSeqSBAIJRestoreArray_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatStoreValues_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatRetrieveValues_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSeqSBAIJSetColumnIndices_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_seqsbaij_seqaij_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_seqsbaij_seqbaij_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSeqSBAIJSetPreallocation_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSeqSBAIJSetPreallocationCSR_C", NULL));
#if defined(PETSC_HAVE_ELEMENTAL)
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_seqsbaij_elemental_C", NULL));
#endif
#if defined(PETSC_HAVE_SCALAPACK)
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_seqsbaij_scalapack_C", NULL));
#endif
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatFactorGetSolverType_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSetOption_SeqSBAIJ(Mat A, MatOption op, PetscBool flg)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
#if defined(PETSC_USE_COMPLEX)
  PetscInt bs;
#endif

  PetscFunctionBegin;
#if defined(PETSC_USE_COMPLEX)
  PetscCall(MatGetBlockSize(A, &bs));
#endif
  switch (op) {
  case MAT_ROW_ORIENTED:
    a->roworiented = flg;
    break;
  case MAT_KEEP_NONZERO_PATTERN:
    a->keepnonzeropattern = flg;
    break;
  case MAT_NEW_NONZERO_LOCATIONS:
    a->nonew = (flg ? 0 : 1);
    break;
  case MAT_NEW_NONZERO_LOCATION_ERR:
    a->nonew = (flg ? -1 : 0);
    break;
  case MAT_NEW_NONZERO_ALLOCATION_ERR:
    a->nonew = (flg ? -2 : 0);
    break;
  case MAT_UNUSED_NONZERO_LOCATION_ERR:
    a->nounused = (flg ? -1 : 0);
    break;
  case MAT_FORCE_DIAGONAL_ENTRIES:
  case MAT_IGNORE_OFF_PROC_ENTRIES:
  case MAT_USE_HASH_TABLE:
  case MAT_SORTED_FULL:
    PetscCall(PetscInfo(A, "Option %s ignored\n", MatOptions[op]));
    break;
  case MAT_HERMITIAN:
#if defined(PETSC_USE_COMPLEX)
    if (flg) { /* disable transpose ops */
      PetscCheck(bs <= 1, PETSC_COMM_SELF, PETSC_ERR_SUP, "No support for Hermitian with block size greater than 1");
      A->ops->multtranspose    = NULL;
      A->ops->multtransposeadd = NULL;
      A->symmetric             = PETSC_BOOL3_FALSE;
    }
#endif
    break;
  case MAT_SYMMETRIC:
  case MAT_SPD:
#if defined(PETSC_USE_COMPLEX)
    if (flg) { /* An hermitian and symmetric matrix has zero imaginary part (restore back transpose ops) */
      A->ops->multtranspose    = A->ops->mult;
      A->ops->multtransposeadd = A->ops->multadd;
    }
#endif
    break;
    /* These options are handled directly by MatSetOption() */
  case MAT_STRUCTURALLY_SYMMETRIC:
  case MAT_SYMMETRY_ETERNAL:
  case MAT_STRUCTURAL_SYMMETRY_ETERNAL:
  case MAT_STRUCTURE_ONLY:
  case MAT_SPD_ETERNAL:
    /* These options are handled directly by MatSetOption() */
    break;
  case MAT_IGNORE_LOWER_TRIANGULAR:
    a->ignore_ltriangular = flg;
    break;
  case MAT_ERROR_LOWER_TRIANGULAR:
    a->ignore_ltriangular = flg;
    break;
  case MAT_GETROW_UPPERTRIANGULAR:
    a->getrow_utriangular = flg;
    break;
  case MAT_SUBMAT_SINGLEIS:
    break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "unknown option %d", op);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatGetRow_SeqSBAIJ(Mat A, PetscInt row, PetscInt *nz, PetscInt **idx, PetscScalar **v)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;

  PetscFunctionBegin;
  PetscCheck(!A || a->getrow_utriangular, PETSC_COMM_SELF, PETSC_ERR_SUP, "MatGetRow is not supported for SBAIJ matrix format. Getting the upper triangular part of row, run with -mat_getrow_uppertriangular, call MatSetOption(mat,MAT_GETROW_UPPERTRIANGULAR,PETSC_TRUE) or MatGetRowUpperTriangular()");

  /* Get the upper triangular part of the row */
  PetscCall(MatGetRow_SeqBAIJ_private(A, row, nz, idx, v, a->i, a->j, a->a));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatRestoreRow_SeqSBAIJ(Mat A, PetscInt row, PetscInt *nz, PetscInt **idx, PetscScalar **v)
{
  PetscFunctionBegin;
  if (nz) *nz = 0;
  if (idx) PetscCall(PetscFree(*idx));
  if (v) PetscCall(PetscFree(*v));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatGetRowUpperTriangular_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;

  PetscFunctionBegin;
  a->getrow_utriangular = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatRestoreRowUpperTriangular_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;

  PetscFunctionBegin;
  a->getrow_utriangular = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatTranspose_SeqSBAIJ(Mat A, MatReuse reuse, Mat *B)
{
  PetscFunctionBegin;
  if (reuse == MAT_REUSE_MATRIX) PetscCall(MatTransposeCheckNonzeroState_Private(A, *B));
  if (reuse == MAT_INITIAL_MATRIX) {
    PetscCall(MatDuplicate(A, MAT_COPY_VALUES, B));
  } else if (reuse == MAT_REUSE_MATRIX) {
    PetscCall(MatCopy(A, *B, SAME_NONZERO_PATTERN));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatView_SeqSBAIJ_ASCII(Mat A, PetscViewer viewer)
{
  Mat_SeqSBAIJ     *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt          i, j, bs = A->rmap->bs, k, l, bs2 = a->bs2;
  PetscViewerFormat format;
  PetscInt         *diag;
  const char       *matname;

  PetscFunctionBegin;
  PetscCall(PetscViewerGetFormat(viewer, &format));
  if (format == PETSC_VIEWER_ASCII_INFO || format == PETSC_VIEWER_ASCII_INFO_DETAIL) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  block size is %" PetscInt_FMT "\n", bs));
  } else if (format == PETSC_VIEWER_ASCII_MATLAB) {
    Mat aij;

    if (A->factortype && bs > 1) {
      PetscCall(PetscPrintf(PETSC_COMM_SELF, "Warning: matrix is factored with bs>1. MatView() with PETSC_VIEWER_ASCII_MATLAB is not supported and ignored!\n"));
      PetscFunctionReturn(PETSC_SUCCESS);
    }
    PetscCall(MatConvert(A, MATSEQAIJ, MAT_INITIAL_MATRIX, &aij));
    if (((PetscObject)A)->name) PetscCall(PetscObjectGetName((PetscObject)A, &matname));
    if (((PetscObject)A)->name) PetscCall(PetscObjectSetName((PetscObject)aij, matname));
    PetscCall(MatView_SeqAIJ(aij, viewer));
    PetscCall(MatDestroy(&aij));
  } else if (format == PETSC_VIEWER_ASCII_COMMON) {
    Mat B;

    PetscCall(MatConvert(A, MATSEQAIJ, MAT_INITIAL_MATRIX, &B));
    if (((PetscObject)A)->name) PetscCall(PetscObjectGetName((PetscObject)A, &matname));
    if (((PetscObject)A)->name) PetscCall(PetscObjectSetName((PetscObject)B, matname));
    PetscCall(MatView_SeqAIJ(B, viewer));
    PetscCall(MatDestroy(&B));
  } else if (format == PETSC_VIEWER_ASCII_FACTOR_INFO) {
    PetscFunctionReturn(PETSC_SUCCESS);
  } else {
    PetscCall(PetscViewerASCIIUseTabs(viewer, PETSC_FALSE));
    if (A->factortype) { /* for factored matrix */
      PetscCheck(bs <= 1, PETSC_COMM_SELF, PETSC_ERR_SUP, "matrix is factored with bs>1. Not implemented yet");

      diag = a->diag;
      for (i = 0; i < a->mbs; i++) { /* for row block i */
        PetscCall(PetscViewerASCIIPrintf(viewer, "row %" PetscInt_FMT ":", i));
        /* diagonal entry */
#if defined(PETSC_USE_COMPLEX)
        if (PetscImaginaryPart(a->a[diag[i]]) > 0.0) {
          PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g + %g i) ", a->j[diag[i]], (double)PetscRealPart(1.0 / a->a[diag[i]]), (double)PetscImaginaryPart(1.0 / a->a[diag[i]])));
        } else if (PetscImaginaryPart(a->a[diag[i]]) < 0.0) {
          PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g - %g i) ", a->j[diag[i]], (double)PetscRealPart(1.0 / a->a[diag[i]]), -(double)PetscImaginaryPart(1.0 / a->a[diag[i]])));
        } else {
          PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g) ", a->j[diag[i]], (double)PetscRealPart(1.0 / a->a[diag[i]])));
        }
#else
        PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g) ", a->j[diag[i]], (double)(1.0 / a->a[diag[i]])));
#endif
        /* off-diagonal entries */
        for (k = a->i[i]; k < a->i[i + 1] - 1; k++) {
#if defined(PETSC_USE_COMPLEX)
          if (PetscImaginaryPart(a->a[k]) > 0.0) {
            PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g + %g i) ", bs * a->j[k], (double)PetscRealPart(a->a[k]), (double)PetscImaginaryPart(a->a[k])));
          } else if (PetscImaginaryPart(a->a[k]) < 0.0) {
            PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g - %g i) ", bs * a->j[k], (double)PetscRealPart(a->a[k]), -(double)PetscImaginaryPart(a->a[k])));
          } else {
            PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g) ", bs * a->j[k], (double)PetscRealPart(a->a[k])));
          }
#else
          PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g) ", a->j[k], (double)a->a[k]));
#endif
        }
        PetscCall(PetscViewerASCIIPrintf(viewer, "\n"));
      }

    } else {                         /* for non-factored matrix */
      for (i = 0; i < a->mbs; i++) { /* for row block i */
        for (j = 0; j < bs; j++) {   /* for row bs*i + j */
          PetscCall(PetscViewerASCIIPrintf(viewer, "row %" PetscInt_FMT ":", i * bs + j));
          for (k = a->i[i]; k < a->i[i + 1]; k++) { /* for column block */
            for (l = 0; l < bs; l++) {              /* for column */
#if defined(PETSC_USE_COMPLEX)
              if (PetscImaginaryPart(a->a[bs2 * k + l * bs + j]) > 0.0) {
                PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g + %g i) ", bs * a->j[k] + l, (double)PetscRealPart(a->a[bs2 * k + l * bs + j]), (double)PetscImaginaryPart(a->a[bs2 * k + l * bs + j])));
              } else if (PetscImaginaryPart(a->a[bs2 * k + l * bs + j]) < 0.0) {
                PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g - %g i) ", bs * a->j[k] + l, (double)PetscRealPart(a->a[bs2 * k + l * bs + j]), -(double)PetscImaginaryPart(a->a[bs2 * k + l * bs + j])));
              } else {
                PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g) ", bs * a->j[k] + l, (double)PetscRealPart(a->a[bs2 * k + l * bs + j])));
              }
#else
              PetscCall(PetscViewerASCIIPrintf(viewer, " (%" PetscInt_FMT ", %g) ", bs * a->j[k] + l, (double)a->a[bs2 * k + l * bs + j]));
#endif
            }
          }
          PetscCall(PetscViewerASCIIPrintf(viewer, "\n"));
        }
      }
    }
    PetscCall(PetscViewerASCIIUseTabs(viewer, PETSC_TRUE));
  }
  PetscCall(PetscViewerFlush(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#include <petscdraw.h>
static PetscErrorCode MatView_SeqSBAIJ_Draw_Zoom(PetscDraw draw, void *Aa)
{
  Mat           A = (Mat)Aa;
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt      row, i, j, k, l, mbs = a->mbs, color, bs = A->rmap->bs, bs2 = a->bs2;
  PetscReal     xl, yl, xr, yr, x_l, x_r, y_l, y_r;
  MatScalar    *aa;
  PetscViewer   viewer;

  PetscFunctionBegin;
  PetscCall(PetscObjectQuery((PetscObject)A, "Zoomviewer", (PetscObject *)&viewer));
  PetscCall(PetscDrawGetCoordinates(draw, &xl, &yl, &xr, &yr));

  /* loop over matrix elements drawing boxes */

  PetscDrawCollectiveBegin(draw);
  PetscCall(PetscDrawString(draw, .3 * (xl + xr), .3 * (yl + yr), PETSC_DRAW_BLACK, "symmetric"));
  /* Blue for negative, Cyan for zero and  Red for positive */
  color = PETSC_DRAW_BLUE;
  for (i = 0, row = 0; i < mbs; i++, row += bs) {
    for (j = a->i[i]; j < a->i[i + 1]; j++) {
      y_l = A->rmap->N - row - 1.0;
      y_r = y_l + 1.0;
      x_l = a->j[j] * bs;
      x_r = x_l + 1.0;
      aa  = a->a + j * bs2;
      for (k = 0; k < bs; k++) {
        for (l = 0; l < bs; l++) {
          if (PetscRealPart(*aa++) >= 0.) continue;
          PetscCall(PetscDrawRectangle(draw, x_l + k, y_l - l, x_r + k, y_r - l, color, color, color, color));
        }
      }
    }
  }
  color = PETSC_DRAW_CYAN;
  for (i = 0, row = 0; i < mbs; i++, row += bs) {
    for (j = a->i[i]; j < a->i[i + 1]; j++) {
      y_l = A->rmap->N - row - 1.0;
      y_r = y_l + 1.0;
      x_l = a->j[j] * bs;
      x_r = x_l + 1.0;
      aa  = a->a + j * bs2;
      for (k = 0; k < bs; k++) {
        for (l = 0; l < bs; l++) {
          if (PetscRealPart(*aa++) != 0.) continue;
          PetscCall(PetscDrawRectangle(draw, x_l + k, y_l - l, x_r + k, y_r - l, color, color, color, color));
        }
      }
    }
  }
  color = PETSC_DRAW_RED;
  for (i = 0, row = 0; i < mbs; i++, row += bs) {
    for (j = a->i[i]; j < a->i[i + 1]; j++) {
      y_l = A->rmap->N - row - 1.0;
      y_r = y_l + 1.0;
      x_l = a->j[j] * bs;
      x_r = x_l + 1.0;
      aa  = a->a + j * bs2;
      for (k = 0; k < bs; k++) {
        for (l = 0; l < bs; l++) {
          if (PetscRealPart(*aa++) <= 0.) continue;
          PetscCall(PetscDrawRectangle(draw, x_l + k, y_l - l, x_r + k, y_r - l, color, color, color, color));
        }
      }
    }
  }
  PetscDrawCollectiveEnd(draw);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatView_SeqSBAIJ_Draw(Mat A, PetscViewer viewer)
{
  PetscReal xl, yl, xr, yr, w, h;
  PetscDraw draw;
  PetscBool isnull;

  PetscFunctionBegin;
  PetscCall(PetscViewerDrawGetDraw(viewer, 0, &draw));
  PetscCall(PetscDrawIsNull(draw, &isnull));
  if (isnull) PetscFunctionReturn(PETSC_SUCCESS);

  xr = A->rmap->N;
  yr = A->rmap->N;
  h  = yr / 10.0;
  w  = xr / 10.0;
  xr += w;
  yr += h;
  xl = -w;
  yl = -h;
  PetscCall(PetscDrawSetCoordinates(draw, xl, yl, xr, yr));
  PetscCall(PetscObjectCompose((PetscObject)A, "Zoomviewer", (PetscObject)viewer));
  PetscCall(PetscDrawZoom(draw, MatView_SeqSBAIJ_Draw_Zoom, A));
  PetscCall(PetscObjectCompose((PetscObject)A, "Zoomviewer", NULL));
  PetscCall(PetscDrawSave(draw));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Used for both MPIBAIJ and MPISBAIJ matrices */
#define MatView_SeqSBAIJ_Binary MatView_SeqBAIJ_Binary

PetscErrorCode MatView_SeqSBAIJ(Mat A, PetscViewer viewer)
{
  PetscBool iascii, isbinary, isdraw;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERBINARY, &isbinary));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  if (iascii) {
    PetscCall(MatView_SeqSBAIJ_ASCII(A, viewer));
  } else if (isbinary) {
    PetscCall(MatView_SeqSBAIJ_Binary(A, viewer));
  } else if (isdraw) {
    PetscCall(MatView_SeqSBAIJ_Draw(A, viewer));
  } else {
    Mat         B;
    const char *matname;
    PetscCall(MatConvert(A, MATSEQAIJ, MAT_INITIAL_MATRIX, &B));
    if (((PetscObject)A)->name) PetscCall(PetscObjectGetName((PetscObject)A, &matname));
    if (((PetscObject)A)->name) PetscCall(PetscObjectSetName((PetscObject)B, matname));
    PetscCall(MatView(B, viewer));
    PetscCall(MatDestroy(&B));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatGetValues_SeqSBAIJ(Mat A, PetscInt m, const PetscInt im[], PetscInt n, const PetscInt in[], PetscScalar v[])
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt     *rp, k, low, high, t, row, nrow, i, col, l, *aj = a->j;
  PetscInt     *ai = a->i, *ailen = a->ilen;
  PetscInt      brow, bcol, ridx, cidx, bs = A->rmap->bs, bs2 = a->bs2;
  MatScalar    *ap, *aa = a->a;

  PetscFunctionBegin;
  for (k = 0; k < m; k++) { /* loop over rows */
    row  = im[k];
    brow = row / bs;
    if (row < 0) {
      v += n;
      continue;
    } /* negative row */
    PetscCheck(row < A->rmap->N, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Row too large: row %" PetscInt_FMT " max %" PetscInt_FMT, row, A->rmap->N - 1);
    rp   = aj + ai[brow];
    ap   = aa + bs2 * ai[brow];
    nrow = ailen[brow];
    for (l = 0; l < n; l++) { /* loop over columns */
      if (in[l] < 0) {
        v++;
        continue;
      } /* negative column */
      PetscCheck(in[l] < A->cmap->n, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Column too large: col %" PetscInt_FMT " max %" PetscInt_FMT, in[l], A->cmap->n - 1);
      col  = in[l];
      bcol = col / bs;
      cidx = col % bs;
      ridx = row % bs;
      high = nrow;
      low  = 0; /* assume unsorted */
      while (high - low > 5) {
        t = (low + high) / 2;
        if (rp[t] > bcol) high = t;
        else low = t;
      }
      for (i = low; i < high; i++) {
        if (rp[i] > bcol) break;
        if (rp[i] == bcol) {
          *v++ = ap[bs2 * i + bs * cidx + ridx];
          goto finished;
        }
      }
      *v++ = 0.0;
    finished:;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPermute_SeqSBAIJ(Mat A, IS rowp, IS colp, Mat *B)
{
  Mat C;

  PetscFunctionBegin;
  PetscCall(MatConvert(A, MATSEQBAIJ, MAT_INITIAL_MATRIX, &C));
  PetscCall(MatPermute(C, rowp, colp, B));
  PetscCall(MatDestroy(&C));
  if (rowp == colp) PetscCall(MatConvert(*B, MATSEQSBAIJ, MAT_INPLACE_MATRIX, B));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSetValuesBlocked_SeqSBAIJ(Mat A, PetscInt m, const PetscInt im[], PetscInt n, const PetscInt in[], const PetscScalar v[], InsertMode is)
{
  Mat_SeqSBAIJ      *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt          *rp, k, low, high, t, ii, jj, row, nrow, i, col, l, rmax, N, lastcol = -1;
  PetscInt          *imax = a->imax, *ai = a->i, *ailen = a->ilen;
  PetscInt          *aj = a->j, nonew = a->nonew, bs2 = a->bs2, bs = A->rmap->bs, stepval;
  PetscBool          roworiented = a->roworiented;
  const PetscScalar *value       = v;
  MatScalar         *ap, *aa = a->a, *bap;

  PetscFunctionBegin;
  if (roworiented) stepval = (n - 1) * bs;
  else stepval = (m - 1) * bs;

  for (k = 0; k < m; k++) { /* loop over added rows */
    row = im[k];
    if (row < 0) continue;
    PetscCheck(row < a->mbs, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Block index row too large %" PetscInt_FMT " max %" PetscInt_FMT, row, a->mbs - 1);
    rp   = aj + ai[row];
    ap   = aa + bs2 * ai[row];
    rmax = imax[row];
    nrow = ailen[row];
    low  = 0;
    high = nrow;
    for (l = 0; l < n; l++) { /* loop over added columns */
      if (in[l] < 0) continue;
      col = in[l];
      PetscCheck(col < a->nbs, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Block index column too large %" PetscInt_FMT " max %" PetscInt_FMT, col, a->nbs - 1);
      if (col < row) {
        if (a->ignore_ltriangular) continue; /* ignore lower triangular block */
        else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_USER, "Lower triangular value cannot be set for sbaij format. Ignoring these values, run with -mat_ignore_lower_triangular or call MatSetOption(mat,MAT_IGNORE_LOWER_TRIANGULAR,PETSC_TRUE)");
      }
      if (roworiented) value = v + k * (stepval + bs) * bs + l * bs;
      else value = v + l * (stepval + bs) * bs + k * bs;

      if (col <= lastcol) low = 0;
      else high = nrow;

      lastcol = col;
      while (high - low > 7) {
        t = (low + high) / 2;
        if (rp[t] > col) high = t;
        else low = t;
      }
      for (i = low; i < high; i++) {
        if (rp[i] > col) break;
        if (rp[i] == col) {
          bap = ap + bs2 * i;
          if (roworiented) {
            if (is == ADD_VALUES) {
              for (ii = 0; ii < bs; ii++, value += stepval) {
                for (jj = ii; jj < bs2; jj += bs) bap[jj] += *value++;
              }
            } else {
              for (ii = 0; ii < bs; ii++, value += stepval) {
                for (jj = ii; jj < bs2; jj += bs) bap[jj] = *value++;
              }
            }
          } else {
            if (is == ADD_VALUES) {
              for (ii = 0; ii < bs; ii++, value += stepval) {
                for (jj = 0; jj < bs; jj++) *bap++ += *value++;
              }
            } else {
              for (ii = 0; ii < bs; ii++, value += stepval) {
                for (jj = 0; jj < bs; jj++) *bap++ = *value++;
              }
            }
          }
          goto noinsert2;
        }
      }
      if (nonew == 1) goto noinsert2;
      PetscCheck(nonew != -1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Inserting a new block index nonzero block (%" PetscInt_FMT ", %" PetscInt_FMT ") in the matrix", row, col);
      MatSeqXAIJReallocateAIJ(A, a->mbs, bs2, nrow, row, col, rmax, aa, ai, aj, rp, ap, imax, nonew, MatScalar);
      N = nrow++ - 1;
      high++;
      /* shift up all the later entries in this row */
      PetscCall(PetscArraymove(rp + i + 1, rp + i, N - i + 1));
      PetscCall(PetscArraymove(ap + bs2 * (i + 1), ap + bs2 * i, bs2 * (N - i + 1)));
      PetscCall(PetscArrayzero(ap + bs2 * i, bs2));
      rp[i] = col;
      bap   = ap + bs2 * i;
      if (roworiented) {
        for (ii = 0; ii < bs; ii++, value += stepval) {
          for (jj = ii; jj < bs2; jj += bs) bap[jj] = *value++;
        }
      } else {
        for (ii = 0; ii < bs; ii++, value += stepval) {
          for (jj = 0; jj < bs; jj++) *bap++ = *value++;
        }
      }
    noinsert2:;
      low = i;
    }
    ailen[row] = nrow;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    This is not yet used
*/
PetscErrorCode MatAssemblyEnd_SeqSBAIJ_SeqAIJ_Inode(Mat A)
{
  Mat_SeqSBAIJ   *a  = (Mat_SeqSBAIJ *)A->data;
  const PetscInt *ai = a->i, *aj = a->j, *cols;
  PetscInt        i = 0, j, blk_size, m = A->rmap->n, node_count = 0, nzx, nzy, *ns, row, nz, cnt, cnt2, *counts;
  PetscBool       flag;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(m, &ns));
  while (i < m) {
    nzx = ai[i + 1] - ai[i]; /* Number of nonzeros */
    /* Limits the number of elements in a node to 'a->inode.limit' */
    for (j = i + 1, blk_size = 1; j < m && blk_size < a->inode.limit; ++j, ++blk_size) {
      nzy = ai[j + 1] - ai[j];
      if (nzy != (nzx - j + i)) break;
      PetscCall(PetscArraycmp(aj + ai[i] + j - i, aj + ai[j], nzy, &flag));
      if (!flag) break;
    }
    ns[node_count++] = blk_size;

    i = j;
  }
  if (!a->inode.size && m && node_count > .9 * m) {
    PetscCall(PetscFree(ns));
    PetscCall(PetscInfo(A, "Found %" PetscInt_FMT " nodes out of %" PetscInt_FMT " rows. Not using Inode routines\n", node_count, m));
  } else {
    a->inode.node_count = node_count;

    PetscCall(PetscMalloc1(node_count, &a->inode.size));
    PetscCall(PetscArraycpy(a->inode.size, ns, node_count));
    PetscCall(PetscFree(ns));
    PetscCall(PetscInfo(A, "Found %" PetscInt_FMT " nodes of %" PetscInt_FMT ". Limit used: %" PetscInt_FMT ". Using Inode routines\n", node_count, m, a->inode.limit));

    /* count collections of adjacent columns in each inode */
    row = 0;
    cnt = 0;
    for (i = 0; i < node_count; i++) {
      cols = aj + ai[row] + a->inode.size[i];
      nz   = ai[row + 1] - ai[row] - a->inode.size[i];
      for (j = 1; j < nz; j++) {
        if (cols[j] != cols[j - 1] + 1) cnt++;
      }
      cnt++;
      row += a->inode.size[i];
    }
    PetscCall(PetscMalloc1(2 * cnt, &counts));
    cnt = 0;
    row = 0;
    for (i = 0; i < node_count; i++) {
      cols            = aj + ai[row] + a->inode.size[i];
      counts[2 * cnt] = cols[0];
      nz              = ai[row + 1] - ai[row] - a->inode.size[i];
      cnt2            = 1;
      for (j = 1; j < nz; j++) {
        if (cols[j] != cols[j - 1] + 1) {
          counts[2 * (cnt++) + 1] = cnt2;
          counts[2 * cnt]         = cols[j];
          cnt2                    = 1;
        } else cnt2++;
      }
      counts[2 * (cnt++) + 1] = cnt2;
      row += a->inode.size[i];
    }
    PetscCall(PetscIntView(2 * cnt, counts, NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatAssemblyEnd_SeqSBAIJ(Mat A, MatAssemblyType mode)
{
  Mat_SeqSBAIJ *a      = (Mat_SeqSBAIJ *)A->data;
  PetscInt      fshift = 0, i, *ai = a->i, *aj = a->j, *imax = a->imax;
  PetscInt      m = A->rmap->N, *ip, N, *ailen = a->ilen;
  PetscInt      mbs = a->mbs, bs2 = a->bs2, rmax = 0;
  MatScalar    *aa = a->a, *ap;

  PetscFunctionBegin;
  if (mode == MAT_FLUSH_ASSEMBLY) PetscFunctionReturn(PETSC_SUCCESS);

  if (m) rmax = ailen[0];
  for (i = 1; i < mbs; i++) {
    /* move each row back by the amount of empty slots (fshift) before it*/
    fshift += imax[i - 1] - ailen[i - 1];
    rmax = PetscMax(rmax, ailen[i]);
    if (fshift) {
      ip = aj + ai[i];
      ap = aa + bs2 * ai[i];
      N  = ailen[i];
      PetscCall(PetscArraymove(ip - fshift, ip, N));
      PetscCall(PetscArraymove(ap - bs2 * fshift, ap, bs2 * N));
    }
    ai[i] = ai[i - 1] + ailen[i - 1];
  }
  if (mbs) {
    fshift += imax[mbs - 1] - ailen[mbs - 1];
    ai[mbs] = ai[mbs - 1] + ailen[mbs - 1];
  }
  /* reset ilen and imax for each row */
  for (i = 0; i < mbs; i++) ailen[i] = imax[i] = ai[i + 1] - ai[i];
  a->nz = ai[mbs];

  /* diagonals may have moved, reset it */
  if (a->diag) PetscCall(PetscArraycpy(a->diag, ai, mbs));
  PetscCheck(!fshift || a->nounused != -1, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Unused space detected in matrix: %" PetscInt_FMT " X %" PetscInt_FMT " block size %" PetscInt_FMT ", %" PetscInt_FMT " unneeded", m, A->cmap->n, A->rmap->bs, fshift * bs2);

  PetscCall(PetscInfo(A, "Matrix size: %" PetscInt_FMT " X %" PetscInt_FMT ", block size %" PetscInt_FMT "; storage space: %" PetscInt_FMT " unneeded, %" PetscInt_FMT " used\n", m, A->rmap->N, A->rmap->bs, fshift * bs2, a->nz * bs2));
  PetscCall(PetscInfo(A, "Number of mallocs during MatSetValues is %" PetscInt_FMT "\n", a->reallocs));
  PetscCall(PetscInfo(A, "Most nonzeros blocks in any row is %" PetscInt_FMT "\n", rmax));

  A->info.mallocs += a->reallocs;
  a->reallocs         = 0;
  A->info.nz_unneeded = (PetscReal)fshift * bs2;
  a->idiagvalid       = PETSC_FALSE;
  a->rmax             = rmax;

  if (A->cmap->n < 65536 && A->cmap->bs == 1) {
    if (a->jshort && a->free_jshort) {
      /* when matrix data structure is changed, previous jshort must be replaced */
      PetscCall(PetscFree(a->jshort));
    }
    PetscCall(PetscMalloc1(a->i[A->rmap->n], &a->jshort));
    for (i = 0; i < a->i[A->rmap->n]; i++) a->jshort[i] = a->j[i];
    A->ops->mult   = MatMult_SeqSBAIJ_1_ushort;
    A->ops->sor    = MatSOR_SeqSBAIJ_ushort;
    a->free_jshort = PETSC_TRUE;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   This function returns an array of flags which indicate the locations of contiguous
   blocks that should be zeroed. for eg: if bs = 3  and is = [0,1,2,3,5,6,7,8,9]
   then the resulting sizes = [3,1,1,3,1] corresponding to sets [(0,1,2),(3),(5),(6,7,8),(9)]
   Assume: sizes should be long enough to hold all the values.
*/
PetscErrorCode MatZeroRows_SeqSBAIJ_Check_Blocks(PetscInt idx[], PetscInt n, PetscInt bs, PetscInt sizes[], PetscInt *bs_max)
{
  PetscInt  i, j, k, row;
  PetscBool flg;

  PetscFunctionBegin;
  for (i = 0, j = 0; i < n; j++) {
    row = idx[i];
    if (row % bs != 0) { /* Not the beginning of a block */
      sizes[j] = 1;
      i++;
    } else if (i + bs > n) { /* Beginning of a block, but complete block doesn't exist (at idx end) */
      sizes[j] = 1;          /* Also makes sure at least 'bs' values exist for next else */
      i++;
    } else { /* Beginning of the block, so check if the complete block exists */
      flg = PETSC_TRUE;
      for (k = 1; k < bs; k++) {
        if (row + k != idx[i + k]) { /* break in the block */
          flg = PETSC_FALSE;
          break;
        }
      }
      if (flg) { /* No break in the bs */
        sizes[j] = bs;
        i += bs;
      } else {
        sizes[j] = 1;
        i++;
      }
    }
  }
  *bs_max = j;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Only add/insert a(i,j) with i<=j (blocks).
   Any a(i,j) with i>j input by user is ignored.
*/

PetscErrorCode MatSetValues_SeqSBAIJ(Mat A, PetscInt m, const PetscInt im[], PetscInt n, const PetscInt in[], const PetscScalar v[], InsertMode is)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt     *rp, k, low, high, t, ii, row, nrow, i, col, l, rmax, N, lastcol = -1;
  PetscInt     *imax = a->imax, *ai = a->i, *ailen = a->ilen, roworiented = a->roworiented;
  PetscInt     *aj = a->j, nonew = a->nonew, bs = A->rmap->bs, brow, bcol;
  PetscInt      ridx, cidx, bs2                 = a->bs2;
  MatScalar    *ap, value, *aa                  = a->a, *bap;

  PetscFunctionBegin;
  for (k = 0; k < m; k++) { /* loop over added rows */
    row  = im[k];           /* row number */
    brow = row / bs;        /* block row number */
    if (row < 0) continue;
    PetscCheck(row < A->rmap->N, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Row too large: row %" PetscInt_FMT " max %" PetscInt_FMT, row, A->rmap->N - 1);
    rp   = aj + ai[brow];       /*ptr to beginning of column value of the row block*/
    ap   = aa + bs2 * ai[brow]; /*ptr to beginning of element value of the row block*/
    rmax = imax[brow];          /* maximum space allocated for this row */
    nrow = ailen[brow];         /* actual length of this row */
    low  = 0;
    high = nrow;
    for (l = 0; l < n; l++) { /* loop over added columns */
      if (in[l] < 0) continue;
      PetscCheck(in[l] < A->cmap->N, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Column too large: col %" PetscInt_FMT " max %" PetscInt_FMT, in[l], A->cmap->N - 1);
      col  = in[l];
      bcol = col / bs; /* block col number */

      if (brow > bcol) {
        if (a->ignore_ltriangular) continue; /* ignore lower triangular values */
        else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_USER, "Lower triangular value cannot be set for sbaij format. Ignoring these values, run with -mat_ignore_lower_triangular or call MatSetOption(mat,MAT_IGNORE_LOWER_TRIANGULAR,PETSC_TRUE)");
      }

      ridx = row % bs;
      cidx = col % bs; /*row and col index inside the block */
      if ((brow == bcol && ridx <= cidx) || (brow < bcol)) {
        /* element value a(k,l) */
        if (roworiented) value = v[l + k * n];
        else value = v[k + l * m];

        /* move pointer bap to a(k,l) quickly and add/insert value */
        if (col <= lastcol) low = 0;
        else high = nrow;

        lastcol = col;
        while (high - low > 7) {
          t = (low + high) / 2;
          if (rp[t] > bcol) high = t;
          else low = t;
        }
        for (i = low; i < high; i++) {
          if (rp[i] > bcol) break;
          if (rp[i] == bcol) {
            bap = ap + bs2 * i + bs * cidx + ridx;
            if (is == ADD_VALUES) *bap += value;
            else *bap = value;
            /* for diag block, add/insert its symmetric element a(cidx,ridx) */
            if (brow == bcol && ridx < cidx) {
              bap = ap + bs2 * i + bs * ridx + cidx;
              if (is == ADD_VALUES) *bap += value;
              else *bap = value;
            }
            goto noinsert1;
          }
        }

        if (nonew == 1) goto noinsert1;
        PetscCheck(nonew != -1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Inserting a new nonzero (%" PetscInt_FMT ", %" PetscInt_FMT ") in the matrix", row, col);
        MatSeqXAIJReallocateAIJ(A, a->mbs, bs2, nrow, brow, bcol, rmax, aa, ai, aj, rp, ap, imax, nonew, MatScalar);

        N = nrow++ - 1;
        high++;
        /* shift up all the later entries in this row */
        PetscCall(PetscArraymove(rp + i + 1, rp + i, N - i + 1));
        PetscCall(PetscArraymove(ap + bs2 * (i + 1), ap + bs2 * i, bs2 * (N - i + 1)));
        PetscCall(PetscArrayzero(ap + bs2 * i, bs2));
        rp[i]                          = bcol;
        ap[bs2 * i + bs * cidx + ridx] = value;
        /* for diag block, add/insert its symmetric element a(cidx,ridx) */
        if (brow == bcol && ridx < cidx) ap[bs2 * i + bs * ridx + cidx] = value;
        A->nonzerostate++;
      noinsert1:;
        low = i;
      }
    } /* end of loop over added columns */
    ailen[brow] = nrow;
  } /* end of loop over added rows */
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatICCFactor_SeqSBAIJ(Mat inA, IS row, const MatFactorInfo *info)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)inA->data;
  Mat           outA;
  PetscBool     row_identity;

  PetscFunctionBegin;
  PetscCheck(info->levels == 0, PETSC_COMM_SELF, PETSC_ERR_SUP, "Only levels=0 is supported for in-place icc");
  PetscCall(ISIdentity(row, &row_identity));
  PetscCheck(row_identity, PETSC_COMM_SELF, PETSC_ERR_SUP, "Matrix reordering is not supported");
  PetscCheck(inA->rmap->bs == 1, PETSC_COMM_SELF, PETSC_ERR_SUP, "Matrix block size %" PetscInt_FMT " is not supported", inA->rmap->bs); /* Need to replace MatCholeskyFactorSymbolic_SeqSBAIJ_MSR()! */

  outA            = inA;
  inA->factortype = MAT_FACTOR_ICC;
  PetscCall(PetscFree(inA->solvertype));
  PetscCall(PetscStrallocpy(MATSOLVERPETSC, &inA->solvertype));

  PetscCall(MatMarkDiagonal_SeqSBAIJ(inA));
  PetscCall(MatSeqSBAIJSetNumericFactorization_inplace(inA, row_identity));

  PetscCall(PetscObjectReference((PetscObject)row));
  PetscCall(ISDestroy(&a->row));
  a->row = row;
  PetscCall(PetscObjectReference((PetscObject)row));
  PetscCall(ISDestroy(&a->col));
  a->col = row;

  /* Create the invert permutation so that it can be used in MatCholeskyFactorNumeric() */
  if (a->icol) PetscCall(ISInvertPermutation(row, PETSC_DECIDE, &a->icol));

  if (!a->solve_work) PetscCall(PetscMalloc1(inA->rmap->N + inA->rmap->bs, &a->solve_work));

  PetscCall(MatCholeskyFactorNumeric(outA, inA, info));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSeqSBAIJSetColumnIndices_SeqSBAIJ(Mat mat, PetscInt *indices)
{
  Mat_SeqSBAIJ *baij = (Mat_SeqSBAIJ *)mat->data;
  PetscInt      i, nz, n;

  PetscFunctionBegin;
  nz = baij->maxnz;
  n  = mat->cmap->n;
  for (i = 0; i < nz; i++) baij->j[i] = indices[i];

  baij->nz = nz;
  for (i = 0; i < n; i++) baij->ilen[i] = baij->imax[i];

  PetscCall(MatSetOption(mat, MAT_NEW_NONZERO_LOCATION_ERR, PETSC_TRUE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  MatSeqSBAIJSetColumnIndices - Set the column indices for all the rows
  in a `MATSEQSBAIJ` matrix.

  Input Parameters:
+  mat     - the `MATSEQSBAIJ` matrix
-  indices - the column indices

  Level: advanced

  Notes:
  This can be called if you have precomputed the nonzero structure of the
  matrix and want to provide it to the matrix object to improve the performance
  of the `MatSetValues()` operation.

  You MUST have set the correct numbers of nonzeros per row in the call to
  `MatCreateSeqSBAIJ()`, and the columns indices MUST be sorted.

  MUST be called before any calls to `MatSetValues()`

.seealso: [](chapter_matrices), `Mat`, `MATSEQSBAIJ`, `MatCreateSeqSBAIJ`
@*/
PetscErrorCode MatSeqSBAIJSetColumnIndices(Mat mat, PetscInt *indices)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscValidIntPointer(indices, 2);
  PetscUseMethod(mat, "MatSeqSBAIJSetColumnIndices_C", (Mat, PetscInt *), (mat, indices));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatCopy_SeqSBAIJ(Mat A, Mat B, MatStructure str)
{
  PetscBool isbaij;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompareAny((PetscObject)B, &isbaij, MATSEQSBAIJ, MATMPISBAIJ, ""));
  PetscCheck(isbaij, PetscObjectComm((PetscObject)B), PETSC_ERR_SUP, "Not for matrix type %s", ((PetscObject)B)->type_name);
  /* If the two matrices have the same copy implementation and nonzero pattern, use fast copy. */
  if (str == SAME_NONZERO_PATTERN && A->ops->copy == B->ops->copy) {
    Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
    Mat_SeqSBAIJ *b = (Mat_SeqSBAIJ *)B->data;

    PetscCheck(a->i[a->mbs] == b->i[b->mbs], PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of nonzeros in two matrices are different");
    PetscCheck(a->mbs == b->mbs, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of rows in two matrices are different");
    PetscCheck(a->bs2 == b->bs2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Different block size");
    PetscCall(PetscArraycpy(b->a, a->a, a->bs2 * a->i[a->mbs]));
    PetscCall(PetscObjectStateIncrease((PetscObject)B));
  } else {
    PetscCall(MatGetRowUpperTriangular(A));
    PetscCall(MatCopy_Basic(A, B, str));
    PetscCall(MatRestoreRowUpperTriangular(A));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSeqSBAIJGetArray_SeqSBAIJ(Mat A, PetscScalar *array[])
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;

  PetscFunctionBegin;
  *array = a->a;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSeqSBAIJRestoreArray_SeqSBAIJ(Mat A, PetscScalar *array[])
{
  PetscFunctionBegin;
  *array = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatAXPYGetPreallocation_SeqSBAIJ(Mat Y, Mat X, PetscInt *nnz)
{
  PetscInt      bs = Y->rmap->bs, mbs = Y->rmap->N / bs;
  Mat_SeqSBAIJ *x = (Mat_SeqSBAIJ *)X->data;
  Mat_SeqSBAIJ *y = (Mat_SeqSBAIJ *)Y->data;

  PetscFunctionBegin;
  /* Set the number of nonzeros in the new matrix */
  PetscCall(MatAXPYGetPreallocation_SeqX_private(mbs, x->i, x->j, y->i, y->j, nnz));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatAXPY_SeqSBAIJ(Mat Y, PetscScalar a, Mat X, MatStructure str)
{
  Mat_SeqSBAIJ *x = (Mat_SeqSBAIJ *)X->data, *y = (Mat_SeqSBAIJ *)Y->data;
  PetscInt      bs = Y->rmap->bs, bs2 = bs * bs;
  PetscBLASInt  one = 1;

  PetscFunctionBegin;
  if (str == UNKNOWN_NONZERO_PATTERN || (PetscDefined(USE_DEBUG) && str == SAME_NONZERO_PATTERN)) {
    PetscBool e = x->nz == y->nz && x->mbs == y->mbs ? PETSC_TRUE : PETSC_FALSE;
    if (e) {
      PetscCall(PetscArraycmp(x->i, y->i, x->mbs + 1, &e));
      if (e) {
        PetscCall(PetscArraycmp(x->j, y->j, x->i[x->mbs], &e));
        if (e) str = SAME_NONZERO_PATTERN;
      }
    }
    if (!e) PetscCheck(str != SAME_NONZERO_PATTERN, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "MatStructure is not SAME_NONZERO_PATTERN");
  }
  if (str == SAME_NONZERO_PATTERN) {
    PetscScalar  alpha = a;
    PetscBLASInt bnz;
    PetscCall(PetscBLASIntCast(x->nz * bs2, &bnz));
    PetscCallBLAS("BLASaxpy", BLASaxpy_(&bnz, &alpha, x->a, &one, y->a, &one));
    PetscCall(PetscObjectStateIncrease((PetscObject)Y));
  } else if (str == SUBSET_NONZERO_PATTERN) { /* nonzeros of X is a subset of Y's */
    PetscCall(MatSetOption(X, MAT_GETROW_UPPERTRIANGULAR, PETSC_TRUE));
    PetscCall(MatAXPY_Basic(Y, a, X, str));
    PetscCall(MatSetOption(X, MAT_GETROW_UPPERTRIANGULAR, PETSC_FALSE));
  } else {
    Mat       B;
    PetscInt *nnz;
    PetscCheck(bs == X->rmap->bs, PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "Matrices must have same block size");
    PetscCall(MatGetRowUpperTriangular(X));
    PetscCall(MatGetRowUpperTriangular(Y));
    PetscCall(PetscMalloc1(Y->rmap->N, &nnz));
    PetscCall(MatCreate(PetscObjectComm((PetscObject)Y), &B));
    PetscCall(PetscObjectSetName((PetscObject)B, ((PetscObject)Y)->name));
    PetscCall(MatSetSizes(B, Y->rmap->n, Y->cmap->n, Y->rmap->N, Y->cmap->N));
    PetscCall(MatSetBlockSizesFromMats(B, Y, Y));
    PetscCall(MatSetType(B, ((PetscObject)Y)->type_name));
    PetscCall(MatAXPYGetPreallocation_SeqSBAIJ(Y, X, nnz));
    PetscCall(MatSeqSBAIJSetPreallocation(B, bs, 0, nnz));

    PetscCall(MatAXPY_BasicWithPreallocation(B, Y, a, X, str));

    PetscCall(MatHeaderMerge(Y, &B));
    PetscCall(PetscFree(nnz));
    PetscCall(MatRestoreRowUpperTriangular(X));
    PetscCall(MatRestoreRowUpperTriangular(Y));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatIsSymmetric_SeqSBAIJ(Mat A, PetscReal tol, PetscBool *flg)
{
  PetscFunctionBegin;
  *flg = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatIsStructurallySymmetric_SeqSBAIJ(Mat A, PetscBool *flg)
{
  PetscFunctionBegin;
  *flg = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatIsHermitian_SeqSBAIJ(Mat A, PetscReal tol, PetscBool *flg)
{
  PetscFunctionBegin;
  *flg = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatConjugate_SeqSBAIJ(Mat A)
{
#if defined(PETSC_USE_COMPLEX)
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt      i, nz = a->bs2 * a->i[a->mbs];
  MatScalar    *aa = a->a;

  PetscFunctionBegin;
  for (i = 0; i < nz; i++) aa[i] = PetscConj(aa[i]);
#else
  PetscFunctionBegin;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatRealPart_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt      i, nz = a->bs2 * a->i[a->mbs];
  MatScalar    *aa = a->a;

  PetscFunctionBegin;
  for (i = 0; i < nz; i++) aa[i] = PetscRealPart(aa[i]);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatImaginaryPart_SeqSBAIJ(Mat A)
{
  Mat_SeqSBAIJ *a = (Mat_SeqSBAIJ *)A->data;
  PetscInt      i, nz = a->bs2 * a->i[a->mbs];
  MatScalar    *aa = a->a;

  PetscFunctionBegin;
  for (i = 0; i < nz; i++) aa[i] = PetscImaginaryPart(aa[i]);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatZeroRowsColumns_SeqSBAIJ(Mat A, PetscInt is_n, const PetscInt is_idx[], PetscScalar diag, Vec x, Vec b)
{
  Mat_SeqSBAIJ      *baij = (Mat_SeqSBAIJ *)A->data;
  PetscInt           i, j, k, count;
  PetscInt           bs = A->rmap->bs, bs2 = baij->bs2, row, col;
  PetscScalar        zero = 0.0;
  MatScalar         *aa;
  const PetscScalar *xx;
  PetscScalar       *bb;
  PetscBool         *zeroed, vecs = PETSC_FALSE;

  PetscFunctionBegin;
  /* fix right hand side if needed */
  if (x && b) {
    PetscCall(VecGetArrayRead(x, &xx));
    PetscCall(VecGetArray(b, &bb));
    vecs = PETSC_TRUE;
  }

  /* zero the columns */
  PetscCall(PetscCalloc1(A->rmap->n, &zeroed));
  for (i = 0; i < is_n; i++) {
    PetscCheck(is_idx[i] >= 0 && is_idx[i] < A->rmap->N, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "row %" PetscInt_FMT " out of range", is_idx[i]);
    zeroed[is_idx[i]] = PETSC_TRUE;
  }
  if (vecs) {
    for (i = 0; i < A->rmap->N; i++) {
      row = i / bs;
      for (j = baij->i[row]; j < baij->i[row + 1]; j++) {
        for (k = 0; k < bs; k++) {
          col = bs * baij->j[j] + k;
          if (col <= i) continue;
          aa = ((MatScalar *)(baij->a)) + j * bs2 + (i % bs) + bs * k;
          if (!zeroed[i] && zeroed[col]) bb[i] -= aa[0] * xx[col];
          if (zeroed[i] && !zeroed[col]) bb[col] -= aa[0] * xx[i];
        }
      }
    }
    for (i = 0; i < is_n; i++) bb[is_idx[i]] = diag * xx[is_idx[i]];
  }

  for (i = 0; i < A->rmap->N; i++) {
    if (!zeroed[i]) {
      row = i / bs;
      for (j = baij->i[row]; j < baij->i[row + 1]; j++) {
        for (k = 0; k < bs; k++) {
          col = bs * baij->j[j] + k;
          if (zeroed[col]) {
            aa    = ((MatScalar *)(baij->a)) + j * bs2 + (i % bs) + bs * k;
            aa[0] = 0.0;
          }
        }
      }
    }
  }
  PetscCall(PetscFree(zeroed));
  if (vecs) {
    PetscCall(VecRestoreArrayRead(x, &xx));
    PetscCall(VecRestoreArray(b, &bb));
  }

  /* zero the rows */
  for (i = 0; i < is_n; i++) {
    row   = is_idx[i];
    count = (baij->i[row / bs + 1] - baij->i[row / bs]) * bs;
    aa    = ((MatScalar *)(baij->a)) + baij->i[row / bs] * bs2 + (row % bs);
    for (k = 0; k < count; k++) {
      aa[0] = zero;
      aa += bs;
    }
    if (diag != 0.0) PetscUseTypeMethod(A, setvalues, 1, &row, 1, &row, &diag, INSERT_VALUES);
  }
  PetscCall(MatAssemblyEnd_SeqSBAIJ(A, MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatShift_SeqSBAIJ(Mat Y, PetscScalar a)
{
  Mat_SeqSBAIJ *aij = (Mat_SeqSBAIJ *)Y->data;

  PetscFunctionBegin;
  if (!Y->preallocated || !aij->nz) PetscCall(MatSeqSBAIJSetPreallocation(Y, Y->rmap->bs, 1, NULL));
  PetscCall(MatShift_Basic(Y, a));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static struct _MatOps MatOps_Values = {MatSetValues_SeqSBAIJ,
                                       MatGetRow_SeqSBAIJ,
                                       MatRestoreRow_SeqSBAIJ,
                                       MatMult_SeqSBAIJ_N,
                                       /*  4*/ MatMultAdd_SeqSBAIJ_N,
                                       MatMult_SeqSBAIJ_N, /* transpose versions are same as non-transpose versions */
                                       MatMultAdd_SeqSBAIJ_N,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 10*/ NULL,
                                       NULL,
                                       MatCholeskyFactor_SeqSBAIJ,
                                       MatSOR_SeqSBAIJ,
                                       MatTranspose_SeqSBAIJ,
                                       /* 15*/ MatGetInfo_SeqSBAIJ,
                                       MatEqual_SeqSBAIJ,
                                       MatGetDiagonal_SeqSBAIJ,
                                       MatDiagonalScale_SeqSBAIJ,
                                       MatNorm_SeqSBAIJ,
                                       /* 20*/ NULL,
                                       MatAssemblyEnd_SeqSBAIJ,
                                       MatSetOption_SeqSBAIJ,
                                       MatZeroEntries_SeqSBAIJ,
                                       /* 24*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 29*/ MatSetUp_Seq_Hash,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 34*/ MatDuplicate_SeqSBAIJ,
                                       NULL,
                                       NULL,
                                       NULL,
                                       MatICCFactor_SeqSBAIJ,
                                       /* 39*/ MatAXPY_SeqSBAIJ,
                                       MatCreateSubMatrices_SeqSBAIJ,
                                       MatIncreaseOverlap_SeqSBAIJ,
                                       MatGetValues_SeqSBAIJ,
                                       MatCopy_SeqSBAIJ,
                                       /* 44*/ NULL,
                                       MatScale_SeqSBAIJ,
                                       MatShift_SeqSBAIJ,
                                       NULL,
                                       MatZeroRowsColumns_SeqSBAIJ,
                                       /* 49*/ NULL,
                                       MatGetRowIJ_SeqSBAIJ,
                                       MatRestoreRowIJ_SeqSBAIJ,
                                       NULL,
                                       NULL,
                                       /* 54*/ NULL,
                                       NULL,
                                       NULL,
                                       MatPermute_SeqSBAIJ,
                                       MatSetValuesBlocked_SeqSBAIJ,
                                       /* 59*/ MatCreateSubMatrix_SeqSBAIJ,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 64*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 69*/ MatGetRowMaxAbs_SeqSBAIJ,
                                       NULL,
                                       MatConvert_MPISBAIJ_Basic,
                                       NULL,
                                       NULL,
                                       /* 74*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 79*/ NULL,
                                       NULL,
                                       NULL,
                                       MatGetInertia_SeqSBAIJ,
                                       MatLoad_SeqSBAIJ,
                                       /* 84*/ MatIsSymmetric_SeqSBAIJ,
                                       MatIsHermitian_SeqSBAIJ,
                                       MatIsStructurallySymmetric_SeqSBAIJ,
                                       NULL,
                                       NULL,
                                       /* 89*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 94*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 99*/ NULL,
                                       NULL,
                                       NULL,
                                       MatConjugate_SeqSBAIJ,
                                       NULL,
                                       /*104*/ NULL,
                                       MatRealPart_SeqSBAIJ,
                                       MatImaginaryPart_SeqSBAIJ,
                                       MatGetRowUpperTriangular_SeqSBAIJ,
                                       MatRestoreRowUpperTriangular_SeqSBAIJ,
                                       /*109*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       MatMissingDiagonal_SeqSBAIJ,
                                       /*114*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*119*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*124*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*129*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*134*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*139*/ MatSetBlockSizes_Default,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*144*/ MatCreateMPIMatConcatenateSeqMat_SeqSBAIJ,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*150*/ NULL,
                                       NULL};

PetscErrorCode MatStoreValues_SeqSBAIJ(Mat mat)
{
  Mat_SeqSBAIJ *aij = (Mat_SeqSBAIJ *)mat->data;
  PetscInt      nz  = aij->i[mat->rmap->N] * mat->rmap->bs * aij->bs2;

  PetscFunctionBegin;
  PetscCheck(aij->nonew == 1, PETSC_COMM_SELF, PETSC_ERR_ORDER, "Must call MatSetOption(A,MAT_NEW_NONZERO_LOCATIONS,PETSC_FALSE);first");

  /* allocate space for values if not already there */
  if (!aij->saved_values) PetscCall(PetscMalloc1(nz + 1, &aij->saved_values));

  /* copy values over */
  PetscCall(PetscArraycpy(aij->saved_values, aij->a, nz));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatRetrieveValues_SeqSBAIJ(Mat mat)
{
  Mat_SeqSBAIJ *aij = (Mat_SeqSBAIJ *)mat->data;
  PetscInt      nz  = aij->i[mat->rmap->N] * mat->rmap->bs * aij->bs2;

  PetscFunctionBegin;
  PetscCheck(aij->nonew == 1, PETSC_COMM_SELF, PETSC_ERR_ORDER, "Must call MatSetOption(A,MAT_NEW_NONZERO_LOCATIONS,PETSC_FALSE);first");
  PetscCheck(aij->saved_values, PETSC_COMM_SELF, PETSC_ERR_ORDER, "Must call MatStoreValues(A);first");

  /* copy values over */
  PetscCall(PetscArraycpy(aij->a, aij->saved_values, nz));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSeqSBAIJSetPreallocation_SeqSBAIJ(Mat B, PetscInt bs, PetscInt nz, PetscInt *nnz)
{
  Mat_SeqSBAIJ *b = (Mat_SeqSBAIJ *)B->data;
  PetscInt      i, mbs, nbs, bs2;
  PetscBool     skipallocation = PETSC_FALSE, flg = PETSC_FALSE, realalloc = PETSC_FALSE;

  if (B->hash_active) {
    PetscInt bs;
    PetscCall(PetscMemcpy(&B->ops, &b->cops, sizeof(*(B->ops))));
    PetscCall(PetscHMapIJVDestroy(&b->ht));
    PetscCall(MatGetBlockSize(B, &bs));
    if (bs > 1) PetscCall(PetscHSetIJDestroy(&b->bht));
    PetscCall(PetscFree(b->dnz));
    PetscCall(PetscFree(b->bdnz));
    B->hash_active = PETSC_FALSE;
  }
  PetscFunctionBegin;
  if (nz >= 0 || nnz) realalloc = PETSC_TRUE;

  PetscCall(MatSetBlockSize(B, PetscAbs(bs)));
  PetscCall(PetscLayoutSetUp(B->rmap));
  PetscCall(PetscLayoutSetUp(B->cmap));
  PetscCheck(B->rmap->N <= B->cmap->N, PETSC_COMM_SELF, PETSC_ERR_SUP, "SEQSBAIJ matrix cannot have more rows %" PetscInt_FMT " than columns %" PetscInt_FMT, B->rmap->N, B->cmap->N);
  PetscCall(PetscLayoutGetBlockSize(B->rmap, &bs));

  B->preallocated = PETSC_TRUE;

  mbs = B->rmap->N / bs;
  nbs = B->cmap->n / bs;
  bs2 = bs * bs;

  PetscCheck(mbs * bs == B->rmap->N && nbs * bs == B->cmap->n, PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "Number rows, cols must be divisible by blocksize");

  if (nz == MAT_SKIP_ALLOCATION) {
    skipallocation = PETSC_TRUE;
    nz             = 0;
  }

  if (nz == PETSC_DEFAULT || nz == PETSC_DECIDE) nz = 3;
  PetscCheck(nz >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "nz cannot be less than 0: value %" PetscInt_FMT, nz);
  if (nnz) {
    for (i = 0; i < mbs; i++) {
      PetscCheck(nnz[i] >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "nnz cannot be less than 0: local row %" PetscInt_FMT " value %" PetscInt_FMT, i, nnz[i]);
      PetscCheck(nnz[i] <= nbs, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "nnz cannot be greater than block row length: local row %" PetscInt_FMT " value %" PetscInt_FMT " block rowlength %" PetscInt_FMT, i, nnz[i], nbs);
    }
  }

  B->ops->mult             = MatMult_SeqSBAIJ_N;
  B->ops->multadd          = MatMultAdd_SeqSBAIJ_N;
  B->ops->multtranspose    = MatMult_SeqSBAIJ_N;
  B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_N;

  PetscCall(PetscOptionsGetBool(((PetscObject)B)->options, ((PetscObject)B)->prefix, "-mat_no_unroll", &flg, NULL));
  if (!flg) {
    switch (bs) {
    case 1:
      B->ops->mult             = MatMult_SeqSBAIJ_1;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_1;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_1;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_1;
      break;
    case 2:
      B->ops->mult             = MatMult_SeqSBAIJ_2;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_2;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_2;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_2;
      break;
    case 3:
      B->ops->mult             = MatMult_SeqSBAIJ_3;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_3;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_3;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_3;
      break;
    case 4:
      B->ops->mult             = MatMult_SeqSBAIJ_4;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_4;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_4;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_4;
      break;
    case 5:
      B->ops->mult             = MatMult_SeqSBAIJ_5;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_5;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_5;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_5;
      break;
    case 6:
      B->ops->mult             = MatMult_SeqSBAIJ_6;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_6;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_6;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_6;
      break;
    case 7:
      B->ops->mult             = MatMult_SeqSBAIJ_7;
      B->ops->multadd          = MatMultAdd_SeqSBAIJ_7;
      B->ops->multtranspose    = MatMult_SeqSBAIJ_7;
      B->ops->multtransposeadd = MatMultAdd_SeqSBAIJ_7;
      break;
    }
  }

  b->mbs = mbs;
  b->nbs = nbs;
  if (!skipallocation) {
    if (!b->imax) {
      PetscCall(PetscMalloc2(mbs, &b->imax, mbs, &b->ilen));

      b->free_imax_ilen = PETSC_TRUE;
    }
    if (!nnz) {
      if (nz == PETSC_DEFAULT || nz == PETSC_DECIDE) nz = 5;
      else if (nz <= 0) nz = 1;
      nz = PetscMin(nbs, nz);
      for (i = 0; i < mbs; i++) b->imax[i] = nz;
      PetscCall(PetscIntMultError(nz, mbs, &nz));
    } else {
      PetscInt64 nz64 = 0;
      for (i = 0; i < mbs; i++) {
        b->imax[i] = nnz[i];
        nz64 += nnz[i];
      }
      PetscCall(PetscIntCast(nz64, &nz));
    }
    /* b->ilen will count nonzeros in each block row so far. */
    for (i = 0; i < mbs; i++) b->ilen[i] = 0;
    /* nz=(nz+mbs)/2; */ /* total diagonal and superdiagonal nonzero blocks */

    /* allocate the matrix space */
    PetscCall(MatSeqXAIJFreeAIJ(B, &b->a, &b->j, &b->i));
    PetscCall(PetscMalloc3(bs2 * nz, &b->a, nz, &b->j, B->rmap->N + 1, &b->i));
    PetscCall(PetscArrayzero(b->a, nz * bs2));
    PetscCall(PetscArrayzero(b->j, nz));

    b->singlemalloc = PETSC_TRUE;

    /* pointer to beginning of each row */
    b->i[0] = 0;
    for (i = 1; i < mbs + 1; i++) b->i[i] = b->i[i - 1] + b->imax[i - 1];

    b->free_a  = PETSC_TRUE;
    b->free_ij = PETSC_TRUE;
  } else {
    b->free_a  = PETSC_FALSE;
    b->free_ij = PETSC_FALSE;
  }

  b->bs2     = bs2;
  b->nz      = 0;
  b->maxnz   = nz;
  b->inew    = NULL;
  b->jnew    = NULL;
  b->anew    = NULL;
  b->a2anew  = NULL;
  b->permute = PETSC_FALSE;

  B->was_assembled = PETSC_FALSE;
  B->assembled     = PETSC_FALSE;
  if (realalloc) PetscCall(MatSetOption(B, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_TRUE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSeqSBAIJSetPreallocationCSR_SeqSBAIJ(Mat B, PetscInt bs, const PetscInt ii[], const PetscInt jj[], const PetscScalar V[])
{
  PetscInt     i, j, m, nz, anz, nz_max = 0, *nnz;
  PetscScalar *values      = NULL;
  PetscBool    roworiented = ((Mat_SeqSBAIJ *)B->data)->roworiented;

  PetscFunctionBegin;
  PetscCheck(bs >= 1, PetscObjectComm((PetscObject)B), PETSC_ERR_ARG_OUTOFRANGE, "Invalid block size specified, must be positive but it is %" PetscInt_FMT, bs);
  PetscCall(PetscLayoutSetBlockSize(B->rmap, bs));
  PetscCall(PetscLayoutSetBlockSize(B->cmap, bs));
  PetscCall(PetscLayoutSetUp(B->rmap));
  PetscCall(PetscLayoutSetUp(B->cmap));
  PetscCall(PetscLayoutGetBlockSize(B->rmap, &bs));
  m = B->rmap->n / bs;

  PetscCheck(!ii[0], PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "ii[0] must be 0 but it is %" PetscInt_FMT, ii[0]);
  PetscCall(PetscMalloc1(m + 1, &nnz));
  for (i = 0; i < m; i++) {
    nz = ii[i + 1] - ii[i];
    PetscCheck(nz >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Row %" PetscInt_FMT " has a negative number of columns %" PetscInt_FMT, i, nz);
    anz = 0;
    for (j = 0; j < nz; j++) {
      /* count only values on the diagonal or above */
      if (jj[ii[i] + j] >= i) {
        anz = nz - j;
        break;
      }
    }
    nz_max = PetscMax(nz_max, anz);
    nnz[i] = anz;
  }
  PetscCall(MatSeqSBAIJSetPreallocation(B, bs, 0, nnz));
  PetscCall(PetscFree(nnz));

  values = (PetscScalar *)V;
  if (!values) PetscCall(PetscCalloc1(bs * bs * nz_max, &values));
  for (i = 0; i < m; i++) {
    PetscInt        ncols = ii[i + 1] - ii[i];
    const PetscInt *icols = jj + ii[i];
    if (!roworiented || bs == 1) {
      const PetscScalar *svals = values + (V ? (bs * bs * ii[i]) : 0);
      PetscCall(MatSetValuesBlocked_SeqSBAIJ(B, 1, &i, ncols, icols, svals, INSERT_VALUES));
    } else {
      for (j = 0; j < ncols; j++) {
        const PetscScalar *svals = values + (V ? (bs * bs * (ii[i] + j)) : 0);
        PetscCall(MatSetValuesBlocked_SeqSBAIJ(B, 1, &i, 1, &icols[j], svals, INSERT_VALUES));
      }
    }
  }
  if (!V) PetscCall(PetscFree(values));
  PetscCall(MatAssemblyBegin(B, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(B, MAT_FINAL_ASSEMBLY));
  PetscCall(MatSetOption(B, MAT_NEW_NONZERO_LOCATION_ERR, PETSC_TRUE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   This is used to set the numeric factorization for both Cholesky and ICC symbolic factorization
*/
PetscErrorCode MatSeqSBAIJSetNumericFactorization_inplace(Mat B, PetscBool natural)
{
  PetscBool flg = PETSC_FALSE;
  PetscInt  bs  = B->rmap->bs;

  PetscFunctionBegin;
  PetscCall(PetscOptionsGetBool(((PetscObject)B)->options, ((PetscObject)B)->prefix, "-mat_no_unroll", &flg, NULL));
  if (flg) bs = 8;

  if (!natural) {
    switch (bs) {
    case 1:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_1_inplace;
      break;
    case 2:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_2;
      break;
    case 3:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_3;
      break;
    case 4:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_4;
      break;
    case 5:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_5;
      break;
    case 6:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_6;
      break;
    case 7:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_7;
      break;
    default:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_N;
      break;
    }
  } else {
    switch (bs) {
    case 1:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_1_NaturalOrdering_inplace;
      break;
    case 2:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_2_NaturalOrdering;
      break;
    case 3:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_3_NaturalOrdering;
      break;
    case 4:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_4_NaturalOrdering;
      break;
    case 5:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_5_NaturalOrdering;
      break;
    case 6:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_6_NaturalOrdering;
      break;
    case 7:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_7_NaturalOrdering;
      break;
    default:
      B->ops->choleskyfactornumeric = MatCholeskyFactorNumeric_SeqSBAIJ_N_NaturalOrdering;
      break;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode MatConvert_SeqSBAIJ_SeqAIJ(Mat, MatType, MatReuse, Mat *);
PETSC_INTERN PetscErrorCode MatConvert_SeqSBAIJ_SeqBAIJ(Mat, MatType, MatReuse, Mat *);
static PetscErrorCode       MatFactorGetSolverType_petsc(Mat A, MatSolverType *type)
{
  PetscFunctionBegin;
  *type = MATSOLVERPETSC;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode MatGetFactor_seqsbaij_petsc(Mat A, MatFactorType ftype, Mat *B)
{
  PetscInt n = A->rmap->n;

  PetscFunctionBegin;
#if defined(PETSC_USE_COMPLEX)
  PetscCheck(A->hermitian != PETSC_BOOL3_TRUE || A->symmetric == PETSC_BOOL3_TRUE || (ftype != MAT_FACTOR_CHOLESKY && ftype != MAT_FACTOR_ICC), PETSC_COMM_SELF, PETSC_ERR_SUP, "Hermitian CHOLESKY or ICC Factor is not supported");
#endif

  PetscCall(MatCreate(PetscObjectComm((PetscObject)A), B));
  PetscCall(MatSetSizes(*B, n, n, n, n));
  if (ftype == MAT_FACTOR_CHOLESKY || ftype == MAT_FACTOR_ICC) {
    PetscCall(MatSetType(*B, MATSEQSBAIJ));
    PetscCall(MatSeqSBAIJSetPreallocation(*B, A->rmap->bs, MAT_SKIP_ALLOCATION, NULL));

    (*B)->ops->choleskyfactorsymbolic = MatCholeskyFactorSymbolic_SeqSBAIJ;
    (*B)->ops->iccfactorsymbolic      = MatICCFactorSymbolic_SeqSBAIJ;
    PetscCall(PetscStrallocpy(MATORDERINGNATURAL, (char **)&(*B)->preferredordering[MAT_FACTOR_CHOLESKY]));
    PetscCall(PetscStrallocpy(MATORDERINGNATURAL, (char **)&(*B)->preferredordering[MAT_FACTOR_ICC]));
  } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Factor type not supported");

  (*B)->factortype     = ftype;
  (*B)->canuseordering = PETSC_TRUE;
  PetscCall(PetscFree((*B)->solvertype));
  PetscCall(PetscStrallocpy(MATSOLVERPETSC, &(*B)->solvertype));
  PetscCall(PetscObjectComposeFunction((PetscObject)*B, "MatFactorGetSolverType_C", MatFactorGetSolverType_petsc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatSeqSBAIJGetArray - gives access to the array where the numerical data for a `MATSEQSBAIJ` matrix is stored

   Not Collective

   Input Parameter:
.  mat - a `MATSEQSBAIJ` matrix

   Output Parameter:
.   array - pointer to the data

   Level: intermediate

.seealso: [](chapter_matrices), `Mat`, `MATSEQSBAIJ`, `MatSeqSBAIJRestoreArray()`, `MatSeqAIJGetArray()`, `MatSeqAIJRestoreArray()`
@*/
PetscErrorCode MatSeqSBAIJGetArray(Mat A, PetscScalar **array)
{
  PetscFunctionBegin;
  PetscUseMethod(A, "MatSeqSBAIJGetArray_C", (Mat, PetscScalar **), (A, array));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatSeqSBAIJRestoreArray - returns access to the array where the numerical data for a `MATSEQSBAIJ` matrix is stored obtained by `MatSeqSBAIJGetArray()`

   Not Collective

   Input Parameters:
+  mat - a `MATSEQSBAIJ` matrix
-  array - pointer to the data

   Level: intermediate

.seealso: [](chapter_matrices), `Mat`, `MATSEQSBAIJ`, `MatSeqSBAIJGetArray()`, `MatSeqAIJGetArray()`, `MatSeqAIJRestoreArray()`
@*/
PetscErrorCode MatSeqSBAIJRestoreArray(Mat A, PetscScalar **array)
{
  PetscFunctionBegin;
  PetscUseMethod(A, "MatSeqSBAIJRestoreArray_C", (Mat, PetscScalar **), (A, array));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
  MATSEQSBAIJ - MATSEQSBAIJ = "seqsbaij" - A matrix type to be used for sequential symmetric block sparse matrices,
  based on block compressed sparse row format.  Only the upper triangular portion of the matrix is stored.

  For complex numbers by default this matrix is symmetric, NOT Hermitian symmetric. To make it Hermitian symmetric you
  can call `MatSetOption`(`Mat`, `MAT_HERMITIAN`).

  Options Database Key:
  . -mat_type seqsbaij - sets the matrix type to "seqsbaij" during a call to `MatSetFromOptions()`

  Level: beginner

  Notes:
    By default if you insert values into the lower triangular part of the matrix they are simply ignored (since they are not
     stored and it is assumed they symmetric to the upper triangular). If you call `MatSetOption`(`Mat`,`MAT_IGNORE_LOWER_TRIANGULAR`,`PETSC_FALSE`) or use
     the options database `-mat_ignore_lower_triangular` false it will generate an error if you try to set a value in the lower triangular portion.

    The number of rows in the matrix must be less than or equal to the number of columns

.seealso: [](chapter_matrices), `Mat`, `MATSEQSBAIJ`, `MatCreateSeqSBAIJ()`, `MatType`, `MATMPISBAIJ`
M*/
PETSC_EXTERN PetscErrorCode MatCreate_SeqSBAIJ(Mat B)
{
  Mat_SeqSBAIJ *b;
  PetscMPIInt   size;
  PetscBool     no_unroll = PETSC_FALSE, no_inode = PETSC_FALSE;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)B), &size));
  PetscCheck(size <= 1, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Comm must be of size 1");

  PetscCall(PetscNew(&b));
  B->data = (void *)b;
  PetscCall(PetscMemcpy(B->ops, &MatOps_Values, sizeof(struct _MatOps)));

  B->ops->destroy    = MatDestroy_SeqSBAIJ;
  B->ops->view       = MatView_SeqSBAIJ;
  b->row             = NULL;
  b->icol            = NULL;
  b->reallocs        = 0;
  b->saved_values    = NULL;
  b->inode.limit     = 5;
  b->inode.max_limit = 5;

  b->roworiented        = PETSC_TRUE;
  b->nonew              = 0;
  b->diag               = NULL;
  b->solve_work         = NULL;
  b->mult_work          = NULL;
  B->spptr              = NULL;
  B->info.nz_unneeded   = (PetscReal)b->maxnz * b->bs2;
  b->keepnonzeropattern = PETSC_FALSE;

  b->inew    = NULL;
  b->jnew    = NULL;
  b->anew    = NULL;
  b->a2anew  = NULL;
  b->permute = PETSC_FALSE;

  b->ignore_ltriangular = PETSC_TRUE;

  PetscCall(PetscOptionsGetBool(((PetscObject)B)->options, ((PetscObject)B)->prefix, "-mat_ignore_lower_triangular", &b->ignore_ltriangular, NULL));

  b->getrow_utriangular = PETSC_FALSE;

  PetscCall(PetscOptionsGetBool(((PetscObject)B)->options, ((PetscObject)B)->prefix, "-mat_getrow_uppertriangular", &b->getrow_utriangular, NULL));

  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatSeqSBAIJGetArray_C", MatSeqSBAIJGetArray_SeqSBAIJ));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatSeqSBAIJRestoreArray_C", MatSeqSBAIJRestoreArray_SeqSBAIJ));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatStoreValues_C", MatStoreValues_SeqSBAIJ));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatRetrieveValues_C", MatRetrieveValues_SeqSBAIJ));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatSeqSBAIJSetColumnIndices_C", MatSeqSBAIJSetColumnIndices_SeqSBAIJ));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatConvert_seqsbaij_seqaij_C", MatConvert_SeqSBAIJ_SeqAIJ));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatConvert_seqsbaij_seqbaij_C", MatConvert_SeqSBAIJ_SeqBAIJ));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatSeqSBAIJSetPreallocation_C", MatSeqSBAIJSetPreallocation_SeqSBAIJ));
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatSeqSBAIJSetPreallocationCSR_C", MatSeqSBAIJSetPreallocationCSR_SeqSBAIJ));
#if defined(PETSC_HAVE_ELEMENTAL)
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatConvert_seqsbaij_elemental_C", MatConvert_SeqSBAIJ_Elemental));
#endif
#if defined(PETSC_HAVE_SCALAPACK)
  PetscCall(PetscObjectComposeFunction((PetscObject)B, "MatConvert_seqsbaij_scalapack_C", MatConvert_SBAIJ_ScaLAPACK));
#endif

  B->symmetry_eternal            = PETSC_TRUE;
  B->structural_symmetry_eternal = PETSC_TRUE;
  B->symmetric                   = PETSC_BOOL3_TRUE;
  B->structurally_symmetric      = PETSC_BOOL3_TRUE;
#if defined(PETSC_USE_COMPLEX)
  B->hermitian = PETSC_BOOL3_FALSE;
#else
  B->hermitian = PETSC_BOOL3_TRUE;
#endif

  PetscCall(PetscObjectChangeTypeName((PetscObject)B, MATSEQSBAIJ));

  PetscOptionsBegin(PetscObjectComm((PetscObject)B), ((PetscObject)B)->prefix, "Options for SEQSBAIJ matrix", "Mat");
  PetscCall(PetscOptionsBool("-mat_no_unroll", "Do not optimize for inodes (slower)", NULL, no_unroll, &no_unroll, NULL));
  if (no_unroll) PetscCall(PetscInfo(B, "Not using Inode routines due to -mat_no_unroll\n"));
  PetscCall(PetscOptionsBool("-mat_no_inode", "Do not optimize for inodes (slower)", NULL, no_inode, &no_inode, NULL));
  if (no_inode) PetscCall(PetscInfo(B, "Not using Inode routines due to -mat_no_inode\n"));
  PetscCall(PetscOptionsInt("-mat_inode_limit", "Do not use inodes larger then this value", NULL, b->inode.limit, &b->inode.limit, NULL));
  PetscOptionsEnd();
  b->inode.use = (PetscBool)(!(no_unroll || no_inode));
  if (b->inode.limit > b->inode.max_limit) b->inode.limit = b->inode.max_limit;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatSeqSBAIJSetPreallocation - Creates a sparse symmetric matrix in block AIJ (block
   compressed row) `MATSEQSBAIJ` format.  For good matrix assembly performance the
   user should preallocate the matrix storage by setting the parameter `nz`
   (or the array `nnz`).

   Collective

   Input Parameters:
+  B - the symmetric matrix
.  bs - size of block, the blocks are ALWAYS square. One can use `MatSetBlockSizes()` to set a different row and column blocksize but the row
          blocksize always defines the size of the blocks. The column blocksize sets the blocksize of the vectors obtained with `MatCreateVecs()`
.  nz - number of block nonzeros per block row (same for all rows)
-  nnz - array containing the number of block nonzeros in the upper triangular plus
         diagonal portion of each block (possibly different for each block row) or `NULL`

   Options Database Keys:
+   -mat_no_unroll - uses code that does not unroll the loops in the
                     block calculations (much slower)
-   -mat_block_size - size of the blocks to use (only works if a negative bs is passed in

   Level: intermediate

   Notes:
   Specify the preallocated storage with either `nz` or `nnz` (not both).
   Set `nz` = `PETSC_DEFAULT` and `nnz` = `NULL` for PETSc to control dynamic memory
   allocation.  See [Sparse Matrices](sec_matsparse) for details.

   You can call `MatGetInfo()` to get information on how effective the preallocation was;
   for example the fields mallocs,nz_allocated,nz_used,nz_unneeded;
   You can also run with the option `-info` and look for messages with the string
   malloc in them to see if additional memory allocation was needed.

   If the `nnz` parameter is given then the `nz` parameter is ignored

.seealso: [](chapter_matrices), `Mat`, [Sparse Matrices](sec_matsparse), `MATSEQSBAIJ`, `MatCreate()`, `MatCreateSeqAIJ()`, `MatSetValues()`, `MatCreateSBAIJ()`
@*/
PetscErrorCode MatSeqSBAIJSetPreallocation(Mat B, PetscInt bs, PetscInt nz, const PetscInt nnz[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(B, MAT_CLASSID, 1);
  PetscValidType(B, 1);
  PetscValidLogicalCollectiveInt(B, bs, 2);
  PetscTryMethod(B, "MatSeqSBAIJSetPreallocation_C", (Mat, PetscInt, PetscInt, const PetscInt[]), (B, bs, nz, nnz));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatSeqSBAIJSetPreallocationCSR - Creates a sparse parallel matrix in `MATSEQSBAIJ` format using the given nonzero structure and (optional) numerical values

   Input Parameters:
+  B - the matrix
.  bs - size of block, the blocks are ALWAYS square.
.  i - the indices into j for the start of each local row (starts with zero)
.  j - the column indices for each local row (starts with zero) these must be sorted for each row
-  v - optional values in the matrix

   Level: advanced

   Notes:
   The order of the entries in values is specified by the `MatOption` `MAT_ROW_ORIENTED`.  For example, C programs
   may want to use the default `MAT_ROW_ORIENTED` = `PETSC_TRUE` and use an array v[nnz][bs][bs] where the second index is
   over rows within a block and the last index is over columns within a block row.  Fortran programs will likely set
   `MAT_ROW_ORIENTED` = `PETSC_FALSE` and use a Fortran array v(bs,bs,nnz) in which the first index is over rows within a
   block column and the second index is over columns within a block.

   Any entries below the diagonal are ignored

   Though this routine has Preallocation() in the name it also sets the exact nonzero locations of the matrix entries
   and usually the numerical values as well

.seealso: [](chapter_matrices), `Mat`, `MATSEQSBAIJ`, `MatCreate()`, `MatCreateSeqSBAIJ()`, `MatSetValuesBlocked()`, `MatSeqSBAIJSetPreallocation()`, `MATSEQSBAIJ`
@*/
PetscErrorCode MatSeqSBAIJSetPreallocationCSR(Mat B, PetscInt bs, const PetscInt i[], const PetscInt j[], const PetscScalar v[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(B, MAT_CLASSID, 1);
  PetscValidType(B, 1);
  PetscValidLogicalCollectiveInt(B, bs, 2);
  PetscTryMethod(B, "MatSeqSBAIJSetPreallocationCSR_C", (Mat, PetscInt, const PetscInt[], const PetscInt[], const PetscScalar[]), (B, bs, i, j, v));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatCreateSeqSBAIJ - Creates a sparse symmetric matrix in (block
   compressed row) `MATSEQSBAIJ` format.  For good matrix assembly performance the
   user should preallocate the matrix storage by setting the parameter `nz`
   (or the array `nnz`).

   Collective

   Input Parameters:
+  comm - MPI communicator, set to `PETSC_COMM_SELF`
.  bs - size of block, the blocks are ALWAYS square. One can use `MatSetBlockSizes()` to set a different row and column blocksize but the row
          blocksize always defines the size of the blocks. The column blocksize sets the blocksize of the vectors obtained with MatCreateVecs()
.  m - number of rows
.  n - number of columns
.  nz - number of block nonzeros per block row (same for all rows)
-  nnz - array containing the number of block nonzeros in the upper triangular plus
         diagonal portion of each block (possibly different for each block row) or `NULL`

   Output Parameter:
.  A - the symmetric matrix

   Options Database Keys:
+   -mat_no_unroll - uses code that does not unroll the loops in the
                     block calculations (much slower)
-   -mat_block_size - size of the blocks to use

   Level: intermediate

   Notes:
   It is recommended that one use the `MatCreate()`, `MatSetType()` and/or `MatSetFromOptions()`,
   MatXXXXSetPreallocation() paradigm instead of this routine directly.
   [MatXXXXSetPreallocation() is, for example, `MatSeqAIJSetPreallocation()`]

   The number of rows and columns must be divisible by blocksize.
   This matrix type does not support complex Hermitian operation.

   Specify the preallocated storage with either `nz` or `nnz` (not both).
   Set `nz` = `PETSC_DEFAULT` and `nnz` = `NULL` for PETSc to control dynamic memory
   allocation.  See [Sparse Matrices](sec_matsparse) for details.

   If the `nnz` parameter is given then the `nz` parameter is ignored

.seealso: [](chapter_matrices), `Mat`, [Sparse Matrices](sec_matsparse), `MATSEQSBAIJ`, `MatCreate()`, `MatCreateSeqAIJ()`, `MatSetValues()`, `MatCreateSBAIJ()`
@*/
PetscErrorCode MatCreateSeqSBAIJ(MPI_Comm comm, PetscInt bs, PetscInt m, PetscInt n, PetscInt nz, const PetscInt nnz[], Mat *A)
{
  PetscFunctionBegin;
  PetscCall(MatCreate(comm, A));
  PetscCall(MatSetSizes(*A, m, n, m, n));
  PetscCall(MatSetType(*A, MATSEQSBAIJ));
  PetscCall(MatSeqSBAIJSetPreallocation(*A, bs, nz, (PetscInt *)nnz));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatDuplicate_SeqSBAIJ(Mat A, MatDuplicateOption cpvalues, Mat *B)
{
  Mat           C;
  Mat_SeqSBAIJ *c, *a  = (Mat_SeqSBAIJ *)A->data;
  PetscInt      i, mbs = a->mbs, nz = a->nz, bs2 = a->bs2;

  PetscFunctionBegin;
  PetscCheck(A->assembled, PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_WRONGSTATE, "Cannot duplicate unassembled matrix");
  PetscCheck(a->i[mbs] == nz, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Corrupt matrix");

  *B = NULL;
  PetscCall(MatCreate(PetscObjectComm((PetscObject)A), &C));
  PetscCall(MatSetSizes(C, A->rmap->N, A->cmap->n, A->rmap->N, A->cmap->n));
  PetscCall(MatSetBlockSizesFromMats(C, A, A));
  PetscCall(MatSetType(C, MATSEQSBAIJ));
  c = (Mat_SeqSBAIJ *)C->data;

  C->preallocated       = PETSC_TRUE;
  C->factortype         = A->factortype;
  c->row                = NULL;
  c->icol               = NULL;
  c->saved_values       = NULL;
  c->keepnonzeropattern = a->keepnonzeropattern;
  C->assembled          = PETSC_TRUE;

  PetscCall(PetscLayoutReference(A->rmap, &C->rmap));
  PetscCall(PetscLayoutReference(A->cmap, &C->cmap));
  c->bs2 = a->bs2;
  c->mbs = a->mbs;
  c->nbs = a->nbs;

  if (cpvalues == MAT_SHARE_NONZERO_PATTERN) {
    c->imax           = a->imax;
    c->ilen           = a->ilen;
    c->free_imax_ilen = PETSC_FALSE;
  } else {
    PetscCall(PetscMalloc2((mbs + 1), &c->imax, (mbs + 1), &c->ilen));
    for (i = 0; i < mbs; i++) {
      c->imax[i] = a->imax[i];
      c->ilen[i] = a->ilen[i];
    }
    c->free_imax_ilen = PETSC_TRUE;
  }

  /* allocate the matrix space */
  if (cpvalues == MAT_SHARE_NONZERO_PATTERN) {
    PetscCall(PetscMalloc1(bs2 * nz, &c->a));
    c->i            = a->i;
    c->j            = a->j;
    c->singlemalloc = PETSC_FALSE;
    c->free_a       = PETSC_TRUE;
    c->free_ij      = PETSC_FALSE;
    c->parent       = A;
    PetscCall(PetscObjectReference((PetscObject)A));
    PetscCall(MatSetOption(A, MAT_NEW_NONZERO_LOCATION_ERR, PETSC_TRUE));
    PetscCall(MatSetOption(C, MAT_NEW_NONZERO_LOCATION_ERR, PETSC_TRUE));
  } else {
    PetscCall(PetscMalloc3(bs2 * nz, &c->a, nz, &c->j, mbs + 1, &c->i));
    PetscCall(PetscArraycpy(c->i, a->i, mbs + 1));
    c->singlemalloc = PETSC_TRUE;
    c->free_a       = PETSC_TRUE;
    c->free_ij      = PETSC_TRUE;
  }
  if (mbs > 0) {
    if (cpvalues != MAT_SHARE_NONZERO_PATTERN) PetscCall(PetscArraycpy(c->j, a->j, nz));
    if (cpvalues == MAT_COPY_VALUES) {
      PetscCall(PetscArraycpy(c->a, a->a, bs2 * nz));
    } else {
      PetscCall(PetscArrayzero(c->a, bs2 * nz));
    }
    if (a->jshort) {
      /* cannot share jshort, it is reallocated in MatAssemblyEnd_SeqSBAIJ() */
      /* if the parent matrix is reassembled, this child matrix will never notice */
      PetscCall(PetscMalloc1(nz, &c->jshort));
      PetscCall(PetscArraycpy(c->jshort, a->jshort, nz));

      c->free_jshort = PETSC_TRUE;
    }
  }

  c->roworiented = a->roworiented;
  c->nonew       = a->nonew;

  if (a->diag) {
    if (cpvalues == MAT_SHARE_NONZERO_PATTERN) {
      c->diag      = a->diag;
      c->free_diag = PETSC_FALSE;
    } else {
      PetscCall(PetscMalloc1(mbs, &c->diag));
      for (i = 0; i < mbs; i++) c->diag[i] = a->diag[i];
      c->free_diag = PETSC_TRUE;
    }
  }
  c->nz         = a->nz;
  c->maxnz      = a->nz; /* Since we allocate exactly the right amount */
  c->solve_work = NULL;
  c->mult_work  = NULL;

  *B = C;
  PetscCall(PetscFunctionListDuplicate(((PetscObject)A)->qlist, &((PetscObject)C)->qlist));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Used for both SeqBAIJ and SeqSBAIJ matrices */
#define MatLoad_SeqSBAIJ_Binary MatLoad_SeqBAIJ_Binary

PetscErrorCode MatLoad_SeqSBAIJ(Mat mat, PetscViewer viewer)
{
  PetscBool isbinary;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERBINARY, &isbinary));
  PetscCheck(isbinary, PetscObjectComm((PetscObject)viewer), PETSC_ERR_SUP, "Viewer type %s not yet supported for reading %s matrices", ((PetscObject)viewer)->type_name, ((PetscObject)mat)->type_name);
  PetscCall(MatLoad_SeqSBAIJ_Binary(mat, viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     MatCreateSeqSBAIJWithArrays - Creates an sequential `MATSEQSBAIJ` matrix using matrix elements
              (upper triangular entries in CSR format) provided by the user.

     Collective

   Input Parameters:
+  comm - must be an MPI communicator of size 1
.  bs - size of block
.  m - number of rows
.  n - number of columns
.  i - row indices; that is i[0] = 0, i[row] = i[row-1] + number of block elements in that row block row of the matrix
.  j - column indices
-  a - matrix values

   Output Parameter:
.  mat - the matrix

   Level: advanced

   Notes:
       The `i`, `j`, and `a` arrays are not copied by this routine, the user must free these arrays
    once the matrix is destroyed

       You cannot set new nonzero locations into this matrix, that will generate an error.

       The `i` and `j` indices are 0 based

       When block size is greater than 1 the matrix values must be stored using the `MATSBAIJ` storage format. For block size of 1
       it is the regular CSR format excluding the lower triangular elements.

.seealso: [](chapter_matrices), `Mat`, `MATSEQSBAIJ`, `MatCreate()`, `MatCreateSBAIJ()`, `MatCreateSeqSBAIJ()`
@*/
PetscErrorCode MatCreateSeqSBAIJWithArrays(MPI_Comm comm, PetscInt bs, PetscInt m, PetscInt n, PetscInt i[], PetscInt j[], PetscScalar a[], Mat *mat)
{
  PetscInt      ii;
  Mat_SeqSBAIJ *sbaij;

  PetscFunctionBegin;
  PetscCheck(bs == 1, PETSC_COMM_SELF, PETSC_ERR_SUP, "block size %" PetscInt_FMT " > 1 is not supported yet", bs);
  PetscCheck(m == 0 || i[0] == 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "i (row indices) must start with 0");

  PetscCall(MatCreate(comm, mat));
  PetscCall(MatSetSizes(*mat, m, n, m, n));
  PetscCall(MatSetType(*mat, MATSEQSBAIJ));
  PetscCall(MatSeqSBAIJSetPreallocation(*mat, bs, MAT_SKIP_ALLOCATION, NULL));
  sbaij = (Mat_SeqSBAIJ *)(*mat)->data;
  PetscCall(PetscMalloc2(m, &sbaij->imax, m, &sbaij->ilen));

  sbaij->i = i;
  sbaij->j = j;
  sbaij->a = a;

  sbaij->singlemalloc   = PETSC_FALSE;
  sbaij->nonew          = -1; /*this indicates that inserting a new value in the matrix that generates a new nonzero is an error*/
  sbaij->free_a         = PETSC_FALSE;
  sbaij->free_ij        = PETSC_FALSE;
  sbaij->free_imax_ilen = PETSC_TRUE;

  for (ii = 0; ii < m; ii++) {
    sbaij->ilen[ii] = sbaij->imax[ii] = i[ii + 1] - i[ii];
    PetscCheck(i[ii + 1] >= i[ii], PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Negative row length in i (row indices) row = %" PetscInt_FMT " length = %" PetscInt_FMT, ii, i[ii + 1] - i[ii]);
  }
  if (PetscDefined(USE_DEBUG)) {
    for (ii = 0; ii < sbaij->i[m]; ii++) {
      PetscCheck(j[ii] >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Negative column index at location = %" PetscInt_FMT " index = %" PetscInt_FMT, ii, j[ii]);
      PetscCheck(j[ii] < n, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Column index too large at location = %" PetscInt_FMT " index = %" PetscInt_FMT, ii, j[ii]);
    }
  }

  PetscCall(MatAssemblyBegin(*mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(*mat, MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatCreateMPIMatConcatenateSeqMat_SeqSBAIJ(MPI_Comm comm, Mat inmat, PetscInt n, MatReuse scall, Mat *outmat)
{
  PetscFunctionBegin;
  PetscCall(MatCreateMPIMatConcatenateSeqMat_MPISBAIJ(comm, inmat, n, scall, outmat));
  PetscFunctionReturn(PETSC_SUCCESS);
}


#include <petsc/private/matimpl.h> /*I "petscmat.h" I*/

typedef struct {
  Mat       Top;
  PetscBool rowisblock;
  PetscBool colisblock;
  PetscErrorCode (*SetValues)(Mat, PetscInt, const PetscInt[], PetscInt, const PetscInt[], const PetscScalar[], InsertMode);
  PetscErrorCode (*SetValuesBlocked)(Mat, PetscInt, const PetscInt[], PetscInt, const PetscInt[], const PetscScalar[], InsertMode);
} Mat_LocalRef;

/* These need to be macros because they use sizeof */
#define IndexSpaceGet(buf, nrow, ncol, irowm, icolm) \
  do { \
    if (nrow + ncol > (PetscInt)PETSC_STATIC_ARRAY_LENGTH(buf)) { \
      PetscCall(PetscMalloc2(nrow, &irowm, ncol, &icolm)); \
    } else { \
      irowm = &buf[0]; \
      icolm = &buf[nrow]; \
    } \
  } while (0)

#define IndexSpaceRestore(buf, nrow, ncol, irowm, icolm) \
  do { \
    if (nrow + ncol > (PetscInt)PETSC_STATIC_ARRAY_LENGTH(buf)) PetscCall(PetscFree2(irowm, icolm)); \
  } while (0)

static void BlockIndicesExpand(PetscInt n, const PetscInt idx[], PetscInt bs, PetscInt idxm[])
{
  PetscInt i, j;
  for (i = 0; i < n; i++) {
    for (j = 0; j < bs; j++) idxm[i * bs + j] = idx[i] * bs + j;
  }
}

static PetscErrorCode MatSetValuesBlockedLocal_LocalRef_Block(Mat A, PetscInt nrow, const PetscInt irow[], PetscInt ncol, const PetscInt icol[], const PetscScalar y[], InsertMode addv)
{
  Mat_LocalRef *lr = (Mat_LocalRef *)A->data;
  PetscInt      buf[4096], *irowm = NULL, *icolm; /* suppress maybe-uninitialized warning */

  PetscFunctionBegin;
  if (!nrow || !ncol) PetscFunctionReturn(PETSC_SUCCESS);
  IndexSpaceGet(buf, nrow, ncol, irowm, icolm);
  PetscCall(ISLocalToGlobalMappingApplyBlock(A->rmap->mapping, nrow, irow, irowm));
  PetscCall(ISLocalToGlobalMappingApplyBlock(A->cmap->mapping, ncol, icol, icolm));
  PetscCall((*lr->SetValuesBlocked)(lr->Top, nrow, irowm, ncol, icolm, y, addv));
  IndexSpaceRestore(buf, nrow, ncol, irowm, icolm);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSetValuesBlockedLocal_LocalRef_Scalar(Mat A, PetscInt nrow, const PetscInt irow[], PetscInt ncol, const PetscInt icol[], const PetscScalar y[], InsertMode addv)
{
  Mat_LocalRef *lr = (Mat_LocalRef *)A->data;
  PetscInt      rbs, cbs, buf[4096], *irowm, *icolm;

  PetscFunctionBegin;
  PetscCall(MatGetBlockSizes(A, &rbs, &cbs));
  IndexSpaceGet(buf, nrow * rbs, ncol * cbs, irowm, icolm);
  BlockIndicesExpand(nrow, irow, rbs, irowm);
  BlockIndicesExpand(ncol, icol, cbs, icolm);
  PetscCall(ISLocalToGlobalMappingApplyBlock(A->rmap->mapping, nrow * rbs, irowm, irowm));
  PetscCall(ISLocalToGlobalMappingApplyBlock(A->cmap->mapping, ncol * cbs, icolm, icolm));
  PetscCall((*lr->SetValues)(lr->Top, nrow * rbs, irowm, ncol * cbs, icolm, y, addv));
  IndexSpaceRestore(buf, nrow * rbs, ncol * cbs, irowm, icolm);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSetValuesLocal_LocalRef_Scalar(Mat A, PetscInt nrow, const PetscInt irow[], PetscInt ncol, const PetscInt icol[], const PetscScalar y[], InsertMode addv)
{
  Mat_LocalRef *lr = (Mat_LocalRef *)A->data;
  PetscInt      buf[4096], *irowm, *icolm;

  PetscFunctionBegin;
  IndexSpaceGet(buf, nrow, ncol, irowm, icolm);
  /* If the row IS defining this submatrix was an ISBLOCK, then the unblocked LGMapApply is the right one to use.  If
   * instead it was (say) an ISSTRIDE with a block size > 1, then we need to use LGMapApplyBlock */
  if (lr->rowisblock) {
    PetscCall(ISLocalToGlobalMappingApply(A->rmap->mapping, nrow, irow, irowm));
  } else {
    PetscCall(ISLocalToGlobalMappingApplyBlock(A->rmap->mapping, nrow, irow, irowm));
  }
  /* As above, but for the column IS. */
  if (lr->colisblock) {
    PetscCall(ISLocalToGlobalMappingApply(A->cmap->mapping, ncol, icol, icolm));
  } else {
    PetscCall(ISLocalToGlobalMappingApplyBlock(A->cmap->mapping, ncol, icol, icolm));
  }
  PetscCall((*lr->SetValues)(lr->Top, nrow, irowm, ncol, icolm, y, addv));
  IndexSpaceRestore(buf, nrow, ncol, irowm, icolm);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Compose an IS with an ISLocalToGlobalMapping to map from IS source indices to global indices */
static PetscErrorCode ISL2GCompose(IS is, ISLocalToGlobalMapping ltog, ISLocalToGlobalMapping *cltog)
{
  const PetscInt *idx;
  PetscInt        m, *idxm;
  PetscInt        bs;
  PetscBool       isblock;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is, IS_CLASSID, 1);
  PetscValidHeaderSpecific(ltog, IS_LTOGM_CLASSID, 2);
  PetscValidPointer(cltog, 3);
  PetscCall(PetscObjectTypeCompare((PetscObject)is, ISBLOCK, &isblock));
  if (isblock) {
    PetscInt lbs;

    PetscCall(ISGetBlockSize(is, &bs));
    PetscCall(ISLocalToGlobalMappingGetBlockSize(ltog, &lbs));
    if (bs == lbs) {
      PetscCall(ISGetLocalSize(is, &m));
      m = m / bs;
      PetscCall(ISBlockGetIndices(is, &idx));
      PetscCall(PetscMalloc1(m, &idxm));
      PetscCall(ISLocalToGlobalMappingApplyBlock(ltog, m, idx, idxm));
      PetscCall(ISLocalToGlobalMappingCreate(PetscObjectComm((PetscObject)is), bs, m, idxm, PETSC_OWN_POINTER, cltog));
      PetscCall(ISBlockRestoreIndices(is, &idx));
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }
  PetscCall(ISGetLocalSize(is, &m));
  PetscCall(ISGetIndices(is, &idx));
  PetscCall(ISGetBlockSize(is, &bs));
  PetscCall(PetscMalloc1(m, &idxm));
  if (ltog) {
    PetscCall(ISLocalToGlobalMappingApply(ltog, m, idx, idxm));
  } else {
    PetscCall(PetscArraycpy(idxm, idx, m));
  }
  PetscCall(ISLocalToGlobalMappingCreate(PetscObjectComm((PetscObject)is), bs, m, idxm, PETSC_OWN_POINTER, cltog));
  PetscCall(ISRestoreIndices(is, &idx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode ISL2GComposeBlock(IS is, ISLocalToGlobalMapping ltog, ISLocalToGlobalMapping *cltog)
{
  const PetscInt *idx;
  PetscInt        m, *idxm, bs;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(is, IS_CLASSID, 1);
  PetscValidHeaderSpecific(ltog, IS_LTOGM_CLASSID, 2);
  PetscValidPointer(cltog, 3);
  PetscCall(ISBlockGetLocalSize(is, &m));
  PetscCall(ISBlockGetIndices(is, &idx));
  PetscCall(ISLocalToGlobalMappingGetBlockSize(ltog, &bs));
  PetscCall(PetscMalloc1(m, &idxm));
  if (ltog) {
    PetscCall(ISLocalToGlobalMappingApplyBlock(ltog, m, idx, idxm));
  } else {
    PetscCall(PetscArraycpy(idxm, idx, m));
  }
  PetscCall(ISLocalToGlobalMappingCreate(PetscObjectComm((PetscObject)is), bs, m, idxm, PETSC_OWN_POINTER, cltog));
  PetscCall(ISBlockRestoreIndices(is, &idx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatDestroy_LocalRef(Mat B)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(B->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatCreateLocalRef - Gets a logical reference to a local submatrix, for use in assembly, that is to set values into the matrix

   Not Collective

   Input Parameters:
+  A - full matrix, generally parallel
.  isrow - Local index set for the rows
-  iscol - Local index set for the columns

   Output Parameter:
. newmat - new serial `Mat`

   Level: developer

   Notes:
   Most will use `MatGetLocalSubMatrix()` which returns a real matrix corresponding to the local
   block if it available, such as with matrix formats that store these blocks separately.

   The new matrix forwards `MatSetValuesLocal()` and `MatSetValuesBlockedLocal()` to the global system.
   In general, it does not define `MatMult()` or any other functions.  Local submatrices can be nested.

.seealso: [](chapter_matrices), `Mat`, MATSUBMATRIX`, `MatCreateSubMatrixVirtual()`, `MatSetValuesLocal()`, `MatSetValuesBlockedLocal()`, `MatGetLocalSubMatrix()`, `MatCreateSubMatrix()`
@*/
PetscErrorCode MatCreateLocalRef(Mat A, IS isrow, IS iscol, Mat *newmat)
{
  Mat_LocalRef *lr;
  Mat           B;
  PetscInt      m, n;
  PetscBool     islr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidHeaderSpecific(isrow, IS_CLASSID, 2);
  PetscValidHeaderSpecific(iscol, IS_CLASSID, 3);
  PetscValidPointer(newmat, 4);
  PetscCheck(A->rmap->mapping, PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_WRONGSTATE, "Matrix must have local to global mapping provided before this call");
  *newmat = NULL;

  PetscCall(MatCreate(PETSC_COMM_SELF, &B));
  PetscCall(ISGetLocalSize(isrow, &m));
  PetscCall(ISGetLocalSize(iscol, &n));
  PetscCall(MatSetSizes(B, m, n, m, n));
  PetscCall(PetscObjectChangeTypeName((PetscObject)B, MATLOCALREF));
  PetscCall(MatSetUp(B));

  B->ops->destroy = MatDestroy_LocalRef;

  PetscCall(PetscNew(&lr));
  B->data = (void *)lr;

  PetscCall(PetscObjectTypeCompare((PetscObject)A, MATLOCALREF, &islr));
  if (islr) {
    Mat_LocalRef *alr = (Mat_LocalRef *)A->data;
    lr->Top           = alr->Top;
  } else {
    /* This does not increase the reference count because MatLocalRef is not allowed to live longer than its parent */
    lr->Top = A;
  }
  {
    ISLocalToGlobalMapping rltog, cltog;
    PetscInt               arbs, acbs, rbs, cbs;

    /* We will translate directly to global indices for the top level */
    lr->SetValues        = MatSetValues;
    lr->SetValuesBlocked = MatSetValuesBlocked;

    B->ops->setvalueslocal = MatSetValuesLocal_LocalRef_Scalar;

    PetscCall(ISL2GCompose(isrow, A->rmap->mapping, &rltog));
    if (isrow == iscol && A->rmap->mapping == A->cmap->mapping) {
      PetscCall(PetscObjectReference((PetscObject)rltog));
      cltog = rltog;
    } else {
      PetscCall(ISL2GCompose(iscol, A->cmap->mapping, &cltog));
    }
    /* Remember if the ISes we used to pull out the submatrix are of type ISBLOCK.  Will be used later in
     * MatSetValuesLocal_LocalRef_Scalar. */
    PetscCall(PetscObjectTypeCompare((PetscObject)isrow, ISBLOCK, &lr->rowisblock));
    PetscCall(PetscObjectTypeCompare((PetscObject)iscol, ISBLOCK, &lr->colisblock));
    PetscCall(MatSetLocalToGlobalMapping(B, rltog, cltog));
    PetscCall(ISLocalToGlobalMappingDestroy(&rltog));
    PetscCall(ISLocalToGlobalMappingDestroy(&cltog));

    PetscCall(MatGetBlockSizes(A, &arbs, &acbs));
    PetscCall(ISGetBlockSize(isrow, &rbs));
    PetscCall(ISGetBlockSize(iscol, &cbs));
    /* Always support block interface insertion on submatrix */
    PetscCall(PetscLayoutSetBlockSize(B->rmap, rbs));
    PetscCall(PetscLayoutSetBlockSize(B->cmap, cbs));
    if (arbs != rbs || acbs != cbs || (arbs == 1 && acbs == 1)) {
      /* Top-level matrix has different block size, so we have to call its scalar insertion interface */
      B->ops->setvaluesblockedlocal = MatSetValuesBlockedLocal_LocalRef_Scalar;
    } else {
      /* Block sizes match so we can forward values to the top level using the block interface */
      B->ops->setvaluesblockedlocal = MatSetValuesBlockedLocal_LocalRef_Block;

      PetscCall(ISL2GComposeBlock(isrow, A->rmap->mapping, &rltog));
      if (isrow == iscol && A->rmap->mapping == A->cmap->mapping) {
        PetscCall(PetscObjectReference((PetscObject)rltog));
        cltog = rltog;
      } else {
        PetscCall(ISL2GComposeBlock(iscol, A->cmap->mapping, &cltog));
      }
      PetscCall(MatSetLocalToGlobalMapping(B, rltog, cltog));
      PetscCall(ISLocalToGlobalMappingDestroy(&rltog));
      PetscCall(ISLocalToGlobalMappingDestroy(&cltog));
    }
  }
  *newmat = B;
  PetscFunctionReturn(PETSC_SUCCESS);
}

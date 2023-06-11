
/*
   This provides a simple shell for Fortran (and C programmers) to
  create a very simple matrix class for use with KSP without coding
  much of anything.
*/

#include <petsc/private/matimpl.h> /*I "petscmat.h" I*/
#include <petsc/private/vecimpl.h>

struct _MatShellOps {
  /*  3 */ PetscErrorCode (*mult)(Mat, Vec, Vec);
  /*  5 */ PetscErrorCode (*multtranspose)(Mat, Vec, Vec);
  /* 17 */ PetscErrorCode (*getdiagonal)(Mat, Vec);
  /* 43 */ PetscErrorCode (*copy)(Mat, Mat, MatStructure);
  /* 60 */ PetscErrorCode (*destroy)(Mat);
};

struct _n_MatShellMatFunctionList {
  PetscErrorCode (*symbolic)(Mat, Mat, Mat, void **);
  PetscErrorCode (*numeric)(Mat, Mat, Mat, void *);
  PetscErrorCode (*destroy)(void *);
  MatProductType ptype;
  char          *composedname; /* string to identify routine with double dispatch */
  char          *resultname;   /* result matrix type */

  struct _n_MatShellMatFunctionList *next;
};
typedef struct _n_MatShellMatFunctionList *MatShellMatFunctionList;

typedef struct {
  struct _MatShellOps ops[1];

  /* The user will manage the scaling and shifts for the MATSHELL, not the default */
  PetscBool managescalingshifts;

  /* support for MatScale, MatShift and MatMultAdd */
  PetscScalar vscale, vshift;
  Vec         dshift;
  Vec         left, right;
  Vec         left_work, right_work;
  Vec         left_add_work, right_add_work;

  /* support for MatAXPY */
  Mat              axpy;
  PetscScalar      axpy_vscale;
  Vec              axpy_left, axpy_right;
  PetscObjectState axpy_state;

  /* support for ZeroRows/Columns operations */
  IS         zrows;
  IS         zcols;
  Vec        zvals;
  Vec        zvals_w;
  VecScatter zvals_sct_r;
  VecScatter zvals_sct_c;

  /* MatMat operations */
  MatShellMatFunctionList matmat;

  /* user defined context */
  PetscContainer ctxcontainer;
} Mat_Shell;

/*
     Store and scale values on zeroed rows
     xx = [x_1, 0], 0 on zeroed columns
*/
static PetscErrorCode MatShellPreZeroRight(Mat A, Vec x, Vec *xx)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  *xx = x;
  if (shell->zrows) {
    PetscCall(VecSet(shell->zvals_w, 0.0));
    PetscCall(VecScatterBegin(shell->zvals_sct_c, x, shell->zvals_w, INSERT_VALUES, SCATTER_FORWARD));
    PetscCall(VecScatterEnd(shell->zvals_sct_c, x, shell->zvals_w, INSERT_VALUES, SCATTER_FORWARD));
    PetscCall(VecPointwiseMult(shell->zvals_w, shell->zvals_w, shell->zvals));
  }
  if (shell->zcols) {
    if (!shell->right_work) PetscCall(MatCreateVecs(A, &shell->right_work, NULL));
    PetscCall(VecCopy(x, shell->right_work));
    PetscCall(VecISSet(shell->right_work, shell->zcols, 0.0));
    *xx = shell->right_work;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Insert properly diagonally scaled values stored in MatShellPreZeroRight */
static PetscErrorCode MatShellPostZeroLeft(Mat A, Vec x)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (shell->zrows) {
    PetscCall(VecScatterBegin(shell->zvals_sct_r, shell->zvals_w, x, INSERT_VALUES, SCATTER_REVERSE));
    PetscCall(VecScatterEnd(shell->zvals_sct_r, shell->zvals_w, x, INSERT_VALUES, SCATTER_REVERSE));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
     Store and scale values on zeroed rows
     xx = [x_1, 0], 0 on zeroed rows
*/
static PetscErrorCode MatShellPreZeroLeft(Mat A, Vec x, Vec *xx)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  *xx = NULL;
  if (!shell->zrows) {
    *xx = x;
  } else {
    if (!shell->left_work) PetscCall(MatCreateVecs(A, NULL, &shell->left_work));
    PetscCall(VecCopy(x, shell->left_work));
    PetscCall(VecSet(shell->zvals_w, 0.0));
    PetscCall(VecScatterBegin(shell->zvals_sct_r, shell->zvals_w, shell->left_work, INSERT_VALUES, SCATTER_REVERSE));
    PetscCall(VecScatterEnd(shell->zvals_sct_r, shell->zvals_w, shell->left_work, INSERT_VALUES, SCATTER_REVERSE));
    PetscCall(VecScatterBegin(shell->zvals_sct_r, x, shell->zvals_w, INSERT_VALUES, SCATTER_FORWARD));
    PetscCall(VecScatterEnd(shell->zvals_sct_r, x, shell->zvals_w, INSERT_VALUES, SCATTER_FORWARD));
    PetscCall(VecPointwiseMult(shell->zvals_w, shell->zvals_w, shell->zvals));
    *xx = shell->left_work;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Zero zero-columns contributions, sum contributions from properly scaled values stored in MatShellPreZeroLeft */
static PetscErrorCode MatShellPostZeroRight(Mat A, Vec x)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (shell->zcols) PetscCall(VecISSet(x, shell->zcols, 0.0));
  if (shell->zrows) {
    PetscCall(VecScatterBegin(shell->zvals_sct_c, shell->zvals_w, x, ADD_VALUES, SCATTER_REVERSE));
    PetscCall(VecScatterEnd(shell->zvals_sct_c, shell->zvals_w, x, ADD_VALUES, SCATTER_REVERSE));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
      xx = diag(left)*x
*/
static PetscErrorCode MatShellPreScaleLeft(Mat A, Vec x, Vec *xx)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  *xx = NULL;
  if (!shell->left) {
    *xx = x;
  } else {
    if (!shell->left_work) PetscCall(VecDuplicate(shell->left, &shell->left_work));
    PetscCall(VecPointwiseMult(shell->left_work, x, shell->left));
    *xx = shell->left_work;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
     xx = diag(right)*x
*/
static PetscErrorCode MatShellPreScaleRight(Mat A, Vec x, Vec *xx)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  *xx = NULL;
  if (!shell->right) {
    *xx = x;
  } else {
    if (!shell->right_work) PetscCall(VecDuplicate(shell->right, &shell->right_work));
    PetscCall(VecPointwiseMult(shell->right_work, x, shell->right));
    *xx = shell->right_work;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    x = diag(left)*x
*/
static PetscErrorCode MatShellPostScaleLeft(Mat A, Vec x)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (shell->left) PetscCall(VecPointwiseMult(x, x, shell->left));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    x = diag(right)*x
*/
static PetscErrorCode MatShellPostScaleRight(Mat A, Vec x)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (shell->right) PetscCall(VecPointwiseMult(x, x, shell->right));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
         Y = vscale*Y + diag(dshift)*X + vshift*X

         On input Y already contains A*x
*/
static PetscErrorCode MatShellShiftAndScale(Mat A, Vec X, Vec Y)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (shell->dshift) { /* get arrays because there is no VecPointwiseMultAdd() */
    PetscInt           i, m;
    const PetscScalar *x, *d;
    PetscScalar       *y;
    PetscCall(VecGetLocalSize(X, &m));
    PetscCall(VecGetArrayRead(shell->dshift, &d));
    PetscCall(VecGetArrayRead(X, &x));
    PetscCall(VecGetArray(Y, &y));
    for (i = 0; i < m; i++) y[i] = shell->vscale * y[i] + d[i] * x[i];
    PetscCall(VecRestoreArrayRead(shell->dshift, &d));
    PetscCall(VecRestoreArrayRead(X, &x));
    PetscCall(VecRestoreArray(Y, &y));
  } else {
    PetscCall(VecScale(Y, shell->vscale));
  }
  if (shell->vshift != 0.0) PetscCall(VecAXPY(Y, shell->vshift, X)); /* if test is for non-square matrices */
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatShellGetContext_Shell(Mat mat, void *ctx)
{
  Mat_Shell *shell = (Mat_Shell *)mat->data;

  PetscFunctionBegin;
  if (shell->ctxcontainer) PetscCall(PetscContainerGetPointer(shell->ctxcontainer, (void **)ctx));
  else *(void **)ctx = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    MatShellGetContext - Returns the user-provided context associated with a `MATSHELL` shell matrix.

    Not Collective

    Input Parameter:
.   mat - the matrix, should have been created with `MatCreateShell()`

    Output Parameter:
.   ctx - the user provided context

    Level: advanced

   Fortran Note:
    You must write a Fortran interface definition for this
    function that tells Fortran the Fortran derived data type that you are passing in as the ctx argument.

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellSetOperation()`, `MatShellSetContext()`
@*/
PetscErrorCode MatShellGetContext(Mat mat, void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscValidPointer(ctx, 2);
  PetscUseMethod(mat, "MatShellGetContext_C", (Mat, void *), (mat, ctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatZeroRowsColumns_Local_Shell(Mat mat, PetscInt nr, PetscInt rows[], PetscInt nc, PetscInt cols[], PetscScalar diag, PetscBool rc)
{
  Mat_Shell      *shell = (Mat_Shell *)mat->data;
  Vec             x = NULL, b = NULL;
  IS              is1, is2;
  const PetscInt *ridxs;
  PetscInt       *idxs, *gidxs;
  PetscInt        cum, rst, cst, i;

  PetscFunctionBegin;
  if (!shell->zvals) PetscCall(MatCreateVecs(mat, NULL, &shell->zvals));
  if (!shell->zvals_w) PetscCall(VecDuplicate(shell->zvals, &shell->zvals_w));
  PetscCall(MatGetOwnershipRange(mat, &rst, NULL));
  PetscCall(MatGetOwnershipRangeColumn(mat, &cst, NULL));

  /* Expand/create index set of zeroed rows */
  PetscCall(PetscMalloc1(nr, &idxs));
  for (i = 0; i < nr; i++) idxs[i] = rows[i] + rst;
  PetscCall(ISCreateGeneral(PETSC_COMM_SELF, nr, idxs, PETSC_OWN_POINTER, &is1));
  PetscCall(ISSort(is1));
  PetscCall(VecISSet(shell->zvals, is1, diag));
  if (shell->zrows) {
    PetscCall(ISSum(shell->zrows, is1, &is2));
    PetscCall(ISDestroy(&shell->zrows));
    PetscCall(ISDestroy(&is1));
    shell->zrows = is2;
  } else shell->zrows = is1;

  /* Create scatters for diagonal values communications */
  PetscCall(VecScatterDestroy(&shell->zvals_sct_c));
  PetscCall(VecScatterDestroy(&shell->zvals_sct_r));

  /* row scatter: from/to left vector */
  PetscCall(MatCreateVecs(mat, &x, &b));
  PetscCall(VecScatterCreate(b, shell->zrows, shell->zvals_w, shell->zrows, &shell->zvals_sct_r));

  /* col scatter: from right vector to left vector */
  PetscCall(ISGetIndices(shell->zrows, &ridxs));
  PetscCall(ISGetLocalSize(shell->zrows, &nr));
  PetscCall(PetscMalloc1(nr, &gidxs));
  for (i = 0, cum = 0; i < nr; i++) {
    if (ridxs[i] >= mat->cmap->N) continue;
    gidxs[cum] = ridxs[i];
    cum++;
  }
  PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)mat), cum, gidxs, PETSC_OWN_POINTER, &is1));
  PetscCall(VecScatterCreate(x, is1, shell->zvals_w, is1, &shell->zvals_sct_c));
  PetscCall(ISDestroy(&is1));
  PetscCall(VecDestroy(&x));
  PetscCall(VecDestroy(&b));

  /* Expand/create index set of zeroed columns */
  if (rc) {
    PetscCall(PetscMalloc1(nc, &idxs));
    for (i = 0; i < nc; i++) idxs[i] = cols[i] + cst;
    PetscCall(ISCreateGeneral(PETSC_COMM_SELF, nc, idxs, PETSC_OWN_POINTER, &is1));
    PetscCall(ISSort(is1));
    if (shell->zcols) {
      PetscCall(ISSum(shell->zcols, is1, &is2));
      PetscCall(ISDestroy(&shell->zcols));
      PetscCall(ISDestroy(&is1));
      shell->zcols = is2;
    } else shell->zcols = is1;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatZeroRows_Shell(Mat mat, PetscInt n, const PetscInt rows[], PetscScalar diag, Vec x, Vec b)
{
  Mat_Shell *shell = (Mat_Shell *)mat->data;
  PetscInt   nr, *lrows;

  PetscFunctionBegin;
  if (x && b) {
    Vec          xt;
    PetscScalar *vals;
    PetscInt    *gcols, i, st, nl, nc;

    PetscCall(PetscMalloc1(n, &gcols));
    for (i = 0, nc = 0; i < n; i++)
      if (rows[i] < mat->cmap->N) gcols[nc++] = rows[i];

    PetscCall(MatCreateVecs(mat, &xt, NULL));
    PetscCall(VecCopy(x, xt));
    PetscCall(PetscCalloc1(nc, &vals));
    PetscCall(VecSetValues(xt, nc, gcols, vals, INSERT_VALUES)); /* xt = [x1, 0] */
    PetscCall(PetscFree(vals));
    PetscCall(VecAssemblyBegin(xt));
    PetscCall(VecAssemblyEnd(xt));
    PetscCall(VecAYPX(xt, -1.0, x)); /* xt = [0, x2] */

    PetscCall(VecGetOwnershipRange(xt, &st, NULL));
    PetscCall(VecGetLocalSize(xt, &nl));
    PetscCall(VecGetArray(xt, &vals));
    for (i = 0; i < nl; i++) {
      PetscInt g = i + st;
      if (g > mat->rmap->N) continue;
      if (PetscAbsScalar(vals[i]) == 0.0) continue;
      PetscCall(VecSetValue(b, g, diag * vals[i], INSERT_VALUES));
    }
    PetscCall(VecRestoreArray(xt, &vals));
    PetscCall(VecAssemblyBegin(b));
    PetscCall(VecAssemblyEnd(b)); /* b  = [b1, x2 * diag] */
    PetscCall(VecDestroy(&xt));
    PetscCall(PetscFree(gcols));
  }
  PetscCall(PetscLayoutMapLocal(mat->rmap, n, rows, &nr, &lrows, NULL));
  PetscCall(MatZeroRowsColumns_Local_Shell(mat, nr, lrows, 0, NULL, diag, PETSC_FALSE));
  if (shell->axpy) PetscCall(MatZeroRows(shell->axpy, n, rows, 0.0, NULL, NULL));
  PetscCall(PetscFree(lrows));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatZeroRowsColumns_Shell(Mat mat, PetscInt n, const PetscInt rowscols[], PetscScalar diag, Vec x, Vec b)
{
  Mat_Shell *shell = (Mat_Shell *)mat->data;
  PetscInt  *lrows, *lcols;
  PetscInt   nr, nc;
  PetscBool  congruent;

  PetscFunctionBegin;
  if (x && b) {
    Vec          xt, bt;
    PetscScalar *vals;
    PetscInt    *grows, *gcols, i, st, nl;

    PetscCall(PetscMalloc2(n, &grows, n, &gcols));
    for (i = 0, nr = 0; i < n; i++)
      if (rowscols[i] < mat->rmap->N) grows[nr++] = rowscols[i];
    for (i = 0, nc = 0; i < n; i++)
      if (rowscols[i] < mat->cmap->N) gcols[nc++] = rowscols[i];
    PetscCall(PetscCalloc1(n, &vals));

    PetscCall(MatCreateVecs(mat, &xt, &bt));
    PetscCall(VecCopy(x, xt));
    PetscCall(VecSetValues(xt, nc, gcols, vals, INSERT_VALUES)); /* xt = [x1, 0] */
    PetscCall(VecAssemblyBegin(xt));
    PetscCall(VecAssemblyEnd(xt));
    PetscCall(VecAXPY(xt, -1.0, x));                             /* xt = [0, -x2] */
    PetscCall(MatMult(mat, xt, bt));                             /* bt = [-A12*x2,-A22*x2] */
    PetscCall(VecSetValues(bt, nr, grows, vals, INSERT_VALUES)); /* bt = [-A12*x2,0] */
    PetscCall(VecAssemblyBegin(bt));
    PetscCall(VecAssemblyEnd(bt));
    PetscCall(VecAXPY(b, 1.0, bt));                              /* b  = [b1 - A12*x2, b2] */
    PetscCall(VecSetValues(bt, nr, grows, vals, INSERT_VALUES)); /* b  = [b1 - A12*x2, 0] */
    PetscCall(VecAssemblyBegin(bt));
    PetscCall(VecAssemblyEnd(bt));
    PetscCall(PetscFree(vals));

    PetscCall(VecGetOwnershipRange(xt, &st, NULL));
    PetscCall(VecGetLocalSize(xt, &nl));
    PetscCall(VecGetArray(xt, &vals));
    for (i = 0; i < nl; i++) {
      PetscInt g = i + st;
      if (g > mat->rmap->N) continue;
      if (PetscAbsScalar(vals[i]) == 0.0) continue;
      PetscCall(VecSetValue(b, g, -diag * vals[i], INSERT_VALUES));
    }
    PetscCall(VecRestoreArray(xt, &vals));
    PetscCall(VecAssemblyBegin(b));
    PetscCall(VecAssemblyEnd(b)); /* b  = [b1 - A12*x2, x2 * diag] */
    PetscCall(VecDestroy(&xt));
    PetscCall(VecDestroy(&bt));
    PetscCall(PetscFree2(grows, gcols));
  }
  PetscCall(PetscLayoutMapLocal(mat->rmap, n, rowscols, &nr, &lrows, NULL));
  PetscCall(MatHasCongruentLayouts(mat, &congruent));
  if (congruent) {
    nc    = nr;
    lcols = lrows;
  } else { /* MatZeroRowsColumns implicitly assumes the rowscols indices are for a square matrix, here we handle a more general case */
    PetscInt i, nt, *t;

    PetscCall(PetscMalloc1(n, &t));
    for (i = 0, nt = 0; i < n; i++)
      if (rowscols[i] < mat->cmap->N) t[nt++] = rowscols[i];
    PetscCall(PetscLayoutMapLocal(mat->cmap, nt, t, &nc, &lcols, NULL));
    PetscCall(PetscFree(t));
  }
  PetscCall(MatZeroRowsColumns_Local_Shell(mat, nr, lrows, nc, lcols, diag, PETSC_TRUE));
  if (!congruent) PetscCall(PetscFree(lcols));
  PetscCall(PetscFree(lrows));
  if (shell->axpy) PetscCall(MatZeroRowsColumns(shell->axpy, n, rowscols, 0.0, NULL, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatDestroy_Shell(Mat mat)
{
  Mat_Shell              *shell = (Mat_Shell *)mat->data;
  MatShellMatFunctionList matmat;

  PetscFunctionBegin;
  if (shell->ops->destroy) PetscCall((*shell->ops->destroy)(mat));
  PetscCall(PetscMemzero(shell->ops, sizeof(struct _MatShellOps)));
  PetscCall(VecDestroy(&shell->left));
  PetscCall(VecDestroy(&shell->right));
  PetscCall(VecDestroy(&shell->dshift));
  PetscCall(VecDestroy(&shell->left_work));
  PetscCall(VecDestroy(&shell->right_work));
  PetscCall(VecDestroy(&shell->left_add_work));
  PetscCall(VecDestroy(&shell->right_add_work));
  PetscCall(VecDestroy(&shell->axpy_left));
  PetscCall(VecDestroy(&shell->axpy_right));
  PetscCall(MatDestroy(&shell->axpy));
  PetscCall(VecDestroy(&shell->zvals_w));
  PetscCall(VecDestroy(&shell->zvals));
  PetscCall(VecScatterDestroy(&shell->zvals_sct_c));
  PetscCall(VecScatterDestroy(&shell->zvals_sct_r));
  PetscCall(ISDestroy(&shell->zrows));
  PetscCall(ISDestroy(&shell->zcols));

  matmat = shell->matmat;
  while (matmat) {
    MatShellMatFunctionList next = matmat->next;

    PetscCall(PetscObjectComposeFunction((PetscObject)mat, matmat->composedname, NULL));
    PetscCall(PetscFree(matmat->composedname));
    PetscCall(PetscFree(matmat->resultname));
    PetscCall(PetscFree(matmat));
    matmat = next;
  }
  PetscCall(MatShellSetContext(mat, NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)mat, "MatShellGetContext_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)mat, "MatShellSetContext_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)mat, "MatShellSetContextDestroy_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)mat, "MatShellSetVecType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)mat, "MatShellSetManageScalingShifts_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)mat, "MatShellSetOperation_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)mat, "MatShellGetOperation_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)mat, "MatShellSetMatProductOperation_C", NULL));
  PetscCall(PetscFree(mat->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

typedef struct {
  PetscErrorCode (*numeric)(Mat, Mat, Mat, void *);
  PetscErrorCode (*destroy)(void *);
  void *userdata;
  Mat   B;
  Mat   Bt;
  Mat   axpy;
} MatMatDataShell;

static PetscErrorCode DestroyMatMatDataShell(void *data)
{
  MatMatDataShell *mmdata = (MatMatDataShell *)data;

  PetscFunctionBegin;
  if (mmdata->destroy) PetscCall((*mmdata->destroy)(mmdata->userdata));
  PetscCall(MatDestroy(&mmdata->B));
  PetscCall(MatDestroy(&mmdata->Bt));
  PetscCall(MatDestroy(&mmdata->axpy));
  PetscCall(PetscFree(mmdata));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatProductNumeric_Shell_X(Mat D)
{
  Mat_Product     *product;
  Mat              A, B;
  MatMatDataShell *mdata;
  PetscScalar      zero = 0.0;

  PetscFunctionBegin;
  MatCheckProduct(D, 1);
  product = D->product;
  PetscCheck(product->data, PetscObjectComm((PetscObject)D), PETSC_ERR_PLIB, "Product data empty");
  A     = product->A;
  B     = product->B;
  mdata = (MatMatDataShell *)product->data;
  if (mdata->numeric) {
    Mat_Shell *shell                = (Mat_Shell *)A->data;
    PetscErrorCode (*stashsym)(Mat) = D->ops->productsymbolic;
    PetscErrorCode (*stashnum)(Mat) = D->ops->productnumeric;
    PetscBool useBmdata = PETSC_FALSE, newB = PETSC_TRUE;

    if (shell->managescalingshifts) {
      PetscCheck(!shell->zcols && !shell->zrows, PetscObjectComm((PetscObject)D), PETSC_ERR_SUP, "MatProduct not supported with zeroed rows/columns");
      if (shell->right || shell->left) {
        useBmdata = PETSC_TRUE;
        if (!mdata->B) {
          PetscCall(MatDuplicate(B, MAT_SHARE_NONZERO_PATTERN, &mdata->B));
        } else {
          newB = PETSC_FALSE;
        }
        PetscCall(MatCopy(B, mdata->B, SAME_NONZERO_PATTERN));
      }
      switch (product->type) {
      case MATPRODUCT_AB: /* s L A R B + v L R B + L D R B */
        if (shell->right) PetscCall(MatDiagonalScale(mdata->B, shell->right, NULL));
        break;
      case MATPRODUCT_AtB: /* s R A^t L B + v R L B + R D L B */
        if (shell->left) PetscCall(MatDiagonalScale(mdata->B, shell->left, NULL));
        break;
      case MATPRODUCT_ABt: /* s L A R B^t + v L R B^t + L D R B^t */
        if (shell->right) PetscCall(MatDiagonalScale(mdata->B, NULL, shell->right));
        break;
      case MATPRODUCT_RARt: /* s B L A R B^t + v B L R B^t + B L D R B^t */
        if (shell->right && shell->left) {
          PetscBool flg;

          PetscCall(VecEqual(shell->right, shell->left, &flg));
          PetscCheck(flg, PetscObjectComm((PetscObject)D), PETSC_ERR_SUP, "MatProductSymbolic type %s not supported for %s and %s matrices because left scaling != from right scaling", MatProductTypes[product->type], ((PetscObject)A)->type_name,
                     ((PetscObject)B)->type_name);
        }
        if (shell->right) PetscCall(MatDiagonalScale(mdata->B, NULL, shell->right));
        break;
      case MATPRODUCT_PtAP: /* s B^t L A R B + v B^t L R B + B^t L D R B */
        if (shell->right && shell->left) {
          PetscBool flg;

          PetscCall(VecEqual(shell->right, shell->left, &flg));
          PetscCheck(flg, PetscObjectComm((PetscObject)D), PETSC_ERR_SUP, "MatProductSymbolic type %s not supported for %s and %s matrices because left scaling != from right scaling", MatProductTypes[product->type], ((PetscObject)A)->type_name,
                     ((PetscObject)B)->type_name);
        }
        if (shell->right) PetscCall(MatDiagonalScale(mdata->B, shell->right, NULL));
        break;
      default:
        SETERRQ(PetscObjectComm((PetscObject)D), PETSC_ERR_SUP, "MatProductSymbolic type %s not supported for %s and %s matrices", MatProductTypes[product->type], ((PetscObject)A)->type_name, ((PetscObject)B)->type_name);
      }
    }
    /* allow the user to call MatMat operations on D */
    D->product              = NULL;
    D->ops->productsymbolic = NULL;
    D->ops->productnumeric  = NULL;

    PetscCall((*mdata->numeric)(A, useBmdata ? mdata->B : B, D, mdata->userdata));

    /* clear any leftover user data and restore D pointers */
    PetscCall(MatProductClear(D));
    D->ops->productsymbolic = stashsym;
    D->ops->productnumeric  = stashnum;
    D->product              = product;

    if (shell->managescalingshifts) {
      PetscCall(MatScale(D, shell->vscale));
      switch (product->type) {
      case MATPRODUCT_AB:  /* s L A R B + v L R B + L D R B */
      case MATPRODUCT_ABt: /* s L A R B^t + v L R B^t + L D R B^t */
        if (shell->left) {
          PetscCall(MatDiagonalScale(D, shell->left, NULL));
          if (shell->dshift || shell->vshift != zero) {
            if (!shell->left_work) PetscCall(MatCreateVecs(A, NULL, &shell->left_work));
            if (shell->dshift) {
              PetscCall(VecCopy(shell->dshift, shell->left_work));
              PetscCall(VecShift(shell->left_work, shell->vshift));
              PetscCall(VecPointwiseMult(shell->left_work, shell->left_work, shell->left));
            } else {
              PetscCall(VecSet(shell->left_work, shell->vshift));
            }
            if (product->type == MATPRODUCT_ABt) {
              MatReuse     reuse = mdata->Bt ? MAT_REUSE_MATRIX : MAT_INITIAL_MATRIX;
              MatStructure str   = mdata->Bt ? SUBSET_NONZERO_PATTERN : DIFFERENT_NONZERO_PATTERN;

              PetscCall(MatTranspose(mdata->B, reuse, &mdata->Bt));
              PetscCall(MatDiagonalScale(mdata->Bt, shell->left_work, NULL));
              PetscCall(MatAXPY(D, 1.0, mdata->Bt, str));
            } else {
              MatStructure str = newB ? DIFFERENT_NONZERO_PATTERN : SUBSET_NONZERO_PATTERN;

              PetscCall(MatDiagonalScale(mdata->B, shell->left_work, NULL));
              PetscCall(MatAXPY(D, 1.0, mdata->B, str));
            }
          }
        }
        break;
      case MATPRODUCT_AtB: /* s R A^t L B + v R L B + R D L B */
        if (shell->right) {
          PetscCall(MatDiagonalScale(D, shell->right, NULL));
          if (shell->dshift || shell->vshift != zero) {
            MatStructure str = newB ? DIFFERENT_NONZERO_PATTERN : SUBSET_NONZERO_PATTERN;

            if (!shell->right_work) PetscCall(MatCreateVecs(A, &shell->right_work, NULL));
            if (shell->dshift) {
              PetscCall(VecCopy(shell->dshift, shell->right_work));
              PetscCall(VecShift(shell->right_work, shell->vshift));
              PetscCall(VecPointwiseMult(shell->right_work, shell->right_work, shell->right));
            } else {
              PetscCall(VecSet(shell->right_work, shell->vshift));
            }
            PetscCall(MatDiagonalScale(mdata->B, shell->right_work, NULL));
            PetscCall(MatAXPY(D, 1.0, mdata->B, str));
          }
        }
        break;
      case MATPRODUCT_PtAP: /* s B^t L A R B + v B^t L R B + B^t L D R B */
      case MATPRODUCT_RARt: /* s B L A R B^t + v B L R B^t + B L D R B^t */
        PetscCheck(!shell->dshift && shell->vshift == zero, PetscObjectComm((PetscObject)D), PETSC_ERR_SUP, "MatProductSymbolic type %s not supported for %s and %s matrices with diagonal shift", MatProductTypes[product->type], ((PetscObject)A)->type_name,
                   ((PetscObject)B)->type_name);
        break;
      default:
        SETERRQ(PetscObjectComm((PetscObject)D), PETSC_ERR_SUP, "MatProductSymbolic type %s not supported for %s and %s matrices", MatProductTypes[product->type], ((PetscObject)A)->type_name, ((PetscObject)B)->type_name);
      }
      if (shell->axpy && shell->axpy_vscale != zero) {
        Mat              X;
        PetscObjectState axpy_state;
        MatStructure     str = DIFFERENT_NONZERO_PATTERN; /* not sure it is safe to ever use SUBSET_NONZERO_PATTERN */

        PetscCall(MatShellGetContext(shell->axpy, &X));
        PetscCall(PetscObjectStateGet((PetscObject)X, &axpy_state));
        PetscCheck(shell->axpy_state == axpy_state, PetscObjectComm((PetscObject)A), PETSC_ERR_ORDER, "Invalid AXPY state: cannot modify the X matrix passed to MatAXPY(Y,a,X,...)");
        if (!mdata->axpy) {
          str = DIFFERENT_NONZERO_PATTERN;
          PetscCall(MatProductCreate(shell->axpy, B, NULL, &mdata->axpy));
          PetscCall(MatProductSetType(mdata->axpy, product->type));
          PetscCall(MatProductSetFromOptions(mdata->axpy));
          PetscCall(MatProductSymbolic(mdata->axpy));
        } else { /* May be that shell->axpy has changed */
          PetscBool flg;

          PetscCall(MatProductReplaceMats(shell->axpy, B, NULL, mdata->axpy));
          PetscCall(MatHasOperation(mdata->axpy, MATOP_PRODUCTSYMBOLIC, &flg));
          if (!flg) {
            str = DIFFERENT_NONZERO_PATTERN;
            PetscCall(MatProductSetFromOptions(mdata->axpy));
            PetscCall(MatProductSymbolic(mdata->axpy));
          }
        }
        PetscCall(MatProductNumeric(mdata->axpy));
        PetscCall(MatAXPY(D, shell->axpy_vscale, mdata->axpy, str));
      }
    }
  } else SETERRQ(PetscObjectComm((PetscObject)D), PETSC_ERR_PLIB, "Missing numeric operation");
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatProductSymbolic_Shell_X(Mat D)
{
  Mat_Product            *product;
  Mat                     A, B;
  MatShellMatFunctionList matmat;
  Mat_Shell              *shell;
  PetscBool               flg = PETSC_FALSE;
  char                    composedname[256];
  MatMatDataShell        *mdata;

  PetscFunctionBegin;
  MatCheckProduct(D, 1);
  product = D->product;
  PetscCheck(!product->data, PetscObjectComm((PetscObject)D), PETSC_ERR_PLIB, "Product data not empty");
  A      = product->A;
  B      = product->B;
  shell  = (Mat_Shell *)A->data;
  matmat = shell->matmat;
  PetscCall(PetscSNPrintf(composedname, sizeof(composedname), "MatProductSetFromOptions_%s_%s_C", ((PetscObject)A)->type_name, ((PetscObject)B)->type_name));
  while (matmat) {
    PetscCall(PetscStrcmp(composedname, matmat->composedname, &flg));
    flg = (PetscBool)(flg && (matmat->ptype == product->type));
    if (flg) break;
    matmat = matmat->next;
  }
  PetscCheck(flg, PetscObjectComm((PetscObject)A), PETSC_ERR_PLIB, "Composedname \"%s\" for product type %s not found", composedname, MatProductTypes[product->type]);
  switch (product->type) {
  case MATPRODUCT_AB:
    PetscCall(MatSetSizes(D, A->rmap->n, B->cmap->n, A->rmap->N, B->cmap->N));
    break;
  case MATPRODUCT_AtB:
    PetscCall(MatSetSizes(D, A->cmap->n, B->cmap->n, A->cmap->N, B->cmap->N));
    break;
  case MATPRODUCT_ABt:
    PetscCall(MatSetSizes(D, A->rmap->n, B->rmap->n, A->rmap->N, B->rmap->N));
    break;
  case MATPRODUCT_RARt:
    PetscCall(MatSetSizes(D, B->rmap->n, B->rmap->n, B->rmap->N, B->rmap->N));
    break;
  case MATPRODUCT_PtAP:
    PetscCall(MatSetSizes(D, B->cmap->n, B->cmap->n, B->cmap->N, B->cmap->N));
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)D), PETSC_ERR_SUP, "MatProductSymbolic type %s not supported for %s and %s matrices", MatProductTypes[product->type], ((PetscObject)A)->type_name, ((PetscObject)B)->type_name);
  }
  /* respect users who passed in a matrix for which resultname is the base type */
  if (matmat->resultname) {
    PetscCall(PetscObjectBaseTypeCompare((PetscObject)D, matmat->resultname, &flg));
    if (!flg) PetscCall(MatSetType(D, matmat->resultname));
  }
  /* If matrix type was not set or different, we need to reset this pointers */
  D->ops->productsymbolic = MatProductSymbolic_Shell_X;
  D->ops->productnumeric  = MatProductNumeric_Shell_X;
  /* attach product data */
  PetscCall(PetscNew(&mdata));
  mdata->numeric = matmat->numeric;
  mdata->destroy = matmat->destroy;
  if (matmat->symbolic) {
    PetscCall((*matmat->symbolic)(A, B, D, &mdata->userdata));
  } else { /* call general setup if symbolic operation not provided */
    PetscCall(MatSetUp(D));
  }
  PetscCheck(D->product, PetscObjectComm((PetscObject)D), PETSC_ERR_COR, "Product disappeared after user symbolic phase");
  PetscCheck(!D->product->data, PetscObjectComm((PetscObject)D), PETSC_ERR_COR, "Product data not empty after user symbolic phase");
  D->product->data    = mdata;
  D->product->destroy = DestroyMatMatDataShell;
  /* Be sure to reset these pointers if the user did something unexpected */
  D->ops->productsymbolic = MatProductSymbolic_Shell_X;
  D->ops->productnumeric  = MatProductNumeric_Shell_X;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatProductSetFromOptions_Shell_X(Mat D)
{
  Mat_Product            *product;
  Mat                     A, B;
  MatShellMatFunctionList matmat;
  Mat_Shell              *shell;
  PetscBool               flg;
  char                    composedname[256];

  PetscFunctionBegin;
  MatCheckProduct(D, 1);
  product = D->product;
  A       = product->A;
  B       = product->B;
  PetscCall(MatIsShell(A, &flg));
  if (!flg) PetscFunctionReturn(PETSC_SUCCESS);
  shell  = (Mat_Shell *)A->data;
  matmat = shell->matmat;
  PetscCall(PetscSNPrintf(composedname, sizeof(composedname), "MatProductSetFromOptions_%s_%s_C", ((PetscObject)A)->type_name, ((PetscObject)B)->type_name));
  while (matmat) {
    PetscCall(PetscStrcmp(composedname, matmat->composedname, &flg));
    flg = (PetscBool)(flg && (matmat->ptype == product->type));
    if (flg) break;
    matmat = matmat->next;
  }
  if (flg) {
    D->ops->productsymbolic = MatProductSymbolic_Shell_X;
  } else PetscCall(PetscInfo(D, "  symbolic product %s not registered for product type %s\n", composedname, MatProductTypes[product->type]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatShellSetMatProductOperation_Private(Mat A, MatProductType ptype, PetscErrorCode (*symbolic)(Mat, Mat, Mat, void **), PetscErrorCode (*numeric)(Mat, Mat, Mat, void *), PetscErrorCode (*destroy)(void *), char *composedname, const char *resultname)
{
  PetscBool               flg;
  Mat_Shell              *shell;
  MatShellMatFunctionList matmat;

  PetscFunctionBegin;
  PetscCheck(numeric, PetscObjectComm((PetscObject)A), PETSC_ERR_PLIB, "Missing numeric routine");
  PetscCheck(composedname, PetscObjectComm((PetscObject)A), PETSC_ERR_PLIB, "Missing composed name");

  /* add product callback */
  shell  = (Mat_Shell *)A->data;
  matmat = shell->matmat;
  if (!matmat) {
    PetscCall(PetscNew(&shell->matmat));
    matmat = shell->matmat;
  } else {
    MatShellMatFunctionList entry = matmat;
    while (entry) {
      PetscCall(PetscStrcmp(composedname, entry->composedname, &flg));
      flg    = (PetscBool)(flg && (entry->ptype == ptype));
      matmat = entry;
      if (flg) goto set;
      entry = entry->next;
    }
    PetscCall(PetscNew(&matmat->next));
    matmat = matmat->next;
  }

set:
  matmat->symbolic = symbolic;
  matmat->numeric  = numeric;
  matmat->destroy  = destroy;
  matmat->ptype    = ptype;
  PetscCall(PetscFree(matmat->composedname));
  PetscCall(PetscFree(matmat->resultname));
  PetscCall(PetscStrallocpy(composedname, &matmat->composedname));
  PetscCall(PetscStrallocpy(resultname, &matmat->resultname));
  PetscCall(PetscInfo(A, "Composing %s for product type %s with result %s\n", matmat->composedname, MatProductTypes[matmat->ptype], matmat->resultname ? matmat->resultname : "not specified"));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, matmat->composedname, MatProductSetFromOptions_Shell_X));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    MatShellSetMatProductOperation - Allows user to set a matrix matrix operation for a `MATSHELL` shell matrix.

   Logically Collective; No Fortran Support

    Input Parameters:
+   A - the `MATSHELL` shell matrix
.   ptype - the product type
.   symbolic - the function for the symbolic phase (can be `NULL`)
.   numeric - the function for the numerical phase
.   destroy - the function for the destruction of the needed data generated during the symbolic phase (can be `NULL`)
.   Btype - the matrix type for the matrix to be multiplied against
-   Ctype - the matrix type for the result (can be `NULL`)

   Level: advanced

    Usage:
.vb
      extern PetscErrorCode usersymbolic(Mat,Mat,Mat,void**);
      extern PetscErrorCode usernumeric(Mat,Mat,Mat,void*);
      extern PetscErrorCode userdestroy(void*);
      MatCreateShell(comm,m,n,M,N,ctx,&A);
      MatShellSetMatProductOperation(A,MATPRODUCT_AB,usersymbolic,usernumeric,userdestroy,MATSEQAIJ,MATDENSE);
      [ create B of type SEQAIJ etc..]
      MatProductCreate(A,B,NULL,&C);
      MatProductSetType(C,MATPRODUCT_AB);
      MatProductSetFromOptions(C);
      MatProductSymbolic(C); -> actually runs the user defined symbolic operation
      MatProductNumeric(C); -> actually runs the user defined numeric operation
      [ use C = A*B ]
.ve

    Notes:
    `MATPRODUCT_ABC` is not supported yet.

    If the symbolic phase is not specified, `MatSetUp()` is called on the result matrix that must have its type set if Ctype is `NULL`.

    Any additional data needed by the matrix product needs to be returned during the symbolic phase and destroyed with the destroy callback.
    PETSc will take care of calling the user-defined callbacks.
    It is allowed to specify the same callbacks for different Btype matrix types.
    The couple (Btype,ptype) uniquely identifies the operation, the last specified callbacks takes precedence.

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellGetContext()`, `MatShellGetOperation()`, `MatShellSetContext()`, `MatSetOperation()`, `MatProductType`, `MatType`, `MatSetUp()`
@*/
PetscErrorCode MatShellSetMatProductOperation(Mat A, MatProductType ptype, PetscErrorCode (*symbolic)(Mat, Mat, Mat, void **), PetscErrorCode (*numeric)(Mat, Mat, Mat, void *), PetscErrorCode (*destroy)(void *), MatType Btype, MatType Ctype)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscValidLogicalCollectiveEnum(A, ptype, 2);
  PetscCheck(ptype != MATPRODUCT_ABC, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "Not for product type %s", MatProductTypes[ptype]);
  PetscCheck(numeric, PetscObjectComm((PetscObject)A), PETSC_ERR_USER, "Missing numeric routine, argument 4");
  PetscValidCharPointer(Btype, 6);
  if (Ctype) PetscValidCharPointer(Ctype, 7);
  PetscTryMethod(A, "MatShellSetMatProductOperation_C", (Mat, MatProductType, PetscErrorCode(*)(Mat, Mat, Mat, void **), PetscErrorCode(*)(Mat, Mat, Mat, void *), PetscErrorCode(*)(void *), MatType, MatType), (A, ptype, symbolic, numeric, destroy, Btype, Ctype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatShellSetMatProductOperation_Shell(Mat A, MatProductType ptype, PetscErrorCode (*symbolic)(Mat, Mat, Mat, void **), PetscErrorCode (*numeric)(Mat, Mat, Mat, void *), PetscErrorCode (*destroy)(void *), MatType Btype, MatType Ctype)
{
  PetscBool   flg;
  char        composedname[256];
  MatRootName Bnames = MatRootNameList, Cnames = MatRootNameList;
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscValidType(A, 1);
  while (Bnames) { /* user passed in the root name */
    PetscCall(PetscStrcmp(Btype, Bnames->rname, &flg));
    if (flg) break;
    Bnames = Bnames->next;
  }
  while (Cnames) { /* user passed in the root name */
    PetscCall(PetscStrcmp(Ctype, Cnames->rname, &flg));
    if (flg) break;
    Cnames = Cnames->next;
  }
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)A), &size));
  Btype = Bnames ? (size > 1 ? Bnames->mname : Bnames->sname) : Btype;
  Ctype = Cnames ? (size > 1 ? Cnames->mname : Cnames->sname) : Ctype;
  PetscCall(PetscSNPrintf(composedname, sizeof(composedname), "MatProductSetFromOptions_%s_%s_C", ((PetscObject)A)->type_name, Btype));
  PetscCall(MatShellSetMatProductOperation_Private(A, ptype, symbolic, numeric, destroy, composedname, Ctype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatCopy_Shell(Mat A, Mat B, MatStructure str)
{
  Mat_Shell              *shellA = (Mat_Shell *)A->data, *shellB = (Mat_Shell *)B->data;
  PetscBool               matflg;
  MatShellMatFunctionList matmatA;

  PetscFunctionBegin;
  PetscCall(MatIsShell(B, &matflg));
  PetscCheck(matflg, PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_WRONG, "Matrix %s not derived from MATSHELL", ((PetscObject)B)->type_name);

  PetscCall(PetscMemcpy(B->ops, A->ops, sizeof(struct _MatOps)));
  PetscCall(PetscMemcpy(shellB->ops, shellA->ops, sizeof(struct _MatShellOps)));

  if (shellA->ops->copy) PetscCall((*shellA->ops->copy)(A, B, str));
  shellB->vscale = shellA->vscale;
  shellB->vshift = shellA->vshift;
  if (shellA->dshift) {
    if (!shellB->dshift) PetscCall(VecDuplicate(shellA->dshift, &shellB->dshift));
    PetscCall(VecCopy(shellA->dshift, shellB->dshift));
  } else {
    PetscCall(VecDestroy(&shellB->dshift));
  }
  if (shellA->left) {
    if (!shellB->left) PetscCall(VecDuplicate(shellA->left, &shellB->left));
    PetscCall(VecCopy(shellA->left, shellB->left));
  } else {
    PetscCall(VecDestroy(&shellB->left));
  }
  if (shellA->right) {
    if (!shellB->right) PetscCall(VecDuplicate(shellA->right, &shellB->right));
    PetscCall(VecCopy(shellA->right, shellB->right));
  } else {
    PetscCall(VecDestroy(&shellB->right));
  }
  PetscCall(MatDestroy(&shellB->axpy));
  shellB->axpy_vscale = 0.0;
  shellB->axpy_state  = 0;
  if (shellA->axpy) {
    PetscCall(PetscObjectReference((PetscObject)shellA->axpy));
    shellB->axpy        = shellA->axpy;
    shellB->axpy_vscale = shellA->axpy_vscale;
    shellB->axpy_state  = shellA->axpy_state;
  }
  if (shellA->zrows) {
    PetscCall(ISDuplicate(shellA->zrows, &shellB->zrows));
    if (shellA->zcols) PetscCall(ISDuplicate(shellA->zcols, &shellB->zcols));
    PetscCall(VecDuplicate(shellA->zvals, &shellB->zvals));
    PetscCall(VecCopy(shellA->zvals, shellB->zvals));
    PetscCall(VecDuplicate(shellA->zvals_w, &shellB->zvals_w));
    PetscCall(PetscObjectReference((PetscObject)shellA->zvals_sct_r));
    PetscCall(PetscObjectReference((PetscObject)shellA->zvals_sct_c));
    shellB->zvals_sct_r = shellA->zvals_sct_r;
    shellB->zvals_sct_c = shellA->zvals_sct_c;
  }

  matmatA = shellA->matmat;
  if (matmatA) {
    while (matmatA->next) {
      PetscCall(MatShellSetMatProductOperation_Private(B, matmatA->ptype, matmatA->symbolic, matmatA->numeric, matmatA->destroy, matmatA->composedname, matmatA->resultname));
      matmatA = matmatA->next;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatDuplicate_Shell(Mat mat, MatDuplicateOption op, Mat *M)
{
  PetscFunctionBegin;
  PetscCall(MatCreateShell(PetscObjectComm((PetscObject)mat), mat->rmap->n, mat->cmap->n, mat->rmap->N, mat->cmap->N, NULL, M));
  ((Mat_Shell *)(*M)->data)->ctxcontainer = ((Mat_Shell *)mat->data)->ctxcontainer;
  PetscCall(PetscObjectCompose((PetscObject)(*M), "MatShell ctx", (PetscObject)((Mat_Shell *)(*M)->data)->ctxcontainer));
  PetscCall(PetscObjectChangeTypeName((PetscObject)(*M), ((PetscObject)mat)->type_name));
  if (op != MAT_DO_NOT_COPY_VALUES) PetscCall(MatCopy(mat, *M, SAME_NONZERO_PATTERN));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatMult_Shell(Mat A, Vec x, Vec y)
{
  Mat_Shell       *shell = (Mat_Shell *)A->data;
  Vec              xx;
  PetscObjectState instate, outstate;

  PetscFunctionBegin;
  PetscCall(MatShellPreZeroRight(A, x, &xx));
  PetscCall(MatShellPreScaleRight(A, xx, &xx));
  PetscCall(PetscObjectStateGet((PetscObject)y, &instate));
  PetscCall((*shell->ops->mult)(A, xx, y));
  PetscCall(PetscObjectStateGet((PetscObject)y, &outstate));
  if (instate == outstate) {
    /* increase the state of the output vector since the user did not update its state themself as should have been done */
    PetscCall(PetscObjectStateIncrease((PetscObject)y));
  }
  PetscCall(MatShellShiftAndScale(A, xx, y));
  PetscCall(MatShellPostScaleLeft(A, y));
  PetscCall(MatShellPostZeroLeft(A, y));

  if (shell->axpy) {
    Mat              X;
    PetscObjectState axpy_state;

    PetscCall(MatShellGetContext(shell->axpy, &X));
    PetscCall(PetscObjectStateGet((PetscObject)X, &axpy_state));
    PetscCheck(shell->axpy_state == axpy_state, PetscObjectComm((PetscObject)A), PETSC_ERR_ORDER, "Invalid AXPY state: cannot modify the X matrix passed to MatAXPY(Y,a,X,...)");

    PetscCall(MatCreateVecs(shell->axpy, shell->axpy_right ? NULL : &shell->axpy_right, shell->axpy_left ? NULL : &shell->axpy_left));
    PetscCall(VecCopy(x, shell->axpy_right));
    PetscCall(MatMult(shell->axpy, shell->axpy_right, shell->axpy_left));
    PetscCall(VecAXPY(y, shell->axpy_vscale, shell->axpy_left));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMultAdd_Shell(Mat A, Vec x, Vec y, Vec z)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (y == z) {
    if (!shell->right_add_work) PetscCall(VecDuplicate(z, &shell->right_add_work));
    PetscCall(MatMult(A, x, shell->right_add_work));
    PetscCall(VecAXPY(z, 1.0, shell->right_add_work));
  } else {
    PetscCall(MatMult(A, x, z));
    PetscCall(VecAXPY(z, 1.0, y));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMultTranspose_Shell(Mat A, Vec x, Vec y)
{
  Mat_Shell       *shell = (Mat_Shell *)A->data;
  Vec              xx;
  PetscObjectState instate, outstate;

  PetscFunctionBegin;
  PetscCall(MatShellPreZeroLeft(A, x, &xx));
  PetscCall(MatShellPreScaleLeft(A, xx, &xx));
  PetscCall(PetscObjectStateGet((PetscObject)y, &instate));
  PetscCall((*shell->ops->multtranspose)(A, xx, y));
  PetscCall(PetscObjectStateGet((PetscObject)y, &outstate));
  if (instate == outstate) {
    /* increase the state of the output vector since the user did not update its state themself as should have been done */
    PetscCall(PetscObjectStateIncrease((PetscObject)y));
  }
  PetscCall(MatShellShiftAndScale(A, xx, y));
  PetscCall(MatShellPostScaleRight(A, y));
  PetscCall(MatShellPostZeroRight(A, y));

  if (shell->axpy) {
    Mat              X;
    PetscObjectState axpy_state;

    PetscCall(MatShellGetContext(shell->axpy, &X));
    PetscCall(PetscObjectStateGet((PetscObject)X, &axpy_state));
    PetscCheck(shell->axpy_state == axpy_state, PetscObjectComm((PetscObject)A), PETSC_ERR_ORDER, "Invalid AXPY state: cannot modify the X matrix passed to MatAXPY(Y,a,X,...)");
    PetscCall(MatCreateVecs(shell->axpy, shell->axpy_right ? NULL : &shell->axpy_right, shell->axpy_left ? NULL : &shell->axpy_left));
    PetscCall(VecCopy(x, shell->axpy_left));
    PetscCall(MatMultTranspose(shell->axpy, shell->axpy_left, shell->axpy_right));
    PetscCall(VecAXPY(y, shell->axpy_vscale, shell->axpy_right));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMultTransposeAdd_Shell(Mat A, Vec x, Vec y, Vec z)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (y == z) {
    if (!shell->left_add_work) PetscCall(VecDuplicate(z, &shell->left_add_work));
    PetscCall(MatMultTranspose(A, x, shell->left_add_work));
    PetscCall(VecAXPY(z, 1.0, shell->left_add_work));
  } else {
    PetscCall(MatMultTranspose(A, x, z));
    PetscCall(VecAXPY(z, 1.0, y));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
          diag(left)(vscale*A + diag(dshift) + vshift I)diag(right)
*/
static PetscErrorCode MatGetDiagonal_Shell(Mat A, Vec v)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (shell->ops->getdiagonal) {
    PetscCall((*shell->ops->getdiagonal)(A, v));
  } else SETERRQ(PetscObjectComm((PetscObject)A), PETSC_ERR_ARG_WRONGSTATE, "Must provide shell matrix with routine to return diagonal using\nMatShellSetOperation(S,MATOP_GET_DIAGONAL,...)");
  PetscCall(VecScale(v, shell->vscale));
  if (shell->dshift) PetscCall(VecAXPY(v, 1.0, shell->dshift));
  PetscCall(VecShift(v, shell->vshift));
  if (shell->left) PetscCall(VecPointwiseMult(v, v, shell->left));
  if (shell->right) PetscCall(VecPointwiseMult(v, v, shell->right));
  if (shell->zrows) {
    PetscCall(VecScatterBegin(shell->zvals_sct_r, shell->zvals, v, INSERT_VALUES, SCATTER_REVERSE));
    PetscCall(VecScatterEnd(shell->zvals_sct_r, shell->zvals, v, INSERT_VALUES, SCATTER_REVERSE));
  }
  if (shell->axpy) {
    Mat              X;
    PetscObjectState axpy_state;

    PetscCall(MatShellGetContext(shell->axpy, &X));
    PetscCall(PetscObjectStateGet((PetscObject)X, &axpy_state));
    PetscCheck(shell->axpy_state == axpy_state, PetscObjectComm((PetscObject)A), PETSC_ERR_ORDER, "Invalid AXPY state: cannot modify the X matrix passed to MatAXPY(Y,a,X,...)");
    PetscCall(MatCreateVecs(shell->axpy, NULL, shell->axpy_left ? NULL : &shell->axpy_left));
    PetscCall(MatGetDiagonal(shell->axpy, shell->axpy_left));
    PetscCall(VecAXPY(v, shell->axpy_vscale, shell->axpy_left));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatShift_Shell(Mat Y, PetscScalar a)
{
  Mat_Shell *shell = (Mat_Shell *)Y->data;
  PetscBool  flg;

  PetscFunctionBegin;
  PetscCall(MatHasCongruentLayouts(Y, &flg));
  PetscCheck(flg, PetscObjectComm((PetscObject)Y), PETSC_ERR_SUP, "Cannot shift shell matrix if it is not congruent");
  if (shell->left || shell->right) {
    if (!shell->dshift) {
      PetscCall(VecDuplicate(shell->left ? shell->left : shell->right, &shell->dshift));
      PetscCall(VecSet(shell->dshift, a));
    } else {
      if (shell->left) PetscCall(VecPointwiseMult(shell->dshift, shell->dshift, shell->left));
      if (shell->right) PetscCall(VecPointwiseMult(shell->dshift, shell->dshift, shell->right));
      PetscCall(VecShift(shell->dshift, a));
    }
    if (shell->left) PetscCall(VecPointwiseDivide(shell->dshift, shell->dshift, shell->left));
    if (shell->right) PetscCall(VecPointwiseDivide(shell->dshift, shell->dshift, shell->right));
  } else shell->vshift += a;
  if (shell->zrows) PetscCall(VecShift(shell->zvals, a));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatDiagonalSet_Shell_Private(Mat A, Vec D, PetscScalar s)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  if (!shell->dshift) PetscCall(VecDuplicate(D, &shell->dshift));
  if (shell->left || shell->right) {
    if (!shell->right_work) PetscCall(VecDuplicate(shell->left ? shell->left : shell->right, &shell->right_work));
    if (shell->left && shell->right) {
      PetscCall(VecPointwiseDivide(shell->right_work, D, shell->left));
      PetscCall(VecPointwiseDivide(shell->right_work, shell->right_work, shell->right));
    } else if (shell->left) {
      PetscCall(VecPointwiseDivide(shell->right_work, D, shell->left));
    } else {
      PetscCall(VecPointwiseDivide(shell->right_work, D, shell->right));
    }
    PetscCall(VecAXPY(shell->dshift, s, shell->right_work));
  } else {
    PetscCall(VecAXPY(shell->dshift, s, D));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatDiagonalSet_Shell(Mat A, Vec D, InsertMode ins)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;
  Vec        d;
  PetscBool  flg;

  PetscFunctionBegin;
  PetscCall(MatHasCongruentLayouts(A, &flg));
  PetscCheck(flg, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "Cannot diagonal set or shift shell matrix if it is not congruent");
  if (ins == INSERT_VALUES) {
    PetscCall(VecDuplicate(D, &d));
    PetscCall(MatGetDiagonal(A, d));
    PetscCall(MatDiagonalSet_Shell_Private(A, d, -1.));
    PetscCall(MatDiagonalSet_Shell_Private(A, D, 1.));
    PetscCall(VecDestroy(&d));
    if (shell->zrows) PetscCall(VecCopy(D, shell->zvals));
  } else {
    PetscCall(MatDiagonalSet_Shell_Private(A, D, 1.));
    if (shell->zrows) PetscCall(VecAXPY(shell->zvals, 1.0, D));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatScale_Shell(Mat Y, PetscScalar a)
{
  Mat_Shell *shell = (Mat_Shell *)Y->data;

  PetscFunctionBegin;
  shell->vscale *= a;
  shell->vshift *= a;
  if (shell->dshift) PetscCall(VecScale(shell->dshift, a));
  shell->axpy_vscale *= a;
  if (shell->zrows) PetscCall(VecScale(shell->zvals, a));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatDiagonalScale_Shell(Mat Y, Vec left, Vec right)
{
  Mat_Shell *shell = (Mat_Shell *)Y->data;

  PetscFunctionBegin;
  if (left) {
    if (!shell->left) {
      PetscCall(VecDuplicate(left, &shell->left));
      PetscCall(VecCopy(left, shell->left));
    } else {
      PetscCall(VecPointwiseMult(shell->left, shell->left, left));
    }
    if (shell->zrows) PetscCall(VecPointwiseMult(shell->zvals, shell->zvals, left));
  }
  if (right) {
    if (!shell->right) {
      PetscCall(VecDuplicate(right, &shell->right));
      PetscCall(VecCopy(right, shell->right));
    } else {
      PetscCall(VecPointwiseMult(shell->right, shell->right, right));
    }
    if (shell->zrows) {
      if (!shell->left_work) PetscCall(MatCreateVecs(Y, NULL, &shell->left_work));
      PetscCall(VecSet(shell->zvals_w, 1.0));
      PetscCall(VecScatterBegin(shell->zvals_sct_c, right, shell->zvals_w, INSERT_VALUES, SCATTER_FORWARD));
      PetscCall(VecScatterEnd(shell->zvals_sct_c, right, shell->zvals_w, INSERT_VALUES, SCATTER_FORWARD));
      PetscCall(VecPointwiseMult(shell->zvals, shell->zvals, shell->zvals_w));
    }
  }
  if (shell->axpy) PetscCall(MatDiagonalScale(shell->axpy, left, right));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatAssemblyEnd_Shell(Mat Y, MatAssemblyType t)
{
  Mat_Shell *shell = (Mat_Shell *)Y->data;

  PetscFunctionBegin;
  if (t == MAT_FINAL_ASSEMBLY) {
    shell->vshift      = 0.0;
    shell->vscale      = 1.0;
    shell->axpy_vscale = 0.0;
    shell->axpy_state  = 0;
    PetscCall(VecDestroy(&shell->dshift));
    PetscCall(VecDestroy(&shell->left));
    PetscCall(VecDestroy(&shell->right));
    PetscCall(MatDestroy(&shell->axpy));
    PetscCall(VecDestroy(&shell->axpy_left));
    PetscCall(VecDestroy(&shell->axpy_right));
    PetscCall(VecScatterDestroy(&shell->zvals_sct_c));
    PetscCall(VecScatterDestroy(&shell->zvals_sct_r));
    PetscCall(ISDestroy(&shell->zrows));
    PetscCall(ISDestroy(&shell->zcols));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMissingDiagonal_Shell(Mat A, PetscBool *missing, PetscInt *d)
{
  PetscFunctionBegin;
  *missing = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatAXPY_Shell(Mat Y, PetscScalar a, Mat X, MatStructure str)
{
  Mat_Shell *shell = (Mat_Shell *)Y->data;

  PetscFunctionBegin;
  if (X == Y) {
    PetscCall(MatScale(Y, 1.0 + a));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (!shell->axpy) {
    PetscCall(MatConvertFrom_Shell(X, MATSHELL, MAT_INITIAL_MATRIX, &shell->axpy));
    shell->axpy_vscale = a;
    PetscCall(PetscObjectStateGet((PetscObject)X, &shell->axpy_state));
  } else {
    PetscCall(MatAXPY(shell->axpy, a / shell->axpy_vscale, X, str));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static struct _MatOps MatOps_Values = {NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /* 4*/ MatMultAdd_Shell,
                                       NULL,
                                       MatMultTransposeAdd_Shell,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*10*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*15*/ NULL,
                                       NULL,
                                       NULL,
                                       MatDiagonalScale_Shell,
                                       NULL,
                                       /*20*/ NULL,
                                       MatAssemblyEnd_Shell,
                                       NULL,
                                       NULL,
                                       /*24*/ MatZeroRows_Shell,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*29*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*34*/ MatDuplicate_Shell,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*39*/ MatAXPY_Shell,
                                       NULL,
                                       NULL,
                                       NULL,
                                       MatCopy_Shell,
                                       /*44*/ NULL,
                                       MatScale_Shell,
                                       MatShift_Shell,
                                       MatDiagonalSet_Shell,
                                       MatZeroRowsColumns_Shell,
                                       /*49*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*54*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*59*/ NULL,
                                       MatDestroy_Shell,
                                       NULL,
                                       MatConvertFrom_Shell,
                                       NULL,
                                       /*64*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*69*/ NULL,
                                       NULL,
                                       MatConvert_Shell,
                                       NULL,
                                       NULL,
                                       /*74*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*79*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*84*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*89*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*94*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*99*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*104*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*109*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       MatMissingDiagonal_Shell,
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
                                       /*139*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*144*/ NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       /*150*/ NULL,
                                       NULL};

static PetscErrorCode MatShellSetContext_Shell(Mat mat, void *ctx)
{
  Mat_Shell *shell = (Mat_Shell *)mat->data;

  PetscFunctionBegin;
  if (ctx) {
    PetscContainer ctxcontainer;
    PetscCall(PetscContainerCreate(PetscObjectComm((PetscObject)mat), &ctxcontainer));
    PetscCall(PetscContainerSetPointer(ctxcontainer, ctx));
    PetscCall(PetscObjectCompose((PetscObject)mat, "MatShell ctx", (PetscObject)ctxcontainer));
    shell->ctxcontainer = ctxcontainer;
    PetscCall(PetscContainerDestroy(&ctxcontainer));
  } else {
    PetscCall(PetscObjectCompose((PetscObject)mat, "MatShell ctx", NULL));
    shell->ctxcontainer = NULL;
  }

  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatShellSetContextDestroy_Shell(Mat mat, PetscErrorCode (*f)(void *))
{
  Mat_Shell *shell = (Mat_Shell *)mat->data;

  PetscFunctionBegin;
  if (shell->ctxcontainer) PetscCall(PetscContainerSetUserDestroy(shell->ctxcontainer, f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatShellSetVecType_Shell(Mat mat, VecType vtype)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(mat->defaultvectype));
  PetscCall(PetscStrallocpy(vtype, (char **)&mat->defaultvectype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatShellSetManageScalingShifts_Shell(Mat A)
{
  Mat_Shell *shell = (Mat_Shell *)A->data;

  PetscFunctionBegin;
  shell->managescalingshifts = PETSC_FALSE;
  A->ops->diagonalset        = NULL;
  A->ops->diagonalscale      = NULL;
  A->ops->scale              = NULL;
  A->ops->shift              = NULL;
  A->ops->axpy               = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatShellSetOperation_Shell(Mat mat, MatOperation op, void (*f)(void))
{
  Mat_Shell *shell = (Mat_Shell *)mat->data;

  PetscFunctionBegin;
  switch (op) {
  case MATOP_DESTROY:
    shell->ops->destroy = (PetscErrorCode(*)(Mat))f;
    break;
  case MATOP_VIEW:
    if (!mat->ops->viewnative) mat->ops->viewnative = mat->ops->view;
    mat->ops->view = (PetscErrorCode(*)(Mat, PetscViewer))f;
    break;
  case MATOP_COPY:
    shell->ops->copy = (PetscErrorCode(*)(Mat, Mat, MatStructure))f;
    break;
  case MATOP_DIAGONAL_SET:
  case MATOP_DIAGONAL_SCALE:
  case MATOP_SHIFT:
  case MATOP_SCALE:
  case MATOP_AXPY:
  case MATOP_ZERO_ROWS:
  case MATOP_ZERO_ROWS_COLUMNS:
    PetscCheck(!shell->managescalingshifts, PetscObjectComm((PetscObject)mat), PETSC_ERR_ARG_WRONGSTATE, "MATSHELL is managing scalings and shifts, see MatShellSetManageScalingShifts()");
    (((void (**)(void))mat->ops)[op]) = f;
    break;
  case MATOP_GET_DIAGONAL:
    if (shell->managescalingshifts) {
      shell->ops->getdiagonal = (PetscErrorCode(*)(Mat, Vec))f;
      mat->ops->getdiagonal   = MatGetDiagonal_Shell;
    } else {
      shell->ops->getdiagonal = NULL;
      mat->ops->getdiagonal   = (PetscErrorCode(*)(Mat, Vec))f;
    }
    break;
  case MATOP_MULT:
    if (shell->managescalingshifts) {
      shell->ops->mult = (PetscErrorCode(*)(Mat, Vec, Vec))f;
      mat->ops->mult   = MatMult_Shell;
    } else {
      shell->ops->mult = NULL;
      mat->ops->mult   = (PetscErrorCode(*)(Mat, Vec, Vec))f;
    }
    break;
  case MATOP_MULT_TRANSPOSE:
    if (shell->managescalingshifts) {
      shell->ops->multtranspose = (PetscErrorCode(*)(Mat, Vec, Vec))f;
      mat->ops->multtranspose   = MatMultTranspose_Shell;
    } else {
      shell->ops->multtranspose = NULL;
      mat->ops->multtranspose   = (PetscErrorCode(*)(Mat, Vec, Vec))f;
    }
    break;
  default:
    (((void (**)(void))mat->ops)[op]) = f;
    break;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatShellGetOperation_Shell(Mat mat, MatOperation op, void (**f)(void))
{
  Mat_Shell *shell = (Mat_Shell *)mat->data;

  PetscFunctionBegin;
  switch (op) {
  case MATOP_DESTROY:
    *f = (void (*)(void))shell->ops->destroy;
    break;
  case MATOP_VIEW:
    *f = (void (*)(void))mat->ops->view;
    break;
  case MATOP_COPY:
    *f = (void (*)(void))shell->ops->copy;
    break;
  case MATOP_DIAGONAL_SET:
  case MATOP_DIAGONAL_SCALE:
  case MATOP_SHIFT:
  case MATOP_SCALE:
  case MATOP_AXPY:
  case MATOP_ZERO_ROWS:
  case MATOP_ZERO_ROWS_COLUMNS:
    *f = (((void (**)(void))mat->ops)[op]);
    break;
  case MATOP_GET_DIAGONAL:
    if (shell->ops->getdiagonal) *f = (void (*)(void))shell->ops->getdiagonal;
    else *f = (((void (**)(void))mat->ops)[op]);
    break;
  case MATOP_MULT:
    if (shell->ops->mult) *f = (void (*)(void))shell->ops->mult;
    else *f = (((void (**)(void))mat->ops)[op]);
    break;
  case MATOP_MULT_TRANSPOSE:
    if (shell->ops->multtranspose) *f = (void (*)(void))shell->ops->multtranspose;
    else *f = (((void (**)(void))mat->ops)[op]);
    break;
  default:
    *f = (((void (**)(void))mat->ops)[op]);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   MATSHELL - MATSHELL = "shell" - A matrix type to be used to define your own matrix type -- perhaps matrix free.

  Level: advanced

.seealso: [](chapter_matrices), `Mat`, `MatCreateShell()`
M*/

PETSC_EXTERN PetscErrorCode MatCreate_Shell(Mat A)
{
  Mat_Shell *b;

  PetscFunctionBegin;
  PetscCall(PetscMemcpy(A->ops, &MatOps_Values, sizeof(struct _MatOps)));

  PetscCall(PetscNew(&b));
  A->data = (void *)b;

  b->ctxcontainer        = NULL;
  b->vshift              = 0.0;
  b->vscale              = 1.0;
  b->managescalingshifts = PETSC_TRUE;
  A->assembled           = PETSC_TRUE;
  A->preallocated        = PETSC_FALSE;

  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatShellGetContext_C", MatShellGetContext_Shell));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatShellSetContext_C", MatShellSetContext_Shell));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatShellSetContextDestroy_C", MatShellSetContextDestroy_Shell));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatShellSetVecType_C", MatShellSetVecType_Shell));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatShellSetManageScalingShifts_C", MatShellSetManageScalingShifts_Shell));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatShellSetOperation_C", MatShellSetOperation_Shell));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatShellGetOperation_C", MatShellGetOperation_Shell));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatShellSetMatProductOperation_C", MatShellSetMatProductOperation_Shell));
  PetscCall(PetscObjectChangeTypeName((PetscObject)A, MATSHELL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatCreateShell - Creates a new matrix of `MatType` `MATSHELL` for use with a user-defined
   private data storage format.

  Collective

   Input Parameters:
+  comm - MPI communicator
.  m - number of local rows (must be given)
.  n - number of local columns (must be given)
.  M - number of global rows (may be `PETSC_DETERMINE`)
.  N - number of global columns (may be `PETSC_DETERMINE`)
-  ctx - pointer to data needed by the shell matrix routines

   Output Parameter:
.  A - the matrix

   Level: advanced

  Usage:
.vb
    extern PetscErrorCode mult(Mat,Vec,Vec);
    MatCreateShell(comm,m,n,M,N,ctx,&mat);
    MatShellSetOperation(mat,MATOP_MULT,(void(*)(void))mult);
    [ Use matrix for operations that have been set ]
    MatDestroy(mat);
.ve

   Notes:
   The shell matrix type is intended to provide a simple class to use
   with `KSP` (such as, for use with matrix-free methods). You should not
   use the shell type if you plan to define a complete matrix class.

   PETSc requires that matrices and vectors being used for certain
   operations are partitioned accordingly.  For example, when
   creating a shell matrix, `A`, that supports parallel matrix-vector
   products using `MatMult`(A,x,y) the user should set the number
   of local matrix rows to be the number of local elements of the
   corresponding result vector, y. Note that this is information is
   required for use of the matrix interface routines, even though
   the shell matrix may not actually be physically partitioned.
   For example,

.vb
     Vec x, y
     extern PetscErrorCode mult(Mat,Vec,Vec);
     Mat A

     VecCreateMPI(comm,PETSC_DECIDE,M,&y);
     VecCreateMPI(comm,PETSC_DECIDE,N,&x);
     VecGetLocalSize(y,&m);
     VecGetLocalSize(x,&n);
     MatCreateShell(comm,m,n,M,N,ctx,&A);
     MatShellSetOperation(mat,MATOP_MULT,(void(*)(void))mult);
     MatMult(A,x,y);
     MatDestroy(&A);
     VecDestroy(&y);
     VecDestroy(&x);
.ve

   `MATSHELL` handles `MatShift()`, `MatDiagonalSet()`, `MatDiagonalScale()`, `MatAXPY()`, `MatScale()`, `MatZeroRows()` and `MatZeroRowsColumns()`
   internally, so these  operations cannot be overwritten unless `MatShellSetManageScalingShifts()` is called.

    Developer Notes:
    For rectangular matrices do all the scalings and shifts make sense?

    Regarding shifting and scaling. The general form is

          diag(left)(vscale*A + diag(dshift) + vshift I)diag(right)

      The order you apply the operations is important. For example if you have a dshift then
      apply a MatScale(s) you get s*vscale*A + s*diag(shift). But if you first scale and then shift
      you get s*vscale*A + diag(shift)

          A is the user provided function.

   `KSP`/`PC` uses changes in the `Mat`'s "state" to decide if preconditioners need to be rebuilt `PCSetUp()` only calls the setup() for
   for the `PC` implementation if the `Mat` state has increased from the previous call. Thus to get changes in a `MATSHELL` to trigger
   an update in the preconditioner you must call `MatAssemblyBegin()` and `MatAssemblyEnd()` or `PetscObjectStateIncrease`((`PetscObject`)mat);
   each time the `MATSHELL` matrix has changed.

   Matrix product operations (i.e. `MatMat()`, `MatTransposeMat()` etc) can be specified using `MatShellSetMatProductOperation()`

   Calling `MatAssemblyBegin()`/`MatAssemblyEnd()` on a `MATSHELL` removes any previously supplied shift and scales that were provided
   with `MatDiagonalSet()`, `MatShift()`, `MatScale()`, or `MatDiagonalScale()`.

   Fortran Note:
    To use this from Fortran with a `ctx` you must write an interface definition for this
    function and for `MatShellGetContext()` that tells Fortran the Fortran derived data type you are passing
    in as the `ctx` argument.

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatShellSetOperation()`, `MatHasOperation()`, `MatShellGetContext()`, `MatShellSetContext()`, `MATSHELL`, `MatShellSetManageScalingShifts()`, `MatShellSetMatProductOperation()`
@*/
PetscErrorCode MatCreateShell(MPI_Comm comm, PetscInt m, PetscInt n, PetscInt M, PetscInt N, void *ctx, Mat *A)
{
  PetscFunctionBegin;
  PetscCall(MatCreate(comm, A));
  PetscCall(MatSetSizes(*A, m, n, M, N));
  PetscCall(MatSetType(*A, MATSHELL));
  PetscCall(MatShellSetContext(*A, ctx));
  PetscCall(MatSetUp(*A));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    MatShellSetContext - sets the context for a `MATSHELL` shell matrix

   Logically Collective

    Input Parameters:
+   mat - the `MATSHELL` shell matrix
-   ctx - the context

   Level: advanced

   Fortran Note:
    You must write a Fortran interface definition for this
    function that tells Fortran the Fortran derived data type that you are passing in as the `ctx` argument.

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellGetContext()`, `MatShellGetOperation()`
@*/
PetscErrorCode MatShellSetContext(Mat mat, void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscTryMethod(mat, "MatShellSetContext_C", (Mat, void *), (mat, ctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    MatShellSetContextDestroy - sets the destroy function for a `MATSHELL` shell matrix context

   Logically Collective

    Input Parameters:
+   mat - the shell matrix
-   f - the context destroy function

   Level: advanced

    Note:
    If the `MatShell` is never duplicated, the behavior of this function is equivalent
    to `MatShellSetOperation`(`Mat`,`MATOP_DESTROY`,f). However, `MatShellSetContextDestroy()`
    ensures proper reference counting for the user provided context data in the case that
    the `MATSHELL` is duplicated.

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellSetContext()`
@*/
PetscErrorCode MatShellSetContextDestroy(Mat mat, PetscErrorCode (*f)(void *))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscTryMethod(mat, "MatShellSetContextDestroy_C", (Mat, PetscErrorCode(*)(void *)), (mat, f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 MatShellSetVecType - Sets the `VecType` of `Vec` returned by `MatCreateVecs()`

 Logically Collective

    Input Parameters:
+   mat   - the `MATSHELL` shell matrix
-   vtype - type to use for creating vectors

 Level: advanced

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateVecs()`
@*/
PetscErrorCode MatShellSetVecType(Mat mat, VecType vtype)
{
  PetscFunctionBegin;
  PetscTryMethod(mat, "MatShellSetVecType_C", (Mat, VecType), (mat, vtype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    MatShellSetManageScalingShifts - Allows the user to control the scaling and shift operations of the `MATSHELL`. Must be called immediately
          after `MatCreateShell()`

   Logically Collective

    Input Parameter:
.   mat - the `MATSHELL` shell matrix

  Level: advanced

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellGetContext()`, `MatShellGetOperation()`, `MatShellSetContext()`, `MatShellSetOperation()`
@*/
PetscErrorCode MatShellSetManageScalingShifts(Mat A)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscTryMethod(A, "MatShellSetManageScalingShifts_C", (Mat), (A));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    MatShellTestMult - Compares the multiply routine provided to the `MATSHELL` with differencing on a given function.

   Logically Collective; No Fortran Support

    Input Parameters:
+   mat - the `MATSHELL` shell matrix
.   f - the function
.   base - differences are computed around this vector, see `MatMFFDSetBase()`, for Jacobians this is the point at which the Jacobian is being evaluated
-   ctx - an optional context for the function

   Output Parameter:
.   flg - `PETSC_TRUE` if the multiply is likely correct

   Options Database Key:
.   -mat_shell_test_mult_view - print if any differences are detected between the products and print the difference

   Level: advanced

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellGetContext()`, `MatShellGetOperation()`, `MatShellTestMultTranspose()`
@*/
PetscErrorCode MatShellTestMult(Mat mat, PetscErrorCode (*f)(void *, Vec, Vec), Vec base, void *ctx, PetscBool *flg)
{
  PetscInt  m, n;
  Mat       mf, Dmf, Dmat, Ddiff;
  PetscReal Diffnorm, Dmfnorm;
  PetscBool v = PETSC_FALSE, flag = PETSC_TRUE;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscCall(PetscOptionsHasName(NULL, ((PetscObject)mat)->prefix, "-mat_shell_test_mult_view", &v));
  PetscCall(MatGetLocalSize(mat, &m, &n));
  PetscCall(MatCreateMFFD(PetscObjectComm((PetscObject)mat), m, n, PETSC_DECIDE, PETSC_DECIDE, &mf));
  PetscCall(MatMFFDSetFunction(mf, f, ctx));
  PetscCall(MatMFFDSetBase(mf, base, NULL));

  PetscCall(MatComputeOperator(mf, MATAIJ, &Dmf));
  PetscCall(MatComputeOperator(mat, MATAIJ, &Dmat));

  PetscCall(MatDuplicate(Dmat, MAT_COPY_VALUES, &Ddiff));
  PetscCall(MatAXPY(Ddiff, -1.0, Dmf, DIFFERENT_NONZERO_PATTERN));
  PetscCall(MatNorm(Ddiff, NORM_FROBENIUS, &Diffnorm));
  PetscCall(MatNorm(Dmf, NORM_FROBENIUS, &Dmfnorm));
  if (Diffnorm / Dmfnorm > 10 * PETSC_SQRT_MACHINE_EPSILON) {
    flag = PETSC_FALSE;
    if (v) {
      PetscCall(PetscPrintf(PetscObjectComm((PetscObject)mat), "MATSHELL and matrix free multiple appear to produce different results.\n Norm Ratio %g Difference results followed by finite difference one\n", (double)(Diffnorm / Dmfnorm)));
      PetscCall(MatViewFromOptions(Ddiff, (PetscObject)mat, "-mat_shell_test_mult_view"));
      PetscCall(MatViewFromOptions(Dmf, (PetscObject)mat, "-mat_shell_test_mult_view"));
      PetscCall(MatViewFromOptions(Dmat, (PetscObject)mat, "-mat_shell_test_mult_view"));
    }
  } else if (v) {
    PetscCall(PetscPrintf(PetscObjectComm((PetscObject)mat), "MATSHELL and matrix free multiple appear to produce the same results\n"));
  }
  if (flg) *flg = flag;
  PetscCall(MatDestroy(&Ddiff));
  PetscCall(MatDestroy(&mf));
  PetscCall(MatDestroy(&Dmf));
  PetscCall(MatDestroy(&Dmat));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    MatShellTestMultTranspose - Compares the multiply transpose routine provided to the `MATSHELL` with differencing on a given function.

   Logically Collective; No Fortran Support

    Input Parameters:
+   mat - the `MATSHELL` shell matrix
.   f - the function
.   base - differences are computed around this vector, see `MatMFFDSetBase()`, for Jacobians this is the point at which the Jacobian is being evaluated
-   ctx - an optional context for the function

   Output Parameter:
.   flg - `PETSC_TRUE` if the multiply is likely correct

   Options Database Key:
.   -mat_shell_test_mult_view - print if any differences are detected between the products and print the difference

   Level: advanced

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellGetContext()`, `MatShellGetOperation()`, `MatShellTestMult()`
@*/
PetscErrorCode MatShellTestMultTranspose(Mat mat, PetscErrorCode (*f)(void *, Vec, Vec), Vec base, void *ctx, PetscBool *flg)
{
  Vec       x, y, z;
  PetscInt  m, n, M, N;
  Mat       mf, Dmf, Dmat, Ddiff;
  PetscReal Diffnorm, Dmfnorm;
  PetscBool v = PETSC_FALSE, flag = PETSC_TRUE;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscCall(PetscOptionsHasName(NULL, ((PetscObject)mat)->prefix, "-mat_shell_test_mult_transpose_view", &v));
  PetscCall(MatCreateVecs(mat, &x, &y));
  PetscCall(VecDuplicate(y, &z));
  PetscCall(MatGetLocalSize(mat, &m, &n));
  PetscCall(MatGetSize(mat, &M, &N));
  PetscCall(MatCreateMFFD(PetscObjectComm((PetscObject)mat), m, n, M, N, &mf));
  PetscCall(MatMFFDSetFunction(mf, f, ctx));
  PetscCall(MatMFFDSetBase(mf, base, NULL));
  PetscCall(MatComputeOperator(mf, MATAIJ, &Dmf));
  PetscCall(MatTranspose(Dmf, MAT_INPLACE_MATRIX, &Dmf));
  PetscCall(MatComputeOperatorTranspose(mat, MATAIJ, &Dmat));

  PetscCall(MatDuplicate(Dmat, MAT_COPY_VALUES, &Ddiff));
  PetscCall(MatAXPY(Ddiff, -1.0, Dmf, DIFFERENT_NONZERO_PATTERN));
  PetscCall(MatNorm(Ddiff, NORM_FROBENIUS, &Diffnorm));
  PetscCall(MatNorm(Dmf, NORM_FROBENIUS, &Dmfnorm));
  if (Diffnorm / Dmfnorm > 10 * PETSC_SQRT_MACHINE_EPSILON) {
    flag = PETSC_FALSE;
    if (v) {
      PetscCall(PetscPrintf(PetscObjectComm((PetscObject)mat), "MATSHELL and matrix free multiple appear to produce different results.\n Norm Ratio %g Difference results followed by finite difference one\n", (double)(Diffnorm / Dmfnorm)));
      PetscCall(MatViewFromOptions(Ddiff, (PetscObject)mat, "-mat_shell_test_mult_transpose_view"));
      PetscCall(MatViewFromOptions(Dmf, (PetscObject)mat, "-mat_shell_test_mult_transpose_view"));
      PetscCall(MatViewFromOptions(Dmat, (PetscObject)mat, "-mat_shell_test_mult_transpose_view"));
    }
  } else if (v) {
    PetscCall(PetscPrintf(PetscObjectComm((PetscObject)mat), "MATSHELL transpose and matrix free multiple appear to produce the same results\n"));
  }
  if (flg) *flg = flag;
  PetscCall(MatDestroy(&mf));
  PetscCall(MatDestroy(&Dmat));
  PetscCall(MatDestroy(&Ddiff));
  PetscCall(MatDestroy(&Dmf));
  PetscCall(VecDestroy(&x));
  PetscCall(VecDestroy(&y));
  PetscCall(VecDestroy(&z));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    MatShellSetOperation - Allows user to set a matrix operation for a `MATSHELL` shell matrix.

   Logically Collective

    Input Parameters:
+   mat - the `MATSHELL` shell matrix
.   op - the name of the operation
-   g - the function that provides the operation.

   Level: advanced

    Usage:
.vb
      extern PetscErrorCode usermult(Mat,Vec,Vec);
      MatCreateShell(comm,m,n,M,N,ctx,&A);
      MatShellSetOperation(A,MATOP_MULT,(void(*)(void))usermult);
.ve

    Notes:
    See the file include/petscmat.h for a complete list of matrix
    operations, which all have the form MATOP_<OPERATION>, where
    <OPERATION> is the name (in all capital letters) of the
    user interface routine (e.g., `MatMult()` -> `MATOP_MULT`).

    All user-provided functions (except for `MATOP_DESTROY`) should have the same calling
    sequence as the usual matrix interface routines, since they
    are intended to be accessed via the usual matrix interface
    routines, e.g.,
$       MatMult(Mat,Vec,Vec) -> usermult(Mat,Vec,Vec)

    In particular each function MUST return an error code of 0 on success and
    nonzero on failure.

    Within each user-defined routine, the user should call
    `MatShellGetContext()` to obtain the user-defined context that was
    set by `MatCreateShell()`.

    Use `MatSetOperation()` to set an operation for any matrix type. For matrix product operations (i.e. `MatMatXXX()`, `MatTransposeMatXXX()` etc)
    use `MatShellSetMatProductOperation()`

    Fortran Note:
    For `MatCreateVecs()` the user code should check if the input left or right matrix is -1 and in that case not
       generate a matrix. See src/mat/tests/ex120f.F

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellGetContext()`, `MatShellGetOperation()`, `MatShellSetContext()`, `MatSetOperation()`, `MatShellSetManageScalingShifts()`, `MatShellSetMatProductOperation()`
@*/
PetscErrorCode MatShellSetOperation(Mat mat, MatOperation op, void (*g)(void))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscTryMethod(mat, "MatShellSetOperation_C", (Mat, MatOperation, void (*)(void)), (mat, op, g));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    MatShellGetOperation - Gets a matrix function for a `MATSHELL` shell matrix.

    Not Collective

    Input Parameters:
+   mat - the `MATSHELL` shell matrix
-   op - the name of the operation

    Output Parameter:
.   g - the function that provides the operation.

    Level: advanced

    Notes:
    See the file include/petscmat.h for a complete list of matrix
    operations, which all have the form MATOP_<OPERATION>, where
    <OPERATION> is the name (in all capital letters) of the
    user interface routine (e.g., `MatMult()` -> `MATOP_MULT`).

    All user-provided functions have the same calling
    sequence as the usual matrix interface routines, since they
    are intended to be accessed via the usual matrix interface
    routines, e.g.,
$       MatMult(Mat,Vec,Vec) -> usermult(Mat,Vec,Vec)

    Within each user-defined routine, the user should call
    `MatShellGetContext()` to obtain the user-defined context that was
    set by `MatCreateShell()`.

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MatCreateShell()`, `MatShellGetContext()`, `MatShellSetOperation()`, `MatShellSetContext()`
@*/
PetscErrorCode MatShellGetOperation(Mat mat, MatOperation op, void (**g)(void))
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscUseMethod(mat, "MatShellGetOperation_C", (Mat, MatOperation, void (**)(void)), (mat, op, g));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    MatIsShell - Inquires if a matrix is derived from `MATSHELL`

    Input Parameter:
.   mat - the matrix

    Output Parameter:
.   flg - the boolean value

    Level: developer

    Developer Note:
    In the future, we should allow the object type name to be changed still using the `MATSHELL` data structure for other matrices
    (i.e. `MATTRANSPOSEVIRTUAL`, `MATSCHURCOMPLEMENT` etc)

.seealso: [](chapter_matrices), `Mat`, `MATSHELL`, `MATMFFD`, `MatCreateShell()`, `MATTRANSPOSEVIRTUAL`, `MATSCHURCOMPLEMENT`
@*/
PetscErrorCode MatIsShell(Mat mat, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(mat, MAT_CLASSID, 1);
  PetscValidBoolPointer(flg, 2);
  *flg = (PetscBool)(mat->ops->destroy == MatDestroy_Shell);
  PetscFunctionReturn(PETSC_SUCCESS);
}

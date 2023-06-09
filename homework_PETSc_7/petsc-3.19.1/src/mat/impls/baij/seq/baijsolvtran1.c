#include <../src/mat/impls/baij/seq/baij.h>
#include <petsc/private/kernels/blockinvert.h>

PetscErrorCode MatSolveTranspose_SeqBAIJ_1(Mat A, Vec bb, Vec xx)
{
  Mat_SeqBAIJ       *a     = (Mat_SeqBAIJ *)A->data;
  IS                 iscol = a->col, isrow = a->row;
  const PetscInt    *rout, *cout, *r, *c, *adiag = a->diag, *ai = a->i, *aj = a->j, *vi;
  PetscInt           i, n = a->mbs, j;
  PetscInt           nz;
  PetscScalar       *x, *tmp, s1;
  const MatScalar   *aa = a->a, *v;
  const PetscScalar *b;

  PetscFunctionBegin;
  PetscCall(VecGetArrayRead(bb, &b));
  PetscCall(VecGetArray(xx, &x));
  tmp = a->solve_work;

  PetscCall(ISGetIndices(isrow, &rout));
  r = rout;
  PetscCall(ISGetIndices(iscol, &cout));
  c = cout;

  /* copy the b into temp work space according to permutation */
  for (i = 0; i < n; i++) tmp[i] = b[c[i]];

  /* forward solve the U^T */
  for (i = 0; i < n; i++) {
    v  = aa + adiag[i + 1] + 1;
    vi = aj + adiag[i + 1] + 1;
    nz = adiag[i] - adiag[i + 1] - 1;
    s1 = tmp[i];
    s1 *= v[nz]; /* multiply by inverse of diagonal entry */
    for (j = 0; j < nz; j++) tmp[vi[j]] -= s1 * v[j];
    tmp[i] = s1;
  }

  /* backward solve the L^T */
  for (i = n - 1; i >= 0; i--) {
    v  = aa + ai[i];
    vi = aj + ai[i];
    nz = ai[i + 1] - ai[i];
    s1 = tmp[i];
    for (j = 0; j < nz; j++) tmp[vi[j]] -= s1 * v[j];
  }

  /* copy tmp into x according to permutation */
  for (i = 0; i < n; i++) x[r[i]] = tmp[i];

  PetscCall(ISRestoreIndices(isrow, &rout));
  PetscCall(ISRestoreIndices(iscol, &cout));
  PetscCall(VecRestoreArrayRead(bb, &b));
  PetscCall(VecRestoreArray(xx, &x));

  PetscCall(PetscLogFlops(2.0 * a->nz - A->cmap->n));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSolveTranspose_SeqBAIJ_1_inplace(Mat A, Vec bb, Vec xx)
{
  Mat_SeqBAIJ       *a     = (Mat_SeqBAIJ *)A->data;
  IS                 iscol = a->col, isrow = a->row;
  const PetscInt    *r, *c, *rout, *cout;
  const PetscInt    *diag = a->diag, n = a->mbs, *vi, *ai = a->i, *aj = a->j;
  PetscInt           i, nz;
  const MatScalar   *aa = a->a, *v;
  PetscScalar        s1, *x, *t;
  const PetscScalar *b;

  PetscFunctionBegin;
  PetscCall(VecGetArrayRead(bb, &b));
  PetscCall(VecGetArray(xx, &x));
  t = a->solve_work;

  PetscCall(ISGetIndices(isrow, &rout));
  r = rout;
  PetscCall(ISGetIndices(iscol, &cout));
  c = cout;

  /* copy the b into temp work space according to permutation */
  for (i = 0; i < n; i++) t[i] = b[c[i]];

  /* forward solve the U^T */
  for (i = 0; i < n; i++) {
    v = aa + diag[i];
    /* multiply by the inverse of the block diagonal */
    s1 = (*v++) * t[i];
    vi = aj + diag[i] + 1;
    nz = ai[i + 1] - diag[i] - 1;
    while (nz--) t[*vi++] -= (*v++) * s1;
    t[i] = s1;
  }
  /* backward solve the L^T */
  for (i = n - 1; i >= 0; i--) {
    v  = aa + diag[i] - 1;
    vi = aj + diag[i] - 1;
    nz = diag[i] - ai[i];
    s1 = t[i];
    while (nz--) t[*vi--] -= (*v--) * s1;
  }

  /* copy t into x according to permutation */
  for (i = 0; i < n; i++) x[r[i]] = t[i];

  PetscCall(ISRestoreIndices(isrow, &rout));
  PetscCall(ISRestoreIndices(iscol, &cout));
  PetscCall(VecRestoreArrayRead(bb, &b));
  PetscCall(VecRestoreArray(xx, &x));
  PetscCall(PetscLogFlops(2.0 * (a->nz) - A->cmap->n));
  PetscFunctionReturn(PETSC_SUCCESS);
}

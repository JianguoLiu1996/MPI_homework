#include <../src/mat/impls/baij/seq/baij.h>
#include <petsc/private/kernels/blockinvert.h>

PetscErrorCode MatSolveTranspose_SeqBAIJ_2_inplace(Mat A, Vec bb, Vec xx)
{
  Mat_SeqBAIJ       *a     = (Mat_SeqBAIJ *)A->data;
  IS                 iscol = a->col, isrow = a->row;
  const PetscInt    *r, *c, *rout, *cout;
  const PetscInt    *diag = a->diag, n = a->mbs, *vi, *ai = a->i, *aj = a->j;
  PetscInt           i, nz, idx, idt, ii, ic, ir, oidx;
  const MatScalar   *aa = a->a, *v;
  PetscScalar        s1, s2, x1, x2, *x, *t;
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
  ii = 0;
  for (i = 0; i < n; i++) {
    ic        = 2 * c[i];
    t[ii]     = b[ic];
    t[ii + 1] = b[ic + 1];
    ii += 2;
  }

  /* forward solve the U^T */
  idx = 0;
  for (i = 0; i < n; i++) {
    v = aa + 4 * diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = t[idx];
    x2 = t[1 + idx];
    s1 = v[0] * x1 + v[1] * x2;
    s2 = v[2] * x1 + v[3] * x2;
    v += 4;

    vi = aj + diag[i] + 1;
    nz = ai[i + 1] - diag[i] - 1;
    while (nz--) {
      oidx = 2 * (*vi++);
      t[oidx] -= v[0] * s1 + v[1] * s2;
      t[oidx + 1] -= v[2] * s1 + v[3] * s2;
      v += 4;
    }
    t[idx]     = s1;
    t[1 + idx] = s2;
    idx += 2;
  }
  /* backward solve the L^T */
  for (i = n - 1; i >= 0; i--) {
    v   = aa + 4 * diag[i] - 4;
    vi  = aj + diag[i] - 1;
    nz  = diag[i] - ai[i];
    idt = 2 * i;
    s1  = t[idt];
    s2  = t[1 + idt];
    while (nz--) {
      idx = 2 * (*vi--);
      t[idx] -= v[0] * s1 + v[1] * s2;
      t[idx + 1] -= v[2] * s1 + v[3] * s2;
      v -= 4;
    }
  }

  /* copy t into x according to permutation */
  ii = 0;
  for (i = 0; i < n; i++) {
    ir        = 2 * r[i];
    x[ir]     = t[ii];
    x[ir + 1] = t[ii + 1];
    ii += 2;
  }

  PetscCall(ISRestoreIndices(isrow, &rout));
  PetscCall(ISRestoreIndices(iscol, &cout));
  PetscCall(VecRestoreArrayRead(bb, &b));
  PetscCall(VecRestoreArray(xx, &x));
  PetscCall(PetscLogFlops(2.0 * 4 * (a->nz) - 2.0 * A->cmap->n));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSolveTranspose_SeqBAIJ_2(Mat A, Vec bb, Vec xx)
{
  Mat_SeqBAIJ       *a     = (Mat_SeqBAIJ *)A->data;
  IS                 iscol = a->col, isrow = a->row;
  const PetscInt     n = a->mbs, *vi, *ai = a->i, *aj = a->j, *diag = a->diag;
  const PetscInt    *r, *c, *rout, *cout;
  PetscInt           nz, idx, idt, j, i, oidx, ii, ic, ir;
  const PetscInt     bs = A->rmap->bs, bs2 = a->bs2;
  const MatScalar   *aa = a->a, *v;
  PetscScalar        s1, s2, x1, x2, *x, *t;
  const PetscScalar *b;

  PetscFunctionBegin;
  PetscCall(VecGetArrayRead(bb, &b));
  PetscCall(VecGetArray(xx, &x));
  t = a->solve_work;

  PetscCall(ISGetIndices(isrow, &rout));
  r = rout;
  PetscCall(ISGetIndices(iscol, &cout));
  c = cout;

  /* copy b into temp work space according to permutation */
  for (i = 0; i < n; i++) {
    ii        = bs * i;
    ic        = bs * c[i];
    t[ii]     = b[ic];
    t[ii + 1] = b[ic + 1];
  }

  /* forward solve the U^T */
  idx = 0;
  for (i = 0; i < n; i++) {
    v = aa + bs2 * diag[i];
    /* multiply by the inverse of the block diagonal */
    x1 = t[idx];
    x2 = t[1 + idx];
    s1 = v[0] * x1 + v[1] * x2;
    s2 = v[2] * x1 + v[3] * x2;
    v -= bs2;

    vi = aj + diag[i] - 1;
    nz = diag[i] - diag[i + 1] - 1;
    for (j = 0; j > -nz; j--) {
      oidx = bs * vi[j];
      t[oidx] -= v[0] * s1 + v[1] * s2;
      t[oidx + 1] -= v[2] * s1 + v[3] * s2;
      v -= bs2;
    }
    t[idx]     = s1;
    t[1 + idx] = s2;
    idx += bs;
  }
  /* backward solve the L^T */
  for (i = n - 1; i >= 0; i--) {
    v   = aa + bs2 * ai[i];
    vi  = aj + ai[i];
    nz  = ai[i + 1] - ai[i];
    idt = bs * i;
    s1  = t[idt];
    s2  = t[1 + idt];
    for (j = 0; j < nz; j++) {
      idx = bs * vi[j];
      t[idx] -= v[0] * s1 + v[1] * s2;
      t[idx + 1] -= v[2] * s1 + v[3] * s2;
      v += bs2;
    }
  }

  /* copy t into x according to permutation */
  for (i = 0; i < n; i++) {
    ii        = bs * i;
    ir        = bs * r[i];
    x[ir]     = t[ii];
    x[ir + 1] = t[ii + 1];
  }

  PetscCall(ISRestoreIndices(isrow, &rout));
  PetscCall(ISRestoreIndices(iscol, &cout));
  PetscCall(VecRestoreArrayRead(bb, &b));
  PetscCall(VecRestoreArray(xx, &x));
  PetscCall(PetscLogFlops(2.0 * bs2 * (a->nz) - bs * A->cmap->n));
  PetscFunctionReturn(PETSC_SUCCESS);
}

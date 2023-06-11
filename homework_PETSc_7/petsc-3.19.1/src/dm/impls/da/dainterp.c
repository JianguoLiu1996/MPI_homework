
/*
  Code for interpolating between grids represented by DMDAs
*/

/*
      For linear elements there are two branches of code to compute the interpolation. They should compute the same results but may not. The "new version" does
   not work for periodic domains, the old does. Change NEWVERSION to 1 to compile in the new version. Eventually when we are sure the two produce identical results
   we will remove/merge the new version. Based on current tests, these both produce the same results. We are leaving NEWVERSION for now in the code since some
   consider it cleaner, but old version is turned on since it handles periodic case.
*/
#define NEWVERSION 0

#include <petsc/private/dmdaimpl.h> /*I   "petscdmda.h"   I*/

/*
   Since the interpolation uses MATMAIJ for dof > 0 we convert request for non-MATAIJ baseded matrices to MATAIJ.
   This is a bit of a hack, the reason for it is partially because -dm_mat_type defines the
   matrix type for both the operator matrices and the interpolation matrices so that users
   can select matrix types of base MATAIJ for accelerators
*/
static PetscErrorCode ConvertToAIJ(MatType intype, MatType *outtype)
{
  PetscInt    i;
  char const *types[3] = {MATAIJ, MATSEQAIJ, MATMPIAIJ};
  PetscBool   flg;

  PetscFunctionBegin;
  *outtype = MATAIJ;
  for (i = 0; i < 3; i++) {
    PetscCall(PetscStrbeginswith(intype, types[i], &flg));
    if (flg) {
      *outtype = intype;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInterpolation_DA_1D_Q1(DM dac, DM daf, Mat *A)
{
  PetscInt               i, i_start, m_f, Mx;
  const PetscInt        *idx_f, *idx_c;
  PetscInt               m_ghost, m_ghost_c;
  PetscInt               row, col, i_start_ghost, mx, m_c, nc, ratio;
  PetscInt               i_c, i_start_c, i_start_ghost_c, cols[2], dof;
  PetscScalar            v[2], x;
  Mat                    mat;
  DMBoundaryType         bx;
  ISLocalToGlobalMapping ltog_f, ltog_c;
  MatType                mattype;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bx, NULL, NULL, NULL));
  PetscCall(DMDAGetInfo(daf, NULL, &mx, NULL, NULL, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  if (bx == DM_BOUNDARY_PERIODIC) {
    ratio = mx / Mx;
    PetscCheck(ratio * Mx == mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mx/Mx  must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  } else {
    ratio = (mx - 1) / (Mx - 1);
    PetscCheck(ratio * (Mx - 1) == mx - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mx - 1)/(Mx - 1) must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  }

  PetscCall(DMDAGetCorners(daf, &i_start, NULL, NULL, &m_f, NULL, NULL));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, NULL, NULL, &m_ghost, NULL, NULL));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, NULL, NULL, &m_c, NULL, NULL));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, NULL, NULL, &m_ghost_c, NULL, NULL));
  PetscCall(DMGetLocalToGlobalMapping(dac, &ltog_c));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_c, &idx_c));

  /* create interpolation matrix */
  PetscCall(MatCreate(PetscObjectComm((PetscObject)dac), &mat));
#if defined(PETSC_HAVE_CUDA)
  /*
     Temporary hack: Since the MAIJ matrix must be converted to AIJ before being used by the GPU
     we don't want the original unconverted matrix copied to the GPU
   */
  if (dof > 1) PetscCall(MatBindToCPU(mat, PETSC_TRUE));
#endif
  PetscCall(MatSetSizes(mat, m_f, m_c, mx, Mx));
  PetscCall(ConvertToAIJ(dac->mattype, &mattype));
  PetscCall(MatSetType(mat, mattype));
  PetscCall(MatSeqAIJSetPreallocation(mat, 2, NULL));
  PetscCall(MatMPIAIJSetPreallocation(mat, 2, NULL, 1, NULL));

  /* loop over local fine grid nodes setting interpolation for those*/
  if (!NEWVERSION) {
    for (i = i_start; i < i_start + m_f; i++) {
      /* convert to local "natural" numbering and then to PETSc global numbering */
      row = idx_f[i - i_start_ghost];

      i_c = (i / ratio); /* coarse grid node to left of fine grid node */
      PetscCheck(i_c >= i_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
                                          i_start %" PetscInt_FMT " i_c %" PetscInt_FMT " i_start_ghost_c %" PetscInt_FMT,
                 i_start, i_c, i_start_ghost_c);

      /*
       Only include those interpolation points that are truly
       nonzero. Note this is very important for final grid lines
       in x direction; since they have no right neighbor
       */
      x  = ((PetscReal)(i - i_c * ratio)) / ((PetscReal)ratio);
      nc = 0;
      /* one left and below; or we are right on it */
      col      = (i_c - i_start_ghost_c);
      cols[nc] = idx_c[col];
      v[nc++]  = -x + 1.0;
      /* one right? */
      if (i_c * ratio != i) {
        cols[nc] = idx_c[col + 1];
        v[nc++]  = x;
      }
      PetscCall(MatSetValues(mat, 1, &row, nc, cols, v, INSERT_VALUES));
    }

  } else {
    PetscScalar *xi;
    PetscInt     li, nxi, n;
    PetscScalar  Ni[2];

    /* compute local coordinate arrays */
    nxi = ratio + 1;
    PetscCall(PetscMalloc1(nxi, &xi));
    for (li = 0; li < nxi; li++) xi[li] = -1.0 + (PetscScalar)li * (2.0 / (PetscScalar)(nxi - 1));

    for (i = i_start; i < i_start + m_f; i++) {
      /* convert to local "natural" numbering and then to PETSc global numbering */
      row = idx_f[(i - i_start_ghost)];

      i_c = (i / ratio); /* coarse grid node to left of fine grid node */
      PetscCheck(i_c >= i_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
                                          i_start %" PetscInt_FMT " i_c %" PetscInt_FMT " i_start_ghost_c %" PetscInt_FMT,
                 i_start, i_c, i_start_ghost_c);

      /* remainders */
      li = i - ratio * (i / ratio);
      if (i == mx - 1) li = nxi - 1;

      /* corners */
      col     = (i_c - i_start_ghost_c);
      cols[0] = idx_c[col];
      Ni[0]   = 1.0;
      if ((li == 0) || (li == nxi - 1)) {
        PetscCall(MatSetValue(mat, row, cols[0], Ni[0], INSERT_VALUES));
        continue;
      }

      /* edges + interior */
      /* remainders */
      if (i == mx - 1) i_c--;

      col     = (i_c - i_start_ghost_c);
      cols[0] = idx_c[col]; /* one left and below; or we are right on it */
      cols[1] = idx_c[col + 1];

      Ni[0] = 0.5 * (1.0 - xi[li]);
      Ni[1] = 0.5 * (1.0 + xi[li]);
      for (n = 0; n < 2; n++) {
        if (PetscAbsScalar(Ni[n]) < 1.0e-32) cols[n] = -1;
      }
      PetscCall(MatSetValues(mat, 1, &row, 2, cols, Ni, INSERT_VALUES));
    }
    PetscCall(PetscFree(xi));
  }
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_c, &idx_c));
  PetscCall(MatAssemblyBegin(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatCreateMAIJ(mat, dof, A));
  PetscCall(MatDestroy(&mat));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInterpolation_DA_1D_Q0(DM dac, DM daf, Mat *A)
{
  PetscInt               i, i_start, m_f, Mx;
  const PetscInt        *idx_f, *idx_c;
  ISLocalToGlobalMapping ltog_f, ltog_c;
  PetscInt               m_ghost, m_ghost_c;
  PetscInt               row, col, i_start_ghost, mx, m_c, nc, ratio;
  PetscInt               i_c, i_start_c, i_start_ghost_c, cols[2], dof;
  PetscScalar            v[2], x;
  Mat                    mat;
  DMBoundaryType         bx;
  MatType                mattype;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bx, NULL, NULL, NULL));
  PetscCall(DMDAGetInfo(daf, NULL, &mx, NULL, NULL, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  if (bx == DM_BOUNDARY_PERIODIC) {
    PetscCheck(Mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of x coarse grid points %" PetscInt_FMT " must be positive", Mx);
    ratio = mx / Mx;
    PetscCheck(ratio * Mx == mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mx/Mx  must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  } else {
    PetscCheck(Mx >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of x coarse grid points %" PetscInt_FMT " must be greater than 1", Mx);
    ratio = (mx - 1) / (Mx - 1);
    PetscCheck(ratio * (Mx - 1) == mx - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mx - 1)/(Mx - 1) must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  }

  PetscCall(DMDAGetCorners(daf, &i_start, NULL, NULL, &m_f, NULL, NULL));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, NULL, NULL, &m_ghost, NULL, NULL));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, NULL, NULL, &m_c, NULL, NULL));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, NULL, NULL, &m_ghost_c, NULL, NULL));
  PetscCall(DMGetLocalToGlobalMapping(dac, &ltog_c));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_c, &idx_c));

  /* create interpolation matrix */
  PetscCall(MatCreate(PetscObjectComm((PetscObject)dac), &mat));
#if defined(PETSC_HAVE_CUDA)
  /*
     Temporary hack: Since the MAIJ matrix must be converted to AIJ before being used by the GPU
     we don't want the original unconverted matrix copied to the GPU
   */
  if (dof > 1) PetscCall(MatBindToCPU(mat, PETSC_TRUE));
#endif
  PetscCall(MatSetSizes(mat, m_f, m_c, mx, Mx));
  PetscCall(ConvertToAIJ(dac->mattype, &mattype));
  PetscCall(MatSetType(mat, mattype));
  PetscCall(MatSeqAIJSetPreallocation(mat, 2, NULL));
  PetscCall(MatMPIAIJSetPreallocation(mat, 2, NULL, 0, NULL));

  /* loop over local fine grid nodes setting interpolation for those*/
  for (i = i_start; i < i_start + m_f; i++) {
    /* convert to local "natural" numbering and then to PETSc global numbering */
    row = idx_f[(i - i_start_ghost)];

    i_c = (i / ratio); /* coarse grid node to left of fine grid node */

    /*
         Only include those interpolation points that are truly
         nonzero. Note this is very important for final grid lines
         in x direction; since they have no right neighbor
    */
    x  = ((PetscReal)(i - i_c * ratio)) / ((PetscReal)ratio);
    nc = 0;
    /* one left and below; or we are right on it */
    col      = (i_c - i_start_ghost_c);
    cols[nc] = idx_c[col];
    v[nc++]  = -x + 1.0;
    /* one right? */
    if (i_c * ratio != i) {
      cols[nc] = idx_c[col + 1];
      v[nc++]  = x;
    }
    PetscCall(MatSetValues(mat, 1, &row, nc, cols, v, INSERT_VALUES));
  }
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_c, &idx_c));
  PetscCall(MatAssemblyBegin(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatCreateMAIJ(mat, dof, A));
  PetscCall(MatDestroy(&mat));
  PetscCall(PetscLogFlops(5.0 * m_f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInterpolation_DA_2D_Q1(DM dac, DM daf, Mat *A)
{
  PetscInt               i, j, i_start, j_start, m_f, n_f, Mx, My, dof;
  const PetscInt        *idx_c, *idx_f;
  ISLocalToGlobalMapping ltog_f, ltog_c;
  PetscInt               m_ghost, n_ghost, m_ghost_c, n_ghost_c, *dnz, *onz;
  PetscInt               row, col, i_start_ghost, j_start_ghost, cols[4], mx, m_c, my, nc, ratioi, ratioj;
  PetscInt               i_c, j_c, i_start_c, j_start_c, n_c, i_start_ghost_c, j_start_ghost_c, col_shift, col_scale;
  PetscMPIInt            size_c, size_f, rank_f;
  PetscScalar            v[4], x, y;
  Mat                    mat;
  DMBoundaryType         bx, by;
  MatType                mattype;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, &My, NULL, NULL, NULL, NULL, NULL, NULL, &bx, &by, NULL, NULL));
  PetscCall(DMDAGetInfo(daf, NULL, &mx, &my, NULL, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  if (bx == DM_BOUNDARY_PERIODIC) {
    PetscCheck(Mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of x coarse grid points %" PetscInt_FMT " must be positive", Mx);
    ratioi = mx / Mx;
    PetscCheck(ratioi * Mx == mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mx/Mx  must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  } else {
    PetscCheck(Mx >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of x coarse grid points %" PetscInt_FMT " must be greater than 1", Mx);
    ratioi = (mx - 1) / (Mx - 1);
    PetscCheck(ratioi * (Mx - 1) == mx - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mx - 1)/(Mx - 1) must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  }
  if (by == DM_BOUNDARY_PERIODIC) {
    PetscCheck(My, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of y coarse grid points %" PetscInt_FMT " must be positive", My);
    ratioj = my / My;
    PetscCheck(ratioj * My == my, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: my/My  must be integer: my %" PetscInt_FMT " My %" PetscInt_FMT, my, My);
  } else {
    PetscCheck(My >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of y coarse grid points %" PetscInt_FMT " must be greater than 1", My);
    ratioj = (my - 1) / (My - 1);
    PetscCheck(ratioj * (My - 1) == my - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (my - 1)/(My - 1) must be integer: my %" PetscInt_FMT " My %" PetscInt_FMT, my, My);
  }

  PetscCall(DMDAGetCorners(daf, &i_start, &j_start, NULL, &m_f, &n_f, NULL));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, &j_start_ghost, NULL, &m_ghost, &n_ghost, NULL));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, &j_start_c, NULL, &m_c, &n_c, NULL));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, &j_start_ghost_c, NULL, &m_ghost_c, &n_ghost_c, NULL));
  PetscCall(DMGetLocalToGlobalMapping(dac, &ltog_c));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_c, &idx_c));

  /*
   Used for handling a coarse DMDA that lives on 1/4 the processors of the fine DMDA.
   The coarse vector is then duplicated 4 times (each time it lives on 1/4 of the
   processors). It's effective length is hence 4 times its normal length, this is
   why the col_scale is multiplied by the interpolation matrix column sizes.
   sol_shift allows each set of 1/4 processors do its own interpolation using ITS
   copy of the coarse vector. A bit of a hack but you do better.

   In the standard case when size_f == size_c col_scale == 1 and col_shift == 0
   */
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)dac), &size_c));
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)daf), &size_f));
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)daf), &rank_f));
  col_scale = size_f / size_c;
  col_shift = Mx * My * (rank_f / size_c);

  MatPreallocateBegin(PetscObjectComm((PetscObject)daf), m_f * n_f, col_scale * m_c * n_c, dnz, onz);
  for (j = j_start; j < j_start + n_f; j++) {
    for (i = i_start; i < i_start + m_f; i++) {
      /* convert to local "natural" numbering and then to PETSc global numbering */
      row = idx_f[(m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

      i_c = (i / ratioi); /* coarse grid node to left of fine grid node */
      j_c = (j / ratioj); /* coarse grid node below fine grid node */

      PetscCheck(j_c >= j_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
                                          j_start %" PetscInt_FMT " j_c %" PetscInt_FMT " j_start_ghost_c %" PetscInt_FMT,
                 j_start, j_c, j_start_ghost_c);
      PetscCheck(i_c >= i_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
                                          i_start %" PetscInt_FMT " i_c %" PetscInt_FMT " i_start_ghost_c %" PetscInt_FMT,
                 i_start, i_c, i_start_ghost_c);

      /*
       Only include those interpolation points that are truly
       nonzero. Note this is very important for final grid lines
       in x and y directions; since they have no right/top neighbors
       */
      nc = 0;
      /* one left and below; or we are right on it */
      col        = (m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
      cols[nc++] = col_shift + idx_c[col];
      /* one right and below */
      if (i_c * ratioi != i) cols[nc++] = col_shift + idx_c[col + 1];
      /* one left and above */
      if (j_c * ratioj != j) cols[nc++] = col_shift + idx_c[col + m_ghost_c];
      /* one right and above */
      if (i_c * ratioi != i && j_c * ratioj != j) cols[nc++] = col_shift + idx_c[col + (m_ghost_c + 1)];
      PetscCall(MatPreallocateSet(row, nc, cols, dnz, onz));
    }
  }
  PetscCall(MatCreate(PetscObjectComm((PetscObject)daf), &mat));
#if defined(PETSC_HAVE_CUDA)
  /*
     Temporary hack: Since the MAIJ matrix must be converted to AIJ before being used by the GPU
     we don't want the original unconverted matrix copied to the GPU
  */
  if (dof > 1) PetscCall(MatBindToCPU(mat, PETSC_TRUE));
#endif
  PetscCall(MatSetSizes(mat, m_f * n_f, col_scale * m_c * n_c, mx * my, col_scale * Mx * My));
  PetscCall(ConvertToAIJ(dac->mattype, &mattype));
  PetscCall(MatSetType(mat, mattype));
  PetscCall(MatSeqAIJSetPreallocation(mat, 0, dnz));
  PetscCall(MatMPIAIJSetPreallocation(mat, 0, dnz, 0, onz));
  MatPreallocateEnd(dnz, onz);

  /* loop over local fine grid nodes setting interpolation for those*/
  if (!NEWVERSION) {
    for (j = j_start; j < j_start + n_f; j++) {
      for (i = i_start; i < i_start + m_f; i++) {
        /* convert to local "natural" numbering and then to PETSc global numbering */
        row = idx_f[(m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

        i_c = (i / ratioi); /* coarse grid node to left of fine grid node */
        j_c = (j / ratioj); /* coarse grid node below fine grid node */

        /*
         Only include those interpolation points that are truly
         nonzero. Note this is very important for final grid lines
         in x and y directions; since they have no right/top neighbors
         */
        x = ((PetscReal)(i - i_c * ratioi)) / ((PetscReal)ratioi);
        y = ((PetscReal)(j - j_c * ratioj)) / ((PetscReal)ratioj);

        nc = 0;
        /* one left and below; or we are right on it */
        col      = (m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
        cols[nc] = col_shift + idx_c[col];
        v[nc++]  = x * y - x - y + 1.0;
        /* one right and below */
        if (i_c * ratioi != i) {
          cols[nc] = col_shift + idx_c[col + 1];
          v[nc++]  = -x * y + x;
        }
        /* one left and above */
        if (j_c * ratioj != j) {
          cols[nc] = col_shift + idx_c[col + m_ghost_c];
          v[nc++]  = -x * y + y;
        }
        /* one right and above */
        if (j_c * ratioj != j && i_c * ratioi != i) {
          cols[nc] = col_shift + idx_c[col + (m_ghost_c + 1)];
          v[nc++]  = x * y;
        }
        PetscCall(MatSetValues(mat, 1, &row, nc, cols, v, INSERT_VALUES));
      }
    }

  } else {
    PetscScalar  Ni[4];
    PetscScalar *xi, *eta;
    PetscInt     li, nxi, lj, neta;

    /* compute local coordinate arrays */
    nxi  = ratioi + 1;
    neta = ratioj + 1;
    PetscCall(PetscMalloc1(nxi, &xi));
    PetscCall(PetscMalloc1(neta, &eta));
    for (li = 0; li < nxi; li++) xi[li] = -1.0 + (PetscScalar)li * (2.0 / (PetscScalar)(nxi - 1));
    for (lj = 0; lj < neta; lj++) eta[lj] = -1.0 + (PetscScalar)lj * (2.0 / (PetscScalar)(neta - 1));

    /* loop over local fine grid nodes setting interpolation for those*/
    for (j = j_start; j < j_start + n_f; j++) {
      for (i = i_start; i < i_start + m_f; i++) {
        /* convert to local "natural" numbering and then to PETSc global numbering */
        row = idx_f[(m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

        i_c = (i / ratioi); /* coarse grid node to left of fine grid node */
        j_c = (j / ratioj); /* coarse grid node below fine grid node */

        /* remainders */
        li = i - ratioi * (i / ratioi);
        if (i == mx - 1) li = nxi - 1;
        lj = j - ratioj * (j / ratioj);
        if (j == my - 1) lj = neta - 1;

        /* corners */
        col     = (m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
        cols[0] = col_shift + idx_c[col]; /* left, below */
        Ni[0]   = 1.0;
        if ((li == 0) || (li == nxi - 1)) {
          if ((lj == 0) || (lj == neta - 1)) {
            PetscCall(MatSetValue(mat, row, cols[0], Ni[0], INSERT_VALUES));
            continue;
          }
        }

        /* edges + interior */
        /* remainders */
        if (i == mx - 1) i_c--;
        if (j == my - 1) j_c--;

        col     = (m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
        cols[0] = col_shift + idx_c[col];                   /* left, below */
        cols[1] = col_shift + idx_c[col + 1];               /* right, below */
        cols[2] = col_shift + idx_c[col + m_ghost_c];       /* left, above */
        cols[3] = col_shift + idx_c[col + (m_ghost_c + 1)]; /* right, above */

        Ni[0] = 0.25 * (1.0 - xi[li]) * (1.0 - eta[lj]);
        Ni[1] = 0.25 * (1.0 + xi[li]) * (1.0 - eta[lj]);
        Ni[2] = 0.25 * (1.0 - xi[li]) * (1.0 + eta[lj]);
        Ni[3] = 0.25 * (1.0 + xi[li]) * (1.0 + eta[lj]);

        nc = 0;
        if (PetscAbsScalar(Ni[0]) < 1.0e-32) cols[0] = -1;
        if (PetscAbsScalar(Ni[1]) < 1.0e-32) cols[1] = -1;
        if (PetscAbsScalar(Ni[2]) < 1.0e-32) cols[2] = -1;
        if (PetscAbsScalar(Ni[3]) < 1.0e-32) cols[3] = -1;

        PetscCall(MatSetValues(mat, 1, &row, 4, cols, Ni, INSERT_VALUES));
      }
    }
    PetscCall(PetscFree(xi));
    PetscCall(PetscFree(eta));
  }
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_c, &idx_c));
  PetscCall(MatAssemblyBegin(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatCreateMAIJ(mat, dof, A));
  PetscCall(MatDestroy(&mat));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
       Contributed by Andrei Draganescu <aidraga@sandia.gov>
*/
PetscErrorCode DMCreateInterpolation_DA_2D_Q0(DM dac, DM daf, Mat *A)
{
  PetscInt               i, j, i_start, j_start, m_f, n_f, Mx, My, dof;
  const PetscInt        *idx_c, *idx_f;
  ISLocalToGlobalMapping ltog_f, ltog_c;
  PetscInt               m_ghost, n_ghost, m_ghost_c, n_ghost_c, *dnz, *onz;
  PetscInt               row, col, i_start_ghost, j_start_ghost, cols[4], mx, m_c, my, nc, ratioi, ratioj;
  PetscInt               i_c, j_c, i_start_c, j_start_c, n_c, i_start_ghost_c, j_start_ghost_c, col_shift, col_scale;
  PetscMPIInt            size_c, size_f, rank_f;
  PetscScalar            v[4];
  Mat                    mat;
  DMBoundaryType         bx, by;
  MatType                mattype;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, &My, NULL, NULL, NULL, NULL, NULL, NULL, &bx, &by, NULL, NULL));
  PetscCall(DMDAGetInfo(daf, NULL, &mx, &my, NULL, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  PetscCheck(Mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of x coarse grid points %" PetscInt_FMT " must be positive", Mx);
  PetscCheck(My, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of y coarse grid points %" PetscInt_FMT " must be positive", My);
  ratioi = mx / Mx;
  ratioj = my / My;
  PetscCheck(ratioi * Mx == mx, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Fine grid points must be multiple of coarse grid points in x");
  PetscCheck(ratioj * My == my, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Fine grid points must be multiple of coarse grid points in y");
  PetscCheck(ratioi == 2, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Coarsening factor in x must be 2");
  PetscCheck(ratioj == 2, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Coarsening factor in y must be 2");

  PetscCall(DMDAGetCorners(daf, &i_start, &j_start, NULL, &m_f, &n_f, NULL));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, &j_start_ghost, NULL, &m_ghost, &n_ghost, NULL));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, &j_start_c, NULL, &m_c, &n_c, NULL));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, &j_start_ghost_c, NULL, &m_ghost_c, &n_ghost_c, NULL));
  PetscCall(DMGetLocalToGlobalMapping(dac, &ltog_c));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_c, &idx_c));

  /*
     Used for handling a coarse DMDA that lives on 1/4 the processors of the fine DMDA.
     The coarse vector is then duplicated 4 times (each time it lives on 1/4 of the
     processors). It's effective length is hence 4 times its normal length, this is
     why the col_scale is multiplied by the interpolation matrix column sizes.
     sol_shift allows each set of 1/4 processors do its own interpolation using ITS
     copy of the coarse vector. A bit of a hack but you do better.

     In the standard case when size_f == size_c col_scale == 1 and col_shift == 0
  */
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)dac), &size_c));
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)daf), &size_f));
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)daf), &rank_f));
  col_scale = size_f / size_c;
  col_shift = Mx * My * (rank_f / size_c);

  MatPreallocateBegin(PetscObjectComm((PetscObject)daf), m_f * n_f, col_scale * m_c * n_c, dnz, onz);
  for (j = j_start; j < j_start + n_f; j++) {
    for (i = i_start; i < i_start + m_f; i++) {
      /* convert to local "natural" numbering and then to PETSc global numbering */
      row = idx_f[(m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

      i_c = (i / ratioi); /* coarse grid node to left of fine grid node */
      j_c = (j / ratioj); /* coarse grid node below fine grid node */

      PetscCheck(j_c >= j_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
    j_start %" PetscInt_FMT " j_c %" PetscInt_FMT " j_start_ghost_c %" PetscInt_FMT,
                 j_start, j_c, j_start_ghost_c);
      PetscCheck(i_c >= i_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
    i_start %" PetscInt_FMT " i_c %" PetscInt_FMT " i_start_ghost_c %" PetscInt_FMT,
                 i_start, i_c, i_start_ghost_c);

      /*
         Only include those interpolation points that are truly
         nonzero. Note this is very important for final grid lines
         in x and y directions; since they have no right/top neighbors
      */
      nc = 0;
      /* one left and below; or we are right on it */
      col        = (m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
      cols[nc++] = col_shift + idx_c[col];
      PetscCall(MatPreallocateSet(row, nc, cols, dnz, onz));
    }
  }
  PetscCall(MatCreate(PetscObjectComm((PetscObject)daf), &mat));
#if defined(PETSC_HAVE_CUDA)
  /*
     Temporary hack: Since the MAIJ matrix must be converted to AIJ before being used by the GPU
     we don't want the original unconverted matrix copied to the GPU
  */
  if (dof > 1) PetscCall(MatBindToCPU(mat, PETSC_TRUE));
#endif
  PetscCall(MatSetSizes(mat, m_f * n_f, col_scale * m_c * n_c, mx * my, col_scale * Mx * My));
  PetscCall(ConvertToAIJ(dac->mattype, &mattype));
  PetscCall(MatSetType(mat, mattype));
  PetscCall(MatSeqAIJSetPreallocation(mat, 0, dnz));
  PetscCall(MatMPIAIJSetPreallocation(mat, 0, dnz, 0, onz));
  MatPreallocateEnd(dnz, onz);

  /* loop over local fine grid nodes setting interpolation for those*/
  for (j = j_start; j < j_start + n_f; j++) {
    for (i = i_start; i < i_start + m_f; i++) {
      /* convert to local "natural" numbering and then to PETSc global numbering */
      row = idx_f[(m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

      i_c = (i / ratioi); /* coarse grid node to left of fine grid node */
      j_c = (j / ratioj); /* coarse grid node below fine grid node */
      nc  = 0;
      /* one left and below; or we are right on it */
      col      = (m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
      cols[nc] = col_shift + idx_c[col];
      v[nc++]  = 1.0;

      PetscCall(MatSetValues(mat, 1, &row, nc, cols, v, INSERT_VALUES));
    }
  }
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_c, &idx_c));
  PetscCall(MatAssemblyBegin(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatCreateMAIJ(mat, dof, A));
  PetscCall(MatDestroy(&mat));
  PetscCall(PetscLogFlops(13.0 * m_f * n_f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
       Contributed by Jianming Yang <jianming-yang@uiowa.edu>
*/
PetscErrorCode DMCreateInterpolation_DA_3D_Q0(DM dac, DM daf, Mat *A)
{
  PetscInt               i, j, l, i_start, j_start, l_start, m_f, n_f, p_f, Mx, My, Mz, dof;
  const PetscInt        *idx_c, *idx_f;
  ISLocalToGlobalMapping ltog_f, ltog_c;
  PetscInt               m_ghost, n_ghost, p_ghost, m_ghost_c, n_ghost_c, p_ghost_c, nc, *dnz, *onz;
  PetscInt               row, col, i_start_ghost, j_start_ghost, l_start_ghost, cols[8], mx, m_c, my, n_c, mz, p_c, ratioi, ratioj, ratiol;
  PetscInt               i_c, j_c, l_c, i_start_c, j_start_c, l_start_c, i_start_ghost_c, j_start_ghost_c, l_start_ghost_c, col_shift, col_scale;
  PetscMPIInt            size_c, size_f, rank_f;
  PetscScalar            v[8];
  Mat                    mat;
  DMBoundaryType         bx, by, bz;
  MatType                mattype;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, &My, &Mz, NULL, NULL, NULL, NULL, NULL, &bx, &by, &bz, NULL));
  PetscCheck(Mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of x coarse grid points %" PetscInt_FMT " must be positive", Mx);
  PetscCheck(My, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of y coarse grid points %" PetscInt_FMT " must be positive", My);
  PetscCheck(Mz, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of z coarse grid points %" PetscInt_FMT " must be positive", Mz);
  PetscCall(DMDAGetInfo(daf, NULL, &mx, &my, &mz, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  ratioi = mx / Mx;
  ratioj = my / My;
  ratiol = mz / Mz;
  PetscCheck(ratioi * Mx == mx, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Fine grid points must be multiple of coarse grid points in x");
  PetscCheck(ratioj * My == my, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Fine grid points must be multiple of coarse grid points in y");
  PetscCheck(ratiol * Mz == mz, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Fine grid points must be multiple of coarse grid points in z");
  PetscCheck(ratioi == 2 || ratioi == 1, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Coarsening factor in x must be 1 or 2");
  PetscCheck(ratioj == 2 || ratioj == 1, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Coarsening factor in y must be 1 or 2");
  PetscCheck(ratiol == 2 || ratiol == 1, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_WRONG, "Coarsening factor in z must be 1 or 2");

  PetscCall(DMDAGetCorners(daf, &i_start, &j_start, &l_start, &m_f, &n_f, &p_f));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, &j_start_ghost, &l_start_ghost, &m_ghost, &n_ghost, &p_ghost));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, &j_start_c, &l_start_c, &m_c, &n_c, &p_c));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, &j_start_ghost_c, &l_start_ghost_c, &m_ghost_c, &n_ghost_c, &p_ghost_c));
  PetscCall(DMGetLocalToGlobalMapping(dac, &ltog_c));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_c, &idx_c));

  /*
     Used for handling a coarse DMDA that lives on 1/4 the processors of the fine DMDA.
     The coarse vector is then duplicated 4 times (each time it lives on 1/4 of the
     processors). It's effective length is hence 4 times its normal length, this is
     why the col_scale is multiplied by the interpolation matrix column sizes.
     sol_shift allows each set of 1/4 processors do its own interpolation using ITS
     copy of the coarse vector. A bit of a hack but you do better.

     In the standard case when size_f == size_c col_scale == 1 and col_shift == 0
  */
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)dac), &size_c));
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)daf), &size_f));
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)daf), &rank_f));
  col_scale = size_f / size_c;
  col_shift = Mx * My * Mz * (rank_f / size_c);

  MatPreallocateBegin(PetscObjectComm((PetscObject)daf), m_f * n_f * p_f, col_scale * m_c * n_c * p_c, dnz, onz);
  for (l = l_start; l < l_start + p_f; l++) {
    for (j = j_start; j < j_start + n_f; j++) {
      for (i = i_start; i < i_start + m_f; i++) {
        /* convert to local "natural" numbering and then to PETSc global numbering */
        row = idx_f[(m_ghost * n_ghost * (l - l_start_ghost) + m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

        i_c = (i / ratioi); /* coarse grid node to left of fine grid node */
        j_c = (j / ratioj); /* coarse grid node below fine grid node */
        l_c = (l / ratiol);

        PetscCheck(l_c >= l_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
    l_start %" PetscInt_FMT " l_c %" PetscInt_FMT " l_start_ghost_c %" PetscInt_FMT,
                   l_start, l_c, l_start_ghost_c);
        PetscCheck(j_c >= j_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
    j_start %" PetscInt_FMT " j_c %" PetscInt_FMT " j_start_ghost_c %" PetscInt_FMT,
                   j_start, j_c, j_start_ghost_c);
        PetscCheck(i_c >= i_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
    i_start %" PetscInt_FMT " i_c %" PetscInt_FMT " i_start_ghost_c %" PetscInt_FMT,
                   i_start, i_c, i_start_ghost_c);

        /*
           Only include those interpolation points that are truly
           nonzero. Note this is very important for final grid lines
           in x and y directions; since they have no right/top neighbors
        */
        nc = 0;
        /* one left and below; or we are right on it */
        col        = (m_ghost_c * n_ghost_c * (l_c - l_start_ghost_c) + m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
        cols[nc++] = col_shift + idx_c[col];
        PetscCall(MatPreallocateSet(row, nc, cols, dnz, onz));
      }
    }
  }
  PetscCall(MatCreate(PetscObjectComm((PetscObject)daf), &mat));
#if defined(PETSC_HAVE_CUDA)
  /*
     Temporary hack: Since the MAIJ matrix must be converted to AIJ before being used by the GPU
     we don't want the original unconverted matrix copied to the GPU
  */
  if (dof > 1) PetscCall(MatBindToCPU(mat, PETSC_TRUE));
#endif
  PetscCall(MatSetSizes(mat, m_f * n_f * p_f, col_scale * m_c * n_c * p_c, mx * my * mz, col_scale * Mx * My * Mz));
  PetscCall(ConvertToAIJ(dac->mattype, &mattype));
  PetscCall(MatSetType(mat, mattype));
  PetscCall(MatSeqAIJSetPreallocation(mat, 0, dnz));
  PetscCall(MatMPIAIJSetPreallocation(mat, 0, dnz, 0, onz));
  MatPreallocateEnd(dnz, onz);

  /* loop over local fine grid nodes setting interpolation for those*/
  for (l = l_start; l < l_start + p_f; l++) {
    for (j = j_start; j < j_start + n_f; j++) {
      for (i = i_start; i < i_start + m_f; i++) {
        /* convert to local "natural" numbering and then to PETSc global numbering */
        row = idx_f[(m_ghost * n_ghost * (l - l_start_ghost) + m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

        i_c = (i / ratioi); /* coarse grid node to left of fine grid node */
        j_c = (j / ratioj); /* coarse grid node below fine grid node */
        l_c = (l / ratiol);
        nc  = 0;
        /* one left and below; or we are right on it */
        col      = (m_ghost_c * n_ghost_c * (l_c - l_start_ghost_c) + m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
        cols[nc] = col_shift + idx_c[col];
        v[nc++]  = 1.0;

        PetscCall(MatSetValues(mat, 1, &row, nc, cols, v, INSERT_VALUES));
      }
    }
  }
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_c, &idx_c));
  PetscCall(MatAssemblyBegin(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatCreateMAIJ(mat, dof, A));
  PetscCall(MatDestroy(&mat));
  PetscCall(PetscLogFlops(13.0 * m_f * n_f * p_f));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInterpolation_DA_3D_Q1(DM dac, DM daf, Mat *A)
{
  PetscInt               i, j, i_start, j_start, m_f, n_f, Mx, My, dof, l;
  const PetscInt        *idx_c, *idx_f;
  ISLocalToGlobalMapping ltog_f, ltog_c;
  PetscInt               m_ghost, n_ghost, m_ghost_c, n_ghost_c, Mz, mz;
  PetscInt               row, col, i_start_ghost, j_start_ghost, cols[8], mx, m_c, my, nc, ratioi, ratioj, ratiok;
  PetscInt               i_c, j_c, i_start_c, j_start_c, n_c, i_start_ghost_c, j_start_ghost_c;
  PetscInt               l_start, p_f, l_start_ghost, p_ghost, l_start_c, p_c;
  PetscInt               l_start_ghost_c, p_ghost_c, l_c, *dnz, *onz;
  PetscScalar            v[8], x, y, z;
  Mat                    mat;
  DMBoundaryType         bx, by, bz;
  MatType                mattype;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, &My, &Mz, NULL, NULL, NULL, NULL, NULL, &bx, &by, &bz, NULL));
  PetscCall(DMDAGetInfo(daf, NULL, &mx, &my, &mz, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  if (mx == Mx) {
    ratioi = 1;
  } else if (bx == DM_BOUNDARY_PERIODIC) {
    PetscCheck(Mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of x coarse grid points %" PetscInt_FMT " must be positive", Mx);
    ratioi = mx / Mx;
    PetscCheck(ratioi * Mx == mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mx/Mx  must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  } else {
    PetscCheck(Mx >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of x coarse grid points %" PetscInt_FMT " must be greater than 1", Mx);
    ratioi = (mx - 1) / (Mx - 1);
    PetscCheck(ratioi * (Mx - 1) == mx - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mx - 1)/(Mx - 1) must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  }
  if (my == My) {
    ratioj = 1;
  } else if (by == DM_BOUNDARY_PERIODIC) {
    PetscCheck(My, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of y coarse grid points %" PetscInt_FMT " must be positive", My);
    ratioj = my / My;
    PetscCheck(ratioj * My == my, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: my/My  must be integer: my %" PetscInt_FMT " My %" PetscInt_FMT, my, My);
  } else {
    PetscCheck(My >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of y coarse grid points %" PetscInt_FMT " must be greater than 1", My);
    ratioj = (my - 1) / (My - 1);
    PetscCheck(ratioj * (My - 1) == my - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (my - 1)/(My - 1) must be integer: my %" PetscInt_FMT " My %" PetscInt_FMT, my, My);
  }
  if (mz == Mz) {
    ratiok = 1;
  } else if (bz == DM_BOUNDARY_PERIODIC) {
    PetscCheck(Mz, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of z coarse grid points %" PetscInt_FMT " must be positive", Mz);
    ratiok = mz / Mz;
    PetscCheck(ratiok * Mz == mz, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mz/Mz  must be integer: mz %" PetscInt_FMT " Mz %" PetscInt_FMT, mz, Mz);
  } else {
    PetscCheck(Mz >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Number of z coarse grid points %" PetscInt_FMT " must be greater than 1", Mz);
    ratiok = (mz - 1) / (Mz - 1);
    PetscCheck(ratiok * (Mz - 1) == mz - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mz - 1)/(Mz - 1) must be integer: mz %" PetscInt_FMT " Mz %" PetscInt_FMT, mz, Mz);
  }

  PetscCall(DMDAGetCorners(daf, &i_start, &j_start, &l_start, &m_f, &n_f, &p_f));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, &j_start_ghost, &l_start_ghost, &m_ghost, &n_ghost, &p_ghost));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, &j_start_c, &l_start_c, &m_c, &n_c, &p_c));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, &j_start_ghost_c, &l_start_ghost_c, &m_ghost_c, &n_ghost_c, &p_ghost_c));
  PetscCall(DMGetLocalToGlobalMapping(dac, &ltog_c));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_c, &idx_c));

  /* create interpolation matrix, determining exact preallocation */
  MatPreallocateBegin(PetscObjectComm((PetscObject)dac), m_f * n_f * p_f, m_c * n_c * p_c, dnz, onz);
  /* loop over local fine grid nodes counting interpolating points */
  for (l = l_start; l < l_start + p_f; l++) {
    for (j = j_start; j < j_start + n_f; j++) {
      for (i = i_start; i < i_start + m_f; i++) {
        /* convert to local "natural" numbering and then to PETSc global numbering */
        row = idx_f[(m_ghost * n_ghost * (l - l_start_ghost) + m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];
        i_c = (i / ratioi);
        j_c = (j / ratioj);
        l_c = (l / ratiok);
        PetscCheck(l_c >= l_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
                                            l_start %" PetscInt_FMT " l_c %" PetscInt_FMT " l_start_ghost_c %" PetscInt_FMT,
                   l_start, l_c, l_start_ghost_c);
        PetscCheck(j_c >= j_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
                                            j_start %" PetscInt_FMT " j_c %" PetscInt_FMT " j_start_ghost_c %" PetscInt_FMT,
                   j_start, j_c, j_start_ghost_c);
        PetscCheck(i_c >= i_start_ghost_c, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
                                            i_start %" PetscInt_FMT " i_c %" PetscInt_FMT " i_start_ghost_c %" PetscInt_FMT,
                   i_start, i_c, i_start_ghost_c);

        /*
         Only include those interpolation points that are truly
         nonzero. Note this is very important for final grid lines
         in x and y directions; since they have no right/top neighbors
         */
        nc         = 0;
        col        = (m_ghost_c * n_ghost_c * (l_c - l_start_ghost_c) + m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
        cols[nc++] = idx_c[col];
        if (i_c * ratioi != i) cols[nc++] = idx_c[col + 1];
        if (j_c * ratioj != j) cols[nc++] = idx_c[col + m_ghost_c];
        if (l_c * ratiok != l) cols[nc++] = idx_c[col + m_ghost_c * n_ghost_c];
        if (j_c * ratioj != j && i_c * ratioi != i) cols[nc++] = idx_c[col + (m_ghost_c + 1)];
        if (j_c * ratioj != j && l_c * ratiok != l) cols[nc++] = idx_c[col + (m_ghost_c * n_ghost_c + m_ghost_c)];
        if (i_c * ratioi != i && l_c * ratiok != l) cols[nc++] = idx_c[col + (m_ghost_c * n_ghost_c + 1)];
        if (i_c * ratioi != i && l_c * ratiok != l && j_c * ratioj != j) cols[nc++] = idx_c[col + (m_ghost_c * n_ghost_c + m_ghost_c + 1)];
        PetscCall(MatPreallocateSet(row, nc, cols, dnz, onz));
      }
    }
  }
  PetscCall(MatCreate(PetscObjectComm((PetscObject)dac), &mat));
#if defined(PETSC_HAVE_CUDA)
  /*
     Temporary hack: Since the MAIJ matrix must be converted to AIJ before being used by the GPU
     we don't want the original unconverted matrix copied to the GPU
  */
  if (dof > 1) PetscCall(MatBindToCPU(mat, PETSC_TRUE));
#endif
  PetscCall(MatSetSizes(mat, m_f * n_f * p_f, m_c * n_c * p_c, mx * my * mz, Mx * My * Mz));
  PetscCall(ConvertToAIJ(dac->mattype, &mattype));
  PetscCall(MatSetType(mat, mattype));
  PetscCall(MatSeqAIJSetPreallocation(mat, 0, dnz));
  PetscCall(MatMPIAIJSetPreallocation(mat, 0, dnz, 0, onz));
  MatPreallocateEnd(dnz, onz);

  /* loop over local fine grid nodes setting interpolation for those*/
  if (!NEWVERSION) {
    for (l = l_start; l < l_start + p_f; l++) {
      for (j = j_start; j < j_start + n_f; j++) {
        for (i = i_start; i < i_start + m_f; i++) {
          /* convert to local "natural" numbering and then to PETSc global numbering */
          row = idx_f[(m_ghost * n_ghost * (l - l_start_ghost) + m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

          i_c = (i / ratioi);
          j_c = (j / ratioj);
          l_c = (l / ratiok);

          /*
           Only include those interpolation points that are truly
           nonzero. Note this is very important for final grid lines
           in x and y directions; since they have no right/top neighbors
           */
          x = ((PetscReal)(i - i_c * ratioi)) / ((PetscReal)ratioi);
          y = ((PetscReal)(j - j_c * ratioj)) / ((PetscReal)ratioj);
          z = ((PetscReal)(l - l_c * ratiok)) / ((PetscReal)ratiok);

          nc = 0;
          /* one left and below; or we are right on it */
          col = (m_ghost_c * n_ghost_c * (l_c - l_start_ghost_c) + m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));

          cols[nc] = idx_c[col];
          v[nc++]  = .125 * (1. - (2.0 * x - 1.)) * (1. - (2.0 * y - 1.)) * (1. - (2.0 * z - 1.));

          if (i_c * ratioi != i) {
            cols[nc] = idx_c[col + 1];
            v[nc++]  = .125 * (1. + (2.0 * x - 1.)) * (1. - (2.0 * y - 1.)) * (1. - (2.0 * z - 1.));
          }

          if (j_c * ratioj != j) {
            cols[nc] = idx_c[col + m_ghost_c];
            v[nc++]  = .125 * (1. - (2.0 * x - 1.)) * (1. + (2.0 * y - 1.)) * (1. - (2.0 * z - 1.));
          }

          if (l_c * ratiok != l) {
            cols[nc] = idx_c[col + m_ghost_c * n_ghost_c];
            v[nc++]  = .125 * (1. - (2.0 * x - 1.)) * (1. - (2.0 * y - 1.)) * (1. + (2.0 * z - 1.));
          }

          if (j_c * ratioj != j && i_c * ratioi != i) {
            cols[nc] = idx_c[col + (m_ghost_c + 1)];
            v[nc++]  = .125 * (1. + (2.0 * x - 1.)) * (1. + (2.0 * y - 1.)) * (1. - (2.0 * z - 1.));
          }

          if (j_c * ratioj != j && l_c * ratiok != l) {
            cols[nc] = idx_c[col + (m_ghost_c * n_ghost_c + m_ghost_c)];
            v[nc++]  = .125 * (1. - (2.0 * x - 1.)) * (1. + (2.0 * y - 1.)) * (1. + (2.0 * z - 1.));
          }

          if (i_c * ratioi != i && l_c * ratiok != l) {
            cols[nc] = idx_c[col + (m_ghost_c * n_ghost_c + 1)];
            v[nc++]  = .125 * (1. + (2.0 * x - 1.)) * (1. - (2.0 * y - 1.)) * (1. + (2.0 * z - 1.));
          }

          if (i_c * ratioi != i && l_c * ratiok != l && j_c * ratioj != j) {
            cols[nc] = idx_c[col + (m_ghost_c * n_ghost_c + m_ghost_c + 1)];
            v[nc++]  = .125 * (1. + (2.0 * x - 1.)) * (1. + (2.0 * y - 1.)) * (1. + (2.0 * z - 1.));
          }
          PetscCall(MatSetValues(mat, 1, &row, nc, cols, v, INSERT_VALUES));
        }
      }
    }

  } else {
    PetscScalar *xi, *eta, *zeta;
    PetscInt     li, nxi, lj, neta, lk, nzeta, n;
    PetscScalar  Ni[8];

    /* compute local coordinate arrays */
    nxi   = ratioi + 1;
    neta  = ratioj + 1;
    nzeta = ratiok + 1;
    PetscCall(PetscMalloc1(nxi, &xi));
    PetscCall(PetscMalloc1(neta, &eta));
    PetscCall(PetscMalloc1(nzeta, &zeta));
    for (li = 0; li < nxi; li++) xi[li] = -1.0 + (PetscScalar)li * (2.0 / (PetscScalar)(nxi - 1));
    for (lj = 0; lj < neta; lj++) eta[lj] = -1.0 + (PetscScalar)lj * (2.0 / (PetscScalar)(neta - 1));
    for (lk = 0; lk < nzeta; lk++) zeta[lk] = -1.0 + (PetscScalar)lk * (2.0 / (PetscScalar)(nzeta - 1));

    for (l = l_start; l < l_start + p_f; l++) {
      for (j = j_start; j < j_start + n_f; j++) {
        for (i = i_start; i < i_start + m_f; i++) {
          /* convert to local "natural" numbering and then to PETSc global numbering */
          row = idx_f[(m_ghost * n_ghost * (l - l_start_ghost) + m_ghost * (j - j_start_ghost) + (i - i_start_ghost))];

          i_c = (i / ratioi);
          j_c = (j / ratioj);
          l_c = (l / ratiok);

          /* remainders */
          li = i - ratioi * (i / ratioi);
          if (i == mx - 1) li = nxi - 1;
          lj = j - ratioj * (j / ratioj);
          if (j == my - 1) lj = neta - 1;
          lk = l - ratiok * (l / ratiok);
          if (l == mz - 1) lk = nzeta - 1;

          /* corners */
          col     = (m_ghost_c * n_ghost_c * (l_c - l_start_ghost_c) + m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
          cols[0] = idx_c[col];
          Ni[0]   = 1.0;
          if ((li == 0) || (li == nxi - 1)) {
            if ((lj == 0) || (lj == neta - 1)) {
              if ((lk == 0) || (lk == nzeta - 1)) {
                PetscCall(MatSetValue(mat, row, cols[0], Ni[0], INSERT_VALUES));
                continue;
              }
            }
          }

          /* edges + interior */
          /* remainders */
          if (i == mx - 1) i_c--;
          if (j == my - 1) j_c--;
          if (l == mz - 1) l_c--;

          col     = (m_ghost_c * n_ghost_c * (l_c - l_start_ghost_c) + m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c));
          cols[0] = idx_c[col];                   /* one left and below; or we are right on it */
          cols[1] = idx_c[col + 1];               /* one right and below */
          cols[2] = idx_c[col + m_ghost_c];       /* one left and above */
          cols[3] = idx_c[col + (m_ghost_c + 1)]; /* one right and above */

          cols[4] = idx_c[col + m_ghost_c * n_ghost_c];                   /* one left and below and front; or we are right on it */
          cols[5] = idx_c[col + (m_ghost_c * n_ghost_c + 1)];             /* one right and below, and front */
          cols[6] = idx_c[col + (m_ghost_c * n_ghost_c + m_ghost_c)];     /* one left and above and front*/
          cols[7] = idx_c[col + (m_ghost_c * n_ghost_c + m_ghost_c + 1)]; /* one right and above and front */

          Ni[0] = 0.125 * (1.0 - xi[li]) * (1.0 - eta[lj]) * (1.0 - zeta[lk]);
          Ni[1] = 0.125 * (1.0 + xi[li]) * (1.0 - eta[lj]) * (1.0 - zeta[lk]);
          Ni[2] = 0.125 * (1.0 - xi[li]) * (1.0 + eta[lj]) * (1.0 - zeta[lk]);
          Ni[3] = 0.125 * (1.0 + xi[li]) * (1.0 + eta[lj]) * (1.0 - zeta[lk]);

          Ni[4] = 0.125 * (1.0 - xi[li]) * (1.0 - eta[lj]) * (1.0 + zeta[lk]);
          Ni[5] = 0.125 * (1.0 + xi[li]) * (1.0 - eta[lj]) * (1.0 + zeta[lk]);
          Ni[6] = 0.125 * (1.0 - xi[li]) * (1.0 + eta[lj]) * (1.0 + zeta[lk]);
          Ni[7] = 0.125 * (1.0 + xi[li]) * (1.0 + eta[lj]) * (1.0 + zeta[lk]);

          for (n = 0; n < 8; n++) {
            if (PetscAbsScalar(Ni[n]) < 1.0e-32) cols[n] = -1;
          }
          PetscCall(MatSetValues(mat, 1, &row, 8, cols, Ni, INSERT_VALUES));
        }
      }
    }
    PetscCall(PetscFree(xi));
    PetscCall(PetscFree(eta));
    PetscCall(PetscFree(zeta));
  }
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_c, &idx_c));
  PetscCall(MatAssemblyBegin(mat, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(mat, MAT_FINAL_ASSEMBLY));

  PetscCall(MatCreateMAIJ(mat, dof, A));
  PetscCall(MatDestroy(&mat));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInterpolation_DA(DM dac, DM daf, Mat *A, Vec *scale)
{
  PetscInt        dimc, Mc, Nc, Pc, mc, nc, pc, dofc, sc, dimf, Mf, Nf, Pf, mf, nf, pf, doff, sf;
  DMBoundaryType  bxc, byc, bzc, bxf, byf, bzf;
  DMDAStencilType stc, stf;
  DM_DA          *ddc = (DM_DA *)dac->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dac, DM_CLASSID, 1);
  PetscValidHeaderSpecific(daf, DM_CLASSID, 2);
  PetscValidPointer(A, 3);
  if (scale) PetscValidPointer(scale, 4);

  PetscCall(DMDAGetInfo(dac, &dimc, &Mc, &Nc, &Pc, &mc, &nc, &pc, &dofc, &sc, &bxc, &byc, &bzc, &stc));
  PetscCall(DMDAGetInfo(daf, &dimf, &Mf, &Nf, &Pf, &mf, &nf, &pf, &doff, &sf, &bxf, &byf, &bzf, &stf));
  PetscCheck(dimc == dimf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Dimensions of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, dimc, dimf);
  PetscCheck(dofc == doff, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "DOF of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, dofc, doff);
  PetscCheck(sc == sf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Stencil width of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, sc, sf);
  PetscCheck(bxc == bxf && byc == byf && bzc == bzf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Boundary type different in two DMDAs");
  PetscCheck(stc == stf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Stencil type different in two DMDAs");
  PetscCheck(Mc >= 2 || Mf <= 1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Coarse grid requires at least 2 points in x direction");
  PetscCheck(dimc <= 1 || Nc >= 2 || Nf <= 1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Coarse grid requires at least 2 points in y direction");
  PetscCheck(dimc <= 2 || Pc >= 2 || Pf <= 1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Coarse grid requires at least 2 points in z direction");

  if (ddc->interptype == DMDA_Q1) {
    if (dimc == 1) {
      PetscCall(DMCreateInterpolation_DA_1D_Q1(dac, daf, A));
    } else if (dimc == 2) {
      PetscCall(DMCreateInterpolation_DA_2D_Q1(dac, daf, A));
    } else if (dimc == 3) {
      PetscCall(DMCreateInterpolation_DA_3D_Q1(dac, daf, A));
    } else SETERRQ(PetscObjectComm((PetscObject)daf), PETSC_ERR_SUP, "No support for this DMDA dimension %" PetscInt_FMT " for interpolation type %d", dimc, (int)ddc->interptype);
  } else if (ddc->interptype == DMDA_Q0) {
    if (dimc == 1) {
      PetscCall(DMCreateInterpolation_DA_1D_Q0(dac, daf, A));
    } else if (dimc == 2) {
      PetscCall(DMCreateInterpolation_DA_2D_Q0(dac, daf, A));
    } else if (dimc == 3) {
      PetscCall(DMCreateInterpolation_DA_3D_Q0(dac, daf, A));
    } else SETERRQ(PetscObjectComm((PetscObject)daf), PETSC_ERR_SUP, "No support for this DMDA dimension %" PetscInt_FMT " for interpolation type %d", dimc, (int)ddc->interptype);
  }
  if (scale) PetscCall(DMCreateInterpolationScale((DM)dac, (DM)daf, *A, scale));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInjection_DA_1D(DM dac, DM daf, VecScatter *inject)
{
  PetscInt               i, i_start, m_f, Mx, dof;
  const PetscInt        *idx_f;
  ISLocalToGlobalMapping ltog_f;
  PetscInt               m_ghost, m_ghost_c;
  PetscInt               row, i_start_ghost, mx, m_c, nc, ratioi;
  PetscInt               i_start_c, i_start_ghost_c;
  PetscInt              *cols;
  DMBoundaryType         bx;
  Vec                    vecf, vecc;
  IS                     isf;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bx, NULL, NULL, NULL));
  PetscCall(DMDAGetInfo(daf, NULL, &mx, NULL, NULL, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  if (bx == DM_BOUNDARY_PERIODIC) {
    ratioi = mx / Mx;
    PetscCheck(ratioi * Mx == mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mx/Mx  must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  } else {
    ratioi = (mx - 1) / (Mx - 1);
    PetscCheck(ratioi * (Mx - 1) == mx - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mx - 1)/(Mx - 1) must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  }

  PetscCall(DMDAGetCorners(daf, &i_start, NULL, NULL, &m_f, NULL, NULL));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, NULL, NULL, &m_ghost, NULL, NULL));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, NULL, NULL, &m_c, NULL, NULL));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, NULL, NULL, &m_ghost_c, NULL, NULL));

  /* loop over local fine grid nodes setting interpolation for those*/
  nc = 0;
  PetscCall(PetscMalloc1(m_f, &cols));

  for (i = i_start_c; i < i_start_c + m_c; i++) {
    PetscInt i_f = i * ratioi;

    PetscCheck(i_f >= i_start_ghost && i_f < i_start_ghost + m_ghost, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\ni_c %" PetscInt_FMT " i_f %" PetscInt_FMT " fine ghost range [%" PetscInt_FMT ",%" PetscInt_FMT "]", i, i_f, i_start_ghost, i_start_ghost + m_ghost);

    row        = idx_f[(i_f - i_start_ghost)];
    cols[nc++] = row;
  }

  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISCreateBlock(PetscObjectComm((PetscObject)daf), dof, nc, cols, PETSC_OWN_POINTER, &isf));
  PetscCall(DMGetGlobalVector(dac, &vecc));
  PetscCall(DMGetGlobalVector(daf, &vecf));
  PetscCall(VecScatterCreate(vecf, isf, vecc, NULL, inject));
  PetscCall(DMRestoreGlobalVector(dac, &vecc));
  PetscCall(DMRestoreGlobalVector(daf, &vecf));
  PetscCall(ISDestroy(&isf));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInjection_DA_2D(DM dac, DM daf, VecScatter *inject)
{
  PetscInt               i, j, i_start, j_start, m_f, n_f, Mx, My, dof;
  const PetscInt        *idx_c, *idx_f;
  ISLocalToGlobalMapping ltog_f, ltog_c;
  PetscInt               m_ghost, n_ghost, m_ghost_c, n_ghost_c;
  PetscInt               row, i_start_ghost, j_start_ghost, mx, m_c, my, nc, ratioi, ratioj;
  PetscInt               i_start_c, j_start_c, n_c, i_start_ghost_c, j_start_ghost_c;
  PetscInt              *cols;
  DMBoundaryType         bx, by;
  Vec                    vecf, vecc;
  IS                     isf;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, &My, NULL, NULL, NULL, NULL, NULL, NULL, &bx, &by, NULL, NULL));
  PetscCall(DMDAGetInfo(daf, NULL, &mx, &my, NULL, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  if (bx == DM_BOUNDARY_PERIODIC) {
    ratioi = mx / Mx;
    PetscCheck(ratioi * Mx == mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mx/Mx  must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  } else {
    ratioi = (mx - 1) / (Mx - 1);
    PetscCheck(ratioi * (Mx - 1) == mx - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mx - 1)/(Mx - 1) must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  }
  if (by == DM_BOUNDARY_PERIODIC) {
    ratioj = my / My;
    PetscCheck(ratioj * My == my, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: my/My  must be integer: my %" PetscInt_FMT " My %" PetscInt_FMT, my, My);
  } else {
    ratioj = (my - 1) / (My - 1);
    PetscCheck(ratioj * (My - 1) == my - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (my - 1)/(My - 1) must be integer: my %" PetscInt_FMT " My %" PetscInt_FMT, my, My);
  }

  PetscCall(DMDAGetCorners(daf, &i_start, &j_start, NULL, &m_f, &n_f, NULL));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, &j_start_ghost, NULL, &m_ghost, &n_ghost, NULL));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, &j_start_c, NULL, &m_c, &n_c, NULL));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, &j_start_ghost_c, NULL, &m_ghost_c, &n_ghost_c, NULL));
  PetscCall(DMGetLocalToGlobalMapping(dac, &ltog_c));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_c, &idx_c));

  /* loop over local fine grid nodes setting interpolation for those*/
  nc = 0;
  PetscCall(PetscMalloc1(n_f * m_f, &cols));
  for (j = j_start_c; j < j_start_c + n_c; j++) {
    for (i = i_start_c; i < i_start_c + m_c; i++) {
      PetscInt i_f = i * ratioi, j_f = j * ratioj;
      PetscCheck(j_f >= j_start_ghost && j_f < j_start_ghost + n_ghost, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
    j_c %" PetscInt_FMT " j_f %" PetscInt_FMT " fine ghost range [%" PetscInt_FMT ",%" PetscInt_FMT "]",
                 j, j_f, j_start_ghost, j_start_ghost + n_ghost);
      PetscCheck(i_f >= i_start_ghost && i_f < i_start_ghost + m_ghost, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Processor's coarse DMDA must lie over fine DMDA\n\
    i_c %" PetscInt_FMT " i_f %" PetscInt_FMT " fine ghost range [%" PetscInt_FMT ",%" PetscInt_FMT "]",
                 i, i_f, i_start_ghost, i_start_ghost + m_ghost);
      row        = idx_f[(m_ghost * (j_f - j_start_ghost) + (i_f - i_start_ghost))];
      cols[nc++] = row;
    }
  }
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_c, &idx_c));

  PetscCall(ISCreateBlock(PetscObjectComm((PetscObject)daf), dof, nc, cols, PETSC_OWN_POINTER, &isf));
  PetscCall(DMGetGlobalVector(dac, &vecc));
  PetscCall(DMGetGlobalVector(daf, &vecf));
  PetscCall(VecScatterCreate(vecf, isf, vecc, NULL, inject));
  PetscCall(DMRestoreGlobalVector(dac, &vecc));
  PetscCall(DMRestoreGlobalVector(daf, &vecf));
  PetscCall(ISDestroy(&isf));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInjection_DA_3D(DM dac, DM daf, VecScatter *inject)
{
  PetscInt               i, j, k, i_start, j_start, k_start, m_f, n_f, p_f, Mx, My, Mz;
  PetscInt               m_ghost, n_ghost, p_ghost, m_ghost_c, n_ghost_c, p_ghost_c;
  PetscInt               i_start_ghost, j_start_ghost, k_start_ghost;
  PetscInt               mx, my, mz, ratioi, ratioj, ratiok;
  PetscInt               i_start_c, j_start_c, k_start_c;
  PetscInt               m_c, n_c, p_c;
  PetscInt               i_start_ghost_c, j_start_ghost_c, k_start_ghost_c;
  PetscInt               row, nc, dof;
  const PetscInt        *idx_c, *idx_f;
  ISLocalToGlobalMapping ltog_f, ltog_c;
  PetscInt              *cols;
  DMBoundaryType         bx, by, bz;
  Vec                    vecf, vecc;
  IS                     isf;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(dac, NULL, &Mx, &My, &Mz, NULL, NULL, NULL, NULL, NULL, &bx, &by, &bz, NULL));
  PetscCall(DMDAGetInfo(daf, NULL, &mx, &my, &mz, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));

  if (bx == DM_BOUNDARY_PERIODIC) {
    ratioi = mx / Mx;
    PetscCheck(ratioi * Mx == mx, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mx/Mx  must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  } else {
    ratioi = (mx - 1) / (Mx - 1);
    PetscCheck(ratioi * (Mx - 1) == mx - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mx - 1)/(Mx - 1) must be integer: mx %" PetscInt_FMT " Mx %" PetscInt_FMT, mx, Mx);
  }
  if (by == DM_BOUNDARY_PERIODIC) {
    ratioj = my / My;
    PetscCheck(ratioj * My == my, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: my/My  must be integer: my %" PetscInt_FMT " My %" PetscInt_FMT, my, My);
  } else {
    ratioj = (my - 1) / (My - 1);
    PetscCheck(ratioj * (My - 1) == my - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (my - 1)/(My - 1) must be integer: my %" PetscInt_FMT " My %" PetscInt_FMT, my, My);
  }
  if (bz == DM_BOUNDARY_PERIODIC) {
    ratiok = mz / Mz;
    PetscCheck(ratiok * Mz == mz, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: mz/Mz  must be integer: mz %" PetscInt_FMT " My %" PetscInt_FMT, mz, Mz);
  } else {
    ratiok = (mz - 1) / (Mz - 1);
    PetscCheck(ratiok * (Mz - 1) == mz - 1, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Ratio between levels: (mz - 1)/(Mz - 1) must be integer: mz %" PetscInt_FMT " Mz %" PetscInt_FMT, mz, Mz);
  }

  PetscCall(DMDAGetCorners(daf, &i_start, &j_start, &k_start, &m_f, &n_f, &p_f));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, &j_start_ghost, &k_start_ghost, &m_ghost, &n_ghost, &p_ghost));
  PetscCall(DMGetLocalToGlobalMapping(daf, &ltog_f));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_f, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, &j_start_c, &k_start_c, &m_c, &n_c, &p_c));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, &j_start_ghost_c, &k_start_ghost_c, &m_ghost_c, &n_ghost_c, &p_ghost_c));
  PetscCall(DMGetLocalToGlobalMapping(dac, &ltog_c));
  PetscCall(ISLocalToGlobalMappingGetBlockIndices(ltog_c, &idx_c));

  /* loop over local fine grid nodes setting interpolation for those*/
  nc = 0;
  PetscCall(PetscMalloc1(n_f * m_f * p_f, &cols));
  for (k = k_start_c; k < k_start_c + p_c; k++) {
    for (j = j_start_c; j < j_start_c + n_c; j++) {
      for (i = i_start_c; i < i_start_c + m_c; i++) {
        PetscInt i_f = i * ratioi, j_f = j * ratioj, k_f = k * ratiok;
        PetscCheck(k_f >= k_start_ghost && k_f < k_start_ghost + p_ghost, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP,
                   "Processor's coarse DMDA must lie over fine DMDA  "
                   "k_c %" PetscInt_FMT " k_f %" PetscInt_FMT " fine ghost range [%" PetscInt_FMT ",%" PetscInt_FMT "]",
                   k, k_f, k_start_ghost, k_start_ghost + p_ghost);
        PetscCheck(j_f >= j_start_ghost && j_f < j_start_ghost + n_ghost, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP,
                   "Processor's coarse DMDA must lie over fine DMDA  "
                   "j_c %" PetscInt_FMT " j_f %" PetscInt_FMT " fine ghost range [%" PetscInt_FMT ",%" PetscInt_FMT "]",
                   j, j_f, j_start_ghost, j_start_ghost + n_ghost);
        PetscCheck(i_f >= i_start_ghost && i_f < i_start_ghost + m_ghost, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP,
                   "Processor's coarse DMDA must lie over fine DMDA  "
                   "i_c %" PetscInt_FMT " i_f %" PetscInt_FMT " fine ghost range [%" PetscInt_FMT ",%" PetscInt_FMT "]",
                   i, i_f, i_start_ghost, i_start_ghost + m_ghost);
        row        = idx_f[(m_ghost * n_ghost * (k_f - k_start_ghost) + m_ghost * (j_f - j_start_ghost) + (i_f - i_start_ghost))];
        cols[nc++] = row;
      }
    }
  }
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_f, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreBlockIndices(ltog_c, &idx_c));

  PetscCall(ISCreateBlock(PetscObjectComm((PetscObject)daf), dof, nc, cols, PETSC_OWN_POINTER, &isf));
  PetscCall(DMGetGlobalVector(dac, &vecc));
  PetscCall(DMGetGlobalVector(daf, &vecf));
  PetscCall(VecScatterCreate(vecf, isf, vecc, NULL, inject));
  PetscCall(DMRestoreGlobalVector(dac, &vecc));
  PetscCall(DMRestoreGlobalVector(daf, &vecf));
  PetscCall(ISDestroy(&isf));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMCreateInjection_DA(DM dac, DM daf, Mat *mat)
{
  PetscInt        dimc, Mc, Nc, Pc, mc, nc, pc, dofc, sc, dimf, Mf, Nf, Pf, mf, nf, pf, doff, sf;
  DMBoundaryType  bxc, byc, bzc, bxf, byf, bzf;
  DMDAStencilType stc, stf;
  VecScatter      inject = NULL;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dac, DM_CLASSID, 1);
  PetscValidHeaderSpecific(daf, DM_CLASSID, 2);
  PetscValidPointer(mat, 3);

  PetscCall(DMDAGetInfo(dac, &dimc, &Mc, &Nc, &Pc, &mc, &nc, &pc, &dofc, &sc, &bxc, &byc, &bzc, &stc));
  PetscCall(DMDAGetInfo(daf, &dimf, &Mf, &Nf, &Pf, &mf, &nf, &pf, &doff, &sf, &bxf, &byf, &bzf, &stf));
  PetscCheck(dimc == dimf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Dimensions of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, dimc, dimf);
  PetscCheck(dofc == doff, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "DOF of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, dofc, doff);
  PetscCheck(sc == sf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Stencil width of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, sc, sf);
  PetscCheck(bxc == bxf && byc == byf && bzc == bzf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Boundary type different in two DMDAs");
  PetscCheck(stc == stf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Stencil type different in two DMDAs");
  PetscCheck(Mc >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Coarse grid requires at least 2 points in x direction");
  PetscCheck(dimc <= 1 || Nc >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Coarse grid requires at least 2 points in y direction");
  PetscCheck(dimc <= 2 || Pc >= 2, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Coarse grid requires at least 2 points in z direction");

  if (dimc == 1) {
    PetscCall(DMCreateInjection_DA_1D(dac, daf, &inject));
  } else if (dimc == 2) {
    PetscCall(DMCreateInjection_DA_2D(dac, daf, &inject));
  } else if (dimc == 3) {
    PetscCall(DMCreateInjection_DA_3D(dac, daf, &inject));
  }
  PetscCall(MatCreateScatter(PetscObjectComm((PetscObject)inject), inject, mat));
  PetscCall(VecScatterDestroy(&inject));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   DMCreateAggregates - Deprecated, see DMDACreateAggregates.

   Level: intermediate
@*/
PetscErrorCode DMCreateAggregates(DM dac, DM daf, Mat *mat)
{
  return DMDACreateAggregates(dac, daf, mat);
}

/*@
   DMDACreateAggregates - Gets the aggregates that map between
   grids associated with two `DMDA`

   Collective

   Input Parameters:
+  dmc - the coarse grid `DMDA`
-  dmf - the fine grid `DMDA`

   Output Parameter:
.  rest - the restriction matrix (transpose of the projection matrix)

   Level: intermediate

   Note:
   This routine is not used by PETSc.
   It is not clear what its use case is and it may be removed in a future release.
   Users should contact petsc-maint@mcs.anl.gov if they plan to use it.

.seealso: `DMRefine()`, `DMCreateInjection()`, `DMCreateInterpolation()`
@*/
PetscErrorCode DMDACreateAggregates(DM dac, DM daf, Mat *rest)
{
  PetscInt               dimc, Mc, Nc, Pc, mc, nc, pc, dofc, sc;
  PetscInt               dimf, Mf, Nf, Pf, mf, nf, pf, doff, sf;
  DMBoundaryType         bxc, byc, bzc, bxf, byf, bzf;
  DMDAStencilType        stc, stf;
  PetscInt               i, j, l;
  PetscInt               i_start, j_start, l_start, m_f, n_f, p_f;
  PetscInt               i_start_ghost, j_start_ghost, l_start_ghost, m_ghost, n_ghost, p_ghost;
  const PetscInt        *idx_f;
  PetscInt               i_c, j_c, l_c;
  PetscInt               i_start_c, j_start_c, l_start_c, m_c, n_c, p_c;
  PetscInt               i_start_ghost_c, j_start_ghost_c, l_start_ghost_c, m_ghost_c, n_ghost_c, p_ghost_c;
  const PetscInt        *idx_c;
  PetscInt               d;
  PetscInt               a;
  PetscInt               max_agg_size;
  PetscInt              *fine_nodes;
  PetscScalar           *one_vec;
  PetscInt               fn_idx;
  ISLocalToGlobalMapping ltogmf, ltogmc;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(dac, DM_CLASSID, 1, DMDA);
  PetscValidHeaderSpecificType(daf, DM_CLASSID, 2, DMDA);
  PetscValidPointer(rest, 3);

  PetscCall(DMDAGetInfo(dac, &dimc, &Mc, &Nc, &Pc, &mc, &nc, &pc, &dofc, &sc, &bxc, &byc, &bzc, &stc));
  PetscCall(DMDAGetInfo(daf, &dimf, &Mf, &Nf, &Pf, &mf, &nf, &pf, &doff, &sf, &bxf, &byf, &bzf, &stf));
  PetscCheck(dimc == dimf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Dimensions of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, dimc, dimf);
  PetscCheck(dofc == doff, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "DOF of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, dofc, doff);
  PetscCheck(sc == sf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Stencil width of DMDA do not match %" PetscInt_FMT " %" PetscInt_FMT, sc, sf);
  PetscCheck(bxc == bxf && byc == byf && bzc == bzf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Boundary type different in two DMDAs");
  PetscCheck(stc == stf, PetscObjectComm((PetscObject)daf), PETSC_ERR_ARG_INCOMP, "Stencil type different in two DMDAs");

  PetscCheck(Mf >= Mc, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Coarse grid has more points than fine grid, Mc %" PetscInt_FMT ", Mf %" PetscInt_FMT, Mc, Mf);
  PetscCheck(Nf >= Nc, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Coarse grid has more points than fine grid, Nc %" PetscInt_FMT ", Nf %" PetscInt_FMT, Nc, Nf);
  PetscCheck(Pf >= Pc, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Coarse grid has more points than fine grid, Pc %" PetscInt_FMT ", Pf %" PetscInt_FMT, Pc, Pf);

  if (Pc < 0) Pc = 1;
  if (Pf < 0) Pf = 1;
  if (Nc < 0) Nc = 1;
  if (Nf < 0) Nf = 1;

  PetscCall(DMDAGetCorners(daf, &i_start, &j_start, &l_start, &m_f, &n_f, &p_f));
  PetscCall(DMDAGetGhostCorners(daf, &i_start_ghost, &j_start_ghost, &l_start_ghost, &m_ghost, &n_ghost, &p_ghost));

  PetscCall(DMGetLocalToGlobalMapping(daf, &ltogmf));
  PetscCall(ISLocalToGlobalMappingGetIndices(ltogmf, &idx_f));

  PetscCall(DMDAGetCorners(dac, &i_start_c, &j_start_c, &l_start_c, &m_c, &n_c, &p_c));
  PetscCall(DMDAGetGhostCorners(dac, &i_start_ghost_c, &j_start_ghost_c, &l_start_ghost_c, &m_ghost_c, &n_ghost_c, &p_ghost_c));

  PetscCall(DMGetLocalToGlobalMapping(dac, &ltogmc));
  PetscCall(ISLocalToGlobalMappingGetIndices(ltogmc, &idx_c));

  /*
     Basic idea is as follows. Here's a 2D example, suppose r_x, r_y are the ratios
     for dimension 1 and 2 respectively.
     Let (i,j) be a coarse grid node. All the fine grid nodes between r_x*i and r_x*(i+1)
     and r_y*j and r_y*(j+1) will be grouped into the same coarse grid aggregate.
     Each specific dof on the fine grid is mapped to one dof on the coarse grid.
  */

  max_agg_size = (Mf / Mc + 1) * (Nf / Nc + 1) * (Pf / Pc + 1);

  /* create the matrix that will contain the restriction operator */
  PetscCall(MatCreateAIJ(PetscObjectComm((PetscObject)daf), m_c * n_c * p_c * dofc, m_f * n_f * p_f * doff, Mc * Nc * Pc * dofc, Mf * Nf * Pf * doff, max_agg_size, NULL, max_agg_size, NULL, rest));

  /* store nodes in the fine grid here */
  PetscCall(PetscMalloc2(max_agg_size, &one_vec, max_agg_size, &fine_nodes));
  for (i = 0; i < max_agg_size; i++) one_vec[i] = 1.0;

  /* loop over all coarse nodes */
  for (l_c = l_start_c; l_c < l_start_c + p_c; l_c++) {
    for (j_c = j_start_c; j_c < j_start_c + n_c; j_c++) {
      for (i_c = i_start_c; i_c < i_start_c + m_c; i_c++) {
        for (d = 0; d < dofc; d++) {
          /* convert to local "natural" numbering and then to PETSc global numbering */
          a = idx_c[dofc * (m_ghost_c * n_ghost_c * (l_c - l_start_ghost_c) + m_ghost_c * (j_c - j_start_ghost_c) + (i_c - i_start_ghost_c))] + d;

          fn_idx = 0;
          /* Corresponding fine points are all points (i_f, j_f, l_f) such that
             i_c*Mf/Mc <= i_f < (i_c+1)*Mf/Mc
             (same for other dimensions)
          */
          for (l = l_c * Pf / Pc; l < PetscMin((l_c + 1) * Pf / Pc, Pf); l++) {
            for (j = j_c * Nf / Nc; j < PetscMin((j_c + 1) * Nf / Nc, Nf); j++) {
              for (i = i_c * Mf / Mc; i < PetscMin((i_c + 1) * Mf / Mc, Mf); i++) {
                fine_nodes[fn_idx] = idx_f[doff * (m_ghost * n_ghost * (l - l_start_ghost) + m_ghost * (j - j_start_ghost) + (i - i_start_ghost))] + d;
                fn_idx++;
              }
            }
          }
          /* add all these points to one aggregate */
          PetscCall(MatSetValues(*rest, 1, &a, fn_idx, fine_nodes, one_vec, INSERT_VALUES));
        }
      }
    }
  }
  PetscCall(ISLocalToGlobalMappingRestoreIndices(ltogmf, &idx_f));
  PetscCall(ISLocalToGlobalMappingRestoreIndices(ltogmc, &idx_c));
  PetscCall(PetscFree2(one_vec, fine_nodes));
  PetscCall(MatAssemblyBegin(*rest, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(*rest, MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(PETSC_SUCCESS);
}

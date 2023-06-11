#include <petscvec_kokkos.hpp>
#include <../src/vec/vec/impls/seq/kokkos/veckokkosimpl.hpp>
#include <petscdevice.h>
#include <../src/ksp/pc/impls/vpbjacobi/vpbjacobi.h>

/* A class that manages helper arrays assisting parallel PCApply() with Kokkos */
struct PC_VPBJacobi_Kokkos {
  /* Cache the old sizes to check if we need realloc */
  PetscInt n;       /* number of rows of the local matrix */
  PetscInt nblocks; /* number of point blocks */
  PetscInt nsize;   /* sum of sizes of the point blocks */

  /* Helper arrays that are pre-computed on host and then copied to device.
    bs:     [nblocks+1], "csr" version of bsizes[]
    bs2:    [nblocks+1], "csr" version of squares of bsizes[]
    matIdx: [n], row i of the local matrix belongs to the matIdx_d[i] block
  */
  PetscIntKokkosDualView    bs_dual, bs2_dual, matIdx_dual;
  PetscScalarKokkosDualView diag_dual;

  PC_VPBJacobi_Kokkos(PetscInt n, PetscInt nblocks, PetscInt nsize, const PetscInt *bsizes, MatScalar *diag_ptr_h) :
    n(n), nblocks(nblocks), nsize(nsize), bs_dual("bs_dual", nblocks + 1), bs2_dual("bs2_dual", nblocks + 1), matIdx_dual("matIdx_dual", n)
  {
    PetscScalarKokkosViewHost diag_h(diag_ptr_h, nsize);

    auto diag_d = Kokkos::create_mirror_view(DefaultMemorySpace(), diag_h);
    diag_dual   = PetscScalarKokkosDualView(diag_d, diag_h);
    PetscCallVoid(UpdateOffsetsOnDevice(bsizes, diag_ptr_h));
  }

  PetscErrorCode UpdateOffsetsOnDevice(const PetscInt *bsizes, MatScalar *diag_ptr_h)
  {
    PetscFunctionBegin;
    PetscCheck(diag_dual.view_host().data() == diag_ptr_h, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Host pointer has changed since last call");
    PetscCall(ComputeOffsetsOnHost(bsizes));

    PetscCallCXX(bs_dual.modify_host());
    PetscCallCXX(bs2_dual.modify_host());
    PetscCallCXX(matIdx_dual.modify_host());
    PetscCallCXX(diag_dual.modify_host());

    PetscCallCXX(bs_dual.sync_device());
    PetscCallCXX(bs2_dual.sync_device());
    PetscCallCXX(matIdx_dual.sync_device());
    PetscCallCXX(diag_dual.sync_device());
    PetscCall(PetscLogCpuToGpu(sizeof(PetscInt) * (2 * nblocks + 2 + n) + sizeof(MatScalar) * nsize));
    PetscFunctionReturn(PETSC_SUCCESS);
  }

private:
  PetscErrorCode ComputeOffsetsOnHost(const PetscInt *bsizes)
  {
    PetscInt *bs_h     = bs_dual.view_host().data();
    PetscInt *bs2_h    = bs2_dual.view_host().data();
    PetscInt *matIdx_h = matIdx_dual.view_host().data();

    PetscFunctionBegin;
    bs_h[0] = bs2_h[0] = 0;
    for (PetscInt i = 0; i < nblocks; i++) {
      bs_h[i + 1]  = bs_h[i] + bsizes[i];
      bs2_h[i + 1] = bs2_h[i] + bsizes[i] * bsizes[i];
      for (PetscInt j = 0; j < bsizes[i]; j++) matIdx_h[bs_h[i] + j] = i;
    }
    PetscFunctionReturn(PETSC_SUCCESS);
  }
};

template <PetscBool transpose>
static PetscErrorCode PCApplyOrTranspose_VPBJacobi_Kokkos(PC pc, Vec x, Vec y)
{
  PC_VPBJacobi              *jac   = (PC_VPBJacobi *)pc->data;
  PC_VPBJacobi_Kokkos       *pckok = static_cast<PC_VPBJacobi_Kokkos *>(jac->spptr);
  ConstPetscScalarKokkosView xv;
  PetscScalarKokkosView      yv;
  PetscScalarKokkosView      diag   = pckok->diag_dual.view_device();
  PetscIntKokkosView         bs     = pckok->bs_dual.view_device();
  PetscIntKokkosView         bs2    = pckok->bs2_dual.view_device();
  PetscIntKokkosView         matIdx = pckok->matIdx_dual.view_device();
  const char                *label  = transpose ? "PCApplyTranspose_VPBJacobi_Kokkos" : "PCApply_VPBJacobi_Kokkos";

  PetscFunctionBegin;
  PetscCall(PetscLogGpuTimeBegin());
  VecErrorIfNotKokkos(x);
  VecErrorIfNotKokkos(y);
  PetscCall(VecGetKokkosView(x, &xv));
  PetscCall(VecGetKokkosViewWrite(y, &yv));
  PetscCallCXX(Kokkos::parallel_for(
    label, pckok->n, KOKKOS_LAMBDA(PetscInt row) {
      const PetscScalar *Ap, *xp;
      PetscScalar       *yp;
      PetscInt           i, j, k, m;

      k  = matIdx(row);                             /* k-th block/matrix */
      m  = bs(k + 1) - bs(k);                       /* block size of the k-th block */
      i  = row - bs(k);                             /* i-th row of the block */
      Ap = &diag(bs2(k) + i * (transpose ? m : 1)); /* Ap points to the first entry of i-th row/column */
      xp = &xv(bs(k));
      yp = &yv(bs(k));

      yp[i] = 0.0;
      for (j = 0; j < m; j++) {
        yp[i] += Ap[0] * xp[j];
        Ap += transpose ? 1 : m;
      }
    }));
  PetscCall(VecRestoreKokkosView(x, &xv));
  PetscCall(VecRestoreKokkosViewWrite(y, &yv));
  PetscCall(PetscLogGpuFlops(pckok->nsize * 2)); /* FMA on entries in all blocks */
  PetscCall(PetscLogGpuTimeEnd());
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCDestroy_VPBJacobi_Kokkos(PC pc)
{
  PC_VPBJacobi *jac = (PC_VPBJacobi *)pc->data;

  PetscFunctionBegin;
  PetscCallCXX(delete static_cast<PC_VPBJacobi_Kokkos *>(jac->spptr));
  PetscCall(PCDestroy_VPBJacobi(pc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode PCSetUp_VPBJacobi_Kokkos(PC pc)
{
  PC_VPBJacobi        *jac   = (PC_VPBJacobi *)pc->data;
  PC_VPBJacobi_Kokkos *pckok = static_cast<PC_VPBJacobi_Kokkos *>(jac->spptr);
  PetscInt             i, n, nblocks, nsize = 0;
  const PetscInt      *bsizes;

  PetscFunctionBegin;
  PetscCall(PCSetUp_VPBJacobi_Host(pc)); /* Compute the inverse on host now. Might worth doing it on device directly */
  PetscCall(MatGetVariableBlockSizes(pc->pmat, &nblocks, &bsizes));
  for (i = 0; i < nblocks; i++) nsize += bsizes[i] * bsizes[i];
  PetscCall(MatGetLocalSize(pc->pmat, &n, NULL));

  /* If one calls MatSetVariableBlockSizes() multiple times and sizes have been changed (is it allowed?), we delete the old and rebuild anyway */
  if (pckok && (pckok->n != n || pckok->nblocks != nblocks || pckok->nsize != nsize)) {
    PetscCallCXX(delete pckok);
    pckok = nullptr;
  }

  if (!pckok) { /* allocate the struct along with the helper arrays from the scratch */
    PetscCallCXX(jac->spptr = new PC_VPBJacobi_Kokkos(n, nblocks, nsize, bsizes, jac->diag));
  } else { /* update the value only */
    PetscCall(pckok->UpdateOffsetsOnDevice(bsizes, jac->diag));
  }

  pc->ops->apply          = PCApplyOrTranspose_VPBJacobi_Kokkos<PETSC_FALSE>;
  pc->ops->applytranspose = PCApplyOrTranspose_VPBJacobi_Kokkos<PETSC_TRUE>;
  pc->ops->destroy        = PCDestroy_VPBJacobi_Kokkos;
  PetscFunctionReturn(PETSC_SUCCESS);
}

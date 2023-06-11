/* Portions of this code are under:
   Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
*/
#include <../src/mat/impls/aij/mpi/mpiaij.h> /*I "petscmat.h" I*/
#include <../src/mat/impls/aij/seq/seqhipsparse/hipsparsematimpl.h>
#include <../src/mat/impls/aij/mpi/mpihipsparse/mpihipsparsematimpl.h>
#include <thrust/advance.h>
#include <thrust/partition.h>
#include <thrust/sort.h>
#include <thrust/unique.h>
#include <petscsf.h>

struct VecHIPEquals {
  template <typename Tuple>
  __host__ __device__ void operator()(Tuple t)
  {
    thrust::get<1>(t) = thrust::get<0>(t);
  }
};

static PetscErrorCode MatResetPreallocationCOO_MPIAIJHIPSPARSE(Mat mat)
{
  auto *aij             = static_cast<Mat_MPIAIJ *>(mat->data);
  auto *hipsparseStruct = static_cast<Mat_MPIAIJHIPSPARSE *>(aij->spptr);

  PetscFunctionBegin;
  if (!hipsparseStruct) PetscFunctionReturn(PETSC_SUCCESS);
  if (hipsparseStruct->use_extended_coo) {
    PetscCallHIP(hipFree(hipsparseStruct->Ajmap1_d));
    PetscCallHIP(hipFree(hipsparseStruct->Aperm1_d));
    PetscCallHIP(hipFree(hipsparseStruct->Bjmap1_d));
    PetscCallHIP(hipFree(hipsparseStruct->Bperm1_d));
    PetscCallHIP(hipFree(hipsparseStruct->Aimap2_d));
    PetscCallHIP(hipFree(hipsparseStruct->Ajmap2_d));
    PetscCallHIP(hipFree(hipsparseStruct->Aperm2_d));
    PetscCallHIP(hipFree(hipsparseStruct->Bimap2_d));
    PetscCallHIP(hipFree(hipsparseStruct->Bjmap2_d));
    PetscCallHIP(hipFree(hipsparseStruct->Bperm2_d));
    PetscCallHIP(hipFree(hipsparseStruct->Cperm1_d));
    PetscCallHIP(hipFree(hipsparseStruct->sendbuf_d));
    PetscCallHIP(hipFree(hipsparseStruct->recvbuf_d));
  }
  hipsparseStruct->use_extended_coo = PETSC_FALSE;
  delete hipsparseStruct->coo_p;
  delete hipsparseStruct->coo_pw;
  hipsparseStruct->coo_p  = nullptr;
  hipsparseStruct->coo_pw = nullptr;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSetValuesCOO_MPIAIJHIPSPARSE_Basic(Mat A, const PetscScalar v[], InsertMode imode)
{
  Mat_MPIAIJ          *a    = (Mat_MPIAIJ *)A->data;
  Mat_MPIAIJHIPSPARSE *cusp = (Mat_MPIAIJHIPSPARSE *)a->spptr;
  PetscInt             n    = cusp->coo_nd + cusp->coo_no;

  PetscFunctionBegin;
  if (cusp->coo_p && v) {
    thrust::device_ptr<const PetscScalar> d_v;
    THRUSTARRAY                          *w = NULL;

    if (isHipMem(v)) {
      d_v = thrust::device_pointer_cast(v);
    } else {
      w = new THRUSTARRAY(n);
      w->assign(v, v + n);
      PetscCall(PetscLogCpuToGpu(n * sizeof(PetscScalar)));
      d_v = w->data();
    }

    auto zibit = thrust::make_zip_iterator(thrust::make_tuple(thrust::make_permutation_iterator(d_v, cusp->coo_p->begin()), cusp->coo_pw->begin()));
    auto zieit = thrust::make_zip_iterator(thrust::make_tuple(thrust::make_permutation_iterator(d_v, cusp->coo_p->end()), cusp->coo_pw->end()));
    PetscCall(PetscLogGpuTimeBegin());
    thrust::for_each(zibit, zieit, VecHIPEquals());
    PetscCall(PetscLogGpuTimeEnd());
    delete w;
    PetscCall(MatSetValuesCOO_SeqAIJHIPSPARSE_Basic(a->A, cusp->coo_pw->data().get(), imode));
    PetscCall(MatSetValuesCOO_SeqAIJHIPSPARSE_Basic(a->B, cusp->coo_pw->data().get() + cusp->coo_nd, imode));
  } else {
    PetscCall(MatSetValuesCOO_SeqAIJHIPSPARSE_Basic(a->A, v, imode));
    PetscCall(MatSetValuesCOO_SeqAIJHIPSPARSE_Basic(a->B, v ? v + cusp->coo_nd : nullptr, imode));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

template <typename Tuple>
struct IsNotOffDiagT {
  PetscInt _cstart, _cend;

  IsNotOffDiagT(PetscInt cstart, PetscInt cend) : _cstart(cstart), _cend(cend) { }
  __host__ __device__ bool operator()(Tuple t) { return !(thrust::get<1>(t) < _cstart || thrust::get<1>(t) >= _cend); }
};

struct IsOffDiag {
  PetscInt _cstart, _cend;

  IsOffDiag(PetscInt cstart, PetscInt cend) : _cstart(cstart), _cend(cend) { }
  __host__ __device__ bool operator()(const PetscInt &c) { return c < _cstart || c >= _cend; }
};

struct GlobToLoc {
  PetscInt _start;

  GlobToLoc(PetscInt start) : _start(start) { }
  __host__ __device__ PetscInt operator()(const PetscInt &c) { return c - _start; }
};

static PetscErrorCode MatSetPreallocationCOO_MPIAIJHIPSPARSE_Basic(Mat B, PetscCount n, PetscInt coo_i[], PetscInt coo_j[])
{
  Mat_MPIAIJ            *b    = (Mat_MPIAIJ *)B->data;
  Mat_MPIAIJHIPSPARSE   *cusp = (Mat_MPIAIJHIPSPARSE *)b->spptr;
  PetscInt               N, *jj;
  size_t                 noff = 0;
  THRUSTINTARRAY         d_i(n); /* on device, storing partitioned coo_i with diagonal first, and off-diag next */
  THRUSTINTARRAY         d_j(n);
  ISLocalToGlobalMapping l2g;

  PetscFunctionBegin;
  PetscCall(MatDestroy(&b->A));
  PetscCall(MatDestroy(&b->B));

  PetscCall(PetscLogCpuToGpu(2. * n * sizeof(PetscInt)));
  d_i.assign(coo_i, coo_i + n);
  d_j.assign(coo_j, coo_j + n);
  delete cusp->coo_p;
  delete cusp->coo_pw;
  cusp->coo_p  = NULL;
  cusp->coo_pw = NULL;
  PetscCall(PetscLogGpuTimeBegin());
  auto firstoffd = thrust::find_if(thrust::device, d_j.begin(), d_j.end(), IsOffDiag(B->cmap->rstart, B->cmap->rend));
  auto firstdiag = thrust::find_if_not(thrust::device, firstoffd, d_j.end(), IsOffDiag(B->cmap->rstart, B->cmap->rend));
  if (firstoffd != d_j.end() && firstdiag != d_j.end()) {
    cusp->coo_p  = new THRUSTINTARRAY(n);
    cusp->coo_pw = new THRUSTARRAY(n);
    thrust::sequence(thrust::device, cusp->coo_p->begin(), cusp->coo_p->end(), 0);
    auto fzipp = thrust::make_zip_iterator(thrust::make_tuple(d_i.begin(), d_j.begin(), cusp->coo_p->begin()));
    auto ezipp = thrust::make_zip_iterator(thrust::make_tuple(d_i.end(), d_j.end(), cusp->coo_p->end()));
    auto mzipp = thrust::partition(thrust::device, fzipp, ezipp, IsNotOffDiagT<thrust::tuple<PetscInt, PetscInt, PetscInt>>(B->cmap->rstart, B->cmap->rend));
    firstoffd  = mzipp.get_iterator_tuple().get<1>();
  }
  cusp->coo_nd = thrust::distance(d_j.begin(), firstoffd);
  cusp->coo_no = thrust::distance(firstoffd, d_j.end());

  /* from global to local */
  thrust::transform(thrust::device, d_i.begin(), d_i.end(), d_i.begin(), GlobToLoc(B->rmap->rstart));
  thrust::transform(thrust::device, d_j.begin(), firstoffd, d_j.begin(), GlobToLoc(B->cmap->rstart));
  PetscCall(PetscLogGpuTimeEnd());

  /* copy offdiag column indices to map on the CPU */
  PetscCall(PetscMalloc1(cusp->coo_no, &jj)); /* jj[] will store compacted col ids of the offdiag part */
  PetscCallHIP(hipMemcpy(jj, d_j.data().get() + cusp->coo_nd, cusp->coo_no * sizeof(PetscInt), hipMemcpyDeviceToHost));
  auto o_j = d_j.begin();
  PetscCall(PetscLogGpuTimeBegin());
  thrust::advance(o_j, cusp->coo_nd); /* sort and unique offdiag col ids */
  thrust::sort(thrust::device, o_j, d_j.end());
  auto wit = thrust::unique(thrust::device, o_j, d_j.end()); /* return end iter of the unique range */
  PetscCall(PetscLogGpuTimeEnd());
  noff = thrust::distance(o_j, wit);
  PetscCall(PetscMalloc1(noff, &b->garray));
  PetscCallHIP(hipMemcpy(b->garray, d_j.data().get() + cusp->coo_nd, noff * sizeof(PetscInt), hipMemcpyDeviceToHost));
  PetscCall(PetscLogGpuToCpu((noff + cusp->coo_no) * sizeof(PetscInt)));
  PetscCall(ISLocalToGlobalMappingCreate(PETSC_COMM_SELF, 1, noff, b->garray, PETSC_COPY_VALUES, &l2g));
  PetscCall(ISLocalToGlobalMappingSetType(l2g, ISLOCALTOGLOBALMAPPINGHASH));
  PetscCall(ISGlobalToLocalMappingApply(l2g, IS_GTOLM_DROP, cusp->coo_no, jj, &N, jj));
  PetscCheck(N == cusp->coo_no, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Unexpected is size %" PetscInt_FMT " != %" PetscInt_FMT " coo size", N, cusp->coo_no);
  PetscCall(ISLocalToGlobalMappingDestroy(&l2g));
  PetscCall(MatCreate(PETSC_COMM_SELF, &b->A));
  PetscCall(MatSetSizes(b->A, B->rmap->n, B->cmap->n, B->rmap->n, B->cmap->n));
  PetscCall(MatSetType(b->A, MATSEQAIJHIPSPARSE));
  PetscCall(MatCreate(PETSC_COMM_SELF, &b->B));
  PetscCall(MatSetSizes(b->B, B->rmap->n, noff, B->rmap->n, noff));
  PetscCall(MatSetType(b->B, MATSEQAIJHIPSPARSE));

  /* GPU memory, hipsparse specific call handles it internally */
  PetscCall(MatSetPreallocationCOO_SeqAIJHIPSPARSE_Basic(b->A, cusp->coo_nd, d_i.data().get(), d_j.data().get()));
  PetscCall(MatSetPreallocationCOO_SeqAIJHIPSPARSE_Basic(b->B, cusp->coo_no, d_i.data().get() + cusp->coo_nd, jj));
  PetscCall(PetscFree(jj));

  PetscCall(MatHIPSPARSESetFormat(b->A, MAT_HIPSPARSE_MULT, cusp->diagGPUMatFormat));
  PetscCall(MatHIPSPARSESetFormat(b->B, MAT_HIPSPARSE_MULT, cusp->offdiagGPUMatFormat));
  PetscCall(MatBindToCPU(b->A, B->boundtocpu));
  PetscCall(MatBindToCPU(b->B, B->boundtocpu));
  PetscCall(MatSetUpMultiply_MPIAIJ(B));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatSetPreallocationCOO_MPIAIJHIPSPARSE(Mat mat, PetscCount coo_n, PetscInt coo_i[], PetscInt coo_j[])
{
  Mat_MPIAIJ          *mpiaij = (Mat_MPIAIJ *)mat->data;
  Mat_MPIAIJHIPSPARSE *mpidev;
  PetscBool            coo_basic = PETSC_TRUE;
  PetscMemType         mtype     = PETSC_MEMTYPE_DEVICE;
  PetscInt             rstart, rend;

  PetscFunctionBegin;
  PetscCall(PetscFree(mpiaij->garray));
  PetscCall(VecDestroy(&mpiaij->lvec));
#if defined(PETSC_USE_CTABLE)
  PetscCall(PetscHMapIDestroy(&mpiaij->colmap));
#else
  PetscCall(PetscFree(mpiaij->colmap));
#endif
  PetscCall(VecScatterDestroy(&mpiaij->Mvctx));
  mat->assembled     = PETSC_FALSE;
  mat->was_assembled = PETSC_FALSE;
  PetscCall(MatResetPreallocationCOO_MPIAIJ(mat));
  PetscCall(MatResetPreallocationCOO_MPIAIJHIPSPARSE(mat));
  if (coo_i) {
    PetscCall(PetscLayoutGetRange(mat->rmap, &rstart, &rend));
    PetscCall(PetscGetMemType(coo_i, &mtype));
    if (PetscMemTypeHost(mtype)) {
      for (PetscCount k = 0; k < coo_n; k++) { /* Are there negative indices or remote entries? */
        if (coo_i[k] < 0 || coo_i[k] < rstart || coo_i[k] >= rend || coo_j[k] < 0) {
          coo_basic = PETSC_FALSE;
          break;
        }
      }
    }
  }
  /* All ranks must agree on the value of coo_basic */
  PetscCall(MPIU_Allreduce(MPI_IN_PLACE, &coo_basic, 1, MPIU_BOOL, MPI_LAND, PetscObjectComm((PetscObject)mat)));
  if (coo_basic) {
    PetscCall(MatSetPreallocationCOO_MPIAIJHIPSPARSE_Basic(mat, coo_n, coo_i, coo_j));
  } else {
    PetscCall(MatSetPreallocationCOO_MPIAIJ(mat, coo_n, coo_i, coo_j));
    mat->offloadmask = PETSC_OFFLOAD_CPU;
    /* creates the GPU memory */
    PetscCall(MatSeqAIJHIPSPARSECopyToGPU(mpiaij->A));
    PetscCall(MatSeqAIJHIPSPARSECopyToGPU(mpiaij->B));
    mpidev                   = static_cast<Mat_MPIAIJHIPSPARSE *>(mpiaij->spptr);
    mpidev->use_extended_coo = PETSC_TRUE;

    PetscCallHIP(hipMalloc((void **)&mpidev->Ajmap1_d, (mpiaij->Annz + 1) * sizeof(PetscCount)));
    PetscCallHIP(hipMalloc((void **)&mpidev->Aperm1_d, mpiaij->Atot1 * sizeof(PetscCount)));

    PetscCallHIP(hipMalloc((void **)&mpidev->Bjmap1_d, (mpiaij->Bnnz + 1) * sizeof(PetscCount)));
    PetscCallHIP(hipMalloc((void **)&mpidev->Bperm1_d, mpiaij->Btot1 * sizeof(PetscCount)));

    PetscCallHIP(hipMalloc((void **)&mpidev->Aimap2_d, mpiaij->Annz2 * sizeof(PetscCount)));
    PetscCallHIP(hipMalloc((void **)&mpidev->Ajmap2_d, (mpiaij->Annz2 + 1) * sizeof(PetscCount)));
    PetscCallHIP(hipMalloc((void **)&mpidev->Aperm2_d, mpiaij->Atot2 * sizeof(PetscCount)));

    PetscCallHIP(hipMalloc((void **)&mpidev->Bimap2_d, mpiaij->Bnnz2 * sizeof(PetscCount)));
    PetscCallHIP(hipMalloc((void **)&mpidev->Bjmap2_d, (mpiaij->Bnnz2 + 1) * sizeof(PetscCount)));
    PetscCallHIP(hipMalloc((void **)&mpidev->Bperm2_d, mpiaij->Btot2 * sizeof(PetscCount)));

    PetscCallHIP(hipMalloc((void **)&mpidev->Cperm1_d, mpiaij->sendlen * sizeof(PetscCount)));
    PetscCallHIP(hipMalloc((void **)&mpidev->sendbuf_d, mpiaij->sendlen * sizeof(PetscScalar)));
    PetscCallHIP(hipMalloc((void **)&mpidev->recvbuf_d, mpiaij->recvlen * sizeof(PetscScalar)));

    PetscCallHIP(hipMemcpy(mpidev->Ajmap1_d, mpiaij->Ajmap1, (mpiaij->Annz + 1) * sizeof(PetscCount), hipMemcpyHostToDevice));
    PetscCallHIP(hipMemcpy(mpidev->Aperm1_d, mpiaij->Aperm1, mpiaij->Atot1 * sizeof(PetscCount), hipMemcpyHostToDevice));

    PetscCallHIP(hipMemcpy(mpidev->Bjmap1_d, mpiaij->Bjmap1, (mpiaij->Bnnz + 1) * sizeof(PetscCount), hipMemcpyHostToDevice));
    PetscCallHIP(hipMemcpy(mpidev->Bperm1_d, mpiaij->Bperm1, mpiaij->Btot1 * sizeof(PetscCount), hipMemcpyHostToDevice));

    PetscCallHIP(hipMemcpy(mpidev->Aimap2_d, mpiaij->Aimap2, mpiaij->Annz2 * sizeof(PetscCount), hipMemcpyHostToDevice));
    PetscCallHIP(hipMemcpy(mpidev->Ajmap2_d, mpiaij->Ajmap2, (mpiaij->Annz2 + 1) * sizeof(PetscCount), hipMemcpyHostToDevice));
    PetscCallHIP(hipMemcpy(mpidev->Aperm2_d, mpiaij->Aperm2, mpiaij->Atot2 * sizeof(PetscCount), hipMemcpyHostToDevice));

    PetscCallHIP(hipMemcpy(mpidev->Bimap2_d, mpiaij->Bimap2, mpiaij->Bnnz2 * sizeof(PetscCount), hipMemcpyHostToDevice));
    PetscCallHIP(hipMemcpy(mpidev->Bjmap2_d, mpiaij->Bjmap2, (mpiaij->Bnnz2 + 1) * sizeof(PetscCount), hipMemcpyHostToDevice));
    PetscCallHIP(hipMemcpy(mpidev->Bperm2_d, mpiaij->Bperm2, mpiaij->Btot2 * sizeof(PetscCount), hipMemcpyHostToDevice));

    PetscCallHIP(hipMemcpy(mpidev->Cperm1_d, mpiaij->Cperm1, mpiaij->sendlen * sizeof(PetscCount), hipMemcpyHostToDevice));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

__global__ static void MatPackCOOValues(const PetscScalar kv[], PetscCount nnz, const PetscCount perm[], PetscScalar buf[])
{
  PetscCount       i         = blockIdx.x * blockDim.x + threadIdx.x;
  const PetscCount grid_size = gridDim.x * blockDim.x;
  for (; i < nnz; i += grid_size) buf[i] = kv[perm[i]];
}

__global__ static void MatAddLocalCOOValues(const PetscScalar kv[], InsertMode imode, PetscCount Annz, const PetscCount Ajmap1[], const PetscCount Aperm1[], PetscScalar Aa[], PetscCount Bnnz, const PetscCount Bjmap1[], const PetscCount Bperm1[], PetscScalar Ba[])
{
  PetscCount       i         = blockIdx.x * blockDim.x + threadIdx.x;
  const PetscCount grid_size = gridDim.x * blockDim.x;
  for (; i < Annz + Bnnz; i += grid_size) {
    PetscScalar sum = 0.0;
    if (i < Annz) {
      for (PetscCount k = Ajmap1[i]; k < Ajmap1[i + 1]; k++) sum += kv[Aperm1[k]];
      Aa[i] = (imode == INSERT_VALUES ? 0.0 : Aa[i]) + sum;
    } else {
      i -= Annz;
      for (PetscCount k = Bjmap1[i]; k < Bjmap1[i + 1]; k++) sum += kv[Bperm1[k]];
      Ba[i] = (imode == INSERT_VALUES ? 0.0 : Ba[i]) + sum;
    }
  }
}

__global__ static void MatAddRemoteCOOValues(const PetscScalar kv[], PetscCount Annz2, const PetscCount Aimap2[], const PetscCount Ajmap2[], const PetscCount Aperm2[], PetscScalar Aa[], PetscCount Bnnz2, const PetscCount Bimap2[], const PetscCount Bjmap2[], const PetscCount Bperm2[], PetscScalar Ba[])
{
  PetscCount       i         = blockIdx.x * blockDim.x + threadIdx.x;
  const PetscCount grid_size = gridDim.x * blockDim.x;
  for (; i < Annz2 + Bnnz2; i += grid_size) {
    if (i < Annz2) {
      for (PetscCount k = Ajmap2[i]; k < Ajmap2[i + 1]; k++) Aa[Aimap2[i]] += kv[Aperm2[k]];
    } else {
      i -= Annz2;
      for (PetscCount k = Bjmap2[i]; k < Bjmap2[i + 1]; k++) Ba[Bimap2[i]] += kv[Bperm2[k]];
    }
  }
}

static PetscErrorCode MatSetValuesCOO_MPIAIJHIPSPARSE(Mat mat, const PetscScalar v[], InsertMode imode)
{
  Mat_MPIAIJ          *mpiaij = static_cast<Mat_MPIAIJ *>(mat->data);
  Mat_MPIAIJHIPSPARSE *mpidev = static_cast<Mat_MPIAIJHIPSPARSE *>(mpiaij->spptr);
  Mat                  A = mpiaij->A, B = mpiaij->B;
  PetscCount           Annz = mpiaij->Annz, Annz2 = mpiaij->Annz2, Bnnz = mpiaij->Bnnz, Bnnz2 = mpiaij->Bnnz2;
  PetscScalar         *Aa, *Ba = NULL;
  PetscScalar         *vsend = mpidev->sendbuf_d, *v2 = mpidev->recvbuf_d;
  const PetscScalar   *v1     = v;
  const PetscCount    *Ajmap1 = mpidev->Ajmap1_d, *Ajmap2 = mpidev->Ajmap2_d, *Aimap2 = mpidev->Aimap2_d;
  const PetscCount    *Bjmap1 = mpidev->Bjmap1_d, *Bjmap2 = mpidev->Bjmap2_d, *Bimap2 = mpidev->Bimap2_d;
  const PetscCount    *Aperm1 = mpidev->Aperm1_d, *Aperm2 = mpidev->Aperm2_d, *Bperm1 = mpidev->Bperm1_d, *Bperm2 = mpidev->Bperm2_d;
  const PetscCount    *Cperm1 = mpidev->Cperm1_d;
  PetscMemType         memtype;

  PetscFunctionBegin;
  if (mpidev->use_extended_coo) {
    PetscMPIInt size;

    PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)mat), &size));
    PetscCall(PetscGetMemType(v, &memtype));
    if (PetscMemTypeHost(memtype)) { /* If user gave v[] in host, we need to copy it to device */
      PetscCallHIP(hipMalloc((void **)&v1, mpiaij->coo_n * sizeof(PetscScalar)));
      PetscCallHIP(hipMemcpy((void *)v1, v, mpiaij->coo_n * sizeof(PetscScalar), hipMemcpyHostToDevice));
    }

    if (imode == INSERT_VALUES) {
      PetscCall(MatSeqAIJHIPSPARSEGetArrayWrite(A, &Aa)); /* write matrix values */
      PetscCall(MatSeqAIJHIPSPARSEGetArrayWrite(B, &Ba));
    } else {
      PetscCall(MatSeqAIJHIPSPARSEGetArray(A, &Aa)); /* read & write matrix values */
      PetscCall(MatSeqAIJHIPSPARSEGetArray(B, &Ba));
    }

    /* Pack entries to be sent to remote */
    if (mpiaij->sendlen) {
      hipLaunchKernelGGL(HIP_KERNEL_NAME(MatPackCOOValues), dim3((mpiaij->sendlen + 255) / 256), dim3(256), 0, PetscDefaultHipStream, v1, mpiaij->sendlen, Cperm1, vsend);
      PetscCallHIP(hipPeekAtLastError());
    }

    /* Send remote entries to their owner and overlap the communication with local computation */
    PetscCall(PetscSFReduceWithMemTypeBegin(mpiaij->coo_sf, MPIU_SCALAR, PETSC_MEMTYPE_HIP, vsend, PETSC_MEMTYPE_HIP, v2, MPI_REPLACE));
    /* Add local entries to A and B */
    if (Annz + Bnnz > 0) {
      hipLaunchKernelGGL(HIP_KERNEL_NAME(MatAddLocalCOOValues), dim3((Annz + Bnnz + 255) / 256), dim3(256), 0, PetscDefaultHipStream, v1, imode, Annz, Ajmap1, Aperm1, Aa, Bnnz, Bjmap1, Bperm1, Ba);
      PetscCallHIP(hipPeekAtLastError());
    }
    PetscCall(PetscSFReduceEnd(mpiaij->coo_sf, MPIU_SCALAR, vsend, v2, MPI_REPLACE));

    /* Add received remote entries to A and B */
    if (Annz2 + Bnnz2 > 0) {
      hipLaunchKernelGGL(HIP_KERNEL_NAME(MatAddRemoteCOOValues), dim3((Annz2 + Bnnz2 + 255) / 256), dim3(256), 0, PetscDefaultHipStream, v2, Annz2, Aimap2, Ajmap2, Aperm2, Aa, Bnnz2, Bimap2, Bjmap2, Bperm2, Ba);
      PetscCallHIP(hipPeekAtLastError());
    }

    if (imode == INSERT_VALUES) {
      PetscCall(MatSeqAIJHIPSPARSERestoreArrayWrite(A, &Aa));
      PetscCall(MatSeqAIJHIPSPARSERestoreArrayWrite(B, &Ba));
    } else {
      PetscCall(MatSeqAIJHIPSPARSERestoreArray(A, &Aa));
      PetscCall(MatSeqAIJHIPSPARSERestoreArray(B, &Ba));
    }
    if (PetscMemTypeHost(memtype)) PetscCallHIP(hipFree((void *)v1));
  } else {
    PetscCall(MatSetValuesCOO_MPIAIJHIPSPARSE_Basic(mat, v, imode));
  }
  mat->offloadmask = PETSC_OFFLOAD_GPU;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMPIAIJGetLocalMatMerge_MPIAIJHIPSPARSE(Mat A, MatReuse scall, IS *glob, Mat *A_loc)
{
  Mat             Ad, Ao;
  const PetscInt *cmap;

  PetscFunctionBegin;
  PetscCall(MatMPIAIJGetSeqAIJ(A, &Ad, &Ao, &cmap));
  PetscCall(MatSeqAIJHIPSPARSEMergeMats(Ad, Ao, scall, A_loc));
  if (glob) {
    PetscInt cst, i, dn, on, *gidx;

    PetscCall(MatGetLocalSize(Ad, NULL, &dn));
    PetscCall(MatGetLocalSize(Ao, NULL, &on));
    PetscCall(MatGetOwnershipRangeColumn(A, &cst, NULL));
    PetscCall(PetscMalloc1(dn + on, &gidx));
    for (i = 0; i < dn; i++) gidx[i] = cst + i;
    for (i = 0; i < on; i++) gidx[i + dn] = cmap[i];
    PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)Ad), dn + on, gidx, PETSC_OWN_POINTER, glob));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatMPIAIJSetPreallocation_MPIAIJHIPSPARSE(Mat B, PetscInt d_nz, const PetscInt d_nnz[], PetscInt o_nz, const PetscInt o_nnz[])
{
  Mat_MPIAIJ          *b               = (Mat_MPIAIJ *)B->data;
  Mat_MPIAIJHIPSPARSE *hipsparseStruct = (Mat_MPIAIJHIPSPARSE *)b->spptr;
  PetscInt             i;

  PetscFunctionBegin;
  PetscCall(PetscLayoutSetUp(B->rmap));
  PetscCall(PetscLayoutSetUp(B->cmap));
  if (PetscDefined(USE_DEBUG) && d_nnz) {
    for (i = 0; i < B->rmap->n; i++) PetscCheck(d_nnz[i] >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "d_nnz cannot be less than 0: local row %" PetscInt_FMT " value %" PetscInt_FMT, i, d_nnz[i]);
  }
  if (PetscDefined(USE_DEBUG) && o_nnz) {
    for (i = 0; i < B->rmap->n; i++) PetscCheck(o_nnz[i] >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "o_nnz cannot be less than 0: local row %" PetscInt_FMT " value %" PetscInt_FMT, i, o_nnz[i]);
  }
#if defined(PETSC_USE_CTABLE)
  PetscCall(PetscHMapIDestroy(&b->colmap));
#else
  PetscCall(PetscFree(b->colmap));
#endif
  PetscCall(PetscFree(b->garray));
  PetscCall(VecDestroy(&b->lvec));
  PetscCall(VecScatterDestroy(&b->Mvctx));
  /* Because the B will have been resized we simply destroy it and create a new one each time */
  PetscCall(MatDestroy(&b->B));
  if (!b->A) {
    PetscCall(MatCreate(PETSC_COMM_SELF, &b->A));
    PetscCall(MatSetSizes(b->A, B->rmap->n, B->cmap->n, B->rmap->n, B->cmap->n));
  }
  if (!b->B) {
    PetscMPIInt size;
    PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)B), &size));
    PetscCall(MatCreate(PETSC_COMM_SELF, &b->B));
    PetscCall(MatSetSizes(b->B, B->rmap->n, size > 1 ? B->cmap->N : 0, B->rmap->n, size > 1 ? B->cmap->N : 0));
  }
  PetscCall(MatSetType(b->A, MATSEQAIJHIPSPARSE));
  PetscCall(MatSetType(b->B, MATSEQAIJHIPSPARSE));
  PetscCall(MatBindToCPU(b->A, B->boundtocpu));
  PetscCall(MatBindToCPU(b->B, B->boundtocpu));
  PetscCall(MatSeqAIJSetPreallocation(b->A, d_nz, d_nnz));
  PetscCall(MatSeqAIJSetPreallocation(b->B, o_nz, o_nnz));
  PetscCall(MatHIPSPARSESetFormat(b->A, MAT_HIPSPARSE_MULT, hipsparseStruct->diagGPUMatFormat));
  PetscCall(MatHIPSPARSESetFormat(b->B, MAT_HIPSPARSE_MULT, hipsparseStruct->offdiagGPUMatFormat));
  B->preallocated = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatMult_MPIAIJHIPSPARSE(Mat A, Vec xx, Vec yy)
{
  Mat_MPIAIJ *a = (Mat_MPIAIJ *)A->data;

  PetscFunctionBegin;
  PetscCall(VecScatterBegin(a->Mvctx, xx, a->lvec, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall((*a->A->ops->mult)(a->A, xx, yy));
  PetscCall(VecScatterEnd(a->Mvctx, xx, a->lvec, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall((*a->B->ops->multadd)(a->B, a->lvec, yy, yy));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatZeroEntries_MPIAIJHIPSPARSE(Mat A)
{
  Mat_MPIAIJ *l = (Mat_MPIAIJ *)A->data;

  PetscFunctionBegin;
  PetscCall(MatZeroEntries(l->A));
  PetscCall(MatZeroEntries(l->B));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatMultAdd_MPIAIJHIPSPARSE(Mat A, Vec xx, Vec yy, Vec zz)
{
  Mat_MPIAIJ *a = (Mat_MPIAIJ *)A->data;

  PetscFunctionBegin;
  PetscCall(VecScatterBegin(a->Mvctx, xx, a->lvec, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall((*a->A->ops->multadd)(a->A, xx, yy, zz));
  PetscCall(VecScatterEnd(a->Mvctx, xx, a->lvec, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall((*a->B->ops->multadd)(a->B, a->lvec, zz, zz));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatMultTranspose_MPIAIJHIPSPARSE(Mat A, Vec xx, Vec yy)
{
  Mat_MPIAIJ *a = (Mat_MPIAIJ *)A->data;

  PetscFunctionBegin;
  PetscCall((*a->B->ops->multtranspose)(a->B, xx, a->lvec));
  PetscCall((*a->A->ops->multtranspose)(a->A, xx, yy));
  PetscCall(VecScatterBegin(a->Mvctx, a->lvec, yy, ADD_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterEnd(a->Mvctx, a->lvec, yy, ADD_VALUES, SCATTER_REVERSE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatHIPSPARSESetFormat_MPIAIJHIPSPARSE(Mat A, MatHIPSPARSEFormatOperation op, MatHIPSPARSEStorageFormat format)
{
  Mat_MPIAIJ          *a               = (Mat_MPIAIJ *)A->data;
  Mat_MPIAIJHIPSPARSE *hipsparseStruct = (Mat_MPIAIJHIPSPARSE *)a->spptr;

  PetscFunctionBegin;
  switch (op) {
  case MAT_HIPSPARSE_MULT_DIAG:
    hipsparseStruct->diagGPUMatFormat = format;
    break;
  case MAT_HIPSPARSE_MULT_OFFDIAG:
    hipsparseStruct->offdiagGPUMatFormat = format;
    break;
  case MAT_HIPSPARSE_ALL:
    hipsparseStruct->diagGPUMatFormat    = format;
    hipsparseStruct->offdiagGPUMatFormat = format;
    break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "unsupported operation %d for MatHIPSPARSEFormatOperation. Only MAT_HIPSPARSE_MULT_DIAG, MAT_HIPSPARSE_MULT_DIAG, and MAT_HIPSPARSE_MULT_ALL are currently supported.", op);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatSetFromOptions_MPIAIJHIPSPARSE(Mat A, PetscOptionItems *PetscOptionsObject)
{
  MatHIPSPARSEStorageFormat format;
  PetscBool                 flg;
  Mat_MPIAIJ               *a               = (Mat_MPIAIJ *)A->data;
  Mat_MPIAIJHIPSPARSE      *hipsparseStruct = (Mat_MPIAIJHIPSPARSE *)a->spptr;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "MPIAIJHIPSPARSE options");
  if (A->factortype == MAT_FACTOR_NONE) {
    PetscCall(PetscOptionsEnum("-mat_hipsparse_mult_diag_storage_format", "sets storage format of the diagonal blocks of (mpi)aijhipsparse gpu matrices for SpMV", "MatHIPSPARSESetFormat", MatHIPSPARSEStorageFormats, (PetscEnum)hipsparseStruct->diagGPUMatFormat, (PetscEnum *)&format, &flg));
    if (flg) PetscCall(MatHIPSPARSESetFormat(A, MAT_HIPSPARSE_MULT_DIAG, format));
    PetscCall(PetscOptionsEnum("-mat_hipsparse_mult_offdiag_storage_format", "sets storage format of the off-diagonal blocks (mpi)aijhipsparse gpu matrices for SpMV", "MatHIPSPARSESetFormat", MatHIPSPARSEStorageFormats, (PetscEnum)hipsparseStruct->offdiagGPUMatFormat, (PetscEnum *)&format, &flg));
    if (flg) PetscCall(MatHIPSPARSESetFormat(A, MAT_HIPSPARSE_MULT_OFFDIAG, format));
    PetscCall(PetscOptionsEnum("-mat_hipsparse_storage_format", "sets storage format of the diagonal and off-diagonal blocks (mpi)aijhipsparse gpu matrices for SpMV", "MatHIPSPARSESetFormat", MatHIPSPARSEStorageFormats, (PetscEnum)hipsparseStruct->diagGPUMatFormat, (PetscEnum *)&format, &flg));
    if (flg) PetscCall(MatHIPSPARSESetFormat(A, MAT_HIPSPARSE_ALL, format));
  }
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatAssemblyEnd_MPIAIJHIPSPARSE(Mat A, MatAssemblyType mode)
{
  Mat_MPIAIJ          *mpiaij = (Mat_MPIAIJ *)A->data;
  Mat_MPIAIJHIPSPARSE *cusp   = (Mat_MPIAIJHIPSPARSE *)mpiaij->spptr;
  PetscObjectState     onnz   = A->nonzerostate;

  PetscFunctionBegin;
  PetscCall(MatAssemblyEnd_MPIAIJ(A, mode));
  if (mpiaij->lvec) PetscCall(VecSetType(mpiaij->lvec, VECSEQHIP));
  if (onnz != A->nonzerostate && cusp->deviceMat) {
    PetscSplitCSRDataStructure d_mat = cusp->deviceMat, h_mat;

    PetscCall(PetscInfo(A, "Destroy device mat since nonzerostate changed\n"));
    PetscCall(PetscNew(&h_mat));
    PetscCallHIP(hipMemcpy(h_mat, d_mat, sizeof(*d_mat), hipMemcpyDeviceToHost));
    PetscCallHIP(hipFree(h_mat->colmap));
    if (h_mat->allocated_indices) {
      PetscCallHIP(hipFree(h_mat->diag.i));
      PetscCallHIP(hipFree(h_mat->diag.j));
      if (h_mat->offdiag.j) {
        PetscCallHIP(hipFree(h_mat->offdiag.i));
        PetscCallHIP(hipFree(h_mat->offdiag.j));
      }
    }
    PetscCallHIP(hipFree(d_mat));
    PetscCall(PetscFree(h_mat));
    cusp->deviceMat = NULL;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatDestroy_MPIAIJHIPSPARSE(Mat A)
{
  Mat_MPIAIJ          *aij             = (Mat_MPIAIJ *)A->data;
  Mat_MPIAIJHIPSPARSE *hipsparseStruct = (Mat_MPIAIJHIPSPARSE *)aij->spptr;

  PetscFunctionBegin;
  PetscCheck(hipsparseStruct, PETSC_COMM_SELF, PETSC_ERR_COR, "Missing spptr");
  if (hipsparseStruct->deviceMat) {
    PetscSplitCSRDataStructure d_mat = hipsparseStruct->deviceMat, h_mat;

    PetscCall(PetscInfo(A, "Have device matrix\n"));
    PetscCall(PetscNew(&h_mat));
    PetscCallHIP(hipMemcpy(h_mat, d_mat, sizeof(*d_mat), hipMemcpyDeviceToHost));
    PetscCallHIP(hipFree(h_mat->colmap));
    if (h_mat->allocated_indices) {
      PetscCallHIP(hipFree(h_mat->diag.i));
      PetscCallHIP(hipFree(h_mat->diag.j));
      if (h_mat->offdiag.j) {
        PetscCallHIP(hipFree(h_mat->offdiag.i));
        PetscCallHIP(hipFree(h_mat->offdiag.j));
      }
    }
    PetscCallHIP(hipFree(d_mat));
    PetscCall(PetscFree(h_mat));
  }
  /* Free COO */
  PetscCall(MatResetPreallocationCOO_MPIAIJHIPSPARSE(A));
  PetscCallCXX(delete hipsparseStruct);
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatMPIAIJSetPreallocation_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatMPIAIJGetLocalMatMerge_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSetPreallocationCOO_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSetValuesCOO_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatHIPSPARSESetFormat_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_mpiaijhipsparse_hypre_C", NULL));
  PetscCall(MatDestroy_MPIAIJ(A));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode MatConvert_MPIAIJ_MPIAIJHIPSPARSE(Mat B, MatType mtype, MatReuse reuse, Mat *newmat)
{
  Mat_MPIAIJ *a;
  Mat         A;

  PetscFunctionBegin;
  PetscCall(PetscDeviceInitialize(PETSC_DEVICE_HIP));
  if (reuse == MAT_INITIAL_MATRIX) PetscCall(MatDuplicate(B, MAT_COPY_VALUES, newmat));
  else if (reuse == MAT_REUSE_MATRIX) PetscCall(MatCopy(B, *newmat, SAME_NONZERO_PATTERN));
  A             = *newmat;
  A->boundtocpu = PETSC_FALSE;
  PetscCall(PetscFree(A->defaultvectype));
  PetscCall(PetscStrallocpy(VECHIP, &A->defaultvectype));

  a = (Mat_MPIAIJ *)A->data;
  if (a->A) PetscCall(MatSetType(a->A, MATSEQAIJHIPSPARSE));
  if (a->B) PetscCall(MatSetType(a->B, MATSEQAIJHIPSPARSE));
  if (a->lvec) PetscCall(VecSetType(a->lvec, VECSEQHIP));

  if (reuse != MAT_REUSE_MATRIX && !a->spptr) PetscCallCXX(a->spptr = new Mat_MPIAIJHIPSPARSE);

  A->ops->assemblyend           = MatAssemblyEnd_MPIAIJHIPSPARSE;
  A->ops->mult                  = MatMult_MPIAIJHIPSPARSE;
  A->ops->multadd               = MatMultAdd_MPIAIJHIPSPARSE;
  A->ops->multtranspose         = MatMultTranspose_MPIAIJHIPSPARSE;
  A->ops->setfromoptions        = MatSetFromOptions_MPIAIJHIPSPARSE;
  A->ops->destroy               = MatDestroy_MPIAIJHIPSPARSE;
  A->ops->zeroentries           = MatZeroEntries_MPIAIJHIPSPARSE;
  A->ops->productsetfromoptions = MatProductSetFromOptions_MPIAIJBACKEND;

  PetscCall(PetscObjectChangeTypeName((PetscObject)A, MATMPIAIJHIPSPARSE));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatMPIAIJGetLocalMatMerge_C", MatMPIAIJGetLocalMatMerge_MPIAIJHIPSPARSE));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatMPIAIJSetPreallocation_C", MatMPIAIJSetPreallocation_MPIAIJHIPSPARSE));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatHIPSPARSESetFormat_C", MatHIPSPARSESetFormat_MPIAIJHIPSPARSE));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSetPreallocationCOO_C", MatSetPreallocationCOO_MPIAIJHIPSPARSE));
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatSetValuesCOO_C", MatSetValuesCOO_MPIAIJHIPSPARSE));
#if defined(PETSC_HAVE_HYPRE)
  PetscCall(PetscObjectComposeFunction((PetscObject)A, "MatConvert_mpiaijhipsparse_hypre_C", MatConvert_AIJ_HYPRE));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN PetscErrorCode MatCreate_MPIAIJHIPSPARSE(Mat A)
{
  PetscFunctionBegin;
  PetscCall(PetscDeviceInitialize(PETSC_DEVICE_HIP));
  PetscCall(MatCreate_MPIAIJ(A));
  PetscCall(MatConvert_MPIAIJ_MPIAIJHIPSPARSE(A, MATMPIAIJHIPSPARSE, MAT_INPLACE_MATRIX, &A));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatCreateAIJHIPSPARSE - Creates a sparse matrix in AIJ (compressed row) format
   (the default parallel PETSc format).  This matrix will ultimately pushed down
   to AMD GPUs and use the HIPSPARSE library for calculations. For good matrix
   assembly performance the user should preallocate the matrix storage by setting
   the parameter `nz` (or the array `nnz`).

   Collective

   Input Parameters:
+  comm - MPI communicator, set to `PETSC_COMM_SELF`
.  m - number of local rows (or `PETSC_DECIDE` to have calculated if `M` is given)
           This value should be the same as the local size used in creating the
           y vector for the matrix-vector product y = Ax.
.  n - This value should be the same as the local size used in creating the
       x vector for the matrix-vector product y = Ax. (or PETSC_DECIDE to have
       calculated if `N` is given) For square matrices `n` is almost always `m`.
.  M - number of global rows (or `PETSC_DETERMINE` to have calculated if `m` is given)
.  N - number of global columns (or `PETSC_DETERMINE` to have calculated if `n` is given)
.  d_nz - number of nonzeros per row (same for all rows), for the "diagonal" portion of the matrix
.  d_nnz - array containing the number of nonzeros in the various rows (possibly different for each row) or `NULL`, for the "diagonal" portion of the matrix
.  o_nz - number of nonzeros per row (same for all rows), for the "off-diagonal" portion of the matrix
-  o_nnz - array containing the number of nonzeros in the various rows (possibly different for each row) or `NULL`, for the "off-diagonal" portion of the matrix

   Output Parameter:
.  A - the matrix

   Level: intermediate

   Notes:
   It is recommended that one use the `MatCreate()`, `MatSetType()` and/or `MatSetFromOptions()`,
   MatXXXXSetPreallocation() paradigm instead of this routine directly.
   [MatXXXXSetPreallocation() is, for example, `MatSeqAIJSetPreallocation()`]

   If `d_nnz` (`o_nnz`) is given then `d_nz` (`o_nz`) is ignored

   The `MATAIJ` format (compressed row storage), is fully compatible with standard Fortran
   storage.  That is, the stored row and column indices can begin at
   either one (as in Fortran) or zero.

   Specify the preallocated storage with either `d_nz` (`o_nz`) or `d_nnz` (`o_nnz`) (not both).
   Set `d_nz` (`o_nz`) = `PETSC_DEFAULT` and `d_nnz` (`o_nnz`) = `NULL` for PETSc to control dynamic memory
   allocation.

.seealso: [](chapter_matrices), `Mat`, `MatCreate()`, `MatCreateAIJ()`, `MatSetValues()`, `MatSeqAIJSetColumnIndices()`, `MatCreateSeqAIJWithArrays()`, `MatCreateAIJ()`, `MATMPIAIJHIPSPARSE`, `MATAIJHIPSPARSE`
@*/
PetscErrorCode MatCreateAIJHIPSPARSE(MPI_Comm comm, PetscInt m, PetscInt n, PetscInt M, PetscInt N, PetscInt d_nz, const PetscInt d_nnz[], PetscInt o_nz, const PetscInt o_nnz[], Mat *A)
{
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscCall(MatCreate(comm, A));
  PetscCall(MatSetSizes(*A, m, n, M, N));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  if (size > 1) {
    PetscCall(MatSetType(*A, MATMPIAIJHIPSPARSE));
    PetscCall(MatMPIAIJSetPreallocation(*A, d_nz, d_nnz, o_nz, o_nnz));
  } else {
    PetscCall(MatSetType(*A, MATSEQAIJHIPSPARSE));
    PetscCall(MatSeqAIJSetPreallocation(*A, d_nz, d_nnz));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   MATAIJHIPSPARSE - A matrix type to be used for sparse matrices; it is as same as `MATMPIAIJHIPSPARSE`.

   A matrix type type whose data resides on GPUs. These matrices can be in either
   CSR, ELL, or Hybrid format. All matrix calculations are performed on AMD GPUs using the HIPSPARSE library.

   This matrix type is identical to `MATSEQAIJHIPSPARSE` when constructed with a single process communicator,
   and `MATMPIAIJHIPSPARSE` otherwise.  As a result, for single process communicators,
   `MatSeqAIJSetPreallocation()` is supported, and similarly `MatMPIAIJSetPreallocation()` is supported
   for communicators controlling multiple processes.  It is recommended that you call both of
   the above preallocation routines for simplicity.

   Options Database Keys:
+  -mat_type mpiaijhipsparse - sets the matrix type to `MATMPIAIJHIPSPARSE`
.  -mat_hipsparse_storage_format csr - sets the storage format of diagonal and off-diagonal matrices. Other options include ell (ellpack) or hyb (hybrid).
.  -mat_hipsparse_mult_diag_storage_format csr - sets the storage format of diagonal matrix. Other options include ell (ellpack) or hyb (hybrid).
-  -mat_hipsparse_mult_offdiag_storage_format csr - sets the storage format of off-diagonal matrix. Other options include ell (ellpack) or hyb (hybrid).

  Level: beginner

.seealso: [](chapter_matrices), `Mat`, `MatCreateAIJHIPSPARSE()`, `MATSEQAIJHIPSPARSE`, `MATMPIAIJHIPSPARSE`, `MatCreateSeqAIJHIPSPARSE()`, `MatHIPSPARSESetFormat()`, `MatHIPSPARSEStorageFormat`, `MatHIPSPARSEFormatOperation`
M*/

/*MC
   MATMPIAIJHIPSPARSE - A matrix type to be used for sparse matrices; it is as same as `MATAIJHIPSPARSE`.

  Level: beginner

.seealso: [](chapter_matrices), `Mat`, `MATAIJHIPSPARSE`, `MATSEQAIJHIPSPARSE`
M*/

// get GPU pointers to stripped down Mat. For both seq and MPI Mat.
PetscErrorCode MatHIPSPARSEGetDeviceMatWrite(Mat A, PetscSplitCSRDataStructure *B)
{
  PetscSplitCSRDataStructure d_mat;
  PetscMPIInt                size;
  int                       *ai = NULL, *bi = NULL, *aj = NULL, *bj = NULL;
  PetscScalar               *aa = NULL, *ba = NULL;
  Mat_SeqAIJ                *jaca = NULL, *jacb = NULL;
  Mat_SeqAIJHIPSPARSE       *hipsparsestructA = NULL;
  CsrMatrix                 *matrixA = NULL, *matrixB = NULL;

  PetscFunctionBegin;
  PetscCheck(A->assembled, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "Need already assembled matrix");
  if (A->factortype != MAT_FACTOR_NONE) {
    *B = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)A), &size));
  // get jaca
  if (size == 1) {
    PetscBool isseqaij;

    PetscCall(PetscObjectBaseTypeCompare((PetscObject)A, MATSEQAIJ, &isseqaij));
    if (isseqaij) {
      jaca = (Mat_SeqAIJ *)A->data;
      PetscCheck(jaca->roworiented, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "Device assembly does not currently support column oriented values insertion");
      hipsparsestructA = (Mat_SeqAIJHIPSPARSE *)A->spptr;
      d_mat            = hipsparsestructA->deviceMat;
      PetscCall(MatSeqAIJHIPSPARSECopyToGPU(A));
    } else {
      Mat_MPIAIJ *aij = (Mat_MPIAIJ *)A->data;
      PetscCheck(aij->roworiented, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "Device assembly does not currently support column oriented values insertion");
      Mat_MPIAIJHIPSPARSE *spptr = (Mat_MPIAIJHIPSPARSE *)aij->spptr;
      jaca                       = (Mat_SeqAIJ *)aij->A->data;
      hipsparsestructA           = (Mat_SeqAIJHIPSPARSE *)aij->A->spptr;
      d_mat                      = spptr->deviceMat;
      PetscCall(MatSeqAIJHIPSPARSECopyToGPU(aij->A));
    }
    if (hipsparsestructA->format == MAT_HIPSPARSE_CSR) {
      Mat_SeqAIJHIPSPARSEMultStruct *matstruct = (Mat_SeqAIJHIPSPARSEMultStruct *)hipsparsestructA->mat;
      PetscCheck(matstruct, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Missing Mat_SeqAIJHIPSPARSEMultStruct for A");
      matrixA = (CsrMatrix *)matstruct->mat;
      bi      = NULL;
      bj      = NULL;
      ba      = NULL;
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Device Mat needs MAT_HIPSPARSE_CSR");
  } else {
    Mat_MPIAIJ *aij = (Mat_MPIAIJ *)A->data;
    PetscCheck(aij->roworiented, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "Device assembly does not currently support column oriented values insertion");
    jaca                       = (Mat_SeqAIJ *)aij->A->data;
    jacb                       = (Mat_SeqAIJ *)aij->B->data;
    Mat_MPIAIJHIPSPARSE *spptr = (Mat_MPIAIJHIPSPARSE *)aij->spptr;

    PetscCheck(A->nooffprocentries || aij->donotstash, PetscObjectComm((PetscObject)A), PETSC_ERR_SUP, "Device assembly does not currently support offproc values insertion. Use MatSetOption(A, MAT_NO_OFF_PROC_ENTRIES, PETSC_TRUE) or MatSetOption(A, MAT_IGNORE_OFF_PROC_ENTRIES, PETSC_TRUE)");
    hipsparsestructA                      = (Mat_SeqAIJHIPSPARSE *)aij->A->spptr;
    Mat_SeqAIJHIPSPARSE *hipsparsestructB = (Mat_SeqAIJHIPSPARSE *)aij->B->spptr;
    PetscCheck(hipsparsestructA->format == MAT_HIPSPARSE_CSR, PETSC_COMM_SELF, PETSC_ERR_SUP, "Device Mat A needs MAT_HIPSPARSE_CSR");
    if (hipsparsestructB->format == MAT_HIPSPARSE_CSR) {
      PetscCall(MatSeqAIJHIPSPARSECopyToGPU(aij->A));
      PetscCall(MatSeqAIJHIPSPARSECopyToGPU(aij->B));
      Mat_SeqAIJHIPSPARSEMultStruct *matstructA = (Mat_SeqAIJHIPSPARSEMultStruct *)hipsparsestructA->mat;
      Mat_SeqAIJHIPSPARSEMultStruct *matstructB = (Mat_SeqAIJHIPSPARSEMultStruct *)hipsparsestructB->mat;
      PetscCheck(matstructA, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Missing Mat_SeqAIJHIPSPARSEMultStruct for A");
      PetscCheck(matstructB, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Missing Mat_SeqAIJHIPSPARSEMultStruct for B");
      matrixA = (CsrMatrix *)matstructA->mat;
      matrixB = (CsrMatrix *)matstructB->mat;
      if (jacb->compressedrow.use) {
        if (!hipsparsestructB->rowoffsets_gpu) {
          hipsparsestructB->rowoffsets_gpu = new THRUSTINTARRAY32(A->rmap->n + 1);
          hipsparsestructB->rowoffsets_gpu->assign(jacb->i, jacb->i + A->rmap->n + 1);
        }
        bi = thrust::raw_pointer_cast(hipsparsestructB->rowoffsets_gpu->data());
      } else {
        bi = thrust::raw_pointer_cast(matrixB->row_offsets->data());
      }
      bj = thrust::raw_pointer_cast(matrixB->column_indices->data());
      ba = thrust::raw_pointer_cast(matrixB->values->data());
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Device Mat B needs MAT_HIPSPARSE_CSR");
    d_mat = spptr->deviceMat;
  }
  if (jaca->compressedrow.use) {
    if (!hipsparsestructA->rowoffsets_gpu) {
      hipsparsestructA->rowoffsets_gpu = new THRUSTINTARRAY32(A->rmap->n + 1);
      hipsparsestructA->rowoffsets_gpu->assign(jaca->i, jaca->i + A->rmap->n + 1);
    }
    ai = thrust::raw_pointer_cast(hipsparsestructA->rowoffsets_gpu->data());
  } else {
    ai = thrust::raw_pointer_cast(matrixA->row_offsets->data());
  }
  aj = thrust::raw_pointer_cast(matrixA->column_indices->data());
  aa = thrust::raw_pointer_cast(matrixA->values->data());

  if (!d_mat) {
    PetscSplitCSRDataStructure h_mat;

    // create and populate strucy on host and copy on device
    PetscCall(PetscInfo(A, "Create device matrix\n"));
    PetscCall(PetscNew(&h_mat));
    PetscCallHIP(hipMalloc((void **)&d_mat, sizeof(*d_mat)));
    if (size > 1) { /* need the colmap array */
      Mat_MPIAIJ *aij = (Mat_MPIAIJ *)A->data;
      PetscInt   *colmap;
      PetscInt    ii, n = aij->B->cmap->n, N = A->cmap->N;

      PetscCheck(!n || aij->garray, PETSC_COMM_SELF, PETSC_ERR_PLIB, "MPIAIJ Matrix was assembled but is missing garray");

      PetscCall(PetscCalloc1(N + 1, &colmap));
      for (ii = 0; ii < n; ii++) colmap[aij->garray[ii]] = ii + 1;
#if defined(PETSC_USE_64BIT_INDICES)
      { // have to make a long version of these
        int      *h_bi32, *h_bj32;
        PetscInt *h_bi64, *h_bj64, *d_bi64, *d_bj64;
        PetscCall(PetscCalloc4(A->rmap->n + 1, &h_bi32, jacb->nz, &h_bj32, A->rmap->n + 1, &h_bi64, jacb->nz, &h_bj64));
        PetscCallHIP(hipMemcpy(h_bi32, bi, (A->rmap->n + 1) * sizeof(*h_bi32), hipMemcpyDeviceToHost));
        for (int i = 0; i < A->rmap->n + 1; i++) h_bi64[i] = h_bi32[i];
        PetscCallHIP(hipMemcpy(h_bj32, bj, jacb->nz * sizeof(*h_bj32), hipMemcpyDeviceToHost));
        for (int i = 0; i < jacb->nz; i++) h_bj64[i] = h_bj32[i];

        PetscCallHIP(hipMalloc((void **)&d_bi64, (A->rmap->n + 1) * sizeof(*d_bi64)));
        PetscCallHIP(hipMemcpy(d_bi64, h_bi64, (A->rmap->n + 1) * sizeof(*d_bi64), hipMemcpyHostToDevice));
        PetscCallHIP(hipMalloc((void **)&d_bj64, jacb->nz * sizeof(*d_bj64)));
        PetscCallHIP(hipMemcpy(d_bj64, h_bj64, jacb->nz * sizeof(*d_bj64), hipMemcpyHostToDevice));

        h_mat->offdiag.i         = d_bi64;
        h_mat->offdiag.j         = d_bj64;
        h_mat->allocated_indices = PETSC_TRUE;

        PetscCall(PetscFree4(h_bi32, h_bj32, h_bi64, h_bj64));
      }
#else
      h_mat->offdiag.i         = (PetscInt *)bi;
      h_mat->offdiag.j         = (PetscInt *)bj;
      h_mat->allocated_indices = PETSC_FALSE;
#endif
      h_mat->offdiag.a = ba;
      h_mat->offdiag.n = A->rmap->n;

      PetscCallHIP(hipMalloc((void **)&h_mat->colmap, (N + 1) * sizeof(*h_mat->colmap)));
      PetscCallHIP(hipMemcpy(h_mat->colmap, colmap, (N + 1) * sizeof(*h_mat->colmap), hipMemcpyHostToDevice));
      PetscCall(PetscFree(colmap));
    }
    h_mat->rstart = A->rmap->rstart;
    h_mat->rend   = A->rmap->rend;
    h_mat->cstart = A->cmap->rstart;
    h_mat->cend   = A->cmap->rend;
    h_mat->M      = A->cmap->N;
#if defined(PETSC_USE_64BIT_INDICES)
    {
      int      *h_ai32, *h_aj32;
      PetscInt *h_ai64, *h_aj64, *d_ai64, *d_aj64;
      PetscCall(PetscCalloc4(A->rmap->n + 1, &h_ai32, jaca->nz, &h_aj32, A->rmap->n + 1, &h_ai64, jaca->nz, &h_aj64));
      PetscCallHIP(hipMemcpy(h_ai32, ai, (A->rmap->n + 1) * sizeof(*h_ai32), hipMemcpyDeviceToHost));
      for (int i = 0; i < A->rmap->n + 1; i++) h_ai64[i] = h_ai32[i];
      PetscCallHIP(hipMemcpy(h_aj32, aj, jaca->nz * sizeof(*h_aj32), hipMemcpyDeviceToHost));
      for (int i = 0; i < jaca->nz; i++) h_aj64[i] = h_aj32[i];

      PetscCallHIP(hipMalloc((void **)&d_ai64, (A->rmap->n + 1) * sizeof(*d_ai64)));
      PetscCallHIP(hipMemcpy(d_ai64, h_ai64, (A->rmap->n + 1) * sizeof(*d_ai64), hipMemcpyHostToDevice));
      PetscCallHIP(hipMalloc((void **)&d_aj64, jaca->nz * sizeof(*d_aj64)));
      PetscCallHIP(hipMemcpy(d_aj64, h_aj64, jaca->nz * sizeof(*d_aj64), hipMemcpyHostToDevice));

      h_mat->diag.i            = d_ai64;
      h_mat->diag.j            = d_aj64;
      h_mat->allocated_indices = PETSC_TRUE;

      PetscCall(PetscFree4(h_ai32, h_aj32, h_ai64, h_aj64));
    }
#else
    h_mat->diag.i            = (PetscInt *)ai;
    h_mat->diag.j            = (PetscInt *)aj;
    h_mat->allocated_indices = PETSC_FALSE;
#endif
    h_mat->diag.a = aa;
    h_mat->diag.n = A->rmap->n;
    h_mat->rank   = PetscGlobalRank;
    // copy pointers and metadata to device
    PetscCallHIP(hipMemcpy(d_mat, h_mat, sizeof(*d_mat), hipMemcpyHostToDevice));
    PetscCall(PetscFree(h_mat));
  } else {
    PetscCall(PetscInfo(A, "Reusing device matrix\n"));
  }
  *B             = d_mat;
  A->offloadmask = PETSC_OFFLOAD_GPU;
  PetscFunctionReturn(PETSC_SUCCESS);
}

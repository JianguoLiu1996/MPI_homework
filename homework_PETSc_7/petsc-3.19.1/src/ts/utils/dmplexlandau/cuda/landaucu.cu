/*
  Implements the Landau kernel
*/
#include <petscconf.h>

#include <petsclandau.h>
#if defined(PETSC_HAVE_CUDA_CLANG)
  #define LANDAU_NOT_IMPLEMENTED SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Not supported with CLANG")
PetscErrorCode LandauCUDAJacobian(DM[], const PetscInt, const PetscInt, const PetscInt, const PetscInt[], PetscReal[], PetscScalar[], const PetscScalar[], const LandauStaticData *, const PetscReal, const PetscLogEvent[], const PetscInt[], const PetscInt[], Mat[], Mat)
{
  LANDAU_NOT_IMPLEMENTED;
}
PetscErrorCode LandauCUDACreateMatMaps(P4estVertexMaps *, pointInterpolationP4est (*)[LANDAU_MAX_Q_FACE], PetscInt[], PetscInt, PetscInt)
{
  LANDAU_NOT_IMPLEMENTED;
}
PetscErrorCode LandauCUDADestroyMatMaps(P4estVertexMaps *, PetscInt)
{
  LANDAU_NOT_IMPLEMENTED;
}
PetscErrorCode LandauCUDAStaticDataSet(DM, const PetscInt, const PetscInt, const PetscInt, PetscInt[], PetscInt[], PetscInt[], PetscReal[], PetscReal[], PetscReal[], PetscReal[], PetscReal[], PetscReal[], PetscReal[], PetscReal[], PetscReal[], LandauStaticData *)
{
  LANDAU_NOT_IMPLEMENTED;
}
PetscErrorCode LandauCUDAStaticDataClear(LandauStaticData *)
{
  LANDAU_NOT_IMPLEMENTED;
}
#else
  #include <petsc/private/dmpleximpl.h> /*I  "dmpleximpl.h"   I*/
  #define PETSC_SKIP_IMMINTRIN_H_CUDAWORKAROUND 1
  #include <../src/mat/impls/aij/seq/aij.h>
  #include <petscmat.h>
  #include <petscdevice_cuda.h>

  #include "../land_tensors.h"
  #include <petscaijdevice.h>

PETSC_EXTERN PetscErrorCode LandauCUDACreateMatMaps(P4estVertexMaps maps[], pointInterpolationP4est (*pointMaps)[LANDAU_MAX_Q_FACE], PetscInt Nf[], PetscInt, PetscInt grid)
{
  P4estVertexMaps h_maps;
  PetscFunctionBegin;
  h_maps.num_elements = maps[grid].num_elements;
  h_maps.num_face     = maps[grid].num_face;
  h_maps.num_reduced  = maps[grid].num_reduced;
  h_maps.deviceType   = maps[grid].deviceType;
  h_maps.Nf           = Nf[grid];
  h_maps.numgrids     = maps[grid].numgrids;
  PetscCallCUDA(cudaMalloc((void **)&h_maps.c_maps, maps[grid].num_reduced * sizeof *pointMaps));
  PetscCallCUDA(cudaMemcpy(h_maps.c_maps, maps[grid].c_maps, maps[grid].num_reduced * sizeof *pointMaps, cudaMemcpyHostToDevice));
  PetscCallCUDA(cudaMalloc((void **)&h_maps.gIdx, maps[grid].num_elements * sizeof *maps[grid].gIdx));
  PetscCallCUDA(cudaMemcpy(h_maps.gIdx, maps[grid].gIdx, maps[grid].num_elements * sizeof *maps[grid].gIdx, cudaMemcpyHostToDevice));
  PetscCallCUDA(cudaMalloc((void **)&maps[grid].d_self, sizeof(P4estVertexMaps)));
  PetscCallCUDA(cudaMemcpy(maps[grid].d_self, &h_maps, sizeof(P4estVertexMaps), cudaMemcpyHostToDevice));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN PetscErrorCode LandauCUDADestroyMatMaps(P4estVertexMaps maps[], PetscInt num_grids)
{
  PetscFunctionBegin;
  for (PetscInt grid = 0; grid < num_grids; grid++) {
    P4estVertexMaps *d_maps = maps[grid].d_self, h_maps;
    PetscCallCUDA(cudaMemcpy(&h_maps, d_maps, sizeof(P4estVertexMaps), cudaMemcpyDeviceToHost));
    PetscCallCUDA(cudaFree(h_maps.c_maps));
    PetscCallCUDA(cudaFree(h_maps.gIdx));
    PetscCallCUDA(cudaFree(d_maps));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode LandauCUDAStaticDataSet(DM plex, const PetscInt Nq, const PetscInt batch_sz, const PetscInt num_grids, PetscInt a_numCells[], PetscInt a_species_offset[], PetscInt a_mat_offset[], PetscReal nu_alpha[], PetscReal nu_beta[], PetscReal a_invMass[], PetscReal[], PetscReal a_invJ[], PetscReal a_x[], PetscReal a_y[], PetscReal a_z[], PetscReal a_w[], LandauStaticData *SData_d)
{
  PetscTabulation *Tf;
  PetscReal       *BB, *DD;
  PetscInt         dim, Nb = Nq, szf = sizeof(PetscReal), szs = sizeof(PetscScalar), szi = sizeof(PetscInt);
  PetscInt         h_ip_offset[LANDAU_MAX_GRIDS + 1], h_ipf_offset[LANDAU_MAX_GRIDS + 1], h_elem_offset[LANDAU_MAX_GRIDS + 1], nip, IPfdf_sz, Nf;
  PetscDS          prob;

  PetscFunctionBegin;
  PetscCall(DMGetDimension(plex, &dim));
  PetscCall(DMGetDS(plex, &prob));
  PetscCheck(LANDAU_DIM == dim, PETSC_COMM_WORLD, PETSC_ERR_PLIB, "dim %" PetscInt_FMT " != LANDAU_DIM %d", dim, LANDAU_DIM);
  PetscCall(PetscDSGetTabulation(prob, &Tf));
  BB = Tf[0]->T[0];
  DD = Tf[0]->T[1];
  Nf = h_ip_offset[0] = h_ipf_offset[0] = h_elem_offset[0] = 0;
  nip                                                      = 0;
  IPfdf_sz                                                 = 0;
  for (PetscInt grid = 0; grid < num_grids; grid++) {
    PetscInt nfloc          = a_species_offset[grid + 1] - a_species_offset[grid];
    h_elem_offset[grid + 1] = h_elem_offset[grid] + a_numCells[grid];
    nip += a_numCells[grid] * Nq;
    h_ip_offset[grid + 1] = nip;
    IPfdf_sz += Nq * nfloc * a_numCells[grid];
    h_ipf_offset[grid + 1] = IPfdf_sz;
  }
  Nf = a_species_offset[num_grids];
  {
    PetscCallCUDA(cudaMalloc((void **)&SData_d->B, Nq * Nb * szf)); // kernel input
    PetscCallCUDA(cudaMemcpy(SData_d->B, BB, Nq * Nb * szf, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->D, Nq * Nb * dim * szf)); // kernel input
    PetscCallCUDA(cudaMemcpy(SData_d->D, DD, Nq * Nb * dim * szf, cudaMemcpyHostToDevice));

    PetscCallCUDA(cudaMalloc((void **)&SData_d->alpha, Nf * szf));   // kernel input
    PetscCallCUDA(cudaMalloc((void **)&SData_d->beta, Nf * szf));    // kernel input
    PetscCallCUDA(cudaMalloc((void **)&SData_d->invMass, Nf * szf)); // kernel input

    PetscCallCUDA(cudaMemcpy(SData_d->alpha, nu_alpha, Nf * szf, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMemcpy(SData_d->beta, nu_beta, Nf * szf, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMemcpy(SData_d->invMass, a_invMass, Nf * szf, cudaMemcpyHostToDevice));

    // collect geometry
    PetscCallCUDA(cudaMalloc((void **)&SData_d->invJ, nip * dim * dim * szf)); // kernel input
    PetscCallCUDA(cudaMemcpy(SData_d->invJ, a_invJ, nip * dim * dim * szf, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->x, nip * szf)); // kernel input
    PetscCallCUDA(cudaMemcpy(SData_d->x, a_x, nip * szf, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->y, nip * szf)); // kernel input
    PetscCallCUDA(cudaMemcpy(SData_d->y, a_y, nip * szf, cudaMemcpyHostToDevice));
  #if LANDAU_DIM == 3
    PetscCallCUDA(cudaMalloc((void **)&SData_d->z, nip * szf)); // kernel input
    PetscCallCUDA(cudaMemcpy(SData_d->z, a_z, nip * szf, cudaMemcpyHostToDevice));
  #else
    (void)a_z;
  #endif
    PetscCallCUDA(cudaMalloc((void **)&SData_d->w, nip * szf)); // kernel input
    PetscCallCUDA(cudaMemcpy(SData_d->w, a_w, nip * szf, cudaMemcpyHostToDevice));

    PetscCallCUDA(cudaMalloc((void **)&SData_d->NCells, num_grids * szi));
    PetscCallCUDA(cudaMemcpy(SData_d->NCells, a_numCells, num_grids * szi, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->species_offset, (num_grids + 1) * szi));
    PetscCallCUDA(cudaMemcpy(SData_d->species_offset, a_species_offset, (num_grids + 1) * szi, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->mat_offset, (num_grids + 1) * szi));
    PetscCallCUDA(cudaMemcpy(SData_d->mat_offset, a_mat_offset, (num_grids + 1) * szi, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->ip_offset, (num_grids + 1) * szi));
    PetscCallCUDA(cudaMemcpy(SData_d->ip_offset, h_ip_offset, (num_grids + 1) * szi, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->ipf_offset, (num_grids + 1) * szi));
    PetscCallCUDA(cudaMemcpy(SData_d->ipf_offset, h_ipf_offset, (num_grids + 1) * szi, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->elem_offset, (num_grids + 1) * szi));
    PetscCallCUDA(cudaMemcpy(SData_d->elem_offset, h_elem_offset, (num_grids + 1) * szi, cudaMemcpyHostToDevice));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->maps, num_grids * sizeof(P4estVertexMaps *)));
    // allocate space for dynamic data once
    PetscCallCUDA(cudaMalloc((void **)&SData_d->Eq_m, Nf * szf));               // this could be for each vertex (todo?)
    PetscCallCUDA(cudaMalloc((void **)&SData_d->f, nip * Nf * szs * batch_sz)); // for each vertex in batch
    PetscCallCUDA(cudaMalloc((void **)&SData_d->dfdx, nip * Nf * szs * batch_sz));
    PetscCallCUDA(cudaMalloc((void **)&SData_d->dfdy, nip * Nf * szs * batch_sz));
  #if LANDAU_DIM == 3
    PetscCallCUDA(cudaMalloc((void **)&SData_d->dfdz, nip * Nf * szs * batch_sz));
  #endif
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode LandauCUDAStaticDataClear(LandauStaticData *SData_d)
{
  PetscFunctionBegin;
  if (SData_d->alpha) {
    PetscCallCUDA(cudaFree(SData_d->alpha));
    SData_d->alpha = NULL;
    PetscCallCUDA(cudaFree(SData_d->beta));
    PetscCallCUDA(cudaFree(SData_d->invMass));
    PetscCallCUDA(cudaFree(SData_d->B));
    PetscCallCUDA(cudaFree(SData_d->D));
    PetscCallCUDA(cudaFree(SData_d->invJ));
  #if LANDAU_DIM == 3
    PetscCallCUDA(cudaFree(SData_d->z));
  #endif
    PetscCallCUDA(cudaFree(SData_d->x));
    PetscCallCUDA(cudaFree(SData_d->y));
    PetscCallCUDA(cudaFree(SData_d->w));
    // dynamic data
    PetscCallCUDA(cudaFree(SData_d->Eq_m));
    PetscCallCUDA(cudaFree(SData_d->f));
    PetscCallCUDA(cudaFree(SData_d->dfdx));
    PetscCallCUDA(cudaFree(SData_d->dfdy));
  #if LANDAU_DIM == 3
    PetscCallCUDA(cudaFree(SData_d->dfdz));
  #endif
    PetscCallCUDA(cudaFree(SData_d->NCells));
    PetscCallCUDA(cudaFree(SData_d->species_offset));
    PetscCallCUDA(cudaFree(SData_d->mat_offset));
    PetscCallCUDA(cudaFree(SData_d->ip_offset));
    PetscCallCUDA(cudaFree(SData_d->ipf_offset));
    PetscCallCUDA(cudaFree(SData_d->elem_offset));
    PetscCallCUDA(cudaFree(SData_d->maps));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}
//
// The GPU Landau kernel
//
__global__ void landau_form_fdf(const PetscInt dim, const PetscInt Nb, const PetscInt num_grids, const PetscReal d_invJ[], const PetscReal *const BB, const PetscReal *const DD, PetscScalar *d_vertex_f, P4estVertexMaps *d_maps[], PetscReal d_f[], PetscReal d_dfdx[], PetscReal d_dfdy[],
  #if LANDAU_DIM == 3
                                PetscReal      d_dfdz[],
  #endif
                                const PetscInt d_numCells[], const PetscInt d_species_offset[], const PetscInt d_mat_offset[], const PetscInt d_ip_offset[], const PetscInt d_ipf_offset[], const PetscInt d_elem_offset[]) // output
{
  const PetscInt   Nq = blockDim.y, myQi = threadIdx.y;
  const PetscInt   b_elem_idx = blockIdx.y, b_id = blockIdx.x, IPf_sz_glb = d_ipf_offset[num_grids];
  const PetscReal *Bq = &BB[myQi * Nb], *Dq = &DD[myQi * Nb * dim];
  PetscInt         grid = 0, f, d, b, e, q;
  while (b_elem_idx >= d_elem_offset[grid + 1]) grid++;
  {
    const PetscInt     loc_nip = d_numCells[grid] * Nq, loc_Nf = d_species_offset[grid + 1] - d_species_offset[grid], loc_elem = b_elem_idx - d_elem_offset[grid];
    const PetscInt     moffset = LAND_MOFFSET(b_id, grid, gridDim.x, num_grids, d_mat_offset);
    const PetscScalar *coef;
    PetscReal          u_x[LANDAU_DIM];
    const PetscReal   *invJ = &d_invJ[(d_ip_offset[grid] + loc_elem * Nq + myQi) * dim * dim];
    PetscScalar        coef_buff[LANDAU_MAX_SPECIES * LANDAU_MAX_NQ];
    if (!d_maps) {
      coef = &d_vertex_f[b_id * IPf_sz_glb + d_ipf_offset[grid] + loc_elem * Nb * loc_Nf]; // closure and IP indexing are the same
    } else {
      coef = coef_buff;
      for (f = 0; f < loc_Nf; ++f) {
        LandauIdx *const Idxs = &d_maps[grid]->gIdx[loc_elem][f][0];
        for (b = 0; b < Nb; ++b) {
          PetscInt idx = Idxs[b];
          if (idx >= 0) {
            coef_buff[f * Nb + b] = d_vertex_f[idx + moffset];
          } else {
            idx                   = -idx - 1;
            coef_buff[f * Nb + b] = 0;
            for (q = 0; q < d_maps[grid]->num_face; q++) {
              PetscInt  id    = d_maps[grid]->c_maps[idx][q].gid;
              PetscReal scale = d_maps[grid]->c_maps[idx][q].scale;
              if (id >= 0) coef_buff[f * Nb + b] += scale * d_vertex_f[id + moffset];
            }
          }
        }
      }
    }

    /* get f and df */
    for (f = threadIdx.x; f < loc_Nf; f += blockDim.x) {
      PetscReal      refSpaceDer[LANDAU_DIM];
      const PetscInt idx = b_id * IPf_sz_glb + d_ipf_offset[grid] + f * loc_nip + loc_elem * Nq + myQi;
      d_f[idx]           = 0.0;
      for (d = 0; d < LANDAU_DIM; ++d) refSpaceDer[d] = 0.0;
      for (b = 0; b < Nb; ++b) {
        const PetscInt cidx = b;
        d_f[idx] += Bq[cidx] * PetscRealPart(coef[f * Nb + cidx]);
        for (d = 0; d < dim; ++d) refSpaceDer[d] += Dq[cidx * dim + d] * PetscRealPart(coef[f * Nb + cidx]);
      }
      for (d = 0; d < dim; ++d) {
        for (e = 0, u_x[d] = 0.0; e < dim; ++e) u_x[d] += invJ[e * dim + d] * refSpaceDer[e];
      }
      d_dfdx[idx] = u_x[0];
      d_dfdy[idx] = u_x[1];
  #if LANDAU_DIM == 3
      d_dfdz[idx] = u_x[2];
  #endif
    }
  }
}

__device__ void landau_jac_kernel(const PetscInt num_grids, const PetscInt jpidx, PetscInt nip_global, const PetscInt grid, const PetscReal xx[], const PetscReal yy[], const PetscReal ww[], const PetscReal invJj[], const PetscInt Nftot, const PetscReal nu_alpha[], const PetscReal nu_beta[], const PetscReal invMass[], const PetscReal Eq_m[], const PetscReal *const BB, const PetscReal *const DD, PetscScalar *elemMat, P4estVertexMaps *d_maps[], PetscSplitCSRDataStructure d_mat, // output
                                  PetscScalar s_fieldMats[][LANDAU_MAX_NQ], // all these arrays are in shared memory
                                  PetscReal s_scale[][LANDAU_MAX_Q_FACE], PetscInt s_idx[][LANDAU_MAX_Q_FACE], PetscReal s_g2[][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES], PetscReal s_g3[][LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES], PetscReal s_gg2[][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES], PetscReal s_gg3[][LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES], PetscReal s_nu_alpha[], PetscReal s_nu_beta[], PetscReal s_invMass[], PetscReal s_f[], PetscReal s_dfx[], PetscReal s_dfy[], PetscReal d_f[], PetscReal d_dfdx[], PetscReal d_dfdy[], // global memory
  #if LANDAU_DIM == 3
                                  const PetscReal zz[], PetscReal s_dfz[], PetscReal d_dfdz[],
  #endif
                                  const PetscInt d_numCells[], const PetscInt d_species_offset[], const PetscInt d_mat_offset[], const PetscInt d_ip_offset[], const PetscInt d_ipf_offset[], const PetscInt d_elem_offset[])
{
  const PetscInt  Nq = blockDim.y, myQi = threadIdx.y;
  const PetscInt  b_elem_idx = blockIdx.y, b_id = blockIdx.x, IPf_sz_glb = d_ipf_offset[num_grids];
  const PetscInt  loc_Nf = d_species_offset[grid + 1] - d_species_offset[grid], loc_elem = b_elem_idx - d_elem_offset[grid];
  const PetscInt  moffset = LAND_MOFFSET(b_id, grid, gridDim.x, num_grids, d_mat_offset);
  int             delta, d, f, g, d2, dp, d3, fieldA, ipidx_b;
  PetscReal       gg2_temp[LANDAU_DIM], gg3_temp[LANDAU_DIM][LANDAU_DIM];
  #if LANDAU_DIM == 2
  const PetscReal vj[3] = {xx[jpidx], yy[jpidx]};
  constexpr int   dim   = 2;
  #else
  const PetscReal vj[3] = {xx[jpidx], yy[jpidx], zz[jpidx]};
  constexpr int   dim   = 3;
  #endif
  const PetscInt  f_off = d_species_offset[grid], Nb = Nq;
  // create g2 & g3
  for (f = threadIdx.x; f < loc_Nf; f += blockDim.x) {
    for (d = 0; d < dim; d++) { // clear accumulation data D & K
      s_gg2[d][myQi][f] = 0;
      for (d2 = 0; d2 < dim; d2++) s_gg3[d][d2][myQi][f] = 0;
    }
  }
  #pragma unroll
  for (d2 = 0; d2 < dim; d2++) {
    gg2_temp[d2] = 0;
  #pragma unroll
    for (d3 = 0; d3 < dim; d3++) gg3_temp[d2][d3] = 0;
  }
  if (threadIdx.y == 0) {
    // copy species into shared memory
    for (fieldA = threadIdx.x; fieldA < Nftot; fieldA += blockDim.x) {
      s_nu_alpha[fieldA] = nu_alpha[fieldA];
      s_nu_beta[fieldA]  = nu_beta[fieldA];
      s_invMass[fieldA]  = invMass[fieldA];
    }
  }
  __syncthreads();
  // inner integral, collect gg2/3
  for (ipidx_b = 0; ipidx_b < nip_global; ipidx_b += blockDim.x) {
    const PetscInt ipidx = ipidx_b + threadIdx.x;
    PetscInt       f_off_r, grid_r, loc_Nf_r, nip_loc_r, ipidx_g, fieldB, IPf_idx_r;
    __syncthreads();
    if (ipidx < nip_global) {
      grid_r = 0;
      while (ipidx >= d_ip_offset[grid_r + 1]) grid_r++;
      f_off_r   = d_species_offset[grid_r];
      ipidx_g   = ipidx - d_ip_offset[grid_r];
      nip_loc_r = d_numCells[grid_r] * Nq;
      loc_Nf_r  = d_species_offset[grid_r + 1] - d_species_offset[grid_r];
      IPf_idx_r = b_id * IPf_sz_glb + d_ipf_offset[grid_r] + ipidx_g;
      for (fieldB = threadIdx.y; fieldB < loc_Nf_r; fieldB += blockDim.y) {
        const PetscInt idx                       = IPf_idx_r + fieldB * nip_loc_r;
        s_f[fieldB * blockDim.x + threadIdx.x]   = d_f[idx]; // all vector threads get copy of data
        s_dfx[fieldB * blockDim.x + threadIdx.x] = d_dfdx[idx];
        s_dfy[fieldB * blockDim.x + threadIdx.x] = d_dfdy[idx];
  #if LANDAU_DIM == 3
        s_dfz[fieldB * blockDim.x + threadIdx.x] = d_dfdz[idx];
  #endif
      }
    }
    __syncthreads();
    if (ipidx < nip_global) {
      const PetscReal wi = ww[ipidx], x = xx[ipidx], y = yy[ipidx];
      PetscReal       temp1[3] = {0, 0, 0}, temp2 = 0;
  #if LANDAU_DIM == 2
      PetscReal       Ud[2][2], Uk[2][2], mask = (PetscAbs(vj[0] - x) < 100 * PETSC_SQRT_MACHINE_EPSILON && PetscAbs(vj[1] - y) < 100 * PETSC_SQRT_MACHINE_EPSILON) ? 0. : 1.;
      LandauTensor2D(vj, x, y, Ud, Uk, mask);
  #else
      PetscReal U[3][3], z = zz[ipidx], mask = (PetscAbs(vj[0] - x) < 100 * PETSC_SQRT_MACHINE_EPSILON && PetscAbs(vj[1] - y) < 100 * PETSC_SQRT_MACHINE_EPSILON && PetscAbs(vj[2] - z) < 100 * PETSC_SQRT_MACHINE_EPSILON) ? 0. : 1.;
      LandauTensor3D(vj, x, y, z, U, mask);
  #endif
      for (int fieldB = 0; fieldB < loc_Nf_r; fieldB++) {
        temp1[0] += s_dfx[fieldB * blockDim.x + threadIdx.x] * s_nu_beta[fieldB + f_off_r] * s_invMass[fieldB + f_off_r] * 7; // todo : bring lambdas in
        temp1[1] += s_dfy[fieldB * blockDim.x + threadIdx.x] * s_nu_beta[fieldB + f_off_r] * s_invMass[fieldB + f_off_r] * 7;
  #if LANDAU_DIM == 3
        temp1[2] += s_dfz[fieldB * blockDim.x + threadIdx.x] * s_nu_beta[fieldB + f_off_r] * s_invMass[fieldB + f_off_r] * 7;
  #endif
        temp2 += s_f[fieldB * blockDim.x + threadIdx.x] * s_nu_beta[fieldB + f_off_r] * 7;
      }
      temp1[0] *= wi;
      temp1[1] *= wi;
  #if LANDAU_DIM == 3
      temp1[2] *= wi;
  #endif
      temp2 *= wi;
  #if LANDAU_DIM == 2
    #pragma unroll
      for (d2 = 0; d2 < 2; d2++) {
    #pragma unroll
        for (d3 = 0; d3 < 2; ++d3) {
          /* K = U * grad(f): g2=e: i,A */
          gg2_temp[d2] += Uk[d2][d3] * temp1[d3];
          /* D = -U * (I \kron (fx)): g3=f: i,j,A */
          gg3_temp[d2][d3] += Ud[d2][d3] * temp2;
        }
      }
  #else
    #pragma unroll
      for (d2 = 0; d2 < 3; ++d2) {
    #pragma unroll
        for (d3 = 0; d3 < 3; ++d3) {
          /* K = U * grad(f): g2 = e: i,A */
          gg2_temp[d2] += U[d2][d3] * temp1[d3];
          /* D = -U * (I \kron (fx)): g3 = f: i,j,A */
          gg3_temp[d2][d3] += U[d2][d3] * temp2;
        }
      }
  #endif
    }
  } /* IPs */

  /* reduce gg temp sums across threads */
  for (delta = blockDim.x / 2; delta > 0; delta /= 2) {
  #pragma unroll
    for (d2 = 0; d2 < dim; d2++) {
      gg2_temp[d2] += __shfl_xor_sync(0xffffffff, gg2_temp[d2], delta, blockDim.x);
  #pragma unroll
      for (d3 = 0; d3 < dim; d3++) gg3_temp[d2][d3] += __shfl_xor_sync(0xffffffff, gg3_temp[d2][d3], delta, blockDim.x);
    }
  }
  // add alpha and put in gg2/3
  for (fieldA = threadIdx.x; fieldA < loc_Nf; fieldA += blockDim.x) {
  #pragma unroll
    for (d2 = 0; d2 < dim; d2++) {
      s_gg2[d2][myQi][fieldA] += gg2_temp[d2] * s_nu_alpha[fieldA + f_off];
  #pragma unroll
      for (d3 = 0; d3 < dim; d3++) s_gg3[d2][d3][myQi][fieldA] -= gg3_temp[d2][d3] * s_nu_alpha[fieldA + f_off] * s_invMass[fieldA + f_off];
    }
  }
  __syncthreads();
  /* add electric field term once per IP */
  for (fieldA = threadIdx.x; fieldA < loc_Nf; fieldA += blockDim.x) s_gg2[dim - 1][myQi][fieldA] += Eq_m[fieldA + f_off];
  __syncthreads();
  /* Jacobian transform - g2 */
  for (fieldA = threadIdx.x; fieldA < loc_Nf; fieldA += blockDim.x) {
    PetscReal wj = ww[jpidx];
    for (d = 0; d < dim; ++d) {
      s_g2[d][myQi][fieldA] = 0.0;
      for (d2 = 0; d2 < dim; ++d2) {
        s_g2[d][myQi][fieldA] += invJj[d * dim + d2] * s_gg2[d2][myQi][fieldA];
        s_g3[d][d2][myQi][fieldA] = 0.0;
        for (d3 = 0; d3 < dim; ++d3) {
          for (dp = 0; dp < dim; ++dp) s_g3[d][d2][myQi][fieldA] += invJj[d * dim + d3] * s_gg3[d3][dp][myQi][fieldA] * invJj[d2 * dim + dp];
        }
        s_g3[d][d2][myQi][fieldA] *= wj;
      }
      s_g2[d][myQi][fieldA] *= wj;
    }
  }
  __syncthreads(); // Synchronize (ensure all the data is available) and sum IP matrices
  /* FE matrix construction */
  {
    int fieldA, d, qj, d2, q, idx, totDim = Nb * loc_Nf;
    /* assemble */
    for (fieldA = 0; fieldA < loc_Nf; fieldA++) {
      for (f = threadIdx.y; f < Nb; f += blockDim.y) {
        for (g = threadIdx.x; g < Nb; g += blockDim.x) {
          PetscScalar t = 0;
          for (qj = 0; qj < Nq; qj++) {
            const PetscReal *BJq = &BB[qj * Nb], *DIq = &DD[qj * Nb * dim];
            for (d = 0; d < dim; ++d) {
              t += DIq[f * dim + d] * s_g2[d][qj][fieldA] * BJq[g];
              for (d2 = 0; d2 < dim; ++d2) t += DIq[f * dim + d] * s_g3[d][d2][qj][fieldA] * DIq[g * dim + d2];
            }
          }
          if (elemMat) {
            const PetscInt fOff = (fieldA * Nb + f) * totDim + fieldA * Nb + g;
            elemMat[fOff] += t; // ????
          } else s_fieldMats[f][g] = t;
        }
      }
      if (s_fieldMats) {
        PetscScalar            vals[LANDAU_MAX_Q_FACE * LANDAU_MAX_Q_FACE];
        PetscInt               nr, nc;
        const LandauIdx *const Idxs = &d_maps[grid]->gIdx[loc_elem][fieldA][0];
        __syncthreads();
        if (threadIdx.y == 0) {
          for (f = threadIdx.x; f < Nb; f += blockDim.x) {
            idx = Idxs[f];
            if (idx >= 0) {
              s_idx[f][0]   = idx + moffset;
              s_scale[f][0] = 1.;
            } else {
              idx = -idx - 1;
              for (q = 0; q < d_maps[grid]->num_face; q++) {
                if (d_maps[grid]->c_maps[idx][q].gid >= 0) s_idx[f][q] = d_maps[grid]->c_maps[idx][q].gid + moffset;
                else s_idx[f][q] = -1;
                s_scale[f][q] = d_maps[grid]->c_maps[idx][q].scale;
              }
            }
          }
        }
        __syncthreads();
        for (f = threadIdx.y; f < Nb; f += blockDim.y) {
          idx = Idxs[f];
          if (idx >= 0) {
            nr = 1;
          } else {
            nr = d_maps[grid]->num_face;
          }
          for (g = threadIdx.x; g < Nb; g += blockDim.x) {
            idx = Idxs[g];
            if (idx >= 0) {
              nc = 1;
            } else {
              nc = d_maps[grid]->num_face;
            }
            for (q = 0; q < nr; q++) {
              for (d = 0; d < nc; d++) vals[q * nc + d] = s_scale[f][q] * s_scale[g][d] * s_fieldMats[f][g];
            }
            static_cast<void>(MatSetValuesDevice(d_mat, nr, s_idx[f], nc, s_idx[g], vals, ADD_VALUES));
          }
        }
        __syncthreads();
      }
    }
  }
}

//
// The CUDA Landau kernel
//
__global__ void __launch_bounds__(256, 2) landau_jacobian(const PetscInt nip_global, const PetscInt dim, const PetscInt Nb, const PetscInt num_grids, const PetscReal invJj[], const PetscInt Nftot, const PetscReal nu_alpha[], const PetscReal nu_beta[], const PetscReal invMass[], const PetscReal Eq_m[], const PetscReal *const BB, const PetscReal *const DD, const PetscReal xx[], const PetscReal yy[], const PetscReal ww[], PetscScalar d_elem_mats[], P4estVertexMaps *d_maps[], PetscSplitCSRDataStructure d_mat, PetscReal d_f[], PetscReal d_dfdx[], PetscReal d_dfdy[],
  #if LANDAU_DIM == 3
                                                          const PetscReal zz[], PetscReal d_dfdz[],
  #endif
                                                          const PetscInt d_numCells[], const PetscInt d_species_offset[], const PetscInt d_mat_offset[], const PetscInt d_ip_offset[], const PetscInt d_ipf_offset[], const PetscInt d_elem_offset[])
{
  extern __shared__ PetscReal smem[];
  int                         size                                = 0;
  PetscReal(*s_g2)[LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES] = (PetscReal(*)[LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES]) & smem[size];
  size += LANDAU_MAX_NQ * LANDAU_MAX_SPECIES * LANDAU_DIM;
  PetscReal(*s_g3)[LANDAU_DIM][LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES] = (PetscReal(*)[LANDAU_DIM][LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES]) & smem[size];
  size += LANDAU_DIM * LANDAU_DIM * LANDAU_MAX_NQ * LANDAU_MAX_SPECIES;
  PetscReal(*s_gg2)[LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES] = (PetscReal(*)[LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES]) & smem[size];
  size += LANDAU_MAX_NQ * LANDAU_MAX_SPECIES * LANDAU_DIM;
  PetscReal(*s_gg3)[LANDAU_DIM][LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES] = (PetscReal(*)[LANDAU_DIM][LANDAU_DIM][LANDAU_MAX_NQ][LANDAU_MAX_SPECIES]) & smem[size];
  size += LANDAU_DIM * LANDAU_DIM * LANDAU_MAX_NQ * LANDAU_MAX_SPECIES;
  PetscReal *s_nu_alpha = &smem[size];
  size += LANDAU_MAX_SPECIES;
  PetscReal *s_nu_beta = &smem[size];
  size += LANDAU_MAX_SPECIES;
  PetscReal *s_invMass = &smem[size];
  size += LANDAU_MAX_SPECIES;
  PetscReal *s_f = &smem[size];
  size += blockDim.x * LANDAU_MAX_SPECIES;
  PetscReal *s_dfx = &smem[size];
  size += blockDim.x * LANDAU_MAX_SPECIES;
  PetscReal *s_dfy = &smem[size];
  size += blockDim.x * LANDAU_MAX_SPECIES;
  #if LANDAU_DIM == 3
  PetscReal *s_dfz = &smem[size];
  size += blockDim.x * LANDAU_MAX_SPECIES;
  #endif
  PetscScalar(*s_fieldMats)[LANDAU_MAX_NQ][LANDAU_MAX_NQ];
  PetscReal(*s_scale)[LANDAU_MAX_NQ][LANDAU_MAX_Q_FACE] = nullptr;
  PetscInt(*s_idx)[LANDAU_MAX_NQ][LANDAU_MAX_Q_FACE]    = nullptr;
  const PetscInt b_elem_idx = blockIdx.y, b_id = blockIdx.x;
  PetscInt       Nq = blockDim.y, grid = 0; // Nq == Nb
  PetscScalar   *elemMat = NULL;            /* my output */
  while (b_elem_idx >= d_elem_offset[grid + 1]) grid++;
  {
    const PetscInt   loc_Nf = d_species_offset[grid + 1] - d_species_offset[grid], loc_elem = b_elem_idx - d_elem_offset[grid];
    const PetscInt   myQi  = threadIdx.y;
    const PetscInt   jpidx = d_ip_offset[grid] + myQi + loc_elem * Nq;
    const PetscReal *invJ  = &invJj[jpidx * dim * dim];
    if (d_elem_mats) {
      PetscInt totDim = loc_Nf * Nb;
      elemMat         = d_elem_mats; // start a beginning and get to my element matrix
      for (PetscInt b_id2 = 0; b_id2 < b_id; b_id2++) {
        for (PetscInt grid2 = 0; grid2 < num_grids; grid2++) {
          PetscInt Nfloc2 = d_species_offset[grid2 + 1] - d_species_offset[grid2], totDim2 = Nfloc2 * Nb;
          elemMat += d_numCells[grid2] * totDim2 * totDim2; // jump past grids,could be in an offset
        }
      }
      for (PetscInt grid2 = 0; grid2 < grid; grid2++) {
        PetscInt Nfloc2 = d_species_offset[grid2 + 1] - d_species_offset[grid2], totDim2 = Nfloc2 * Nb;
        elemMat += d_numCells[grid2] * totDim2 * totDim2; // jump past grids, could be in an offset
      }
      elemMat += loc_elem * totDim * totDim; // index into local matrix & zero out
      for (int i = threadIdx.x + threadIdx.y * blockDim.x; i < totDim * totDim; i += blockDim.x * blockDim.y) elemMat[i] = 0;
    }
    __syncthreads();
    if (d_maps) {
      // reuse the space for fieldMats
      s_fieldMats = (PetscScalar(*)[LANDAU_MAX_NQ][LANDAU_MAX_NQ]) & smem[size];
      size += LANDAU_MAX_NQ * LANDAU_MAX_NQ;
      s_scale = (PetscReal(*)[LANDAU_MAX_NQ][LANDAU_MAX_Q_FACE]) & smem[size];
      size += LANDAU_MAX_NQ * LANDAU_MAX_Q_FACE;
      s_idx = (PetscInt(*)[LANDAU_MAX_NQ][LANDAU_MAX_Q_FACE]) & smem[size];
      size += LANDAU_MAX_NQ * LANDAU_MAX_Q_FACE; // this is too big, idx is an integer
    } else {
      s_fieldMats = NULL;
    }
    __syncthreads();
    landau_jac_kernel(num_grids, jpidx, nip_global, grid, xx, yy, ww, invJ, Nftot, nu_alpha, nu_beta, invMass, Eq_m, BB, DD, elemMat, d_maps, d_mat, *s_fieldMats, *s_scale, *s_idx, *s_g2, *s_g3, *s_gg2, *s_gg3, s_nu_alpha, s_nu_beta, s_invMass, s_f, s_dfx, s_dfy, d_f, d_dfdx, d_dfdy,
  #if LANDAU_DIM == 3
                      zz, s_dfz, d_dfdz,
  #endif
                      d_numCells, d_species_offset, d_mat_offset, d_ip_offset, d_ipf_offset, d_elem_offset);
  }
}

__global__ void __launch_bounds__(256, 4) landau_mass(const PetscInt dim, const PetscInt Nb, const PetscInt num_grids, const PetscReal d_w[], const PetscReal *const BB, const PetscReal *const DD, PetscScalar d_elem_mats[], P4estVertexMaps *d_maps[], PetscSplitCSRDataStructure d_mat, PetscReal shift, const PetscInt d_numCells[], const PetscInt d_species_offset[], const PetscInt d_mat_offset[], const PetscInt d_ip_offset[], const PetscInt d_elem_offset[])
{
  extern __shared__ PetscReal smem[];
  const PetscInt              Nq = blockDim.y, b_elem_idx = blockIdx.y, b_id = blockIdx.x;
  PetscScalar                *elemMat = NULL; /* my output */
  PetscInt                    fieldA, d, qj, q, idx, f, g, grid = 0, size = 0;
  PetscScalar(*s_fieldMats)[LANDAU_MAX_NQ][LANDAU_MAX_NQ];
  PetscReal(*s_scale)[LANDAU_MAX_NQ][LANDAU_MAX_Q_FACE];
  PetscInt(*s_idx)[LANDAU_MAX_NQ][LANDAU_MAX_Q_FACE];
  if (d_maps) {
    // reuse the space for fieldMats
    s_fieldMats = (PetscScalar(*)[LANDAU_MAX_NQ][LANDAU_MAX_NQ]) & smem[size];
    size += LANDAU_MAX_NQ * LANDAU_MAX_NQ;
    s_scale = (PetscReal(*)[LANDAU_MAX_NQ][LANDAU_MAX_Q_FACE]) & smem[size];
    size += LANDAU_MAX_NQ * LANDAU_MAX_Q_FACE;
    s_idx = (PetscInt(*)[LANDAU_MAX_NQ][LANDAU_MAX_Q_FACE]) & smem[size];
    size += LANDAU_MAX_NQ * LANDAU_MAX_Q_FACE; // this is too big, idx is an integer
  } else {
    s_fieldMats = NULL;
  }
  while (b_elem_idx >= d_elem_offset[grid + 1]) grid++;
  {
    const PetscInt loc_Nf = d_species_offset[grid + 1] - d_species_offset[grid], loc_elem = b_elem_idx - d_elem_offset[grid];
    const PetscInt moffset = LAND_MOFFSET(b_id, grid, gridDim.x, num_grids, d_mat_offset), totDim = loc_Nf * Nq;
    if (d_elem_mats) {
      elemMat = d_elem_mats; // start a beginning
      for (PetscInt b_id2 = 0; b_id2 < b_id; b_id2++) {
        for (PetscInt grid2 = 0; grid2 < num_grids; grid2++) {
          PetscInt Nfloc2 = d_species_offset[grid2 + 1] - d_species_offset[grid2], totDim2 = Nfloc2 * Nb;
          elemMat += d_numCells[grid2] * totDim2 * totDim2; // jump past grids,could be in an offset
        }
      }
      for (PetscInt grid2 = 0; grid2 < grid; grid2++) {
        PetscInt Nfloc2 = d_species_offset[grid2 + 1] - d_species_offset[grid2], totDim2 = Nfloc2 * Nb;
        elemMat += d_numCells[grid2] * totDim2 * totDim2; // jump past grids,could be in an offset
      }
      elemMat += loc_elem * totDim * totDim;
      for (int i = threadIdx.x + threadIdx.y * blockDim.x; i < totDim * totDim; i += blockDim.x * blockDim.y) elemMat[i] = 0;
    }
    __syncthreads();
    /* FE mass matrix construction */
    for (fieldA = 0; fieldA < loc_Nf; fieldA++) {
      PetscScalar vals[LANDAU_MAX_Q_FACE * LANDAU_MAX_Q_FACE];
      PetscInt    nr, nc;
      for (f = threadIdx.y; f < Nb; f += blockDim.y) {
        for (g = threadIdx.x; g < Nb; g += blockDim.x) {
          PetscScalar t = 0;
          for (qj = 0; qj < Nq; qj++) {
            const PetscReal *BJq   = &BB[qj * Nb];
            const PetscInt   jpidx = d_ip_offset[grid] + qj + loc_elem * Nq;
            if (dim == 2) {
              t += BJq[f] * d_w[jpidx] * shift * BJq[g] * 2. * PETSC_PI;
            } else {
              t += BJq[f] * d_w[jpidx] * shift * BJq[g];
            }
          }
          if (elemMat) {
            const PetscInt fOff = (fieldA * Nb + f) * totDim + fieldA * Nb + g;
            elemMat[fOff] += t; // ????
          } else (*s_fieldMats)[f][g] = t;
        }
      }
      if (!elemMat) {
        const LandauIdx *const Idxs = &d_maps[grid]->gIdx[loc_elem][fieldA][0];
        __syncthreads();
        if (threadIdx.y == 0) {
          for (f = threadIdx.x; f < Nb; f += blockDim.x) {
            idx = Idxs[f];
            if (idx >= 0) {
              (*s_idx)[f][0]   = idx + moffset;
              (*s_scale)[f][0] = 1.;
            } else {
              idx = -idx - 1;
              for (q = 0; q < d_maps[grid]->num_face; q++) {
                if (d_maps[grid]->c_maps[idx][q].gid >= 0) (*s_idx)[f][q] = d_maps[grid]->c_maps[idx][q].gid + moffset;
                else (*s_idx)[f][q] = -1;
                (*s_scale)[f][q] = d_maps[grid]->c_maps[idx][q].scale;
              }
            }
          }
        }
        __syncthreads();
        for (f = threadIdx.y; f < Nb; f += blockDim.y) {
          idx = Idxs[f];
          if (idx >= 0) {
            nr = 1;
          } else {
            nr = d_maps[grid]->num_face;
          }
          for (g = threadIdx.x; g < Nb; g += blockDim.x) {
            idx = Idxs[g];
            if (idx >= 0) {
              nc = 1;
            } else {
              nc = d_maps[grid]->num_face;
            }
            for (q = 0; q < nr; q++) {
              for (d = 0; d < nc; d++) vals[q * nc + d] = (*s_scale)[f][q] * (*s_scale)[g][d] * (*s_fieldMats)[f][g];
            }
            static_cast<void>(MatSetValuesDevice(d_mat, nr, (*s_idx)[f], nc, (*s_idx)[g], vals, ADD_VALUES));
          }
        }
      }
      __syncthreads();
    }
  }
}

PetscErrorCode LandauCUDAJacobian(DM plex[], const PetscInt Nq, const PetscInt batch_sz, const PetscInt num_grids, const PetscInt a_numCells[], PetscReal a_Eq_m[], PetscScalar a_elem_closure[], const PetscScalar a_xarray[], const LandauStaticData *SData_d, const PetscReal shift, const PetscLogEvent events[], const PetscInt a_mat_offset[], const PetscInt a_species_offset[], Mat subJ[], Mat JacP)
{
  cudaError_t                cerr;
  PetscInt                   Nb = Nq, dim, nip_global, num_cells_batch, elem_mat_size_tot;
  PetscInt                  *d_numCells, *d_species_offset, *d_mat_offset, *d_ip_offset, *d_ipf_offset, *d_elem_offset;
  PetscInt                   szf = sizeof(PetscReal), szs = sizeof(PetscScalar), Nftot = a_species_offset[num_grids];
  PetscReal                 *d_BB = NULL, *d_DD = NULL, *d_invJj = NULL, *d_nu_alpha = NULL, *d_nu_beta = NULL, *d_invMass = NULL, *d_Eq_m = NULL, *d_x = NULL, *d_y = NULL, *d_w = NULL;
  PetscScalar               *d_elem_mats = NULL, *d_vertex_f = NULL;
  PetscReal                 *d_f = NULL, *d_dfdx = NULL, *d_dfdy = NULL;
  #if LANDAU_DIM == 3
  PetscReal                 *d_dfdz = NULL, *d_z = NULL;
  #endif
  LandauCtx                 *ctx;
  PetscSplitCSRDataStructure d_mat = NULL;
  P4estVertexMaps          **d_maps, *maps[LANDAU_MAX_GRIDS];
  int                        nnn = 256 / Nq; // machine dependent
  PetscContainer             container;

  PetscFunctionBegin;
  PetscCall(PetscLogEventBegin(events[3], 0, 0, 0, 0));
  while (nnn & nnn - 1) nnn = nnn & nnn - 1;
  if (nnn > 16) nnn = 16;
  PetscCall(DMGetApplicationContext(plex[0], &ctx));
  PetscCheck(ctx, PETSC_COMM_SELF, PETSC_ERR_PLIB, "no context");
  PetscCall(DMGetDimension(plex[0], &dim));
  PetscCheck(dim == LANDAU_DIM, PETSC_COMM_SELF, PETSC_ERR_PLIB, "LANDAU_DIM %d != dim %" PetscInt_FMT, LANDAU_DIM, dim);
  if (ctx->gpu_assembly) {
    PetscCall(PetscObjectQuery((PetscObject)JacP, "assembly_maps", (PetscObject *)&container));
    if (container) {       // not here first call
      static int init = 0; // hack. just do every time, or put in setup (but that is in base class code), or add init_maps flag
      if (!init++) {
        P4estVertexMaps *h_maps = NULL;
        PetscCall(PetscContainerGetPointer(container, (void **)&h_maps));
        for (PetscInt grid = 0; grid < num_grids; grid++) {
          if (h_maps[grid].d_self) {
            maps[grid] = h_maps[grid].d_self;
          } else {
            SETERRQ(PETSC_COMM_SELF, PETSC_ERR_PLIB, "GPU assembly but no metadata in container");
          }
        }
        PetscCallCUDA(cudaMemcpy(SData_d->maps, maps, num_grids * sizeof(P4estVertexMaps *), cudaMemcpyHostToDevice));
      }
      d_maps = (P4estVertexMaps **)SData_d->maps;
      // this does the setup the first time called
      PetscCall(MatCUSPARSEGetDeviceMatWrite(JacP, &d_mat));
    } else {
      d_maps = NULL;
    }
  } else {
    container = NULL;
    d_maps    = NULL;
  }
  PetscCall(PetscLogEventEnd(events[3], 0, 0, 0, 0));
  {
    PetscInt elem_mat_size = 0;
    nip_global = num_cells_batch = 0;
    for (PetscInt grid = 0; grid < num_grids; grid++) {
      PetscInt Nfloc = a_species_offset[grid + 1] - a_species_offset[grid], totDim = Nfloc * Nb;
      nip_global += a_numCells[grid] * Nq;
      num_cells_batch += a_numCells[grid];                 // is in d_elem_offset, but not on host
      elem_mat_size += a_numCells[grid] * totDim * totDim; // could save in an offset here -- batch major ordering
    }
    elem_mat_size_tot = d_maps ? 0 : elem_mat_size;
  }
  dim3 dimGrid(batch_sz, num_cells_batch);
  if (elem_mat_size_tot) {
    PetscCallCUDA(cudaMalloc((void **)&d_elem_mats, batch_sz * elem_mat_size_tot * szs)); // kernel output - first call is on CPU
  } else d_elem_mats = NULL;
  // create data
  d_BB = (PetscReal *)SData_d->B;
  d_DD = (PetscReal *)SData_d->D;
  if (a_elem_closure || a_xarray) { // form f and df
    PetscCall(PetscLogEventBegin(events[1], 0, 0, 0, 0));
    PetscCallCUDA(cudaMemcpy(SData_d->Eq_m, a_Eq_m, Nftot * szf, cudaMemcpyHostToDevice));
    d_invJj    = (PetscReal *)SData_d->invJ;
    d_nu_alpha = (PetscReal *)SData_d->alpha;
    d_nu_beta  = (PetscReal *)SData_d->beta;
    d_invMass  = (PetscReal *)SData_d->invMass;
    d_x        = (PetscReal *)SData_d->x;
    d_y        = (PetscReal *)SData_d->y;
    d_w        = (PetscReal *)SData_d->w;
    d_Eq_m     = (PetscReal *)SData_d->Eq_m;
    d_dfdx     = (PetscReal *)SData_d->dfdx;
    d_dfdy     = (PetscReal *)SData_d->dfdy;
  #if LANDAU_DIM == 3
    d_dfdz     = (PetscReal *)SData_d->dfdz;
    d_z        = (PetscReal *)SData_d->z;
  #endif
    d_f        = (PetscReal *)SData_d->f;
    // get a d_vertex_f
    if (a_elem_closure) {
      PetscInt closure_sz = 0; // argh, don't have this on the host!!!
      for (PetscInt grid = 0; grid < num_grids; grid++) {
        PetscInt nfloc = a_species_offset[grid + 1] - a_species_offset[grid];
        closure_sz += Nq * nfloc * a_numCells[grid];
      }
      closure_sz *= batch_sz;
      PetscCallCUDA(cudaMalloc((void **)&d_vertex_f, closure_sz * sizeof(*a_elem_closure)));
      PetscCallCUDA(cudaMemcpy(d_vertex_f, a_elem_closure, closure_sz * sizeof(*a_elem_closure), cudaMemcpyHostToDevice));
    } else {
      d_vertex_f = (PetscScalar *)a_xarray;
    }
    PetscCall(PetscLogEventEnd(events[1], 0, 0, 0, 0));
  } else {
    d_w = (PetscReal *)SData_d->w; // mass just needs the weights
  }
  //
  d_numCells       = (PetscInt *)SData_d->NCells; // redundant -- remove
  d_species_offset = (PetscInt *)SData_d->species_offset;
  d_mat_offset     = (PetscInt *)SData_d->mat_offset;
  d_ip_offset      = (PetscInt *)SData_d->ip_offset;
  d_ipf_offset     = (PetscInt *)SData_d->ipf_offset;
  d_elem_offset    = (PetscInt *)SData_d->elem_offset;
  if (a_elem_closure || a_xarray) { // form f and df
    dim3 dimBlockFDF(nnn > Nftot ? Nftot : nnn, Nq), dimBlock((nip_global > nnn) ? nnn : nip_global, Nq);
    PetscCall(PetscLogEventBegin(events[8], 0, 0, 0, 0));
    PetscCall(PetscLogGpuTimeBegin());
    PetscCall(PetscInfo(plex[0], "Form F and dF/dx vectors: nip_global=%" PetscInt_FMT " num_grids=%" PetscInt_FMT "\n", nip_global, num_grids));
    landau_form_fdf<<<dimGrid, dimBlockFDF>>>(dim, Nb, num_grids, d_invJj, d_BB, d_DD, d_vertex_f, d_maps, d_f, d_dfdx, d_dfdy,
  #if LANDAU_DIM == 3
                                              d_dfdz,
  #endif
                                              d_numCells, d_species_offset, d_mat_offset, d_ip_offset, d_ipf_offset, d_elem_offset);
    PetscCUDACheckLaunch;
    PetscCall(PetscLogGpuFlops(batch_sz * nip_global * (PetscLogDouble)(2 * Nb * (1 + dim))));
    if (a_elem_closure) {
      PetscCallCUDA(cudaFree(d_vertex_f));
      d_vertex_f = NULL;
    }
    PetscCall(PetscLogGpuTimeEnd());
    PetscCall(PetscLogEventEnd(events[8], 0, 0, 0, 0));
    // Jacobian
    PetscCall(PetscLogEventBegin(events[4], 0, 0, 0, 0));
    PetscCall(PetscLogGpuTimeBegin());
    PetscCall(PetscLogGpuFlops(batch_sz * nip_global * (PetscLogDouble)(a_elem_closure ? (nip_global * (11 * Nftot + 4 * dim * dim) + 6 * Nftot * dim * dim * dim + 10 * Nftot * dim * dim + 4 * Nftot * dim + Nb * Nftot * Nb * Nq * dim * dim * 5) : Nb * Nftot * Nb * Nq * 4)));
    PetscInt ii = 2 * LANDAU_MAX_NQ * LANDAU_MAX_SPECIES * LANDAU_DIM * (1 + LANDAU_DIM) + 3 * LANDAU_MAX_SPECIES + (1 + LANDAU_DIM) * dimBlock.x * LANDAU_MAX_SPECIES + LANDAU_MAX_NQ * LANDAU_MAX_NQ + 2 * LANDAU_MAX_NQ * LANDAU_MAX_Q_FACE;
    if (ii * szf >= 49152) {
      cerr = cudaFuncSetAttribute(landau_jacobian, cudaFuncAttributeMaxDynamicSharedMemorySize, 98304);
      PetscCallCUDA(cerr);
    }
    PetscCall(PetscInfo(plex[0], "Jacobian shared memory size: %" PetscInt_FMT " bytes, d_elem_mats=%p d_maps=%p\n", ii, d_elem_mats, d_maps));
    landau_jacobian<<<dimGrid, dimBlock, ii * szf>>>(nip_global, dim, Nb, num_grids, d_invJj, Nftot, d_nu_alpha, d_nu_beta, d_invMass, d_Eq_m, d_BB, d_DD, d_x, d_y, d_w, d_elem_mats, d_maps, d_mat, d_f, d_dfdx, d_dfdy,
  #if LANDAU_DIM == 3
                                                     d_z, d_dfdz,
  #endif
                                                     d_numCells, d_species_offset, d_mat_offset, d_ip_offset, d_ipf_offset, d_elem_offset);
    PetscCUDACheckLaunch; // has sync
    PetscCall(PetscLogGpuTimeEnd());
    PetscCall(PetscLogEventEnd(events[4], 0, 0, 0, 0));
  } else { // mass
    dim3     dimBlock(nnn, Nq);
    PetscInt ii = LANDAU_MAX_NQ * LANDAU_MAX_NQ + 2 * LANDAU_MAX_NQ * LANDAU_MAX_Q_FACE;
    if (ii * szf >= 49152) {
      cerr = cudaFuncSetAttribute(landau_mass, cudaFuncAttributeMaxDynamicSharedMemorySize, 98304);
      PetscCallCUDA(cerr);
    }
    PetscCall(PetscInfo(plex[0], "Mass d_maps = %p. Nq=%" PetscInt_FMT ", vector size %d num_cells_batch=%" PetscInt_FMT ", %" PetscInt_FMT " shared memory words\n", d_maps, Nq, nnn, num_cells_batch, ii));
    PetscCall(PetscLogEventBegin(events[16], 0, 0, 0, 0));
    PetscCall(PetscLogGpuTimeBegin());
    landau_mass<<<dimGrid, dimBlock, ii * szf>>>(dim, Nb, num_grids, d_w, d_BB, d_DD, d_elem_mats, d_maps, d_mat, shift, d_numCells, d_species_offset, d_mat_offset, d_ip_offset, d_elem_offset);
    PetscCUDACheckLaunch; // has sync
    PetscCall(PetscLogGpuTimeEnd());
    PetscCall(PetscLogEventEnd(events[16], 0, 0, 0, 0));
  }
  // First time assembly with or without GPU assembly
  if (d_elem_mats) {
    PetscInt elem_mats_idx = 0;
    for (PetscInt b_id = 0; b_id < batch_sz; b_id++) {    // OpenMP (once)
      for (PetscInt grid = 0; grid < num_grids; grid++) { // elem_mats_idx += totDim*totDim*a_numCells[grid];
        const PetscInt     Nfloc = a_species_offset[grid + 1] - a_species_offset[grid], totDim = Nfloc * Nq;
        PetscScalar       *elemMats = NULL, *elMat;
        PetscSection       section, globalSection;
        PetscInt           cStart, cEnd, ej;
        PetscInt           moffset = LAND_MOFFSET(b_id, grid, batch_sz, num_grids, a_mat_offset), nloc, nzl, colbuf[1024], row;
        const PetscInt    *cols;
        const PetscScalar *vals;
        Mat                B = subJ[LAND_PACK_IDX(b_id, grid)];
        PetscCall(PetscLogEventBegin(events[5], 0, 0, 0, 0));
        PetscCall(DMPlexGetHeightStratum(plex[grid], 0, &cStart, &cEnd));
        PetscCall(DMGetLocalSection(plex[grid], &section));
        PetscCall(DMGetGlobalSection(plex[grid], &globalSection));
        PetscCall(PetscMalloc1(totDim * totDim * a_numCells[grid], &elemMats));
        PetscCallCUDA(cudaMemcpy(elemMats, &d_elem_mats[elem_mats_idx], totDim * totDim * a_numCells[grid] * sizeof(*elemMats), cudaMemcpyDeviceToHost));
        PetscCall(PetscLogEventEnd(events[5], 0, 0, 0, 0));
        PetscCall(PetscLogEventBegin(events[6], 0, 0, 0, 0));
        for (ej = cStart, elMat = elemMats; ej < cEnd; ++ej, elMat += totDim * totDim) {
          PetscCall(DMPlexMatSetClosure(plex[grid], section, globalSection, B, ej, elMat, ADD_VALUES));
          if (ej == -1) {
            int d, f;
            PetscCall(PetscPrintf(PETSC_COMM_SELF, "GPU Element matrix\n"));
            for (d = 0; d < totDim; ++d) {
              for (f = 0; f < totDim; ++f) PetscCall(PetscPrintf(PETSC_COMM_SELF, " %12.5e", PetscRealPart(elMat[d * totDim + f])));
              PetscCall(PetscPrintf(PETSC_COMM_SELF, "\n"));
            }
          }
        }
        PetscCall(PetscFree(elemMats));
        PetscCall(MatAssemblyBegin(B, MAT_FINAL_ASSEMBLY));
        PetscCall(MatAssemblyEnd(B, MAT_FINAL_ASSEMBLY));
        // move nest matrix to global JacP
        PetscCall(MatGetSize(B, &nloc, NULL));
        for (int i = 0; i < nloc; i++) {
          PetscCall(MatGetRow(B, i, &nzl, &cols, &vals));
          PetscCheck(nzl <= 1024, PetscObjectComm((PetscObject)B), PETSC_ERR_PLIB, "Row too big: %" PetscInt_FMT, nzl);
          for (int j = 0; j < nzl; j++) colbuf[j] = cols[j] + moffset;
          row = i + moffset;
          PetscCall(MatSetValues(JacP, 1, &row, nzl, colbuf, vals, ADD_VALUES));
          PetscCall(MatRestoreRow(B, i, &nzl, &cols, &vals));
        }
        PetscCall(MatDestroy(&B));
        PetscCall(PetscLogEventEnd(events[6], 0, 0, 0, 0));
        elem_mats_idx += totDim * totDim * a_numCells[grid]; // this can be a stored offset?
      }                                                      // grids
    }
    PetscCheck(elem_mats_idx == batch_sz * elem_mat_size_tot, PetscObjectComm((PetscObject)JacP), PETSC_ERR_PLIB, "elem_mats_idx != batch_sz*elem_mat_size_tot: %" PetscInt_FMT " %" PetscInt_FMT, elem_mats_idx, batch_sz * elem_mat_size_tot);
    PetscCallCUDA(cudaFree(d_elem_mats));
  }

  PetscFunctionReturn(PETSC_SUCCESS);
}
#endif

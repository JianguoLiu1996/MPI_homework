static char help[] = "Benchmarking various accessing methods of DMDA vectors on host\n\n";

/*
  On a machine with AMD EPYC-7452 CPUs, we got this data using one MPI rank and a serial-only Kokkos:
           Time (sec.), on Mar. 1, 2022
  ------------------------------------------
  n     PETSc          C          Kokkos
  ------------------------------------------
  32    4.6464E-05  4.7451E-05  1.6880E-04
  64    2.5654E-04  2.5164E-04  5.6780E-04
  128   1.9383E-03  1.8878E-03  4.7938E-03
  256   1.4450E-02  1.3619E-02  3.7778E-02
  512   1.1580E-01  1.1551E-01  2.8428E-01
  1024  1.4179E+00  1.3772E+00  2.2873E+00

  Overall, C is -2% ~ 5% faster than PETSc. But Kokkos is 1.6~3.6x slower than PETSc
*/

#include <petscdmda_kokkos.hpp>
#include <petscdm.h>
#include <petscdmda.h>

using Kokkos::Iterate;
using Kokkos::MDRangePolicy;
using Kokkos::Rank;
using PetscScalarKokkosOffsetView3D      = Kokkos::Experimental::OffsetView<PetscScalar ***, Kokkos::LayoutRight, Kokkos::HostSpace>;
using ConstPetscScalarKokkosOffsetView3D = Kokkos::Experimental::OffsetView<const PetscScalar ***, Kokkos::LayoutRight, Kokkos::HostSpace>;

/* PETSc multi-dimensional array access */
static PetscErrorCode Update1(DM da, const PetscScalar ***__restrict__ x1, PetscScalar ***__restrict__ y1, PetscInt nwarm, PetscInt nloop, PetscLogDouble *avgTime)
{
  PetscInt       it, i, j, k;
  PetscLogDouble tstart = 0.0, tend;
  PetscInt       xm, ym, zm, xs, ys, zs, gxm, gym, gzm, gxs, gys, gzs;

  PetscFunctionBegin;
  PetscCall(DMDAGetCorners(da, &xs, &ys, &zs, &xm, &ym, &zm));
  PetscCall(DMDAGetGhostCorners(da, &gxs, &gys, &gzs, &gxm, &gym, &gzm));
  for (it = 0; it < nwarm + nloop; it++) {
    if (it == nwarm) PetscCall(PetscTime(&tstart));
    for (k = zs; k < zs + zm; k++) {
      for (j = ys; j < ys + ym; j++) {
        for (i = xs; i < xs + xm; i++) y1[k][j][i] = 6 * x1[k][j][i] - x1[k - 1][j][i] - x1[k][j - 1][i] - x1[k][j][i - 1] - x1[k + 1][j][i] - x1[k][j + 1][i] - x1[k][j][i + 1];
      }
    }
  }
  PetscCall(PetscTime(&tend));
  *avgTime = (tend - tstart) / nloop;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* C multi-dimensional array access */
static PetscErrorCode Update2(DM da, const PetscScalar *__restrict__ x2, PetscScalar *__restrict__ y2, PetscInt nwarm, PetscInt nloop, PetscLogDouble *avgTime)
{
  PetscInt       it, i, j, k;
  PetscLogDouble tstart = 0.0, tend;
  PetscInt       xm, ym, zm, xs, ys, zs, gxm, gym, gzm, gxs, gys, gzs;

  PetscFunctionBegin;
  PetscCall(DMDAGetCorners(da, &xs, &ys, &zs, &xm, &ym, &zm));
  PetscCall(DMDAGetGhostCorners(da, &gxs, &gys, &gzs, &gxm, &gym, &gzm));
#define X2(k, j, i) x2[(k - gzs) * gym * gxm + (j - gys) * gxm + (i - gxs)]
#define Y2(k, j, i) y2[(k - zs) * ym * xm + (j - ys) * xm + (i - xs)]
  for (it = 0; it < nwarm + nloop; it++) {
    if (it == nwarm) PetscCall(PetscTime(&tstart));
    for (k = zs; k < zs + zm; k++) {
      for (j = ys; j < ys + ym; j++) {
        for (i = xs; i < xs + xm; i++) Y2(k, j, i) = 6 * X2(k, j, i) - X2(k - 1, j, i) - X2(k, j - 1, i) - X2(k, j, i - 1) - X2(k + 1, j, i) - X2(k, j + 1, i) - X2(k, j, i + 1);
      }
    }
  }
  PetscCall(PetscTime(&tend));
  *avgTime = (tend - tstart) / nloop;
#undef X2
#undef Y2
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  DM                                 da;
  PetscInt                           xm, ym, zm, xs, ys, zs, gxm, gym, gzm, gxs, gys, gzs;
  PetscInt                           dof = 1, sw = 1;
  DMBoundaryType                     bx = DM_BOUNDARY_PERIODIC, by = DM_BOUNDARY_PERIODIC, bz = DM_BOUNDARY_PERIODIC;
  DMDAStencilType                    st = DMDA_STENCIL_STAR;
  Vec                                x, y; /* local/global vectors of the da */
  PetscRandom                        rctx;
  const PetscScalar               ***x1;
  PetscScalar                     ***y1; /* arrays of g, ll */
  const PetscScalar                 *x2;
  PetscScalar                       *y2;
  ConstPetscScalarKokkosOffsetView3D x3;
  PetscScalarKokkosOffsetView3D      y3;
  PetscLogDouble                     tstart = 0.0, tend = 0.0, avgTime = 0.0;
  PetscInt                           nwarm = 2, nloop = 10;
  PetscInt                           min = 32, max = 32 * 8; /* min and max sizes of the grids to sample */

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCall(PetscRandomCreate(PETSC_COMM_WORLD, &rctx));
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-min", &min, NULL));
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-max", &max, NULL));

  for (PetscInt len = min; len <= max; len = len * 2) {
    PetscCall(DMDACreate3d(PETSC_COMM_WORLD, bx, by, bz, st, len, len, len, PETSC_DECIDE, PETSC_DECIDE, PETSC_DECIDE, dof, sw, 0, 0, 0, &da));
    PetscCall(DMSetFromOptions(da));
    PetscCall(DMSetUp(da));

    PetscCall(DMDAGetCorners(da, &xs, &ys, &zs, &xm, &ym, &zm));
    PetscCall(DMDAGetGhostCorners(da, &gxs, &gys, &gzs, &gxm, &gym, &gzm));
    PetscCall(DMCreateLocalVector(da, &x)); /* Create local x and global y */
    PetscCall(DMCreateGlobalVector(da, &y));

    /* Access with petsc multi-dimensional arrays */
    PetscCall(VecSetRandom(x, rctx));
    PetscCall(VecSet(y, 0.0));
    PetscCall(DMDAVecGetArrayRead(da, x, &x1));
    PetscCall(DMDAVecGetArray(da, y, &y1));
    PetscCall(Update1(da, x1, y1, nwarm, nloop, &avgTime));
    PetscCall(DMDAVecRestoreArrayRead(da, x, &x1));
    PetscCall(DMDAVecRestoreArray(da, y, &y1));
    PetscCall(PetscTime(&tend));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%4d^3 -- PETSc average time  = %e\n", len, avgTime));

    /* Access with C multi-dimensional arrays */
    PetscCall(VecSetRandom(x, rctx));
    PetscCall(VecSet(y, 0.0));
    PetscCall(VecGetArrayRead(x, &x2));
    PetscCall(VecGetArray(y, &y2));
    PetscCall(Update2(da, x2, y2, nwarm, nloop, &avgTime));
    PetscCall(VecRestoreArrayRead(x, &x2));
    PetscCall(VecRestoreArray(y, &y2));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%4d^3 -- C average time      = %e\n", len, avgTime));

    /* Access with Kokkos multi-dimensional OffsetViews */
    PetscCall(VecSet(y, 0.0));
    PetscCall(VecSetRandom(x, rctx));
    PetscCall(DMDAVecGetKokkosOffsetView(da, x, &x3));
    PetscCall(DMDAVecGetKokkosOffsetView(da, y, &y3));

    for (PetscInt it = 0; it < nwarm + nloop; it++) {
      if (it == nwarm) PetscCall(PetscTime(&tstart));
      Kokkos::parallel_for(
        "stencil", MDRangePolicy<Kokkos::DefaultHostExecutionSpace, Rank<3, Iterate::Right, Iterate::Right>>({zs, ys, xs}, {zs + zm, ys + ym, xs + xm}),
        KOKKOS_LAMBDA(PetscInt k, PetscInt j, PetscInt i) { y3(k, j, i) = 6 * x3(k, j, i) - x3(k - 1, j, i) - x3(k, j - 1, i) - x3(k, j, i - 1) - x3(k + 1, j, i) - x3(k, j + 1, i) - x3(k, j, i + 1); });
    }
    PetscCall(PetscTime(&tend));
    PetscCall(DMDAVecRestoreKokkosOffsetView(da, x, &x3));
    PetscCall(DMDAVecRestoreKokkosOffsetView(da, y, &y3));
    avgTime = (tend - tstart) / nloop;
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%4d^3 -- Kokkos average time = %e\n", len, avgTime));

    PetscCall(VecDestroy(&x));
    PetscCall(VecDestroy(&y));
    PetscCall(DMDestroy(&da));
  }
  PetscCall(PetscRandomDestroy(&rctx));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST
  build:
    requires: kokkos_kernels

  test:
    suffix: 1
    requires: kokkos_kernels
    args: -min 32 -max 32 -dm_vec_type kokkos
    filter: grep -v "time"

TEST*/

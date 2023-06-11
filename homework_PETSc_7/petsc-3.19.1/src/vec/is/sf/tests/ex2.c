static char help[] = "Test SF cuda stream synchronization in device to host communication\n\n";
/*
  SF uses asynchronous operations internally. When destination data is on GPU, it does asynchronous
  operations in the default stream and does not sync these operations since it assumes routines consume
  the destination data are also on the default stream. However, when destination data in on CPU,
  SF must guarantee the data is ready to use on CPU after PetscSFXxxEnd().
 */

#include <petscvec.h>
int main(int argc, char **argv)
{
  PetscInt           i, n = 100000; /* Big enough to make the asynchronous copy meaningful */
  PetscScalar       *val;
  const PetscScalar *yval;
  Vec                x, y;
  PetscMPIInt        size;
  IS                 ix, iy;
  VecScatter         vscat;

  PetscFunctionBegin;
  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));
  PetscCheck(size == 1, PETSC_COMM_WORLD, PETSC_ERR_WRONG_MPI_SIZE, "This is a uni-processor test");

  /* Create two CUDA vectors x, y. Though we only care y's memory on host, we make y a CUDA vector,
     since we want to have y's memory on host pinned (i.e.,non-pagable), to really trigger asynchronous
     cudaMemcpyDeviceToHost.
   */
  PetscCall(VecCreateSeq(PETSC_COMM_WORLD, n, &x));
  PetscCall(VecSetFromOptions(x));
  PetscCall(VecCreateSeq(PETSC_COMM_WORLD, n, &y));
  PetscCall(VecSetFromOptions(y));

  /* Init x, y, and push them to GPU (their offloadmask = PETSC_OFFLOAD_GPU) */
  PetscCall(VecGetArray(x, &val));
  for (i = 0; i < n; i++) val[i] = i / 2.0;
  PetscCall(VecRestoreArray(x, &val));
  PetscCall(VecScale(x, 2.0));
  PetscCall(VecSet(y, 314));

  /* Pull y to CPU (make its offloadmask = PETSC_OFFLOAD_CPU) */
  PetscCall(VecGetArray(y, &val));
  PetscCall(VecRestoreArray(y, &val));

  /* The vscat is simply a vector copy */
  PetscCall(ISCreateStride(PETSC_COMM_SELF, n, 0, 1, &ix));
  PetscCall(ISCreateStride(PETSC_COMM_SELF, n, 0, 1, &iy));
  PetscCall(VecScatterCreate(x, ix, y, iy, &vscat));

  /* Do device to host vecscatter and then immediately use y on host. VecScat/SF may use asynchronous
     cudaMemcpy or kernels, but it must guarantee y is ready to use on host. Otherwise, wrong data will be displayed.
   */
  PetscCall(VecScatterBegin(vscat, x, y, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(VecScatterEnd(vscat, x, y, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(VecGetArrayRead(y, &yval));
  /* Display the first and the last entries of y to see if it is valid on host */
  PetscCall(PetscPrintf(PETSC_COMM_SELF, "y[0]=%.f, y[%" PetscInt_FMT "] = %.f\n", (float)PetscRealPart(yval[0]), n - 1, (float)PetscRealPart(yval[n - 1])));
  PetscCall(VecRestoreArrayRead(y, &yval));

  PetscCall(VecDestroy(&x));
  PetscCall(VecDestroy(&y));
  PetscCall(ISDestroy(&ix));
  PetscCall(ISDestroy(&iy));
  PetscCall(VecScatterDestroy(&vscat));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   test:
    requires: cuda
    diff_args: -j
    #make sure the host memory is pinned
    # sf_backend cuda is not needed if compiling only with cuda
    args: -vec_type cuda -sf_backend cuda -vec_pinned_memory_min 0

   test:
    suffix: hip
    requires: hip
    diff_args: -j
    output_file: output/ex2_1.out
    #make sure the host memory is pinned
    # sf_backend hip is not needed if compiling only with hip
    args:  -vec_type hip -sf_backend hip -vec_pinned_memory_min 0

TEST*/

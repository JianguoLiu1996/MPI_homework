
static char help[] = "Scatters from a sequential vector to a parallel vector. \n\
uses block index sets\n\n";

#include <petscvec.h>

int main(int argc, char **argv)
{
  PetscInt     bs = 1, n = 5, i, low;
  PetscInt     ix0[3] = {5, 7, 9}, iy0[3] = {1, 2, 4}, ix1[3] = {2, 3, 4}, iy1[3] = {0, 1, 3};
  PetscMPIInt  size, rank;
  PetscScalar *array;
  Vec          x, y;
  IS           isx, isy;
  VecScatter   ctx;
  PetscViewer  sviewer;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));
  PetscCallMPI(MPI_Comm_rank(PETSC_COMM_WORLD, &rank));

  PetscCheck(size >= 2, PETSC_COMM_WORLD, PETSC_ERR_WRONG_MPI_SIZE, "Must run more than one processor");

  PetscCall(PetscOptionsGetInt(NULL, NULL, "-bs", &bs, NULL));
  n = bs * n;

  /* Create vector x over shared memory */
  PetscCall(VecCreate(PETSC_COMM_WORLD, &x));
  PetscCall(VecSetSizes(x, n, PETSC_DECIDE));
  PetscCall(VecSetFromOptions(x));

  PetscCall(VecGetOwnershipRange(x, &low, NULL));
  PetscCall(VecGetArray(x, &array));
  for (i = 0; i < n; i++) array[i] = (PetscScalar)(i + low);
  PetscCall(VecRestoreArray(x, &array));

  /* Create a sequential vector y */
  PetscCall(VecCreateSeq(PETSC_COMM_SELF, n, &y));
  PetscCall(VecGetArray(y, &array));
  for (i = 0; i < n; i++) array[i] = (PetscScalar)(i + 100 * rank);
  PetscCall(VecRestoreArray(y, &array));

  /* Create two index sets */
  if (rank == 0) {
    PetscCall(ISCreateBlock(PETSC_COMM_SELF, bs, 3, ix0, PETSC_COPY_VALUES, &isx));
    PetscCall(ISCreateBlock(PETSC_COMM_SELF, bs, 3, iy0, PETSC_COPY_VALUES, &isy));
  } else {
    PetscCall(ISCreateBlock(PETSC_COMM_SELF, bs, 3, ix1, PETSC_COPY_VALUES, &isx));
    PetscCall(ISCreateBlock(PETSC_COMM_SELF, bs, 3, iy1, PETSC_COPY_VALUES, &isy));
  }

  if (rank == 10) {
    PetscCall(PetscPrintf(PETSC_COMM_SELF, "\n[%d] isx:\n", rank));
    PetscCall(ISView(isx, PETSC_VIEWER_STDOUT_SELF));
  }

  PetscCall(VecScatterCreate(y, isy, x, isx, &ctx));
  PetscCall(VecScatterSetFromOptions(ctx));

  /* Test forward vecscatter */
  PetscCall(VecSet(x, 0.0));
  PetscCall(VecScatterBegin(ctx, y, x, ADD_VALUES, SCATTER_FORWARD));
  PetscCall(VecScatterEnd(ctx, y, x, ADD_VALUES, SCATTER_FORWARD));
  PetscCall(VecView(x, PETSC_VIEWER_STDOUT_WORLD));

  /* Test reverse vecscatter */
  PetscCall(VecScale(x, -1.0));
  PetscCall(VecScatterBegin(ctx, x, y, ADD_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterEnd(ctx, x, y, ADD_VALUES, SCATTER_REVERSE));
  PetscCall(PetscViewerGetSubViewer(PETSC_VIEWER_STDOUT_WORLD, PETSC_COMM_SELF, &sviewer));
  if (rank == 1) PetscCall(VecView(y, sviewer));
  PetscCall(PetscViewerRestoreSubViewer(PETSC_VIEWER_STDOUT_WORLD, PETSC_COMM_SELF, &sviewer));

  /* Free spaces */
  PetscCall(VecScatterDestroy(&ctx));
  PetscCall(ISDestroy(&isx));
  PetscCall(ISDestroy(&isy));
  PetscCall(VecDestroy(&x));
  PetscCall(VecDestroy(&y));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   test:
      nsize: 3

TEST*/

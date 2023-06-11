static char help[] = "Tests MatView()/MatLoad() with binary viewers for BAIJ matrices.\n\n";

#include <petscmat.h>
#include <petscviewer.h>

#include <petsc/private/hashtable.h>
static PetscReal MakeValue(PetscInt i, PetscInt j, PetscInt M)
{
  PetscHash_t h = PetscHashCombine(PetscHashInt(i), PetscHashInt(j));
  return (PetscReal)((h % 5 == 0) ? (1 + i + j * M) : 0);
}

static PetscErrorCode CheckValuesAIJ(Mat A)
{
  PetscInt    M, N, rstart, rend, i, j;
  PetscReal   v, w;
  PetscScalar val;

  PetscFunctionBegin;
  PetscCall(MatGetSize(A, &M, &N));
  PetscCall(MatGetOwnershipRange(A, &rstart, &rend));
  for (i = rstart; i < rend; i++) {
    for (j = 0; j < N; j++) {
      PetscCall(MatGetValue(A, i, j, &val));
      v = MakeValue(i, j, M);
      w = PetscRealPart(val);
      PetscCheck(PetscAbsReal(v - w) <= 0, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Matrix entry (%" PetscInt_FMT ",%" PetscInt_FMT ") should be %g, got %g", i, j, (double)v, (double)w);
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **args)
{
  Mat         A;
  PetscInt    M = 24, N = 48, bs = 2;
  PetscInt    rstart, rend, i, j;
  PetscViewer view;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &args, NULL, help));
  /*
      Create a parallel BAIJ matrix shared by all processors
  */
  PetscCall(MatCreateBAIJ(PETSC_COMM_WORLD, bs, PETSC_DECIDE, PETSC_DECIDE, M, N, PETSC_DECIDE, NULL, PETSC_DECIDE, NULL, &A));

  /*
      Set values into the matrix
  */
  PetscCall(MatGetSize(A, &M, &N));
  PetscCall(MatGetOwnershipRange(A, &rstart, &rend));
  for (i = rstart; i < rend; i++) {
    for (j = 0; j < N; j++) {
      PetscReal v = MakeValue(i, j, M);
      if (PetscAbsReal(v) > 0) PetscCall(MatSetValue(A, i, j, v, INSERT_VALUES));
    }
  }
  PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatViewFromOptions(A, NULL, "-mat_base_view"));

  /*
      Store the binary matrix to a file
  */
  PetscCall(PetscViewerBinaryOpen(PETSC_COMM_WORLD, "matrix.dat", FILE_MODE_WRITE, &view));
  for (i = 0; i < 3; i++) PetscCall(MatView(A, view));
  PetscCall(PetscViewerDestroy(&view));
  PetscCall(MatDestroy(&A));

  /*
      Now reload the matrix and check its values
  */
  PetscCall(PetscViewerBinaryOpen(PETSC_COMM_WORLD, "matrix.dat", FILE_MODE_READ, &view));
  PetscCall(MatCreate(PETSC_COMM_WORLD, &A));
  PetscCall(MatSetType(A, MATBAIJ));
  for (i = 0; i < 3; i++) {
    if (i > 0) PetscCall(MatZeroEntries(A));
    PetscCall(MatLoad(A, view));
    PetscCall(CheckValuesAIJ(A));
  }
  PetscCall(PetscViewerDestroy(&view));
  PetscCall(MatViewFromOptions(A, NULL, "-mat_load_view"));
  PetscCall(MatDestroy(&A));

  /*
      Reload in SEQBAIJ matrix and check its values
  */
  PetscCall(PetscViewerBinaryOpen(PETSC_COMM_SELF, "matrix.dat", FILE_MODE_READ, &view));
  PetscCall(MatCreate(PETSC_COMM_SELF, &A));
  PetscCall(MatSetType(A, MATSEQBAIJ));
  for (i = 0; i < 3; i++) {
    if (i > 0) PetscCall(MatZeroEntries(A));
    PetscCall(MatLoad(A, view));
    PetscCall(CheckValuesAIJ(A));
  }
  PetscCall(PetscViewerDestroy(&view));
  PetscCall(MatDestroy(&A));

  /*
     Reload in MPIBAIJ matrix and check its values
  */
  PetscCall(PetscViewerBinaryOpen(PETSC_COMM_WORLD, "matrix.dat", FILE_MODE_READ, &view));
  PetscCall(MatCreate(PETSC_COMM_WORLD, &A));
  PetscCall(MatSetType(A, MATMPIBAIJ));
  for (i = 0; i < 3; i++) {
    if (i > 0) PetscCall(MatZeroEntries(A));
    PetscCall(MatLoad(A, view));
    PetscCall(CheckValuesAIJ(A));
  }
  PetscCall(PetscViewerDestroy(&view));
  PetscCall(MatDestroy(&A));

  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   testset:
      args: -viewer_binary_mpiio 0
      output_file: output/ex45.out
      test:
        suffix: stdio_1
        nsize: 1
      test:
        suffix: stdio_2
        nsize: 2
      test:
        suffix: stdio_3
        nsize: 3
      test:
        suffix: stdio_4
        nsize: 4
      test:
        suffix: stdio_5
        nsize: 4

   testset:
      requires: mpiio
      args: -viewer_binary_mpiio 1
      output_file: output/ex45.out
      test:
        suffix: mpiio_1
        nsize: 1
      test:
        suffix: mpiio_2
        nsize: 2
      test:
        suffix: mpiio_3
        nsize: 3
      test:
        suffix: mpiio_4
        nsize: 4
      test:
        suffix: mpiio_5
        nsize: 5

TEST*/

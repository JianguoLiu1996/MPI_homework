
static char help[] = "Tests MatDiagonalSet() on MatLoad() matrix \n\n";

#include <petscmat.h>

int main(int argc, char **args)
{
  Mat         A;
  Vec         x;
  PetscViewer fd;                       /* viewer */
  char        file[PETSC_MAX_PATH_LEN]; /* input file name */
  PetscReal   norm;
  PetscBool   flg;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &args, (char *)0, help));
  /* Determine file from which we read the matrix A */
  PetscCall(PetscOptionsGetString(NULL, NULL, "-f", file, sizeof(file), &flg));
  PetscCheck(flg, PETSC_COMM_WORLD, PETSC_ERR_USER, "Must indicate binary file with the -f option");

  /* Load matrix A */
  PetscCall(PetscViewerBinaryOpen(PETSC_COMM_WORLD, file, FILE_MODE_READ, &fd));
  PetscCall(MatCreate(PETSC_COMM_WORLD, &A));
  PetscCall(MatLoad(A, fd));
  PetscCall(PetscViewerDestroy(&fd));
  PetscCall(MatCreateVecs(A, &x, NULL));
  PetscCall(MatGetDiagonal(A, x));
  PetscCall(VecScale(x, -1.0));
  PetscCall(MatDiagonalSet(A, x, ADD_VALUES));
  PetscCall(MatGetDiagonal(A, x));
  PetscCall(VecNorm(x, NORM_2, &norm));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Norm %g\n", (double)norm));

  /* Free data structures */
  PetscCall(MatDestroy(&A));
  PetscCall(VecDestroy(&x));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   test:
      nsize: 4
      requires: datafilespath !complex double !defined(PETSC_USE_64BIT_INDICES)
      args: -f ${wPETSC_DIR}/share/petsc/datafiles/matrices/ns-real-int32-float64 -malloc_dump

TEST*/

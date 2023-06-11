#include <petscksp.h>

int main(int argc, char **argv)
{
    PetscErrorCode ierr;
    PetscMPIInt    rank;
    Vec            x, b;
    Mat            A;
    KSP            ksp;
    PetscInt       n = 5, i, start, end;
    PetscScalar    value;

    ierr = PetscInitialize(&argc, &argv, NULL, NULL);
    if (ierr) return ierr;

    MPI_Comm_rank(PETSC_COMM_WORLD, &rank);

    ierr = VecCreate(PETSC_COMM_WORLD, &x);
    ierr = VecSetSizes(x, PETSC_DECIDE, n);
    ierr = VecSetFromOptions(x);
    ierr = VecDuplicate(x, &b);

    ierr = MatCreate(PETSC_COMM_WORLD, &A);
    ierr = MatSetSizes(A, PETSC_DECIDE, PETSC_DECIDE, n, n);
    ierr = MatSetFromOptions(A);
    ierr = MatSetUp(A);

    ierr = MatGetOwnershipRange(A, &start, &end);
    for (i = start; i < end; i++) {
        value = 2.0;
        ierr = MatSetValues(A, 1, &i, 1, &i, &value, INSERT_VALUES);
    }
    if (rank == 0) {
        value = -1.0;
        i = 0;
        ierr = MatSetValues(A, 1, &i, 1, &i, &value, INSERT_VALUES);
    }
    if (rank == n - 1) {
        value = -1.0;
        i = n - 1;
        ierr = MatSetValues(A, 1, &i, 1, &i, &value, INSERT_VALUES);
    }

    ierr = MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY);
    ierr = MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY);

    ierr = VecSet(b, 1.0);
    ierr = VecAssemblyBegin(b);
    ierr = VecAssemblyEnd(b);

    ierr = KSPCreate(PETSC_COMM_WORLD, &ksp);
    ierr = KSPSetOperators(ksp, A, A);
    ierr = KSPSetFromOptions(ksp);
    ierr = KSPSolve(ksp, b, x);

    ierr = VecView(x, PETSC_VIEWER_STDOUT_WORLD);

    ierr = KSPDestroy(&ksp);
    ierr = VecDestroy(&x);
    ierr = VecDestroy(&b);
    ierr = MatDestroy(&A);
    ierr = PetscFinalize();

    return ierr;
}


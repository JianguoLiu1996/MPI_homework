static char help[] = "Tests 1D cell-based discretization tools.\n\n";

#include <petscdt.h>
#include <petscviewer.h>

int main(int argc, char **argv)
{
  PetscInt  i, j, degrees[1000], ndegrees, nsrc_points, ntarget_points;
  PetscReal src_points[1000], target_points[1000], *R;
  PetscBool flg;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscOptionsBegin(PETSC_COMM_WORLD, NULL, "Discretization tools test options", NULL);
  {
    ndegrees   = 1000;
    degrees[0] = 1;
    degrees[1] = 2;
    degrees[2] = 3;
    PetscCall(PetscOptionsIntArray("-degrees", "list of max degrees to evaluate", "", degrees, &ndegrees, &flg));
    if (!flg) ndegrees = 3;

    nsrc_points   = 1000;
    src_points[0] = -1.;
    src_points[1] = 0.;
    src_points[2] = 1.;
    PetscCall(PetscOptionsRealArray("-src_points", "list of points defining intervals on which to integrate", "", src_points, &nsrc_points, &flg));
    if (!flg) nsrc_points = 3;

    ntarget_points   = 1000;
    target_points[0] = -1.;
    target_points[1] = 0.;
    target_points[2] = 1.;
    PetscCall(PetscOptionsRealArray("-target_points", "list of points defining intervals on which to integrate", "", target_points, &ntarget_points, &flg));
    if (!flg) ntarget_points = 3;
  }
  PetscOptionsEnd();

  PetscCall(PetscMalloc1((nsrc_points - 1) * (ntarget_points - 1), &R));
  for (i = 0; i < ndegrees; i++) {
    PetscCall(PetscDTReconstructPoly(degrees[i], nsrc_points - 1, src_points, ntarget_points - 1, target_points, R));
    for (j = 0; j < (ntarget_points - 1) * (nsrc_points - 1); j++) { /* Truncate to zero for nicer output */
      if (PetscAbs(R[j]) < 10 * PETSC_MACHINE_EPSILON) R[j] = 0;
    }
    for (j = 0; j < ntarget_points - 1; j++) {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Degree %" PetscInt_FMT " target interval (%g,%g)\n", degrees[i], (double)target_points[j], (double)target_points[j + 1]));
      PetscCall(PetscRealView(nsrc_points - 1, R + j * (nsrc_points - 1), PETSC_VIEWER_STDOUT_WORLD));
    }
  }
  PetscCall(PetscFree(R));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST
  test:
    suffix: 1
    args: -degrees 1,2,3 -target_points -0.3,0,.2 -src_points -1,-.3,0,.2,1
TEST*/

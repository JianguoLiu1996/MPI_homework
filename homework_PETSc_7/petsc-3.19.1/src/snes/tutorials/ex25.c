static const char help[] = "Minimum surface problem in 2D.\n\
Uses 2-dimensional distributed arrays.\n\
\n\
  Solves the linear systems via multilevel methods \n\
\n\n";

/*

    This example models the partial differential equation

         - Div((1 + ||GRAD T||^2)^(1/2) (GRAD T)) = 0.

    in the unit square, which is uniformly discretized in each of x and
    y in this simple encoding.  The degrees of freedom are vertex centered

    A finite difference approximation with the usual 5-point stencil
    is used to discretize the boundary value problem to obtain a
    nonlinear system of equations.

*/

#include <petscsnes.h>
#include <petscdm.h>
#include <petscdmda.h>

extern PetscErrorCode FormFunctionLocal(DMDALocalInfo *, PetscScalar **, PetscScalar **, void *);

int main(int argc, char **argv)
{
  SNES      snes;
  PetscInt  its, lits;
  PetscReal litspit;
  DM        da;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  /*
      Set the DMDA (grid structure) for the grids.
  */
  PetscCall(DMDACreate2d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, DMDA_STENCIL_STAR, 5, 5, PETSC_DECIDE, PETSC_DECIDE, 1, 1, 0, 0, &da));
  PetscCall(DMSetFromOptions(da));
  PetscCall(DMSetUp(da));
  PetscCall(DMDASNESSetFunctionLocal(da, INSERT_VALUES, (PetscErrorCode(*)(DMDALocalInfo *, void *, void *, void *))FormFunctionLocal, NULL));
  PetscCall(SNESCreate(PETSC_COMM_WORLD, &snes));
  PetscCall(SNESSetDM(snes, da));
  PetscCall(DMDestroy(&da));

  PetscCall(SNESSetFromOptions(snes));

  PetscCall(SNESSolve(snes, 0, 0));
  PetscCall(SNESGetIterationNumber(snes, &its));
  PetscCall(SNESGetLinearSolveIterations(snes, &lits));
  litspit = ((PetscReal)lits) / ((PetscReal)its);
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Number of SNES iterations = %" PetscInt_FMT "\n", its));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Number of Linear iterations = %" PetscInt_FMT "\n", lits));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Average Linear its / SNES = %e\n", (double)litspit));

  PetscCall(SNESDestroy(&snes));
  PetscCall(PetscFinalize());
  return 0;
}

PetscErrorCode FormFunctionLocal(DMDALocalInfo *info, PetscScalar **t, PetscScalar **f, void *ptr)
{
  PetscInt    i, j;
  PetscScalar hx, hy;
  PetscScalar gradup, graddown, gradleft, gradright, gradx, grady;
  PetscScalar coeffup, coeffdown, coeffleft, coeffright;

  PetscFunctionBeginUser;
  hx = 1.0 / (PetscReal)(info->mx - 1);
  hy = 1.0 / (PetscReal)(info->my - 1);

  /* Evaluate function */
  for (j = info->ys; j < info->ys + info->ym; j++) {
    for (i = info->xs; i < info->xs + info->xm; i++) {
      if (i == 0 || i == info->mx - 1 || j == 0 || j == info->my - 1) {
        f[j][i] = t[j][i] - (1.0 - (2.0 * hx * (PetscReal)i - 1.0) * (2.0 * hx * (PetscReal)i - 1.0));
      } else {
        gradup    = (t[j + 1][i] - t[j][i]) / hy;
        graddown  = (t[j][i] - t[j - 1][i]) / hy;
        gradright = (t[j][i + 1] - t[j][i]) / hx;
        gradleft  = (t[j][i] - t[j][i - 1]) / hx;

        gradx = .5 * (t[j][i + 1] - t[j][i - 1]) / hx;
        grady = .5 * (t[j + 1][i] - t[j - 1][i]) / hy;

        coeffup   = 1.0 / PetscSqrtScalar(1.0 + gradup * gradup + gradx * gradx);
        coeffdown = 1.0 / PetscSqrtScalar(1.0 + graddown * graddown + gradx * gradx);

        coeffleft  = 1.0 / PetscSqrtScalar(1.0 + gradleft * gradleft + grady * grady);
        coeffright = 1.0 / PetscSqrtScalar(1.0 + gradright * gradright + grady * grady);

        f[j][i] = (coeffup * gradup - coeffdown * graddown) * hx + (coeffright * gradright - coeffleft * gradleft) * hy;
      }
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*TEST

   test:
      args: -pc_type mg -da_refine 1 -ksp_type fgmres

   test:
      suffix: 2
      nsize: 2
      args: -pc_type mg -da_refine 1 -ksp_type fgmres

   test:
      suffix: 3
      nsize: 2
      args: -pc_type mg -da_refine 1 -ksp_type fgmres -snes_type newtontrdc -snes_trdc_use_cauchy false

   test:
      suffix: 4
      nsize: 2
      args: -pc_type mg -da_refine 1 -ksp_type fgmres -snes_type newtontrdc
      filter: sed -e "s/SNES iterations = 1[1-3]/SNES iterations = 13/g" |sed -e "s/Linear iterations = 2[8-9]/Linear iterations = 29/g" |sed -e "s/Linear iterations = 3[0-1]/Linear iterations = 29/g"

TEST*/

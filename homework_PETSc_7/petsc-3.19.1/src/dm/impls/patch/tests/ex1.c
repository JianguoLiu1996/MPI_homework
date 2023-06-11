static char help[] = "Make a 2D grid of patches and view them\n\n";

/*
Serial Test
Parallel Test where all zooms are serials
Parallel Test where zooms are parallel

Return DMPatch from Zoom
Override refine from DMPatch to split cells
 */
#include <petscdmpatch.h>

typedef struct {
  PetscInt   debug;     /* The debugging level */
  PetscInt   dim;       /* The spatial dimension */
  MatStencil patchSize; /* Size of patches */
  MatStencil gridSize;  /* Size of patch grid */
  MatStencil commSize;  /* Size of patch comm */
} AppCtx;

PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscInt patchSize, commSize, gridSize;

  PetscFunctionBegin;
  options->debug = 0;
  options->dim   = 2;
  patchSize      = 0;
  commSize       = 0;
  gridSize       = 1;

  PetscOptionsBegin(comm, "", "Patch Test Options", "DMPATCH");
  PetscCall(PetscOptionsBoundedInt("-debug", "The debugging level", "ex1.c", options->debug, &options->debug, NULL, 0));
  PetscCall(PetscOptionsRangeInt("-dim", "The spatial dimension", "ex1.c", options->dim, &options->dim, NULL, 1, 3));
  PetscCall(PetscOptionsBoundedInt("-patch_size", "The patch size in each dimension", "ex1.c", patchSize, &patchSize, NULL, 0));
  PetscCall(PetscOptionsBoundedInt("-comm_size", "The comm size in each dimension", "ex1.c", commSize, &commSize, NULL, 0));
  PetscCall(PetscOptionsBoundedInt("-grid_size", "The grid size in each dimension", "ex1.c", gridSize, &gridSize, NULL, 1));
  PetscOptionsEnd();

  options->patchSize.i = options->patchSize.j = options->patchSize.k = 1;
  options->commSize.i = options->commSize.j = options->commSize.k = 1;
  options->gridSize.i = options->gridSize.j = options->gridSize.k = 1;
  if (options->dim > 0) {
    options->patchSize.i = patchSize;
    options->commSize.i  = commSize;
    options->gridSize.i  = gridSize;
  }
  if (options->dim > 1) {
    options->patchSize.j = patchSize;
    options->commSize.j  = commSize;
    options->gridSize.j  = gridSize;
  }
  if (options->dim > 2) {
    options->patchSize.k = patchSize;
    options->commSize.k  = commSize;
    options->gridSize.k  = gridSize;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  DM     dm;
  AppCtx user; /* user-defined work context */

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  PetscCall(ProcessOptions(PETSC_COMM_WORLD, &user));
  PetscCall(DMPatchCreateGrid(PETSC_COMM_WORLD, user.dim, user.patchSize, user.commSize, user.gridSize, &dm));
  PetscCall(PetscObjectSetName((PetscObject)dm, "Patch Mesh"));
  PetscCall(DMSetFromOptions(dm));
  PetscCall(DMSetUp(dm));
  PetscCall(DMView(dm, PETSC_VIEWER_STDOUT_WORLD));
  PetscCall(DMPatchSolve(dm));
  PetscCall(DMDestroy(&dm));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   test:

TEST*/

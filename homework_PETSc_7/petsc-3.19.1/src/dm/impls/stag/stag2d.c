/* Functions specific to the 2-dimensional implementation of DMStag */
#include <petsc/private/dmstagimpl.h>

/*@C
  DMStagCreate2d - Create an object to manage data living on the elements, faces, and vertices of a parallelized regular 2D grid.

  Collective

  Input Parameters:
+ comm - MPI communicator
. bndx,bndy - boundary type: `DM_BOUNDARY_NONE`, `DM_BOUNDARY_PERIODIC`, or `DM_BOUNDARY_GHOSTED`
. M,N - global number of elements in x,y directions
. m,n - number of ranks in the x,y directions (may be `PETSC_DECIDE`)
. dof0 - number of degrees of freedom per vertex/0-cell
. dof1 - number of degrees of freedom per face/1-cell
. dof2 - number of degrees of freedom per element/2-cell
. stencilType - ghost/halo region type: `DMSTAG_STENCIL_NONE`, `DMSTAG_STENCIL_BOX`, or `DMSTAG_STENCIL_STAR`
. stencilWidth - width, in elements, of halo/ghost region
- lx,ly - arrays of local x,y element counts, of length equal to m,n, summing to M,N

  Output Parameter:
. dm - the new `DMSTAG` object

  Options Database Keys:
+ -dm_view - calls `DMViewFromOptions()` at the conclusion of `DMSetUp()`
. -stag_grid_x <nx> - number of elements in the x direction
. -stag_grid_y <ny> - number of elements in the y direction
. -stag_ranks_x <rx> - number of ranks in the x direction
. -stag_ranks_y <ry> - number of ranks in the y direction
. -stag_ghost_stencil_width - width of ghost region, in elements
. -stag_boundary_type_x <none,ghosted,periodic> - `DMBoundaryType` value
- -stag_boundary_type_y <none,ghosted,periodic> - `DMBoundaryType` value

  Level: beginner

  Notes:
  You must call `DMSetUp()` after this call, before using the `DM`.
  If you wish to use the options database (see the keys above) to change values in the `DMSTAG`, you must call
  `DMSetFromOptions()` after this function but before `DMSetUp()`.

.seealso: [](chapter_stag), `DMSTAG`, `DMStagCreate1d()`, `DMStagCreate3d()`, `DMDestroy()`, `DMView()`, `DMCreateGlobalVector()`, `DMCreateLocalVector()`, `DMLocalToGlobalBegin()`, `DMDACreate2d()`
@*/
PETSC_EXTERN PetscErrorCode DMStagCreate2d(MPI_Comm comm, DMBoundaryType bndx, DMBoundaryType bndy, PetscInt M, PetscInt N, PetscInt m, PetscInt n, PetscInt dof0, PetscInt dof1, PetscInt dof2, DMStagStencilType stencilType, PetscInt stencilWidth, const PetscInt lx[], const PetscInt ly[], DM *dm)
{
  PetscFunctionBegin;
  PetscCall(DMCreate(comm, dm));
  PetscCall(DMSetDimension(*dm, 2));
  PetscCall(DMStagInitialize(bndx, bndy, DM_BOUNDARY_NONE, M, N, 0, m, n, 0, dof0, dof1, dof2, 0, stencilType, stencilWidth, lx, ly, NULL, *dm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode DMStagRestrictSimple_2d(DM dmf, Vec xf_local, DM dmc, Vec xc_local)
{
  PetscScalar ***LA_xf, ***LA_xc;
  PetscInt       i, j, start[2], n[2], nextra[2], N[2];
  PetscInt       d, dof[3];
  PetscInt       slot_down_left_coarse, slot_down_left_fine;
  PetscInt       slot_element_fine, slot_element_coarse;
  PetscInt       slot_left_coarse, slot_down_coarse, slot_left_fine, slot_down_fine;

  PetscFunctionBegin;
  PetscCall(DMStagGetDOF(dmc, &dof[0], &dof[1], &dof[2], NULL));
  PetscCall(DMStagGetCorners(dmc, &start[0], &start[1], NULL, &n[0], &n[1], NULL, &nextra[0], &nextra[1], NULL));
  PetscCall(DMStagGetGlobalSizes(dmc, &N[0], &N[1], NULL));
  if (PetscDefined(USE_DEBUG)) {
    PetscInt dof_check[3], n_fine[2], start_fine[2];

    PetscCall(DMStagGetDOF(dmf, &dof_check[0], &dof_check[1], &dof_check[2], NULL));
    PetscCall(DMStagGetCorners(dmf, &start_fine[0], &start_fine[1], NULL, &n_fine[0], &n_fine[1], NULL, NULL, NULL, NULL));
    for (d = 0; d < 3; ++d) PetscCheck(dof_check[d] == dof[d], PetscObjectComm((PetscObject)dmf), PETSC_ERR_ARG_INCOMP, "Cannot transfer between DMStag objects with different dof on each stratum");
    for (d = 0; d < 2; ++d) PetscCheck(n_fine[d] == 2 * n[d], PetscObjectComm((PetscObject)dmf), PETSC_ERR_ARG_INCOMP, "Cannot transfer between DMStag objects unless there is a 2-1 coarsening");
    for (d = 0; d < 2; ++d) PetscCheck(start_fine[d] == 2 * start[d], PetscObjectComm((PetscObject)dmf), PETSC_ERR_ARG_INCOMP, "Cannot transfer between DMStag objects unless there is a 2-1 coarsening");
    {
      PetscInt size_local, entries_local;

      PetscCall(DMStagGetEntriesLocal(dmf, &entries_local));
      PetscCall(VecGetLocalSize(xf_local, &size_local));
      PetscCheck(entries_local == size_local, PETSC_COMM_WORLD, PETSC_ERR_ARG_INCOMP, "Fine vector must be a local vector of size %" PetscInt_FMT ", but a vector of size %" PetscInt_FMT " was supplied", entries_local, size_local);
    }
    {
      PetscInt size_local, entries_local;

      PetscCall(DMStagGetEntriesLocal(dmc, &entries_local));
      PetscCall(VecGetLocalSize(xc_local, &size_local));
      PetscCheck(entries_local == size_local, PETSC_COMM_WORLD, PETSC_ERR_ARG_INCOMP, "Coarse vector must be a local vector of size %" PetscInt_FMT ", but a vector of size %" PetscInt_FMT " was supplied", entries_local, size_local);
    }
  }
  PetscCall(VecZeroEntries(xc_local));
  PetscCall(DMStagVecGetArray(dmf, xf_local, &LA_xf));
  PetscCall(DMStagVecGetArray(dmc, xc_local, &LA_xc));
  PetscCall(DMStagGetLocationSlot(dmf, DMSTAG_DOWN_LEFT, 0, &slot_down_left_fine));
  PetscCall(DMStagGetLocationSlot(dmf, DMSTAG_LEFT, 0, &slot_left_fine));
  PetscCall(DMStagGetLocationSlot(dmf, DMSTAG_DOWN, 0, &slot_down_fine));
  PetscCall(DMStagGetLocationSlot(dmf, DMSTAG_ELEMENT, 0, &slot_element_fine));
  PetscCall(DMStagGetLocationSlot(dmc, DMSTAG_DOWN_LEFT, 0, &slot_down_left_coarse));
  PetscCall(DMStagGetLocationSlot(dmc, DMSTAG_LEFT, 0, &slot_left_coarse));
  PetscCall(DMStagGetLocationSlot(dmc, DMSTAG_DOWN, 0, &slot_down_coarse));
  PetscCall(DMStagGetLocationSlot(dmc, DMSTAG_ELEMENT, 0, &slot_element_coarse));
  for (j = start[1]; j < start[1] + n[1] + nextra[1]; j++) {
    for (i = start[0]; i < start[0] + n[0] + nextra[0]; i++) {
      for (d = 0; d < dof[0]; ++d) LA_xc[j][i][slot_down_left_coarse + d] = LA_xf[2 * j][2 * i][slot_down_left_fine + d];
      for (d = 0; d < dof[1]; ++d) {
        if (j < N[1]) LA_xc[j][i][slot_left_coarse + d] = 0.5 * (LA_xf[2 * j][2 * i][slot_left_fine + d] + LA_xf[2 * j + 1][2 * i][slot_left_fine + d]);
        if (i < N[0]) LA_xc[j][i][slot_down_coarse + d] = 0.5 * (LA_xf[2 * j][2 * i][slot_down_fine + d] + LA_xf[2 * j][2 * i + 1][slot_down_fine + d]);
      }
      for (d = 0; d < dof[2]; ++d) {
        if (i < N[0] && j < N[1]) {
          LA_xc[j][i][slot_element_coarse + d] = 0.25 * (LA_xf[2 * j][2 * i][slot_element_fine + d] + LA_xf[2 * j + 1][2 * i][slot_element_fine + d] + LA_xf[2 * j][2 * i + 1][slot_element_fine + d] + LA_xf[2 * j + 1][2 * i + 1][slot_element_fine + d]);
        }
      }
    }
  }
  PetscCall(DMStagVecRestoreArray(dmf, xf_local, &LA_xf));
  PetscCall(DMStagVecRestoreArray(dmc, xc_local, &LA_xc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode DMStagSetUniformCoordinatesExplicit_2d(DM dm, PetscReal xmin, PetscReal xmax, PetscReal ymin, PetscReal ymax)
{
  DM_Stag       *stagCoord;
  DM             dmCoord;
  Vec            coordLocal;
  PetscReal      h[2], min[2];
  PetscScalar ***arr;
  PetscInt       ind[2], start_ghost[2], n_ghost[2], s, c;
  PetscInt       idownleft, idown, ileft, ielement;

  PetscFunctionBegin;
  PetscCall(DMGetCoordinateDM(dm, &dmCoord));
  stagCoord = (DM_Stag *)dmCoord->data;
  for (s = 0; s < 3; ++s) {
    PetscCheck(stagCoord->dof[s] == 0 || stagCoord->dof[s] == 2, PetscObjectComm((PetscObject)dm), PETSC_ERR_PLIB, "Coordinate DM in 2 dimensions must have 0 or 2 dof on each stratum, but stratum %" PetscInt_FMT " has %" PetscInt_FMT " dof", s,
               stagCoord->dof[s]);
  }
  PetscCall(DMCreateLocalVector(dmCoord, &coordLocal));

  PetscCall(DMStagVecGetArray(dmCoord, coordLocal, &arr));
  if (stagCoord->dof[0]) PetscCall(DMStagGetLocationSlot(dmCoord, DMSTAG_DOWN_LEFT, 0, &idownleft));
  if (stagCoord->dof[1]) {
    PetscCall(DMStagGetLocationSlot(dmCoord, DMSTAG_DOWN, 0, &idown));
    PetscCall(DMStagGetLocationSlot(dmCoord, DMSTAG_LEFT, 0, &ileft));
  }
  if (stagCoord->dof[2]) PetscCall(DMStagGetLocationSlot(dmCoord, DMSTAG_ELEMENT, 0, &ielement));
  PetscCall(DMStagGetGhostCorners(dmCoord, &start_ghost[0], &start_ghost[1], NULL, &n_ghost[0], &n_ghost[1], NULL));

  min[0] = xmin;
  min[1] = ymin;
  h[0]   = (xmax - xmin) / stagCoord->N[0];
  h[1]   = (ymax - ymin) / stagCoord->N[1];

  for (ind[1] = start_ghost[1]; ind[1] < start_ghost[1] + n_ghost[1]; ++ind[1]) {
    for (ind[0] = start_ghost[0]; ind[0] < start_ghost[0] + n_ghost[0]; ++ind[0]) {
      if (stagCoord->dof[0]) {
        const PetscReal offs[2] = {0.0, 0.0};
        for (c = 0; c < 2; ++c) arr[ind[1]][ind[0]][idownleft + c] = min[c] + ((PetscReal)ind[c] + offs[c]) * h[c];
      }
      if (stagCoord->dof[1]) {
        const PetscReal offs[2] = {0.5, 0.0};
        for (c = 0; c < 2; ++c) arr[ind[1]][ind[0]][idown + c] = min[c] + ((PetscReal)ind[c] + offs[c]) * h[c];
      }
      if (stagCoord->dof[1]) {
        const PetscReal offs[2] = {0.0, 0.5};
        for (c = 0; c < 2; ++c) arr[ind[1]][ind[0]][ileft + c] = min[c] + ((PetscReal)ind[c] + offs[c]) * h[c];
      }
      if (stagCoord->dof[2]) {
        const PetscReal offs[2] = {0.5, 0.5};
        for (c = 0; c < 2; ++c) arr[ind[1]][ind[0]][ielement + c] = min[c] + ((PetscReal)ind[c] + offs[c]) * h[c];
      }
    }
  }
  PetscCall(DMStagVecRestoreArray(dmCoord, coordLocal, &arr));
  PetscCall(DMSetCoordinatesLocal(dm, coordLocal));
  PetscCall(VecDestroy(&coordLocal));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Helper functions used in DMSetUp_Stag() */
static PetscErrorCode DMStagSetUpBuildRankGrid_2d(DM);
static PetscErrorCode DMStagSetUpBuildNeighbors_2d(DM);
static PetscErrorCode DMStagSetUpBuildGlobalOffsets_2d(DM, PetscInt **);
static PetscErrorCode DMStagComputeLocationOffsets_2d(DM);

PETSC_INTERN PetscErrorCode DMSetUp_Stag_2d(DM dm)
{
  DM_Stag *const stag = (DM_Stag *)dm->data;
  PetscMPIInt    size, rank;
  PetscInt       i, j, d, entriesPerElementRowGhost, entriesPerCorner, entriesPerFace, entriesPerElementRow;
  MPI_Comm       comm;
  PetscInt      *globalOffsets;
  PetscBool      star, dummyStart[2], dummyEnd[2];
  const PetscInt dim = 2;

  PetscFunctionBegin;
  PetscCall(PetscObjectGetComm((PetscObject)dm, &comm));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));

  /* Rank grid sizes (populates stag->nRanks) */
  PetscCall(DMStagSetUpBuildRankGrid_2d(dm));

  /* Determine location of rank in grid (these get extra boundary points on the last element)
     Order is x-fast, as usual */
  stag->rank[0] = rank % stag->nRanks[0];
  stag->rank[1] = rank / stag->nRanks[0];
  for (i = 0; i < dim; ++i) {
    stag->firstRank[i] = PetscNot(stag->rank[i]);
    stag->lastRank[i]  = (PetscBool)(stag->rank[i] == stag->nRanks[i] - 1);
  }

  /* Determine Locally owned region

   Divide equally, giving lower ranks in each dimension and extra element if needbe.

   Note that this uses O(P) storage. If this ever becomes an issue, this could
   be refactored to not keep this data around.  */
  for (i = 0; i < dim; ++i) {
    if (!stag->l[i]) {
      const PetscInt Ni = stag->N[i], nRanksi = stag->nRanks[i];
      PetscCall(PetscMalloc1(stag->nRanks[i], &stag->l[i]));
      for (j = 0; j < stag->nRanks[i]; ++j) stag->l[i][j] = Ni / nRanksi + ((Ni % nRanksi) > j);
    }
  }

  /* Retrieve local size in stag->n */
  for (i = 0; i < dim; ++i) stag->n[i] = stag->l[i][stag->rank[i]];
  if (PetscDefined(USE_DEBUG)) {
    for (i = 0; i < dim; ++i) {
      PetscInt Ncheck, j;
      Ncheck = 0;
      for (j = 0; j < stag->nRanks[i]; ++j) Ncheck += stag->l[i][j];
      PetscCheck(Ncheck == stag->N[i], PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "Local sizes in dimension %" PetscInt_FMT " don't add up. %" PetscInt_FMT " != %" PetscInt_FMT, i, Ncheck, stag->N[i]);
    }
  }

  /* Compute starting elements */
  for (i = 0; i < dim; ++i) {
    stag->start[i] = 0;
    for (j = 0; j < stag->rank[i]; ++j) stag->start[i] += stag->l[i][j];
  }

  /* Determine ranks of neighbors, using DMDA's convention

     n6 n7 n8
     n3    n5
     n0 n1 n2                                               */
  PetscCall(DMStagSetUpBuildNeighbors_2d(dm));

  /* Determine whether the ghost region includes dummies or not. This is currently
       equivalent to having a non-periodic boundary. If not, then
       ghostOffset{Start,End}[d] elements correspond to elements on the neighbor.
       If true, then
       - at the start, there are ghostOffsetStart[d] ghost elements
       - at the end, there is a layer of extra "physical" points inside a layer of
         ghostOffsetEnd[d] ghost elements
       Note that this computation should be updated if any boundary types besides
       NONE, GHOSTED, and PERIODIC are supported.  */
  for (d = 0; d < 2; ++d) dummyStart[d] = (PetscBool)(stag->firstRank[d] && stag->boundaryType[d] != DM_BOUNDARY_PERIODIC);
  for (d = 0; d < 2; ++d) dummyEnd[d] = (PetscBool)(stag->lastRank[d] && stag->boundaryType[d] != DM_BOUNDARY_PERIODIC);

  /* Define useful sizes */
  stag->entriesPerElement = stag->dof[0] + 2 * stag->dof[1] + stag->dof[2];
  entriesPerFace          = stag->dof[0] + stag->dof[1];
  entriesPerCorner        = stag->dof[0];
  entriesPerElementRow    = stag->n[0] * stag->entriesPerElement + (dummyEnd[0] ? entriesPerFace : 0);
  stag->entries           = stag->n[1] * entriesPerElementRow + (dummyEnd[1] ? stag->n[0] * entriesPerFace : 0) + (dummyEnd[0] && dummyEnd[1] ? entriesPerCorner : 0);

  /* Compute offsets for each rank into global vectors
     This again requires O(P) storage, which could be replaced with some global
     communication.  */
  PetscCall(DMStagSetUpBuildGlobalOffsets_2d(dm, &globalOffsets));

  for (d = 0; d < dim; ++d)
    PetscCheck(stag->boundaryType[d] == DM_BOUNDARY_NONE || stag->boundaryType[d] == DM_BOUNDARY_PERIODIC || stag->boundaryType[d] == DM_BOUNDARY_GHOSTED, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unsupported boundary type");

  /* Define ghosted/local sizes */
  if (stag->stencilType != DMSTAG_STENCIL_NONE && (stag->n[0] < stag->stencilWidth || stag->n[1] < stag->stencilWidth)) {
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "DMStag 2d setup does not support local sizes (%" PetscInt_FMT " x %" PetscInt_FMT ") smaller than the elementwise stencil width (%" PetscInt_FMT ")", stag->n[0], stag->n[1], stag->stencilWidth);
  }
  for (d = 0; d < dim; ++d) {
    switch (stag->boundaryType[d]) {
    case DM_BOUNDARY_NONE:
      /* Note: for a elements-only DMStag, the extra elements on the faces aren't necessary but we include them anyway */
      switch (stag->stencilType) {
      case DMSTAG_STENCIL_NONE: /* only the extra one on the right/top faces */
        stag->nGhost[d]     = stag->n[d];
        stag->startGhost[d] = stag->start[d];
        if (stag->lastRank[d]) stag->nGhost[d] += 1;
        break;
      case DMSTAG_STENCIL_STAR: /* allocate the corners but don't use them */
      case DMSTAG_STENCIL_BOX:
        stag->nGhost[d]     = stag->n[d];
        stag->startGhost[d] = stag->start[d];
        if (!stag->firstRank[d]) {
          stag->nGhost[d] += stag->stencilWidth; /* add interior ghost elements */
          stag->startGhost[d] -= stag->stencilWidth;
        }
        if (!stag->lastRank[d]) {
          stag->nGhost[d] += stag->stencilWidth; /* add interior ghost elements */
        } else {
          stag->nGhost[d] += 1; /* one element on the boundary to complete blocking */
        }
        break;
      default:
        SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unrecognized ghost stencil type %d", stag->stencilType);
      }
      break;
    case DM_BOUNDARY_GHOSTED:
      switch (stag->stencilType) {
      case DMSTAG_STENCIL_NONE:
        stag->startGhost[d] = stag->start[d];
        stag->nGhost[d]     = stag->n[d] + (stag->lastRank[d] ? 1 : 0);
        break;
      case DMSTAG_STENCIL_STAR:
      case DMSTAG_STENCIL_BOX:
        stag->startGhost[d] = stag->start[d] - stag->stencilWidth; /* This value may be negative */
        stag->nGhost[d]     = stag->n[d] + 2 * stag->stencilWidth + (stag->lastRank[d] && stag->stencilWidth == 0 ? 1 : 0);
        break;
      default:
        SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unrecognized ghost stencil type %d", stag->stencilType);
      }
      break;
    case DM_BOUNDARY_PERIODIC:
      switch (stag->stencilType) {
      case DMSTAG_STENCIL_NONE: /* only the extra one on the right/top faces */
        stag->nGhost[d]     = stag->n[d];
        stag->startGhost[d] = stag->start[d];
        break;
      case DMSTAG_STENCIL_STAR:
      case DMSTAG_STENCIL_BOX:
        stag->nGhost[d]     = stag->n[d] + 2 * stag->stencilWidth;
        stag->startGhost[d] = stag->start[d] - stag->stencilWidth;
        break;
      default:
        SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unrecognized ghost stencil type %d", stag->stencilType);
      }
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unsupported boundary type in dimension %" PetscInt_FMT, d);
    }
  }
  stag->entriesGhost        = stag->nGhost[0] * stag->nGhost[1] * stag->entriesPerElement;
  entriesPerElementRowGhost = stag->nGhost[0] * stag->entriesPerElement;

  /* Create global-->local VecScatter and local->global ISLocalToGlobalMapping

     We iterate over all local points twice. First, we iterate over each neighbor, populating
     1. idxLocal[] : the subset of points, in local numbering ("S" from 0 on all points including ghosts), which correspond to global points. That is, the set of all non-dummy points in the ghosted representation
     2. idxGlobal[]: the corresponding global points, in global numbering (Nested "S"s - ranks then non-ghost points in each rank)

     Next, we iterate over all points in the local ordering, populating
     3. idxGlobalAll[] : entry i is the global point corresponding to local point i, or -1 if local point i is a dummy.

     Note further here that the local/ghosted vectors:
     - Are always an integral number of elements-worth of points, in all directions.
     - Contain three flavors of points:
     1. Points which "live here" in the global representation
     2. Ghost points which correspond to points on other ranks in the global representation
     3. Ghost points, which we call "dummy points," which do not correspond to any point in the global representation

     Dummy ghost points arise in at least three ways:
     1. As padding for the right, top, and front physical boundaries, to complete partial elements
     2. As unused space in the "corners" on interior ranks when using a star stencil
     3. As additional work space on all physical boundaries, when DM_BOUNDARY_GHOSTED is used

     Note that, because of the boundary dummies,
     with a stencil width of zero, on 1 rank, local and global vectors
     are still different!

     We assume that the size on each rank is greater than or equal to the
     stencil width.
     */

  /* Check stencil type */
  PetscCheck(stag->stencilType == DMSTAG_STENCIL_NONE || stag->stencilType == DMSTAG_STENCIL_BOX || stag->stencilType == DMSTAG_STENCIL_STAR, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Unsupported stencil type %s", DMStagStencilTypes[stag->stencilType]);
  star = (PetscBool)(stag->stencilType == DMSTAG_STENCIL_STAR || stag->stencilType == DMSTAG_STENCIL_NONE);

  {
    PetscInt *idxLocal, *idxGlobal, *idxGlobalAll;
    PetscInt  count, countAll, entriesToTransferTotal, i, j, d, ghostOffsetStart[2], ghostOffsetEnd[2];
    IS        isLocal, isGlobal;
    PetscInt  jghost, ighost;
    PetscInt  nNeighbors[9][2];
    PetscBool nextToDummyEnd[2];

    /* Compute numbers of elements on each neighbor */
    for (i = 0; i < 9; ++i) {
      const PetscInt neighborRank = stag->neighbors[i];
      if (neighborRank >= 0) { /* note we copy the values for our own rank (neighbor 4) */
        nNeighbors[i][0] = stag->l[0][neighborRank % stag->nRanks[0]];
        nNeighbors[i][1] = stag->l[1][neighborRank / stag->nRanks[0]];
      } /* else leave uninitialized - error if accessed */
    }

    /* These offsets should always be non-negative, and describe how many
       ghost elements exist at each boundary. These are not always equal to the stencil width,
       because we may have different numbers of ghost elements at the boundaries. In particular,
       we always have at least one ghost (dummy) element at the right/top/front. */
    for (d = 0; d < 2; ++d) ghostOffsetStart[d] = stag->start[d] - stag->startGhost[d];
    for (d = 0; d < 2; ++d) ghostOffsetEnd[d] = stag->startGhost[d] + stag->nGhost[d] - (stag->start[d] + stag->n[d]);

    /* Compute whether the next rank has an extra point (only used in x direction) */
    for (d = 0; d < 2; ++d) nextToDummyEnd[d] = (PetscBool)(stag->boundaryType[d] != DM_BOUNDARY_PERIODIC && stag->rank[d] == stag->nRanks[d] - 2);

    /* Compute the number of local entries which correspond to any global entry */
    {
      PetscInt nNonDummyGhost[2];
      for (d = 0; d < 2; ++d) nNonDummyGhost[d] = stag->nGhost[d] - (dummyStart[d] ? ghostOffsetStart[d] : 0) - (dummyEnd[d] ? ghostOffsetEnd[d] : 0);
      if (star) {
        entriesToTransferTotal = (nNonDummyGhost[0] * stag->n[1] + stag->n[0] * nNonDummyGhost[1] - stag->n[0] * stag->n[1]) * stag->entriesPerElement + (dummyEnd[0] ? nNonDummyGhost[1] * entriesPerFace : 0) + (dummyEnd[1] ? nNonDummyGhost[0] * entriesPerFace : 0) + (dummyEnd[0] && dummyEnd[1] ? entriesPerCorner : 0);
      } else {
        entriesToTransferTotal = nNonDummyGhost[0] * nNonDummyGhost[1] * stag->entriesPerElement + (dummyEnd[0] ? nNonDummyGhost[1] * entriesPerFace : 0) + (dummyEnd[1] ? nNonDummyGhost[0] * entriesPerFace : 0) + (dummyEnd[0] && dummyEnd[1] ? entriesPerCorner : 0);
      }
    }

    /* Allocate arrays to populate */
    PetscCall(PetscMalloc1(entriesToTransferTotal, &idxLocal));
    PetscCall(PetscMalloc1(entriesToTransferTotal, &idxGlobal));

    /* Counts into idxLocal/idxGlobal */
    count = 0;

    /* Here and below, we work with (i,j) describing element numbers within a neighboring rank's global ordering,
       to be offset by that rank's global offset,
       and (ighost,jghost) referring to element numbers within this ranks local (ghosted) ordering */

    /* Neighbor 0 (down left) */
    if (!star && !dummyStart[0] && !dummyStart[1]) {
      const PetscInt        neighbor                     = 0;
      const PetscInt        globalOffset                 = globalOffsets[stag->neighbors[neighbor]];
      const PetscInt *const nNeighbor                    = nNeighbors[neighbor];
      const PetscInt        entriesPerElementRowNeighbor = stag->entriesPerElement * nNeighbor[0];
      for (jghost = 0; jghost < ghostOffsetStart[1]; ++jghost) {
        const PetscInt j = nNeighbor[1] - ghostOffsetStart[1] + jghost;
        for (ighost = 0; ighost < ghostOffsetStart[0]; ++ighost) {
          const PetscInt i = nNeighbor[0] - ghostOffsetStart[0] + ighost;
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
    }

    /* Neighbor 1 (down) */
    if (!dummyStart[1]) {
      /* We may be a ghosted boundary in x, in which case the neighbor is also */
      const PetscInt        neighbor                     = 1;
      const PetscInt        globalOffset                 = globalOffsets[stag->neighbors[neighbor]];
      const PetscInt *const nNeighbor                    = nNeighbors[neighbor];
      const PetscInt        entriesPerElementRowNeighbor = entriesPerElementRow; /* same as here */
      for (jghost = 0; jghost < ghostOffsetStart[1]; ++jghost) {
        const PetscInt j = nNeighbor[1] - ghostOffsetStart[1] + jghost;
        for (ighost = ghostOffsetStart[0]; ighost < stag->nGhost[0] - ghostOffsetEnd[0]; ++ighost) {
          const PetscInt i = ighost - ghostOffsetStart[0];
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
        if (dummyEnd[0]) {
          const PetscInt ighost = stag->nGhost[0] - ghostOffsetEnd[0];
          const PetscInt i      = stag->n[0];
          for (d = 0; d < stag->dof[0]; ++d, ++count) { /* Vertex */
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
          for (d = 0; d < stag->dof[1]; ++d, ++count) { /* Face */
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + stag->dof[0] + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + stag->dof[0] + stag->dof[1] + d;
          }
        }
      }
    }

    /* Neighbor 2 (down right) */
    if (!star && !dummyEnd[0] && !dummyStart[1]) {
      const PetscInt        neighbor                     = 2;
      const PetscInt        globalOffset                 = globalOffsets[stag->neighbors[neighbor]];
      const PetscInt *const nNeighbor                    = nNeighbors[neighbor];
      const PetscInt        entriesPerElementRowNeighbor = nNeighbor[0] * stag->entriesPerElement + (nextToDummyEnd[0] ? entriesPerFace : 0);
      for (jghost = 0; jghost < ghostOffsetStart[1]; ++jghost) {
        const PetscInt j = nNeighbor[1] - ghostOffsetStart[1] + jghost;
        for (i = 0; i < ghostOffsetEnd[0]; ++i) {
          const PetscInt ighost = stag->nGhost[0] - ghostOffsetEnd[0] + i;
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
    }

    /* Neighbor 3 (left) */
    if (!dummyStart[0]) {
      /* Our neighbor is never a ghosted boundary in x, but we may be
         Here, we may be a ghosted boundary in y and thus so will our neighbor be */
      const PetscInt        neighbor                     = 3;
      const PetscInt        globalOffset                 = globalOffsets[stag->neighbors[neighbor]];
      const PetscInt *const nNeighbor                    = nNeighbors[neighbor];
      const PetscInt        entriesPerElementRowNeighbor = nNeighbor[0] * stag->entriesPerElement;
      for (jghost = ghostOffsetStart[1]; jghost < stag->nGhost[1] - ghostOffsetEnd[1]; ++jghost) {
        const PetscInt j = jghost - ghostOffsetStart[1];
        for (ighost = 0; ighost < ghostOffsetStart[0]; ++ighost) {
          const PetscInt i = nNeighbor[0] - ghostOffsetStart[0] + ighost;
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
      if (dummyEnd[1]) {
        const PetscInt jghost = stag->nGhost[1] - ghostOffsetEnd[1];
        const PetscInt j      = stag->n[1];
        for (ighost = 0; ighost < ghostOffsetStart[0]; ++ighost) {
          const PetscInt i = nNeighbor[0] - ghostOffsetStart[0] + ighost;
          for (d = 0; d < entriesPerFace; ++d, ++count) {                                                /* only vertices and horizontal face (which are the first dof) */
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * entriesPerFace + d; /* i moves by face here */
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
    }

    /* Interior/Resident-here-in-global elements ("Neighbor 4" - same rank)
       *including* entries from boundary dummy elements */
    {
      const PetscInt neighbor     = 4;
      const PetscInt globalOffset = globalOffsets[stag->neighbors[neighbor]];
      for (j = 0; j < stag->n[1]; ++j) {
        const PetscInt jghost = j + ghostOffsetStart[1];
        for (i = 0; i < stag->n[0]; ++i) {
          const PetscInt ighost = i + ghostOffsetStart[0];
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
        if (dummyEnd[0]) {
          const PetscInt ighost = i + ghostOffsetStart[0];
          i                     = stag->n[0];
          for (d = 0; d < stag->dof[0]; ++d, ++count) { /* vertex first */
            idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
          for (d = 0; d < stag->dof[1]; ++d, ++count) { /* then left edge (skipping bottom face) */
            idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * stag->entriesPerElement + stag->dof[0] + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + stag->dof[0] + stag->dof[1] + d;
          }
        }
      }
      if (dummyEnd[1]) {
        const PetscInt jghost = j + ghostOffsetStart[1];
        j                     = stag->n[1];
        for (i = 0; i < stag->n[0]; ++i) {
          const PetscInt ighost = i + ghostOffsetStart[0];
          for (d = 0; d < entriesPerFace; ++d, ++count) {                                        /* vertex and bottom face (which are the first entries) */
            idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * entriesPerFace + d; /* note i increment by entriesPerFace */
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
        if (dummyEnd[0]) {
          const PetscInt ighost = i + ghostOffsetStart[0];
          i                     = stag->n[0];
          for (d = 0; d < entriesPerCorner; ++d, ++count) {                                      /* vertex only */
            idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * entriesPerFace + d; /* note i increment by entriesPerFace */
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
    }

    /* Neighbor 5 (right) */
    if (!dummyEnd[0]) {
      /* We can never be right boundary, but we may be a top boundary, along with the right neighbor */
      const PetscInt        neighbor                     = 5;
      const PetscInt        globalOffset                 = globalOffsets[stag->neighbors[neighbor]];
      const PetscInt *const nNeighbor                    = nNeighbors[neighbor];
      const PetscInt        entriesPerElementRowNeighbor = nNeighbor[0] * stag->entriesPerElement + (nextToDummyEnd[0] ? entriesPerFace : 0);
      for (jghost = ghostOffsetStart[1]; jghost < stag->nGhost[1] - ghostOffsetEnd[1]; ++jghost) {
        const PetscInt j = jghost - ghostOffsetStart[1];
        for (i = 0; i < ghostOffsetEnd[0]; ++i) {
          const PetscInt ighost = stag->nGhost[0] - ghostOffsetEnd[0] + i;
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
      if (dummyEnd[1]) {
        const PetscInt jghost = stag->nGhost[1] - ghostOffsetEnd[1];
        const PetscInt j      = nNeighbor[1];
        for (i = 0; i < ghostOffsetEnd[0]; ++i) {
          const PetscInt ighost = stag->nGhost[0] - ghostOffsetEnd[0] + i;
          for (d = 0; d < entriesPerFace; ++d, ++count) {                                                /* only vertices and horizontal face (which are the first dof) */
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * entriesPerFace + d; /* Note i increment by entriesPerFace */
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
    }

    /* Neighbor 6 (up left) */
    if (!star && !dummyStart[0] && !dummyEnd[1]) {
      /* We can never be a top boundary, but our neighbor may be
       We may be a right boundary, but our neighbor cannot be */
      const PetscInt        neighbor                     = 6;
      const PetscInt        globalOffset                 = globalOffsets[stag->neighbors[neighbor]];
      const PetscInt *const nNeighbor                    = nNeighbors[neighbor];
      const PetscInt        entriesPerElementRowNeighbor = nNeighbor[0] * stag->entriesPerElement;
      for (j = 0; j < ghostOffsetEnd[1]; ++j) {
        const PetscInt jghost = stag->nGhost[1] - ghostOffsetEnd[1] + j;
        for (ighost = 0; ighost < ghostOffsetStart[0]; ++ighost) {
          const PetscInt i = nNeighbor[0] - ghostOffsetStart[0] + ighost;
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
    }

    /* Neighbor 7 (up) */
    if (!dummyEnd[1]) {
      /* We cannot be the last rank in y, though our neighbor may be
       We may be the last rank in x, in which case our neighbor is also */
      const PetscInt        neighbor                     = 7;
      const PetscInt        globalOffset                 = globalOffsets[stag->neighbors[neighbor]];
      const PetscInt *const nNeighbor                    = nNeighbors[neighbor];
      const PetscInt        entriesPerElementRowNeighbor = entriesPerElementRow; /* same as here */
      for (j = 0; j < ghostOffsetEnd[1]; ++j) {
        const PetscInt jghost = stag->nGhost[1] - ghostOffsetEnd[1] + j;
        for (ighost = ghostOffsetStart[0]; ighost < stag->nGhost[0] - ghostOffsetEnd[0]; ++ighost) {
          const PetscInt i = ighost - ghostOffsetStart[0];
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
        if (dummyEnd[0]) {
          const PetscInt ighost = stag->nGhost[0] - ghostOffsetEnd[0];
          const PetscInt i      = nNeighbor[0];
          for (d = 0; d < stag->dof[0]; ++d, ++count) { /* Vertex */
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
          for (d = 0; d < stag->dof[1]; ++d, ++count) { /* Face */
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + stag->dof[0] + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + stag->dof[0] + stag->dof[1] + d;
          }
        }
      }
    }

    /* Neighbor 8 (up right) */
    if (!star && !dummyEnd[0] && !dummyEnd[1]) {
      /* We can never be a ghosted boundary
         Our neighbor may be a top boundary, a right boundary, or both */
      const PetscInt        neighbor                     = 8;
      const PetscInt        globalOffset                 = globalOffsets[stag->neighbors[neighbor]];
      const PetscInt *const nNeighbor                    = nNeighbors[neighbor];
      const PetscInt        entriesPerElementRowNeighbor = nNeighbor[0] * stag->entriesPerElement + (nextToDummyEnd[0] ? entriesPerFace : 0);
      for (j = 0; j < ghostOffsetEnd[1]; ++j) {
        const PetscInt jghost = stag->nGhost[1] - ghostOffsetEnd[1] + j;
        for (i = 0; i < ghostOffsetEnd[0]; ++i) {
          const PetscInt ighost = stag->nGhost[0] - ghostOffsetEnd[0] + i;
          for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
            idxGlobal[count] = globalOffset + j * entriesPerElementRowNeighbor + i * stag->entriesPerElement + d;
            idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
          }
        }
      }
    }

    PetscCheck(count == entriesToTransferTotal, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Number of entries computed in gtol (%" PetscInt_FMT ") is not as expected (%" PetscInt_FMT ")", count, entriesToTransferTotal);

    /* Create Local and Global ISs (transferring pointer ownership) */
    PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)dm), entriesToTransferTotal, idxLocal, PETSC_OWN_POINTER, &isLocal));
    PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)dm), entriesToTransferTotal, idxGlobal, PETSC_OWN_POINTER, &isGlobal));

    /* Create stag->gtol. The order is computed as PETSc ordering, and doesn't include dummy entries */
    {
      Vec local, global;
      PetscCall(VecCreateMPIWithArray(PetscObjectComm((PetscObject)dm), 1, stag->entries, PETSC_DECIDE, NULL, &global));
      PetscCall(VecCreateSeqWithArray(PETSC_COMM_SELF, stag->entriesPerElement, stag->entriesGhost, NULL, &local));
      PetscCall(VecScatterCreate(global, isGlobal, local, isLocal, &stag->gtol));
      PetscCall(VecDestroy(&global));
      PetscCall(VecDestroy(&local));
    }

    /* Destroy ISs */
    PetscCall(ISDestroy(&isLocal));
    PetscCall(ISDestroy(&isGlobal));

    /* Next, we iterate over the local entries  again, in local order, recording the global entry to which each maps,
       or -1 if there is none */
    PetscCall(PetscMalloc1(stag->entriesGhost, &idxGlobalAll));

    countAll = 0;

    /* Loop over rows 1/3 : down */
    if (!dummyStart[1]) {
      for (jghost = 0; jghost < ghostOffsetStart[1]; ++jghost) {
        /* Loop over columns 1/3 : down left */
        if (!star && !dummyStart[0]) {
          const PetscInt        neighbor     = 0;
          const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
          const PetscInt *const nNeighbor    = nNeighbors[neighbor];
          const PetscInt j = nNeighbor[1] - ghostOffsetStart[1] + jghost; /* Note: this is actually the same value for the whole row of ranks below, so recomputing it for the next two ranks is redundant, and one could even get rid of jghost entirely if desired */
          const PetscInt eprNeighbor = nNeighbor[0] * stag->entriesPerElement;
          for (i = nNeighbor[0] - ghostOffsetStart[0]; i < nNeighbor[0]; ++i) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
          }
        } else {
          /* Down Left dummies */
          for (ighost = 0; ighost < ghostOffsetStart[0]; ++ighost) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
          }
        }

        /* Loop over columns 2/3 : down middle */
        {
          const PetscInt        neighbor     = 1;
          const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
          const PetscInt *const nNeighbor    = nNeighbors[neighbor];
          const PetscInt        j            = nNeighbor[1] - ghostOffsetStart[1] + jghost;
          const PetscInt        eprNeighbor  = entriesPerElementRow; /* same as here */
          for (i = 0; i < nNeighbor[0]; ++i) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
          }
        }

        /* Loop over columns 3/3 : down right */
        if (!star && !dummyEnd[0]) {
          const PetscInt        neighbor     = 2;
          const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
          const PetscInt *const nNeighbor    = nNeighbors[neighbor];
          const PetscInt        j            = nNeighbor[1] - ghostOffsetStart[1] + jghost;
          const PetscInt        eprNeighbor  = nNeighbor[0] * stag->entriesPerElement + (nextToDummyEnd[0] ? entriesPerFace : 0);
          for (i = 0; i < ghostOffsetEnd[0]; ++i) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
          }
        } else if (dummyEnd[0]) {
          /* Down right partial dummy elements, living on the *down* rank */
          const PetscInt        neighbor     = 1;
          const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
          const PetscInt *const nNeighbor    = nNeighbors[neighbor];
          const PetscInt        j            = nNeighbor[1] - ghostOffsetStart[1] + jghost;
          const PetscInt        eprNeighbor  = entriesPerElementRow; /* same as here */
          PetscInt              dGlobal;
          i = nNeighbor[0];
          for (d = 0, dGlobal = 0; d < stag->dof[0]; ++d, ++dGlobal, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + dGlobal;
          for (; d < stag->dof[0] + stag->dof[1]; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy down face point */ }
          for (; d < stag->dof[0] + 2 * stag->dof[1]; ++d, ++dGlobal, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + dGlobal;
          for (; d < stag->entriesPerElement; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy element point */ }
          ++i;
          for (; i < nNeighbor[0] + ghostOffsetEnd[0]; ++i) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
          }
        } else {
          /* Down Right dummies */
          for (ighost = 0; ighost < ghostOffsetEnd[0]; ++ighost) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
          }
        }
      }
    } else {
      /* Down dummies row */
      for (jghost = 0; jghost < ghostOffsetStart[1]; ++jghost) {
        for (ighost = 0; ighost < stag->nGhost[0]; ++ighost) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
        }
      }
    }

    /* Loop over rows 2/3 : center */
    for (j = 0; j < stag->n[1]; ++j) {
      /* Loop over columns 1/3 : left */
      if (!dummyStart[0]) {
        const PetscInt        neighbor     = 3;
        const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
        const PetscInt *const nNeighbor    = nNeighbors[neighbor];
        const PetscInt        eprNeighbor  = nNeighbor[0] * stag->entriesPerElement;
        for (i = nNeighbor[0] - ghostOffsetStart[0]; i < nNeighbor[0]; ++i) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
        }
      } else {
        /* (Middle) Left dummies */
        for (ighost = 0; ighost < ghostOffsetStart[0]; ++ighost) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
        }
      }

      /* Loop over columns 2/3 : here (the "neighbor" is ourselves, here) */
      {
        const PetscInt neighbor     = 4;
        const PetscInt globalOffset = globalOffsets[stag->neighbors[neighbor]];
        const PetscInt eprNeighbor  = entriesPerElementRow; /* same as here (obviously) */
        for (i = 0; i < stag->n[0]; ++i) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
        }
      }

      /* Loop over columns 3/3 : right */
      if (!dummyEnd[0]) {
        const PetscInt        neighbor     = 5;
        const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
        const PetscInt *const nNeighbor    = nNeighbors[neighbor];
        const PetscInt        eprNeighbor  = nNeighbor[0] * stag->entriesPerElement + (nextToDummyEnd[0] ? entriesPerFace : 0);
        for (i = 0; i < ghostOffsetEnd[0]; ++i) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
        }
      } else {
        /* -1's for right layer of partial dummies, living on *this* rank */
        const PetscInt        neighbor     = 4;
        const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
        const PetscInt *const nNeighbor    = nNeighbors[neighbor];
        const PetscInt        eprNeighbor  = entriesPerElementRow; /* same as here (obviously) */
        PetscInt              dGlobal;
        i = nNeighbor[0];
        for (d = 0, dGlobal = 0; d < stag->dof[0]; ++d, ++dGlobal, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + dGlobal;
        for (; d < stag->dof[0] + stag->dof[1]; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy down face point */ }
        for (; d < stag->dof[0] + 2 * stag->dof[1]; ++d, ++dGlobal, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + dGlobal;
        for (; d < stag->entriesPerElement; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy element point */ }
        ++i;
        for (; i < nNeighbor[0] + ghostOffsetEnd[0]; ++i) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
        }
      }
    }

    /* Loop over rows 3/3 : up */
    if (!dummyEnd[1]) {
      for (j = 0; j < ghostOffsetEnd[1]; ++j) {
        /* Loop over columns 1/3 : up left */
        if (!star && !dummyStart[0]) {
          const PetscInt        neighbor     = 6;
          const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
          const PetscInt *const nNeighbor    = nNeighbors[neighbor];
          const PetscInt        eprNeighbor  = nNeighbor[0] * stag->entriesPerElement;
          for (i = nNeighbor[0] - ghostOffsetStart[0]; i < nNeighbor[0]; ++i) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
          }
        } else {
          /* Up Left dummies */
          for (ighost = 0; ighost < ghostOffsetStart[0]; ++ighost) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
          }
        }

        /* Loop over columns 2/3 : up */
        {
          const PetscInt        neighbor     = 7;
          const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
          const PetscInt *const nNeighbor    = nNeighbors[neighbor];
          const PetscInt        eprNeighbor  = entriesPerElementRow; /* Same as here */
          for (i = 0; i < nNeighbor[0]; ++i) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
          }
        }

        /* Loop over columns 3/3 : up right */
        if (!star && !dummyEnd[0]) {
          const PetscInt        neighbor     = 8;
          const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
          const PetscInt *const nNeighbor    = nNeighbors[neighbor];
          const PetscInt        eprNeighbor  = nNeighbor[0] * stag->entriesPerElement + (nextToDummyEnd[0] ? entriesPerFace : 0);
          for (i = 0; i < ghostOffsetEnd[0]; ++i) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + d;
          }
        } else if (dummyEnd[0]) {
          /* -1's for right layer of partial dummies, living on rank above */
          const PetscInt        neighbor     = 7;
          const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
          const PetscInt *const nNeighbor    = nNeighbors[neighbor];
          const PetscInt        eprNeighbor  = entriesPerElementRow; /* Same as here */
          PetscInt              dGlobal;
          i = nNeighbor[0];
          for (d = 0, dGlobal = 0; d < stag->dof[0]; ++d, ++dGlobal, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + dGlobal;
          for (; d < stag->dof[0] + stag->dof[1]; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy down face point */ }
          for (; d < stag->dof[0] + 2 * stag->dof[1]; ++d, ++dGlobal, ++countAll) idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * stag->entriesPerElement + dGlobal;
          for (; d < stag->entriesPerElement; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy element point */ }
          ++i;
          for (; i < nNeighbor[0] + ghostOffsetEnd[0]; ++i) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
          }
        } else {
          /* Up Right dummies */
          for (ighost = 0; ighost < ghostOffsetEnd[0]; ++ighost) {
            for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
          }
        }
      }
    } else {
      j = stag->n[1];
      /* Top layer of partial dummies */

      /* up left partial dummies layer : Loop over columns 1/3 : living on *left* neighbor */
      if (!dummyStart[0]) {
        const PetscInt        neighbor     = 3;
        const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
        const PetscInt *const nNeighbor    = nNeighbors[neighbor];
        const PetscInt        eprNeighbor  = nNeighbor[0] * stag->entriesPerElement;
        for (i = nNeighbor[0] - ghostOffsetStart[0]; i < nNeighbor[0]; ++i) {
          for (d = 0; d < stag->dof[0] + stag->dof[1]; ++d, ++countAll) { idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * entriesPerFace + d; /* Note entriesPerFace here */ }
          for (; d < stag->entriesPerElement; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy left face and element points */ }
        }
      } else {
        for (ighost = 0; ighost < ghostOffsetStart[0]; ++ighost) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
        }
      }

      /* up partial dummies layer : Loop over columns 2/3 : living on *this* rank */
      {
        const PetscInt neighbor     = 4;
        const PetscInt globalOffset = globalOffsets[stag->neighbors[neighbor]];
        const PetscInt eprNeighbor  = entriesPerElementRow; /* same as here (obviously) */
        for (i = 0; i < stag->n[0]; ++i) {
          for (d = 0; d < stag->dof[0] + stag->dof[1]; ++d, ++countAll) { idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * entriesPerFace + d; /* Note entriesPerFace here */ }
          for (; d < stag->entriesPerElement; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy left face and element points */ }
        }
      }

      if (!dummyEnd[0]) {
        /* up right partial dummies layer : Loop over columns 3/3 :  living on *right* neighbor */
        const PetscInt        neighbor     = 5;
        const PetscInt        globalOffset = globalOffsets[stag->neighbors[neighbor]];
        const PetscInt *const nNeighbor    = nNeighbors[neighbor];
        const PetscInt        eprNeighbor  = nNeighbor[0] * stag->entriesPerElement + (nextToDummyEnd[0] ? entriesPerFace : 0);
        for (i = 0; i < ghostOffsetEnd[0]; ++i) {
          for (d = 0; d < stag->dof[0] + stag->dof[1]; ++d, ++countAll) { idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * entriesPerFace + d; /* Note entriesPerFace here */ }
          for (; d < stag->entriesPerElement; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy left face and element points */ }
        }
      } else {
        /* Top partial dummies layer : Loop over columns 3/3 : right, living *here* */
        const PetscInt neighbor     = 4;
        const PetscInt globalOffset = globalOffsets[stag->neighbors[neighbor]];
        const PetscInt eprNeighbor  = entriesPerElementRow; /* same as here (obviously) */
        i                           = stag->n[0];
        for (d = 0; d < stag->dof[0]; ++d, ++countAll) {                                    /* Note just the vertex here */
          idxGlobalAll[countAll] = globalOffset + j * eprNeighbor + i * entriesPerFace + d; /* Note entriesPerFace here */
        }
        for (; d < stag->entriesPerElement; ++d, ++countAll) { idxGlobalAll[countAll] = -1; /* dummy bottom face, left face and element points */ }
        ++i;
        for (; i < stag->n[0] + ghostOffsetEnd[0]; ++i) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
        }
      }
      ++j;
      /* Additional top dummy layers */
      for (; j < stag->n[1] + ghostOffsetEnd[1]; ++j) {
        for (ighost = 0; ighost < stag->nGhost[0]; ++ighost) {
          for (d = 0; d < stag->entriesPerElement; ++d, ++countAll) idxGlobalAll[countAll] = -1;
        }
      }
    }

    /* Create local-to-global map (in local ordering, includes maps to -1 for dummy points) */
    PetscCall(ISLocalToGlobalMappingCreate(comm, 1, stag->entriesGhost, idxGlobalAll, PETSC_OWN_POINTER, &dm->ltogmap));
  }

  /* In special cases, create a dedicated injective local-to-global map */
  if ((stag->boundaryType[0] == DM_BOUNDARY_PERIODIC && stag->nRanks[0] == 1) || (stag->boundaryType[1] == DM_BOUNDARY_PERIODIC && stag->nRanks[1] == 1)) PetscCall(DMStagPopulateLocalToGlobalInjective(dm));

  /* Free global offsets */
  PetscCall(PetscFree(globalOffsets));

  /* Precompute location offsets */
  PetscCall(DMStagComputeLocationOffsets_2d(dm));

  /* View from Options */
  PetscCall(DMViewFromOptions(dm, NULL, "-dm_view"));

  PetscFunctionReturn(PETSC_SUCCESS);
}

/* adapted from da2.c */
static PetscErrorCode DMStagSetUpBuildRankGrid_2d(DM dm)
{
  DM_Stag *const stag = (DM_Stag *)dm->data;
  PetscInt       m, n;
  PetscMPIInt    rank, size;
  const PetscInt M = stag->N[0];
  const PetscInt N = stag->N[1];

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)dm), &size));
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)dm), &rank));
  m = stag->nRanks[0];
  n = stag->nRanks[1];
  if (m != PETSC_DECIDE) {
    PetscCheck(m >= 1, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Non-positive number of ranks in X direction: %" PetscInt_FMT, m);
    PetscCheck(m <= size, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Too many ranks in X direction: %" PetscInt_FMT " %d", m, size);
  }
  if (n != PETSC_DECIDE) {
    PetscCheck(n >= 1, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Non-positive number of ranks in Y direction: %" PetscInt_FMT, n);
    PetscCheck(n <= size, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Too many ranks in Y direction: %" PetscInt_FMT " %d", n, size);
  }
  if (m == PETSC_DECIDE || n == PETSC_DECIDE) {
    if (n != PETSC_DECIDE) {
      m = size / n;
    } else if (m != PETSC_DECIDE) {
      n = size / m;
    } else {
      /* try for squarish distribution */
      m = (PetscInt)(0.5 + PetscSqrtReal(((PetscReal)M) * ((PetscReal)size) / ((PetscReal)N)));
      if (!m) m = 1;
      while (m > 0) {
        n = size / m;
        if (m * n == size) break;
        m--;
      }
      if (M > N && m < n) {
        PetscInt _m = m;
        m           = n;
        n           = _m;
      }
    }
    PetscCheck(m * n == size, PetscObjectComm((PetscObject)dm), PETSC_ERR_PLIB, "Unable to create partition, check the size of the communicator and input m and n ");
  } else PetscCheck(m * n == size, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Given Bad partition. Product of sizes (%" PetscInt_FMT ") does not equal communicator size (%d)", m * n, size);
  PetscCheck(M >= m, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Partition in x direction is too fine! %" PetscInt_FMT " %" PetscInt_FMT, M, m);
  PetscCheck(N >= n, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Partition in y direction is too fine! %" PetscInt_FMT " %" PetscInt_FMT, N, n);
  stag->nRanks[0] = m;
  stag->nRanks[1] = n;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMStagSetUpBuildNeighbors_2d(DM dm)
{
  DM_Stag *const stag = (DM_Stag *)dm->data;
  PetscInt       d, i;
  PetscBool      per[2], first[2], last[2];
  PetscInt       neighborRank[9][2], r[2], n[2];
  const PetscInt dim = 2;

  PetscFunctionBegin;
  for (d = 0; d < dim; ++d)
    PetscCheck(stag->boundaryType[d] == DM_BOUNDARY_NONE || stag->boundaryType[d] == DM_BOUNDARY_PERIODIC || stag->boundaryType[d] == DM_BOUNDARY_GHOSTED, PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "Neighbor determination not implemented for %s",
               DMBoundaryTypes[stag->boundaryType[d]]);

  /* Assemble some convenience variables */
  for (d = 0; d < dim; ++d) {
    per[d]   = (PetscBool)(stag->boundaryType[d] == DM_BOUNDARY_PERIODIC);
    first[d] = stag->firstRank[d];
    last[d]  = stag->lastRank[d];
    r[d]     = stag->rank[d];
    n[d]     = stag->nRanks[d];
  }

  /* First, compute the position in the rank grid for all neighbors */
  neighborRank[0][0] = first[0] ? (per[0] ? n[0] - 1 : -1) : r[0] - 1; /* left  down */
  neighborRank[0][1] = first[1] ? (per[1] ? n[1] - 1 : -1) : r[1] - 1;

  neighborRank[1][0] = r[0]; /*       down */
  neighborRank[1][1] = first[1] ? (per[1] ? n[1] - 1 : -1) : r[1] - 1;

  neighborRank[2][0] = last[0] ? (per[0] ? 0 : -1) : r[0] + 1; /* right down */
  neighborRank[2][1] = first[1] ? (per[1] ? n[1] - 1 : -1) : r[1] - 1;

  neighborRank[3][0] = first[0] ? (per[0] ? n[0] - 1 : -1) : r[0] - 1; /* left       */
  neighborRank[3][1] = r[1];

  neighborRank[4][0] = r[0]; /*            */
  neighborRank[4][1] = r[1];

  neighborRank[5][0] = last[0] ? (per[0] ? 0 : -1) : r[0] + 1; /* right      */
  neighborRank[5][1] = r[1];

  neighborRank[6][0] = first[0] ? (per[0] ? n[0] - 1 : -1) : r[0] - 1; /* left  up   */
  neighborRank[6][1] = last[1] ? (per[1] ? 0 : -1) : r[1] + 1;

  neighborRank[7][0] = r[0]; /*       up   */
  neighborRank[7][1] = last[1] ? (per[1] ? 0 : -1) : r[1] + 1;

  neighborRank[8][0] = last[0] ? (per[0] ? 0 : -1) : r[0] + 1; /* right up   */
  neighborRank[8][1] = last[1] ? (per[1] ? 0 : -1) : r[1] + 1;

  /* Then, compute the rank of each in the linear ordering */
  PetscCall(PetscMalloc1(9, &stag->neighbors));
  for (i = 0; i < 9; ++i) {
    if (neighborRank[i][0] >= 0 && neighborRank[i][1] >= 0) {
      stag->neighbors[i] = neighborRank[i][0] + n[0] * neighborRank[i][1];
    } else {
      stag->neighbors[i] = -1;
    }
  }

  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMStagSetUpBuildGlobalOffsets_2d(DM dm, PetscInt **pGlobalOffsets)
{
  const DM_Stag *const stag = (DM_Stag *)dm->data;
  PetscInt            *globalOffsets;
  PetscInt             i, j, d, entriesPerFace, count;
  PetscMPIInt          size;
  PetscBool            extra[2];

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)dm), &size));
  for (d = 0; d < 2; ++d) extra[d] = (PetscBool)(stag->boundaryType[d] != DM_BOUNDARY_PERIODIC); /* Extra points in global rep */
  entriesPerFace = stag->dof[0] + stag->dof[1];
  PetscCall(PetscMalloc1(size, pGlobalOffsets));
  globalOffsets    = *pGlobalOffsets;
  globalOffsets[0] = 0;
  count            = 1; /* note the count is offset by 1 here. We add the size of the previous rank */
  for (j = 0; j < stag->nRanks[1] - 1; ++j) {
    const PetscInt nnj = stag->l[1][j];
    for (i = 0; i < stag->nRanks[0] - 1; ++i) {
      const PetscInt nni   = stag->l[0][i];
      globalOffsets[count] = globalOffsets[count - 1] + nnj * nni * stag->entriesPerElement; /* No right/top/front boundaries */
      ++count;
    }
    {
      /* i = stag->nRanks[0]-1; */
      const PetscInt nni   = stag->l[0][i];
      globalOffsets[count] = globalOffsets[count - 1] + nnj * nni * stag->entriesPerElement + (extra[0] ? nnj * entriesPerFace : 0); /* Extra faces on the right */
      ++count;
    }
  }
  {
    /* j = stag->nRanks[1]-1; */
    const PetscInt nnj = stag->l[1][j];
    for (i = 0; i < stag->nRanks[0] - 1; ++i) {
      const PetscInt nni   = stag->l[0][i];
      globalOffsets[count] = globalOffsets[count - 1] + nni * nnj * stag->entriesPerElement + (extra[1] ? nni * entriesPerFace : 0); /* Extra faces on the top */
      ++count;
    }
    /* Don't need to compute entries in last element */
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMStagComputeLocationOffsets_2d(DM dm)
{
  DM_Stag *const stag = (DM_Stag *)dm->data;
  const PetscInt epe  = stag->entriesPerElement;
  const PetscInt epr  = stag->nGhost[0] * epe;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(DMSTAG_NUMBER_LOCATIONS, &stag->locationOffsets));
  stag->locationOffsets[DMSTAG_DOWN_LEFT]  = 0;
  stag->locationOffsets[DMSTAG_DOWN]       = stag->locationOffsets[DMSTAG_DOWN_LEFT] + stag->dof[0];
  stag->locationOffsets[DMSTAG_DOWN_RIGHT] = stag->locationOffsets[DMSTAG_DOWN_LEFT] + epe;
  stag->locationOffsets[DMSTAG_LEFT]       = stag->locationOffsets[DMSTAG_DOWN] + stag->dof[1];
  stag->locationOffsets[DMSTAG_ELEMENT]    = stag->locationOffsets[DMSTAG_LEFT] + stag->dof[1];
  stag->locationOffsets[DMSTAG_RIGHT]      = stag->locationOffsets[DMSTAG_LEFT] + epe;
  stag->locationOffsets[DMSTAG_UP_LEFT]    = stag->locationOffsets[DMSTAG_DOWN_LEFT] + epr;
  stag->locationOffsets[DMSTAG_UP]         = stag->locationOffsets[DMSTAG_DOWN] + epr;
  stag->locationOffsets[DMSTAG_UP_RIGHT]   = stag->locationOffsets[DMSTAG_UP_LEFT] + epe;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode DMStagPopulateLocalToGlobalInjective_2d(DM dm)
{
  DM_Stag *const  stag = (DM_Stag *)dm->data;
  PetscInt       *idxLocal, *idxGlobal, *globalOffsetsRecomputed;
  const PetscInt *globalOffsets;
  PetscInt        i, j, d, count, entriesPerCorner, entriesPerFace, entriesPerElementRowGhost, entriesPerElementRow, ghostOffsetStart[2];
  IS              isLocal, isGlobal;
  PetscBool       dummyEnd[2];

  PetscFunctionBegin;
  PetscCall(DMStagSetUpBuildGlobalOffsets_2d(dm, &globalOffsetsRecomputed)); /* note that we don't actually use all of these. An available optimization is to pass them, when available */
  globalOffsets = globalOffsetsRecomputed;
  PetscCall(PetscMalloc1(stag->entries, &idxLocal));
  PetscCall(PetscMalloc1(stag->entries, &idxGlobal));
  for (d = 0; d < 2; ++d) dummyEnd[d] = (PetscBool)(stag->lastRank[d] && stag->boundaryType[d] != DM_BOUNDARY_PERIODIC);
  entriesPerCorner          = stag->dof[0];
  entriesPerFace            = stag->dof[0] + stag->dof[1];
  entriesPerElementRow      = stag->n[0] * stag->entriesPerElement + (dummyEnd[0] ? entriesPerFace : 0);
  entriesPerElementRowGhost = stag->nGhost[0] * stag->entriesPerElement;
  count                     = 0;
  for (d = 0; d < 2; ++d) ghostOffsetStart[d] = stag->start[d] - stag->startGhost[d];
  {
    const PetscInt neighbor     = 4;
    const PetscInt globalOffset = globalOffsets[stag->neighbors[neighbor]];
    for (j = 0; j < stag->n[1]; ++j) {
      const PetscInt jghost = j + ghostOffsetStart[1];
      for (i = 0; i < stag->n[0]; ++i) {
        const PetscInt ighost = i + ghostOffsetStart[0];
        for (d = 0; d < stag->entriesPerElement; ++d, ++count) {
          idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * stag->entriesPerElement + d;
          idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
        }
      }
      if (dummyEnd[0]) {
        const PetscInt ighost = i + ghostOffsetStart[0];
        i                     = stag->n[0];
        for (d = 0; d < stag->dof[0]; ++d, ++count) { /* vertex first */
          idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * stag->entriesPerElement + d;
          idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
        }
        for (d = 0; d < stag->dof[1]; ++d, ++count) { /* then left edge (skipping bottom face) */
          idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * stag->entriesPerElement + stag->dof[0] + d;
          idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + stag->dof[0] + stag->dof[1] + d;
        }
      }
    }
    if (dummyEnd[1]) {
      const PetscInt jghost = j + ghostOffsetStart[1];
      j                     = stag->n[1];
      for (i = 0; i < stag->n[0]; ++i) {
        const PetscInt ighost = i + ghostOffsetStart[0];
        for (d = 0; d < entriesPerFace; ++d, ++count) {                                        /* vertex and bottom face (which are the first entries) */
          idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * entriesPerFace + d; /* note i increment by entriesPerFace */
          idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
        }
      }
      if (dummyEnd[0]) {
        const PetscInt ighost = i + ghostOffsetStart[0];
        i                     = stag->n[0];
        for (d = 0; d < entriesPerCorner; ++d, ++count) {                                      /* vertex only */
          idxGlobal[count] = globalOffset + j * entriesPerElementRow + i * entriesPerFace + d; /* note i increment by entriesPerFace */
          idxLocal[count]  = jghost * entriesPerElementRowGhost + ighost * stag->entriesPerElement + d;
        }
      }
    }
  }
  PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)dm), stag->entries, idxLocal, PETSC_OWN_POINTER, &isLocal));
  PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)dm), stag->entries, idxGlobal, PETSC_OWN_POINTER, &isGlobal));
  {
    Vec local, global;
    PetscCall(VecCreateMPIWithArray(PetscObjectComm((PetscObject)dm), 1, stag->entries, PETSC_DECIDE, NULL, &global));
    PetscCall(VecCreateSeqWithArray(PETSC_COMM_SELF, stag->entriesPerElement, stag->entriesGhost, NULL, &local));
    PetscCall(VecScatterCreate(local, isLocal, global, isGlobal, &stag->ltog_injective));
    PetscCall(VecDestroy(&global));
    PetscCall(VecDestroy(&local));
  }
  PetscCall(ISDestroy(&isLocal));
  PetscCall(ISDestroy(&isGlobal));
  if (globalOffsetsRecomputed) PetscCall(PetscFree(globalOffsetsRecomputed));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode DMCreateMatrix_Stag_2D_AIJ_Assemble(DM dm, Mat A)
{
  PetscInt          entries, dof[DMSTAG_MAX_STRATA], epe, stencil_width, N[2], start[2], n[2], n_extra[2];
  DMStagStencilType stencil_type;
  DMBoundaryType    boundary_type[2];

  PetscFunctionBegin;
  PetscCall(DMStagGetDOF(dm, &dof[0], &dof[1], &dof[2], NULL));
  PetscCall(DMStagGetStencilType(dm, &stencil_type));
  PetscCall(DMStagGetStencilWidth(dm, &stencil_width));
  PetscCall(DMStagGetEntries(dm, &entries));
  PetscCall(DMStagGetEntriesPerElement(dm, &epe));
  PetscCall(DMStagGetCorners(dm, &start[0], &start[1], NULL, &n[0], &n[1], NULL, &n_extra[0], &n_extra[1], NULL));
  PetscCall(DMStagGetGlobalSizes(dm, &N[0], &N[1], NULL));
  PetscCall(DMStagGetBoundaryTypes(dm, &boundary_type[0], &boundary_type[1], NULL));

  if (stencil_type == DMSTAG_STENCIL_NONE) {
    /* Couple all DOF at each location to each other */
    DMStagStencil *row_vertex, *row_face_down, *row_face_left, *row_element;

    PetscCall(PetscMalloc1(dof[0], &row_vertex));
    for (PetscInt c = 0; c < dof[0]; ++c) {
      row_vertex[c].loc = DMSTAG_DOWN_LEFT;
      row_vertex[c].c   = c;
    }

    PetscCall(PetscMalloc1(dof[1], &row_face_down));
    for (PetscInt c = 0; c < dof[1]; ++c) {
      row_face_down[c].loc = DMSTAG_DOWN;
      row_face_down[c].c   = c;
    }

    PetscCall(PetscMalloc1(dof[1], &row_face_left));
    for (PetscInt c = 0; c < dof[1]; ++c) {
      row_face_left[c].loc = DMSTAG_LEFT;
      row_face_left[c].c   = c;
    }

    PetscCall(PetscMalloc1(dof[2], &row_element));
    for (PetscInt c = 0; c < dof[2]; ++c) {
      row_element[c].loc = DMSTAG_ELEMENT;
      row_element[c].c   = c;
    }

    for (PetscInt ey = start[1]; ey < start[1] + n[1] + n_extra[1]; ++ey) {
      for (PetscInt ex = start[0]; ex < start[0] + n[0] + n_extra[0]; ++ex) {
        {
          for (PetscInt c = 0; c < dof[0]; ++c) {
            row_vertex[c].i = ex;
            row_vertex[c].j = ey;
          }
          PetscCall(DMStagMatSetValuesStencil(dm, A, dof[0], row_vertex, dof[0], row_vertex, NULL, INSERT_VALUES));
        }
        if (ex < N[0]) {
          for (PetscInt c = 0; c < dof[1]; ++c) {
            row_face_down[c].i = ex;
            row_face_down[c].j = ey;
          }
          PetscCall(DMStagMatSetValuesStencil(dm, A, dof[1], row_face_down, dof[1], row_face_down, NULL, INSERT_VALUES));
        }
        if (ey < N[1]) {
          for (PetscInt c = 0; c < dof[1]; ++c) {
            row_face_left[c].i = ex;
            row_face_left[c].j = ey;
          }
          PetscCall(DMStagMatSetValuesStencil(dm, A, dof[1], row_face_left, dof[1], row_face_left, NULL, INSERT_VALUES));
        }
        if (ex < N[0] && ey < N[1]) {
          for (PetscInt c = 0; c < dof[2]; ++c) {
            row_element[c].i = ex;
            row_element[c].j = ey;
          }
          PetscCall(DMStagMatSetValuesStencil(dm, A, dof[2], row_element, dof[2], row_element, NULL, INSERT_VALUES));
        }
      }
    }
    PetscCall(PetscFree(row_vertex));
    PetscCall(PetscFree(row_face_left));
    PetscCall(PetscFree(row_face_down));
    PetscCall(PetscFree(row_element));
  } else if (stencil_type == DMSTAG_STENCIL_STAR || stencil_type == DMSTAG_STENCIL_BOX) {
    DMStagStencil *col, *row;

    PetscCall(PetscMalloc1(epe, &row));
    {
      PetscInt nrows = 0;

      for (PetscInt c = 0; c < dof[0]; ++c) {
        row[nrows].c   = c;
        row[nrows].loc = DMSTAG_DOWN_LEFT;
        ++nrows;
      }
      for (PetscInt c = 0; c < dof[1]; ++c) {
        row[nrows].c   = c;
        row[nrows].loc = DMSTAG_LEFT;
        ++nrows;
      }
      for (PetscInt c = 0; c < dof[1]; ++c) {
        row[nrows].c   = c;
        row[nrows].loc = DMSTAG_DOWN;
        ++nrows;
      }
      for (PetscInt c = 0; c < dof[2]; ++c) {
        row[nrows].c   = c;
        row[nrows].loc = DMSTAG_ELEMENT;
        ++nrows;
      }
    }

    PetscCall(PetscMalloc1(epe, &col));
    {
      PetscInt ncols = 0;

      for (PetscInt c = 0; c < dof[0]; ++c) {
        col[ncols].c   = c;
        col[ncols].loc = DMSTAG_DOWN_LEFT;
        ++ncols;
      }
      for (PetscInt c = 0; c < dof[1]; ++c) {
        col[ncols].c   = c;
        col[ncols].loc = DMSTAG_LEFT;
        ++ncols;
      }
      for (PetscInt c = 0; c < dof[1]; ++c) {
        col[ncols].c   = c;
        col[ncols].loc = DMSTAG_DOWN;
        ++ncols;
      }
      for (PetscInt c = 0; c < dof[2]; ++c) {
        col[ncols].c   = c;
        col[ncols].loc = DMSTAG_ELEMENT;
        ++ncols;
      }
    }

    for (PetscInt ey = start[1]; ey < start[1] + n[1] + n_extra[1]; ++ey) {
      for (PetscInt ex = start[0]; ex < start[0] + n[0] + n_extra[0]; ++ex) {
        for (PetscInt i = 0; i < epe; ++i) {
          row[i].i = ex;
          row[i].j = ey;
        }
        for (PetscInt offset_y = -stencil_width; offset_y <= stencil_width; ++offset_y) {
          const PetscInt ey_offset = ey + offset_y;
          for (PetscInt offset_x = -stencil_width; offset_x <= stencil_width; ++offset_x) {
            const PetscInt ex_offset = ex + offset_x;
            /* Only set values corresponding to elements which can have non-dummy entries,
               meaning those that map to unknowns in the global representation. In the periodic
               case, this is the entire stencil, but in all other cases, only includes a single
               "extra" element which is partially outside the physical domain (those points in the
               global representation */
            if ((stencil_type == DMSTAG_STENCIL_BOX || offset_x == 0 || offset_y == 0) && (boundary_type[0] == DM_BOUNDARY_PERIODIC || (ex_offset < N[0] + 1 && ex_offset >= 0)) && (boundary_type[1] == DM_BOUNDARY_PERIODIC || (ey_offset < N[1] + 1 && ey_offset >= 0))) {
              for (PetscInt i = 0; i < epe; ++i) {
                col[i].i = ex_offset;
                col[i].j = ey_offset;
              }
              PetscCall(DMStagMatSetValuesStencil(dm, A, epe, row, epe, col, NULL, INSERT_VALUES));
            }
          }
        }
      }
    }
    PetscCall(PetscFree(row));
    PetscCall(PetscFree(col));
  } else SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_OUTOFRANGE, "Unsupported stencil type %s", DMStagStencilTypes[stencil_type]);
  PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(PETSC_SUCCESS);
}

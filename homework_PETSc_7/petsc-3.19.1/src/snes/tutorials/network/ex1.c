static char help[] = "This example demonstrates the use of DMNetwork interface with subnetworks for solving a coupled nonlinear \n\
                      electric power grid and water pipe problem.\n\
                      The available solver options are in the ex1options file \n\
                      and the data files are in the datafiles of subdirectories.\n\
                      This example shows the use of subnetwork feature in DMNetwork. \n\
                      Run this program: mpiexec -n <n> ./ex1 \n\\n";

#include "power/power.h"
#include "water/water.h"

typedef struct {
  UserCtx_Power appctx_power;
  AppCtx_Water  appctx_water;
  PetscInt      subsnes_id; /* snes solver id */
  PetscInt      it;         /* iteration number */
  Vec           localXold;  /* store previous solution, used by FormFunction_Dummy() */
} UserCtx;

PetscErrorCode UserMonitor(SNES snes, PetscInt its, PetscReal fnorm, void *appctx)
{
  UserCtx    *user = (UserCtx *)appctx;
  Vec         X, localXold = user->localXold;
  DM          networkdm;
  PetscMPIInt rank;
  MPI_Comm    comm;

  PetscFunctionBegin;
  PetscCall(PetscObjectGetComm((PetscObject)snes, &comm));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
#if 0
  if (rank == 0) {
    PetscInt       subsnes_id = user->subsnes_id;
    if (subsnes_id == 2) {
      PetscCall(PetscPrintf(PETSC_COMM_SELF," it %" PetscInt_FMT ", subsnes_id %" PetscInt_FMT ", fnorm %g\n",user->it,user->subsnes_id,(double)fnorm));
    } else {
      PetscCall(PetscPrintf(PETSC_COMM_SELF,"       subsnes_id %" PetscInt_FMT ", fnorm %g\n",user->subsnes_id,(double)fnorm));
    }
  }
#endif
  PetscCall(SNESGetSolution(snes, &X));
  PetscCall(SNESGetDM(snes, &networkdm));
  PetscCall(DMGlobalToLocalBegin(networkdm, X, INSERT_VALUES, localXold));
  PetscCall(DMGlobalToLocalEnd(networkdm, X, INSERT_VALUES, localXold));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode FormJacobian_subPower(SNES snes, Vec X, Mat J, Mat Jpre, void *appctx)
{
  DM              networkdm;
  Vec             localX;
  PetscInt        nv, ne, i, j, offset, nvar, row;
  const PetscInt *vtx, *edges;
  PetscBool       ghostvtex;
  PetscScalar     one = 1.0;
  PetscMPIInt     rank;
  MPI_Comm        comm;

  PetscFunctionBegin;
  PetscCall(SNESGetDM(snes, &networkdm));
  PetscCall(DMGetLocalVector(networkdm, &localX));

  PetscCall(PetscObjectGetComm((PetscObject)networkdm, &comm));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));

  PetscCall(DMGlobalToLocalBegin(networkdm, X, INSERT_VALUES, localX));
  PetscCall(DMGlobalToLocalEnd(networkdm, X, INSERT_VALUES, localX));

  PetscCall(MatZeroEntries(J));

  /* Power subnetwork: copied from power/FormJacobian_Power() */
  PetscCall(DMNetworkGetSubnetwork(networkdm, 0, &nv, &ne, &vtx, &edges));
  PetscCall(FormJacobian_Power_private(networkdm, localX, J, nv, ne, vtx, edges, appctx));

  /* Water subnetwork: Identity */
  PetscCall(DMNetworkGetSubnetwork(networkdm, 1, &nv, &ne, &vtx, &edges));
  for (i = 0; i < nv; i++) {
    PetscCall(DMNetworkIsGhostVertex(networkdm, vtx[i], &ghostvtex));
    if (ghostvtex) continue;

    PetscCall(DMNetworkGetGlobalVecOffset(networkdm, vtx[i], ALL_COMPONENTS, &offset));
    PetscCall(DMNetworkGetComponent(networkdm, vtx[i], ALL_COMPONENTS, NULL, NULL, &nvar));
    for (j = 0; j < nvar; j++) {
      row = offset + j;
      PetscCall(MatSetValues(J, 1, &row, 1, &row, &one, ADD_VALUES));
    }
  }
  PetscCall(MatAssemblyBegin(J, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(J, MAT_FINAL_ASSEMBLY));

  PetscCall(DMRestoreLocalVector(networkdm, &localX));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Dummy equation localF(X) = localX - localXold */
PetscErrorCode FormFunction_Dummy(DM networkdm, Vec localX, Vec localF, PetscInt nv, PetscInt ne, const PetscInt *vtx, const PetscInt *edges, void *appctx)
{
  const PetscScalar *xarr, *xoldarr;
  PetscScalar       *farr;
  PetscInt           i, j, offset, nvar;
  PetscBool          ghostvtex;
  UserCtx           *user      = (UserCtx *)appctx;
  Vec                localXold = user->localXold;

  PetscFunctionBegin;
  PetscCall(VecGetArrayRead(localX, &xarr));
  PetscCall(VecGetArrayRead(localXold, &xoldarr));
  PetscCall(VecGetArray(localF, &farr));

  for (i = 0; i < nv; i++) {
    PetscCall(DMNetworkIsGhostVertex(networkdm, vtx[i], &ghostvtex));
    if (ghostvtex) continue;

    PetscCall(DMNetworkGetLocalVecOffset(networkdm, vtx[i], ALL_COMPONENTS, &offset));
    PetscCall(DMNetworkGetComponent(networkdm, vtx[i], ALL_COMPONENTS, NULL, NULL, &nvar));
    for (j = 0; j < nvar; j++) farr[offset + j] = xarr[offset + j] - xoldarr[offset + j];
  }

  PetscCall(VecRestoreArrayRead(localX, &xarr));
  PetscCall(VecRestoreArrayRead(localXold, &xoldarr));
  PetscCall(VecRestoreArray(localF, &farr));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode FormFunction(SNES snes, Vec X, Vec F, void *appctx)
{
  DM              networkdm;
  Vec             localX, localF;
  PetscInt        nv, ne, v;
  const PetscInt *vtx, *edges;
  PetscMPIInt     rank;
  MPI_Comm        comm;
  UserCtx        *user         = (UserCtx *)appctx;
  UserCtx_Power   appctx_power = (*user).appctx_power;
  AppCtx_Water    appctx_water = (*user).appctx_water;

  PetscFunctionBegin;
  PetscCall(SNESGetDM(snes, &networkdm));
  PetscCall(PetscObjectGetComm((PetscObject)networkdm, &comm));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));

  PetscCall(DMGetLocalVector(networkdm, &localX));
  PetscCall(DMGetLocalVector(networkdm, &localF));
  PetscCall(VecSet(F, 0.0));
  PetscCall(VecSet(localF, 0.0));

  PetscCall(DMGlobalToLocalBegin(networkdm, X, INSERT_VALUES, localX));
  PetscCall(DMGlobalToLocalEnd(networkdm, X, INSERT_VALUES, localX));

  /* Form Function for power subnetwork */
  PetscCall(DMNetworkGetSubnetwork(networkdm, 0, &nv, &ne, &vtx, &edges));
  if (user->subsnes_id == 1) { /* snes_water only */
    PetscCall(FormFunction_Dummy(networkdm, localX, localF, nv, ne, vtx, edges, user));
  } else {
    PetscCall(FormFunction_Power(networkdm, localX, localF, nv, ne, vtx, edges, &appctx_power));
  }

  /* Form Function for water subnetwork */
  PetscCall(DMNetworkGetSubnetwork(networkdm, 1, &nv, &ne, &vtx, &edges));
  if (user->subsnes_id == 0) { /* snes_power only */
    PetscCall(FormFunction_Dummy(networkdm, localX, localF, nv, ne, vtx, edges, user));
  } else {
    PetscCall(FormFunction_Water(networkdm, localX, localF, nv, ne, vtx, edges, NULL));
  }

  /* Illustrate how to access the coupling vertex of the subnetworks without doing anything to F yet */
  PetscCall(DMNetworkGetSharedVertices(networkdm, &nv, &vtx));
  for (v = 0; v < nv; v++) {
    PetscInt        key, ncomp, nvar, nconnedges, k, e, keye, goffset[3];
    void           *component;
    const PetscInt *connedges;

    PetscCall(DMNetworkGetComponent(networkdm, vtx[v], ALL_COMPONENTS, NULL, NULL, &nvar));
    PetscCall(DMNetworkGetNumComponents(networkdm, vtx[v], &ncomp));
    /* printf("  [%d] coupling vertex[%" PetscInt_FMT "]: v %" PetscInt_FMT ", ncomp %" PetscInt_FMT "; nvar %" PetscInt_FMT "\n",rank,v,vtx[v], ncomp,nvar); */

    for (k = 0; k < ncomp; k++) {
      PetscCall(DMNetworkGetComponent(networkdm, vtx[v], k, &key, &component, &nvar));
      PetscCall(DMNetworkGetGlobalVecOffset(networkdm, vtx[v], k, &goffset[k]));

      /* Verify the coupling vertex is a powernet load vertex or a water vertex */
      switch (k) {
      case 0:
        PetscCheck(key == appctx_power.compkey_bus && nvar == 2, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "key %" PetscInt_FMT " not a power bus vertex or nvar %" PetscInt_FMT " != 2", key, nvar);
        break;
      case 1:
        PetscCheck(key == appctx_power.compkey_load && nvar == 0 && goffset[1] == goffset[0] + 2, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Not a power load vertex");
        break;
      case 2:
        PetscCheck(key == appctx_water.compkey_vtx && nvar == 1 && goffset[2] == goffset[1], PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Not a water vertex");
        break;
      default:
        SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "k %" PetscInt_FMT " is wrong", k);
      }
      /* printf("  [%d] coupling vertex[%" PetscInt_FMT "]: key %" PetscInt_FMT "; nvar %" PetscInt_FMT ", goffset %" PetscInt_FMT "\n",rank,v,key,nvar,goffset[k]); */
    }

    /* Get its supporting edges */
    PetscCall(DMNetworkGetSupportingEdges(networkdm, vtx[v], &nconnedges, &connedges));
    /* printf("\n[%d] coupling vertex: nconnedges %" PetscInt_FMT "\n",rank,nconnedges); */
    for (k = 0; k < nconnedges; k++) {
      e = connedges[k];
      PetscCall(DMNetworkGetNumComponents(networkdm, e, &ncomp));
      /* printf("\n  [%d] connected edge[%" PetscInt_FMT "]=%" PetscInt_FMT " has ncomp %" PetscInt_FMT "\n",rank,k,e,ncomp); */
      PetscCall(DMNetworkGetComponent(networkdm, e, 0, &keye, &component, NULL));
      if (keye != appctx_water.compkey_edge) PetscCheck(keye == appctx_power.compkey_branch, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Not a power branch");
    }
  }

  PetscCall(DMRestoreLocalVector(networkdm, &localX));

  PetscCall(DMLocalToGlobalBegin(networkdm, localF, ADD_VALUES, F));
  PetscCall(DMLocalToGlobalEnd(networkdm, localF, ADD_VALUES, F));
  PetscCall(DMRestoreLocalVector(networkdm, &localF));
#if 0
  if (rank == 0) printf("F:\n");
  PetscCall(VecView(F,PETSC_VIEWER_STDOUT_WORLD));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SetInitialGuess(DM networkdm, Vec X, void *appctx)
{
  PetscInt        nv, ne, i, j, ncomp, offset, key;
  const PetscInt *vtx, *edges;
  UserCtx        *user         = (UserCtx *)appctx;
  Vec             localX       = user->localXold;
  UserCtx_Power   appctx_power = (*user).appctx_power;
  AppCtx_Water    appctx_water = (*user).appctx_water;
  PetscBool       ghost;
  PetscScalar    *xarr;
  VERTEX_Power    bus;
  VERTEX_Water    vertex;
  void           *component;
  GEN             gen;

  PetscFunctionBegin;
  PetscCall(VecSet(X, 0.0));
  PetscCall(VecSet(localX, 0.0));

  /* Set initial guess for power subnetwork */
  PetscCall(DMNetworkGetSubnetwork(networkdm, 0, &nv, &ne, &vtx, &edges));
  PetscCall(SetInitialGuess_Power(networkdm, localX, nv, ne, vtx, edges, &appctx_power));

  /* Set initial guess for water subnetwork */
  PetscCall(DMNetworkGetSubnetwork(networkdm, 1, &nv, &ne, &vtx, &edges));
  PetscCall(SetInitialGuess_Water(networkdm, localX, nv, ne, vtx, edges, NULL));

  /* Set initial guess at the coupling vertex */
  PetscCall(VecGetArray(localX, &xarr));
  PetscCall(DMNetworkGetSharedVertices(networkdm, &nv, &vtx));
  for (i = 0; i < nv; i++) {
    PetscCall(DMNetworkIsGhostVertex(networkdm, vtx[i], &ghost));
    if (ghost) continue;

    PetscCall(DMNetworkGetNumComponents(networkdm, vtx[i], &ncomp));
    for (j = 0; j < ncomp; j++) {
      PetscCall(DMNetworkGetLocalVecOffset(networkdm, vtx[i], j, &offset));
      PetscCall(DMNetworkGetComponent(networkdm, vtx[i], j, &key, (void **)&component, NULL));
      if (key == appctx_power.compkey_bus) {
        bus              = (VERTEX_Power)(component);
        xarr[offset]     = bus->va * PETSC_PI / 180.0;
        xarr[offset + 1] = bus->vm;
      } else if (key == appctx_power.compkey_gen) {
        gen = (GEN)(component);
        if (!gen->status) continue;
        xarr[offset + 1] = gen->vs;
      } else if (key == appctx_water.compkey_vtx) {
        vertex = (VERTEX_Water)(component);
        if (vertex->type == VERTEX_TYPE_JUNCTION) {
          xarr[offset] = 100;
        } else if (vertex->type == VERTEX_TYPE_RESERVOIR) {
          xarr[offset] = vertex->res.head;
        } else {
          xarr[offset] = vertex->tank.initlvl + vertex->tank.elev;
        }
      }
    }
  }
  PetscCall(VecRestoreArray(localX, &xarr));

  PetscCall(DMLocalToGlobalBegin(networkdm, localX, ADD_VALUES, X));
  PetscCall(DMLocalToGlobalEnd(networkdm, localX, ADD_VALUES, X));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Set coordinates */
static PetscErrorCode CoordinateVecSetUp(DM dm, Vec coords)
{
  DM              dmclone;
  PetscInt        i, gidx, offset, v, nv, Nsubnet;
  const PetscInt *vtx;
  PetscScalar    *carray;

  PetscFunctionBeginUser;
  PetscCall(DMGetCoordinateDM(dm, &dmclone));
  PetscCall(VecGetArrayWrite(coords, &carray));
  PetscCall(DMNetworkGetNumSubNetworks(dm, NULL, &Nsubnet));
  for (i = 0; i < Nsubnet; i++) {
    PetscCall(DMNetworkGetSubnetwork(dm, i, &nv, NULL, &vtx, NULL));
    for (v = 0; v < nv; v++) {
      PetscCall(DMNetworkGetGlobalVertexIndex(dm, vtx[v], &gidx));
      PetscCall(DMNetworkGetLocalVecOffset(dmclone, vtx[v], 0, &offset));
      switch (gidx) {
      case 0:
        carray[offset]     = -1.0;
        carray[offset + 1] = -1.0;
        break;
      case 1:
        carray[offset]     = -2.0;
        carray[offset + 1] = 2.0;
        break;
      case 2:
        carray[offset]     = 0.0;
        carray[offset + 1] = 2.0;
        break;
      case 3:
        carray[offset]     = -1.0;
        carray[offset + 1] = 0.0;
        break;
      case 4:
        carray[offset]     = 0.0;
        carray[offset + 1] = 0.0;
        break;
      case 5:
        carray[offset]     = 0.0;
        carray[offset + 1] = 1.0;
        break;
      case 6:
        carray[offset]     = -1.0;
        carray[offset + 1] = 1.0;
        break;
      case 7:
        carray[offset]     = -2.0;
        carray[offset + 1] = 1.0;
        break;
      case 8:
        carray[offset]     = -2.0;
        carray[offset + 1] = 0.0;
        break;
      case 9:
        carray[offset]     = 1.0;
        carray[offset + 1] = 0.0;
        break;
      case 10:
        carray[offset]     = 1.0;
        carray[offset + 1] = -1.0;
        break;
      case 11:
        carray[offset]     = 2.0;
        carray[offset + 1] = -1.0;
        break;
      case 12:
        carray[offset]     = 2.0;
        carray[offset + 1] = 0.0;
        break;
      case 13:
        carray[offset]     = 0.0;
        carray[offset + 1] = -1.0;
        break;
      case 14:
        carray[offset]     = 2.0;
        carray[offset + 1] = 1.0;
        break;
      default:
        PetscCheck(gidx < 15 && gidx > -1, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "gidx %" PetscInt_FMT "must between 0 and 14", gidx);
      }
    }
  }
  PetscCall(VecRestoreArrayWrite(coords, &carray));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CoordinatePrint(DM dm)
{
  DM                 dmclone;
  PetscInt           cdim, v, off, vglobal, vStart, vEnd;
  const PetscScalar *carray;
  Vec                coords;
  MPI_Comm           comm;
  PetscMPIInt        rank;

  PetscFunctionBegin;
  PetscCall(PetscObjectGetComm((PetscObject)dm, &comm));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));

  PetscCall(DMGetCoordinateDM(dm, &dmclone));
  PetscCall(DMNetworkGetVertexRange(dm, &vStart, &vEnd));
  PetscCall(DMGetCoordinatesLocal(dm, &coords));

  PetscCall(DMGetCoordinateDim(dm, &cdim));
  PetscCall(VecGetArrayRead(coords, &carray));

  PetscCall(PetscPrintf(MPI_COMM_WORLD, "\nCoordinatePrint, cdim %" PetscInt_FMT ":\n", cdim));
  PetscCall(PetscSynchronizedPrintf(MPI_COMM_WORLD, "[%i]\n", rank));
  for (v = vStart; v < vEnd; v++) {
    PetscCall(DMNetworkGetLocalVecOffset(dmclone, v, 0, &off));
    PetscCall(DMNetworkGetGlobalVertexIndex(dmclone, v, &vglobal));
    switch (cdim) {
    case 2:
      PetscCall(PetscSynchronizedPrintf(MPI_COMM_WORLD, "Vertex: %" PetscInt_FMT ", x =  %f y = %f \n", vglobal, (double)PetscRealPart(carray[off]), (double)PetscRealPart(carray[off + 1])));
      break;
    default:
      PetscCheck(cdim == 2, MPI_COMM_WORLD, PETSC_ERR_SUP, "Only supports Network embedding dimension of 2, not supplied  %" PetscInt_FMT, cdim);
      break;
    }
  }
  PetscCall(PetscSynchronizedFlush(MPI_COMM_WORLD, NULL));
  PetscCall(VecRestoreArrayRead(coords, &carray));
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  DM                  networkdm, dmclone;
  PetscMPIInt         rank, size;
  PetscInt            Nsubnet = 2, numVertices[2], numEdges[2], i, j, nv, ne, it_max = 10;
  PetscInt            vStart, vEnd, compkey;
  const PetscInt     *vtx, *edges;
  Vec                 X, F, coords;
  SNES                snes, snes_power, snes_water;
  Mat                 Jac;
  PetscBool           ghost, viewJ = PETSC_FALSE, viewX = PETSC_FALSE, test = PETSC_FALSE, distribute = PETSC_TRUE, flg, printCoord = PETSC_FALSE, viewCSV = PETSC_FALSE;
  UserCtx             user;
  SNESConvergedReason reason;
#if defined(PETSC_USE_LOG)
  PetscLogStage stage[4];
#endif

  /* Power subnetwork */
  UserCtx_Power *appctx_power                    = &user.appctx_power;
  char           pfdata_file[PETSC_MAX_PATH_LEN] = "power/case9.m";
  PFDATA        *pfdata                          = NULL;
  PetscInt       genj, loadj, *edgelist_power = NULL, power_netnum;
  PetscScalar    Sbase = 0.0;

  /* Water subnetwork */
  AppCtx_Water *appctx_water                       = &user.appctx_water;
  WATERDATA    *waterdata                          = NULL;
  char          waterdata_file[PETSC_MAX_PATH_LEN] = "water/sample1.inp";
  PetscInt     *edgelist_water                     = NULL, water_netnum;

  /* Shared vertices between subnetworks */
  PetscInt power_svtx, water_svtx;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, "ex1options", help));
  PetscCallMPI(MPI_Comm_rank(PETSC_COMM_WORLD, &rank));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));

  /* (1) Read Data - Only rank 0 reads the data */
  PetscCall(PetscLogStageRegister("Read Data", &stage[0]));
  PetscCall(PetscLogStagePush(stage[0]));

  for (i = 0; i < Nsubnet; i++) {
    numVertices[i] = 0;
    numEdges[i]    = 0;
  }

  /* All processes READ THE DATA FOR THE FIRST SUBNETWORK: Electric Power Grid */
  /* Used for shared vertex, because currently the coupling info must be available in all processes!!! */
  PetscCall(PetscOptionsGetString(NULL, NULL, "-pfdata", pfdata_file, PETSC_MAX_PATH_LEN - 1, NULL));
  PetscCall(PetscNew(&pfdata));
  PetscCall(PFReadMatPowerData(pfdata, pfdata_file));
  if (rank == 0) PetscCall(PetscPrintf(PETSC_COMM_SELF, "Power network: nb = %" PetscInt_FMT ", ngen = %" PetscInt_FMT ", nload = %" PetscInt_FMT ", nbranch = %" PetscInt_FMT "\n", pfdata->nbus, pfdata->ngen, pfdata->nload, pfdata->nbranch));
  Sbase = pfdata->sbase;
  if (rank == 0) { /* proc[0] will create Electric Power Grid */
    numEdges[0]    = pfdata->nbranch;
    numVertices[0] = pfdata->nbus;

    PetscCall(PetscMalloc1(2 * numEdges[0], &edgelist_power));
    PetscCall(GetListofEdges_Power(pfdata, edgelist_power));
  }
  /* Broadcast power Sbase to all processors */
  PetscCallMPI(MPI_Bcast(&Sbase, 1, MPIU_SCALAR, 0, PETSC_COMM_WORLD));
  appctx_power->Sbase     = Sbase;
  appctx_power->jac_error = PETSC_FALSE;
  /* If external option activated. Introduce error in jacobian */
  PetscCall(PetscOptionsHasName(NULL, NULL, "-jac_error", &appctx_power->jac_error));

  /* All processes READ THE DATA FOR THE SECOND SUBNETWORK: Water */
  /* Used for shared vertex, because currently the coupling info must be available in all processes!!! */
  PetscCall(PetscNew(&waterdata));
  PetscCall(PetscOptionsGetString(NULL, NULL, "-waterdata", waterdata_file, PETSC_MAX_PATH_LEN - 1, NULL));
  PetscCall(WaterReadData(waterdata, waterdata_file));
  if (size == 1 || (size > 1 && rank == 1)) {
    PetscCall(PetscCalloc1(2 * waterdata->nedge, &edgelist_water));
    PetscCall(GetListofEdges_Water(waterdata, edgelist_water));
    numEdges[1]    = waterdata->nedge;
    numVertices[1] = waterdata->nvertex;
  }
  PetscCall(PetscLogStagePop());

  /* (2) Create a network consist of two subnetworks */
  PetscCall(PetscLogStageRegister("Net Setup", &stage[1]));
  PetscCall(PetscLogStagePush(stage[1]));

  PetscCall(PetscOptionsGetBool(NULL, NULL, "-viewCSV", &viewCSV, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-printCoord", &printCoord, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-test", &test, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-distribute", &distribute, NULL));

  /* Create an empty network object */
  PetscCall(DMNetworkCreate(PETSC_COMM_WORLD, &networkdm));

  /* Register the components in the network */
  PetscCall(DMNetworkRegisterComponent(networkdm, "branchstruct", sizeof(struct _p_EDGE_Power), &appctx_power->compkey_branch));
  PetscCall(DMNetworkRegisterComponent(networkdm, "busstruct", sizeof(struct _p_VERTEX_Power), &appctx_power->compkey_bus));
  PetscCall(DMNetworkRegisterComponent(networkdm, "genstruct", sizeof(struct _p_GEN), &appctx_power->compkey_gen));
  PetscCall(DMNetworkRegisterComponent(networkdm, "loadstruct", sizeof(struct _p_LOAD), &appctx_power->compkey_load));

  PetscCall(DMNetworkRegisterComponent(networkdm, "edge_water", sizeof(struct _p_EDGE_Water), &appctx_water->compkey_edge));
  PetscCall(DMNetworkRegisterComponent(networkdm, "vertex_water", sizeof(struct _p_VERTEX_Water), &appctx_water->compkey_vtx));

  PetscCall(PetscSynchronizedPrintf(PETSC_COMM_WORLD, "[%d] Total local nvertices %" PetscInt_FMT " + %" PetscInt_FMT " = %" PetscInt_FMT ", nedges %" PetscInt_FMT " + %" PetscInt_FMT " = %" PetscInt_FMT "\n", rank, numVertices[0], numVertices[1], numVertices[0] + numVertices[1], numEdges[0], numEdges[1], numEdges[0] + numEdges[1]));
  PetscCall(PetscSynchronizedFlush(PETSC_COMM_WORLD, PETSC_STDOUT));

  PetscCall(DMNetworkSetNumSubNetworks(networkdm, PETSC_DECIDE, Nsubnet));
  PetscCall(DMNetworkAddSubnetwork(networkdm, "power", numEdges[0], edgelist_power, &power_netnum));
  PetscCall(DMNetworkAddSubnetwork(networkdm, "water", numEdges[1], edgelist_water, &water_netnum));

  /* vertex subnet[0].4 shares with vertex subnet[1].0 */
  power_svtx = 4;
  water_svtx = 0;
  PetscCall(DMNetworkAddSharedVertices(networkdm, power_netnum, water_netnum, 1, &power_svtx, &water_svtx));

  /* Set up the network layout */
  PetscCall(DMNetworkLayoutSetUp(networkdm));

  /* ADD VARIABLES AND COMPONENTS FOR THE POWER SUBNETWORK */
  genj  = 0;
  loadj = 0;
  PetscCall(DMNetworkGetSubnetwork(networkdm, power_netnum, &nv, &ne, &vtx, &edges));

  for (i = 0; i < ne; i++) PetscCall(DMNetworkAddComponent(networkdm, edges[i], appctx_power->compkey_branch, &pfdata->branch[i], 0));

  for (i = 0; i < nv; i++) {
    PetscCall(DMNetworkIsSharedVertex(networkdm, vtx[i], &flg));
    if (flg) continue;

    PetscCall(DMNetworkAddComponent(networkdm, vtx[i], appctx_power->compkey_bus, &pfdata->bus[i], 2));
    if (pfdata->bus[i].ngen) {
      for (j = 0; j < pfdata->bus[i].ngen; j++) PetscCall(DMNetworkAddComponent(networkdm, vtx[i], appctx_power->compkey_gen, &pfdata->gen[genj++], 0));
    }
    if (pfdata->bus[i].nload) {
      for (j = 0; j < pfdata->bus[i].nload; j++) PetscCall(DMNetworkAddComponent(networkdm, vtx[i], appctx_power->compkey_load, &pfdata->load[loadj++], 0));
    }
  }

  /* ADD VARIABLES AND COMPONENTS FOR THE WATER SUBNETWORK */
  PetscCall(DMNetworkGetSubnetwork(networkdm, water_netnum, &nv, &ne, &vtx, &edges));
  for (i = 0; i < ne; i++) PetscCall(DMNetworkAddComponent(networkdm, edges[i], appctx_water->compkey_edge, &waterdata->edge[i], 0));

  for (i = 0; i < nv; i++) {
    PetscCall(DMNetworkIsSharedVertex(networkdm, vtx[i], &flg));
    if (flg) continue;

    PetscCall(DMNetworkAddComponent(networkdm, vtx[i], appctx_water->compkey_vtx, &waterdata->vertex[i], 1));
  }

  /* ADD VARIABLES AND COMPONENTS AT THE SHARED VERTEX: net[0].4 coupls with net[1].0 -- owning and all ghost ranks of the vertex do this */
  PetscCall(DMNetworkGetSharedVertices(networkdm, &nv, &vtx));
  for (i = 0; i < nv; i++) {
    /* power */
    PetscCall(DMNetworkAddComponent(networkdm, vtx[i], appctx_power->compkey_bus, &pfdata->bus[4], 2));
    /* bus[4] is a load, add its component */
    PetscCall(DMNetworkAddComponent(networkdm, vtx[i], appctx_power->compkey_load, &pfdata->load[0], 0));

    /* water */
    PetscCall(DMNetworkAddComponent(networkdm, vtx[i], appctx_water->compkey_vtx, &waterdata->vertex[0], 1));
  }

  /* Set coordinates for visualization */
  PetscCall(DMSetCoordinateDim(networkdm, 2));
  PetscCall(DMGetCoordinateDM(networkdm, &dmclone));
  PetscCall(DMNetworkGetVertexRange(dmclone, &vStart, &vEnd));
  PetscCall(DMNetworkRegisterComponent(dmclone, "coordinates", 0, &compkey));
  for (i = vStart; i < vEnd; i++) PetscCall(DMNetworkAddComponent(dmclone, i, compkey, NULL, 2));
  PetscCall(DMNetworkFinalizeComponents(dmclone));

  PetscCall(DMCreateLocalVector(dmclone, &coords));
  PetscCall(DMSetCoordinatesLocal(networkdm, coords)); /* set/get coords to/from networkdm */
  PetscCall(CoordinateVecSetUp(networkdm, coords));
  if (printCoord) PetscCall(CoordinatePrint(networkdm));

  /* Set up DM for use */
  PetscCall(DMSetUp(networkdm));

  /* Free user objects */
  PetscCall(PetscFree(edgelist_power));
  PetscCall(PetscFree(pfdata->bus));
  PetscCall(PetscFree(pfdata->gen));
  PetscCall(PetscFree(pfdata->branch));
  PetscCall(PetscFree(pfdata->load));
  PetscCall(PetscFree(pfdata));

  PetscCall(PetscFree(edgelist_water));
  PetscCall(PetscFree(waterdata->vertex));
  PetscCall(PetscFree(waterdata->edge));
  PetscCall(PetscFree(waterdata));

  /* Re-distribute networkdm to multiple processes for better job balance */
  if (distribute) {
    PetscCall(DMNetworkDistribute(&networkdm, 0));

    if (printCoord) PetscCall(CoordinatePrint(networkdm));
    if (viewCSV) { /* CSV View of network with coordinates */
      PetscCall(PetscViewerPushFormat(PETSC_VIEWER_STDOUT_WORLD, PETSC_VIEWER_ASCII_CSV));
      PetscCall(DMView(networkdm, PETSC_VIEWER_STDOUT_WORLD));
      PetscCall(PetscViewerPopFormat(PETSC_VIEWER_STDOUT_WORLD));
    }
  }
  PetscCall(VecDestroy(&coords));

  /* Test DMNetworkGetSubnetwork() and DMNetworkGetSubnetworkSharedVertices() */
  if (test) {
    PetscInt v, gidx;
    PetscCallMPI(MPI_Barrier(PETSC_COMM_WORLD));
    for (i = 0; i < Nsubnet; i++) {
      PetscCall(DMNetworkGetSubnetwork(networkdm, i, &nv, &ne, &vtx, &edges));
      PetscCall(PetscPrintf(PETSC_COMM_SELF, "[%d] After distribute, subnet[%" PetscInt_FMT "] ne %" PetscInt_FMT ", nv %" PetscInt_FMT "\n", rank, i, ne, nv));
      PetscCallMPI(MPI_Barrier(PETSC_COMM_WORLD));

      for (v = 0; v < nv; v++) {
        PetscCall(DMNetworkIsGhostVertex(networkdm, vtx[v], &ghost));
        PetscCall(DMNetworkGetGlobalVertexIndex(networkdm, vtx[v], &gidx));
        PetscCall(PetscPrintf(PETSC_COMM_SELF, "[%d] subnet[%" PetscInt_FMT "] v %" PetscInt_FMT " %" PetscInt_FMT "; ghost %d\n", rank, i, vtx[v], gidx, ghost));
      }
      PetscCallMPI(MPI_Barrier(PETSC_COMM_WORLD));
    }
    PetscCallMPI(MPI_Barrier(PETSC_COMM_WORLD));

    PetscCall(DMNetworkGetSharedVertices(networkdm, &nv, &vtx));
    PetscCall(PetscPrintf(PETSC_COMM_SELF, "[%d] After distribute, num of shared vertices nsv = %" PetscInt_FMT "\n", rank, nv));
    for (v = 0; v < nv; v++) {
      PetscCall(DMNetworkGetGlobalVertexIndex(networkdm, vtx[v], &gidx));
      PetscCall(PetscPrintf(PETSC_COMM_SELF, "[%d] sv %" PetscInt_FMT ", gidx=%" PetscInt_FMT "\n", rank, vtx[v], gidx));
    }
    PetscCallMPI(MPI_Barrier(PETSC_COMM_WORLD));
  }

  /* Create solution vector X */
  PetscCall(DMCreateGlobalVector(networkdm, &X));
  PetscCall(VecDuplicate(X, &F));
  PetscCall(DMGetLocalVector(networkdm, &user.localXold));
  PetscCall(PetscLogStagePop());

  /* (3) Setup Solvers */
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-viewJ", &viewJ, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-viewX", &viewX, NULL));

  PetscCall(PetscLogStageRegister("SNES Setup", &stage[2]));
  PetscCall(PetscLogStagePush(stage[2]));

  PetscCall(SetInitialGuess(networkdm, X, &user));

  /* Create coupled snes */
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "SNES_coupled setup ......\n"));
  user.subsnes_id = Nsubnet;
  PetscCall(SNESCreate(PETSC_COMM_WORLD, &snes));
  PetscCall(SNESSetDM(snes, networkdm));
  PetscCall(SNESSetOptionsPrefix(snes, "coupled_"));
  PetscCall(SNESSetFunction(snes, F, FormFunction, &user));
  PetscCall(SNESMonitorSet(snes, UserMonitor, &user, NULL));
  PetscCall(SNESSetFromOptions(snes));

  if (viewJ) {
    /* View Jac structure */
    PetscCall(SNESGetJacobian(snes, &Jac, NULL, NULL, NULL));
    PetscCall(MatView(Jac, PETSC_VIEWER_DRAW_WORLD));
  }

  if (viewX) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Solution:\n"));
    PetscCall(VecView(X, PETSC_VIEWER_STDOUT_WORLD));
  }

  if (viewJ) {
    /* View assembled Jac */
    PetscCall(MatView(Jac, PETSC_VIEWER_DRAW_WORLD));
  }

  /* Create snes_power */
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "SNES_power setup ......\n"));

  user.subsnes_id = 0;
  PetscCall(SNESCreate(PETSC_COMM_WORLD, &snes_power));
  PetscCall(SNESSetDM(snes_power, networkdm));
  PetscCall(SNESSetOptionsPrefix(snes_power, "power_"));
  PetscCall(SNESSetFunction(snes_power, F, FormFunction, &user));
  PetscCall(SNESMonitorSet(snes_power, UserMonitor, &user, NULL));

  /* Use user-provide Jacobian */
  PetscCall(DMCreateMatrix(networkdm, &Jac));
  PetscCall(SNESSetJacobian(snes_power, Jac, Jac, FormJacobian_subPower, &user));
  PetscCall(SNESSetFromOptions(snes_power));

  if (viewX) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Power Solution:\n"));
    PetscCall(VecView(X, PETSC_VIEWER_STDOUT_WORLD));
  }
  if (viewJ) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Power Jac:\n"));
    PetscCall(SNESGetJacobian(snes_power, &Jac, NULL, NULL, NULL));
    PetscCall(MatView(Jac, PETSC_VIEWER_DRAW_WORLD));
    /* PetscCall(MatView(Jac,PETSC_VIEWER_STDOUT_WORLD)); */
  }

  /* Create snes_water */
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "SNES_water setup......\n"));

  user.subsnes_id = 1;
  PetscCall(SNESCreate(PETSC_COMM_WORLD, &snes_water));
  PetscCall(SNESSetDM(snes_water, networkdm));
  PetscCall(SNESSetOptionsPrefix(snes_water, "water_"));
  PetscCall(SNESSetFunction(snes_water, F, FormFunction, &user));
  PetscCall(SNESMonitorSet(snes_water, UserMonitor, &user, NULL));
  PetscCall(SNESSetFromOptions(snes_water));

  if (viewX) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Water Solution:\n"));
    PetscCall(VecView(X, PETSC_VIEWER_STDOUT_WORLD));
  }
  PetscCall(PetscLogStagePop());

  /* (4) Solve */
  PetscCall(PetscLogStageRegister("SNES Solve", &stage[3]));
  PetscCall(PetscLogStagePush(stage[3]));
  user.it = 0;
  reason  = SNES_DIVERGED_DTOL;
  while (user.it < it_max && (PetscInt)reason < 0) {
#if 0
    user.subsnes_id = 0;
    PetscCall(SNESSolve(snes_power,NULL,X));

    user.subsnes_id = 1;
    PetscCall(SNESSolve(snes_water,NULL,X));
#endif
    user.subsnes_id = Nsubnet;
    PetscCall(SNESSolve(snes, NULL, X));

    PetscCall(SNESGetConvergedReason(snes, &reason));
    user.it++;
  }
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Coupled_SNES converged in %" PetscInt_FMT " iterations\n", user.it));
  if (viewX) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Final Solution:\n"));
    PetscCall(VecView(X, PETSC_VIEWER_STDOUT_WORLD));
  }
  PetscCall(PetscLogStagePop());

  /* Free objects */
  /* -------------*/
  PetscCall(VecDestroy(&X));
  PetscCall(VecDestroy(&F));
  PetscCall(DMRestoreLocalVector(networkdm, &user.localXold));

  PetscCall(SNESDestroy(&snes));
  PetscCall(MatDestroy(&Jac));
  PetscCall(SNESDestroy(&snes_power));
  PetscCall(SNESDestroy(&snes_water));

  PetscCall(DMDestroy(&networkdm));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   build:
     requires: !complex double defined(PETSC_HAVE_ATTRIBUTEALIGNED)
     depends: power/PFReadData.c power/pffunctions.c water/waterreaddata.c water/waterfunctions.c

   test:
      args: -coupled_snes_converged_reason -options_left no -dmnetwork_view
      localrunfiles: ex1options power/case9.m water/sample1.inp
      output_file: output/ex1.out

   test:
      suffix: 2
      nsize: 3
      args: -coupled_snes_converged_reason -options_left no -petscpartitioner_type parmetis
      localrunfiles: ex1options power/case9.m water/sample1.inp
      output_file: output/ex1_2.out
      requires: parmetis

   test:
      suffix: 3
      nsize: 3
      args: -coupled_snes_converged_reason -options_left no -distribute false
      localrunfiles: ex1options power/case9.m water/sample1.inp
      output_file: output/ex1_2.out

   test:
      suffix: 4
      nsize: 4
      args: -coupled_snes_converged_reason -options_left no -petscpartitioner_type simple -dmnetwork_view -dmnetwork_view_distributed
      localrunfiles: ex1options power/case9.m water/sample1.inp
      output_file: output/ex1_4.out

   test:
      suffix: 5
      args: -coupled_snes_converged_reason -options_left no -viewCSV
      localrunfiles: ex1options power/case9.m water/sample1.inp
      output_file: output/ex1_5.out

   test:
      suffix: 6
      nsize: 3
      args: -coupled_snes_converged_reason -options_left no -petscpartitioner_type parmetis -dmnetwork_view_distributed draw:null
      localrunfiles: ex1options power/case9.m water/sample1.inp
      output_file: output/ex1_2.out
      requires: parmetis

TEST*/

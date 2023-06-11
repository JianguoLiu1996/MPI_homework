
/*
  Code for manipulating distributed regular arrays in parallel.
*/

#include <petsc/private/dmdaimpl.h> /*I   "petscdmda.h"   I*/

#if defined(PETSC_HAVE_MATLAB)
  #include <mat.h> /* MATLAB include file */

PetscErrorCode DMView_DA_Matlab(DM da, PetscViewer viewer)
{
  PetscMPIInt     rank;
  PetscInt        dim, m, n, p, dof, swidth;
  DMDAStencilType stencil;
  DMBoundaryType  bx, by, bz;
  mxArray        *mx;
  const char     *fnames[] = {"dimension", "m", "n", "p", "dof", "stencil_width", "bx", "by", "bz", "stencil_type"};

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)da), &rank));
  if (rank == 0) {
    PetscCall(DMDAGetInfo(da, &dim, &m, &n, &p, 0, 0, 0, &dof, &swidth, &bx, &by, &bz, &stencil));
    mx = mxCreateStructMatrix(1, 1, 8, (const char **)fnames);
    PetscCheck(mx, PETSC_COMM_SELF, PETSC_ERR_LIB, "Unable to generate MATLAB struct array to hold DMDA information");
    mxSetFieldByNumber(mx, 0, 0, mxCreateDoubleScalar((double)dim));
    mxSetFieldByNumber(mx, 0, 1, mxCreateDoubleScalar((double)m));
    mxSetFieldByNumber(mx, 0, 2, mxCreateDoubleScalar((double)n));
    mxSetFieldByNumber(mx, 0, 3, mxCreateDoubleScalar((double)p));
    mxSetFieldByNumber(mx, 0, 4, mxCreateDoubleScalar((double)dof));
    mxSetFieldByNumber(mx, 0, 5, mxCreateDoubleScalar((double)swidth));
    mxSetFieldByNumber(mx, 0, 6, mxCreateDoubleScalar((double)bx));
    mxSetFieldByNumber(mx, 0, 7, mxCreateDoubleScalar((double)by));
    mxSetFieldByNumber(mx, 0, 8, mxCreateDoubleScalar((double)bz));
    mxSetFieldByNumber(mx, 0, 9, mxCreateDoubleScalar((double)stencil));
    PetscCall(PetscObjectName((PetscObject)da));
    PetscCall(PetscViewerMatlabPutVariable(viewer, ((PetscObject)da)->name, mx));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}
#endif

PetscErrorCode DMView_DA_Binary(DM da, PetscViewer viewer)
{
  PetscMPIInt     rank;
  PetscInt        dim, m, n, p, dof, swidth, M, N, P;
  DMDAStencilType stencil;
  DMBoundaryType  bx, by, bz;
  MPI_Comm        comm;
  PetscBool       coors = PETSC_FALSE;
  Vec             coordinates;

  PetscFunctionBegin;
  PetscCall(PetscObjectGetComm((PetscObject)da, &comm));

  PetscCall(DMDAGetInfo(da, &dim, &m, &n, &p, &M, &N, &P, &dof, &swidth, &bx, &by, &bz, &stencil));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  PetscCall(DMGetCoordinates(da, &coordinates));
  if (rank == 0) {
    PetscCall(PetscViewerBinaryWrite(viewer, &dim, 1, PETSC_INT));
    PetscCall(PetscViewerBinaryWrite(viewer, &m, 1, PETSC_INT));
    PetscCall(PetscViewerBinaryWrite(viewer, &n, 1, PETSC_INT));
    PetscCall(PetscViewerBinaryWrite(viewer, &p, 1, PETSC_INT));
    PetscCall(PetscViewerBinaryWrite(viewer, &dof, 1, PETSC_INT));
    PetscCall(PetscViewerBinaryWrite(viewer, &swidth, 1, PETSC_INT));
    PetscCall(PetscViewerBinaryWrite(viewer, &bx, 1, PETSC_ENUM));
    PetscCall(PetscViewerBinaryWrite(viewer, &by, 1, PETSC_ENUM));
    PetscCall(PetscViewerBinaryWrite(viewer, &bz, 1, PETSC_ENUM));
    PetscCall(PetscViewerBinaryWrite(viewer, &stencil, 1, PETSC_ENUM));
    if (coordinates) coors = PETSC_TRUE;
    PetscCall(PetscViewerBinaryWrite(viewer, &coors, 1, PETSC_BOOL));
  }

  /* save the coordinates if they exist to disk (in the natural ordering) */
  if (coordinates) PetscCall(VecView(coordinates, viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMView_DA_VTK(DM da, PetscViewer viewer)
{
  Vec      coordinates;
  PetscInt dim, dof, M = 0, N = 0, P = 0;

  PetscFunctionBegin;
  PetscCall(DMGetCoordinates(da, &coordinates));
  PetscCall(DMDAGetInfo(da, &dim, &M, &N, &P, NULL, NULL, NULL, &dof, NULL, NULL, NULL, NULL, NULL));
  PetscCheck(coordinates, PetscObjectComm((PetscObject)da), PETSC_ERR_SUP, "VTK output requires DMDA coordinates.");
  /* Write Header */
  PetscCall(PetscViewerASCIIPrintf(viewer, "# vtk DataFile Version 2.0\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "Structured Mesh Example\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "ASCII\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "DATASET STRUCTURED_GRID\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "DIMENSIONS %" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT "\n", M, N, P));
  PetscCall(PetscViewerASCIIPrintf(viewer, "POINTS %" PetscInt_FMT " double\n", M * N * P));
  if (coordinates) {
    DM  dac;
    Vec natural;

    PetscCall(DMGetCoordinateDM(da, &dac));
    PetscCall(DMDACreateNaturalVector(dac, &natural));
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)natural, "coor_"));
    PetscCall(DMDAGlobalToNaturalBegin(dac, coordinates, INSERT_VALUES, natural));
    PetscCall(DMDAGlobalToNaturalEnd(dac, coordinates, INSERT_VALUES, natural));
    PetscCall(PetscViewerPushFormat(viewer, PETSC_VIEWER_ASCII_VTK_COORDS_DEPRECATED));
    PetscCall(VecView(natural, viewer));
    PetscCall(PetscViewerPopFormat(viewer));
    PetscCall(VecDestroy(&natural));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMDAGetInfo - Gets information about a given distributed array.

   Not Collective

   Input Parameter:
.  da - the distributed array

   Output Parameters:
+  dim      - dimension of the distributed array (1, 2, or 3)
.  M        - global dimension in first direction of the array
.  N        - global dimension in second direction of the array
.  P        - global dimension in third direction of the array
.  m        - corresponding number of procs in first dimension
.  n        - corresponding number of procs in second dimension
.  p        - corresponding number of procs in third dimension
.  dof      - number of degrees of freedom per node
.  s        - stencil width
.  bx       - type of ghost nodes at boundary in first dimension
.  by       - type of ghost nodes at boundary in second dimension
.  bz       - type of ghost nodes at boundary in third dimension
-  st       - stencil type, either `DMDA_STENCIL_STAR` or `DMDA_STENCIL_BOX`

   Level: beginner

   Note:
   Use NULL (NULL_INTEGER in Fortran) in place of any output parameter that is not of interest.

.seealso: `DM`, `DMDA`, `DMView()`, `DMDAGetCorners()`, `DMDAGetLocalInfo()`
@*/
PetscErrorCode DMDAGetInfo(DM da, PetscInt *dim, PetscInt *M, PetscInt *N, PetscInt *P, PetscInt *m, PetscInt *n, PetscInt *p, PetscInt *dof, PetscInt *s, DMBoundaryType *bx, DMBoundaryType *by, DMBoundaryType *bz, DMDAStencilType *st)
{
  DM_DA *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  if (dim) *dim = da->dim;
  if (M) {
    if (dd->Mo < 0) *M = dd->M;
    else *M = dd->Mo;
  }
  if (N) {
    if (dd->No < 0) *N = dd->N;
    else *N = dd->No;
  }
  if (P) {
    if (dd->Po < 0) *P = dd->P;
    else *P = dd->Po;
  }
  if (m) *m = dd->m;
  if (n) *n = dd->n;
  if (p) *p = dd->p;
  if (dof) *dof = dd->w;
  if (s) *s = dd->s;
  if (bx) *bx = dd->bx;
  if (by) *by = dd->by;
  if (bz) *bz = dd->bz;
  if (st) *st = dd->stencil_type;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMDAGetLocalInfo - Gets information about a given distributed array and this processors location in it

   Not Collective

   Input Parameter:
.  da - the distributed array

   Output Parameters:
.  dainfo - structure containing the information

   Level: beginner

   Note:
    See `DMDALocalInfo` for the information that is returned

.seealso: `DM`, `DMDA`, `DMDAGetInfo()`, `DMDAGetCorners()`, `DMDALocalInfo`
@*/
PetscErrorCode DMDAGetLocalInfo(DM da, DMDALocalInfo *info)
{
  PetscInt w;
  DM_DA   *dd = (DM_DA *)da->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecificType(da, DM_CLASSID, 1, DMDA);
  PetscValidPointer(info, 2);
  info->da  = da;
  info->dim = da->dim;
  if (dd->Mo < 0) info->mx = dd->M;
  else info->mx = dd->Mo;
  if (dd->No < 0) info->my = dd->N;
  else info->my = dd->No;
  if (dd->Po < 0) info->mz = dd->P;
  else info->mz = dd->Po;
  info->dof = dd->w;
  info->sw  = dd->s;
  info->bx  = dd->bx;
  info->by  = dd->by;
  info->bz  = dd->bz;
  info->st  = dd->stencil_type;

  /* since the xs, xe ... have all been multiplied by the number of degrees
     of freedom per cell, w = dd->w, we divide that out before returning.*/
  w        = dd->w;
  info->xs = dd->xs / w + dd->xo;
  info->xm = (dd->xe - dd->xs) / w;
  /* the y and z have NOT been multiplied by w */
  info->ys = dd->ys + dd->yo;
  info->ym = (dd->ye - dd->ys);
  info->zs = dd->zs + dd->zo;
  info->zm = (dd->ze - dd->zs);

  info->gxs = dd->Xs / w + dd->xo;
  info->gxm = (dd->Xe - dd->Xs) / w;
  /* the y and z have NOT been multiplied by w */
  info->gys = dd->Ys + dd->yo;
  info->gym = (dd->Ye - dd->Ys);
  info->gzs = dd->Zs + dd->zo;
  info->gzm = (dd->Ze - dd->Zs);
  PetscFunctionReturn(PETSC_SUCCESS);
}

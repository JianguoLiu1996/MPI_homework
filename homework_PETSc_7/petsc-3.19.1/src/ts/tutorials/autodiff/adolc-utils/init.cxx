#include <petscdmda.h>
#include <adolc/adalloc.h>

/*
   REQUIRES configuration of PETSc with option --download-adolc.

   For documentation on ADOL-C, see
     $PETSC_ARCH/externalpackages/ADOL-C-2.6.0/ADOL-C/doc/adolc-manual.pdf
*/

/*
  Wrapper function for allocating contiguous memory in a 2d array

  Input parameters:
  m,n - number of rows and columns of array, respectively

  Output parameter:
  A   - pointer to array for which memory is allocated

  Note: Only arrays of doubles are currently accounted for in ADOL-C's myalloc2 function.
*/
template <class T>
PetscErrorCode AdolcMalloc2(PetscInt m, PetscInt n, T **A[])
{
  PetscFunctionBegin;
  *A = myalloc2(m, n);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Wrapper function for freeing memory allocated with AdolcMalloc2

  Input parameter:
  A - array to free memory of

  Note: Only arrays of doubles are currently accounted for in ADOL-C's myfree2 function.
*/
template <class T>
PetscErrorCode AdolcFree2(T **A)
{
  PetscFunctionBegin;
  myfree2(A);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Shift indices in an array of type T to endow it with ghost points.
  (e.g. This works for arrays of adoubles or arrays (of structs) thereof.)

  Input parameters:
  da   - distributed array upon which variables are defined
  cgs  - contiguously allocated 1-array with as many entries as there are
         interior and ghost points, in total

  Output parameter:
  array - contiguously allocated array of the appropriate dimension with
          ghost points, pointing to the 1-array
*/
template <class T>
PetscErrorCode GiveGhostPoints(DM da, T *cgs, void *array)
{
  PetscInt dim;

  PetscFunctionBegin;
  PetscCall(DMDAGetInfo(da, &dim, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
  if (dim == 1) {
    PetscCall(GiveGhostPoints1d(da, (T **)array));
  } else if (dim == 2) {
    PetscCall(GiveGhostPoints2d(da, cgs, (T ***)array));
  } else PetscCheck(dim != 3, PetscObjectComm((PetscObject)da), PETSC_ERR_SUP, "GiveGhostPoints3d not yet implemented"); // TODO
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Shift indices in a 1-array of type T to endow it with ghost points.
  (e.g. This works for arrays of adoubles or arrays (of structs) thereof.)

  Input parameters:
  da  - distributed array upon which variables are defined

  Output parameter:
  a1d - contiguously allocated 1-array
*/
template <class T>
PetscErrorCode GiveGhostPoints1d(DM da, T *a1d[])
{
  PetscInt gxs;

  PetscFunctionBegin;
  PetscCall(DMDAGetGhostCorners(da, &gxs, NULL, NULL, NULL, NULL, NULL));
  *a1d -= gxs;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Shift indices in a 2-array of type T to endow it with ghost points.
  (e.g. This works for arrays of adoubles or arrays (of structs) thereof.)

  Input parameters:
  da  - distributed array upon which variables are defined
  cgs - contiguously allocated 1-array with as many entries as there are
        interior and ghost points, in total

  Output parameter:
  a2d - contiguously allocated 2-array with ghost points, pointing to the
        1-array
*/
template <class T>
PetscErrorCode GiveGhostPoints2d(DM da, T *cgs, T **a2d[])
{
  PetscInt gxs, gys, gxm, gym;

  PetscFunctionBegin;
  PetscCall(DMDAGetGhostCorners(da, &gxs, &gys, NULL, &gxm, &gym, NULL));
  for (PetscInt j = 0; j < gym; j++) (*a2d)[j] = cgs + j * gxm - gxs;
  *a2d -= gys;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Create a rectangular sub-identity of the m x m identity matrix, as an array.

  Input parameters:
  n - number of (adjacent) rows to take in slice
  s - starting row index

  Output parameter:
  S - resulting n x m submatrix
*/
template <class T>
PetscErrorCode Subidentity(PetscInt n, PetscInt s, T **S)
{
  PetscInt i;

  PetscFunctionBegin;
  for (i = 0; i < n; i++) S[i][i + s] = 1.;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Create an identity matrix, as an array.

  Input parameter:
  n - number of rows/columns
  I - n x n array with memory pre-allocated
*/
template <class T>
PetscErrorCode Identity(PetscInt n, T **I)
{
  PetscFunctionBegin;
  PetscCall(Subidentity(n, 0, I));
  PetscFunctionReturn(PETSC_SUCCESS);
}

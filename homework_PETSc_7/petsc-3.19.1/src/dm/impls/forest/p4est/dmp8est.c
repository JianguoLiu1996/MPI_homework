#include <petscsys.h>
#if defined(PETSC_HAVE_MPIUNI)
  #undef MPI_SUCCESS
#endif
#if defined(PETSC_HAVE_P4EST)
  #include <p4est_to_p8est.h>
#endif
#if defined(PETSC_HAVE_MPIUNI)
  #define MPI_SUCCESS 0
#endif

static const PetscInt PetscFaceToP4estFace[6]  = {4, 5, 2, 3, 1, 0};
static const PetscInt P4estFaceToPetscFace[6]  = {5, 4, 2, 3, 0, 1};
static const PetscInt P4estFaceToPetscOrnt[6]  = {-2, 0, 0, -3, -2, 0};
static const PetscInt PetscEdgeToP4estEdge[12] = {4, 1, 5, 0, 2, 7, 3, 6, 9, 8, 10, 11};
static const PetscInt P4estEdgeToPetscEdge[12] = {3, 1, 4, 6, 0, 2, 7, 5, 9, 8, 10, 11};
static const PetscInt P4estEdgeToPetscOrnt[12] = {-1, 0, 0, -1, 0, -1, -1, 0, -1, 0, 0, -1};
static const PetscInt PetscVertToP4estVert[8]  = {0, 2, 3, 1, 4, 5, 7, 6};
static const PetscInt P4estVertToPetscVert[8]  = {0, 3, 1, 2, 4, 5, 7, 6};

#define DMPFOREST DMP8EST

#define _append_pforest(a)   PetscConcat_(a, _p8est)
#define _infix_pforest(a, b) PetscConcat(_append_pforest(a), b)
#include "pforest.h"

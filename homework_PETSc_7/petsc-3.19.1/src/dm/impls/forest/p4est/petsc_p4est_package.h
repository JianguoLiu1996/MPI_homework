#ifndef PETSC_P4EST_PACKAGE_H
#define PETSC_P4EST_PACKAGE_H
#include <petscsys.h>
#if defined(PETSC_HAVE_MPIUNI)
  #undef MPI_SUCCESS
#endif
#include <p4est_base.h>
#if defined(PETSC_HAVE_MPIUNI)
  #define MPI_SUCCESS 0
#endif

#if defined(PETSC_HAVE_SETJMP_H) && defined(PETSC_USE_DEBUG)
  #include <setjmp.h>
PETSC_INTERN jmp_buf PetscScJumpBuf;

  #define PetscCallP4est(func, args) \
    do { \
      if (setjmp(PetscScJumpBuf)) { \
        return PetscError(PETSC_COMM_SELF, __LINE__, PETSC_FUNCTION_NAME, __FILE__, PETSC_ERR_LIB, PETSC_ERROR_REPEAT, "Error in p4est/libsc call %s()", #func); \
      } else { \
        PetscStackPushExternal(#func); \
        func args; \
        PetscStackPop; \
      } \
    } while (0)
  #define PetscCallP4estReturn(ret, func, args) \
    do { \
      if (setjmp(PetscScJumpBuf)) { \
        return PetscError(PETSC_COMM_SELF, __LINE__, PETSC_FUNCTION_NAME, __FILE__, PETSC_ERR_LIB, PETSC_ERROR_REPEAT, "Error in p4est/libsc call %s()", #func); \
      } else { \
        PetscStackPushExternal(#func); \
        ret = func args; \
        PetscStackPop; \
      } \
    } while (0)
#else
  #define PetscCallP4est(func, args) \
    do { \
      PetscStackPushExternal(#func); \
      func args; \
      PetscStackPop; \
    } while (0)
  #define PetscCallP4estReturn(ret, func, args) \
    do { \
      PetscStackPushExternal(#func); \
      ret = func args; \
      PetscStackPop; \
    } while (0)
#endif

#if defined(P4EST_ENABLE_DEBUG)
  #define PETSC_P4EST_ASSERT(x) P4EST_ASSERT(x)
#else
  #define PETSC_P4EST_ASSERT(x) (void)(x)
#endif

PETSC_EXTERN PetscErrorCode PetscP4estInitialize(void);

static inline PetscErrorCode P4estLocidxCast(PetscInt a, p4est_locidx_t *b)
{
  PetscFunctionBegin;
  *b = (p4est_locidx_t)(a);
#if defined(PETSC_USE_64BIT_INDICES)
  PetscCheck((a) <= P4EST_LOCIDX_MAX, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Index to large for p4est_locidx_t");
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode P4estTopidxCast(PetscInt a, p4est_topidx_t *b)
{
  PetscFunctionBegin;
  *b = (p4est_topidx_t)(a);
#if defined(PETSC_USE_64BIT_INDICES)
  PetscCheck((a) <= P4EST_TOPIDX_MAX, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Index to large for p4est_topidx_t");
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

#endif

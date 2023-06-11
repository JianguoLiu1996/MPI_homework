#ifndef _PETSCHYPRE_H
#define _PETSCHYPRE_H

#include <petscsys.h>
#include <petscpkg_version.h>
#include <HYPRE_config.h>
#include <HYPRE_utilities.h>

/* from version 2.16 on, HYPRE_BigInt is 64 bit for 64bit installations
   and 32 bit for 32bit installations -> not the best name for a variable */
#if PETSC_PKG_HYPRE_VERSION_LT(2, 16, 0)
typedef PetscInt HYPRE_BigInt;
#endif

#if defined(HYPRE_BIGINT) || defined(HYPRE_MIXEDINT)
  #define PetscHYPRE_BigInt_FMT "lld"
  #ifdef __cplusplus /* make sure our format specifiers line up */
    #include <type_traits>
static_assert(std::is_same<HYPRE_BigInt, long long int>::value, "");
  #endif
#else
  #define PetscHYPRE_BigInt_FMT "d"
  #ifdef __cplusplus /* make sure our format specifiers line up */
    #include <type_traits>
static_assert(std::is_same<HYPRE_BigInt, int>::value, "");
  #endif
#endif

/*
  With scalar type == real, HYPRE_Complex == PetscScalar;
  With scalar type == complex,  HYPRE_Complex is double __complex__ while PetscScalar may be std::complex<double>
*/
static inline PetscErrorCode PetscHYPREScalarCast(PetscScalar a, HYPRE_Complex *b)
{
  PetscFunctionBegin;
#if defined(HYPRE_COMPLEX)
  ((PetscReal *)b)[0] = PetscRealPart(a);
  ((PetscReal *)b)[1] = PetscImaginaryPart(a);
#else
  *b = a;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}
#endif

#if PETSC_PKG_HYPRE_VERSION_LT(2, 19, 0)
typedef int HYPRE_MemoryLocation;
#define hypre_IJVectorMemoryLocation(a) 0
#define hypre_IJMatrixMemoryLocation(a) 0
#endif

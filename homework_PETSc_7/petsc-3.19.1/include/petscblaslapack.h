/*
  This file dispatches between various header files for blas/lapack distributions to handle the name mangling.
  It also provides C prototypes for all the BLAS/LAPACK functions that PETSc uses

  This is not included automatically by petscsys.h because some external packages include their own prototypes for
  certain BLAS/LAPACK functions that conflict with the ones given here. Hence this should only be included when needed.

  The BLAS/LAPACK name mangling is almost (but not always) the same as the Fortran mangling; and exists even if there is
  not Fortran compiler.

  PETSC_BLASLAPACK_UNDERSCORE BLAS/LAPACK function have an underscore at the end of each function name
  PETSC_BLASLAPACK_CAPS BLAS/LAPACK function names are all in capital letters
  PETSC_BLASLAPACK_C BLAS/LAPACK function names have no mangling

  PETSC_BLASLAPACK_SINGLEISDOUBLE - for Cray systems where the BLAS/LAPACK single precision (i.e. Fortran single precision is actually 64 bits)
                                    old Cray vector machines used to be this way, it is is not clear if any exist now.

  PetscBLASInt is almost always 32 bit integers but can be 64 bit integers for certain usages of MKL and OpenBLAS BLAS/LAPACK libraries

*/
#ifndef _BLASLAPACK_H
#define _BLASLAPACK_H

#include <petscsys.h>
#if defined(__cplusplus)
  #define BLAS_EXTERN extern "C"
#else
  #define BLAS_EXTERN extern
#endif

/* SUBMANSEC = Sys */

/*MC
    PetscCallBLAS - Calls a BLAS or LAPACK routine and catches exceptions

    Not Collective

    Synopsis:
   #include <petscsys.h>
   void PetscCallBLAS(char *name,routine)

  Input Parameters:
+   name - string that gives the name of the function being called
-   routine - actual call to the routine including its arguments

   Level: developer

   Developer Note:
   This is so that when a BLAS/LAPACK routine results in a crash or corrupts memory, they get blamed instead of PETSc.

.seealso: `PetscCall()`, `PetscStackPushNoCheck()`, `PetscStackPush()`, `PetscCallExternal()`, `PetscStackCallExternalVoid()`
M*/
#define PetscCallBLAS(name, routine) \
  do { \
    PetscStackPushExternal(name); \
    routine; \
    PetscStackPop; \
  } while (0)

static inline void PetscMissingLapack(const char *fname, ...)
{
  SETERRABORT(PETSC_COMM_SELF, PETSC_ERR_SUP, "%s - Lapack routine is unavailable.", fname);
}

#include <petscblaslapack_mangle.h>

BLAS_EXTERN void LAPACKgetrf_(const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKREALgetrf_(const PetscBLASInt *, const PetscBLASInt *, PetscReal *, const PetscBLASInt *, PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKgetri_(const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKREALgetri_(const PetscBLASInt *, PetscReal *, const PetscBLASInt *, const PetscBLASInt *, PetscReal *, const PetscBLASInt *, PetscBLASInt *);
#if !defined(PETSC_MISSING_LAPACK_ORGQR)
BLAS_EXTERN void LAPACKorgqr_(const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, const PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
#else
  #define LAPACKorgqr_(a, b, c, d, e, f, g, h, i) PetscMissingLapack("ORGQR", a, b, c, d, e, f, g, h, i)
#endif
BLAS_EXTERN void LAPACKgeqrf_(const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
#if defined(PETSC_USE_REAL_SINGLE) && defined(PETSC_BLASLAPACK_SNRM2_RETURNS_DOUBLE)
BLAS_EXTERN double BLASnrm2_(const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *);
#else
BLAS_EXTERN PetscReal BLASnrm2_(const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *);
#endif
BLAS_EXTERN void BLASscal_(const PetscBLASInt *, const PetscScalar *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void BLAScopy_(const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void BLASswap_(const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void BLASaxpy_(const PetscBLASInt *, const PetscScalar *, const PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *);
#if defined(PETSC_USE_REAL_SINGLE) && defined(PETSC_BLASLAPACK_SNRM2_RETURNS_DOUBLE)
BLAS_EXTERN double BLASasum_(const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *);
#else
BLAS_EXTERN PetscReal BLASasum_(const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *);
#endif
BLAS_EXTERN void LAPACKpttrf_(const PetscBLASInt *, PetscReal *, PetscScalar *, PetscBLASInt *);
#if !defined(PETSC_MISSING_LAPACK_STEIN)
BLAS_EXTERN void LAPACKstein_(const PetscBLASInt *, const PetscReal *, const PetscReal *, const PetscBLASInt *, const PetscReal *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *, PetscBLASInt *, PetscBLASInt *);
#else
  #define LAPACKstein_(a, b, c, d, e, f, g, h, i, j, k, l, m) PetscMissingLapack("STEIN", a, b, c, d, e, f, g, h, i, j, k, l)
#endif
BLAS_EXTERN void LAPACKgesv_(const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);

BLAS_EXTERN void LAPACKpotrf_(const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKpotri_(const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKpotrs_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKsytrf_(const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKsytrs_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
#if !defined(PETSC_MISSING_LAPACK_SYTRI)
BLAS_EXTERN void LAPACKsytri_(const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, PetscBLASInt *);
#else
  #define LAPACKsytri_(a, b, c, d, e, f, g) PetscMissingLapack("SYTRI", a, b, c, d, e, f, g)
#endif
BLAS_EXTERN void BLASsyrk_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void BLASsyr2k_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void BLASgemv_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void LAPACKgetrs_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void BLAStrmv_(const char *, const char *, const char *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void BLASgemm_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void BLASREALgemm_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, const PetscReal *, const PetscBLASInt *, const PetscReal *, const PetscBLASInt *, const PetscReal *, PetscReal *, const PetscBLASInt *);
BLAS_EXTERN void BLASsymm_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN void BLAStrsm_(const char *, const char *, const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *);
#if !defined(PETSC_MISSING_LAPACK_ORMQR)
BLAS_EXTERN void LAPACKormqr_(const char *, const char *, PetscBLASInt *, PetscBLASInt *, PetscBLASInt *, PetscScalar *, PetscBLASInt *, PetscScalar *, PetscScalar *, PetscBLASInt *, PetscScalar *, PetscBLASInt *, PetscBLASInt *);
#else
  #define LAPACKormqr_(a, b, c, d, e, f, g, h, i, j, k, l, m) PetscMissingLapack("ORMQR", a, b, c, d, e, f, g, h, i, j, k, l, m)
#endif
#if !defined(PETSC_MISSING_LAPACK_STEGR)
BLAS_EXTERN void LAPACKstegr_(const char *, const char *, const PetscBLASInt *, PetscReal *, PetscReal *, const PetscReal *, const PetscReal *, const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscReal *, const PetscBLASInt *, PetscBLASInt *, const PetscBLASInt *, PetscBLASInt *);
#else
  #define LAPACKstegr_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) PetscMissingLapack("STEGR", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t)
#endif
#if !defined(PETSC_MISSING_LAPACK_STEQR)
BLAS_EXTERN void LAPACKsteqr_(const char *, const PetscBLASInt *, PetscReal *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *);
BLAS_EXTERN void LAPACKREALsteqr_(const char *, const PetscBLASInt *, PetscReal *, PetscReal *, PetscReal *, const PetscBLASInt *, PetscReal *, PetscBLASInt *);
#else
  #define LAPACKsteqr_(a, b, c, d, e, f, g, h)     PetscMissingLapack("STEQR", a, b, c, d, e, f, g, h)
  #define LAPACKREALsteqr_(a, b, c, d, e, f, g, h) PetscMissingLapack("STEQR", a, b, c, d, e, f, g, h)
#endif
#if !defined(PETSC_MISSING_LAPACK_HGEQZ)
BLAS_EXTERN void LAPACKhgeqz_(const char *, const char *, const char *, PetscBLASInt *, PetscBLASInt *, PetscBLASInt *, PetscScalar *, PetscBLASInt *, PetscScalar *, PetscBLASInt *, PetscScalar *, PetscScalar *, PetscScalar *, PetscScalar *, PetscBLASInt *, PetscScalar *, PetscBLASInt *, PetscScalar *, PetscBLASInt *, PetscBLASInt *);
#else
  #define LAPACKhgeqz_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) PetscMissingLapack("HGEQZ", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t)
#endif
#if !defined(PETSC_MISSING_LAPACK_TRTRS)
BLAS_EXTERN void LAPACKtrtrs_(const char *, const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
#else
  #define LAPACKtrtrs_(a, b, c, d, e, f, g, h, i, j) PetscMissingLapack("TRTRS", a, b, c, d, e, f, g, h, i, j)
#endif
BLAS_EXTERN void LAPACKgels_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);

/* handle complex dot() with special code */
#if defined(PETSC_USE_COMPLEX)
static inline PetscScalar BLASdot_(const PetscBLASInt *n, const PetscScalar *x, const PetscBLASInt *sx, const PetscScalar *y, const PetscBLASInt *sy)
{
  PetscScalar sum = 0.0;
  PetscInt    i, j, k;
  if (*sx == 1 && *sy == 1) {
    for (i = 0; i < *n; i++) sum += PetscConj(x[i]) * y[i];
  } else {
    for (i = 0, j = 0, k = 0; i < *n; i++, j += *sx, k += *sy) sum += PetscConj(x[j]) * y[k];
  }
  return sum;
}
static inline PetscScalar BLASdotu_(const PetscBLASInt *n, const PetscScalar *x, const PetscBLASInt *sx, const PetscScalar *y, const PetscBLASInt *sy)
{
  PetscScalar sum = 0.0;
  PetscInt    i, j, k;
  if (*sx == 1 && *sy == 1) {
    for (i = 0; i < *n; i++) sum += x[i] * y[i];
  } else {
    for (i = 0, j = 0, k = 0; i < *n; i++, j += *sx, k += *sy) sum += x[j] * y[k];
  }
  return sum;
}
#else
  #if defined(PETSC_USE_REAL_SINGLE) && defined(PETSC_BLASLAPACK_SDOT_RETURNS_DOUBLE)
BLAS_EXTERN double    BLASdot_(const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *);
BLAS_EXTERN double    BLASdotu_(const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *);
  #else
BLAS_EXTERN PetscScalar BLASdot_(const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *);
  #endif
#endif

/* Some functions prototypes do not exist for reals */
#if defined(PETSC_USE_COMPLEX)
BLAS_EXTERN void LAPACKhetrf_(const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKhetrs_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKhetri_(const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, PetscBLASInt *);
BLAS_EXTERN void LAPACKheev_(const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *);
#endif
/* Some functions prototypes differ between real and complex */
#if defined(PETSC_USE_COMPLEX)
  #if !defined(PETSC_MISSING_LAPACK_GELSS)
BLAS_EXTERN void LAPACKgelss_(const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, const PetscReal *, PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *);
  #else
    #define LAPACKgelss_(a, b, c, d, e, f, g, h, i, j, k, l, m, n) PetscMissingLapack("GELSS", a, b, c, d, e, f, g, h, i, j, k, l, m, n)
  #endif
BLAS_EXTERN void LAPACKsyev_(const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *);
BLAS_EXTERN void LAPACKsyevx_(const char *, const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, const PetscReal *, const PetscReal *, const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *, PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKsygv_(const PetscBLASInt *, const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *);
BLAS_EXTERN void LAPACKsygvx_(PetscBLASInt *, const char *, const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, const PetscReal *, const PetscReal *, const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *, PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKpttrs_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, const PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
  #if !defined(PETSC_MISSING_LAPACK_GERFS)
BLAS_EXTERN void LAPACKgerfs_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscScalar *, PetscScalar *, PetscReal *, PetscBLASInt *);
  #else
    #define LAPACKgerfs_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) PetscMissingLapack("GERFS", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_TRSEN)
BLAS_EXTERN void LAPACKtrsen_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscBLASInt *, PetscReal *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKtrsen_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) PetscMissingLapack("TRSEN", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_TGSEN)
BLAS_EXTERN void LAPACKtgsen_(const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscReal *, PetscReal *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, const PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKtgsen_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x) PetscMissingLapack("TGSEN", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_GGES)
BLAS_EXTERN void LAPACKgges_(const char *, const char *, const char *, PetscBLASInt (*)(void), const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscScalar *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKgges_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) PetscMissingLapack("GGES", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_HSEQR)
BLAS_EXTERN void LAPACKhseqr_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKhseqr_(a, b, c, d, e, f, g, h, i, j, k, l, m) PetscMissingLapack("HSEQR", a, b, c, d, e, f, g, h, i, j, k, l, m)
  #endif
#else /* !defined(PETSC_USE_COMPLEX) */
  #if !defined(PETSC_MISSING_LAPACK_GELSS)
BLAS_EXTERN void LAPACKgelss_(const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, const PetscReal *, PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKgelss_(a, b, c, d, e, f, g, h, i, j, k, l, m) PetscMissingLapack("GELSS", a, b, c, d, e, f, g, h, i, j, k, l, m)
  #endif
BLAS_EXTERN void LAPACKsyev_(const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKsyevx_(const char *, const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, const PetscReal *, const PetscReal *, const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKsygv_(const PetscBLASInt *, const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKsygvx_(const PetscBLASInt *, const char *, const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, const PetscReal *, const PetscReal *, const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, PetscBLASInt *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKpttrs_(const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, const PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
  #if !defined(PETSC_MISSING_LAPACK_STEBZ)
BLAS_EXTERN void LAPACKstebz_(const char *, const char *, const PetscBLASInt *, const PetscReal *, const PetscReal *, const PetscBLASInt *, const PetscBLASInt *, const PetscReal *, const PetscReal *, const PetscReal *, PetscBLASInt *, PetscBLASInt *, PetscReal *, PetscBLASInt *, PetscBLASInt *, PetscReal *, PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKstebz_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) PetscMissingLapack("STEBZ", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_GERFS)
BLAS_EXTERN void LAPACKgerfs_(const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, const PetscBLASInt *, const PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscScalar *, PetscScalar *, PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKgerfs_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) PetscMissingLapack("GERFS", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_TRSEN)
BLAS_EXTERN void LAPACKtrsen_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, PetscReal *, const PetscBLASInt *, PetscReal *, const PetscBLASInt *, PetscReal *, PetscReal *, PetscBLASInt *, PetscReal *, PetscReal *, PetscReal *, const PetscBLASInt *, PetscBLASInt *, const PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKtrsen_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) PetscMissingLapack("TRSEN", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_TGSEN)
BLAS_EXTERN void LAPACKtgsen_(const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, PetscReal *, const PetscBLASInt *, PetscReal *, const PetscBLASInt *, PetscReal *, PetscReal *, PetscReal *, PetscReal *, const PetscBLASInt *, PetscReal *, const PetscBLASInt *, PetscBLASInt *, PetscReal *, PetscReal *, PetscReal *, PetscReal *, const PetscBLASInt *, PetscBLASInt *, const PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKtgsen_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y) PetscMissingLapack("TGSEN", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_GGES)
BLAS_EXTERN void LAPACKgges_(const char *, const char *, const char *, PetscBLASInt (*)(void), const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscScalar *, PetscScalar *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKgges_(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) PetscMissingLapack("GGES", a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u)
  #endif
  #if !defined(PETSC_MISSING_LAPACK_HSEQR)
BLAS_EXTERN void LAPACKhseqr_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
  #else
    #define LAPACKhseqr_(a, b, c, d, e, f, g, h, i, j, k, l, m, n) PetscMissingLapack("HSEQR", a, b, c, d, e, f, g, h, i, j, k, l, m, n)
  #endif
#endif /* defined(PETSC_USE_COMPLEX) */

#if defined(PETSC_USE_COMPLEX)
BLAS_EXTERN void LAPACKgeev_(const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *);
BLAS_EXTERN void LAPACKgesvd_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscBLASInt *);
#else
BLAS_EXTERN void LAPACKgeev_(const char *, const char *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
BLAS_EXTERN void LAPACKgesvd_(const char *, const char *, const PetscBLASInt *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscReal *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscScalar *, const PetscBLASInt *, PetscBLASInt *);
#endif

#endif

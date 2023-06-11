#include <petsc/private/fortranimpl.h>

#if defined(PETSC_HAVE_FORTRAN_CAPS)
  #define petscfprintf_             PETSCFPRINTF
  #define petscprintf_              PETSCPRINTF
  #define petscsynchronizedfprintf_ PETSCSYNCHRONIZEDFPRINTF
  #define petscsynchronizedprintf_  PETSCSYNCHRONIZEDPRINTF
  #define petscsynchronizedflush_   PETSCSYNCHRONIZEDFLUSH
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
  #define petscfprintf_             petscfprintf
  #define petscprintf_              petscprintf
  #define petscsynchronizedfprintf_ petscsynchronizedfprintf
  #define petscsynchronizedprintf_  petscsynchronizedprintf
  #define petscsynchronizedflush_   petscsynchronizedflush
#endif

#if defined(__cplusplus)
extern "C" {
#endif

PETSC_EXTERN void petscsynchronizedflush_(MPI_Fint *comm, FILE **file, int *ierr)
{
  FILE *f = *file;
  if (!f) f = PETSC_STDOUT; /* support for PETSC_STDOUT in Fortran */
  *ierr = PetscSynchronizedFlush(MPI_Comm_f2c(*(comm)), f);
}

static PetscErrorCode PetscFixSlashN(const char *in, char **out)
{
  size_t i, len;

  PetscFunctionBegin;
  PetscCall(PetscStrallocpy(in, out));
  PetscCall(PetscStrlen(*out, &len));
  for (i = 0; i < len - 1; i++) {
    if ((*out)[i] == '\\' && (*out)[i + 1] == 'n') {
      (*out)[i]     = ' ';
      (*out)[i + 1] = '\n';
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN void petscfprintf_(MPI_Comm *comm, FILE **file, char *fname, PetscErrorCode *ierr, PETSC_FORTRAN_CHARLEN_T len1)
{
  char *c1, *tmp;

  FIXCHAR(fname, len1, c1);
  *ierr = PetscFixSlashN(c1, &tmp);
  if (*ierr) return;
  FREECHAR(fname, c1);
  *ierr = PetscFPrintf(MPI_Comm_f2c(*(MPI_Fint *)&*comm), *file, "%s", tmp);
  if (*ierr) return;
  *ierr = PetscFree(tmp);
}

PETSC_EXTERN void petscprintf_(MPI_Comm *comm, char *fname, PetscErrorCode *ierr, PETSC_FORTRAN_CHARLEN_T len1)
{
  char *c1, *tmp;

  FIXCHAR(fname, len1, c1);
  *ierr = PetscFixSlashN(c1, &tmp);
  if (*ierr) return;
  FREECHAR(fname, c1);
  *ierr = PetscPrintf(MPI_Comm_f2c(*(MPI_Fint *)&*comm), "%s", tmp);
  if (*ierr) return;
  *ierr = PetscFree(tmp);
}

PETSC_EXTERN void petscsynchronizedfprintf_(MPI_Comm *comm, FILE **file, char *fname, PetscErrorCode *ierr, PETSC_FORTRAN_CHARLEN_T len1)
{
  char *c1, *tmp;

  FIXCHAR(fname, len1, c1);
  *ierr = PetscFixSlashN(c1, &tmp);
  if (*ierr) return;
  FREECHAR(fname, c1);
  *ierr = PetscSynchronizedFPrintf(MPI_Comm_f2c(*(MPI_Fint *)&*comm), *file, "%s", tmp);
  if (*ierr) return;
  *ierr = PetscFree(tmp);
}

PETSC_EXTERN void petscsynchronizedprintf_(MPI_Comm *comm, char *fname, PetscErrorCode *ierr, PETSC_FORTRAN_CHARLEN_T len1)
{
  char *c1, *tmp;

  FIXCHAR(fname, len1, c1);
  *ierr = PetscFixSlashN(c1, &tmp);
  if (*ierr) return;
  FREECHAR(fname, c1);
  *ierr = PetscSynchronizedPrintf(MPI_Comm_f2c(*(MPI_Fint *)&*comm), "%s", tmp);
  if (*ierr) return;
  *ierr = PetscFree(tmp);
}
#if defined(__cplusplus)
}
#endif

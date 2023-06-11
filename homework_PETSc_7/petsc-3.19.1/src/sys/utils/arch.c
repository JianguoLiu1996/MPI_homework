
#include <petscsys.h> /*I  "petscsys.h"  I*/

/*@C
     PetscGetArchType - Returns the $PETSC_ARCH that was used for this configuration of PETSc

     Not Collective

     Input Parameter:
.    slen - length of string buffer

     Output Parameter:
.    str - string area to contain architecture name, should be at least
           10 characters long. Name is truncated if string is not long enough.

     Level: developer

   Note:
    This name is arbitrary and need not correspond to the physical hardware or the software running on the system.

   Fortran Note:
   This routine has the format
.vb
       character*(10) str
       call PetscGetArchType(str,ierr)
.ve

.seealso: `PetscGetUserName()`, `PetscGetHostName()`
@*/
PetscErrorCode PetscGetArchType(char str[], size_t slen)
{
  PetscFunctionBegin;
#if defined(PETSC_ARCH)
  PetscCall(PetscStrncpy(str, PETSC_ARCH, slen - 1));
#else
  #error "$PETSC_ARCH/include/petscconf.h is missing PETSC_ARCH"
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

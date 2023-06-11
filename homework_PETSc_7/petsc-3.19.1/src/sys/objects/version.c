#include <petscsys.h>
/*@C
    PetscGetVersion - Gets the PETSc version information in a string.

    Not Collective; No Fortran Support

    Input Parameter:
.   len - length of the string

    Output Parameter:
.   version - version string

    Level: developer

    Note:
    For doing runtime checking of supported versions we recommend using `PetscGetVersionNumber()` instead of this routine.

.seealso: `PetscGetProgramName()`, `PetscGetVersionNumber()`
@*/

PetscErrorCode PetscGetVersion(char version[], size_t len)
{
  PetscFunctionBegin;
#if (PETSC_VERSION_RELEASE == 1)
  PetscCall(PetscSNPrintf(version, len, "Petsc Release Version %d.%d.%d, %s ", PETSC_VERSION_MAJOR, PETSC_VERSION_MINOR, PETSC_VERSION_SUBMINOR, PETSC_VERSION_DATE));
#else
  PetscCall(PetscSNPrintf(version, len, "Petsc Development GIT revision: %s  GIT Date: %s", PETSC_VERSION_GIT, PETSC_VERSION_DATE_GIT));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscGetVersionNumber - Gets the PETSc version information from the library

    Not Collective

    Output Parameters:
+   major - the major version (optional, pass `NULL` if not requested)
.   minor - the minor version (optional, pass `NULL` if not requested)
.   subminor - the subminor version (patch number)  (optional, pass `NULL` if not requested)
-   release - indicates the library is from a release, not random git repository  (optional, pass `NULL` if not requested)

    Level: developer

    Notes:
    The C macros `PETSC_VERSION_MAJOR`, `PETSC_VERSION_MINOR`, `PETSC_VERSION_SUBMINOR`, `PETSC_VERSION_RELEASE` provide the information at
    compile time. This can be used to confirm that the shared library being loaded at runtime has the appropriate version updates.

    This function can be called before `PetscInitialize()`

.seealso: `PetscGetProgramName()`, `PetscGetVersion()`, `PetscInitialize()`
@*/
PetscErrorCode PetscGetVersionNumber(PetscInt *major, PetscInt *minor, PetscInt *subminor, PetscInt *release)
{
  if (major) *major = PETSC_VERSION_MAJOR;
  if (minor) *minor = PETSC_VERSION_MINOR;
  if (subminor) *subminor = PETSC_VERSION_SUBMINOR;
  if (release) *release = PETSC_VERSION_RELEASE;
  return PETSC_SUCCESS;
}

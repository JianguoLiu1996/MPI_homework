
#include <../src/mat/impls/mffd/mffdimpl.h> /*I  "petscmat.h"   I*/

PETSC_EXTERN PetscErrorCode MatCreateMFFD_DS(MatMFFD);
PETSC_EXTERN PetscErrorCode MatCreateMFFD_WP(MatMFFD);

/*@C
  MatMFFDRegisterAll - Registers all of the compute-h in the `MATMFFD` package.

  Not Collective

  Level: developer

.seealso: `MATMFFD`, `MatMFFDRegisterDestroy()`, `MatMFFDRegister()`, `MatCreateMFFD()`,
          `MatMFFDSetType()`
@*/
PetscErrorCode MatMFFDRegisterAll(void)
{
  PetscFunctionBegin;
  if (MatMFFDRegisterAllCalled) PetscFunctionReturn(PETSC_SUCCESS);
  MatMFFDRegisterAllCalled = PETSC_TRUE;

  PetscCall(MatMFFDRegister(MATMFFD_DS, MatCreateMFFD_DS));
  PetscCall(MatMFFDRegister(MATMFFD_WP, MatCreateMFFD_WP));
  PetscFunctionReturn(PETSC_SUCCESS);
}


/*
    Identity preconditioner, simply copies vector x to y.
*/
#include <petsc/private/pcimpl.h> /*I "petscpc.h" I*/

PetscErrorCode PCApply_None(PC pc, Vec x, Vec y)
{
  PetscFunctionBegin;
  PetscCall(VecCopy(x, y));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCMatApply_None(PC pc, Mat X, Mat Y)
{
  PetscFunctionBegin;
  PetscCall(MatCopy(X, Y, SAME_NONZERO_PATTERN));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
     PCNONE - This is used when you wish to employ a nonpreconditioned
             Krylov method.

   Level: beginner

  Developer Note:
  This is implemented by a `VecCopy()`. It would be nice if the `KSP` implementations could be organized to avoid this copy without making them
  more complex.

.seealso: `PCCreate()`, `PCSetType()`, `PCType`, `PC`
M*/

PETSC_EXTERN PetscErrorCode PCCreate_None(PC pc)
{
  PetscFunctionBegin;
  pc->ops->apply               = PCApply_None;
  pc->ops->matapply            = PCMatApply_None;
  pc->ops->applytranspose      = PCApply_None;
  pc->ops->destroy             = NULL;
  pc->ops->setup               = NULL;
  pc->ops->view                = NULL;
  pc->ops->applysymmetricleft  = PCApply_None;
  pc->ops->applysymmetricright = PCApply_None;

  pc->data = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

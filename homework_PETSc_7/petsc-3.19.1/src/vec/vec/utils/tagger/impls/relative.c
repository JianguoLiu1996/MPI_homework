
#include <petsc/private/vecimpl.h> /*I "petscvec.h" I*/
#include "../src/vec/vec/utils/tagger/impls/simple.h"

static PetscErrorCode VecTaggerComputeBoxes_Relative(VecTagger tagger, Vec vec, PetscInt *numBoxes, VecTaggerBox **boxes, PetscBool *listed)
{
  VecTagger_Simple  *smpl = (VecTagger_Simple *)tagger->data;
  PetscInt           bs, i, j, k, n;
  VecTaggerBox      *bxs;
  const PetscScalar *vArray;

  PetscFunctionBegin;
  PetscCall(VecTaggerGetBlockSize(tagger, &bs));
  *numBoxes = 1;
  PetscCall(PetscMalloc1(bs, &bxs));
  PetscCall(VecGetLocalSize(vec, &n));
  n /= bs;
  for (i = 0; i < bs; i++) {
#if !defined(PETSC_USE_COMPLEX)
    bxs[i].min = PETSC_MAX_REAL;
    bxs[i].max = PETSC_MIN_REAL;
#else
    bxs[i].min = PetscCMPLX(PETSC_MAX_REAL, PETSC_MAX_REAL);
    bxs[i].max = PetscCMPLX(PETSC_MIN_REAL, PETSC_MIN_REAL);
#endif
  }
  PetscCall(VecGetArrayRead(vec, &vArray));
  for (i = 0, k = 0; i < n; i++) {
    for (j = 0; j < bs; j++, k++) {
#if !defined(PETSC_USE_COMPLEX)
      bxs[j].min = PetscMin(bxs[j].min, vArray[k]);
      bxs[j].max = PetscMax(bxs[j].max, vArray[k]);
#else
      bxs[j].min = PetscCMPLX(PetscMin(PetscRealPart(bxs[j].min), PetscRealPart(vArray[k])), PetscMin(PetscImaginaryPart(bxs[j].min), PetscImaginaryPart(vArray[k])));
      bxs[j].max = PetscCMPLX(PetscMax(PetscRealPart(bxs[j].max), PetscRealPart(vArray[k])), PetscMax(PetscImaginaryPart(bxs[j].max), PetscImaginaryPart(vArray[k])));
#endif
    }
  }
  for (i = 0; i < bs; i++) bxs[i].max = -bxs[i].max;
  PetscCall(VecRestoreArrayRead(vec, &vArray));
  PetscCall(MPIU_Allreduce(MPI_IN_PLACE, (PetscReal *)bxs, 2 * (sizeof(PetscScalar) / sizeof(PetscReal)) * bs, MPIU_REAL, MPIU_MIN, PetscObjectComm((PetscObject)tagger)));
  for (i = 0; i < bs; i++) {
    PetscScalar mins = bxs[i].min;
    PetscScalar difs = -bxs[i].max - mins;
#if !defined(PETSC_USE_COMPLEX)
    bxs[i].min = mins + smpl->box[i].min * difs;
    bxs[i].max = mins + smpl->box[i].max * difs;
#else
    bxs[i].min = mins + PetscCMPLX(PetscRealPart(smpl->box[i].min) * PetscRealPart(difs), PetscImaginaryPart(smpl->box[i].min) * PetscImaginaryPart(difs));
    bxs[i].max = mins + PetscCMPLX(PetscRealPart(smpl->box[i].max) * PetscRealPart(difs), PetscImaginaryPart(smpl->box[i].max) * PetscImaginaryPart(difs));
#endif
  }
  *boxes = bxs;
  if (listed) *listed = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  VecTaggerRelativeSetBox - Set the relative box defining the values to be tagged by the tagger, where relative boxes are subsets of [0,1] (or [0,1]+[0,1]i for complex scalars), where 0 indicates the smallest value present in the vector and 1 indicates the largest.

  Logically Collective

  Input Parameters:
+ tagger - the VecTagger context
- box - a blocksize list of VecTaggerBox boxes

  Level: advanced

.seealso: `VecTaggerRelativeGetBox()`
@*/
PetscErrorCode VecTaggerRelativeSetBox(VecTagger tagger, VecTaggerBox *box)
{
  PetscFunctionBegin;
  PetscCall(VecTaggerSetBox_Simple(tagger, box));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  VecTaggerRelativeGetBox - Get the relative box defining the values to be tagged by the tagger, where relative boxess are subsets of [0,1] (or [0,1]+[0,1]i for complex scalars), where 0 indicates the smallest value present in the vector and 1 indicates the largest.

  Logically Collective

  Input Parameter:
. tagger - the VecTagger context

  Output Parameter:
. box - a blocksize list of VecTaggerBox boxes

  Level: advanced

.seealso: `VecTaggerRelativeSetBox()`
@*/
PetscErrorCode VecTaggerRelativeGetBox(VecTagger tagger, const VecTaggerBox **box)
{
  PetscFunctionBegin;
  PetscCall(VecTaggerGetBox_Simple(tagger, box));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode VecTaggerCreate_Relative(VecTagger tagger)
{
  PetscFunctionBegin;
  PetscCall(VecTaggerCreate_Simple(tagger));
  tagger->ops->computeboxes = VecTaggerComputeBoxes_Relative;
  PetscFunctionReturn(PETSC_SUCCESS);
}

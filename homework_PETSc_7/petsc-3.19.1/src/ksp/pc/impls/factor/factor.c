
#include <../src/ksp/pc/impls/factor/factor.h> /*I "petscpc.h" I*/
#include <petsc/private/matimpl.h>

/*
    If an ordering is not yet set and the matrix is available determine a default ordering
*/
PetscErrorCode PCFactorSetDefaultOrdering_Factor(PC pc)
{
  PetscBool   foundmtype, flg;
  const char *prefix;

  PetscFunctionBegin;
  if (pc->pmat) {
    PetscCall(PCGetOptionsPrefix(pc, &prefix));
    PetscCall(MatSetOptionsPrefixFactor(pc->pmat, prefix));
    PC_Factor *fact = (PC_Factor *)pc->data;
    PetscCall(MatSolverTypeGet(fact->solvertype, ((PetscObject)pc->pmat)->type_name, fact->factortype, NULL, &foundmtype, NULL));
    if (foundmtype) {
      if (!fact->fact) {
        PetscCall(MatGetFactor(pc->pmat, fact->solvertype, fact->factortype, &fact->fact));
      } else if (!fact->fact->assembled) {
        PetscCall(PetscStrcmp(fact->solvertype, fact->fact->solvertype, &flg));
        if (!flg) {
          Mat B;
          PetscCall(MatGetFactor(pc->pmat, fact->solvertype, fact->factortype, &B));
          PetscCall(MatHeaderReplace(fact->fact, &B));
        }
      }
      if (!fact->ordering) {
        PetscBool       canuseordering;
        MatOrderingType otype;

        PetscCall(MatFactorGetCanUseOrdering(fact->fact, &canuseordering));
        if (canuseordering) {
          PetscCall(MatFactorGetPreferredOrdering(fact->fact, fact->factortype, &otype));
        } else otype = MATORDERINGEXTERNAL;
        PetscCall(PetscStrallocpy(otype, (char **)&fact->ordering));
      }
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCFactorSetReuseOrdering_Factor(PC pc, PetscBool flag)
{
  PC_Factor *lu = (PC_Factor *)pc->data;

  PetscFunctionBegin;
  lu->reuseordering = flag;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCFactorSetReuseFill_Factor(PC pc, PetscBool flag)
{
  PC_Factor *lu = (PC_Factor *)pc->data;

  PetscFunctionBegin;
  lu->reusefill = flag;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCFactorSetUseInPlace_Factor(PC pc, PetscBool flg)
{
  PC_Factor *dir = (PC_Factor *)pc->data;

  PetscFunctionBegin;
  dir->inplace = flg;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCFactorGetUseInPlace_Factor(PC pc, PetscBool *flg)
{
  PC_Factor *dir = (PC_Factor *)pc->data;

  PetscFunctionBegin;
  *flg = dir->inplace;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PCFactorSetUpMatSolverType - Can be called after `KSPSetOperators()` or `PCSetOperators()`, causes `MatGetFactor()` to be called so then one may
       set the options for that particular factorization object.

  Input Parameter:
.  pc  - the preconditioner context

  Note:
  After you have called this function (which has to be after the `KSPSetOperators()` or `PCSetOperators()`) you can call `PCFactorGetMatrix()` and then set factor options on that matrix.

  Level: intermediate

.seealso: `PCCHOLESKY`, `PCLU`, `PCFactorSetMatSolverType()`, `PCFactorGetMatrix()`
@*/
PetscErrorCode PCFactorSetUpMatSolverType(PC pc)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscTryMethod(pc, "PCFactorSetUpMatSolverType_C", (PC), (pc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetZeroPivot - Sets the size at which smaller pivots are declared to be zero

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  zero - all pivots smaller than this will be considered zero

   Options Database Key:
.  -pc_factor_zeropivot <zero> - Sets tolerance for what is considered a zero pivot

   Level: intermediate

.seealso: `PCCHOLESKY`, `PCLU`, `PCFactorSetShiftType()`, `PCFactorSetShiftAmount()`
@*/
PetscErrorCode PCFactorSetZeroPivot(PC pc, PetscReal zero)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidLogicalCollectiveReal(pc, zero, 2);
  PetscTryMethod(pc, "PCFactorSetZeroPivot_C", (PC, PetscReal), (pc, zero));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetShiftType - adds a particular type of quantity to the diagonal of the matrix during
     numerical factorization, thus the matrix has nonzero pivots

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  shifttype - type of shift; one of `MAT_SHIFT_NONE`, `MAT_SHIFT_NONZERO`, `MAT_SHIFT_POSITIVE_DEFINITE`, `MAT_SHIFT_INBLOCKS`

   Options Database Key:
.  -pc_factor_shift_type <shifttype> - Sets shift type; use '-help' for a list of available types

   Level: intermediate

.seealso: `PCCHOLESKY`, `PCLU`, `PCFactorSetZeroPivot()`, `PCFactorSetShiftAmount()`
@*/
PetscErrorCode PCFactorSetShiftType(PC pc, MatFactorShiftType shifttype)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidLogicalCollectiveEnum(pc, shifttype, 2);
  PetscTryMethod(pc, "PCFactorSetShiftType_C", (PC, MatFactorShiftType), (pc, shifttype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetShiftAmount - adds a quantity to the diagonal of the matrix during
     numerical factorization, thus the matrix has nonzero pivots

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  shiftamount - amount of shift or `PETSC_DECIDE` for the default

   Options Database Key:
.  -pc_factor_shift_amount <shiftamount> - Sets shift amount or -1 for the default

   Level: intermediate

.seealso: `PCCHOLESKY`, `PCLU`, ``PCFactorSetZeroPivot()`, `PCFactorSetShiftType()`
@*/
PetscErrorCode PCFactorSetShiftAmount(PC pc, PetscReal shiftamount)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidLogicalCollectiveReal(pc, shiftamount, 2);
  PetscTryMethod(pc, "PCFactorSetShiftAmount_C", (PC, PetscReal), (pc, shiftamount));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetDropTolerance - The preconditioner will use an `PCILU`
   based on a drop tolerance.

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
.  dt - the drop tolerance, try from 1.e-10 to .1
.  dtcol - tolerance for column pivot, good values [0.1 to 0.01]
-  maxrowcount - the max number of nonzeros allowed in a row, best value
                 depends on the number of nonzeros in row of original matrix

   Options Database Key:
.  -pc_factor_drop_tolerance <dt,dtcol,maxrowcount> - Sets drop tolerance

   Level: intermediate

   Note:
   There are NO default values for the 3 parameters, you must set them with reasonable values for your
   matrix. We don't know how to compute reasonable values.

.seealso: `PCILU`
@*/
PetscErrorCode PCFactorSetDropTolerance(PC pc, PetscReal dt, PetscReal dtcol, PetscInt maxrowcount)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidLogicalCollectiveReal(pc, dtcol, 3);
  PetscValidLogicalCollectiveInt(pc, maxrowcount, 4);
  PetscTryMethod(pc, "PCFactorSetDropTolerance_C", (PC, PetscReal, PetscReal, PetscInt), (pc, dt, dtcol, maxrowcount));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorGetZeroPivot - Gets the tolerance used to define a zero privot

   Not Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.  pivot - the tolerance

   Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `PCFactorSetZeroPivot()`
@*/
PetscErrorCode PCFactorGetZeroPivot(PC pc, PetscReal *pivot)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscUseMethod(pc, "PCFactorGetZeroPivot_C", (PC, PetscReal *), (pc, pivot));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorGetShiftAmount - Gets the tolerance used to define a zero privot

   Not Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.  shift - how much to shift the diagonal entry

   Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `PCFactorSetShiftAmount()`, `PCFactorSetShiftType()`, `PCFactorGetShiftType()`
@*/
PetscErrorCode PCFactorGetShiftAmount(PC pc, PetscReal *shift)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscUseMethod(pc, "PCFactorGetShiftAmount_C", (PC, PetscReal *), (pc, shift));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorGetShiftType - Gets the type of shift, if any, done when a zero pivot is detected

   Not Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.  type - one of `MAT_SHIFT_NONE`, `MAT_SHIFT_NONZERO`, `MAT_SHIFT_POSITIVE_DEFINITE`, or `MAT_SHIFT_INBLOCKS`

   Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `PCFactorSetShiftType()`, `MatFactorShiftType`, `PCFactorSetShiftAmount()`, `PCFactorGetShiftAmount()`
@*/
PetscErrorCode PCFactorGetShiftType(PC pc, MatFactorShiftType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscUseMethod(pc, "PCFactorGetShiftType_C", (PC, MatFactorShiftType *), (pc, type));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorGetLevels - Gets the number of levels of fill to use.

   Logically Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.  levels - number of levels of fill

   Level: intermediate

.seealso: `PCILU`, `PCICC`, `PCFactorSetLevels()`
@*/
PetscErrorCode PCFactorGetLevels(PC pc, PetscInt *levels)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscUseMethod(pc, "PCFactorGetLevels_C", (PC, PetscInt *), (pc, levels));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetLevels - Sets the number of levels of fill to use.

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  levels - number of levels of fill

   Options Database Key:
.  -pc_factor_levels <levels> - Sets fill level

   Level: intermediate

.seealso: `PCILU`, `PCICC`, `PCFactorGetLevels()`
@*/
PetscErrorCode PCFactorSetLevels(PC pc, PetscInt levels)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscCheck(levels >= 0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "negative levels");
  PetscValidLogicalCollectiveInt(pc, levels, 2);
  PetscTryMethod(pc, "PCFactorSetLevels_C", (PC, PetscInt), (pc, levels));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetAllowDiagonalFill - Causes all diagonal matrix entries to be
   treated as level 0 fill even if there is no non-zero location.

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  flg - `PETSC_TRUE` to turn on, `PETSC_FALSE` to turn off

   Options Database Key:
.  -pc_factor_diagonal_fill <bool> - allow the diagonal fill

   Note:
   Does not apply with 0 fill.

   Level: intermediate

.seealso: `PCILU`, `PCICC`, `PCFactorGetAllowDiagonalFill()`
@*/
PetscErrorCode PCFactorSetAllowDiagonalFill(PC pc, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscTryMethod(pc, "PCFactorSetAllowDiagonalFill_C", (PC, PetscBool), (pc, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorGetAllowDiagonalFill - Determines if all diagonal matrix entries are
       treated as level 0 fill even if there is no non-zero location.

   Logically Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.   flg - `PETSC_TRUE` to turn on, `PETSC_FALSE` to turn off

   Note:
   Does not apply with 0 fill.

   Level: intermediate

.seealso:  `PCILU`, `PCICC`, `PCFactorSetAllowDiagonalFill()`
@*/
PetscErrorCode PCFactorGetAllowDiagonalFill(PC pc, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscUseMethod(pc, "PCFactorGetAllowDiagonalFill_C", (PC, PetscBool *), (pc, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorReorderForNonzeroDiagonal - reorders rows/columns of matrix to remove zeros from diagonal

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  tol - diagonal entries smaller than this in absolute value are considered zero

   Options Database Key:
.  -pc_factor_nonzeros_along_diagonal <tol> - perform the reordering with the given tolerance

   Level: intermediate

.seealso:  `PCILU`, `PCICC`, `PCFactorSetFill()`, `PCFactorSetShiftNonzero()`, `PCFactorSetZeroPivot()`, `MatReorderForNonzeroDiagonal()`
@*/
PetscErrorCode PCFactorReorderForNonzeroDiagonal(PC pc, PetscReal rtol)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidLogicalCollectiveReal(pc, rtol, 2);
  PetscTryMethod(pc, "PCFactorReorderForNonzeroDiagonal_C", (PC, PetscReal), (pc, rtol));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PCFactorSetMatSolverType - sets the solver package that is used to perform the factorization

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  stype - for example, `MATSOLVERSUPERLU`, `MATSOLVERSUPERLU_DIST`, `MATSOLVERMUMPS`

   Options Database Key:
.  -pc_factor_mat_solver_type <stype> - petsc, superlu, superlu_dist, mumps, cusparse

   Level: intermediate

   Note:
   By default this will use the PETSc factorization if it exists

.seealso: `PCLU`, `PCCHOLESKY`, `MatGetFactor()`, `MatSolverType`, `PCFactorGetMatSolverType()`, `MatSolverType`,
          `MATSOLVERSUPERLU`, `MATSOLVERSUPERLU_DIST`, `MATSOLVERMUMPS`
@*/
PetscErrorCode PCFactorSetMatSolverType(PC pc, MatSolverType stype)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscTryMethod(pc, "PCFactorSetMatSolverType_C", (PC, MatSolverType), (pc, stype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PCFactorGetMatSolverType - gets the solver package that is used to perform the factorization

   Not Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.   stype - for example, `MATSOLVERSUPERLU`, `MATSOLVERSUPERLU_DIST`, `MATSOLVERMUMPS`

   Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `MatGetFactor()`, `MatSolverType`, `PCFactorGetMatSolverType()`, `MatSolverType`,
          `MATSOLVERSUPERLU`, `MATSOLVERSUPERLU_DIST`, `MATSOLVERMUMPS`
@*/
PetscErrorCode PCFactorGetMatSolverType(PC pc, MatSolverType *stype)
{
  PetscErrorCode (*f)(PC, MatSolverType *);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidPointer(stype, 2);
  PetscCall(PetscObjectQueryFunction((PetscObject)pc, "PCFactorGetMatSolverType_C", &f));
  if (f) PetscCall((*f)(pc, stype));
  else *stype = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetFill - Indicate the amount of fill you expect in the factored matrix,
   fill = number nonzeros in factor/number nonzeros in original matrix.

   Not Collective, each process can expect a different amount of fill

   Input Parameters:
+  pc - the preconditioner context
-  fill - amount of expected fill

   Options Database Key:
.  -pc_factor_fill <fill> - Sets fill amount

   Level: intermediate

   Notes:
   For sparse matrix factorizations it is difficult to predict how much
   fill to expect. By running with the option -info PETSc will print the
   actual amount of fill used; allowing you to set the value accurately for
   future runs. Default PETSc uses a value of 5.0

   This is ignored for most solver packages

   This parameter has NOTHING to do with the levels-of-fill of ILU(). That is set with `PCFactorSetLevels()` or -pc_factor_levels.

.seealso: `PCLU`, `PCCHOLESKY`, `PCILU`, `PCICC`, `PCFactorSetReuseFill()`
@*/
PetscErrorCode PCFactorSetFill(PC pc, PetscReal fill)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscCheck(fill >= 1.0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Fill factor cannot be less then 1.0");
  PetscTryMethod(pc, "PCFactorSetFill_C", (PC, PetscReal), (pc, fill));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetUseInPlace - Tells the system to do an in-place factorization.
   For dense matrices, this enables the solution of much larger problems.
   For sparse matrices the factorization cannot be done truly in-place
   so this does not save memory during the factorization, but after the matrix
   is factored, the original unfactored matrix is freed, thus recovering that
   space. For ICC(0) and ILU(0) with the default natural ordering the factorization is done efficiently in-place.

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  flg - `PETSC_TRUE` to enable, `PETSC_FALSE` to disable

   Options Database Key:
.  -pc_factor_in_place <true,false>- Activate/deactivate in-place factorization

   Note:
   `PCFactorSetUseInplace()` can only be used with the `KSP` method `KSPPREONLY` or when
   a different matrix is provided for the multiply and the preconditioner in
   a call to `KSPSetOperators()`.
   This is because the Krylov space methods require an application of the
   matrix multiplication, which is not possible here because the matrix has
   been factored in-place, replacing the original matrix.

   Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `PCILU`, `PCICC`, `PCFactorGetUseInPlace()`
@*/
PetscErrorCode PCFactorSetUseInPlace(PC pc, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscTryMethod(pc, "PCFactorSetUseInPlace_C", (PC, PetscBool), (pc, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorGetUseInPlace - Determines if an in-place factorization is being used.

   Logically Collective

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.  flg - `PETSC_TRUE` to enable, `PETSC_FALSE` to disable

   Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `PCILU`, `PCICC`, `PCFactorSetUseInPlace()`
@*/
PetscErrorCode PCFactorGetUseInPlace(PC pc, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscUseMethod(pc, "PCFactorGetUseInPlace_C", (PC, PetscBool *), (pc, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PCFactorSetMatOrderingType - Sets the ordering routine (to reduce fill) to
    be used in the `PCLU`, `PCCHOLESKY`, `PCILU`,  or `PCICC` preconditioners

    Logically Collective

    Input Parameters:
+   pc - the preconditioner context
-   ordering - the matrix ordering name, for example, `MATORDERINGND` or `MATORDERINGRCM`

    Options Database Key:
.   -pc_factor_mat_ordering_type <nd,rcm,...,external> - Sets ordering routine

    Level: intermediate

    Notes:
      Nested dissection is used by default for some of PETSc's sparse matrix formats

     For `PCCHOLESKY` and `PCICC` and the `MATSBAIJ` format the only reordering available is natural since only the upper half of the matrix is stored
     and reordering this matrix is very expensive.

    You can use a `MATSEQAIJ` matrix with Cholesky and ICC and use any ordering.

    `MATORDERINGEXTERNAL` means PETSc will not compute an ordering and the package will use its own ordering, usable with `MATSOLVERCHOLMOD`, `MATSOLVERUMFPACK`, and others.

.seealso: `PCLU`, `PCCHOLESKY`, `PCILU`, `PCICC`, `MatOrderingType`, `MATORDERINGEXTERNAL`, `MATORDERINGND`, `MATORDERINGRCM`
@*/
PetscErrorCode PCFactorSetMatOrderingType(PC pc, MatOrderingType ordering)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscTryMethod(pc, "PCFactorSetMatOrderingType_C", (PC, MatOrderingType), (pc, ordering));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PCFactorSetColumnPivot - Determines when column pivoting is done during matrix factorization.
      For PETSc dense matrices column pivoting is always done, for PETSc sparse matrices
      it is never done. For the MATLAB and `MATSOLVERSUPERLU` factorization this is used.

    Logically Collective

    Input Parameters:
+   pc - the preconditioner context
-   dtcol - 0.0 implies no pivoting, 1.0 complete pivoting (slower, requires more memory but more stable)

    Options Database Key:
.   -pc_factor_pivoting <dtcol> - perform the pivoting with the given tolerance

    Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `PCILU`, `PCICC`, `PCILUSetMatOrdering()`, `PCFactorSetPivotInBlocks()`
@*/
PetscErrorCode PCFactorSetColumnPivot(PC pc, PetscReal dtcol)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidLogicalCollectiveReal(pc, dtcol, 2);
  PetscTryMethod(pc, "PCFactorSetColumnPivot_C", (PC, PetscReal), (pc, dtcol));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PCFactorSetPivotInBlocks - Determines if pivoting is done while factoring each block
      with `MATBAIJ` or `MATSBAIJ` matrices

    Logically Collective

    Input Parameters:
+   pc - the preconditioner context
-   pivot - `PETSC_TRUE` or `PETSC_FALSE`

    Options Database Key:
.   -pc_factor_pivot_in_blocks <true,false> - Pivot inside matrix dense blocks for `MATBAIJ` and `MATSBAIJ`

    Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `PCILU`, `PCICC`, `PCILUSetMatOrdering()`, `PCFactorSetColumnPivot()`
@*/
PetscErrorCode PCFactorSetPivotInBlocks(PC pc, PetscBool pivot)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidLogicalCollectiveBool(pc, pivot, 2);
  PetscTryMethod(pc, "PCFactorSetPivotInBlocks_C", (PC, PetscBool), (pc, pivot));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCFactorSetReuseFill - When matrices with different nonzero structure are factored,
   this causes later ones to use the fill ratio computed in the initial factorization.

   Logically Collective

   Input Parameters:
+  pc - the preconditioner context
-  flag - `PETSC_TRUE` to reuse else `PETSC_FALSE`

   Options Database Key:
.  -pc_factor_reuse_fill - Activates `PCFactorSetReuseFill()`

   Level: intermediate

.seealso: `PCLU`, `PCCHOLESKY`, `PCILU`, `PCICC`, `PCFactorSetReuseOrdering()`, `PCFactorSetFill()`
@*/
PetscErrorCode PCFactorSetReuseFill(PC pc, PetscBool flag)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidLogicalCollectiveBool(pc, flag, 2);
  PetscTryMethod(pc, "PCFactorSetReuseFill_C", (PC, PetscBool), (pc, flag));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCFactorInitialize(PC pc, MatFactorType ftype)
{
  PC_Factor *fact = (PC_Factor *)pc->data;

  PetscFunctionBegin;
  PetscCall(MatFactorInfoInitialize(&fact->info));
  fact->factortype           = ftype;
  fact->info.shifttype       = (PetscReal)MAT_SHIFT_NONE;
  fact->info.shiftamount     = 100.0 * PETSC_MACHINE_EPSILON;
  fact->info.zeropivot       = 100.0 * PETSC_MACHINE_EPSILON;
  fact->info.pivotinblocks   = 1.0;
  pc->ops->getfactoredmatrix = PCFactorGetMatrix_Factor;

  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetZeroPivot_C", PCFactorSetZeroPivot_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetZeroPivot_C", PCFactorGetZeroPivot_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetShiftType_C", PCFactorSetShiftType_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetShiftType_C", PCFactorGetShiftType_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetShiftAmount_C", PCFactorSetShiftAmount_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetShiftAmount_C", PCFactorGetShiftAmount_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetMatSolverType_C", PCFactorGetMatSolverType_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetMatSolverType_C", PCFactorSetMatSolverType_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetUpMatSolverType_C", PCFactorSetUpMatSolverType_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetFill_C", PCFactorSetFill_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetMatOrderingType_C", PCFactorSetMatOrderingType_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetLevels_C", PCFactorSetLevels_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetLevels_C", PCFactorGetLevels_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetAllowDiagonalFill_C", PCFactorSetAllowDiagonalFill_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetAllowDiagonalFill_C", PCFactorGetAllowDiagonalFill_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetPivotInBlocks_C", PCFactorSetPivotInBlocks_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetUseInPlace_C", PCFactorSetUseInPlace_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetUseInPlace_C", PCFactorGetUseInPlace_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetReuseOrdering_C", PCFactorSetReuseOrdering_Factor));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetReuseFill_C", PCFactorSetReuseFill_Factor));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCFactorClearComposedFunctions(PC pc)
{
  PetscFunctionBegin;
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetZeroPivot_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetZeroPivot_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetShiftType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetShiftType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetShiftAmount_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetShiftAmount_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetMatSolverType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetMatSolverType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetUpMatSolverType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetFill_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetMatOrderingType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetLevels_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetLevels_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetAllowDiagonalFill_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetAllowDiagonalFill_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetPivotInBlocks_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetUseInPlace_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorGetUseInPlace_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetReuseOrdering_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetReuseFill_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorReorderForNonzeroDiagonal_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCFactorSetDropTolerance_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

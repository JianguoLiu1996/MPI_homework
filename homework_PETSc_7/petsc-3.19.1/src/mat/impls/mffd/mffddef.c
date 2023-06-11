
/*
  Implements the DS PETSc approach for computing the h
  parameter used with the finite difference based matrix-free
  Jacobian-vector products.

  To make your own: clone this file and modify for your needs.

  Mandatory functions:
  -------------------
      MatMFFDCompute_  - for a given point and direction computes h

      MatCreateMFFD _ - fills in the MatMFFD data structure
                           for this particular implementation

   Optional functions:
   -------------------
      MatMFFDView_ - prints information about the parameters being used.
                       This is called when SNESView() or -snes_view is used.

      MatMFFDSetFromOptions_ - checks the options database for options that
                               apply to this method.

      MatMFFDDestroy_ - frees any space allocated by the routines above

*/

/*
    This include file defines the data structure  MatMFFD that
   includes information about the computation of h. It is shared by
   all implementations that people provide
*/
#include <petsc/private/matimpl.h>
#include <../src/mat/impls/mffd/mffdimpl.h> /*I  "petscmat.h"   I*/

/*
      The  method has one parameter that is used to
   "cutoff" very small values. This is stored in a data structure
   that is only visible to this file. If your method has no parameters
   it can omit this, if it has several simply reorganize the data structure.
   The data structure is "hung-off" the MatMFFD data structure in
   the void *hctx; field.
*/
typedef struct {
  PetscReal umin; /* minimum allowable u'a value relative to |u|_1 */
} MatMFFD_DS;

static PetscErrorCode MatMFFDCompute_DS(MatMFFD ctx, Vec U, Vec a, PetscScalar *h, PetscBool *zeroa)
{
  MatMFFD_DS *hctx = (MatMFFD_DS *)ctx->hctx;
  PetscReal   nrm, sum, umin = hctx->umin;
  PetscScalar dot;

  PetscFunctionBegin;
  if (!(ctx->count % ctx->recomputeperiod)) {
    /*
     This algorithm requires 2 norms and 1 inner product. Rather than
     use directly the VecNorm() and VecDot() routines (and thus have
     three separate collective operations, we use the VecxxxBegin/End() routines
    */
    PetscCall(VecDotBegin(U, a, &dot));
    PetscCall(VecNormBegin(a, NORM_1, &sum));
    PetscCall(VecNormBegin(a, NORM_2, &nrm));
    PetscCall(VecDotEnd(U, a, &dot));
    PetscCall(VecNormEnd(a, NORM_1, &sum));
    PetscCall(VecNormEnd(a, NORM_2, &nrm));

    if (nrm == 0.0) {
      *zeroa = PETSC_TRUE;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
    *zeroa = PETSC_FALSE;

    /*
      Safeguard for step sizes that are "too small"
    */
    if (PetscAbsScalar(dot) < umin * sum && PetscRealPart(dot) >= 0.0) dot = umin * sum;
    else if (PetscAbsScalar(dot) < 0.0 && PetscRealPart(dot) > -umin * sum) dot = -umin * sum;
    *h = ctx->error_rel * dot / (nrm * nrm);
    PetscCheck(!PetscIsInfOrNanScalar(*h), PETSC_COMM_SELF, PETSC_ERR_PLIB, "Differencing parameter is not a number sum = %g dot = %g norm = %g", (double)sum, (double)PetscRealPart(dot), (double)nrm);
  } else {
    *h = ctx->currenth;
  }
  ctx->count++;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMFFDView_DS(MatMFFD ctx, PetscViewer viewer)
{
  MatMFFD_DS *hctx = (MatMFFD_DS *)ctx->hctx;
  PetscBool   iascii;

  PetscFunctionBegin;
  /*
     Currently this only handles the ascii file viewers, others
     could be added, but for this type of object other viewers
     make less sense
  */
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) PetscCall(PetscViewerASCIIPrintf(viewer, "    umin=%g (minimum iterate parameter)\n", (double)hctx->umin));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMFFDSetFromOptions_DS(MatMFFD ctx, PetscOptionItems *PetscOptionsObject)
{
  MatMFFD_DS *hctx = (MatMFFD_DS *)ctx->hctx;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "Finite difference matrix free parameters");
  PetscCall(PetscOptionsReal("-mat_mffd_umin", "umin", "MatMFFDDSSetUmin", hctx->umin, &hctx->umin, NULL));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode MatMFFDDestroy_DS(MatMFFD ctx)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(ctx->hctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   The following two routines use the PetscObjectCompose() and PetscObjectQuery()
   mechanism to allow the user to change the Umin parameter used in this method.
*/
PetscErrorCode MatMFFDDSSetUmin_DS(Mat mat, PetscReal umin)
{
  MatMFFD     ctx = NULL;
  MatMFFD_DS *hctx;

  PetscFunctionBegin;
  PetscCall(MatShellGetContext(mat, &ctx));
  PetscCheck(ctx, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "MatMFFDDSSetUmin() attached to non-shell matrix");
  hctx       = (MatMFFD_DS *)ctx->hctx;
  hctx->umin = umin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    MatMFFDDSSetUmin - Sets the "umin" parameter used by the
    PETSc routine for computing the differencing parameter, h, which is used
    for matrix-free Jacobian-vector products for a `MATMFFD` matrix.

   Input Parameters:
+  A - the `MATMFFD` matrix
-  umin - the parameter

   Level: advanced

   Note:
   See the manual page for `MatCreateSNESMF()` for a complete description of the
   algorithm used to compute h.

.seealso: `MATMFFD`, `MatMFFDSetFunctionError()`, `MatCreateSNESMF()`
@*/
PetscErrorCode MatMFFDDSSetUmin(Mat A, PetscReal umin)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, MAT_CLASSID, 1);
  PetscTryMethod(A, "MatMFFDDSSetUmin_C", (Mat, PetscReal), (A, umin));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
     MATMFFD_DS - algorithm for compute the "h" used in the finite difference matrix-free matrix vector product, `MatMult()`.

   Options Database Keys:
.  -mat_mffd_umin <umin> - see `MatMFFDDSSetUmin()`

   Level: intermediate

   Notes:
    Requires 2 norms and 1 inner product, but they are computed together
       so only one parallel collective operation is needed. See `MATMFFD_WP` for a method
       (with `KSPGMRES`) that requires NO collective operations.

   Formula used:
     F'(u)*a = [F(u+h*a) - F(u)]/h where
     h = error_rel*u'a/||a||^2                        if  |u'a| > umin*||a||_{1}
       = error_rel*umin*sign(u'a)*||a||_{1}/||a||^2   otherwise
 where
     error_rel = square root of relative error in function evaluation
     umin = minimum iterate parameter

  References:
.  * -  Dennis and Schnabel, "Numerical Methods for Unconstrained Optimization and Nonlinear Equations"

.seealso: `MATMFFD`, `MATMFFD_WP`, `MatCreateMFFD()`, `MatCreateSNESMF()`, `MATMFFD_WP`, `MatMFFDDSSetUmin()`
M*/
PETSC_EXTERN PetscErrorCode MatCreateMFFD_DS(MatMFFD ctx)
{
  MatMFFD_DS *hctx;

  PetscFunctionBegin;
  /* allocate my own private data structure */
  PetscCall(PetscNew(&hctx));
  ctx->hctx = (void *)hctx;
  /* set a default for my parameter */
  hctx->umin = 1.e-6;

  /* set the functions I am providing */
  ctx->ops->compute        = MatMFFDCompute_DS;
  ctx->ops->destroy        = MatMFFDDestroy_DS;
  ctx->ops->view           = MatMFFDView_DS;
  ctx->ops->setfromoptions = MatMFFDSetFromOptions_DS;

  PetscCall(PetscObjectComposeFunction((PetscObject)ctx->mat, "MatMFFDDSSetUmin_C", MatMFFDDSSetUmin_DS));
  PetscFunctionReturn(PETSC_SUCCESS);
}

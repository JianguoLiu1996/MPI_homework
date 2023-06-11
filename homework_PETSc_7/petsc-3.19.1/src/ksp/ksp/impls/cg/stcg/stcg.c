
#include <../src/ksp/ksp/impls/cg/stcg/stcgimpl.h> /*I "petscksp.h" I*/

#define STCG_PRECONDITIONED_DIRECTION   0
#define STCG_UNPRECONDITIONED_DIRECTION 1
#define STCG_DIRECTION_TYPES            2

static const char *DType_Table[64] = {"preconditioned", "unpreconditioned"};

static PetscErrorCode KSPCGSolve_STCG(KSP ksp)
{
#if defined(PETSC_USE_COMPLEX)
  SETERRQ(PetscObjectComm((PetscObject)ksp), PETSC_ERR_SUP, "STCG is not available for complex systems");
#else
  KSPCG_STCG *cg = (KSPCG_STCG *)ksp->data;
  Mat         Qmat, Mmat;
  Vec         r, z, p, d;
  PC          pc;
  PetscReal   norm_r, norm_d, norm_dp1, norm_p, dMp;
  PetscReal   alpha, beta, kappa, rz, rzm1;
  PetscReal   rr, r2, step;
  PetscInt    max_cg_its;
  PetscBool   diagonalscale;

  /***************************************************************************/
  /* Check the arguments and parameters.                                     */
  /***************************************************************************/

  PetscFunctionBegin;
  PetscCall(PCGetDiagonalScale(ksp->pc, &diagonalscale));
  PetscCheck(!diagonalscale, PetscObjectComm((PetscObject)ksp), PETSC_ERR_SUP, "Krylov method %s does not support diagonal scaling", ((PetscObject)ksp)->type_name);
  PetscCheck(cg->radius >= 0.0, PetscObjectComm((PetscObject)ksp), PETSC_ERR_ARG_OUTOFRANGE, "Input error: radius < 0");

  /***************************************************************************/
  /* Get the workspace vectors and initialize variables                      */
  /***************************************************************************/

  r2 = cg->radius * cg->radius;
  r  = ksp->work[0];
  z  = ksp->work[1];
  p  = ksp->work[2];
  d  = ksp->vec_sol;
  pc = ksp->pc;

  PetscCall(PCGetOperators(pc, &Qmat, &Mmat));

  PetscCall(VecGetSize(d, &max_cg_its));
  max_cg_its = PetscMin(max_cg_its, ksp->max_it);
  ksp->its   = 0;

  /***************************************************************************/
  /* Initialize objective function and direction.                            */
  /***************************************************************************/

  cg->o_fcn = 0.0;

  PetscCall(VecSet(d, 0.0)); /* d = 0             */
  cg->norm_d = 0.0;

  /***************************************************************************/
  /* Begin the conjugate gradient method.  Check the right-hand side for     */
  /* numerical problems.  The check for not-a-number and infinite values     */
  /* need be performed only once.                                            */
  /***************************************************************************/

  PetscCall(VecCopy(ksp->vec_rhs, r)); /* r = -grad         */
  PetscCall(VecDot(r, r, &rr));        /* rr = r^T r        */
  KSPCheckDot(ksp, rr);

  /***************************************************************************/
  /* Check the preconditioner for numerical problems and for positive        */
  /* definiteness.  The check for not-a-number and infinite values need be   */
  /* performed only once.                                                    */
  /***************************************************************************/

  PetscCall(KSP_PCApply(ksp, r, z)); /* z = inv(M) r      */
  PetscCall(VecDot(r, z, &rz));      /* rz = r^T inv(M) r */
  if (PetscIsInfOrNanScalar(rz)) {
    /*************************************************************************/
    /* The preconditioner contains not-a-number or an infinite value.        */
    /* Return the gradient direction intersected with the trust region.      */
    /*************************************************************************/

    ksp->reason = KSP_DIVERGED_NANORINF;
    PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: bad preconditioner: rz=%g\n", (double)rz));

    if (cg->radius != 0) {
      if (r2 >= rr) {
        alpha      = 1.0;
        cg->norm_d = PetscSqrtReal(rr);
      } else {
        alpha      = PetscSqrtReal(r2 / rr);
        cg->norm_d = cg->radius;
      }

      PetscCall(VecAXPY(d, alpha, r)); /* d = d + alpha r   */

      /***********************************************************************/
      /* Compute objective function.                                         */
      /***********************************************************************/

      PetscCall(KSP_MatMult(ksp, Qmat, d, z));
      PetscCall(VecAYPX(z, -0.5, ksp->vec_rhs));
      PetscCall(VecDot(d, z, &cg->o_fcn));
      cg->o_fcn = -cg->o_fcn;
      ++ksp->its;
    }
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  if (rz < 0.0) {
    /*************************************************************************/
    /* The preconditioner is indefinite.  Because this is the first          */
    /* and we do not have a direction yet, we use the gradient step.  Note   */
    /* that we cannot use the preconditioned norm when computing the step    */
    /* because the matrix is indefinite.                                     */
    /*************************************************************************/

    ksp->reason = KSP_DIVERGED_INDEFINITE_PC;
    PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: indefinite preconditioner: rz=%g\n", (double)rz));

    if (cg->radius != 0.0) {
      if (r2 >= rr) {
        alpha      = 1.0;
        cg->norm_d = PetscSqrtReal(rr);
      } else {
        alpha      = PetscSqrtReal(r2 / rr);
        cg->norm_d = cg->radius;
      }

      PetscCall(VecAXPY(d, alpha, r)); /* d = d + alpha r   */

      /***********************************************************************/
      /* Compute objective function.                                         */
      /***********************************************************************/

      PetscCall(KSP_MatMult(ksp, Qmat, d, z));
      PetscCall(VecAYPX(z, -0.5, ksp->vec_rhs));
      PetscCall(VecDot(d, z, &cg->o_fcn));
      cg->o_fcn = -cg->o_fcn;
      ++ksp->its;
    }
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  /***************************************************************************/
  /* As far as we know, the preconditioner is positive semidefinite.         */
  /* Compute and log the residual.  Check convergence because this           */
  /* initializes things, but do not terminate until at least one conjugate   */
  /* gradient iteration has been performed.                                  */
  /***************************************************************************/

  switch (ksp->normtype) {
  case KSP_NORM_PRECONDITIONED:
    PetscCall(VecNorm(z, NORM_2, &norm_r)); /* norm_r = |z|      */
    break;

  case KSP_NORM_UNPRECONDITIONED:
    norm_r = PetscSqrtReal(rr); /* norm_r = |r|      */
    break;

  case KSP_NORM_NATURAL:
    norm_r = PetscSqrtReal(rz); /* norm_r = |r|_M    */
    break;

  default:
    norm_r = 0.0;
    break;
  }

  PetscCall(KSPLogResidualHistory(ksp, norm_r));
  PetscCall(KSPMonitor(ksp, ksp->its, norm_r));
  ksp->rnorm = norm_r;

  PetscCall((*ksp->converged)(ksp, ksp->its, norm_r, &ksp->reason, ksp->cnvP));

  /***************************************************************************/
  /* Compute the first direction and update the iteration.                   */
  /***************************************************************************/

  PetscCall(VecCopy(z, p));                /* p = z             */
  PetscCall(KSP_MatMult(ksp, Qmat, p, z)); /* z = Q * p         */
  ++ksp->its;

  /***************************************************************************/
  /* Check the matrix for numerical problems.                                */
  /***************************************************************************/

  PetscCall(VecDot(p, z, &kappa)); /* kappa = p^T Q p   */
  if (PetscIsInfOrNanScalar(kappa)) {
    /*************************************************************************/
    /* The matrix produced not-a-number or an infinite value.  In this case, */
    /* we must stop and use the gradient direction.  This condition need     */
    /* only be checked once.                                                 */
    /*************************************************************************/

    ksp->reason = KSP_DIVERGED_NANORINF;
    PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: bad matrix: kappa=%g\n", (double)kappa));

    if (cg->radius) {
      if (r2 >= rr) {
        alpha      = 1.0;
        cg->norm_d = PetscSqrtReal(rr);
      } else {
        alpha      = PetscSqrtReal(r2 / rr);
        cg->norm_d = cg->radius;
      }

      PetscCall(VecAXPY(d, alpha, r)); /* d = d + alpha r   */

      /***********************************************************************/
      /* Compute objective function.                                         */
      /***********************************************************************/

      PetscCall(KSP_MatMult(ksp, Qmat, d, z));
      PetscCall(VecAYPX(z, -0.5, ksp->vec_rhs));
      PetscCall(VecDot(d, z, &cg->o_fcn));
      cg->o_fcn = -cg->o_fcn;
      ++ksp->its;
    }
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  /***************************************************************************/
  /* Initialize variables for calculating the norm of the direction.         */
  /***************************************************************************/

  dMp    = 0.0;
  norm_d = 0.0;
  switch (cg->dtype) {
  case STCG_PRECONDITIONED_DIRECTION:
    norm_p = rz;
    break;

  default:
    PetscCall(VecDot(p, p, &norm_p));
    break;
  }

  /***************************************************************************/
  /* Check for negative curvature.                                           */
  /***************************************************************************/

  if (kappa <= 0.0) {
    /*************************************************************************/
    /* In this case, the matrix is indefinite and we have encountered a      */
    /* direction of negative curvature.  Because negative curvature occurs   */
    /* during the first step, we must follow a direction.                    */
    /*************************************************************************/

    ksp->reason = ksp->converged_neg_curve ? KSP_CONVERGED_NEG_CURVE : KSP_DIVERGED_INDEFINITE_MAT;
    PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: negative curvature: kappa=%g\n", (double)kappa));

    if (cg->radius != 0.0 && norm_p > 0.0) {
      /***********************************************************************/
      /* Follow direction of negative curvature to the boundary of the       */
      /* trust region.                                                       */
      /***********************************************************************/

      step       = PetscSqrtReal(r2 / norm_p);
      cg->norm_d = cg->radius;

      PetscCall(VecAXPY(d, step, p)); /* d = d + step p    */

      /***********************************************************************/
      /* Update objective function.                                          */
      /***********************************************************************/

      cg->o_fcn += step * (0.5 * step * kappa - rz);
    } else if (cg->radius != 0.0) {
      /***********************************************************************/
      /* The norm of the preconditioned direction is zero; use the gradient  */
      /* step.                                                               */
      /***********************************************************************/

      if (r2 >= rr) {
        alpha      = 1.0;
        cg->norm_d = PetscSqrtReal(rr);
      } else {
        alpha      = PetscSqrtReal(r2 / rr);
        cg->norm_d = cg->radius;
      }

      PetscCall(VecAXPY(d, alpha, r)); /* d = d + alpha r   */

      /***********************************************************************/
      /* Compute objective function.                                         */
      /***********************************************************************/

      PetscCall(KSP_MatMult(ksp, Qmat, d, z));
      PetscCall(VecAYPX(z, -0.5, ksp->vec_rhs));
      PetscCall(VecDot(d, z, &cg->o_fcn));

      cg->o_fcn = -cg->o_fcn;
      ++ksp->its;
    }
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  /***************************************************************************/
  /* Run the conjugate gradient method until either the problem is solved,   */
  /* we encounter the boundary of the trust region, or the conjugate         */
  /* gradient method breaks down.                                            */
  /***************************************************************************/

  while (1) {
    /*************************************************************************/
    /* Know that kappa is nonzero, because we have not broken down, so we    */
    /* can compute the steplength.                                           */
    /*************************************************************************/

    alpha = rz / kappa;

    /*************************************************************************/
    /* Compute the steplength and check for intersection with the trust      */
    /* region.                                                               */
    /*************************************************************************/

    norm_dp1 = norm_d + alpha * (2.0 * dMp + alpha * norm_p);
    if (cg->radius != 0.0 && norm_dp1 >= r2) {
      /***********************************************************************/
      /* In this case, the matrix is positive definite as far as we know.    */
      /* However, the full step goes beyond the trust region.                */
      /***********************************************************************/

      ksp->reason = KSP_CONVERGED_STEP_LENGTH;
      PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: constrained step: radius=%g\n", (double)cg->radius));

      if (norm_p > 0.0) {
        /*********************************************************************/
        /* Follow the direction to the boundary of the trust region.         */
        /*********************************************************************/

        step       = (PetscSqrtReal(dMp * dMp + norm_p * (r2 - norm_d)) - dMp) / norm_p;
        cg->norm_d = cg->radius;

        PetscCall(VecAXPY(d, step, p)); /* d = d + step p    */

        /*********************************************************************/
        /* Update objective function.                                        */
        /*********************************************************************/

        cg->o_fcn += step * (0.5 * step * kappa - rz);
      } else {
        /*********************************************************************/
        /* The norm of the direction is zero; there is nothing to follow.    */
        /*********************************************************************/
      }
      break;
    }

    /*************************************************************************/
    /* Now we can update the direction and residual.                         */
    /*************************************************************************/

    PetscCall(VecAXPY(d, alpha, p));   /* d = d + alpha p   */
    PetscCall(VecAXPY(r, -alpha, z));  /* r = r - alpha Q p */
    PetscCall(KSP_PCApply(ksp, r, z)); /* z = inv(M) r      */

    switch (cg->dtype) {
    case STCG_PRECONDITIONED_DIRECTION:
      norm_d = norm_dp1;
      break;

    default:
      PetscCall(VecDot(d, d, &norm_d));
      break;
    }
    cg->norm_d = PetscSqrtReal(norm_d);

    /*************************************************************************/
    /* Update objective function.                                            */
    /*************************************************************************/

    cg->o_fcn -= 0.5 * alpha * rz;

    /*************************************************************************/
    /* Check that the preconditioner appears positive semidefinite.          */
    /*************************************************************************/

    rzm1 = rz;
    PetscCall(VecDot(r, z, &rz)); /* rz = r^T z        */
    if (rz < 0.0) {
      /***********************************************************************/
      /* The preconditioner is indefinite.                                   */
      /***********************************************************************/

      ksp->reason = KSP_DIVERGED_INDEFINITE_PC;
      PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: cg indefinite preconditioner: rz=%g\n", (double)rz));
      break;
    }

    /*************************************************************************/
    /* As far as we know, the preconditioner is positive semidefinite.       */
    /* Compute the residual and check for convergence.                       */
    /*************************************************************************/

    switch (ksp->normtype) {
    case KSP_NORM_PRECONDITIONED:
      PetscCall(VecNorm(z, NORM_2, &norm_r)); /* norm_r = |z|      */
      break;

    case KSP_NORM_UNPRECONDITIONED:
      PetscCall(VecNorm(r, NORM_2, &norm_r)); /* norm_r = |r|      */
      break;

    case KSP_NORM_NATURAL:
      norm_r = PetscSqrtReal(rz); /* norm_r = |r|_M    */
      break;

    default:
      norm_r = 0.0;
      break;
    }

    PetscCall(KSPLogResidualHistory(ksp, norm_r));
    PetscCall(KSPMonitor(ksp, ksp->its, norm_r));
    ksp->rnorm = norm_r;

    PetscCall((*ksp->converged)(ksp, ksp->its, norm_r, &ksp->reason, ksp->cnvP));
    if (ksp->reason) {
      /***********************************************************************/
      /* The method has converged.                                           */
      /***********************************************************************/

      PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: truncated step: rnorm=%g, radius=%g\n", (double)norm_r, (double)cg->radius));
      break;
    }

    /*************************************************************************/
    /* We have not converged yet.  Check for breakdown.                      */
    /*************************************************************************/

    beta = rz / rzm1;
    if (PetscAbsScalar(beta) <= 0.0) {
      /***********************************************************************/
      /* Conjugate gradients has broken down.                                */
      /***********************************************************************/

      ksp->reason = KSP_DIVERGED_BREAKDOWN;
      PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: breakdown: beta=%g\n", (double)beta));
      break;
    }

    /*************************************************************************/
    /* Check iteration limit.                                                */
    /*************************************************************************/

    if (ksp->its >= max_cg_its) {
      ksp->reason = KSP_DIVERGED_ITS;
      PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: iterlim: its=%" PetscInt_FMT "\n", ksp->its));
      break;
    }

    /*************************************************************************/
    /* Update p and the norms.                                               */
    /*************************************************************************/

    PetscCall(VecAYPX(p, beta, z)); /* p = z + beta p    */

    switch (cg->dtype) {
    case STCG_PRECONDITIONED_DIRECTION:
      dMp    = beta * (dMp + alpha * norm_p);
      norm_p = beta * (rzm1 + beta * norm_p);
      break;

    default:
      PetscCall(VecDot(d, p, &dMp));
      PetscCall(VecDot(p, p, &norm_p));
      break;
    }

    /*************************************************************************/
    /* Compute the new direction and update the iteration.                   */
    /*************************************************************************/

    PetscCall(KSP_MatMult(ksp, Qmat, p, z)); /* z = Q * p         */
    PetscCall(VecDot(p, z, &kappa));         /* kappa = p^T Q p   */
    ++ksp->its;

    /*************************************************************************/
    /* Check for negative curvature.                                         */
    /*************************************************************************/

    if (kappa <= 0.0) {
      /***********************************************************************/
      /* In this case, the matrix is indefinite and we have encountered      */
      /* a direction of negative curvature.  Follow the direction to the     */
      /* boundary of the trust region.                                       */
      /***********************************************************************/

      ksp->reason = ksp->converged_neg_curve ? KSP_CONVERGED_NEG_CURVE : KSP_DIVERGED_INDEFINITE_MAT;
      PetscCall(PetscInfo(ksp, "KSPCGSolve_STCG: negative curvature: kappa=%g\n", (double)kappa));

      if (cg->radius != 0.0 && norm_p > 0.0) {
        /*********************************************************************/
        /* Follow direction of negative curvature to boundary.               */
        /*********************************************************************/

        step       = (PetscSqrtReal(dMp * dMp + norm_p * (r2 - norm_d)) - dMp) / norm_p;
        cg->norm_d = cg->radius;

        PetscCall(VecAXPY(d, step, p)); /* d = d + step p    */

        /*********************************************************************/
        /* Update objective function.                                        */
        /*********************************************************************/

        cg->o_fcn += step * (0.5 * step * kappa - rz);
      } else if (cg->radius != 0.0) {
        /*********************************************************************/
        /* The norm of the direction is zero; there is nothing to follow.    */
        /*********************************************************************/
      }
      break;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
#endif
}

static PetscErrorCode KSPCGSetUp_STCG(KSP ksp)
{
  PetscFunctionBegin;
  /***************************************************************************/
  /* Set work vectors needed by conjugate gradient method and allocate       */
  /***************************************************************************/

  PetscCall(KSPSetWorkVecs(ksp, 3));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPCGDestroy_STCG(KSP ksp)
{
  PetscFunctionBegin;
  /***************************************************************************/
  /* Clear composed functions                                                */
  /***************************************************************************/

  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPCGSetRadius_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPCGGetNormD_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPCGGetObjFcn_C", NULL));

  /***************************************************************************/
  /* Destroy KSP object.                                                     */
  /***************************************************************************/

  PetscCall(KSPDestroyDefault(ksp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPCGSetRadius_STCG(KSP ksp, PetscReal radius)
{
  KSPCG_STCG *cg = (KSPCG_STCG *)ksp->data;

  PetscFunctionBegin;
  cg->radius = radius;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPCGGetNormD_STCG(KSP ksp, PetscReal *norm_d)
{
  KSPCG_STCG *cg = (KSPCG_STCG *)ksp->data;

  PetscFunctionBegin;
  *norm_d = cg->norm_d;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPCGGetObjFcn_STCG(KSP ksp, PetscReal *o_fcn)
{
  KSPCG_STCG *cg = (KSPCG_STCG *)ksp->data;

  PetscFunctionBegin;
  *o_fcn = cg->o_fcn;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPCGSetFromOptions_STCG(KSP ksp, PetscOptionItems *PetscOptionsObject)
{
  KSPCG_STCG *cg = (KSPCG_STCG *)ksp->data;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "KSPCG STCG options");
  PetscCall(PetscOptionsReal("-ksp_cg_radius", "Trust Region Radius", "KSPCGSetRadius", cg->radius, &cg->radius, NULL));
  PetscCall(PetscOptionsEList("-ksp_cg_dtype", "Norm used for direction", "", DType_Table, STCG_DIRECTION_TYPES, DType_Table[cg->dtype], &cg->dtype, NULL));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
     KSPSTCG -   Code to run conjugate gradient method subject to a constraint on the solution norm.

   Options Database Keys:
.      -ksp_cg_radius <r> - Trust Region Radius

   Notes:
    This is rarely used directly, it is used in Trust Region methods for nonlinear equations, `SNESNEWTONTR`

  Use preconditioned conjugate gradient to compute
  an approximate minimizer of the quadratic function

            q(s) = g^T * s + 0.5 * s^T * H * s

   subject to the trust region constraint

            || s || <= delta,

   where

     delta is the trust region radius,
     g is the gradient vector,
     H is the Hessian approximation, and
     M is the positive definite preconditioner matrix.

   `KSPConvergedReason` may be
.vb
   KSP_CONVERGED_NEG_CURVE if convergence is reached along a negative curvature direction,
   KSP_CONVERGED_STEP_LENGTH if convergence is reached along a constrained step,
.ve
   other `KSP` converged/diverged reasons

  The preconditioner supplied should be symmetric and positive definite.

   References:
+  * - Steihaug, T. (1983): The conjugate gradient method and trust regions in large scale optimization. SIAM J. Numer. Anal. 20, 626--637
-  * - Toint, Ph.L. (1981): Towards an efficient sparsity exploiting Newton method for minimization. In: Duff, I., ed., Sparse Matrices and Their Uses, pp. 57--88. Academic Press

   Level: developer

.seealso: [](chapter_ksp), `KSPCreate()`, `KSPCGSetType()`, `KSPType`, `KSP`, `KSPCGSetRadius()`, `KSPCGGetNormD()`, `KSPCGGetObjFcn()`, `KSPNASH`, `KSPGLTR`, `KSPQCG`
M*/

PETSC_EXTERN PetscErrorCode KSPCreate_STCG(KSP ksp)
{
  KSPCG_STCG *cg;

  PetscFunctionBegin;
  PetscCall(PetscNew(&cg));

  cg->radius = 0.0;
  cg->dtype  = STCG_UNPRECONDITIONED_DIRECTION;

  ksp->data = (void *)cg;
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_UNPRECONDITIONED, PC_LEFT, 3));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_PRECONDITIONED, PC_LEFT, 2));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_NATURAL, PC_LEFT, 2));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_NONE, PC_LEFT, 1));
  PetscCall(KSPSetConvergedNegativeCurvature(ksp, PETSC_TRUE));

  /***************************************************************************/
  /* Sets the functions that are associated with this data structure         */
  /* (in C++ this is the same as defining virtual functions).                */
  /***************************************************************************/

  ksp->ops->setup          = KSPCGSetUp_STCG;
  ksp->ops->solve          = KSPCGSolve_STCG;
  ksp->ops->destroy        = KSPCGDestroy_STCG;
  ksp->ops->setfromoptions = KSPCGSetFromOptions_STCG;
  ksp->ops->buildsolution  = KSPBuildSolutionDefault;
  ksp->ops->buildresidual  = KSPBuildResidualDefault;
  ksp->ops->view           = NULL;

  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPCGSetRadius_C", KSPCGSetRadius_STCG));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPCGGetNormD_C", KSPCGGetNormD_STCG));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPCGGetObjFcn_C", KSPCGGetObjFcn_STCG));
  PetscFunctionReturn(PETSC_SUCCESS);
}

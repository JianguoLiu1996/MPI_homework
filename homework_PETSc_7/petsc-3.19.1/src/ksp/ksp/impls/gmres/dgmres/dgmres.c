/*
    Implements deflated GMRES.
*/

#include <../src/ksp/ksp/impls/gmres/dgmres/dgmresimpl.h> /*I  "petscksp.h"  I*/

PetscLogEvent KSP_DGMRESComputeDeflationData, KSP_DGMRESApplyDeflation;

#define GMRES_DELTA_DIRECTIONS 10
#define GMRES_DEFAULT_MAXK     30
static PetscErrorCode KSPDGMRESGetNewVectors(KSP, PetscInt);
static PetscErrorCode KSPDGMRESUpdateHessenberg(KSP, PetscInt, PetscBool, PetscReal *);
static PetscErrorCode KSPDGMRESBuildSoln(PetscScalar *, Vec, Vec, KSP, PetscInt);

PetscErrorCode KSPDGMRESSetEigen(KSP ksp, PetscInt nb_eig)
{
  PetscFunctionBegin;
  PetscTryMethod((ksp), "KSPDGMRESSetEigen_C", (KSP, PetscInt), (ksp, nb_eig));
  PetscFunctionReturn(PETSC_SUCCESS);
}
PetscErrorCode KSPDGMRESSetMaxEigen(KSP ksp, PetscInt max_neig)
{
  PetscFunctionBegin;
  PetscTryMethod((ksp), "KSPDGMRESSetMaxEigen_C", (KSP, PetscInt), (ksp, max_neig));
  PetscFunctionReturn(PETSC_SUCCESS);
}
PetscErrorCode KSPDGMRESForce(KSP ksp, PetscBool force)
{
  PetscFunctionBegin;
  PetscTryMethod((ksp), "KSPDGMRESForce_C", (KSP, PetscBool), (ksp, force));
  PetscFunctionReturn(PETSC_SUCCESS);
}
PetscErrorCode KSPDGMRESSetRatio(KSP ksp, PetscReal ratio)
{
  PetscFunctionBegin;
  PetscTryMethod((ksp), "KSPDGMRESSetRatio_C", (KSP, PetscReal), (ksp, ratio));
  PetscFunctionReturn(PETSC_SUCCESS);
}
PetscErrorCode KSPDGMRESComputeSchurForm(KSP ksp, PetscInt *neig)
{
  PetscFunctionBegin;
  PetscUseMethod((ksp), "KSPDGMRESComputeSchurForm_C", (KSP, PetscInt *), (ksp, neig));
  PetscFunctionReturn(PETSC_SUCCESS);
}
PetscErrorCode KSPDGMRESComputeDeflationData(KSP ksp, PetscInt *curneigh)
{
  PetscFunctionBegin;
  PetscUseMethod((ksp), "KSPDGMRESComputeDeflationData_C", (KSP, PetscInt *), (ksp, curneigh));
  PetscFunctionReturn(PETSC_SUCCESS);
}
PetscErrorCode KSPDGMRESApplyDeflation(KSP ksp, Vec x, Vec y)
{
  PetscFunctionBegin;
  PetscUseMethod((ksp), "KSPDGMRESApplyDeflation_C", (KSP, Vec, Vec), (ksp, x, y));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPDGMRESImproveEig(KSP ksp, PetscInt neig)
{
  PetscFunctionBegin;
  PetscUseMethod((ksp), "KSPDGMRESImproveEig_C", (KSP, PetscInt), (ksp, neig));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPSetUp_DGMRES(KSP ksp)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;
  PetscInt    neig   = dgmres->neig + EIG_OFFSET;
  PetscInt    max_k  = dgmres->max_k + 1;

  PetscFunctionBegin;
  PetscCall(KSPSetUp_GMRES(ksp));
  if (!dgmres->neig) PetscFunctionReturn(PETSC_SUCCESS);

  /* Allocate workspace for the Schur vectors*/
  PetscCall(PetscMalloc1(neig * max_k, &SR));
  dgmres->wr    = NULL;
  dgmres->wi    = NULL;
  dgmres->perm  = NULL;
  dgmres->modul = NULL;
  dgmres->Q     = NULL;
  dgmres->Z     = NULL;

  UU   = NULL;
  XX   = NULL;
  MX   = NULL;
  AUU  = NULL;
  XMX  = NULL;
  XMU  = NULL;
  UMX  = NULL;
  AUAU = NULL;
  TT   = NULL;
  TTF  = NULL;
  INVP = NULL;
  X1   = NULL;
  X2   = NULL;
  MU   = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
 Run GMRES, possibly with restart.  Return residual history if requested.
 input parameters:

 .       gmres  - structure containing parameters and work areas

 output parameters:
 .        nres    - residuals (from preconditioned system) at each step.
 If restarting, consider passing nres+it.  If null,
 ignored
 .        itcount - number of iterations used.  nres[0] to nres[itcount]
 are defined.  If null, ignored.

 Notes:
 On entry, the value in vector VEC_VV(0) should be the initial residual
 (this allows shortcuts where the initial preconditioned residual is 0).
 */
PetscErrorCode KSPDGMRESCycle(PetscInt *itcount, KSP ksp)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)(ksp->data);
  PetscReal   res_norm, res, hapbnd, tt;
  PetscInt    it     = 0;
  PetscInt    max_k  = dgmres->max_k;
  PetscBool   hapend = PETSC_FALSE;
  PetscReal   res_old;
  PetscInt    test = 0;

  PetscFunctionBegin;
  PetscCall(VecNormalize(VEC_VV(0), &res_norm));
  KSPCheckNorm(ksp, res_norm);
  res     = res_norm;
  *GRS(0) = res_norm;

  /* check for the convergence */
  PetscCall(PetscObjectSAWsTakeAccess((PetscObject)ksp));
  if (ksp->normtype != KSP_NORM_NONE) ksp->rnorm = res;
  else ksp->rnorm = 0.0;
  PetscCall(PetscObjectSAWsGrantAccess((PetscObject)ksp));
  dgmres->it = (it - 1);
  PetscCall(KSPLogResidualHistory(ksp, ksp->rnorm));
  PetscCall(KSPMonitor(ksp, ksp->its, ksp->rnorm));
  if (!res) {
    if (itcount) *itcount = 0;
    ksp->reason = KSP_CONVERGED_ATOL;
    PetscCall(PetscInfo(ksp, "Converged due to zero residual norm on entry\n"));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  /* record the residual norm to test if deflation is needed */
  res_old = res;

  PetscCall((*ksp->converged)(ksp, ksp->its, ksp->rnorm, &ksp->reason, ksp->cnvP));
  while (!ksp->reason && it < max_k && ksp->its < ksp->max_it) {
    if (it) {
      PetscCall(KSPLogResidualHistory(ksp, ksp->rnorm));
      PetscCall(KSPMonitor(ksp, ksp->its, ksp->rnorm));
    }
    dgmres->it = (it - 1);
    if (dgmres->vv_allocated <= it + VEC_OFFSET + 1) PetscCall(KSPDGMRESGetNewVectors(ksp, it + 1));
    if (dgmres->r > 0) {
      if (ksp->pc_side == PC_LEFT) {
        /* Apply the first preconditioner */
        PetscCall(KSP_PCApplyBAorAB(ksp, VEC_VV(it), VEC_TEMP, VEC_TEMP_MATOP));
        /* Then apply Deflation as a preconditioner */
        PetscCall(KSPDGMRESApplyDeflation(ksp, VEC_TEMP, VEC_VV(1 + it)));
      } else if (ksp->pc_side == PC_RIGHT) {
        PetscCall(KSPDGMRESApplyDeflation(ksp, VEC_VV(it), VEC_TEMP));
        PetscCall(KSP_PCApplyBAorAB(ksp, VEC_TEMP, VEC_VV(1 + it), VEC_TEMP_MATOP));
      }
    } else {
      PetscCall(KSP_PCApplyBAorAB(ksp, VEC_VV(it), VEC_VV(1 + it), VEC_TEMP_MATOP));
    }
    dgmres->matvecs += 1;
    /* update hessenberg matrix and do Gram-Schmidt */
    PetscCall((*dgmres->orthog)(ksp, it));

    /* vv(i+1) . vv(i+1) */
    PetscCall(VecNormalize(VEC_VV(it + 1), &tt));
    /* save the magnitude */
    *HH(it + 1, it)  = tt;
    *HES(it + 1, it) = tt;

    /* check for the happy breakdown */
    hapbnd = PetscAbsScalar(tt / *GRS(it));
    if (hapbnd > dgmres->haptol) hapbnd = dgmres->haptol;
    if (tt < hapbnd) {
      PetscCall(PetscInfo(ksp, "Detected happy breakdown, current hapbnd = %g tt = %g\n", (double)hapbnd, (double)tt));
      hapend = PETSC_TRUE;
    }
    PetscCall(KSPDGMRESUpdateHessenberg(ksp, it, hapend, &res));

    it++;
    dgmres->it = (it - 1); /* For converged */
    ksp->its++;
    if (ksp->normtype != KSP_NORM_NONE) ksp->rnorm = res;
    else ksp->rnorm = 0.0;
    if (ksp->reason) break;

    PetscCall((*ksp->converged)(ksp, ksp->its, ksp->rnorm, &ksp->reason, ksp->cnvP));

    /* Catch error in happy breakdown and signal convergence and break from loop */
    if (hapend) {
      if (!ksp->reason) {
        PetscCheck(!ksp->errorifnotconverged, PetscObjectComm((PetscObject)ksp), PETSC_ERR_NOT_CONVERGED, "You reached the happy break down, but convergence was not indicated. Residual norm = %g", (double)res);
        ksp->reason = KSP_DIVERGED_BREAKDOWN;
        break;
      }
    }
  }

  /* Monitor if we know that we will not return for a restart */
  if (it && (ksp->reason || ksp->its >= ksp->max_it)) {
    PetscCall(KSPLogResidualHistory(ksp, ksp->rnorm));
    PetscCall(KSPMonitor(ksp, ksp->its, ksp->rnorm));
  }
  if (itcount) *itcount = it;

  /*
   Down here we have to solve for the "best" coefficients of the Krylov
   columns, add the solution values together, and possibly unwind the
   preconditioning from the solution
   */
  /* Form the solution (or the solution so far) */
  PetscCall(KSPDGMRESBuildSoln(GRS(0), ksp->vec_sol, ksp->vec_sol, ksp, it - 1));

  /* Compute data for the deflation to be used during the next restart */
  if (!ksp->reason && ksp->its < ksp->max_it) {
    test = max_k * PetscLogReal(ksp->rtol / res) / PetscLogReal(res / res_old);
    /* Compute data for the deflation if the residual rtol will not be reached in the remaining number of steps allowed  */
    if ((test > dgmres->smv * (ksp->max_it - ksp->its)) || dgmres->force) PetscCall(KSPDGMRESComputeDeflationData(ksp, NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPSolve_DGMRES(KSP ksp)
{
  PetscInt    i, its, itcount;
  KSP_DGMRES *dgmres     = (KSP_DGMRES *)ksp->data;
  PetscBool   guess_zero = ksp->guess_zero;

  PetscFunctionBegin;
  PetscCheck(!ksp->calc_sings || dgmres->Rsvd, PetscObjectComm((PetscObject)ksp), PETSC_ERR_ORDER, "Must call KSPSetComputeSingularValues() before KSPSetUp() is called");

  PetscCall(PetscObjectSAWsTakeAccess((PetscObject)ksp));
  ksp->its        = 0;
  dgmres->matvecs = 0;
  PetscCall(PetscObjectSAWsGrantAccess((PetscObject)ksp));

  itcount = 0;
  while (!ksp->reason) {
    PetscCall(KSPInitialResidual(ksp, ksp->vec_sol, VEC_TEMP, VEC_TEMP_MATOP, VEC_VV(0), ksp->vec_rhs));
    if (ksp->pc_side == PC_LEFT) {
      dgmres->matvecs += 1;
      if (dgmres->r > 0) {
        PetscCall(KSPDGMRESApplyDeflation(ksp, VEC_VV(0), VEC_TEMP));
        PetscCall(VecCopy(VEC_TEMP, VEC_VV(0)));
      }
    }

    PetscCall(KSPDGMRESCycle(&its, ksp));
    itcount += its;
    if (itcount >= ksp->max_it) {
      if (!ksp->reason) ksp->reason = KSP_DIVERGED_ITS;
      break;
    }
    ksp->guess_zero = PETSC_FALSE; /* every future call to KSPInitialResidual() will have nonzero guess */
  }
  ksp->guess_zero = guess_zero; /* restore if user provided nonzero initial guess */

  for (i = 0; i < dgmres->r; i++) PetscCall(VecViewFromOptions(UU[i], (PetscObject)ksp, "-ksp_dgmres_view_deflation_vecs"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPDestroy_DGMRES(KSP ksp)
{
  KSP_DGMRES *dgmres   = (KSP_DGMRES *)ksp->data;
  PetscInt    neig1    = dgmres->neig + EIG_OFFSET;
  PetscInt    max_neig = dgmres->max_neig;

  PetscFunctionBegin;
  if (dgmres->r) {
    PetscCall(VecDestroyVecs(max_neig, &UU));
    PetscCall(VecDestroyVecs(max_neig, &MU));
    if (XX) {
      PetscCall(VecDestroyVecs(neig1, &XX));
      PetscCall(VecDestroyVecs(neig1, &MX));
    }
    PetscCall(PetscFree(TT));
    PetscCall(PetscFree(TTF));
    PetscCall(PetscFree(INVP));
    PetscCall(PetscFree(XMX));
    PetscCall(PetscFree(UMX));
    PetscCall(PetscFree(XMU));
    PetscCall(PetscFree(X1));
    PetscCall(PetscFree(X2));
    PetscCall(PetscFree(dgmres->work));
    PetscCall(PetscFree(dgmres->iwork));
    PetscCall(PetscFree(dgmres->wr));
    PetscCall(PetscFree(dgmres->wi));
    PetscCall(PetscFree(dgmres->modul));
    PetscCall(PetscFree(dgmres->Q));
    PetscCall(PetscFree(ORTH));
    PetscCall(PetscFree(AUAU));
    PetscCall(PetscFree(AUU));
    PetscCall(PetscFree(SR2));
  }
  PetscCall(PetscFree(SR));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESSetEigen_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESSetMaxEigen_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESSetRatio_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESForce_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESComputeSchurForm_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESComputeDeflationData_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESApplyDeflation_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESImproveEig_C", NULL));
  PetscCall(KSPDestroy_GMRES(ksp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
 KSPDGMRESBuildSoln - create the solution from the starting vector and the
 current iterates.

 Input parameters:
 nrs - work area of size it + 1.
 vs  - index of initial guess
 vdest - index of result.  Note that vs may == vdest (replace
 guess with the solution).

 This is an internal routine that knows about the GMRES internals.
 */
static PetscErrorCode KSPDGMRESBuildSoln(PetscScalar *nrs, Vec vs, Vec vdest, KSP ksp, PetscInt it)
{
  PetscScalar tt;
  PetscInt    ii, k, j;
  KSP_DGMRES *dgmres = (KSP_DGMRES *)(ksp->data);

  /* Solve for solution vector that minimizes the residual */

  PetscFunctionBegin;
  /* If it is < 0, no gmres steps have been performed */
  if (it < 0) {
    PetscCall(VecCopy(vs, vdest)); /* VecCopy() is smart, exists immediately if vguess == vdest */
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCheck(*HH(it, it) != 0.0, PetscObjectComm((PetscObject)ksp), PETSC_ERR_CONV_FAILED, "Likely your matrix is the zero operator. HH(it,it) is identically zero; it = %" PetscInt_FMT " GRS(it) = %g", it, (double)PetscAbsScalar(*GRS(it)));
  if (*HH(it, it) != 0.0) nrs[it] = *GRS(it) / *HH(it, it);
  else nrs[it] = 0.0;

  for (ii = 1; ii <= it; ii++) {
    k  = it - ii;
    tt = *GRS(k);
    for (j = k + 1; j <= it; j++) tt = tt - *HH(k, j) * nrs[j];
    PetscCheck(*HH(k, k) != 0.0, PetscObjectComm((PetscObject)ksp), PETSC_ERR_CONV_FAILED, "Likely your matrix is singular. HH(k,k) is identically zero; it = %" PetscInt_FMT " k = %" PetscInt_FMT, it, k);
    nrs[k] = tt / *HH(k, k);
  }

  /* Accumulate the correction to the solution of the preconditioned problem in TEMP */
  PetscCall(VecSet(VEC_TEMP, 0.0));
  PetscCall(VecMAXPY(VEC_TEMP, it + 1, nrs, &VEC_VV(0)));

  /* Apply deflation */
  if (ksp->pc_side == PC_RIGHT && dgmres->r > 0) {
    PetscCall(KSPDGMRESApplyDeflation(ksp, VEC_TEMP, VEC_TEMP_MATOP));
    PetscCall(VecCopy(VEC_TEMP_MATOP, VEC_TEMP));
  }
  PetscCall(KSPUnwindPreconditioner(ksp, VEC_TEMP, VEC_TEMP_MATOP));

  /* add solution to previous solution */
  if (vdest != vs) PetscCall(VecCopy(vs, vdest));
  PetscCall(VecAXPY(vdest, 1.0, VEC_TEMP));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
 Do the scalar work for the orthogonalization.  Return new residual norm.
 */
static PetscErrorCode KSPDGMRESUpdateHessenberg(KSP ksp, PetscInt it, PetscBool hapend, PetscReal *res)
{
  PetscScalar *hh, *cc, *ss, tt;
  PetscInt     j;
  KSP_DGMRES  *dgmres = (KSP_DGMRES *)(ksp->data);

  PetscFunctionBegin;
  hh = HH(0, it);
  cc = CC(0);
  ss = SS(0);

  /* Apply all the previously computed plane rotations to the new column
   of the Hessenberg matrix */
  for (j = 1; j <= it; j++) {
    tt  = *hh;
    *hh = PetscConj(*cc) * tt + *ss * *(hh + 1);
    hh++;
    *hh = *cc++ * *hh - (*ss++ * tt);
  }

  /*
   compute the new plane rotation, and apply it to:
   1) the right-hand-side of the Hessenberg system
   2) the new column of the Hessenberg matrix
   thus obtaining the updated value of the residual
   */
  if (!hapend) {
    tt = PetscSqrtScalar(PetscConj(*hh) * *hh + PetscConj(*(hh + 1)) * *(hh + 1));
    if (tt == 0.0) {
      ksp->reason = KSP_DIVERGED_NULL;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
    *cc          = *hh / tt;
    *ss          = *(hh + 1) / tt;
    *GRS(it + 1) = -(*ss * *GRS(it));
    *GRS(it)     = PetscConj(*cc) * *GRS(it);
    *hh          = PetscConj(*cc) * *hh + *ss * *(hh + 1);
    *res         = PetscAbsScalar(*GRS(it + 1));
  } else {
    /* happy breakdown: HH(it+1, it) = 0, therefore we don't need to apply
     another rotation matrix (so RH doesn't change).  The new residual is
     always the new sine term times the residual from last time (GRS(it)),
     but now the new sine rotation would be zero...so the residual should
     be zero...so we will multiply "zero" by the last residual.  This might
     not be exactly what we want to do here -could just return "zero". */

    *res = 0.0;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Allocates more work vectors, starting from VEC_VV(it).
 */
static PetscErrorCode KSPDGMRESGetNewVectors(KSP ksp, PetscInt it)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;
  PetscInt    nwork  = dgmres->nwork_alloc, k, nalloc;

  PetscFunctionBegin;
  nalloc = PetscMin(ksp->max_it, dgmres->delta_allocate);
  /* Adjust the number to allocate to make sure that we don't exceed the
   number of available slots */
  if (it + VEC_OFFSET + nalloc >= dgmres->vecs_allocated) nalloc = dgmres->vecs_allocated - it - VEC_OFFSET;
  if (!nalloc) PetscFunctionReturn(PETSC_SUCCESS);

  dgmres->vv_allocated += nalloc;

  PetscCall(KSPCreateVecs(ksp, nalloc, &dgmres->user_work[nwork], 0, NULL));

  dgmres->mwork_alloc[nwork] = nalloc;
  for (k = 0; k < nalloc; k++) dgmres->vecs[it + VEC_OFFSET + k] = dgmres->user_work[nwork][k];
  dgmres->nwork_alloc++;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPBuildSolution_DGMRES(KSP ksp, Vec ptr, Vec *result)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;

  PetscFunctionBegin;
  if (!ptr) {
    if (!dgmres->sol_temp) PetscCall(VecDuplicate(ksp->vec_sol, &dgmres->sol_temp));
    ptr = dgmres->sol_temp;
  }
  if (!dgmres->nrs) {
    /* allocate the work area */
    PetscCall(PetscMalloc1(dgmres->max_k, &dgmres->nrs));
  }
  PetscCall(KSPDGMRESBuildSoln(dgmres->nrs, ksp->vec_sol, ptr, ksp, dgmres->it));
  if (result) *result = ptr;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPView_DGMRES(KSP ksp, PetscViewer viewer)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;
  PetscBool   iascii, isharmonic;

  PetscFunctionBegin;
  PetscCall(KSPView_GMRES(ksp, viewer));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Adaptive strategy is used: %s\n", PetscBools[dgmres->force]));
    PetscCall(PetscOptionsHasName(((PetscObject)ksp)->options, ((PetscObject)ksp)->prefix, "-ksp_dgmres_harmonic_ritz", &isharmonic));
    if (isharmonic) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "   Frequency of extracted eigenvalues = %" PetscInt_FMT " using Harmonic Ritz values \n", dgmres->neig));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "   Frequency of extracted eigenvalues = %" PetscInt_FMT " using Ritz values \n", dgmres->neig));
    }
    PetscCall(PetscViewerASCIIPrintf(viewer, "   Total number of extracted eigenvalues = %" PetscInt_FMT "\n", dgmres->r));
    PetscCall(PetscViewerASCIIPrintf(viewer, "   Maximum number of eigenvalues set to be extracted = %" PetscInt_FMT "\n", dgmres->max_neig));
    PetscCall(PetscViewerASCIIPrintf(viewer, "   relaxation parameter for the adaptive strategy(smv)  = %g\n", (double)dgmres->smv));
    PetscCall(PetscViewerASCIIPrintf(viewer, "   Number of matvecs : %" PetscInt_FMT "\n", dgmres->matvecs));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPDGMRESSetEigen_DGMRES(KSP ksp, PetscInt neig)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;

  PetscFunctionBegin;
  PetscCheck(neig >= 0 && neig <= dgmres->max_k, PetscObjectComm((PetscObject)ksp), PETSC_ERR_ARG_OUTOFRANGE, "The value of neig must be positive and less than the restart value ");
  dgmres->neig = neig;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPDGMRESSetMaxEigen_DGMRES(KSP ksp, PetscInt max_neig)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;

  PetscFunctionBegin;
  PetscCheck(max_neig >= 0 && max_neig <= dgmres->max_k, PetscObjectComm((PetscObject)ksp), PETSC_ERR_ARG_OUTOFRANGE, "The value of max_neig must be positive and less than the restart value ");
  dgmres->max_neig = max_neig;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPDGMRESSetRatio_DGMRES(KSP ksp, PetscReal ratio)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;

  PetscFunctionBegin;
  PetscCheck(ratio > 0, PetscObjectComm((PetscObject)ksp), PETSC_ERR_ARG_OUTOFRANGE, "The relaxation parameter value must be positive");
  dgmres->smv = ratio;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPDGMRESForce_DGMRES(KSP ksp, PetscBool force)
{
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;

  PetscFunctionBegin;
  dgmres->force = force;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPSetFromOptions_DGMRES(KSP ksp, PetscOptionItems *PetscOptionsObject)
{
  PetscInt    neig;
  PetscInt    max_neig;
  KSP_DGMRES *dgmres = (KSP_DGMRES *)ksp->data;
  PetscBool   flg;

  PetscFunctionBegin;
  PetscCall(KSPSetFromOptions_GMRES(ksp, PetscOptionsObject));
  PetscOptionsHeadBegin(PetscOptionsObject, "KSP DGMRES Options");
  PetscCall(PetscOptionsInt("-ksp_dgmres_eigen", "Number of smallest eigenvalues to extract at each restart", "KSPDGMRESSetEigen", dgmres->neig, &neig, &flg));
  if (flg) PetscCall(KSPDGMRESSetEigen(ksp, neig));
  PetscCall(PetscOptionsInt("-ksp_dgmres_max_eigen", "Maximum Number of smallest eigenvalues to extract ", "KSPDGMRESSetMaxEigen", dgmres->max_neig, &max_neig, &flg));
  if (flg) PetscCall(KSPDGMRESSetMaxEigen(ksp, max_neig));
  PetscCall(PetscOptionsReal("-ksp_dgmres_ratio", "Relaxation parameter for the smaller number of matrix-vectors product allowed", "KSPDGMRESSetRatio", dgmres->smv, &dgmres->smv, NULL));
  PetscCall(PetscOptionsBool("-ksp_dgmres_improve", "Improve the computation of eigenvalues by solving a new generalized eigenvalue problem (experimental - not stable at this time)", NULL, dgmres->improve, &dgmres->improve, NULL));
  PetscCall(PetscOptionsBool("-ksp_dgmres_force", "Sets DGMRES always at restart active, i.e do not use the adaptive strategy", "KSPDGMRESForce", dgmres->force, &dgmres->force, NULL));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPDGMRESComputeDeflationData_DGMRES(KSP ksp, PetscInt *ExtrNeig)
{
  KSP_DGMRES  *dgmres = (KSP_DGMRES *)ksp->data;
  PetscInt     i, j, k;
  PetscBLASInt nr, bmax;
  PetscInt     r = dgmres->r;
  PetscInt     neig;                                 /* number of eigenvalues to extract at each restart */
  PetscInt     neig1    = dgmres->neig + EIG_OFFSET; /* max number of eig that can be extracted at each restart */
  PetscInt     max_neig = dgmres->max_neig;          /* Max number of eigenvalues to extract during the iterative process */
  PetscInt     N        = dgmres->max_k + 1;
  PetscInt     n        = dgmres->it + 1;
  PetscReal    alpha;

  PetscFunctionBegin;
  PetscCall(PetscLogEventBegin(KSP_DGMRESComputeDeflationData, ksp, 0, 0, 0));
  if (dgmres->neig == 0 || (max_neig < (r + neig1) && !dgmres->improve)) {
    PetscCall(PetscLogEventEnd(KSP_DGMRESComputeDeflationData, ksp, 0, 0, 0));
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  PetscCall(KSPDGMRESComputeSchurForm(ksp, &neig));
  /* Form the extended Schur vectors X=VV*Sr */
  if (!XX) PetscCall(VecDuplicateVecs(VEC_VV(0), neig1, &XX));
  for (j = 0; j < neig; j++) {
    PetscCall(VecZeroEntries(XX[j]));
    PetscCall(VecMAXPY(XX[j], n, &SR[j * N], &VEC_VV(0)));
  }

  /* Orthogonalize X against U */
  if (!ORTH) PetscCall(PetscMalloc1(max_neig, &ORTH));
  if (r > 0) {
    /* modified Gram-Schmidt */
    for (j = 0; j < neig; j++) {
      for (i = 0; i < r; i++) {
        /* First, compute U'*X[j] */
        PetscCall(VecDot(XX[j], UU[i], &alpha));
        /* Then, compute X(j)=X(j)-U*U'*X(j) */
        PetscCall(VecAXPY(XX[j], -alpha, UU[i]));
      }
    }
  }
  /* Compute MX = M^{-1}*A*X */
  if (!MX) PetscCall(VecDuplicateVecs(VEC_VV(0), neig1, &MX));
  for (j = 0; j < neig; j++) PetscCall(KSP_PCApplyBAorAB(ksp, XX[j], MX[j], VEC_TEMP_MATOP));
  dgmres->matvecs += neig;

  if ((r + neig1) > max_neig && dgmres->improve) { /* Improve the approximate eigenvectors in X by solving a new generalized eigenvalue -- Quite expensive to do this actually */
    PetscCall(KSPDGMRESImproveEig(ksp, neig));
    PetscCall(PetscLogEventEnd(KSP_DGMRESComputeDeflationData, ksp, 0, 0, 0));
    PetscFunctionReturn(PETSC_SUCCESS); /* We return here since data for M have been improved in  KSPDGMRESImproveEig()*/
  }

  /* Compute XMX = X'*M^{-1}*A*X -- size (neig, neig) */
  if (!XMX) PetscCall(PetscMalloc1(neig1 * neig1, &XMX));
  for (j = 0; j < neig; j++) PetscCall(VecMDot(MX[j], neig, XX, &(XMX[j * neig1])));

  if (r > 0) {
    /* Compute UMX = U'*M^{-1}*A*X -- size (r, neig) */
    if (!UMX) PetscCall(PetscMalloc1(max_neig * neig1, &UMX));
    for (j = 0; j < neig; j++) PetscCall(VecMDot(MX[j], r, UU, &(UMX[j * max_neig])));
    /* Compute XMU = X'*M^{-1}*A*U -- size(neig, r) */
    if (!XMU) PetscCall(PetscMalloc1(max_neig * neig1, &XMU));
    for (j = 0; j < r; j++) PetscCall(VecMDot(MU[j], neig, XX, &(XMU[j * neig1])));
  }

  /* Form the new matrix T = [T UMX; XMU XMX]; */
  if (!TT) PetscCall(PetscMalloc1(max_neig * max_neig, &TT));
  if (r > 0) {
    /* Add XMU to T */
    for (j = 0; j < r; j++) PetscCall(PetscArraycpy(&(TT[max_neig * j + r]), &(XMU[neig1 * j]), neig));
    /* Add [UMX; XMX] to T */
    for (j = 0; j < neig; j++) {
      k = r + j;
      PetscCall(PetscArraycpy(&(TT[max_neig * k]), &(UMX[max_neig * j]), r));
      PetscCall(PetscArraycpy(&(TT[max_neig * k + r]), &(XMX[neig1 * j]), neig));
    }
  } else { /* Add XMX to T */
    for (j = 0; j < neig; j++) PetscCall(PetscArraycpy(&(TT[max_neig * j]), &(XMX[neig1 * j]), neig));
  }

  dgmres->r += neig;
  r = dgmres->r;
  PetscCall(PetscBLASIntCast(r, &nr));
  /*LU Factorize T with Lapack xgetrf routine */

  PetscCall(PetscBLASIntCast(max_neig, &bmax));
  if (!TTF) PetscCall(PetscMalloc1(bmax * bmax, &TTF));
  PetscCall(PetscArraycpy(TTF, TT, bmax * r));
  if (!INVP) PetscCall(PetscMalloc1(bmax, &INVP));
  {
    PetscBLASInt info;
    PetscCallBLAS("LAPACKgetrf", LAPACKgetrf_(&nr, &nr, TTF, &bmax, INVP, &info));
    PetscCheck(!info, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "Error in LAPACK routine XGETRF INFO=%d", (int)info);
  }

  /* Save X in U and MX in MU for the next cycles and increase the size of the invariant subspace */
  if (!UU) {
    PetscCall(VecDuplicateVecs(VEC_VV(0), max_neig, &UU));
    PetscCall(VecDuplicateVecs(VEC_VV(0), max_neig, &MU));
  }
  for (j = 0; j < neig; j++) {
    PetscCall(VecCopy(XX[j], UU[r - neig + j]));
    PetscCall(VecCopy(MX[j], MU[r - neig + j]));
  }
  PetscCall(PetscLogEventEnd(KSP_DGMRESComputeDeflationData, ksp, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPDGMRESComputeSchurForm_DGMRES(KSP ksp, PetscInt *neig)
{
  KSP_DGMRES   *dgmres = (KSP_DGMRES *)ksp->data;
  PetscInt      N = dgmres->max_k + 1, n = dgmres->it + 1;
  PetscBLASInt  bn;
  PetscReal    *A;
  PetscBLASInt  ihi;
  PetscBLASInt  ldA = 0; /* leading dimension of A */
  PetscBLASInt  ldQ;     /* leading dimension of Q */
  PetscReal    *Q;       /*  orthogonal matrix of  (left) Schur vectors */
  PetscReal    *work;    /* working vector */
  PetscBLASInt  lwork;   /* size of the working vector */
  PetscInt     *perm;    /* Permutation vector to sort eigenvalues */
  PetscInt      i, j;
  PetscBLASInt  NbrEig;          /* Number of eigenvalues really extracted */
  PetscReal    *wr, *wi, *modul; /* Real and imaginary part and modulus of the eigenvalues of A */
  PetscBLASInt *select;
  PetscBLASInt *iwork;
  PetscBLASInt  liwork;
  PetscScalar  *Ht;   /* Transpose of the Hessenberg matrix */
  PetscScalar  *t;    /* Store the result of the solution of H^T*t=h_{m+1,m}e_m */
  PetscBLASInt *ipiv; /* Permutation vector to be used in LAPACK */
  PetscBool     flag; /* determine whether to use Ritz vectors or harmonic Ritz vectors */

  PetscFunctionBegin;
  PetscCall(PetscBLASIntCast(n, &bn));
  PetscCall(PetscBLASIntCast(N, &ldA));
  ihi = ldQ = bn;
  PetscCall(PetscBLASIntCast(5 * N, &lwork));

#if defined(PETSC_USE_COMPLEX)
  SETERRQ(PetscObjectComm((PetscObject)ksp), PETSC_ERR_SUP, "No support for complex numbers.");
#endif

  PetscCall(PetscMalloc1(ldA * ldA, &A));
  PetscCall(PetscMalloc1(ldQ * n, &Q));
  PetscCall(PetscMalloc1(lwork, &work));
  if (!dgmres->wr) {
    PetscCall(PetscMalloc1(n, &dgmres->wr));
    PetscCall(PetscMalloc1(n, &dgmres->wi));
  }
  wr = dgmres->wr;
  wi = dgmres->wi;
  PetscCall(PetscMalloc1(n, &modul));
  PetscCall(PetscMalloc1(n, &perm));
  /* copy the Hessenberg matrix to work space */
  PetscCall(PetscArraycpy(A, dgmres->hes_origin, ldA * ldA));
  PetscCall(PetscOptionsHasName(((PetscObject)ksp)->options, ((PetscObject)ksp)->prefix, "-ksp_dgmres_harmonic_ritz", &flag));
  if (flag) {
    /* Compute the matrix H + H^{-T}*h^2_{m+1,m}e_m*e_m^T */
    /* Transpose the Hessenberg matrix */
    PetscCall(PetscMalloc1(bn * bn, &Ht));
    for (i = 0; i < bn; i++) {
      for (j = 0; j < bn; j++) Ht[i * bn + j] = dgmres->hes_origin[j * ldA + i];
    }

    /* Solve the system H^T*t = h_{m+1,m}e_m */
    PetscCall(PetscCalloc1(bn, &t));
    t[bn - 1] = dgmres->hes_origin[(bn - 1) * ldA + bn]; /* Pick the last element H(m+1,m) */
    PetscCall(PetscMalloc1(bn, &ipiv));
    /* Call the LAPACK routine dgesv to solve the system Ht^-1 * t */
    {
      PetscBLASInt info;
      PetscBLASInt nrhs = 1;
      PetscCallBLAS("LAPACKgesv", LAPACKgesv_(&bn, &nrhs, Ht, &bn, ipiv, t, &bn, &info));
      PetscCheck(!info, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "Error while calling the Lapack routine DGESV");
    }
    /* Now form H + H^{-T}*h^2_{m+1,m}e_m*e_m^T */
    for (i = 0; i < bn; i++) A[(bn - 1) * bn + i] += t[i];
    PetscCall(PetscFree(t));
    PetscCall(PetscFree(Ht));
  }
  /* Compute eigenvalues with the Schur form */
  {
    PetscBLASInt info = 0;
    PetscBLASInt ilo  = 1;
    PetscCallBLAS("LAPACKhseqr", LAPACKhseqr_("S", "I", &bn, &ilo, &ihi, A, &ldA, wr, wi, Q, &ldQ, work, &lwork, &info));
    PetscCheck(!info, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "Error in LAPACK routine XHSEQR %d", (int)info);
  }
  PetscCall(PetscFree(work));

  /* sort the eigenvalues */
  for (i = 0; i < n; i++) modul[i] = PetscSqrtReal(wr[i] * wr[i] + wi[i] * wi[i]);
  for (i = 0; i < n; i++) perm[i] = i;

  PetscCall(PetscSortRealWithPermutation(n, modul, perm));
  /* save the complex modulus of the largest eigenvalue in magnitude */
  if (dgmres->lambdaN < modul[perm[n - 1]]) dgmres->lambdaN = modul[perm[n - 1]];
  /* count the number of extracted eigenvalues (with complex conjugates) */
  NbrEig = 0;
  while (NbrEig < dgmres->neig) {
    if (wi[perm[NbrEig]] != 0) NbrEig += 2;
    else NbrEig += 1;
  }
  /* Reorder the Schur decomposition so that the cluster of smallest eigenvalues appears in the leading diagonal blocks of A */

  PetscCall(PetscCalloc1(n, &select));

  if (!dgmres->GreatestEig) {
    for (j = 0; j < NbrEig; j++) select[perm[j]] = 1;
  } else {
    for (j = 0; j < NbrEig; j++) select[perm[n - j - 1]] = 1;
  }
  /* call Lapack dtrsen */
  lwork  = PetscMax(1, 4 * NbrEig * (bn - NbrEig));
  liwork = PetscMax(1, 2 * NbrEig * (bn - NbrEig));
  PetscCall(PetscMalloc1(lwork, &work));
  PetscCall(PetscMalloc1(liwork, &iwork));
  {
    PetscBLASInt info = 0;
    PetscReal    CondEig; /* lower bound on the reciprocal condition number for the selected cluster of eigenvalues */
    PetscReal    CondSub; /* estimated reciprocal condition number of the specified invariant subspace. */
    PetscCallBLAS("LAPACKtrsen", LAPACKtrsen_("B", "V", select, &bn, A, &ldA, Q, &ldQ, wr, wi, &NbrEig, &CondEig, &CondSub, work, &lwork, iwork, &liwork, &info));
    PetscCheck(info != 1, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "UNABLE TO REORDER THE EIGENVALUES WITH THE LAPACK ROUTINE : ILL-CONDITIONED PROBLEM");
  }
  PetscCall(PetscFree(select));

  /* Extract the Schur vectors */
  for (j = 0; j < NbrEig; j++) PetscCall(PetscArraycpy(&SR[j * N], &(Q[j * ldQ]), n));
  *neig = NbrEig;
  PetscCall(PetscFree(A));
  PetscCall(PetscFree(work));
  PetscCall(PetscFree(perm));
  PetscCall(PetscFree(work));
  PetscCall(PetscFree(iwork));
  PetscCall(PetscFree(modul));
  PetscCall(PetscFree(Q));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPDGMRESApplyDeflation_DGMRES(KSP ksp, Vec x, Vec y)
{
  KSP_DGMRES  *dgmres = (KSP_DGMRES *)ksp->data;
  PetscInt     i, r = dgmres->r;
  PetscReal    alpha    = 1.0;
  PetscInt     max_neig = dgmres->max_neig;
  PetscBLASInt br, bmax;
  PetscReal    lambda = dgmres->lambdaN;

  PetscFunctionBegin;
  PetscCall(PetscBLASIntCast(r, &br));
  PetscCall(PetscBLASIntCast(max_neig, &bmax));
  PetscCall(PetscLogEventBegin(KSP_DGMRESApplyDeflation, ksp, 0, 0, 0));
  if (!r) {
    PetscCall(VecCopy(x, y));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  /* Compute U'*x */
  if (!X1) {
    PetscCall(PetscMalloc1(bmax, &X1));
    PetscCall(PetscMalloc1(bmax, &X2));
  }
  PetscCall(VecMDot(x, r, UU, X1));

  /* Solve T*X1=X2 for X1*/
  PetscCall(PetscArraycpy(X2, X1, br));
  {
    PetscBLASInt info;
    PetscBLASInt nrhs = 1;
    PetscCallBLAS("LAPACKgetrs", LAPACKgetrs_("N", &br, &nrhs, TTF, &bmax, INVP, X1, &bmax, &info));
    PetscCheck(!info, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "Error in LAPACK routine XGETRS %d", (int)info);
  }
  /* Iterative refinement -- is it really necessary ?? */
  if (!WORK) {
    PetscCall(PetscMalloc1(3 * bmax, &WORK));
    PetscCall(PetscMalloc1(bmax, &IWORK));
  }
  {
    PetscBLASInt info;
    PetscReal    berr, ferr;
    PetscBLASInt nrhs = 1;
    PetscCallBLAS("LAPACKgerfs", LAPACKgerfs_("N", &br, &nrhs, TT, &bmax, TTF, &bmax, INVP, X2, &bmax, X1, &bmax, &ferr, &berr, WORK, IWORK, &info));
    PetscCheck(!info, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "Error in LAPACK routine XGERFS %d", (int)info);
  }

  for (i = 0; i < r; i++) X2[i] = X1[i] / lambda - X2[i];

  /* Compute X2=U*X2 */
  PetscCall(VecZeroEntries(y));
  PetscCall(VecMAXPY(y, r, X2, UU));
  PetscCall(VecAXPY(y, alpha, x));

  PetscCall(PetscLogEventEnd(KSP_DGMRESApplyDeflation, ksp, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPDGMRESImproveEig_DGMRES(KSP ksp, PetscInt neig)
{
  KSP_DGMRES   *dgmres = (KSP_DGMRES *)ksp->data;
  PetscInt      j, r_old, r = dgmres->r;
  PetscBLASInt  i     = 0;
  PetscInt      neig1 = dgmres->neig + EIG_OFFSET;
  PetscInt      bmax  = dgmres->max_neig;
  PetscInt      aug   = r + neig;       /* actual size of the augmented invariant basis */
  PetscInt      aug1  = bmax + neig1;   /* maximum size of the augmented invariant basis */
  PetscBLASInt  ldA;                    /* leading dimension of AUAU and AUU*/
  PetscBLASInt  N;                      /* size of AUAU */
  PetscReal    *Q;                      /*  orthogonal matrix of  (left) schur vectors */
  PetscReal    *Z;                      /*  orthogonal matrix of  (right) schur vectors */
  PetscReal    *work;                   /* working vector */
  PetscBLASInt  lwork;                  /* size of the working vector */
  PetscInt     *perm;                   /* Permutation vector to sort eigenvalues */
  PetscReal    *wr, *wi, *beta, *modul; /* Real and imaginary part and modulus of the eigenvalues of A*/
  PetscBLASInt  NbrEig = 0, nr, bm;
  PetscBLASInt *select;
  PetscBLASInt  liwork, *iwork;

  PetscFunctionBegin;
  /* Block construction of the matrices AUU=(AU)'*U and (AU)'*AU*/
  if (!AUU) {
    PetscCall(PetscMalloc1(aug1 * aug1, &AUU));
    PetscCall(PetscMalloc1(aug1 * aug1, &AUAU));
  }
  /* AUU = (AU)'*U = [(MU)'*U (MU)'*X; (MX)'*U (MX)'*X]
   * Note that MU and MX have been computed previously either in ComputeDataDeflation() or down here in a previous call to this function */
  /* (MU)'*U size (r x r) -- store in the <r> first columns of AUU*/
  for (j = 0; j < r; j++) PetscCall(VecMDot(UU[j], r, MU, &AUU[j * aug1]));
  /* (MU)'*X size (r x neig) -- store in AUU from the column <r>*/
  for (j = 0; j < neig; j++) PetscCall(VecMDot(XX[j], r, MU, &AUU[(r + j) * aug1]));
  /* (MX)'*U size (neig x r) -- store in the <r> first columns of AUU from the row <r>*/
  for (j = 0; j < r; j++) PetscCall(VecMDot(UU[j], neig, MX, &AUU[j * aug1 + r]));
  /* (MX)'*X size (neig neig) --  store in AUU from the column <r> and the row <r>*/
  for (j = 0; j < neig; j++) PetscCall(VecMDot(XX[j], neig, MX, &AUU[(r + j) * aug1 + r]));

  /* AUAU = (AU)'*AU = [(MU)'*MU (MU)'*MX; (MX)'*MU (MX)'*MX] */
  /* (MU)'*MU size (r x r) -- store in the <r> first columns of AUAU*/
  for (j = 0; j < r; j++) PetscCall(VecMDot(MU[j], r, MU, &AUAU[j * aug1]));
  /* (MU)'*MX size (r x neig) -- store in AUAU from the column <r>*/
  for (j = 0; j < neig; j++) PetscCall(VecMDot(MX[j], r, MU, &AUAU[(r + j) * aug1]));
  /* (MX)'*MU size (neig x r) -- store in the <r> first columns of AUAU from the row <r>*/
  for (j = 0; j < r; j++) PetscCall(VecMDot(MU[j], neig, MX, &AUAU[j * aug1 + r]));
  /* (MX)'*MX size (neig neig) --  store in AUAU from the column <r> and the row <r>*/
  for (j = 0; j < neig; j++) PetscCall(VecMDot(MX[j], neig, MX, &AUAU[(r + j) * aug1 + r]));

  /* Computation of the eigenvectors */
  PetscCall(PetscBLASIntCast(aug1, &ldA));
  PetscCall(PetscBLASIntCast(aug, &N));
  lwork = 8 * N + 20; /* sizeof the working space */
  PetscCall(PetscMalloc1(N, &wr));
  PetscCall(PetscMalloc1(N, &wi));
  PetscCall(PetscMalloc1(N, &beta));
  PetscCall(PetscMalloc1(N, &modul));
  PetscCall(PetscMalloc1(N, &perm));
  PetscCall(PetscMalloc1(N * N, &Q));
  PetscCall(PetscMalloc1(N * N, &Z));
  PetscCall(PetscMalloc1(lwork, &work));
  {
    PetscBLASInt info = 0;
    PetscCallBLAS("LAPACKgges", LAPACKgges_("V", "V", "N", NULL, &N, AUAU, &ldA, AUU, &ldA, &i, wr, wi, beta, Q, &N, Z, &N, work, &lwork, NULL, &info));
    PetscCheck(!info, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "Error in LAPACK routine XGGES %d", (int)info);
  }
  for (i = 0; i < N; i++) {
    if (beta[i] != 0.0) {
      wr[i] /= beta[i];
      wi[i] /= beta[i];
    }
  }
  /* sort the eigenvalues */
  for (i = 0; i < N; i++) modul[i] = PetscSqrtReal(wr[i] * wr[i] + wi[i] * wi[i]);
  for (i = 0; i < N; i++) perm[i] = i;
  PetscCall(PetscSortRealWithPermutation(N, modul, perm));
  /* Save the norm of the largest eigenvalue */
  if (dgmres->lambdaN < modul[perm[N - 1]]) dgmres->lambdaN = modul[perm[N - 1]];
  /* Allocate space to extract the first r schur vectors   */
  if (!SR2) PetscCall(PetscMalloc1(aug1 * bmax, &SR2));
  /* count the number of extracted eigenvalues (complex conjugates count as 2) */
  while (NbrEig < bmax) {
    if (wi[perm[NbrEig]] == 0) NbrEig += 1;
    else NbrEig += 2;
  }
  if (NbrEig > bmax) NbrEig = bmax - 1;
  r_old     = r; /* previous size of r */
  dgmres->r = r = NbrEig;

  /* Select the eigenvalues to reorder */
  PetscCall(PetscCalloc1(N, &select));
  if (!dgmres->GreatestEig) {
    for (j = 0; j < NbrEig; j++) select[perm[j]] = 1;
  } else {
    for (j = 0; j < NbrEig; j++) select[perm[N - j - 1]] = 1;
  }
  /* Reorder and extract the new <r> schur vectors */
  lwork  = PetscMax(4 * N + 16, 2 * NbrEig * (N - NbrEig));
  liwork = PetscMax(N + 6, 2 * NbrEig * (N - NbrEig));
  PetscCall(PetscFree(work));
  PetscCall(PetscMalloc1(lwork, &work));
  PetscCall(PetscMalloc1(liwork, &iwork));
  {
    PetscBLASInt info = 0;
    PetscReal    Dif[2];
    PetscBLASInt ijob  = 2;
    PetscBLASInt wantQ = 1, wantZ = 1;
    PetscCallBLAS("LAPACKtgsen", LAPACKtgsen_(&ijob, &wantQ, &wantZ, select, &N, AUAU, &ldA, AUU, &ldA, wr, wi, beta, Q, &N, Z, &N, &NbrEig, NULL, NULL, &(Dif[0]), work, &lwork, iwork, &liwork, &info));
    PetscCheck(info != 1, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "Unable to reorder the eigenvalues with the LAPACK routine: ill-conditioned problem.");
  }
  PetscCall(PetscFree(select));

  for (j = 0; j < r; j++) PetscCall(PetscArraycpy(&SR2[j * aug1], &(Z[j * N]), N));

  /* Multiply the Schur vectors SR2 by U (and X)  to get a new U
   -- save it temporarily in MU */
  for (j = 0; j < r; j++) {
    PetscCall(VecZeroEntries(MU[j]));
    PetscCall(VecMAXPY(MU[j], r_old, &SR2[j * aug1], UU));
    PetscCall(VecMAXPY(MU[j], neig, &SR2[j * aug1 + r_old], XX));
  }
  /* Form T = U'*MU*U */
  for (j = 0; j < r; j++) {
    PetscCall(VecCopy(MU[j], UU[j]));
    PetscCall(KSP_PCApplyBAorAB(ksp, UU[j], MU[j], VEC_TEMP_MATOP));
  }
  dgmres->matvecs += r;
  for (j = 0; j < r; j++) PetscCall(VecMDot(MU[j], r, UU, &TT[j * bmax]));
  /* Factorize T */
  PetscCall(PetscArraycpy(TTF, TT, bmax * r));
  PetscCall(PetscBLASIntCast(r, &nr));
  PetscCall(PetscBLASIntCast(bmax, &bm));
  {
    PetscBLASInt info;
    PetscCallBLAS("LAPACKgetrf", LAPACKgetrf_(&nr, &nr, TTF, &bm, INVP, &info));
    PetscCheck(!info, PetscObjectComm((PetscObject)ksp), PETSC_ERR_LIB, "Error in LAPACK routine XGETRF INFO=%d", (int)info);
  }
  /* Free Memory */
  PetscCall(PetscFree(wr));
  PetscCall(PetscFree(wi));
  PetscCall(PetscFree(beta));
  PetscCall(PetscFree(modul));
  PetscCall(PetscFree(perm));
  PetscCall(PetscFree(Q));
  PetscCall(PetscFree(Z));
  PetscCall(PetscFree(work));
  PetscCall(PetscFree(iwork));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
     KSPDGMRES - Implements the deflated GMRES as defined in [1,2].
                 In this implementation, the adaptive strategy allows to switch to the deflated GMRES when the
                 stagnation occurs.

   Options Database Keys:
   GMRES Options (inherited):
+   -ksp_gmres_restart <restart> - the number of Krylov directions to orthogonalize against
.   -ksp_gmres_haptol <tol> - sets the tolerance for "happy ending" (exact convergence)
.   -ksp_gmres_preallocate - preallocate all the Krylov search directions initially (otherwise groups of
                             vectors are allocated as needed)
.   -ksp_gmres_classicalgramschmidt - use classical (unmodified) Gram-Schmidt to orthogonalize against the Krylov space (fast) (the default)
.   -ksp_gmres_modifiedgramschmidt - use modified Gram-Schmidt in the orthogonalization (more stable, but slower)
.   -ksp_gmres_cgs_refinement_type <refine_never,refine_ifneeded,refine_always> - determine if iterative refinement is used to increase the
                                   stability of the classical Gram-Schmidt  orthogonalization.
-   -ksp_gmres_krylov_monitor - plot the Krylov space generated

   DGMRES Options Database Keys:
+   -ksp_dgmres_eigen <neig> - number of smallest eigenvalues to extract at each restart
.   -ksp_dgmres_max_eigen <max_neig> - maximum number of eigenvalues that can be extracted during the iterative
                                       process
.   -ksp_dgmres_force - use the deflation at each restart; switch off the adaptive strategy.
-   -ksp_dgmres_view_deflation_vecs <viewerspec> - View the deflation vectors, where viewerspec is a key that can be
                                                   parsed by `PetscOptionsGetViewer()`.  If neig > 1, viewerspec should
                                                   end with ":append".  No vectors will be viewed if the adaptive
                                                   strategy chooses not to deflate, so -ksp_dgmres_force should also
                                                   be given.
                                                   The deflation vectors span a subspace that may be a good
                                                   approximation of the subspace of smallest eigenvectors of the
                                                   preconditioned operator, so this option can aid in understanding
                                                   the performance of a preconditioner.

 Level: beginner

 Note:
    Left and right preconditioning are supported, but not symmetric preconditioning. Complex arithmetic is not supported

 References:
+ [1] - J. Erhel, K. Burrage and B. Pohl,  Restarted GMRES preconditioned by deflation,J. Computational and Applied Mathematics, 69(1996).
- [2] - D. NUENTSA WAKAM and F. PACULL, Memory Efficient Hybrid Algebraic Solvers for Linear Systems Arising from Compressible Flows, Computers and Fluids,
   In Press, http://dx.doi.org/10.1016/j.compfluid.2012.03.023

 Contributed by:
  Desire NUENTSA WAKAM, INRIA

.seealso: [](chapter_ksp), `KSPCreate()`, `KSPSetType()`, `KSPType`, `KSP`, `KSPFGMRES`, `KSPLGMRES`,
           `KSPGMRESSetRestart()`, `KSPGMRESSetHapTol()`, `KSPGMRESSetPreAllocateVectors()`, `KSPGMRESSetOrthogonalization()`, `KSPGMRESGetOrthogonalization()`,
           `KSPGMRESClassicalGramSchmidtOrthogonalization()`, `KSPGMRESModifiedGramSchmidtOrthogonalization()`,
           `KSPGMRESCGSRefinementType`, `KSPGMRESSetCGSRefinementType()`, `KSPGMRESGetCGSRefinementType()`, `KSPGMRESMonitorKrylov()`, `KSPSetPCSide()`
 M*/

PETSC_EXTERN PetscErrorCode KSPCreate_DGMRES(KSP ksp)
{
  KSP_DGMRES *dgmres;

  PetscFunctionBegin;
  PetscCall(PetscNew(&dgmres));
  ksp->data = (void *)dgmres;

  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_PRECONDITIONED, PC_LEFT, 3));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_UNPRECONDITIONED, PC_RIGHT, 2));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_NONE, PC_RIGHT, 1));

  ksp->ops->buildsolution                = KSPBuildSolution_DGMRES;
  ksp->ops->setup                        = KSPSetUp_DGMRES;
  ksp->ops->solve                        = KSPSolve_DGMRES;
  ksp->ops->destroy                      = KSPDestroy_DGMRES;
  ksp->ops->view                         = KSPView_DGMRES;
  ksp->ops->setfromoptions               = KSPSetFromOptions_DGMRES;
  ksp->ops->computeextremesingularvalues = KSPComputeExtremeSingularValues_GMRES;
  ksp->ops->computeeigenvalues           = KSPComputeEigenvalues_GMRES;

  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPGMRESSetPreAllocateVectors_C", KSPGMRESSetPreAllocateVectors_GMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPGMRESSetOrthogonalization_C", KSPGMRESSetOrthogonalization_GMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPGMRESSetRestart_C", KSPGMRESSetRestart_GMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPGMRESSetHapTol_C", KSPGMRESSetHapTol_GMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPGMRESSetCGSRefinementType_C", KSPGMRESSetCGSRefinementType_GMRES));
  /* -- New functions defined in DGMRES -- */
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESSetEigen_C", KSPDGMRESSetEigen_DGMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESSetMaxEigen_C", KSPDGMRESSetMaxEigen_DGMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESSetRatio_C", KSPDGMRESSetRatio_DGMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESForce_C", KSPDGMRESForce_DGMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESComputeSchurForm_C", KSPDGMRESComputeSchurForm_DGMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESComputeDeflationData_C", KSPDGMRESComputeDeflationData_DGMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESApplyDeflation_C", KSPDGMRESApplyDeflation_DGMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPDGMRESImproveEig_C", KSPDGMRESImproveEig_DGMRES));

  PetscCall(PetscLogEventRegister("DGMRESCompDefl", KSP_CLASSID, &KSP_DGMRESComputeDeflationData));
  PetscCall(PetscLogEventRegister("DGMRESApplyDefl", KSP_CLASSID, &KSP_DGMRESApplyDeflation));

  dgmres->haptol         = 1.0e-30;
  dgmres->q_preallocate  = 0;
  dgmres->delta_allocate = GMRES_DELTA_DIRECTIONS;
  dgmres->orthog         = KSPGMRESClassicalGramSchmidtOrthogonalization;
  dgmres->nrs            = NULL;
  dgmres->sol_temp       = NULL;
  dgmres->max_k          = GMRES_DEFAULT_MAXK;
  dgmres->Rsvd           = NULL;
  dgmres->cgstype        = KSP_GMRES_CGS_REFINE_NEVER;
  dgmres->orthogwork     = NULL;

  /* Default values for the deflation */
  dgmres->r           = 0;
  dgmres->neig        = DGMRES_DEFAULT_EIG;
  dgmres->max_neig    = DGMRES_DEFAULT_MAXEIG - 1;
  dgmres->lambdaN     = 0.0;
  dgmres->smv         = SMV;
  dgmres->matvecs     = 0;
  dgmres->GreatestEig = PETSC_FALSE; /* experimental */
  dgmres->HasSchur    = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

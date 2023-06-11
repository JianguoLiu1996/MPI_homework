
#include <../src/ksp/ksp/impls/gmres/pipefgmres/pipefgmresimpl.h> /*I  "petscksp.h"  I*/

static PetscBool  cited      = PETSC_FALSE;
static const char citation[] = "@article{SSM2016,\n"
                               "  author = {P. Sanan and S.M. Schnepp and D.A. May},\n"
                               "  title = {Pipelined, Flexible Krylov Subspace Methods},\n"
                               "  journal = {SIAM Journal on Scientific Computing},\n"
                               "  volume = {38},\n"
                               "  number = {5},\n"
                               "  pages = {C441-C470},\n"
                               "  year = {2016},\n"
                               "  doi = {10.1137/15M1049130},\n"
                               "  URL = {http://dx.doi.org/10.1137/15M1049130},\n"
                               "  eprint = {http://dx.doi.org/10.1137/15M1049130}\n"
                               "}\n";

#define PIPEFGMRES_DELTA_DIRECTIONS 10
#define PIPEFGMRES_DEFAULT_MAXK     30

static PetscErrorCode KSPPIPEFGMRESGetNewVectors(KSP, PetscInt);
static PetscErrorCode KSPPIPEFGMRESUpdateHessenberg(KSP, PetscInt, PetscBool *, PetscReal *);
static PetscErrorCode KSPPIPEFGMRESBuildSoln(PetscScalar *, Vec, Vec, KSP, PetscInt);
extern PetscErrorCode KSPReset_PIPEFGMRES(KSP);

/*

    KSPSetUp_PIPEFGMRES - Sets up the workspace needed by pipefgmres.

    This is called once, usually automatically by KSPSolve() or KSPSetUp(),
    but can be called directly by KSPSetUp().

*/
static PetscErrorCode KSPSetUp_PIPEFGMRES(KSP ksp)
{
  PetscInt        k;
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)ksp->data;
  const PetscInt  max_k      = pipefgmres->max_k;

  PetscFunctionBegin;
  PetscCall(KSPSetUp_GMRES(ksp));

  PetscCall(PetscMalloc1((VEC_OFFSET + max_k), &pipefgmres->prevecs));
  PetscCall(PetscMalloc1((VEC_OFFSET + max_k), &pipefgmres->prevecs_user_work));

  PetscCall(KSPCreateVecs(ksp, pipefgmres->vv_allocated, &pipefgmres->prevecs_user_work[0], 0, NULL));
  for (k = 0; k < pipefgmres->vv_allocated; k++) pipefgmres->prevecs[k] = pipefgmres->prevecs_user_work[0][k];

  PetscCall(PetscMalloc1((VEC_OFFSET + max_k), &pipefgmres->zvecs));
  PetscCall(PetscMalloc1((VEC_OFFSET + max_k), &pipefgmres->zvecs_user_work));

  PetscCall(PetscMalloc1((VEC_OFFSET + max_k), &pipefgmres->redux));

  PetscCall(KSPCreateVecs(ksp, pipefgmres->vv_allocated, &pipefgmres->zvecs_user_work[0], 0, NULL));
  for (k = 0; k < pipefgmres->vv_allocated; k++) pipefgmres->zvecs[k] = pipefgmres->zvecs_user_work[0][k];

  PetscFunctionReturn(PETSC_SUCCESS);
}

/*

    KSPPIPEFGMRESCycle - Run pipefgmres, possibly with restart.  Return residual
                  history if requested.

    input parameters:
.        pipefgmres  - structure containing parameters and work areas

    output parameters:
.        itcount - number of iterations used.  If null, ignored.
.        converged - 0 if not converged

    Notes:
    On entry, the value in vector VEC_VV(0) should be
    the initial residual.

*/
static PetscErrorCode KSPPIPEFGMRESCycle(PetscInt *itcount, KSP ksp)
{
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)(ksp->data);
  PetscReal       res_norm;
  PetscReal       hapbnd, tt;
  PetscScalar    *hh, *hes, *lhh, shift = pipefgmres->shift;
  PetscBool       hapend = PETSC_FALSE;      /* indicates happy breakdown ending */
  PetscInt        loc_it;                    /* local count of # of dir. in Krylov space */
  PetscInt        max_k = pipefgmres->max_k; /* max # of directions Krylov space */
  PetscInt        i, j, k;
  Mat             Amat, Pmat;
  Vec             Q, W;                      /* Pipelining vectors */
  Vec            *redux = pipefgmres->redux; /* workspace for single reduction */

  PetscFunctionBegin;
  if (itcount) *itcount = 0;

  /* Assign simpler names to these vectors, allocated as pipelining workspace */
  Q = VEC_Q;
  W = VEC_W;

  /* Allocate memory for orthogonalization work (freed in the GMRES Destroy routine)*/
  /* Note that we add an extra value here to allow for a single reduction */
  if (!pipefgmres->orthogwork) PetscCall(PetscMalloc1(pipefgmres->max_k + 2, &pipefgmres->orthogwork));
  lhh = pipefgmres->orthogwork;

  /* Number of pseudo iterations since last restart is the number
     of prestart directions */
  loc_it = 0;

  /* note: (pipefgmres->it) is always set one less than (loc_it) It is used in
     KSPBUILDSolution_PIPEFGMRES, where it is passed to KSPPIPEFGMRESBuildSoln.
     Note that when KSPPIPEFGMRESBuildSoln is called from this function,
     (loc_it -1) is passed, so the two are equivalent */
  pipefgmres->it = (loc_it - 1);

  /* initial residual is in VEC_VV(0)  - compute its norm*/
  PetscCall(VecNorm(VEC_VV(0), NORM_2, &res_norm));

  /* first entry in right-hand-side of hessenberg system is just
     the initial residual norm */
  *RS(0) = res_norm;

  PetscCall(PetscObjectSAWsTakeAccess((PetscObject)ksp));
  if (ksp->normtype != KSP_NORM_NONE) ksp->rnorm = res_norm;
  else ksp->rnorm = 0;
  PetscCall(PetscObjectSAWsGrantAccess((PetscObject)ksp));
  PetscCall(KSPLogResidualHistory(ksp, ksp->rnorm));
  PetscCall(KSPMonitor(ksp, ksp->its, ksp->rnorm));

  /* check for the convergence - maybe the current guess is good enough */
  PetscCall((*ksp->converged)(ksp, ksp->its, ksp->rnorm, &ksp->reason, ksp->cnvP));
  if (ksp->reason) {
    if (itcount) *itcount = 0;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  /* scale VEC_VV (the initial residual) */
  PetscCall(VecScale(VEC_VV(0), 1.0 / res_norm));

  /* Fill the pipeline */
  PetscCall(KSP_PCApply(ksp, VEC_VV(loc_it), PREVEC(loc_it)));
  PetscCall(PCGetOperators(ksp->pc, &Amat, &Pmat));
  PetscCall(KSP_MatMult(ksp, Amat, PREVEC(loc_it), ZVEC(loc_it)));
  PetscCall(VecAXPY(ZVEC(loc_it), -shift, VEC_VV(loc_it))); /* Note shift */

  /* MAIN ITERATION LOOP BEGINNING*/
  /* keep iterating until we have converged OR generated the max number
     of directions OR reached the max number of iterations for the method */
  while (!ksp->reason && loc_it < max_k && ksp->its < ksp->max_it) {
    if (loc_it) {
      PetscCall(KSPLogResidualHistory(ksp, res_norm));
      PetscCall(KSPMonitor(ksp, ksp->its, res_norm));
    }
    pipefgmres->it = (loc_it - 1);

    /* see if more space is needed for work vectors */
    if (pipefgmres->vv_allocated <= loc_it + VEC_OFFSET + 1) {
      PetscCall(KSPPIPEFGMRESGetNewVectors(ksp, loc_it + 1));
      /* (loc_it+1) is passed in as number of the first vector that should
         be allocated */
    }

    /* Note that these inner products are with "Z" now, so
       in particular, lhh[loc_it] is the 'barred' or 'shifted' value,
       not the value from the equivalent FGMRES run (even in exact arithmetic)
       That is, the H we need for the Arnoldi relation is different from the
       coefficients we use in the orthogonalization process,because of the shift */

    /* Do some local twiddling to allow for a single reduction */
    for (i = 0; i < loc_it + 1; i++) redux[i] = VEC_VV(i);
    redux[loc_it + 1] = ZVEC(loc_it);

    /* note the extra dot product which ends up in lh[loc_it+1], which computes ||z||^2 */
    PetscCall(VecMDotBegin(ZVEC(loc_it), loc_it + 2, redux, lhh));

    /* Start the split reduction (This actually calls the MPI_Iallreduce, otherwise, the reduction is simply delayed until the "end" call)*/
    PetscCall(PetscCommSplitReductionBegin(PetscObjectComm((PetscObject)ZVEC(loc_it))));

    /* The work to be overlapped with the inner products follows.
       This is application of the preconditioner and the operator
       to compute intermediate quantites which will be combined (locally)
       with the results of the inner products.
       */
    PetscCall(KSP_PCApply(ksp, ZVEC(loc_it), Q));
    PetscCall(PCGetOperators(ksp->pc, &Amat, &Pmat));
    PetscCall(KSP_MatMult(ksp, Amat, Q, W));

    /* Compute inner products of the new direction with previous directions,
       and the norm of the to-be-orthogonalized direction "Z".
       This information is enough to build the required entries
       of H. The inner product with VEC_VV(it_loc) is
       *different* than in the standard FGMRES and need to be dealt with specially.
       That is, for standard FGMRES the orthogonalization coefficients are the same
       as the coefficients used in the Arnoldi relation to reconstruct, but here this
       is not true (albeit only for the one entry of H which we "unshift" below. */

    /* Finish the dot product, retrieving the extra entry */
    PetscCall(VecMDotEnd(ZVEC(loc_it), loc_it + 2, redux, lhh));
    tt = PetscRealPart(lhh[loc_it + 1]);

    /* Hessenberg entries, and entries for (naive) classical Graham-Schmidt
      Note that the Hessenberg entries require a shift, as these are for the
      relation AU = VH, which is wrt unshifted basis vectors */
    hh  = HH(0, loc_it);
    hes = HES(0, loc_it);
    for (j = 0; j < loc_it; j++) {
      hh[j]  = lhh[j];
      hes[j] = lhh[j];
    }
    hh[loc_it]  = lhh[loc_it] + shift;
    hes[loc_it] = lhh[loc_it] + shift;

    /* we delay applying the shift here */
    for (j = 0; j <= loc_it; j++) { lhh[j] = -lhh[j]; /* flip sign */ }

    /* Compute the norm of the un-normalized new direction using the rearranged formula
       Note that these are shifted ("barred") quantities */
    for (k = 0; k <= loc_it; k++) tt -= ((PetscReal)(PetscAbsScalar(lhh[k]) * PetscAbsScalar(lhh[k])));
    /* On AVX512 this is accumulating roundoff errors for eg: tt=-2.22045e-16 */
    if ((tt < 0.0) && tt > -PETSC_SMALL) tt = 0.0;
    if (tt < 0.0) {
      /* If we detect square root breakdown in the norm, we must restart the algorithm.
         Here this means we simply break the current loop and reconstruct the solution
         using the basis we have computed thus far. Note that by breaking immediately,
         we do not update the iteration count, so computation done in this iteration
         should be disregarded.
         */
      PetscCall(PetscInfo(ksp, "Restart due to square root breakdown at it = %" PetscInt_FMT ", tt=%g\n", ksp->its, (double)tt));
      break;
    } else {
      tt = PetscSqrtReal(tt);
    }

    /* new entry in hessenburg is the 2-norm of our new direction */
    hh[loc_it + 1]  = tt;
    hes[loc_it + 1] = tt;

    /* The recurred computation for the new direction
       The division by tt is delayed to the happy breakdown check later
       Note placement BEFORE the unshift
       */
    PetscCall(VecCopy(ZVEC(loc_it), VEC_VV(loc_it + 1)));
    PetscCall(VecMAXPY(VEC_VV(loc_it + 1), loc_it + 1, lhh, &VEC_VV(0)));
    /* (VEC_VV(loc_it+1) is not normalized yet) */

    /* The recurred computation for the preconditioned vector (u) */
    PetscCall(VecCopy(Q, PREVEC(loc_it + 1)));
    PetscCall(VecMAXPY(PREVEC(loc_it + 1), loc_it + 1, lhh, &PREVEC(0)));
    PetscCall(VecScale(PREVEC(loc_it + 1), 1.0 / tt));

    /* Unshift an entry in the GS coefficients ("removing the bar") */
    lhh[loc_it] -= shift;

    /* The recurred computation for z (Au)
       Note placement AFTER the "unshift" */
    PetscCall(VecCopy(W, ZVEC(loc_it + 1)));
    PetscCall(VecMAXPY(ZVEC(loc_it + 1), loc_it + 1, lhh, &ZVEC(0)));
    PetscCall(VecScale(ZVEC(loc_it + 1), 1.0 / tt));

    /* Happy Breakdown Check */
    hapbnd = PetscAbsScalar((tt) / *RS(loc_it));
    /* RS(loc_it) contains the res_norm from the last iteration  */
    hapbnd = PetscMin(pipefgmres->haptol, hapbnd);
    if (tt > hapbnd) {
      /* scale new direction by its norm  */
      PetscCall(VecScale(VEC_VV(loc_it + 1), 1.0 / tt));
    } else {
      /* This happens when the solution is exactly reached. */
      /* So there is no new direction... */
      PetscCall(VecSet(VEC_TEMP, 0.0)); /* set VEC_TEMP to 0 */
      hapend = PETSC_TRUE;
    }
    /* note that for pipefgmres we could get HES(loc_it+1, loc_it)  = 0 and the
       current solution would not be exact if HES was singular.  Note that
       HH non-singular implies that HES is not singular, and HES is guaranteed
       to be nonsingular when PREVECS are linearly independent and A is
       nonsingular (in GMRES, the nonsingularity of A implies the nonsingularity
       of HES). So we should really add a check to verify that HES is nonsingular.*/

    /* Note that to be thorough, in debug mode, one could call a LAPACK routine
       here to check that the hessenberg matrix is indeed non-singular (since
       FGMRES does not guarantee this) */

    /* Now apply rotations to new col of hessenberg (and right side of system),
       calculate new rotation, and get new residual norm at the same time*/
    PetscCall(KSPPIPEFGMRESUpdateHessenberg(ksp, loc_it, &hapend, &res_norm));
    if (ksp->reason) break;

    loc_it++;
    pipefgmres->it = (loc_it - 1); /* Add this here in case it has converged */

    PetscCall(PetscObjectSAWsTakeAccess((PetscObject)ksp));
    ksp->its++;
    if (ksp->normtype != KSP_NORM_NONE) ksp->rnorm = res_norm;
    else ksp->rnorm = 0;
    PetscCall(PetscObjectSAWsGrantAccess((PetscObject)ksp));

    PetscCall((*ksp->converged)(ksp, ksp->its, ksp->rnorm, &ksp->reason, ksp->cnvP));

    /* Catch error in happy breakdown and signal convergence and break from loop */
    if (hapend) {
      if (!ksp->reason) {
        PetscCheck(!ksp->errorifnotconverged, PetscObjectComm((PetscObject)ksp), PETSC_ERR_NOT_CONVERGED, "You reached the happy break down, but convergence was not indicated. Residual norm = %g", (double)res_norm);
        ksp->reason = KSP_DIVERGED_BREAKDOWN;
        break;
      }
    }
  }
  /* END OF ITERATION LOOP */
  PetscCall(KSPLogResidualHistory(ksp, ksp->rnorm));

  /*
     Monitor if we know that we will not return for a restart */
  if (loc_it && (ksp->reason || ksp->its >= ksp->max_it)) PetscCall(KSPMonitor(ksp, ksp->its, ksp->rnorm));

  if (itcount) *itcount = loc_it;

  /*
    Down here we have to solve for the "best" coefficients of the Krylov
    columns, add the solution values together, and possibly unwind the
    preconditioning from the solution
   */

  /* Form the solution (or the solution so far) */
  /* Note: must pass in (loc_it-1) for iteration count so that KSPPIPEGMRESIIBuildSoln
     properly navigates */

  PetscCall(KSPPIPEFGMRESBuildSoln(RS(0), ksp->vec_sol, ksp->vec_sol, ksp, loc_it - 1));

  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    KSPSolve_PIPEFGMRES - This routine applies the PIPEFGMRES method.

   Input Parameter:
.     ksp - the Krylov space object that was set to use pipefgmres

   Output Parameter:
.     outits - number of iterations used

*/
static PetscErrorCode KSPSolve_PIPEFGMRES(KSP ksp)
{
  PetscInt        its, itcount;
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)ksp->data;
  PetscBool       guess_zero = ksp->guess_zero;

  PetscFunctionBegin;
  /* We have not checked these routines for use with complex numbers. The inner products
     are likely not defined correctly for that case */
  PetscCheck(!PetscDefined(USE_COMPLEX) || PetscDefined(SKIP_COMPLEX), PETSC_COMM_WORLD, PETSC_ERR_SUP, "PIPEFGMRES has not been implemented for use with complex scalars");

  PetscCall(PetscCitationsRegister(citation, &cited));

  PetscCheck(!ksp->calc_sings || pipefgmres->Rsvd, PetscObjectComm((PetscObject)ksp), PETSC_ERR_ORDER, "Must call KSPSetComputeSingularValues() before KSPSetUp() is called");
  PetscCall(PetscObjectSAWsTakeAccess((PetscObject)ksp));
  ksp->its = 0;
  PetscCall(PetscObjectSAWsGrantAccess((PetscObject)ksp));

  itcount     = 0;
  ksp->reason = KSP_CONVERGED_ITERATING;
  while (!ksp->reason) {
    PetscCall(KSPInitialResidual(ksp, ksp->vec_sol, VEC_TEMP, VEC_TEMP_MATOP, VEC_VV(0), ksp->vec_rhs));
    PetscCall(KSPPIPEFGMRESCycle(&its, ksp));
    itcount += its;
    if (itcount >= ksp->max_it) {
      if (!ksp->reason) ksp->reason = KSP_DIVERGED_ITS;
      break;
    }
    ksp->guess_zero = PETSC_FALSE; /* every future call to KSPInitialResidual() will have nonzero guess */
  }
  ksp->guess_zero = guess_zero; /* restore if user provided nonzero initial guess */
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPDestroy_PIPEFGMRES(KSP ksp)
{
  PetscFunctionBegin;
  PetscCall(KSPReset_PIPEFGMRES(ksp));
  PetscCall(KSPDestroy_GMRES(ksp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    KSPPIPEFGMRESBuildSoln - create the solution from the starting vector and the
                      current iterates.

    Input parameters:
        nrs - work area of size it + 1.
        vguess  - index of initial guess
        vdest - index of result.  Note that vguess may == vdest (replace
                guess with the solution).
        it - HH upper triangular part is a block of size (it+1) x (it+1)

     This is an internal routine that knows about the PIPEFGMRES internals.
 */
static PetscErrorCode KSPPIPEFGMRESBuildSoln(PetscScalar *nrs, Vec vguess, Vec vdest, KSP ksp, PetscInt it)
{
  PetscScalar     tt;
  PetscInt        k, j;
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)(ksp->data);

  PetscFunctionBegin;
  /* Solve for solution vector that minimizes the residual */

  if (it < 0) {                        /* no pipefgmres steps have been performed */
    PetscCall(VecCopy(vguess, vdest)); /* VecCopy() is smart, exits immediately if vguess == vdest */
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  /* solve the upper triangular system - RS is the right side and HH is
     the upper triangular matrix  - put soln in nrs */
  if (*HH(it, it) != 0.0) nrs[it] = *RS(it) / *HH(it, it);
  else nrs[it] = 0.0;

  for (k = it - 1; k >= 0; k--) {
    tt = *RS(k);
    for (j = k + 1; j <= it; j++) tt -= *HH(k, j) * nrs[j];
    nrs[k] = tt / *HH(k, k);
  }

  /* Accumulate the correction to the solution of the preconditioned problem in VEC_TEMP */
  PetscCall(VecZeroEntries(VEC_TEMP));
  PetscCall(VecMAXPY(VEC_TEMP, it + 1, nrs, &PREVEC(0)));

  /* add solution to previous solution */
  if (vdest == vguess) {
    PetscCall(VecAXPY(vdest, 1.0, VEC_TEMP));
  } else {
    PetscCall(VecWAXPY(vdest, 1.0, VEC_TEMP, vguess));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*

    KSPPIPEFGMRESUpdateHessenberg - Do the scalar work for the orthogonalization.
                            Return new residual.

    input parameters:

.        ksp -    Krylov space object
.        it  -    plane rotations are applied to the (it+1)th column of the
                  modified hessenberg (i.e. HH(:,it))
.        hapend - PETSC_FALSE not happy breakdown ending.

    output parameters:
.        res - the new residual

 */
/*
.  it - column of the Hessenberg that is complete, PIPEFGMRES is actually computing two columns ahead of this
 */
static PetscErrorCode KSPPIPEFGMRESUpdateHessenberg(KSP ksp, PetscInt it, PetscBool *hapend, PetscReal *res)
{
  PetscScalar    *hh, *cc, *ss, *rs;
  PetscInt        j;
  PetscReal       hapbnd;
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)(ksp->data);

  PetscFunctionBegin;
  hh = HH(0, it); /* pointer to beginning of column to update */
  cc = CC(0);     /* beginning of cosine rotations */
  ss = SS(0);     /* beginning of sine rotations */
  rs = RS(0);     /* right hand side of least squares system */

  /* The Hessenberg matrix is now correct through column it, save that form for possible spectral analysis */
  for (j = 0; j <= it + 1; j++) *HES(j, it) = hh[j];

  /* check for the happy breakdown */
  hapbnd = PetscMin(PetscAbsScalar(hh[it + 1] / rs[it]), pipefgmres->haptol);
  if (PetscAbsScalar(hh[it + 1]) < hapbnd) {
    PetscCall(PetscInfo(ksp, "Detected happy breakdown, current hapbnd = %14.12e H(%" PetscInt_FMT ",%" PetscInt_FMT ") = %14.12e\n", (double)hapbnd, it + 1, it, (double)PetscAbsScalar(*HH(it + 1, it))));
    *hapend = PETSC_TRUE;
  }

  /* Apply all the previously computed plane rotations to the new column
     of the Hessenberg matrix */
  /* Note: this uses the rotation [conj(c)  s ; -s   c], c= cos(theta), s= sin(theta),
     and some refs have [c   s ; -conj(s)  c] (don't be confused!) */

  for (j = 0; j < it; j++) {
    PetscScalar hhj = hh[j];
    hh[j]           = PetscConj(cc[j]) * hhj + ss[j] * hh[j + 1];
    hh[j + 1]       = -ss[j] * hhj + cc[j] * hh[j + 1];
  }

  /*
    compute the new plane rotation, and apply it to:
     1) the right-hand-side of the Hessenberg system (RS)
        note: it affects RS(it) and RS(it+1)
     2) the new column of the Hessenberg matrix
        note: it affects HH(it,it) which is currently pointed to
        by hh and HH(it+1, it) (*(hh+1))
    thus obtaining the updated value of the residual...
  */

  /* compute new plane rotation */

  if (!*hapend) {
    PetscReal delta = PetscSqrtReal(PetscSqr(PetscAbsScalar(hh[it])) + PetscSqr(PetscAbsScalar(hh[it + 1])));
    if (delta == 0.0) {
      ksp->reason = KSP_DIVERGED_NULL;
      PetscFunctionReturn(PETSC_SUCCESS);
    }

    cc[it] = hh[it] / delta;     /* new cosine value */
    ss[it] = hh[it + 1] / delta; /* new sine value */

    hh[it]     = PetscConj(cc[it]) * hh[it] + ss[it] * hh[it + 1];
    rs[it + 1] = -ss[it] * rs[it];
    rs[it]     = PetscConj(cc[it]) * rs[it];
    *res       = PetscAbsScalar(rs[it + 1]);
  } else { /* happy breakdown: HH(it+1, it) = 0, therefore we don't need to apply
            another rotation matrix (so RH doesn't change).  The new residual is
            always the new sine term times the residual from last time (RS(it)),
            but now the new sine rotation would be zero...so the residual should
            be zero...so we will multiply "zero" by the last residual.  This might
            not be exactly what we want to do here -could just return "zero". */

    *res = 0.0;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   KSPBuildSolution_PIPEFGMRES

     Input Parameter:
.     ksp - the Krylov space object
.     ptr-

   Output Parameter:
.     result - the solution

   Note: this calls KSPPIPEFGMRESBuildSoln - the same function that KSPPIPEFGMRESCycle
   calls directly.

*/
PetscErrorCode KSPBuildSolution_PIPEFGMRES(KSP ksp, Vec ptr, Vec *result)
{
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)ksp->data;

  PetscFunctionBegin;
  if (!ptr) {
    if (!pipefgmres->sol_temp) PetscCall(VecDuplicate(ksp->vec_sol, &pipefgmres->sol_temp));
    ptr = pipefgmres->sol_temp;
  }
  if (!pipefgmres->nrs) {
    /* allocate the work area */
    PetscCall(PetscMalloc1(pipefgmres->max_k, &pipefgmres->nrs));
  }

  PetscCall(KSPPIPEFGMRESBuildSoln(pipefgmres->nrs, ksp->vec_sol, ptr, ksp, pipefgmres->it));
  if (result) *result = ptr;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPSetFromOptions_PIPEFGMRES(KSP ksp, PetscOptionItems *PetscOptionsObject)
{
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)ksp->data;
  PetscBool       flg;
  PetscScalar     shift;

  PetscFunctionBegin;
  PetscCall(KSPSetFromOptions_GMRES(ksp, PetscOptionsObject));
  PetscOptionsHeadBegin(PetscOptionsObject, "KSP pipelined FGMRES Options");
  PetscCall(PetscOptionsScalar("-ksp_pipefgmres_shift", "shift parameter", "KSPPIPEFGMRESSetShift", pipefgmres->shift, &shift, &flg));
  if (flg) PetscCall(KSPPIPEFGMRESSetShift(ksp, shift));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPView_PIPEFGMRES(KSP ksp, PetscViewer viewer)
{
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)ksp->data;
  PetscBool       iascii, isstring;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERSTRING, &isstring));

  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  restart=%" PetscInt_FMT "\n", pipefgmres->max_k));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  happy breakdown tolerance %g\n", (double)pipefgmres->haptol));
#if defined(PETSC_USE_COMPLEX)
    PetscCall(PetscViewerASCIIPrintf(viewer, "  shift=%g+%gi\n", (double)PetscRealPart(pipefgmres->shift), (double)PetscImaginaryPart(pipefgmres->shift)));
#else
    PetscCall(PetscViewerASCIIPrintf(viewer, "  shift=%g\n", (double)pipefgmres->shift));
#endif
  } else if (isstring) {
    PetscCall(PetscViewerStringSPrintf(viewer, "restart %" PetscInt_FMT, pipefgmres->max_k));
#if defined(PETSC_USE_COMPLEX)
    PetscCall(PetscViewerStringSPrintf(viewer, "   shift=%g+%gi\n", (double)PetscRealPart(pipefgmres->shift), (double)PetscImaginaryPart(pipefgmres->shift)));
#else
    PetscCall(PetscViewerStringSPrintf(viewer, "   shift=%g\n", (double)pipefgmres->shift));
#endif
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPReset_PIPEFGMRES(KSP ksp)
{
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)ksp->data;
  PetscInt        i;

  PetscFunctionBegin;
  PetscCall(PetscFree(pipefgmres->prevecs));
  PetscCall(PetscFree(pipefgmres->zvecs));
  for (i = 0; i < pipefgmres->nwork_alloc; i++) {
    PetscCall(VecDestroyVecs(pipefgmres->mwork_alloc[i], &pipefgmres->prevecs_user_work[i]));
    PetscCall(VecDestroyVecs(pipefgmres->mwork_alloc[i], &pipefgmres->zvecs_user_work[i]));
  }
  PetscCall(PetscFree(pipefgmres->prevecs_user_work));
  PetscCall(PetscFree(pipefgmres->zvecs_user_work));
  PetscCall(PetscFree(pipefgmres->redux));
  PetscCall(KSPReset_GMRES(ksp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   KSPPIPEFGMRES - Implements the Pipelined (1-stage) Flexible Generalized Minimal Residual method. [](sec_pipelineksp). [](sec_flexibleksp)

   Options Database Keys:
+   -ksp_gmres_restart <restart> - the number of Krylov directions to orthogonalize against
.   -ksp_gmres_haptol <tol> - sets the tolerance for "happy ending" (exact convergence)
.   -ksp_gmres_preallocate - preallocate all the Krylov search directions initially (otherwise groups of
.   -ksp_pipefgmres_shift - the shift to use (defaults to 1. See KSPPIPEFGMRESSetShift()
                             vectors are allocated as needed)
-   -ksp_gmres_krylov_monitor - plot the Krylov space generated

   Level: intermediate

   Notes:
   This variant is not "explicitly normalized" like `KSPPGMRES`, and requires a shift parameter.

   A heuristic for choosing the shift parameter is the largest eigenvalue of the preconditioned operator.

   Only right preconditioning is supported (but this preconditioner may be nonlinear/variable/inexact, as with `KSPFGMRES`).

   MPI configuration may be necessary for reductions to make asynchronous progress, which is important for performance of pipelined methods.
   See [](doc_faq_pipelined)

   Developer Note:
    This class is subclassed off of `KSPGMRES`.

   Contributed by:
   P. Sanan and S.M. Schnepp

   Reference:
    P. Sanan, S.M. Schnepp, and D.A. May,
    "Pipelined, Flexible Krylov Subspace Methods,"
    SIAM Journal on Scientific Computing 2016 38:5, C441-C470,
    DOI: 10.1137/15M1049130

.seealso: [](chapter_ksp), [](doc_faq_pipelined), [](sec_pipelineksp), [](sec_flexibleksp), `KSPCreate()`, `KSPSetType()`, `KSPType`, `KSP`, `KSPLGMRES`, `KSPPIPECG`, `KSPPIPECR`, `KSPPGMRES`, `KSPFGMRES`
          `KSPGMRESSetRestart()`, `KSPGMRESSetHapTol()`, `KSPGMRESSetPreAllocateVectors()`, `KSPGMRESMonitorKrylov()`, `KSPPIPEFGMRESSetShift()`
M*/

PETSC_EXTERN PetscErrorCode KSPCreate_PIPEFGMRES(KSP ksp)
{
  KSP_PIPEFGMRES *pipefgmres;

  PetscFunctionBegin;
  PetscCall(PetscNew(&pipefgmres));

  ksp->data                              = (void *)pipefgmres;
  ksp->ops->buildsolution                = KSPBuildSolution_PIPEFGMRES;
  ksp->ops->setup                        = KSPSetUp_PIPEFGMRES;
  ksp->ops->solve                        = KSPSolve_PIPEFGMRES;
  ksp->ops->reset                        = KSPReset_PIPEFGMRES;
  ksp->ops->destroy                      = KSPDestroy_PIPEFGMRES;
  ksp->ops->view                         = KSPView_PIPEFGMRES;
  ksp->ops->setfromoptions               = KSPSetFromOptions_PIPEFGMRES;
  ksp->ops->computeextremesingularvalues = KSPComputeExtremeSingularValues_GMRES;
  ksp->ops->computeeigenvalues           = KSPComputeEigenvalues_GMRES;

  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_UNPRECONDITIONED, PC_RIGHT, 3));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_NONE, PC_RIGHT, 1));

  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPGMRESSetPreAllocateVectors_C", KSPGMRESSetPreAllocateVectors_GMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPGMRESSetRestart_C", KSPGMRESSetRestart_GMRES));
  PetscCall(PetscObjectComposeFunction((PetscObject)ksp, "KSPGMRESGetRestart_C", KSPGMRESGetRestart_GMRES));

  pipefgmres->nextra_vecs    = 1;
  pipefgmres->haptol         = 1.0e-30;
  pipefgmres->q_preallocate  = 0;
  pipefgmres->delta_allocate = PIPEFGMRES_DELTA_DIRECTIONS;
  pipefgmres->orthog         = NULL;
  pipefgmres->nrs            = NULL;
  pipefgmres->sol_temp       = NULL;
  pipefgmres->max_k          = PIPEFGMRES_DEFAULT_MAXK;
  pipefgmres->Rsvd           = NULL;
  pipefgmres->orthogwork     = NULL;
  pipefgmres->cgstype        = KSP_GMRES_CGS_REFINE_NEVER;
  pipefgmres->shift          = 1.0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPPIPEFGMRESGetNewVectors(KSP ksp, PetscInt it)
{
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)ksp->data;
  PetscInt        nwork      = pipefgmres->nwork_alloc; /* number of work vector chunks allocated */
  PetscInt        nalloc;                               /* number to allocate */
  PetscInt        k;

  PetscFunctionBegin;
  nalloc = pipefgmres->delta_allocate; /* number of vectors to allocate
                                      in a single chunk */

  /* Adjust the number to allocate to make sure that we don't exceed the
     number of available slots (pipefgmres->vecs_allocated)*/
  if (it + VEC_OFFSET + nalloc >= pipefgmres->vecs_allocated) nalloc = pipefgmres->vecs_allocated - it - VEC_OFFSET;
  if (!nalloc) PetscFunctionReturn(PETSC_SUCCESS);

  pipefgmres->vv_allocated += nalloc; /* vv_allocated is the number of vectors allocated */

  /* work vectors */
  PetscCall(KSPCreateVecs(ksp, nalloc, &pipefgmres->user_work[nwork], 0, NULL));
  for (k = 0; k < nalloc; k++) pipefgmres->vecs[it + VEC_OFFSET + k] = pipefgmres->user_work[nwork][k];
  /* specify size of chunk allocated */
  pipefgmres->mwork_alloc[nwork] = nalloc;

  /* preconditioned vectors (note we don't use VEC_OFFSET) */
  PetscCall(KSPCreateVecs(ksp, nalloc, &pipefgmres->prevecs_user_work[nwork], 0, NULL));
  for (k = 0; k < nalloc; k++) pipefgmres->prevecs[it + k] = pipefgmres->prevecs_user_work[nwork][k];

  PetscCall(KSPCreateVecs(ksp, nalloc, &pipefgmres->zvecs_user_work[nwork], 0, NULL));
  for (k = 0; k < nalloc; k++) pipefgmres->zvecs[it + k] = pipefgmres->zvecs_user_work[nwork][k];

  /* increment the number of work vector chunks */
  pipefgmres->nwork_alloc++;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  KSPPIPEFGMRESSetShift - Set the shift parameter for the flexible, pipelined `KSPPIPEFGMRES` solver.

  Logically Collective

  Input Parameters:
+  ksp - the Krylov space context
-  shift - the shift

  Options Database Key:
.  -ksp_pipefgmres_shift <shift> - set the shift parameter

  Level: intermediate

  Note:
  A heuristic is to set this to be comparable to the largest eigenvalue of the preconditioned operator.
  This can be achieved with PETSc itself by using a few iterations of a Krylov method.
  See `KSPComputeEigenvalues()` (and note the caveats there).

.seealso: [](chapter_ksp), `KSPPIPEFGMRES`, `KSPComputeEigenvalues()`
@*/
PetscErrorCode KSPPIPEFGMRESSetShift(KSP ksp, PetscScalar shift)
{
  KSP_PIPEFGMRES *pipefgmres = (KSP_PIPEFGMRES *)ksp->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ksp, KSP_CLASSID, 1);
  PetscValidLogicalCollectiveScalar(ksp, shift, 2);
  pipefgmres->shift = shift;
  PetscFunctionReturn(PETSC_SUCCESS);
}

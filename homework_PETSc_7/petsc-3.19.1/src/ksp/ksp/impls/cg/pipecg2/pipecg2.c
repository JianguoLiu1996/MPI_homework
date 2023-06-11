#include <petsc/private/kspimpl.h>

/* The auxiliary functions below are merged vector operations that load vectors from memory once and use
   the data multiple times by performing vector operations element-wise. These functions
   only apply to sequential vectors.
*/
/*   VecMergedDot_Private function merges the dot products for gamma, delta and dp */
static PetscErrorCode VecMergedDot_Private(Vec U, Vec W, Vec R, PetscInt normtype, PetscScalar *ru, PetscScalar *wu, PetscScalar *uu)
{
  const PetscScalar *PETSC_RESTRICT PU, *PETSC_RESTRICT PW, *PETSC_RESTRICT PR;
  PetscScalar sumru = 0.0, sumwu = 0.0, sumuu = 0.0;
  PetscInt    j, n;

  PetscFunctionBegin;
  PetscCall(VecGetArrayRead(U, (const PetscScalar **)&PU));
  PetscCall(VecGetArrayRead(W, (const PetscScalar **)&PW));
  PetscCall(VecGetArrayRead(R, (const PetscScalar **)&PR));
  PetscCall(VecGetLocalSize(U, &n));

  if (normtype == KSP_NORM_PRECONDITIONED) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      sumwu += PW[j] * PetscConj(PU[j]);
      sumru += PR[j] * PetscConj(PU[j]);
      sumuu += PU[j] * PetscConj(PU[j]);
    }
  } else if (normtype == KSP_NORM_UNPRECONDITIONED) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      sumwu += PW[j] * PetscConj(PU[j]);
      sumru += PR[j] * PetscConj(PU[j]);
      sumuu += PR[j] * PetscConj(PR[j]);
    }
  } else if (normtype == KSP_NORM_NATURAL) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      sumwu += PW[j] * PetscConj(PU[j]);
      sumru += PR[j] * PetscConj(PU[j]);
    }
    sumuu = sumru;
  }

  *ru = sumru;
  *wu = sumwu;
  *uu = sumuu;

  PetscCall(VecRestoreArrayRead(U, (const PetscScalar **)&PU));
  PetscCall(VecRestoreArrayRead(W, (const PetscScalar **)&PW));
  PetscCall(VecRestoreArrayRead(R, (const PetscScalar **)&PR));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*   VecMergedDot2_Private function merges the dot products for lambda_1 and lambda_4 */
static PetscErrorCode VecMergedDot2_Private(Vec N, Vec M, Vec W, PetscScalar *wm, PetscScalar *nm)
{
  const PetscScalar *PETSC_RESTRICT PN, *PETSC_RESTRICT PM, *PETSC_RESTRICT PW;
  PetscScalar sumwm = 0.0, sumnm = 0.0;
  PetscInt    j, n;

  PetscFunctionBegin;
  PetscCall(VecGetArrayRead(W, (const PetscScalar **)&PW));
  PetscCall(VecGetArrayRead(N, (const PetscScalar **)&PN));
  PetscCall(VecGetArrayRead(M, (const PetscScalar **)&PM));
  PetscCall(VecGetLocalSize(N, &n));

  PetscPragmaSIMD
  for (j = 0; j < n; j++) {
    sumwm += PW[j] * PetscConj(PM[j]);
    sumnm += PN[j] * PetscConj(PM[j]);
  }

  *wm = sumwm;
  *nm = sumnm;

  PetscCall(VecRestoreArrayRead(W, (const PetscScalar **)&PW));
  PetscCall(VecRestoreArrayRead(N, (const PetscScalar **)&PN));
  PetscCall(VecRestoreArrayRead(M, (const PetscScalar **)&PM));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*   VecMergedOpsShort_Private function merges the dot products, AXPY and SAXPY operations for all vectors for iteration 0  */
static PetscErrorCode VecMergedOpsShort_Private(Vec vx, Vec vr, Vec vz, Vec vw, Vec vp, Vec vq, Vec vc, Vec vd, Vec vg0, Vec vh0, Vec vg1, Vec vh1, Vec vs, Vec va1, Vec vb1, Vec ve, Vec vf, Vec vm, Vec vn, Vec vu, PetscInt normtype, PetscScalar beta0, PetscScalar alpha0, PetscScalar beta1, PetscScalar alpha1, PetscScalar *lambda)
{
  PetscScalar *PETSC_RESTRICT px, *PETSC_RESTRICT pr, *PETSC_RESTRICT pz, *PETSC_RESTRICT pw;
  PetscScalar *PETSC_RESTRICT pp, *PETSC_RESTRICT pq;
  PetscScalar *PETSC_RESTRICT pc, *PETSC_RESTRICT pd, *PETSC_RESTRICT pg0, *PETSC_RESTRICT ph0, *PETSC_RESTRICT pg1, *PETSC_RESTRICT ph1, *PETSC_RESTRICT ps, *PETSC_RESTRICT pa1, *PETSC_RESTRICT pb1, *PETSC_RESTRICT pe, *PETSC_RESTRICT pf, *PETSC_RESTRICT pm, *PETSC_RESTRICT pn, *PETSC_RESTRICT pu;
  PetscInt j, n;

  PetscFunctionBegin;
  PetscCall(VecGetArray(vx, (PetscScalar **)&px));
  PetscCall(VecGetArray(vr, (PetscScalar **)&pr));
  PetscCall(VecGetArray(vz, (PetscScalar **)&pz));
  PetscCall(VecGetArray(vw, (PetscScalar **)&pw));
  PetscCall(VecGetArray(vp, (PetscScalar **)&pp));
  PetscCall(VecGetArray(vq, (PetscScalar **)&pq));
  PetscCall(VecGetArray(vc, (PetscScalar **)&pc));
  PetscCall(VecGetArray(vd, (PetscScalar **)&pd));
  PetscCall(VecGetArray(vg0, (PetscScalar **)&pg0));
  PetscCall(VecGetArray(vh0, (PetscScalar **)&ph0));
  PetscCall(VecGetArray(vg1, (PetscScalar **)&pg1));
  PetscCall(VecGetArray(vh1, (PetscScalar **)&ph1));
  PetscCall(VecGetArray(vs, (PetscScalar **)&ps));
  PetscCall(VecGetArray(va1, (PetscScalar **)&pa1));
  PetscCall(VecGetArray(vb1, (PetscScalar **)&pb1));
  PetscCall(VecGetArray(ve, (PetscScalar **)&pe));
  PetscCall(VecGetArray(vf, (PetscScalar **)&pf));
  PetscCall(VecGetArray(vm, (PetscScalar **)&pm));
  PetscCall(VecGetArray(vn, (PetscScalar **)&pn));
  PetscCall(VecGetArray(vu, (PetscScalar **)&pu));

  PetscCall(VecGetLocalSize(vx, &n));
  for (j = 0; j < 15; j++) lambda[j] = 0.0;

  if (normtype == KSP_NORM_PRECONDITIONED) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      pz[j]  = pn[j];
      pq[j]  = pm[j];
      ps[j]  = pw[j];
      pp[j]  = pu[j];
      pc[j]  = pg0[j];
      pd[j]  = ph0[j];
      pa1[j] = pe[j];
      pb1[j] = pf[j];

      px[j]  = px[j] + alpha0 * pp[j];
      pr[j]  = pr[j] - alpha0 * ps[j];
      pu[j]  = pu[j] - alpha0 * pq[j];
      pw[j]  = pw[j] - alpha0 * pz[j];
      pm[j]  = pm[j] - alpha0 * pc[j];
      pn[j]  = pn[j] - alpha0 * pd[j];
      pg0[j] = pg0[j] - alpha0 * pa1[j];
      ph0[j] = ph0[j] - alpha0 * pb1[j];

      pg1[j] = pg0[j];
      ph1[j] = ph0[j];

      pz[j] = pn[j] + beta1 * pz[j];
      pq[j] = pm[j] + beta1 * pq[j];
      ps[j] = pw[j] + beta1 * ps[j];
      pp[j] = pu[j] + beta1 * pp[j];
      pc[j] = pg0[j] + beta1 * pc[j];
      pd[j] = ph0[j] + beta1 * pd[j];

      px[j] = px[j] + alpha1 * pp[j];
      pr[j] = pr[j] - alpha1 * ps[j];
      pu[j] = pu[j] - alpha1 * pq[j];
      pw[j] = pw[j] - alpha1 * pz[j];
      pm[j] = pm[j] - alpha1 * pc[j];
      pn[j] = pn[j] - alpha1 * pd[j];

      lambda[0] += ps[j] * PetscConj(pu[j]);
      lambda[1] += pw[j] * PetscConj(pm[j]);
      lambda[2] += pw[j] * PetscConj(pq[j]);
      lambda[4] += ps[j] * PetscConj(pq[j]);
      lambda[6] += pn[j] * PetscConj(pm[j]);
      lambda[7] += pn[j] * PetscConj(pq[j]);
      lambda[9] += pz[j] * PetscConj(pq[j]);
      lambda[10] += pr[j] * PetscConj(pu[j]);
      lambda[11] += pw[j] * PetscConj(pu[j]);
      lambda[12] += pu[j] * PetscConj(pu[j]);
    }
    lambda[3]  = PetscConj(lambda[2]);
    lambda[5]  = PetscConj(lambda[1]);
    lambda[8]  = PetscConj(lambda[7]);
    lambda[13] = PetscConj(lambda[11]);
    lambda[14] = PetscConj(lambda[0]);

  } else if (normtype == KSP_NORM_UNPRECONDITIONED) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      pz[j]  = pn[j];
      pq[j]  = pm[j];
      ps[j]  = pw[j];
      pp[j]  = pu[j];
      pc[j]  = pg0[j];
      pd[j]  = ph0[j];
      pa1[j] = pe[j];
      pb1[j] = pf[j];

      px[j]  = px[j] + alpha0 * pp[j];
      pr[j]  = pr[j] - alpha0 * ps[j];
      pu[j]  = pu[j] - alpha0 * pq[j];
      pw[j]  = pw[j] - alpha0 * pz[j];
      pm[j]  = pm[j] - alpha0 * pc[j];
      pn[j]  = pn[j] - alpha0 * pd[j];
      pg0[j] = pg0[j] - alpha0 * pa1[j];
      ph0[j] = ph0[j] - alpha0 * pb1[j];

      pg1[j] = pg0[j];
      ph1[j] = ph0[j];

      pz[j] = pn[j] + beta1 * pz[j];
      pq[j] = pm[j] + beta1 * pq[j];
      ps[j] = pw[j] + beta1 * ps[j];
      pp[j] = pu[j] + beta1 * pp[j];
      pc[j] = pg0[j] + beta1 * pc[j];
      pd[j] = ph0[j] + beta1 * pd[j];

      px[j] = px[j] + alpha1 * pp[j];
      pr[j] = pr[j] - alpha1 * ps[j];
      pu[j] = pu[j] - alpha1 * pq[j];
      pw[j] = pw[j] - alpha1 * pz[j];
      pm[j] = pm[j] - alpha1 * pc[j];
      pn[j] = pn[j] - alpha1 * pd[j];

      lambda[0] += ps[j] * PetscConj(pu[j]);
      lambda[1] += pw[j] * PetscConj(pm[j]);
      lambda[2] += pw[j] * PetscConj(pq[j]);
      lambda[4] += ps[j] * PetscConj(pq[j]);
      lambda[6] += pn[j] * PetscConj(pm[j]);
      lambda[7] += pn[j] * PetscConj(pq[j]);
      lambda[9] += pz[j] * PetscConj(pq[j]);
      lambda[10] += pr[j] * PetscConj(pu[j]);
      lambda[11] += pw[j] * PetscConj(pu[j]);
      lambda[12] += pr[j] * PetscConj(pr[j]);
    }
    lambda[3]  = PetscConj(lambda[2]);
    lambda[5]  = PetscConj(lambda[1]);
    lambda[8]  = PetscConj(lambda[7]);
    lambda[13] = PetscConj(lambda[11]);
    lambda[14] = PetscConj(lambda[0]);

  } else if (normtype == KSP_NORM_NATURAL) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      pz[j]  = pn[j];
      pq[j]  = pm[j];
      ps[j]  = pw[j];
      pp[j]  = pu[j];
      pc[j]  = pg0[j];
      pd[j]  = ph0[j];
      pa1[j] = pe[j];
      pb1[j] = pf[j];

      px[j]  = px[j] + alpha0 * pp[j];
      pr[j]  = pr[j] - alpha0 * ps[j];
      pu[j]  = pu[j] - alpha0 * pq[j];
      pw[j]  = pw[j] - alpha0 * pz[j];
      pm[j]  = pm[j] - alpha0 * pc[j];
      pn[j]  = pn[j] - alpha0 * pd[j];
      pg0[j] = pg0[j] - alpha0 * pa1[j];
      ph0[j] = ph0[j] - alpha0 * pb1[j];

      pg1[j] = pg0[j];
      ph1[j] = ph0[j];

      pz[j] = pn[j] + beta1 * pz[j];
      pq[j] = pm[j] + beta1 * pq[j];
      ps[j] = pw[j] + beta1 * ps[j];
      pp[j] = pu[j] + beta1 * pp[j];
      pc[j] = pg0[j] + beta1 * pc[j];
      pd[j] = ph0[j] + beta1 * pd[j];

      px[j] = px[j] + alpha1 * pp[j];
      pr[j] = pr[j] - alpha1 * ps[j];
      pu[j] = pu[j] - alpha1 * pq[j];
      pw[j] = pw[j] - alpha1 * pz[j];
      pm[j] = pm[j] - alpha1 * pc[j];
      pn[j] = pn[j] - alpha1 * pd[j];

      lambda[0] += ps[j] * PetscConj(pu[j]);
      lambda[1] += pw[j] * PetscConj(pm[j]);
      lambda[2] += pw[j] * PetscConj(pq[j]);
      lambda[4] += ps[j] * PetscConj(pq[j]);
      lambda[6] += pn[j] * PetscConj(pm[j]);
      lambda[7] += pn[j] * PetscConj(pq[j]);
      lambda[9] += pz[j] * PetscConj(pq[j]);
      lambda[10] += pr[j] * PetscConj(pu[j]);
      lambda[11] += pw[j] * PetscConj(pu[j]);
    }
    lambda[3]  = PetscConj(lambda[2]);
    lambda[5]  = PetscConj(lambda[1]);
    lambda[8]  = PetscConj(lambda[7]);
    lambda[13] = PetscConj(lambda[11]);
    lambda[14] = PetscConj(lambda[0]);
    lambda[12] = lambda[10];
  }

  PetscCall(VecRestoreArray(vx, (PetscScalar **)&px));
  PetscCall(VecRestoreArray(vr, (PetscScalar **)&pr));
  PetscCall(VecRestoreArray(vz, (PetscScalar **)&pz));
  PetscCall(VecRestoreArray(vw, (PetscScalar **)&pw));
  PetscCall(VecRestoreArray(vp, (PetscScalar **)&pp));
  PetscCall(VecRestoreArray(vq, (PetscScalar **)&pq));
  PetscCall(VecRestoreArray(vc, (PetscScalar **)&pc));
  PetscCall(VecRestoreArray(vd, (PetscScalar **)&pd));
  PetscCall(VecRestoreArray(vg0, (PetscScalar **)&pg0));
  PetscCall(VecRestoreArray(vh0, (PetscScalar **)&ph0));
  PetscCall(VecRestoreArray(vg1, (PetscScalar **)&pg1));
  PetscCall(VecRestoreArray(vh1, (PetscScalar **)&ph1));
  PetscCall(VecRestoreArray(vs, (PetscScalar **)&ps));
  PetscCall(VecRestoreArray(va1, (PetscScalar **)&pa1));
  PetscCall(VecRestoreArray(vb1, (PetscScalar **)&pb1));
  PetscCall(VecRestoreArray(ve, (PetscScalar **)&pe));
  PetscCall(VecRestoreArray(vf, (PetscScalar **)&pf));
  PetscCall(VecRestoreArray(vm, (PetscScalar **)&pm));
  PetscCall(VecRestoreArray(vn, (PetscScalar **)&pn));
  PetscCall(VecRestoreArray(vu, (PetscScalar **)&pu));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*   VecMergedOps_Private function merges the dot products, AXPY and SAXPY operations for all vectors for iteration > 0  */
static PetscErrorCode VecMergedOps_Private(Vec vx, Vec vr, Vec vz, Vec vw, Vec vp, Vec vq, Vec vc, Vec vd, Vec vg0, Vec vh0, Vec vg1, Vec vh1, Vec vs, Vec va1, Vec vb1, Vec ve, Vec vf, Vec vm, Vec vn, Vec vu, PetscInt normtype, PetscScalar beta0, PetscScalar alpha0, PetscScalar beta1, PetscScalar alpha1, PetscScalar *lambda, PetscScalar alphaold)
{
  PetscScalar *PETSC_RESTRICT px, *PETSC_RESTRICT pr, *PETSC_RESTRICT pz, *PETSC_RESTRICT pw;
  PetscScalar *PETSC_RESTRICT pp, *PETSC_RESTRICT pq;
  PetscScalar *PETSC_RESTRICT pc, *PETSC_RESTRICT pd, *PETSC_RESTRICT pg0, *PETSC_RESTRICT ph0, *PETSC_RESTRICT pg1, *PETSC_RESTRICT ph1, *PETSC_RESTRICT ps, *PETSC_RESTRICT pa1, *PETSC_RESTRICT pb1, *PETSC_RESTRICT pe, *PETSC_RESTRICT pf, *PETSC_RESTRICT pm, *PETSC_RESTRICT pn, *PETSC_RESTRICT pu;
  PetscInt j, n;

  PetscFunctionBegin;
  PetscCall(VecGetArray(vx, (PetscScalar **)&px));
  PetscCall(VecGetArray(vr, (PetscScalar **)&pr));
  PetscCall(VecGetArray(vz, (PetscScalar **)&pz));
  PetscCall(VecGetArray(vw, (PetscScalar **)&pw));
  PetscCall(VecGetArray(vp, (PetscScalar **)&pp));
  PetscCall(VecGetArray(vq, (PetscScalar **)&pq));
  PetscCall(VecGetArray(vc, (PetscScalar **)&pc));
  PetscCall(VecGetArray(vd, (PetscScalar **)&pd));
  PetscCall(VecGetArray(vg0, (PetscScalar **)&pg0));
  PetscCall(VecGetArray(vh0, (PetscScalar **)&ph0));
  PetscCall(VecGetArray(vg1, (PetscScalar **)&pg1));
  PetscCall(VecGetArray(vh1, (PetscScalar **)&ph1));
  PetscCall(VecGetArray(vs, (PetscScalar **)&ps));
  PetscCall(VecGetArray(va1, (PetscScalar **)&pa1));
  PetscCall(VecGetArray(vb1, (PetscScalar **)&pb1));
  PetscCall(VecGetArray(ve, (PetscScalar **)&pe));
  PetscCall(VecGetArray(vf, (PetscScalar **)&pf));
  PetscCall(VecGetArray(vm, (PetscScalar **)&pm));
  PetscCall(VecGetArray(vn, (PetscScalar **)&pn));
  PetscCall(VecGetArray(vu, (PetscScalar **)&pu));

  PetscCall(VecGetLocalSize(vx, &n));
  for (j = 0; j < 15; j++) lambda[j] = 0.0;

  if (normtype == KSP_NORM_PRECONDITIONED) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      pa1[j] = (pg1[j] - pg0[j]) / alphaold;
      pb1[j] = (ph1[j] - ph0[j]) / alphaold;

      pz[j]  = pn[j] + beta0 * pz[j];
      pq[j]  = pm[j] + beta0 * pq[j];
      ps[j]  = pw[j] + beta0 * ps[j];
      pp[j]  = pu[j] + beta0 * pp[j];
      pc[j]  = pg0[j] + beta0 * pc[j];
      pd[j]  = ph0[j] + beta0 * pd[j];
      pa1[j] = pe[j] + beta0 * pa1[j];
      pb1[j] = pf[j] + beta0 * pb1[j];

      px[j]  = px[j] + alpha0 * pp[j];
      pr[j]  = pr[j] - alpha0 * ps[j];
      pu[j]  = pu[j] - alpha0 * pq[j];
      pw[j]  = pw[j] - alpha0 * pz[j];
      pm[j]  = pm[j] - alpha0 * pc[j];
      pn[j]  = pn[j] - alpha0 * pd[j];
      pg0[j] = pg0[j] - alpha0 * pa1[j];
      ph0[j] = ph0[j] - alpha0 * pb1[j];

      pg1[j] = pg0[j];
      ph1[j] = ph0[j];

      pz[j] = pn[j] + beta1 * pz[j];
      pq[j] = pm[j] + beta1 * pq[j];
      ps[j] = pw[j] + beta1 * ps[j];
      pp[j] = pu[j] + beta1 * pp[j];
      pc[j] = pg0[j] + beta1 * pc[j];
      pd[j] = ph0[j] + beta1 * pd[j];

      px[j] = px[j] + alpha1 * pp[j];
      pr[j] = pr[j] - alpha1 * ps[j];
      pu[j] = pu[j] - alpha1 * pq[j];
      pw[j] = pw[j] - alpha1 * pz[j];
      pm[j] = pm[j] - alpha1 * pc[j];
      pn[j] = pn[j] - alpha1 * pd[j];

      lambda[0] += ps[j] * PetscConj(pu[j]);
      lambda[1] += pw[j] * PetscConj(pm[j]);
      lambda[2] += pw[j] * PetscConj(pq[j]);
      lambda[4] += ps[j] * PetscConj(pq[j]);
      lambda[6] += pn[j] * PetscConj(pm[j]);
      lambda[7] += pn[j] * PetscConj(pq[j]);
      lambda[9] += pz[j] * PetscConj(pq[j]);
      lambda[10] += pr[j] * PetscConj(pu[j]);
      lambda[11] += pw[j] * PetscConj(pu[j]);
      lambda[12] += pu[j] * PetscConj(pu[j]);
    }
    lambda[3]  = PetscConj(lambda[2]);
    lambda[5]  = PetscConj(lambda[1]);
    lambda[8]  = PetscConj(lambda[7]);
    lambda[13] = PetscConj(lambda[11]);
    lambda[14] = PetscConj(lambda[0]);
  } else if (normtype == KSP_NORM_UNPRECONDITIONED) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      pa1[j] = (pg1[j] - pg0[j]) / alphaold;
      pb1[j] = (ph1[j] - ph0[j]) / alphaold;

      pz[j]  = pn[j] + beta0 * pz[j];
      pq[j]  = pm[j] + beta0 * pq[j];
      ps[j]  = pw[j] + beta0 * ps[j];
      pp[j]  = pu[j] + beta0 * pp[j];
      pc[j]  = pg0[j] + beta0 * pc[j];
      pd[j]  = ph0[j] + beta0 * pd[j];
      pa1[j] = pe[j] + beta0 * pa1[j];
      pb1[j] = pf[j] + beta0 * pb1[j];

      px[j]  = px[j] + alpha0 * pp[j];
      pr[j]  = pr[j] - alpha0 * ps[j];
      pu[j]  = pu[j] - alpha0 * pq[j];
      pw[j]  = pw[j] - alpha0 * pz[j];
      pm[j]  = pm[j] - alpha0 * pc[j];
      pn[j]  = pn[j] - alpha0 * pd[j];
      pg0[j] = pg0[j] - alpha0 * pa1[j];
      ph0[j] = ph0[j] - alpha0 * pb1[j];

      pg1[j] = pg0[j];
      ph1[j] = ph0[j];

      pz[j] = pn[j] + beta1 * pz[j];
      pq[j] = pm[j] + beta1 * pq[j];
      ps[j] = pw[j] + beta1 * ps[j];
      pp[j] = pu[j] + beta1 * pp[j];
      pc[j] = pg0[j] + beta1 * pc[j];
      pd[j] = ph0[j] + beta1 * pd[j];

      px[j] = px[j] + alpha1 * pp[j];
      pr[j] = pr[j] - alpha1 * ps[j];
      pu[j] = pu[j] - alpha1 * pq[j];
      pw[j] = pw[j] - alpha1 * pz[j];
      pm[j] = pm[j] - alpha1 * pc[j];
      pn[j] = pn[j] - alpha1 * pd[j];

      lambda[0] += ps[j] * PetscConj(pu[j]);
      lambda[1] += pw[j] * PetscConj(pm[j]);
      lambda[2] += pw[j] * PetscConj(pq[j]);
      lambda[4] += ps[j] * PetscConj(pq[j]);
      lambda[6] += pn[j] * PetscConj(pm[j]);
      lambda[7] += pn[j] * PetscConj(pq[j]);
      lambda[9] += pz[j] * PetscConj(pq[j]);
      lambda[10] += pr[j] * PetscConj(pu[j]);
      lambda[11] += pw[j] * PetscConj(pu[j]);
      lambda[12] += pr[j] * PetscConj(pr[j]);
    }
    lambda[3]  = PetscConj(lambda[2]);
    lambda[5]  = PetscConj(lambda[1]);
    lambda[8]  = PetscConj(lambda[7]);
    lambda[13] = PetscConj(lambda[11]);
    lambda[14] = PetscConj(lambda[0]);
  } else if (normtype == KSP_NORM_NATURAL) {
    PetscPragmaSIMD
    for (j = 0; j < n; j++) {
      pa1[j] = (pg1[j] - pg0[j]) / alphaold;
      pb1[j] = (ph1[j] - ph0[j]) / alphaold;

      pz[j]  = pn[j] + beta0 * pz[j];
      pq[j]  = pm[j] + beta0 * pq[j];
      ps[j]  = pw[j] + beta0 * ps[j];
      pp[j]  = pu[j] + beta0 * pp[j];
      pc[j]  = pg0[j] + beta0 * pc[j];
      pd[j]  = ph0[j] + beta0 * pd[j];
      pa1[j] = pe[j] + beta0 * pa1[j];
      pb1[j] = pf[j] + beta0 * pb1[j];

      px[j]  = px[j] + alpha0 * pp[j];
      pr[j]  = pr[j] - alpha0 * ps[j];
      pu[j]  = pu[j] - alpha0 * pq[j];
      pw[j]  = pw[j] - alpha0 * pz[j];
      pm[j]  = pm[j] - alpha0 * pc[j];
      pn[j]  = pn[j] - alpha0 * pd[j];
      pg0[j] = pg0[j] - alpha0 * pa1[j];
      ph0[j] = ph0[j] - alpha0 * pb1[j];

      pg1[j] = pg0[j];
      ph1[j] = ph0[j];

      pz[j] = pn[j] + beta1 * pz[j];
      pq[j] = pm[j] + beta1 * pq[j];
      ps[j] = pw[j] + beta1 * ps[j];
      pp[j] = pu[j] + beta1 * pp[j];
      pc[j] = pg0[j] + beta1 * pc[j];
      pd[j] = ph0[j] + beta1 * pd[j];

      px[j] = px[j] + alpha1 * pp[j];
      pr[j] = pr[j] - alpha1 * ps[j];
      pu[j] = pu[j] - alpha1 * pq[j];
      pw[j] = pw[j] - alpha1 * pz[j];
      pm[j] = pm[j] - alpha1 * pc[j];
      pn[j] = pn[j] - alpha1 * pd[j];

      lambda[0] += ps[j] * PetscConj(pu[j]);
      lambda[1] += pw[j] * PetscConj(pm[j]);
      lambda[2] += pw[j] * PetscConj(pq[j]);
      lambda[4] += ps[j] * PetscConj(pq[j]);
      lambda[6] += pn[j] * PetscConj(pm[j]);
      lambda[7] += pn[j] * PetscConj(pq[j]);
      lambda[9] += pz[j] * PetscConj(pq[j]);
      lambda[10] += pr[j] * PetscConj(pu[j]);
      lambda[11] += pw[j] * PetscConj(pu[j]);
    }
    lambda[3]  = PetscConj(lambda[2]);
    lambda[5]  = PetscConj(lambda[1]);
    lambda[8]  = PetscConj(lambda[7]);
    lambda[13] = PetscConj(lambda[11]);
    lambda[14] = PetscConj(lambda[0]);
    lambda[12] = lambda[10];
  }

  PetscCall(VecRestoreArray(vx, (PetscScalar **)&px));
  PetscCall(VecRestoreArray(vr, (PetscScalar **)&pr));
  PetscCall(VecRestoreArray(vz, (PetscScalar **)&pz));
  PetscCall(VecRestoreArray(vw, (PetscScalar **)&pw));
  PetscCall(VecRestoreArray(vp, (PetscScalar **)&pp));
  PetscCall(VecRestoreArray(vq, (PetscScalar **)&pq));
  PetscCall(VecRestoreArray(vc, (PetscScalar **)&pc));
  PetscCall(VecRestoreArray(vd, (PetscScalar **)&pd));
  PetscCall(VecRestoreArray(vg0, (PetscScalar **)&pg0));
  PetscCall(VecRestoreArray(vh0, (PetscScalar **)&ph0));
  PetscCall(VecRestoreArray(vg1, (PetscScalar **)&pg1));
  PetscCall(VecRestoreArray(vh1, (PetscScalar **)&ph1));
  PetscCall(VecRestoreArray(vs, (PetscScalar **)&ps));
  PetscCall(VecRestoreArray(va1, (PetscScalar **)&pa1));
  PetscCall(VecRestoreArray(vb1, (PetscScalar **)&pb1));
  PetscCall(VecRestoreArray(ve, (PetscScalar **)&pe));
  PetscCall(VecRestoreArray(vf, (PetscScalar **)&pf));
  PetscCall(VecRestoreArray(vm, (PetscScalar **)&pm));
  PetscCall(VecRestoreArray(vn, (PetscScalar **)&pn));
  PetscCall(VecRestoreArray(vu, (PetscScalar **)&pu));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
     KSPSetUp_PIPECG2 - Sets up the workspace needed by the PIPECG method.

      This is called once, usually automatically by KSPSolve() or KSPSetUp()
     but can be called directly by KSPSetUp()
*/
static PetscErrorCode KSPSetUp_PIPECG2(KSP ksp)
{
  PetscFunctionBegin;
  /* get work vectors needed by PIPECG2 */
  PetscCall(KSPSetWorkVecs(ksp, 20));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
 KSPSolve_PIPECG2 - This routine actually applies the PIPECG2 method
*/
static PetscErrorCode KSPSolve_PIPECG2(KSP ksp)
{
  PetscInt    i, n;
  PetscScalar alpha[2], beta[2], gamma[2], delta[2], lambda[15];
  PetscScalar dps = 0.0, alphaold = 0.0;
  PetscReal   dp = 0.0;
  Vec         X, B, Z, P, W, Q, U, M, N, R, S, C, D, E, F, G[2], H[2], A1, B1;
  Mat         Amat, Pmat;
  PetscBool   diagonalscale;
  MPI_Comm    pcomm;
  MPI_Request req;
  MPI_Status  stat;

  PetscFunctionBegin;
  pcomm = PetscObjectComm((PetscObject)ksp);
  PetscCall(PCGetDiagonalScale(ksp->pc, &diagonalscale));
  PetscCheck(!diagonalscale, PetscObjectComm((PetscObject)ksp), PETSC_ERR_SUP, "Krylov method %s does not support diagonal scaling", ((PetscObject)ksp)->type_name);

  X    = ksp->vec_sol;
  B    = ksp->vec_rhs;
  M    = ksp->work[0];
  Z    = ksp->work[1];
  P    = ksp->work[2];
  N    = ksp->work[3];
  W    = ksp->work[4];
  Q    = ksp->work[5];
  U    = ksp->work[6];
  R    = ksp->work[7];
  S    = ksp->work[8];
  C    = ksp->work[9];
  D    = ksp->work[10];
  E    = ksp->work[11];
  F    = ksp->work[12];
  G[0] = ksp->work[13];
  H[0] = ksp->work[14];
  G[1] = ksp->work[15];
  H[1] = ksp->work[16];
  A1   = ksp->work[17];
  B1   = ksp->work[18];

  PetscCall(PetscMemzero(alpha, 2 * sizeof(PetscScalar)));
  PetscCall(PetscMemzero(beta, 2 * sizeof(PetscScalar)));
  PetscCall(PetscMemzero(gamma, 2 * sizeof(PetscScalar)));
  PetscCall(PetscMemzero(delta, 2 * sizeof(PetscScalar)));
  PetscCall(PetscMemzero(lambda, 15 * sizeof(PetscScalar)));

  PetscCall(VecGetLocalSize(B, &n));
  PetscCall(PCGetOperators(ksp->pc, &Amat, &Pmat));

  ksp->its = 0;
  if (!ksp->guess_zero) {
    PetscCall(KSP_MatMult(ksp, Amat, X, R)); /*  r <- b - Ax  */
    PetscCall(VecAYPX(R, -1.0, B));
  } else {
    PetscCall(VecCopy(B, R)); /*  r <- b (x is 0) */
  }

  PetscCall(KSP_PCApply(ksp, R, U));       /*  u <- Br  */
  PetscCall(KSP_MatMult(ksp, Amat, U, W)); /*  w <- Au  */

  PetscCall(VecMergedDot_Private(U, W, R, ksp->normtype, &gamma[0], &delta[0], &dps)); /*  gamma  <- r'*u , delta <- w'*u , dp <- u'*u or r'*r or r'*u depending on ksp_norm_type  */
  lambda[10] = gamma[0];
  lambda[11] = delta[0];
  lambda[12] = dps;

#if defined(PETSC_HAVE_MPI_NONBLOCKING_COLLECTIVES)
  PetscCallMPI(MPI_Iallreduce(MPI_IN_PLACE, &lambda[10], 3, MPIU_SCALAR, MPIU_SUM, pcomm, &req));
#else
  PetscCall(MPIU_Allreduce(MPI_IN_PLACE, &lambda[10], 3, MPIU_SCALAR, MPIU_SUM, pcomm));
  req = MPI_REQUEST_NULL;
#endif

  PetscCall(KSP_PCApply(ksp, W, M));       /*  m <- Bw  */
  PetscCall(KSP_MatMult(ksp, Amat, M, N)); /*  n <- Am  */

  PetscCall(KSP_PCApply(ksp, N, G[0]));          /*  g <- Bn  */
  PetscCall(KSP_MatMult(ksp, Amat, G[0], H[0])); /*  h <- Ag  */

  PetscCall(KSP_PCApply(ksp, H[0], E));    /*  e <- Bh  */
  PetscCall(KSP_MatMult(ksp, Amat, E, F)); /*  f <- Ae  */

  PetscCallMPI(MPI_Wait(&req, &stat));

  gamma[0] = lambda[10];
  delta[0] = lambda[11];
  dp       = PetscSqrtReal(PetscAbsScalar(lambda[12]));

  PetscCall(VecMergedDot2_Private(N, M, W, &lambda[1], &lambda[6])); /*  lambda_1 <- w'*m , lambda_4 <- n'*m  */
  PetscCall(MPIU_Allreduce(MPI_IN_PLACE, &lambda[1], 1, MPIU_SCALAR, MPIU_SUM, pcomm));
  PetscCall(MPIU_Allreduce(MPI_IN_PLACE, &lambda[6], 1, MPIU_SCALAR, MPIU_SUM, pcomm));

  lambda[5]  = PetscConj(lambda[1]);
  lambda[13] = PetscConj(lambda[11]);

  PetscCall(KSPLogResidualHistory(ksp, dp));
  PetscCall(KSPMonitor(ksp, 0, dp));
  ksp->rnorm = dp;

  PetscCall((*ksp->converged)(ksp, 0, dp, &ksp->reason, ksp->cnvP)); /* test for convergence */
  if (ksp->reason) PetscFunctionReturn(PETSC_SUCCESS);

  for (i = 2; i < ksp->max_it; i += 2) {
    if (i == 2) {
      beta[0]  = 0;
      alpha[0] = gamma[0] / delta[0];

      gamma[1] = gamma[0] - alpha[0] * lambda[13] - alpha[0] * delta[0] + alpha[0] * alpha[0] * lambda[1];
      delta[1] = delta[0] - alpha[0] * lambda[1] - alpha[0] * lambda[5] + alpha[0] * alpha[0] * lambda[6];

      beta[1]  = gamma[1] / gamma[0];
      alpha[1] = gamma[1] / (delta[1] - beta[1] / alpha[0] * gamma[1]);

      PetscCall(VecMergedOpsShort_Private(X, R, Z, W, P, Q, C, D, G[0], H[0], G[1], H[1], S, A1, B1, E, F, M, N, U, ksp->normtype, beta[0], alpha[0], beta[1], alpha[1], lambda));
    } else {
      beta[0]  = gamma[1] / gamma[0];
      alpha[0] = gamma[1] / (delta[1] - beta[0] / alpha[1] * gamma[1]);

      gamma[0] = gamma[1];
      delta[0] = delta[1];

      gamma[1] = gamma[0] - alpha[0] * (lambda[13] + beta[0] * lambda[14]) - alpha[0] * (delta[0] + beta[0] * lambda[0]) + alpha[0] * alpha[0] * (lambda[1] + beta[0] * lambda[2] + beta[0] * lambda[3] + beta[0] * beta[0] * lambda[4]);

      delta[1] = delta[0] - alpha[0] * (lambda[1] + beta[0] * lambda[2]) - alpha[0] * (lambda[5] + beta[0] * lambda[3]) + alpha[0] * alpha[0] * (lambda[6] + beta[0] * lambda[7] + beta[0] * lambda[8] + beta[0] * beta[0] * lambda[9]);

      beta[1]  = gamma[1] / gamma[0];
      alpha[1] = gamma[1] / (delta[1] - beta[1] / alpha[0] * gamma[1]);

      PetscCall(VecMergedOps_Private(X, R, Z, W, P, Q, C, D, G[0], H[0], G[1], H[1], S, A1, B1, E, F, M, N, U, ksp->normtype, beta[0], alpha[0], beta[1], alpha[1], lambda, alphaold));
    }

    gamma[0] = gamma[1];
    delta[0] = delta[1];

#if defined(PETSC_HAVE_MPI_NONBLOCKING_COLLECTIVES)
    PetscCallMPI(MPI_Iallreduce(MPI_IN_PLACE, lambda, 15, MPIU_SCALAR, MPIU_SUM, pcomm, &req));
#else
    PetscCall(MPIU_Allreduce(MPI_IN_PLACE, lambda, 15, MPIU_SCALAR, MPIU_SUM, pcomm));
    req = MPI_REQUEST_NULL;
#endif

    PetscCall(KSP_PCApply(ksp, N, G[0]));          /*  g <- Bn  */
    PetscCall(KSP_MatMult(ksp, Amat, G[0], H[0])); /*  h <- Ag  */

    PetscCall(KSP_PCApply(ksp, H[0], E));    /*  e <- Bh  */
    PetscCall(KSP_MatMult(ksp, Amat, E, F)); /*  f <- Ae */

    PetscCallMPI(MPI_Wait(&req, &stat));

    gamma[1] = lambda[10];
    delta[1] = lambda[11];
    dp       = PetscSqrtReal(PetscAbsScalar(lambda[12]));

    alphaold = alpha[1];
    ksp->its = i;

    if (i > 0) {
      if (ksp->normtype == KSP_NORM_NONE) dp = 0.0;
      ksp->rnorm = dp;
      PetscCall(KSPLogResidualHistory(ksp, dp));
      PetscCall(KSPMonitor(ksp, i, dp));
      PetscCall((*ksp->converged)(ksp, i, dp, &ksp->reason, ksp->cnvP));
      if (ksp->reason) break;
    }
  }

  if (i >= ksp->max_it) ksp->reason = KSP_DIVERGED_ITS;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   KSPPIPECG2 - Pipelined conjugate gradient method with a single non-blocking reduction per two iterations. [](sec_pipelineksp)

   Level: intermediate

   Notes:
   This method has only a single non-blocking reduction per two iterations, compared to 2 blocking for standard CG.  The
   non-blocking reduction is overlapped by two matrix-vector products and two preconditioner applications.

   MPI configuration may be necessary for reductions to make asynchronous progress, which is important for performance of pipelined methods.
   See [](doc_faq_pipelined)

   Contributed by:
   Manasi Tiwari, Computational and Data Sciences, Indian Institute of Science, Bangalore

   Reference:
   Manasi Tiwari and Sathish Vadhiyar, "Pipelined Conjugate Gradient Methods for Distributed Memory Systems",
   Submitted to International Conference on High Performance Computing, Data and Analytics 2020.

   Developer Note:
   The implementation code contains a good amount of hand tuned fusion of multiple inner products and similar computations on multiple vectors

.seealso: [](chapter_ksp), [](doc_faq_pipelined), [](sec_pipelineksp), `KSPCreate()`, `KSPSetType()`, `KSPCG`, `KSPPIPECG`, `KSPGROPPCG`
M*/
PETSC_EXTERN PetscErrorCode KSPCreate_PIPECG2(KSP ksp)
{
  PetscFunctionBegin;
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_UNPRECONDITIONED, PC_LEFT, 2));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_PRECONDITIONED, PC_LEFT, 2));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_NATURAL, PC_LEFT, 2));
  PetscCall(KSPSetSupportedNorm(ksp, KSP_NORM_NONE, PC_LEFT, 1));

  ksp->ops->setup          = KSPSetUp_PIPECG2;
  ksp->ops->solve          = KSPSolve_PIPECG2;
  ksp->ops->destroy        = KSPDestroyDefault;
  ksp->ops->view           = NULL;
  ksp->ops->setfromoptions = NULL;
  ksp->ops->buildsolution  = KSPBuildSolutionDefault;
  ksp->ops->buildresidual  = KSPBuildResidualDefault;
  PetscFunctionReturn(PETSC_SUCCESS);
}

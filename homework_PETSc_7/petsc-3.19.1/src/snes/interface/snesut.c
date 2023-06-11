
#include <petsc/private/snesimpl.h> /*I   "petsc/private/snesimpl.h"   I*/
#include <petscdm.h>
#include <petscsection.h>
#include <petscblaslapack.h>

/*@C
   SNESMonitorSolution - Monitors progress of the `SNES` solvers by calling
   `VecView()` for the approximate solution at each iteration.

   Collective

   Input Parameters:
+  snes - the `SNES` context
.  its - iteration number
.  fgnorm - 2-norm of residual
-  dummy -  a viewer

   Options Database Key:
.  -snes_monitor_solution [ascii binary draw][:filename][:viewer format] - plots solution at each iteration

   Level: intermediate

   Note:
   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNESSolve()`

.seealso: [](chapter_snes), `SNES`, `SNESMonitorSet()`, `SNESMonitorDefault()`, `VecView()`
@*/
PetscErrorCode SNESMonitorSolution(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  Vec         x;
  PetscViewer viewer = vf->viewer;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  PetscCall(SNESGetSolution(snes, &x));
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  PetscCall(VecView(x, viewer));
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESMonitorResidual - Monitors progress of the `SNES` solvers by calling
   `VecView()` for the residual at each iteration.

   Collective

   Input Parameters:
+  snes - the `SNES` context
.  its - iteration number
.  fgnorm - 2-norm of residual
-  dummy -  a viewer

   Options Database Key:
.  -snes_monitor_residual [ascii binary draw][:filename][:viewer format] - plots residual (not its norm) at each iteration

   Level: intermediate

   Note:
   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNES` solve.

.seealso: [](chapter_snes), `SNES`, `SNESMonitorSet()`, `SNESMonitorDefault()`, `VecView()`, `SNESMonitor()`
@*/
PetscErrorCode SNESMonitorResidual(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  Vec x;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vf->viewer, PETSC_VIEWER_CLASSID, 4);
  PetscCall(SNESGetFunction(snes, &x, NULL, NULL));
  PetscCall(PetscViewerPushFormat(vf->viewer, vf->format));
  PetscCall(VecView(x, vf->viewer));
  PetscCall(PetscViewerPopFormat(vf->viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESMonitorSolutionUpdate - Monitors progress of the `SNES` solvers by calling
   `VecView()` for the UPDATE to the solution at each iteration.

   Collective

   Input Parameters:
+  snes - the `SNES` context
.  its - iteration number
.  fgnorm - 2-norm of residual
-  dummy - a viewer

   Options Database Key:
.  -snes_monitor_solution_update [ascii binary draw][:filename][:viewer format] - plots update to solution at each iteration

   Level: intermediate

   Note:
   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNES` solve.

.seealso: [](chapter_snes), `SNESMonitorSet()`, `SNESMonitorDefault()`, `VecView()`, `SNESMonitor()`, `SNESMonitor()`
@*/
PetscErrorCode SNESMonitorSolutionUpdate(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  Vec         x;
  PetscViewer viewer = vf->viewer;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  PetscCall(SNESGetSolutionUpdate(snes, &x));
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  PetscCall(VecView(x, viewer));
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#include <petscdraw.h>

/*@C
  KSPMonitorSNESResidual - Prints the `SNES` residual norm, as well as the `KSP` residual norm, at each iteration of an iterative solver.

  Collective

  Input Parameters:
+ ksp   - iterative context
. n     - iteration number
. rnorm - 2-norm (preconditioned) residual value (may be estimated).
- vf    - The viewer context

  Options Database Key:
. -snes_monitor_ksp - Activates `KSPMonitorSNESResidual()`

  Level: intermediate

   Note:
   This is not called directly by users, rather one calls `KSPMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `KSP` solve.

.seealso: [](chapter_snes), `KSPMonitorSet()`, `KSPMonitorResidual()`, `KSPMonitorTrueResidualMaxNorm()`, `KSPMonitor()`, `SNESMonitor()`
@*/
PetscErrorCode KSPMonitorSNESResidual(KSP ksp, PetscInt n, PetscReal rnorm, PetscViewerAndFormat *vf)
{
  PetscViewer       viewer = vf->viewer;
  PetscViewerFormat format = vf->format;
  SNES              snes   = (SNES)vf->data;
  Vec               snes_solution, work1, work2;
  PetscReal         snorm;
  PetscInt          tablevel;
  const char       *prefix;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  PetscCall(SNESGetSolution(snes, &snes_solution));
  PetscCall(VecDuplicate(snes_solution, &work1));
  PetscCall(VecDuplicate(snes_solution, &work2));
  PetscCall(KSPBuildSolution(ksp, work1, NULL));
  PetscCall(VecAYPX(work1, -1.0, snes_solution));
  PetscCall(SNESComputeFunction(snes, work1, work2));
  PetscCall(VecNorm(work2, NORM_2, &snorm));
  PetscCall(VecDestroy(&work1));
  PetscCall(VecDestroy(&work2));

  PetscCall(PetscObjectGetTabLevel((PetscObject)ksp, &tablevel));
  PetscCall(PetscObjectGetOptionsPrefix((PetscObject)ksp, &prefix));
  PetscCall(PetscViewerPushFormat(viewer, format));
  PetscCall(PetscViewerASCIIAddTab(viewer, tablevel));
  if (n == 0 && prefix) PetscCall(PetscViewerASCIIPrintf(viewer, "  Residual norms for %s solve.\n", prefix));
  PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Residual norm %5.3e KSP Residual norm %5.3e \n", n, (double)snorm, (double)rnorm));
  PetscCall(PetscViewerASCIISubtractTab(viewer, tablevel));
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  KSPMonitorSNESResidualDrawLG - Plots the linear `KSP` residual norm and the `SNES` residual norm at each iteration of an iterative solver.

  Collective

  Input Parameters:
+ ksp   - iterative context
. n     - iteration number
. rnorm - 2-norm (preconditioned) residual value (may be estimated).
- vf    - The viewer context, created with `KSPMonitorSNESResidualDrawLGCreate()`

  Options Database Key:
. -snes_monitor_ksp draw::draw_lg - Activates `KSPMonitorSNESResidualDrawLG()`

  Level: intermediate

   Note:
   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNESSolve()`

.seealso: [](chapter_snes), `KSPMonitorSet()`, `KSPMonitorTrueResidual()`, `SNESMonitor()`, `KSPMonitor()`, `KSPMonitorSNESResidualDrawLGCreate()`
@*/
PetscErrorCode KSPMonitorSNESResidualDrawLG(KSP ksp, PetscInt n, PetscReal rnorm, PetscViewerAndFormat *vf)
{
  PetscViewer        viewer = vf->viewer;
  PetscViewerFormat  format = vf->format;
  PetscDrawLG        lg     = vf->lg;
  SNES               snes   = (SNES)vf->data;
  Vec                snes_solution, work1, work2;
  PetscReal          snorm;
  KSPConvergedReason reason;
  PetscReal          x[2], y[2];

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  PetscValidHeaderSpecific(lg, PETSC_DRAWLG_CLASSID, 4);
  PetscCall(SNESGetSolution(snes, &snes_solution));
  PetscCall(VecDuplicate(snes_solution, &work1));
  PetscCall(VecDuplicate(snes_solution, &work2));
  PetscCall(KSPBuildSolution(ksp, work1, NULL));
  PetscCall(VecAYPX(work1, -1.0, snes_solution));
  PetscCall(SNESComputeFunction(snes, work1, work2));
  PetscCall(VecNorm(work2, NORM_2, &snorm));
  PetscCall(VecDestroy(&work1));
  PetscCall(VecDestroy(&work2));

  PetscCall(PetscViewerPushFormat(viewer, format));
  if (!n) PetscCall(PetscDrawLGReset(lg));
  x[0] = (PetscReal)n;
  if (rnorm > 0.0) y[0] = PetscLog10Real(rnorm);
  else y[0] = -15.0;
  x[1] = (PetscReal)n;
  if (snorm > 0.0) y[1] = PetscLog10Real(snorm);
  else y[1] = -15.0;
  PetscCall(PetscDrawLGAddPoint(lg, x, y));
  PetscCall(KSPGetConvergedReason(ksp, &reason));
  if (n <= 20 || !(n % 5) || reason) {
    PetscCall(PetscDrawLGDraw(lg));
    PetscCall(PetscDrawLGSave(lg));
  }
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  KSPMonitorSNESResidualDrawLGCreate - Creates the `PetscViewer` used by `KSPMonitorSNESResidualDrawLG()`

  Collective

  Input Parameters:
+ viewer - The `PetscViewer`
. format - The viewer format
- ctx    - An optional user context

  Output Parameter:
. vf    - The viewer context

  Level: intermediate

.seealso: [](chapter_snes), `KSP`, `SNES`, `PetscViewerFormat`, `PetscViewerAndFormat`, `KSPMonitorSet()`, `KSPMonitorTrueResidual()`
@*/
PetscErrorCode KSPMonitorSNESResidualDrawLGCreate(PetscViewer viewer, PetscViewerFormat format, void *ctx, PetscViewerAndFormat **vf)
{
  const char *names[] = {"linear", "nonlinear"};

  PetscFunctionBegin;
  PetscCall(PetscViewerAndFormatCreate(viewer, format, vf));
  (*vf)->data = ctx;
  PetscCall(KSPMonitorLGCreate(PetscObjectComm((PetscObject)viewer), NULL, NULL, "Log Residual Norm", 2, names, PETSC_DECIDE, PETSC_DECIDE, 400, 300, &(*vf)->lg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode SNESMonitorDefaultSetUp(SNES snes, PetscViewerAndFormat *vf)
{
  PetscFunctionBegin;
  if (vf->format == PETSC_VIEWER_DRAW_LG) PetscCall(KSPMonitorLGCreate(PetscObjectComm((PetscObject)vf->viewer), NULL, NULL, "Log Residual Norm", 1, NULL, PETSC_DECIDE, PETSC_DECIDE, 400, 300, &vf->lg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESMonitorDefault - Monitors progress of the `SNES` solvers (default).

   Collective

   Input Parameters:
+  snes - the `SNES` context
.  its - iteration number
.  fgnorm - 2-norm of residual
-  vf - viewer and format structure

   Options Database Key:
.  -snes_monitor - use this function to monitor the convergence of the nonlinear solver

   Level: intermediate

   Notes:
   This routine prints the residual norm at each iteration.

   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNES` solve.

.seealso: [](chapter_snes), `SNESMonitorSet()`, `SNESMonitorSolution()`, `SNESMonitorFunction()`, `SNESMonitorSolution()`, `SNESMonitorResidual()`,
          `SNESMonitorSolutionUpdate()`, `SNESMonitorDefault()`, `SNESMonitorScaling()`, `SNESMonitorRange()`, `SNESMonitorRatio()`,
          `SNESMonitorDefaultField()`, `PetscViewerFormat`, `PetscViewerAndFormat`
@*/
PetscErrorCode SNESMonitorDefault(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  PetscViewer       viewer = vf->viewer;
  PetscViewerFormat format = vf->format;
  PetscBool         isascii, isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &isascii));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  PetscCall(PetscViewerPushFormat(viewer, format));
  if (isascii) {
    PetscCall(PetscViewerASCIIAddTab(viewer, ((PetscObject)snes)->tablevel));
    if (format == PETSC_VIEWER_ASCII_INFO_DETAIL) {
      Vec       dx;
      PetscReal upnorm;
      PetscErrorCode (*objective)(SNES, Vec, PetscReal *, void *);

      PetscCall(SNESGetSolutionUpdate(snes, &dx));
      PetscCall(VecNorm(dx, NORM_2, &upnorm));
      PetscCall(SNESGetObjective(snes, &objective, NULL));
      if (objective) {
        Vec       x;
        PetscReal obj;

        PetscCall(SNESGetSolution(snes, &x));
        PetscCall(SNESComputeObjective(snes, x, &obj));
        PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm %14.12e, Update norm %14.12e, Objective %14.12e\n", its, (double)fgnorm, (double)upnorm, (double)obj));
      } else {
        PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm %14.12e, Update norm %14.12e\n", its, (double)fgnorm, (double)upnorm));
      }
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm %14.12e \n", its, (double)fgnorm));
    }
    PetscCall(PetscViewerASCIISubtractTab(viewer, ((PetscObject)snes)->tablevel));
  } else if (isdraw) {
    if (format == PETSC_VIEWER_DRAW_LG) {
      PetscDrawLG lg = (PetscDrawLG)vf->lg;
      PetscReal   x, y;

      PetscValidHeaderSpecific(lg, PETSC_DRAWLG_CLASSID, 4);
      if (!its) PetscCall(PetscDrawLGReset(lg));
      x = (PetscReal)its;
      if (fgnorm > 0.0) y = PetscLog10Real(fgnorm);
      else y = -15.0;
      PetscCall(PetscDrawLGAddPoint(lg, &x, &y));
      if (its <= 20 || !(its % 5) || snes->reason) {
        PetscCall(PetscDrawLGDraw(lg));
        PetscCall(PetscDrawLGSave(lg));
      }
    }
  }
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESMonitorScaling - Monitors the largest value in each row of the Jacobian.

   Collective

   Input Parameters:
+  snes - the `SNES` context
.  its - iteration number
.  fgnorm - 2-norm of residual
-  vf - viewer and format structure

   Level: intermediate

   Notes:
   This routine prints the largest value in each row of the Jacobian

   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNES` solve.

.seealso: [](chapter_snes), `SNESMonitorSet()`, `SNESMonitorSolution()`, `SNESMonitorRange()`, `SNESMonitorJacUpdateSpectrum()`,
          `PetscViewerFormat`, `PetscViewerAndFormat`
@*/
PetscErrorCode SNESMonitorScaling(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  PetscViewer viewer = vf->viewer;
  KSP         ksp;
  Mat         J;
  Vec         v;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  PetscCall(SNESGetKSP(snes, &ksp));
  PetscCall(KSPGetOperators(ksp, &J, NULL));
  PetscCall(MatCreateVecs(J, &v, NULL));
  PetscCall(MatGetRowMaxAbs(J, v, NULL));
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  PetscCall(PetscViewerASCIIAddTab(viewer, ((PetscObject)snes)->tablevel));
  PetscCall(PetscViewerASCIIPrintf(viewer, "SNES Jacobian maximum row entries\n"));
  PetscCall(VecView(v, viewer));
  PetscCall(PetscViewerASCIISubtractTab(viewer, ((PetscObject)snes)->tablevel));
  PetscCall(PetscViewerPopFormat(viewer));
  PetscCall(VecDestroy(&v));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESMonitorJacUpdateSpectrum - Monitors the spectrun of the change in the Jacobian from the last Jacobian evaluation

   Collective

   Input Parameters:
+  snes - the `SNES` context
.  its - iteration number
.  fgnorm - 2-norm of residual
-  vf - viewer and format structure

   Options Database Key:
.  -snes_monitor_jacupdate_spectrum - activates this monitor

   Level: intermediate

   Notes:
   This routine prints the eigenvalues of the difference in the Jacobians

   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNES` solve.

.seealso: [](chapter_snes), `SNESMonitorSet()`, `SNESMonitorSolution()`, `SNESMonitorRange()`, `PetscViewerFormat`, `PetscViewerAndFormat`
@*/
PetscErrorCode SNESMonitorJacUpdateSpectrum(SNES snes, PetscInt it, PetscReal fnorm, PetscViewerAndFormat *vf)
{
  Vec X;
  Mat J, dJ, dJdense;
  PetscErrorCode (*func)(SNES, Vec, Mat, Mat, void *);
  PetscInt     n;
  PetscBLASInt nb = 0, lwork;
  PetscReal   *eigr, *eigi;
  PetscScalar *work;
  PetscScalar *a;

  PetscFunctionBegin;
  if (it == 0) PetscFunctionReturn(PETSC_SUCCESS);
  /* create the difference between the current update and the current Jacobian */
  PetscCall(SNESGetSolution(snes, &X));
  PetscCall(SNESGetJacobian(snes, NULL, &J, &func, NULL));
  PetscCall(MatDuplicate(J, MAT_COPY_VALUES, &dJ));
  PetscCall(SNESComputeJacobian(snes, X, dJ, dJ));
  PetscCall(MatAXPY(dJ, -1.0, J, SAME_NONZERO_PATTERN));

  /* compute the spectrum directly */
  PetscCall(MatConvert(dJ, MATSEQDENSE, MAT_INITIAL_MATRIX, &dJdense));
  PetscCall(MatGetSize(dJ, &n, NULL));
  PetscCall(PetscBLASIntCast(n, &nb));
  lwork = 3 * nb;
  PetscCall(PetscMalloc1(n, &eigr));
  PetscCall(PetscMalloc1(n, &eigi));
  PetscCall(PetscMalloc1(lwork, &work));
  PetscCall(MatDenseGetArray(dJdense, &a));
#if !defined(PETSC_USE_COMPLEX)
  {
    PetscBLASInt lierr;
    PetscInt     i;
    PetscCall(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
    PetscCallBLAS("LAPACKgeev", LAPACKgeev_("N", "N", &nb, a, &nb, eigr, eigi, NULL, &nb, NULL, &nb, work, &lwork, &lierr));
    PetscCheck(!lierr, PETSC_COMM_SELF, PETSC_ERR_LIB, "geev() error %" PetscBLASInt_FMT, lierr);
    PetscCall(PetscFPTrapPop());
    PetscCall(PetscPrintf(PetscObjectComm((PetscObject)snes), "Eigenvalues of J_%" PetscInt_FMT " - J_%" PetscInt_FMT ":\n", it, it - 1));
    for (i = 0; i < n; i++) PetscCall(PetscPrintf(PetscObjectComm((PetscObject)snes), "%5" PetscInt_FMT ": %20.5g + %20.5gi\n", i, (double)eigr[i], (double)eigi[i]));
  }
  PetscCall(MatDenseRestoreArray(dJdense, &a));
  PetscCall(MatDestroy(&dJ));
  PetscCall(MatDestroy(&dJdense));
  PetscCall(PetscFree(eigr));
  PetscCall(PetscFree(eigi));
  PetscCall(PetscFree(work));
  PetscFunctionReturn(PETSC_SUCCESS);
#else
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Not coded for complex");
#endif
}

PETSC_INTERN PetscErrorCode SNESMonitorRange_Private(SNES, PetscInt, PetscReal *);

PetscErrorCode SNESMonitorRange_Private(SNES snes, PetscInt it, PetscReal *per)
{
  Vec          resid;
  PetscReal    rmax, pwork;
  PetscInt     i, n, N;
  PetscScalar *r;

  PetscFunctionBegin;
  PetscCall(SNESGetFunction(snes, &resid, NULL, NULL));
  PetscCall(VecNorm(resid, NORM_INFINITY, &rmax));
  PetscCall(VecGetLocalSize(resid, &n));
  PetscCall(VecGetSize(resid, &N));
  PetscCall(VecGetArray(resid, &r));
  pwork = 0.0;
  for (i = 0; i < n; i++) pwork += (PetscAbsScalar(r[i]) > .20 * rmax);
  PetscCall(MPIU_Allreduce(&pwork, per, 1, MPIU_REAL, MPIU_SUM, PetscObjectComm((PetscObject)snes)));
  PetscCall(VecRestoreArray(resid, &r));
  *per = *per / N;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESMonitorRange - Prints the percentage of residual elements that are more then 10 percent of the maximum entry in the residual

   Collective

   Input Parameters:
+  snes   - iterative context
.  it    - iteration number
.  rnorm - 2-norm (preconditioned) residual value (may be estimated).
-  dummy - unused monitor context

   Options Database Key:
.  -snes_monitor_range - Activates `SNESMonitorRange()`

   Level: intermediate

   Note:
   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNES` solve.

.seealso: [](chapter_snes), `SNESMonitorSet()`, `SNESMonitorDefault()`, `SNESMonitorLGCreate()`, `SNESMonitorScaling()`, `PetscViewerFormat`, `PetscViewerAndFormat`
@*/
PetscErrorCode SNESMonitorRange(SNES snes, PetscInt it, PetscReal rnorm, PetscViewerAndFormat *vf)
{
  PetscReal   perc, rel;
  PetscViewer viewer = vf->viewer;
  /* should be in a MonitorRangeContext */
  static PetscReal prev;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  if (!it) prev = rnorm;
  PetscCall(SNESMonitorRange_Private(snes, it, &perc));

  rel  = (prev - rnorm) / prev;
  prev = rnorm;
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  PetscCall(PetscViewerASCIIAddTab(viewer, ((PetscObject)snes)->tablevel));
  PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES preconditioned resid norm %14.12e Percent values above 20 percent of maximum %5.2g relative decrease %5.2e ratio %5.2e \n", it, (double)rnorm, (double)(100.0 * perc), (double)rel, (double)(rel / perc)));
  PetscCall(PetscViewerASCIISubtractTab(viewer, ((PetscObject)snes)->tablevel));
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESMonitorRatio - Monitors progress of the `SNES` solvers by printing the ratio
   of residual norm at each iteration to the previous.

   Collective

   Input Parameters:
+  snes - the `SNES` context
.  its - iteration number
.  fgnorm - 2-norm of residual (or gradient)
-  dummy -  context of monitor

   Options Database Key:
.  -snes_monitor_ratio - activate this monitor

   Level: intermediate

   Notes:
   This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `SNES` solve.

   Be sure to call `SNESMonitorRationSetUp()` before using this monitor.

.seealso: [](chapter_snes), `SNESMonitorRationSetUp()`, `SNESMonitorSet()`, `SNESMonitorSolution()`, `SNESMonitorDefault()`, `PetscViewerFormat`, `PetscViewerAndFormat`
@*/
PetscErrorCode SNESMonitorRatio(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  PetscInt    len;
  PetscReal  *history;
  PetscViewer viewer = vf->viewer;

  PetscFunctionBegin;
  PetscCall(SNESGetConvergenceHistory(snes, &history, NULL, &len));
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  PetscCall(PetscViewerASCIIAddTab(viewer, ((PetscObject)snes)->tablevel));
  if (!its || !history || its > len) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm %14.12e \n", its, (double)fgnorm));
  } else {
    PetscReal ratio = fgnorm / history[its - 1];
    PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm %14.12e %14.12e \n", its, (double)fgnorm, (double)ratio));
  }
  PetscCall(PetscViewerASCIISubtractTab(viewer, ((PetscObject)snes)->tablevel));
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESMonitorRatioSetUp - Insures the `SNES` object is saving its history since this monitor needs access to it

   Collective

   Input Parameters:
+   snes - the `SNES` context
-   viewer - the `PetscViewer` object (ignored)

   Level: intermediate

.seealso: [](chapter_snes), `SNESMonitorSet()`, `SNESMonitorSolution()`, `SNESMonitorDefault()`, `SNESMonitorRatio()`, `PetscViewerFormat`, `PetscViewerAndFormat`
@*/
PetscErrorCode SNESMonitorRatioSetUp(SNES snes, PetscViewerAndFormat *vf)
{
  PetscReal *history;

  PetscFunctionBegin;
  PetscCall(SNESGetConvergenceHistory(snes, &history, NULL, NULL));
  if (!history) PetscCall(SNESSetConvergenceHistory(snes, NULL, NULL, 100, PETSC_TRUE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
     Default (short) SNES Monitor, same as SNESMonitorDefault() except
  it prints fewer digits of the residual as the residual gets smaller.
  This is because the later digits are meaningless and are often
  different on different machines; by using this routine different
  machines will usually generate the same output.

  Deprecated: Intentionally has no manual page
*/
PetscErrorCode SNESMonitorDefaultShort(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  PetscViewer viewer = vf->viewer;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  PetscCall(PetscViewerASCIIAddTab(viewer, ((PetscObject)snes)->tablevel));
  if (fgnorm > 1.e-9) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm %g \n", its, (double)fgnorm));
  } else if (fgnorm > 1.e-11) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm %5.3e \n", its, (double)fgnorm));
  } else {
    PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm < 1.e-11\n", its));
  }
  PetscCall(PetscViewerASCIISubtractTab(viewer, ((PetscObject)snes)->tablevel));
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  SNESMonitorDefaultField - Monitors progress of the `SNES` solvers, separated into fields.

  Collective

  Input Parameters:
+ snes   - the `SNES` context
. its    - iteration number
. fgnorm - 2-norm of residual
- ctx    - the PetscViewer

   Options Database Key:
.  -snes_monitor_field - activate this monitor

  Level: intermediate

  Notes:
  This routine uses the `DM` attached to the residual vector to define the fields.

  This is not called directly by users, rather one calls `SNESMonitorSet()`, with this function as an argument, to cause the monitor
  to be used during the `SNES` solve.

.seealso: [](chapter_snes), `SNESMonitorSet()`, `SNESMonitorSolution()`, `SNESMonitorDefault()`, `PetscViewerFormat`, `PetscViewerAndFormat`
@*/
PetscErrorCode SNESMonitorDefaultField(SNES snes, PetscInt its, PetscReal fgnorm, PetscViewerAndFormat *vf)
{
  PetscViewer viewer = vf->viewer;
  Vec         r;
  DM          dm;
  PetscReal   res[256];
  PetscInt    tablevel;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 4);
  PetscCall(SNESGetFunction(snes, &r, NULL, NULL));
  PetscCall(VecGetDM(r, &dm));
  if (!dm) PetscCall(SNESMonitorDefault(snes, its, fgnorm, vf));
  else {
    PetscSection s, gs;
    PetscInt     Nf, f;

    PetscCall(DMGetLocalSection(dm, &s));
    PetscCall(DMGetGlobalSection(dm, &gs));
    if (!s || !gs) PetscCall(SNESMonitorDefault(snes, its, fgnorm, vf));
    PetscCall(PetscSectionGetNumFields(s, &Nf));
    PetscCheck(Nf <= 256, PetscObjectComm((PetscObject)snes), PETSC_ERR_SUP, "Do not support %" PetscInt_FMT " fields > 256", Nf);
    PetscCall(PetscSectionVecNorm(s, gs, r, NORM_2, res));
    PetscCall(PetscObjectGetTabLevel((PetscObject)snes, &tablevel));
    PetscCall(PetscViewerPushFormat(viewer, vf->format));
    PetscCall(PetscViewerASCIIAddTab(viewer, tablevel));
    PetscCall(PetscViewerASCIIPrintf(viewer, "%3" PetscInt_FMT " SNES Function norm %14.12e [", its, (double)fgnorm));
    for (f = 0; f < Nf; ++f) {
      if (f) PetscCall(PetscViewerASCIIPrintf(viewer, ", "));
      PetscCall(PetscViewerASCIIPrintf(viewer, "%14.12e", (double)res[f]));
    }
    PetscCall(PetscViewerASCIIPrintf(viewer, "] \n"));
    PetscCall(PetscViewerASCIISubtractTab(viewer, tablevel));
    PetscCall(PetscViewerPopFormat(viewer));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESConvergedDefault - Default onvergence test of the solvers for
   systems of nonlinear equations.

   Collective

   Input Parameters:
+  snes - the `SNES` context
.  it - the iteration (0 indicates before any Newton steps)
.  xnorm - 2-norm of current iterate
.  snorm - 2-norm of current step
.  fnorm - 2-norm of function at current iterate
-  dummy - unused context

   Output Parameter:
.   reason  - one of
.vb
   SNES_CONVERGED_FNORM_ABS       - (fnorm < abstol),
   SNES_CONVERGED_SNORM_RELATIVE  - (snorm < stol*xnorm),
   SNES_CONVERGED_FNORM_RELATIVE  - (fnorm < rtol*fnorm0),
   SNES_DIVERGED_FUNCTION_COUNT   - (nfct > maxf),
   SNES_DIVERGED_FNORM_NAN        - (fnorm == NaN),
   SNES_CONVERGED_ITERATING       - (otherwise),
   SNES_DIVERGED_DTOL             - (fnorm > divtol*snes->fnorm0)
.ve

   where
+    maxf - maximum number of function evaluations,  set with `SNESSetTolerances()`
.    nfct - number of function evaluations,
.    abstol - absolute function norm tolerance, set with `SNESSetTolerances()`
.    rtol - relative function norm tolerance, set with `SNESSetTolerances()`
.    divtol - divergence tolerance, set with `SNESSetDivergenceTolerance()`
-    fnorm0 - 2-norm of the function at the initial solution (initial guess; zeroth iteration)

  Options Database Keys:
+  -snes_convergence_test default - see `SNESSetFromOptions()`
.  -snes_stol - convergence tolerance in terms of the norm  of the change in the solution between steps
.  -snes_atol <abstol> - absolute tolerance of residual norm
.  -snes_rtol <rtol> - relative decrease in tolerance norm from the initial 2-norm of the solution
.  -snes_divergence_tolerance <divtol> - if the residual goes above divtol*rnorm0, exit with divergence
.  -snes_max_funcs <max_funcs> - maximum number of function evaluations
.  -snes_max_fail <max_fail> - maximum number of line search failures allowed before stopping, default is none
-  -snes_max_linear_solve_fail - number of linear solver failures before `SNESSolve()` stops

   Level: intermediate

.seealso: [](chapter_snes), `SNES`, `SNESSolve()`, `SNESSetConvergenceTest()`, `SNESConvergedSkip()`, `SNESSetTolerances()`, `SNESSetDivergenceTolerance()`,
          `SNESConvergedReason`
@*/
PetscErrorCode SNESConvergedDefault(SNES snes, PetscInt it, PetscReal xnorm, PetscReal snorm, PetscReal fnorm, SNESConvergedReason *reason, void *dummy)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);
  PetscValidPointer(reason, 6);

  *reason = SNES_CONVERGED_ITERATING;
  if (!it) {
    /* set parameter for default relative tolerance convergence test */
    snes->ttol   = fnorm * snes->rtol;
    snes->rnorm0 = fnorm;
  }
  if (PetscIsInfOrNanReal(fnorm)) {
    PetscCall(PetscInfo(snes, "Failed to converged, function norm is NaN\n"));
    *reason = SNES_DIVERGED_FNORM_NAN;
  } else if (fnorm < snes->abstol && (it || !snes->forceiteration)) {
    PetscCall(PetscInfo(snes, "Converged due to function norm %14.12e < %14.12e\n", (double)fnorm, (double)snes->abstol));
    *reason = SNES_CONVERGED_FNORM_ABS;
  } else if (snes->nfuncs >= snes->max_funcs && snes->max_funcs >= 0) {
    PetscCall(PetscInfo(snes, "Exceeded maximum number of function evaluations: %" PetscInt_FMT " > %" PetscInt_FMT "\n", snes->nfuncs, snes->max_funcs));
    *reason = SNES_DIVERGED_FUNCTION_COUNT;
  }

  if (it && !*reason) {
    if (fnorm <= snes->ttol) {
      PetscCall(PetscInfo(snes, "Converged due to function norm %14.12e < %14.12e (relative tolerance)\n", (double)fnorm, (double)snes->ttol));
      *reason = SNES_CONVERGED_FNORM_RELATIVE;
    } else if (snorm < snes->stol * xnorm) {
      PetscCall(PetscInfo(snes, "Converged due to small update length: %14.12e < %14.12e * %14.12e\n", (double)snorm, (double)snes->stol, (double)xnorm));
      *reason = SNES_CONVERGED_SNORM_RELATIVE;
    } else if (snes->divtol > 0 && (fnorm > snes->divtol * snes->rnorm0)) {
      PetscCall(PetscInfo(snes, "Diverged due to increase in function norm: %14.12e > %14.12e * %14.12e\n", (double)fnorm, (double)snes->divtol, (double)snes->rnorm0));
      *reason = SNES_DIVERGED_DTOL;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   SNESConvergedSkip - Convergence test for `SNES` that NEVER returns as
   converged, UNLESS the maximum number of iteration have been reached.

   Logically Collective

   Input Parameters:
+  snes - the `SNES` context
.  it - the iteration (0 indicates before any Newton steps)
.  xnorm - 2-norm of current iterate
.  snorm - 2-norm of current step
.  fnorm - 2-norm of function at current iterate
-  dummy - unused context

   Output Parameter:
.   reason  - `SNES_CONVERGED_ITERATING`, `SNES_CONVERGED_ITS`, or `SNES_DIVERGED_FNORM_NAN`

   Options Database Key:
.  -snes_convergence_test skip - see `SNESSetFromOptions()`

   Level: advanced

.seealso: [](chapter_snes), `SNES`, `SNESSolve()`, `SNESConvergedDefault()`, `SNESSetConvergenceTest()`, `SNESConvergedReason`
@*/
PetscErrorCode SNESConvergedSkip(SNES snes, PetscInt it, PetscReal xnorm, PetscReal snorm, PetscReal fnorm, SNESConvergedReason *reason, void *dummy)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 1);
  PetscValidPointer(reason, 6);

  *reason = SNES_CONVERGED_ITERATING;

  if (fnorm != fnorm) {
    PetscCall(PetscInfo(snes, "Failed to converged, function norm is NaN\n"));
    *reason = SNES_DIVERGED_FNORM_NAN;
  } else if (it == snes->max_its) {
    *reason = SNES_CONVERGED_ITS;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  SNESSetWorkVecs - Gets a number of work vectors to be used internally by `SNES` solvers

  Input Parameters:
+ snes  - the `SNES` context
- nw - number of work vectors to allocate

  Level: developer

.seealso: [](chapter_snes), `SNES`
@*/
PetscErrorCode SNESSetWorkVecs(SNES snes, PetscInt nw)
{
  DM  dm;
  Vec v;

  PetscFunctionBegin;
  if (snes->work) PetscCall(VecDestroyVecs(snes->nwork, &snes->work));
  snes->nwork = nw;

  PetscCall(SNESGetDM(snes, &dm));
  PetscCall(DMGetGlobalVector(dm, &v));
  PetscCall(VecDuplicateVecs(v, snes->nwork, &snes->work));
  PetscCall(DMRestoreGlobalVector(dm, &v));
  PetscFunctionReturn(PETSC_SUCCESS);
}

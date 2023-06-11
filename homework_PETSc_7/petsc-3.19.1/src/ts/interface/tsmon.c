#include <petsc/private/tsimpl.h> /*I "petscts.h"  I*/
#include <petscdm.h>
#include <petscds.h>
#include <petscdmswarm.h>
#include <petscdraw.h>

/*@C
   TSMonitor - Runs all user-provided monitor routines set using `TSMonitorSet()`

   Collective

   Input Parameters:
+  ts - time stepping context obtained from `TSCreate()`
.  step - step number that has just completed
.  ptime - model time of the state
-  u - state at the current model time

   Level: developer

   Notes:
   `TSMonitor()` is typically used automatically within the time stepping implementations.
   Users would almost never call this routine directly.

   A step of -1 indicates that the monitor is being called on a solution obtained by interpolating from computed solutions

.seealso: `TS`, `TSMonitorSet()`, `TSMonitorSetFromOptions()`
@*/
PetscErrorCode TSMonitor(TS ts, PetscInt step, PetscReal ptime, Vec u)
{
  DM       dm;
  PetscInt i, n = ts->numbermonitors;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID, 1);
  PetscValidHeaderSpecific(u, VEC_CLASSID, 4);

  PetscCall(TSGetDM(ts, &dm));
  PetscCall(DMSetOutputSequenceNumber(dm, step, ptime));

  PetscCall(VecLockReadPush(u));
  for (i = 0; i < n; i++) PetscCall((*ts->monitor[i])(ts, step, ptime, u, ts->monitorcontext[i]));
  PetscCall(VecLockReadPop(u));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorSetFromOptions - Sets a monitor function and viewer appropriate for the type indicated by the user

   Collective

   Input Parameters:
+  ts - `TS` object you wish to monitor
.  name - the monitor type one is seeking
.  help - message indicating what monitoring is done
.  manual - manual page for the monitor
.  monitor - the monitor function
-  monitorsetup - a function that is called once ONLY if the user selected this monitor that may set additional features of the `TS` or `PetscViewer` objects

   Level: developer

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `PetscOptionsGetViewer()`, `PetscOptionsGetReal()`, `PetscOptionsHasName()`, `PetscOptionsGetString()`,
          `PetscOptionsGetIntArray()`, `PetscOptionsGetRealArray()`, `PetscOptionsBool()`
          `PetscOptionsInt()`, `PetscOptionsString()`, `PetscOptionsReal()`, `PetscOptionsBool()`,
          `PetscOptionsName()`, `PetscOptionsBegin()`, `PetscOptionsEnd()`, `PetscOptionsHeadBegin()`,
          `PetscOptionsStringArray()`, `PetscOptionsRealArray()`, `PetscOptionsScalar()`,
          `PetscOptionsBoolGroupBegin()`, `PetscOptionsBoolGroup()`, `PetscOptionsBoolGroupEnd()`,
          `PetscOptionsFList()`, `PetscOptionsEList()`
@*/
PetscErrorCode TSMonitorSetFromOptions(TS ts, const char name[], const char help[], const char manual[], PetscErrorCode (*monitor)(TS, PetscInt, PetscReal, Vec, PetscViewerAndFormat *), PetscErrorCode (*monitorsetup)(TS, PetscViewerAndFormat *))
{
  PetscViewer       viewer;
  PetscViewerFormat format;
  PetscBool         flg;

  PetscFunctionBegin;
  PetscCall(PetscOptionsGetViewer(PetscObjectComm((PetscObject)ts), ((PetscObject)ts)->options, ((PetscObject)ts)->prefix, name, &viewer, &format, &flg));
  if (flg) {
    PetscViewerAndFormat *vf;
    char                  interval_key[1024];
    PetscCall(PetscViewerAndFormatCreate(viewer, format, &vf));
    PetscCall(PetscSNPrintf(interval_key, sizeof interval_key, "%s_interval", name));
    PetscCall(PetscOptionsGetInt(((PetscObject)ts)->options, ((PetscObject)ts)->prefix, interval_key, &vf->view_interval, NULL));
    PetscCall(PetscObjectDereference((PetscObject)viewer));
    if (monitorsetup) PetscCall((*monitorsetup)(ts, vf));
    PetscCall(TSMonitorSet(ts, (PetscErrorCode(*)(TS, PetscInt, PetscReal, Vec, void *))monitor, vf, (PetscErrorCode(*)(void **))PetscViewerAndFormatDestroy));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorSet - Sets an ADDITIONAL function that is to be used at every
   timestep to display the iteration's  progress.

   Logically Collective

   Input Parameters:
+  ts - the `TS` context obtained from `TSCreate()`
.  monitor - monitoring routine
.  mctx - [optional] user-defined context for private data for the monitor routine (use `NULL` if no context is desired)
-  monitordestroy - [optional] routine that frees monitor context (may be `NULL`)

   Calling sequence of `monitor`:
$    PetscErrorCode monitor(TS ts, PetscInt steps, PetscReal time, Vec u, void *mctx)
+    ts - the `TS` context
.    steps - iteration number (after the final time step the monitor routine may be called with a step of -1, this indicates the solution has been interpolated to this time)
.    time - current time
.    u - current iterate
-    mctx - [optional] monitoring context

   Level: intermediate

   Note:
   This routine adds an additional monitor to the list of monitors that already has been loaded.

   Fortran Note:
   Only a single monitor function can be set for each `TS` object

.seealso: [](chapter_ts), `TSMonitorDefault()`, `TSMonitorCancel()`, `TSDMSwarmMonitorMoments()`, `TSMonitorExtreme()`, `TSMonitorDrawSolution()`,
          `TSMonitorDrawSolutionPhase()`, `TSMonitorDrawSolutionFunction()`, `TSMonitorDrawError()`, `TSMonitorSolution()`, `TSMonitorSolutionVTK()`,
          `TSMonitorLGSolution()`, `TSMonitorLGError()`, `TSMonitorSPSwarmSolution()`, `TSMonitorError()`, `TSMonitorEnvelope()`, `TSDMSwarmMonitorMoments()`
@*/
PetscErrorCode TSMonitorSet(TS ts, PetscErrorCode (*monitor)(TS, PetscInt, PetscReal, Vec, void *), void *mctx, PetscErrorCode (*mdestroy)(void **))
{
  PetscInt  i;
  PetscBool identical;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID, 1);
  for (i = 0; i < ts->numbermonitors; i++) {
    PetscCall(PetscMonitorCompare((PetscErrorCode(*)(void))monitor, mctx, mdestroy, (PetscErrorCode(*)(void))ts->monitor[i], ts->monitorcontext[i], ts->monitordestroy[i], &identical));
    if (identical) PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCheck(ts->numbermonitors < MAXTSMONITORS, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Too many monitors set");
  ts->monitor[ts->numbermonitors]          = monitor;
  ts->monitordestroy[ts->numbermonitors]   = mdestroy;
  ts->monitorcontext[ts->numbermonitors++] = (void *)mctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorCancel - Clears all the monitors that have been set on a time-step object.

   Logically Collective

   Input Parameter:
.  ts - the `TS` context obtained from `TSCreate()`

   Level: intermediate

   Note:
   There is no way to remove a single, specific monitor.

.seealso: [](chapter_ts), `TS`, `TSMonitorDefault()`, `TSMonitorSet()`
@*/
PetscErrorCode TSMonitorCancel(TS ts)
{
  PetscInt i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID, 1);
  for (i = 0; i < ts->numbermonitors; i++) {
    if (ts->monitordestroy[i]) PetscCall((*ts->monitordestroy[i])(&ts->monitorcontext[i]));
  }
  ts->numbermonitors = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorDefault - The default monitor, prints the timestep and time for each step

   Options Database Key:
.  -ts_monitor - monitors the time integration

   Level: intermediate

   Notes:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TSMonitorSet()`, `TSDMSwarmMonitorMoments()`, `TSMonitorExtreme()`, `TSMonitorDrawSolution()`,
          `TSMonitorDrawSolutionPhase()`, `TSMonitorDrawSolutionFunction()`, `TSMonitorDrawError()`, `TSMonitorSolution()`, `TSMonitorSolutionVTK()`,
          `TSMonitorLGSolution()`, `TSMonitorLGError()`, `TSMonitorSPSwarmSolution()`, `TSMonitorError()`, `TSMonitorEnvelope()`, `TSDMSwarmMonitorMoments()`
@*/
PetscErrorCode TSMonitorDefault(TS ts, PetscInt step, PetscReal ptime, Vec v, PetscViewerAndFormat *vf)
{
  PetscViewer viewer = vf->viewer;
  PetscBool   iascii, ibinary;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 5);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERBINARY, &ibinary));
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  if (iascii) {
    PetscCall(PetscViewerASCIIAddTab(viewer, ((PetscObject)ts)->tablevel));
    if (step == -1) { /* this indicates it is an interpolated solution */
      PetscCall(PetscViewerASCIIPrintf(viewer, "Interpolated solution at time %g between steps %" PetscInt_FMT " and %" PetscInt_FMT "\n", (double)ptime, ts->steps - 1, ts->steps));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "%" PetscInt_FMT " TS dt %g time %g%s", step, (double)ts->time_step, (double)ptime, ts->steprollback ? " (r)\n" : "\n"));
    }
    PetscCall(PetscViewerASCIISubtractTab(viewer, ((PetscObject)ts)->tablevel));
  } else if (ibinary) {
    PetscMPIInt rank;
    PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)viewer), &rank));
    if (rank == 0) {
      PetscBool skipHeader;
      PetscInt  classid = REAL_FILE_CLASSID;

      PetscCall(PetscViewerBinaryGetSkipHeader(viewer, &skipHeader));
      if (!skipHeader) PetscCall(PetscViewerBinaryWrite(viewer, &classid, 1, PETSC_INT));
      PetscCall(PetscRealView(1, &ptime, viewer));
    } else {
      PetscCall(PetscRealView(0, &ptime, viewer));
    }
  }
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorExtreme - Prints the extreme values of the solution at each timestep

   Level: intermediate

   Note:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`
@*/
PetscErrorCode TSMonitorExtreme(TS ts, PetscInt step, PetscReal ptime, Vec v, PetscViewerAndFormat *vf)
{
  PetscViewer viewer = vf->viewer;
  PetscBool   iascii;
  PetscReal   max, min;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 5);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscViewerPushFormat(viewer, vf->format));
  if (iascii) {
    PetscCall(VecMax(v, NULL, &max));
    PetscCall(VecMin(v, NULL, &min));
    PetscCall(PetscViewerASCIIAddTab(viewer, ((PetscObject)ts)->tablevel));
    PetscCall(PetscViewerASCIIPrintf(viewer, "%" PetscInt_FMT " TS dt %g time %g%s max %g min %g\n", step, (double)ts->time_step, (double)ptime, ts->steprollback ? " (r)" : "", (double)max, (double)min));
    PetscCall(PetscViewerASCIISubtractTab(viewer, ((PetscObject)ts)->tablevel));
  }
  PetscCall(PetscViewerPopFormat(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGCtxCreate - Creates a `TSMonitorLGCtx` context for use with
   `TS` to monitor the solution process graphically in various ways

   Collective

   Input Parameters:
+  host - the X display to open, or `NULL` for the local machine
.  label - the title to put in the title bar
.  x, y - the screen coordinates of the upper left coordinate of the window
.  m, n - the screen width and height in pixels
-  howoften - if positive then determines the frequency of the plotting, if -1 then only at the final time

   Output Parameter:
.  ctx - the context

   Options Database Keys:
+  -ts_monitor_lg_timestep - automatically sets line graph monitor
+  -ts_monitor_lg_timestep_log - automatically sets line graph monitor
.  -ts_monitor_lg_solution - monitor the solution (or certain values of the solution by calling `TSMonitorLGSetDisplayVariables()` or `TSMonitorLGCtxSetDisplayVariables()`)
.  -ts_monitor_lg_error -  monitor the error
.  -ts_monitor_lg_ksp_iterations - monitor the number of `KSP` iterations needed for each timestep
.  -ts_monitor_lg_snes_iterations - monitor the number of `SNES` iterations needed for each timestep
-  -lg_use_markers <true,false> - mark the data points (at each time step) on the plot; default is true

   Level: intermediate

   Notes:
   Pass the context and `TSMonitorLGCtxDestroy()` to `TSMonitorSet()` to have the context destroyed when no longer needed.

   One can provide a function that transforms the solution before plotting it with `TSMonitorLGCtxSetTransform()` or `TSMonitorLGSetTransform()`

   Many of the functions that control the monitoring have two forms: TSMonitorLGSet/GetXXXX() and TSMonitorLGCtxSet/GetXXXX() the first take a `TS` object as the
   first argument (if that `TS` object does not have a `TSMonitorLGCtx` associated with it the function call is ignored) and the second takes a `TSMonitorLGCtx` object
   as the first argument.

   One can control the names displayed for each solution or error variable with `TSMonitorLGCtxSetVariableNames()` or `TSMonitorLGSetVariableNames()`

.seealso: [](chapter_ts), `TSMonitorLGTimeStep()`, `TSMonitorSet()`, `TSMonitorLGSolution()`, `TSMonitorLGError()`, `TSMonitorDefault()`, `VecView()`,
          `TSMonitorLGCtxCreate()`, `TSMonitorLGCtxSetVariableNames()`, `TSMonitorLGCtxGetVariableNames()`,
          `TSMonitorLGSetVariableNames()`, `TSMonitorLGGetVariableNames()`, `TSMonitorLGSetDisplayVariables()`, `TSMonitorLGCtxSetDisplayVariables()`,
          `TSMonitorLGCtxSetTransform()`, `TSMonitorLGSetTransform()`, `TSMonitorLGError()`, `TSMonitorLGSNESIterations()`, `TSMonitorLGKSPIterations()`,
          `TSMonitorEnvelopeCtxCreate()`, `TSMonitorEnvelopeGetBounds()`, `TSMonitorEnvelopeCtxDestroy()`, `TSMonitorEnvelop()`
@*/
PetscErrorCode TSMonitorLGCtxCreate(MPI_Comm comm, const char host[], const char label[], int x, int y, int m, int n, PetscInt howoften, TSMonitorLGCtx *ctx)
{
  PetscDraw draw;

  PetscFunctionBegin;
  PetscCall(PetscNew(ctx));
  PetscCall(PetscDrawCreate(comm, host, label, x, y, m, n, &draw));
  PetscCall(PetscDrawSetFromOptions(draw));
  PetscCall(PetscDrawLGCreate(draw, 1, &(*ctx)->lg));
  PetscCall(PetscDrawLGSetFromOptions((*ctx)->lg));
  PetscCall(PetscDrawDestroy(&draw));
  (*ctx)->howoften = howoften;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSMonitorLGTimeStep(TS ts, PetscInt step, PetscReal ptime, Vec v, void *monctx)
{
  TSMonitorLGCtx ctx = (TSMonitorLGCtx)monctx;
  PetscReal      x   = ptime, y;

  PetscFunctionBegin;
  if (step < 0) PetscFunctionReturn(PETSC_SUCCESS); /* -1 indicates an interpolated solution */
  if (!step) {
    PetscDrawAxis axis;
    const char   *ylabel = ctx->semilogy ? "Log Time Step" : "Time Step";
    PetscCall(PetscDrawLGGetAxis(ctx->lg, &axis));
    PetscCall(PetscDrawAxisSetLabels(axis, "Timestep as function of time", "Time", ylabel));
    PetscCall(PetscDrawLGReset(ctx->lg));
  }
  PetscCall(TSGetTimeStep(ts, &y));
  if (ctx->semilogy) y = PetscLog10Real(y);
  PetscCall(PetscDrawLGAddPoint(ctx->lg, &x, &y));
  if (((ctx->howoften > 0) && (!(step % ctx->howoften))) || ((ctx->howoften == -1) && ts->reason)) {
    PetscCall(PetscDrawLGDraw(ctx->lg));
    PetscCall(PetscDrawLGSave(ctx->lg));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGCtxDestroy - Destroys a line graph context that was created with `TSMonitorLGCtxCreate()`.

   Collective

   Input Parameter:
.  ctx - the monitor context

   Level: intermediate

   Note:
   Pass to `TSMonitorSet()` along with the context and `TSMonitorLGTimeStep()`

.seealso: [](chapter_ts), `TS`, `TSMonitorLGCtxCreate()`, `TSMonitorSet()`, `TSMonitorLGTimeStep();`
@*/
PetscErrorCode TSMonitorLGCtxDestroy(TSMonitorLGCtx *ctx)
{
  PetscFunctionBegin;
  if ((*ctx)->transformdestroy) PetscCall(((*ctx)->transformdestroy)((*ctx)->transformctx));
  PetscCall(PetscDrawLGDestroy(&(*ctx)->lg));
  PetscCall(PetscStrArrayDestroy(&(*ctx)->names));
  PetscCall(PetscStrArrayDestroy(&(*ctx)->displaynames));
  PetscCall(PetscFree((*ctx)->displayvariables));
  PetscCall(PetscFree((*ctx)->displayvalues));
  PetscCall(PetscFree(*ctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Creates a TSMonitorSPCtx for use with DMSwarm particle visualizations */
PetscErrorCode TSMonitorSPCtxCreate(MPI_Comm comm, const char host[], const char label[], int x, int y, int m, int n, PetscInt howoften, PetscInt retain, PetscBool phase, PetscBool multispecies, TSMonitorSPCtx *ctx)
{
  PetscDraw draw;

  PetscFunctionBegin;
  PetscCall(PetscNew(ctx));
  PetscCall(PetscDrawCreate(comm, host, label, x, y, m, n, &draw));
  PetscCall(PetscDrawSetFromOptions(draw));
  PetscCall(PetscDrawSPCreate(draw, 1, &(*ctx)->sp));
  PetscCall(PetscDrawDestroy(&draw));
  (*ctx)->howoften     = howoften;
  (*ctx)->retain       = retain;
  (*ctx)->phase        = phase;
  (*ctx)->multispecies = multispecies;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Destroys a TSMonitorSPCtx that was created with TSMonitorSPCtxCreate */
PetscErrorCode TSMonitorSPCtxDestroy(TSMonitorSPCtx *ctx)
{
  PetscFunctionBegin;

  PetscCall(PetscDrawSPDestroy(&(*ctx)->sp));
  PetscCall(PetscFree(*ctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Creates a TSMonitorHGCtx for use with DMSwarm particle visualizations */
PetscErrorCode TSMonitorHGCtxCreate(MPI_Comm comm, const char host[], const char label[], int x, int y, int m, int n, PetscInt howoften, PetscInt Ns, PetscInt Nb, PetscBool velocity, TSMonitorHGCtx *ctx)
{
  PetscDraw draw;
  PetscInt  s;

  PetscFunctionBegin;
  PetscCall(PetscNew(ctx));
  PetscCall(PetscMalloc1(Ns, &(*ctx)->hg));
  for (s = 0; s < Ns; ++s) {
    PetscCall(PetscDrawCreate(comm, host, label, x + s * m, y, m, n, &draw));
    PetscCall(PetscDrawSetFromOptions(draw));
    PetscCall(PetscDrawHGCreate(draw, Nb, &(*ctx)->hg[s]));
    PetscCall(PetscDrawHGCalcStats((*ctx)->hg[s], PETSC_TRUE));
    PetscCall(PetscDrawDestroy(&draw));
  }
  (*ctx)->howoften = howoften;
  (*ctx)->Ns       = Ns;
  (*ctx)->velocity = velocity;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Destroys a TSMonitorHGCtx that was created with TSMonitorHGCtxCreate */
PetscErrorCode TSMonitorHGCtxDestroy(TSMonitorHGCtx *ctx)
{
  PetscInt s;

  PetscFunctionBegin;
  for (s = 0; s < (*ctx)->Ns; ++s) PetscCall(PetscDrawHGDestroy(&(*ctx)->hg[s]));
  PetscCall(PetscFree((*ctx)->hg));
  PetscCall(PetscFree(*ctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorDrawSolution - Monitors progress of the `TS` solvers by calling
   `VecView()` for the solution at each timestep

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
-  dummy - either a viewer or `NULL`

   Options Database Keys:
+   -ts_monitor_draw_solution - draw the solution at each time-step
-   -ts_monitor_draw_solution_initial - show initial solution as well as current solution

   Level: intermediate

   Notes:
   The initial solution and current solution are not displayed with a common axis scaling so generally the option `-ts_monitor_draw_solution_initial`
   will look bad

   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, as well as the context created with
   `TSMonitorDrawCtxCreate()` and the function `TSMonitorDrawCtxDestroy()` to cause the monitor to be used during the `TS` integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorDrawCtxCreate()`, `TSMonitorDrawCtxDestroy()`
@*/
PetscErrorCode TSMonitorDrawSolution(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dummy)
{
  TSMonitorDrawCtx ictx = (TSMonitorDrawCtx)dummy;
  PetscDraw        draw;

  PetscFunctionBegin;
  if (!step && ictx->showinitial) {
    if (!ictx->initialsolution) PetscCall(VecDuplicate(u, &ictx->initialsolution));
    PetscCall(VecCopy(u, ictx->initialsolution));
  }
  if (!(((ictx->howoften > 0) && (!(step % ictx->howoften))) || ((ictx->howoften == -1) && ts->reason))) PetscFunctionReturn(PETSC_SUCCESS);

  if (ictx->showinitial) {
    PetscReal pause;
    PetscCall(PetscViewerDrawGetPause(ictx->viewer, &pause));
    PetscCall(PetscViewerDrawSetPause(ictx->viewer, 0.0));
    PetscCall(VecView(ictx->initialsolution, ictx->viewer));
    PetscCall(PetscViewerDrawSetPause(ictx->viewer, pause));
    PetscCall(PetscViewerDrawSetHold(ictx->viewer, PETSC_TRUE));
  }
  PetscCall(VecView(u, ictx->viewer));
  if (ictx->showtimestepandtime) {
    PetscReal xl, yl, xr, yr, h;
    char      time[32];

    PetscCall(PetscViewerDrawGetDraw(ictx->viewer, 0, &draw));
    PetscCall(PetscSNPrintf(time, 32, "Timestep %d Time %g", (int)step, (double)ptime));
    PetscCall(PetscDrawGetCoordinates(draw, &xl, &yl, &xr, &yr));
    h = yl + .95 * (yr - yl);
    PetscCall(PetscDrawStringCentered(draw, .5 * (xl + xr), h, PETSC_DRAW_BLACK, time));
    PetscCall(PetscDrawFlush(draw));
  }

  if (ictx->showinitial) PetscCall(PetscViewerDrawSetHold(ictx->viewer, PETSC_FALSE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorDrawSolutionPhase - Monitors progress of the `TS` solvers by plotting the solution as a phase diagram

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
-  dummy - either a viewer or `NULL`

   Level: intermediate

   Notes:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`
@*/
PetscErrorCode TSMonitorDrawSolutionPhase(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dummy)
{
  TSMonitorDrawCtx   ictx = (TSMonitorDrawCtx)dummy;
  PetscDraw          draw;
  PetscDrawAxis      axis;
  PetscInt           n;
  PetscMPIInt        size;
  PetscReal          U0, U1, xl, yl, xr, yr, h;
  char               time[32];
  const PetscScalar *U;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)ts), &size));
  PetscCheck(size == 1, PetscObjectComm((PetscObject)ts), PETSC_ERR_SUP, "Only allowed for sequential runs");
  PetscCall(VecGetSize(u, &n));
  PetscCheck(n == 2, PetscObjectComm((PetscObject)ts), PETSC_ERR_SUP, "Only for ODEs with two unknowns");

  PetscCall(PetscViewerDrawGetDraw(ictx->viewer, 0, &draw));
  PetscCall(PetscViewerDrawGetDrawAxis(ictx->viewer, 0, &axis));
  PetscCall(PetscDrawAxisGetLimits(axis, &xl, &xr, &yl, &yr));
  if (!step) {
    PetscCall(PetscDrawClear(draw));
    PetscCall(PetscDrawAxisDraw(axis));
  }

  PetscCall(VecGetArrayRead(u, &U));
  U0 = PetscRealPart(U[0]);
  U1 = PetscRealPart(U[1]);
  PetscCall(VecRestoreArrayRead(u, &U));
  if ((U0 < xl) || (U1 < yl) || (U0 > xr) || (U1 > yr)) PetscFunctionReturn(PETSC_SUCCESS);

  PetscDrawCollectiveBegin(draw);
  PetscCall(PetscDrawPoint(draw, U0, U1, PETSC_DRAW_BLACK));
  if (ictx->showtimestepandtime) {
    PetscCall(PetscDrawGetCoordinates(draw, &xl, &yl, &xr, &yr));
    PetscCall(PetscSNPrintf(time, 32, "Timestep %d Time %g", (int)step, (double)ptime));
    h = yl + .95 * (yr - yl);
    PetscCall(PetscDrawStringCentered(draw, .5 * (xl + xr), h, PETSC_DRAW_BLACK, time));
  }
  PetscDrawCollectiveEnd(draw);
  PetscCall(PetscDrawFlush(draw));
  PetscCall(PetscDrawPause(draw));
  PetscCall(PetscDrawSave(draw));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorDrawCtxDestroy - Destroys the monitor context for `TSMonitorDrawSolution()`

   Collective

   Input Parameter:
.    ctx - the monitor context

   Level: intermediate

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorDrawSolution()`, `TSMonitorDrawError()`, `TSMonitorDrawCtx`
@*/
PetscErrorCode TSMonitorDrawCtxDestroy(TSMonitorDrawCtx *ictx)
{
  PetscFunctionBegin;
  PetscCall(PetscViewerDestroy(&(*ictx)->viewer));
  PetscCall(VecDestroy(&(*ictx)->initialsolution));
  PetscCall(PetscFree(*ictx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorDrawCtxCreate - Creates the monitor context for `TSMonitorDrawCtx`

   Collective

   Input Parameter:
.    ts - time-step context

   Output Parameter:
.    ctx - the monitor context

   Options Database Keys:
+   -ts_monitor_draw_solution - draw the solution at each time-step
-   -ts_monitor_draw_solution_initial - show initial solution as well as current solution

   Level: intermediate

   Note:
   The context created by this function, `PetscMonitorDrawSolution()`, and `TSMonitorDrawCtxDestroy()` should be passed together to `TSMonitorSet()`.

.seealso: [](chapter_ts), `TS`, `TSMonitorDrawCtxDestroy()`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorDrawCtx`, `PetscMonitorDrawSolution()`
@*/
PetscErrorCode TSMonitorDrawCtxCreate(MPI_Comm comm, const char host[], const char label[], int x, int y, int m, int n, PetscInt howoften, TSMonitorDrawCtx *ctx)
{
  PetscFunctionBegin;
  PetscCall(PetscNew(ctx));
  PetscCall(PetscViewerDrawOpen(comm, host, label, x, y, m, n, &(*ctx)->viewer));
  PetscCall(PetscViewerSetFromOptions((*ctx)->viewer));

  (*ctx)->howoften    = howoften;
  (*ctx)->showinitial = PETSC_FALSE;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-ts_monitor_draw_solution_initial", &(*ctx)->showinitial, NULL));

  (*ctx)->showtimestepandtime = PETSC_FALSE;
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-ts_monitor_draw_solution_show_time", &(*ctx)->showtimestepandtime, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorDrawSolutionFunction - Monitors progress of the `TS` solvers by calling
   `VecView()` for the solution provided by `TSSetSolutionFunction()` at each timestep

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
-  dummy - either a viewer or `NULL`

   Options Database Key:
.  -ts_monitor_draw_solution_function - Monitor error graphically, requires user to have provided `TSSetSolutionFunction()`

   Level: intermediate

   Note:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSSetSolutionFunction()`
@*/
PetscErrorCode TSMonitorDrawSolutionFunction(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dummy)
{
  TSMonitorDrawCtx ctx    = (TSMonitorDrawCtx)dummy;
  PetscViewer      viewer = ctx->viewer;
  Vec              work;

  PetscFunctionBegin;
  if (!(((ctx->howoften > 0) && (!(step % ctx->howoften))) || ((ctx->howoften == -1) && ts->reason))) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(VecDuplicate(u, &work));
  PetscCall(TSComputeSolutionFunction(ts, ptime, work));
  PetscCall(VecView(work, viewer));
  PetscCall(VecDestroy(&work));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorDrawError - Monitors progress of the `TS` solvers by calling
   `VecView()` for the error at each timestep

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
-  dummy - either a viewer or `NULL`

   Options Database Key:
.  -ts_monitor_draw_error - Monitor error graphically, requires user to have provided `TSSetSolutionFunction()`

   Level: intermediate

   Notes:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSSetSolutionFunction()`
@*/
PetscErrorCode TSMonitorDrawError(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dummy)
{
  TSMonitorDrawCtx ctx    = (TSMonitorDrawCtx)dummy;
  PetscViewer      viewer = ctx->viewer;
  Vec              work;

  PetscFunctionBegin;
  if (!(((ctx->howoften > 0) && (!(step % ctx->howoften))) || ((ctx->howoften == -1) && ts->reason))) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(VecDuplicate(u, &work));
  PetscCall(TSComputeSolutionFunction(ts, ptime, work));
  PetscCall(VecAXPY(work, -1.0, u));
  PetscCall(VecView(work, viewer));
  PetscCall(VecDestroy(&work));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorSolution - Monitors progress of the `TS` solvers by `VecView()` for the solution at each timestep. Normally the viewer is a binary file or a `PetscDraw` object

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
.  u - current state
-  vf - viewer and its format

   Level: intermediate

   Notes:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`
@*/
PetscErrorCode TSMonitorSolution(TS ts, PetscInt step, PetscReal ptime, Vec u, PetscViewerAndFormat *vf)
{
  PetscFunctionBegin;
  if (vf->view_interval > 0 && !ts->reason && step % vf->view_interval != 0) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscViewerPushFormat(vf->viewer, vf->format));
  PetscCall(VecView(u, vf->viewer));
  PetscCall(PetscViewerPopFormat(vf->viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorSolutionVTK - Monitors progress of the `TS` solvers by `VecView()` for the solution at each timestep.

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
.  u - current state
-  filenametemplate - string containing a format specifier for the integer time step (e.g. %03" PetscInt_FMT ")

   Level: intermediate

   Notes:
   The VTK format does not allow writing multiple time steps in the same file, therefore a different file will be written for each time step.
   These are named according to the file name template.

   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`
@*/
PetscErrorCode TSMonitorSolutionVTK(TS ts, PetscInt step, PetscReal ptime, Vec u, void *filenametemplate)
{
  char        filename[PETSC_MAX_PATH_LEN];
  PetscViewer viewer;

  PetscFunctionBegin;
  if (step < 0) PetscFunctionReturn(PETSC_SUCCESS); /* -1 indicates interpolated solution */
  PetscCall(PetscSNPrintf(filename, sizeof(filename), (const char *)filenametemplate, step));
  PetscCall(PetscViewerVTKOpen(PetscObjectComm((PetscObject)ts), filename, FILE_MODE_WRITE, &viewer));
  PetscCall(VecView(u, viewer));
  PetscCall(PetscViewerDestroy(&viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorSolutionVTKDestroy - Destroy filename template string created for use with `TSMonitorSolutionVTK()`

   Not Collective

   Input Parameter:
.  filenametemplate - string containing a format specifier for the integer time step (e.g. %03" PetscInt_FMT ")

   Level: intermediate

   Note:
   This function is normally passed to `TSMonitorSet()` along with `TSMonitorSolutionVTK()`.

.seealso: [](chapter_ts), `TSMonitorSet()`, `TSMonitorSolutionVTK()`
@*/
PetscErrorCode TSMonitorSolutionVTKDestroy(void *filenametemplate)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(*(char **)filenametemplate));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGSolution - Monitors progress of the `TS` solvers by plotting each component of the solution vector
       in a time based line graph

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
.  u - current solution
-  dctx - the `TSMonitorLGCtx` object that contains all the options for the monitoring, this is created with `TSMonitorLGCtxCreate()`

   Options Database Key:
.   -ts_monitor_lg_solution_variables - enable monitor of lg solution variables

   Level: intermediate

   Notes:
   Each process in a parallel run displays its component solutions in a separate window

   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGCtxCreate()`, `TSMonitorLGCtxSetVariableNames()`, `TSMonitorLGCtxGetVariableNames()`,
          `TSMonitorLGSetVariableNames()`, `TSMonitorLGGetVariableNames()`, `TSMonitorLGSetDisplayVariables()`, `TSMonitorLGCtxSetDisplayVariables()`,
          `TSMonitorLGCtxSetTransform()`, `TSMonitorLGSetTransform()`, `TSMonitorLGError()`, `TSMonitorLGSNESIterations()`, `TSMonitorLGKSPIterations()`,
          `TSMonitorEnvelopeCtxCreate()`, `TSMonitorEnvelopeGetBounds()`, `TSMonitorEnvelopeCtxDestroy()`, `TSMonitorEnvelop()`
@*/
PetscErrorCode TSMonitorLGSolution(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dctx)
{
  TSMonitorLGCtx     ctx = (TSMonitorLGCtx)dctx;
  const PetscScalar *yy;
  Vec                v;

  PetscFunctionBegin;
  if (step < 0) PetscFunctionReturn(PETSC_SUCCESS); /* -1 indicates interpolated solution */
  if (!step) {
    PetscDrawAxis axis;
    PetscInt      dim;
    PetscCall(PetscDrawLGGetAxis(ctx->lg, &axis));
    PetscCall(PetscDrawAxisSetLabels(axis, "Solution as function of time", "Time", "Solution"));
    if (!ctx->names) {
      PetscBool flg;
      /* user provides names of variables to plot but no names has been set so assume names are integer values */
      PetscCall(PetscOptionsHasName(((PetscObject)ts)->options, ((PetscObject)ts)->prefix, "-ts_monitor_lg_solution_variables", &flg));
      if (flg) {
        PetscInt i, n;
        char   **names;
        PetscCall(VecGetSize(u, &n));
        PetscCall(PetscMalloc1(n + 1, &names));
        for (i = 0; i < n; i++) {
          PetscCall(PetscMalloc1(5, &names[i]));
          PetscCall(PetscSNPrintf(names[i], 5, "%" PetscInt_FMT, i));
        }
        names[n]   = NULL;
        ctx->names = names;
      }
    }
    if (ctx->names && !ctx->displaynames) {
      char    **displaynames;
      PetscBool flg;
      PetscCall(VecGetLocalSize(u, &dim));
      PetscCall(PetscCalloc1(dim + 1, &displaynames));
      PetscCall(PetscOptionsGetStringArray(((PetscObject)ts)->options, ((PetscObject)ts)->prefix, "-ts_monitor_lg_solution_variables", displaynames, &dim, &flg));
      if (flg) PetscCall(TSMonitorLGCtxSetDisplayVariables(ctx, (const char *const *)displaynames));
      PetscCall(PetscStrArrayDestroy(&displaynames));
    }
    if (ctx->displaynames) {
      PetscCall(PetscDrawLGSetDimension(ctx->lg, ctx->ndisplayvariables));
      PetscCall(PetscDrawLGSetLegend(ctx->lg, (const char *const *)ctx->displaynames));
    } else if (ctx->names) {
      PetscCall(VecGetLocalSize(u, &dim));
      PetscCall(PetscDrawLGSetDimension(ctx->lg, dim));
      PetscCall(PetscDrawLGSetLegend(ctx->lg, (const char *const *)ctx->names));
    } else {
      PetscCall(VecGetLocalSize(u, &dim));
      PetscCall(PetscDrawLGSetDimension(ctx->lg, dim));
    }
    PetscCall(PetscDrawLGReset(ctx->lg));
  }

  if (!ctx->transform) v = u;
  else PetscCall((*ctx->transform)(ctx->transformctx, u, &v));
  PetscCall(VecGetArrayRead(v, &yy));
  if (ctx->displaynames) {
    PetscInt i;
    for (i = 0; i < ctx->ndisplayvariables; i++) ctx->displayvalues[i] = PetscRealPart(yy[ctx->displayvariables[i]]);
    PetscCall(PetscDrawLGAddCommonPoint(ctx->lg, ptime, ctx->displayvalues));
  } else {
#if defined(PETSC_USE_COMPLEX)
    PetscInt   i, n;
    PetscReal *yreal;
    PetscCall(VecGetLocalSize(v, &n));
    PetscCall(PetscMalloc1(n, &yreal));
    for (i = 0; i < n; i++) yreal[i] = PetscRealPart(yy[i]);
    PetscCall(PetscDrawLGAddCommonPoint(ctx->lg, ptime, yreal));
    PetscCall(PetscFree(yreal));
#else
    PetscCall(PetscDrawLGAddCommonPoint(ctx->lg, ptime, yy));
#endif
  }
  PetscCall(VecRestoreArrayRead(v, &yy));
  if (ctx->transform) PetscCall(VecDestroy(&v));

  if (((ctx->howoften > 0) && (!(step % ctx->howoften))) || ((ctx->howoften == -1) && ts->reason)) {
    PetscCall(PetscDrawLGDraw(ctx->lg));
    PetscCall(PetscDrawLGSave(ctx->lg));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGSetVariableNames - Sets the name of each component in the solution vector so that it may be displayed in the plot

   Collective

   Input Parameters:
+  ts - the `TS` context
-  names - the names of the components, final string must be `NULL`

   Level: intermediate

   Notes:
    If the `TS` object does not have a `TSMonitorLGCtx` associated with it then this function is ignored

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGSetDisplayVariables()`, `TSMonitorLGCtxSetVariableNames()`
@*/
PetscErrorCode TSMonitorLGSetVariableNames(TS ts, const char *const *names)
{
  PetscInt i;

  PetscFunctionBegin;
  for (i = 0; i < ts->numbermonitors; i++) {
    if (ts->monitor[i] == TSMonitorLGSolution) {
      PetscCall(TSMonitorLGCtxSetVariableNames((TSMonitorLGCtx)ts->monitorcontext[i], names));
      break;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGCtxSetVariableNames - Sets the name of each component in the solution vector so that it may be displayed in the plot

   Collective

   Input Parameters:
+  ts - the `TS` context
-  names - the names of the components, final string must be `NULL`

   Level: intermediate

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGSetDisplayVariables()`, `TSMonitorLGSetVariableNames()`
@*/
PetscErrorCode TSMonitorLGCtxSetVariableNames(TSMonitorLGCtx ctx, const char *const *names)
{
  PetscFunctionBegin;
  PetscCall(PetscStrArrayDestroy(&ctx->names));
  PetscCall(PetscStrArrayallocpy(names, &ctx->names));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGGetVariableNames - Gets the name of each component in the solution vector so that it may be displayed in the plot

   Collective

   Input Parameter:
.  ts - the `TS` context

   Output Parameter:
.  names - the names of the components, final string must be `NULL`

   Level: intermediate

   Note:
    If the `TS` object does not have a `TSMonitorLGCtx` associated with it then this function is ignored

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGSetDisplayVariables()`
@*/
PetscErrorCode TSMonitorLGGetVariableNames(TS ts, const char *const **names)
{
  PetscInt i;

  PetscFunctionBegin;
  *names = NULL;
  for (i = 0; i < ts->numbermonitors; i++) {
    if (ts->monitor[i] == TSMonitorLGSolution) {
      TSMonitorLGCtx ctx = (TSMonitorLGCtx)ts->monitorcontext[i];
      *names             = (const char *const *)ctx->names;
      break;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGCtxSetDisplayVariables - Sets the variables that are to be display in the monitor

   Collective

   Input Parameters:
+  ctx - the `TSMonitorLG` context
-  displaynames - the names of the components, final string must be `NULL`

   Level: intermediate

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGSetVariableNames()`
@*/
PetscErrorCode TSMonitorLGCtxSetDisplayVariables(TSMonitorLGCtx ctx, const char *const *displaynames)
{
  PetscInt j = 0, k;

  PetscFunctionBegin;
  if (!ctx->names) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscStrArrayDestroy(&ctx->displaynames));
  PetscCall(PetscStrArrayallocpy(displaynames, &ctx->displaynames));
  while (displaynames[j]) j++;
  ctx->ndisplayvariables = j;
  PetscCall(PetscMalloc1(ctx->ndisplayvariables, &ctx->displayvariables));
  PetscCall(PetscMalloc1(ctx->ndisplayvariables, &ctx->displayvalues));
  j = 0;
  while (displaynames[j]) {
    k = 0;
    while (ctx->names[k]) {
      PetscBool flg;
      PetscCall(PetscStrcmp(displaynames[j], ctx->names[k], &flg));
      if (flg) {
        ctx->displayvariables[j] = k;
        break;
      }
      k++;
    }
    j++;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGSetDisplayVariables - Sets the variables that are to be display in the monitor

   Collective

   Input Parameters:
+  ts - the `TS` context
-  displaynames - the names of the components, final string must be `NULL`

   Level: intermediate

   Note:
    If the `TS` object does not have a `TSMonitorLGCtx` associated with it then this function is ignored

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGSetVariableNames()`
@*/
PetscErrorCode TSMonitorLGSetDisplayVariables(TS ts, const char *const *displaynames)
{
  PetscInt i;

  PetscFunctionBegin;
  for (i = 0; i < ts->numbermonitors; i++) {
    if (ts->monitor[i] == TSMonitorLGSolution) {
      PetscCall(TSMonitorLGCtxSetDisplayVariables((TSMonitorLGCtx)ts->monitorcontext[i], displaynames));
      break;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGSetTransform - Solution vector will be transformed by provided function before being displayed

   Collective

   Input Parameters:
+  ts - the `TS` context
.  transform - the transform function
.  destroy - function to destroy the optional context
-  ctx - optional context used by transform function

   Level: intermediate

   Note:
    If the `TS` object does not have a `TSMonitorLGCtx` associated with it then this function is ignored

.seealso: [](chapter_ts), `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGSetVariableNames()`, `TSMonitorLGCtxSetTransform()`
@*/
PetscErrorCode TSMonitorLGSetTransform(TS ts, PetscErrorCode (*transform)(void *, Vec, Vec *), PetscErrorCode (*destroy)(void *), void *tctx)
{
  PetscInt i;

  PetscFunctionBegin;
  for (i = 0; i < ts->numbermonitors; i++) {
    if (ts->monitor[i] == TSMonitorLGSolution) PetscCall(TSMonitorLGCtxSetTransform((TSMonitorLGCtx)ts->monitorcontext[i], transform, destroy, tctx));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGCtxSetTransform - Solution vector will be transformed by provided function before being displayed

   Collective

   Input Parameters:
+  ts - the `TS` context
.  transform - the transform function
.  destroy - function to destroy the optional context
-  ctx - optional context used by transform function

   Level: intermediate

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGSetVariableNames()`, `TSMonitorLGSetTransform()`
@*/
PetscErrorCode TSMonitorLGCtxSetTransform(TSMonitorLGCtx ctx, PetscErrorCode (*transform)(void *, Vec, Vec *), PetscErrorCode (*destroy)(void *), void *tctx)
{
  PetscFunctionBegin;
  ctx->transform        = transform;
  ctx->transformdestroy = destroy;
  ctx->transformctx     = tctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorLGError - Monitors progress of the `TS` solvers by plotting each component of the error
       in a time based line graph

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
.  u - current solution
-  dctx - `TSMonitorLGCtx` object created with `TSMonitorLGCtxCreate()`

   Options Database Key:
.  -ts_monitor_lg_error - create a graphical monitor of error history

   Level: intermediate

   Notes:
    Each process in a parallel run displays its component errors in a separate window

   The user must provide the solution using `TSSetSolutionFunction()` to use this monitor.

   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the TS integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSSetSolutionFunction()`
@*/
PetscErrorCode TSMonitorLGError(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dummy)
{
  TSMonitorLGCtx     ctx = (TSMonitorLGCtx)dummy;
  const PetscScalar *yy;
  Vec                y;

  PetscFunctionBegin;
  if (!step) {
    PetscDrawAxis axis;
    PetscInt      dim;
    PetscCall(PetscDrawLGGetAxis(ctx->lg, &axis));
    PetscCall(PetscDrawAxisSetLabels(axis, "Error in solution as function of time", "Time", "Error"));
    PetscCall(VecGetLocalSize(u, &dim));
    PetscCall(PetscDrawLGSetDimension(ctx->lg, dim));
    PetscCall(PetscDrawLGReset(ctx->lg));
  }
  PetscCall(VecDuplicate(u, &y));
  PetscCall(TSComputeSolutionFunction(ts, ptime, y));
  PetscCall(VecAXPY(y, -1.0, u));
  PetscCall(VecGetArrayRead(y, &yy));
#if defined(PETSC_USE_COMPLEX)
  {
    PetscReal *yreal;
    PetscInt   i, n;
    PetscCall(VecGetLocalSize(y, &n));
    PetscCall(PetscMalloc1(n, &yreal));
    for (i = 0; i < n; i++) yreal[i] = PetscRealPart(yy[i]);
    PetscCall(PetscDrawLGAddCommonPoint(ctx->lg, ptime, yreal));
    PetscCall(PetscFree(yreal));
  }
#else
  PetscCall(PetscDrawLGAddCommonPoint(ctx->lg, ptime, yy));
#endif
  PetscCall(VecRestoreArrayRead(y, &yy));
  PetscCall(VecDestroy(&y));
  if (((ctx->howoften > 0) && (!(step % ctx->howoften))) || ((ctx->howoften == -1) && ts->reason)) {
    PetscCall(PetscDrawLGDraw(ctx->lg));
    PetscCall(PetscDrawLGSave(ctx->lg));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorSPSwarmSolution - Graphically displays phase plots of `DMSWARM` particles on a scatter plot

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
.  u - current solution
-  dctx - the `TSMonitorSPCtx` object that contains all the options for the monitoring, this is created with `TSMonitorSPCtxCreate()`

   Options Database Keys:
+ -ts_monitor_sp_swarm <n>                  - Monitor the solution every n steps, or -1 for plotting only the final solution
. -ts_monitor_sp_swarm_retain <n>           - Retain n old points so we can see the history, or -1 for all points
. -ts_monitor_sp_swarm_multi_species <bool> - Color each species differently
- -ts_monitor_sp_swarm_phase <bool>         - Plot in phase space, as opposed to coordinate space

   Level: intermediate

   Notes:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TS`, `TSMonitoSet()`, `DMSWARM`, `TSMonitorSPCtxCreate()`
@*/
PetscErrorCode TSMonitorSPSwarmSolution(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dctx)
{
  TSMonitorSPCtx     ctx = (TSMonitorSPCtx)dctx;
  PetscDraw          draw;
  DM                 dm, cdm;
  const PetscScalar *yy;
  PetscInt           Np, p, dim = 2, *species;
  PetscReal          species_color;

  PetscFunctionBegin;
  if (step < 0) PetscFunctionReturn(PETSC_SUCCESS); /* -1 indicates interpolated solution */
  PetscCall(TSGetDM(ts, &dm));
  if (!step) {
    PetscDrawAxis axis;
    PetscReal     dmboxlower[2], dmboxupper[2];

    PetscCall(TSGetDM(ts, &dm));
    PetscCall(DMGetDimension(dm, &dim));
    PetscCheck(dim == 2, PETSC_COMM_SELF, PETSC_ERR_SUP, "Monitor only supports two dimensional fields");
    PetscCall(DMSwarmGetCellDM(dm, &cdm));
    PetscCall(DMGetBoundingBox(cdm, dmboxlower, dmboxupper));
    PetscCall(VecGetLocalSize(u, &Np));
    Np /= dim * 2;
    PetscCall(PetscDrawSPGetAxis(ctx->sp, &axis));
    if (ctx->phase) {
      PetscCall(PetscDrawAxisSetLabels(axis, "Particles", "X", "V"));
      PetscCall(PetscDrawAxisSetLimits(axis, dmboxlower[0], dmboxupper[0], -10, 10));
    } else {
      PetscCall(PetscDrawAxisSetLabels(axis, "Particles", "X", "Y"));
      PetscCall(PetscDrawAxisSetLimits(axis, dmboxlower[0], dmboxupper[0], dmboxlower[1], dmboxupper[1]));
    }
    PetscCall(PetscDrawAxisSetHoldLimits(axis, PETSC_TRUE));
    PetscCall(PetscDrawSPReset(ctx->sp));
  }
  if (ctx->multispecies) PetscCall(DMSwarmGetField(dm, "species", NULL, NULL, (void **)&species));
  PetscCall(VecGetLocalSize(u, &Np));
  Np /= dim * 2;
  if (((ctx->howoften > 0) && (!(step % ctx->howoften))) || ((ctx->howoften == -1) && ts->reason)) {
    PetscCall(PetscDrawSPGetDraw(ctx->sp, &draw));
    if ((ctx->retain == 0) || (ctx->retain > 0 && !(step % ctx->retain))) PetscCall(PetscDrawClear(draw));
    PetscCall(PetscDrawFlush(draw));
    PetscCall(PetscDrawSPReset(ctx->sp));
    PetscCall(VecGetArrayRead(u, &yy));
    for (p = 0; p < Np; ++p) {
      PetscReal x, y;

      if (ctx->phase) {
        x = PetscRealPart(yy[p * dim * 2]);
        y = PetscRealPart(yy[p * dim * 2 + dim]);
      } else {
        x = PetscRealPart(yy[p * dim * 2]);
        y = PetscRealPart(yy[p * dim * 2 + 1]);
      }
      if (ctx->multispecies) {
        species_color = species[p] + 2;
        PetscCall(PetscDrawSPAddPointColorized(ctx->sp, &x, &y, &species_color));
      } else {
        PetscCall(PetscDrawSPAddPoint(ctx->sp, &x, &y));
      }
      PetscCall(PetscDrawSPAddPoint(ctx->sp, &x, &y));
    }
    PetscCall(VecRestoreArrayRead(u, &yy));
    PetscCall(PetscDrawSPDraw(ctx->sp, PETSC_FALSE));
    PetscCall(PetscDrawSPSave(ctx->sp));
    if (ctx->multispecies) PetscCall(DMSwarmRestoreField(dm, "species", NULL, NULL, (void **)&species));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorHGSwarmSolution - Graphically displays histograms of `DMSWARM` particles

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
.  u - current solution
-  dctx - the `TSMonitorSPCtx` object that contains all the options for the monitoring, this is created with `TSMonitorHGCtxCreate()`

   Options Database Keys:
+ -ts_monitor_hg_swarm <n>             - Monitor the solution every n steps, or -1 for plotting only the final solution
. -ts_monitor_hg_swarm_species <num>   - Number of species to histogram
. -ts_monitor_hg_swarm_bins <num>      - Number of histogram bins
- -ts_monitor_hg_swarm_velocity <bool> - Plot in velocity space, as opposed to coordinate space

   Level: intermediate

   Note:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: `TSMonitoSet()`
@*/
PetscErrorCode TSMonitorHGSwarmSolution(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dctx)
{
  TSMonitorHGCtx     ctx = (TSMonitorHGCtx)dctx;
  PetscDraw          draw;
  DM                 sw;
  const PetscScalar *yy;
  PetscInt          *species;
  PetscInt           dim, d = 0, Np, p, Ns, s;

  PetscFunctionBegin;
  if (step < 0) PetscFunctionReturn(PETSC_SUCCESS); /* -1 indicates interpolated solution */
  PetscCall(TSGetDM(ts, &sw));
  PetscCall(DMGetDimension(sw, &dim));
  PetscCall(DMSwarmGetNumSpecies(sw, &Ns));
  Ns = PetscMin(Ns, ctx->Ns);
  PetscCall(VecGetLocalSize(u, &Np));
  Np /= dim * 2;
  if (!step) {
    PetscDrawAxis axis;
    char          title[PETSC_MAX_PATH_LEN];

    for (s = 0; s < Ns; ++s) {
      PetscCall(PetscDrawHGGetAxis(ctx->hg[s], &axis));
      PetscCall(PetscSNPrintf(title, PETSC_MAX_PATH_LEN, "Species %" PetscInt_FMT, s));
      if (ctx->velocity) PetscCall(PetscDrawAxisSetLabels(axis, title, "V", "N"));
      else PetscCall(PetscDrawAxisSetLabels(axis, title, "X", "N"));
    }
  }
  if (((ctx->howoften > 0) && (!(step % ctx->howoften))) || ((ctx->howoften == -1) && ts->reason)) {
    PetscCall(DMSwarmGetField(sw, "species", NULL, NULL, (void **)&species));
    for (s = 0; s < Ns; ++s) {
      PetscCall(PetscDrawHGReset(ctx->hg[s]));
      PetscCall(PetscDrawHGGetDraw(ctx->hg[s], &draw));
      PetscCall(PetscDrawClear(draw));
      PetscCall(PetscDrawFlush(draw));
    }
    PetscCall(VecGetArrayRead(u, &yy));
    for (p = 0; p < Np; ++p) {
      const PetscInt s = species[p] < Ns ? species[p] : 0;
      PetscReal      v;

      if (ctx->velocity) v = PetscRealPart(yy[p * dim * 2 + d + dim]);
      else v = PetscRealPart(yy[p * dim * 2 + d]);
      PetscCall(PetscDrawHGAddValue(ctx->hg[s], v));
    }
    PetscCall(VecRestoreArrayRead(u, &yy));
    for (s = 0; s < Ns; ++s) {
      PetscCall(PetscDrawHGDraw(ctx->hg[s]));
      PetscCall(PetscDrawHGSave(ctx->hg[s]));
    }
    PetscCall(DMSwarmRestoreField(sw, "species", NULL, NULL, (void **)&species));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorError - Monitors progress of the `TS` solvers by printing the 2 norm of the error at each timestep

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
.  u - current solution
-  dctx - unused context

   Options Database Key:
.  -ts_monitor_error - create a graphical monitor of error history

   Level: intermediate

   Notes:
   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

   The user must provide the solution using `TSSetSolutionFunction()` to use this monitor.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSSetSolutionFunction()`
@*/
PetscErrorCode TSMonitorError(TS ts, PetscInt step, PetscReal ptime, Vec u, PetscViewerAndFormat *vf)
{
  DM        dm;
  PetscDS   ds = NULL;
  PetscInt  Nf = -1, f;
  PetscBool flg;

  PetscFunctionBegin;
  PetscCall(TSGetDM(ts, &dm));
  if (dm) PetscCall(DMGetDS(dm, &ds));
  if (ds) PetscCall(PetscDSGetNumFields(ds, &Nf));
  if (Nf <= 0) {
    Vec       y;
    PetscReal nrm;

    PetscCall(VecDuplicate(u, &y));
    PetscCall(TSComputeSolutionFunction(ts, ptime, y));
    PetscCall(VecAXPY(y, -1.0, u));
    PetscCall(PetscObjectTypeCompare((PetscObject)vf->viewer, PETSCVIEWERASCII, &flg));
    if (flg) {
      PetscCall(VecNorm(y, NORM_2, &nrm));
      PetscCall(PetscViewerASCIIPrintf(vf->viewer, "2-norm of error %g\n", (double)nrm));
    }
    PetscCall(PetscObjectTypeCompare((PetscObject)vf->viewer, PETSCVIEWERDRAW, &flg));
    if (flg) PetscCall(VecView(y, vf->viewer));
    PetscCall(VecDestroy(&y));
  } else {
    PetscErrorCode (**exactFuncs)(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nf, PetscScalar *u, void *ctx);
    void    **ctxs;
    Vec       v;
    PetscReal ferrors[1];

    PetscCall(PetscMalloc2(Nf, &exactFuncs, Nf, &ctxs));
    for (f = 0; f < Nf; ++f) PetscCall(PetscDSGetExactSolution(ds, f, &exactFuncs[f], &ctxs[f]));
    PetscCall(DMComputeL2FieldDiff(dm, ptime, exactFuncs, ctxs, u, ferrors));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Timestep: %04d time = %-8.4g \t L_2 Error: [", (int)step, (double)ptime));
    for (f = 0; f < Nf; ++f) {
      if (f > 0) PetscCall(PetscPrintf(PETSC_COMM_WORLD, ", "));
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%2.3g", (double)ferrors[f]));
    }
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "]\n"));

    PetscCall(VecViewFromOptions(u, NULL, "-sol_vec_view"));

    PetscCall(PetscOptionsHasName(NULL, NULL, "-exact_vec_view", &flg));
    if (flg) {
      PetscCall(DMGetGlobalVector(dm, &v));
      PetscCall(DMProjectFunction(dm, ptime, exactFuncs, ctxs, INSERT_ALL_VALUES, v));
      PetscCall(PetscObjectSetName((PetscObject)v, "Exact Solution"));
      PetscCall(VecViewFromOptions(v, NULL, "-exact_vec_view"));
      PetscCall(DMRestoreGlobalVector(dm, &v));
    }
    PetscCall(PetscFree2(exactFuncs, ctxs));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSMonitorLGSNESIterations(TS ts, PetscInt n, PetscReal ptime, Vec v, void *monctx)
{
  TSMonitorLGCtx ctx = (TSMonitorLGCtx)monctx;
  PetscReal      x   = ptime, y;
  PetscInt       its;

  PetscFunctionBegin;
  if (n < 0) PetscFunctionReturn(PETSC_SUCCESS); /* -1 indicates interpolated solution */
  if (!n) {
    PetscDrawAxis axis;
    PetscCall(PetscDrawLGGetAxis(ctx->lg, &axis));
    PetscCall(PetscDrawAxisSetLabels(axis, "Nonlinear iterations as function of time", "Time", "SNES Iterations"));
    PetscCall(PetscDrawLGReset(ctx->lg));
    ctx->snes_its = 0;
  }
  PetscCall(TSGetSNESIterations(ts, &its));
  y = its - ctx->snes_its;
  PetscCall(PetscDrawLGAddPoint(ctx->lg, &x, &y));
  if (((ctx->howoften > 0) && (!(n % ctx->howoften)) && (n > -1)) || ((ctx->howoften == -1) && (n == -1))) {
    PetscCall(PetscDrawLGDraw(ctx->lg));
    PetscCall(PetscDrawLGSave(ctx->lg));
  }
  ctx->snes_its = its;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSMonitorLGKSPIterations(TS ts, PetscInt n, PetscReal ptime, Vec v, void *monctx)
{
  TSMonitorLGCtx ctx = (TSMonitorLGCtx)monctx;
  PetscReal      x   = ptime, y;
  PetscInt       its;

  PetscFunctionBegin;
  if (n < 0) PetscFunctionReturn(PETSC_SUCCESS); /* -1 indicates interpolated solution */
  if (!n) {
    PetscDrawAxis axis;
    PetscCall(PetscDrawLGGetAxis(ctx->lg, &axis));
    PetscCall(PetscDrawAxisSetLabels(axis, "Linear iterations as function of time", "Time", "KSP Iterations"));
    PetscCall(PetscDrawLGReset(ctx->lg));
    ctx->ksp_its = 0;
  }
  PetscCall(TSGetKSPIterations(ts, &its));
  y = its - ctx->ksp_its;
  PetscCall(PetscDrawLGAddPoint(ctx->lg, &x, &y));
  if (((ctx->howoften > 0) && (!(n % ctx->howoften)) && (n > -1)) || ((ctx->howoften == -1) && (n == -1))) {
    PetscCall(PetscDrawLGDraw(ctx->lg));
    PetscCall(PetscDrawLGSave(ctx->lg));
  }
  ctx->ksp_its = its;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorEnvelopeCtxCreate - Creates a context for use with `TSMonitorEnvelope()`

   Collective

   Input Parameter:
.  ts  - the `TS` solver object

   Output Parameter:
.  ctx - the context

   Level: intermediate

.seealso: [](chapter_ts), `TS`, `TSMonitorLGTimeStep()`, `TSMonitorSet()`, `TSMonitorLGSolution()`, `TSMonitorLGError()`
@*/
PetscErrorCode TSMonitorEnvelopeCtxCreate(TS ts, TSMonitorEnvelopeCtx *ctx)
{
  PetscFunctionBegin;
  PetscCall(PetscNew(ctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorEnvelope - Monitors the maximum and minimum value of each component of the solution

   Collective

   Input Parameters:
+  ts - the `TS` context
.  step - current time-step
.  ptime - current time
.  u  - current solution
-  dctx - the envelope context

   Options Database Key:
.  -ts_monitor_envelope - determine maximum and minimum value of each component of the solution over the solution time

   Level: intermediate

   Notes:
   After a solve you can use `TSMonitorEnvelopeGetBounds()` to access the envelope

   This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
   to be used during the `TS` integration.

.seealso: [](chapter_ts), `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorEnvelopeGetBounds()`, `TSMonitorEnvelopeCtxCreate()`
@*/
PetscErrorCode TSMonitorEnvelope(TS ts, PetscInt step, PetscReal ptime, Vec u, void *dctx)
{
  TSMonitorEnvelopeCtx ctx = (TSMonitorEnvelopeCtx)dctx;

  PetscFunctionBegin;
  if (!ctx->max) {
    PetscCall(VecDuplicate(u, &ctx->max));
    PetscCall(VecDuplicate(u, &ctx->min));
    PetscCall(VecCopy(u, ctx->max));
    PetscCall(VecCopy(u, ctx->min));
  } else {
    PetscCall(VecPointwiseMax(ctx->max, u, ctx->max));
    PetscCall(VecPointwiseMin(ctx->min, u, ctx->min));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorEnvelopeGetBounds - Gets the bounds for the components of the solution

   Collective

   Input Parameter:
.  ts - the `TS` context

   Output Parameters:
+  max - the maximum values
-  min - the minimum values

   Level: intermediate

   Notes:
    If the `TS` does not have a `TSMonitorEnvelopeCtx` associated with it then this function is ignored

.seealso: [](chapter_ts), `TSMonitorEnvelopeCtx`, `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `VecView()`, `TSMonitorLGSetDisplayVariables()`
@*/
PetscErrorCode TSMonitorEnvelopeGetBounds(TS ts, Vec *max, Vec *min)
{
  PetscInt i;

  PetscFunctionBegin;
  if (max) *max = NULL;
  if (min) *min = NULL;
  for (i = 0; i < ts->numbermonitors; i++) {
    if (ts->monitor[i] == TSMonitorEnvelope) {
      TSMonitorEnvelopeCtx ctx = (TSMonitorEnvelopeCtx)ts->monitorcontext[i];
      if (max) *max = ctx->max;
      if (min) *min = ctx->min;
      break;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSMonitorEnvelopeCtxDestroy - Destroys a context that was created  with `TSMonitorEnvelopeCtxCreate()`.

   Collective

   Input Parameter:
.  ctx - the monitor context

   Level: intermediate

.seealso: [](chapter_ts), `TS`, `TSMonitorLGCtxCreate()`, `TSMonitorSet()`, `TSMonitorLGTimeStep()`
@*/
PetscErrorCode TSMonitorEnvelopeCtxDestroy(TSMonitorEnvelopeCtx *ctx)
{
  PetscFunctionBegin;
  PetscCall(VecDestroy(&(*ctx)->min));
  PetscCall(VecDestroy(&(*ctx)->max));
  PetscCall(PetscFree(*ctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  TSDMSwarmMonitorMoments - Monitors the first three moments of a `DMSWARM` being evolved by the `TS`

  Not Collective

  Input Parameters:
+ ts   - the `TS` context
. step - current timestep
. t    - current time
. u    - current solution
- ctx  - not used

  Options Database Key:
. -ts_dmswarm_monitor_moments - Monitor moments of particle distribution

  Level: intermediate

  Notes:
  This requires a `DMSWARM` be attached to the `TS`.

  This is not called directly by users, rather one calls `TSMonitorSet()`, with this function as an argument, to cause the monitor
  to be used during the TS integration.

.seealso: [](chapter_ts), `TS`, `TSMonitorSet()`, `TSMonitorDefault()`, `DMSWARM`
@*/
PetscErrorCode TSDMSwarmMonitorMoments(TS ts, PetscInt step, PetscReal t, Vec U, PetscViewerAndFormat *vf)
{
  DM                 sw;
  const PetscScalar *u;
  PetscReal          m = 1.0, totE = 0., totMom[3] = {0., 0., 0.};
  PetscInt           dim, d, Np, p;
  MPI_Comm           comm;

  PetscFunctionBeginUser;
  PetscCall(TSGetDM(ts, &sw));
  if (!sw || step % ts->monitorFrequency != 0) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscObjectGetComm((PetscObject)ts, &comm));
  PetscCall(DMGetDimension(sw, &dim));
  PetscCall(VecGetLocalSize(U, &Np));
  Np /= dim;
  PetscCall(VecGetArrayRead(U, &u));
  for (p = 0; p < Np; ++p) {
    for (d = 0; d < dim; ++d) {
      totE += PetscRealPart(u[p * dim + d] * u[p * dim + d]);
      totMom[d] += PetscRealPart(u[p * dim + d]);
    }
  }
  PetscCall(VecRestoreArrayRead(U, &u));
  for (d = 0; d < dim; ++d) totMom[d] *= m;
  totE *= 0.5 * m;
  PetscCall(PetscPrintf(comm, "Step %4" PetscInt_FMT " Total Energy: %10.8lf", step, (double)totE));
  for (d = 0; d < dim; ++d) PetscCall(PetscPrintf(comm, "    Total Momentum %c: %10.8lf", (char)('x' + d), (double)totMom[d]));
  PetscCall(PetscPrintf(comm, "\n"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

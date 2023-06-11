
/*
      PETSc code to log object creation and destruction and PETSc events.

      This provides the public API used by the rest of PETSc and by users.

      These routines use a private API that is not used elsewhere in PETSc and is not
      accessible to users. The private API is defined in logimpl.h and the utils directory.

*/
#include <petsc/private/logimpl.h> /*I    "petscsys.h"   I*/
#include <petsctime.h>
#include <petscviewer.h>
#include <petscdevice.h>
#include <petsc/private/deviceimpl.h>
#if defined(PETSC_HAVE_TAU_PERFSTUBS)
  #include <../src/sys/perfstubs/timer.h>
#endif

PetscLogEvent PETSC_LARGEST_EVENT = PETSC_EVENT;

#if defined(PETSC_USE_LOG)
  #include <petscmachineinfo.h>
  #include <petscconfiginfo.h>

  #if defined(PETSC_HAVE_THREADSAFETY)

PetscInt           petsc_log_gid = -1; /* Global threadId counter */
PETSC_TLS PetscInt petsc_log_tid = -1; /* Local threadId */

/* shared variables */
PetscSpinlock  PetscLogSpinLock;
PetscHMapEvent eventInfoMap_th = NULL;

  #endif

/* used in the MPI_XXX() count macros in petsclog.h */

/* Action and object logging variables */
Action   *petsc_actions    = NULL;
Object   *petsc_objects    = NULL;
PetscBool petsc_logActions = PETSC_FALSE;
PetscBool petsc_logObjects = PETSC_FALSE;
int       petsc_numActions = 0, petsc_maxActions = 100;
int       petsc_numObjects = 0, petsc_maxObjects = 100;
int       petsc_numObjectsDestroyed = 0;

/* Global counters */
PetscLogDouble petsc_BaseTime        = 0.0;
PetscLogDouble petsc_TotalFlops      = 0.0; /* The number of flops */
PetscLogDouble petsc_send_ct         = 0.0; /* The number of sends */
PetscLogDouble petsc_recv_ct         = 0.0; /* The number of receives */
PetscLogDouble petsc_send_len        = 0.0; /* The total length of all sent messages */
PetscLogDouble petsc_recv_len        = 0.0; /* The total length of all received messages */
PetscLogDouble petsc_isend_ct        = 0.0; /* The number of immediate sends */
PetscLogDouble petsc_irecv_ct        = 0.0; /* The number of immediate receives */
PetscLogDouble petsc_isend_len       = 0.0; /* The total length of all immediate send messages */
PetscLogDouble petsc_irecv_len       = 0.0; /* The total length of all immediate receive messages */
PetscLogDouble petsc_wait_ct         = 0.0; /* The number of waits */
PetscLogDouble petsc_wait_any_ct     = 0.0; /* The number of anywaits */
PetscLogDouble petsc_wait_all_ct     = 0.0; /* The number of waitalls */
PetscLogDouble petsc_sum_of_waits_ct = 0.0; /* The total number of waits */
PetscLogDouble petsc_allreduce_ct    = 0.0; /* The number of reductions */
PetscLogDouble petsc_gather_ct       = 0.0; /* The number of gathers and gathervs */
PetscLogDouble petsc_scatter_ct      = 0.0; /* The number of scatters and scattervs */

/* Thread Local storage */
PETSC_TLS PetscLogDouble petsc_TotalFlops_th      = 0.0;
PETSC_TLS PetscLogDouble petsc_send_ct_th         = 0.0;
PETSC_TLS PetscLogDouble petsc_recv_ct_th         = 0.0;
PETSC_TLS PetscLogDouble petsc_send_len_th        = 0.0;
PETSC_TLS PetscLogDouble petsc_recv_len_th        = 0.0;
PETSC_TLS PetscLogDouble petsc_isend_ct_th        = 0.0;
PETSC_TLS PetscLogDouble petsc_irecv_ct_th        = 0.0;
PETSC_TLS PetscLogDouble petsc_isend_len_th       = 0.0;
PETSC_TLS PetscLogDouble petsc_irecv_len_th       = 0.0;
PETSC_TLS PetscLogDouble petsc_wait_ct_th         = 0.0;
PETSC_TLS PetscLogDouble petsc_wait_any_ct_th     = 0.0;
PETSC_TLS PetscLogDouble petsc_wait_all_ct_th     = 0.0;
PETSC_TLS PetscLogDouble petsc_sum_of_waits_ct_th = 0.0;
PETSC_TLS PetscLogDouble petsc_allreduce_ct_th    = 0.0;
PETSC_TLS PetscLogDouble petsc_gather_ct_th       = 0.0;
PETSC_TLS PetscLogDouble petsc_scatter_ct_th      = 0.0;

  #if defined(PETSC_HAVE_DEVICE)
PetscLogDouble petsc_ctog_ct        = 0.0; /* The total number of CPU to GPU copies */
PetscLogDouble petsc_gtoc_ct        = 0.0; /* The total number of GPU to CPU copies */
PetscLogDouble petsc_ctog_sz        = 0.0; /* The total size of CPU to GPU copies */
PetscLogDouble petsc_gtoc_sz        = 0.0; /* The total size of GPU to CPU copies */
PetscLogDouble petsc_ctog_ct_scalar = 0.0; /* The total number of CPU to GPU copies */
PetscLogDouble petsc_gtoc_ct_scalar = 0.0; /* The total number of GPU to CPU copies */
PetscLogDouble petsc_ctog_sz_scalar = 0.0; /* The total size of CPU to GPU copies */
PetscLogDouble petsc_gtoc_sz_scalar = 0.0; /* The total size of GPU to CPU copies */
PetscLogDouble petsc_gflops         = 0.0; /* The flops done on a GPU */
PetscLogDouble petsc_gtime          = 0.0; /* The time spent on a GPU */

PETSC_TLS PetscLogDouble petsc_ctog_ct_th        = 0.0;
PETSC_TLS PetscLogDouble petsc_gtoc_ct_th        = 0.0;
PETSC_TLS PetscLogDouble petsc_ctog_sz_th        = 0.0;
PETSC_TLS PetscLogDouble petsc_gtoc_sz_th        = 0.0;
PETSC_TLS PetscLogDouble petsc_ctog_ct_scalar_th = 0.0;
PETSC_TLS PetscLogDouble petsc_gtoc_ct_scalar_th = 0.0;
PETSC_TLS PetscLogDouble petsc_ctog_sz_scalar_th = 0.0;
PETSC_TLS PetscLogDouble petsc_gtoc_sz_scalar_th = 0.0;
PETSC_TLS PetscLogDouble petsc_gflops_th         = 0.0;
PETSC_TLS PetscLogDouble petsc_gtime_th          = 0.0;
  #endif

  #if defined(PETSC_HAVE_THREADSAFETY)
PetscErrorCode PetscAddLogDouble(PetscLogDouble *tot, PetscLogDouble *tot_th, PetscLogDouble tmp)
{
  *tot_th += tmp;
  PetscCall(PetscSpinlockLock(&PetscLogSpinLock));
  *tot += tmp;
  PetscCall(PetscSpinlockUnlock(&PetscLogSpinLock));
  return PETSC_SUCCESS;
}

PetscErrorCode PetscAddLogDoubleCnt(PetscLogDouble *cnt, PetscLogDouble *tot, PetscLogDouble *cnt_th, PetscLogDouble *tot_th, PetscLogDouble tmp)
{
  *cnt_th = *cnt_th + 1;
  *tot_th += tmp;
  PetscCall(PetscSpinlockLock(&PetscLogSpinLock));
  *tot += (PetscLogDouble)(tmp);
  *cnt += *cnt + 1;
  PetscCall(PetscSpinlockUnlock(&PetscLogSpinLock));
  return PETSC_SUCCESS;
}

PetscInt PetscLogGetTid(void)
{
  if (petsc_log_tid < 0) {
    PetscCall(PetscSpinlockLock(&PetscLogSpinLock));
    petsc_log_tid = ++petsc_log_gid;
    PetscCall(PetscSpinlockUnlock(&PetscLogSpinLock));
  }
  return petsc_log_tid;
}

  #endif

/* Logging functions */
PetscErrorCode (*PetscLogPHC)(PetscObject)                                                            = NULL;
PetscErrorCode (*PetscLogPHD)(PetscObject)                                                            = NULL;
PetscErrorCode (*PetscLogPLB)(PetscLogEvent, int, PetscObject, PetscObject, PetscObject, PetscObject) = NULL;
PetscErrorCode (*PetscLogPLE)(PetscLogEvent, int, PetscObject, PetscObject, PetscObject, PetscObject) = NULL;

/* Tracing event logging variables */
FILE            *petsc_tracefile          = NULL;
int              petsc_tracelevel         = 0;
const char      *petsc_traceblanks        = "                                                                                                    ";
char             petsc_tracespace[128]    = " ";
PetscLogDouble   petsc_tracetime          = 0.0;
static PetscBool PetscLogInitializeCalled = PETSC_FALSE;

static PetscIntStack current_log_event_stack = NULL;

PETSC_INTERN PetscErrorCode PetscLogInitialize(void)
{
  int       stage;
  PetscBool opt;

  PetscFunctionBegin;
  if (PetscLogInitializeCalled) PetscFunctionReturn(PETSC_SUCCESS);
  PetscLogInitializeCalled = PETSC_TRUE;

  PetscCall(PetscIntStackCreate(&current_log_event_stack));
  PetscCall(PetscOptionsHasName(NULL, NULL, "-log_exclude_actions", &opt));
  if (opt) petsc_logActions = PETSC_FALSE;
  PetscCall(PetscOptionsHasName(NULL, NULL, "-log_exclude_objects", &opt));
  if (opt) petsc_logObjects = PETSC_FALSE;
  if (petsc_logActions) PetscCall(PetscMalloc1(petsc_maxActions, &petsc_actions));
  if (petsc_logObjects) PetscCall(PetscMalloc1(petsc_maxObjects, &petsc_objects));
  PetscLogPHC = PetscLogObjCreateDefault;
  PetscLogPHD = PetscLogObjDestroyDefault;
  /* Setup default logging structures */
  PetscCall(PetscStageLogCreate(&petsc_stageLog));
  PetscCall(PetscStageLogRegister(petsc_stageLog, "Main Stage", &stage));

  PetscCall(PetscSpinlockCreate(&PetscLogSpinLock));
  #if defined(PETSC_HAVE_THREADSAFETY)
  petsc_log_tid = 0;
  petsc_log_gid = 0;
  PetscCall(PetscHMapEventCreate(&eventInfoMap_th));
  #endif

  /* All processors sync here for more consistent logging */
  PetscCallMPI(MPI_Barrier(PETSC_COMM_WORLD));
  PetscCall(PetscTime(&petsc_BaseTime));
  PetscCall(PetscLogStagePush(stage));
  #if defined(PETSC_HAVE_TAU_PERFSTUBS)
  PetscStackCallExternalVoid("ps_initialize_", ps_initialize_());
  #endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_INTERN PetscErrorCode PetscLogFinalize(void)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  #if defined(PETSC_HAVE_THREADSAFETY)
  if (eventInfoMap_th) {
    PetscEventPerfInfo **array;
    PetscInt             n, off = 0;

    PetscCall(PetscHMapEventGetSize(eventInfoMap_th, &n));
    PetscCall(PetscMalloc1(n, &array));
    PetscCall(PetscHMapEventGetVals(eventInfoMap_th, &off, array));
    for (PetscInt i = 0; i < n; i++) PetscCall(PetscFree(array[i]));
    PetscCall(PetscFree(array));
    PetscCall(PetscHMapEventDestroy(&eventInfoMap_th));
  }
  #endif
  PetscCall(PetscFree(petsc_actions));
  PetscCall(PetscFree(petsc_objects));
  PetscCall(PetscLogNestedEnd());
  PetscCall(PetscLogSet(NULL, NULL));

  /* Resetting phase */
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogDestroy(stageLog));
  PetscCall(PetscIntStackDestroy(current_log_event_stack));
  current_log_event_stack = NULL;

  petsc_TotalFlops          = 0.0;
  petsc_numActions          = 0;
  petsc_numObjects          = 0;
  petsc_numObjectsDestroyed = 0;
  petsc_maxActions          = 100;
  petsc_maxObjects          = 100;
  petsc_actions             = NULL;
  petsc_objects             = NULL;
  petsc_logActions          = PETSC_FALSE;
  petsc_logObjects          = PETSC_FALSE;
  petsc_BaseTime            = 0.0;
  petsc_TotalFlops          = 0.0;
  petsc_send_ct             = 0.0;
  petsc_recv_ct             = 0.0;
  petsc_send_len            = 0.0;
  petsc_recv_len            = 0.0;
  petsc_isend_ct            = 0.0;
  petsc_irecv_ct            = 0.0;
  petsc_isend_len           = 0.0;
  petsc_irecv_len           = 0.0;
  petsc_wait_ct             = 0.0;
  petsc_wait_any_ct         = 0.0;
  petsc_wait_all_ct         = 0.0;
  petsc_sum_of_waits_ct     = 0.0;
  petsc_allreduce_ct        = 0.0;
  petsc_gather_ct           = 0.0;
  petsc_scatter_ct          = 0.0;
  petsc_TotalFlops_th       = 0.0;
  petsc_send_ct_th          = 0.0;
  petsc_recv_ct_th          = 0.0;
  petsc_send_len_th         = 0.0;
  petsc_recv_len_th         = 0.0;
  petsc_isend_ct_th         = 0.0;
  petsc_irecv_ct_th         = 0.0;
  petsc_isend_len_th        = 0.0;
  petsc_irecv_len_th        = 0.0;
  petsc_wait_ct_th          = 0.0;
  petsc_wait_any_ct_th      = 0.0;
  petsc_wait_all_ct_th      = 0.0;
  petsc_sum_of_waits_ct_th  = 0.0;
  petsc_allreduce_ct_th     = 0.0;
  petsc_gather_ct_th        = 0.0;
  petsc_scatter_ct_th       = 0.0;

  #if defined(PETSC_HAVE_DEVICE)
  petsc_ctog_ct    = 0.0;
  petsc_gtoc_ct    = 0.0;
  petsc_ctog_sz    = 0.0;
  petsc_gtoc_sz    = 0.0;
  petsc_gflops     = 0.0;
  petsc_gtime      = 0.0;
  petsc_ctog_ct_th = 0.0;
  petsc_gtoc_ct_th = 0.0;
  petsc_ctog_sz_th = 0.0;
  petsc_gtoc_sz_th = 0.0;
  petsc_gflops_th  = 0.0;
  petsc_gtime_th   = 0.0;
  #endif

  PETSC_LARGEST_EVENT      = PETSC_EVENT;
  PetscLogPHC              = NULL;
  PetscLogPHD              = NULL;
  petsc_tracefile          = NULL;
  petsc_tracelevel         = 0;
  petsc_traceblanks        = "                                                                                                    ";
  petsc_tracespace[0]      = ' ';
  petsc_tracespace[1]      = 0;
  petsc_tracetime          = 0.0;
  PETSC_LARGEST_CLASSID    = PETSC_SMALLEST_CLASSID;
  PETSC_OBJECT_CLASSID     = 0;
  petsc_stageLog           = NULL;
  PetscLogInitializeCalled = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogSet - Sets the logging functions called at the beginning and ending of every event.

  Not Collective

  Input Parameters:
+ b - The function called at beginning of event
- e - The function called at end of event

  Level: developer

  Developer Note:
  The default loggers are `PetscLogEventBeginDefault()` and `PetscLogEventEndDefault()`.

.seealso: [](ch_profiling), `PetscLogDump()`, `PetscLogDefaultBegin()`, `PetscLogAllBegin()`, `PetscLogTraceBegin()`, `PetscLogEventBeginDefault()`, `PetscLogEventEndDefault()`
@*/
PetscErrorCode PetscLogSet(PetscErrorCode (*b)(PetscLogEvent, int, PetscObject, PetscObject, PetscObject, PetscObject), PetscErrorCode (*e)(PetscLogEvent, int, PetscObject, PetscObject, PetscObject, PetscObject))
{
  PetscFunctionBegin;
  PetscLogPLB = b;
  PetscLogPLE = e;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogIsActive - Check if logging is currently in progress.

  Not Collective

  Output Parameter:
. isActive - `PETSC_TRUE` if logging is in progress, `PETSC_FALSE` otherwise

  Level: beginner

.seealso: [](ch_profiling), `PetscLogDefaultBegin()`, `PetscLogAllBegin()`, `PetscLogSet()`
@*/
PetscErrorCode PetscLogIsActive(PetscBool *isActive)
{
  PetscFunctionBegin;
  *isActive = (PetscLogPLB && PetscLogPLE) ? PETSC_TRUE : PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogDefaultBegin - Turns on logging of objects and events using the default logging functions `PetscLogEventBeginDefault()` and `PetscLogEventEndDefault()`. This logs flop
  rates and object creation and should not slow programs down too much.
  This routine may be called more than once.

  Logically Collective over `PETSC_COMM_WORLD`

  Options Database Key:
. -log_view [viewertype:filename:viewerformat] - Prints summary of flop and timing information to the
                  screen (for code configured with --with-log=1 (which is the default))

  Usage:
.vb
      PetscInitialize(...);
      PetscLogDefaultBegin();
       ... code ...
      PetscLogView(viewer); or PetscLogDump();
      PetscFinalize();
.ve

  Level: advanced

  Note:
  `PetscLogView()` or `PetscLogDump()` actually cause the printing of
  the logging information.

.seealso: [](ch_profiling), `PetscLogDump()`, `PetscLogAllBegin()`, `PetscLogView()`, `PetscLogTraceBegin()`
@*/
PetscErrorCode PetscLogDefaultBegin(void)
{
  PetscFunctionBegin;
  PetscCall(PetscLogSet(PetscLogEventBeginDefault, PetscLogEventEndDefault));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogAllBegin - Turns on extensive logging of objects and events. Logs
  all events. This creates large log files and slows the program down.

  Logically Collective on `PETSC_COMM_WORLD`

  Options Database Key:
. -log_all - Prints extensive log information

  Usage:
.vb
     PetscInitialize(...);
     PetscLogAllBegin();
     ... code ...
     PetscLogDump(filename);
     PetscFinalize();
.ve

  Level: advanced

  Note:
  A related routine is `PetscLogDefaultBegin()` (with the options key -log_view), which is
  intended for production runs since it logs only flop rates and object
  creation (and shouldn't significantly slow the programs).

.seealso: [](ch_profiling), `PetscLogDump()`, `PetscLogDefaultBegin()`, `PetscLogTraceBegin()`
@*/
PetscErrorCode PetscLogAllBegin(void)
{
  PetscFunctionBegin;
  PetscCall(PetscLogSet(PetscLogEventBeginComplete, PetscLogEventEndComplete));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogTraceBegin - Activates trace logging.  Every time a PETSc event
  begins or ends, the event name is printed.

  Logically Collective on `PETSC_COMM_WORLD`

  Input Parameter:
. file - The file to print trace in (e.g. stdout)

  Options Database Key:
. -log_trace [filename] - Activates `PetscLogTraceBegin()`

  Level: intermediate

  Notes:
  `PetscLogTraceBegin()` prints the processor number, the execution time (sec),
  then "Event begin:" or "Event end:" followed by the event name.

  `PetscLogTraceBegin()` allows tracing of all PETSc calls, which is useful
  to determine where a program is hanging without running in the
  debugger.  Can be used in conjunction with the -info option.

.seealso: [](ch_profiling), `PetscLogDump()`, `PetscLogAllBegin()`, `PetscLogView()`, `PetscLogDefaultBegin()`
@*/
PetscErrorCode PetscLogTraceBegin(FILE *file)
{
  PetscFunctionBegin;
  petsc_tracefile = file;

  PetscCall(PetscLogSet(PetscLogEventBeginTrace, PetscLogEventEndTrace));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogActions - Determines whether actions are logged for the graphical viewer.

  Not Collective

  Input Parameter:
. flag - `PETSC_TRUE` if actions are to be logged

  Options Database Key:
. -log_exclude_actions - Turns off actions logging

  Level: intermediate

  Note:
  Logging of actions continues to consume more memory as the program
  runs. Long running programs should consider turning this feature off.
.seealso: [](ch_profiling), `PetscLogStagePush()`, `PetscLogStagePop()`
@*/
PetscErrorCode PetscLogActions(PetscBool flag)
{
  PetscFunctionBegin;
  petsc_logActions = flag;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogObjects - Determines whether objects are logged for the graphical viewer.

  Not Collective

  Input Parameter:
. flag - `PETSC_TRUE` if objects are to be logged

  Options Database Key:
. -log_exclude_objects - Turns off objects logging

  Level: intermediate

  Note:
  Logging of objects continues to consume more memory as the program
  runs. Long running programs should consider turning this feature off.

.seealso: [](ch_profiling), `PetscLogStagePush()`, `PetscLogStagePop()`
@*/
PetscErrorCode PetscLogObjects(PetscBool flag)
{
  PetscFunctionBegin;
  petsc_logObjects = flag;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*------------------------------------------------ Stage Functions --------------------------------------------------*/
/*@C
  PetscLogStageRegister - Attaches a character string name to a logging stage.

  Not Collective

  Input Parameter:
. sname - The name to associate with that stage

  Output Parameter:
. stage - The stage number

  Level: intermediate

.seealso: [](ch_profiling), `PetscLogStagePush()`, `PetscLogStagePop()`
@*/
PetscErrorCode PetscLogStageRegister(const char sname[], PetscLogStage *stage)
{
  PetscStageLog stageLog;
  PetscLogEvent event;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogRegister(stageLog, sname, stage));
  /* Copy events already changed in the main stage, this sucks */
  PetscCall(PetscEventPerfLogEnsureSize(stageLog->stageInfo[*stage].eventLog, stageLog->eventLog->numEvents));
  for (event = 0; event < stageLog->eventLog->numEvents; event++) PetscCall(PetscEventPerfInfoCopy(&stageLog->stageInfo[0].eventLog->eventInfo[event], &stageLog->stageInfo[*stage].eventLog->eventInfo[event]));
  PetscCall(PetscClassPerfLogEnsureSize(stageLog->stageInfo[*stage].classLog, stageLog->classLog->numClasses));
  #if defined(PETSC_HAVE_TAU_PERFSTUBS)
  if (perfstubs_initialized == PERFSTUBS_SUCCESS) PetscStackCallExternalVoid("ps_timer_create_", stageLog->stageInfo[*stage].timer = ps_timer_create_(sname));
  #endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogStagePush - This function pushes a stage on the logging stack. Events started and stopped until `PetscLogStagePop()` will be associated with the stage

  Not Collective

  Input Parameter:
. stage - The stage on which to log

  Usage:
  If the option -log_view is used to run the program containing the
  following code, then 2 sets of summary data will be printed during
  PetscFinalize().
.vb
      PetscInitialize(int *argc,char ***args,0,0);
      [stage 0 of code]
      PetscLogStagePush(1);
      [stage 1 of code]
      PetscLogStagePop();
      PetscBarrier(...);
      [more stage 0 of code]
      PetscFinalize();
.ve

  Level: intermediate

  Note:
  Use `PetscLogStageRegister()` to register a stage.

.seealso: [](ch_profiling), `PetscLogStagePop()`, `PetscLogStageRegister()`, `PetscBarrier()`
@*/
PetscErrorCode PetscLogStagePush(PetscLogStage stage)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogPush(stageLog, stage));
  #if defined(PETSC_HAVE_TAU_PERFSTUBS)
  if (perfstubs_initialized == PERFSTUBS_SUCCESS && stageLog->stageInfo[stage].timer != NULL) PetscStackCallExternalVoid("ps_timer_start_", ps_timer_start_(stageLog->stageInfo[stage].timer));
  #endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogStagePop - This function pops a stage from the logging stack that was pushed with `PetscLogStagePush()`

  Not Collective

  Usage:
  If the option -log_view is used to run the program containing the
  following code, then 2 sets of summary data will be printed during
  PetscFinalize().
.vb
      PetscInitialize(int *argc,char ***args,0,0);
      [stage 0 of code]
      PetscLogStagePush(1);
      [stage 1 of code]
      PetscLogStagePop();
      PetscBarrier(...);
      [more stage 0 of code]
      PetscFinalize();
.ve

  Level: intermediate

.seealso: [](ch_profiling), `PetscLogStagePush()`, `PetscLogStageRegister()`, `PetscBarrier()`
@*/
PetscErrorCode PetscLogStagePop(void)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  #if defined(PETSC_HAVE_TAU_PERFSTUBS)
  if (perfstubs_initialized == PERFSTUBS_SUCCESS && stageLog->stageInfo[stageLog->curStage].timer != NULL) PetscStackCallExternalVoid("ps_timer_stop_", ps_timer_stop_(stageLog->stageInfo[stageLog->curStage].timer));
  #endif
  PetscCall(PetscStageLogPop(stageLog));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogStageSetActive - Sets if a stage is used for `PetscLogEventBegin()` and `PetscLogEventEnd()`.

  Not Collective

  Input Parameters:
+ stage    - The stage
- isActive - The activity flag, `PETSC_TRUE` for logging, else `PETSC_FALSE` (defaults to `PETSC_TRUE`)

  Level: intermediate

  Note:
  If this is set to `PETSC_FALSE` the logging acts as if the stage did not exist

.seealso: [](ch_profiling), `PetscLogStageRegister()`, `PetscLogStagePush()`, `PetscLogStagePop()`, `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscPreLoadBegin()`, `PetscPreLoadEnd()`, `PetscPreLoadStage()`
@*/
PetscErrorCode PetscLogStageSetActive(PetscLogStage stage, PetscBool isActive)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogSetActive(stageLog, stage, isActive));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogStageGetActive - Checks if a stage is used for `PetscLogEventBegin()` and `PetscLogEventEnd()`.

  Not Collective

  Input Parameter:
. stage    - The stage

  Output Parameter:
. isActive - The activity flag, `PETSC_TRUE` for logging, else `PETSC_FALSE` (defaults to `PETSC_TRUE`)

  Level: intermediate

.seealso: [](ch_profiling), `PetscLogStageRegister()`, `PetscLogStagePush()`, `PetscLogStagePop()`, `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscPreLoadBegin()`, `PetscPreLoadEnd()`, `PetscPreLoadStage()`
@*/
PetscErrorCode PetscLogStageGetActive(PetscLogStage stage, PetscBool *isActive)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetActive(stageLog, stage, isActive));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogStageSetVisible - Determines stage visibility in `PetscLogView()`

  Not Collective

  Input Parameters:
+ stage     - The stage
- isVisible - The visibility flag, `PETSC_TRUE` to print, else `PETSC_FALSE` (defaults to `PETSC_TRUE`)

  Level: intermediate

  Developer Note:
  What does visible mean, needs to be documented.

.seealso: [](ch_profiling), `PetscLogStageRegister()`, `PetscLogStagePush()`, `PetscLogStagePop()`, `PetscLogView()`
@*/
PetscErrorCode PetscLogStageSetVisible(PetscLogStage stage, PetscBool isVisible)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogSetVisible(stageLog, stage, isVisible));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogStageGetVisible - Returns stage visibility in `PetscLogView()`

  Not Collective

  Input Parameter:
. stage     - The stage

  Output Parameter:
. isVisible - The visibility flag, `PETSC_TRUE` to print, else `PETSC_FALSE` (defaults to `PETSC_TRUE`)

  Level: intermediate

.seealso: [](ch_profiling), `PetscLogStageRegister()`, `PetscLogStagePush()`, `PetscLogStagePop()`, `PetscLogView()`
@*/
PetscErrorCode PetscLogStageGetVisible(PetscLogStage stage, PetscBool *isVisible)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetVisible(stageLog, stage, isVisible));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogStageGetId - Returns the stage id when given the stage name.

  Not Collective

  Input Parameter:
. name  - The stage name

  Output Parameter:
. stage - The stage, , or -1 if no stage with that name exists

  Level: intermediate

.seealso: [](ch_profiling), `PetscLogStageRegister()`, `PetscLogStagePush()`, `PetscLogStagePop()`, `PetscPreLoadBegin()`, `PetscPreLoadEnd()`, `PetscPreLoadStage()`
@*/
PetscErrorCode PetscLogStageGetId(const char name[], PetscLogStage *stage)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetStage(stageLog, name, stage));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*------------------------------------------------ Event Functions --------------------------------------------------*/

/*@C
  PetscLogEventRegister - Registers an event name for logging operations

  Not Collective

  Input Parameters:
+ name   - The name associated with the event
- classid - The classid associated to the class for this event, obtain either with
           `PetscClassIdRegister()` or use a predefined one such as `KSP_CLASSID`, `SNES_CLASSID`, the predefined ones
           are only available in C code

  Output Parameter:
. event - The event id for use with `PetscLogEventBegin()` and `PetscLogEventEnd()`.

  Example of Usage:
.vb
      PetscLogEvent USER_EVENT;
      PetscClassId classid;
      PetscLogDouble user_event_flops;
      PetscClassIdRegister("class name",&classid);
      PetscLogEventRegister("User event name",classid,&USER_EVENT);
      PetscLogEventBegin(USER_EVENT,0,0,0,0);
         [code segment to monitor]
         PetscLogFlops(user_event_flops);
      PetscLogEventEnd(USER_EVENT,0,0,0,0);
.ve

  Level: intermediate

  Notes:
  PETSc automatically logs library events if the code has been
  configured with --with-log (which is the default) and
  -log_view or -log_all is specified.  `PetscLogEventRegister()` is
  intended for logging user events to supplement this PETSc
  information.

  PETSc can gather data for use with the utilities Jumpshot
  (part of the MPICH distribution).  If PETSc has been compiled
  with flag -DPETSC_HAVE_MPE (MPE is an additional utility within
  MPICH), the user can employ another command line option, -log_mpe,
  to create a logfile, "mpe.log", which can be visualized
  Jumpshot.

  The classid is associated with each event so that classes of events
  can be disabled simultaneously, such as all matrix events. The user
  can either use an existing classid, such as `MAT_CLASSID`, or create
  their own as shown in the example.

  If an existing event with the same name exists, its event handle is
  returned instead of creating a new event.

.seealso: [](ch_profiling), `PetscLogStageRegister()`, `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscLogFlops()`,
          `PetscLogEventActivate()`, `PetscLogEventDeactivate()`, `PetscClassIdRegister()`
@*/
PetscErrorCode PetscLogEventRegister(const char name[], PetscClassId classid, PetscLogEvent *event)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  *event = PETSC_DECIDE;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscEventRegLogGetEvent(stageLog->eventLog, name, event));
  if (*event > 0) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscEventRegLogRegister(stageLog->eventLog, name, classid, event));
  for (stage = 0; stage < stageLog->numStages; stage++) {
    PetscCall(PetscEventPerfLogEnsureSize(stageLog->stageInfo[stage].eventLog, stageLog->eventLog->numEvents));
    PetscCall(PetscClassPerfLogEnsureSize(stageLog->stageInfo[stage].classLog, stageLog->classLog->numClasses));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventSetCollective - Indicates that a particular event is collective.

  Not Collective

  Input Parameters:
+ event - The event id
- collective - Boolean flag indicating whether a particular event is collective

  Level: developer

  Notes:
  New events returned from `PetscLogEventRegister()` are collective by default.

  Collective events are handled specially if the -log_sync is used. In that case the logging saves information about
  two parts of the event; the time for all the MPI ranks to synchronize and then the time for the actual computation/communication
  to be performed. This option is useful to debug imbalance within the computations or communications

.seealso: [](ch_profiling), `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscLogEventRegister()`
@*/
PetscErrorCode PetscLogEventSetCollective(PetscLogEvent event, PetscBool collective)
{
  PetscStageLog    stageLog;
  PetscEventRegLog eventRegLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetEventRegLog(stageLog, &eventRegLog));
  PetscCheck(event >= 0 && event <= eventRegLog->numEvents, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Invalid event id");
  eventRegLog->eventInfo[event].collective = collective;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventIncludeClass - Activates event logging for a PETSc object class in every stage.

  Not Collective

  Input Parameter:
. classid - The object class, for example `MAT_CLASSID`, `SNES_CLASSID`, etc.

  Level: developer

.seealso: [](ch_profiling), `PetscLogEventActivateClass()`, `PetscLogEventDeactivateClass()`, `PetscLogEventActivate()`, `PetscLogEventDeactivate()`
@*/
PetscErrorCode PetscLogEventIncludeClass(PetscClassId classid)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  for (stage = 0; stage < stageLog->numStages; stage++) PetscCall(PetscEventPerfLogActivateClass(stageLog->stageInfo[stage].eventLog, stageLog->eventLog, classid));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventExcludeClass - Deactivates event logging for a PETSc object class in every stage.

  Not Collective

  Input Parameter:
. classid - The object class, for example `MAT_CLASSID`, `SNES_CLASSID`, etc.

  Level: developer

  Note:
  If a class is excluded then events associated with that class are not logged.

.seealso: [](ch_profiling), `PetscLogEventDeactivateClass()`, `PetscLogEventActivateClass()`, `PetscLogEventDeactivate()`, `PetscLogEventActivate()`
@*/
PetscErrorCode PetscLogEventExcludeClass(PetscClassId classid)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  for (stage = 0; stage < stageLog->numStages; stage++) PetscCall(PetscEventPerfLogDeactivateClass(stageLog->stageInfo[stage].eventLog, stageLog->eventLog, classid));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventActivate - Indicates that a particular event should be logged.

  Not Collective

  Input Parameter:
. event - The event id

  Usage:
.vb
      PetscLogEventDeactivate(VEC_SetValues);
        [code where you do not want to log VecSetValues()]
      PetscLogEventActivate(VEC_SetValues);
        [code where you do want to log VecSetValues()]
.ve

  Level: advanced

  Note:
  The event may be either a pre-defined PETSc event (found in include/petsclog.h)
  or an event number obtained with `PetscLogEventRegister()`.

.seealso: [](ch_profiling), `PlogEventDeactivate()`, `PlogEventDeactivatePush()`, `PetscLogEventDeactivatePop()`
@*/
PetscErrorCode PetscLogEventActivate(PetscLogEvent event)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  PetscCall(PetscEventPerfLogActivate(stageLog->stageInfo[stage].eventLog, event));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventDeactivate - Indicates that a particular event should not be logged.

  Not Collective

  Input Parameter:
. event - The event id

  Usage:
.vb
      PetscLogEventDeactivate(VEC_SetValues);
        [code where you do not want to log VecSetValues()]
      PetscLogEventActivate(VEC_SetValues);
        [code where you do want to log VecSetValues()]
.ve

  Level: advanced

  Note:
  The event may be either a pre-defined PETSc event (found in
  include/petsclog.h) or an event number obtained with `PetscLogEventRegister()`).

.seealso: [](ch_profiling), `PetscLogEventActivate()`, `PetscLogEventDeactivatePush()`, `PetscLogEventDeactivatePop()`
@*/
PetscErrorCode PetscLogEventDeactivate(PetscLogEvent event)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  PetscCall(PetscEventPerfLogDeactivate(stageLog->stageInfo[stage].eventLog, event));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventDeactivatePush - Indicates that a particular event should not be logged until `PetscLogEventDeactivatePop()` is called

  Not Collective

  Input Parameter:
. event - The event id

  Usage:
.vb
      PetscLogEventDeactivatePush(VEC_SetValues);
        [code where you do not want to log VecSetValues()]
      PetscLogEventDeactivatePop(VEC_SetValues);
        [code where you do want to log VecSetValues()]
.ve

  Level: advanced

  Note:
  The event may be either a pre-defined PETSc event (found in
  include/petsclog.h) or an event number obtained with `PetscLogEventRegister()`).

.seealso: [](ch_profiling), `PetscLogEventActivate()`, `PetscLogEventDeactivatePop()`, `PetscLogEventDeactivate()`
@*/
PetscErrorCode PetscLogEventDeactivatePush(PetscLogEvent event)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  PetscCall(PetscEventPerfLogDeactivatePush(stageLog->stageInfo[stage].eventLog, event));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventDeactivatePop - Indicates that a particular event should again be logged after the logging was turned off with `PetscLogEventDeactivatePush()`

  Not Collective

  Input Parameter:
. event - The event id

  Usage:
.vb
      PetscLogEventDeactivatePush(VEC_SetValues);
        [code where you do not want to log VecSetValues()]
      PetscLogEventDeactivatePop(VEC_SetValues);
        [code where you do want to log VecSetValues()]
.ve

  Level: advanced

  Note:
  The event may be either a pre-defined PETSc event (found in
  include/petsclog.h) or an event number obtained with `PetscLogEventRegister()`).

.seealso: [](ch_profiling), `PetscLogEventActivate()`, `PetscLogEventDeactivatePush()`
@*/
PetscErrorCode PetscLogEventDeactivatePop(PetscLogEvent event)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  PetscCall(PetscEventPerfLogDeactivatePop(stageLog->stageInfo[stage].eventLog, event));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventSetActiveAll - Turns on logging of all events

  Not Collective

  Input Parameters:
+ event    - The event id
- isActive - The activity flag determining whether the event is logged

  Level: advanced

.seealso: [](ch_profiling), `PlogEventActivate()`, `PlogEventDeactivate()`
@*/
PetscErrorCode PetscLogEventSetActiveAll(PetscLogEvent event, PetscBool isActive)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  for (stage = 0; stage < stageLog->numStages; stage++) {
    if (isActive) {
      PetscCall(PetscEventPerfLogActivate(stageLog->stageInfo[stage].eventLog, event));
    } else {
      PetscCall(PetscEventPerfLogDeactivate(stageLog->stageInfo[stage].eventLog, event));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventActivateClass - Activates event logging for a PETSc object class for the current stage

  Not Collective

  Input Parameter:
. classid - The event class, for example `MAT_CLASSID`, `SNES_CLASSID`, etc.

  Level: developer

.seealso: [](ch_profiling), `PetscLogEventIncludeClass()`, `PetscLogEventExcludeClass()`, `PetscLogEventDeactivateClass()`, `PetscLogEventActivate()`, `PetscLogEventDeactivate()`
@*/
PetscErrorCode PetscLogEventActivateClass(PetscClassId classid)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  PetscCall(PetscEventPerfLogActivateClass(stageLog->stageInfo[stage].eventLog, stageLog->eventLog, classid));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscLogEventDeactivateClass - Deactivates event logging for a PETSc object class for the current stage

  Not Collective

  Input Parameter:
. classid - The event class, for example `MAT_CLASSID`, `SNES_CLASSID`, etc.

  Level: developer

.seealso: [](ch_profiling), `PetscLogEventIncludeClass()`, `PetscLogEventExcludeClass()`, `PetscLogEventActivateClass()`, `PetscLogEventActivate()`, `PetscLogEventDeactivate()`
@*/
PetscErrorCode PetscLogEventDeactivateClass(PetscClassId classid)
{
  PetscStageLog stageLog;
  int           stage;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  PetscCall(PetscEventPerfLogDeactivateClass(stageLog->stageInfo[stage].eventLog, stageLog->eventLog, classid));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscLogEventSync - Synchronizes the beginning of a user event.

   Synopsis:
   #include <petsclog.h>
   PetscErrorCode PetscLogEventSync(int e,MPI_Comm comm)

   Collective

   Input Parameters:
+  e - integer associated with the event obtained from PetscLogEventRegister()
-  comm - an MPI communicator

   Usage:
.vb
     PetscLogEvent USER_EVENT;
     PetscLogEventRegister("User event",0,&USER_EVENT);
     PetscLogEventSync(USER_EVENT,PETSC_COMM_WORLD);
     PetscLogEventBegin(USER_EVENT,0,0,0,0);
        [code segment to monitor]
     PetscLogEventEnd(USER_EVENT,0,0,0,0);
.ve

   Level: developer

   Note:
   This routine should be called only if there is not a
   `PetscObject` available to pass to `PetscLogEventBegin()`.

.seealso: [](ch_profiling), `PetscLogEventRegister()`, `PetscLogEventBegin()`, `PetscLogEventEnd()`
M*/

/*MC
   PetscLogEventBegin - Logs the beginning of a user event.

   Synopsis:
   #include <petsclog.h>
   PetscErrorCode PetscLogEventBegin(int e,PetscObject o1,PetscObject o2,PetscObject o3,PetscObject o4)

   Not Collective

   Input Parameters:
+  e - integer associated with the event obtained from PetscLogEventRegister()
-  o1,o2,o3,o4 - objects associated with the event, or 0

   Fortran Synopsis:
   void PetscLogEventBegin(int e,PetscErrorCode ierr)

   Usage:
.vb
     PetscLogEvent USER_EVENT;
     PetscLogDouble user_event_flops;
     PetscLogEventRegister("User event",0,&USER_EVENT);
     PetscLogEventBegin(USER_EVENT,0,0,0,0);
        [code segment to monitor]
        PetscLogFlops(user_event_flops);
     PetscLogEventEnd(USER_EVENT,0,0,0,0);
.ve

   Level: intermediate

   Developer Note:
     `PetscLogEventBegin()` and `PetscLogEventBegin()` return error codes instead of explicitly handling the
     errors that occur in the macro directly because other packages that use this macros have used them in their
     own functions or methods that do not return error codes and it would be disruptive to change the current
     behavior.

.seealso: [](ch_profiling), `PetscLogEventRegister()`, `PetscLogEventEnd()`, `PetscLogFlops()`
M*/

/*MC
   PetscLogEventEnd - Log the end of a user event.

   Synopsis:
   #include <petsclog.h>
   PetscErrorCode PetscLogEventEnd(int e,PetscObject o1,PetscObject o2,PetscObject o3,PetscObject o4)

   Not Collective

   Input Parameters:
+  e - integer associated with the event obtained with PetscLogEventRegister()
-  o1,o2,o3,o4 - objects associated with the event, or 0

   Fortran Synopsis:
   void PetscLogEventEnd(int e,PetscErrorCode ierr)

   Usage:
.vb
     PetscLogEvent USER_EVENT;
     PetscLogDouble user_event_flops;
     PetscLogEventRegister("User event",0,&USER_EVENT,);
     PetscLogEventBegin(USER_EVENT,0,0,0,0);
        [code segment to monitor]
        PetscLogFlops(user_event_flops);
     PetscLogEventEnd(USER_EVENT,0,0,0,0);
.ve

   Level: intermediate

.seealso: [](ch_profiling), `PetscLogEventRegister()`, `PetscLogEventBegin()`, `PetscLogFlops()`
M*/

/*@C
  PetscLogEventGetId - Returns the event id when given the event name.

  Not Collective

  Input Parameter:
. name  - The event name

  Output Parameter:
. event - The event, or -1 if no event with that name exists

  Level: intermediate

.seealso: [](ch_profiling), `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscLogStageGetId()`
@*/
PetscErrorCode PetscLogEventGetId(const char name[], PetscLogEvent *event)
{
  PetscStageLog stageLog;

  PetscFunctionBegin;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscEventRegLogGetEvent(stageLog->eventLog, name, event));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscLogPushCurrentEvent_Internal(PetscLogEvent event)
{
  PetscFunctionBegin;
  if (!PetscDefined(HAVE_THREADSAFETY)) PetscCall(PetscIntStackPush(current_log_event_stack, event));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscLogPopCurrentEvent_Internal(void)
{
  PetscFunctionBegin;
  if (!PetscDefined(HAVE_THREADSAFETY)) PetscCall(PetscIntStackPop(current_log_event_stack, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscLogGetCurrentEvent_Internal(PetscLogEvent *event)
{
  PetscBool empty;

  PetscFunctionBegin;
  PetscValidIntPointer(event, 1);
  *event = PETSC_DECIDE;
  PetscCall(PetscIntStackEmpty(current_log_event_stack, &empty));
  if (!empty) PetscCall(PetscIntStackTop(current_log_event_stack, event));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscLogEventPause_Internal(PetscLogEvent event)
{
  PetscFunctionBegin;
  if (event != PETSC_DECIDE) PetscCall(PetscLogEventEnd(event, NULL, NULL, NULL, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscLogEventResume_Internal(PetscLogEvent event)
{
  PetscStageLog     stageLog;
  PetscEventPerfLog eventLog;
  int               stage;

  PetscFunctionBegin;
  if (event == PETSC_DECIDE) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscLogEventBegin(event, NULL, NULL, NULL, NULL));
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  PetscCall(PetscStageLogGetEventPerfLog(stageLog, stage, &eventLog));
  eventLog->eventInfo[event].count--;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*------------------------------------------------ Output Functions -------------------------------------------------*/
/*@C
  PetscLogDump - Dumps logs of objects to a file. This file is intended to
  be read by bin/petscview. This program no longer exists.

  Collective on `PETSC_COMM_WORLD`

  Input Parameter:
. name - an optional file name

  Usage:
.vb
     PetscInitialize(...);
     PetscLogDefaultBegin(); or PetscLogAllBegin();
     ... code ...
     PetscLogDump(filename);
     PetscFinalize();
.ve

  Level: advanced

  Note:
  The default file name is
$    Log.<rank>
  where <rank> is the processor number. If no name is specified,
  this file will be used.

.seealso: [](ch_profiling), `PetscLogDefaultBegin()`, `PetscLogAllBegin()`, `PetscLogView()`
@*/
PetscErrorCode PetscLogDump(const char sname[])
{
  PetscStageLog       stageLog;
  PetscEventPerfInfo *eventInfo;
  FILE               *fd;
  char                file[PETSC_MAX_PATH_LEN], fname[PETSC_MAX_PATH_LEN];
  PetscLogDouble      flops, _TotalTime;
  PetscMPIInt         rank;
  int                 action, object, curStage;
  PetscLogEvent       event;

  PetscFunctionBegin;
  /* Calculate the total elapsed time */
  PetscCall(PetscTime(&_TotalTime));
  _TotalTime -= petsc_BaseTime;
  /* Open log file */
  PetscCallMPI(MPI_Comm_rank(PETSC_COMM_WORLD, &rank));
  PetscCall(PetscSNPrintf(file, PETSC_STATIC_ARRAY_LENGTH(file), "%s.%d", sname && sname[0] ? sname : "Log", rank));
  PetscCall(PetscFixFilename(file, fname));
  PetscCall(PetscFOpen(PETSC_COMM_WORLD, fname, "w", &fd));
  PetscCheck(!(rank == 0) || !(!fd), PETSC_COMM_SELF, PETSC_ERR_FILE_OPEN, "Cannot open file: %s", fname);
  /* Output totals */
  PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "Total Flop %14e %16.8e\n", petsc_TotalFlops, _TotalTime));
  PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "Clock Resolution %g\n", 0.0));
  /* Output actions */
  if (petsc_logActions) {
    PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "Actions accomplished %d\n", petsc_numActions));
    for (action = 0; action < petsc_numActions; action++) {
      PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "%g %d %d %d %d %d %d %g %g %g\n", petsc_actions[action].time, petsc_actions[action].action, (int)petsc_actions[action].event, (int)petsc_actions[action].classid, petsc_actions[action].id1,
                             petsc_actions[action].id2, petsc_actions[action].id3, petsc_actions[action].flops, petsc_actions[action].mem, petsc_actions[action].maxmem));
    }
  }
  /* Output objects */
  if (petsc_logObjects) {
    PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "Objects created %d destroyed %d\n", petsc_numObjects, petsc_numObjectsDestroyed));
    for (object = 0; object < petsc_numObjects; object++) {
      PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "Parent ID: %d Memory: %d\n", petsc_objects[object].parent, (int)petsc_objects[object].mem));
      if (!petsc_objects[object].name[0]) {
        PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "No Name\n"));
      } else {
        PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "Name: %s\n", petsc_objects[object].name));
      }
      if (petsc_objects[object].info[0] != 0) {
        PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "No Info\n"));
      } else {
        PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "Info: %s\n", petsc_objects[object].info));
      }
    }
  }
  /* Output events */
  PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "Event log:\n"));
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscIntStackTop(stageLog->stack, &curStage));
  eventInfo = stageLog->stageInfo[curStage].eventLog->eventInfo;
  for (event = 0; event < stageLog->stageInfo[curStage].eventLog->numEvents; event++) {
    if (eventInfo[event].time != 0.0) flops = eventInfo[event].flops / eventInfo[event].time;
    else flops = 0.0;
    PetscCall(PetscFPrintf(PETSC_COMM_WORLD, fd, "%d %16d %16g %16g %16g\n", event, eventInfo[event].count, eventInfo[event].flops, eventInfo[event].time, flops));
  }
  PetscCall(PetscFClose(PETSC_COMM_WORLD, fd));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  PetscLogView_Detailed - Each process prints the times for its own events

*/
PetscErrorCode PetscLogView_Detailed(PetscViewer viewer)
{
  PetscStageLog       stageLog;
  PetscEventPerfInfo *eventInfo = NULL, *stageInfo = NULL;
  PetscLogDouble      locTotalTime, numRed, maxMem;
  int                 numStages, numEvents, stage, event;
  MPI_Comm            comm = PetscObjectComm((PetscObject)viewer);
  PetscMPIInt         rank, size;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  /* Must preserve reduction count before we go on */
  numRed = petsc_allreduce_ct + petsc_gather_ct + petsc_scatter_ct;
  /* Get the total elapsed time */
  PetscCall(PetscTime(&locTotalTime));
  locTotalTime -= petsc_BaseTime;
  PetscCall(PetscViewerASCIIPrintf(viewer, "size = %d\n", size));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LocalTimes = {}\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LocalMessages = {}\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LocalMessageLens = {}\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LocalReductions = {}\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LocalFlop = {}\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LocalObjects = {}\n"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "LocalMemory = {}\n"));
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCallMPI(MPI_Allreduce(&stageLog->numStages, &numStages, 1, MPI_INT, MPI_MAX, comm));
  PetscCall(PetscViewerASCIIPrintf(viewer, "Stages = {}\n"));
  for (stage = 0; stage < numStages; stage++) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "Stages[\"%s\"] = {}\n", stageLog->stageInfo[stage].name));
    PetscCall(PetscViewerASCIIPrintf(viewer, "Stages[\"%s\"][\"summary\"] = {}\n", stageLog->stageInfo[stage].name));
    PetscCallMPI(MPI_Allreduce(&stageLog->stageInfo[stage].eventLog->numEvents, &numEvents, 1, MPI_INT, MPI_MAX, comm));
    for (event = 0; event < numEvents; event++) PetscCall(PetscViewerASCIIPrintf(viewer, "Stages[\"%s\"][\"%s\"] = {}\n", stageLog->stageInfo[stage].name, stageLog->eventLog->eventInfo[event].name));
  }
  PetscCall(PetscMallocGetMaximumUsage(&maxMem));
  PetscCall(PetscViewerASCIIPushSynchronized(viewer));
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "LocalTimes[%d] = %g\n", rank, locTotalTime));
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "LocalMessages[%d] = %g\n", rank, (petsc_irecv_ct + petsc_isend_ct + petsc_recv_ct + petsc_send_ct)));
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "LocalMessageLens[%d] = %g\n", rank, (petsc_irecv_len + petsc_isend_len + petsc_recv_len + petsc_send_len)));
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "LocalReductions[%d] = %g\n", rank, numRed));
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "LocalFlop[%d] = %g\n", rank, petsc_TotalFlops));
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "LocalObjects[%d] = %d\n", rank, petsc_numObjects));
  PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "LocalMemory[%d] = %g\n", rank, maxMem));
  PetscCall(PetscViewerFlush(viewer));
  for (stage = 0; stage < numStages; stage++) {
    stageInfo = &stageLog->stageInfo[stage].perfInfo;
    PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "Stages[\"%s\"][\"summary\"][%d] = {\"time\" : %g, \"numMessages\" : %g, \"messageLength\" : %g, \"numReductions\" : %g, \"flop\" : %g}\n", stageLog->stageInfo[stage].name, rank, stageInfo->time,
                                                 stageInfo->numMessages, stageInfo->messageLength, stageInfo->numReductions, stageInfo->flops));
    PetscCallMPI(MPI_Allreduce(&stageLog->stageInfo[stage].eventLog->numEvents, &numEvents, 1, MPI_INT, MPI_MAX, comm));
    for (event = 0; event < numEvents; event++) {
      eventInfo = &stageLog->stageInfo[stage].eventLog->eventInfo[event];
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "Stages[\"%s\"][\"%s\"][%d] = {\"count\" : %d, \"time\" : %g, \"syncTime\" : %g, \"numMessages\" : %g, \"messageLength\" : %g, \"numReductions\" : %g, \"flop\" : %g",
                                                   stageLog->stageInfo[stage].name, stageLog->eventLog->eventInfo[event].name, rank, eventInfo->count, eventInfo->time, eventInfo->syncTime, eventInfo->numMessages, eventInfo->messageLength, eventInfo->numReductions,
                                                   eventInfo->flops));
      if (eventInfo->dof[0] >= 0.) {
        PetscInt d, e;

        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, ", \"dof\" : ["));
        for (d = 0; d < 8; ++d) {
          if (d > 0) PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, ", "));
          PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%g", eventInfo->dof[d]));
        }
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "]"));
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, ", \"error\" : ["));
        for (e = 0; e < 8; ++e) {
          if (e > 0) PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, ", "));
          PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%g", eventInfo->errors[e]));
        }
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "]"));
      }
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "}\n"));
    }
  }
  PetscCall(PetscViewerFlush(viewer));
  PetscCall(PetscViewerASCIIPopSynchronized(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  PetscLogView_CSV - Each process prints the times for its own events in Comma-Separated Value Format
*/
PetscErrorCode PetscLogView_CSV(PetscViewer viewer)
{
  PetscStageLog       stageLog;
  PetscEventPerfInfo *eventInfo = NULL;
  PetscLogDouble      locTotalTime, maxMem;
  int                 numStages, numEvents, stage, event;
  MPI_Comm            comm = PetscObjectComm((PetscObject)viewer);
  PetscMPIInt         rank, size;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  /* Must preserve reduction count before we go on */
  /* Get the total elapsed time */
  PetscCall(PetscTime(&locTotalTime));
  locTotalTime -= petsc_BaseTime;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCallMPI(MPI_Allreduce(&stageLog->numStages, &numStages, 1, MPI_INT, MPI_MAX, comm));
  PetscCall(PetscMallocGetMaximumUsage(&maxMem));
  PetscCall(PetscViewerASCIIPushSynchronized(viewer));
  PetscCall(PetscViewerASCIIPrintf(viewer, "Stage Name,Event Name,Rank,Count,Time,Num Messages,Message Length,Num Reductions,FLOP,dof0,dof1,dof2,dof3,dof4,dof5,dof6,dof7,e0,e1,e2,e3,e4,e5,e6,e7,%d\n", size));
  PetscCall(PetscViewerFlush(viewer));
  for (stage = 0; stage < numStages; stage++) {
    PetscEventPerfInfo *stageInfo = &stageLog->stageInfo[stage].perfInfo;

    PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%s,summary,%d,1,%g,%g,%g,%g,%g\n", stageLog->stageInfo[stage].name, rank, stageInfo->time, stageInfo->numMessages, stageInfo->messageLength, stageInfo->numReductions, stageInfo->flops));
    PetscCallMPI(MPI_Allreduce(&stageLog->stageInfo[stage].eventLog->numEvents, &numEvents, 1, MPI_INT, MPI_MAX, comm));
    for (event = 0; event < numEvents; event++) {
      eventInfo = &stageLog->stageInfo[stage].eventLog->eventInfo[event];
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%s,%s,%d,%d,%g,%g,%g,%g,%g", stageLog->stageInfo[stage].name, stageLog->eventLog->eventInfo[event].name, rank, eventInfo->count, eventInfo->time, eventInfo->numMessages, eventInfo->messageLength,
                                                   eventInfo->numReductions, eventInfo->flops));
      if (eventInfo->dof[0] >= 0.) {
        PetscInt d, e;

        for (d = 0; d < 8; ++d) PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, ",%g", eventInfo->dof[d]));
        for (e = 0; e < 8; ++e) PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, ",%g", eventInfo->errors[e]));
      }
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "\n"));
    }
  }
  PetscCall(PetscViewerFlush(viewer));
  PetscCall(PetscViewerASCIIPopSynchronized(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscLogViewWarnSync(MPI_Comm comm, FILE *fd)
{
  PetscFunctionBegin;
  if (!PetscLogSyncOn) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscFPrintf(comm, fd, "\n\n"));
  PetscCall(PetscFPrintf(comm, fd, "      ##########################################################\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                       WARNING!!!                       #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   This program was run with logging synchronization.   #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   This option provides more meaningful imbalance       #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   figures at the expense of slowing things down and    #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   providing a distorted view of the overall runtime.   #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      ##########################################################\n\n\n"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscLogViewWarnDebugging(MPI_Comm comm, FILE *fd)
{
  PetscFunctionBegin;
  if (PetscDefined(USE_DEBUG)) {
    PetscCall(PetscFPrintf(comm, fd, "\n\n"));
    PetscCall(PetscFPrintf(comm, fd, "      ##########################################################\n"));
    PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
    PetscCall(PetscFPrintf(comm, fd, "      #                       WARNING!!!                       #\n"));
    PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
    PetscCall(PetscFPrintf(comm, fd, "      #   This code was compiled with a debugging option.      #\n"));
    PetscCall(PetscFPrintf(comm, fd, "      #   To get timing results run ./configure                #\n"));
    PetscCall(PetscFPrintf(comm, fd, "      #   using --with-debugging=no, the performance will      #\n"));
    PetscCall(PetscFPrintf(comm, fd, "      #   be generally two or three times faster.              #\n"));
    PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
    PetscCall(PetscFPrintf(comm, fd, "      ##########################################################\n\n\n"));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscLogViewWarnNoGpuAwareMpi(MPI_Comm comm, FILE *fd)
{
  #if defined(PETSC_HAVE_DEVICE)
  PetscMPIInt size;
  PetscBool   deviceInitialized = PETSC_FALSE;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(comm, &size));
  for (int i = PETSC_DEVICE_HOST + 1; i < PETSC_DEVICE_MAX; ++i) {
    const PetscDeviceType dtype = PetscDeviceTypeCast(i);
    if (PetscDeviceInitialized(dtype)) { /* a non-host device was initialized */
      deviceInitialized = PETSC_TRUE;
      break;
    }
  }
  /* the last condition says petsc is configured with device but it is a pure CPU run, so don't print misleading warnings */
  if (use_gpu_aware_mpi || size == 1 || !deviceInitialized) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscFPrintf(comm, fd, "\n\n"));
  PetscCall(PetscFPrintf(comm, fd, "      ##########################################################\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                       WARNING!!!                       #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   This code was compiled with GPU support and you've   #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   created PETSc/GPU objects, but you intentionally     #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   used -use_gpu_aware_mpi 0, requiring PETSc to copy   #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   additional data between the GPU and CPU. To obtain   #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   meaningful timing results on multi-rank runs, use    #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   GPU-aware MPI instead.                               #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      ##########################################################\n\n\n"));
  PetscFunctionReturn(PETSC_SUCCESS);
  #else
  return PETSC_SUCCESS;
  #endif
}

static PetscErrorCode PetscLogViewWarnGpuTime(MPI_Comm comm, FILE *fd)
{
  #if defined(PETSC_HAVE_DEVICE)

  PetscFunctionBegin;
  if (!PetscLogGpuTimeFlag || petsc_gflops == 0) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscFPrintf(comm, fd, "\n\n"));
  PetscCall(PetscFPrintf(comm, fd, "      ##########################################################\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                       WARNING!!!                       #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   This code was run with -log_view_gpu_time            #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   This provides accurate timing within the GPU kernels #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   but can slow down the entire computation by a        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   measurable amount. For fastest runs we recommend     #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #   not using this option.                               #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      #                                                        #\n"));
  PetscCall(PetscFPrintf(comm, fd, "      ##########################################################\n\n\n"));
  PetscFunctionReturn(PETSC_SUCCESS);
  #else
  return PETSC_SUCCESS;
  #endif
}

PetscErrorCode PetscLogView_Default(PetscViewer viewer)
{
  FILE               *fd;
  PetscLogDouble      zero = 0.0;
  PetscStageLog       stageLog;
  PetscStageInfo     *stageInfo = NULL;
  PetscEventPerfInfo *eventInfo = NULL;
  PetscClassPerfInfo *classInfo;
  char                arch[128], hostname[128], username[128], pname[PETSC_MAX_PATH_LEN], date[128];
  const char         *name;
  PetscLogDouble      locTotalTime, TotalTime, TotalFlops;
  PetscLogDouble      numMessages, messageLength, avgMessLen, numReductions;
  PetscLogDouble      stageTime, flops, flopr, mem, mess, messLen, red;
  PetscLogDouble      fracTime, fracFlops, fracMessages, fracLength, fracReductions, fracMess, fracMessLen, fracRed;
  PetscLogDouble      fracStageTime, fracStageFlops, fracStageMess, fracStageMessLen, fracStageRed;
  PetscLogDouble      min, max, tot, ratio, avg, x, y;
  PetscLogDouble      minf, maxf, totf, ratf, mint, maxt, tott, ratt, ratC, totm, totml, totr, mal, malmax, emalmax;
  #if defined(PETSC_HAVE_DEVICE)
  PetscLogEvent  KSP_Solve, SNES_Solve, TS_Step, TAO_Solve; /* These need to be fixed to be some events registered with certain objects */
  PetscLogDouble cct, gct, csz, gsz, gmaxt, gflops, gflopr, fracgflops;
  #endif
  PetscMPIInt   minC, maxC;
  PetscMPIInt   size, rank;
  PetscBool    *localStageUsed, *stageUsed;
  PetscBool    *localStageVisible, *stageVisible;
  int           numStages, localNumEvents, numEvents;
  int           stage, oclass;
  PetscLogEvent event;
  char          version[256];
  MPI_Comm      comm;
  #if defined(PETSC_HAVE_DEVICE)
  PetscLogEvent eventid;
  PetscInt64    nas = 0x7FF0000000000002;
  #endif

  PetscFunctionBegin;
  PetscCall(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
  PetscCall(PetscObjectGetComm((PetscObject)viewer, &comm));
  PetscCall(PetscViewerASCIIGetPointer(viewer, &fd));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  /* Get the total elapsed time */
  PetscCall(PetscTime(&locTotalTime));
  locTotalTime -= petsc_BaseTime;

  PetscCall(PetscFPrintf(comm, fd, "****************************************************************************************************************************************************************\n"));
  PetscCall(PetscFPrintf(comm, fd, "***                                WIDEN YOUR WINDOW TO 160 CHARACTERS.  Use 'enscript -r -fCourier9' to print this document                                 ***\n"));
  PetscCall(PetscFPrintf(comm, fd, "****************************************************************************************************************************************************************\n"));
  PetscCall(PetscFPrintf(comm, fd, "\n------------------------------------------------------------------ PETSc Performance Summary: ------------------------------------------------------------------\n\n"));
  PetscCall(PetscLogViewWarnSync(comm, fd));
  PetscCall(PetscLogViewWarnDebugging(comm, fd));
  PetscCall(PetscLogViewWarnNoGpuAwareMpi(comm, fd));
  PetscCall(PetscLogViewWarnGpuTime(comm, fd));
  PetscCall(PetscGetArchType(arch, sizeof(arch)));
  PetscCall(PetscGetHostName(hostname, sizeof(hostname)));
  PetscCall(PetscGetUserName(username, sizeof(username)));
  PetscCall(PetscGetProgramName(pname, sizeof(pname)));
  PetscCall(PetscGetDate(date, sizeof(date)));
  PetscCall(PetscGetVersion(version, sizeof(version)));
  if (size == 1) {
    PetscCall(PetscFPrintf(comm, fd, "%s on a %s named %s with %d processor, by %s %s\n", pname, arch, hostname, size, username, date));
  } else {
    PetscCall(PetscFPrintf(comm, fd, "%s on a %s named %s with %d processors, by %s %s\n", pname, arch, hostname, size, username, date));
  }
  #if defined(PETSC_HAVE_OPENMP)
  PetscCall(PetscFPrintf(comm, fd, "Using %" PetscInt_FMT " OpenMP threads\n", PetscNumOMPThreads));
  #endif
  PetscCall(PetscFPrintf(comm, fd, "Using %s\n", version));

  /* Must preserve reduction count before we go on */
  red = petsc_allreduce_ct + petsc_gather_ct + petsc_scatter_ct;

  /* Calculate summary information */
  PetscCall(PetscFPrintf(comm, fd, "\n                         Max       Max/Min     Avg       Total\n"));
  /*   Time */
  PetscCall(MPIU_Allreduce(&locTotalTime, &min, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
  PetscCall(MPIU_Allreduce(&locTotalTime, &max, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  PetscCall(MPIU_Allreduce(&locTotalTime, &tot, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
  avg = tot / ((PetscLogDouble)size);
  if (min != 0.0) ratio = max / min;
  else ratio = 0.0;
  PetscCall(PetscFPrintf(comm, fd, "Time (sec):           %5.3e   %7.3f   %5.3e\n", max, ratio, avg));
  TotalTime = tot;
  /*   Objects */
  avg = (PetscLogDouble)petsc_numObjects;
  PetscCall(MPIU_Allreduce(&avg, &min, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
  PetscCall(MPIU_Allreduce(&avg, &max, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  PetscCall(MPIU_Allreduce(&avg, &tot, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
  avg = tot / ((PetscLogDouble)size);
  if (min != 0.0) ratio = max / min;
  else ratio = 0.0;
  PetscCall(PetscFPrintf(comm, fd, "Objects:              %5.3e   %7.3f   %5.3e\n", max, ratio, avg));
  /*   Flops */
  PetscCall(MPIU_Allreduce(&petsc_TotalFlops, &min, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
  PetscCall(MPIU_Allreduce(&petsc_TotalFlops, &max, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  PetscCall(MPIU_Allreduce(&petsc_TotalFlops, &tot, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
  avg = tot / ((PetscLogDouble)size);
  if (min != 0.0) ratio = max / min;
  else ratio = 0.0;
  PetscCall(PetscFPrintf(comm, fd, "Flops:                %5.3e   %7.3f   %5.3e  %5.3e\n", max, ratio, avg, tot));
  TotalFlops = tot;
  /*   Flops/sec -- Must talk to Barry here */
  if (locTotalTime != 0.0) flops = petsc_TotalFlops / locTotalTime;
  else flops = 0.0;
  PetscCall(MPIU_Allreduce(&flops, &min, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
  PetscCall(MPIU_Allreduce(&flops, &max, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  PetscCall(MPIU_Allreduce(&flops, &tot, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
  avg = tot / ((PetscLogDouble)size);
  if (min != 0.0) ratio = max / min;
  else ratio = 0.0;
  PetscCall(PetscFPrintf(comm, fd, "Flops/sec:            %5.3e   %7.3f   %5.3e  %5.3e\n", max, ratio, avg, tot));
  /*   Memory */
  PetscCall(PetscMallocGetMaximumUsage(&mem));
  if (mem > 0.0) {
    PetscCall(MPIU_Allreduce(&mem, &min, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
    PetscCall(MPIU_Allreduce(&mem, &max, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
    PetscCall(MPIU_Allreduce(&mem, &tot, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
    avg = tot / ((PetscLogDouble)size);
    if (min != 0.0) ratio = max / min;
    else ratio = 0.0;
    PetscCall(PetscFPrintf(comm, fd, "Memory (bytes):       %5.3e   %7.3f   %5.3e  %5.3e\n", max, ratio, avg, tot));
  }
  /*   Messages */
  mess = 0.5 * (petsc_irecv_ct + petsc_isend_ct + petsc_recv_ct + petsc_send_ct);
  PetscCall(MPIU_Allreduce(&mess, &min, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
  PetscCall(MPIU_Allreduce(&mess, &max, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  PetscCall(MPIU_Allreduce(&mess, &tot, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
  avg = tot / ((PetscLogDouble)size);
  if (min != 0.0) ratio = max / min;
  else ratio = 0.0;
  PetscCall(PetscFPrintf(comm, fd, "MPI Msg Count:        %5.3e   %7.3f   %5.3e  %5.3e\n", max, ratio, avg, tot));
  numMessages = tot;
  /*   Message Lengths */
  mess = 0.5 * (petsc_irecv_len + petsc_isend_len + petsc_recv_len + petsc_send_len);
  PetscCall(MPIU_Allreduce(&mess, &min, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
  PetscCall(MPIU_Allreduce(&mess, &max, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  PetscCall(MPIU_Allreduce(&mess, &tot, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
  if (numMessages != 0) avg = tot / numMessages;
  else avg = 0.0;
  if (min != 0.0) ratio = max / min;
  else ratio = 0.0;
  PetscCall(PetscFPrintf(comm, fd, "MPI Msg Len (bytes):  %5.3e   %7.3f   %5.3e  %5.3e\n", max, ratio, avg, tot));
  messageLength = tot;
  /*   Reductions */
  PetscCall(MPIU_Allreduce(&red, &min, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
  PetscCall(MPIU_Allreduce(&red, &max, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  PetscCall(MPIU_Allreduce(&red, &tot, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
  if (min != 0.0) ratio = max / min;
  else ratio = 0.0;
  PetscCall(PetscFPrintf(comm, fd, "MPI Reductions:       %5.3e   %7.3f\n", max, ratio));
  numReductions = red; /* wrong because uses count from process zero */
  PetscCall(PetscFPrintf(comm, fd, "\nFlop counting convention: 1 flop = 1 real number operation of type (multiply/divide/add/subtract)\n"));
  PetscCall(PetscFPrintf(comm, fd, "                            e.g., VecAXPY() for real vectors of length N --> 2N flops\n"));
  PetscCall(PetscFPrintf(comm, fd, "                            and VecAXPY() for complex vectors of length N --> 8N flops\n"));

  /* Get total number of stages --
       Currently, a single processor can register more stages than another, but stages must all be registered in order.
       We can removed this requirement if necessary by having a global stage numbering and indirection on the stage ID.
       This seems best accomplished by assoicating a communicator with each stage.
  */
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCallMPI(MPI_Allreduce(&stageLog->numStages, &numStages, 1, MPI_INT, MPI_MAX, comm));
  PetscCall(PetscMalloc1(numStages, &localStageUsed));
  PetscCall(PetscMalloc1(numStages, &stageUsed));
  PetscCall(PetscMalloc1(numStages, &localStageVisible));
  PetscCall(PetscMalloc1(numStages, &stageVisible));
  if (numStages > 0) {
    stageInfo = stageLog->stageInfo;
    for (stage = 0; stage < numStages; stage++) {
      if (stage < stageLog->numStages) {
        localStageUsed[stage]    = stageInfo[stage].used;
        localStageVisible[stage] = stageInfo[stage].perfInfo.visible;
      } else {
        localStageUsed[stage]    = PETSC_FALSE;
        localStageVisible[stage] = PETSC_TRUE;
      }
    }
    PetscCall(MPIU_Allreduce(localStageUsed, stageUsed, numStages, MPIU_BOOL, MPI_LOR, comm));
    PetscCall(MPIU_Allreduce(localStageVisible, stageVisible, numStages, MPIU_BOOL, MPI_LAND, comm));
    for (stage = 0; stage < numStages; stage++) {
      if (stageUsed[stage]) {
        PetscCall(PetscFPrintf(comm, fd, "\nSummary of Stages:   ----- Time ------  ----- Flop ------  --- Messages ---  -- Message Lengths --  -- Reductions --\n"));
        PetscCall(PetscFPrintf(comm, fd, "                        Avg     %%Total     Avg     %%Total    Count   %%Total     Avg         %%Total    Count   %%Total\n"));
        break;
      }
    }
    for (stage = 0; stage < numStages; stage++) {
      if (!stageUsed[stage]) continue;
      /* CANNOT use MPI_Allreduce() since it might fail the line number check */
      if (localStageUsed[stage]) {
        PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.time, &stageTime, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.flops, &flops, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.numMessages, &mess, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.messageLength, &messLen, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.numReductions, &red, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        name = stageInfo[stage].name;
      } else {
        PetscCall(MPIU_Allreduce(&zero, &stageTime, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &flops, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &mess, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &messLen, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &red, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        name = "";
      }
      mess *= 0.5;
      messLen *= 0.5;
      red /= size;
      if (TotalTime != 0.0) fracTime = stageTime / TotalTime;
      else fracTime = 0.0;
      if (TotalFlops != 0.0) fracFlops = flops / TotalFlops;
      else fracFlops = 0.0;
      /* Talk to Barry if (stageTime     != 0.0) flops          = (size*flops)/stageTime; else flops          = 0.0; */
      if (numMessages != 0.0) fracMessages = mess / numMessages;
      else fracMessages = 0.0;
      if (mess != 0.0) avgMessLen = messLen / mess;
      else avgMessLen = 0.0;
      if (messageLength != 0.0) fracLength = messLen / messageLength;
      else fracLength = 0.0;
      if (numReductions != 0.0) fracReductions = red / numReductions;
      else fracReductions = 0.0;
      PetscCall(PetscFPrintf(comm, fd, "%2d: %15s: %6.4e %5.1f%%  %6.4e %5.1f%%  %5.3e %5.1f%%  %5.3e      %5.1f%%  %5.3e %5.1f%%\n", stage, name, stageTime / size, 100.0 * fracTime, flops, 100.0 * fracFlops, mess, 100.0 * fracMessages, avgMessLen, 100.0 * fracLength, red, 100.0 * fracReductions));
    }
  }

  PetscCall(PetscFPrintf(comm, fd, "\n------------------------------------------------------------------------------------------------------------------------\n"));
  PetscCall(PetscFPrintf(comm, fd, "See the 'Profiling' chapter of the users' manual for details on interpreting output.\n"));
  PetscCall(PetscFPrintf(comm, fd, "Phase summary info:\n"));
  PetscCall(PetscFPrintf(comm, fd, "   Count: number of times phase was executed\n"));
  PetscCall(PetscFPrintf(comm, fd, "   Time and Flop: Max - maximum over all processors\n"));
  PetscCall(PetscFPrintf(comm, fd, "                  Ratio - ratio of maximum to minimum over all processors\n"));
  PetscCall(PetscFPrintf(comm, fd, "   Mess: number of messages sent\n"));
  PetscCall(PetscFPrintf(comm, fd, "   AvgLen: average message length (bytes)\n"));
  PetscCall(PetscFPrintf(comm, fd, "   Reduct: number of global reductions\n"));
  PetscCall(PetscFPrintf(comm, fd, "   Global: entire computation\n"));
  PetscCall(PetscFPrintf(comm, fd, "   Stage: stages of a computation. Set stages with PetscLogStagePush() and PetscLogStagePop().\n"));
  PetscCall(PetscFPrintf(comm, fd, "      %%T - percent time in this phase         %%F - percent flop in this phase\n"));
  PetscCall(PetscFPrintf(comm, fd, "      %%M - percent messages in this phase     %%L - percent message lengths in this phase\n"));
  PetscCall(PetscFPrintf(comm, fd, "      %%R - percent reductions in this phase\n"));
  PetscCall(PetscFPrintf(comm, fd, "   Total Mflop/s: 10e-6 * (sum of flop over all processors)/(max time over all processors)\n"));
  if (PetscLogMemory) {
    PetscCall(PetscFPrintf(comm, fd, "   Malloc Mbytes: Memory allocated and kept during event (sum over all calls to event). May be negative\n"));
    PetscCall(PetscFPrintf(comm, fd, "   EMalloc Mbytes: extra memory allocated during event and then freed (maximum over all calls to events). Never negative\n"));
    PetscCall(PetscFPrintf(comm, fd, "   MMalloc Mbytes: Increase in high water mark of allocated memory (sum over all calls to event). Never negative\n"));
    PetscCall(PetscFPrintf(comm, fd, "   RMI Mbytes: Increase in resident memory (sum over all calls to event)\n"));
  }
  #if defined(PETSC_HAVE_DEVICE)
  PetscCall(PetscFPrintf(comm, fd, "   GPU Mflop/s: 10e-6 * (sum of flop on GPU over all processors)/(max GPU time over all processors)\n"));
  PetscCall(PetscFPrintf(comm, fd, "   CpuToGpu Count: total number of CPU to GPU copies per processor\n"));
  PetscCall(PetscFPrintf(comm, fd, "   CpuToGpu Size (Mbytes): 10e-6 * (total size of CPU to GPU copies per processor)\n"));
  PetscCall(PetscFPrintf(comm, fd, "   GpuToCpu Count: total number of GPU to CPU copies per processor\n"));
  PetscCall(PetscFPrintf(comm, fd, "   GpuToCpu Size (Mbytes): 10e-6 * (total size of GPU to CPU copies per processor)\n"));
  PetscCall(PetscFPrintf(comm, fd, "   GPU %%F: percent flops on GPU in this event\n"));
  #endif
  PetscCall(PetscFPrintf(comm, fd, "------------------------------------------------------------------------------------------------------------------------\n"));

  PetscCall(PetscLogViewWarnDebugging(comm, fd));

  /* Report events */
  PetscCall(PetscFPrintf(comm, fd, "Event                Count      Time (sec)     Flop                              --- Global ---  --- Stage ----  Total"));
  if (PetscLogMemory) PetscCall(PetscFPrintf(comm, fd, "  Malloc EMalloc MMalloc RMI"));
  #if defined(PETSC_HAVE_DEVICE)
  PetscCall(PetscFPrintf(comm, fd, "   GPU    - CpuToGpu -   - GpuToCpu - GPU"));
  #endif
  PetscCall(PetscFPrintf(comm, fd, "\n"));
  PetscCall(PetscFPrintf(comm, fd, "                   Max Ratio  Max     Ratio   Max  Ratio  Mess   AvgLen  Reduct  %%T %%F %%M %%L %%R  %%T %%F %%M %%L %%R Mflop/s"));
  if (PetscLogMemory) PetscCall(PetscFPrintf(comm, fd, " Mbytes Mbytes Mbytes Mbytes"));
  #if defined(PETSC_HAVE_DEVICE)
  PetscCall(PetscFPrintf(comm, fd, " Mflop/s Count   Size   Count   Size  %%F"));
  #endif
  PetscCall(PetscFPrintf(comm, fd, "\n"));
  PetscCall(PetscFPrintf(comm, fd, "------------------------------------------------------------------------------------------------------------------------"));
  if (PetscLogMemory) PetscCall(PetscFPrintf(comm, fd, "-----------------------------"));
  #if defined(PETSC_HAVE_DEVICE)
  PetscCall(PetscFPrintf(comm, fd, "---------------------------------------"));
  #endif
  PetscCall(PetscFPrintf(comm, fd, "\n"));

  #if defined(PETSC_HAVE_DEVICE)
  /* this indirect way of accessing these values is needed when PETSc is build with multiple libraries since the symbols are not in libpetscsys */
  PetscCall(PetscEventRegLogGetEvent(stageLog->eventLog, "TAOSolve", &TAO_Solve));
  PetscCall(PetscEventRegLogGetEvent(stageLog->eventLog, "TSStep", &TS_Step));
  PetscCall(PetscEventRegLogGetEvent(stageLog->eventLog, "SNESSolve", &SNES_Solve));
  PetscCall(PetscEventRegLogGetEvent(stageLog->eventLog, "KSPSolve", &KSP_Solve));
  #endif

  /* Problem: The stage name will not show up unless the stage executed on proc 1 */
  for (stage = 0; stage < numStages; stage++) {
    if (!stageVisible[stage]) continue;
    /* CANNOT use MPI_Allreduce() since it might fail the line number check */
    if (localStageUsed[stage]) {
      PetscCall(PetscFPrintf(comm, fd, "\n--- Event Stage %d: %s\n\n", stage, stageInfo[stage].name));
      PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.time, &stageTime, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
      PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.flops, &flops, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
      PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.numMessages, &mess, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
      PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.messageLength, &messLen, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
      PetscCall(MPIU_Allreduce(&stageInfo[stage].perfInfo.numReductions, &red, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
    } else {
      PetscCall(PetscFPrintf(comm, fd, "\n--- Event Stage %d: Unknown\n\n", stage));
      PetscCall(MPIU_Allreduce(&zero, &stageTime, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
      PetscCall(MPIU_Allreduce(&zero, &flops, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
      PetscCall(MPIU_Allreduce(&zero, &mess, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
      PetscCall(MPIU_Allreduce(&zero, &messLen, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
      PetscCall(MPIU_Allreduce(&zero, &red, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
    }
    mess *= 0.5;
    messLen *= 0.5;
    red /= size;

    /* Get total number of events in this stage --
       Currently, a single processor can register more events than another, but events must all be registered in order,
       just like stages. We can removed this requirement if necessary by having a global event numbering and indirection
       on the event ID. This seems best accomplished by associating a communicator with each stage.

       Problem: If the event did not happen on proc 1, its name will not be available.
       Problem: Event visibility is not implemented
    */
    if (localStageUsed[stage]) {
      eventInfo      = stageLog->stageInfo[stage].eventLog->eventInfo;
      localNumEvents = stageLog->stageInfo[stage].eventLog->numEvents;
    } else localNumEvents = 0;
    PetscCallMPI(MPI_Allreduce(&localNumEvents, &numEvents, 1, MPI_INT, MPI_MAX, comm));
    for (event = 0; event < numEvents; event++) {
      /* CANNOT use MPI_Allreduce() since it might fail the line number check */
      if (localStageUsed[stage] && (event < stageLog->stageInfo[stage].eventLog->numEvents) && (eventInfo[event].depth == 0)) {
        if ((eventInfo[event].count > 0) && (eventInfo[event].time > 0.0)) flopr = eventInfo[event].flops;
        else flopr = 0.0;
        PetscCall(MPIU_Allreduce(&flopr, &minf, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
        PetscCall(MPIU_Allreduce(&flopr, &maxf, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].flops, &totf, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].time, &mint, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].time, &maxt, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].time, &tott, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].numMessages, &totm, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].messageLength, &totml, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].numReductions, &totr, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCallMPI(MPI_Allreduce(&eventInfo[event].count, &minC, 1, MPI_INT, MPI_MIN, comm));
        PetscCallMPI(MPI_Allreduce(&eventInfo[event].count, &maxC, 1, MPI_INT, MPI_MAX, comm));
        if (PetscLogMemory) {
          PetscCall(MPIU_Allreduce(&eventInfo[event].memIncrease, &mem, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
          PetscCall(MPIU_Allreduce(&eventInfo[event].mallocSpace, &mal, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
          PetscCall(MPIU_Allreduce(&eventInfo[event].mallocIncrease, &malmax, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
          PetscCall(MPIU_Allreduce(&eventInfo[event].mallocIncreaseEvent, &emalmax, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        }
  #if defined(PETSC_HAVE_DEVICE)
        PetscCall(MPIU_Allreduce(&eventInfo[event].CpuToGpuCount, &cct, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].GpuToCpuCount, &gct, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].CpuToGpuSize, &csz, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].GpuToCpuSize, &gsz, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].GpuFlops, &gflops, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&eventInfo[event].GpuTime, &gmaxt, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  #endif
        name = stageLog->eventLog->eventInfo[event].name;
      } else {
        int ierr = 0;

        flopr = 0.0;
        PetscCall(MPIU_Allreduce(&flopr, &minf, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
        PetscCall(MPIU_Allreduce(&flopr, &maxf, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
        PetscCall(MPIU_Allreduce(&zero, &totf, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &mint, 1, MPIU_PETSCLOGDOUBLE, MPI_MIN, comm));
        PetscCall(MPIU_Allreduce(&zero, &maxt, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
        PetscCall(MPIU_Allreduce(&zero, &tott, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &totm, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &totml, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &totr, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCallMPI(MPI_Allreduce(&ierr, &minC, 1, MPI_INT, MPI_MIN, comm));
        PetscCallMPI(MPI_Allreduce(&ierr, &maxC, 1, MPI_INT, MPI_MAX, comm));
        if (PetscLogMemory) {
          PetscCall(MPIU_Allreduce(&zero, &mem, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
          PetscCall(MPIU_Allreduce(&zero, &mal, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
          PetscCall(MPIU_Allreduce(&zero, &malmax, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
          PetscCall(MPIU_Allreduce(&zero, &emalmax, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        }
  #if defined(PETSC_HAVE_DEVICE)
        PetscCall(MPIU_Allreduce(&zero, &cct, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &gct, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &csz, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &gsz, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &gflops, 1, MPIU_PETSCLOGDOUBLE, MPI_SUM, comm));
        PetscCall(MPIU_Allreduce(&zero, &gmaxt, 1, MPIU_PETSCLOGDOUBLE, MPI_MAX, comm));
  #endif
        name = "";
      }
      if (mint < 0.0) {
        PetscCall(PetscFPrintf(comm, fd, "WARNING!!! Minimum time %g over all processors for %s is negative! This happens\n on some machines whose times cannot handle too rapid calls.!\n artificially changing minimum to zero.\n", mint, name));
        mint = 0;
      }
      PetscCheck(minf >= 0.0, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Minimum flop %g over all processors for %s is negative! Not possible!", minf, name);
  /* Put NaN into the time for all events that may not be time accurately since they may happen asynchronously on the GPU */
  #if defined(PETSC_HAVE_DEVICE)
      if (!PetscLogGpuTimeFlag && petsc_gflops > 0) {
        memcpy(&gmaxt, &nas, sizeof(PetscLogDouble));
        PetscCall(PetscEventRegLogGetEvent(stageLog->eventLog, name, &eventid));
        if (eventid != SNES_Solve && eventid != KSP_Solve && eventid != TS_Step && eventid != TAO_Solve) {
          memcpy(&mint, &nas, sizeof(PetscLogDouble));
          memcpy(&maxt, &nas, sizeof(PetscLogDouble));
        }
      }
  #endif
      totm *= 0.5;
      totml *= 0.5;
      totr /= size;

      if (maxC != 0) {
        if (minC != 0) ratC = ((PetscLogDouble)maxC) / minC;
        else ratC = 0.0;
        if (mint != 0.0) ratt = maxt / mint;
        else ratt = 0.0;
        if (minf != 0.0) ratf = maxf / minf;
        else ratf = 0.0;
        if (TotalTime != 0.0) fracTime = tott / TotalTime;
        else fracTime = 0.0;
        if (TotalFlops != 0.0) fracFlops = totf / TotalFlops;
        else fracFlops = 0.0;
        if (stageTime != 0.0) fracStageTime = tott / stageTime;
        else fracStageTime = 0.0;
        if (flops != 0.0) fracStageFlops = totf / flops;
        else fracStageFlops = 0.0;
        if (numMessages != 0.0) fracMess = totm / numMessages;
        else fracMess = 0.0;
        if (messageLength != 0.0) fracMessLen = totml / messageLength;
        else fracMessLen = 0.0;
        if (numReductions != 0.0) fracRed = totr / numReductions;
        else fracRed = 0.0;
        if (mess != 0.0) fracStageMess = totm / mess;
        else fracStageMess = 0.0;
        if (messLen != 0.0) fracStageMessLen = totml / messLen;
        else fracStageMessLen = 0.0;
        if (red != 0.0) fracStageRed = totr / red;
        else fracStageRed = 0.0;
        if (totm != 0.0) totml /= totm;
        else totml = 0.0;
        if (maxt != 0.0) flopr = totf / maxt;
        else flopr = 0.0;
        if (fracStageTime > 1.0 || fracStageFlops > 1.0 || fracStageMess > 1.0 || fracStageMessLen > 1.0 || fracStageRed > 1.0)
          PetscCall(PetscFPrintf(comm, fd, "%-16s %7d %3.1f %5.4e %3.1f %3.2e %3.1f %2.1e %2.1e %2.1e %2.0f %2.0f %2.0f %2.0f %2.0f Multiple stages %5.0f", name, maxC, ratC, maxt, ratt, maxf, ratf, totm, totml, totr, 100.0 * fracTime, 100.0 * fracFlops, 100.0 * fracMess, 100.0 * fracMessLen, 100.0 * fracRed, PetscAbs(flopr) / 1.0e6));
        else
          PetscCall(PetscFPrintf(comm, fd, "%-16s %7d %3.1f %5.4e %3.1f %3.2e %3.1f %2.1e %2.1e %2.1e %2.0f %2.0f %2.0f %2.0f %2.0f %3.0f %2.0f %2.0f %2.0f %2.0f %5.0f", name, maxC, ratC, maxt, ratt, maxf, ratf, totm, totml, totr, 100.0 * fracTime, 100.0 * fracFlops, 100.0 * fracMess, 100.0 * fracMessLen, 100.0 * fracRed, 100.0 * fracStageTime, 100.0 * fracStageFlops, 100.0 * fracStageMess, 100.0 * fracStageMessLen, 100.0 * fracStageRed, PetscAbs(flopr) / 1.0e6));
        if (PetscLogMemory) PetscCall(PetscFPrintf(comm, fd, " %5.0f   %5.0f   %5.0f   %5.0f", mal / 1.0e6, emalmax / 1.0e6, malmax / 1.0e6, mem / 1.0e6));
  #if defined(PETSC_HAVE_DEVICE)
        if (totf != 0.0) fracgflops = gflops / totf;
        else fracgflops = 0.0;
        if (gmaxt != 0.0) gflopr = gflops / gmaxt;
        else gflopr = 0.0;
        PetscCall(PetscFPrintf(comm, fd, "   %5.0f   %4.0f %3.2e %4.0f %3.2e % 2.0f", PetscAbs(gflopr) / 1.0e6, cct / size, csz / (1.0e6 * size), gct / size, gsz / (1.0e6 * size), 100.0 * fracgflops));
  #endif
        PetscCall(PetscFPrintf(comm, fd, "\n"));
      }
    }
  }

  /* Memory usage and object creation */
  PetscCall(PetscFPrintf(comm, fd, "------------------------------------------------------------------------------------------------------------------------"));
  if (PetscLogMemory) PetscCall(PetscFPrintf(comm, fd, "-----------------------------"));
  #if defined(PETSC_HAVE_DEVICE)
  PetscCall(PetscFPrintf(comm, fd, "---------------------------------------"));
  #endif
  PetscCall(PetscFPrintf(comm, fd, "\n"));
  PetscCall(PetscFPrintf(comm, fd, "\n"));

  /* Right now, only stages on the first processor are reported here, meaning only objects associated with
     the global communicator, or MPI_COMM_SELF for proc 1. We really should report global stats and then
     stats for stages local to processor sets.
  */
  /* We should figure out the longest object name here (now 20 characters) */
  PetscCall(PetscFPrintf(comm, fd, "Object Type          Creations   Destructions. Reports information only for process 0.\n"));
  for (stage = 0; stage < numStages; stage++) {
    if (localStageUsed[stage]) {
      classInfo = stageLog->stageInfo[stage].classLog->classInfo;
      PetscCall(PetscFPrintf(comm, fd, "\n--- Event Stage %d: %s\n\n", stage, stageInfo[stage].name));
      for (oclass = 0; oclass < stageLog->stageInfo[stage].classLog->numClasses; oclass++) {
        if ((classInfo[oclass].creations > 0) || (classInfo[oclass].destructions > 0)) {
          PetscCall(PetscFPrintf(comm, fd, "%20s %5d          %5d\n", stageLog->classLog->classInfo[oclass].name, classInfo[oclass].creations, classInfo[oclass].destructions));
        }
      }
    } else {
      if (!localStageVisible[stage]) continue;
      PetscCall(PetscFPrintf(comm, fd, "\n--- Event Stage %d: Unknown\n\n", stage));
    }
  }

  PetscCall(PetscFree(localStageUsed));
  PetscCall(PetscFree(stageUsed));
  PetscCall(PetscFree(localStageVisible));
  PetscCall(PetscFree(stageVisible));

  /* Information unrelated to this particular run */
  PetscCall(PetscFPrintf(comm, fd, "========================================================================================================================\n"));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&x));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscTime(&y));
  PetscCall(PetscFPrintf(comm, fd, "Average time to get PetscTime(): %g\n", (y - x) / 10.0));
  /* MPI information */
  if (size > 1) {
    MPI_Status  status;
    PetscMPIInt tag;
    MPI_Comm    newcomm;

    PetscCallMPI(MPI_Barrier(comm));
    PetscCall(PetscTime(&x));
    PetscCallMPI(MPI_Barrier(comm));
    PetscCallMPI(MPI_Barrier(comm));
    PetscCallMPI(MPI_Barrier(comm));
    PetscCallMPI(MPI_Barrier(comm));
    PetscCallMPI(MPI_Barrier(comm));
    PetscCall(PetscTime(&y));
    PetscCall(PetscFPrintf(comm, fd, "Average time for MPI_Barrier(): %g\n", (y - x) / 5.0));
    PetscCall(PetscCommDuplicate(comm, &newcomm, &tag));
    PetscCallMPI(MPI_Barrier(comm));
    if (rank) {
      PetscCallMPI(MPI_Recv(NULL, 0, MPI_INT, rank - 1, tag, newcomm, &status));
      PetscCallMPI(MPI_Send(NULL, 0, MPI_INT, (rank + 1) % size, tag, newcomm));
    } else {
      PetscCall(PetscTime(&x));
      PetscCallMPI(MPI_Send(NULL, 0, MPI_INT, 1, tag, newcomm));
      PetscCallMPI(MPI_Recv(NULL, 0, MPI_INT, size - 1, tag, newcomm, &status));
      PetscCall(PetscTime(&y));
      PetscCall(PetscFPrintf(comm, fd, "Average time for zero size MPI_Send(): %g\n", (y - x) / size));
    }
    PetscCall(PetscCommDestroy(&newcomm));
  }
  PetscCall(PetscOptionsView(NULL, viewer));

  /* Machine and compile information */
  #if defined(PETSC_USE_FORTRAN_KERNELS)
  PetscCall(PetscFPrintf(comm, fd, "Compiled with FORTRAN kernels\n"));
  #else
  PetscCall(PetscFPrintf(comm, fd, "Compiled without FORTRAN kernels\n"));
  #endif
  #if defined(PETSC_USE_64BIT_INDICES)
  PetscCall(PetscFPrintf(comm, fd, "Compiled with 64 bit PetscInt\n"));
  #elif defined(PETSC_USE___FLOAT128)
  PetscCall(PetscFPrintf(comm, fd, "Compiled with 32 bit PetscInt\n"));
  #endif
  #if defined(PETSC_USE_REAL_SINGLE)
  PetscCall(PetscFPrintf(comm, fd, "Compiled with single precision PetscScalar and PetscReal\n"));
  #elif defined(PETSC_USE___FLOAT128)
  PetscCall(PetscFPrintf(comm, fd, "Compiled with 128 bit precision PetscScalar and PetscReal\n"));
  #endif
  #if defined(PETSC_USE_REAL_MAT_SINGLE)
  PetscCall(PetscFPrintf(comm, fd, "Compiled with single precision matrices\n"));
  #else
  PetscCall(PetscFPrintf(comm, fd, "Compiled with full precision matrices (default)\n"));
  #endif
  PetscCall(PetscFPrintf(comm, fd, "sizeof(short) %d sizeof(int) %d sizeof(long) %d sizeof(void*) %d sizeof(PetscScalar) %d sizeof(PetscInt) %d\n", (int)sizeof(short), (int)sizeof(int), (int)sizeof(long), (int)sizeof(void *), (int)sizeof(PetscScalar), (int)sizeof(PetscInt)));

  PetscCall(PetscFPrintf(comm, fd, "Configure options: %s", petscconfigureoptions));
  PetscCall(PetscFPrintf(comm, fd, "%s", petscmachineinfo));
  PetscCall(PetscFPrintf(comm, fd, "%s", petsccompilerinfo));
  PetscCall(PetscFPrintf(comm, fd, "%s", petsccompilerflagsinfo));
  PetscCall(PetscFPrintf(comm, fd, "%s", petsclinkerinfo));

  /* Cleanup */
  PetscCall(PetscFPrintf(comm, fd, "\n"));
  PetscCall(PetscLogViewWarnNoGpuAwareMpi(comm, fd));
  PetscCall(PetscLogViewWarnDebugging(comm, fd));
  PetscCall(PetscFPTrapPop());
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogView - Prints a summary of the logging.

  Collective over MPI_Comm

  Input Parameter:
.  viewer - an ASCII viewer

  Options Database Keys:
+  -log_view [:filename] - Prints summary of log information
.  -log_view :filename.py:ascii_info_detail - Saves logging information from each process as a Python file
.  -log_view :filename.xml:ascii_xml - Saves a summary of the logging information in a nested format (see below for how to view it)
.  -log_view :filename.txt:ascii_flamegraph - Saves logging information in a format suitable for visualising as a Flame Graph (see below for how to view it)
.  -log_view_memory - Also display memory usage in each event
.  -log_view_gpu_time - Also display time in each event for GPU kernels (Note this may slow the computation)
.  -log_all - Saves a file Log.rank for each MPI rank with details of each step of the computation
-  -log_trace [filename] - Displays a trace of what each process is doing

  Level: beginner

  Notes:
  It is possible to control the logging programmatically but we recommend using the options database approach whenever possible
  By default the summary is printed to stdout.

  Before calling this routine you must have called either PetscLogDefaultBegin() or PetscLogNestedBegin()

  If PETSc is configured with --with-logging=0 then this functionality is not available

  To view the nested XML format filename.xml first copy  ${PETSC_DIR}/share/petsc/xml/performance_xml2html.xsl to the current
  directory then open filename.xml with your browser. Specific notes for certain browsers
$    Firefox and Internet explorer - simply open the file
$    Google Chrome - you must start up Chrome with the option --allow-file-access-from-files
$    Safari - see https://ccm.net/faq/36342-safari-how-to-enable-local-file-access
  or one can use the package http://xmlsoft.org/XSLT/xsltproc2.html to translate the xml file to html and then open it with
  your browser.
  Alternatively, use the script ${PETSC_DIR}/lib/petsc/bin/petsc-performance-view to automatically open a new browser
  window and render the XML log file contents.

  The nested XML format was kindly donated by Koos Huijssen and Christiaan M. Klaij  MARITIME  RESEARCH  INSTITUTE  NETHERLANDS

  The Flame Graph output can be visualised using either the original Flame Graph script (https://github.com/brendangregg/FlameGraph)
  or using speedscope (https://www.speedscope.app).
  Old XML profiles may be converted into this format using the script ${PETSC_DIR}/lib/petsc/bin/xml2flamegraph.py.

.seealso: [](ch_profiling), `PetscLogDefaultBegin()`, `PetscLogDump()`
@*/
PetscErrorCode PetscLogView(PetscViewer viewer)
{
  PetscBool         isascii;
  PetscViewerFormat format;
  int               stage, lastStage;
  PetscStageLog     stageLog;

  PetscFunctionBegin;
  PetscCheck(PetscLogPLB, PETSC_COMM_SELF, PETSC_ERR_SUP, "Must use -log_view or PetscLogDefaultBegin() before calling this routine");
  /* Pop off any stages the user forgot to remove */
  lastStage = 0;
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  while (stage >= 0) {
    lastStage = stage;
    PetscCall(PetscStageLogPop(stageLog));
    PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  }
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &isascii));
  PetscCheck(isascii, PetscObjectComm((PetscObject)viewer), PETSC_ERR_SUP, "Currently can only view logging to ASCII");
  PetscCall(PetscViewerGetFormat(viewer, &format));
  if (format == PETSC_VIEWER_DEFAULT || format == PETSC_VIEWER_ASCII_INFO) {
    PetscCall(PetscLogView_Default(viewer));
  } else if (format == PETSC_VIEWER_ASCII_INFO_DETAIL) {
    PetscCall(PetscLogView_Detailed(viewer));
  } else if (format == PETSC_VIEWER_ASCII_CSV) {
    PetscCall(PetscLogView_CSV(viewer));
  } else if (format == PETSC_VIEWER_ASCII_XML) {
    PetscCall(PetscLogView_Nested(viewer));
  } else if (format == PETSC_VIEWER_ASCII_FLAMEGRAPH) {
    PetscCall(PetscLogView_Flamegraph(viewer));
  }
  PetscCall(PetscStageLogPush(stageLog, lastStage));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogViewFromOptions - Processes command line options to determine if/how a `PetscLog` is to be viewed.

  Collective on `PETSC_COMM_WORLD`

  Level: developer

.seealso: [](ch_profiling), `PetscLogView()`
@*/
PetscErrorCode PetscLogViewFromOptions(void)
{
  PetscViewer       viewer;
  PetscBool         flg;
  PetscViewerFormat format;

  PetscFunctionBegin;
  PetscCall(PetscOptionsGetViewer(PETSC_COMM_WORLD, NULL, NULL, "-log_view", &viewer, &format, &flg));
  if (flg) {
    PetscCall(PetscViewerPushFormat(viewer, format));
    PetscCall(PetscLogView(viewer));
    PetscCall(PetscViewerPopFormat(viewer));
    PetscCall(PetscViewerDestroy(&viewer));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*----------------------------------------------- Counter Functions -------------------------------------------------*/
/*@C
   PetscGetFlops - Returns the number of flops used on this processor
   since the program began.

   Not Collective

   Output Parameter:
   flops - number of floating point operations

   Level: intermediate

   Notes:
   A global counter logs all PETSc flop counts.  The user can use
   `PetscLogFlops()` to increment this counter to include flops for the
   application code.

   A separate counter `PetscLogGPUFlops()` logs the flops that occur on any GPU associated with this MPI rank

.seealso: [](ch_profiling), `PetscLogGPUFlops()`, `PetscTime()`, `PetscLogFlops()`
@*/
PetscErrorCode PetscGetFlops(PetscLogDouble *flops)
{
  PetscFunctionBegin;
  *flops = petsc_TotalFlops;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscLogObjectState(PetscObject obj, const char format[], ...)
{
  size_t  fullLength;
  va_list Argp;

  PetscFunctionBegin;
  if (!petsc_logObjects) PetscFunctionReturn(PETSC_SUCCESS);
  va_start(Argp, format);
  PetscCall(PetscVSNPrintf(petsc_objects[obj->id].info, 64, format, &fullLength, Argp));
  va_end(Argp);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscLogFlops - Adds floating point operations to the global counter.

   Synopsis:
   #include <petsclog.h>
   PetscErrorCode PetscLogFlops(PetscLogDouble f)

   Not Collective

   Input Parameter:
.  f - flop counter

   Usage:
.vb
     PetscLogEvent USER_EVENT;
     PetscLogEventRegister("User event",0,&USER_EVENT);
     PetscLogEventBegin(USER_EVENT,0,0,0,0);
        [code segment to monitor]
        PetscLogFlops(user_flops)
     PetscLogEventEnd(USER_EVENT,0,0,0,0);
.ve

   Level: intermediate

   Note:
   A global counter logs all PETSc flop counts.  The user can use
   PetscLogFlops() to increment this counter to include flops for the
   application code.

.seealso: [](ch_profiling), `PetscLogGPUFlops()`, `PetscLogEventRegister()`, `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscGetFlops()`
M*/

/*MC
   PetscPreLoadBegin - Begin a segment of code that may be preloaded (run twice)
    to get accurate timings

   Synopsis:
   #include <petsclog.h>
   void PetscPreLoadBegin(PetscBool  flag,char *name);

   Not Collective

   Input Parameters:
+   flag - `PETSC_TRUE` to run twice, `PETSC_FALSE` to run once, may be overridden
           with command line option -preload true or -preload false
-   name - name of first stage (lines of code timed separately with `-log_view`) to
           be preloaded

   Usage:
.vb
     PetscPreLoadBegin(PETSC_TRUE,"first stage);
       lines of code
       PetscPreLoadStage("second stage");
       lines of code
     PetscPreLoadEnd();
.ve

   Level: intermediate

   Note:
    Only works in C/C++, not Fortran

     Flags available within the macro.
+    PetscPreLoadingUsed - true if we are or have done preloading
.    PetscPreLoadingOn - true if it is CURRENTLY doing preload
.    PetscPreLoadIt - 0 for the first computation (with preloading turned off it is only 0) 1 for the second
-    PetscPreLoadMax - number of times it will do the computation, only one when preloading is turned on
     The first two variables are available throughout the program, the second two only between the `PetscPreLoadBegin()`
     and `PetscPreLoadEnd()`

.seealso: [](ch_profiling), `PetscLogEventRegister()`, `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscPreLoadEnd()`, `PetscPreLoadStage()`
M*/

/*MC
   PetscPreLoadEnd - End a segment of code that may be preloaded (run twice)
    to get accurate timings

   Synopsis:
   #include <petsclog.h>
   void PetscPreLoadEnd(void);

   Not Collective

   Usage:
.vb
     PetscPreLoadBegin(PETSC_TRUE,"first stage);
       lines of code
       PetscPreLoadStage("second stage");
       lines of code
     PetscPreLoadEnd();
.ve

   Level: intermediate

   Note:
    Only works in C/C++ not fortran

.seealso: [](ch_profiling), `PetscLogEventRegister()`, `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscPreLoadBegin()`, `PetscPreLoadStage()`
M*/

/*MC
   PetscPreLoadStage - Start a new segment of code to be timed separately.
    to get accurate timings

   Synopsis:
   #include <petsclog.h>
   void PetscPreLoadStage(char *name);

   Not Collective

   Usage:
.vb
     PetscPreLoadBegin(PETSC_TRUE,"first stage);
       lines of code
       PetscPreLoadStage("second stage");
       lines of code
     PetscPreLoadEnd();
.ve

   Level: intermediate

   Note:
    Only works in C/C++ not fortran

.seealso: [](ch_profiling), `PetscLogEventRegister()`, `PetscLogEventBegin()`, `PetscLogEventEnd()`, `PetscPreLoadBegin()`, `PetscPreLoadEnd()`
M*/

  #if PetscDefined(HAVE_DEVICE)
    #include <petsc/private/deviceimpl.h>

PetscBool PetscLogGpuTimeFlag = PETSC_FALSE;

/*
   This cannot be called by users between PetscInitialize() and PetscFinalize() at any random location in the code
   because it will result in timing results that cannot be interpreted.
*/
static PetscErrorCode PetscLogGpuTime_Off(void)
{
  PetscLogGpuTimeFlag = PETSC_FALSE;
  return PETSC_SUCCESS;
}

/*@C
     PetscLogGpuTime - turn on the logging of GPU time for GPU kernels

  Options Database Key:
.   -log_view_gpu_time - provide the GPU times in the -log_view output

   Level: advanced

  Notes:
    Turning on the timing of the
    GPU kernels can slow down the entire computation and should only be used when studying the performance
    of operations on GPU such as vector operations and matrix-vector operations.

    This routine should only be called once near the beginning of the program. Once it is started it cannot be turned off.

.seealso: [](ch_profiling), `PetscLogView()`, `PetscLogGpuFlops()`, `PetscLogGpuTimeEnd()`, `PetscLogGpuTimeBegin()`
@*/
PetscErrorCode PetscLogGpuTime(void)
{
  if (!PetscLogGpuTimeFlag) PetscCall(PetscRegisterFinalize(PetscLogGpuTime_Off));
  PetscLogGpuTimeFlag = PETSC_TRUE;
  return PETSC_SUCCESS;
}

/*@C
  PetscLogGpuTimeBegin - Start timer for device

  Level: intermediate

  Notes:
    When CUDA or HIP is enabled, the timer is run on the GPU, it is a separate logging of time devoted to GPU computations (excluding kernel launch times).

    When CUDA or HIP is not available, the timer is run on the CPU, it is a separate logging of time devoted to GPU computations (including kernel launch times).

    There is no need to call WaitForCUDA() or WaitForHIP() between `PetscLogGpuTimeBegin()` and `PetscLogGpuTimeEnd()`

    This timer should NOT include times for data transfers between the GPU and CPU, nor setup actions such as allocating space.

    The regular logging captures the time for data transfers and any CPU activities during the event

    It is used to compute the flop rate on the GPU as it is actively engaged in running a kernel.

  Developer Notes:
    The GPU event timer captures the execution time of all the kernels launched in the default stream by the CPU between `PetscLogGpuTimeBegin()` and `PetsLogGpuTimeEnd()`.

    `PetscLogGpuTimeBegin()` and `PetsLogGpuTimeEnd()` insert the begin and end events into the default stream (stream 0). The device will record a time stamp for the
    event when it reaches that event in the stream. The function xxxEventSynchronize() is called in `PetsLogGpuTimeEnd()` to block CPU execution,
    but not continued GPU execution, until the timer event is recorded.

.seealso: [](ch_profiling), `PetscLogView()`, `PetscLogGpuFlops()`, `PetscLogGpuTimeEnd()`, `PetscLogGpuTime()`
@*/
PetscErrorCode PetscLogGpuTimeBegin(void)
{
  PetscFunctionBegin;
  if (!PetscLogPLB || !PetscLogGpuTimeFlag) PetscFunctionReturn(PETSC_SUCCESS);
  if (PetscDefined(HAVE_CUDA) || PetscDefined(HAVE_HIP)) {
    PetscDeviceContext dctx;

    PetscCall(PetscDeviceContextGetCurrentContext(&dctx));
    PetscCall(PetscDeviceContextBeginTimer_Internal(dctx));
  } else {
    PetscCall(PetscTimeSubtract(&petsc_gtime));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscLogGpuTimeEnd - Stop timer for device

  Level: intermediate

.seealso: [](ch_profiling), `PetscLogView()`, `PetscLogGpuFlops()`, `PetscLogGpuTimeBegin()`
@*/
PetscErrorCode PetscLogGpuTimeEnd(void)
{
  PetscFunctionBegin;
  if (!PetscLogPLE || !PetscLogGpuTimeFlag) PetscFunctionReturn(PETSC_SUCCESS);
  if (PetscDefined(HAVE_CUDA) || PetscDefined(HAVE_HIP)) {
    PetscDeviceContext dctx;
    PetscLogDouble     elapsed;

    PetscCall(PetscDeviceContextGetCurrentContext(&dctx));
    PetscCall(PetscDeviceContextEndTimer_Internal(dctx, &elapsed));
    petsc_gtime += (elapsed / 1000.0);
  } else {
    PetscCall(PetscTimeAdd(&petsc_gtime));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

  #endif /* end of PETSC_HAVE_DEVICE */

#else /* end of -DPETSC_USE_LOG section */

PetscErrorCode PetscLogObjectState(PetscObject obj, const char format[], ...)
{
  PetscFunctionBegin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

#endif /* PETSC_USE_LOG*/

PetscClassId PETSC_LARGEST_CLASSID = PETSC_SMALLEST_CLASSID;
PetscClassId PETSC_OBJECT_CLASSID  = 0;

/*@C
  PetscClassIdRegister - Registers a new class name for objects and logging operations in an application code.

  Not Collective

  Input Parameter:
. name   - The class name

  Output Parameter:
. oclass - The class id or classid

  Level: developer

.seealso: [](ch_profiling), `PetscLogEventRegister()`
@*/
PetscErrorCode PetscClassIdRegister(const char name[], PetscClassId *oclass)
{
#if defined(PETSC_USE_LOG)
  PetscStageLog stageLog;
  PetscInt      stage;
#endif

  PetscFunctionBegin;
  *oclass = ++PETSC_LARGEST_CLASSID;
#if defined(PETSC_USE_LOG)
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscClassRegLogRegister(stageLog->classLog, name, *oclass));
  for (stage = 0; stage < stageLog->numStages; stage++) PetscCall(PetscClassPerfLogEnsureSize(stageLog->stageInfo[stage].classLog, stageLog->classLog->numClasses));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

#if defined(PETSC_USE_LOG) && defined(PETSC_HAVE_MPE)
  #include <mpe.h>

PetscBool PetscBeganMPE = PETSC_FALSE;

PETSC_INTERN PetscErrorCode PetscLogEventBeginMPE(PetscLogEvent, int, PetscObject, PetscObject, PetscObject, PetscObject);
PETSC_INTERN PetscErrorCode PetscLogEventEndMPE(PetscLogEvent, int, PetscObject, PetscObject, PetscObject, PetscObject);

/*@C
   PetscLogMPEBegin - Turns on MPE logging of events. This creates large log files
   and slows the program down.

   Collective over `PETSC_COMM_WORLD`

   Options Database Key:
. -log_mpe - Prints extensive log information

   Level: advanced

   Note:
   A related routine is `PetscLogDefaultBegin()` (with the options key -log_view), which is
   intended for production runs since it logs only flop rates and object
   creation (and should not significantly slow the programs).

.seealso: [](ch_profiling), `PetscLogDump()`, `PetscLogDefaultBegin()`, `PetscLogAllBegin()`, `PetscLogEventActivate()`,
          `PetscLogEventDeactivate()`
@*/
PetscErrorCode PetscLogMPEBegin(void)
{
  PetscFunctionBegin;
  /* Do MPE initialization */
  if (!MPE_Initialized_logging()) { /* This function exists in mpich 1.1.2 and higher */
    PetscCall(PetscInfo(0, "Initializing MPE.\n"));
    PetscCall(MPE_Init_log());

    PetscBeganMPE = PETSC_TRUE;
  } else {
    PetscCall(PetscInfo(0, "MPE already initialized. Not attempting to reinitialize.\n"));
  }
  PetscCall(PetscLogSet(PetscLogEventBeginMPE, PetscLogEventEndMPE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscLogMPEDump - Dumps the MPE logging info to file for later use with Jumpshot.

   Collective over `PETSC_COMM_WORLD`

   Level: advanced

.seealso: [](ch_profiling), `PetscLogDump()`, `PetscLogAllBegin()`, `PetscLogMPEBegin()`
@*/
PetscErrorCode PetscLogMPEDump(const char sname[])
{
  char name[PETSC_MAX_PATH_LEN];

  PetscFunctionBegin;
  if (PetscBeganMPE) {
    PetscCall(PetscInfo(0, "Finalizing MPE.\n"));
    if (sname) {
      PetscCall(PetscStrncpy(name, sname, sizeof(name)));
    } else {
      PetscCall(PetscGetProgramName(name, sizeof(name)));
    }
    PetscCall(MPE_Finish_log(name));
  } else {
    PetscCall(PetscInfo(0, "Not finalizing MPE (not started by PETSc).\n"));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

  #define PETSC_RGB_COLORS_MAX 39
static const char *PetscLogMPERGBColors[PETSC_RGB_COLORS_MAX] = {"OliveDrab:      ", "BlueViolet:     ", "CadetBlue:      ", "CornflowerBlue: ", "DarkGoldenrod:  ", "DarkGreen:      ", "DarkKhaki:      ", "DarkOliveGreen: ",
                                                                 "DarkOrange:     ", "DarkOrchid:     ", "DarkSeaGreen:   ", "DarkSlateGray:  ", "DarkTurquoise:  ", "DeepPink:       ", "DarkKhaki:      ", "DimGray:        ",
                                                                 "DodgerBlue:     ", "GreenYellow:    ", "HotPink:        ", "IndianRed:      ", "LavenderBlush:  ", "LawnGreen:      ", "LemonChiffon:   ", "LightCoral:     ",
                                                                 "LightCyan:      ", "LightPink:      ", "LightSalmon:    ", "LightSlateGray: ", "LightYellow:    ", "LimeGreen:      ", "MediumPurple:   ", "MediumSeaGreen: ",
                                                                 "MediumSlateBlue:", "MidnightBlue:   ", "MintCream:      ", "MistyRose:      ", "NavajoWhite:    ", "NavyBlue:       ", "OliveDrab:      "};

/*@C
  PetscLogMPEGetRGBColor - This routine returns a rgb color useable with `PetscLogEventRegister()`

  Not collective. Maybe it should be?

  Output Parameter:
. str - character string representing the color

  Level: developer

.seealso: [](ch_profiling), `PetscLogEventRegister()`
@*/
PetscErrorCode PetscLogMPEGetRGBColor(const char *str[])
{
  static int idx = 0;

  PetscFunctionBegin;
  *str = PetscLogMPERGBColors[idx];
  idx  = (idx + 1) % PETSC_RGB_COLORS_MAX;
  PetscFunctionReturn(PETSC_SUCCESS);
}

#endif /* PETSC_USE_LOG && PETSC_HAVE_MPE */

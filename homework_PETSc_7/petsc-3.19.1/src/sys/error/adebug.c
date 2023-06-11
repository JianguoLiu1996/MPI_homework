/*
      Code to handle PETSc starting up in debuggers,etc.
*/

#include <petscsys.h> /*I   "petscsys.h"   I*/
#include <signal.h>
#if defined(PETSC_HAVE_UNISTD_H)
  #include <unistd.h>
#endif

/*
      These are the debugger and display used if the debugger is started up
*/
static char      PetscDebugger[PETSC_MAX_PATH_LEN];
static char      DebugTerminal[PETSC_MAX_PATH_LEN];
static PetscBool UseDebugTerminal    = PETSC_TRUE;
PetscBool        petscwaitonerrorflg = PETSC_FALSE;
PetscBool        petscindebugger     = PETSC_FALSE;

/*@C
   PetscSetDebugTerminal - Sets the terminal to use for debugging.

   Not Collective; No Fortran Support

   Input Parameter:
.  terminal - name of terminal and any flags required to execute a program.
              For example xterm, "urxvt -e", "gnome-terminal -x".
              On Apple MacOS you can use Terminal (note the capital T)

   Options Database Key:
   -debug_terminal terminal - use this terminal instead of the default

   Level: developer

   Notes:
   You can start the debugger for all processes in the same GNU screen session.

     mpiexec -n 4 ./myapp -start_in_debugger -debug_terminal "screen -X -S debug screen"

   will open 4 windows in the session named "debug".

   The default on Apple is Terminal, on other systems the default is xterm

.seealso: `PetscSetDebugger()`
@*/
PetscErrorCode PetscSetDebugTerminal(const char terminal[])
{
  PetscBool xterm;

  PetscFunctionBegin;
  PetscCall(PetscStrncpy(DebugTerminal, terminal, sizeof(DebugTerminal)));
  PetscCall(PetscStrcmp(terminal, "xterm", &xterm));
  if (xterm) PetscCall(PetscStrlcat(DebugTerminal, " -e", sizeof(DebugTerminal)));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSetDebugger - Sets options associated with the debugger.

   Not Collective; No Fortran Support

   Input Parameters:
+  debugger - name of debugger, which should be in your path,
              usually "lldb", "dbx", "gdb", "cuda-gdb", "idb", "xxgdb", "kdgb" or "ddd". Also, HP-UX
              supports "xdb", and IBM rs6000 supports "xldb".

-  usedebugterminal - flag to indicate debugger window, set to either PETSC_TRUE (to indicate
            debugger should be started in a new terminal window) or PETSC_FALSE (to start debugger
            in initial window (the option PETSC_FALSE makes no sense when using more
            than one MPI process.)

   Level: developer

.seealso: `PetscAttachDebugger()`, `PetscAttachDebuggerErrorHandler()`, `PetscSetDebugTerminal()`
@*/
PetscErrorCode PetscSetDebugger(const char debugger[], PetscBool usedebugterminal)
{
  PetscFunctionBegin;
  if (debugger) PetscCall(PetscStrncpy(PetscDebugger, debugger, sizeof(PetscDebugger)));
  if (UseDebugTerminal) UseDebugTerminal = usedebugterminal;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscSetDefaultDebugger - Causes PETSc to use its default debugger and output terminal

   Not Collective

    Level: developer

.seealso: `PetscSetDebugger()`, `PetscSetDebuggerFromString()`
@*/
PetscErrorCode PetscSetDefaultDebugger(void)
{
  PetscFunctionBegin;
#if defined(PETSC_USE_DEBUGGER)
  PetscCall(PetscSetDebugger(PETSC_USE_DEBUGGER, PETSC_TRUE));
#endif
#if defined(__APPLE__)
  PetscCall(PetscSetDebugTerminal("Terminal"));
#else
  PetscCall(PetscSetDebugTerminal("xterm"));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscCheckDebugger_Private(const char defaultDbg[], const char string[], const char *debugger[])
{
  char *f = NULL;

  PetscFunctionBegin;
  PetscCall(PetscStrstr(string, defaultDbg, &f));
  if (f) {
    PetscBool exists;

    PetscCall(PetscTestFile(string, 'x', &exists));
    if (exists) *debugger = string;
    else *debugger = defaultDbg;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscSetDebuggerFromString - Set the complete path for the
       debugger for PETSc to use.

   Not Collective

   Level: developer

.seealso: `PetscSetDebugger()`, `PetscSetDefaultDebugger()`
@*/
PetscErrorCode PetscSetDebuggerFromString(const char *string)
{
  const char *debugger    = NULL;
  PetscBool   useterminal = PETSC_TRUE;
  char       *f           = NULL;

  PetscFunctionBegin;
  PetscCall(PetscStrstr(string, "noxterm", &f));
  if (f) useterminal = PETSC_FALSE;
  PetscCall(PetscStrstr(string, "ddd", &f));
  if (f) useterminal = PETSC_FALSE;
  PetscCall(PetscStrstr(string, "noterminal", &f));
  if (f) useterminal = PETSC_FALSE;
  PetscCall(PetscCheckDebugger_Private("xdb", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("dbx", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("xldb", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("gdb", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("cuda-gdb", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("idb", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("xxgdb", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("ddd", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("kdbg", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("ups", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("workshop", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("pgdbg", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("pathdb", string, &debugger));
  PetscCall(PetscCheckDebugger_Private("lldb", string, &debugger));
  PetscCall(PetscSetDebugger(debugger, useterminal));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscWaitOnError - If an error is detected and the process would normally exit the main program with `MPI_Abort()` sleep instead
                      of exiting.

   Not Collective

   Level: advanced

   Note:
      When -start_in_debugger -debugger_ranks x,y,z is used this prevents the processes NOT listed in x,y,z from calling MPI_Abort and
      killing the user's debugging sessions.

.seealso: `PetscSetDebugger()`, `PetscAttachDebugger()`
@*/
PetscErrorCode PetscWaitOnError(void)
{
  petscwaitonerrorflg = PETSC_TRUE;
  return PETSC_SUCCESS;
}

/*@
   PetscAttachDebugger - Attaches the debugger to the running process.

   Not Collective

   Options Database Keys:
-  -start_in_debugger [noxterm,dbx,xxgdb,xdb,xldb,gdb] [-display name] [-debugger_ranks m,n] -debug_terminal xterm or Terminal (for Apple)
.  -on_error_attach_debugger [noxterm,dbx,xxgdb,xdb,xldb,gdb] [-display name] - Activates debugger attachment

   Level: advanced

   Developer Note:
    Since this can be called by the error handler should it be calling `SETERRQ()` and `PetscCall()`?

.seealso: `PetscSetDebugger()`, `PetscSetDefaultDebugger()`, `PetscSetDebugTerminal()`, `PetscAttachDebuggerErrorHandler()`, `PetscStopForDebugger()`
@*/
PetscErrorCode PetscAttachDebugger(void)
{
  PetscErrorCode PETSC_UNUSED ierr;
#if !defined(PETSC_CANNOT_START_DEBUGGER) && defined(PETSC_HAVE_FORK)
  int       child     = 0;
  PetscReal sleeptime = 0;
  char      program[PETSC_MAX_PATH_LEN], display[256], hostname[64];
#endif

#if defined(PETSC_CANNOT_START_DEBUGGER) || !defined(PETSC_HAVE_FORK)
  ierr = (*PetscErrorPrintf)("System cannot start debugger\n");
  ierr = (*PetscErrorPrintf)("On Cray run program in Totalview debugger\n");
  ierr = (*PetscErrorPrintf)("On Windows use Developer Studio(MSDEV)\n");
  PETSCABORT(PETSC_COMM_WORLD, PETSC_ERR_SUP_SYS);
#else
  if (PetscUnlikely(PetscGetDisplay(display, sizeof(display)))) {
    ierr = (*PetscErrorPrintf)("Cannot determine display\n");
    return PETSC_ERR_SYS;
  }
  if (PetscUnlikely(PetscGetProgramName(program, sizeof(program)))) {
    ierr = (*PetscErrorPrintf)("Cannot determine program name needed to attach debugger\n");
    return PETSC_ERR_SYS;
  }
  if (PetscUnlikely(!program[0])) {
    ierr = (*PetscErrorPrintf)("Cannot determine program name needed to attach debugger\n");
    return PETSC_ERR_SYS;
  }
  child = (int)fork();
  if (PetscUnlikely(child < 0)) {
    ierr = (*PetscErrorPrintf)("Error in fork() prior to attaching debugger\n");
    return PETSC_ERR_SYS;
  }
  petscindebugger = PETSC_TRUE;

  /*
      Swap role the parent and child. This is (I think) so that control c typed
    in the debugger goes to the correct process.
  */
  #if !defined(PETSC_DO_NOT_SWAP_CHILD_FOR_DEBUGGER)
  child = child ? 0 : (int)getppid();
  #endif

  if (child) { /* I am the parent, will run the debugger */
    const char *args[10];
    char        pid[10];
    PetscInt    j, jj;
    PetscBool   isdbx, isidb, isxldb, isxxgdb, isups, isxdb, isworkshop, isddd, iskdbg, islldb;

    PetscCall(PetscGetHostName(hostname, sizeof(hostname)));
    /*
         We need to send a continue signal to the "child" process on the
       alpha, otherwise it just stays off forever
    */
  #if defined(PETSC_NEED_KILL_FOR_DEBUGGER)
    kill(child, SIGCONT);
  #endif
    PetscCall(PetscSNPrintf(pid, PETSC_STATIC_ARRAY_LENGTH(pid), "%d", child));

    PetscCall(PetscStrcmp(PetscDebugger, "xxgdb", &isxxgdb));
    PetscCall(PetscStrcmp(PetscDebugger, "ddd", &isddd));
    PetscCall(PetscStrcmp(PetscDebugger, "kdbg", &iskdbg));
    PetscCall(PetscStrcmp(PetscDebugger, "ups", &isups));
    PetscCall(PetscStrcmp(PetscDebugger, "xldb", &isxldb));
    PetscCall(PetscStrcmp(PetscDebugger, "xdb", &isxdb));
    PetscCall(PetscStrcmp(PetscDebugger, "dbx", &isdbx));
    PetscCall(PetscStrcmp(PetscDebugger, "idb", &isidb));
    PetscCall(PetscStrcmp(PetscDebugger, "workshop", &isworkshop));
    PetscCall(PetscStrcmp(PetscDebugger, "lldb", &islldb));

    if (isxxgdb || isups || isddd) {
      args[1] = program;
      args[2] = pid;
      args[3] = "-display";
      args[0] = PetscDebugger;
      args[4] = display;
      args[5] = NULL;
      printf("PETSC: Attaching %s to %s %s on %s\n", args[0], args[1], pid, hostname);
      if (execvp(args[0], (char **)args) < 0) {
        perror("Unable to start debugger");
        exit(0);
      }
    } else if (iskdbg) {
      args[1] = "-p";
      args[2] = pid;
      args[3] = program;
      args[4] = "-display";
      args[0] = PetscDebugger;
      args[5] = display;
      args[6] = NULL;
      printf("PETSC: Attaching %s to %s %s on %s\n", args[0], args[3], pid, hostname);
      if (execvp(args[0], (char **)args) < 0) {
        perror("Unable to start debugger");
        exit(0);
      }
    } else if (isxldb) {
      args[1] = "-a";
      args[2] = pid;
      args[3] = program;
      args[4] = "-display";
      args[0] = PetscDebugger;
      args[5] = display;
      args[6] = NULL;
      printf("PETSC: Attaching %s to %s %s on %s\n", args[0], args[1], pid, hostname);
      if (execvp(args[0], (char **)args) < 0) {
        perror("Unable to start debugger");
        exit(0);
      }
    } else if (isworkshop) {
      args[1] = "-s";
      args[2] = pid;
      args[3] = "-D";
      args[4] = "-";
      args[0] = PetscDebugger;
      args[5] = pid;
      args[6] = "-display";
      args[7] = display;
      args[8] = NULL;
      printf("PETSC: Attaching %s to %s on %s\n", args[0], pid, hostname);
      if (execvp(args[0], (char **)args) < 0) {
        perror("Unable to start debugger");
        exit(0);
      }
    } else {
      j = 0;
      if (UseDebugTerminal) {
        PetscBool cmp;
        char     *tmp, *tmp1 = NULL;
        PetscCall(PetscStrncmp(DebugTerminal, "Terminal", 8, &cmp));
        if (cmp) {
          char command[1024];
          if (islldb) PetscCall(PetscSNPrintf(command, sizeof(command), "osascript -e 'tell app \"Terminal\" to do script \"lldb  -p %s \"'\n", pid));
          else {
            char fullprogram[PETSC_MAX_PATH_LEN];
            PetscCall(PetscGetFullPath(program, fullprogram, sizeof(fullprogram)));
            PetscCall(PetscSNPrintf(command, sizeof(command), "osascript -e 'tell app \"Terminal\" to do script \"%s  %s %s \"'\n", PetscDebugger, fullprogram, pid));
          }
  #if defined(PETSC_HAVE_POPEN)
          PetscCall(PetscPOpen(PETSC_COMM_SELF, NULL, command, "r", NULL));
  #else
          printf("-debug_terminal Terminal is not available on this system since PETSC_HAVE_POPEN is not defined in this configuration\n");
  #endif
          exit(0);
        }

        PetscCall(PetscStrncmp(DebugTerminal, "screen", 6, &cmp));
        if (!cmp) PetscCall(PetscStrncmp(DebugTerminal, "gnome-terminal", 6, &cmp));
        if (cmp) display[0] = 0; /* when using screen, we never pass -display */
        args[j++] = tmp = DebugTerminal;
        if (display[0]) {
          args[j++] = "-display";
          args[j++] = display;
        }
        while (*tmp) {
          PetscCall(PetscStrchr(tmp, ' ', &tmp1));
          if (!tmp1) break;
          *tmp1     = 0;
          tmp       = tmp1 + 1;
          args[j++] = tmp;
        }
      }
      args[j++] = PetscDebugger;
      jj        = j;
      /* this is for default gdb */
      args[j++] = program;
      args[j++] = pid;
      args[j++] = NULL;

      if (isidb) {
        j         = jj;
        args[j++] = "-pid";
        args[j++] = pid;
        args[j++] = "-gdb";
        args[j++] = program;
        args[j++] = NULL;
      }
      if (islldb) {
        j         = jj;
        args[j++] = "-p";
        args[j++] = pid;
        args[j++] = NULL;
      }
      if (isdbx) {
        j         = jj;
  #if defined(PETSC_USE_P_FOR_DEBUGGER)
        args[j++] = "-p";
        args[j++] = pid;
        args[j++] = program;
  #elif defined(PETSC_USE_LARGEP_FOR_DEBUGGER)
        args[j++] = "-l";
        args[j++] = "ALL";
        args[j++] = "-P";
        args[j++] = pid;
        args[j++] = program;
  #elif defined(PETSC_USE_A_FOR_DEBUGGER)
        args[j++] = "-a";
        args[j++] = pid;
  #elif defined(PETSC_USE_PID_FOR_DEBUGGER)
        args[j++] = "-pid";
        args[j++] = pid;
        args[j++] = program;
  #else
        args[j++] = program;
        args[j++] = pid;
  #endif
        args[j++] = NULL;
      }
      if (UseDebugTerminal) {
        if (display[0]) printf("PETSC: Attaching %s to %s of pid %s on display %s on machine %s\n", PetscDebugger, program, pid, display, hostname);
        else printf("PETSC: Attaching %s to %s on pid %s on %s\n", PetscDebugger, program, pid, hostname);

        if (execvp(args[0], (char **)args) < 0) {
          perror("Unable to start debugger in xterm");
          exit(0);
        }
      } else {
        printf("PETSC: Attaching %s to %s of pid %s on %s\n", PetscDebugger, program, pid, hostname);
        if (execvp(args[0], (char **)args) < 0) {
          perror("Unable to start debugger");
          exit(0);
        }
      }
    }
  } else {          /* I am the child, continue with user code */
    sleeptime = 10; /* default to sleep waiting for debugger */
    PetscCall(PetscOptionsGetReal(NULL, NULL, "-debugger_pause", &sleeptime, NULL));
    if (sleeptime < 0) sleeptime = -sleeptime;
  #if defined(PETSC_NEED_DEBUGGER_NO_SLEEP)
    /*
        HP cannot attach process to sleeping debugger, hence count instead
    */
    {
      PetscReal x = 1.0;
      int       i = 10000000;
      while (i--) x++; /* cannot attach to sleeper */
    }
  #elif defined(PETSC_HAVE_SLEEP_RETURNS_EARLY)
    /*
        IBM sleep may return at anytime, hence must see if there is more time to sleep
    */
    {
      int left = sleeptime;
      while (left > 0) left = PetscSleep(left) - 1;
    }
  #else
    PetscCall(PetscSleep(sleeptime));
  #endif
  }
#endif
  return PETSC_SUCCESS;
}

/*@C
   PetscAttachDebuggerErrorHandler - Error handler that attaches
   a debugger to a running process when an error is detected.
   This routine is useful for examining variables, etc.

   Not Collective

   Input Parameters:
+  comm - communicator over which error occurred
.  line - the line number of the error (indicated by __LINE__)
.  file - the file in which the error was detected (indicated by __FILE__)
.  message - an error text string, usually just printed to the screen
.  number - the generic error number
.  p - `PETSC_ERROR_INITIAL` if error just detected, otherwise `PETSC_ERROR_REPEAT`
-  ctx - error handler context

   Options Database Keys:
+  -on_error_attach_debugger [noxterm,dbx,xxgdb,xdb,xldb,gdb] [-display name] - Activates debugger attachment
-  -start_in_debugger [noxterm,dbx,xxgdb,xdb,xldb,gdb] [-display name] [-debugger_ranks m,n]

   Level: developer

   Notes:
   By default the GNU debugger, gdb, is used.  Alternatives are cuda-gdb, lldb, dbx and
   xxgdb,xldb (on IBM rs6000), xdb (on HP-UX).

   Most users need not directly employ this routine and the other error
   handlers, but can instead use the simplified interface SETERR, which has
   the calling sequence
$     SETERRQ(PETSC_COMM_SELF,number,p,message)

   Notes for experienced users:
   Use `PetscPushErrorHandler()` to set the desired error handler.  The
   currently available PETSc error handlers are
.vb
    PetscTraceBackErrorHandler()
    PetscAttachDebuggerErrorHandler()
    PetscAbortErrorHandler()
.ve
  or you may write your own.

   Developer Note:
     This routine calls abort instead of returning because if it returned then `MPI_Abort()` would get called which can generate an exception
     causing the debugger to be attached again in a cycle.

.seealso: `PetscSetDebuggerFromString()`, `PetscSetDebugger()`, `PetscSetDefaultDebugger()`, `PetscError()`, `PetscPushErrorHandler()`, `PetscPopErrorHandler()`, `PetscTraceBackErrorHandler()`,
          `PetscAbortErrorHandler()`, `PetscMPIAbortErrorHandler()`, `PetscEmacsClientErrorHandler()`, `PetscReturnErrorHandler()`, `PetscSetDebugTermainal()`
@*/
PetscErrorCode PetscAttachDebuggerErrorHandler(MPI_Comm comm, int line, const char *fun, const char *file, PetscErrorCode num, PetscErrorType p, const char *mess, void *ctx)
{
  PetscErrorCode ierr;
  if (!mess) mess = " ";

  if (fun) ierr = (*PetscErrorPrintf)("%s() at %s:%d %s\n", fun, file, line, mess);
  else ierr = (*PetscErrorPrintf)("%s:%d %s\n", file, line, mess);

  ierr = PetscAttachDebugger();
  (void)ierr;
  abort(); /* call abort because don't want to kill other MPI ranks that may successfully attach to debugger */
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscStopForDebugger - Prints a message to the screen indicating how to
         attach to the process with the debugger and then waits for the
         debugger to attach.

   Not Collective

   Options Database Key:
.   -stop_for_debugger - will stop for you to attach the debugger when PetscInitialize() is called

   Level: developer

   Note:
    This is likely never needed since `PetscAttachDebugger()` is easier to use and seems to always work.

   Developer Note:
    Since this can be called by the error handler, should it be calling `SETERRQ()` and `PetscCall()`?

.seealso: `PetscSetDebugger()`, `PetscAttachDebugger()`
@*/
PetscErrorCode PetscStopForDebugger(void)
{
  PetscErrorCode ierr;
  PetscInt       sleeptime = 0;
#if !defined(PETSC_CANNOT_START_DEBUGGER)
  int         ppid;
  PetscMPIInt rank;
  char        program[PETSC_MAX_PATH_LEN], hostname[256];
  PetscBool   isdbx, isxldb, isxxgdb, isddd, iskdbg, isups, isxdb, islldb;
#endif

  PetscFunctionBegin;
#if defined(PETSC_CANNOT_START_DEBUGGER)
  PetscCall((*PetscErrorPrintf)("System cannot start debugger; just continuing program\n"));
#else
  if (MPI_Comm_rank(PETSC_COMM_WORLD, &rank)) rank = 0; /* ignore error since this may be already in error handler */
  ierr = PetscGetHostName(hostname, sizeof(hostname));
  if (ierr) {
    PetscCall((*PetscErrorPrintf)("Cannot determine hostname; just continuing program\n"));
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  ierr = PetscGetProgramName(program, sizeof(program));
  if (ierr) {
    PetscCall((*PetscErrorPrintf)("Cannot determine program name; just continuing program\n"));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (!program[0]) {
    PetscCall((*PetscErrorPrintf)("Cannot determine program name; just continuing program\n"));
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  ppid = getpid();

  PetscCall(PetscStrcmp(PetscDebugger, "xxgdb", &isxxgdb));
  PetscCall(PetscStrcmp(PetscDebugger, "ddd", &isddd));
  PetscCall(PetscStrcmp(PetscDebugger, "kdbg", &iskdbg));
  PetscCall(PetscStrcmp(PetscDebugger, "ups", &isups));
  PetscCall(PetscStrcmp(PetscDebugger, "xldb", &isxldb));
  PetscCall(PetscStrcmp(PetscDebugger, "xdb", &isxdb));
  PetscCall(PetscStrcmp(PetscDebugger, "dbx", &isdbx));
  PetscCall(PetscStrcmp(PetscDebugger, "lldb", &islldb));

  if (isxxgdb || isups || isddd || iskdbg) printf("[%d]%s>>%s %s %d\n", rank, hostname, PetscDebugger, program, ppid);
  else if (isxldb) printf("[%d]%s>>%s -a %d %s\n", rank, hostname, PetscDebugger, ppid, program);
  else if (islldb) printf("[%d]%s>>%s -p %d\n", rank, hostname, PetscDebugger, ppid);
  else if (isdbx) {
  #if defined(PETSC_USE_P_FOR_DEBUGGER)
    printf("[%d]%s>>%s -p %d %s\n", rank, hostname, PetscDebugger, ppid, program);
  #elif defined(PETSC_USE_LARGEP_FOR_DEBUGGER)
    printf("[%d]%s>>%s -l ALL -P %d %s\n", rank, hostname, PetscDebugger, ppid, program);
  #elif defined(PETSC_USE_A_FOR_DEBUGGER)
    printf("[%d]%s>>%s -a %d\n", rank, hostname, PetscDebugger, ppid);
  #elif defined(PETSC_USE_PID_FOR_DEBUGGER)
    printf("[%d]%s>>%s -pid %d %s\n", rank, hostname, PetscDebugger, ppid, program);
  #else
    printf("[%d]%s>>%s %s %d\n", rank, hostname, PetscDebugger, program, ppid);
  #endif
  }
#endif /* PETSC_CANNOT_START_DEBUGGER */

  fflush(stdout); /* ignore error because may already be in error handler */

  sleeptime = 25;                                                                         /* default to sleep waiting for debugger */
  PetscCallContinue(PetscOptionsGetInt(NULL, NULL, "-debugger_pause", &sleeptime, NULL)); /* ignore error because may already be in error handler */
  if (sleeptime < 0) sleeptime = -sleeptime;
#if defined(PETSC_NEED_DEBUGGER_NO_SLEEP)
  /*
      HP cannot attach process to sleeping debugger, hence count instead
  */
  {
    // this *will* get optimized away by any compiler known to man
    PetscReal x = 1.0;
    int       i = 10000000;
    while (i--) x++; /* cannot attach to sleeper */
  }
#elif defined(PETSC_HAVE_SLEEP_RETURNS_EARLY)
  /*
      IBM sleep may return at anytime, hence must see if there is more time to sleep
  */
  {
    int left = sleeptime;
    while (left > 0) left = sleep(left) - 1;
  }
#else
  PetscCall(PetscSleep(sleeptime));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

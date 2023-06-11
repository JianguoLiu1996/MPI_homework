
/*
      Code that allows one to set the error handlers
      Portions of this code are under:
      Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
*/
#include <petsc/private/petscimpl.h> /*I "petscsys.h" I*/
#include <petscviewer.h>

typedef struct _EH *EH;
struct _EH {
  PetscErrorCode (*handler)(MPI_Comm, int, const char *, const char *, PetscErrorCode, PetscErrorType, const char *, void *);
  void *ctx;
  EH    previous;
};

static EH eh = NULL;

/*@C
   PetscEmacsClientErrorHandler - Error handler that uses the emacsclient program to
    load the file where the error occurred. Then calls the "previous" error handler.

   Not Collective

   Input Parameters:
+  comm - communicator over which error occurred
.  line - the line number of the error (indicated by __LINE__)
.  file - the file in which the error was detected (indicated by __FILE__)
.  mess - an error text string, usually just printed to the screen
.  n - the generic error number
.  p - specific error number
-  ctx - error handler context

   Options Database Key:
.   -on_error_emacs <machinename> - will contact machinename to open the Emacs client there

   Level: developer

   Note:
   You must put (server-start) in your .emacs file for the emacsclient software to work

   Developer Note:
   Since this is an error handler it cannot call `PetscCall()`; thus we just return if an error is detected.
   But some of the functions it calls do perform error checking that may not be appropriate in a error handler call.

.seealso: `PetscError()`, `PetscPushErrorHandler()`, `PetscPopErrorHandler()`, `PetscAttachDebuggerErrorHandler()`,
          `PetscAbortErrorHandler()`, `PetscMPIAbortErrorHandler()`, `PetscTraceBackErrorHandler()`, `PetscReturnErrorHandler()`
 @*/
PetscErrorCode PetscEmacsClientErrorHandler(MPI_Comm comm, int line, const char *fun, const char *file, PetscErrorCode n, PetscErrorType p, const char *mess, void *ctx)
{
  PetscErrorCode ierr;
  char           command[PETSC_MAX_PATH_LEN];
  const char    *pdir;
  FILE          *fp;

  ierr = PetscGetPetscDir(&pdir);
  if (ierr) return ierr;
  ierr = PetscSNPrintf(command, PETSC_STATIC_ARRAY_LENGTH(command), "cd %s; emacsclient --no-wait +%d %s\n", pdir, line, file);
  if (ierr) return ierr;
#if defined(PETSC_HAVE_POPEN)
  ierr = PetscPOpen(MPI_COMM_WORLD, (char *)ctx, command, "r", &fp);
  if (ierr) return ierr;
  ierr = PetscPClose(MPI_COMM_WORLD, fp);
  if (ierr) return ierr;
#else
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "Cannot run external programs on this machine");
#endif
  ierr = PetscPopErrorHandler();
  if (ierr) return ierr; /* remove this handler from the stack of handlers */
  if (!eh) {
    ierr = PetscTraceBackErrorHandler(comm, line, fun, file, n, p, mess, NULL);
    if (ierr) return ierr;
  } else {
    ierr = (*eh->handler)(comm, line, fun, file, n, p, mess, eh->ctx);
    if (ierr) return ierr;
  }
  return PETSC_SUCCESS;
}

/*@C
   PetscPushErrorHandler - Sets a routine to be called on detection of errors.

   Not Collective

   Input Parameters:
+  handler - error handler routine
-  ctx - optional handler context that contains information needed by the handler (for
         example file pointers for error messages etc.)

   Calling sequence of `handler`:
$  PetscErrorCode handler(MPI_Comm comm, int line, char *func, char *file, PetscErrorCode n, int p, char *mess, void *ctx);
+  comm - communicator over which error occurred
.  line - the line number of the error (indicated by __LINE__)
.  file - the file in which the error was detected (indicated by __FILE__)
.  n - the generic error number (see list defined in include/petscerror.h)
.  p - `PETSC_ERROR_INITIAL` if error just detected, otherwise `PETSC_ERROR_REPEAT`
.  mess - an error text string, usually just printed to the screen
-  ctx - the error handler context

   Options Database Keys:
+   -on_error_attach_debugger <noxterm,gdb or dbx> - starts up the debugger if an error occurs
-   -on_error_abort - aborts the program if an error occurs

   Level: intermediate

   Note:
   The currently available PETSc error handlers include `PetscTraceBackErrorHandler()`,
   `PetscAttachDebuggerErrorHandler()`, `PetscAbortErrorHandler()`, and `PetscMPIAbortErrorHandler()`, `PetscReturnErrorHandler()`.

   Fortran Note:
    You can only push one error handler from Fortran before popping it.

.seealso: `PetscPopErrorHandler()`, `PetscAttachDebuggerErrorHandler()`, `PetscAbortErrorHandler()`, `PetscTraceBackErrorHandler()`, `PetscPushSignalHandler()`
@*/
PetscErrorCode PetscPushErrorHandler(PetscErrorCode (*handler)(MPI_Comm comm, int, const char *, const char *, PetscErrorCode, PetscErrorType, const char *, void *), void *ctx)
{
  EH neweh;

  PetscFunctionBegin;
  PetscCall(PetscNew(&neweh));
  if (eh) neweh->previous = eh;
  else neweh->previous = NULL;
  neweh->handler = handler;
  neweh->ctx     = ctx;
  eh             = neweh;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscPopErrorHandler - Removes the latest error handler that was
   pushed with `PetscPushErrorHandler()`.

   Not Collective

   Level: intermediate

.seealso: `PetscPushErrorHandler()`
@*/
PetscErrorCode PetscPopErrorHandler(void)
{
  EH tmp;

  PetscFunctionBegin;
  if (!eh) PetscFunctionReturn(PETSC_SUCCESS);
  tmp = eh;
  eh  = eh->previous;
  PetscCall(PetscFree(tmp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscReturnErrorHandler - Error handler that causes a return without printing an error message.

   Not Collective

   Input Parameters:
+  comm - communicator over which error occurred
.  line - the line number of the error (indicated by __LINE__)
.  file - the file in which the error was detected (indicated by __FILE__)
.  mess - an error text string, usually just printed to the screen
.  n - the generic error number
.  p - specific error number
-  ctx - error handler context

   Level: developer

   Notes:
   Most users need not directly employ this routine and the other error
   handlers, but can instead use the simplified interface `SETERRQ()`, which has
   the calling sequence
$     SETERRQ(comm,number,mess)

   `PetscIgnoreErrorHandler()` does the same thing as this function, but is deprecated, you should use this function.

   Use `PetscPushErrorHandler()` to set the desired error handler.

.seealso: `PetscPushErrorHandler()`, `PetscPopErrorHandler()`, `PetscError()`, `PetscAbortErrorHandler()`, `PetscMPIAbortErrorHandler()`, `PetscTraceBackErrorHandler()`,
          `PetscAttachDebuggerErrorHandler()`, `PetscEmacsClientErrorHandler()`
 @*/
PetscErrorCode PetscReturnErrorHandler(MPI_Comm comm, int line, const char *fun, const char *file, PetscErrorCode n, PetscErrorType p, const char *mess, void *ctx)
{
  return n;
}

static char PetscErrorBaseMessage[1024];
/*
       The numerical values for these are defined in include/petscerror.h; any changes
   there must also be made here
*/
static const char *PetscErrorStrings[] = {
  /*55 */ "Out of memory",
  "No support for this operation for this object type",
  "No support for this operation on this system",
  /*58 */ "Operation done in wrong order",
  /*59 */ "Signal received",
  /*60 */ "Nonconforming object sizes",
  "Argument aliasing not permitted",
  "Invalid argument",
  /*63 */ "Argument out of range",
  "Corrupt argument: https://petsc.org/release/faq/#valgrind",
  "Unable to open file",
  "Read from file failed",
  "Write to file failed",
  "Invalid pointer",
  /*69 */ "Arguments must have same type",
  /*70 */ "Attempt to use a pointer that does not point to a valid accessible location",
  /*71 */ "Zero pivot in LU factorization: https://petsc.org/release/faq/#zeropivot",
  /*72 */ "Floating point exception",
  /*73 */ "Object is in wrong state",
  "Corrupted Petsc object",
  "Arguments are incompatible",
  "Error in external library",
  /*77 */ "Petsc has generated inconsistent data",
  "Memory corruption: https://petsc.org/release/faq/#valgrind",
  "Unexpected data in file",
  /*80 */ "Arguments must have same communicators",
  /*81 */ "Zero pivot in Cholesky factorization: https://petsc.org/release/faq/#zeropivot",
  "",
  "",
  "Overflow in integer operation: https://petsc.org/release/faq/#64-bit-indices",
  /*85 */ "Null argument, when expecting valid pointer",
  /*86 */ "Unknown type. Check for miss-spelling or missing package: https://petsc.org/release/install/install/#external-packages",
  /*87 */ "MPI library at runtime is not compatible with MPI used at compile time",
  /*88 */ "Error in system call",
  /*89 */ "Object Type not set: https://petsc.org/release/faq/#object-type-not-set",
  /*90 */ "",
  /*   */ "",
  /*92 */ "See https://petsc.org/release/overview/linear_solve_table/ for possible LU and Cholesky solvers",
  /*93 */ "You cannot overwrite this option since that will conflict with other previously set options",
  /*94 */ "Example/application run with number of MPI ranks it does not support",
  /*95 */ "Missing or incorrect user input",
  /*96 */ "GPU resources unavailable",
  /*97 */ "GPU error",
  /*98 */ "General MPI error",
  /*99 */ "PetscError() incorrectly returned an error code of 0"};

/*@C
  PetscErrorMessage - Returns the text string associated with a PETSc error code.

  Not Collective

  Input Parameter:
. errnum - the error code

  Output Parameters:
+ text - the error message (`NULL` if not desired)
- specific - the specific error message that was set with `SETERRQ()` or
             `PetscError()`. (`NULL` if not desired)

  Level: developer

.seealso: `PetscErrorCode`, `PetscPushErrorHandler()`, `PetscAttachDebuggerErrorHandler()`,
`PetscError()`, `SETERRQ()`, `PetscCall()` `PetscAbortErrorHandler()`,
`PetscTraceBackErrorHandler()`
@*/
PetscErrorCode PetscErrorMessage(PetscErrorCode errnum, const char *text[], char **specific)
{
  PetscFunctionBegin;
  if (text) {
    if (errnum > PETSC_ERR_MIN_VALUE && errnum < PETSC_ERR_MAX_VALUE) {
      size_t len;

      *text = PetscErrorStrings[errnum - PETSC_ERR_MIN_VALUE - 1];
      PetscCall(PetscStrlen(*text, &len));
      if (!len) *text = NULL;
    } else if (errnum == PETSC_ERR_BOOLEAN_MACRO_FAILURE) {
      /* this "error code" arises from failures in boolean macros, where the || operator is
         used to short-circuit the macro call in case of error. This has the side effect of
         "returning" either 0 (PETSC_SUCCESS) or 1 (PETSC_ERR_UNKNONWN):

         #define PETSC_FOO(x) ((PetscErrorCode)(PetscBar(x) || PetscBaz(x)))

         If PetscBar() fails (returns nonzero) PetscBaz() is not executed but the result of
         this expression is boolean false, hence PETSC_ERR_UNNOWN
       */
      *text = "Error occurred in boolean shortcuit in macro";
    } else {
      *text = NULL;
    }
  }
  if (specific) *specific = PetscErrorBaseMessage;
  PetscFunctionReturn(PETSC_SUCCESS);
}

#if defined(PETSC_CLANGUAGE_CXX)
  /* C++ exceptions are formally not allowed to propagate through extern "C" code. In practice, far too much software
 * would be broken if implementations did not handle it it some common cases. However, keep in mind
 *
 *   Rule 62. Don't allow exceptions to propagate across module boundaries
 *
 * in "C++ Coding Standards" by Sutter and Alexandrescu. (This accounts for part of the ongoing C++ binary interface
 * instability.) Having PETSc raise errors as C++ exceptions was probably misguided and should eventually be removed.
 *
 * Here is the problem: You have a C++ function call a PETSc function, and you would like to maintain the error message
 * and stack information from the PETSc error. You could make everyone write exactly this code in their C++, but that
 * seems crazy to me.
 */
  #include <sstream>
  #include <stdexcept>
static void PetscCxxErrorThrow()
{
  const char *str;
  if (eh && eh->ctx) {
    std::ostringstream *msg;
    msg = (std::ostringstream *)eh->ctx;
    str = msg->str().c_str();
  } else str = "Error detected in C PETSc";

  throw std::runtime_error(str);
}
#endif

/*@C
   PetscError - Routine that is called when an error has been detected, usually called through the macro SETERRQ(PETSC_COMM_SELF,).

  Collective

   Input Parameters:
+  comm - communicator over which error occurred.  ALL ranks of this communicator MUST call this routine
.  line - the line number of the error (indicated by __LINE__)
.  func - the function name in which the error was detected
.  file - the file in which the error was detected (indicated by __FILE__)
.  n - the generic error number
.  p - `PETSC_ERROR_INITIAL` indicates the error was initially detected, `PETSC_ERROR_REPEAT` indicates this is a traceback from a previously detected error
-  mess - formatted message string - aka printf

  Options Database Keys:
+  -error_output_stdout - output the error messages to stdout instead of the default stderr
-  -error_output_none - do not output the error messages

  Level: intermediate

   Notes:
   PETSc error handling is done with error return codes. A non-zero return indicates an error
   was detected. The return-value of this routine is what is ultimately returned by
   `SETERRQ()`.

   Note that numerical errors (potential divide by zero, for example) are not managed by the
   error return codes; they are managed via, for example, `KSPGetConvergedReason()` that
   indicates if the solve was successful or not. The option `-ksp_error_if_not_converged`, for
   example, turns numerical failures into hard errors managed via `PetscError()`.

   PETSc provides a rich supply of error handlers, see the list below, and users can also
   provide their own error handlers.

   If the user sets their own error handler (via `PetscPushErrorHandler()`) they may return any
   arbitrary value from it, but are encouraged to return nonzero values. If the return value is
   zero, `SETERRQ()` will ignore the value and return `PETSC_ERR_RETURN` (a nonzero value)
   instead.

   Most users need not directly use this routine and the error handlers, but can instead use
   the simplified interface `PetscCall()` or `SETERRQ()`.

   Fortran Note:
   This routine is used differently from Fortran
$    PetscError(MPI_Comm comm,PetscErrorCode n,PetscErrorType p,char *message)

   Developer Note:
   Since this is called after an error condition it should not be calling any error handlers (currently it ignores any error codes)
   BUT this routine does call regular PETSc functions that may call error handlers, this is problematic and could be fixed by never calling other PETSc routines
   but this annoying.

.seealso: `PetscErrorCode`, `PetscPushErrorHandler()`, `PetscPopErrorHandler()`, `PetscTraceBackErrorHandler()`, `PetscAbortErrorHandler()`, `PetscMPIAbortErrorHandler()`,
          `PetscReturnErrorHandler()`, `PetscAttachDebuggerErrorHandler()`, `PetscEmacsClientErrorHandler()`,
          `SETERRQ()`, `PetscCall()`, `CHKMEMQ`, `SETERRQ()`, `SETERRQ()`, `PetscErrorMessage()`, `PETSCABORT()`
@*/
PetscErrorCode PetscError(MPI_Comm comm, int line, const char *func, const char *file, PetscErrorCode n, PetscErrorType p, const char *mess, ...)
{
  va_list        Argp;
  size_t         fullLength;
  char           buf[2048], *lbuf = NULL;
  PetscBool      ismain;
  PetscErrorCode ierr;

  if (!PetscErrorHandlingInitialized) return n;
  if (comm == MPI_COMM_NULL) comm = PETSC_COMM_SELF;

  /* Compose the message evaluating the print format */
  if (mess) {
    va_start(Argp, mess);
    ierr = PetscVSNPrintf(buf, 2048, mess, &fullLength, Argp);
    va_end(Argp);
    lbuf = buf;
    if (p == PETSC_ERROR_INITIAL) ierr = PetscStrncpy(PetscErrorBaseMessage, lbuf, sizeof(PetscErrorBaseMessage));
  }

  if (p == PETSC_ERROR_INITIAL && n != PETSC_ERR_MEMC) ierr = PetscMallocValidate(__LINE__, PETSC_FUNCTION_NAME, __FILE__);

  if (!eh) ierr = PetscTraceBackErrorHandler(comm, line, func, file, n, p, lbuf, NULL);
  else ierr = (*eh->handler)(comm, line, func, file, n, p, lbuf, eh->ctx);
  PetscStackClearTop;

  /*
      If this is called from the main() routine we abort the program.
      We cannot just return because them some MPI processes may continue to attempt to run
      while this process simply exits.
  */
  if (func) {
    PetscErrorCode cmp_ierr = PetscStrncmp(func, "main", 4, &ismain);
    if (ismain) {
      if (petscwaitonerrorflg) cmp_ierr = PetscSleep(1000);
      (void)cmp_ierr;
      PETSCABORT(comm, ierr);
    }
  }
#if defined(PETSC_CLANGUAGE_CXX)
  if (p == PETSC_ERROR_IN_CXX) PetscCxxErrorThrow();
#endif
  return ierr;
}

/* -------------------------------------------------------------------------*/

/*@C
    PetscIntView - Prints an array of integers; useful for debugging.

    Collective

    Input Parameters:
+   N - number of integers in array
.   idx - array of integers
-   viewer - location to print array, `PETSC_VIEWER_STDOUT_WORLD`, `PETSC_VIEWER_STDOUT_SELF` or 0

  Level: intermediate

    Note:
    This may be called from within the debugger

    Developer Note:
    idx cannot be const because may be passed to binary viewer where temporary byte swapping may be done

.seealso: `PetscViewer`, `PetscRealView()`
@*/
PetscErrorCode PetscIntView(PetscInt N, const PetscInt idx[], PetscViewer viewer)
{
  PetscMPIInt rank, size;
  PetscInt    j, i, n = N / 20, p = N % 20;
  PetscBool   iascii, isbinary;
  MPI_Comm    comm;

  PetscFunctionBegin;
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_SELF;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 3);
  if (N) PetscValidIntPointer(idx, 2);
  PetscCall(PetscObjectGetComm((PetscObject)viewer, &comm));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));

  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERBINARY, &isbinary));
  if (iascii) {
    PetscCall(PetscViewerASCIIPushSynchronized(viewer));
    for (i = 0; i < n; i++) {
      if (size > 1) {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] %" PetscInt_FMT ":", rank, 20 * i));
      } else {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%" PetscInt_FMT ":", 20 * i));
      }
      for (j = 0; j < 20; j++) PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, " %" PetscInt_FMT, idx[i * 20 + j]));
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "\n"));
    }
    if (p) {
      if (size > 1) {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] %" PetscInt_FMT ":", rank, 20 * n));
      } else {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%" PetscInt_FMT ":", 20 * n));
      }
      for (i = 0; i < p; i++) PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, " %" PetscInt_FMT, idx[20 * n + i]));
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "\n"));
    }
    PetscCall(PetscViewerFlush(viewer));
    PetscCall(PetscViewerASCIIPopSynchronized(viewer));
  } else if (isbinary) {
    PetscMPIInt *sizes, Ntotal, *displs, NN;
    PetscInt    *array;

    PetscCall(PetscMPIIntCast(N, &NN));

    if (size > 1) {
      if (rank) {
        PetscCallMPI(MPI_Gather(&NN, 1, MPI_INT, NULL, 0, MPI_INT, 0, comm));
        PetscCallMPI(MPI_Gatherv((void *)idx, NN, MPIU_INT, NULL, NULL, NULL, MPIU_INT, 0, comm));
      } else {
        PetscCall(PetscMalloc1(size, &sizes));
        PetscCallMPI(MPI_Gather(&NN, 1, MPI_INT, sizes, 1, MPI_INT, 0, comm));
        Ntotal = sizes[0];
        PetscCall(PetscMalloc1(size, &displs));
        displs[0] = 0;
        for (i = 1; i < size; i++) {
          Ntotal += sizes[i];
          displs[i] = displs[i - 1] + sizes[i - 1];
        }
        PetscCall(PetscMalloc1(Ntotal, &array));
        PetscCallMPI(MPI_Gatherv((void *)idx, NN, MPIU_INT, array, sizes, displs, MPIU_INT, 0, comm));
        PetscCall(PetscViewerBinaryWrite(viewer, array, Ntotal, PETSC_INT));
        PetscCall(PetscFree(sizes));
        PetscCall(PetscFree(displs));
        PetscCall(PetscFree(array));
      }
    } else {
      PetscCall(PetscViewerBinaryWrite(viewer, idx, N, PETSC_INT));
    }
  } else {
    const char *tname;
    PetscCall(PetscObjectGetName((PetscObject)viewer, &tname));
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Cannot handle that PetscViewer of type %s", tname);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscRealView - Prints an array of doubles; useful for debugging.

    Collective

    Input Parameters:
+   N - number of `PetscReal` in array
.   idx - array of `PetscReal`
-   viewer - location to print array, `PETSC_VIEWER_STDOUT_WORLD`, `PETSC_VIEWER_STDOUT_SELF` or 0

  Level: intermediate

    Note:
    This may be called from within the debugger

    Developer Note:
    idx cannot be const because may be passed to binary viewer where temporary byte swapping may be done

.seealso: `PetscViewer`, `PetscIntView()`
@*/
PetscErrorCode PetscRealView(PetscInt N, const PetscReal idx[], PetscViewer viewer)
{
  PetscMPIInt rank, size;
  PetscInt    j, i, n = N / 5, p = N % 5;
  PetscBool   iascii, isbinary;
  MPI_Comm    comm;

  PetscFunctionBegin;
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_SELF;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 3);
  PetscValidRealPointer(idx, 2);
  PetscCall(PetscObjectGetComm((PetscObject)viewer, &comm));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));

  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERBINARY, &isbinary));
  if (iascii) {
    PetscInt tab;

    PetscCall(PetscViewerASCIIPushSynchronized(viewer));
    PetscCall(PetscViewerASCIIGetTab(viewer, &tab));
    for (i = 0; i < n; i++) {
      PetscCall(PetscViewerASCIISetTab(viewer, tab));
      if (size > 1) {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] %2" PetscInt_FMT ":", rank, 5 * i));
      } else {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%2" PetscInt_FMT ":", 5 * i));
      }
      PetscCall(PetscViewerASCIISetTab(viewer, 0));
      for (j = 0; j < 5; j++) PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, " %12.4e", (double)idx[i * 5 + j]));
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "\n"));
    }
    if (p) {
      PetscCall(PetscViewerASCIISetTab(viewer, tab));
      if (size > 1) {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] %2" PetscInt_FMT ":", rank, 5 * n));
      } else {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%2" PetscInt_FMT ":", 5 * n));
      }
      PetscCall(PetscViewerASCIISetTab(viewer, 0));
      for (i = 0; i < p; i++) PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, " %12.4e", (double)idx[5 * n + i]));
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "\n"));
    }
    PetscCall(PetscViewerFlush(viewer));
    PetscCall(PetscViewerASCIISetTab(viewer, tab));
    PetscCall(PetscViewerASCIIPopSynchronized(viewer));
  } else if (isbinary) {
    PetscMPIInt *sizes, *displs, Ntotal, NN;
    PetscReal   *array;

    PetscCall(PetscMPIIntCast(N, &NN));

    if (size > 1) {
      if (rank) {
        PetscCallMPI(MPI_Gather(&NN, 1, MPI_INT, NULL, 0, MPI_INT, 0, comm));
        PetscCallMPI(MPI_Gatherv((PetscReal *)idx, NN, MPIU_REAL, NULL, NULL, NULL, MPIU_REAL, 0, comm));
      } else {
        PetscCall(PetscMalloc1(size, &sizes));
        PetscCallMPI(MPI_Gather(&NN, 1, MPI_INT, sizes, 1, MPI_INT, 0, comm));
        Ntotal = sizes[0];
        PetscCall(PetscMalloc1(size, &displs));
        displs[0] = 0;
        for (i = 1; i < size; i++) {
          Ntotal += sizes[i];
          displs[i] = displs[i - 1] + sizes[i - 1];
        }
        PetscCall(PetscMalloc1(Ntotal, &array));
        PetscCallMPI(MPI_Gatherv((PetscReal *)idx, NN, MPIU_REAL, array, sizes, displs, MPIU_REAL, 0, comm));
        PetscCall(PetscViewerBinaryWrite(viewer, array, Ntotal, PETSC_REAL));
        PetscCall(PetscFree(sizes));
        PetscCall(PetscFree(displs));
        PetscCall(PetscFree(array));
      }
    } else {
      PetscCall(PetscViewerBinaryWrite(viewer, (void *)idx, N, PETSC_REAL));
    }
  } else {
    const char *tname;
    PetscCall(PetscObjectGetName((PetscObject)viewer, &tname));
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Cannot handle that PetscViewer of type %s", tname);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscScalarView - Prints an array of `PetscScalar`; useful for debugging.

    Collective

    Input Parameters:
+   N - number of scalars in array
.   idx - array of scalars
-   viewer - location to print array, `PETSC_VIEWER_STDOUT_WORLD`, `PETSC_VIEWER_STDOUT_SELF` or 0

  Level: intermediate

    Note:
    This may be called from within the debugger

    Developer Note:
    idx cannot be const because may be passed to binary viewer where byte swapping may be done

.seealso: `PetscViewer`, `PetscIntView()`, `PetscRealView()`
@*/
PetscErrorCode PetscScalarView(PetscInt N, const PetscScalar idx[], PetscViewer viewer)
{
  PetscMPIInt rank, size;
  PetscInt    j, i, n = N / 3, p = N % 3;
  PetscBool   iascii, isbinary;
  MPI_Comm    comm;

  PetscFunctionBegin;
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_SELF;
  PetscValidHeader(viewer, 3);
  if (N) PetscValidScalarPointer(idx, 2);
  PetscCall(PetscObjectGetComm((PetscObject)viewer, &comm));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));

  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERBINARY, &isbinary));
  if (iascii) {
    PetscCall(PetscViewerASCIIPushSynchronized(viewer));
    for (i = 0; i < n; i++) {
      if (size > 1) {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] %2" PetscInt_FMT ":", rank, 3 * i));
      } else {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%2" PetscInt_FMT ":", 3 * i));
      }
      for (j = 0; j < 3; j++) {
#if defined(PETSC_USE_COMPLEX)
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, " (%12.4e,%12.4e)", (double)PetscRealPart(idx[i * 3 + j]), (double)PetscImaginaryPart(idx[i * 3 + j])));
#else
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, " %12.4e", (double)idx[i * 3 + j]));
#endif
      }
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "\n"));
    }
    if (p) {
      if (size > 1) {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "[%d] %2" PetscInt_FMT ":", rank, 3 * n));
      } else {
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "%2" PetscInt_FMT ":", 3 * n));
      }
      for (i = 0; i < p; i++) {
#if defined(PETSC_USE_COMPLEX)
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, " (%12.4e,%12.4e)", (double)PetscRealPart(idx[n * 3 + i]), (double)PetscImaginaryPart(idx[n * 3 + i])));
#else
        PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, " %12.4e", (double)idx[3 * n + i]));
#endif
      }
      PetscCall(PetscViewerASCIISynchronizedPrintf(viewer, "\n"));
    }
    PetscCall(PetscViewerFlush(viewer));
    PetscCall(PetscViewerASCIIPopSynchronized(viewer));
  } else if (isbinary) {
    PetscMPIInt *sizes, Ntotal, *displs, NN;
    PetscScalar *array;

    PetscCall(PetscMPIIntCast(N, &NN));

    if (size > 1) {
      if (rank) {
        PetscCallMPI(MPI_Gather(&NN, 1, MPI_INT, NULL, 0, MPI_INT, 0, comm));
        PetscCallMPI(MPI_Gatherv((void *)idx, NN, MPIU_SCALAR, NULL, NULL, NULL, MPIU_SCALAR, 0, comm));
      } else {
        PetscCall(PetscMalloc1(size, &sizes));
        PetscCallMPI(MPI_Gather(&NN, 1, MPI_INT, sizes, 1, MPI_INT, 0, comm));
        Ntotal = sizes[0];
        PetscCall(PetscMalloc1(size, &displs));
        displs[0] = 0;
        for (i = 1; i < size; i++) {
          Ntotal += sizes[i];
          displs[i] = displs[i - 1] + sizes[i - 1];
        }
        PetscCall(PetscMalloc1(Ntotal, &array));
        PetscCallMPI(MPI_Gatherv((void *)idx, NN, MPIU_SCALAR, array, sizes, displs, MPIU_SCALAR, 0, comm));
        PetscCall(PetscViewerBinaryWrite(viewer, array, Ntotal, PETSC_SCALAR));
        PetscCall(PetscFree(sizes));
        PetscCall(PetscFree(displs));
        PetscCall(PetscFree(array));
      }
    } else {
      PetscCall(PetscViewerBinaryWrite(viewer, (void *)idx, N, PETSC_SCALAR));
    }
  } else {
    const char *tname;
    PetscCall(PetscObjectGetName((PetscObject)viewer, &tname));
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Cannot handle that PetscViewer of type %s", tname);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

#if defined(PETSC_HAVE_CUDA)
  #include <petscdevice_cuda.h>
PETSC_EXTERN const char *PetscCUBLASGetErrorName(cublasStatus_t status)
{
  switch (status) {
  #if (CUDART_VERSION >= 8000) /* At least CUDA 8.0 of Sep. 2016 had these */
  case CUBLAS_STATUS_SUCCESS:
    return "CUBLAS_STATUS_SUCCESS";
  case CUBLAS_STATUS_NOT_INITIALIZED:
    return "CUBLAS_STATUS_NOT_INITIALIZED";
  case CUBLAS_STATUS_ALLOC_FAILED:
    return "CUBLAS_STATUS_ALLOC_FAILED";
  case CUBLAS_STATUS_INVALID_VALUE:
    return "CUBLAS_STATUS_INVALID_VALUE";
  case CUBLAS_STATUS_ARCH_MISMATCH:
    return "CUBLAS_STATUS_ARCH_MISMATCH";
  case CUBLAS_STATUS_MAPPING_ERROR:
    return "CUBLAS_STATUS_MAPPING_ERROR";
  case CUBLAS_STATUS_EXECUTION_FAILED:
    return "CUBLAS_STATUS_EXECUTION_FAILED";
  case CUBLAS_STATUS_INTERNAL_ERROR:
    return "CUBLAS_STATUS_INTERNAL_ERROR";
  case CUBLAS_STATUS_NOT_SUPPORTED:
    return "CUBLAS_STATUS_NOT_SUPPORTED";
  case CUBLAS_STATUS_LICENSE_ERROR:
    return "CUBLAS_STATUS_LICENSE_ERROR";
  #endif
  default:
    return "unknown error";
  }
}
PETSC_EXTERN const char *PetscCUSolverGetErrorName(cusolverStatus_t status)
{
  switch (status) {
  #if (CUDART_VERSION >= 8000) /* At least CUDA 8.0 of Sep. 2016 had these */
  case CUSOLVER_STATUS_SUCCESS:
    return "CUSOLVER_STATUS_SUCCESS";
  case CUSOLVER_STATUS_NOT_INITIALIZED:
    return "CUSOLVER_STATUS_NOT_INITIALIZED";
  case CUSOLVER_STATUS_INVALID_VALUE:
    return "CUSOLVER_STATUS_INVALID_VALUE";
  case CUSOLVER_STATUS_ARCH_MISMATCH:
    return "CUSOLVER_STATUS_ARCH_MISMATCH";
  case CUSOLVER_STATUS_INTERNAL_ERROR:
    return "CUSOLVER_STATUS_INTERNAL_ERROR";
    #if (CUDART_VERSION >= 9000) /* CUDA 9.0 had these defined on June 2021 */
  case CUSOLVER_STATUS_ALLOC_FAILED:
    return "CUSOLVER_STATUS_ALLOC_FAILED";
  case CUSOLVER_STATUS_MAPPING_ERROR:
    return "CUSOLVER_STATUS_MAPPING_ERROR";
  case CUSOLVER_STATUS_EXECUTION_FAILED:
    return "CUSOLVER_STATUS_EXECUTION_FAILED";
  case CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED:
    return "CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED";
  case CUSOLVER_STATUS_NOT_SUPPORTED:
    return "CUSOLVER_STATUS_NOT_SUPPORTED ";
  case CUSOLVER_STATUS_ZERO_PIVOT:
    return "CUSOLVER_STATUS_ZERO_PIVOT";
  case CUSOLVER_STATUS_INVALID_LICENSE:
    return "CUSOLVER_STATUS_INVALID_LICENSE";
    #endif
  #endif
  default:
    return "unknown error";
  }
}
PETSC_EXTERN const char *PetscCUFFTGetErrorName(cufftResult result)
{
  switch (result) {
  case CUFFT_SUCCESS:
    return "CUFFT_SUCCESS";
  case CUFFT_INVALID_PLAN:
    return "CUFFT_INVALID_PLAN";
  case CUFFT_ALLOC_FAILED:
    return "CUFFT_ALLOC_FAILED";
  case CUFFT_INVALID_TYPE:
    return "CUFFT_INVALID_TYPE";
  case CUFFT_INVALID_VALUE:
    return "CUFFT_INVALID_VALUE";
  case CUFFT_INTERNAL_ERROR:
    return "CUFFT_INTERNAL_ERROR";
  case CUFFT_EXEC_FAILED:
    return "CUFFT_EXEC_FAILED";
  case CUFFT_SETUP_FAILED:
    return "CUFFT_SETUP_FAILED";
  case CUFFT_INVALID_SIZE:
    return "CUFFT_INVALID_SIZE";
  case CUFFT_UNALIGNED_DATA:
    return "CUFFT_UNALIGNED_DATA";
  case CUFFT_INCOMPLETE_PARAMETER_LIST:
    return "CUFFT_INCOMPLETE_PARAMETER_LIST";
  case CUFFT_INVALID_DEVICE:
    return "CUFFT_INVALID_DEVICE";
  case CUFFT_PARSE_ERROR:
    return "CUFFT_PARSE_ERROR";
  case CUFFT_NO_WORKSPACE:
    return "CUFFT_NO_WORKSPACE";
  case CUFFT_NOT_IMPLEMENTED:
    return "CUFFT_NOT_IMPLEMENTED";
  case CUFFT_LICENSE_ERROR:
    return "CUFFT_LICENSE_ERROR";
  case CUFFT_NOT_SUPPORTED:
    return "CUFFT_NOT_SUPPORTED";
  default:
    return "unknown error";
  }
}
#endif

#if defined(PETSC_HAVE_HIP)
  #include <petscdevice_hip.h>
PETSC_EXTERN const char *PetscHIPBLASGetErrorName(hipblasStatus_t status)
{
  switch (status) {
  case HIPBLAS_STATUS_SUCCESS:
    return "HIPBLAS_STATUS_SUCCESS";
  case HIPBLAS_STATUS_NOT_INITIALIZED:
    return "HIPBLAS_STATUS_NOT_INITIALIZED";
  case HIPBLAS_STATUS_ALLOC_FAILED:
    return "HIPBLAS_STATUS_ALLOC_FAILED";
  case HIPBLAS_STATUS_INVALID_VALUE:
    return "HIPBLAS_STATUS_INVALID_VALUE";
  case HIPBLAS_STATUS_ARCH_MISMATCH:
    return "HIPBLAS_STATUS_ARCH_MISMATCH";
  case HIPBLAS_STATUS_MAPPING_ERROR:
    return "HIPBLAS_STATUS_MAPPING_ERROR";
  case HIPBLAS_STATUS_EXECUTION_FAILED:
    return "HIPBLAS_STATUS_EXECUTION_FAILED";
  case HIPBLAS_STATUS_INTERNAL_ERROR:
    return "HIPBLAS_STATUS_INTERNAL_ERROR";
  case HIPBLAS_STATUS_NOT_SUPPORTED:
    return "HIPBLAS_STATUS_NOT_SUPPORTED";
  default:
    return "unknown error";
  }
}
PETSC_EXTERN const char *PetscHIPSPARSEGetErrorName(hipsparseStatus_t status)
{
  switch (status) {
  case HIPSPARSE_STATUS_SUCCESS:
    return "HIPSPARSE_STATUS_SUCCESS";
  case HIPSPARSE_STATUS_NOT_INITIALIZED:
    return "HIPSPARSE_STATUS_NOT_INITIALIZED";
  case HIPSPARSE_STATUS_ALLOC_FAILED:
    return "HIPSPARSE_STATUS_ALLOC_FAILED";
  case HIPSPARSE_STATUS_INVALID_VALUE:
    return "HIPSPARSE_STATUS_INVALID_VALUE";
  case HIPSPARSE_STATUS_ARCH_MISMATCH:
    return "HIPSPARSE_STATUS_ARCH_MISMATCH";
  case HIPSPARSE_STATUS_MAPPING_ERROR:
    return "HIPSPARSE_STATUS_MAPPING_ERROR";
  case HIPSPARSE_STATUS_EXECUTION_FAILED:
    return "HIPSPARSE_STATUS_EXECUTION_FAILED";
  case HIPSPARSE_STATUS_INTERNAL_ERROR:
    return "HIPSPARSE_STATUS_INTERNAL_ERROR";
  case HIPSPARSE_STATUS_MATRIX_TYPE_NOT_SUPPORTED:
    return "HIPSPARSE_STATUS_MATRIX_TYPE_NOT_SUPPORTED";
  case HIPSPARSE_STATUS_ZERO_PIVOT:
    return "HIPSPARSE_STATUS_ZERO_PIVOT";
  case HIPSPARSE_STATUS_NOT_SUPPORTED:
    return "HIPSPARSE_STATUS_NOT_SUPPORTED";
  case HIPSPARSE_STATUS_INSUFFICIENT_RESOURCES:
    return "HIPSPARSE_STATUS_INSUFFICIENT_RESOURCES";
  default:
    return "unknown error";
  }
}
PETSC_EXTERN const char *PetscHIPSolverGetErrorName(hipsolverStatus_t status)
{
  switch (status) {
  case HIPSOLVER_STATUS_SUCCESS:
    return "HIPSOLVER_STATUS_SUCCESS";
  case HIPSOLVER_STATUS_NOT_INITIALIZED:
    return "HIPSOLVER_STATUS_NOT_INITIALIZED";
  case HIPSOLVER_STATUS_ALLOC_FAILED:
    return "HIPSOLVER_STATUS_ALLOC_FAILED";
  case HIPSOLVER_STATUS_MAPPING_ERROR:
    return "HIPSOLVER_STATUS_MAPPING_ERROR";
  case HIPSOLVER_STATUS_INVALID_VALUE:
    return "HIPSOLVER_STATUS_INVALID_VALUE";
  case HIPSOLVER_STATUS_EXECUTION_FAILED:
    return "HIPSOLVER_STATUS_EXECUTION_FAILED";
  case HIPSOLVER_STATUS_INTERNAL_ERROR:
    return "HIPSOLVER_STATUS_INTERNAL_ERROR";
  case HIPSOLVER_STATUS_NOT_SUPPORTED:
    return "HIPSOLVER_STATUS_NOT_SUPPORTED ";
  case HIPSOLVER_STATUS_ARCH_MISMATCH:
    return "HIPSOLVER_STATUS_ARCH_MISMATCH";
  case HIPSOLVER_STATUS_HANDLE_IS_NULLPTR:
    return "HIPSOLVER_STATUS_HANDLE_IS_NULLPTR";
  case HIPSOLVER_STATUS_INVALID_ENUM:
    return "HIPSOLVER_STATUS_INVALID_ENUM";
  case HIPSOLVER_STATUS_UNKNOWN:
  default:
    return "HIPSOLVER_STATUS_UNKNOWN";
  }
}
#endif

/*@
      PetscMPIErrorString - Given an MPI error code returns the `MPI_Error_string()` appropriately
           formatted for displaying with the PETSc error handlers.

 Input Parameter:
.  err - the MPI error code

 Output Parameter:
.  string - the MPI error message, should declare its length to be larger than `MPI_MAX_ERROR_STRING`

   Level: developer

 Note:
    Does not return an error code or do error handling because it may be called from inside an error handler

@*/
void PetscMPIErrorString(PetscMPIInt err, char *string)
{
  char        errorstring[MPI_MAX_ERROR_STRING];
  PetscMPIInt len, j = 0;

  MPI_Error_string(err, (char *)errorstring, &len);
  for (PetscMPIInt i = 0; i < len; i++) {
    string[j++] = errorstring[i];
    if (errorstring[i] == '\n') {
      for (PetscMPIInt k = 0; k < 16; k++) string[j++] = ' ';
    }
  }
  string[j] = 0;
}

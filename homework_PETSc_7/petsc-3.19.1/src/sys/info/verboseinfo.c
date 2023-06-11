/*
      PetscInfo() is contained in a different file from the other profiling to
   allow it to be replaced at link time by an alternative routine.
*/
#include <petsc/private/petscimpl.h> /*I    "petscsys.h"   I*/

/*
  The next set of variables determine which, if any, PetscInfo() calls are used.
  If PetscLogPrintInfo is false, no info messages are printed.

  If PetscInfoFlags[OBJECT_CLASSID - PETSC_SMALLEST_CLASSID] is zero, no messages related
  to that object are printed. OBJECT_CLASSID is, for example, MAT_CLASSID.
  Note for developers: the PetscInfoFlags array is currently 160 entries large, to ensure headroom. Perhaps it is worth
  dynamically allocating this array intelligently rather than just some big number.

  PetscInfoFilename determines where PetscInfo() output is piped.
  PetscInfoClassnames holds a char array of classes which are filtered out/for in PetscInfo() calls.
*/
const char *const        PetscInfoCommFlags[]   = {"all", "no_self", "only_self", "PetscInfoCommFlag", "PETSC_INFO_COMM_", NULL};
static PetscBool         PetscInfoClassesLocked = PETSC_FALSE, PetscInfoInvertClasses = PETSC_FALSE, PetscInfoClassesSet = PETSC_FALSE;
static char            **PetscInfoClassnames                                       = NULL;
static char             *PetscInfoFilename                                         = NULL;
static PetscInt          PetscInfoNumClasses                                       = -1;
static PetscInfoCommFlag PetscInfoCommFilter                                       = PETSC_INFO_COMM_ALL;
static int               PetscInfoFlags[]                                          = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                                                                      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                                                                      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static char             *PetscInfoNames[PETSC_STATIC_ARRAY_LENGTH(PetscInfoFlags)] = {NULL};
PetscBool                PetscLogPrintInfo                                         = PETSC_FALSE;
FILE                    *PetscInfoFile                                             = NULL;

/*@
    PetscInfoEnabled - Checks whether a given OBJECT_CLASSID is allowed to print using `PetscInfo()`

    Not Collective

    Input Parameter:
.   classid - `PetscClassid` retrieved from a `PetscObject` e.g. `VEC_CLASSID`

    Output Parameter:
.   enabled - `PetscBool` indicating whether this classid is allowed to print

    Level: advanced

    Note:
    Use `PETSC_SMALLEST_CLASSID` to check if "sys" `PetscInfo()` calls are enabled. When PETSc is configured with debugging
    support this function checks if classid >= `PETSC_SMALLEST_CLASSID`, otherwise it assumes valid classid.

.seealso: `PetscInfo()`, `PetscInfoAllow()`, `PetscInfoGetInfo()`, `PetscObjectGetClassid()`
@*/
PetscErrorCode PetscInfoEnabled(PetscClassId classid, PetscBool *enabled)
{
  PetscFunctionBegin;
  PetscValidBoolPointer(enabled, 2);
  PetscCheck(classid >= PETSC_SMALLEST_CLASSID, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Classid (current: %d) must be equal to or greater than PETSC_SMALLEST_CLASSID", classid);
  *enabled = (PetscBool)(PetscLogPrintInfo && PetscInfoFlags[classid - PETSC_SMALLEST_CLASSID]);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscInfoAllow - Enables/disables `PetscInfo()` messages

    Not Collective

    Input Parameter:
.   flag - `PETSC_TRUE` or `PETSC_FALSE`

    Level: advanced

.seealso: `PetscInfo()`, `PetscInfoEnabled()`, `PetscInfoGetInfo()`, `PetscInfoSetFromOptions()`
@*/
PetscErrorCode PetscInfoAllow(PetscBool flag)
{
  PetscFunctionBegin;
  PetscLogPrintInfo = flag;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscInfoSetFile - Sets the printing destination for all `PetscInfo()` calls

    Not Collective

    Input Parameters:
+   filename - Name of the file where `PetscInfo()` will print to
-   mode - Write mode passed to `PetscFOpen()`

    Level: advanced

    Note:
    Use `filename = NULL` to set `PetscInfo()` to write to `PETSC_STDOUT`.

.seealso: `PetscInfo()`, `PetscInfoSetFile()`, `PetscInfoSetFromOptions()`, `PetscFOpen()`
@*/
PetscErrorCode PetscInfoSetFile(const char filename[], const char mode[])
{
  PetscFunctionBegin;
  if (!PetscInfoFile) PetscInfoFile = PETSC_STDOUT;
  PetscCall(PetscFree(PetscInfoFilename));
  if (filename) {
    PetscMPIInt rank;
    char        fname[PETSC_MAX_PATH_LEN], tname[11];

    PetscValidCharPointer(filename, 1);
    PetscValidCharPointer(mode, 2);
    PetscCall(PetscFixFilename(filename, fname));
    PetscCall(PetscStrallocpy(fname, &PetscInfoFilename));
    PetscCallMPI(MPI_Comm_rank(PETSC_COMM_WORLD, &rank));
    PetscCall(PetscSNPrintf(tname, PETSC_STATIC_ARRAY_LENGTH(tname), ".%d", rank));
    PetscCall(PetscStrlcat(fname, tname, PETSC_STATIC_ARRAY_LENGTH(fname)));
    {
      const PetscBool oldflag = PetscLogPrintInfo;

      PetscLogPrintInfo = PETSC_FALSE;
      PetscCall(PetscFOpen(PETSC_COMM_SELF, fname, mode, &PetscInfoFile));
      PetscLogPrintInfo = oldflag;
      /*
        PetscFOpen will write to PETSC_STDOUT and not PetscInfoFile here, so we disable the
        PetscInfo call inside it, and call it afterwards so that it actually writes to file
      */
    }
    PetscCall(PetscInfo(NULL, "Opened PetscInfo file %s\n", fname));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscInfoGetFile - Gets the `filename` and `FILE` pointer of the file where `PetscInfo()` prints to

    Not Collective; No Fortran Support

    Output Parameters:
+   filename - The name of the output file
-   InfoFile - The `FILE` pointer for the output file

    Level: advanced

    Note:
    This routine allocates and copies the `filename` so that the `filename` survives `PetscInfoDestroy()`. The user is
    therefore responsible for freeing the allocated `filename` pointer afterwards.

.seealso: `PetscInfo()`, `PetscInfoSetFile()`, `PetscInfoSetFromOptions()`, `PetscInfoDestroy()`
@*/
PetscErrorCode PetscInfoGetFile(char **filename, FILE **InfoFile)
{
  PetscFunctionBegin;
  PetscValidPointer(filename, 1);
  PetscValidPointer(InfoFile, 2);
  PetscCall(PetscStrallocpy(PetscInfoFilename, filename));
  *InfoFile = PetscInfoFile;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscInfoSetClasses - Sets the classes which `PetscInfo()` is filtered for/against

    Not Collective; No Fortran Support

    Input Parameters:
+   exclude - Whether or not to invert the filter, i.e. if exclude is true, `PetscInfo()` will print from every class that
    is NOT one of the classes specified
.   n - Number of classes to filter for (size of `classnames`)
-   classnames - String array containing the names of classes to filter for, e.g. "vec"

    Level: developer

    Notes:
    This function CANNOT be called after `PetscInfoGetClass()` or `PetscInfoProcessClass()` has been called.

    Names in the `classnames` list should correspond to the names returned by `PetscObjectGetClassName()`.

    This function only sets the list of class names.
    The actual filtering is deferred to `PetscInfoProcessClass()`, except of sys which is processed right away.
    The reason for this is that we need to set the list of included/excluded classes before their classids are known.
    Typically the classid is assigned and `PetscInfoProcessClass()` called in <Class>InitializePackage() (e.g. `VecInitializePackage()`).

.seealso: `PetscInfo()`, `PetscInfoGetClass()`, `PetscInfoProcessClass()`, `PetscInfoSetFromOptions()`, `PetscStrToArray()`, `PetscObjectGetName()`
@*/
PetscErrorCode PetscInfoSetClasses(PetscBool exclude, PetscInt n, const char *const *classnames)
{
  PetscFunctionBegin;
  PetscCheck(!PetscInfoClassesLocked, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "PetscInfoSetClasses() cannot be called after PetscInfoGetClass() or PetscInfoProcessClass()");
  PetscCall(PetscStrNArrayDestroy(PetscInfoNumClasses, &PetscInfoClassnames));
  PetscCall(PetscStrNArrayallocpy(n, classnames, &PetscInfoClassnames));
  PetscInfoNumClasses    = n;
  PetscInfoInvertClasses = exclude;
  /* Process sys class right away */
  {
    const PetscClassId id = PETSC_SMALLEST_CLASSID;

    PetscCall(PetscInfoProcessClass("sys", 1, &id));
  }
  PetscInfoClassesSet = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscInfoGetClass - Indicates whether the provided `classname` is marked as a filter in `PetscInfo()` as set by `PetscInfoSetClasses()`

    Not Collective

    Input Parameter:
.   classname - Name of the class to search for

    Output Parameter:
.   found - `PetscBool` indicating whether the classname was found

    Level: developer

    Note:
    Use `PetscObjectGetName()` to retrieve an appropriate classname

.seealso: `PetscInfo()`, `PetscInfoSetClasses()`, `PetscInfoSetFromOptions()`, `PetscObjectGetName()`
@*/
PetscErrorCode PetscInfoGetClass(const char *classname, PetscBool *found)
{
  PetscInt unused;

  PetscFunctionBegin;
  PetscValidCharPointer(classname, 1);
  PetscValidBoolPointer(found, 2);
  PetscCall(PetscEListFind(PetscInfoNumClasses, (const char *const *)PetscInfoClassnames, classname ? classname : "sys", &unused, found));
  PetscInfoClassesLocked = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscInfoGetInfo - Returns the current state of several important flags for `PetscInfo()`

    Not Collective

    Output Parameters:
+   infoEnabled - `PETSC_TRUE` if `PetscInfoAllow`(`PETSC_TRUE`) has been called
.   classesSet - `PETSC_TRUE` if the list of classes to filter for has been set
.   exclude - `PETSC_TRUE` if the class filtering for `PetscInfo()` is inverted
.   locked - `PETSC_TRUE` if the list of classes to filter for has been locked
-   commSelfFlag - Enum indicating whether `PetscInfo()` will print for communicators of size 1, any size != 1, or all
    communicators

    Level: developer

    Note:
    Initially commSelfFlag = `PETSC_INFO_COMM_ALL`

.seealso: `PetscInfo()`, `PetscInfoAllow()`, `PetscInfoSetFilterCommSelf`, `PetscInfoSetFromOptions()`
@*/
PetscErrorCode PetscInfoGetInfo(PetscBool *infoEnabled, PetscBool *classesSet, PetscBool *exclude, PetscBool *locked, PetscInfoCommFlag *commSelfFlag)
{
  PetscFunctionBegin;
  if (infoEnabled) PetscValidBoolPointer(infoEnabled, 1);
  if (classesSet) PetscValidBoolPointer(classesSet, 2);
  if (exclude) PetscValidBoolPointer(exclude, 3);
  if (locked) PetscValidBoolPointer(locked, 4);
  if (commSelfFlag) PetscValidPointer(commSelfFlag, 5);
  if (infoEnabled) *infoEnabled = PetscLogPrintInfo;
  if (classesSet) *classesSet = PetscInfoClassesSet;
  if (exclude) *exclude = PetscInfoInvertClasses;
  if (locked) *locked = PetscInfoClassesLocked;
  if (commSelfFlag) *commSelfFlag = PetscInfoCommFilter;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscInfoProcessClass - Activates or deactivates a class based on the filtering status of `PetscInfo()`

    Not Collective

    Input Parameters:
+   classname - Name of the class to activate/deactivate `PetscInfo()` for
.   numClassID - Number of entries in `classIDs`
-   classIDs - Array containing all of the `PetscClassId`s associated with `classname`

      Options Database Key:
.   -info [filename][:[~]<list,of,classnames>[:[~]self]] - specify which informative messages are printed, see `PetscInfo()`.

    Level: developer

.seealso: `PetscInfo()`, `PetscInfoActivateClass()`, `PetscInfoDeactivateClass()`, `PetscInfoSetFromOptions()`
@*/
PetscErrorCode PetscInfoProcessClass(const char classname[], PetscInt numClassID, const PetscClassId classIDs[])
{
  PetscBool enabled, exclude, found, opt;
  char      logList[256];

  PetscFunctionBegin;
  PetscValidCharPointer(classname, 1);
  PetscAssert(numClassID > 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Number of classids %" PetscInt_FMT " <= 0", numClassID);
  if (numClassID) PetscValidPointer(classIDs, 3);
  PetscCall(PetscInfoGetInfo(&enabled, NULL, &exclude, NULL, NULL));
  PetscCall(PetscOptionsDeprecated_Private(NULL, "-info_exclude", NULL, "3.13", "Use ~ with -info to indicate classes to exclude"));
  PetscCall(PetscOptionsGetString(NULL, NULL, "-info_exclude", logList, sizeof(logList), &opt));
  if (opt) {
    PetscBool pkg;

    PetscCall(PetscStrInList(classname, logList, ',', &pkg));
    if (pkg) {
      for (PetscInt i = 0; i < numClassID; ++i) PetscCall(PetscInfoDeactivateClass(classIDs[i]));
    }
  }
  for (PetscInt i = 0; i < numClassID; ++i) {
    const PetscClassId idx = classIDs[i] - PETSC_SMALLEST_CLASSID;

    PetscCall(PetscFree(PetscInfoNames[idx]));
    PetscCall(PetscStrallocpy(classname, PetscInfoNames + idx));
  }
  PetscCall(PetscInfoGetClass(classname, &found));
  if ((found && exclude) || (!found && !exclude)) {
    if (PetscInfoNumClasses > 0) {
      /* Check if -info was called empty */
      for (PetscInt i = 0; i < numClassID; ++i) PetscCall(PetscInfoDeactivateClass(classIDs[i]));
    }
  } else {
    for (PetscInt i = 0; i < numClassID; ++i) PetscCall(PetscInfoActivateClass(classIDs[i]));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscInfoSetFilterCommSelf - Sets `PetscInfoCommFlag` enum to determine communicator filtering for `PetscInfo()`

    Not Collective

    Input Parameter:
.   commSelfFlag - Enum value indicating method with which to filter `PetscInfo()` based on the size of the communicator of the object calling `PetscInfo()`

    Options Database Key:
.   -info [filename][:[~]<list,of,classnames>[:[~]self]] - specify which informative messages are printed, See `PetscInfo()`.

    Level: advanced

.seealso: `PetscInfo()`, `PetscInfoGetInfo()`
@*/
PetscErrorCode PetscInfoSetFilterCommSelf(PetscInfoCommFlag commSelfFlag)
{
  PetscFunctionBegin;
  PetscInfoCommFilter = commSelfFlag;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscInfoSetFromOptions - Configure `PetscInfo()` using command line options, enabling or disabling various calls to `PetscInfo()`

    Not Collective

    Input Parameter:
.   options - Options database, use `NULL` for default global database

    Options Database Key:
.   -info [filename][:[~]<list,of,classnames>[:[~]self]] - specify which informative messages are printed, See `PetscInfo()`.

    Level: advanced

    Note:
    This function is called automatically during `PetscInitialize()` so users usually do not need to call it themselves.

.seealso: `PetscInfo()`, `PetscInfoAllow()`, `PetscInfoSetFile()`, `PetscInfoSetClasses()`, `PetscInfoSetFilterCommSelf()`, `PetscInfoDestroy()`
@*/
PetscErrorCode PetscInfoSetFromOptions(PetscOptions options)
{
  char      optstring[PETSC_MAX_PATH_LEN];
  PetscBool set;

  PetscFunctionBegin;
  PetscCall(PetscOptionsGetString(options, NULL, "-info", optstring, PETSC_STATIC_ARRAY_LENGTH(optstring), &set));
  if (set) {
    size_t            size_loc0_, size_loc1_, size_loc2_;
    char             *loc0_ = NULL, *loc1_ = NULL, *loc2_ = NULL;
    char            **loc1_array  = NULL;
    PetscBool         loc1_invert = PETSC_FALSE, loc2_invert = PETSC_FALSE;
    int               nLoc1_       = 0;
    PetscInfoCommFlag commSelfFlag = PETSC_INFO_COMM_ALL;

    PetscInfoClassesSet = PETSC_TRUE;
    PetscCall(PetscInfoAllow(PETSC_TRUE));
    PetscCall(PetscStrallocpy(optstring, &loc0_));
    PetscCall(PetscStrchr(loc0_, ':', &loc1_));
    if (loc1_) {
      *loc1_++ = 0;
      if (*loc1_ == '~') {
        loc1_invert = PETSC_TRUE;
        ++loc1_;
      }
      PetscCall(PetscStrchr(loc1_, ':', &loc2_));
    }
    if (loc2_) {
      *loc2_++ = 0;
      if (*loc2_ == '~') {
        loc2_invert = PETSC_TRUE;
        ++loc2_;
      }
    }
    PetscCall(PetscStrlen(loc0_, &size_loc0_));
    PetscCall(PetscStrlen(loc1_, &size_loc1_));
    PetscCall(PetscStrlen(loc2_, &size_loc2_));
    if (size_loc1_) {
      PetscCall(PetscStrtolower(loc1_));
      PetscCall(PetscStrToArray(loc1_, ',', &nLoc1_, &loc1_array));
    }
    if (size_loc2_) {
      PetscBool foundSelf;

      PetscCall(PetscStrtolower(loc2_));
      PetscCall(PetscStrcmp("self", loc2_, &foundSelf));
      if (foundSelf) commSelfFlag = loc2_invert ? PETSC_INFO_COMM_NO_SELF : PETSC_INFO_COMM_ONLY_SELF;
    }
    PetscCall(PetscInfoSetFile(size_loc0_ ? loc0_ : NULL, "w"));
    PetscCall(PetscInfoSetClasses(loc1_invert, (PetscInt)nLoc1_, (const char *const *)loc1_array));
    PetscCall(PetscInfoSetFilterCommSelf(commSelfFlag));
    PetscCall(PetscStrToArrayDestroy(nLoc1_, loc1_array));
    PetscCall(PetscFree(loc0_));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscInfoDestroy - Destroys and resets internal `PetscInfo()` data structures.

  Not Collective

  Level: developer

  Note:
  This is automatically called in `PetscFinalize()`. Useful for changing filters mid-program, or culling subsequent
  `PetscInfo()` calls down the line.

.seealso: `PetscInfo()`, `PetscInfoSetFromOptions()`
@*/
PetscErrorCode PetscInfoDestroy(void)
{
  PetscFunctionBegin;
  PetscCall(PetscInfoAllow(PETSC_FALSE));
  PetscCall(PetscStrNArrayDestroy(PetscInfoNumClasses, &PetscInfoClassnames));
  PetscCall(PetscFFlush(PetscInfoFile));
  if (PetscInfoFilename) PetscCall(PetscFClose(PETSC_COMM_SELF, PetscInfoFile));
  PetscCall(PetscFree(PetscInfoFilename));
  PetscAssert(PETSC_STATIC_ARRAY_LENGTH(PetscInfoFlags) == PETSC_STATIC_ARRAY_LENGTH(PetscInfoNames), PETSC_COMM_SELF, PETSC_ERR_PLIB, "PetscInfoFlags and PetscInfoNames must be the same size");
  for (size_t i = 0; i < PETSC_STATIC_ARRAY_LENGTH(PetscInfoFlags); ++i) {
    PetscInfoFlags[i] = 1;
    PetscCall(PetscFree(PetscInfoNames[i]));
  }

  PetscInfoClassesLocked = PETSC_FALSE;
  PetscInfoInvertClasses = PETSC_FALSE;
  PetscInfoClassesSet    = PETSC_FALSE;
  PetscInfoNumClasses    = -1;
  PetscInfoCommFilter    = PETSC_INFO_COMM_ALL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscInfoSetClassActivation_Private(PetscClassId classid, int value)
{
  PetscFunctionBegin;
  if (!classid) classid = PETSC_SMALLEST_CLASSID;
  PetscInfoFlags[classid - PETSC_SMALLEST_CLASSID] = value;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscInfoDeactivateClass - Deactivates `PetscInfo()` messages for a PETSc object class.

  Not Collective

  Input Parameter:
. classid - The object class,  e.g., `MAT_CLASSID`, `SNES_CLASSID`, etc.

    Options Database Key:
.   -info [filename][:[~]<list,of,classnames>[:[~]self]] - specify which informative messages are printed, See `PetscInfo()`.

  Level: developer

  Note:
  One can pass 0 to deactivate all messages that are not associated with an object.

.seealso: `PetscInfoActivateClass()`, `PetscInfo()`, `PetscInfoAllow()`, `PetscInfoSetFromOptions()`
@*/
PetscErrorCode PetscInfoDeactivateClass(PetscClassId classid)
{
  PetscFunctionBegin;
  PetscCall(PetscInfoSetClassActivation_Private(classid, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscInfoActivateClass - Activates `PetscInfo()` messages for a PETSc object class.

  Not Collective

  Input Parameter:
. classid - The object class, e.g., `MAT_CLASSID`, `SNES_CLASSID`, etc.

    Options Database Key:
.   -info [filename][:[~]<list,of,classnames>[:[~]self]] - specify which informative messages are printed, See `PetscInfo()`.

  Level: developer

  Note:
  One can pass 0 to activate all messages that are not associated with an object.

.seealso: `PetscInfoDeactivateClass()`, `PetscInfo()`, `PetscInfoAllow()`, `PetscInfoSetFromOptions()`
@*/
PetscErrorCode PetscInfoActivateClass(PetscClassId classid)
{
  PetscFunctionBegin;
  PetscCall(PetscInfoSetClassActivation_Private(classid, 1));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   If the option -history was used, then all printed PetscInfo()
  messages are also printed to the history file, called by default
  .petschistory in ones home directory.
*/
PETSC_INTERN FILE *petsc_history;

/*MC
    PetscInfo - Logs informative data

   Synopsis:
       #include <petscsys.h>
       PetscErrorCode PetscInfo(PetscObject obj, const char message[])
       PetscErrorCode PetscInfo(PetscObject obj, const char formatmessage[],arg1)
       PetscErrorCode PetscInfo(PetscObject obj, const char formatmessage[],arg1,arg2)
       ...

    Collective

    Input Parameters:
+   obj - object most closely associated with the logging statement or `NULL`
.   message - logging message
.   formatmessage - logging message using standard "printf" format
-   arg1, arg2, ... - arguments of the format

      Options Database Key:
.   -info [filename][:[~]<list,of,classnames>[:[~]self]] - specify which informative messages are printed, See `PetscInfo()`.

    Level: intermediate

    Notes:
    `PetscInfo()` prints only from the first processor in the communicator of obj.
    If obj is NULL, the `PETSC_COMM_SELF` communicator is used, i.e. every rank of `PETSC_COMM_WORLD` prints the message.

    The optional <list,of,classnames> is a comma separated list of enabled classes, e.g. vec,mat,ksp.
    If this list is not specified, all classes are enabled.
    Prepending the list with ~ means inverted selection, i.e. all classes except the listed are enabled.
    A special classname `sys` relates to `PetscInfo()` with obj being `NULL`.

    The optional keyword `self` specifies that `PetscInfo()` is enabled only for a communicator size of 1 (e.g. `PETSC_COMM_SELF`).
    By contrast, ~self means that `PetscInfo()` is enabled only for communicator size > 1 (e.g. `PETSC_COMM_WORLD`), i.e. those `PetscInfo()` calls which print from every rank of `PETSC_COMM_WORLD` are disabled.

    All classname/self matching is case insensitive. Filename is case sensitive.

    Example of Usage:
.vb
     Mat A;
     PetscInt alpha;
     ...
     PetscInfo(A,"Matrix uses parameter alpha=%" PetscInt_FMT "\n",alpha);
.ve

    Options Examples:
    Each call of the form
.vb
     PetscInfo(obj, msg);
     PetscInfo(obj, msg, arg1);
     PetscInfo(obj, msg, arg1, arg2);
.ve
    is evaluated as follows.
.vb
    -info or -info :: prints msg to PETSC_STDOUT, for any obj regardless class or communicator
    -info :mat:self prints msg to PETSC_STDOUT only if class of obj is Mat, and its communicator has size = 1
    -info myInfoFileName:~vec:~self prints msg to file named myInfoFileName, only if the obj's class is NULL or other than Vec, and obj's communicator has size > 1
    -info :sys prints to PETSC_STDOUT only if obj is NULL
    -info :sys:~self deactivates all info messages because sys means obj = NULL which implies PETSC_COMM_SELF but ~self filters out everything on PETSC_COMM_SELF.
.ve
    Fortran Note:
    This function does not take the `obj` argument, there is only the `PetscInfo()`
     version, not `PetscInfo()` etc.

.seealso: `PetscInfoAllow()`, `PetscInfoSetFromOptions()`
M*/
PetscErrorCode PetscInfo_Private(const char func[], PetscObject obj, const char message[], ...)
{
  PetscClassId classid = PETSC_SMALLEST_CLASSID;
  PetscBool    enabled = PETSC_FALSE;
  MPI_Comm     comm    = PETSC_COMM_SELF;
  PetscMPIInt  rank;

  PetscFunctionBegin;
  if (obj) {
    PetscValidHeader(obj, 2);
    classid = obj->classid;
  }
  PetscValidCharPointer(message, 3);
  PetscCall(PetscInfoEnabled(classid, &enabled));
  if (!enabled) PetscFunctionReturn(PETSC_SUCCESS);
  if (obj) PetscCall(PetscObjectGetComm(obj, &comm));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  /* rank > 0 always jumps out */
  if (rank) PetscFunctionReturn(PETSC_SUCCESS);
  else {
    PetscMPIInt size;

    PetscCallMPI(MPI_Comm_size(comm, &size));
    /* If no self printing is allowed, and size too small, get out */
    if ((PetscInfoCommFilter == PETSC_INFO_COMM_NO_SELF) && (size < 2)) PetscFunctionReturn(PETSC_SUCCESS);
    /* If ONLY self printing, and size too big, get out */
    if ((PetscInfoCommFilter == PETSC_INFO_COMM_ONLY_SELF) && (size > 1)) PetscFunctionReturn(PETSC_SUCCESS);
  }
  /* Mute info messages within this function */
  {
    const PetscBool oldflag = PetscLogPrintInfo;
    va_list         Argp;
    PetscMPIInt     urank;
    char            string[8 * 1024];
    size_t          fullLength, len;

    PetscLogPrintInfo = PETSC_FALSE;
    PetscCallMPI(MPI_Comm_rank(MPI_COMM_WORLD, &urank));
    PetscCall(PetscSNPrintf(string, PETSC_STATIC_ARRAY_LENGTH(string), "[%d] <%s> %s(): ", urank, PetscInfoNames[classid - PETSC_SMALLEST_CLASSID], func));
    PetscCall(PetscStrlen(string, &len));
    va_start(Argp, message);
    PetscCall(PetscVSNPrintf(string + len, 8 * 1024 - len, message, &fullLength, Argp));
    va_end(Argp);
    PetscCall(PetscFPrintf(PETSC_COMM_SELF, PetscInfoFile, "%s", string));
    PetscCall(PetscFFlush(PetscInfoFile));
    if (petsc_history) {
      va_start(Argp, message);
      PetscCall((*PetscVFPrintf)(petsc_history, message, Argp));
      va_end(Argp);
    }
    PetscLogPrintInfo = oldflag;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

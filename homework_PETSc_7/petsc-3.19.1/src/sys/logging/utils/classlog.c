
/*
     This defines part of the private API for logging performance information. It is intended to be used only by the
   PETSc PetscLog...() interface and not elsewhere, nor by users. Hence the prototypes for these functions are NOT
   in the public PETSc include files.

*/
#include <petsc/private/logimpl.h> /*I    "petscsys.h"   I*/

/*@C
  PetscClassRegLogCreate - This creates a `PetscClassRegLog` object.

  Not Collective

  Input Parameter:
. classLog - The `PetscClassRegLog`

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscClassRegLogDestroy()`, `PetscStageLogCreate()`
@*/
PetscErrorCode PetscClassRegLogCreate(PetscClassRegLog *classLog)
{
  PetscClassRegLog l;

  PetscFunctionBegin;
  PetscCall(PetscNew(&l));

  l->numClasses = 0;
  l->maxClasses = 100;

  PetscCall(PetscMalloc1(l->maxClasses, &l->classInfo));

  *classLog = l;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscClassRegLogDestroy - This destroys a `PetscClassRegLog` object.

  Not Collective

  Input Parameter:
. classLog - The `PetscClassRegLog`

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscClassRegLogCreate()`
@*/
PetscErrorCode PetscClassRegLogDestroy(PetscClassRegLog classLog)
{
  int c;

  PetscFunctionBegin;
  for (c = 0; c < classLog->numClasses; c++) PetscCall(PetscClassRegInfoDestroy(&classLog->classInfo[c]));
  PetscCall(PetscFree(classLog->classInfo));
  PetscCall(PetscFree(classLog));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscClassRegInfoDestroy - This destroys a `PetscClassRegInfo` object.

  Not Collective

  Input Parameter:
. c - The PetscClassRegInfo

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscStageLogDestroy()`, `EventLogDestroy()`
@*/
PetscErrorCode PetscClassRegInfoDestroy(PetscClassRegInfo *c)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(c->name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscClassPerfLogCreate - This creates a `PetscClassPerfLog` object.

  Not Collective

  Input Parameter:
. classLog - The `PetscClassPerfLog`

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscClassPerfLogDestroy()`, `PetscStageLogCreate()`
@*/
PetscErrorCode PetscClassPerfLogCreate(PetscClassPerfLog *classLog)
{
  PetscClassPerfLog l;

  PetscFunctionBegin;
  PetscCall(PetscNew(&l));

  l->numClasses = 0;
  l->maxClasses = 100;

  PetscCall(PetscMalloc1(l->maxClasses, &l->classInfo));

  *classLog = l;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscClassPerfLogDestroy - This destroys a `PetscClassPerfLog` object.

  Not Collective

  Input Parameter:
. classLog - The `PetscClassPerfLog`

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscClassPerfLogCreate()`
@*/
PetscErrorCode PetscClassPerfLogDestroy(PetscClassPerfLog classLog)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(classLog->classInfo));
  PetscCall(PetscFree(classLog));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*------------------------------------------------ General Functions -------------------------------------------------*/
/*@C
  PetscClassPerfInfoClear - This clears a `PetscClassPerfInfo` object.

  Not Collective

  Input Parameter:
. classInfo - The `PetscClassPerfInfo`

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscClassPerfLogCreate()`
@*/
PetscErrorCode PetscClassPerfInfoClear(PetscClassPerfInfo *classInfo)
{
  PetscFunctionBegin;
  classInfo->id           = -1;
  classInfo->creations    = 0;
  classInfo->destructions = 0;
  classInfo->mem          = 0.0;
  classInfo->descMem      = 0.0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscClassPerfLogEnsureSize - This ensures that a `PetscClassPerfLog` is at least of a certain size.

  Not Collective

  Input Parameters:
+ classLog - The `PetscClassPerfLog`
- size     - The size

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscClassPerfLogCreate()`
@*/
PetscErrorCode PetscClassPerfLogEnsureSize(PetscClassPerfLog classLog, int size)
{
  PetscClassPerfInfo *classInfo;

  PetscFunctionBegin;
  while (size > classLog->maxClasses) {
    PetscCall(PetscMalloc1(classLog->maxClasses * 2, &classInfo));
    PetscCall(PetscArraycpy(classInfo, classLog->classInfo, classLog->maxClasses));
    PetscCall(PetscFree(classLog->classInfo));

    classLog->classInfo = classInfo;
    classLog->maxClasses *= 2;
  }
  while (classLog->numClasses < size) PetscCall(PetscClassPerfInfoClear(&classLog->classInfo[classLog->numClasses++]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*--------------------------------------------- Registration Functions ----------------------------------------------*/
/*@C
  PetscClassRegLogRegister - Registers a class for logging operations in an application code.

  Not Collective

  Input Parameters:
+ classLog - The `PetscClassRegLog`
- cname    - The name associated with the class

  Output Parameter:
.  classid   - The classid

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscClassIdRegister()`
@*/
PetscErrorCode PetscClassRegLogRegister(PetscClassRegLog classLog, const char cname[], PetscClassId classid)
{
  PetscClassRegInfo *classInfo;
  int                c;

  PetscFunctionBegin;
  PetscValidCharPointer(cname, 2);
  c = classLog->numClasses++;
  if (classLog->numClasses > classLog->maxClasses) {
    PetscCall(PetscMalloc1(classLog->maxClasses * 2, &classInfo));
    PetscCall(PetscArraycpy(classInfo, classLog->classInfo, classLog->maxClasses));
    PetscCall(PetscFree(classLog->classInfo));

    classLog->classInfo = classInfo;
    classLog->maxClasses *= 2;
  }
  PetscCall(PetscStrallocpy(cname, &(classLog->classInfo[c].name)));
  classLog->classInfo[c].classid = classid;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*------------------------------------------------ Query Functions --------------------------------------------------*/
/*@C
  PetscClassRegLogGetClass - This function returns the class corresponding to a given classid.

  Not Collective

  Input Parameters:
+ classLog - The `PetscClassRegLog`
- classid  - The cookie

  Output Parameter:
. oclass   - The class id

  Level: developer

  Note:
  This is a low level routine used by the logging functions in PETSc

.seealso: `PetscClassIdRegister()`, `PetscLogObjCreateDefault()`, `PetscLogObjDestroyDefault()`
@*/
PetscErrorCode PetscClassRegLogGetClass(PetscClassRegLog classLog, PetscClassId classid, int *oclass)
{
  int c;

  PetscFunctionBegin;
  PetscValidIntPointer(oclass, 3);
  for (c = 0; c < classLog->numClasses; c++) {
    /* Could do bisection here */
    if (classLog->classInfo[c].classid == classid) break;
  }
  PetscCheck(c < classLog->numClasses, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Invalid object classid %d\nThis could happen if you compile with PETSC_HAVE_DYNAMIC_LIBRARIES, but link with static libraries.", classid);
  *oclass = c;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*----------------------------------------------- Logging Functions -------------------------------------------------*/
/* Default object create logger */
PetscErrorCode PetscLogObjCreateDefault(PetscObject obj)
{
  PetscStageLog     stageLog;
  PetscClassRegLog  classRegLog;
  PetscClassPerfLog classPerfLog;
  Action           *tmpAction;
  Object           *tmpObjects;
  PetscLogDouble    start, end;
  int               oclass = 0;
  int               stage;

  PetscFunctionBegin;
  PetscCall(PetscSpinlockLock(&PetscLogSpinLock));
  /* Record stage info */
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  PetscCall(PetscStageLogGetClassRegLog(stageLog, &classRegLog));
  PetscCall(PetscStageLogGetClassPerfLog(stageLog, stage, &classPerfLog));
  PetscCall(PetscClassRegLogGetClass(classRegLog, obj->classid, &oclass));
  classPerfLog->classInfo[oclass].creations++;
  /* Dynamically enlarge logging structures */
  if (petsc_numActions >= petsc_maxActions) {
    PetscCall(PetscTime(&start));
    PetscCall(PetscMalloc1(petsc_maxActions * 2, &tmpAction));
    PetscCall(PetscArraycpy(tmpAction, petsc_actions, petsc_maxActions));
    PetscCall(PetscFree(petsc_actions));

    petsc_actions = tmpAction;
    petsc_maxActions *= 2;
    PetscCall(PetscTime(&end));
    petsc_BaseTime += (end - start);
  }

  petsc_numObjects = obj->id;
  /* Record the creation action */
  if (petsc_logActions) {
    PetscCall(PetscTime(&petsc_actions[petsc_numActions].time));
    petsc_actions[petsc_numActions].time -= petsc_BaseTime;
    petsc_actions[petsc_numActions].action  = CREATE;
    petsc_actions[petsc_numActions].classid = obj->classid;
    petsc_actions[petsc_numActions].id1     = petsc_numObjects;
    petsc_actions[petsc_numActions].id2     = -1;
    petsc_actions[petsc_numActions].id3     = -1;
    petsc_actions[petsc_numActions].flops   = petsc_TotalFlops;

    PetscCall(PetscMallocGetCurrentUsage(&petsc_actions[petsc_numActions].mem));
    PetscCall(PetscMallocGetMaximumUsage(&petsc_actions[petsc_numActions].maxmem));
    petsc_numActions++;
  }
  /* Record the object */
  if (petsc_logObjects) {
    petsc_objects[petsc_numObjects].parent = -1;
    petsc_objects[petsc_numObjects].obj    = obj;

    PetscCall(PetscMemzero(petsc_objects[petsc_numObjects].name, sizeof(petsc_objects[0].name)));
    PetscCall(PetscMemzero(petsc_objects[petsc_numObjects].info, sizeof(petsc_objects[0].info)));

    /* Dynamically enlarge logging structures */
    if (petsc_numObjects >= petsc_maxObjects) {
      PetscCall(PetscTime(&start));
      PetscCall(PetscMalloc1(petsc_maxObjects * 2, &tmpObjects));
      PetscCall(PetscArraycpy(tmpObjects, petsc_objects, petsc_maxObjects));
      PetscCall(PetscFree(petsc_objects));

      petsc_objects = tmpObjects;
      petsc_maxObjects *= 2;
      PetscCall(PetscTime(&end));
      petsc_BaseTime += (end - start);
    }
  }
  PetscCall(PetscSpinlockUnlock(&PetscLogSpinLock));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Default object destroy logger */
PetscErrorCode PetscLogObjDestroyDefault(PetscObject obj)
{
  PetscStageLog     stageLog;
  PetscClassRegLog  classRegLog;
  PetscClassPerfLog classPerfLog;
  Action           *tmpAction;
  PetscLogDouble    start, end;
  int               oclass = 0;
  int               stage;

  PetscFunctionBegin;
  /* Record stage info */
  PetscCall(PetscSpinlockLock(&PetscLogSpinLock));
  PetscCall(PetscLogGetStageLog(&stageLog));
  PetscCall(PetscStageLogGetCurrent(stageLog, &stage));
  if (stage != -1) {
    /* That can happen if the log summary is output before some things are destroyed */
    PetscCall(PetscStageLogGetClassRegLog(stageLog, &classRegLog));
    PetscCall(PetscStageLogGetClassPerfLog(stageLog, stage, &classPerfLog));
    PetscCall(PetscClassRegLogGetClass(classRegLog, obj->classid, &oclass));
    classPerfLog->classInfo[oclass].destructions++;
  }
  /* Cannot Credit all ancestors with your memory because they may have already been destroyed*/
  petsc_numObjectsDestroyed++;
  /* Dynamically enlarge logging structures */
  if (petsc_numActions >= petsc_maxActions) {
    PetscCall(PetscTime(&start));
    PetscCall(PetscMalloc1(petsc_maxActions * 2, &tmpAction));
    PetscCall(PetscArraycpy(tmpAction, petsc_actions, petsc_maxActions));
    PetscCall(PetscFree(petsc_actions));

    petsc_actions = tmpAction;
    petsc_maxActions *= 2;
    PetscCall(PetscTime(&end));
    petsc_BaseTime += (end - start);
  }
  /* Record the destruction action */
  if (petsc_logActions) {
    PetscCall(PetscTime(&petsc_actions[petsc_numActions].time));
    petsc_actions[petsc_numActions].time -= petsc_BaseTime;
    petsc_actions[petsc_numActions].action  = DESTROY;
    petsc_actions[petsc_numActions].classid = obj->classid;
    petsc_actions[petsc_numActions].id1     = obj->id;
    petsc_actions[petsc_numActions].id2     = -1;
    petsc_actions[petsc_numActions].id3     = -1;
    petsc_actions[petsc_numActions].flops   = petsc_TotalFlops;

    PetscCall(PetscMallocGetCurrentUsage(&petsc_actions[petsc_numActions].mem));
    PetscCall(PetscMallocGetMaximumUsage(&petsc_actions[petsc_numActions].maxmem));
    petsc_numActions++;
  }
  if (petsc_logObjects) {
    if (obj->name) PetscCall(PetscStrncpy(petsc_objects[obj->id].name, obj->name, 64));
    petsc_objects[obj->id].obj = NULL;
  }
  PetscCall(PetscSpinlockUnlock(&PetscLogSpinLock));
  PetscFunctionReturn(PETSC_SUCCESS);
}

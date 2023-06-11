#include <petsc/private/dmpleximpl.h> /*I      "petscdmplex.h"   I*/

PetscErrorCode DMPlexPointQueueCreate(PetscInt size, DMPlexPointQueue *queue)
{
  DMPlexPointQueue q;

  PetscFunctionBegin;
  PetscCheck(size >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Queue size %" PetscInt_FMT " must be non-negative", size);
  PetscCall(PetscCalloc1(1, &q));
  q->size = size;
  PetscCall(PetscMalloc1(q->size, &q->points));
  q->num   = 0;
  q->front = 0;
  q->back  = q->size - 1;
  *queue   = q;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexPointQueueDestroy(DMPlexPointQueue *queue)
{
  DMPlexPointQueue q = *queue;

  PetscFunctionBegin;
  PetscCall(PetscFree(q->points));
  PetscCall(PetscFree(q));
  *queue = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexPointQueueEnsureSize(DMPlexPointQueue queue)
{
  PetscFunctionBegin;
  if (queue->num < queue->size) PetscFunctionReturn(PETSC_SUCCESS);
  queue->size *= 2;
  PetscCall(PetscRealloc(queue->size * sizeof(PetscInt), &queue->points));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexPointQueueEnqueue(DMPlexPointQueue queue, PetscInt p)
{
  PetscFunctionBegin;
  PetscCall(DMPlexPointQueueEnsureSize(queue));
  queue->back                = (queue->back + 1) % queue->size;
  queue->points[queue->back] = p;
  ++queue->num;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexPointQueueDequeue(DMPlexPointQueue queue, PetscInt *p)
{
  PetscFunctionBegin;
  PetscCheck(queue->num, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Cannot dequeue from an empty queue");
  *p           = queue->points[queue->front];
  queue->front = (queue->front + 1) % queue->size;
  --queue->num;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexPointQueueFront(DMPlexPointQueue queue, PetscInt *p)
{
  PetscFunctionBegin;
  PetscCheck(queue->num, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Cannot get the front of an empty queue");
  *p = queue->points[queue->front];
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexPointQueueBack(DMPlexPointQueue queue, PetscInt *p)
{
  PetscFunctionBegin;
  PetscCheck(queue->num, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Cannot get the back of an empty queue");
  *p = queue->points[queue->back];
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscBool DMPlexPointQueueEmpty(DMPlexPointQueue queue)
{
  if (!queue->num) return PETSC_TRUE;
  return PETSC_FALSE;
}

PetscErrorCode DMPlexPointQueueEmptyCollective(PetscObject obj, DMPlexPointQueue queue, PetscBool *empty)
{
  PetscFunctionBeginHot;
  *empty = DMPlexPointQueueEmpty(queue);
  PetscCall(MPIU_Allreduce(MPI_IN_PLACE, empty, 1, MPIU_BOOL, MPI_LAND, PetscObjectComm(obj)));
  PetscFunctionReturn(PETSC_SUCCESS);
}

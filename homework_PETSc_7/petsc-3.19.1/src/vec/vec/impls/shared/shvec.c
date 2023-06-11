
/*
   This file contains routines for Parallel vector operations that use shared memory
 */
#include <../src/vec/vec/impls/mpi/pvecimpl.h> /*I  "petscvec.h"   I*/

#if defined(PETSC_USE_SHARED_MEMORY)

extern PetscErrorCode PetscSharedMalloc(MPI_Comm, PetscInt, PetscInt, void **);

PetscErrorCode VecDuplicate_Shared(Vec win, Vec *v)
{
  Vec_MPI     *w = (Vec_MPI *)win->data;
  PetscScalar *array;

  PetscFunctionBegin;
  /* first processor allocates entire array and sends its address to the others */
  PetscCall(PetscSharedMalloc(PetscObjectComm((PetscObject)win), win->map->n * sizeof(PetscScalar), win->map->N * sizeof(PetscScalar), (void **)&array));

  PetscCall(VecCreate(PetscObjectComm((PetscObject)win), v));
  PetscCall(VecSetSizes(*v, win->map->n, win->map->N));
  PetscCall(VecCreate_MPI_Private(*v, PETSC_FALSE, w->nghost, array));
  PetscCall(PetscLayoutReference(win->map, &(*v)->map));

  /* New vector should inherit stashing property of parent */
  (*v)->stash.donotstash   = win->stash.donotstash;
  (*v)->stash.ignorenegidx = win->stash.ignorenegidx;

  PetscCall(PetscObjectListDuplicate(((PetscObject)win)->olist, &((PetscObject)*v)->olist));
  PetscCall(PetscFunctionListDuplicate(((PetscObject)win)->qlist, &((PetscObject)*v)->qlist));

  (*v)->ops->duplicate = VecDuplicate_Shared;
  (*v)->bstash.bs      = win->bstash.bs;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN PetscErrorCode VecCreate_Shared(Vec vv)
{
  PetscScalar *array;

  PetscFunctionBegin;
  PetscCall(PetscSplitOwnership(PetscObjectComm((PetscObject)vv), &vv->map->n, &vv->map->N));
  PetscCall(PetscSharedMalloc(PetscObjectComm((PetscObject)vv), vv->map->n * sizeof(PetscScalar), vv->map->N * sizeof(PetscScalar), (void **)&array));

  PetscCall(VecCreate_MPI_Private(vv, PETSC_FALSE, 0, array));
  vv->ops->duplicate = VecDuplicate_Shared;
  PetscFunctionReturn(PETSC_SUCCESS);
}

  /* ----------------------------------------------------------------------------------------
     Code to manage shared memory allocation using standard Unix shared memory
*/
  #include <petscsys.h>
  #if defined(PETSC_HAVE_PWD_H)
    #include <pwd.h>
  #endif
  #include <ctype.h>
  #include <sys/stat.h>
  #if defined(PETSC_HAVE_UNISTD_H)
    #include <unistd.h>
  #endif
  #if defined(PETSC_HAVE_SYS_UTSNAME_H)
    #include <sys/utsname.h>
  #endif
  #include <fcntl.h>
  #include <time.h>
  #if defined(PETSC_HAVE_SYS_SYSTEMINFO_H)
    #include <sys/systeminfo.h>
  #endif
  #include <sys/shm.h>
  #include <sys/mman.h>

static PetscMPIInt Petsc_ShmComm_keyval = MPI_KEYVAL_INVALID;

/*
   Private routine to delete internal storage when a communicator is freed.
  This is called by MPI, not by users.

  The binding for the first argument changed from MPI 1.0 to 1.1; in 1.0
  it was MPI_Comm *comm.
*/
static PetscErrorCode Petsc_DeleteShared(MPI_Comm comm, PetscInt keyval, void *attr_val, void *extra_state)
{
  PetscFunctionBegin;
  PetscCall(PetscFree(attr_val));
  PetscFunctionReturn(MPI_SUCCESS);
}

/*

    This routine is still incomplete and needs work.

    For this to work on the Apple Mac OS X you will likely need to add something line the following to the file /etc/sysctl.conf
cat /etc/sysctl.conf
kern.sysv.shmmax=67108864
kern.sysv.shmmin=1
kern.sysv.shmmni=32
kern.sysv.shmseg=512
kern.sysv.shmall=1024

  This does not currently free the shared memory after the program runs. Use the Unix command ipcs to see the shared memory in use and
ipcrm to remove the shared memory in use.

*/
PetscErrorCode PetscSharedMalloc(MPI_Comm comm, PetscInt llen, PetscInt len, void **result)
{
  PetscInt    shift;
  PetscMPIInt rank, flag;
  int        *arena, id, key = 0;
  char       *value;

  PetscFunctionBegin;
  *result = 0;

  PetscCallMPI(MPI_Scan(&llen, &shift, 1, MPI_INT, MPI_SUM, comm));
  shift -= llen;

  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  if (rank == 0) {
    id = shmget(key, len, 0666 | IPC_CREAT);
    if (id == -1) {
      perror("Unable to malloc shared memory");
      SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Unable to malloc shared memory");
    }
  } else {
    id = shmget(key, len, 0666);
    if (id == -1) {
      perror("Unable to malloc shared memory");
      SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Unable to malloc shared memory");
    }
  }
  value = shmat(id, (void *)0, 0);
  if (value == (char *)-1) {
    perror("Unable to access shared memory allocated");
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Unable to access shared memory allocated");
  }
  *result = (void *)(value + shift);
  PetscFunctionReturn(PETSC_SUCCESS);
}

#else

PETSC_EXTERN PetscErrorCode VecCreate_Shared(Vec vv)
{
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)vv), &size));
  PetscCheck(size <= 1, PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "No supported for shared memory vector objects on this machine");
  PetscCall(VecCreate_Seq(vv));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#endif

/*@
   VecCreateShared - Creates a parallel vector that uses shared memory.

   Collective

   Input Parameters:
+  comm - the MPI communicator to use
.  n - local vector length (or `PETSC_DECIDE` to have calculated if `N` is given)
-  N - global vector length (or `PETSC_DECIDE` to have calculated if `n` is given)

   Output Parameter:
.  vv - the vector

   Level: advanced

   Notes:
   Currently `VecCreateShared()` is available only on the SGI; otherwise,
   this routine is the same as `VecCreateMPI()`.

   Use `VecDuplicate()` or `VecDuplicateVecs()` to form additional vectors of the
   same type as an existing vector.

.seealso: [](chapter_vectors), `Vec`, `VecType`, `VecCreateSeq()`, `VecCreate()`, `VecCreateMPI()`, `VecDuplicate()`, `VecDuplicateVecs()`,
          `VecCreateGhost()`, `VecCreateMPIWithArray()`, `VecCreateGhostWithArray()`
@*/
PetscErrorCode VecCreateShared(MPI_Comm comm, PetscInt n, PetscInt N, Vec *v)
{
  PetscFunctionBegin;
  PetscCall(VecCreate(comm, v));
  PetscCall(VecSetSizes(*v, n, N));
  PetscCall(VecSetType(*v, VECSHARED));
  PetscFunctionReturn(PETSC_SUCCESS);
}

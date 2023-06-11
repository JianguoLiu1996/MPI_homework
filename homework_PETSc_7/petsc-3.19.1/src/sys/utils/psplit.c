
#include <petscsys.h> /*I    "petscsys.h" I*/

/*@
    PetscSplitOwnershipBlock - Given a global (or local) length determines a local
        (or global) length via a simple formula. Splits so each processors local size
        is divisible by the block size.

   Collective (if `N` is `PETSC_DECIDE`)

   Input Parameters:
+    comm - MPI communicator that shares the object being divided
.    bs - block size
.    n - local length (or `PETSC_DECIDE` to have it set)
-    N - global length (or `PETSC_DECIDE`)

  Level: developer

   Notes:
     `n` and `N` cannot be both `PETSC_DECIDE`

     If one processor calls this with `N` of `PETSC_DECIDE` then all processors
     must, otherwise the program will hang.

.seealso: `PetscSplitOwnership()`, `PetscSplitOwnershipEqual()`
@*/
PetscErrorCode PetscSplitOwnershipBlock(MPI_Comm comm, PetscInt bs, PetscInt *n, PetscInt *N)
{
  PetscMPIInt size, rank;

  PetscFunctionBegin;
  PetscCheck(*N != PETSC_DECIDE || *n != PETSC_DECIDE, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Both n and N cannot be PETSC_DECIDE");

  if (*N == PETSC_DECIDE) {
    PetscCheck(*n % bs == 0, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "local size %" PetscInt_FMT " not divisible by block size %" PetscInt_FMT, *n, bs);
    PetscCall(MPIU_Allreduce(n, N, 1, MPIU_INT, MPI_SUM, comm));
  } else if (*n == PETSC_DECIDE) {
    PetscInt Nbs = *N / bs;
    PetscCallMPI(MPI_Comm_size(comm, &size));
    PetscCallMPI(MPI_Comm_rank(comm, &rank));
    *n = bs * (Nbs / size + ((Nbs % size) > rank));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscSplitOwnership - Given a global (or local) length determines a local
        (or global) length via a simple formula

   Collective (if `n` or `N` is `PETSC_DECIDE`)

   Input Parameters:
+    comm - MPI communicator that shares the object being divided
.    n - local length (or `PETSC_DECIDE` to have it set)
-    N - global length (or `PETSC_DECIDE`)

  Level: developer

   Notes:
     `n` and `N` cannot be both `PETSC_DECIDE`

     If one processor calls this with `n` or `N` of `PETSC_DECIDE` then all processors
     must. Otherwise, an error is thrown in debug mode while the program will hang
     in optimized (i.e. configured --with-debugging=0) mode.

.seealso: `PetscSplitOwnershipBlock()`, `PetscSplitOwnershipEqual()`
@*/
PetscErrorCode PetscSplitOwnership(MPI_Comm comm, PetscInt *n, PetscInt *N)
{
  PetscMPIInt size, rank;

  PetscFunctionBegin;
  PetscCheck(*N != PETSC_DECIDE || *n != PETSC_DECIDE, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Both n and N cannot be PETSC_DECIDE\n  likely a call to VecSetSizes() or MatSetSizes() is wrong.\nSee https://petsc.org/release/faq/#split-ownership");
  if (PetscDefined(USE_DEBUG)) {
    PetscMPIInt l[2], g[2];
    l[0] = (*n == PETSC_DECIDE) ? 1 : 0;
    l[1] = (*N == PETSC_DECIDE) ? 1 : 0;
    PetscCallMPI(MPI_Comm_size(comm, &size));
    PetscCall(MPIU_Allreduce(l, g, 2, MPI_INT, MPI_SUM, comm));
    PetscCheck(!g[0] || g[0] == size, comm, PETSC_ERR_ARG_INCOMP, "All processes must supply PETSC_DECIDE for local size");
    PetscCheck(!g[1] || g[1] == size, comm, PETSC_ERR_ARG_INCOMP, "All processes must supply PETSC_DECIDE for global size");
  }

  if (*N == PETSC_DECIDE) {
    PetscInt64 m = *n, M;
    PetscCall(MPIU_Allreduce(&m, &M, 1, MPIU_INT64, MPI_SUM, comm));
    PetscCheck(M <= PETSC_MAX_INT, comm, PETSC_ERR_INT_OVERFLOW, "Global size overflow %" PetscInt64_FMT ". You may consider ./configure PETSc with --with-64-bit-indices for the case you are running", M);
    *N = (PetscInt)M;
  } else if (*n == PETSC_DECIDE) {
    PetscCallMPI(MPI_Comm_size(comm, &size));
    PetscCallMPI(MPI_Comm_rank(comm, &rank));
    *n = *N / size + ((*N % size) > rank);
  } else if (PetscDefined(USE_DEBUG)) {
    PetscInt tmp;
    PetscCall(MPIU_Allreduce(n, &tmp, 1, MPIU_INT, MPI_SUM, comm));
    PetscCheck(tmp == *N, PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "Sum of local lengths %" PetscInt_FMT " does not equal global length %" PetscInt_FMT ", my local length %" PetscInt_FMT "\n  likely a call to VecSetSizes() or MatSetSizes() is wrong.\nSee https://petsc.org/release/faq/#split-ownership", tmp, *N, *n);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscSplitOwnershipEqual - Given a global (or local) length determines a local
        (or global) length via a simple formula, trying to have all local lengths equal

   Collective (if `n` or `N` is `PETSC_DECIDE`)

   Input Parameters:
+    comm - MPI communicator that shares the object being divided
.    n - local length (or `PETSC_DECIDE` to have it set)
-    N - global length (or `PETSC_DECIDE`)

   Level: developer

   Notes:
     This is intended to be used with `MATSCALAPACK`, where the local size must
     be equal in all processes (except possibly the last one). For instance,
     the local sizes when splitting `N`=50 with 6 processes are 9,9,9,9,9,5

     n and N cannot be both `PETSC_DECIDE`

     If one processor calls this with `n` or `N` of `PETSC_DECIDE` then all processors
     must. Otherwise, an error is thrown in debug mode while the program will hang
     in optimized (i.e. configured --with-debugging=0) mode.

.seealso: `PetscSplitOwnership()`, `PetscSplitOwnershipBlock()`
@*/
PetscErrorCode PetscSplitOwnershipEqual(MPI_Comm comm, PetscInt *n, PetscInt *N)
{
  PetscMPIInt size, rank;

  PetscFunctionBegin;
  PetscCheck(*N != PETSC_DECIDE || *n != PETSC_DECIDE, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Both n and N cannot be PETSC_DECIDE");
  if (PetscDefined(USE_DEBUG)) {
    PetscMPIInt l[2], g[2];
    l[0] = (*n == PETSC_DECIDE) ? 1 : 0;
    l[1] = (*N == PETSC_DECIDE) ? 1 : 0;
    PetscCallMPI(MPI_Comm_size(comm, &size));
    PetscCall(MPIU_Allreduce(l, g, 2, MPI_INT, MPI_SUM, comm));
    PetscCheck(!g[0] || g[0] == size, comm, PETSC_ERR_ARG_INCOMP, "All processes must supply PETSC_DECIDE for local size");
    PetscCheck(!g[1] || g[1] == size, comm, PETSC_ERR_ARG_INCOMP, "All processes must supply PETSC_DECIDE for global size");
  }

  if (*N == PETSC_DECIDE) {
    PetscInt64 m = *n, M;
    PetscCall(MPIU_Allreduce(&m, &M, 1, MPIU_INT64, MPI_SUM, comm));
    PetscCheck(M <= PETSC_MAX_INT, comm, PETSC_ERR_INT_OVERFLOW, "Global size overflow %" PetscInt64_FMT ". You may consider ./configure PETSc with --with-64-bit-indices for the case you are running", M);
    *N = (PetscInt)M;
  } else if (*n == PETSC_DECIDE) {
    PetscCallMPI(MPI_Comm_size(comm, &size));
    PetscCallMPI(MPI_Comm_rank(comm, &rank));
    *n = *N / size;
    if (*N % size) {
      if ((rank + 1) * (*n + 1) <= *N) *n = *n + 1;
      else if (rank * (*n + 1) <= *N) *n = *N - rank * (*n + 1);
      else *n = 0;
    }
  } else if (PetscDefined(USE_DEBUG)) {
    PetscInt tmp;
    PetscCall(MPIU_Allreduce(n, &tmp, 1, MPIU_INT, MPI_SUM, comm));
    PetscCheck(tmp == *N, PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "Sum of local lengths %" PetscInt_FMT " does not equal global length %" PetscInt_FMT ", my local length %" PetscInt_FMT, tmp, *N, *n);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

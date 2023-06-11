/*
Build a few basic tools to help with partitioned domains.

1)
On each processor, have a DomainExchangerTopology.
This is a doubly-connected edge list which enumerates the
communication paths between connected processors. By numbering
these paths we can always uniquely assign message identifiers.

        edge
         10
proc  --------->  proc
 0    <--------    1
         11
        twin

Eg: Proc 0 send to proc 1 with message id is 10. To receive the correct
message, proc 1 looks for the edge connected to proc 0, and then the
message id comes from the twin of that edge

2)
A DomainExchangerArrayPacker.
A little function which given a piece of data, will memcpy the data into
an array (which will be sent to procs) into the correct place.

On Proc 1 we sent data to procs 0,2,3. The data is on different lengths.
All data gets jammed into single array. Need to "jam" data into correct locations
The Packer knows how much is to going to each processor and keeps track of the inserts
so as to avoid ever packing TOO much into one slot, and inevatbly corrupting some memory

data to 0    data to 2       data to 3

|--------|-----------------|--|

User has to unpack message themselves. I can get you the pointer for each i
entry, but you'll have to cast it to the appropriate data type.

Phase A: Build topology

Phase B: Define message lengths

Phase C: Pack data

Phase D: Send data

+ Constructor
DMSwarmDataExCreate()
+ Phase A
DMSwarmDataExTopologyInitialize()
DMSwarmDataExTopologyAddNeighbour()
DMSwarmDataExTopologyAddNeighbour()
DMSwarmDataExTopologyFinalize()
+ Phase B
DMSwarmDataExZeroAllSendCount()
DMSwarmDataExAddToSendCount()
DMSwarmDataExAddToSendCount()
DMSwarmDataExAddToSendCount()
+ Phase C
DMSwarmDataExPackInitialize()
DMSwarmDataExPackData()
DMSwarmDataExPackData()
DMSwarmDataExPackFinalize()
+Phase D
DMSwarmDataExBegin()
 ... perform any calculations ...
DMSwarmDataExEnd()

... user calls any getters here ...

*/
#include <petscvec.h>
#include <petscmat.h>

#include "../src/dm/impls/swarm/data_ex.h"

const char *status_names[] = {"initialized", "finalized", "unknown"};

PETSC_EXTERN PetscLogEvent DMSWARM_DataExchangerTopologySetup;
PETSC_EXTERN PetscLogEvent DMSWARM_DataExchangerBegin;
PETSC_EXTERN PetscLogEvent DMSWARM_DataExchangerEnd;
PETSC_EXTERN PetscLogEvent DMSWARM_DataExchangerSendCount;
PETSC_EXTERN PetscLogEvent DMSWARM_DataExchangerPack;

PetscErrorCode DMSwarmDataExCreate(MPI_Comm comm, const PetscInt count, DMSwarmDataEx *ex)
{
  DMSwarmDataEx d;

  PetscFunctionBegin;
  PetscCall(PetscNew(&d));
  PetscCallMPI(MPI_Comm_dup(comm, &d->comm));
  PetscCallMPI(MPI_Comm_rank(d->comm, &d->rank));

  d->instance = count;

  d->topology_status        = DEOBJECT_STATE_UNKNOWN;
  d->message_lengths_status = DEOBJECT_STATE_UNKNOWN;
  d->packer_status          = DEOBJECT_STATE_UNKNOWN;
  d->communication_status   = DEOBJECT_STATE_UNKNOWN;

  d->n_neighbour_procs = -1;
  d->neighbour_procs   = NULL;

  d->messages_to_be_sent      = NULL;
  d->message_offsets          = NULL;
  d->messages_to_be_recvieved = NULL;

  d->unit_message_size   = (size_t)-1;
  d->send_message        = NULL;
  d->send_message_length = -1;
  d->recv_message        = NULL;
  d->recv_message_length = -1;
  d->total_pack_cnt      = -1;
  d->pack_cnt            = NULL;

  d->send_tags = NULL;
  d->recv_tags = NULL;

  d->_stats    = NULL;
  d->_requests = NULL;
  *ex          = d;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    This code is horrible, who let it get into main.

    Should be printing to a viewer, should not be using PETSC_COMM_WORLD

*/
PetscErrorCode DMSwarmDataExView(DMSwarmDataEx d)
{
  PetscMPIInt p;

  PetscFunctionBegin;
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "DMSwarmDataEx: instance=%" PetscInt_FMT "\n", d->instance));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "  topology status:        %s \n", status_names[d->topology_status]));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "  message lengths status: %s \n", status_names[d->message_lengths_status]));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "  packer status status:   %s \n", status_names[d->packer_status]));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "  communication status:   %s \n", status_names[d->communication_status]));

  if (d->topology_status == DEOBJECT_FINALIZED) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "  Topology:\n"));
    PetscCall(PetscSynchronizedPrintf(PETSC_COMM_WORLD, "    [%d] neighbours: %d \n", d->rank, d->n_neighbour_procs));
    for (p = 0; p < d->n_neighbour_procs; p++) PetscCall(PetscSynchronizedPrintf(PETSC_COMM_WORLD, "    [%d]   neighbour[%d] = %d \n", d->rank, p, d->neighbour_procs[p]));
    PetscCall(PetscSynchronizedFlush(PETSC_COMM_WORLD, stdout));
  }

  if (d->message_lengths_status == DEOBJECT_FINALIZED) {
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "  Message lengths:\n"));
    PetscCall(PetscSynchronizedPrintf(PETSC_COMM_WORLD, "    [%d] atomic size: %ld \n", d->rank, (long int)d->unit_message_size));
    for (p = 0; p < d->n_neighbour_procs; p++) {
      PetscCall(PetscSynchronizedPrintf(PETSC_COMM_WORLD, "    [%d] >>>>> ( %" PetscInt_FMT " units :: tag = %d) >>>>> [%d] \n", d->rank, d->messages_to_be_sent[p], d->send_tags[p], d->neighbour_procs[p]));
    }
    for (p = 0; p < d->n_neighbour_procs; p++) {
      PetscCall(PetscSynchronizedPrintf(PETSC_COMM_WORLD, "    [%d] <<<<< ( %" PetscInt_FMT " units :: tag = %d) <<<<< [%d] \n", d->rank, d->messages_to_be_recvieved[p], d->recv_tags[p], d->neighbour_procs[p]));
    }
    PetscCall(PetscSynchronizedFlush(PETSC_COMM_WORLD, stdout));
  }
  if (d->packer_status == DEOBJECT_FINALIZED) { }
  if (d->communication_status == DEOBJECT_FINALIZED) { }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataExDestroy(DMSwarmDataEx d)
{
  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_free(&d->comm));
  if (d->neighbour_procs) PetscCall(PetscFree(d->neighbour_procs));
  if (d->messages_to_be_sent) PetscCall(PetscFree(d->messages_to_be_sent));
  if (d->message_offsets) PetscCall(PetscFree(d->message_offsets));
  if (d->messages_to_be_recvieved) PetscCall(PetscFree(d->messages_to_be_recvieved));
  if (d->send_message) PetscCall(PetscFree(d->send_message));
  if (d->recv_message) PetscCall(PetscFree(d->recv_message));
  if (d->pack_cnt) PetscCall(PetscFree(d->pack_cnt));
  if (d->send_tags) PetscCall(PetscFree(d->send_tags));
  if (d->recv_tags) PetscCall(PetscFree(d->recv_tags));
  if (d->_stats) PetscCall(PetscFree(d->_stats));
  if (d->_requests) PetscCall(PetscFree(d->_requests));
  PetscCall(PetscFree(d));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* === Phase A === */

PetscErrorCode DMSwarmDataExTopologyInitialize(DMSwarmDataEx d)
{
  PetscFunctionBegin;
  d->topology_status   = DEOBJECT_INITIALIZED;
  d->n_neighbour_procs = 0;
  PetscCall(PetscFree(d->neighbour_procs));
  PetscCall(PetscFree(d->messages_to_be_sent));
  PetscCall(PetscFree(d->message_offsets));
  PetscCall(PetscFree(d->messages_to_be_recvieved));
  PetscCall(PetscFree(d->pack_cnt));
  PetscCall(PetscFree(d->send_tags));
  PetscCall(PetscFree(d->recv_tags));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataExTopologyAddNeighbour(DMSwarmDataEx d, const PetscMPIInt proc_id)
{
  PetscMPIInt n, found;
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscCheck(d->topology_status != DEOBJECT_FINALIZED, d->comm, PETSC_ERR_ARG_WRONGSTATE, "Topology has been finalized. To modify or update call DMSwarmDataExTopologyInitialize() first");
  PetscCheck(d->topology_status == DEOBJECT_INITIALIZED, d->comm, PETSC_ERR_ARG_WRONGSTATE, "Topology must be initialised. Call DMSwarmDataExTopologyInitialize() first");

  /* error on negative entries */
  PetscCheck(proc_id >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Trying to set proc neighbour with a rank < 0");
  /* error on ranks larger than number of procs in communicator */
  PetscCallMPI(MPI_Comm_size(d->comm, &size));
  PetscCheck(proc_id < size, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Trying to set proc neighbour %d with a rank >= size %d", proc_id, size);
  if (d->n_neighbour_procs == 0) PetscCall(PetscMalloc1(1, &d->neighbour_procs));
  /* check for proc_id */
  found = 0;
  for (n = 0; n < d->n_neighbour_procs; n++) {
    if (d->neighbour_procs[n] == proc_id) found = 1;
  }
  if (found == 0) { /* add it to list */
    PetscCall(PetscRealloc(sizeof(PetscMPIInt) * (d->n_neighbour_procs + 1), &d->neighbour_procs));
    d->neighbour_procs[d->n_neighbour_procs] = proc_id;
    d->n_neighbour_procs++;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
counter: the index of the communication object
N: the number of processors
r0: rank of sender
r1: rank of receiver

procs = { 0, 1, 2, 3 }

0 ==> 0         e=0
0 ==> 1         e=1
0 ==> 2         e=2
0 ==> 3         e=3

1 ==> 0         e=4
1 ==> 1         e=5
1 ==> 2         e=6
1 ==> 3         e=7

2 ==> 0         e=8
2 ==> 1         e=9
2 ==> 2         e=10
2 ==> 3         e=11

3 ==> 0         e=12
3 ==> 1         e=13
3 ==> 2         e=14
3 ==> 3         e=15

If we require that proc A sends to proc B, then the SEND tag index will be given by
  N * rank(A) + rank(B) + offset
If we require that proc A will receive from proc B, then the RECV tag index will be given by
  N * rank(B) + rank(A) + offset

*/
static void _get_tags(PetscInt counter, PetscMPIInt N, PetscMPIInt r0, PetscMPIInt r1, PetscMPIInt maxtag, PetscMPIInt *_st, PetscMPIInt *_rt)
{
  PetscMPIInt st, rt;

  st   = (N * r0 + r1 + N * N * counter) % maxtag;
  rt   = (N * r1 + r0 + N * N * counter) % maxtag;
  *_st = st;
  *_rt = rt;
}

/*
Makes the communication map symmetric
*/
PetscErrorCode _DMSwarmDataExCompleteCommunicationMap(MPI_Comm comm, PetscMPIInt n, PetscMPIInt proc_neighbours[], PetscMPIInt *n_new, PetscMPIInt **proc_neighbours_new)
{
  Mat                A;
  PetscInt           i, j, nc;
  PetscInt           n_, *proc_neighbours_;
  PetscInt           rank_;
  PetscMPIInt        size, rank;
  PetscScalar       *vals;
  const PetscInt    *cols;
  const PetscScalar *red_vals;
  PetscMPIInt        _n_new, *_proc_neighbours_new;

  PetscFunctionBegin;
  n_ = n;
  PetscCall(PetscMalloc(sizeof(PetscInt) * n_, &proc_neighbours_));
  for (i = 0; i < n_; ++i) proc_neighbours_[i] = proc_neighbours[i];
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  rank_ = rank;

  PetscCall(MatCreate(comm, &A));
  PetscCall(MatSetSizes(A, PETSC_DECIDE, PETSC_DECIDE, size, size));
  PetscCall(MatSetType(A, MATAIJ));
  PetscCall(MatSeqAIJSetPreallocation(A, 1, NULL));
  PetscCall(MatMPIAIJSetPreallocation(A, n_, NULL, n_, NULL));
  PetscCall(MatSetOption(A, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_FALSE));
  /* Build original map */
  PetscCall(PetscMalloc1(n_, &vals));
  for (i = 0; i < n_; ++i) vals[i] = 1.0;
  PetscCall(MatSetValues(A, 1, &rank_, n_, proc_neighbours_, vals, INSERT_VALUES));
  PetscCall(MatAssemblyBegin(A, MAT_FLUSH_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FLUSH_ASSEMBLY));
  /* Now force all other connections if they are not already there */
  /* It's more efficient to do them all at once */
  for (i = 0; i < n_; ++i) vals[i] = 2.0;
  PetscCall(MatSetValues(A, n_, proc_neighbours_, 1, &rank_, vals, INSERT_VALUES));
  PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
  /*
  PetscCall(PetscViewerPushFormat(PETSC_VIEWER_STDOUT_WORLD,PETSC_VIEWER_ASCII_INFO));
  PetscCall(MatView(A,PETSC_VIEWER_STDOUT_WORLD));
  PetscCall(PetscViewerPopFormat(PETSC_VIEWER_STDOUT_WORLD));
*/
  if ((n_new != NULL) && (proc_neighbours_new != NULL)) {
    PetscCall(MatGetRow(A, rank_, &nc, &cols, &red_vals));
    _n_new = (PetscMPIInt)nc;
    PetscCall(PetscMalloc1(_n_new, &_proc_neighbours_new));
    for (j = 0; j < nc; ++j) _proc_neighbours_new[j] = (PetscMPIInt)cols[j];
    PetscCall(MatRestoreRow(A, rank_, &nc, &cols, &red_vals));
    *n_new               = (PetscMPIInt)_n_new;
    *proc_neighbours_new = (PetscMPIInt *)_proc_neighbours_new;
  }
  PetscCall(MatDestroy(&A));
  PetscCall(PetscFree(vals));
  PetscCall(PetscFree(proc_neighbours_));
  PetscCallMPI(MPI_Barrier(comm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataExTopologyFinalize(DMSwarmDataEx d)
{
  PetscMPIInt symm_nn, *symm_procs, r0, n, st, rt, size, *maxtag, flg;

  PetscFunctionBegin;
  PetscCheck(d->topology_status == DEOBJECT_INITIALIZED, d->comm, PETSC_ERR_ARG_WRONGSTATE, "Topology must be initialised. Call DMSwarmDataExTopologyInitialize() first");

  PetscCall(PetscLogEventBegin(DMSWARM_DataExchangerTopologySetup, 0, 0, 0, 0));
  /* given information about all my neighbours, make map symmetric */
  PetscCall(_DMSwarmDataExCompleteCommunicationMap(d->comm, d->n_neighbour_procs, d->neighbour_procs, &symm_nn, &symm_procs));
  /* update my arrays */
  PetscCall(PetscFree(d->neighbour_procs));
  d->n_neighbour_procs = symm_nn;
  d->neighbour_procs   = symm_procs;
  /* allocates memory */
  if (!d->messages_to_be_sent) PetscCall(PetscMalloc1(d->n_neighbour_procs + 1, &d->messages_to_be_sent));
  if (!d->message_offsets) PetscCall(PetscMalloc1(d->n_neighbour_procs + 1, &d->message_offsets));
  if (!d->messages_to_be_recvieved) PetscCall(PetscMalloc1(d->n_neighbour_procs + 1, &d->messages_to_be_recvieved));
  if (!d->pack_cnt) PetscCall(PetscMalloc(sizeof(PetscInt) * d->n_neighbour_procs, &d->pack_cnt));
  if (!d->_stats) PetscCall(PetscMalloc(sizeof(MPI_Status) * 2 * d->n_neighbour_procs, &d->_stats));
  if (!d->_requests) PetscCall(PetscMalloc(sizeof(MPI_Request) * 2 * d->n_neighbour_procs, &d->_requests));
  if (!d->send_tags) PetscCall(PetscMalloc(sizeof(int) * d->n_neighbour_procs, &d->send_tags));
  if (!d->recv_tags) PetscCall(PetscMalloc(sizeof(int) * d->n_neighbour_procs, &d->recv_tags));
  /* compute message tags */
  PetscCallMPI(MPI_Comm_size(d->comm, &size));
  PetscCallMPI(MPI_Comm_get_attr(MPI_COMM_WORLD, MPI_TAG_UB, &maxtag, &flg));
  PetscCheck(flg, d->comm, PETSC_ERR_LIB, "MPI error: MPI_Comm_get_attr() is not returning a MPI_TAG_UB");
  r0 = d->rank;
  for (n = 0; n < d->n_neighbour_procs; ++n) {
    PetscMPIInt r1 = d->neighbour_procs[n];

    _get_tags(d->instance, size, r0, r1, *maxtag, &st, &rt);
    d->send_tags[n] = (int)st;
    d->recv_tags[n] = (int)rt;
  }
  d->topology_status = DEOBJECT_FINALIZED;
  PetscCall(PetscLogEventEnd(DMSWARM_DataExchangerTopologySetup, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* === Phase B === */
PetscErrorCode _DMSwarmDataExConvertProcIdToLocalIndex(DMSwarmDataEx de, PetscMPIInt proc_id, PetscMPIInt *local)
{
  PetscMPIInt i, np;

  PetscFunctionBegin;
  np     = de->n_neighbour_procs;
  *local = -1;
  for (i = 0; i < np; ++i) {
    if (proc_id == de->neighbour_procs[i]) {
      *local = i;
      break;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataExInitializeSendCount(DMSwarmDataEx de)
{
  PetscMPIInt i;

  PetscFunctionBegin;
  PetscCheck(de->topology_status == DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Topology not finalized");
  PetscCall(PetscLogEventBegin(DMSWARM_DataExchangerSendCount, 0, 0, 0, 0));
  de->message_lengths_status = DEOBJECT_INITIALIZED;
  for (i = 0; i < de->n_neighbour_procs; ++i) de->messages_to_be_sent[i] = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
1) only allows counters to be set on neighbouring cpus
*/
PetscErrorCode DMSwarmDataExAddToSendCount(DMSwarmDataEx de, const PetscMPIInt proc_id, const PetscInt count)
{
  PetscMPIInt local_val;

  PetscFunctionBegin;
  PetscCheck(de->message_lengths_status != DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Message lengths have been defined. To modify these call DMSwarmDataExInitializeSendCount() first");
  PetscCheck(de->message_lengths_status == DEOBJECT_INITIALIZED, de->comm, PETSC_ERR_ORDER, "Message lengths must be defined. Call DMSwarmDataExInitializeSendCount() first");

  PetscCall(_DMSwarmDataExConvertProcIdToLocalIndex(de, proc_id, &local_val));
  PetscCheck(local_val != -1, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Proc %d is not a valid neighbour rank", (int)proc_id);

  de->messages_to_be_sent[local_val] = de->messages_to_be_sent[local_val] + count;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataExFinalizeSendCount(DMSwarmDataEx de)
{
  PetscFunctionBegin;
  PetscCheck(de->message_lengths_status == DEOBJECT_INITIALIZED, de->comm, PETSC_ERR_ORDER, "Message lengths must be defined. Call DMSwarmDataExInitializeSendCount() first");

  de->message_lengths_status = DEOBJECT_FINALIZED;
  PetscCall(PetscLogEventEnd(DMSWARM_DataExchangerSendCount, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* === Phase C === */
/*
  zero out all send counts
  free send and recv buffers
  zeros out message length
  zeros out all counters
  zero out packed data counters
*/
PetscErrorCode _DMSwarmDataExInitializeTmpStorage(DMSwarmDataEx de)
{
  PetscMPIInt i, np;

  PetscFunctionBegin;
  np = de->n_neighbour_procs;
  for (i = 0; i < np; ++i) {
    /*  de->messages_to_be_sent[i] = -1; */
    de->messages_to_be_recvieved[i] = -1;
  }
  PetscCall(PetscFree(de->send_message));
  PetscCall(PetscFree(de->recv_message));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   Zeros out pack data counters
   Ensures mesaage length is set
   Checks send counts properly initialized
   allocates space for pack data
*/
PetscErrorCode DMSwarmDataExPackInitialize(DMSwarmDataEx de, size_t unit_message_size)
{
  PetscMPIInt i, np;
  PetscInt    total;

  PetscFunctionBegin;
  PetscCheck(de->topology_status == DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Topology not finalized");
  PetscCheck(de->message_lengths_status == DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Message lengths not finalized");
  PetscCall(PetscLogEventBegin(DMSWARM_DataExchangerPack, 0, 0, 0, 0));
  de->packer_status = DEOBJECT_INITIALIZED;
  PetscCall(_DMSwarmDataExInitializeTmpStorage(de));
  np                    = de->n_neighbour_procs;
  de->unit_message_size = unit_message_size;
  total                 = 0;
  for (i = 0; i < np; ++i) {
    if (de->messages_to_be_sent[i] == -1) {
      PetscMPIInt proc_neighour = de->neighbour_procs[i];
      SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ORDER, "Messages_to_be_sent[neighbour_proc=%d] is un-initialised. Call DMSwarmDataExSetSendCount() first", (int)proc_neighour);
    }
    total = total + de->messages_to_be_sent[i];
  }
  /* create space for the data to be sent */
  PetscCall(PetscMalloc(unit_message_size * (total + 1), &de->send_message));
  /* initialize memory */
  PetscCall(PetscMemzero(de->send_message, unit_message_size * (total + 1)));
  /* set total items to send */
  de->send_message_length = total;
  de->message_offsets[0]  = 0;
  total                   = de->messages_to_be_sent[0];
  for (i = 1; i < np; ++i) {
    de->message_offsets[i] = total;
    total                  = total + de->messages_to_be_sent[i];
  }
  /* init the packer counters */
  de->total_pack_cnt = 0;
  for (i = 0; i < np; ++i) de->pack_cnt[i] = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    Ensures data gets been packed appropriately and no overlaps occur
*/
PetscErrorCode DMSwarmDataExPackData(DMSwarmDataEx de, PetscMPIInt proc_id, PetscInt n, void *data)
{
  PetscMPIInt local;
  PetscInt    insert_location;
  void       *dest;

  PetscFunctionBegin;
  PetscCheck(de->packer_status != DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Packed data have been defined. To modify these call DMSwarmDataExInitializeSendCount(), DMSwarmDataExAddToSendCount(), DMSwarmDataExPackInitialize() first");
  PetscCheck(de->packer_status == DEOBJECT_INITIALIZED, de->comm, PETSC_ERR_ORDER, "Packed data must be defined. Call DMSwarmDataExInitializeSendCount(), DMSwarmDataExAddToSendCount(), DMSwarmDataExPackInitialize() first");

  PetscCheck(de->send_message, de->comm, PETSC_ERR_ORDER, "send_message is not initialized. Call DMSwarmDataExPackInitialize() first");
  PetscCall(_DMSwarmDataExConvertProcIdToLocalIndex(de, proc_id, &local));
  PetscCheck(local != -1, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "proc_id %d is not registered neighbour", (int)proc_id);
  PetscCheck(n + de->pack_cnt[local] <= de->messages_to_be_sent[local], PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Trying to pack too many entries to be sent to proc %d. Space requested = %" PetscInt_FMT ": Attempt to insert %" PetscInt_FMT, (int)proc_id, de->messages_to_be_sent[local], n + de->pack_cnt[local]);

  /* copy memory */
  insert_location = de->message_offsets[local] + de->pack_cnt[local];
  dest            = ((char *)de->send_message) + de->unit_message_size * insert_location;
  PetscCall(PetscMemcpy(dest, data, de->unit_message_size * n));
  /* increment counter */
  de->pack_cnt[local] = de->pack_cnt[local] + n;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
*) Ensures all data has been packed
*/
PetscErrorCode DMSwarmDataExPackFinalize(DMSwarmDataEx de)
{
  PetscMPIInt i, np;
  PetscInt    total;

  PetscFunctionBegin;
  PetscCheck(de->packer_status == DEOBJECT_INITIALIZED, de->comm, PETSC_ERR_ORDER, "Packer has not been initialized. Must call DMSwarmDataExPackInitialize() first.");
  np = de->n_neighbour_procs;
  for (i = 0; i < np; ++i) {
    PetscCheck(de->pack_cnt[i] == de->messages_to_be_sent[i], PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "Not all messages for neighbour[%d] have been packed. Expected %" PetscInt_FMT " : Inserted %" PetscInt_FMT, (int)de->neighbour_procs[i], de->messages_to_be_sent[i], de->pack_cnt[i]);
  }
  /* init */
  for (i = 0; i < np; ++i) de->messages_to_be_recvieved[i] = -1;
  /* figure out the recv counts here */
  for (i = 0; i < np; ++i) PetscCallMPI(MPI_Isend(&de->messages_to_be_sent[i], 1, MPIU_INT, de->neighbour_procs[i], de->send_tags[i], de->comm, &de->_requests[i]));
  for (i = 0; i < np; ++i) PetscCallMPI(MPI_Irecv(&de->messages_to_be_recvieved[i], 1, MPIU_INT, de->neighbour_procs[i], de->recv_tags[i], de->comm, &de->_requests[np + i]));
  PetscCallMPI(MPI_Waitall(2 * np, de->_requests, de->_stats));
  /* create space for the data to be recvieved */
  total = 0;
  for (i = 0; i < np; ++i) total = total + de->messages_to_be_recvieved[i];
  PetscCall(PetscMalloc(de->unit_message_size * (total + 1), &de->recv_message));
  /* initialize memory */
  PetscCall(PetscMemzero(de->recv_message, de->unit_message_size * (total + 1)));
  /* set total items to receive */
  de->recv_message_length  = total;
  de->packer_status        = DEOBJECT_FINALIZED;
  de->communication_status = DEOBJECT_INITIALIZED;
  PetscCall(PetscLogEventEnd(DMSWARM_DataExchangerPack, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* do the actual message passing */
PetscErrorCode DMSwarmDataExBegin(DMSwarmDataEx de)
{
  PetscMPIInt i, np;
  void       *dest;
  PetscInt    length;

  PetscFunctionBegin;
  PetscCheck(de->topology_status == DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Topology not finalized");
  PetscCheck(de->message_lengths_status == DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Message lengths not finalized");
  PetscCheck(de->packer_status == DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Packer not finalized");
  PetscCheck(de->communication_status != DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ORDER, "Communication has already been finalized. Must call DMSwarmDataExInitialize() first.");
  PetscCheck(de->recv_message, de->comm, PETSC_ERR_ORDER, "recv_message has not been initialized. Must call DMSwarmDataExPackFinalize() first");
  PetscCall(PetscLogEventBegin(DMSWARM_DataExchangerBegin, 0, 0, 0, 0));
  np = de->n_neighbour_procs;
  /* == NON BLOCKING == */
  for (i = 0; i < np; ++i) {
    length = de->messages_to_be_sent[i] * de->unit_message_size;
    dest   = ((char *)de->send_message) + de->unit_message_size * de->message_offsets[i];
    PetscCallMPI(MPI_Isend(dest, length, MPI_CHAR, de->neighbour_procs[i], de->send_tags[i], de->comm, &de->_requests[i]));
  }
  PetscCall(PetscLogEventEnd(DMSWARM_DataExchangerBegin, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* do the actual message passing now */
PetscErrorCode DMSwarmDataExEnd(DMSwarmDataEx de)
{
  PetscMPIInt i, np;
  PetscInt    total;
  PetscInt   *message_recv_offsets;
  void       *dest;
  PetscInt    length;

  PetscFunctionBegin;
  PetscCheck(de->communication_status == DEOBJECT_INITIALIZED, de->comm, PETSC_ERR_ORDER, "Communication has not been initialized. Must call DMSwarmDataExInitialize() first.");
  PetscCheck(de->recv_message, de->comm, PETSC_ERR_ORDER, "recv_message has not been initialized. Must call DMSwarmDataExPackFinalize() first");
  PetscCall(PetscLogEventBegin(DMSWARM_DataExchangerEnd, 0, 0, 0, 0));
  np = de->n_neighbour_procs;
  PetscCall(PetscMalloc1(np + 1, &message_recv_offsets));
  message_recv_offsets[0] = 0;
  total                   = de->messages_to_be_recvieved[0];
  for (i = 1; i < np; ++i) {
    message_recv_offsets[i] = total;
    total                   = total + de->messages_to_be_recvieved[i];
  }
  /* == NON BLOCKING == */
  for (i = 0; i < np; ++i) {
    length = de->messages_to_be_recvieved[i] * de->unit_message_size;
    dest   = ((char *)de->recv_message) + de->unit_message_size * message_recv_offsets[i];
    PetscCallMPI(MPI_Irecv(dest, length, MPI_CHAR, de->neighbour_procs[i], de->recv_tags[i], de->comm, &de->_requests[np + i]));
  }
  PetscCallMPI(MPI_Waitall(2 * np, de->_requests, de->_stats));
  PetscCall(PetscFree(message_recv_offsets));
  de->communication_status = DEOBJECT_FINALIZED;
  PetscCall(PetscLogEventEnd(DMSWARM_DataExchangerEnd, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataExGetSendData(DMSwarmDataEx de, PetscInt *length, void **send)
{
  PetscFunctionBegin;
  PetscCheck(de->packer_status == DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ARG_WRONGSTATE, "Data has not finished being packed.");
  *length = de->send_message_length;
  *send   = de->send_message;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataExGetRecvData(DMSwarmDataEx de, PetscInt *length, void **recv)
{
  PetscFunctionBegin;
  PetscCheck(de->communication_status == DEOBJECT_FINALIZED, de->comm, PETSC_ERR_ARG_WRONGSTATE, "Data has not finished being sent.");
  *length = de->recv_message_length;
  *recv   = de->recv_message;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMSwarmDataExTopologyGetNeighbours(DMSwarmDataEx de, PetscMPIInt *n, PetscMPIInt *neigh[])
{
  PetscFunctionBegin;
  if (n) *n = de->n_neighbour_procs;
  if (neigh) *neigh = de->neighbour_procs;
  PetscFunctionReturn(PETSC_SUCCESS);
}

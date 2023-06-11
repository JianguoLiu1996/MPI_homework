
#include <../src/mat/impls/adj/mpi/mpiadj.h> /*I "petscmat.h" I*/

#if defined(PETSC_HAVE_UNISTD_H)
  #include <unistd.h>
#endif

EXTERN_C_BEGIN
#include <party_lib.h>
EXTERN_C_END

typedef struct {
  PetscBool redm;
  PetscBool redo;
  PetscBool recursive;
  PetscBool verbose;
  char      global[15];   /* global method */
  char      local[15];    /* local method */
  PetscInt  nbvtxcoarsed; /* number of vertices for the coarse graph */
} MatPartitioning_Party;

#define SIZE_LOG 10000 /* size of buffer for mesg_log */

static PetscErrorCode MatPartitioningApply_Party(MatPartitioning part, IS *partitioning)
{
  int                    perr;
  PetscInt               i, *parttab, *locals, nb_locals, M, N;
  PetscMPIInt            size, rank;
  Mat                    mat = part->adj, matAdj, matSeq, *A;
  Mat_MPIAdj            *adj;
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;
  PetscBool              flg;
  IS                     isrow, iscol;
  int                    n, *edge_p, *edge, *vertex_w, p, *part_party, cutsize, redl, rec;
  const char            *redm, *redo;
  char                  *mesg_log;
#if defined(PETSC_HAVE_UNISTD_H)
  int fd_stdout, fd_pipe[2], count;
#endif

  PetscFunctionBegin;
  PetscCheck(!part->use_edge_weights, PetscObjectComm((PetscObject)part), PETSC_ERR_SUP, "Party does not support edge weights");
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)mat), &size));
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)mat), &rank));
  PetscCall(PetscObjectTypeCompare((PetscObject)mat, MATMPIADJ, &flg));
  if (size > 1) {
    if (flg) {
      PetscCall(MatMPIAdjToSeq(mat, &matSeq));
    } else {
      PetscCall(PetscInfo(part, "Converting distributed matrix to sequential: this could be a performance loss\n"));
      PetscCall(MatGetSize(mat, &M, &N));
      PetscCall(ISCreateStride(PETSC_COMM_SELF, M, 0, 1, &isrow));
      PetscCall(ISCreateStride(PETSC_COMM_SELF, N, 0, 1, &iscol));
      PetscCall(MatCreateSubMatrices(mat, 1, &isrow, &iscol, MAT_INITIAL_MATRIX, &A));
      PetscCall(ISDestroy(&isrow));
      PetscCall(ISDestroy(&iscol));
      matSeq = *A;
      PetscCall(PetscFree(A));
    }
  } else {
    PetscCall(PetscObjectReference((PetscObject)mat));
    matSeq = mat;
  }

  if (!flg) { /* convert regular matrix to MPIADJ */
    PetscCall(MatConvert(matSeq, MATMPIADJ, MAT_INITIAL_MATRIX, &matAdj));
  } else {
    PetscCall(PetscObjectReference((PetscObject)matSeq));
    matAdj = matSeq;
  }

  adj = (Mat_MPIAdj *)matAdj->data; /* finally adj contains adjacency graph */

  /* arguments for Party library */
  n        = mat->rmap->N;             /* number of vertices in full graph */
  edge_p   = adj->i;                   /* start of edge list for each vertex */
  edge     = adj->j;                   /* edge list data */
  vertex_w = part->vertex_weights;     /* weights for all vertices */
  p        = part->n;                  /* number of parts to create */
  redl     = party->nbvtxcoarsed;      /* how many vertices to coarsen down to? */
  rec      = party->recursive ? 1 : 0; /* recursive bisection */
  redm     = party->redm ? "lam" : ""; /* matching method */
  redo     = party->redo ? "w3" : "";  /* matching optimization method */

  PetscCall(PetscMalloc1(mat->rmap->N, &part_party));

  /* redirect output to buffer */
#if defined(PETSC_HAVE_UNISTD_H)
  fd_stdout = dup(1);
  PetscCheck(!pipe(fd_pipe), PETSC_COMM_SELF, PETSC_ERR_SYS, "Could not open pipe");
  close(1);
  dup2(fd_pipe[1], 1);
  PetscCall(PetscMalloc1(SIZE_LOG, &mesg_log));
#endif

  /* library call */
  party_lib_times_start();
  perr = party_lib(n, vertex_w, NULL, NULL, NULL, edge_p, edge, NULL, p, part_party, &cutsize, redl, (char *)redm, (char *)redo, party->global, party->local, rec, 1);

  party_lib_times_output(1);
  part_info(n, vertex_w, edge_p, edge, NULL, p, part_party, 1);

#if defined(PETSC_HAVE_UNISTD_H)
  PetscCall(PetscFFlush(stdout));
  count = read(fd_pipe[0], mesg_log, (SIZE_LOG - 1) * sizeof(char));
  if (count < 0) count = 0;
  mesg_log[count] = 0;
  close(1);
  dup2(fd_stdout, 1);
  close(fd_stdout);
  close(fd_pipe[0]);
  close(fd_pipe[1]);
  if (party->verbose) PetscCall(PetscPrintf(PetscObjectComm((PetscObject)mat), "%s", mesg_log));
  PetscCall(PetscFree(mesg_log));
#endif
  PetscCheck(!perr, PETSC_COMM_SELF, PETSC_ERR_LIB, "Party failed");

  PetscCall(PetscMalloc1(mat->rmap->N, &parttab));
  for (i = 0; i < mat->rmap->N; i++) parttab[i] = part_party[i];

  /* creation of the index set */
  nb_locals = mat->rmap->n;
  locals    = parttab + mat->rmap->rstart;

  PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)part), nb_locals, locals, PETSC_COPY_VALUES, partitioning));

  /* clean up */
  PetscCall(PetscFree(parttab));
  PetscCall(PetscFree(part_party));
  PetscCall(MatDestroy(&matSeq));
  PetscCall(MatDestroy(&matAdj));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPartitioningView_Party(MatPartitioning part, PetscViewer viewer)
{
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;
  PetscBool              isascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &isascii));
  if (isascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  Global method: %s\n", party->global));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  Local method: %s\n", party->local));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  Number of vertices for the coarse graph: %d\n", party->nbvtxcoarsed));
    if (party->redm) PetscCall(PetscViewerASCIIPrintf(viewer, "  Using matching method for graph reduction\n"));
    if (party->redo) PetscCall(PetscViewerASCIIPrintf(viewer, "  Using matching optimization\n"));
    if (party->recursive) PetscCall(PetscViewerASCIIPrintf(viewer, "  Using recursive bipartitioning\n"));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatPartitioningPartySetGlobal - Set global method for Party partitioner.

   Collective

   Input Parameters:
+  part - the partitioning context
-  method - a string representing the method

   Options Database Key:
.  -mat_partitioning_party_global <method> - the global method

   Level: advanced

   Note:
   The method may be one of `MP_PARTY_OPT`, `MP_PARTY_LIN`, `MP_PARTY_SCA`,
   `MP_PARTY_RAN`, `MP_PARTY_GBF`, `MP_PARTY_GCF`, `MP_PARTY_BUB` or `MP_PARTY_DEF`, or
   alternatively a string describing the method. Two or more methods can be
   combined like "gbf,gcf". Check the Party Library Users Manual for details.

.seealso: `MATPARTITIONINGPARTY`, `MatPartitioningPartySetLocal()`
@*/
PetscErrorCode MatPartitioningPartySetGlobal(MatPartitioning part, const char *global)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(part, MAT_PARTITIONING_CLASSID, 1);
  PetscTryMethod(part, "MatPartitioningPartySetGlobal_C", (MatPartitioning, const char *), (part, global));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPartitioningPartySetGlobal_Party(MatPartitioning part, const char *global)
{
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;

  PetscFunctionBegin;
  PetscCall(PetscStrncpy(party->global, global, 15));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   MatPartitioningPartySetLocal - Set local method used by the Party partitioner.

   Collective

   Input Parameters:
+  part - the partitioning context
-  method - a string representing the method

   Options Database Key:
.  -mat_partitioning_party_local <method> - the local method

   Level: advanced

   Note:
   The method may be one of `MP_PARTY_HELPFUL_SETS`, `MP_PARTY_KERNIGHAN_LIN`, or
   `MP_PARTY_NONE`. Check the Party Library Users Manual for details.

.seealso: `MATPARTITIONINGPARTY`, `MatPartitioningPartySetGlobal()`
@*/
PetscErrorCode MatPartitioningPartySetLocal(MatPartitioning part, const char *local)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(part, MAT_PARTITIONING_CLASSID, 1);
  PetscTryMethod(part, "MatPartitioningPartySetLocal_C", (MatPartitioning, const char *), (part, local));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPartitioningPartySetLocal_Party(MatPartitioning part, const char *local)

{
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;

  PetscFunctionBegin;
  PetscCall(PetscStrncpy(party->local, local, 15));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatPartitioningPartySetCoarseLevel - Set the coarse level parameter for the
   Party partitioner.

   Collective

   Input Parameters:
+  part - the partitioning context
-  level - the coarse level in range [0.0,1.0]

   Options Database Key:
.  -mat_partitioning_party_coarse <l> - Coarse level

   Level: advanced

.seealso: `MATPARTITIONINGPARTY`
@*/
PetscErrorCode MatPartitioningPartySetCoarseLevel(MatPartitioning part, PetscReal level)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(part, MAT_PARTITIONING_CLASSID, 1);
  PetscValidLogicalCollectiveReal(part, level, 2);
  PetscTryMethod(part, "MatPartitioningPartySetCoarseLevel_C", (MatPartitioning, PetscReal), (part, level));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPartitioningPartySetCoarseLevel_Party(MatPartitioning part, PetscReal level)
{
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;

  PetscFunctionBegin;
  PetscCheck(level >= 0.0 && level <= 1.0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Party: level of coarsening out of range [0.0-1.0]");
  party->nbvtxcoarsed = (PetscInt)(part->adj->cmap->N * level);
  if (party->nbvtxcoarsed < 20) party->nbvtxcoarsed = 20;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatPartitioningPartySetMatchOptimization - Activate matching optimization for
   graph reduction.

   Collective

   Input Parameters:
+  part - the partitioning context
-  opt - boolean flag

   Options Database Key:
.  -mat_partitioning_party_match_optimization - Matching optimization on/off

   Level: advanced

.seealso:  `MATPARTITIONINGPARTY`
@*/
PetscErrorCode MatPartitioningPartySetMatchOptimization(MatPartitioning part, PetscBool opt)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(part, MAT_PARTITIONING_CLASSID, 1);
  PetscValidLogicalCollectiveBool(part, opt, 2);
  PetscTryMethod(part, "MatPartitioningPartySetMatchOptimization_C", (MatPartitioning, PetscBool), (part, opt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPartitioningPartySetMatchOptimization_Party(MatPartitioning part, PetscBool opt)
{
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;

  PetscFunctionBegin;
  party->redo = opt;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   MatPartitioningPartySetBipart - Activate or deactivate recursive bisection in the Party partitioner

   Collective

   Input Parameters:
+  part - the partitioning context
-  bp - boolean flag

   Options Database Key:
-  -mat_partitioning_party_bipart - Bipartitioning option on/off

   Level: advanced

.seealso:  `MATPARTITIONINGPARTY`
@*/
PetscErrorCode MatPartitioningPartySetBipart(MatPartitioning part, PetscBool bp)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(part, MAT_PARTITIONING_CLASSID, 1);
  PetscValidLogicalCollectiveBool(part, bp, 2);
  PetscTryMethod(part, "MatPartitioningPartySetBipart_C", (MatPartitioning, PetscBool), (part, bp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPartitioningPartySetBipart_Party(MatPartitioning part, PetscBool bp)
{
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;

  PetscFunctionBegin;
  party->recursive = bp;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPartitioningSetFromOptions_Party(MatPartitioning part, PetscOptionItems *PetscOptionsObject)
{
  PetscBool              flag;
  char                   value[256];
  PetscReal              r;
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "Set Party partitioning options");
  PetscCall(PetscOptionsString("-mat_partitioning_party_global", "Global method", "MatPartitioningPartySetGlobal", party->global, value, sizeof(value), &flag));
  if (flag) PetscCall(MatPartitioningPartySetGlobal(part, value));
  PetscCall(PetscOptionsString("-mat_partitioning_party_local", "Local method", "MatPartitioningPartySetLocal", party->local, value, sizeof(value), &flag));
  if (flag) PetscCall(MatPartitioningPartySetLocal(part, value));
  PetscCall(PetscOptionsReal("-mat_partitioning_party_coarse", "Coarse level", "MatPartitioningPartySetCoarseLevel", 0.0, &r, &flag));
  if (flag) PetscCall(MatPartitioningPartySetCoarseLevel(part, r));
  PetscCall(PetscOptionsBool("-mat_partitioning_party_match_optimization", "Matching optimization on/off", "MatPartitioningPartySetMatchOptimization", party->redo, &party->redo, NULL));
  PetscCall(PetscOptionsBool("-mat_partitioning_party_bipart", "Bipartitioning on/off", "MatPartitioningPartySetBipart", party->recursive, &party->recursive, NULL));
  PetscCall(PetscOptionsBool("-mat_partitioning_party_verbose", "Show library output", "", party->verbose, &party->verbose, NULL));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MatPartitioningDestroy_Party(MatPartitioning part)
{
  MatPartitioning_Party *party = (MatPartitioning_Party *)part->data;

  PetscFunctionBegin;
  PetscCall(PetscFree(party));
  /* clear composed functions */
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetGlobal_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetLocal_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetCoarseLevel_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetMatchOptimization_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetBipart_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   MATPARTITIONINGPARTY - Creates a partitioning context via the external package Party.

   Level: beginner

   Notes:
    See http://wwwcs.upb.de/fachbereich/AG/monien/RESEARCH/PART/party.html

    Does not support the `MatPartitioningSetUseEdgeWeights()` option

.seealso: `MatPartitioningSetType()`, `MatPartitioningType`, `MatPartitioningPartySetGlobal()`, `MatPartitioningPartySetLocal()`,
          `MatPartitioningPartySetCoarseLevel()`, `MatPartitioningPartySetMatchOptimization()`, `MatPartitioningPartySetBipart()`
M*/

PETSC_EXTERN PetscErrorCode MatPartitioningCreate_Party(MatPartitioning part)
{
  MatPartitioning_Party *party;

  PetscFunctionBegin;
  PetscCall(PetscNew(&party));
  part->data = (void *)party;

  PetscCall(PetscStrncpy(party->global, "gcf,gbf", sizeof(party->global)));
  PetscCall(PetscStrncpy(party->local, "kl", sizeof(party->local)));

  party->redm         = PETSC_TRUE;
  party->redo         = PETSC_TRUE;
  party->recursive    = PETSC_TRUE;
  party->verbose      = PETSC_FALSE;
  party->nbvtxcoarsed = 200;

  part->ops->apply          = MatPartitioningApply_Party;
  part->ops->view           = MatPartitioningView_Party;
  part->ops->destroy        = MatPartitioningDestroy_Party;
  part->ops->setfromoptions = MatPartitioningSetFromOptions_Party;

  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetGlobal_C", MatPartitioningPartySetGlobal_Party));
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetLocal_C", MatPartitioningPartySetLocal_Party));
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetCoarseLevel_C", MatPartitioningPartySetCoarseLevel_Party));
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetMatchOptimization_C", MatPartitioningPartySetMatchOptimization_Party));
  PetscCall(PetscObjectComposeFunction((PetscObject)part, "MatPartitioningPartySetBipart_C", MatPartitioningPartySetBipart_Party));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   An index set is a generalization of a subset of integers.  Index sets
   are used for defining scatters and gathers.
*/
#ifndef PETSCIS_H
#define PETSCIS_H

#include <petscsys.h>
#include <petscsftypes.h>
#include <petscsectiontypes.h>
#include <petscistypes.h> /*I  "petscis.h" I*/

/* SUBMANSEC = IS */

#define IS_FILE_CLASSID 1211218
PETSC_EXTERN PetscClassId IS_CLASSID;

PETSC_EXTERN PetscErrorCode ISInitializePackage(void);

/*J
    ISType - String with the name of a PETSc index set type

   Values:
+  `ISGENERAL` - the values are stored with an arry of indices and generally have no structure
.  `ISSTRIDE` - the values have a simple structure of an initial offset and then a step size between values
-  `ISBLOCK` - values are an array of indices, each representing a block of values

   Level: beginner

.seealso: `ISSetType()`, `IS`, `ISCreateGeneral()`, `ISCreateStride()`, `ISCreateBlock()`, `ISCreate()`, `ISRegister()`,
          `VecScatterCreate()`, `MatGetSubMatrices()`
J*/
typedef const char *ISType;
#define ISGENERAL "general"
#define ISSTRIDE  "stride"
#define ISBLOCK   "block"

/* Dynamic creation and loading functions */
PETSC_EXTERN PetscFunctionList ISList;
PETSC_EXTERN PetscErrorCode    ISSetType(IS, ISType);
PETSC_EXTERN PetscErrorCode    ISGetType(IS, ISType *);
PETSC_EXTERN PetscErrorCode    ISRegister(const char[], PetscErrorCode (*)(IS));
PETSC_EXTERN PetscErrorCode    ISCreate(MPI_Comm, IS *);

PETSC_EXTERN PetscErrorCode ISDestroy(IS *);
PETSC_EXTERN PetscErrorCode ISSetPermutation(IS);
PETSC_EXTERN PetscErrorCode ISPermutation(IS, PetscBool *);
PETSC_EXTERN PetscErrorCode ISSetIdentity(IS);
PETSC_EXTERN PetscErrorCode ISIdentity(IS, PetscBool *);
PETSC_EXTERN PetscErrorCode ISContiguousLocal(IS, PetscInt, PetscInt, PetscInt *, PetscBool *);

/*E
    ISInfo - Info that may either be computed or set as known for an index set

    Level: intermediate

   Developer Notes:
   Entries that are negative need not be called collectively by all processes.

   Any additions/changes here MUST also be made in include/petsc/finclude/petscis.h

   Any additions/changes here must also be made in src/vec/vec/interface/dlregisvec.c in ISInfos[]

.seealso: `IS`, `ISType`, `ISSetInfo()`
E*/
typedef enum {
  IS_INFO_MIN    = -1,
  IS_SORTED      = 0,
  IS_UNIQUE      = 1,
  IS_PERMUTATION = 2,
  IS_INTERVAL    = 3,
  IS_IDENTITY    = 4,
  IS_INFO_MAX    = 5
} ISInfo;

typedef enum {
  IS_LOCAL,
  IS_GLOBAL
} ISInfoType;

PETSC_EXTERN PetscErrorCode ISSetInfo(IS, ISInfo, ISInfoType, PetscBool, PetscBool);
PETSC_EXTERN PetscErrorCode ISGetInfo(IS, ISInfo, ISInfoType, PetscBool, PetscBool *);
PETSC_EXTERN PetscErrorCode ISClearInfoCache(IS, PetscBool);
PETSC_EXTERN PetscErrorCode ISGetIndices(IS, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode ISRestoreIndices(IS, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode ISGetTotalIndices(IS, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode ISRestoreTotalIndices(IS, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode ISGetNonlocalIndices(IS, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode ISRestoreNonlocalIndices(IS, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode ISGetNonlocalIS(IS, IS *);
PETSC_EXTERN PetscErrorCode ISRestoreNonlocalIS(IS, IS *);
PETSC_EXTERN PetscErrorCode ISGetSize(IS, PetscInt *);
PETSC_EXTERN PetscErrorCode ISGetLocalSize(IS, PetscInt *);
PETSC_EXTERN PetscErrorCode ISInvertPermutation(IS, PetscInt, IS *);
PETSC_EXTERN PetscErrorCode ISView(IS, PetscViewer);
PETSC_EXTERN PetscErrorCode ISViewFromOptions(IS, PetscObject, const char[]);
PETSC_EXTERN PetscErrorCode ISLoad(IS, PetscViewer);
PETSC_EXTERN PetscErrorCode ISEqual(IS, IS, PetscBool *);
PETSC_EXTERN PetscErrorCode ISEqualUnsorted(IS, IS, PetscBool *);
PETSC_EXTERN PetscErrorCode ISSort(IS);
PETSC_EXTERN PetscErrorCode ISSortRemoveDups(IS);
PETSC_EXTERN PetscErrorCode ISSorted(IS, PetscBool *);
PETSC_EXTERN PetscErrorCode ISDifference(IS, IS, IS *);
PETSC_EXTERN PetscErrorCode ISSum(IS, IS, IS *);
PETSC_EXTERN PetscErrorCode ISExpand(IS, IS, IS *);
PETSC_EXTERN PetscErrorCode ISIntersect(IS, IS, IS *);
PETSC_EXTERN PetscErrorCode ISGetMinMax(IS, PetscInt *, PetscInt *);

PETSC_EXTERN PetscErrorCode ISLocate(IS, PetscInt, PetscInt *);
PETSC_EXTERN PetscErrorCode ISGetPointRange(IS, PetscInt *, PetscInt *, const PetscInt **);
PETSC_EXTERN PetscErrorCode ISRestorePointRange(IS, PetscInt *, PetscInt *, const PetscInt **);
PETSC_EXTERN PetscErrorCode ISGetPointSubrange(IS, PetscInt, PetscInt, const PetscInt *);

PETSC_EXTERN PetscErrorCode ISGetBlockSize(IS, PetscInt *);
PETSC_EXTERN PetscErrorCode ISSetBlockSize(IS, PetscInt);

PETSC_EXTERN PetscErrorCode ISToGeneral(IS);

PETSC_EXTERN PetscErrorCode ISDuplicate(IS, IS *);
PETSC_EXTERN PetscErrorCode ISCopy(IS, IS);
PETSC_EXTERN PetscErrorCode ISShift(IS, PetscInt, IS);
PETSC_EXTERN PetscErrorCode ISAllGather(IS, IS *);
PETSC_EXTERN PetscErrorCode ISComplement(IS, PetscInt, PetscInt, IS *);
PETSC_EXTERN PetscErrorCode ISConcatenate(MPI_Comm, PetscInt, const IS[], IS *);
PETSC_EXTERN PetscErrorCode ISListToPair(MPI_Comm, PetscInt, IS[], IS *, IS *);
PETSC_EXTERN PetscErrorCode ISPairToList(IS, IS, PetscInt *, IS *[]);
PETSC_EXTERN PetscErrorCode ISEmbed(IS, IS, PetscBool, IS *);
PETSC_EXTERN PetscErrorCode ISSortPermutation(IS, PetscBool, IS *);
PETSC_EXTERN PetscErrorCode ISOnComm(IS, MPI_Comm, PetscCopyMode, IS *);
PETSC_EXTERN PetscErrorCode ISRenumber(IS, IS, PetscInt *, IS *);
PETSC_EXTERN PetscErrorCode ISCreateSubIS(IS, IS, IS *);

/* ISGENERAL specific */
PETSC_EXTERN PetscErrorCode ISCreateGeneral(MPI_Comm, PetscInt, const PetscInt[], PetscCopyMode, IS *);
PETSC_EXTERN PetscErrorCode ISGeneralSetIndices(IS, PetscInt, const PetscInt[], PetscCopyMode);
PETSC_EXTERN PetscErrorCode ISGeneralSetIndicesFromMask(IS, PetscInt, PetscInt, const PetscBool[]);
PETSC_EXTERN PetscErrorCode ISGeneralFilter(IS, PetscInt, PetscInt);

/* ISBLOCK specific */
PETSC_EXTERN PetscErrorCode ISCreateBlock(MPI_Comm, PetscInt, PetscInt, const PetscInt[], PetscCopyMode, IS *);
PETSC_EXTERN PetscErrorCode ISBlockSetIndices(IS, PetscInt, PetscInt, const PetscInt[], PetscCopyMode);
PETSC_EXTERN PetscErrorCode ISBlockGetIndices(IS, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode ISBlockRestoreIndices(IS, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode ISBlockGetLocalSize(IS, PetscInt *);
PETSC_EXTERN PetscErrorCode ISBlockGetSize(IS, PetscInt *);

/* ISSTRIDE specific */
PETSC_EXTERN PetscErrorCode ISCreateStride(MPI_Comm, PetscInt, PetscInt, PetscInt, IS *);
PETSC_EXTERN PetscErrorCode ISStrideSetStride(IS, PetscInt, PetscInt, PetscInt);
PETSC_EXTERN PetscErrorCode ISStrideGetInfo(IS, PetscInt *, PetscInt *);

PETSC_EXTERN PetscClassId IS_LTOGM_CLASSID;

/*E
    ISGlobalToLocalMappingMode - Indicates mapping behavior if global indices are missing

   Values:
+   `IS_GTOLM_MASK` - missing global indices are masked by mapping them to a local index of -1
-   `IS_GTOLM_DROP` - missing global indices are dropped

   Level: beginner

.seealso: `ISGlobalToLocalMappingApplyBlock()`, `ISGlobalToLocalMappingApply()`
E*/
typedef enum {
  IS_GTOLM_MASK,
  IS_GTOLM_DROP
} ISGlobalToLocalMappingMode;

/*J
    ISLocalToGlobalMappingType - String with the name of a mapping method

   Values:
+  `ISLOCALTOGLOBALMAPPINGBASIC` - a non-memory scalable way of storing `ISLocalToGlobalMapping` that allows applying `ISGlobalToLocalMappingApply()` efficiently
-  `ISLOCALTOGLOBALMAPPINGHASH` - a memory scalable way of storing `ISLocalToGlobalMapping` that allows applying `ISGlobalToLocalMappingApply()` reasonably efficiently

  Level: beginner

.seealso: `ISLocalToGlobalMapping`, `ISLocalToGlobalMappingSetType()`, `ISLocalToGlobalSetFromOptions()`, `ISGlobalToLocalMappingMode`
J*/
typedef const char *ISLocalToGlobalMappingType;
#define ISLOCALTOGLOBALMAPPINGBASIC "basic"
#define ISLOCALTOGLOBALMAPPINGHASH  "hash"

PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingSetType(ISLocalToGlobalMapping, ISLocalToGlobalMappingType);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingGetType(ISLocalToGlobalMapping, ISLocalToGlobalMappingType *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingRegister(const char[], PetscErrorCode (*)(ISLocalToGlobalMapping));
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingRegisterAll(void);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingCreate(MPI_Comm, PetscInt, PetscInt, const PetscInt[], PetscCopyMode, ISLocalToGlobalMapping *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingCreateIS(IS, ISLocalToGlobalMapping *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingCreateSF(PetscSF, PetscInt, ISLocalToGlobalMapping *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingSetFromOptions(ISLocalToGlobalMapping);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingSetUp(ISLocalToGlobalMapping);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingView(ISLocalToGlobalMapping, PetscViewer);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingViewFromOptions(ISLocalToGlobalMapping, PetscObject, const char[]);

PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingDestroy(ISLocalToGlobalMapping *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingApply(ISLocalToGlobalMapping, PetscInt, const PetscInt[], PetscInt[]);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingApplyBlock(ISLocalToGlobalMapping, PetscInt, const PetscInt[], PetscInt[]);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingApplyIS(ISLocalToGlobalMapping, IS, IS *);
PETSC_EXTERN PetscErrorCode ISGlobalToLocalMappingApply(ISLocalToGlobalMapping, ISGlobalToLocalMappingMode, PetscInt, const PetscInt[], PetscInt *, PetscInt[]);
PETSC_EXTERN PetscErrorCode ISGlobalToLocalMappingApplyBlock(ISLocalToGlobalMapping, ISGlobalToLocalMappingMode, PetscInt, const PetscInt[], PetscInt *, PetscInt[]);
PETSC_EXTERN PetscErrorCode ISGlobalToLocalMappingApplyIS(ISLocalToGlobalMapping, ISGlobalToLocalMappingMode, IS, IS *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingGetSize(ISLocalToGlobalMapping, PetscInt *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingGetNodeInfo(ISLocalToGlobalMapping, PetscInt *, PetscInt *[], PetscInt **[]);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingRestoreNodeInfo(ISLocalToGlobalMapping, PetscInt *, PetscInt *[], PetscInt **[]);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingGetInfo(ISLocalToGlobalMapping, PetscInt *, PetscInt *[], PetscInt *[], PetscInt **[]);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingRestoreInfo(ISLocalToGlobalMapping, PetscInt *, PetscInt *[], PetscInt *[], PetscInt **[]);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingGetBlockInfo(ISLocalToGlobalMapping, PetscInt *, PetscInt *[], PetscInt *[], PetscInt **[]);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingRestoreBlockInfo(ISLocalToGlobalMapping, PetscInt *, PetscInt *[], PetscInt *[], PetscInt **[]);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingGetIndices(ISLocalToGlobalMapping, const PetscInt **);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingRestoreIndices(ISLocalToGlobalMapping, const PetscInt **);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingGetBlockIndices(ISLocalToGlobalMapping, const PetscInt **);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingRestoreBlockIndices(ISLocalToGlobalMapping, const PetscInt **);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingConcatenate(MPI_Comm, PetscInt, const ISLocalToGlobalMapping[], ISLocalToGlobalMapping *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingGetBlockSize(ISLocalToGlobalMapping, PetscInt *);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingSetBlockSize(ISLocalToGlobalMapping, PetscInt);
PETSC_EXTERN PetscErrorCode ISLocalToGlobalMappingDuplicate(ISLocalToGlobalMapping, ISLocalToGlobalMapping *);

/*E
    ISColoringType - determines if the coloring is for the entire parallel grid/graph/matrix
                     or for just the local ghosted portion

    Values:
+   `IS_COLORING_GLOBAL` - does not include the colors for ghost points, this is used when the function
                        is called synchronously in parallel. This requires generating a "parallel coloring".
-   `IS_COLORING_LOCAL` - includes colors for ghost points, this is used when the function can be called
                         separately on individual processes with the ghost points already filled in. Does not
                         require a "parallel coloring", rather each process colors its local + ghost part.
                         Using this can result in much less parallel communication. Currently only works
                         with `DMDA` and if you call `MatFDColoringSetFunction()` with the local function.

   Level: beginner

.seealso: `ISColoring`, `ISColoringSetType()`, `ISColoringGetType()`, `DMCreateColoring()`
E*/
typedef enum {
  IS_COLORING_GLOBAL,
  IS_COLORING_LOCAL
} ISColoringType;
PETSC_EXTERN const char *const                ISColoringTypes[];
typedef unsigned PETSC_IS_COLORING_VALUE_TYPE ISColoringValue;
#define IS_COLORING_MAX     PETSC_IS_COLORING_MAX
#define MPIU_COLORING_VALUE PETSC_MPIU_IS_COLORING_VALUE_TYPE
PETSC_EXTERN PetscErrorCode ISAllGatherColors(MPI_Comm, PetscInt, ISColoringValue *, PetscInt *, ISColoringValue *[]);

PETSC_EXTERN PetscErrorCode ISColoringCreate(MPI_Comm, PetscInt, PetscInt, const ISColoringValue[], PetscCopyMode, ISColoring *);
PETSC_EXTERN PetscErrorCode ISColoringDestroy(ISColoring *);
PETSC_EXTERN PetscErrorCode ISColoringView(ISColoring, PetscViewer);
PETSC_EXTERN PetscErrorCode ISColoringViewFromOptions(ISColoring, PetscObject, const char[]);
PETSC_EXTERN PetscErrorCode ISColoringGetIS(ISColoring, PetscCopyMode, PetscInt *, IS *[]);
PETSC_EXTERN PetscErrorCode ISColoringRestoreIS(ISColoring, PetscCopyMode, IS *[]);
PETSC_EXTERN PetscErrorCode ISColoringReference(ISColoring);
PETSC_EXTERN PetscErrorCode ISColoringSetType(ISColoring, ISColoringType);
PETSC_EXTERN PetscErrorCode ISColoringGetType(ISColoring, ISColoringType *);
PETSC_EXTERN PetscErrorCode ISColoringGetColors(ISColoring, PetscInt *, PetscInt *, const ISColoringValue **);

PETSC_EXTERN PetscErrorCode ISBuildTwoSided(IS, IS, IS *);
PETSC_EXTERN PetscErrorCode ISPartitioningToNumbering(IS, IS *);
PETSC_EXTERN PetscErrorCode ISPartitioningCount(IS, PetscInt, PetscInt[]);

PETSC_EXTERN PetscErrorCode ISCompressIndicesGeneral(PetscInt, PetscInt, PetscInt, PetscInt, const IS[], IS[]);
PETSC_DEPRECATED_FUNCTION("Use ISCompressIndicesGeneral() (since version 3.19)") static inline PetscErrorCode ISCompressIndicesSorted(PetscInt n, PetscInt bs, PetscInt imax, const IS is_in[], IS is_out[])
{
  return ISCompressIndicesGeneral(n, bs, n, imax, is_in, is_out);
}
PETSC_EXTERN PetscErrorCode ISExpandIndicesGeneral(PetscInt, PetscInt, PetscInt, PetscInt, const IS[], IS[]);

struct _n_PetscLayout {
  MPI_Comm               comm;
  PetscMPIInt            size;
  PetscInt               n, N;         /* local, global vector size */
  PetscInt               rstart, rend; /* local start, local end + 1 */
  PetscInt              *range;        /* the offset of each processor */
  PetscBool              range_alloc;  /* should range be freed in Destroy? */
  PetscInt               bs;           /* number of elements in each block (generally for multi-component
                                       * problems). Defaults to -1 and can be arbitrarily lazy so always use
                                       * PetscAbs(map->bs) when accessing directly and expecting result to be
                                       * positive. Do NOT multiply above numbers by bs */
  PetscInt               refcnt;       /* MPI Vecs obtained with VecDuplicate() and from MatCreateVecs() reuse map of input object */
  ISLocalToGlobalMapping mapping;      /* mapping used in Vec/MatSetValuesLocal() */
  PetscBool              setupcalled;  /* Forbid setup more than once */
  PetscInt               oldn, oldN;   /* Checking if setup is allowed */
  PetscInt               oldbs;        /* And again */
};

/*@C
     PetscLayoutFindOwner - Find the owning rank for a global index

    Not Collective; No Fortran Support

   Input Parameters:
+    map - the layout
-    idx - global index to find the owner of

   Output Parameter:
.    owner - the owning rank

   Level: developer

.seealso: `PetscLayout`, `PetscLayoutFindOwnerIndex()`
@*/
static inline PetscErrorCode PetscLayoutFindOwner(PetscLayout map, PetscInt idx, PetscMPIInt *owner)
{
  PetscMPIInt lo = 0, hi, t;

  PetscFunctionBegin;
  *owner = -1; /* GCC erroneously issues warning about possibly uninitialized use when error condition */
#if defined(PETSC_USE_DEBUG)
  PetscCheck((map->n >= 0) && (map->N >= 0) && (map->range), PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "PetscLayoutSetUp() must be called first");
  PetscCheck(idx >= 0 && idx <= map->N, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Index %" PetscInt_FMT " is out of range", idx);
#endif
  hi = map->size;
  while (hi - lo > 1) {
    t = lo + (hi - lo) / 2;
    if (idx < map->range[t]) hi = t;
    else lo = t;
  }
  *owner = lo;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
     PetscLayoutFindOwnerIndex - Find the owning MPI rank and the local index on that rank for a global index

    Not Collective; No Fortran Support

   Input Parameters:
+    map   - the layout
-    idx   - global index to find the owner of

   Output Parameters:
+    owner - the owning rank
-    lidx  - local index used by the owner for `idx`

   Level: developer

.seealso: `PetscLayout`, `PetscLayoutFindOwner()`
@*/
static inline PetscErrorCode PetscLayoutFindOwnerIndex(PetscLayout map, PetscInt idx, PetscMPIInt *owner, PetscInt *lidx)
{
  PetscMPIInt lo = 0, hi, t;

  PetscFunctionBegin;
#if defined(PETSC_USE_DEBUG)
  PetscCheck((map->n >= 0) && (map->N >= 0) && (map->range), PETSC_COMM_SELF, PETSC_ERR_ARG_WRONGSTATE, "PetscLayoutSetUp() must be called first");
  PetscCheck(idx >= 0 && idx <= map->N, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Index %" PetscInt_FMT " is out of range", idx);
#endif
  hi = map->size;
  while (hi - lo > 1) {
    t = lo + (hi - lo) / 2;
    if (idx < map->range[t]) hi = t;
    else lo = t;
  }
  if (owner) *owner = lo;
  if (lidx) *lidx = idx - map->range[lo];
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN PetscErrorCode PetscLayoutCreate(MPI_Comm, PetscLayout *);
PETSC_EXTERN PetscErrorCode PetscLayoutCreateFromSizes(MPI_Comm, PetscInt, PetscInt, PetscInt, PetscLayout *);
PETSC_EXTERN PetscErrorCode PetscLayoutCreateFromRanges(MPI_Comm, const PetscInt[], PetscCopyMode, PetscInt, PetscLayout *);
PETSC_EXTERN PetscErrorCode PetscLayoutSetUp(PetscLayout);
PETSC_EXTERN PetscErrorCode PetscLayoutDestroy(PetscLayout *);
PETSC_EXTERN PetscErrorCode PetscLayoutDuplicate(PetscLayout, PetscLayout *);
PETSC_EXTERN PetscErrorCode PetscLayoutReference(PetscLayout, PetscLayout *);
PETSC_EXTERN PetscErrorCode PetscLayoutSetLocalSize(PetscLayout, PetscInt);
PETSC_EXTERN PetscErrorCode PetscLayoutGetLocalSize(PetscLayout, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscLayoutSetSize(PetscLayout, PetscInt);
PETSC_EXTERN PetscErrorCode PetscLayoutGetSize(PetscLayout, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscLayoutSetBlockSize(PetscLayout, PetscInt);
PETSC_EXTERN PetscErrorCode PetscLayoutGetBlockSize(PetscLayout, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscLayoutGetRange(PetscLayout, PetscInt *, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscLayoutGetRanges(PetscLayout, const PetscInt *[]);
PETSC_EXTERN PetscErrorCode PetscLayoutCompare(PetscLayout, PetscLayout, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscLayoutSetISLocalToGlobalMapping(PetscLayout, ISLocalToGlobalMapping);
PETSC_EXTERN PetscErrorCode PetscLayoutMapLocal(PetscLayout, PetscInt, const PetscInt[], PetscInt *, PetscInt **, PetscInt **);

PETSC_EXTERN PetscErrorCode PetscParallelSortInt(PetscLayout, PetscLayout, PetscInt *, PetscInt *);

PETSC_EXTERN PetscErrorCode ISGetLayout(IS, PetscLayout *);
PETSC_EXTERN PetscErrorCode ISSetLayout(IS, PetscLayout);

#endif

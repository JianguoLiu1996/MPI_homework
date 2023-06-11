/*
   Defines the interface functions for the Krylov subspace accelerators.
*/
#ifndef PETSCKSP_H
#define PETSCKSP_H

#include <petscpc.h>

/* SUBMANSEC = KSP */

PETSC_EXTERN PetscErrorCode KSPInitializePackage(void);

/*S
     KSP - Abstract PETSc object that manages all Krylov methods. This is the object that manages the
         linear solves in PETSc (even those such as direct factorization-based solvers that do no use Krylov accelerators).

   Level: beginner

   Note:
    When a direct solver is used, but no Krylov solver is used, the `KSP` object is still used but with a
    `KSPType` of `KSPPREONLY` (or equivalently `KSPNONE`), meaning that only application of the preconditioner is used as the linear solver.

.seealso: [](doc_linsolve), [](chapter_ksp), `KSPCreate()`, `KSPSetType()`, `KSPType`, `SNES`, `TS`, `PC`, `KSP`, `KSPDestroy()`, `KSPCG`, `KSPGMRES`
S*/
typedef struct _p_KSP *KSP;

/*J
    KSPType - String with the name of a PETSc Krylov method.

   Level: beginner

.seealso: [](doc_linsolve), [](chapter_ksp), `KSPSetType()`, `KSP`, `KSPRegister()`, `KSPCreate()`, `KSPSetFromOptions()`
J*/
typedef const char *KSPType;
#define KSPRICHARDSON "richardson"
#define KSPCHEBYSHEV  "chebyshev"
#define KSPCG         "cg"
#define KSPGROPPCG    "groppcg"
#define KSPPIPECG     "pipecg"
#define KSPPIPECGRR   "pipecgrr"
#define KSPPIPELCG    "pipelcg"
#define KSPPIPEPRCG   "pipeprcg"
#define KSPPIPECG2    "pipecg2"
#define KSPCGNE       "cgne"
#define KSPNASH       "nash"
#define KSPSTCG       "stcg"
#define KSPGLTR       "gltr"
#define KSPCGNASH     PETSC_DEPRECATED_MACRO("GCC warning \"KSPCGNASH macro is deprecated use KSPNASH (since version 3.11)\"") "nash"
#define KSPCGSTCG     PETSC_DEPRECATED_MACRO("GCC warning \"KSPCGSTCG macro is deprecated use KSPSTCG (since version 3.11)\"") "stcg"
#define KSPCGGLTR     PETSC_DEPRECATED_MACRO("GCC warning \"KSPCGGLTR macro is deprecated use KSPSGLTR (since version 3.11)\"") "gltr"
#define KSPFCG        "fcg"
#define KSPPIPEFCG    "pipefcg"
#define KSPGMRES      "gmres"
#define KSPPIPEFGMRES "pipefgmres"
#define KSPFGMRES     "fgmres"
#define KSPLGMRES     "lgmres"
#define KSPDGMRES     "dgmres"
#define KSPPGMRES     "pgmres"
#define KSPTCQMR      "tcqmr"
#define KSPBCGS       "bcgs"
#define KSPIBCGS      "ibcgs"
#define KSPQMRCGS     "qmrcgs"
#define KSPFBCGS      "fbcgs"
#define KSPFBCGSR     "fbcgsr"
#define KSPBCGSL      "bcgsl"
#define KSPPIPEBCGS   "pipebcgs"
#define KSPCGS        "cgs"
#define KSPTFQMR      "tfqmr"
#define KSPCR         "cr"
#define KSPPIPECR     "pipecr"
#define KSPLSQR       "lsqr"
#define KSPPREONLY    "preonly"
#define KSPNONE       "none"
#define KSPQCG        "qcg"
#define KSPBICG       "bicg"
#define KSPMINRES     "minres"
#define KSPSYMMLQ     "symmlq"
#define KSPLCD        "lcd"
#define KSPPYTHON     "python"
#define KSPGCR        "gcr"
#define KSPPIPEGCR    "pipegcr"
#define KSPTSIRM      "tsirm"
#define KSPCGLS       "cgls"
#define KSPFETIDP     "fetidp"
#define KSPHPDDM      "hpddm"

/* Logging support */
PETSC_EXTERN PetscClassId KSP_CLASSID;
PETSC_EXTERN PetscClassId KSPGUESS_CLASSID;
PETSC_EXTERN PetscClassId DMKSP_CLASSID;

PETSC_EXTERN PetscErrorCode KSPCreate(MPI_Comm, KSP *);
PETSC_EXTERN PetscErrorCode KSPSetType(KSP, KSPType);
PETSC_EXTERN PetscErrorCode KSPGetType(KSP, KSPType *);
PETSC_EXTERN PetscErrorCode KSPSetUp(KSP);
PETSC_EXTERN PetscErrorCode KSPSetUpOnBlocks(KSP);
PETSC_EXTERN PetscErrorCode KSPSolve(KSP, Vec, Vec);
PETSC_EXTERN PetscErrorCode KSPSolveTranspose(KSP, Vec, Vec);
PETSC_EXTERN PetscErrorCode KSPSetUseExplicitTranspose(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPMatSolve(KSP, Mat, Mat);
PETSC_EXTERN PetscErrorCode KSPMatSolveTranspose(KSP, Mat, Mat);
PETSC_EXTERN PetscErrorCode KSPSetMatSolveBatchSize(KSP, PetscInt);
PETSC_DEPRECATED_FUNCTION("Use KSPSetMatSolveBatchSize() (since version 3.15)") static inline PetscErrorCode KSPSetMatSolveBlockSize(KSP ksp, PetscInt n)
{
  return KSPSetMatSolveBatchSize(ksp, n);
}
PETSC_EXTERN PetscErrorCode KSPGetMatSolveBatchSize(KSP, PetscInt *);
PETSC_DEPRECATED_FUNCTION("Use KSPGetMatSolveBatchSize() (since version 3.15)") static inline PetscErrorCode KSPGetMatSolveBlockSize(KSP ksp, PetscInt *n)
{
  return KSPGetMatSolveBatchSize(ksp, n);
}
PETSC_EXTERN PetscErrorCode KSPReset(KSP);
PETSC_EXTERN PetscErrorCode KSPResetViewers(KSP);
PETSC_EXTERN PetscErrorCode KSPDestroy(KSP *);
PETSC_EXTERN PetscErrorCode KSPSetReusePreconditioner(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetReusePreconditioner(KSP, PetscBool *);
PETSC_EXTERN PetscErrorCode KSPSetSkipPCSetFromOptions(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPCheckSolve(KSP, PC, Vec);

PETSC_EXTERN PetscFunctionList KSPList;
PETSC_EXTERN PetscFunctionList KSPGuessList;
PETSC_EXTERN PetscFunctionList KSPMonitorList;
PETSC_EXTERN PetscFunctionList KSPMonitorCreateList;
PETSC_EXTERN PetscFunctionList KSPMonitorDestroyList;
PETSC_EXTERN PetscErrorCode    KSPRegister(const char[], PetscErrorCode (*)(KSP));
PETSC_EXTERN PetscErrorCode KSPMonitorRegister(const char[], PetscViewerType, PetscViewerFormat, PetscErrorCode (*)(KSP, PetscInt, PetscReal, PetscViewerAndFormat *), PetscErrorCode (*)(PetscViewer, PetscViewerFormat, void *, PetscViewerAndFormat **), PetscErrorCode (*)(PetscViewerAndFormat **));

PETSC_EXTERN PetscErrorCode KSPSetPCSide(KSP, PCSide);
PETSC_EXTERN PetscErrorCode KSPGetPCSide(KSP, PCSide *);
PETSC_EXTERN PetscErrorCode KSPSetTolerances(KSP, PetscReal, PetscReal, PetscReal, PetscInt);
PETSC_EXTERN PetscErrorCode KSPGetTolerances(KSP, PetscReal *, PetscReal *, PetscReal *, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPSetInitialGuessNonzero(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetInitialGuessNonzero(KSP, PetscBool *);
PETSC_EXTERN PetscErrorCode KSPSetErrorIfNotConverged(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetErrorIfNotConverged(KSP, PetscBool *);
PETSC_EXTERN PetscErrorCode KSPSetComputeEigenvalues(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPSetComputeRitz(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetComputeEigenvalues(KSP, PetscBool *);
PETSC_EXTERN PetscErrorCode KSPSetComputeSingularValues(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetComputeSingularValues(KSP, PetscBool *);
PETSC_EXTERN PetscErrorCode KSPGetRhs(KSP, Vec *);
PETSC_EXTERN PetscErrorCode KSPGetSolution(KSP, Vec *);
PETSC_EXTERN PetscErrorCode KSPGetResidualNorm(KSP, PetscReal *);
PETSC_EXTERN PetscErrorCode KSPGetIterationNumber(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPGetTotalIterations(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPCreateVecs(KSP, PetscInt, Vec **, PetscInt, Vec **);
PETSC_DEPRECATED_FUNCTION("Use KSPCreateVecs() (since version 3.6)") static inline PetscErrorCode KSPGetVecs(KSP ksp, PetscInt n, Vec **x, PetscInt m, Vec **y)
{
  return KSPCreateVecs(ksp, n, x, m, y);
}

PETSC_EXTERN PetscErrorCode KSPSetPreSolve(KSP, PetscErrorCode (*)(KSP, Vec, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode KSPSetPostSolve(KSP, PetscErrorCode (*)(KSP, Vec, Vec, void *), void *);

PETSC_EXTERN PetscErrorCode KSPSetPC(KSP, PC);
PETSC_EXTERN PetscErrorCode KSPGetPC(KSP, PC *);

PETSC_EXTERN PetscErrorCode KSPMonitor(KSP, PetscInt, PetscReal);
PETSC_EXTERN PetscErrorCode KSPMonitorSet(KSP, PetscErrorCode (*)(KSP, PetscInt, PetscReal, void *), void *, PetscErrorCode (*)(void **));
PETSC_EXTERN PetscErrorCode KSPMonitorCancel(KSP);
PETSC_EXTERN PetscErrorCode KSPGetMonitorContext(KSP, void *);
PETSC_EXTERN PetscErrorCode KSPGetResidualHistory(KSP, const PetscReal *[], PetscInt *);
PETSC_EXTERN PetscErrorCode KSPSetResidualHistory(KSP, PetscReal[], PetscInt, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetErrorHistory(KSP, const PetscReal *[], PetscInt *);
PETSC_EXTERN PetscErrorCode KSPSetErrorHistory(KSP, PetscReal[], PetscInt, PetscBool);

PETSC_EXTERN PetscErrorCode KSPBuildSolutionDefault(KSP, Vec, Vec *);
PETSC_EXTERN PetscErrorCode KSPBuildResidualDefault(KSP, Vec, Vec, Vec *);
PETSC_EXTERN PetscErrorCode KSPDestroyDefault(KSP);
PETSC_EXTERN PetscErrorCode KSPSetWorkVecs(KSP, PetscInt);

PETSC_EXTERN PetscErrorCode PCKSPGetKSP(PC, KSP *);
PETSC_EXTERN PetscErrorCode PCKSPSetKSP(PC, KSP);
PETSC_EXTERN PetscErrorCode PCBJacobiGetSubKSP(PC, PetscInt *, PetscInt *, KSP *[]);
PETSC_EXTERN PetscErrorCode PCASMGetSubKSP(PC, PetscInt *, PetscInt *, KSP *[]);
PETSC_EXTERN PetscErrorCode PCGASMGetSubKSP(PC, PetscInt *, PetscInt *, KSP *[]);
PETSC_EXTERN PetscErrorCode PCFieldSplitGetSubKSP(PC, PetscInt *, KSP *[]);
PETSC_EXTERN PetscErrorCode PCFieldSplitSchurGetSubKSP(PC, PetscInt *, KSP *[]);
PETSC_EXTERN PetscErrorCode PCMGGetSmoother(PC, PetscInt, KSP *);
PETSC_EXTERN PetscErrorCode PCMGGetSmootherDown(PC, PetscInt, KSP *);
PETSC_EXTERN PetscErrorCode PCMGGetSmootherUp(PC, PetscInt, KSP *);
PETSC_EXTERN PetscErrorCode PCMGGetCoarseSolve(PC, KSP *);
PETSC_EXTERN PetscErrorCode PCGalerkinGetKSP(PC, KSP *);
PETSC_EXTERN PetscErrorCode PCDeflationGetCoarseKSP(PC, KSP *);
/*
  PCMGCoarseList contains the list of coarse space constructor currently registered
  These are added with PCMGRegisterCoarseSpaceConstructor()
*/
PETSC_EXTERN PetscFunctionList PCMGCoarseList;
PETSC_EXTERN PetscErrorCode    PCMGRegisterCoarseSpaceConstructor(const char[], PetscErrorCode (*)(PC, PetscInt, DM, KSP, PetscInt, Mat, Mat *));
PETSC_EXTERN PetscErrorCode    PCMGGetCoarseSpaceConstructor(const char[], PetscErrorCode (**)(PC, PetscInt, DM, KSP, PetscInt, Mat, Mat *));

PETSC_EXTERN PetscErrorCode KSPBuildSolution(KSP, Vec, Vec *);
PETSC_EXTERN PetscErrorCode KSPBuildResidual(KSP, Vec, Vec, Vec *);

/*E
  KSPChebyshevKind - Which kind of Chebyshev polynomial to use

  Values:
+ `KSP_CHEBYSHEV_FIRST`      - "classic" first-kind Chebyshev polynomial
. `KSP_CHEBYSHEV_FOURTH`     - fourth-kind Chebyshev polynomial
- `KSP_CHEBYSHEV_OPT_FOURTH` - optimized fourth-kind Chebyshev polynomial

  Level: intermediate

.seealso: [](chapter_ksp), `KSPCHEBYSHEV`, `KSPChebyshevSetKind()`, `KSPChebyshevGetKind()`
E*/
typedef enum {
  KSP_CHEBYSHEV_FIRST,
  KSP_CHEBYSHEV_FOURTH,
  KSP_CHEBYSHEV_OPT_FOURTH
} KSPChebyshevKind;

PETSC_EXTERN PetscErrorCode KSPRichardsonSetScale(KSP, PetscReal);
PETSC_EXTERN PetscErrorCode KSPRichardsonSetSelfScale(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPChebyshevSetEigenvalues(KSP, PetscReal, PetscReal);
PETSC_EXTERN PetscErrorCode KSPChebyshevEstEigSet(KSP, PetscReal, PetscReal, PetscReal, PetscReal);
PETSC_EXTERN PetscErrorCode KSPChebyshevEstEigSetUseNoisy(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPChebyshevSetKind(KSP, KSPChebyshevKind);
PETSC_EXTERN PetscErrorCode KSPChebyshevGetKind(KSP, KSPChebyshevKind *);
PETSC_EXTERN PetscErrorCode KSPChebyshevEstEigGetKSP(KSP, KSP *);
PETSC_EXTERN PetscErrorCode KSPComputeExtremeSingularValues(KSP, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode KSPComputeEigenvalues(KSP, PetscInt, PetscReal[], PetscReal[], PetscInt *);
PETSC_EXTERN PetscErrorCode KSPComputeEigenvaluesExplicitly(KSP, PetscInt, PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode KSPComputeRitz(KSP, PetscBool, PetscBool, PetscInt *, Vec[], PetscReal[], PetscReal[]);

/*E

  KSPFCDTruncationType - Define how stored directions are used to orthogonalize in flexible conjugate directions (FCD) methods

  Values:
+ `KSP_FCD_TRUNC_TYPE_STANDARD` - uses all (up to mmax) stored directions
- `KSP_FCD_TRUNC_TYPE_NOTAY` - uses the last max(1,mod(i,mmax)) stored directions at iteration i=0,1..

   Level: intermediate

.seealso: [](chapter_ksp), `KSP`, `KSPFCG`, `KSPPIPEFCG`, `KSPPIPEGCR`, `KSPFCGSetTruncationType()`, `KSPFCGGetTruncationType()`
E*/
typedef enum {
  KSP_FCD_TRUNC_TYPE_STANDARD,
  KSP_FCD_TRUNC_TYPE_NOTAY
} KSPFCDTruncationType;
PETSC_EXTERN const char *const KSPFCDTruncationTypes[];

PETSC_EXTERN PetscErrorCode KSPFCGSetMmax(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPFCGGetMmax(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPFCGSetNprealloc(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPFCGGetNprealloc(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPFCGSetTruncationType(KSP, KSPFCDTruncationType);
PETSC_EXTERN PetscErrorCode KSPFCGGetTruncationType(KSP, KSPFCDTruncationType *);

PETSC_EXTERN PetscErrorCode KSPPIPEFCGSetMmax(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPPIPEFCGGetMmax(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPPIPEFCGSetNprealloc(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPPIPEFCGGetNprealloc(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPPIPEFCGSetTruncationType(KSP, KSPFCDTruncationType);
PETSC_EXTERN PetscErrorCode KSPPIPEFCGGetTruncationType(KSP, KSPFCDTruncationType *);

PETSC_EXTERN PetscErrorCode KSPPIPEGCRSetMmax(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPPIPEGCRGetMmax(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPPIPEGCRSetNprealloc(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPPIPEGCRGetNprealloc(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPPIPEGCRSetTruncationType(KSP, KSPFCDTruncationType);
PETSC_EXTERN PetscErrorCode KSPPIPEGCRGetTruncationType(KSP, KSPFCDTruncationType *);
PETSC_EXTERN PetscErrorCode KSPPIPEGCRSetUnrollW(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPPIPEGCRGetUnrollW(KSP, PetscBool *);

PETSC_EXTERN PetscErrorCode KSPGMRESSetRestart(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPGMRESGetRestart(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPGMRESSetHapTol(KSP, PetscReal);
PETSC_EXTERN PetscErrorCode KSPGMRESSetBreakdownTolerance(KSP, PetscReal);

PETSC_EXTERN PetscErrorCode KSPGMRESSetPreAllocateVectors(KSP);
PETSC_EXTERN PetscErrorCode KSPGMRESSetOrthogonalization(KSP, PetscErrorCode (*)(KSP, PetscInt));
PETSC_EXTERN PetscErrorCode KSPGMRESGetOrthogonalization(KSP, PetscErrorCode (**)(KSP, PetscInt));
PETSC_EXTERN PetscErrorCode KSPGMRESModifiedGramSchmidtOrthogonalization(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPGMRESClassicalGramSchmidtOrthogonalization(KSP, PetscInt);

PETSC_EXTERN PetscErrorCode KSPLGMRESSetAugDim(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPLGMRESSetConstant(KSP);

PETSC_EXTERN PetscErrorCode KSPPIPEFGMRESSetShift(KSP, PetscScalar);

PETSC_EXTERN PetscErrorCode KSPGCRSetRestart(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPGCRGetRestart(KSP, PetscInt *);
PETSC_EXTERN PetscErrorCode KSPGCRSetModifyPC(KSP, PetscErrorCode (*)(KSP, PetscInt, PetscReal, void *), void *, PetscErrorCode (*)(void *));

PETSC_EXTERN PetscErrorCode KSPMINRESSetRadius(KSP, PetscReal);
PETSC_EXTERN PetscErrorCode KSPMINRESGetUseQLP(KSP, PetscBool *);
PETSC_EXTERN PetscErrorCode KSPMINRESSetUseQLP(KSP, PetscBool);

PETSC_EXTERN PetscErrorCode KSPFETIDPGetInnerBDDC(KSP, PC *);
PETSC_EXTERN PetscErrorCode KSPFETIDPSetInnerBDDC(KSP, PC);
PETSC_EXTERN PetscErrorCode KSPFETIDPGetInnerKSP(KSP, KSP *);
PETSC_EXTERN PetscErrorCode KSPFETIDPSetPressureOperator(KSP, Mat);

PETSC_EXTERN PetscErrorCode KSPHPDDMSetDeflationMat(KSP, Mat);
PETSC_EXTERN PetscErrorCode KSPHPDDMGetDeflationMat(KSP, Mat *);
#if PetscDefined(HAVE_HPDDM)
PETSC_DEPRECATED_FUNCTION("Use KSPHPDDMSetDeflationMat() (since version 3.18)") static inline PetscErrorCode KSPHPDDMSetDeflationSpace(KSP ksp, Mat U)
{
  return KSPHPDDMSetDeflationMat(ksp, U);
}
PETSC_DEPRECATED_FUNCTION("Use KSPHPDDMGetDeflationMat() (since version 3.18)") static inline PetscErrorCode KSPHPDDMGetDeflationSpace(KSP ksp, Mat *U)
{
  return KSPHPDDMGetDeflationMat(ksp, U);
}
#endif
PETSC_DEPRECATED_FUNCTION("Use KSPMatSolve() (since version 3.14)") static inline PetscErrorCode KSPHPDDMMatSolve(KSP ksp, Mat B, Mat X)
{
  return KSPMatSolve(ksp, B, X);
}
/*E
    KSPHPDDMType - Type of Krylov method used by `KSPHPDDM`

    Values:
+   `KSP_HPDDM_TYPE_GMRES` (default) - Generalized Minimal Residual method
.   `KSP_HPDDM_TYPE_BGMRES` - block GMRES
.   `KSP_HPDDM_TYPE_CG` - Conjugate Gradient
.   `KSP_HPDDM_TYPE_BCG` - block CG
.   `KSP_HPDDM_TYPE_GCRODR` - Generalized Conjugate Residual method with inner Orthogonalization and Deflated Restarting
.   `KSP_HPDDM_TYPE_BGCRODR` - block GCRODR
.   `KSP_HPDDM_TYPE_BFBCG` - breakdown-free BCG
-   `KSP_HPDDM_TYPE_PREONLY` - apply the preconditioner only

    Level: intermediate

.seealso: [](chapter_ksp), `KSPHPDDM`, `KSPHPDDMSetType()`
E*/
typedef enum {
  KSP_HPDDM_TYPE_GMRES   = 0,
  KSP_HPDDM_TYPE_BGMRES  = 1,
  KSP_HPDDM_TYPE_CG      = 2,
  KSP_HPDDM_TYPE_BCG     = 3,
  KSP_HPDDM_TYPE_GCRODR  = 4,
  KSP_HPDDM_TYPE_BGCRODR = 5,
  KSP_HPDDM_TYPE_BFBCG   = 6,
  KSP_HPDDM_TYPE_PREONLY = 7
} KSPHPDDMType;
PETSC_EXTERN const char *const KSPHPDDMTypes[];

/*E
    KSPHPDDMPrecision - Precision of Krylov bases used by `KSPHPDDM`

    Values:
+   `KSP_HPDDM_PRECISION_HALF` - default when PETSc is configured `--with-precision=__fp16`
.   `KSP_HPDDM_PRECISION_SINGLE` - default when PETSc is configured `--with-precision=single`
.   `KSP_HPDDM_PRECISION_DOUBLE` - default when PETSc is configured `--with-precision=double`
-   `KSP_HPDDM_PRECISION_QUADRUPLE` - default when PETSc is configured `--with-precision=__float128`

    Level: intermediate

.seealso: [](chapter_ksp), `KSP`, `KSPHPDDM`
E*/
typedef enum {
  KSP_HPDDM_PRECISION_HALF      = 0,
  KSP_HPDDM_PRECISION_SINGLE    = 1,
  KSP_HPDDM_PRECISION_DOUBLE    = 2,
  KSP_HPDDM_PRECISION_QUADRUPLE = 3
} KSPHPDDMPrecision;
PETSC_EXTERN PetscErrorCode KSPHPDDMSetType(KSP, KSPHPDDMType);
PETSC_EXTERN PetscErrorCode KSPHPDDMGetType(KSP, KSPHPDDMType *);

/*E
    KSPGMRESCGSRefinementType - How the classical (unmodified) Gram-Schmidt is performed in the GMRES solvers

    Values:
+   `KSP_GMRES_CGS_REFINE_NEVER` - one step of classical Gram-Schmidt
.   `KSP_GMRES_CGS_REFINE_IFNEEDED` - a second step is performed if the first step does not satisfy some criteria
-   `KSP_GMRES_CGS_REFINE_ALWAYS` - always perform two steps

   Level: advanced

.seealso: [](chapter_ksp), `KSP`, `KSPGMRESClassicalGramSchmidtOrthogonalization()`, `KSPGMRESSetOrthogonalization()`,
          `KSPGMRESGetOrthogonalization()`,
          `KSPGMRESSetCGSRefinementType()`, `KSPGMRESGetCGSRefinementType()`, `KSPGMRESModifiedGramSchmidtOrthogonalization()`
E*/
typedef enum {
  KSP_GMRES_CGS_REFINE_NEVER,
  KSP_GMRES_CGS_REFINE_IFNEEDED,
  KSP_GMRES_CGS_REFINE_ALWAYS
} KSPGMRESCGSRefinementType;
PETSC_EXTERN const char *const KSPGMRESCGSRefinementTypes[];

/*MC
    KSP_GMRES_CGS_REFINE_NEVER - Do the classical (unmodified) Gram-Schmidt process

   Level: advanced

   Note:
   Possibly unstable, but the fastest to compute

.seealso: [](chapter_ksp), `KSPGMRESCGSRefinementType`, `KSPGMRESClassicalGramSchmidtOrthogonalization()`, `KSPGMRESSetOrthogonalization()`,
          `KSP`, `KSPGMRESGetOrthogonalization()`,
          `KSPGMRESSetCGSRefinementType()`, `KSPGMRESGetCGSRefinementType()`, `KSP_GMRES_CGS_REFINE_IFNEEDED`, `KSP_GMRES_CGS_REFINE_ALWAYS`,
          `KSPGMRESModifiedGramSchmidtOrthogonalization()`
M*/

/*MC
    KSP_GMRES_CGS_REFINE_IFNEEDED - Do the classical (unmodified) Gram-Schmidt process and one step of
          iterative refinement if an estimate of the orthogonality of the resulting vectors indicates
          poor orthogonality.

   Level: advanced

   Note:
   This is slower than `KSP_GMRES_CGS_REFINE_NEVER` because it requires an extra norm computation to
   estimate the orthogonality but is more stable.

.seealso: [](chapter_ksp), `KSPGMRESCGSRefinementType`, `KSPGMRESClassicalGramSchmidtOrthogonalization()`, `KSPGMRESSetOrthogonalization()`,
          `KSP`, `KSPGMRESGetOrthogonalization()`,
          `KSPGMRESSetCGSRefinementType()`, `KSPGMRESGetCGSRefinementType()`, `KSP_GMRES_CGS_REFINE_NEVER`, `KSP_GMRES_CGS_REFINE_ALWAYS`,
          `KSPGMRESModifiedGramSchmidtOrthogonalization()`
M*/

/*MC
    KSP_GMRES_CGS_REFINE_ALWAYS - Do two steps of the classical (unmodified) Gram-Schmidt process.

   Level: advanced

   Notes:
   This is roughly twice the cost of `KSP_GMRES_CGS_REFINE_NEVER` because it performs the process twice
   but it saves the extra norm calculation needed by `KSP_GMRES_CGS_REFINE_IFNEEDED`.

   You should only use this if you absolutely know that the iterative refinement is needed.

.seealso: [](chapter_ksp), `KSPGMRESCGSRefinementType`, `KSPGMRESClassicalGramSchmidtOrthogonalization()`, `KSPGMRESSetOrthogonalization()`,
          `KSP`, `KSPGMRESGetOrthogonalization()`,
          `KSPGMRESSetCGSRefinementType()`, `KSPGMRESGetCGSRefinementType()`, `KSP_GMRES_CGS_REFINE_IFNEEDED`, `KSP_GMRES_CGS_REFINE_ALWAYS`,
          `KSPGMRESModifiedGramSchmidtOrthogonalization()`
M*/

PETSC_EXTERN PetscErrorCode KSPGMRESSetCGSRefinementType(KSP, KSPGMRESCGSRefinementType);
PETSC_EXTERN PetscErrorCode KSPGMRESGetCGSRefinementType(KSP, KSPGMRESCGSRefinementType *);

PETSC_EXTERN PetscErrorCode KSPFGMRESModifyPCNoChange(KSP, PetscInt, PetscInt, PetscReal, void *);
PETSC_EXTERN PetscErrorCode KSPFGMRESModifyPCKSP(KSP, PetscInt, PetscInt, PetscReal, void *);
PETSC_EXTERN PetscErrorCode KSPFGMRESSetModifyPC(KSP, PetscErrorCode (*)(KSP, PetscInt, PetscInt, PetscReal, void *), void *, PetscErrorCode (*)(void *));

PETSC_EXTERN PetscErrorCode KSPQCGSetTrustRegionRadius(KSP, PetscReal);
PETSC_EXTERN PetscErrorCode KSPQCGGetQuadratic(KSP, PetscReal *);
PETSC_EXTERN PetscErrorCode KSPQCGGetTrialStepNorm(KSP, PetscReal *);

PETSC_EXTERN PetscErrorCode KSPBCGSLSetXRes(KSP, PetscReal);
PETSC_EXTERN PetscErrorCode KSPBCGSLSetPol(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPBCGSLSetEll(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPBCGSLSetUsePseudoinverse(KSP, PetscBool);

PETSC_EXTERN PetscErrorCode KSPSetFromOptions(KSP);
PETSC_EXTERN PetscErrorCode KSPResetFromOptions(KSP);
PETSC_EXTERN PetscErrorCode KSPAddOptionsChecker(PetscErrorCode (*)(KSP));

PETSC_EXTERN PetscErrorCode KSPMonitorSetFromOptions(KSP, const char[], const char[], void *);
PETSC_EXTERN PetscErrorCode KSPMonitorLGCreate(MPI_Comm, const char[], const char[], const char[], PetscInt, const char *[], int, int, int, int, PetscDrawLG *);
PETSC_EXTERN PetscErrorCode KSPMonitorResidual(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorResidualDraw(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorResidualDrawLG(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorResidualDrawLGCreate(PetscViewer, PetscViewerFormat, void *, PetscViewerAndFormat **);
PETSC_EXTERN PetscErrorCode KSPMonitorResidualShort(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorResidualRange(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorTrueResidual(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorTrueResidualDraw(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorTrueResidualDrawLG(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorTrueResidualDrawLGCreate(PetscViewer, PetscViewerFormat, void *, PetscViewerAndFormat **);
PETSC_EXTERN PetscErrorCode KSPMonitorTrueResidualMax(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorError(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorErrorDraw(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorErrorDrawLG(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorErrorDrawLGCreate(PetscViewer, PetscViewerFormat, void *, PetscViewerAndFormat **);
PETSC_EXTERN PetscErrorCode KSPMonitorSolution(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorSolutionDraw(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorSolutionDrawLG(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorSolutionDrawLGCreate(PetscViewer, PetscViewerFormat, void *, PetscViewerAndFormat **);
PETSC_EXTERN PetscErrorCode KSPMonitorSingularValue(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPMonitorSingularValueCreate(PetscViewer, PetscViewerFormat, void *, PetscViewerAndFormat **);
PETSC_DEPRECATED_FUNCTION("Use KSPMonitorResidual() (since version 3.15)") static inline PetscErrorCode KSPMonitorDefault(KSP ksp, PetscInt n, PetscReal rnorm, PetscViewerAndFormat *vf)
{
  return KSPMonitorResidual(ksp, n, rnorm, vf);
}
PETSC_DEPRECATED_FUNCTION("Use KSPMonitorTrueResidual() (since version 3.15)") static inline PetscErrorCode KSPMonitorTrueResidualNorm(KSP ksp, PetscInt n, PetscReal rnorm, PetscViewerAndFormat *vf)
{
  return KSPMonitorTrueResidual(ksp, n, rnorm, vf);
}
PETSC_DEPRECATED_FUNCTION("Use KSPMonitorTrueResidualMax() (since version 3.15)") static inline PetscErrorCode KSPMonitorTrueResidualMaxNorm(KSP ksp, PetscInt n, PetscReal rnorm, PetscViewerAndFormat *vf)
{
  return KSPMonitorTrueResidualMax(ksp, n, rnorm, vf);
}

PETSC_EXTERN PetscErrorCode KSPGMRESMonitorKrylov(KSP, PetscInt, PetscReal, void *);
PETSC_EXTERN PetscErrorCode KSPMonitorDynamicTolerance(KSP, PetscInt, PetscReal, void *);
PETSC_EXTERN PetscErrorCode KSPMonitorDynamicToleranceDestroy(void **);
PETSC_EXTERN PetscErrorCode KSPMonitorDynamicToleranceCreate(void *);
PETSC_EXTERN PetscErrorCode KSPMonitorDynamicToleranceSetCoefficient(void *, PetscReal);
PETSC_EXTERN PetscErrorCode KSPMonitorSAWs(KSP, PetscInt, PetscReal, void *);
PETSC_EXTERN PetscErrorCode KSPMonitorSAWsCreate(KSP, void **);
PETSC_EXTERN PetscErrorCode KSPMonitorSAWsDestroy(void **);

PETSC_EXTERN PetscErrorCode KSPUnwindPreconditioner(KSP, Vec, Vec);
PETSC_EXTERN PetscErrorCode KSPInitialResidual(KSP, Vec, Vec, Vec, Vec, Vec);

PETSC_EXTERN PetscErrorCode KSPSetOperators(KSP, Mat, Mat);
PETSC_EXTERN PetscErrorCode KSPGetOperators(KSP, Mat *, Mat *);
PETSC_EXTERN PetscErrorCode KSPGetOperatorsSet(KSP, PetscBool *, PetscBool *);
PETSC_EXTERN PetscErrorCode KSPSetOptionsPrefix(KSP, const char[]);
PETSC_EXTERN PetscErrorCode KSPAppendOptionsPrefix(KSP, const char[]);
PETSC_EXTERN PetscErrorCode KSPGetOptionsPrefix(KSP, const char *[]);

PETSC_EXTERN PetscErrorCode KSPSetDiagonalScale(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetDiagonalScale(KSP, PetscBool *);
PETSC_EXTERN PetscErrorCode KSPSetDiagonalScaleFix(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetDiagonalScaleFix(KSP, PetscBool *);

PETSC_EXTERN PetscErrorCode KSPView(KSP, PetscViewer);
PETSC_EXTERN PetscErrorCode KSPLoad(KSP, PetscViewer);
PETSC_EXTERN PetscErrorCode KSPViewFromOptions(KSP, PetscObject, const char[]);
PETSC_EXTERN PetscErrorCode KSPConvergedReasonView(KSP, PetscViewer);
PETSC_EXTERN PetscErrorCode KSPConvergedReasonViewSet(KSP, PetscErrorCode (*)(KSP, void *), void *vctx, PetscErrorCode (*)(void **));
PETSC_EXTERN PetscErrorCode KSPConvergedReasonViewFromOptions(KSP);
PETSC_EXTERN PetscErrorCode KSPConvergedReasonViewCancel(KSP);
PETSC_EXTERN PetscErrorCode KSPConvergedRateView(KSP, PetscViewer);

PETSC_DEPRECATED_FUNCTION("Use KSPConvergedReasonView() (since version 3.14)") static inline PetscErrorCode KSPReasonView(KSP ksp, PetscViewer v)
{
  return KSPConvergedReasonView(ksp, v);
}
PETSC_DEPRECATED_FUNCTION("Use KSPConvergedReasonViewFromOptions() (since version 3.14)") static inline PetscErrorCode KSPReasonViewFromOptions(KSP ksp)
{
  return KSPConvergedReasonViewFromOptions(ksp);
}

#define KSP_FILE_CLASSID 1211223

PETSC_EXTERN PetscErrorCode KSPLSQRSetExactMatNorm(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPLSQRSetComputeStandardErrorVec(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPLSQRGetStandardErrorVec(KSP, Vec *);
PETSC_EXTERN PetscErrorCode KSPLSQRGetNorms(KSP, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode KSPLSQRMonitorResidual(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPLSQRMonitorResidualDrawLG(KSP, PetscInt, PetscReal, PetscViewerAndFormat *);
PETSC_EXTERN PetscErrorCode KSPLSQRMonitorResidualDrawLGCreate(PetscViewer, PetscViewerFormat, void *, PetscViewerAndFormat **);

PETSC_EXTERN PetscErrorCode PCRedundantGetKSP(PC, KSP *);
PETSC_EXTERN PetscErrorCode PCRedistributeGetKSP(PC, KSP *);
PETSC_EXTERN PetscErrorCode PCTelescopeGetKSP(PC, KSP *);

/*E
    KSPNormType - Norm calculated by the `KSP` and passed in the Krylov convergence
       test routines.

    Values:
+   `KSP_NORM_DEFAULT` - use the default for the current `KSPType`
.   `KSP_NORM_NONE` - use no norm calculation
.   `KSP_NORM_PRECONDITIONED` - use the preconditioned residual norm
.   `KSP_NORM_UNPRECONDITIONED` - use the unpreconditioned residual norm
-   `KSP_NORM_NATURAL` - use the natural norm (the norm induced by the linear operator)

   Level: advanced

   Note:
   Each solver only supports a subset of these and some may support different ones
   depending on whether left or right preconditioning is used, see `KSPSetPCSide()`

   Developer Note:
   This must match the values in petsc/finclude/petscksp.h

.seealso: [](chapter_ksp), `KSP`, `PCSide`, `KSPSolve()`, `KSPGetConvergedReason()`, `KSPSetNormType()`,
          `KSPSetConvergenceTest()`, `KSPSetPCSide()`
E*/
typedef enum {
  KSP_NORM_DEFAULT          = -1,
  KSP_NORM_NONE             = 0,
  KSP_NORM_PRECONDITIONED   = 1,
  KSP_NORM_UNPRECONDITIONED = 2,
  KSP_NORM_NATURAL          = 3
} KSPNormType;
#define KSP_NORM_MAX (KSP_NORM_NATURAL + 1)
PETSC_EXTERN const char *const *const KSPNormTypes;

/*MC
    KSP_NORM_NONE - Do not compute a norm during the Krylov process. This will
          possibly save some computation but means the convergence test cannot
          be based on a norm of a residual etc.

   Level: advanced

    Note:
    Some Krylov methods need to compute a residual norm (such as `KPSGMRES`) and then this option is ignored

.seealso: [](chapter_ksp), `KSPNormType`, `KSP`, `KSPSetNormType()`, `KSP_NORM_PRECONDITIONED`, `KSP_NORM_UNPRECONDITIONED`, `KSP_NORM_NATURAL`
M*/

/*MC
    KSP_NORM_PRECONDITIONED - Compute the norm of the preconditioned residual B*(b - A*x), if left preconditioning, and pass that to the
       convergence test routine.

   Level: advanced

.seealso: [](chapter_ksp), `KSPNormType`, `KSP`, `KSPSetNormType()`, `KSP_NORM_NONE`, `KSP_NORM_UNPRECONDITIONED`, `KSP_NORM_NATURAL`, `KSPSetConvergenceTest()`
M*/

/*MC
    KSP_NORM_UNPRECONDITIONED - Compute the norm of the true residual (b - A*x) and pass that to the
       convergence test routine.

   Level: advanced

.seealso: [](chapter_ksp), `KSPNormType`, `KSP`, `KSPSetNormType()`, `KSP_NORM_NONE`, `KSP_NORM_PRECONDITIONED`, `KSP_NORM_NATURAL`, `KSPSetConvergenceTest()`
M*/

/*MC
    KSP_NORM_NATURAL - Compute the 'natural norm' of residual sqrt((b - A*x)*B*(b - A*x)) and pass that to the
       convergence test routine. This is only supported by  `KSPCG`, `KSPCR`, `KSPCGNE`, `KSPCGS`, `KSPFCG`, `KSPPIPEFCG`, `KSPPIPEGCR`

   Level: advanced

.seealso: [](chapter_ksp), `KSPNormType`, `KSP`, `KSPSetNormType()`, `KSP_NORM_NONE`, `KSP_NORM_PRECONDITIONED`, `KSP_NORM_UNPRECONDITIONED`, `KSPSetConvergenceTest()`
M*/

PETSC_EXTERN PetscErrorCode KSPSetNormType(KSP, KSPNormType);
PETSC_EXTERN PetscErrorCode KSPGetNormType(KSP, KSPNormType *);
PETSC_EXTERN PetscErrorCode KSPSetSupportedNorm(KSP ksp, KSPNormType, PCSide, PetscInt);
PETSC_EXTERN PetscErrorCode KSPSetCheckNormIteration(KSP, PetscInt);
PETSC_EXTERN PetscErrorCode KSPSetLagNorm(KSP, PetscBool);

#define KSP_CONVERGED_CG_NEG_CURVE_DEPRECATED   KSP_CONVERGED_CG_NEG_CURVE PETSC_DEPRECATED_ENUM("Use KSP_CONVERGED_NEG_CURVE (since version 3.19)")
#define KSP_CONVERGED_CG_CONSTRAINED_DEPRECATED KSP_CONVERGED_CG_CONSTRAINED PETSC_DEPRECATED_ENUM("Use KSP_CONVERGED_STEP_LENGTH (since version 3.19)")
#define KSP_DIVERGED_PCSETUP_FAILED_DEPRECATED  KSP_DIVERGED_PCSETUP_FAILED PETSC_DEPRECATED_ENUM("Use KSP_DIVERGED_PC_FAILED (since version 3.11)")
/*E
    KSPConvergedReason - reason a Krylov method was determined to have converged or diverged

   Values:
+  `KSP_CONVERGED_RTOL_NORMAL` - requested decrease in the residual for the normal equations
.  `KSP_CONVERGED_ATOL_NORMAL` - requested absolute value in the residual for the normal equations
.  `KSP_CONVERGED_RTOL` - requested decrease in the residual
.  `KSP_CONVERGED_ATOL` - requested absolute value in the residual
.  `KSP_CONVERGED_ITS` - requested number of iterations
.  `KSP_CONVERGED_NEG_CURVE` - see note below
.  `KSP_CONVERGED_STEP_LENGTH` - see note below
.  `KSP_CONVERGED_HAPPY_BREAKDOWN` - happy breakdown (meaning early convergence of the `KSPType` occurred.
.  `KSP_DIVERGED_NULL` - breakdown when solving the Hessenberg system within GMRES
.  `KSP_DIVERGED_ITS` - requested number of iterations
.  `KSP_DIVERGED_DTOL` - large increase in the residual norm
.  `KSP_DIVERGED_BREAKDOWN` - breakdown in the Krylov method
.  `KSP_DIVERGED_BREAKDOWN_BICG` - breakdown in the `KSPBGCS` Krylov method
.  `KSP_DIVERGED_NONSYMMETRIC` - the operator or preonditioner was not symmetric for a `KSPType` that requires symmetry
.  `KSP_DIVERGED_INDEFINITE_PC` - the preconditioner was indefinite for a `KSPType` that requires it be definite
.  `KSP_DIVERGED_NANORINF` - a not a number of infinity was detected in a vector during the computation
.  `KSP_DIVERGED_INDEFINITE_MAT` - the operator was indefinite for a `KSPType` that requires it be definite
-  `KSP_DIVERGED_PC_FAILED` - the action of the preconditioner failed for some reason

   Level: beginner

   Note:
   The values  `KSP_CONVERGED_NEG_CURVE`, and `KSP_CONVERGED_STEP_LENGTH` are returned only by the special `KSPNASH`,
   `KSPSTCG`, and `KSPGLTR` solvers which are used by the `SNESNEWTONTR` (trust region) solver.

   Developer Notes:
   This must match the values in petsc/finclude/petscksp.h

   The string versions of these are `KSPConvergedReasons`; if you change
   any of the values here also change them that array of names.

.seealso: [](chapter_ksp), `KSP`, `KSPSolve()`, `KSPGetConvergedReason()`, `KSPSetTolerances()`, `KSPConvergedReasonView()`
E*/
typedef enum { /* converged */
  KSP_CONVERGED_RTOL_NORMAL               = 1,
  KSP_CONVERGED_ATOL_NORMAL               = 9,
  KSP_CONVERGED_RTOL                      = 2,
  KSP_CONVERGED_ATOL                      = 3,
  KSP_CONVERGED_ITS                       = 4,
  KSP_CONVERGED_NEG_CURVE                 = 5,
  KSP_CONVERGED_CG_NEG_CURVE_DEPRECATED   = 5,
  KSP_CONVERGED_CG_CONSTRAINED_DEPRECATED = 6,
  KSP_CONVERGED_STEP_LENGTH               = 6,
  KSP_CONVERGED_HAPPY_BREAKDOWN           = 7,
  /* diverged */
  KSP_DIVERGED_NULL                      = -2,
  KSP_DIVERGED_ITS                       = -3,
  KSP_DIVERGED_DTOL                      = -4,
  KSP_DIVERGED_BREAKDOWN                 = -5,
  KSP_DIVERGED_BREAKDOWN_BICG            = -6,
  KSP_DIVERGED_NONSYMMETRIC              = -7,
  KSP_DIVERGED_INDEFINITE_PC             = -8,
  KSP_DIVERGED_NANORINF                  = -9,
  KSP_DIVERGED_INDEFINITE_MAT            = -10,
  KSP_DIVERGED_PC_FAILED                 = -11,
  KSP_DIVERGED_PCSETUP_FAILED_DEPRECATED = -11,

  KSP_CONVERGED_ITERATING = 0
} KSPConvergedReason;
PETSC_EXTERN const char *const *KSPConvergedReasons;

/*MC
     KSP_CONVERGED_RTOL - norm(r) <= rtol*norm(b) or rtol*norm(b - A*x_0) if `KSPConvergedDefaultSetUIRNorm()` was called

   Level: beginner

   See `KSPNormType` and `KSPSetNormType()` for possible norms that may be used. By default
   for left preconditioning it is the 2-norm of the preconditioned residual, and the
   2-norm of the residual for right preconditioning

   See also `KSP_CONVERGED_ATOL` which may apply before this tolerance.

.seealso: [](chapter_ksp), `KSPNormType`, `KSP_CONVERGED_ATOL`, `KSP_DIVERGED_DTOL`, `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_CONVERGED_ATOL - norm(r) <= atol

   Level: beginner

   See `KSPNormType` and `KSPSetNormType()` for possible norms that may be used. By default
       for left preconditioning it is the 2-norm of the preconditioned residual, and the
       2-norm of the residual for right preconditioning

   See also `KSP_CONVERGED_RTOL` which may apply before this tolerance.

.seealso: [](chapter_ksp), `KSPNormType`, `KSP_CONVERGED_RTOL`, `KSP_DIVERGED_DTOL`, `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_DIVERGED_DTOL - norm(r) >= dtol*norm(b)

   Level: beginner

   See `KSPNormType` and `KSPSetNormType()` for possible norms that may be used. By default
       for left preconditioning it is the 2-norm of the preconditioned residual, and the
       2-norm of the residual for right preconditioning

   Level: beginner

.seealso: [](chapter_ksp), `KSPNormType`, `KSP_CONVERGED_ATOL`, `KSP_DIVERGED_RTOL`, `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_DIVERGED_ITS - Ran out of iterations before any convergence criteria was
      reached

   Level: beginner

.seealso: [](chapter_ksp), `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_CONVERGED_ITS - Used by the `KSPPREONLY` solver after the single iteration of
           the preconditioner is applied. Also used when the `KSPConvergedSkip()` convergence
           test routine is set in KSP.

   Level: beginner

.seealso: [](chapter_ksp), `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_DIVERGED_BREAKDOWN - A breakdown in the Krylov method was detected so the
          method could not continue to enlarge the Krylov space. Could be due to a singular matrix or
          preconditioner. In `KSPHPDDM`, this is also returned when some search directions within a block
          are colinear.

   Level: beginner

.seealso: [](chapter_ksp), `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_DIVERGED_BREAKDOWN_BICG - A breakdown in the `KSPBICG` method was detected so the
          method could not continue to enlarge the Krylov space.

   Level: beginner

.seealso: [](chapter_ksp), `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_DIVERGED_NONSYMMETRIC - It appears the operator or preconditioner is not
        symmetric and this Krylov method (`KSPCG`, `KSPMINRES`, `KSPCR`) requires symmetry

   Level: beginner

.seealso: [](chapter_ksp), `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_DIVERGED_INDEFINITE_PC - It appears the preconditioner is indefinite (has both
        positive and negative eigenvalues) and this Krylov method (`KSPCG`) requires it to
        be positive definite

   Level: beginner

     Note:
    This can happen with the `PCICC` preconditioner, use the options database option `-pc_factor_shift_positive_definite` to force
  the `PCICC` preconditioner to generate a positive definite preconditioner

.seealso: [](chapter_ksp), `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_DIVERGED_PC_FAILED - It was not possible to build or use the requested preconditioner. This is usually due to a
     zero pivot in a factorization. It can also result from a failure in a subpreconditioner inside a nested preconditioner
     such as `PCFIELDSPLIT`.

   Level: beginner

    Note:
    Run with `-ksp_error_if_not_converged` to stop the program when the error is detected and print an error message with details.

.seealso: [](chapter_ksp), `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

/*MC
     KSP_CONVERGED_ITERATING - This flag is returned if `KSPGetConvergedReason()` is called
        while `KSPSolve()` is still running.

   Level: beginner

.seealso: [](chapter_ksp), `KSPSolve()`, `KSPGetConvergedReason()`, `KSPConvergedReason`, `KSPSetTolerances()`
M*/

PETSC_EXTERN PetscErrorCode KSPSetConvergenceTest(KSP, PetscErrorCode (*)(KSP, PetscInt, PetscReal, KSPConvergedReason *, void *), void *, PetscErrorCode (*)(void *));
PETSC_EXTERN PetscErrorCode KSPGetConvergenceTest(KSP, PetscErrorCode (**)(KSP, PetscInt, PetscReal, KSPConvergedReason *, void *), void **, PetscErrorCode (**)(void *));
PETSC_EXTERN PetscErrorCode KSPGetAndClearConvergenceTest(KSP, PetscErrorCode (**)(KSP, PetscInt, PetscReal, KSPConvergedReason *, void *), void **, PetscErrorCode (**)(void *));
PETSC_EXTERN PetscErrorCode KSPGetConvergenceContext(KSP, void *);
PETSC_EXTERN PetscErrorCode KSPConvergedDefault(KSP, PetscInt, PetscReal, KSPConvergedReason *, void *);
PETSC_EXTERN PetscErrorCode KSPLSQRConvergedDefault(KSP, PetscInt, PetscReal, KSPConvergedReason *, void *);
PETSC_EXTERN PetscErrorCode KSPConvergedDefaultDestroy(void *);
PETSC_EXTERN PetscErrorCode KSPConvergedDefaultCreate(void **);
PETSC_EXTERN PetscErrorCode KSPConvergedDefaultSetUIRNorm(KSP);
PETSC_EXTERN PetscErrorCode KSPConvergedDefaultSetUMIRNorm(KSP);
PETSC_EXTERN PetscErrorCode KSPConvergedDefaultSetConvergedMaxits(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPConvergedSkip(KSP, PetscInt, PetscReal, KSPConvergedReason *, void *);
PETSC_EXTERN PetscErrorCode KSPGetConvergedReason(KSP, KSPConvergedReason *);
PETSC_EXTERN PetscErrorCode KSPGetConvergedReasonString(KSP, const char **);
PETSC_EXTERN PetscErrorCode KSPComputeConvergenceRate(KSP, PetscReal *, PetscReal *, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode KSPSetConvergedNegativeCurvature(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetConvergedNegativeCurvature(KSP, PetscBool *);

PETSC_DEPRECATED_FUNCTION("Use KSPConvergedDefault() (since version 3.5)") static inline void KSPDefaultConverged(void)
{ /* never called */
}
#define KSPDefaultConverged (KSPDefaultConverged, KSPConvergedDefault)
PETSC_DEPRECATED_FUNCTION("Use KSPConvergedDefaultDestroy() (since version 3.5)") static inline void KSPDefaultConvergedDestroy(void)
{ /* never called */
}
#define KSPDefaultConvergedDestroy (KSPDefaultConvergedDestroy, KSPConvergedDefaultDestroy)
PETSC_DEPRECATED_FUNCTION("Use KSPConvergedDefaultCreate() (since version 3.5)") static inline void KSPDefaultConvergedCreate(void)
{ /* never called */
}
#define KSPDefaultConvergedCreate (KSPDefaultConvergedCreate, KSPConvergedDefaultCreate)
PETSC_DEPRECATED_FUNCTION("Use KSPConvergedDefaultSetUIRNorm() (since version 3.5)") static inline void KSPDefaultConvergedSetUIRNorm(void)
{ /* never called */
}
#define KSPDefaultConvergedSetUIRNorm (KSPDefaultConvergedSetUIRNorm, KSPConvergedDefaultSetUIRNorm)
PETSC_DEPRECATED_FUNCTION("Use KSPConvergedDefaultSetUMIRNorm() (since version 3.5)") static inline void KSPDefaultConvergedSetUMIRNorm(void)
{ /* never called */
}
#define KSPDefaultConvergedSetUMIRNorm (KSPDefaultConvergedSetUMIRNorm, KSPConvergedDefaultSetUMIRNorm)
PETSC_DEPRECATED_FUNCTION("Use KSPConvergedSkip() (since version 3.5)") static inline void KSPSkipConverged(void)
{ /* never called */
}
#define KSPSkipConverged (KSPSkipConverged, KSPConvergedSkip)

PETSC_EXTERN PetscErrorCode KSPComputeOperator(KSP, MatType, Mat *);
PETSC_DEPRECATED_FUNCTION("Use KSPComputeOperator() (since version 3.12)") static inline PetscErrorCode KSPComputeExplicitOperator(KSP A, Mat *B)
{
  return KSPComputeOperator(A, PETSC_NULLPTR, B);
}

/*E
    KSPCGType - Determines what type of `KSPCG` to use

   Values:
 + `KSP_CG_SYMMETRIC` - the matrix is complex symmetric
 - `KSP_CG_HERMITIAN` - the matrix is complex Hermitian

   Level: beginner

.seealso: [](chapter_ksp), `KSPCG`, `KSP`, `KSPCGSetType()`
E*/
typedef enum {
  KSP_CG_SYMMETRIC = 0,
  KSP_CG_HERMITIAN = 1
} KSPCGType;
PETSC_EXTERN const char *const KSPCGTypes[];

PETSC_EXTERN PetscErrorCode KSPCGSetType(KSP, KSPCGType);
PETSC_EXTERN PetscErrorCode KSPCGUseSingleReduction(KSP, PetscBool);

PETSC_EXTERN PetscErrorCode KSPCGSetRadius(KSP, PetscReal);
PETSC_EXTERN PetscErrorCode KSPCGSetObjectiveTarget(KSP, PetscReal);
PETSC_EXTERN PetscErrorCode KSPCGGetNormD(KSP, PetscReal *);
PETSC_EXTERN PetscErrorCode KSPCGGetObjFcn(KSP, PetscReal *);

PETSC_EXTERN PetscErrorCode KSPGLTRGetMinEig(KSP, PetscReal *);
PETSC_EXTERN PetscErrorCode KSPGLTRGetLambda(KSP, PetscReal *);
PETSC_DEPRECATED_FUNCTION("Use KSPGLTRGetMinEig (since v3.12)") static inline PetscErrorCode KSPCGGLTRGetMinEig(KSP ksp, PetscReal *x)
{
  return KSPGLTRGetMinEig(ksp, x);
}
PETSC_DEPRECATED_FUNCTION("Use KSPGLTRGetLambda (since v3.12)") static inline PetscErrorCode KSPCGGLTRGetLambda(KSP ksp, PetscReal *x)
{
  return KSPGLTRGetLambda(ksp, x);
}

PETSC_EXTERN PetscErrorCode KSPPythonSetType(KSP, const char[]);
PETSC_EXTERN PetscErrorCode KSPPythonGetType(KSP, const char *[]);

PETSC_EXTERN PetscErrorCode PCSetPreSolve(PC, PetscErrorCode (*)(PC, KSP));
PETSC_EXTERN PetscErrorCode PCPreSolve(PC, KSP);
PETSC_EXTERN PetscErrorCode PCPostSolve(PC, KSP);

#include <petscdrawtypes.h>
PETSC_EXTERN PetscErrorCode KSPMonitorLGRange(KSP, PetscInt, PetscReal, void *);

PETSC_EXTERN PetscErrorCode PCShellSetPreSolve(PC, PetscErrorCode (*)(PC, KSP, Vec, Vec));
PETSC_EXTERN PetscErrorCode PCShellSetPostSolve(PC, PetscErrorCode (*)(PC, KSP, Vec, Vec));

/*S
     KSPGuess - Abstract PETSc object that manages all initial guess generation methods for Krylov methods.

   Level: intermediate

.seealso: [](chapter_ksp), `KSPCreate()`, `KSPSetGuessType()`, `KSPGuessType`
S*/
typedef struct _p_KSPGuess *KSPGuess;

/*J
    KSPGuessType - String with the name of a PETSc initial guess approach for Krylov methods.

   Values:
 + `KSPGUESSFISCHER` - methodology developed by Paul Fischer
 - `KSPGUESSPOD` - methodology based on proper orthogonal decomposition

   Level: intermediate

.seealso: [](chapter_ksp), `KSP`, `KSPGuess`
J*/
typedef const char *KSPGuessType;
#define KSPGUESSFISCHER "fischer"
#define KSPGUESSPOD     "pod"

PETSC_EXTERN PetscErrorCode KSPGuessRegister(const char[], PetscErrorCode (*)(KSPGuess));
PETSC_EXTERN PetscErrorCode KSPSetGuess(KSP, KSPGuess);
PETSC_EXTERN PetscErrorCode KSPGetGuess(KSP, KSPGuess *);
PETSC_EXTERN PetscErrorCode KSPGuessView(KSPGuess, PetscViewer);
PETSC_EXTERN PetscErrorCode KSPGuessDestroy(KSPGuess *);
PETSC_EXTERN PetscErrorCode KSPGuessCreate(MPI_Comm, KSPGuess *);
PETSC_EXTERN PetscErrorCode KSPGuessSetType(KSPGuess, KSPGuessType);
PETSC_EXTERN PetscErrorCode KSPGuessGetType(KSPGuess, KSPGuessType *);
PETSC_EXTERN PetscErrorCode KSPGuessSetTolerance(KSPGuess, PetscReal);
PETSC_EXTERN PetscErrorCode KSPGuessSetUp(KSPGuess);
PETSC_EXTERN PetscErrorCode KSPGuessUpdate(KSPGuess, Vec, Vec);
PETSC_EXTERN PetscErrorCode KSPGuessFormGuess(KSPGuess, Vec, Vec);
PETSC_EXTERN PetscErrorCode KSPGuessSetFromOptions(KSPGuess);
PETSC_EXTERN PetscErrorCode KSPGuessFischerSetModel(KSPGuess, PetscInt, PetscInt);
PETSC_EXTERN PetscErrorCode KSPSetUseFischerGuess(KSP, PetscInt, PetscInt);
PETSC_EXTERN PetscErrorCode KSPSetInitialGuessKnoll(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetInitialGuessKnoll(KSP, PetscBool *);

/*E
    MatSchurComplementAinvType - Determines how to approximate the inverse of the (0,0) block in Schur complement preconditioning matrix assembly routines

    Level: intermediate

.seealso: `MatSchurComplementGetAinvType()`, `MatSchurComplementSetAinvType()`, `MatSchurComplementGetPmat()`, `MatGetSchurComplement()`,
          `MatCreateSchurComplementPmat()`
E*/
typedef enum {
  MAT_SCHUR_COMPLEMENT_AINV_DIAG,
  MAT_SCHUR_COMPLEMENT_AINV_LUMP,
  MAT_SCHUR_COMPLEMENT_AINV_BLOCK_DIAG,
  MAT_SCHUR_COMPLEMENT_AINV_FULL
} MatSchurComplementAinvType;
PETSC_EXTERN const char *const MatSchurComplementAinvTypes[];

typedef enum {
  MAT_LMVM_SYMBROYDEN_SCALE_NONE     = 0,
  MAT_LMVM_SYMBROYDEN_SCALE_SCALAR   = 1,
  MAT_LMVM_SYMBROYDEN_SCALE_DIAGONAL = 2,
  MAT_LMVM_SYMBROYDEN_SCALE_USER     = 3
} MatLMVMSymBroydenScaleType;
PETSC_EXTERN const char *const MatLMVMSymBroydenScaleTypes[];

PETSC_EXTERN PetscErrorCode MatCreateSchurComplement(Mat, Mat, Mat, Mat, Mat, Mat *);
PETSC_EXTERN PetscErrorCode MatSchurComplementGetKSP(Mat, KSP *);
PETSC_EXTERN PetscErrorCode MatSchurComplementSetKSP(Mat, KSP);
PETSC_EXTERN PetscErrorCode MatSchurComplementSetSubMatrices(Mat, Mat, Mat, Mat, Mat, Mat);
PETSC_EXTERN PetscErrorCode MatSchurComplementUpdateSubMatrices(Mat, Mat, Mat, Mat, Mat, Mat);
PETSC_EXTERN PetscErrorCode MatSchurComplementGetSubMatrices(Mat, Mat *, Mat *, Mat *, Mat *, Mat *);
PETSC_EXTERN PetscErrorCode MatSchurComplementSetAinvType(Mat, MatSchurComplementAinvType);
PETSC_EXTERN PetscErrorCode MatSchurComplementGetAinvType(Mat, MatSchurComplementAinvType *);
PETSC_EXTERN PetscErrorCode MatSchurComplementGetPmat(Mat, MatReuse, Mat *);
PETSC_EXTERN PetscErrorCode MatSchurComplementComputeExplicitOperator(Mat, Mat *);
PETSC_EXTERN PetscErrorCode MatGetSchurComplement(Mat, IS, IS, IS, IS, MatReuse, Mat *, MatSchurComplementAinvType, MatReuse, Mat *);
PETSC_EXTERN PetscErrorCode MatCreateSchurComplementPmat(Mat, Mat, Mat, Mat, MatSchurComplementAinvType, MatReuse, Mat *);

PETSC_EXTERN PetscErrorCode MatCreateLMVMDFP(MPI_Comm, PetscInt, PetscInt, Mat *);
PETSC_EXTERN PetscErrorCode MatCreateLMVMBFGS(MPI_Comm, PetscInt, PetscInt, Mat *);
PETSC_EXTERN PetscErrorCode MatCreateLMVMSR1(MPI_Comm, PetscInt, PetscInt, Mat *);
PETSC_EXTERN PetscErrorCode MatCreateLMVMBroyden(MPI_Comm, PetscInt, PetscInt, Mat *);
PETSC_EXTERN PetscErrorCode MatCreateLMVMBadBroyden(MPI_Comm, PetscInt, PetscInt, Mat *);
PETSC_EXTERN PetscErrorCode MatCreateLMVMSymBroyden(MPI_Comm, PetscInt, PetscInt, Mat *);
PETSC_EXTERN PetscErrorCode MatCreateLMVMSymBadBroyden(MPI_Comm, PetscInt, PetscInt, Mat *);
PETSC_EXTERN PetscErrorCode MatCreateLMVMDiagBroyden(MPI_Comm, PetscInt, PetscInt, Mat *);

PETSC_EXTERN PetscErrorCode MatLMVMUpdate(Mat, Vec, Vec);
PETSC_EXTERN PetscErrorCode MatLMVMIsAllocated(Mat, PetscBool *);
PETSC_EXTERN PetscErrorCode MatLMVMAllocate(Mat, Vec, Vec);
PETSC_EXTERN PetscErrorCode MatLMVMReset(Mat, PetscBool);
PETSC_EXTERN PetscErrorCode MatLMVMResetShift(Mat);
PETSC_EXTERN PetscErrorCode MatLMVMClearJ0(Mat);
PETSC_EXTERN PetscErrorCode MatLMVMSetJ0(Mat, Mat);
PETSC_EXTERN PetscErrorCode MatLMVMSetJ0Scale(Mat, PetscReal);
PETSC_EXTERN PetscErrorCode MatLMVMSetJ0Diag(Mat, Vec);
PETSC_EXTERN PetscErrorCode MatLMVMSetJ0PC(Mat, PC);
PETSC_EXTERN PetscErrorCode MatLMVMSetJ0KSP(Mat, KSP);
PETSC_EXTERN PetscErrorCode MatLMVMApplyJ0Fwd(Mat, Vec, Vec);
PETSC_EXTERN PetscErrorCode MatLMVMApplyJ0Inv(Mat, Vec, Vec);
PETSC_EXTERN PetscErrorCode MatLMVMGetJ0(Mat, Mat *);
PETSC_EXTERN PetscErrorCode MatLMVMGetJ0PC(Mat, PC *);
PETSC_EXTERN PetscErrorCode MatLMVMGetJ0KSP(Mat, KSP *);
PETSC_EXTERN PetscErrorCode MatLMVMSetHistorySize(Mat, PetscInt);
PETSC_EXTERN PetscErrorCode MatLMVMGetUpdateCount(Mat, PetscInt *);
PETSC_EXTERN PetscErrorCode MatLMVMGetRejectCount(Mat, PetscInt *);
PETSC_EXTERN PetscErrorCode MatLMVMSymBroydenSetDelta(Mat, PetscScalar);
PETSC_EXTERN PetscErrorCode MatLMVMSymBroydenSetScaleType(Mat, MatLMVMSymBroydenScaleType);

PETSC_EXTERN PetscErrorCode KSPSetDM(KSP, DM);
PETSC_EXTERN PetscErrorCode KSPSetDMActive(KSP, PetscBool);
PETSC_EXTERN PetscErrorCode KSPGetDM(KSP, DM *);
PETSC_EXTERN PetscErrorCode KSPSetApplicationContext(KSP, void *);
PETSC_EXTERN PetscErrorCode KSPGetApplicationContext(KSP, void *);
PETSC_EXTERN PetscErrorCode KSPSetComputeRHS(KSP, PetscErrorCode (*func)(KSP, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode KSPSetComputeOperators(KSP, PetscErrorCode (*)(KSP, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode KSPSetComputeInitialGuess(KSP, PetscErrorCode (*)(KSP, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode DMKSPSetComputeOperators(DM, PetscErrorCode (*)(KSP, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode DMKSPGetComputeOperators(DM, PetscErrorCode (**)(KSP, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode DMKSPSetComputeRHS(DM, PetscErrorCode (*)(KSP, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode DMKSPGetComputeRHS(DM, PetscErrorCode (**)(KSP, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode DMKSPSetComputeInitialGuess(DM, PetscErrorCode (*)(KSP, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode DMKSPGetComputeInitialGuess(DM, PetscErrorCode (**)(KSP, Vec, void *), void *);

PETSC_EXTERN PetscErrorCode DMGlobalToLocalSolve(DM, Vec, Vec);
PETSC_EXTERN PetscErrorCode DMProjectField(DM, PetscReal, Vec, void (**)(PetscInt, PetscInt, PetscInt, const PetscInt[], const PetscInt[], const PetscScalar[], const PetscScalar[], const PetscScalar[], const PetscInt[], const PetscInt[], const PetscScalar[], const PetscScalar[], const PetscScalar[], PetscReal, const PetscReal[], PetscInt, const PetscScalar[], PetscScalar[]), InsertMode, Vec);

PETSC_EXTERN PetscErrorCode DMAdaptInterpolator(DM, DM, Mat, KSP, Mat, Mat, Mat *, void *);
PETSC_EXTERN PetscErrorCode DMCheckInterpolator(DM, Mat, Mat, Mat, PetscReal);
#endif

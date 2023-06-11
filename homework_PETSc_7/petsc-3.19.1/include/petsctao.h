#ifndef PETSCTAO_H
#define PETSCTAO_H

#include <petscsnes.h>

/* SUBMANSEC = Tao */

PETSC_EXTERN PetscErrorCode VecFischer(Vec, Vec, Vec, Vec, Vec);
PETSC_EXTERN PetscErrorCode VecSFischer(Vec, Vec, Vec, Vec, PetscReal, Vec);
PETSC_EXTERN PetscErrorCode MatDFischer(Mat, Vec, Vec, Vec, Vec, Vec, Vec, Vec, Vec);
PETSC_EXTERN PetscErrorCode MatDSFischer(Mat, Vec, Vec, Vec, Vec, PetscReal, Vec, Vec, Vec, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoSoftThreshold(Vec, PetscReal, PetscReal, Vec);

/*E
  TaoSubsetType - Type representing the way TAO handles active sets

  Values:
+ `TAO_SUBSET_SUBVEC` - Tao uses `MatCreateSubMatrix()` and `VecGetSubVector()`
. `TAO_SUBSET_MASK` - Matrices are zeroed out corresponding to active set entries
- `TAO_SUBSET_MATRIXFREE` - Same as `TAO_SUBSET_MASK` but it can be applied to matrix-free operators

  Options database Key:
. -different_hessian - Tao will use a copy of the Hessian operator for masking.  By default TAO will directly alter the Hessian operator.

  Level: intermediate

.seealso: [](chapter_tao), `TaoVecGetSubVec()`, `TaoMatGetSubMat()`, `Tao`, `TaoCreate()`, `TaoDestroy()`, `TaoSetType()`, `TaoType`
E*/
typedef enum {
  TAO_SUBSET_SUBVEC,
  TAO_SUBSET_MASK,
  TAO_SUBSET_MATRIXFREE
} TaoSubsetType;
PETSC_EXTERN const char *const TaoSubsetTypes[];

/*S
     Tao - Abstract PETSc object that manages nonlinear optimization solves

   Level: advanced

.seealso: [](doc_taosolve), [](chapter_tao), `TaoCreate()`, `TaoDestroy()`, `TaoSetType()`, `TaoType`
S*/
typedef struct _p_Tao *Tao;

/*E
     TaoADMMUpdateType - Determine spectral penalty update routine for Lagrange augmented term for `TAOADMM`.

  Level: advanced

.seealso: [](chapter_tao), `Tao`, `TAOADMM`, `TaoADMMSetUpdateType()`
E*/
typedef enum {
  TAO_ADMM_UPDATE_BASIC,
  TAO_ADMM_UPDATE_ADAPTIVE,
  TAO_ADMM_UPDATE_ADAPTIVE_RELAXED
} TaoADMMUpdateType;
PETSC_EXTERN const char *const TaoADMMUpdateTypes[];

/*MC
     TAO_ADMM_UPDATE_BASIC - Use same spectral penalty set at the beginning. No update

  Level: advanced

  Note:
  Most basic implementation of `TAOADMM`. Generally slower than adaptive or adaptive relaxed version.

.seealso: [](chapter_tao), `Tao`, `TAOADMM`, `TaoADMMSetUpdateType()`, `TAO_ADMM_UPDATE_ADAPTIVE`, `TAO_ADMM_UPDATE_ADAPTIVE_RELAXED`
M*/

/*MC
     TAO_ADMM_UPDATE_ADAPTIVE - Adaptively update spectral penalty

  Level: advanced

  Note:
  Adaptively updates spectral penalty of `TAOADMM` by using both steepest descent and minimum gradient.

.seealso: [](chapter_tao), `Tao`, `TAOADMM`, `TaoADMMSetUpdateType()`, `TAO_ADMM_UPDATE_BASIC`, `TAO_ADMM_UPDATE_ADAPTIVE_RELAXED`
M*/

/*MC
     ADMM_UPDATE_ADAPTIVE_RELAXED - Adaptively update spectral penalty, and relaxes parameter update

  Level: advanced

  Note:
  With adaptive spectral penalty update, it also relaxes x vector update by a factor.

.seealso: [](chapter_tao), `Tao`, `TaoADMMSetUpdateType()`, `TAO_ADMM_UPDATE_BASIC`, `TAO_ADMM_UPDATE_ADAPTIVE`
M*/

/*E
     TaoADMMRegularizerType - Determine regularizer routine - either user provided or soft threshold for `TAOADMM`

  Level: advanced

.seealso: [](chapter_tao), `Tao`, `TAOADMM`, `TaoADMMSetRegularizerType()`
E*/
typedef enum {
  TAO_ADMM_REGULARIZER_USER,
  TAO_ADMM_REGULARIZER_SOFT_THRESH
} TaoADMMRegularizerType;
PETSC_EXTERN const char *const TaoADMMRegularizerTypes[];

/*MC
  TAO_ADMM_REGULARIZER_USER - User provided routines for regularizer part of `TAOADMM`

  Level: advanced

  Note:
  User needs to provided appropriate routines and type for regularizer solver

.seealso: [](chapter_tao), `Tao`, `TAOADMM`, `TaoADMMSetRegularizerType()`, `TAO_ADMM_REGULARIZER_SOFT_THRESH`
M*/

/*MC
  TAO_ADMM_REGULARIZER_SOFT_THRESH - Soft threshold to solve regularizer part of `TAOADMM`

  Level: advanced

  Note:
  Utilizes built-in SoftThreshold routines

.seealso: [](chapter_tao), `Tao`, `TAOADMM`, `TaoSoftThreshold()`, `TaoADMMSetRegularizerObjectiveAndGradientRoutine()`,
          `TaoADMMSetRegularizerHessianRoutine()`, `TaoADMMSetRegularizerType()`, `TAO_ADMM_REGULARIZER_USER`
M*/

/*E
     TaoALMMType - Determine the augmented Lagrangian formulation used in the `TAOALMM` subproblem.

   Values:
+  `TAO_ALMM_CLASSIC` - classic augmented Lagrangian definition including slack variables for inequality constraints
-  `TAO_ALMM_PHR`     - Powell-Hestenes-Rockafellar formulation without slack variables, uses pointwise min() for inequalities

  Level: advanced

.seealso: [](chapter_tao), `Tao`, `TAOALMM`, `TaoALMMSetType()`, `TaoALMMGetType()`
E*/
typedef enum {
  TAO_ALMM_CLASSIC,
  TAO_ALMM_PHR
} TaoALMMType;
PETSC_EXTERN const char *const TaoALMMTypes[];

/*J
        TaoType - String with the name of a `Tao` method

  Values:
+    `TAONLS` - nls Newton's method with line search for unconstrained minimization
.    `TAONTR` - ntr Newton's method with trust region for unconstrained minimization
.    `TAONTL` - ntl Newton's method with trust region, line search for unconstrained minimization
.    `TAOLMVM` - lmvm Limited memory variable metric method for unconstrained minimization
.    `TAOCG` - cg Nonlinear conjugate gradient method for unconstrained minimization
.    `TAONM` - nm Nelder-Mead algorithm for derivate-free unconstrained minimization
.    `TAOTRON` - tron Newton Trust Region method for bound constrained minimization
.    `TAOGPCG` - gpcg Newton Trust Region method for quadratic bound constrained minimization
.    `TAOBLMVM` - blmvm Limited memory variable metric method for bound constrained minimization
.    `TAOLCL` - lcl Linearly constrained Lagrangian method for pde-constrained minimization
-    `TAOPOUNDERS` - Pounders Model-based algorithm for nonlinear least squares

       Level: beginner

.seealso: [](doc_taosolve), [](chapter_tao), `Tao`, `TaoCreate()`, `TaoSetType()`
J*/
typedef const char *TaoType;
#define TAOLMVM     "lmvm"
#define TAONLS      "nls"
#define TAONTR      "ntr"
#define TAONTL      "ntl"
#define TAOCG       "cg"
#define TAOTRON     "tron"
#define TAOOWLQN    "owlqn"
#define TAOBMRM     "bmrm"
#define TAOBLMVM    "blmvm"
#define TAOBQNLS    "bqnls"
#define TAOBNCG     "bncg"
#define TAOBNLS     "bnls"
#define TAOBNTR     "bntr"
#define TAOBNTL     "bntl"
#define TAOBQNKLS   "bqnkls"
#define TAOBQNKTR   "bqnktr"
#define TAOBQNKTL   "bqnktl"
#define TAOBQPIP    "bqpip"
#define TAOGPCG     "gpcg"
#define TAONM       "nm"
#define TAOPOUNDERS "pounders"
#define TAOBRGN     "brgn"
#define TAOLCL      "lcl"
#define TAOSSILS    "ssils"
#define TAOSSFLS    "ssfls"
#define TAOASILS    "asils"
#define TAOASFLS    "asfls"
#define TAOIPM      "ipm"
#define TAOPDIPM    "pdipm"
#define TAOSHELL    "shell"
#define TAOADMM     "admm"
#define TAOALMM     "almm"
#define TAOPYTHON   "python"
#define TAOSNES     "snes"

PETSC_EXTERN PetscClassId      TAO_CLASSID;
PETSC_EXTERN PetscFunctionList TaoList;

/*E
    TaoConvergedReason - reason a `Tao` optimizer was said to have converged or diverged

   Values:
+  `TAO_CONVERGED_GATOL` - ||g(X)|| < gatol
.  `TAO_CONVERGED_GRTOL` - ||g(X)|| / f(X)  < grtol
.  `TAO_CONVERGED_GTTOL` - ||g(X)|| / ||g(X0)|| < gttol
.  `TAO_CONVERGED_STEPTOL` - step size smaller than tolerance
.  `TAO_CONVERGED_MINF` - F < F_min
.  `TAO_CONVERGED_USER` - the user indicates the optimization has succeeded
.  `TAO_DIVERGED_MAXITS` - the maximum number of iterations allowed has been achieved
.  `TAO_DIVERGED_NAN` - not a number appeared in the computations
.  `TAO_DIVERGED_MAXFCN` - the maximum number of function evaluations has been computed
.  `TAO_DIVERGED_LS_FAILURE` - a linesearch failed
.  `TAO_DIVERGED_TR_REDUCTION` - trust region failure
.  `TAO_DIVERGED_USER` - the user has indicated the optimization has failed
-  `TAO_CONTINUE_ITERATING` - the optimization is still running, `TaoSolve()`

   where
+  X - current solution
.  X0 - initial guess
.  f(X) - current function value
.  f(X*) - true solution (estimated)
.  g(X) - current gradient
.  its - current iterate number
.  maxits - maximum number of iterates
.  fevals - number of function evaluations
-  max_funcsals - maximum number of function evaluations

   Level: beginner

   Note:
   The two most common reasons for divergence are  an incorrectly coded or computed gradient or Hessian failure or lack of convergence
   in the linear system solve (in this case we recommend testing with `-pc_type lu` to eliminate the linear solver as the cause of the problem).

   Developer Note:
   The names in `KSPConvergedReason`, `SNESConvergedReason`, and `TaoConvergedReason` should be uniformized

.seealso: [](chapter_tao), `Tao`, `TaoSolve()`, `TaoGetConvergedReason()`, `KSPConvergedReason`, `SNESConvergedReason`
E*/
typedef enum {               /* converged */
  TAO_CONVERGED_GATOL   = 3, /* ||g(X)|| < gatol */
  TAO_CONVERGED_GRTOL   = 4, /* ||g(X)|| / f(X)  < grtol */
  TAO_CONVERGED_GTTOL   = 5, /* ||g(X)|| / ||g(X0)|| < gttol */
  TAO_CONVERGED_STEPTOL = 6, /* step size small */
  TAO_CONVERGED_MINF    = 7, /* F < F_min */
  TAO_CONVERGED_USER    = 8, /* User defined */
  /* diverged */
  TAO_DIVERGED_MAXITS       = -2,
  TAO_DIVERGED_NAN          = -4,
  TAO_DIVERGED_MAXFCN       = -5,
  TAO_DIVERGED_LS_FAILURE   = -6,
  TAO_DIVERGED_TR_REDUCTION = -7,
  TAO_DIVERGED_USER         = -8, /* User defined */
  /* keep going */
  TAO_CONTINUE_ITERATING = 0
} TaoConvergedReason;

PETSC_EXTERN const char **TaoConvergedReasons;

PETSC_EXTERN PetscErrorCode TaoInitializePackage(void);
PETSC_EXTERN PetscErrorCode TaoFinalizePackage(void);
PETSC_EXTERN PetscErrorCode TaoCreate(MPI_Comm, Tao *);
PETSC_EXTERN PetscErrorCode TaoSetFromOptions(Tao);
PETSC_EXTERN PetscErrorCode TaoSetUp(Tao);
PETSC_EXTERN PetscErrorCode TaoSetType(Tao, TaoType);
PETSC_EXTERN PetscErrorCode TaoGetType(Tao, TaoType *);
PETSC_EXTERN PetscErrorCode TaoSetApplicationContext(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoGetApplicationContext(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoDestroy(Tao *);

PETSC_EXTERN PetscErrorCode TaoSetOptionsPrefix(Tao, const char[]);
PETSC_EXTERN PetscErrorCode TaoView(Tao, PetscViewer);
PETSC_EXTERN PetscErrorCode TaoViewFromOptions(Tao, PetscObject, const char[]);

PETSC_EXTERN PetscErrorCode TaoSolve(Tao);

PETSC_EXTERN PetscErrorCode TaoRegister(const char[], PetscErrorCode (*)(Tao));
PETSC_EXTERN PetscErrorCode TaoRegisterDestroy(void);

PETSC_EXTERN PetscErrorCode TaoGetConvergedReason(Tao, TaoConvergedReason *);
PETSC_EXTERN PetscErrorCode TaoGetSolutionStatus(Tao, PetscInt *, PetscReal *, PetscReal *, PetscReal *, PetscReal *, TaoConvergedReason *);
PETSC_EXTERN PetscErrorCode TaoSetConvergedReason(Tao, TaoConvergedReason);
PETSC_EXTERN PetscErrorCode TaoSetSolution(Tao, Vec);
PETSC_EXTERN PetscErrorCode TaoGetSolution(Tao, Vec *);
PETSC_DEPRECATED_FUNCTION("Use TaoSetSolution() (since version 3.17)") static inline PetscErrorCode TaoSetInitialVector(Tao t, Vec v)
{
  return TaoSetSolution(t, v);
}
PETSC_DEPRECATED_FUNCTION("Use TaoGetSolution() (since version 3.17)") static inline PetscErrorCode TaoGetInitialVector(Tao t, Vec *v)
{
  return TaoGetSolution(t, v);
}

PETSC_EXTERN PetscErrorCode TaoSetObjective(Tao, PetscErrorCode (*)(Tao, Vec, PetscReal *, void *), void *);
PETSC_EXTERN PetscErrorCode TaoGetObjective(Tao, PetscErrorCode (**)(Tao, Vec, PetscReal *, void *), void **);
PETSC_EXTERN PetscErrorCode TaoSetGradient(Tao, Vec, PetscErrorCode (*)(Tao, Vec, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoGetGradient(Tao, Vec *, PetscErrorCode (**)(Tao, Vec, Vec, void *), void **);
PETSC_EXTERN PetscErrorCode TaoSetObjectiveAndGradient(Tao, Vec, PetscErrorCode (*)(Tao, Vec, PetscReal *, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoGetObjectiveAndGradient(Tao, Vec *, PetscErrorCode (**)(Tao, Vec, PetscReal *, Vec, void *), void **);
PETSC_EXTERN PetscErrorCode TaoSetHessian(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoGetHessian(Tao, Mat *, Mat *, PetscErrorCode (**)(Tao, Vec, Mat, Mat, void *), void **);
PETSC_DEPRECATED_FUNCTION("Use TaoSetObjective() (since version 3.17)") static inline PetscErrorCode TaoSetObjectiveRoutine(Tao t, PetscErrorCode (*f)(Tao, Vec, PetscReal *, void *), void *c)
{
  return TaoSetObjective(t, f, c);
}
PETSC_DEPRECATED_FUNCTION("Use TaoGetGradient() (since version 3.17)") static inline PetscErrorCode TaoGetGradientVector(Tao t, Vec *v)
{
  return TaoGetGradient(t, v, PETSC_NULLPTR, PETSC_NULLPTR);
}
PETSC_DEPRECATED_FUNCTION("Use TaoSetGradient() (since version 3.17)") static inline PetscErrorCode TaoSetGradientRoutine(Tao t, PetscErrorCode (*f)(Tao, Vec, Vec, void *), void *c)
{
  return TaoSetGradient(t, PETSC_NULLPTR, f, c);
}
PETSC_DEPRECATED_FUNCTION("Use TaoSetObjectiveAndGradient() (since version 3.17)") static inline PetscErrorCode TaoSetObjectiveAndGradientRoutine(Tao t, PetscErrorCode (*f)(Tao, Vec, PetscReal *, Vec, void *), void *c)
{
  return TaoSetObjectiveAndGradient(t, PETSC_NULLPTR, f, c);
}
PETSC_DEPRECATED_FUNCTION("Use TaoSetHessian() (since version 3.17)") static inline PetscErrorCode TaoSetHessianRoutine(Tao t, Mat H, Mat P, PetscErrorCode (*f)(Tao, Vec, Mat, Mat, void *), void *c)
{
  return TaoSetHessian(t, H, P, f, c);
}

PETSC_EXTERN PetscErrorCode TaoSetGradientNorm(Tao, Mat);
PETSC_EXTERN PetscErrorCode TaoGetGradientNorm(Tao, Mat *);
PETSC_EXTERN PetscErrorCode TaoSetLMVMMatrix(Tao, Mat);
PETSC_EXTERN PetscErrorCode TaoGetLMVMMatrix(Tao, Mat *);
PETSC_EXTERN PetscErrorCode TaoSetRecycleHistory(Tao, PetscBool);
PETSC_EXTERN PetscErrorCode TaoGetRecycleHistory(Tao, PetscBool *);
PETSC_EXTERN PetscErrorCode TaoLMVMSetH0(Tao, Mat);
PETSC_EXTERN PetscErrorCode TaoLMVMGetH0(Tao, Mat *);
PETSC_EXTERN PetscErrorCode TaoLMVMGetH0KSP(Tao, KSP *);
PETSC_EXTERN PetscErrorCode TaoLMVMRecycle(Tao, PetscBool);
PETSC_EXTERN PetscErrorCode TaoSetResidualRoutine(Tao, Vec, PetscErrorCode (*)(Tao, Vec, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetResidualWeights(Tao, Vec, PetscInt, PetscInt *, PetscInt *, PetscReal *);
PETSC_EXTERN PetscErrorCode TaoSetConstraintsRoutine(Tao, Vec, PetscErrorCode (*)(Tao, Vec, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetInequalityConstraintsRoutine(Tao, Vec, PetscErrorCode (*)(Tao, Vec, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetEqualityConstraintsRoutine(Tao, Vec, PetscErrorCode (*)(Tao, Vec, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetJacobianResidualRoutine(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetJacobianRoutine(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetJacobianStateRoutine(Tao, Mat, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetJacobianDesignRoutine(Tao, Mat, PetscErrorCode (*)(Tao, Vec, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetJacobianInequalityRoutine(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoSetJacobianEqualityRoutine(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);

PETSC_EXTERN PetscErrorCode TaoPythonSetType(Tao, const char[]);
PETSC_EXTERN PetscErrorCode TaoPythonGetType(Tao, const char *[]);

PETSC_EXTERN PetscErrorCode TaoShellSetSolve(Tao, PetscErrorCode (*)(Tao));
PETSC_EXTERN PetscErrorCode TaoShellSetContext(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoShellGetContext(Tao, void *);

PETSC_DEPRECATED_FUNCTION("Use TaoSetResidualRoutine() (since version 3.11)") static inline PetscErrorCode TaoSetSeparableObjectiveRoutine(Tao tao, Vec res, PetscErrorCode (*func)(Tao, Vec, Vec, void *), void *ctx)
{
  return TaoSetResidualRoutine(tao, res, func, ctx);
}
PETSC_DEPRECATED_FUNCTION("Use TaoSetResidualWeights() (since version 3.11)") static inline PetscErrorCode TaoSetSeparableObjectiveWeights(Tao tao, Vec sigma_v, PetscInt n, PetscInt *rows, PetscInt *cols, PetscReal *vals)
{
  return TaoSetResidualWeights(tao, sigma_v, n, rows, cols, vals);
}

PETSC_EXTERN PetscErrorCode TaoSetStateDesignIS(Tao, IS, IS);

PETSC_EXTERN PetscErrorCode TaoComputeObjective(Tao, Vec, PetscReal *);
PETSC_EXTERN PetscErrorCode TaoComputeResidual(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoTestGradient(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoComputeGradient(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoComputeObjectiveAndGradient(Tao, Vec, PetscReal *, Vec);
PETSC_EXTERN PetscErrorCode TaoComputeConstraints(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoComputeInequalityConstraints(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoComputeEqualityConstraints(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoDefaultComputeGradient(Tao, Vec, Vec, void *);
PETSC_EXTERN PetscErrorCode TaoIsObjectiveDefined(Tao, PetscBool *);
PETSC_EXTERN PetscErrorCode TaoIsGradientDefined(Tao, PetscBool *);
PETSC_EXTERN PetscErrorCode TaoIsObjectiveAndGradientDefined(Tao, PetscBool *);

PETSC_DEPRECATED_FUNCTION("Use TaoComputeResidual() (since version 3.11)") static inline PetscErrorCode TaoComputeSeparableObjective(Tao tao, Vec X, Vec F)
{
  return TaoComputeResidual(tao, X, F);
}

PETSC_EXTERN PetscErrorCode TaoTestHessian(Tao);
PETSC_EXTERN PetscErrorCode TaoComputeHessian(Tao, Vec, Mat, Mat);
PETSC_EXTERN PetscErrorCode TaoComputeResidualJacobian(Tao, Vec, Mat, Mat);
PETSC_EXTERN PetscErrorCode TaoComputeJacobian(Tao, Vec, Mat, Mat);
PETSC_EXTERN PetscErrorCode TaoComputeJacobianState(Tao, Vec, Mat, Mat, Mat);
PETSC_EXTERN PetscErrorCode TaoComputeJacobianEquality(Tao, Vec, Mat, Mat);
PETSC_EXTERN PetscErrorCode TaoComputeJacobianInequality(Tao, Vec, Mat, Mat);
PETSC_EXTERN PetscErrorCode TaoComputeJacobianDesign(Tao, Vec, Mat);

PETSC_EXTERN PetscErrorCode TaoDefaultComputeHessian(Tao, Vec, Mat, Mat, void *);
PETSC_EXTERN PetscErrorCode TaoDefaultComputeHessianColor(Tao, Vec, Mat, Mat, void *);
PETSC_EXTERN PetscErrorCode TaoDefaultComputeHessianMFFD(Tao, Vec, Mat, Mat, void *);
PETSC_EXTERN PetscErrorCode TaoComputeDualVariables(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoSetVariableBounds(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoGetVariableBounds(Tao, Vec *, Vec *);
PETSC_EXTERN PetscErrorCode TaoGetDualVariables(Tao, Vec *, Vec *);
PETSC_EXTERN PetscErrorCode TaoSetInequalityBounds(Tao, Vec, Vec);
PETSC_EXTERN PetscErrorCode TaoGetInequalityBounds(Tao, Vec *, Vec *);
PETSC_EXTERN PetscErrorCode TaoSetVariableBoundsRoutine(Tao, PetscErrorCode (*)(Tao, Vec, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoComputeVariableBounds(Tao);

PETSC_EXTERN PetscErrorCode TaoGetTolerances(Tao, PetscReal *, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode TaoSetTolerances(Tao, PetscReal, PetscReal, PetscReal);
PETSC_EXTERN PetscErrorCode TaoGetConstraintTolerances(Tao, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode TaoSetConstraintTolerances(Tao, PetscReal, PetscReal);
PETSC_EXTERN PetscErrorCode TaoSetFunctionLowerBound(Tao, PetscReal);
PETSC_EXTERN PetscErrorCode TaoSetInitialTrustRegionRadius(Tao, PetscReal);
PETSC_EXTERN PetscErrorCode TaoSetMaximumIterations(Tao, PetscInt);
PETSC_EXTERN PetscErrorCode TaoSetMaximumFunctionEvaluations(Tao, PetscInt);
PETSC_EXTERN PetscErrorCode TaoGetFunctionLowerBound(Tao, PetscReal *);
PETSC_EXTERN PetscErrorCode TaoGetInitialTrustRegionRadius(Tao, PetscReal *);
PETSC_EXTERN PetscErrorCode TaoGetCurrentTrustRegionRadius(Tao, PetscReal *);
PETSC_EXTERN PetscErrorCode TaoGetMaximumIterations(Tao, PetscInt *);
PETSC_EXTERN PetscErrorCode TaoGetCurrentFunctionEvaluations(Tao, PetscInt *);
PETSC_EXTERN PetscErrorCode TaoGetMaximumFunctionEvaluations(Tao, PetscInt *);
PETSC_EXTERN PetscErrorCode TaoGetIterationNumber(Tao, PetscInt *);
PETSC_EXTERN PetscErrorCode TaoSetIterationNumber(Tao, PetscInt);
PETSC_EXTERN PetscErrorCode TaoGetTotalIterationNumber(Tao, PetscInt *);
PETSC_EXTERN PetscErrorCode TaoSetTotalIterationNumber(Tao, PetscInt);
PETSC_EXTERN PetscErrorCode TaoGetResidualNorm(Tao, PetscReal *);

PETSC_EXTERN PetscErrorCode TaoAppendOptionsPrefix(Tao, const char[]);
PETSC_EXTERN PetscErrorCode TaoGetOptionsPrefix(Tao, const char *[]);
PETSC_EXTERN PetscErrorCode TaoResetStatistics(Tao);
PETSC_EXTERN PetscErrorCode TaoSetUpdate(Tao, PetscErrorCode (*)(Tao, PetscInt, void *), void *);

PETSC_EXTERN PetscErrorCode TaoGetKSP(Tao, KSP *);
PETSC_EXTERN PetscErrorCode TaoGetLinearSolveIterations(Tao, PetscInt *);
PETSC_EXTERN PetscErrorCode TaoKSPSetUseEW(Tao, PetscBool);

#include <petsctaolinesearch.h>

PETSC_EXTERN PetscErrorCode TaoGetLineSearch(Tao, TaoLineSearch *);

PETSC_EXTERN PetscErrorCode TaoSetConvergenceHistory(Tao, PetscReal *, PetscReal *, PetscReal *, PetscInt *, PetscInt, PetscBool);
PETSC_EXTERN PetscErrorCode TaoGetConvergenceHistory(Tao, PetscReal **, PetscReal **, PetscReal **, PetscInt **, PetscInt *);
PETSC_EXTERN PetscErrorCode TaoSetMonitor(Tao, PetscErrorCode (*)(Tao, void *), void *, PetscErrorCode (*)(void **));
PETSC_EXTERN PetscErrorCode TaoCancelMonitors(Tao);
PETSC_EXTERN PetscErrorCode TaoMonitorDefault(Tao, void *);
PETSC_DEPRECATED_FUNCTION("Use TaoMonitorDefault() (since version 3.9)") static inline PetscErrorCode TaoDefaultMonitor(Tao tao, void *ctx)
{
  return TaoMonitorDefault(tao, ctx);
}
PETSC_EXTERN PetscErrorCode TaoDefaultGMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoDefaultSMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoDefaultCMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoSolutionMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoResidualMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoGradientMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoStepDirectionMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoDrawSolutionMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoDrawStepMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoDrawGradientMonitor(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoAddLineSearchCounts(Tao);

PETSC_EXTERN PetscErrorCode TaoDefaultConvergenceTest(Tao, void *);
PETSC_EXTERN PetscErrorCode TaoSetConvergenceTest(Tao, PetscErrorCode (*)(Tao, void *), void *);

PETSC_EXTERN PetscErrorCode          TaoLCLSetStateDesignIS(Tao, IS, IS);
PETSC_EXTERN PetscErrorCode          TaoMonitor(Tao, PetscInt, PetscReal, PetscReal, PetscReal, PetscReal);
typedef struct _n_TaoMonitorDrawCtx *TaoMonitorDrawCtx;
PETSC_EXTERN PetscErrorCode          TaoMonitorDrawCtxCreate(MPI_Comm, const char[], const char[], int, int, int, int, PetscInt, TaoMonitorDrawCtx *);
PETSC_EXTERN PetscErrorCode          TaoMonitorDrawCtxDestroy(TaoMonitorDrawCtx *);

PETSC_EXTERN PetscErrorCode TaoBRGNGetSubsolver(Tao, Tao *);
PETSC_EXTERN PetscErrorCode TaoBRGNSetRegularizerObjectiveAndGradientRoutine(Tao, PetscErrorCode (*)(Tao, Vec, PetscReal *, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoBRGNSetRegularizerHessianRoutine(Tao, Mat, PetscErrorCode (*)(Tao, Vec, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoBRGNSetRegularizerWeight(Tao, PetscReal);
PETSC_EXTERN PetscErrorCode TaoBRGNSetL1SmoothEpsilon(Tao, PetscReal);
PETSC_EXTERN PetscErrorCode TaoBRGNSetDictionaryMatrix(Tao, Mat);
PETSC_EXTERN PetscErrorCode TaoBRGNGetDampingVector(Tao, Vec *);

PETSC_EXTERN PetscErrorCode TaoADMMGetMisfitSubsolver(Tao, Tao *);
PETSC_EXTERN PetscErrorCode TaoADMMGetRegularizationSubsolver(Tao, Tao *);
PETSC_EXTERN PetscErrorCode TaoADMMGetDualVector(Tao, Vec *);
PETSC_EXTERN PetscErrorCode TaoADMMGetSpectralPenalty(Tao, PetscReal *);
PETSC_EXTERN PetscErrorCode TaoADMMSetSpectralPenalty(Tao, PetscReal);
PETSC_EXTERN PetscErrorCode TaoGetADMMParentTao(Tao, Tao *);
PETSC_EXTERN PetscErrorCode TaoADMMSetConstraintVectorRHS(Tao, Vec);
PETSC_EXTERN PetscErrorCode TaoADMMSetRegularizerCoefficient(Tao, PetscReal);
PETSC_EXTERN PetscErrorCode TaoADMMSetMisfitConstraintJacobian(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoADMMSetRegularizerConstraintJacobian(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoADMMSetRegularizerHessianRoutine(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoADMMSetRegularizerObjectiveAndGradientRoutine(Tao, PetscErrorCode (*)(Tao, Vec, PetscReal *, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoADMMSetMisfitHessianRoutine(Tao, Mat, Mat, PetscErrorCode (*)(Tao, Vec, Mat, Mat, void *), void *);
PETSC_EXTERN PetscErrorCode TaoADMMSetMisfitObjectiveAndGradientRoutine(Tao, PetscErrorCode (*)(Tao, Vec, PetscReal *, Vec, void *), void *);
PETSC_EXTERN PetscErrorCode TaoADMMSetMisfitHessianChangeStatus(Tao, PetscBool);
PETSC_EXTERN PetscErrorCode TaoADMMSetRegHessianChangeStatus(Tao, PetscBool);
PETSC_EXTERN PetscErrorCode TaoADMMSetMinimumSpectralPenalty(Tao, PetscReal);
PETSC_EXTERN PetscErrorCode TaoADMMSetRegularizerType(Tao, TaoADMMRegularizerType);
PETSC_EXTERN PetscErrorCode TaoADMMGetRegularizerType(Tao, TaoADMMRegularizerType *);
PETSC_EXTERN PetscErrorCode TaoADMMSetUpdateType(Tao, TaoADMMUpdateType);
PETSC_EXTERN PetscErrorCode TaoADMMGetUpdateType(Tao, TaoADMMUpdateType *);

PETSC_EXTERN PetscErrorCode TaoALMMGetType(Tao, TaoALMMType *);
PETSC_EXTERN PetscErrorCode TaoALMMSetType(Tao, TaoALMMType);
PETSC_EXTERN PetscErrorCode TaoALMMGetSubsolver(Tao, Tao *);
PETSC_EXTERN PetscErrorCode TaoALMMSetSubsolver(Tao, Tao);
PETSC_EXTERN PetscErrorCode TaoALMMGetMultipliers(Tao, Vec *);
PETSC_EXTERN PetscErrorCode TaoALMMSetMultipliers(Tao, Vec);
PETSC_EXTERN PetscErrorCode TaoALMMGetPrimalIS(Tao, IS *, IS *);
PETSC_EXTERN PetscErrorCode TaoALMMGetDualIS(Tao, IS *, IS *);
#endif

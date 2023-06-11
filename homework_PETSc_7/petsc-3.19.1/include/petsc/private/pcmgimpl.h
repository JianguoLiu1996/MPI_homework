/*
      Data structure used for Multigrid preconditioner.
*/
#ifndef PETSC_PCMGIMPL_H
#define PETSC_PCMGIMPL_H

#include <petsc/private/pcimpl.h>
#include <petscksp.h>
#include <petscpctypes.h>
#define PETSC_MG_MAXLEVELS 10
/*
     Each level has its own copy of this data.
     Level (0) is always the coarsest level and Level (levels-1) is the finest.
*/
typedef struct {
  PetscInt cycles; /* Type of cycle to run: 1 V 2 W */
  PetscInt level;  /* level = 0 coarsest level */
  PetscInt levels; /* number of active levels used */
  Vec      b;      /* Right hand side */
  Vec      x;      /* Solution */
  Vec      r;      /* Residual */
  Mat      B;
  Mat      X;
  Mat      R;
  Mat      coarseSpace; /* A vector space which should be accurately captured by the next coarser mesh,
                                                  and thus accurately interpolated. The columns of this dense matrix
                                                  correspond to the same function discretized in
                                                  the sequence of spaces. */

  PetscErrorCode (*residual)(Mat, Vec, Vec, Vec);
  PetscErrorCode (*residualtranspose)(Mat, Vec, Vec, Vec);
  PetscErrorCode (*matresidual)(Mat, Mat, Mat, Mat);
  PetscErrorCode (*matresidualtranspose)(Mat, Mat, Mat, Mat);

  Mat           A;       /* matrix used in forming residual*/
  KSP           smoothd; /* pre smoother */
  KSP           smoothu; /* post smoother */
  KSP           cr;      /* post compatible relaxation (cr) */
  Vec           crx;     /* cr solution */
  Vec           crb;     /* cr rhs */
  Mat           interpolate;
  Mat           restrct;          /* restrict is a reserved word in C99 and on Cray */
  Mat           inject;           /* Used for moving state if provided. */
  Vec           rscale;           /* scaling of restriction matrix */
  PetscLogEvent eventsmoothsetup; /* if logging times for each level */
  PetscLogEvent eventsmoothsolve;
  PetscLogEvent eventresidual;
  PetscLogEvent eventinterprestrict;
} PC_MG_Levels;

/*
    This data structure is shared by all the levels.
*/
typedef struct {
  PCMGType         am;                     /* Multiplicative, additive or full */
  PetscInt         cyclesperpcapply;       /* Number of cycles to use in each PCApply(), multiplicative only*/
  PetscInt         maxlevels;              /* total number of levels allocated */
  PCMGGalerkinType galerkin;               /* use Galerkin process to compute coarser matrices */
  PetscBool        usedmfornumberoflevels; /* sets the number of levels by getting this information out of the DM */

  PetscBool           adaptInterpolation; /* flag to adapt the interpolator based upon the coarseSpace */
  PCMGCoarseSpaceType coarseSpaceType;    /* Type of coarse space: polynomials, harmonics, eigenvectors, ... */
  PetscInt            Nc;                 /* The number of vectors in coarseSpace */
  PetscInt            eigenvalue;         /* Key for storing the eigenvalue as a scalar in the eigenvector Vec */
  PetscBool           mespMonitor;        /* flag to monitor the multilevel eigensolver */

  PetscBool compatibleRelaxation; /* flag to monitor the coarse space quality using an auxiliary solve with compatible relaxation */

  PetscInt       nlevels;
  PC_MG_Levels **levels;
  PetscInt       default_smoothu;          /* number of smooths per level if not over-ridden */
  PetscInt       default_smoothd;          /*  with calls to KSPSetTolerances() */
  PetscReal      rtol, abstol, dtol, ttol; /* tolerances for when running with PCApplyRichardson_MG */

  void         *innerctx; /* optional data for preconditioner, like PCEXOTIC that inherits off of PCMG */
  PetscLogStage stageApply;
  PetscErrorCode (*view)(PC, PetscViewer); /* GAMG and other objects that use PCMG can set their own viewer here */
  PetscReal min_eigen_DinvA[PETSC_MG_MAXLEVELS];
  PetscReal max_eigen_DinvA[PETSC_MG_MAXLEVELS];
} PC_MG;

PETSC_INTERN PetscErrorCode PCSetUp_MG(PC);
PETSC_INTERN PetscErrorCode PCDestroy_MG(PC);
PETSC_INTERN PetscErrorCode PCSetFromOptions_MG(PC, PetscOptionItems *PetscOptionsObject);
PETSC_INTERN PetscErrorCode PCView_MG(PC, PetscViewer);
PETSC_INTERN PetscErrorCode PCMGGetLevels_MG(PC, PetscInt *);
PETSC_INTERN PetscErrorCode PCMGSetLevels_MG(PC, PetscInt, MPI_Comm *);
PETSC_DEPRECATED_FUNCTION("Use PCMGResidualDefault() (since version 3.5)") static inline PetscErrorCode PCMGResidual_Default(Mat A, Vec b, Vec x, Vec r)
{
  return PCMGResidualDefault(A, b, x, r);
}

PETSC_INTERN PetscErrorCode DMSetBasisFunction_Internal(PetscInt, PetscBool, PetscInt, PetscErrorCode (**)(PetscInt, PetscReal, const PetscReal[], PetscInt, PetscScalar *, void *));
PETSC_INTERN PetscErrorCode PCMGComputeCoarseSpace_Internal(PC, PetscInt, PCMGCoarseSpaceType, PetscInt, Mat, Mat *);
PETSC_INTERN PetscErrorCode PCMGAdaptInterpolator_Internal(PC, PetscInt, KSP, KSP, Mat, Mat);
PETSC_INTERN PetscErrorCode PCMGRecomputeLevelOperators_Internal(PC, PetscInt);
PETSC_INTERN PetscErrorCode PCMGACycle_Private(PC, PC_MG_Levels **, PetscBool, PetscBool);
PETSC_INTERN PetscErrorCode PCMGFCycle_Private(PC, PC_MG_Levels **, PetscBool, PetscBool);
PETSC_INTERN PetscErrorCode PCMGKCycle_Private(PC, PC_MG_Levels **, PetscBool, PetscBool);
PETSC_INTERN PetscErrorCode PCMGMCycle_Private(PC, PC_MG_Levels **, PetscBool, PetscBool, PCRichardsonConvergedReason *);

PETSC_INTERN PetscErrorCode PCMGGDSWCreateCoarseSpace_Private(PC, PetscInt, DM, KSP, PetscInt, Mat, Mat *);
#endif // PETSC_PCMGIMPL_H

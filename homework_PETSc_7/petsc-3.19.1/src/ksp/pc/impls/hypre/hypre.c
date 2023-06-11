/*
   Provides an interface to the LLNL package hypre
*/

#include <petscpkg_version.h>
#include <petsc/private/pcimpl.h> /*I "petscpc.h" I*/
/* this include is needed ONLY to allow access to the private data inside the Mat object specific to hypre */
#include <petsc/private/matimpl.h>
#include <petsc/private/vecimpl.h>
#include <../src/vec/vec/impls/hypre/vhyp.h>
#include <../src/mat/impls/hypre/mhypre.h>
#include <../src/dm/impls/da/hypre/mhyp.h>
#include <_hypre_parcsr_ls.h>
#include <petscmathypre.h>

#if defined(PETSC_HAVE_HYPRE_DEVICE)
  #include <petsc/private/deviceimpl.h>
#endif

static PetscBool  cite            = PETSC_FALSE;
static const char hypreCitation[] = "@manual{hypre-web-page,\n  title  = {{\\sl hypre}: High Performance Preconditioners},\n  organization = {Lawrence Livermore National Laboratory},\n  note  = "
                                    "{\\url{https://www.llnl.gov/casc/hypre}}\n}\n";

/*
   Private context (data structure) for the  preconditioner.
*/
typedef struct {
  HYPRE_Solver hsolver;
  Mat          hpmat; /* MatHYPRE */

  HYPRE_Int (*destroy)(HYPRE_Solver);
  HYPRE_Int (*solve)(HYPRE_Solver, HYPRE_ParCSRMatrix, HYPRE_ParVector, HYPRE_ParVector);
  HYPRE_Int (*setup)(HYPRE_Solver, HYPRE_ParCSRMatrix, HYPRE_ParVector, HYPRE_ParVector);

  MPI_Comm comm_hypre;
  char    *hypre_type;

  /* options for Pilut and BoomerAMG*/
  PetscInt  maxiter;
  PetscReal tol;

  /* options for Pilut */
  PetscInt factorrowsize;

  /* options for ParaSails */
  PetscInt  nlevels;
  PetscReal threshold;
  PetscReal filter;
  PetscReal loadbal;
  PetscInt  logging;
  PetscInt  ruse;
  PetscInt  symt;

  /* options for BoomerAMG */
  PetscBool printstatistics;

  /* options for BoomerAMG */
  PetscInt  cycletype;
  PetscInt  maxlevels;
  PetscReal strongthreshold;
  PetscReal maxrowsum;
  PetscInt  gridsweeps[3];
  PetscInt  coarsentype;
  PetscInt  measuretype;
  PetscInt  smoothtype;
  PetscInt  smoothnumlevels;
  PetscInt  eu_level;         /* Number of levels for ILU(k) in Euclid */
  PetscReal eu_droptolerance; /* Drop tolerance for ILU(k) in Euclid */
  PetscInt  eu_bj;            /* Defines use of Block Jacobi ILU in Euclid */
  PetscInt  relaxtype[3];
  PetscReal relaxweight;
  PetscReal outerrelaxweight;
  PetscInt  relaxorder;
  PetscReal truncfactor;
  PetscBool applyrichardson;
  PetscInt  pmax;
  PetscInt  interptype;
  PetscInt  maxc;
  PetscInt  minc;
#if PETSC_PKG_HYPRE_VERSION_GE(2, 23, 0)
  char *spgemm_type; // this is a global hypre parameter but is closely associated with BoomerAMG
#endif
  /* GPU */
  PetscBool keeptranspose;
  PetscInt  rap2;
  PetscInt  mod_rap2;

  /* AIR */
  PetscInt  Rtype;
  PetscReal Rstrongthreshold;
  PetscReal Rfilterthreshold;
  PetscInt  Adroptype;
  PetscReal Adroptol;

  PetscInt  agg_nl;
  PetscInt  agg_interptype;
  PetscInt  agg_num_paths;
  PetscBool nodal_relax;
  PetscInt  nodal_relax_levels;

  PetscInt  nodal_coarsening;
  PetscInt  nodal_coarsening_diag;
  PetscInt  vec_interp_variant;
  PetscInt  vec_interp_qmax;
  PetscBool vec_interp_smooth;
  PetscInt  interp_refine;

  /* NearNullSpace support */
  VecHYPRE_IJVector *hmnull;
  HYPRE_ParVector   *phmnull;
  PetscInt           n_hmnull;
  Vec                hmnull_constant;

  /* options for AS (Auxiliary Space preconditioners) */
  PetscInt  as_print;
  PetscInt  as_max_iter;
  PetscReal as_tol;
  PetscInt  as_relax_type;
  PetscInt  as_relax_times;
  PetscReal as_relax_weight;
  PetscReal as_omega;
  PetscInt  as_amg_alpha_opts[5]; /* AMG coarsen type, agg_levels, relax_type, interp_type, Pmax for vector Poisson (AMS) or Curl problem (ADS) */
  PetscReal as_amg_alpha_theta;   /* AMG strength for vector Poisson (AMS) or Curl problem (ADS) */
  PetscInt  as_amg_beta_opts[5];  /* AMG coarsen type, agg_levels, relax_type, interp_type, Pmax for scalar Poisson (AMS) or vector Poisson (ADS) */
  PetscReal as_amg_beta_theta;    /* AMG strength for scalar Poisson (AMS) or vector Poisson (ADS)  */
  PetscInt  ams_cycle_type;
  PetscInt  ads_cycle_type;

  /* additional data */
  Mat G;             /* MatHYPRE */
  Mat C;             /* MatHYPRE */
  Mat alpha_Poisson; /* MatHYPRE */
  Mat beta_Poisson;  /* MatHYPRE */

  /* extra information for AMS */
  PetscInt          dim; /* geometrical dimension */
  VecHYPRE_IJVector coords[3];
  VecHYPRE_IJVector constants[3];
  VecHYPRE_IJVector interior;
  Mat               RT_PiFull, RT_Pi[3];
  Mat               ND_PiFull, ND_Pi[3];
  PetscBool         ams_beta_is_zero;
  PetscBool         ams_beta_is_zero_part;
  PetscInt          ams_proj_freq;
} PC_HYPRE;

PetscErrorCode PCHYPREGetSolver(PC pc, HYPRE_Solver *hsolver)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;

  PetscFunctionBegin;
  *hsolver = jac->hsolver;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Matrices with AIJ format are created IN PLACE with using (I,J,data) from BoomerAMG. Since the data format in hypre_ParCSRMatrix
  is different from that used in PETSc, the original hypre_ParCSRMatrix can not be used any more after call this routine.
  It is used in PCHMG. Other users should avoid using this function.
*/
static PetscErrorCode PCGetCoarseOperators_BoomerAMG(PC pc, PetscInt *nlevels, Mat *operators[])
{
  PC_HYPRE            *jac  = (PC_HYPRE *)pc->data;
  PetscBool            same = PETSC_FALSE;
  PetscInt             num_levels, l;
  Mat                 *mattmp;
  hypre_ParCSRMatrix **A_array;

  PetscFunctionBegin;
  PetscCall(PetscStrcmp(jac->hypre_type, "boomeramg", &same));
  PetscCheck(same, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_NOTSAMETYPE, "Hypre type is not BoomerAMG ");
  num_levels = hypre_ParAMGDataNumLevels((hypre_ParAMGData *)(jac->hsolver));
  PetscCall(PetscMalloc1(num_levels, &mattmp));
  A_array = hypre_ParAMGDataAArray((hypre_ParAMGData *)(jac->hsolver));
  for (l = 1; l < num_levels; l++) {
    PetscCall(MatCreateFromParCSR(A_array[l], MATAIJ, PETSC_OWN_POINTER, &(mattmp[num_levels - 1 - l])));
    /* We want to own the data, and HYPRE can not touch this matrix any more */
    A_array[l] = NULL;
  }
  *nlevels   = num_levels;
  *operators = mattmp;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Matrices with AIJ format are created IN PLACE with using (I,J,data) from BoomerAMG. Since the data format in hypre_ParCSRMatrix
  is different from that used in PETSc, the original hypre_ParCSRMatrix can not be used any more after call this routine.
  It is used in PCHMG. Other users should avoid using this function.
*/
static PetscErrorCode PCGetInterpolations_BoomerAMG(PC pc, PetscInt *nlevels, Mat *interpolations[])
{
  PC_HYPRE            *jac  = (PC_HYPRE *)pc->data;
  PetscBool            same = PETSC_FALSE;
  PetscInt             num_levels, l;
  Mat                 *mattmp;
  hypre_ParCSRMatrix **P_array;

  PetscFunctionBegin;
  PetscCall(PetscStrcmp(jac->hypre_type, "boomeramg", &same));
  PetscCheck(same, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_NOTSAMETYPE, "Hypre type is not BoomerAMG ");
  num_levels = hypre_ParAMGDataNumLevels((hypre_ParAMGData *)(jac->hsolver));
  PetscCall(PetscMalloc1(num_levels, &mattmp));
  P_array = hypre_ParAMGDataPArray((hypre_ParAMGData *)(jac->hsolver));
  for (l = 1; l < num_levels; l++) {
    PetscCall(MatCreateFromParCSR(P_array[num_levels - 1 - l], MATAIJ, PETSC_OWN_POINTER, &(mattmp[l - 1])));
    /* We want to own the data, and HYPRE can not touch this matrix any more */
    P_array[num_levels - 1 - l] = NULL;
  }
  *nlevels        = num_levels;
  *interpolations = mattmp;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Resets (frees) Hypre's representation of the near null space */
static PetscErrorCode PCHYPREResetNearNullSpace_Private(PC pc)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscInt  i;

  PetscFunctionBegin;
  for (i = 0; i < jac->n_hmnull; i++) PetscCall(VecHYPRE_IJVectorDestroy(&jac->hmnull[i]));
  PetscCall(PetscFree(jac->hmnull));
  PetscCall(PetscFree(jac->phmnull));
  PetscCall(VecDestroy(&jac->hmnull_constant));
  jac->n_hmnull = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCSetUp_HYPRE(PC pc)
{
  PC_HYPRE          *jac = (PC_HYPRE *)pc->data;
  Mat_HYPRE         *hjac;
  HYPRE_ParCSRMatrix hmat;
  HYPRE_ParVector    bv, xv;
  PetscBool          ishypre;

  PetscFunctionBegin;
  if (!jac->hypre_type) PetscCall(PCHYPRESetType(pc, "boomeramg"));

  PetscCall(PetscObjectTypeCompare((PetscObject)pc->pmat, MATHYPRE, &ishypre));
  if (!ishypre) {
    PetscCall(MatDestroy(&jac->hpmat));
    PetscCall(MatConvert(pc->pmat, MATHYPRE, MAT_INITIAL_MATRIX, &jac->hpmat));
  } else {
    PetscCall(PetscObjectReference((PetscObject)pc->pmat));
    PetscCall(MatDestroy(&jac->hpmat));
    jac->hpmat = pc->pmat;
  }
  /* allow debug */
  PetscCall(MatViewFromOptions(jac->hpmat, NULL, "-pc_hypre_mat_view"));
  hjac = (Mat_HYPRE *)(jac->hpmat->data);

  /* special case for BoomerAMG */
  if (jac->setup == HYPRE_BoomerAMGSetup) {
    MatNullSpace mnull;
    PetscBool    has_const;
    PetscInt     bs, nvec, i;
    const Vec   *vecs;

    PetscCall(MatGetBlockSize(pc->pmat, &bs));
    if (bs > 1) PetscCallExternal(HYPRE_BoomerAMGSetNumFunctions, jac->hsolver, bs);
    PetscCall(MatGetNearNullSpace(pc->mat, &mnull));
    if (mnull) {
      PetscCall(PCHYPREResetNearNullSpace_Private(pc));
      PetscCall(MatNullSpaceGetVecs(mnull, &has_const, &nvec, &vecs));
      PetscCall(PetscMalloc1(nvec + 1, &jac->hmnull));
      PetscCall(PetscMalloc1(nvec + 1, &jac->phmnull));
      for (i = 0; i < nvec; i++) {
        PetscCall(VecHYPRE_IJVectorCreate(vecs[i]->map, &jac->hmnull[i]));
        PetscCall(VecHYPRE_IJVectorCopy(vecs[i], jac->hmnull[i]));
        PetscCallExternal(HYPRE_IJVectorGetObject, jac->hmnull[i]->ij, (void **)&jac->phmnull[i]);
      }
      if (has_const) {
        PetscCall(MatCreateVecs(pc->pmat, &jac->hmnull_constant, NULL));
        PetscCall(VecSet(jac->hmnull_constant, 1));
        PetscCall(VecNormalize(jac->hmnull_constant, NULL));
        PetscCall(VecHYPRE_IJVectorCreate(jac->hmnull_constant->map, &jac->hmnull[nvec]));
        PetscCall(VecHYPRE_IJVectorCopy(jac->hmnull_constant, jac->hmnull[nvec]));
        PetscCallExternal(HYPRE_IJVectorGetObject, jac->hmnull[nvec]->ij, (void **)&jac->phmnull[nvec]);
        nvec++;
      }
      PetscCallExternal(HYPRE_BoomerAMGSetInterpVectors, jac->hsolver, nvec, jac->phmnull);
      jac->n_hmnull = nvec;
    }
  }

  /* special case for AMS */
  if (jac->setup == HYPRE_AMSSetup) {
    Mat_HYPRE         *hm;
    HYPRE_ParCSRMatrix parcsr;
    if (!jac->coords[0] && !jac->constants[0] && !(jac->ND_PiFull || (jac->ND_Pi[0] && jac->ND_Pi[1]))) {
      SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "HYPRE AMS preconditioner needs either the coordinate vectors via PCSetCoordinates() or the edge constant vectors via PCHYPRESetEdgeConstantVectors() or the interpolation matrix via PCHYPRESetInterpolations()");
    }
    if (jac->dim) PetscCallExternal(HYPRE_AMSSetDimension, jac->hsolver, jac->dim);
    if (jac->constants[0]) {
      HYPRE_ParVector ozz, zoz, zzo = NULL;
      PetscCallExternal(HYPRE_IJVectorGetObject, jac->constants[0]->ij, (void **)(&ozz));
      PetscCallExternal(HYPRE_IJVectorGetObject, jac->constants[1]->ij, (void **)(&zoz));
      if (jac->constants[2]) PetscCallExternal(HYPRE_IJVectorGetObject, jac->constants[2]->ij, (void **)(&zzo));
      PetscCallExternal(HYPRE_AMSSetEdgeConstantVectors, jac->hsolver, ozz, zoz, zzo);
    }
    if (jac->coords[0]) {
      HYPRE_ParVector coords[3];
      coords[0] = NULL;
      coords[1] = NULL;
      coords[2] = NULL;
      if (jac->coords[0]) PetscCallExternal(HYPRE_IJVectorGetObject, jac->coords[0]->ij, (void **)(&coords[0]));
      if (jac->coords[1]) PetscCallExternal(HYPRE_IJVectorGetObject, jac->coords[1]->ij, (void **)(&coords[1]));
      if (jac->coords[2]) PetscCallExternal(HYPRE_IJVectorGetObject, jac->coords[2]->ij, (void **)(&coords[2]));
      PetscCallExternal(HYPRE_AMSSetCoordinateVectors, jac->hsolver, coords[0], coords[1], coords[2]);
    }
    PetscCheck(jac->G, PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "HYPRE AMS preconditioner needs the discrete gradient operator via PCHYPRESetDiscreteGradient");
    hm = (Mat_HYPRE *)(jac->G->data);
    PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&parcsr));
    PetscCallExternal(HYPRE_AMSSetDiscreteGradient, jac->hsolver, parcsr);
    if (jac->alpha_Poisson) {
      hm = (Mat_HYPRE *)(jac->alpha_Poisson->data);
      PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&parcsr));
      PetscCallExternal(HYPRE_AMSSetAlphaPoissonMatrix, jac->hsolver, parcsr);
    }
    if (jac->ams_beta_is_zero) {
      PetscCallExternal(HYPRE_AMSSetBetaPoissonMatrix, jac->hsolver, NULL);
    } else if (jac->beta_Poisson) {
      hm = (Mat_HYPRE *)(jac->beta_Poisson->data);
      PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&parcsr));
      PetscCallExternal(HYPRE_AMSSetBetaPoissonMatrix, jac->hsolver, parcsr);
    } else if (jac->ams_beta_is_zero_part) {
      if (jac->interior) {
        HYPRE_ParVector interior = NULL;
        PetscCallExternal(HYPRE_IJVectorGetObject, jac->interior->ij, (void **)(&interior));
        PetscCallExternal(HYPRE_AMSSetInteriorNodes, jac->hsolver, interior);
      } else {
        jac->ams_beta_is_zero_part = PETSC_FALSE;
      }
    }
    if (jac->ND_PiFull || (jac->ND_Pi[0] && jac->ND_Pi[1])) {
      PetscInt           i;
      HYPRE_ParCSRMatrix nd_parcsrfull, nd_parcsr[3];
      if (jac->ND_PiFull) {
        hm = (Mat_HYPRE *)(jac->ND_PiFull->data);
        PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&nd_parcsrfull));
      } else {
        nd_parcsrfull = NULL;
      }
      for (i = 0; i < 3; ++i) {
        if (jac->ND_Pi[i]) {
          hm = (Mat_HYPRE *)(jac->ND_Pi[i]->data);
          PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&nd_parcsr[i]));
        } else {
          nd_parcsr[i] = NULL;
        }
      }
      PetscCallExternal(HYPRE_AMSSetInterpolations, jac->hsolver, nd_parcsrfull, nd_parcsr[0], nd_parcsr[1], nd_parcsr[2]);
    }
  }
  /* special case for ADS */
  if (jac->setup == HYPRE_ADSSetup) {
    Mat_HYPRE         *hm;
    HYPRE_ParCSRMatrix parcsr;
    if (!jac->coords[0] && !((jac->RT_PiFull || (jac->RT_Pi[0] && jac->RT_Pi[1])) && (jac->ND_PiFull || (jac->ND_Pi[0] && jac->ND_Pi[1])))) {
      SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "HYPRE ADS preconditioner needs either the coordinate vectors via PCSetCoordinates() or the interpolation matrices via PCHYPRESetInterpolations");
    } else PetscCheck(jac->coords[1] && jac->coords[2], PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "HYPRE ADS preconditioner has been designed for three dimensional problems! For two dimensional problems, use HYPRE AMS instead");
    PetscCheck(jac->G, PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "HYPRE ADS preconditioner needs the discrete gradient operator via PCHYPRESetDiscreteGradient");
    PetscCheck(jac->C, PetscObjectComm((PetscObject)pc), PETSC_ERR_USER, "HYPRE ADS preconditioner needs the discrete curl operator via PCHYPRESetDiscreteGradient");
    if (jac->coords[0]) {
      HYPRE_ParVector coords[3];
      coords[0] = NULL;
      coords[1] = NULL;
      coords[2] = NULL;
      if (jac->coords[0]) PetscCallExternal(HYPRE_IJVectorGetObject, jac->coords[0]->ij, (void **)(&coords[0]));
      if (jac->coords[1]) PetscCallExternal(HYPRE_IJVectorGetObject, jac->coords[1]->ij, (void **)(&coords[1]));
      if (jac->coords[2]) PetscCallExternal(HYPRE_IJVectorGetObject, jac->coords[2]->ij, (void **)(&coords[2]));
      PetscCallExternal(HYPRE_ADSSetCoordinateVectors, jac->hsolver, coords[0], coords[1], coords[2]);
    }
    hm = (Mat_HYPRE *)(jac->G->data);
    PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&parcsr));
    PetscCallExternal(HYPRE_ADSSetDiscreteGradient, jac->hsolver, parcsr);
    hm = (Mat_HYPRE *)(jac->C->data);
    PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&parcsr));
    PetscCallExternal(HYPRE_ADSSetDiscreteCurl, jac->hsolver, parcsr);
    if ((jac->RT_PiFull || (jac->RT_Pi[0] && jac->RT_Pi[1])) && (jac->ND_PiFull || (jac->ND_Pi[0] && jac->ND_Pi[1]))) {
      PetscInt           i;
      HYPRE_ParCSRMatrix rt_parcsrfull, rt_parcsr[3];
      HYPRE_ParCSRMatrix nd_parcsrfull, nd_parcsr[3];
      if (jac->RT_PiFull) {
        hm = (Mat_HYPRE *)(jac->RT_PiFull->data);
        PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&rt_parcsrfull));
      } else {
        rt_parcsrfull = NULL;
      }
      for (i = 0; i < 3; ++i) {
        if (jac->RT_Pi[i]) {
          hm = (Mat_HYPRE *)(jac->RT_Pi[i]->data);
          PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&rt_parcsr[i]));
        } else {
          rt_parcsr[i] = NULL;
        }
      }
      if (jac->ND_PiFull) {
        hm = (Mat_HYPRE *)(jac->ND_PiFull->data);
        PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&nd_parcsrfull));
      } else {
        nd_parcsrfull = NULL;
      }
      for (i = 0; i < 3; ++i) {
        if (jac->ND_Pi[i]) {
          hm = (Mat_HYPRE *)(jac->ND_Pi[i]->data);
          PetscCallExternal(HYPRE_IJMatrixGetObject, hm->ij, (void **)(&nd_parcsr[i]));
        } else {
          nd_parcsr[i] = NULL;
        }
      }
      PetscCallExternal(HYPRE_ADSSetInterpolations, jac->hsolver, rt_parcsrfull, rt_parcsr[0], rt_parcsr[1], rt_parcsr[2], nd_parcsrfull, nd_parcsr[0], nd_parcsr[1], nd_parcsr[2]);
    }
  }
  PetscCallExternal(HYPRE_IJMatrixGetObject, hjac->ij, (void **)&hmat);
  PetscCallExternal(HYPRE_IJVectorGetObject, hjac->b->ij, (void **)&bv);
  PetscCallExternal(HYPRE_IJVectorGetObject, hjac->x->ij, (void **)&xv);
  PetscCallExternal(jac->setup, jac->hsolver, hmat, bv, xv);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCApply_HYPRE(PC pc, Vec b, Vec x)
{
  PC_HYPRE          *jac  = (PC_HYPRE *)pc->data;
  Mat_HYPRE         *hjac = (Mat_HYPRE *)(jac->hpmat->data);
  HYPRE_ParCSRMatrix hmat;
  HYPRE_ParVector    jbv, jxv;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  if (!jac->applyrichardson) PetscCall(VecSet(x, 0.0));
  PetscCall(VecHYPRE_IJVectorPushVecRead(hjac->b, b));
  if (jac->applyrichardson) PetscCall(VecHYPRE_IJVectorPushVec(hjac->x, x));
  else PetscCall(VecHYPRE_IJVectorPushVecWrite(hjac->x, x));
  PetscCallExternal(HYPRE_IJMatrixGetObject, hjac->ij, (void **)&hmat);
  PetscCallExternal(HYPRE_IJVectorGetObject, hjac->b->ij, (void **)&jbv);
  PetscCallExternal(HYPRE_IJVectorGetObject, hjac->x->ij, (void **)&jxv);
  PetscStackCallExternalVoid(
    "Hypre solve", do {
      HYPRE_Int hierr = (*jac->solve)(jac->hsolver, hmat, jbv, jxv);
      if (hierr) {
        PetscCheck(hierr == HYPRE_ERROR_CONV, PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in HYPRE solver, error code %d", (int)hierr);
        hypre__global_error = 0;
      }
    } while (0));

  if (jac->setup == HYPRE_AMSSetup && jac->ams_beta_is_zero_part) PetscCallExternal(HYPRE_AMSProjectOutGradients, jac->hsolver, jxv);
  PetscCall(VecHYPRE_IJVectorPopVec(hjac->x));
  PetscCall(VecHYPRE_IJVectorPopVec(hjac->b));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCReset_HYPRE(PC pc)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;

  PetscFunctionBegin;
  PetscCall(MatDestroy(&jac->hpmat));
  PetscCall(MatDestroy(&jac->G));
  PetscCall(MatDestroy(&jac->C));
  PetscCall(MatDestroy(&jac->alpha_Poisson));
  PetscCall(MatDestroy(&jac->beta_Poisson));
  PetscCall(MatDestroy(&jac->RT_PiFull));
  PetscCall(MatDestroy(&jac->RT_Pi[0]));
  PetscCall(MatDestroy(&jac->RT_Pi[1]));
  PetscCall(MatDestroy(&jac->RT_Pi[2]));
  PetscCall(MatDestroy(&jac->ND_PiFull));
  PetscCall(MatDestroy(&jac->ND_Pi[0]));
  PetscCall(MatDestroy(&jac->ND_Pi[1]));
  PetscCall(MatDestroy(&jac->ND_Pi[2]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->coords[0]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->coords[1]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->coords[2]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->constants[0]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->constants[1]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->constants[2]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->interior));
  PetscCall(PCHYPREResetNearNullSpace_Private(pc));
  jac->ams_beta_is_zero      = PETSC_FALSE;
  jac->ams_beta_is_zero_part = PETSC_FALSE;
  jac->dim                   = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCDestroy_HYPRE(PC pc)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;

  PetscFunctionBegin;
  PetscCall(PCReset_HYPRE(pc));
  if (jac->destroy) PetscCallExternal(jac->destroy, jac->hsolver);
  PetscCall(PetscFree(jac->hypre_type));
#if PETSC_PKG_HYPRE_VERSION_GE(2, 23, 0)
  PetscCall(PetscFree(jac->spgemm_type));
#endif
  if (jac->comm_hypre != MPI_COMM_NULL) PetscCall(PetscCommRestoreComm(PetscObjectComm((PetscObject)pc), &jac->comm_hypre));
  PetscCall(PetscFree(pc->data));

  PetscCall(PetscObjectChangeTypeName((PetscObject)pc, 0));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPREGetType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetDiscreteGradient_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetDiscreteCurl_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetInterpolations_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetConstantEdgeVectors_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetPoissonMatrix_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetEdgeConstantVectors_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPREAMSSetInteriorNodes_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCGetInterpolations_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCGetCoarseOperators_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCMGGalerkinSetMatProductAlgorithm_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCMGGalerkinGetMatProductAlgorithm_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCSetCoordinates_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCSetFromOptions_HYPRE_Pilut(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool flag;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "HYPRE Pilut Options");
  PetscCall(PetscOptionsInt("-pc_hypre_pilut_maxiter", "Number of iterations", "None", jac->maxiter, &jac->maxiter, &flag));
  if (flag) PetscCallExternal(HYPRE_ParCSRPilutSetMaxIter, jac->hsolver, jac->maxiter);
  PetscCall(PetscOptionsReal("-pc_hypre_pilut_tol", "Drop tolerance", "None", jac->tol, &jac->tol, &flag));
  if (flag) PetscCallExternal(HYPRE_ParCSRPilutSetDropTolerance, jac->hsolver, jac->tol);
  PetscCall(PetscOptionsInt("-pc_hypre_pilut_factorrowsize", "FactorRowSize", "None", jac->factorrowsize, &jac->factorrowsize, &flag));
  if (flag) PetscCallExternal(HYPRE_ParCSRPilutSetFactorRowSize, jac->hsolver, jac->factorrowsize);
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCView_HYPRE_Pilut(PC pc, PetscViewer viewer)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE Pilut preconditioning\n"));
    if (jac->maxiter != PETSC_DEFAULT) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    maximum number of iterations %" PetscInt_FMT "\n", jac->maxiter));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    default maximum number of iterations \n"));
    }
    if (jac->tol != PETSC_DEFAULT) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    drop tolerance %g\n", (double)jac->tol));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    default drop tolerance \n"));
    }
    if (jac->factorrowsize != PETSC_DEFAULT) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    factor row size %" PetscInt_FMT "\n", jac->factorrowsize));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    default factor row size \n"));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCSetFromOptions_HYPRE_Euclid(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool flag, eu_bj = jac->eu_bj ? PETSC_TRUE : PETSC_FALSE;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "HYPRE Euclid Options");
  PetscCall(PetscOptionsInt("-pc_hypre_euclid_level", "Factorization levels", "None", jac->eu_level, &jac->eu_level, &flag));
  if (flag) PetscCallExternal(HYPRE_EuclidSetLevel, jac->hsolver, jac->eu_level);

  PetscCall(PetscOptionsReal("-pc_hypre_euclid_droptolerance", "Drop tolerance for ILU(k) in Euclid", "None", jac->eu_droptolerance, &jac->eu_droptolerance, &flag));
  if (flag) {
    PetscMPIInt size;

    PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)pc), &size));
    PetscCheck(size == 1, PetscObjectComm((PetscObject)pc), PETSC_ERR_SUP, "hypre's Euclid does not support a parallel drop tolerance");
    PetscCallExternal(HYPRE_EuclidSetILUT, jac->hsolver, jac->eu_droptolerance);
  }

  PetscCall(PetscOptionsBool("-pc_hypre_euclid_bj", "Use Block Jacobi for ILU in Euclid", "None", eu_bj, &eu_bj, &flag));
  if (flag) {
    jac->eu_bj = eu_bj ? 1 : 0;
    PetscCallExternal(HYPRE_EuclidSetBJ, jac->hsolver, jac->eu_bj);
  }
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCView_HYPRE_Euclid(PC pc, PetscViewer viewer)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE Euclid preconditioning\n"));
    if (jac->eu_level != PETSC_DEFAULT) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    factorization levels %" PetscInt_FMT "\n", jac->eu_level));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    default factorization levels \n"));
    }
    PetscCall(PetscViewerASCIIPrintf(viewer, "    drop tolerance %g\n", (double)jac->eu_droptolerance));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    use Block-Jacobi? %" PetscInt_FMT "\n", jac->eu_bj));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCApplyTranspose_HYPRE_BoomerAMG(PC pc, Vec b, Vec x)
{
  PC_HYPRE          *jac  = (PC_HYPRE *)pc->data;
  Mat_HYPRE         *hjac = (Mat_HYPRE *)(jac->hpmat->data);
  HYPRE_ParCSRMatrix hmat;
  HYPRE_ParVector    jbv, jxv;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  PetscCall(VecSet(x, 0.0));
  PetscCall(VecHYPRE_IJVectorPushVecRead(hjac->x, b));
  PetscCall(VecHYPRE_IJVectorPushVecWrite(hjac->b, x));

  PetscCallExternal(HYPRE_IJMatrixGetObject, hjac->ij, (void **)&hmat);
  PetscCallExternal(HYPRE_IJVectorGetObject, hjac->b->ij, (void **)&jbv);
  PetscCallExternal(HYPRE_IJVectorGetObject, hjac->x->ij, (void **)&jxv);

  PetscStackCallExternalVoid(
    "Hypre Transpose solve", do {
      HYPRE_Int hierr = HYPRE_BoomerAMGSolveT(jac->hsolver, hmat, jbv, jxv);
      if (hierr) {
        /* error code of 1 in BoomerAMG merely means convergence not achieved */
        PetscCheck(hierr == 1, PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in HYPRE solver, error code %d", (int)hierr);
        hypre__global_error = 0;
      }
    } while (0));

  PetscCall(VecHYPRE_IJVectorPopVec(hjac->x));
  PetscCall(VecHYPRE_IJVectorPopVec(hjac->b));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCMGGalerkinSetMatProductAlgorithm_HYPRE_BoomerAMG(PC pc, const char name[])
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool flag;

#if PETSC_PKG_HYPRE_VERSION_GE(2, 23, 0)
  PetscFunctionBegin;
  if (jac->spgemm_type) {
    PetscCall(PetscStrcmp(jac->spgemm_type, name, &flag));
    PetscCheck(flag, PetscObjectComm((PetscObject)pc), PETSC_ERR_ORDER, "Cannot reset the HYPRE SpGEMM (really we can)");
    PetscFunctionReturn(PETSC_SUCCESS);
  } else {
    PetscCall(PetscStrallocpy(name, &jac->spgemm_type));
  }
  PetscCall(PetscStrcmp("cusparse", jac->spgemm_type, &flag));
  if (flag) {
    PetscCallExternal(HYPRE_SetSpGemmUseCusparse, 1);
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscStrcmp("hypre", jac->spgemm_type, &flag));
  if (flag) {
    PetscCallExternal(HYPRE_SetSpGemmUseCusparse, 0);
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  jac->spgemm_type = NULL;
  SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_UNKNOWN_TYPE, "Unknown HYPRE SpGEM type %s; Choices are cusparse, hypre", name);
#endif
}

static PetscErrorCode PCMGGalerkinGetMatProductAlgorithm_HYPRE_BoomerAMG(PC pc, const char *spgemm[])
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
#if PETSC_PKG_HYPRE_VERSION_GE(2, 23, 0)
  *spgemm = jac->spgemm_type;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

static const char *HYPREBoomerAMGCycleType[]   = {"", "V", "W"};
static const char *HYPREBoomerAMGCoarsenType[] = {"CLJP", "Ruge-Stueben", "", "modifiedRuge-Stueben", "", "", "Falgout", "", "PMIS", "", "HMIS"};
static const char *HYPREBoomerAMGMeasureType[] = {"local", "global"};
/* The following corresponds to HYPRE_BoomerAMGSetRelaxType which has many missing numbers in the enum */
static const char *HYPREBoomerAMGSmoothType[] = {"Schwarz-smoothers", "Pilut", "ParaSails", "Euclid"};
static const char *HYPREBoomerAMGRelaxType[] = {"Jacobi", "sequential-Gauss-Seidel", "seqboundary-Gauss-Seidel", "SOR/Jacobi", "backward-SOR/Jacobi", "" /* [5] hybrid chaotic Gauss-Seidel (works only with OpenMP) */, "symmetric-SOR/Jacobi", "" /* 7 */, "l1scaled-SOR/Jacobi", "Gaussian-elimination", "" /* 10 */, "" /* 11 */, "" /* 12 */, "l1-Gauss-Seidel" /* nonsymmetric */, "backward-l1-Gauss-Seidel" /* nonsymmetric */, "CG" /* non-stationary */, "Chebyshev", "FCF-Jacobi", "l1scaled-Jacobi"};
static const char    *HYPREBoomerAMGInterpType[] = {"classical", "", "", "direct", "multipass", "multipass-wts", "ext+i", "ext+i-cc", "standard", "standard-wts", "block", "block-wtd", "FF", "FF1", "ext", "ad-wts", "ext-mm", "ext+i-mm", "ext+e-mm"};
static PetscErrorCode PCSetFromOptions_HYPRE_BoomerAMG(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_HYPRE   *jac = (PC_HYPRE *)pc->data;
  PetscInt    bs, n, indx, level;
  PetscBool   flg, tmp_truth;
  double      tmpdbl, twodbl[2];
  const char *symtlist[]           = {"nonsymmetric", "SPD", "nonsymmetric,SPD"};
  const char *PCHYPRESpgemmTypes[] = {"cusparse", "hypre"};

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "HYPRE BoomerAMG Options");
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_cycle_type", "Cycle type", "None", HYPREBoomerAMGCycleType + 1, 2, HYPREBoomerAMGCycleType[jac->cycletype], &indx, &flg));
  if (flg) {
    jac->cycletype = indx + 1;
    PetscCallExternal(HYPRE_BoomerAMGSetCycleType, jac->hsolver, jac->cycletype);
  }
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_max_levels", "Number of levels (of grids) allowed", "None", jac->maxlevels, &jac->maxlevels, &flg));
  if (flg) {
    PetscCheck(jac->maxlevels >= 2, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Number of levels %" PetscInt_FMT " must be at least two", jac->maxlevels);
    PetscCallExternal(HYPRE_BoomerAMGSetMaxLevels, jac->hsolver, jac->maxlevels);
  }
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_max_iter", "Maximum iterations used PER hypre call", "None", jac->maxiter, &jac->maxiter, &flg));
  if (flg) {
    PetscCheck(jac->maxiter >= 1, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Number of iterations %" PetscInt_FMT " must be at least one", jac->maxiter);
    PetscCallExternal(HYPRE_BoomerAMGSetMaxIter, jac->hsolver, jac->maxiter);
  }
  PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_tol", "Convergence tolerance PER hypre call (0.0 = use a fixed number of iterations)", "None", jac->tol, &jac->tol, &flg));
  if (flg) {
    PetscCheck(jac->tol >= 0.0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Tolerance %g must be greater than or equal to zero", (double)jac->tol);
    PetscCallExternal(HYPRE_BoomerAMGSetTol, jac->hsolver, jac->tol);
  }
  bs = 1;
  if (pc->pmat) PetscCall(MatGetBlockSize(pc->pmat, &bs));
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_numfunctions", "Number of functions", "HYPRE_BoomerAMGSetNumFunctions", bs, &bs, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetNumFunctions, jac->hsolver, bs);

  PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_truncfactor", "Truncation factor for interpolation (0=no truncation)", "None", jac->truncfactor, &jac->truncfactor, &flg));
  if (flg) {
    PetscCheck(jac->truncfactor >= 0.0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Truncation factor %g must be great than or equal zero", (double)jac->truncfactor);
    PetscCallExternal(HYPRE_BoomerAMGSetTruncFactor, jac->hsolver, jac->truncfactor);
  }

  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_P_max", "Max elements per row for interpolation operator (0=unlimited)", "None", jac->pmax, &jac->pmax, &flg));
  if (flg) {
    PetscCheck(jac->pmax >= 0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "P_max %" PetscInt_FMT " must be greater than or equal to zero", jac->pmax);
    PetscCallExternal(HYPRE_BoomerAMGSetPMaxElmts, jac->hsolver, jac->pmax);
  }

  PetscCall(PetscOptionsRangeInt("-pc_hypre_boomeramg_agg_nl", "Number of levels of aggressive coarsening", "None", jac->agg_nl, &jac->agg_nl, &flg, 0, jac->maxlevels));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetAggNumLevels, jac->hsolver, jac->agg_nl);

  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_agg_num_paths", "Number of paths for aggressive coarsening", "None", jac->agg_num_paths, &jac->agg_num_paths, &flg));
  if (flg) {
    PetscCheck(jac->agg_num_paths >= 1, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Number of paths %" PetscInt_FMT " must be greater than or equal to 1", jac->agg_num_paths);
    PetscCallExternal(HYPRE_BoomerAMGSetNumPaths, jac->hsolver, jac->agg_num_paths);
  }

  PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_strong_threshold", "Threshold for being strongly connected", "None", jac->strongthreshold, &jac->strongthreshold, &flg));
  if (flg) {
    PetscCheck(jac->strongthreshold >= 0.0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Strong threshold %g must be great than or equal zero", (double)jac->strongthreshold);
    PetscCallExternal(HYPRE_BoomerAMGSetStrongThreshold, jac->hsolver, jac->strongthreshold);
  }
  PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_max_row_sum", "Maximum row sum", "None", jac->maxrowsum, &jac->maxrowsum, &flg));
  if (flg) {
    PetscCheck(jac->maxrowsum >= 0.0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Maximum row sum %g must be greater than zero", (double)jac->maxrowsum);
    PetscCheck(jac->maxrowsum <= 1.0, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Maximum row sum %g must be less than or equal one", (double)jac->maxrowsum);
    PetscCallExternal(HYPRE_BoomerAMGSetMaxRowSum, jac->hsolver, jac->maxrowsum);
  }

  /* Grid sweeps */
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_grid_sweeps_all", "Number of sweeps for the up and down grid levels", "None", jac->gridsweeps[0], &indx, &flg));
  if (flg) {
    PetscCallExternal(HYPRE_BoomerAMGSetNumSweeps, jac->hsolver, indx);
    /* modify the jac structure so we can view the updated options with PC_View */
    jac->gridsweeps[0] = indx;
    jac->gridsweeps[1] = indx;
    /*defaults coarse to 1 */
    jac->gridsweeps[2] = 1;
  }
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_nodal_coarsen", "Use a nodal based coarsening 1-6", "HYPRE_BoomerAMGSetNodal", jac->nodal_coarsening, &jac->nodal_coarsening, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetNodal, jac->hsolver, jac->nodal_coarsening);
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_nodal_coarsen_diag", "Diagonal in strength matrix for nodal based coarsening 0-2", "HYPRE_BoomerAMGSetNodalDiag", jac->nodal_coarsening_diag, &jac->nodal_coarsening_diag, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetNodalDiag, jac->hsolver, jac->nodal_coarsening_diag);
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_vec_interp_variant", "Variant of algorithm 1-3", "HYPRE_BoomerAMGSetInterpVecVariant", jac->vec_interp_variant, &jac->vec_interp_variant, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetInterpVecVariant, jac->hsolver, jac->vec_interp_variant);
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_vec_interp_qmax", "Max elements per row for each Q", "HYPRE_BoomerAMGSetInterpVecQMax", jac->vec_interp_qmax, &jac->vec_interp_qmax, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetInterpVecQMax, jac->hsolver, jac->vec_interp_qmax);
  PetscCall(PetscOptionsBool("-pc_hypre_boomeramg_vec_interp_smooth", "Whether to smooth the interpolation vectors", "HYPRE_BoomerAMGSetSmoothInterpVectors", jac->vec_interp_smooth, &jac->vec_interp_smooth, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetSmoothInterpVectors, jac->hsolver, jac->vec_interp_smooth);
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_interp_refine", "Preprocess the interpolation matrix through iterative weight refinement", "HYPRE_BoomerAMGSetInterpRefine", jac->interp_refine, &jac->interp_refine, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetInterpRefine, jac->hsolver, jac->interp_refine);
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_grid_sweeps_down", "Number of sweeps for the down cycles", "None", jac->gridsweeps[0], &indx, &flg));
  if (flg) {
    PetscCallExternal(HYPRE_BoomerAMGSetCycleNumSweeps, jac->hsolver, indx, 1);
    jac->gridsweeps[0] = indx;
  }
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_grid_sweeps_up", "Number of sweeps for the up cycles", "None", jac->gridsweeps[1], &indx, &flg));
  if (flg) {
    PetscCallExternal(HYPRE_BoomerAMGSetCycleNumSweeps, jac->hsolver, indx, 2);
    jac->gridsweeps[1] = indx;
  }
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_grid_sweeps_coarse", "Number of sweeps for the coarse level", "None", jac->gridsweeps[2], &indx, &flg));
  if (flg) {
    PetscCallExternal(HYPRE_BoomerAMGSetCycleNumSweeps, jac->hsolver, indx, 3);
    jac->gridsweeps[2] = indx;
  }

  /* Smooth type */
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_smooth_type", "Enable more complex smoothers", "None", HYPREBoomerAMGSmoothType, PETSC_STATIC_ARRAY_LENGTH(HYPREBoomerAMGSmoothType), HYPREBoomerAMGSmoothType[0], &indx, &flg));
  if (flg) {
    jac->smoothtype = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetSmoothType, jac->hsolver, indx + 6);
    jac->smoothnumlevels = 25;
    PetscCallExternal(HYPRE_BoomerAMGSetSmoothNumLevels, jac->hsolver, 25);
  }

  /* Number of smoothing levels */
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_smooth_num_levels", "Number of levels on which more complex smoothers are used", "None", 25, &indx, &flg));
  if (flg && (jac->smoothtype != -1)) {
    jac->smoothnumlevels = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetSmoothNumLevels, jac->hsolver, indx);
  }

  /* Number of levels for ILU(k) for Euclid */
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_eu_level", "Number of levels for ILU(k) in Euclid smoother", "None", 0, &indx, &flg));
  if (flg && (jac->smoothtype == 3)) {
    jac->eu_level = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetEuLevel, jac->hsolver, indx);
  }

  /* Filter for ILU(k) for Euclid */
  double droptolerance;
  PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_eu_droptolerance", "Drop tolerance for ILU(k) in Euclid smoother", "None", 0, &droptolerance, &flg));
  if (flg && (jac->smoothtype == 3)) {
    jac->eu_droptolerance = droptolerance;
    PetscCallExternal(HYPRE_BoomerAMGSetEuLevel, jac->hsolver, droptolerance);
  }

  /* Use Block Jacobi ILUT for Euclid */
  PetscCall(PetscOptionsBool("-pc_hypre_boomeramg_eu_bj", "Use Block Jacobi for ILU in Euclid smoother?", "None", PETSC_FALSE, &tmp_truth, &flg));
  if (flg && (jac->smoothtype == 3)) {
    jac->eu_bj = tmp_truth;
    PetscCallExternal(HYPRE_BoomerAMGSetEuBJ, jac->hsolver, jac->eu_bj);
  }

  /* Relax type */
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_relax_type_all", "Relax type for the up and down cycles", "None", HYPREBoomerAMGRelaxType, PETSC_STATIC_ARRAY_LENGTH(HYPREBoomerAMGRelaxType), HYPREBoomerAMGRelaxType[6], &indx, &flg));
  if (flg) {
    jac->relaxtype[0] = jac->relaxtype[1] = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetRelaxType, jac->hsolver, indx);
    /* by default, coarse type set to 9 */
    jac->relaxtype[2] = 9;
    PetscCallExternal(HYPRE_BoomerAMGSetCycleRelaxType, jac->hsolver, 9, 3);
  }
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_relax_type_down", "Relax type for the down cycles", "None", HYPREBoomerAMGRelaxType, PETSC_STATIC_ARRAY_LENGTH(HYPREBoomerAMGRelaxType), HYPREBoomerAMGRelaxType[6], &indx, &flg));
  if (flg) {
    jac->relaxtype[0] = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetCycleRelaxType, jac->hsolver, indx, 1);
  }
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_relax_type_up", "Relax type for the up cycles", "None", HYPREBoomerAMGRelaxType, PETSC_STATIC_ARRAY_LENGTH(HYPREBoomerAMGRelaxType), HYPREBoomerAMGRelaxType[6], &indx, &flg));
  if (flg) {
    jac->relaxtype[1] = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetCycleRelaxType, jac->hsolver, indx, 2);
  }
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_relax_type_coarse", "Relax type on coarse grid", "None", HYPREBoomerAMGRelaxType, PETSC_STATIC_ARRAY_LENGTH(HYPREBoomerAMGRelaxType), HYPREBoomerAMGRelaxType[9], &indx, &flg));
  if (flg) {
    jac->relaxtype[2] = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetCycleRelaxType, jac->hsolver, indx, 3);
  }

  /* Relaxation Weight */
  PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_relax_weight_all", "Relaxation weight for all levels (0 = hypre estimates, -k = determined with k CG steps)", "None", jac->relaxweight, &tmpdbl, &flg));
  if (flg) {
    PetscCallExternal(HYPRE_BoomerAMGSetRelaxWt, jac->hsolver, tmpdbl);
    jac->relaxweight = tmpdbl;
  }

  n         = 2;
  twodbl[0] = twodbl[1] = 1.0;
  PetscCall(PetscOptionsRealArray("-pc_hypre_boomeramg_relax_weight_level", "Set the relaxation weight for a particular level (weight,level)", "None", twodbl, &n, &flg));
  if (flg) {
    PetscCheck(n == 2, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Relax weight level: you must provide 2 values separated by a comma (and no space), you provided %" PetscInt_FMT, n);
    indx = (int)PetscAbsReal(twodbl[1]);
    PetscCallExternal(HYPRE_BoomerAMGSetLevelRelaxWt, jac->hsolver, twodbl[0], indx);
  }

  /* Outer relaxation Weight */
  PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_outer_relax_weight_all", "Outer relaxation weight for all levels (-k = determined with k CG steps)", "None", jac->outerrelaxweight, &tmpdbl, &flg));
  if (flg) {
    PetscCallExternal(HYPRE_BoomerAMGSetOuterWt, jac->hsolver, tmpdbl);
    jac->outerrelaxweight = tmpdbl;
  }

  n         = 2;
  twodbl[0] = twodbl[1] = 1.0;
  PetscCall(PetscOptionsRealArray("-pc_hypre_boomeramg_outer_relax_weight_level", "Set the outer relaxation weight for a particular level (weight,level)", "None", twodbl, &n, &flg));
  if (flg) {
    PetscCheck(n == 2, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_OUTOFRANGE, "Relax weight outer level: You must provide 2 values separated by a comma (and no space), you provided %" PetscInt_FMT, n);
    indx = (int)PetscAbsReal(twodbl[1]);
    PetscCallExternal(HYPRE_BoomerAMGSetLevelOuterWt, jac->hsolver, twodbl[0], indx);
  }

  /* the Relax Order */
  PetscCall(PetscOptionsBool("-pc_hypre_boomeramg_no_CF", "Do not use CF-relaxation", "None", PETSC_FALSE, &tmp_truth, &flg));

  if (flg && tmp_truth) {
    jac->relaxorder = 0;
    PetscCallExternal(HYPRE_BoomerAMGSetRelaxOrder, jac->hsolver, jac->relaxorder);
  }
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_measure_type", "Measure type", "None", HYPREBoomerAMGMeasureType, PETSC_STATIC_ARRAY_LENGTH(HYPREBoomerAMGMeasureType), HYPREBoomerAMGMeasureType[0], &indx, &flg));
  if (flg) {
    jac->measuretype = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetMeasureType, jac->hsolver, jac->measuretype);
  }
  /* update list length 3/07 */
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_coarsen_type", "Coarsen type", "None", HYPREBoomerAMGCoarsenType, PETSC_STATIC_ARRAY_LENGTH(HYPREBoomerAMGCoarsenType), HYPREBoomerAMGCoarsenType[6], &indx, &flg));
  if (flg) {
    jac->coarsentype = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetCoarsenType, jac->hsolver, jac->coarsentype);
  }

  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_max_coarse_size", "Maximum size of coarsest grid", "None", jac->maxc, &jac->maxc, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetMaxCoarseSize, jac->hsolver, jac->maxc);
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_min_coarse_size", "Minimum size of coarsest grid", "None", jac->minc, &jac->minc, &flg));
  if (flg) PetscCallExternal(HYPRE_BoomerAMGSetMinCoarseSize, jac->hsolver, jac->minc);
#if PETSC_PKG_HYPRE_VERSION_GE(2, 23, 0)
  // global parameter but is closely associated with BoomerAMG
  PetscCall(PetscOptionsEList("-pc_mg_galerkin_mat_product_algorithm", "Type of SpGEMM to use in hypre (only for now)", "PCMGGalerkinSetMatProductAlgorithm", PCHYPRESpgemmTypes, PETSC_STATIC_ARRAY_LENGTH(PCHYPRESpgemmTypes), PCHYPRESpgemmTypes[0], &indx, &flg));
  if (!flg) indx = 0;
  PetscCall(PCMGGalerkinSetMatProductAlgorithm_HYPRE_BoomerAMG(pc, PCHYPRESpgemmTypes[indx]));
#endif
  /* AIR */
#if PETSC_PKG_HYPRE_VERSION_GE(2, 18, 0)
  PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_restriction_type", "Type of AIR method (distance 1 or 2, 0 means no AIR)", "None", jac->Rtype, &jac->Rtype, NULL));
  PetscCallExternal(HYPRE_BoomerAMGSetRestriction, jac->hsolver, jac->Rtype);
  if (jac->Rtype) {
    jac->interptype = 100; /* no way we can pass this with strings... Set it as default as in MFEM, then users can still customize it back to a different one */

    PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_strongthresholdR", "Threshold for R", "None", jac->Rstrongthreshold, &jac->Rstrongthreshold, NULL));
    PetscCallExternal(HYPRE_BoomerAMGSetStrongThresholdR, jac->hsolver, jac->Rstrongthreshold);

    PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_filterthresholdR", "Filter threshold for R", "None", jac->Rfilterthreshold, &jac->Rfilterthreshold, NULL));
    PetscCallExternal(HYPRE_BoomerAMGSetFilterThresholdR, jac->hsolver, jac->Rfilterthreshold);

    PetscCall(PetscOptionsReal("-pc_hypre_boomeramg_Adroptol", "Defines the drop tolerance for the A-matrices from the 2nd level of AMG", "None", jac->Adroptol, &jac->Adroptol, NULL));
    PetscCallExternal(HYPRE_BoomerAMGSetADropTol, jac->hsolver, jac->Adroptol);

    PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_Adroptype", "Drops the entries that are not on the diagonal and smaller than its row norm: type 1: 1-norm, 2: 2-norm, -1: infinity norm", "None", jac->Adroptype, &jac->Adroptype, NULL));
    PetscCallExternal(HYPRE_BoomerAMGSetADropType, jac->hsolver, jac->Adroptype);
  }
#endif

#if PETSC_PKG_HYPRE_VERSION_LE(9, 9, 9)
  PetscCheck(!jac->Rtype || !jac->agg_nl, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_INCOMP, "-pc_hypre_boomeramg_restriction_type (%" PetscInt_FMT ") and -pc_hypre_boomeramg_agg_nl (%" PetscInt_FMT ")", jac->Rtype, jac->agg_nl);
#endif

  /* new 3/07 */
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_interp_type", "Interpolation type", "None", HYPREBoomerAMGInterpType, PETSC_STATIC_ARRAY_LENGTH(HYPREBoomerAMGInterpType), HYPREBoomerAMGInterpType[0], &indx, &flg));
  if (flg || jac->Rtype) {
    if (flg) jac->interptype = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetInterpType, jac->hsolver, jac->interptype);
  }

  PetscCall(PetscOptionsName("-pc_hypre_boomeramg_print_statistics", "Print statistics", "None", &flg));
  if (flg) {
    level = 3;
    PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_print_statistics", "Print statistics", "None", level, &level, NULL));

    jac->printstatistics = PETSC_TRUE;
    PetscCallExternal(HYPRE_BoomerAMGSetPrintLevel, jac->hsolver, level);
  }

  PetscCall(PetscOptionsName("-pc_hypre_boomeramg_print_debug", "Print debug information", "None", &flg));
  if (flg) {
    level = 3;
    PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_print_debug", "Print debug information", "None", level, &level, NULL));

    jac->printstatistics = PETSC_TRUE;
    PetscCallExternal(HYPRE_BoomerAMGSetDebugFlag, jac->hsolver, level);
  }

  PetscCall(PetscOptionsBool("-pc_hypre_boomeramg_nodal_relaxation", "Nodal relaxation via Schwarz", "None", PETSC_FALSE, &tmp_truth, &flg));
  if (flg && tmp_truth) {
    PetscInt tmp_int;
    PetscCall(PetscOptionsInt("-pc_hypre_boomeramg_nodal_relaxation", "Nodal relaxation via Schwarz", "None", jac->nodal_relax_levels, &tmp_int, &flg));
    if (flg) jac->nodal_relax_levels = tmp_int;
    PetscCallExternal(HYPRE_BoomerAMGSetSmoothType, jac->hsolver, 6);
    PetscCallExternal(HYPRE_BoomerAMGSetDomainType, jac->hsolver, 1);
    PetscCallExternal(HYPRE_BoomerAMGSetOverlap, jac->hsolver, 0);
    PetscCallExternal(HYPRE_BoomerAMGSetSmoothNumLevels, jac->hsolver, jac->nodal_relax_levels);
  }

  PetscCall(PetscOptionsBool("-pc_hypre_boomeramg_keeptranspose", "Avoid transpose matvecs in preconditioner application", "None", jac->keeptranspose, &jac->keeptranspose, NULL));
  PetscCallExternal(HYPRE_BoomerAMGSetKeepTranspose, jac->hsolver, jac->keeptranspose ? 1 : 0);

  /* options for ParaSails solvers */
  PetscCall(PetscOptionsEList("-pc_hypre_boomeramg_parasails_sym", "Symmetry of matrix and preconditioner", "None", symtlist, PETSC_STATIC_ARRAY_LENGTH(symtlist), symtlist[0], &indx, &flg));
  if (flg) {
    jac->symt = indx;
    PetscCallExternal(HYPRE_BoomerAMGSetSym, jac->hsolver, jac->symt);
  }

  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCApplyRichardson_HYPRE_BoomerAMG(PC pc, Vec b, Vec y, Vec w, PetscReal rtol, PetscReal abstol, PetscReal dtol, PetscInt its, PetscBool guesszero, PetscInt *outits, PCRichardsonConvergedReason *reason)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  HYPRE_Int oits;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  PetscCallExternal(HYPRE_BoomerAMGSetMaxIter, jac->hsolver, its * jac->maxiter);
  PetscCallExternal(HYPRE_BoomerAMGSetTol, jac->hsolver, rtol);
  jac->applyrichardson = PETSC_TRUE;
  PetscCall(PCApply_HYPRE(pc, b, y));
  jac->applyrichardson = PETSC_FALSE;
  PetscCallExternal(HYPRE_BoomerAMGGetNumIterations, jac->hsolver, &oits);
  *outits = oits;
  if (oits == its) *reason = PCRICHARDSON_CONVERGED_ITS;
  else *reason = PCRICHARDSON_CONVERGED_RTOL;
  PetscCallExternal(HYPRE_BoomerAMGSetTol, jac->hsolver, jac->tol);
  PetscCallExternal(HYPRE_BoomerAMGSetMaxIter, jac->hsolver, jac->maxiter);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCView_HYPRE_BoomerAMG(PC pc, PetscViewer viewer)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE BoomerAMG preconditioning\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Cycle type %s\n", HYPREBoomerAMGCycleType[jac->cycletype]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Maximum number of levels %" PetscInt_FMT "\n", jac->maxlevels));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Maximum number of iterations PER hypre call %" PetscInt_FMT "\n", jac->maxiter));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Convergence tolerance PER hypre call %g\n", (double)jac->tol));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Threshold for strong coupling %g\n", (double)jac->strongthreshold));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Interpolation truncation factor %g\n", (double)jac->truncfactor));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Interpolation: max elements per row %" PetscInt_FMT "\n", jac->pmax));
    if (jac->interp_refine) PetscCall(PetscViewerASCIIPrintf(viewer, "    Interpolation: number of steps of weighted refinement %" PetscInt_FMT "\n", jac->interp_refine));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Number of levels of aggressive coarsening %" PetscInt_FMT "\n", jac->agg_nl));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Number of paths for aggressive coarsening %" PetscInt_FMT "\n", jac->agg_num_paths));

    PetscCall(PetscViewerASCIIPrintf(viewer, "    Maximum row sums %g\n", (double)jac->maxrowsum));

    PetscCall(PetscViewerASCIIPrintf(viewer, "    Sweeps down         %" PetscInt_FMT "\n", jac->gridsweeps[0]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Sweeps up           %" PetscInt_FMT "\n", jac->gridsweeps[1]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Sweeps on coarse    %" PetscInt_FMT "\n", jac->gridsweeps[2]));

    PetscCall(PetscViewerASCIIPrintf(viewer, "    Relax down          %s\n", HYPREBoomerAMGRelaxType[jac->relaxtype[0]]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Relax up            %s\n", HYPREBoomerAMGRelaxType[jac->relaxtype[1]]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Relax on coarse     %s\n", HYPREBoomerAMGRelaxType[jac->relaxtype[2]]));

    PetscCall(PetscViewerASCIIPrintf(viewer, "    Relax weight  (all)      %g\n", (double)jac->relaxweight));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Outer relax weight (all) %g\n", (double)jac->outerrelaxweight));

    if (jac->relaxorder) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Using CF-relaxation\n"));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Not using CF-relaxation\n"));
    }
    if (jac->smoothtype != -1) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Smooth type          %s\n", HYPREBoomerAMGSmoothType[jac->smoothtype]));
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Smooth num levels    %" PetscInt_FMT "\n", jac->smoothnumlevels));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Not using more complex smoothers.\n"));
    }
    if (jac->smoothtype == 3) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Euclid ILU(k) levels %" PetscInt_FMT "\n", jac->eu_level));
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Euclid ILU(k) drop tolerance %g\n", (double)jac->eu_droptolerance));
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Euclid ILU use Block-Jacobi? %" PetscInt_FMT "\n", jac->eu_bj));
    }
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Measure type        %s\n", HYPREBoomerAMGMeasureType[jac->measuretype]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Coarsen type        %s\n", HYPREBoomerAMGCoarsenType[jac->coarsentype]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    Interpolation type  %s\n", jac->interptype != 100 ? HYPREBoomerAMGInterpType[jac->interptype] : "1pt"));
    if (jac->nodal_coarsening) PetscCall(PetscViewerASCIIPrintf(viewer, "    Using nodal coarsening with HYPRE_BOOMERAMGSetNodal() %" PetscInt_FMT "\n", jac->nodal_coarsening));
    if (jac->vec_interp_variant) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    HYPRE_BoomerAMGSetInterpVecVariant() %" PetscInt_FMT "\n", jac->vec_interp_variant));
      PetscCall(PetscViewerASCIIPrintf(viewer, "    HYPRE_BoomerAMGSetInterpVecQMax() %" PetscInt_FMT "\n", jac->vec_interp_qmax));
      PetscCall(PetscViewerASCIIPrintf(viewer, "    HYPRE_BoomerAMGSetSmoothInterpVectors() %d\n", jac->vec_interp_smooth));
    }
    if (jac->nodal_relax) PetscCall(PetscViewerASCIIPrintf(viewer, "    Using nodal relaxation via Schwarz smoothing on levels %" PetscInt_FMT "\n", jac->nodal_relax_levels));
#if PETSC_PKG_HYPRE_VERSION_GE(2, 23, 0)
    PetscCall(PetscViewerASCIIPrintf(viewer, "    SpGEMM type         %s\n", jac->spgemm_type));
#endif
    /* AIR */
    if (jac->Rtype) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    Using approximate ideal restriction type %" PetscInt_FMT "\n", jac->Rtype));
      PetscCall(PetscViewerASCIIPrintf(viewer, "      Threshold for R %g\n", (double)jac->Rstrongthreshold));
      PetscCall(PetscViewerASCIIPrintf(viewer, "      Filter for R %g\n", (double)jac->Rfilterthreshold));
      PetscCall(PetscViewerASCIIPrintf(viewer, "      A drop tolerance %g\n", (double)jac->Adroptol));
      PetscCall(PetscViewerASCIIPrintf(viewer, "      A drop type %" PetscInt_FMT "\n", jac->Adroptype));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCSetFromOptions_HYPRE_ParaSails(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_HYPRE   *jac = (PC_HYPRE *)pc->data;
  PetscInt    indx;
  PetscBool   flag;
  const char *symtlist[] = {"nonsymmetric", "SPD", "nonsymmetric,SPD"};

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "HYPRE ParaSails Options");
  PetscCall(PetscOptionsInt("-pc_hypre_parasails_nlevels", "Number of number of levels", "None", jac->nlevels, &jac->nlevels, 0));
  PetscCall(PetscOptionsReal("-pc_hypre_parasails_thresh", "Threshold", "None", jac->threshold, &jac->threshold, &flag));
  if (flag) PetscCallExternal(HYPRE_ParaSailsSetParams, jac->hsolver, jac->threshold, jac->nlevels);

  PetscCall(PetscOptionsReal("-pc_hypre_parasails_filter", "filter", "None", jac->filter, &jac->filter, &flag));
  if (flag) PetscCallExternal(HYPRE_ParaSailsSetFilter, jac->hsolver, jac->filter);

  PetscCall(PetscOptionsReal("-pc_hypre_parasails_loadbal", "Load balance", "None", jac->loadbal, &jac->loadbal, &flag));
  if (flag) PetscCallExternal(HYPRE_ParaSailsSetLoadbal, jac->hsolver, jac->loadbal);

  PetscCall(PetscOptionsBool("-pc_hypre_parasails_logging", "Print info to screen", "None", (PetscBool)jac->logging, (PetscBool *)&jac->logging, &flag));
  if (flag) PetscCallExternal(HYPRE_ParaSailsSetLogging, jac->hsolver, jac->logging);

  PetscCall(PetscOptionsBool("-pc_hypre_parasails_reuse", "Reuse nonzero pattern in preconditioner", "None", (PetscBool)jac->ruse, (PetscBool *)&jac->ruse, &flag));
  if (flag) PetscCallExternal(HYPRE_ParaSailsSetReuse, jac->hsolver, jac->ruse);

  PetscCall(PetscOptionsEList("-pc_hypre_parasails_sym", "Symmetry of matrix and preconditioner", "None", symtlist, PETSC_STATIC_ARRAY_LENGTH(symtlist), symtlist[0], &indx, &flag));
  if (flag) {
    jac->symt = indx;
    PetscCallExternal(HYPRE_ParaSailsSetSym, jac->hsolver, jac->symt);
  }

  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCView_HYPRE_ParaSails(PC pc, PetscViewer viewer)
{
  PC_HYPRE   *jac = (PC_HYPRE *)pc->data;
  PetscBool   iascii;
  const char *symt = 0;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE ParaSails preconditioning\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    nlevels %" PetscInt_FMT "\n", jac->nlevels));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    threshold %g\n", (double)jac->threshold));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    filter %g\n", (double)jac->filter));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    load balance %g\n", (double)jac->loadbal));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    reuse nonzero structure %s\n", PetscBools[jac->ruse]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    print info to screen %s\n", PetscBools[jac->logging]));
    if (!jac->symt) symt = "nonsymmetric matrix and preconditioner";
    else if (jac->symt == 1) symt = "SPD matrix and preconditioner";
    else if (jac->symt == 2) symt = "nonsymmetric matrix but SPD preconditioner";
    else SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_WRONG, "Unknown HYPRE ParaSails symmetric option %" PetscInt_FMT, jac->symt);
    PetscCall(PetscViewerASCIIPrintf(viewer, "    %s\n", symt));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCSetFromOptions_HYPRE_AMS(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscInt  n;
  PetscBool flag, flag2, flag3, flag4;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "HYPRE AMS Options");
  PetscCall(PetscOptionsInt("-pc_hypre_ams_print_level", "Debugging output level for AMS", "None", jac->as_print, &jac->as_print, &flag));
  if (flag) PetscCallExternal(HYPRE_AMSSetPrintLevel, jac->hsolver, jac->as_print);
  PetscCall(PetscOptionsInt("-pc_hypre_ams_max_iter", "Maximum number of AMS multigrid iterations within PCApply", "None", jac->as_max_iter, &jac->as_max_iter, &flag));
  if (flag) PetscCallExternal(HYPRE_AMSSetMaxIter, jac->hsolver, jac->as_max_iter);
  PetscCall(PetscOptionsInt("-pc_hypre_ams_cycle_type", "Cycle type for AMS multigrid", "None", jac->ams_cycle_type, &jac->ams_cycle_type, &flag));
  if (flag) PetscCallExternal(HYPRE_AMSSetCycleType, jac->hsolver, jac->ams_cycle_type);
  PetscCall(PetscOptionsReal("-pc_hypre_ams_tol", "Error tolerance for AMS multigrid", "None", jac->as_tol, &jac->as_tol, &flag));
  if (flag) PetscCallExternal(HYPRE_AMSSetTol, jac->hsolver, jac->as_tol);
  PetscCall(PetscOptionsInt("-pc_hypre_ams_relax_type", "Relaxation type for AMS smoother", "None", jac->as_relax_type, &jac->as_relax_type, &flag));
  PetscCall(PetscOptionsInt("-pc_hypre_ams_relax_times", "Number of relaxation steps for AMS smoother", "None", jac->as_relax_times, &jac->as_relax_times, &flag2));
  PetscCall(PetscOptionsReal("-pc_hypre_ams_relax_weight", "Relaxation weight for AMS smoother", "None", jac->as_relax_weight, &jac->as_relax_weight, &flag3));
  PetscCall(PetscOptionsReal("-pc_hypre_ams_omega", "SSOR coefficient for AMS smoother", "None", jac->as_omega, &jac->as_omega, &flag4));
  if (flag || flag2 || flag3 || flag4) PetscCallExternal(HYPRE_AMSSetSmoothingOptions, jac->hsolver, jac->as_relax_type, jac->as_relax_times, jac->as_relax_weight, jac->as_omega);
  PetscCall(PetscOptionsReal("-pc_hypre_ams_amg_alpha_theta", "Threshold for strong coupling of vector Poisson AMG solver", "None", jac->as_amg_alpha_theta, &jac->as_amg_alpha_theta, &flag));
  n = 5;
  PetscCall(PetscOptionsIntArray("-pc_hypre_ams_amg_alpha_options", "AMG options for vector Poisson", "None", jac->as_amg_alpha_opts, &n, &flag2));
  if (flag || flag2) {
    PetscCallExternal(HYPRE_AMSSetAlphaAMGOptions, jac->hsolver, jac->as_amg_alpha_opts[0], /* AMG coarsen type */
                      jac->as_amg_alpha_opts[1],                                            /* AMG agg_levels */
                      jac->as_amg_alpha_opts[2],                                            /* AMG relax_type */
                      jac->as_amg_alpha_theta, jac->as_amg_alpha_opts[3],                   /* AMG interp_type */
                      jac->as_amg_alpha_opts[4]);                                           /* AMG Pmax */
  }
  PetscCall(PetscOptionsReal("-pc_hypre_ams_amg_beta_theta", "Threshold for strong coupling of scalar Poisson AMG solver", "None", jac->as_amg_beta_theta, &jac->as_amg_beta_theta, &flag));
  n = 5;
  PetscCall(PetscOptionsIntArray("-pc_hypre_ams_amg_beta_options", "AMG options for scalar Poisson solver", "None", jac->as_amg_beta_opts, &n, &flag2));
  if (flag || flag2) {
    PetscCallExternal(HYPRE_AMSSetBetaAMGOptions, jac->hsolver, jac->as_amg_beta_opts[0], /* AMG coarsen type */
                      jac->as_amg_beta_opts[1],                                           /* AMG agg_levels */
                      jac->as_amg_beta_opts[2],                                           /* AMG relax_type */
                      jac->as_amg_beta_theta, jac->as_amg_beta_opts[3],                   /* AMG interp_type */
                      jac->as_amg_beta_opts[4]);                                          /* AMG Pmax */
  }
  PetscCall(PetscOptionsInt("-pc_hypre_ams_projection_frequency", "Frequency at which a projection onto the compatible subspace for problems with zero conductivity regions is performed", "None", jac->ams_proj_freq, &jac->ams_proj_freq, &flag));
  if (flag) { /* override HYPRE's default only if the options is used */
    PetscCallExternal(HYPRE_AMSSetProjectionFrequency, jac->hsolver, jac->ams_proj_freq);
  }
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCView_HYPRE_AMS(PC pc, PetscViewer viewer)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE AMS preconditioning\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    subspace iterations per application %" PetscInt_FMT "\n", jac->as_max_iter));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    subspace cycle type %" PetscInt_FMT "\n", jac->ams_cycle_type));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    subspace iteration tolerance %g\n", (double)jac->as_tol));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    smoother type %" PetscInt_FMT "\n", jac->as_relax_type));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    number of smoothing steps %" PetscInt_FMT "\n", jac->as_relax_times));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    smoother weight %g\n", (double)jac->as_relax_weight));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    smoother omega %g\n", (double)jac->as_omega));
    if (jac->alpha_Poisson) {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    vector Poisson solver (passed in by user)\n"));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    vector Poisson solver (computed) \n"));
    }
    PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG coarsening type %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[0]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG levels of aggressive coarsening %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[1]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG relaxation type %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[2]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG interpolation type %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[3]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG max nonzero elements in interpolation rows %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[4]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG strength threshold %g\n", (double)jac->as_amg_alpha_theta));
    if (!jac->ams_beta_is_zero) {
      if (jac->beta_Poisson) {
        PetscCall(PetscViewerASCIIPrintf(viewer, "    scalar Poisson solver (passed in by user)\n"));
      } else {
        PetscCall(PetscViewerASCIIPrintf(viewer, "    scalar Poisson solver (computed) \n"));
      }
      PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG coarsening type %" PetscInt_FMT "\n", jac->as_amg_beta_opts[0]));
      PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG levels of aggressive coarsening %" PetscInt_FMT "\n", jac->as_amg_beta_opts[1]));
      PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG relaxation type %" PetscInt_FMT "\n", jac->as_amg_beta_opts[2]));
      PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG interpolation type %" PetscInt_FMT "\n", jac->as_amg_beta_opts[3]));
      PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG max nonzero elements in interpolation rows %" PetscInt_FMT "\n", jac->as_amg_beta_opts[4]));
      PetscCall(PetscViewerASCIIPrintf(viewer, "        boomerAMG strength threshold %g\n", (double)jac->as_amg_beta_theta));
      if (jac->ams_beta_is_zero_part) PetscCall(PetscViewerASCIIPrintf(viewer, "        compatible subspace projection frequency %" PetscInt_FMT " (-1 HYPRE uses default)\n", jac->ams_proj_freq));
    } else {
      PetscCall(PetscViewerASCIIPrintf(viewer, "    scalar Poisson solver not used (zero-conductivity everywhere) \n"));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCSetFromOptions_HYPRE_ADS(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscInt  n;
  PetscBool flag, flag2, flag3, flag4;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "HYPRE ADS Options");
  PetscCall(PetscOptionsInt("-pc_hypre_ads_print_level", "Debugging output level for ADS", "None", jac->as_print, &jac->as_print, &flag));
  if (flag) PetscCallExternal(HYPRE_ADSSetPrintLevel, jac->hsolver, jac->as_print);
  PetscCall(PetscOptionsInt("-pc_hypre_ads_max_iter", "Maximum number of ADS multigrid iterations within PCApply", "None", jac->as_max_iter, &jac->as_max_iter, &flag));
  if (flag) PetscCallExternal(HYPRE_ADSSetMaxIter, jac->hsolver, jac->as_max_iter);
  PetscCall(PetscOptionsInt("-pc_hypre_ads_cycle_type", "Cycle type for ADS multigrid", "None", jac->ads_cycle_type, &jac->ads_cycle_type, &flag));
  if (flag) PetscCallExternal(HYPRE_ADSSetCycleType, jac->hsolver, jac->ads_cycle_type);
  PetscCall(PetscOptionsReal("-pc_hypre_ads_tol", "Error tolerance for ADS multigrid", "None", jac->as_tol, &jac->as_tol, &flag));
  if (flag) PetscCallExternal(HYPRE_ADSSetTol, jac->hsolver, jac->as_tol);
  PetscCall(PetscOptionsInt("-pc_hypre_ads_relax_type", "Relaxation type for ADS smoother", "None", jac->as_relax_type, &jac->as_relax_type, &flag));
  PetscCall(PetscOptionsInt("-pc_hypre_ads_relax_times", "Number of relaxation steps for ADS smoother", "None", jac->as_relax_times, &jac->as_relax_times, &flag2));
  PetscCall(PetscOptionsReal("-pc_hypre_ads_relax_weight", "Relaxation weight for ADS smoother", "None", jac->as_relax_weight, &jac->as_relax_weight, &flag3));
  PetscCall(PetscOptionsReal("-pc_hypre_ads_omega", "SSOR coefficient for ADS smoother", "None", jac->as_omega, &jac->as_omega, &flag4));
  if (flag || flag2 || flag3 || flag4) PetscCallExternal(HYPRE_ADSSetSmoothingOptions, jac->hsolver, jac->as_relax_type, jac->as_relax_times, jac->as_relax_weight, jac->as_omega);
  PetscCall(PetscOptionsReal("-pc_hypre_ads_ams_theta", "Threshold for strong coupling of AMS solver inside ADS", "None", jac->as_amg_alpha_theta, &jac->as_amg_alpha_theta, &flag));
  n = 5;
  PetscCall(PetscOptionsIntArray("-pc_hypre_ads_ams_options", "AMG options for AMS solver inside ADS", "None", jac->as_amg_alpha_opts, &n, &flag2));
  PetscCall(PetscOptionsInt("-pc_hypre_ads_ams_cycle_type", "Cycle type for AMS solver inside ADS", "None", jac->ams_cycle_type, &jac->ams_cycle_type, &flag3));
  if (flag || flag2 || flag3) {
    PetscCallExternal(HYPRE_ADSSetAMSOptions, jac->hsolver, jac->ams_cycle_type, /* AMS cycle type */
                      jac->as_amg_alpha_opts[0],                                 /* AMG coarsen type */
                      jac->as_amg_alpha_opts[1],                                 /* AMG agg_levels */
                      jac->as_amg_alpha_opts[2],                                 /* AMG relax_type */
                      jac->as_amg_alpha_theta, jac->as_amg_alpha_opts[3],        /* AMG interp_type */
                      jac->as_amg_alpha_opts[4]);                                /* AMG Pmax */
  }
  PetscCall(PetscOptionsReal("-pc_hypre_ads_amg_theta", "Threshold for strong coupling of vector AMG solver inside ADS", "None", jac->as_amg_beta_theta, &jac->as_amg_beta_theta, &flag));
  n = 5;
  PetscCall(PetscOptionsIntArray("-pc_hypre_ads_amg_options", "AMG options for vector AMG solver inside ADS", "None", jac->as_amg_beta_opts, &n, &flag2));
  if (flag || flag2) {
    PetscCallExternal(HYPRE_ADSSetAMGOptions, jac->hsolver, jac->as_amg_beta_opts[0], /* AMG coarsen type */
                      jac->as_amg_beta_opts[1],                                       /* AMG agg_levels */
                      jac->as_amg_beta_opts[2],                                       /* AMG relax_type */
                      jac->as_amg_beta_theta, jac->as_amg_beta_opts[3],               /* AMG interp_type */
                      jac->as_amg_beta_opts[4]);                                      /* AMG Pmax */
  }
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCView_HYPRE_ADS(PC pc, PetscViewer viewer)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE ADS preconditioning\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    subspace iterations per application %" PetscInt_FMT "\n", jac->as_max_iter));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    subspace cycle type %" PetscInt_FMT "\n", jac->ads_cycle_type));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    subspace iteration tolerance %g\n", (double)jac->as_tol));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    smoother type %" PetscInt_FMT "\n", jac->as_relax_type));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    number of smoothing steps %" PetscInt_FMT "\n", jac->as_relax_times));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    smoother weight %g\n", (double)jac->as_relax_weight));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    smoother omega %g\n", (double)jac->as_omega));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    AMS solver using boomerAMG\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        subspace cycle type %" PetscInt_FMT "\n", jac->ams_cycle_type));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        coarsening type %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[0]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        levels of aggressive coarsening %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[1]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        relaxation type %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[2]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        interpolation type %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[3]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        max nonzero elements in interpolation rows %" PetscInt_FMT "\n", jac->as_amg_alpha_opts[4]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        strength threshold %g\n", (double)jac->as_amg_alpha_theta));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    vector Poisson solver using boomerAMG\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        coarsening type %" PetscInt_FMT "\n", jac->as_amg_beta_opts[0]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        levels of aggressive coarsening %" PetscInt_FMT "\n", jac->as_amg_beta_opts[1]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        relaxation type %" PetscInt_FMT "\n", jac->as_amg_beta_opts[2]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        interpolation type %" PetscInt_FMT "\n", jac->as_amg_beta_opts[3]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        max nonzero elements in interpolation rows %" PetscInt_FMT "\n", jac->as_amg_beta_opts[4]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "        strength threshold %g\n", (double)jac->as_amg_beta_theta));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCHYPRESetDiscreteGradient_HYPRE(PC pc, Mat G)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool ishypre;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)G, MATHYPRE, &ishypre));
  if (ishypre) {
    PetscCall(PetscObjectReference((PetscObject)G));
    PetscCall(MatDestroy(&jac->G));
    jac->G = G;
  } else {
    PetscCall(MatDestroy(&jac->G));
    PetscCall(MatConvert(G, MATHYPRE, MAT_INITIAL_MATRIX, &jac->G));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCHYPRESetDiscreteGradient - Set discrete gradient matrix for `PCHYPRE` type of ams or ads

   Collective

   Input Parameters:
+  pc - the preconditioning context
-  G - the discrete gradient

   Level: intermediate

   Notes:
    G should have as many rows as the number of edges and as many columns as the number of vertices in the mesh

    Each row of G has 2 nonzeros, with column indexes being the global indexes of edge's endpoints: matrix entries are +1 and -1 depending on edge orientation

   Developer Note:
   This automatically converts the matrix to `MATHYPRE` if it is not already of that type

.seealso: `PCHYPRE`, `PCHYPRESetDiscreteCurl()`
@*/
PetscErrorCode PCHYPRESetDiscreteGradient(PC pc, Mat G)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidHeaderSpecific(G, MAT_CLASSID, 2);
  PetscCheckSameComm(pc, 1, G, 2);
  PetscTryMethod(pc, "PCHYPRESetDiscreteGradient_C", (PC, Mat), (pc, G));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCHYPRESetDiscreteCurl_HYPRE(PC pc, Mat C)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool ishypre;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)C, MATHYPRE, &ishypre));
  if (ishypre) {
    PetscCall(PetscObjectReference((PetscObject)C));
    PetscCall(MatDestroy(&jac->C));
    jac->C = C;
  } else {
    PetscCall(MatDestroy(&jac->C));
    PetscCall(MatConvert(C, MATHYPRE, MAT_INITIAL_MATRIX, &jac->C));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCHYPRESetDiscreteCurl - Set discrete curl matrx for `PCHYPRE` type of ads

   Collective

   Input Parameters:
+  pc - the preconditioning context
-  C - the discrete curl

   Level: intermediate

   Notes:
    C should have as many rows as the number of faces and as many columns as the number of edges in the mesh

    Each row of G has as many nonzeros as the number of edges of a face, with column indexes being the global indexes of the corresponding edge: matrix entries are +1 and -1 depending on edge orientation with respect to the face orientation

   Developer Note:
   This automatically converts the matrix to `MATHYPRE` if it is not already of that type

   If this is only for  `PCHYPRE` type of ads it should be called `PCHYPREADSSetDiscreteCurl()`

.seealso: `PCHYPRE`, `PCHYPRESetDiscreteGradient()`
@*/
PetscErrorCode PCHYPRESetDiscreteCurl(PC pc, Mat C)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidHeaderSpecific(C, MAT_CLASSID, 2);
  PetscCheckSameComm(pc, 1, C, 2);
  PetscTryMethod(pc, "PCHYPRESetDiscreteCurl_C", (PC, Mat), (pc, C));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCHYPRESetInterpolations_HYPRE(PC pc, PetscInt dim, Mat RT_PiFull, Mat RT_Pi[], Mat ND_PiFull, Mat ND_Pi[])
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool ishypre;
  PetscInt  i;
  PetscFunctionBegin;

  PetscCall(MatDestroy(&jac->RT_PiFull));
  PetscCall(MatDestroy(&jac->ND_PiFull));
  for (i = 0; i < 3; ++i) {
    PetscCall(MatDestroy(&jac->RT_Pi[i]));
    PetscCall(MatDestroy(&jac->ND_Pi[i]));
  }

  jac->dim = dim;
  if (RT_PiFull) {
    PetscCall(PetscObjectTypeCompare((PetscObject)RT_PiFull, MATHYPRE, &ishypre));
    if (ishypre) {
      PetscCall(PetscObjectReference((PetscObject)RT_PiFull));
      jac->RT_PiFull = RT_PiFull;
    } else {
      PetscCall(MatConvert(RT_PiFull, MATHYPRE, MAT_INITIAL_MATRIX, &jac->RT_PiFull));
    }
  }
  if (RT_Pi) {
    for (i = 0; i < dim; ++i) {
      if (RT_Pi[i]) {
        PetscCall(PetscObjectTypeCompare((PetscObject)RT_Pi[i], MATHYPRE, &ishypre));
        if (ishypre) {
          PetscCall(PetscObjectReference((PetscObject)RT_Pi[i]));
          jac->RT_Pi[i] = RT_Pi[i];
        } else {
          PetscCall(MatConvert(RT_Pi[i], MATHYPRE, MAT_INITIAL_MATRIX, &jac->RT_Pi[i]));
        }
      }
    }
  }
  if (ND_PiFull) {
    PetscCall(PetscObjectTypeCompare((PetscObject)ND_PiFull, MATHYPRE, &ishypre));
    if (ishypre) {
      PetscCall(PetscObjectReference((PetscObject)ND_PiFull));
      jac->ND_PiFull = ND_PiFull;
    } else {
      PetscCall(MatConvert(ND_PiFull, MATHYPRE, MAT_INITIAL_MATRIX, &jac->ND_PiFull));
    }
  }
  if (ND_Pi) {
    for (i = 0; i < dim; ++i) {
      if (ND_Pi[i]) {
        PetscCall(PetscObjectTypeCompare((PetscObject)ND_Pi[i], MATHYPRE, &ishypre));
        if (ishypre) {
          PetscCall(PetscObjectReference((PetscObject)ND_Pi[i]));
          jac->ND_Pi[i] = ND_Pi[i];
        } else {
          PetscCall(MatConvert(ND_Pi[i], MATHYPRE, MAT_INITIAL_MATRIX, &jac->ND_Pi[i]));
        }
      }
    }
  }

  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCHYPRESetInterpolations - Set interpolation matrices for `PCHYPRE` type of ams or ads

   Collective

   Input Parameters:
+  pc - the preconditioning context
.  dim - the dimension of the problem, only used in AMS
.  RT_PiFull - Raviart-Thomas interpolation matrix
.  RT_Pi - x/y/z component of Raviart-Thomas interpolation matrix
.  ND_PiFull - Nedelec interpolation matrix
-  ND_Pi - x/y/z component of Nedelec interpolation matrix

   Level: intermediate

   Notes:
    For AMS, only Nedelec interpolation matrices are needed, the Raviart-Thomas interpolation matrices can be set to NULL.

    For ADS, both type of interpolation matrices are needed.

   Developer Note:
   This automatically converts the matrix to `MATHYPRE` if it is not already of that type

.seealso: `PCHYPRE`
@*/
PetscErrorCode PCHYPRESetInterpolations(PC pc, PetscInt dim, Mat RT_PiFull, Mat RT_Pi[], Mat ND_PiFull, Mat ND_Pi[])
{
  PetscInt i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  if (RT_PiFull) {
    PetscValidHeaderSpecific(RT_PiFull, MAT_CLASSID, 3);
    PetscCheckSameComm(pc, 1, RT_PiFull, 3);
  }
  if (RT_Pi) {
    PetscValidPointer(RT_Pi, 4);
    for (i = 0; i < dim; ++i) {
      if (RT_Pi[i]) {
        PetscValidHeaderSpecific(RT_Pi[i], MAT_CLASSID, 4);
        PetscCheckSameComm(pc, 1, RT_Pi[i], 4);
      }
    }
  }
  if (ND_PiFull) {
    PetscValidHeaderSpecific(ND_PiFull, MAT_CLASSID, 5);
    PetscCheckSameComm(pc, 1, ND_PiFull, 5);
  }
  if (ND_Pi) {
    PetscValidPointer(ND_Pi, 6);
    for (i = 0; i < dim; ++i) {
      if (ND_Pi[i]) {
        PetscValidHeaderSpecific(ND_Pi[i], MAT_CLASSID, 6);
        PetscCheckSameComm(pc, 1, ND_Pi[i], 6);
      }
    }
  }
  PetscTryMethod(pc, "PCHYPRESetInterpolations_C", (PC, PetscInt, Mat, Mat[], Mat, Mat[]), (pc, dim, RT_PiFull, RT_Pi, ND_PiFull, ND_Pi));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCHYPRESetPoissonMatrix_HYPRE(PC pc, Mat A, PetscBool isalpha)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool ishypre;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)A, MATHYPRE, &ishypre));
  if (ishypre) {
    if (isalpha) {
      PetscCall(PetscObjectReference((PetscObject)A));
      PetscCall(MatDestroy(&jac->alpha_Poisson));
      jac->alpha_Poisson = A;
    } else {
      if (A) {
        PetscCall(PetscObjectReference((PetscObject)A));
      } else {
        jac->ams_beta_is_zero = PETSC_TRUE;
      }
      PetscCall(MatDestroy(&jac->beta_Poisson));
      jac->beta_Poisson = A;
    }
  } else {
    if (isalpha) {
      PetscCall(MatDestroy(&jac->alpha_Poisson));
      PetscCall(MatConvert(A, MATHYPRE, MAT_INITIAL_MATRIX, &jac->alpha_Poisson));
    } else {
      if (A) {
        PetscCall(MatDestroy(&jac->beta_Poisson));
        PetscCall(MatConvert(A, MATHYPRE, MAT_INITIAL_MATRIX, &jac->beta_Poisson));
      } else {
        PetscCall(MatDestroy(&jac->beta_Poisson));
        jac->ams_beta_is_zero = PETSC_TRUE;
      }
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCHYPRESetAlphaPoissonMatrix - Set vector Poisson matrix for `PCHYPRE` of type ams

   Collective

   Input Parameters:
+  pc - the preconditioning context
-  A - the matrix

   Level: intermediate

   Note:
    A should be obtained by discretizing the vector valued Poisson problem with linear finite elements

   Developer Note:
   This automatically converts the matrix to `MATHYPRE` if it is not already of that type

   If this is only for  `PCHYPRE` type of ams it should be called `PCHYPREAMSSetAlphaPoissonMatrix()`

.seealso: `PCHYPRE`, `PCHYPRESetDiscreteGradient()`, `PCHYPRESetDiscreteCurl()`, `PCHYPRESetBetaPoissonMatrix()`
@*/
PetscErrorCode PCHYPRESetAlphaPoissonMatrix(PC pc, Mat A)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidHeaderSpecific(A, MAT_CLASSID, 2);
  PetscCheckSameComm(pc, 1, A, 2);
  PetscTryMethod(pc, "PCHYPRESetPoissonMatrix_C", (PC, Mat, PetscBool), (pc, A, PETSC_TRUE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCHYPRESetBetaPoissonMatrix - Set Poisson matrix for `PCHYPRE` of type ams

   Collective

   Input Parameters:
+  pc - the preconditioning context
-  A - the matrix, or NULL to turn it off

   Level: intermediate

   Note:
   A should be obtained by discretizing the Poisson problem with linear finite elements.

   Developer Note:
   This automatically converts the matrix to `MATHYPRE` if it is not already of that type

   If this is only for  `PCHYPRE` type of ams it should be called `PCHYPREAMSPCHYPRESetBetaPoissonMatrix()`

.seealso: `PCHYPRE`, `PCHYPRESetDiscreteGradient()`, `PCHYPRESetDiscreteCurl()`, `PCHYPRESetAlphaPoissonMatrix()`
@*/
PetscErrorCode PCHYPRESetBetaPoissonMatrix(PC pc, Mat A)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  if (A) {
    PetscValidHeaderSpecific(A, MAT_CLASSID, 2);
    PetscCheckSameComm(pc, 1, A, 2);
  }
  PetscTryMethod(pc, "PCHYPRESetPoissonMatrix_C", (PC, Mat, PetscBool), (pc, A, PETSC_FALSE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCHYPRESetEdgeConstantVectors_HYPRE(PC pc, Vec ozz, Vec zoz, Vec zzo)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;

  PetscFunctionBegin;
  /* throw away any vector if already set */
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->constants[0]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->constants[1]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->constants[2]));
  PetscCall(VecHYPRE_IJVectorCreate(ozz->map, &jac->constants[0]));
  PetscCall(VecHYPRE_IJVectorCopy(ozz, jac->constants[0]));
  PetscCall(VecHYPRE_IJVectorCreate(zoz->map, &jac->constants[1]));
  PetscCall(VecHYPRE_IJVectorCopy(zoz, jac->constants[1]));
  jac->dim = 2;
  if (zzo) {
    PetscCall(VecHYPRE_IJVectorCreate(zzo->map, &jac->constants[2]));
    PetscCall(VecHYPRE_IJVectorCopy(zzo, jac->constants[2]));
    jac->dim++;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PCHYPRESetEdgeConstantVectors - Set the representation of the constant vector fields in the edge element basis for `PCHYPRE` of type ams

   Collective

   Input Parameters:
+  pc - the preconditioning context
.  ozz - vector representing (1,0,0) (or (1,0) in 2D)
.  zoz - vector representing (0,1,0) (or (0,1) in 2D)
-  zzo - vector representing (0,0,1) (use NULL in 2D)

   Level: intermediate

   Developer Note:
   If this is only for  `PCHYPRE` type of ams it should be called `PCHYPREAMSSetEdgeConstantVectors()`

.seealso: `PCHYPRE`, `PCHYPRESetDiscreteGradient()`, `PCHYPRESetDiscreteCurl()`, `PCHYPRESetAlphaPoissonMatrix()`
@*/
PetscErrorCode PCHYPRESetEdgeConstantVectors(PC pc, Vec ozz, Vec zoz, Vec zzo)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidHeaderSpecific(ozz, VEC_CLASSID, 2);
  PetscValidHeaderSpecific(zoz, VEC_CLASSID, 3);
  if (zzo) PetscValidHeaderSpecific(zzo, VEC_CLASSID, 4);
  PetscCheckSameComm(pc, 1, ozz, 2);
  PetscCheckSameComm(pc, 1, zoz, 3);
  if (zzo) PetscCheckSameComm(pc, 1, zzo, 4);
  PetscTryMethod(pc, "PCHYPRESetEdgeConstantVectors_C", (PC, Vec, Vec, Vec), (pc, ozz, zoz, zzo));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCHYPREAMSSetInteriorNodes_HYPRE(PC pc, Vec interior)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;

  PetscFunctionBegin;
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->interior));
  PetscCall(VecHYPRE_IJVectorCreate(interior->map, &jac->interior));
  PetscCall(VecHYPRE_IJVectorCopy(interior, jac->interior));
  jac->ams_beta_is_zero_part = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PCHYPREAMSSetInteriorNodes - Set the list of interior nodes to a zero-conductivity region for `PCHYPRE` of type ams

   Collective

   Input Parameters:
+  pc - the preconditioning context
-  interior - vector. node is interior if its entry in the array is 1.0.

   Level: intermediate

   Note:
   This calls `HYPRE_AMSSetInteriorNodes()`

   Developer Note:
   If this is only for  `PCHYPRE` type of ams it should be called `PCHYPREAMSSetInteriorNodes()`

.seealso: `PCHYPRE`, `PCHYPRESetDiscreteGradient()`, `PCHYPRESetDiscreteCurl()`, `PCHYPRESetAlphaPoissonMatrix()`
@*/
PetscErrorCode PCHYPREAMSSetInteriorNodes(PC pc, Vec interior)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidHeaderSpecific(interior, VEC_CLASSID, 2);
  PetscCheckSameComm(pc, 1, interior, 2);
  PetscTryMethod(pc, "PCHYPREAMSSetInteriorNodes_C", (PC, Vec), (pc, interior));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCSetCoordinates_HYPRE(PC pc, PetscInt dim, PetscInt nloc, PetscReal *coords)
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  Vec       tv;
  PetscInt  i;

  PetscFunctionBegin;
  /* throw away any coordinate vector if already set */
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->coords[0]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->coords[1]));
  PetscCall(VecHYPRE_IJVectorDestroy(&jac->coords[2]));
  jac->dim = dim;

  /* compute IJ vector for coordinates */
  PetscCall(VecCreate(PetscObjectComm((PetscObject)pc), &tv));
  PetscCall(VecSetType(tv, VECSTANDARD));
  PetscCall(VecSetSizes(tv, nloc, PETSC_DECIDE));
  for (i = 0; i < dim; i++) {
    PetscScalar *array;
    PetscInt     j;

    PetscCall(VecHYPRE_IJVectorCreate(tv->map, &jac->coords[i]));
    PetscCall(VecGetArrayWrite(tv, &array));
    for (j = 0; j < nloc; j++) array[j] = coords[j * dim + i];
    PetscCall(VecRestoreArrayWrite(tv, &array));
    PetscCall(VecHYPRE_IJVectorCopy(tv, jac->coords[i]));
  }
  PetscCall(VecDestroy(&tv));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCHYPREGetType_HYPRE(PC pc, const char *name[])
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;

  PetscFunctionBegin;
  *name = jac->hypre_type;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCHYPRESetType_HYPRE(PC pc, const char name[])
{
  PC_HYPRE *jac = (PC_HYPRE *)pc->data;
  PetscBool flag;

  PetscFunctionBegin;
  if (jac->hypre_type) {
    PetscCall(PetscStrcmp(jac->hypre_type, name, &flag));
    PetscCheck(flag, PetscObjectComm((PetscObject)pc), PETSC_ERR_ORDER, "Cannot reset the HYPRE preconditioner type once it has been set");
    PetscFunctionReturn(PETSC_SUCCESS);
  } else {
    PetscCall(PetscStrallocpy(name, &jac->hypre_type));
  }

  jac->maxiter         = PETSC_DEFAULT;
  jac->tol             = PETSC_DEFAULT;
  jac->printstatistics = PetscLogPrintInfo;

  PetscCall(PetscStrcmp("pilut", jac->hypre_type, &flag));
  if (flag) {
    PetscCall(PetscCommGetComm(PetscObjectComm((PetscObject)pc), &jac->comm_hypre));
    PetscCallExternal(HYPRE_ParCSRPilutCreate, jac->comm_hypre, &jac->hsolver);
    pc->ops->setfromoptions = PCSetFromOptions_HYPRE_Pilut;
    pc->ops->view           = PCView_HYPRE_Pilut;
    jac->destroy            = HYPRE_ParCSRPilutDestroy;
    jac->setup              = HYPRE_ParCSRPilutSetup;
    jac->solve              = HYPRE_ParCSRPilutSolve;
    jac->factorrowsize      = PETSC_DEFAULT;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscStrcmp("euclid", jac->hypre_type, &flag));
  if (flag) {
#if defined(PETSC_USE_64BIT_INDICES)
    SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_SUP, "Hypre Euclid does not support 64 bit indices");
#endif
    PetscCall(PetscCommGetComm(PetscObjectComm((PetscObject)pc), &jac->comm_hypre));
    PetscCallExternal(HYPRE_EuclidCreate, jac->comm_hypre, &jac->hsolver);
    pc->ops->setfromoptions = PCSetFromOptions_HYPRE_Euclid;
    pc->ops->view           = PCView_HYPRE_Euclid;
    jac->destroy            = HYPRE_EuclidDestroy;
    jac->setup              = HYPRE_EuclidSetup;
    jac->solve              = HYPRE_EuclidSolve;
    jac->factorrowsize      = PETSC_DEFAULT;
    jac->eu_level           = PETSC_DEFAULT; /* default */
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscStrcmp("parasails", jac->hypre_type, &flag));
  if (flag) {
    PetscCall(PetscCommGetComm(PetscObjectComm((PetscObject)pc), &jac->comm_hypre));
    PetscCallExternal(HYPRE_ParaSailsCreate, jac->comm_hypre, &jac->hsolver);
    pc->ops->setfromoptions = PCSetFromOptions_HYPRE_ParaSails;
    pc->ops->view           = PCView_HYPRE_ParaSails;
    jac->destroy            = HYPRE_ParaSailsDestroy;
    jac->setup              = HYPRE_ParaSailsSetup;
    jac->solve              = HYPRE_ParaSailsSolve;
    /* initialize */
    jac->nlevels   = 1;
    jac->threshold = .1;
    jac->filter    = .1;
    jac->loadbal   = 0;
    if (PetscLogPrintInfo) jac->logging = (int)PETSC_TRUE;
    else jac->logging = (int)PETSC_FALSE;

    jac->ruse = (int)PETSC_FALSE;
    jac->symt = 0;
    PetscCallExternal(HYPRE_ParaSailsSetParams, jac->hsolver, jac->threshold, jac->nlevels);
    PetscCallExternal(HYPRE_ParaSailsSetFilter, jac->hsolver, jac->filter);
    PetscCallExternal(HYPRE_ParaSailsSetLoadbal, jac->hsolver, jac->loadbal);
    PetscCallExternal(HYPRE_ParaSailsSetLogging, jac->hsolver, jac->logging);
    PetscCallExternal(HYPRE_ParaSailsSetReuse, jac->hsolver, jac->ruse);
    PetscCallExternal(HYPRE_ParaSailsSetSym, jac->hsolver, jac->symt);
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscStrcmp("boomeramg", jac->hypre_type, &flag));
  if (flag) {
    PetscCallExternal(HYPRE_BoomerAMGCreate, &jac->hsolver);
    pc->ops->setfromoptions  = PCSetFromOptions_HYPRE_BoomerAMG;
    pc->ops->view            = PCView_HYPRE_BoomerAMG;
    pc->ops->applytranspose  = PCApplyTranspose_HYPRE_BoomerAMG;
    pc->ops->applyrichardson = PCApplyRichardson_HYPRE_BoomerAMG;
    PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCGetInterpolations_C", PCGetInterpolations_BoomerAMG));
    PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCGetCoarseOperators_C", PCGetCoarseOperators_BoomerAMG));
    jac->destroy         = HYPRE_BoomerAMGDestroy;
    jac->setup           = HYPRE_BoomerAMGSetup;
    jac->solve           = HYPRE_BoomerAMGSolve;
    jac->applyrichardson = PETSC_FALSE;
    /* these defaults match the hypre defaults */
    jac->cycletype       = 1;
    jac->maxlevels       = 25;
    jac->maxiter         = 1;
    jac->tol             = 0.0; /* tolerance of zero indicates use as preconditioner (suppresses convergence errors) */
    jac->truncfactor     = 0.0;
    jac->strongthreshold = .25;
    jac->maxrowsum       = .9;
    jac->coarsentype     = 6;
    jac->measuretype     = 0;
    jac->gridsweeps[0] = jac->gridsweeps[1] = jac->gridsweeps[2] = 1;
    jac->smoothtype                                              = -1; /* Not set by default */
    jac->smoothnumlevels                                         = 25;
    jac->eu_level                                                = 0;
    jac->eu_droptolerance                                        = 0;
    jac->eu_bj                                                   = 0;
    jac->relaxtype[0] = jac->relaxtype[1] = 6; /* Defaults to SYMMETRIC since in PETSc we are using a PC - most likely with CG */
    jac->relaxtype[2]                     = 9; /*G.E. */
    jac->relaxweight                      = 1.0;
    jac->outerrelaxweight                 = 1.0;
    jac->relaxorder                       = 1;
    jac->interptype                       = 0;
    jac->Rtype                            = 0;
    jac->Rstrongthreshold                 = 0.25;
    jac->Rfilterthreshold                 = 0.0;
    jac->Adroptype                        = -1;
    jac->Adroptol                         = 0.0;
    jac->agg_nl                           = 0;
    jac->agg_interptype                   = 4;
    jac->pmax                             = 0;
    jac->truncfactor                      = 0.0;
    jac->agg_num_paths                    = 1;
    jac->maxc                             = 9;
    jac->minc                             = 1;
    jac->nodal_coarsening                 = 0;
    jac->nodal_coarsening_diag            = 0;
    jac->vec_interp_variant               = 0;
    jac->vec_interp_qmax                  = 0;
    jac->vec_interp_smooth                = PETSC_FALSE;
    jac->interp_refine                    = 0;
    jac->nodal_relax                      = PETSC_FALSE;
    jac->nodal_relax_levels               = 1;
    jac->rap2                             = 0;

    /* GPU defaults
         from https://hypre.readthedocs.io/en/latest/solvers-boomeramg.html#gpu-supported-options
         and /src/parcsr_ls/par_amg.c */
#if defined(PETSC_HAVE_HYPRE_DEVICE)
    jac->keeptranspose  = PETSC_TRUE;
    jac->mod_rap2       = 1;
    jac->coarsentype    = 8;
    jac->relaxorder     = 0;
    jac->interptype     = 6;
    jac->relaxtype[0]   = 18;
    jac->relaxtype[1]   = 18;
    jac->agg_interptype = 7;
#else
    jac->keeptranspose = PETSC_FALSE;
    jac->mod_rap2      = 0;
#endif
    PetscCallExternal(HYPRE_BoomerAMGSetCycleType, jac->hsolver, jac->cycletype);
    PetscCallExternal(HYPRE_BoomerAMGSetMaxLevels, jac->hsolver, jac->maxlevels);
    PetscCallExternal(HYPRE_BoomerAMGSetMaxIter, jac->hsolver, jac->maxiter);
    PetscCallExternal(HYPRE_BoomerAMGSetTol, jac->hsolver, jac->tol);
    PetscCallExternal(HYPRE_BoomerAMGSetTruncFactor, jac->hsolver, jac->truncfactor);
    PetscCallExternal(HYPRE_BoomerAMGSetStrongThreshold, jac->hsolver, jac->strongthreshold);
    PetscCallExternal(HYPRE_BoomerAMGSetMaxRowSum, jac->hsolver, jac->maxrowsum);
    PetscCallExternal(HYPRE_BoomerAMGSetCoarsenType, jac->hsolver, jac->coarsentype);
    PetscCallExternal(HYPRE_BoomerAMGSetMeasureType, jac->hsolver, jac->measuretype);
    PetscCallExternal(HYPRE_BoomerAMGSetRelaxOrder, jac->hsolver, jac->relaxorder);
    PetscCallExternal(HYPRE_BoomerAMGSetInterpType, jac->hsolver, jac->interptype);
    PetscCallExternal(HYPRE_BoomerAMGSetAggNumLevels, jac->hsolver, jac->agg_nl);
    PetscCallExternal(HYPRE_BoomerAMGSetAggInterpType, jac->hsolver, jac->agg_interptype);
    PetscCallExternal(HYPRE_BoomerAMGSetPMaxElmts, jac->hsolver, jac->pmax);
    PetscCallExternal(HYPRE_BoomerAMGSetNumPaths, jac->hsolver, jac->agg_num_paths);
    PetscCallExternal(HYPRE_BoomerAMGSetRelaxType, jac->hsolver, jac->relaxtype[0]);  /* defaults coarse to 9 */
    PetscCallExternal(HYPRE_BoomerAMGSetNumSweeps, jac->hsolver, jac->gridsweeps[0]); /* defaults coarse to 1 */
    PetscCallExternal(HYPRE_BoomerAMGSetMaxCoarseSize, jac->hsolver, jac->maxc);
    PetscCallExternal(HYPRE_BoomerAMGSetMinCoarseSize, jac->hsolver, jac->minc);
    /* GPU */
#if PETSC_PKG_HYPRE_VERSION_GE(2, 18, 0)
    PetscCallExternal(HYPRE_BoomerAMGSetKeepTranspose, jac->hsolver, jac->keeptranspose ? 1 : 0);
    PetscCallExternal(HYPRE_BoomerAMGSetRAP2, jac->hsolver, jac->rap2);
    PetscCallExternal(HYPRE_BoomerAMGSetModuleRAP2, jac->hsolver, jac->mod_rap2);
#endif

    /* AIR */
#if PETSC_PKG_HYPRE_VERSION_GE(2, 18, 0)
    PetscCallExternal(HYPRE_BoomerAMGSetRestriction, jac->hsolver, jac->Rtype);
    PetscCallExternal(HYPRE_BoomerAMGSetStrongThresholdR, jac->hsolver, jac->Rstrongthreshold);
    PetscCallExternal(HYPRE_BoomerAMGSetFilterThresholdR, jac->hsolver, jac->Rfilterthreshold);
    PetscCallExternal(HYPRE_BoomerAMGSetADropTol, jac->hsolver, jac->Adroptol);
    PetscCallExternal(HYPRE_BoomerAMGSetADropType, jac->hsolver, jac->Adroptype);
#endif
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscStrcmp("ams", jac->hypre_type, &flag));
  if (flag) {
    PetscCallExternal(HYPRE_AMSCreate, &jac->hsolver);
    pc->ops->setfromoptions = PCSetFromOptions_HYPRE_AMS;
    pc->ops->view           = PCView_HYPRE_AMS;
    jac->destroy            = HYPRE_AMSDestroy;
    jac->setup              = HYPRE_AMSSetup;
    jac->solve              = HYPRE_AMSSolve;
    jac->coords[0]          = NULL;
    jac->coords[1]          = NULL;
    jac->coords[2]          = NULL;
    jac->interior           = NULL;
    /* solver parameters: these are borrowed from mfem package, and they are not the default values from HYPRE AMS */
    jac->as_print       = 0;
    jac->as_max_iter    = 1;  /* used as a preconditioner */
    jac->as_tol         = 0.; /* used as a preconditioner */
    jac->ams_cycle_type = 13;
    /* Smoothing options */
    jac->as_relax_type   = 2;
    jac->as_relax_times  = 1;
    jac->as_relax_weight = 1.0;
    jac->as_omega        = 1.0;
    /* Vector valued Poisson AMG solver parameters: coarsen type, agg_levels, relax_type, interp_type, Pmax */
    jac->as_amg_alpha_opts[0] = 10;
    jac->as_amg_alpha_opts[1] = 1;
    jac->as_amg_alpha_opts[2] = 6;
    jac->as_amg_alpha_opts[3] = 6;
    jac->as_amg_alpha_opts[4] = 4;
    jac->as_amg_alpha_theta   = 0.25;
    /* Scalar Poisson AMG solver parameters: coarsen type, agg_levels, relax_type, interp_type, Pmax */
    jac->as_amg_beta_opts[0] = 10;
    jac->as_amg_beta_opts[1] = 1;
    jac->as_amg_beta_opts[2] = 6;
    jac->as_amg_beta_opts[3] = 6;
    jac->as_amg_beta_opts[4] = 4;
    jac->as_amg_beta_theta   = 0.25;
    PetscCallExternal(HYPRE_AMSSetPrintLevel, jac->hsolver, jac->as_print);
    PetscCallExternal(HYPRE_AMSSetMaxIter, jac->hsolver, jac->as_max_iter);
    PetscCallExternal(HYPRE_AMSSetCycleType, jac->hsolver, jac->ams_cycle_type);
    PetscCallExternal(HYPRE_AMSSetTol, jac->hsolver, jac->as_tol);
    PetscCallExternal(HYPRE_AMSSetSmoothingOptions, jac->hsolver, jac->as_relax_type, jac->as_relax_times, jac->as_relax_weight, jac->as_omega);
    PetscCallExternal(HYPRE_AMSSetAlphaAMGOptions, jac->hsolver, jac->as_amg_alpha_opts[0], /* AMG coarsen type */
                      jac->as_amg_alpha_opts[1],                                            /* AMG agg_levels */
                      jac->as_amg_alpha_opts[2],                                            /* AMG relax_type */
                      jac->as_amg_alpha_theta, jac->as_amg_alpha_opts[3],                   /* AMG interp_type */
                      jac->as_amg_alpha_opts[4]);                                           /* AMG Pmax */
    PetscCallExternal(HYPRE_AMSSetBetaAMGOptions, jac->hsolver, jac->as_amg_beta_opts[0],   /* AMG coarsen type */
                      jac->as_amg_beta_opts[1],                                             /* AMG agg_levels */
                      jac->as_amg_beta_opts[2],                                             /* AMG relax_type */
                      jac->as_amg_beta_theta, jac->as_amg_beta_opts[3],                     /* AMG interp_type */
                      jac->as_amg_beta_opts[4]);                                            /* AMG Pmax */
    /* Zero conductivity */
    jac->ams_beta_is_zero      = PETSC_FALSE;
    jac->ams_beta_is_zero_part = PETSC_FALSE;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscStrcmp("ads", jac->hypre_type, &flag));
  if (flag) {
    PetscCallExternal(HYPRE_ADSCreate, &jac->hsolver);
    pc->ops->setfromoptions = PCSetFromOptions_HYPRE_ADS;
    pc->ops->view           = PCView_HYPRE_ADS;
    jac->destroy            = HYPRE_ADSDestroy;
    jac->setup              = HYPRE_ADSSetup;
    jac->solve              = HYPRE_ADSSolve;
    jac->coords[0]          = NULL;
    jac->coords[1]          = NULL;
    jac->coords[2]          = NULL;
    /* solver parameters: these are borrowed from mfem package, and they are not the default values from HYPRE ADS */
    jac->as_print       = 0;
    jac->as_max_iter    = 1;  /* used as a preconditioner */
    jac->as_tol         = 0.; /* used as a preconditioner */
    jac->ads_cycle_type = 13;
    /* Smoothing options */
    jac->as_relax_type   = 2;
    jac->as_relax_times  = 1;
    jac->as_relax_weight = 1.0;
    jac->as_omega        = 1.0;
    /* AMS solver parameters: cycle_type, coarsen type, agg_levels, relax_type, interp_type, Pmax */
    jac->ams_cycle_type       = 14;
    jac->as_amg_alpha_opts[0] = 10;
    jac->as_amg_alpha_opts[1] = 1;
    jac->as_amg_alpha_opts[2] = 6;
    jac->as_amg_alpha_opts[3] = 6;
    jac->as_amg_alpha_opts[4] = 4;
    jac->as_amg_alpha_theta   = 0.25;
    /* Vector Poisson AMG solver parameters: coarsen type, agg_levels, relax_type, interp_type, Pmax */
    jac->as_amg_beta_opts[0] = 10;
    jac->as_amg_beta_opts[1] = 1;
    jac->as_amg_beta_opts[2] = 6;
    jac->as_amg_beta_opts[3] = 6;
    jac->as_amg_beta_opts[4] = 4;
    jac->as_amg_beta_theta   = 0.25;
    PetscCallExternal(HYPRE_ADSSetPrintLevel, jac->hsolver, jac->as_print);
    PetscCallExternal(HYPRE_ADSSetMaxIter, jac->hsolver, jac->as_max_iter);
    PetscCallExternal(HYPRE_ADSSetCycleType, jac->hsolver, jac->ams_cycle_type);
    PetscCallExternal(HYPRE_ADSSetTol, jac->hsolver, jac->as_tol);
    PetscCallExternal(HYPRE_ADSSetSmoothingOptions, jac->hsolver, jac->as_relax_type, jac->as_relax_times, jac->as_relax_weight, jac->as_omega);
    PetscCallExternal(HYPRE_ADSSetAMSOptions, jac->hsolver, jac->ams_cycle_type,      /* AMG coarsen type */
                      jac->as_amg_alpha_opts[0],                                      /* AMG coarsen type */
                      jac->as_amg_alpha_opts[1],                                      /* AMG agg_levels */
                      jac->as_amg_alpha_opts[2],                                      /* AMG relax_type */
                      jac->as_amg_alpha_theta, jac->as_amg_alpha_opts[3],             /* AMG interp_type */
                      jac->as_amg_alpha_opts[4]);                                     /* AMG Pmax */
    PetscCallExternal(HYPRE_ADSSetAMGOptions, jac->hsolver, jac->as_amg_beta_opts[0], /* AMG coarsen type */
                      jac->as_amg_beta_opts[1],                                       /* AMG agg_levels */
                      jac->as_amg_beta_opts[2],                                       /* AMG relax_type */
                      jac->as_amg_beta_theta, jac->as_amg_beta_opts[3],               /* AMG interp_type */
                      jac->as_amg_beta_opts[4]);                                      /* AMG Pmax */
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  PetscCall(PetscFree(jac->hypre_type));

  jac->hypre_type = NULL;
  SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_UNKNOWN_TYPE, "Unknown HYPRE preconditioner %s; Choices are euclid, pilut, parasails, boomeramg, ams", name);
}

/*
    It only gets here if the HYPRE type has not been set before the call to
   ...SetFromOptions() which actually is most of the time
*/
PetscErrorCode PCSetFromOptions_HYPRE(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PetscInt    indx;
  const char *type[] = {"euclid", "pilut", "parasails", "boomeramg", "ams", "ads"};
  PetscBool   flg;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "HYPRE preconditioner options");
  PetscCall(PetscOptionsEList("-pc_hypre_type", "HYPRE preconditioner type", "PCHYPRESetType", type, PETSC_STATIC_ARRAY_LENGTH(type), "boomeramg", &indx, &flg));
  if (flg) {
    PetscCall(PCHYPRESetType_HYPRE(pc, type[indx]));
  } else {
    PetscCall(PCHYPRESetType_HYPRE(pc, "boomeramg"));
  }
  PetscTryTypeMethod(pc, setfromoptions, PetscOptionsObject);
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
     PCHYPRESetType - Sets which hypre preconditioner you wish to use

   Input Parameters:
+     pc - the preconditioner context
-     name - either  euclid, pilut, parasails, boomeramg, ams, ads

   Options Database Key:
   -pc_hypre_type - One of euclid, pilut, parasails, boomeramg, ams, ads

   Level: intermediate

.seealso: `PCCreate()`, `PCSetType()`, `PCType`, `PC`, `PCHYPRE`
@*/
PetscErrorCode PCHYPRESetType(PC pc, const char name[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  PetscTryMethod(pc, "PCHYPRESetType_C", (PC, const char[]), (pc, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
     PCHYPREGetType - Gets which hypre preconditioner you are using

   Input Parameter:
.     pc - the preconditioner context

   Output Parameter:
.     name - either  euclid, pilut, parasails, boomeramg, ams, ads

   Level: intermediate

.seealso: `PCCreate()`, `PCHYPRESetType()`, `PCType`, `PC`, `PCHYPRE`
@*/
PetscErrorCode PCHYPREGetType(PC pc, const char *name[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscValidPointer(name, 2);
  PetscTryMethod(pc, "PCHYPREGetType_C", (PC, const char *[]), (pc, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PCMGGalerkinSetMatProductAlgorithm - Set type of SpGEMM for hypre to use on GPUs

   Logically Collective

   Input Parameters:
+  pc - the hypre context
-  type - one of 'cusparse', 'hypre'

   Options Database Key:
.  -pc_mg_galerkin_mat_product_algorithm <cusparse,hypre> - Type of SpGEMM to use in hypre

   Level: intermediate

   Developer Note:
   How the name starts with `PCMG`, should it not be `PCHYPREBoomerAMG`?

.seealso: `PCHYPRE`, `PCMGGalerkinGetMatProductAlgorithm()`
@*/
PetscErrorCode PCMGGalerkinSetMatProductAlgorithm(PC pc, const char name[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscTryMethod(pc, "PCMGGalerkinSetMatProductAlgorithm_C", (PC, const char[]), (pc, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PCMGGalerkinGetMatProductAlgorithm - Get type of SpGEMM for hypre to use on GPUs

   Not Collective

   Input Parameter:
.  pc - the multigrid context

   Output Parameter:
.  name - one of 'cusparse', 'hypre'

   Level: intermediate

.seealso: `PCHYPRE`, ``PCMGGalerkinSetMatProductAlgorithm()`
@*/
PetscErrorCode PCMGGalerkinGetMatProductAlgorithm(PC pc, const char *name[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscTryMethod(pc, "PCMGGalerkinGetMatProductAlgorithm_C", (PC, const char *[]), (pc, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
     PCHYPRE - Allows you to use the matrix element based preconditioners in the LLNL package hypre as PETSc `PC`

   Options Database Keys:
+   -pc_hypre_type - One of euclid, pilut, parasails, boomeramg, ams, ads
.   -pc_hypre_boomeramg_nodal_coarsen <n> - where n is from 1 to 6 (see `HYPRE_BOOMERAMGSetNodal()`)
.   -pc_hypre_boomeramg_vec_interp_variant <v> - where v is from 1 to 3 (see `HYPRE_BoomerAMGSetInterpVecVariant()`)
-   Many others, run with -pc_type hypre -pc_hypre_type XXX -help to see options for the XXX preconditioner

   Level: intermediate

   Notes:
    Apart from pc_hypre_type (for which there is `PCHYPRESetType()`),
          the many hypre options can ONLY be set via the options database (e.g. the command line
          or with `PetscOptionsSetValue()`, there are no functions to set them)

          The options -pc_hypre_boomeramg_max_iter and -pc_hypre_boomeramg_tol refer to the number of iterations
          (V-cycles) and tolerance that boomeramg does EACH time it is called. So for example, if
          -pc_hypre_boomeramg_max_iter is set to 2 then 2-V-cycles are being used to define the preconditioner
          (-pc_hypre_boomeramg_tol should be set to 0.0 - the default - to strictly use a fixed number of
          iterations per hypre call). -ksp_max_it and -ksp_rtol STILL determine the total number of iterations
          and tolerance for the Krylov solver. For example, if -pc_hypre_boomeramg_max_iter is 2 and -ksp_max_it is 10
          then AT MOST twenty V-cycles of boomeramg will be called.

           Note that the option -pc_hypre_boomeramg_relax_type_all defaults to symmetric relaxation
           (symmetric-SOR/Jacobi), which is required for Krylov solvers like CG that expect symmetry.
           Otherwise, you may want to use -pc_hypre_boomeramg_relax_type_all SOR/Jacobi.
          If you wish to use BoomerAMG WITHOUT a Krylov method use -ksp_type richardson NOT -ksp_type preonly
          and use -ksp_max_it to control the number of V-cycles.
          (see the PETSc FAQ.html at the PETSc website under the Documentation tab).

          `MatSetNearNullSpace()` - if you provide a near null space to your matrix it is ignored by hypre UNLESS you also use
          the following two options: ``-pc_hypre_boomeramg_nodal_coarsen <n> -pc_hypre_boomeramg_vec_interp_variant <v>``

          See `PCPFMG`, `PCSMG`, and `PCSYSPFMG` for access to hypre's other (nonalgebraic) multigrid solvers

          For `PCHYPRE` type of ams or ads auxiliary data must be provided to the preconditioner with `PCHYPRESetDiscreteGradient()`,
          `PCHYPRESetDiscreteCurl()`, `PCHYPRESetInterpolations()`, `PCHYPRESetAlphaPoissonMatrix()`, `PCHYPRESetBetaPoissonMatrix()`, `PCHYPRESetEdgeConstantVectors()`,
          `PCHYPREAMSSetInteriorNodes()`

   PETSc provides its own geometric and algebraic multigrid solvers `PCMG` and `PCGAMG`, also see `PCHMG` which is useful for certain multicomponent problems

   GPU Notes:
     To configure hypre BoomerAMG so that it can utilize NVIDIA GPUs run ./configure --download-hypre --with-cuda
     Then pass `VECCUDA` vectors and `MATAIJCUSPARSE` matrices to the solvers and PETSc will automatically utilize hypre's GPU solvers.

     To configure hypre BoomerAMG so that it can utilize AMD GPUs run ./configure --download-hypre --with-hip
     Then pass `VECHIP` vectors to the solvers and PETSc will automatically utilize hypre's GPU solvers.

.seealso: `PCCreate()`, `PCSetType()`, `PCType`, `PC`, `PCHYPRESetType()`, `PCPFMG`, `PCGAMG`, `PCSYSPFMG`, `PCSMG`, `PCHYPRESetDiscreteGradient()`,
          `PCHYPRESetDiscreteCurl()`, `PCHYPRESetInterpolations()`, `PCHYPRESetAlphaPoissonMatrix()`, `PCHYPRESetBetaPoissonMatrix()`, `PCHYPRESetEdgeConstantVectors()`,
          PCHYPREAMSSetInteriorNodes()
M*/

PETSC_EXTERN PetscErrorCode PCCreate_HYPRE(PC pc)
{
  PC_HYPRE *jac;

  PetscFunctionBegin;
  PetscCall(PetscNew(&jac));

  pc->data                = jac;
  pc->ops->reset          = PCReset_HYPRE;
  pc->ops->destroy        = PCDestroy_HYPRE;
  pc->ops->setfromoptions = PCSetFromOptions_HYPRE;
  pc->ops->setup          = PCSetUp_HYPRE;
  pc->ops->apply          = PCApply_HYPRE;
  jac->comm_hypre         = MPI_COMM_NULL;
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetType_C", PCHYPRESetType_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPREGetType_C", PCHYPREGetType_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCSetCoordinates_C", PCSetCoordinates_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetDiscreteGradient_C", PCHYPRESetDiscreteGradient_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetDiscreteCurl_C", PCHYPRESetDiscreteCurl_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetInterpolations_C", PCHYPRESetInterpolations_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetEdgeConstantVectors_C", PCHYPRESetEdgeConstantVectors_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPREAMSSetInteriorNodes_C", PCHYPREAMSSetInteriorNodes_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCHYPRESetPoissonMatrix_C", PCHYPRESetPoissonMatrix_HYPRE));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCMGGalerkinSetMatProductAlgorithm_C", PCMGGalerkinSetMatProductAlgorithm_HYPRE_BoomerAMG));
  PetscCall(PetscObjectComposeFunction((PetscObject)pc, "PCMGGalerkinGetMatProductAlgorithm_C", PCMGGalerkinGetMatProductAlgorithm_HYPRE_BoomerAMG));
#if defined(PETSC_HAVE_HYPRE_DEVICE)
  #if defined(HYPRE_USING_HIP)
  PetscCall(PetscDeviceInitialize(PETSC_DEVICE_HIP));
  #endif
  #if defined(HYPRE_USING_CUDA)
  PetscCall(PetscDeviceInitialize(PETSC_DEVICE_CUDA));
  #endif
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

typedef struct {
  MPI_Comm           hcomm; /* does not share comm with HYPRE_StructMatrix because need to create solver before getting matrix */
  HYPRE_StructSolver hsolver;

  /* keep copy of PFMG options used so may view them */
  PetscInt  its;
  double    tol;
  PetscInt  relax_type;
  PetscInt  rap_type;
  PetscInt  num_pre_relax, num_post_relax;
  PetscInt  max_levels;
  PetscInt  skip_relax;
  PetscBool print_statistics;
} PC_PFMG;

PetscErrorCode PCDestroy_PFMG(PC pc)
{
  PC_PFMG *ex = (PC_PFMG *)pc->data;

  PetscFunctionBegin;
  if (ex->hsolver) PetscCallExternal(HYPRE_StructPFMGDestroy, ex->hsolver);
  PetscCall(PetscCommRestoreComm(PetscObjectComm((PetscObject)pc), &ex->hcomm));
  PetscCall(PetscFree(pc->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static const char *PFMGRelaxType[] = {"Jacobi", "Weighted-Jacobi", "symmetric-Red/Black-Gauss-Seidel", "Red/Black-Gauss-Seidel"};
static const char *PFMGRAPType[]   = {"Galerkin", "non-Galerkin"};

PetscErrorCode PCView_PFMG(PC pc, PetscViewer viewer)
{
  PetscBool iascii;
  PC_PFMG  *ex = (PC_PFMG *)pc->data;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE PFMG preconditioning\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    max iterations %" PetscInt_FMT "\n", ex->its));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    tolerance %g\n", ex->tol));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    relax type %s\n", PFMGRelaxType[ex->relax_type]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    RAP type %s\n", PFMGRAPType[ex->rap_type]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    number pre-relax %" PetscInt_FMT " post-relax %" PetscInt_FMT "\n", ex->num_pre_relax, ex->num_post_relax));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    max levels %" PetscInt_FMT "\n", ex->max_levels));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    skip relax %" PetscInt_FMT "\n", ex->skip_relax));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCSetFromOptions_PFMG(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_PFMG *ex = (PC_PFMG *)pc->data;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "PFMG options");
  PetscCall(PetscOptionsBool("-pc_pfmg_print_statistics", "Print statistics", "HYPRE_StructPFMGSetPrintLevel", ex->print_statistics, &ex->print_statistics, NULL));
  PetscCall(PetscOptionsInt("-pc_pfmg_its", "Number of iterations of PFMG to use as preconditioner", "HYPRE_StructPFMGSetMaxIter", ex->its, &ex->its, NULL));
  PetscCallExternal(HYPRE_StructPFMGSetMaxIter, ex->hsolver, ex->its);
  PetscCall(PetscOptionsInt("-pc_pfmg_num_pre_relax", "Number of smoothing steps before coarse grid", "HYPRE_StructPFMGSetNumPreRelax", ex->num_pre_relax, &ex->num_pre_relax, NULL));
  PetscCallExternal(HYPRE_StructPFMGSetNumPreRelax, ex->hsolver, ex->num_pre_relax);
  PetscCall(PetscOptionsInt("-pc_pfmg_num_post_relax", "Number of smoothing steps after coarse grid", "HYPRE_StructPFMGSetNumPostRelax", ex->num_post_relax, &ex->num_post_relax, NULL));
  PetscCallExternal(HYPRE_StructPFMGSetNumPostRelax, ex->hsolver, ex->num_post_relax);

  PetscCall(PetscOptionsInt("-pc_pfmg_max_levels", "Max Levels for MG hierarchy", "HYPRE_StructPFMGSetMaxLevels", ex->max_levels, &ex->max_levels, NULL));
  PetscCallExternal(HYPRE_StructPFMGSetMaxLevels, ex->hsolver, ex->max_levels);

  PetscCall(PetscOptionsReal("-pc_pfmg_tol", "Tolerance of PFMG", "HYPRE_StructPFMGSetTol", ex->tol, &ex->tol, NULL));
  PetscCallExternal(HYPRE_StructPFMGSetTol, ex->hsolver, ex->tol);
  PetscCall(PetscOptionsEList("-pc_pfmg_relax_type", "Relax type for the up and down cycles", "HYPRE_StructPFMGSetRelaxType", PFMGRelaxType, PETSC_STATIC_ARRAY_LENGTH(PFMGRelaxType), PFMGRelaxType[ex->relax_type], &ex->relax_type, NULL));
  PetscCallExternal(HYPRE_StructPFMGSetRelaxType, ex->hsolver, ex->relax_type);
  PetscCall(PetscOptionsEList("-pc_pfmg_rap_type", "RAP type", "HYPRE_StructPFMGSetRAPType", PFMGRAPType, PETSC_STATIC_ARRAY_LENGTH(PFMGRAPType), PFMGRAPType[ex->rap_type], &ex->rap_type, NULL));
  PetscCallExternal(HYPRE_StructPFMGSetRAPType, ex->hsolver, ex->rap_type);
  PetscCall(PetscOptionsInt("-pc_pfmg_skip_relax", "Skip relaxation on certain grids for isotropic problems. This can greatly improve efficiency by eliminating unnecessary relaxations when the underlying problem is isotropic", "HYPRE_StructPFMGSetSkipRelax", ex->skip_relax, &ex->skip_relax, NULL));
  PetscCallExternal(HYPRE_StructPFMGSetSkipRelax, ex->hsolver, ex->skip_relax);
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCApply_PFMG(PC pc, Vec x, Vec y)
{
  PC_PFMG           *ex = (PC_PFMG *)pc->data;
  PetscScalar       *yy;
  const PetscScalar *xx;
  PetscInt           ilower[3], iupper[3];
  HYPRE_Int          hlower[3], hupper[3];
  Mat_HYPREStruct   *mx = (Mat_HYPREStruct *)(pc->pmat->data);

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  PetscCall(DMDAGetCorners(mx->da, &ilower[0], &ilower[1], &ilower[2], &iupper[0], &iupper[1], &iupper[2]));
  /* when HYPRE_MIXEDINT is defined, sizeof(HYPRE_Int) == 32 */
  iupper[0] += ilower[0] - 1;
  iupper[1] += ilower[1] - 1;
  iupper[2] += ilower[2] - 1;
  hlower[0] = (HYPRE_Int)ilower[0];
  hlower[1] = (HYPRE_Int)ilower[1];
  hlower[2] = (HYPRE_Int)ilower[2];
  hupper[0] = (HYPRE_Int)iupper[0];
  hupper[1] = (HYPRE_Int)iupper[1];
  hupper[2] = (HYPRE_Int)iupper[2];

  /* copy x values over to hypre */
  PetscCallExternal(HYPRE_StructVectorSetConstantValues, mx->hb, 0.0);
  PetscCall(VecGetArrayRead(x, &xx));
  PetscCallExternal(HYPRE_StructVectorSetBoxValues, mx->hb, hlower, hupper, (HYPRE_Complex *)xx);
  PetscCall(VecRestoreArrayRead(x, &xx));
  PetscCallExternal(HYPRE_StructVectorAssemble, mx->hb);
  PetscCallExternal(HYPRE_StructPFMGSolve, ex->hsolver, mx->hmat, mx->hb, mx->hx);

  /* copy solution values back to PETSc */
  PetscCall(VecGetArray(y, &yy));
  PetscCallExternal(HYPRE_StructVectorGetBoxValues, mx->hx, hlower, hupper, (HYPRE_Complex *)yy);
  PetscCall(VecRestoreArray(y, &yy));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCApplyRichardson_PFMG(PC pc, Vec b, Vec y, Vec w, PetscReal rtol, PetscReal abstol, PetscReal dtol, PetscInt its, PetscBool guesszero, PetscInt *outits, PCRichardsonConvergedReason *reason)
{
  PC_PFMG  *jac = (PC_PFMG *)pc->data;
  HYPRE_Int oits;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  PetscCallExternal(HYPRE_StructPFMGSetMaxIter, jac->hsolver, its * jac->its);
  PetscCallExternal(HYPRE_StructPFMGSetTol, jac->hsolver, rtol);

  PetscCall(PCApply_PFMG(pc, b, y));
  PetscCallExternal(HYPRE_StructPFMGGetNumIterations, jac->hsolver, &oits);
  *outits = oits;
  if (oits == its) *reason = PCRICHARDSON_CONVERGED_ITS;
  else *reason = PCRICHARDSON_CONVERGED_RTOL;
  PetscCallExternal(HYPRE_StructPFMGSetTol, jac->hsolver, jac->tol);
  PetscCallExternal(HYPRE_StructPFMGSetMaxIter, jac->hsolver, jac->its);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCSetUp_PFMG(PC pc)
{
  PC_PFMG         *ex = (PC_PFMG *)pc->data;
  Mat_HYPREStruct *mx = (Mat_HYPREStruct *)(pc->pmat->data);
  PetscBool        flg;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)pc->pmat, MATHYPRESTRUCT, &flg));
  PetscCheck(flg, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_INCOMP, "Must use MATHYPRESTRUCT with this preconditioner");

  /* create the hypre solver object and set its information */
  if (ex->hsolver) PetscCallExternal(HYPRE_StructPFMGDestroy, ex->hsolver);
  PetscCallExternal(HYPRE_StructPFMGCreate, ex->hcomm, &ex->hsolver);

  // Print Hypre statistics about the solve process
  if (ex->print_statistics) PetscCallExternal(HYPRE_StructPFMGSetPrintLevel, ex->hsolver, 3);

  // The hypre options must be repeated here because the StructPFMG was destroyed and recreated
  PetscCallExternal(HYPRE_StructPFMGSetMaxIter, ex->hsolver, ex->its);
  PetscCallExternal(HYPRE_StructPFMGSetNumPreRelax, ex->hsolver, ex->num_pre_relax);
  PetscCallExternal(HYPRE_StructPFMGSetNumPostRelax, ex->hsolver, ex->num_post_relax);
  PetscCallExternal(HYPRE_StructPFMGSetMaxLevels, ex->hsolver, ex->max_levels);
  PetscCallExternal(HYPRE_StructPFMGSetTol, ex->hsolver, ex->tol);
  PetscCallExternal(HYPRE_StructPFMGSetRelaxType, ex->hsolver, ex->relax_type);
  PetscCallExternal(HYPRE_StructPFMGSetRAPType, ex->hsolver, ex->rap_type);

  PetscCallExternal(HYPRE_StructPFMGSetup, ex->hsolver, mx->hmat, mx->hb, mx->hx);
  PetscCallExternal(HYPRE_StructPFMGSetZeroGuess, ex->hsolver);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
     PCPFMG - the hypre PFMG multigrid solver

   Options Database Keys:
+ -pc_pfmg_its <its> - number of iterations of PFMG to use as preconditioner
. -pc_pfmg_num_pre_relax <steps> - number of smoothing steps before coarse grid solve
. -pc_pfmg_num_post_relax <steps> - number of smoothing steps after coarse grid solve
. -pc_pfmg_tol <tol> - tolerance of PFMG
. -pc_pfmg_relax_type - relaxation type for the up and down cycles, one of Jacobi,Weighted-Jacobi,symmetric-Red/Black-Gauss-Seidel,Red/Black-Gauss-Seidel
. -pc_pfmg_rap_type - type of coarse matrix generation, one of Galerkin,non-Galerkin
- -pc_pfmg_skip_relax - skip relaxation on certain grids for isotropic problems. This can greatly improve efficiency by eliminating unnecessary relaxations
                        when the underlying problem is isotropic, one of 0,1

   Level: advanced

   Notes:
   This is for CELL-centered descretizations

   See `PCSYSPFMG` for a version suitable for systems of PDEs, and `PCSMG`

   See `PCHYPRE` for hypre's BoomerAMG algebraic multigrid solver

   This must be used with the `MATHYPRESTRUCT` matrix type.

   This provides only some of the functionality of PFMG, it supports only one block per process defined by a PETSc `DMDA`.

.seealso: `PCMG`, `MATHYPRESTRUCT`, `PCHYPRE`, `PCGAMG`, `PCSYSPFMG`, `PCSMG`
M*/

PETSC_EXTERN PetscErrorCode PCCreate_PFMG(PC pc)
{
  PC_PFMG *ex;

  PetscFunctionBegin;
  PetscCall(PetscNew(&ex));
  pc->data = ex;

  ex->its              = 1;
  ex->tol              = 1.e-8;
  ex->relax_type       = 1;
  ex->rap_type         = 0;
  ex->num_pre_relax    = 1;
  ex->num_post_relax   = 1;
  ex->max_levels       = 0;
  ex->skip_relax       = 0;
  ex->print_statistics = PETSC_FALSE;

  pc->ops->setfromoptions  = PCSetFromOptions_PFMG;
  pc->ops->view            = PCView_PFMG;
  pc->ops->destroy         = PCDestroy_PFMG;
  pc->ops->apply           = PCApply_PFMG;
  pc->ops->applyrichardson = PCApplyRichardson_PFMG;
  pc->ops->setup           = PCSetUp_PFMG;

  PetscCall(PetscCommGetComm(PetscObjectComm((PetscObject)pc), &ex->hcomm));
  PetscCallExternal(HYPRE_StructPFMGCreate, ex->hcomm, &ex->hsolver);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* we know we are working with a HYPRE_SStructMatrix */
typedef struct {
  MPI_Comm            hcomm; /* does not share comm with HYPRE_SStructMatrix because need to create solver before getting matrix */
  HYPRE_SStructSolver ss_solver;

  /* keep copy of SYSPFMG options used so may view them */
  PetscInt its;
  double   tol;
  PetscInt relax_type;
  PetscInt num_pre_relax, num_post_relax;
} PC_SysPFMG;

PetscErrorCode PCDestroy_SysPFMG(PC pc)
{
  PC_SysPFMG *ex = (PC_SysPFMG *)pc->data;

  PetscFunctionBegin;
  if (ex->ss_solver) PetscCallExternal(HYPRE_SStructSysPFMGDestroy, ex->ss_solver);
  PetscCall(PetscCommRestoreComm(PetscObjectComm((PetscObject)pc), &ex->hcomm));
  PetscCall(PetscFree(pc->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static const char *SysPFMGRelaxType[] = {"Weighted-Jacobi", "Red/Black-Gauss-Seidel"};

PetscErrorCode PCView_SysPFMG(PC pc, PetscViewer viewer)
{
  PetscBool   iascii;
  PC_SysPFMG *ex = (PC_SysPFMG *)pc->data;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE SysPFMG preconditioning\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  max iterations %" PetscInt_FMT "\n", ex->its));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  tolerance %g\n", ex->tol));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  relax type %s\n", PFMGRelaxType[ex->relax_type]));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  number pre-relax %" PetscInt_FMT " post-relax %" PetscInt_FMT "\n", ex->num_pre_relax, ex->num_post_relax));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCSetFromOptions_SysPFMG(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_SysPFMG *ex  = (PC_SysPFMG *)pc->data;
  PetscBool   flg = PETSC_FALSE;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "SysPFMG options");
  PetscCall(PetscOptionsBool("-pc_syspfmg_print_statistics", "Print statistics", "HYPRE_SStructSysPFMGSetPrintLevel", flg, &flg, NULL));
  if (flg) PetscCallExternal(HYPRE_SStructSysPFMGSetPrintLevel, ex->ss_solver, 3);
  PetscCall(PetscOptionsInt("-pc_syspfmg_its", "Number of iterations of SysPFMG to use as preconditioner", "HYPRE_SStructSysPFMGSetMaxIter", ex->its, &ex->its, NULL));
  PetscCallExternal(HYPRE_SStructSysPFMGSetMaxIter, ex->ss_solver, ex->its);
  PetscCall(PetscOptionsInt("-pc_syspfmg_num_pre_relax", "Number of smoothing steps before coarse grid", "HYPRE_SStructSysPFMGSetNumPreRelax", ex->num_pre_relax, &ex->num_pre_relax, NULL));
  PetscCallExternal(HYPRE_SStructSysPFMGSetNumPreRelax, ex->ss_solver, ex->num_pre_relax);
  PetscCall(PetscOptionsInt("-pc_syspfmg_num_post_relax", "Number of smoothing steps after coarse grid", "HYPRE_SStructSysPFMGSetNumPostRelax", ex->num_post_relax, &ex->num_post_relax, NULL));
  PetscCallExternal(HYPRE_SStructSysPFMGSetNumPostRelax, ex->ss_solver, ex->num_post_relax);

  PetscCall(PetscOptionsReal("-pc_syspfmg_tol", "Tolerance of SysPFMG", "HYPRE_SStructSysPFMGSetTol", ex->tol, &ex->tol, NULL));
  PetscCallExternal(HYPRE_SStructSysPFMGSetTol, ex->ss_solver, ex->tol);
  PetscCall(PetscOptionsEList("-pc_syspfmg_relax_type", "Relax type for the up and down cycles", "HYPRE_SStructSysPFMGSetRelaxType", SysPFMGRelaxType, PETSC_STATIC_ARRAY_LENGTH(SysPFMGRelaxType), SysPFMGRelaxType[ex->relax_type], &ex->relax_type, NULL));
  PetscCallExternal(HYPRE_SStructSysPFMGSetRelaxType, ex->ss_solver, ex->relax_type);
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCApply_SysPFMG(PC pc, Vec x, Vec y)
{
  PC_SysPFMG        *ex = (PC_SysPFMG *)pc->data;
  PetscScalar       *yy;
  const PetscScalar *xx;
  PetscInt           ilower[3], iupper[3];
  HYPRE_Int          hlower[3], hupper[3];
  Mat_HYPRESStruct  *mx       = (Mat_HYPRESStruct *)(pc->pmat->data);
  PetscInt           ordering = mx->dofs_order;
  PetscInt           nvars    = mx->nvars;
  PetscInt           part     = 0;
  PetscInt           size;
  PetscInt           i;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  PetscCall(DMDAGetCorners(mx->da, &ilower[0], &ilower[1], &ilower[2], &iupper[0], &iupper[1], &iupper[2]));
  /* when HYPRE_MIXEDINT is defined, sizeof(HYPRE_Int) == 32 */
  iupper[0] += ilower[0] - 1;
  iupper[1] += ilower[1] - 1;
  iupper[2] += ilower[2] - 1;
  hlower[0] = (HYPRE_Int)ilower[0];
  hlower[1] = (HYPRE_Int)ilower[1];
  hlower[2] = (HYPRE_Int)ilower[2];
  hupper[0] = (HYPRE_Int)iupper[0];
  hupper[1] = (HYPRE_Int)iupper[1];
  hupper[2] = (HYPRE_Int)iupper[2];

  size = 1;
  for (i = 0; i < 3; i++) size *= (iupper[i] - ilower[i] + 1);

  /* copy x values over to hypre for variable ordering */
  if (ordering) {
    PetscCallExternal(HYPRE_SStructVectorSetConstantValues, mx->ss_b, 0.0);
    PetscCall(VecGetArrayRead(x, &xx));
    for (i = 0; i < nvars; i++) PetscCallExternal(HYPRE_SStructVectorSetBoxValues, mx->ss_b, part, hlower, hupper, i, (HYPRE_Complex *)(xx + (size * i)));
    PetscCall(VecRestoreArrayRead(x, &xx));
    PetscCallExternal(HYPRE_SStructVectorAssemble, mx->ss_b);
    PetscCallExternal(HYPRE_SStructMatrixMatvec, 1.0, mx->ss_mat, mx->ss_b, 0.0, mx->ss_x);
    PetscCallExternal(HYPRE_SStructSysPFMGSolve, ex->ss_solver, mx->ss_mat, mx->ss_b, mx->ss_x);

    /* copy solution values back to PETSc */
    PetscCall(VecGetArray(y, &yy));
    for (i = 0; i < nvars; i++) PetscCallExternal(HYPRE_SStructVectorGetBoxValues, mx->ss_x, part, hlower, hupper, i, (HYPRE_Complex *)(yy + (size * i)));
    PetscCall(VecRestoreArray(y, &yy));
  } else { /* nodal ordering must be mapped to variable ordering for sys_pfmg */
    PetscScalar *z;
    PetscInt     j, k;

    PetscCall(PetscMalloc1(nvars * size, &z));
    PetscCallExternal(HYPRE_SStructVectorSetConstantValues, mx->ss_b, 0.0);
    PetscCall(VecGetArrayRead(x, &xx));

    /* transform nodal to hypre's variable ordering for sys_pfmg */
    for (i = 0; i < size; i++) {
      k = i * nvars;
      for (j = 0; j < nvars; j++) z[j * size + i] = xx[k + j];
    }
    for (i = 0; i < nvars; i++) PetscCallExternal(HYPRE_SStructVectorSetBoxValues, mx->ss_b, part, hlower, hupper, i, (HYPRE_Complex *)(z + (size * i)));
    PetscCall(VecRestoreArrayRead(x, &xx));
    PetscCallExternal(HYPRE_SStructVectorAssemble, mx->ss_b);
    PetscCallExternal(HYPRE_SStructSysPFMGSolve, ex->ss_solver, mx->ss_mat, mx->ss_b, mx->ss_x);

    /* copy solution values back to PETSc */
    PetscCall(VecGetArray(y, &yy));
    for (i = 0; i < nvars; i++) PetscCallExternal(HYPRE_SStructVectorGetBoxValues, mx->ss_x, part, hlower, hupper, i, (HYPRE_Complex *)(z + (size * i)));
    /* transform hypre's variable ordering for sys_pfmg to nodal ordering */
    for (i = 0; i < size; i++) {
      k = i * nvars;
      for (j = 0; j < nvars; j++) yy[k + j] = z[j * size + i];
    }
    PetscCall(VecRestoreArray(y, &yy));
    PetscCall(PetscFree(z));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCApplyRichardson_SysPFMG(PC pc, Vec b, Vec y, Vec w, PetscReal rtol, PetscReal abstol, PetscReal dtol, PetscInt its, PetscBool guesszero, PetscInt *outits, PCRichardsonConvergedReason *reason)
{
  PC_SysPFMG *jac = (PC_SysPFMG *)pc->data;
  HYPRE_Int   oits;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  PetscCallExternal(HYPRE_SStructSysPFMGSetMaxIter, jac->ss_solver, its * jac->its);
  PetscCallExternal(HYPRE_SStructSysPFMGSetTol, jac->ss_solver, rtol);
  PetscCall(PCApply_SysPFMG(pc, b, y));
  PetscCallExternal(HYPRE_SStructSysPFMGGetNumIterations, jac->ss_solver, &oits);
  *outits = oits;
  if (oits == its) *reason = PCRICHARDSON_CONVERGED_ITS;
  else *reason = PCRICHARDSON_CONVERGED_RTOL;
  PetscCallExternal(HYPRE_SStructSysPFMGSetTol, jac->ss_solver, jac->tol);
  PetscCallExternal(HYPRE_SStructSysPFMGSetMaxIter, jac->ss_solver, jac->its);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCSetUp_SysPFMG(PC pc)
{
  PC_SysPFMG       *ex = (PC_SysPFMG *)pc->data;
  Mat_HYPRESStruct *mx = (Mat_HYPRESStruct *)(pc->pmat->data);
  PetscBool         flg;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)pc->pmat, MATHYPRESSTRUCT, &flg));
  PetscCheck(flg, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_INCOMP, "Must use MATHYPRESSTRUCT with this preconditioner");

  /* create the hypre sstruct solver object and set its information */
  if (ex->ss_solver) PetscCallExternal(HYPRE_SStructSysPFMGDestroy, ex->ss_solver);
  PetscCallExternal(HYPRE_SStructSysPFMGCreate, ex->hcomm, &ex->ss_solver);
  PetscCallExternal(HYPRE_SStructSysPFMGSetZeroGuess, ex->ss_solver);
  PetscCallExternal(HYPRE_SStructSysPFMGSetup, ex->ss_solver, mx->ss_mat, mx->ss_b, mx->ss_x);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
     PCSYSPFMG - the hypre SysPFMG multigrid solver

   Level: advanced

   Options Database Keys:
+ -pc_syspfmg_its <its> - number of iterations of SysPFMG to use as preconditioner
. -pc_syspfmg_num_pre_relax <steps> - number of smoothing steps before coarse grid
. -pc_syspfmg_num_post_relax <steps> - number of smoothing steps after coarse grid
. -pc_syspfmg_tol <tol> - tolerance of SysPFMG
- -pc_syspfmg_relax_type <Weighted-Jacobi,Red/Black-Gauss-Seidel> - relaxation type for the up and down cycles

   Notes:
   See `PCPFMG` for hypre's PFMG that works for a scalar PDE and `PCSMG`

   See `PCHYPRE` for hypre's BoomerAMG algebraic multigrid solver

   This is for CELL-centered descretizations

   This must be used with the `MATHYPRESSTRUCT` matrix type.

   This does not give access to all the functionality of hypres SysPFMG, it supports only one part, and one block per process defined by a PETSc `DMDA`.

.seealso: `PCMG`, `MATHYPRESSTRUCT`, `PCPFMG`, `PCHYPRE`, `PCGAMG`, `PCSMG`
M*/

PETSC_EXTERN PetscErrorCode PCCreate_SysPFMG(PC pc)
{
  PC_SysPFMG *ex;

  PetscFunctionBegin;
  PetscCall(PetscNew(&ex));
  pc->data = ex;

  ex->its            = 1;
  ex->tol            = 1.e-8;
  ex->relax_type     = 1;
  ex->num_pre_relax  = 1;
  ex->num_post_relax = 1;

  pc->ops->setfromoptions  = PCSetFromOptions_SysPFMG;
  pc->ops->view            = PCView_SysPFMG;
  pc->ops->destroy         = PCDestroy_SysPFMG;
  pc->ops->apply           = PCApply_SysPFMG;
  pc->ops->applyrichardson = PCApplyRichardson_SysPFMG;
  pc->ops->setup           = PCSetUp_SysPFMG;

  PetscCall(PetscCommGetComm(PetscObjectComm((PetscObject)pc), &ex->hcomm));
  PetscCallExternal(HYPRE_SStructSysPFMGCreate, ex->hcomm, &ex->ss_solver);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* PC SMG */
typedef struct {
  MPI_Comm           hcomm; /* does not share comm with HYPRE_StructMatrix because need to create solver before getting matrix */
  HYPRE_StructSolver hsolver;
  PetscInt           its; /* keep copy of SMG options used so may view them */
  double             tol;
  PetscBool          print_statistics;
  PetscInt           num_pre_relax, num_post_relax;
} PC_SMG;

PetscErrorCode PCDestroy_SMG(PC pc)
{
  PC_SMG *ex = (PC_SMG *)pc->data;

  PetscFunctionBegin;
  if (ex->hsolver) PetscCallExternal(HYPRE_StructSMGDestroy, ex->hsolver);
  PetscCall(PetscCommRestoreComm(PetscObjectComm((PetscObject)pc), &ex->hcomm));
  PetscCall(PetscFree(pc->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCView_SMG(PC pc, PetscViewer viewer)
{
  PetscBool iascii;
  PC_SMG   *ex = (PC_SMG *)pc->data;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  HYPRE SMG preconditioning\n"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    max iterations %" PetscInt_FMT "\n", ex->its));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    tolerance %g\n", ex->tol));
    PetscCall(PetscViewerASCIIPrintf(viewer, "    number pre-relax %" PetscInt_FMT " post-relax %" PetscInt_FMT "\n", ex->num_pre_relax, ex->num_post_relax));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCSetFromOptions_SMG(PC pc, PetscOptionItems *PetscOptionsObject)
{
  PC_SMG *ex = (PC_SMG *)pc->data;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "SMG options");

  PetscCall(PetscOptionsInt("-pc_smg_its", "Number of iterations of SMG to use as preconditioner", "HYPRE_StructSMGSetMaxIter", ex->its, &ex->its, NULL));
  PetscCall(PetscOptionsInt("-pc_smg_num_pre_relax", "Number of smoothing steps before coarse grid", "HYPRE_StructSMGSetNumPreRelax", ex->num_pre_relax, &ex->num_pre_relax, NULL));
  PetscCall(PetscOptionsInt("-pc_smg_num_post_relax", "Number of smoothing steps after coarse grid", "HYPRE_StructSMGSetNumPostRelax", ex->num_post_relax, &ex->num_post_relax, NULL));
  PetscCall(PetscOptionsReal("-pc_smg_tol", "Tolerance of SMG", "HYPRE_StructSMGSetTol", ex->tol, &ex->tol, NULL));

  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCApply_SMG(PC pc, Vec x, Vec y)
{
  PC_SMG            *ex = (PC_SMG *)pc->data;
  PetscScalar       *yy;
  const PetscScalar *xx;
  PetscInt           ilower[3], iupper[3];
  HYPRE_Int          hlower[3], hupper[3];
  Mat_HYPREStruct   *mx = (Mat_HYPREStruct *)(pc->pmat->data);

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  PetscCall(DMDAGetCorners(mx->da, &ilower[0], &ilower[1], &ilower[2], &iupper[0], &iupper[1], &iupper[2]));
  /* when HYPRE_MIXEDINT is defined, sizeof(HYPRE_Int) == 32 */
  iupper[0] += ilower[0] - 1;
  iupper[1] += ilower[1] - 1;
  iupper[2] += ilower[2] - 1;
  hlower[0] = (HYPRE_Int)ilower[0];
  hlower[1] = (HYPRE_Int)ilower[1];
  hlower[2] = (HYPRE_Int)ilower[2];
  hupper[0] = (HYPRE_Int)iupper[0];
  hupper[1] = (HYPRE_Int)iupper[1];
  hupper[2] = (HYPRE_Int)iupper[2];

  /* copy x values over to hypre */
  PetscCallExternal(HYPRE_StructVectorSetConstantValues, mx->hb, 0.0);
  PetscCall(VecGetArrayRead(x, &xx));
  PetscCallExternal(HYPRE_StructVectorSetBoxValues, mx->hb, hlower, hupper, (HYPRE_Complex *)xx);
  PetscCall(VecRestoreArrayRead(x, &xx));
  PetscCallExternal(HYPRE_StructVectorAssemble, mx->hb);
  PetscCallExternal(HYPRE_StructSMGSolve, ex->hsolver, mx->hmat, mx->hb, mx->hx);

  /* copy solution values back to PETSc */
  PetscCall(VecGetArray(y, &yy));
  PetscCallExternal(HYPRE_StructVectorGetBoxValues, mx->hx, hlower, hupper, (HYPRE_Complex *)yy);
  PetscCall(VecRestoreArray(y, &yy));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PCApplyRichardson_SMG(PC pc, Vec b, Vec y, Vec w, PetscReal rtol, PetscReal abstol, PetscReal dtol, PetscInt its, PetscBool guesszero, PetscInt *outits, PCRichardsonConvergedReason *reason)
{
  PC_SMG   *jac = (PC_SMG *)pc->data;
  HYPRE_Int oits;

  PetscFunctionBegin;
  PetscCall(PetscCitationsRegister(hypreCitation, &cite));
  PetscCallExternal(HYPRE_StructSMGSetMaxIter, jac->hsolver, its * jac->its);
  PetscCallExternal(HYPRE_StructSMGSetTol, jac->hsolver, rtol);

  PetscCall(PCApply_SMG(pc, b, y));
  PetscCallExternal(HYPRE_StructSMGGetNumIterations, jac->hsolver, &oits);
  *outits = oits;
  if (oits == its) *reason = PCRICHARDSON_CONVERGED_ITS;
  else *reason = PCRICHARDSON_CONVERGED_RTOL;
  PetscCallExternal(HYPRE_StructSMGSetTol, jac->hsolver, jac->tol);
  PetscCallExternal(HYPRE_StructSMGSetMaxIter, jac->hsolver, jac->its);
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCSetUp_SMG(PC pc)
{
  PetscInt         i, dim;
  PC_SMG          *ex = (PC_SMG *)pc->data;
  Mat_HYPREStruct *mx = (Mat_HYPREStruct *)(pc->pmat->data);
  PetscBool        flg;
  DMBoundaryType   p[3];
  PetscInt         M[3];

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)pc->pmat, MATHYPRESTRUCT, &flg));
  PetscCheck(flg, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_INCOMP, "Must use MATHYPRESTRUCT with this preconditioner");

  PetscCall(DMDAGetInfo(mx->da, &dim, &M[0], &M[1], &M[2], 0, 0, 0, 0, 0, &p[0], &p[1], &p[2], 0));
  // Check if power of 2 in periodic directions
  for (i = 0; i < dim; i++) {
    if (((M[i] & (M[i] - 1)) != 0) && (p[i] == DM_BOUNDARY_PERIODIC)) {
      SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_INCOMP, "With SMG, the number of points in a periodic direction must be a power of 2, but is here %" PetscInt_FMT ".", M[i]);
    }
  }

  /* create the hypre solver object and set its information */
  if (ex->hsolver) PetscCallExternal(HYPRE_StructSMGDestroy, (ex->hsolver));
  PetscCallExternal(HYPRE_StructSMGCreate, ex->hcomm, &ex->hsolver);
  // The hypre options must be set here and not in SetFromOptions because it is created here!
  PetscCallExternal(HYPRE_StructSMGSetMaxIter, ex->hsolver, ex->its);
  PetscCallExternal(HYPRE_StructSMGSetNumPreRelax, ex->hsolver, ex->num_pre_relax);
  PetscCallExternal(HYPRE_StructSMGSetNumPostRelax, ex->hsolver, ex->num_post_relax);
  PetscCallExternal(HYPRE_StructSMGSetTol, ex->hsolver, ex->tol);

  PetscCallExternal(HYPRE_StructSMGSetup, ex->hsolver, mx->hmat, mx->hb, mx->hx);
  PetscCallExternal(HYPRE_StructSMGSetZeroGuess, ex->hsolver);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
     PCSMG - the hypre (structured grid) SMG multigrid solver

   Level: advanced

   Options Database Keys:
+ -pc_smg_its <its> - number of iterations of SMG to use as preconditioner
. -pc_smg_num_pre_relax <steps> - number of smoothing steps before coarse grid
. -pc_smg_num_post_relax <steps> - number of smoothing steps after coarse grid
- -pc_smg_tol <tol> - tolerance of SMG

   Notes:
   This is for CELL-centered descretizations

   This must be used with the `MATHYPRESTRUCT` `MatType`.

   This does not provide all the functionality of  hypre's SMG solver, it supports only one block per process defined by a PETSc `DMDA`.

   See `PCSYSPFMG`, `PCSMG`, `PCPFMG`, and `PCHYPRE` for access to hypre's other preconditioners

.seealso:  `PCMG`, `MATHYPRESTRUCT`, `PCPFMG`, `PCSYSPFMG`, `PCHYPRE`, `PCGAMG`
M*/

PETSC_EXTERN PetscErrorCode PCCreate_SMG(PC pc)
{
  PC_SMG *ex;

  PetscFunctionBegin;
  PetscCall(PetscNew(&ex));
  pc->data = ex;

  ex->its            = 1;
  ex->tol            = 1.e-8;
  ex->num_pre_relax  = 1;
  ex->num_post_relax = 1;

  pc->ops->setfromoptions  = PCSetFromOptions_SMG;
  pc->ops->view            = PCView_SMG;
  pc->ops->destroy         = PCDestroy_SMG;
  pc->ops->apply           = PCApply_SMG;
  pc->ops->applyrichardson = PCApplyRichardson_SMG;
  pc->ops->setup           = PCSetUp_SMG;

  PetscCall(PetscCommGetComm(PetscObjectComm((PetscObject)pc), &ex->hcomm));
  PetscCallExternal(HYPRE_StructSMGCreate, ex->hcomm, &ex->hsolver);
  PetscFunctionReturn(PETSC_SUCCESS);
}

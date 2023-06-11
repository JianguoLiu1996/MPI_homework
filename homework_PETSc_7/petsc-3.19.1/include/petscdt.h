/*
  Common tools for constructing discretizations
*/
#ifndef PETSCDT_H
#define PETSCDT_H

#include <petscsys.h>
#include <petscdmtypes.h>
#include <petscistypes.h>

/* SUBMANSEC = DT */

PETSC_EXTERN PetscClassId PETSCQUADRATURE_CLASSID;

/*S
  PetscQuadrature - Quadrature rule for numerical integration.

  Level: beginner

.seealso: `PetscQuadratureCreate()`, `PetscQuadratureDestroy()`
S*/
typedef struct _p_PetscQuadrature *PetscQuadrature;

/*E
  PetscGaussLobattoLegendreCreateType - algorithm used to compute the Gauss-Lobatto-Legendre nodes and weights

  Values:
+  `PETSCGAUSSLOBATTOLEGENDRE_VIA_LINEAR_ALGEBRA` - compute the nodes via linear algebra
-  `PETSCGAUSSLOBATTOLEGENDRE_VIA_NEWTON` - compute the nodes by solving a nonlinear equation with Newton's method

  Level: intermediate

.seealso: `PetscQuadrature`
E*/
typedef enum {
  PETSCGAUSSLOBATTOLEGENDRE_VIA_LINEAR_ALGEBRA,
  PETSCGAUSSLOBATTOLEGENDRE_VIA_NEWTON
} PetscGaussLobattoLegendreCreateType;

/*E
  PetscDTNodeType - A description of strategies for generating nodes (both
  quadrature nodes and nodes for Lagrange polynomials)

   Values:
+  `PETSCDTNODES_DEFAULT` - Nodes chosen by PETSc
.  `PETSCDTNODES_GAUSSJACOBI` - Nodes at either Gauss-Jacobi or Gauss-Lobatto-Jacobi quadrature points
.  `PETSCDTNODES_EQUISPACED` - Nodes equispaced either including the endpoints or excluding them
-  `PETSCDTNODES_TANHSINH` - Nodes at Tanh-Sinh quadrature points

  Level: intermediate

  Note:
  A `PetscDTNodeType` can be paired with a `PetscBool` to indicate whether
  the nodes include endpoints or not, and in the case of `PETSCDT_GAUSSJACOBI`
  with exponents for the weight function.

.seealso: `PetscQuadrature`
E*/
typedef enum {
  PETSCDTNODES_DEFAULT = -1,
  PETSCDTNODES_GAUSSJACOBI,
  PETSCDTNODES_EQUISPACED,
  PETSCDTNODES_TANHSINH
} PetscDTNodeType;

PETSC_EXTERN const char *const *const PetscDTNodeTypes;

/*E
  PetscDTSimplexQuadratureType - A description of classes of quadrature rules for simplices

  Values:
+  `PETSCDTSIMPLEXQUAD_DEFAULT` - Quadrature rule chosen by PETSc
.  `PETSCDTSIMPLEXQUAD_CONIC`   - Quadrature rules constructed as
                                conically-warped tensor products of 1D
                                Gauss-Jacobi quadrature rules.  These are
                                explicitly computable in any dimension for any
                                degree, and the tensor-product structure can be
                                exploited by sum-factorization methods, but
                                they are not efficient in terms of nodes per
                                polynomial degree.
-  `PETSCDTSIMPLEXQUAD_MINSYM`  - Quadrature rules that are fully symmetric
                                (symmetries of the simplex preserve the nodes
                                and weights) with minimal (or near minimal)
                                number of nodes.  In dimensions higher than 1
                                these are not simple to compute, so lookup
                                tables are used.

  Level: intermediate

.seealso: `PetscQuadrature`, `PetscDTSimplexQuadrature()`
E*/
typedef enum {
  PETSCDTSIMPLEXQUAD_DEFAULT = -1,
  PETSCDTSIMPLEXQUAD_CONIC   = 0,
  PETSCDTSIMPLEXQUAD_MINSYM
} PetscDTSimplexQuadratureType;

PETSC_EXTERN const char *const *const PetscDTSimplexQuadratureTypes;

PETSC_EXTERN PetscErrorCode PetscQuadratureCreate(MPI_Comm, PetscQuadrature *);
PETSC_EXTERN PetscErrorCode PetscQuadratureDuplicate(PetscQuadrature, PetscQuadrature *);
PETSC_EXTERN PetscErrorCode PetscQuadratureGetCellType(PetscQuadrature, DMPolytopeType *);
PETSC_EXTERN PetscErrorCode PetscQuadratureSetCellType(PetscQuadrature, DMPolytopeType);
PETSC_EXTERN PetscErrorCode PetscQuadratureGetOrder(PetscQuadrature, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscQuadratureSetOrder(PetscQuadrature, PetscInt);
PETSC_EXTERN PetscErrorCode PetscQuadratureGetNumComponents(PetscQuadrature, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscQuadratureSetNumComponents(PetscQuadrature, PetscInt);
PETSC_EXTERN PetscErrorCode PetscQuadratureEqual(PetscQuadrature, PetscQuadrature, PetscBool *);
PETSC_EXTERN PetscErrorCode PetscQuadratureGetData(PetscQuadrature, PetscInt *, PetscInt *, PetscInt *, const PetscReal *[], const PetscReal *[]);
PETSC_EXTERN PetscErrorCode PetscQuadratureSetData(PetscQuadrature, PetscInt, PetscInt, PetscInt, const PetscReal[], const PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscQuadratureView(PetscQuadrature, PetscViewer);
PETSC_EXTERN PetscErrorCode PetscQuadratureDestroy(PetscQuadrature *);

PETSC_EXTERN PetscErrorCode PetscDTTensorQuadratureCreate(PetscQuadrature, PetscQuadrature, PetscQuadrature *);
PETSC_EXTERN PetscErrorCode PetscQuadratureExpandComposite(PetscQuadrature, PetscInt, const PetscReal[], const PetscReal[], PetscQuadrature *);
PETSC_EXTERN PetscErrorCode PetscQuadratureComputePermutations(PetscQuadrature, PetscInt *, IS *[]);

PETSC_EXTERN PetscErrorCode PetscQuadraturePushForward(PetscQuadrature, PetscInt, const PetscReal[], const PetscReal[], const PetscReal[], PetscInt, PetscQuadrature *);

PETSC_EXTERN PetscErrorCode PetscDTLegendreEval(PetscInt, const PetscReal *, PetscInt, const PetscInt *, PetscReal *, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTJacobiNorm(PetscReal, PetscReal, PetscInt, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTJacobiEval(PetscInt, PetscReal, PetscReal, const PetscReal *, PetscInt, const PetscInt *, PetscReal *, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTJacobiEvalJet(PetscReal, PetscReal, PetscInt, const PetscReal[], PetscInt, PetscInt, PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscDTPKDEvalJet(PetscInt, PetscInt, const PetscReal[], PetscInt, PetscInt, PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscDTPTrimmedSize(PetscInt, PetscInt, PetscInt, PetscInt *);
PETSC_EXTERN PetscErrorCode PetscDTPTrimmedEvalJet(PetscInt, PetscInt, const PetscReal[], PetscInt, PetscInt, PetscInt, PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscDTGaussQuadrature(PetscInt, PetscReal, PetscReal, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTGaussJacobiQuadrature(PetscInt, PetscReal, PetscReal, PetscReal, PetscReal, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTGaussLobattoJacobiQuadrature(PetscInt, PetscReal, PetscReal, PetscReal, PetscReal, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTGaussLobattoLegendreQuadrature(PetscInt, PetscGaussLobattoLegendreCreateType, PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTReconstructPoly(PetscInt, PetscInt, const PetscReal *, PetscInt, const PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTGaussTensorQuadrature(PetscInt, PetscInt, PetscInt, PetscReal, PetscReal, PetscQuadrature *);
PETSC_EXTERN PetscErrorCode PetscDTStroudConicalQuadrature(PetscInt, PetscInt, PetscInt, PetscReal, PetscReal, PetscQuadrature *);
PETSC_EXTERN PetscErrorCode PetscDTSimplexQuadrature(PetscInt, PetscInt, PetscDTSimplexQuadratureType, PetscQuadrature *);
PETSC_EXTERN PetscErrorCode PetscDTCreateDefaultQuadrature(DMPolytopeType, PetscInt, PetscQuadrature *, PetscQuadrature *);

PETSC_EXTERN PetscErrorCode PetscDTTanhSinhTensorQuadrature(PetscInt, PetscInt, PetscReal, PetscReal, PetscQuadrature *);
PETSC_EXTERN PetscErrorCode PetscDTTanhSinhIntegrate(void (*)(const PetscReal[], void *, PetscReal *), PetscReal, PetscReal, PetscInt, void *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTTanhSinhIntegrateMPFR(void (*)(const PetscReal[], void *, PetscReal *), PetscReal, PetscReal, PetscInt, void *, PetscReal *);

PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreIntegrate(PetscInt, PetscReal *, PetscReal *, const PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreElementLaplacianCreate(PetscInt, PetscReal *, PetscReal *, PetscReal ***);
PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreElementLaplacianDestroy(PetscInt, PetscReal *, PetscReal *, PetscReal ***);
PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreElementGradientCreate(PetscInt, PetscReal *, PetscReal *, PetscReal ***, PetscReal ***);
PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreElementGradientDestroy(PetscInt, PetscReal *, PetscReal *, PetscReal ***, PetscReal ***);
PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreElementAdvectionCreate(PetscInt, PetscReal *, PetscReal *, PetscReal ***);
PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreElementAdvectionDestroy(PetscInt, PetscReal *, PetscReal *, PetscReal ***);
PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreElementMassCreate(PetscInt, PetscReal *, PetscReal *, PetscReal ***);
PETSC_EXTERN PetscErrorCode PetscGaussLobattoLegendreElementMassDestroy(PetscInt, PetscReal *, PetscReal *, PetscReal ***);

PETSC_EXTERN PetscErrorCode PetscDTAltVApply(PetscInt, PetscInt, const PetscReal *, const PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTAltVWedge(PetscInt, PetscInt, PetscInt, const PetscReal *, const PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTAltVWedgeMatrix(PetscInt, PetscInt, PetscInt, const PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTAltVPullback(PetscInt, PetscInt, const PetscReal *, PetscInt, const PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTAltVPullbackMatrix(PetscInt, PetscInt, const PetscReal *, PetscInt, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTAltVInterior(PetscInt, PetscInt, const PetscReal *, const PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTAltVInteriorMatrix(PetscInt, PetscInt, const PetscReal *, PetscReal *);
PETSC_EXTERN PetscErrorCode PetscDTAltVInteriorPattern(PetscInt, PetscInt, PetscInt (*)[3]);
PETSC_EXTERN PetscErrorCode PetscDTAltVStar(PetscInt, PetscInt, PetscInt, const PetscReal *, PetscReal *);

PETSC_EXTERN PetscErrorCode PetscDTBaryToIndex(PetscInt, PetscInt, const PetscInt[], PetscInt *);
PETSC_EXTERN PetscErrorCode PetscDTIndexToBary(PetscInt, PetscInt, PetscInt, PetscInt[]);
PETSC_EXTERN PetscErrorCode PetscDTGradedOrderToIndex(PetscInt, const PetscInt[], PetscInt *);
PETSC_EXTERN PetscErrorCode PetscDTIndexToGradedOrder(PetscInt, PetscInt, PetscInt[]);

#if defined(PETSC_USE_64BIT_INDICES)
  #define PETSC_FACTORIAL_MAX 20
  #define PETSC_BINOMIAL_MAX  61
#else
  #define PETSC_FACTORIAL_MAX 12
  #define PETSC_BINOMIAL_MAX  29
#endif

/*MC
   PetscDTFactorial - Approximate n! as a real number

   Input Parameter:
.  n - a non-negative integer

   Output Parameter:
.  factorial - n!

   Level: beginner

.seealso: `PetscDTFactorialInt()`, `PetscDTBinomialInt()`, `PetscDTBinomial()`
M*/
static inline PetscErrorCode PetscDTFactorial(PetscInt n, PetscReal *factorial)
{
  PetscReal f = 1.0;

  PetscFunctionBegin;
  *factorial = -1.0;
  PetscCheck(n >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Factorial called with negative number %" PetscInt_FMT, n);
  for (PetscInt i = 1; i < n + 1; ++i) f *= (PetscReal)i;
  *factorial = f;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscDTFactorialInt - Compute n! as an integer

   Input Parameter:
.  n - a non-negative integer

   Output Parameter:
.  factorial - n!

   Level: beginner

   Note:
   This is limited to n such that n! can be represented by `PetscInt`, which is 12 if `PetscInt` is a signed 32-bit integer and 20 if `PetscInt` is a signed 64-bit integer.

.seealso: `PetscDTFactorial()`, `PetscDTBinomialInt()`, `PetscDTBinomial()`
M*/
static inline PetscErrorCode PetscDTFactorialInt(PetscInt n, PetscInt *factorial)
{
  PetscInt facLookup[13] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800, 39916800, 479001600};

  PetscFunctionBegin;
  *factorial = -1;
  PetscCheck(n >= 0 && n <= PETSC_FACTORIAL_MAX, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Number of elements %" PetscInt_FMT " is not in supported range [0,%d]", n, PETSC_FACTORIAL_MAX);
  if (n <= 12) {
    *factorial = facLookup[n];
  } else {
    PetscInt f = facLookup[12];
    PetscInt i;

    for (i = 13; i < n + 1; ++i) f *= i;
    *factorial = f;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscDTBinomial - Approximate the binomial coefficient "n choose k"

   Input Parameters:
+  n - a non-negative integer
-  k - an integer between 0 and n, inclusive

   Output Parameter:
.  binomial - approximation of the binomial coefficient n choose k

   Level: beginner

.seealso: `PetscDTFactorial()`, `PetscDTFactorialInt()`, `PetscDTBinomialInt()`, `PetscDTEnumPerm()`
M*/
static inline PetscErrorCode PetscDTBinomial(PetscInt n, PetscInt k, PetscReal *binomial)
{
  PetscFunctionBeginHot;
  *binomial = -1.0;
  PetscCheck(n >= 0 && k >= 0 && k <= n, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Binomial arguments (%" PetscInt_FMT " %" PetscInt_FMT ") must be non-negative, k <= n", n, k);
  if (n <= 3) {
    PetscInt binomLookup[4][4] = {
      {1, 0, 0, 0},
      {1, 1, 0, 0},
      {1, 2, 1, 0},
      {1, 3, 3, 1}
    };

    *binomial = (PetscReal)binomLookup[n][k];
  } else {
    PetscReal binom = 1.0;

    k = PetscMin(k, n - k);
    for (PetscInt i = 0; i < k; i++) binom = (binom * (PetscReal)(n - i)) / (PetscReal)(i + 1);
    *binomial = binom;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscDTBinomialInt - Compute the binomial coefficient "n choose k"

   Input Parameters:
+  n - a non-negative integer
-  k - an integer between 0 and n, inclusive

   Output Parameter:
.  binomial - the binomial coefficient n choose k

   Level: beginner

   Note:
   This is limited by integers that can be represented by `PetscInt`.

   Use `PetscDTBinomial()` for real number approximations of larger values

.seealso: `PetscDTFactorial()`, `PetscDTFactorialInt()`, `PetscDTBinomial()`, `PetscDTEnumPerm()`
M*/
static inline PetscErrorCode PetscDTBinomialInt(PetscInt n, PetscInt k, PetscInt *binomial)
{
  PetscInt bin;

  PetscFunctionBegin;
  *binomial = -1;
  PetscCheck(n >= 0 && k >= 0 && k <= n, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Binomial arguments (%" PetscInt_FMT " %" PetscInt_FMT ") must be non-negative, k <= n", n, k);
  PetscCheck(n <= PETSC_BINOMIAL_MAX, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Binomial elements %" PetscInt_FMT " is larger than max for PetscInt, %d", n, PETSC_BINOMIAL_MAX);
  if (n <= 3) {
    PetscInt binomLookup[4][4] = {
      {1, 0, 0, 0},
      {1, 1, 0, 0},
      {1, 2, 1, 0},
      {1, 3, 3, 1}
    };

    bin = binomLookup[n][k];
  } else {
    PetscInt binom = 1;

    k = PetscMin(k, n - k);
    for (PetscInt i = 0; i < k; i++) binom = (binom * (n - i)) / (i + 1);
    bin = binom;
  }
  *binomial = bin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscDTEnumPerm - Get a permutation of `n` integers from its encoding into the integers [0, n!) as a sequence of swaps.

   Input Parameters:
+  n - a non-negative integer (see note about limits below)
-  k - an integer in [0, n!)

   Output Parameters:
+  perm - the permuted list of the integers [0, ..., n-1]
-  isOdd - if not `NULL`, returns whether the permutation used an even or odd number of swaps.

   Level: intermediate

   Notes:
   A permutation can be described by the operations that convert the lists [0, 1, ..., n-1] into the permutation,
   by a sequence of swaps, where the ith step swaps whatever number is in ith position with a number that is in
   some position j >= i.  This swap is encoded as the difference (j - i).  The difference d_i at step i is less than
   (n - i).  This sequence of n-1 differences [d_0, ..., d_{n-2}] is encoded as the number
   (n-1)! * d_0 + (n-2)! * d_1 + ... + 1! * d_{n-2}.

   Limited to `n` such that `n`! can be represented by `PetscInt`, which is 12 if `PetscInt` is a signed 32-bit integer and 20 if `PetscInt` is a signed 64-bit integer.

.seealso: `PetscDTFactorial()`, `PetscDTFactorialInt()`, `PetscDTBinomial()`, `PetscDTBinomialInt()`, `PetscDTPermIndex()`
M*/
static inline PetscErrorCode PetscDTEnumPerm(PetscInt n, PetscInt k, PetscInt *perm, PetscBool *isOdd)
{
  PetscInt  odd = 0;
  PetscInt  i;
  PetscInt  work[PETSC_FACTORIAL_MAX];
  PetscInt *w;

  PetscFunctionBegin;
  if (isOdd) *isOdd = PETSC_FALSE;
  PetscCheck(n >= 0 && n <= PETSC_FACTORIAL_MAX, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Number of elements %" PetscInt_FMT " is not in supported range [0,%d]", n, PETSC_FACTORIAL_MAX);
  w = &work[n - 2];
  for (i = 2; i <= n; i++) {
    *(w--) = k % i;
    k /= i;
  }
  for (i = 0; i < n; i++) perm[i] = i;
  for (i = 0; i < n - 1; i++) {
    PetscInt s    = work[i];
    PetscInt swap = perm[i];

    perm[i]     = perm[i + s];
    perm[i + s] = swap;
    odd ^= (!!s);
  }
  if (isOdd) *isOdd = odd ? PETSC_TRUE : PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscDTPermIndex - Encode a permutation of n into an integer in [0, n!).  This inverts `PetscDTEnumPerm()`.

   Input Parameters:
+  n - a non-negative integer (see note about limits below)
-  perm - the permuted list of the integers [0, ..., n-1]

   Output Parameters:
+  k - an integer in [0, n!)
-  isOdd - if not `NULL`, returns whether the permutation used an even or odd number of swaps.

   Level: beginner

   Note:
   Limited to `n` such that `n`! can be represented by `PetscInt`, which is 12 if `PetscInt` is a signed 32-bit integer and 20 if `PetscInt` is a signed 64-bit integer.

.seealso: `PetscDTFactorial()`, `PetscDTFactorialInt()`, `PetscDTBinomial()`, `PetscDTBinomialInt()`, `PetscDTEnumPerm()`
M*/
static inline PetscErrorCode PetscDTPermIndex(PetscInt n, const PetscInt *perm, PetscInt *k, PetscBool *isOdd)
{
  PetscInt odd = 0;
  PetscInt i, idx;
  PetscInt work[PETSC_FACTORIAL_MAX];
  PetscInt iwork[PETSC_FACTORIAL_MAX];

  PetscFunctionBeginHot;
  *k = -1;
  if (isOdd) *isOdd = PETSC_FALSE;
  PetscCheck(n >= 0 && n <= PETSC_FACTORIAL_MAX, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Number of elements %" PetscInt_FMT " is not in supported range [0,%d]", n, PETSC_FACTORIAL_MAX);
  for (i = 0; i < n; i++) work[i] = i;  /* partial permutation */
  for (i = 0; i < n; i++) iwork[i] = i; /* partial permutation inverse */
  for (idx = 0, i = 0; i < n - 1; i++) {
    PetscInt j    = perm[i];
    PetscInt icur = work[i];
    PetscInt jloc = iwork[j];
    PetscInt diff = jloc - i;

    idx = idx * (n - i) + diff;
    /* swap (i, jloc) */
    work[i]     = j;
    work[jloc]  = icur;
    iwork[j]    = i;
    iwork[icur] = jloc;
    odd ^= (!!diff);
  }
  *k = idx;
  if (isOdd) *isOdd = odd ? PETSC_TRUE : PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscDTEnumSubset - Get an ordered subset of the integers [0, ..., n - 1] from its encoding as an integers in [0, n choose k).
   The encoding is in lexicographic order.

   Input Parameters:
+  n - a non-negative integer (see note about limits below)
.  k - an integer in [0, n]
-  j - an index in [0, n choose k)

   Output Parameter:
.  subset - the jth subset of size k of the integers [0, ..., n - 1]

   Level: beginner

   Note:
   Limited by arguments such that `n` choose `k` can be represented by `PetscInt`

.seealso: `PetscDTSubsetIndex()`, `PetscDTFactorial()`, `PetscDTFactorialInt()`, `PetscDTBinomial()`, `PetscDTBinomialInt()`, `PetscDTEnumPerm()`, `PetscDTPermIndex()`
M*/
static inline PetscErrorCode PetscDTEnumSubset(PetscInt n, PetscInt k, PetscInt j, PetscInt *subset)
{
  PetscInt Nk;

  PetscFunctionBeginHot;
  PetscCall(PetscDTBinomialInt(n, k, &Nk));
  for (PetscInt i = 0, l = 0; i < n && l < k; i++) {
    PetscInt Nminuskminus = (Nk * (k - l)) / (n - i);
    PetscInt Nminusk      = Nk - Nminuskminus;

    if (j < Nminuskminus) {
      subset[l++] = i;
      Nk          = Nminuskminus;
    } else {
      j -= Nminuskminus;
      Nk = Nminusk;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscDTSubsetIndex - Convert an ordered subset of k integers from the set [0, ..., n - 1] to its encoding as an integers in [0, n choose k) in lexicographic order.
   This is the inverse of `PetscDTEnumSubset`.

   Input Parameters:
+  n - a non-negative integer (see note about limits below)
.  k - an integer in [0, n]
-  subset - an ordered subset of the integers [0, ..., n - 1]

   Output Parameter:
.  index - the rank of the subset in lexicographic order

   Level: beginner

   Note:
   Limited by arguments such that `n` choose `k` can be represented by `PetscInt`

.seealso: `PetscDTEnumSubset()`, `PetscDTFactorial()`, `PetscDTFactorialInt()`, `PetscDTBinomial()`, `PetscDTBinomialInt()`, `PetscDTEnumPerm()`, `PetscDTPermIndex()`
M*/
static inline PetscErrorCode PetscDTSubsetIndex(PetscInt n, PetscInt k, const PetscInt *subset, PetscInt *index)
{
  PetscInt j = 0, Nk;

  PetscFunctionBegin;
  *index = -1;
  PetscCall(PetscDTBinomialInt(n, k, &Nk));
  for (PetscInt i = 0, l = 0; i < n && l < k; i++) {
    PetscInt Nminuskminus = (Nk * (k - l)) / (n - i);
    PetscInt Nminusk      = Nk - Nminuskminus;

    if (subset[l] == i) {
      l++;
      Nk = Nminuskminus;
    } else {
      j += Nminuskminus;
      Nk = Nminusk;
    }
  }
  *index = j;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PetscDTEnumSubset - Split the integers [0, ..., n - 1] into two complementary ordered subsets, the first subset of size k and being the jth subset of that size in lexicographic order.

   Input Parameters:
+  n - a non-negative integer (see note about limits below)
.  k - an integer in [0, n]
-  j - an index in [0, n choose k)

   Output Parameters:
+  perm - the jth subset of size k of the integers [0, ..., n - 1], followed by its complementary set.
-  isOdd - if not `NULL`, return whether perm is an even or odd permutation.

   Level: beginner

   Note:
   Limited by arguments such that `n` choose `k` can be represented by `PetscInt`

.seealso: `PetscDTEnumSubset()`, `PetscDTSubsetIndex()`, `PetscDTFactorial()`, `PetscDTFactorialInt()`, `PetscDTBinomial()`, `PetscDTBinomialInt()`, `PetscDTEnumPerm()`,
          `PetscDTPermIndex()`
M*/
static inline PetscErrorCode PetscDTEnumSplit(PetscInt n, PetscInt k, PetscInt j, PetscInt *perm, PetscBool *isOdd)
{
  PetscInt  i, l, m, Nk, odd = 0;
  PetscInt *subcomp = perm + k;

  PetscFunctionBegin;
  if (isOdd) *isOdd = PETSC_FALSE;
  PetscCall(PetscDTBinomialInt(n, k, &Nk));
  for (i = 0, l = 0, m = 0; i < n && l < k; i++) {
    PetscInt Nminuskminus = (Nk * (k - l)) / (n - i);
    PetscInt Nminusk      = Nk - Nminuskminus;

    if (j < Nminuskminus) {
      perm[l++] = i;
      Nk        = Nminuskminus;
    } else {
      subcomp[m++] = i;
      j -= Nminuskminus;
      odd ^= ((k - l) & 1);
      Nk = Nminusk;
    }
  }
  for (; i < n; i++) subcomp[m++] = i;
  if (isOdd) *isOdd = odd ? PETSC_TRUE : PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

struct _p_PetscTabulation {
  PetscInt    K;    /* Indicates a k-jet, namely tabulated derivatives up to order k */
  PetscInt    Nr;   /* The number of tabulation replicas (often 1) */
  PetscInt    Np;   /* The number of tabulation points in a replica */
  PetscInt    Nb;   /* The number of functions tabulated */
  PetscInt    Nc;   /* The number of function components */
  PetscInt    cdim; /* The coordinate dimension */
  PetscReal **T;    /* The tabulation T[K] of functions and their derivatives
                       T[0] = B[Nr*Np][Nb][Nc]:             The basis function values at quadrature points
                       T[1] = D[Nr*Np][Nb][Nc][cdim]:       The basis function derivatives at quadrature points
                       T[2] = H[Nr*Np][Nb][Nc][cdim][cdim]: The basis function second derivatives at quadrature points */
};
typedef struct _p_PetscTabulation *PetscTabulation;

typedef PetscErrorCode (*PetscProbFunc)(const PetscReal[], const PetscReal[], PetscReal[]);

typedef enum {
  DTPROB_DENSITY_CONSTANT,
  DTPROB_DENSITY_GAUSSIAN,
  DTPROB_DENSITY_MAXWELL_BOLTZMANN,
  DTPROB_NUM_DENSITY
} DTProbDensityType;
PETSC_EXTERN const char *const DTProbDensityTypes[];

PETSC_EXTERN PetscErrorCode PetscPDFMaxwellBoltzmann1D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscCDFMaxwellBoltzmann1D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFMaxwellBoltzmann2D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscCDFMaxwellBoltzmann2D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFMaxwellBoltzmann3D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscCDFMaxwellBoltzmann3D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFGaussian1D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscCDFGaussian1D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFSampleGaussian1D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFGaussian2D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFSampleGaussian2D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFGaussian3D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFSampleGaussian3D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFConstant1D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscCDFConstant1D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFSampleConstant1D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFConstant2D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscCDFConstant2D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFSampleConstant2D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFConstant3D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscCDFConstant3D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscPDFSampleConstant3D(const PetscReal[], const PetscReal[], PetscReal[]);
PETSC_EXTERN PetscErrorCode PetscProbCreateFromOptions(PetscInt, const char[], const char[], PetscProbFunc *, PetscProbFunc *, PetscProbFunc *);

#include <petscvec.h>

PETSC_EXTERN PetscErrorCode PetscProbComputeKSStatistic(Vec, PetscProbFunc, PetscReal *);

#endif

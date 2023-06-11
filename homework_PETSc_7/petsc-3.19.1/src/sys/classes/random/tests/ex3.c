
static char help[] = "Run Birthday Spacing Tests for PetscRandom.\n\n";

#include <petscsys.h>
#include <petscviewer.h>

/* L'Ecuyer & Simard, 2001.
 * "On the performance of birthday spacings tests with certain families of random number generators"
 * https://doi.org/10.1016/S0378-4754(00)00253-6
 */

static int PetscInt64Compare(const void *a, const void *b)
{
  PetscInt64 A = *((const PetscInt64 *)a);
  PetscInt64 B = *((const PetscInt64 *)b);
  if (A < B) return -1;
  if (A == B) return 0;
  return 1;
}

static PetscErrorCode PoissonTailProbability(PetscReal lambda, PetscInt Y, PetscReal *prob)
{
  PetscReal p = 1.;
  PetscReal logLambda;
  PetscInt  i;
  PetscReal logFact = 0.;

  PetscFunctionBegin;
  logLambda = PetscLogReal(lambda);
  for (i = 0; i < Y; i++) {
    PetscReal exponent = -lambda + i * logLambda - logFact;

    logFact += PetscLogReal(i + 1);

    p -= PetscExpReal(exponent);
  }
  *prob = p;
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  PetscMPIInt size;
  PetscInt    log2d, log2n, t, N, Y;
  PetscInt64  d, k, *X;
  size_t      n, i;
  PetscReal   lambda, p;
  PetscRandom random;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  t     = 8;
  log2d = 7;
  log2n = 20;
  PetscOptionsBegin(PETSC_COMM_WORLD, NULL, "Birthday spacing test parameters", "PetscRandom");
  PetscCall(PetscOptionsInt("-t", "t, the dimension of the sample space", "ex3.c", t, &t, NULL));
  PetscCall(PetscOptionsInt("-log2d", "The log of d, the number of bins per direction", "ex3.c", log2d, &log2d, NULL));
  PetscCall(PetscOptionsInt("-log2n", "The log of n, the number of samples per process", "ex3.c", log2n, &log2n, NULL));
  PetscOptionsEnd();

  PetscCheck((size_t)log2d * t <= sizeof(PetscInt64) * 8 - 2, PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "The number of bins (2^%" PetscInt_FMT ") is too big for PetscInt64.", log2d * t);
  d = ((PetscInt64)1) << log2d;
  k = ((PetscInt64)1) << (log2d * t);
  PetscCheck((size_t)log2n <= sizeof(size_t) * 8 - 1, PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "The number of samples per process (2^%" PetscInt_FMT ") is too big for size_t.", log2n);
  n = ((size_t)1) << log2n;
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));
  N      = size;
  lambda = PetscPowRealInt(2., (3 * log2n - (2 + log2d * t)));

  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Generating %zu samples (%g GB) per process in a %" PetscInt_FMT " dimensional space with %" PetscInt64_FMT " bins per dimension.\n", n, (n * 1.e-9) * sizeof(PetscInt64), t, d));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "Expected spacing collisions per process %g (%g total).\n", (double)lambda, (double)(N * lambda)));
  PetscCall(PetscRandomCreate(PETSC_COMM_WORLD, &random));
  PetscCall(PetscRandomSetFromOptions(random));
  PetscCall(PetscRandomSetInterval(random, 0.0, 1.0));
  PetscCall(PetscRandomView(random, PETSC_VIEWER_STDOUT_WORLD));
  PetscCall(PetscMalloc1(n, &X));
  for (i = 0; i < n; i++) {
    PetscInt   j;
    PetscInt64 bin  = 0;
    PetscInt64 mult = 1;

    for (j = 0; j < t; j++, mult *= d) {
      PetscReal  x;
      PetscInt64 slot;

      PetscCall(PetscRandomGetValueReal(random, &x));
      /* worried about precision here */
      slot = (PetscInt64)(x * d);
      bin += mult * slot;
    }
    PetscCheck(bin < k, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Generated point in bin %" PetscInt64_FMT ", but only %" PetscInt64_FMT " bins", bin, k);
    X[i] = bin;
  }

  qsort(X, n, sizeof(PetscInt64), PetscInt64Compare);
  for (i = 0; i < n - 1; i++) X[i] = X[i + 1] - X[i];
  qsort(X, n - 1, sizeof(PetscInt64), PetscInt64Compare);
  for (i = 0, Y = 0; i < n - 2; i++) Y += (X[i + 1] == X[i]);

  PetscCall(MPIU_Allreduce(MPI_IN_PLACE, &Y, 1, MPIU_INT, MPI_SUM, MPI_COMM_WORLD));
  PetscCall(PoissonTailProbability(N * lambda, Y, &p));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%" PetscInt_FMT " total collisions counted: that many or more should occur with probability %g.\n", Y, (double)p));

  PetscCall(PetscFree(X));
  PetscCall(PetscRandomDestroy(&random));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

  test:
    args: -t 4 -log2d 7 -log2n 10

TEST*/

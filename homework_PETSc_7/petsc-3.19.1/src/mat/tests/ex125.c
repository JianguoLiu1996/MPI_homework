static char help[] = "Tests MatSolve() and MatMatSolve() (interface to superlu_dist, mumps and mkl_pardiso).\n\
Example: mpiexec -n <np> ./ex125 -f <matrix binary file> -nrhs 4 -mat_solver_type <>\n\n";

/*
-mat_solver_type:
  superlu
  superlu_dist
  mumps
  mkl_pardiso
  cusparse
  petsc
*/

#include <petscmat.h>

int main(int argc, char **args)
{
  Mat           A, RHS = NULL, RHS1 = NULL, C, F, X;
  Vec           u, x, b;
  PetscMPIInt   size;
  PetscInt      m, n, nfact, nsolve, nrhs, ipack = 5;
  PetscReal     norm, tol = 1.e-10;
  IS            perm, iperm;
  MatFactorInfo info;
  PetscRandom   rand;
  PetscBool     flg, symm, testMatSolve = PETSC_TRUE, testMatMatSolve = PETSC_TRUE, testMatMatSolveTranspose = PETSC_TRUE, testMatSolveTranspose = PETSC_TRUE, match = PETSC_FALSE;
  PetscBool     chol = PETSC_FALSE, view = PETSC_FALSE, matsolvexx = PETSC_FALSE;
#if defined(PETSC_HAVE_MUMPS)
  PetscBool test_mumps_opts = PETSC_FALSE;
#endif
  PetscViewer fd;                       /* viewer */
  char        file[PETSC_MAX_PATH_LEN]; /* input file name */
  char        pack[PETSC_MAX_PATH_LEN];

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &args, (char *)0, help));
  PetscCallMPI(MPI_Comm_size(PETSC_COMM_WORLD, &size));

  /* Determine file from which we read the matrix A */
  PetscCall(PetscOptionsGetString(NULL, NULL, "-f", file, sizeof(file), &flg));
  if (flg) { /* Load matrix A */
    PetscCall(PetscViewerBinaryOpen(PETSC_COMM_WORLD, file, FILE_MODE_READ, &fd));
    PetscCall(MatCreate(PETSC_COMM_WORLD, &A));
    PetscCall(MatSetFromOptions(A));
    PetscCall(MatLoad(A, fd));
    PetscCall(PetscViewerDestroy(&fd));
  } else {
    n = 13;
    PetscCall(PetscOptionsGetInt(NULL, NULL, "-n", &n, NULL));
    PetscCall(MatCreate(PETSC_COMM_WORLD, &A));
    PetscCall(MatSetType(A, MATAIJ));
    PetscCall(MatSetFromOptions(A));
    PetscCall(MatSetSizes(A, PETSC_DECIDE, PETSC_DECIDE, n, n));
    PetscCall(MatSetUp(A));
    PetscCall(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
    PetscCall(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
    PetscCall(MatShift(A, 1.0));
  }
  PetscCall(MatGetLocalSize(A, &m, &n));
  PetscCheck(m == n, PETSC_COMM_SELF, PETSC_ERR_ARG_SIZ, "This example is not intended for rectangular matrices (%" PetscInt_FMT ", %" PetscInt_FMT ")", m, n);

  /* if A is symmetric, set its flag -- required by MatGetInertia() */
  PetscCall(MatIsSymmetric(A, 0.0, &symm));
  PetscCall(MatSetOption(A, MAT_SYMMETRIC, symm));

  PetscCall(MatViewFromOptions(A, NULL, "-A_view"));

  /* Create dense matrix C and X; C holds true solution with identical columns */
  nrhs = 2;
  PetscCall(PetscOptionsGetInt(NULL, NULL, "-nrhs", &nrhs, NULL));
  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "ex125: nrhs %" PetscInt_FMT "\n", nrhs));
  PetscCall(MatCreate(PETSC_COMM_WORLD, &C));
  PetscCall(MatSetOptionsPrefix(C, "rhs_"));
  PetscCall(MatSetSizes(C, m, PETSC_DECIDE, PETSC_DECIDE, nrhs));
  PetscCall(MatSetType(C, MATDENSE));
  PetscCall(MatSetFromOptions(C));
  PetscCall(MatSetUp(C));

  PetscCall(PetscOptionsGetBool(NULL, NULL, "-view_factor", &view, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-test_matmatsolve", &testMatMatSolve, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-test_matmatsolvetranspose", &testMatMatSolveTranspose, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-test_matsolvetranspose", &testMatSolveTranspose, NULL));
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-cholesky", &chol, NULL));
#if defined(PETSC_HAVE_MUMPS)
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-test_mumps_opts", &test_mumps_opts, NULL));
#endif

  PetscCall(PetscRandomCreate(PETSC_COMM_WORLD, &rand));
  PetscCall(PetscRandomSetFromOptions(rand));
  PetscCall(MatSetRandom(C, rand));
  PetscCall(MatDuplicate(C, MAT_DO_NOT_COPY_VALUES, &X));

  /* Create vectors */
  PetscCall(MatCreateVecs(A, &x, &b));
  PetscCall(VecDuplicate(x, &u)); /* save the true solution */

  /* Test Factorization */
  PetscCall(MatGetOrdering(A, MATORDERINGND, &perm, &iperm));

  PetscCall(PetscOptionsGetString(NULL, NULL, "-mat_solver_type", pack, sizeof(pack), NULL));
#if defined(PETSC_HAVE_SUPERLU)
  PetscCall(PetscStrcmp(MATSOLVERSUPERLU, pack, &match));
  if (match) {
    PetscCheck(!chol, PETSC_COMM_WORLD, PETSC_ERR_SUP, "SuperLU does not provide Cholesky!");
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, " SUPERLU LU:\n"));
    PetscCall(MatGetFactor(A, MATSOLVERSUPERLU, MAT_FACTOR_LU, &F));
    matsolvexx = PETSC_FALSE; /* Test MatMatSolve(F,RHS,RHS), RHS is a dense matrix, need further work */
    ipack      = 0;
    goto skipoptions;
  }
#endif
#if defined(PETSC_HAVE_SUPERLU_DIST)
  PetscCall(PetscStrcmp(MATSOLVERSUPERLU_DIST, pack, &match));
  if (match) {
    PetscCheck(!chol, PETSC_COMM_WORLD, PETSC_ERR_SUP, "SuperLU does not provide Cholesky!");
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, " SUPERLU_DIST LU:\n"));
    PetscCall(MatGetFactor(A, MATSOLVERSUPERLU_DIST, MAT_FACTOR_LU, &F));
    matsolvexx = PETSC_TRUE;
    if (symm) { /* A is symmetric */
      testMatMatSolveTranspose = PETSC_TRUE;
      testMatSolveTranspose    = PETSC_TRUE;
    } else { /* superlu_dist does not support solving A^t x = rhs yet */
      testMatMatSolveTranspose = PETSC_FALSE;
      testMatSolveTranspose    = PETSC_FALSE;
    }
    ipack = 1;
    goto skipoptions;
  }
#endif
#if defined(PETSC_HAVE_MUMPS)
  PetscCall(PetscStrcmp(MATSOLVERMUMPS, pack, &match));
  if (match) {
    if (chol) {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " MUMPS CHOLESKY:\n"));
      PetscCall(MatGetFactor(A, MATSOLVERMUMPS, MAT_FACTOR_CHOLESKY, &F));
    } else {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " MUMPS LU:\n"));
      PetscCall(MatGetFactor(A, MATSOLVERMUMPS, MAT_FACTOR_LU, &F));
    }
    matsolvexx = PETSC_TRUE;
    if (test_mumps_opts) {
      /* test mumps options */
      PetscInt  icntl;
      PetscReal cntl;

      icntl = 2; /* sequential matrix ordering */
      PetscCall(MatMumpsSetIcntl(F, 7, icntl));

      cntl = 1.e-6; /* threshold for row pivot detection */
      PetscCall(MatMumpsSetIcntl(F, 24, 1));
      PetscCall(MatMumpsSetCntl(F, 3, cntl));
    }
    ipack = 2;
    goto skipoptions;
  }
#endif
#if defined(PETSC_HAVE_MKL_PARDISO)
  PetscCall(PetscStrcmp(MATSOLVERMKL_PARDISO, pack, &match));
  if (match) {
    if (chol) {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " MKL_PARDISO CHOLESKY:\n"));
      PetscCall(MatGetFactor(A, MATSOLVERMKL_PARDISO, MAT_FACTOR_CHOLESKY, &F));
    } else {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " MKL_PARDISO LU:\n"));
      PetscCall(MatGetFactor(A, MATSOLVERMKL_PARDISO, MAT_FACTOR_LU, &F));
    }
    ipack = 3;
    goto skipoptions;
  }
#endif
#if defined(PETSC_HAVE_CUDA)
  PetscCall(PetscStrcmp(MATSOLVERCUSPARSE, pack, &match));
  if (match) {
    if (chol) {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " CUSPARSE CHOLESKY:\n"));
      PetscCall(MatGetFactor(A, MATSOLVERCUSPARSE, MAT_FACTOR_CHOLESKY, &F));
    } else {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " CUSPARSE LU:\n"));
      PetscCall(MatGetFactor(A, MATSOLVERCUSPARSE, MAT_FACTOR_LU, &F));
    }
    testMatSolveTranspose    = PETSC_FALSE;
    testMatMatSolveTranspose = PETSC_FALSE;
    ipack                    = 4;
    goto skipoptions;
  }
#endif
  /* PETSc */
  match = PETSC_TRUE;
  if (match) {
    if (chol) {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " PETSC CHOLESKY:\n"));
      PetscCall(MatGetFactor(A, MATSOLVERPETSC, MAT_FACTOR_CHOLESKY, &F));
    } else {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " PETSC LU:\n"));
      PetscCall(MatGetFactor(A, MATSOLVERPETSC, MAT_FACTOR_LU, &F));
    }
    matsolvexx = PETSC_TRUE;
    ipack      = 5;
    goto skipoptions;
  }

skipoptions:
  PetscCall(MatFactorInfoInitialize(&info));
  info.fill      = 5.0;
  info.shifttype = (PetscReal)MAT_SHIFT_NONE;
  if (chol) {
    PetscCall(MatCholeskyFactorSymbolic(F, A, perm, &info));
  } else {
    PetscCall(MatLUFactorSymbolic(F, A, perm, iperm, &info));
  }

  for (nfact = 0; nfact < 2; nfact++) {
    if (chol) {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " %" PetscInt_FMT "-the CHOLESKY numfactorization \n", nfact));
      PetscCall(MatCholeskyFactorNumeric(F, A, &info));
    } else {
      PetscCall(PetscPrintf(PETSC_COMM_WORLD, " %" PetscInt_FMT "-the LU numfactorization \n", nfact));
      PetscCall(MatLUFactorNumeric(F, A, &info));
    }
    if (view) {
      PetscCall(PetscViewerPushFormat(PETSC_VIEWER_STDOUT_WORLD, PETSC_VIEWER_ASCII_INFO));
      PetscCall(MatView(F, PETSC_VIEWER_STDOUT_WORLD));
      PetscCall(PetscViewerPopFormat(PETSC_VIEWER_STDOUT_WORLD));
      view = PETSC_FALSE;
    }

#if defined(PETSC_HAVE_SUPERLU_DIST)
    if (ipack == 1) { /* Test MatSuperluDistGetDiagU()
       -- input: matrix factor F; output: main diagonal of matrix U on all processes */
      PetscInt     M;
      PetscScalar *diag;
  #if !defined(PETSC_USE_COMPLEX)
      PetscInt nneg, nzero, npos;
  #endif

      PetscCall(MatGetSize(F, &M, NULL));
      PetscCall(PetscMalloc1(M, &diag));
      PetscCall(MatSuperluDistGetDiagU(F, diag));
      PetscCall(PetscFree(diag));

  #if !defined(PETSC_USE_COMPLEX)
      /* Test MatGetInertia() */
      if (symm) { /* A is symmetric */
        PetscCall(MatGetInertia(F, &nneg, &nzero, &npos));
        PetscCall(PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD, " MatInertia: nneg: %" PetscInt_FMT ", nzero: %" PetscInt_FMT ", npos: %" PetscInt_FMT "\n", nneg, nzero, npos));
      }
  #endif
    }
#endif

#if defined(PETSC_HAVE_MUMPS)
    /* mumps interface allows repeated call of MatCholeskyFactorSymbolic(), while the succession calls do nothing */
    if (ipack == 2) {
      if (chol) {
        PetscCall(MatCholeskyFactorSymbolic(F, A, perm, &info));
        PetscCall(MatCholeskyFactorNumeric(F, A, &info));
      } else {
        PetscCall(MatLUFactorSymbolic(F, A, perm, iperm, &info));
        PetscCall(MatLUFactorNumeric(F, A, &info));
      }
    }
#endif

    /* Test MatMatSolve(), A X = B, where B can be dense or sparse */
    if (testMatMatSolve) {
      if (!nfact) {
        PetscCall(MatMatMult(A, C, MAT_INITIAL_MATRIX, 2.0, &RHS));
      } else {
        PetscCall(MatMatMult(A, C, MAT_REUSE_MATRIX, 2.0, &RHS));
      }
      for (nsolve = 0; nsolve < 2; nsolve++) {
        PetscCall(PetscPrintf(PETSC_COMM_WORLD, "   %" PetscInt_FMT "-the MatMatSolve \n", nsolve));
        PetscCall(MatMatSolve(F, RHS, X));

        /* Check the error */
        PetscCall(MatAXPY(X, -1.0, C, SAME_NONZERO_PATTERN));
        PetscCall(MatNorm(X, NORM_FROBENIUS, &norm));
        if (norm > tol) PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%" PetscInt_FMT "-the MatMatSolve: Norm of error %g, nsolve %" PetscInt_FMT "\n", nsolve, (double)norm, nsolve));
      }

      if (matsolvexx) {
        /* Test MatMatSolve(F,RHS,RHS), RHS is a dense matrix */
        PetscCall(MatCopy(RHS, X, SAME_NONZERO_PATTERN));
        PetscCall(MatMatSolve(F, X, X));
        /* Check the error */
        PetscCall(MatAXPY(X, -1.0, C, SAME_NONZERO_PATTERN));
        PetscCall(MatNorm(X, NORM_FROBENIUS, &norm));
        if (norm > tol) PetscCall(PetscPrintf(PETSC_COMM_WORLD, "MatMatSolve(F,RHS,RHS): Norm of error %g\n", (double)norm));
      }

      if (ipack == 2 && size == 1) {
        Mat spRHS, spRHST, RHST;

        PetscCall(MatTranspose(RHS, MAT_INITIAL_MATRIX, &RHST));
        PetscCall(MatConvert(RHST, MATAIJ, MAT_INITIAL_MATRIX, &spRHST));
        PetscCall(MatCreateTranspose(spRHST, &spRHS));
        for (nsolve = 0; nsolve < 2; nsolve++) {
          PetscCall(PetscPrintf(PETSC_COMM_WORLD, "   %" PetscInt_FMT "-the sparse MatMatSolve \n", nsolve));
          PetscCall(MatMatSolve(F, spRHS, X));

          /* Check the error */
          PetscCall(MatAXPY(X, -1.0, C, SAME_NONZERO_PATTERN));
          PetscCall(MatNorm(X, NORM_FROBENIUS, &norm));
          if (norm > tol) PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%" PetscInt_FMT "-the sparse MatMatSolve: Norm of error %g, nsolve %" PetscInt_FMT "\n", nsolve, (double)norm, nsolve));
        }
        PetscCall(MatDestroy(&spRHST));
        PetscCall(MatDestroy(&spRHS));
        PetscCall(MatDestroy(&RHST));
      }
    }

    /* Test testMatMatSolveTranspose(), A^T X = B, where B can be dense or sparse */
    if (testMatMatSolveTranspose) {
      if (!nfact) {
        PetscCall(MatTransposeMatMult(A, C, MAT_INITIAL_MATRIX, 2.0, &RHS1));
      } else {
        PetscCall(MatTransposeMatMult(A, C, MAT_REUSE_MATRIX, 2.0, &RHS1));
      }

      for (nsolve = 0; nsolve < 2; nsolve++) {
        PetscCall(PetscPrintf(PETSC_COMM_WORLD, "   %" PetscInt_FMT "-the MatMatSolveTranspose\n", nsolve));
        PetscCall(MatMatSolveTranspose(F, RHS1, X));

        /* Check the error */
        PetscCall(MatAXPY(X, -1.0, C, SAME_NONZERO_PATTERN));
        PetscCall(MatNorm(X, NORM_FROBENIUS, &norm));
        if (norm > tol) PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%" PetscInt_FMT "-the MatMatSolveTranspose: Norm of error %g, nsolve %" PetscInt_FMT "\n", nsolve, (double)norm, nsolve));
      }

      if (ipack == 2 && size == 1) {
        Mat spRHS, spRHST, RHST;

        PetscCall(MatTranspose(RHS1, MAT_INITIAL_MATRIX, &RHST));
        PetscCall(MatConvert(RHST, MATAIJ, MAT_INITIAL_MATRIX, &spRHST));
        PetscCall(MatCreateTranspose(spRHST, &spRHS));
        for (nsolve = 0; nsolve < 2; nsolve++) {
          PetscCall(MatMatSolveTranspose(F, spRHS, X));

          /* Check the error */
          PetscCall(MatAXPY(X, -1.0, C, SAME_NONZERO_PATTERN));
          PetscCall(MatNorm(X, NORM_FROBENIUS, &norm));
          if (norm > tol) PetscCall(PetscPrintf(PETSC_COMM_WORLD, "%" PetscInt_FMT "-the sparse MatMatSolveTranspose: Norm of error %g, nsolve %" PetscInt_FMT "\n", nsolve, (double)norm, nsolve));
        }
        PetscCall(MatDestroy(&spRHST));
        PetscCall(MatDestroy(&spRHS));
        PetscCall(MatDestroy(&RHST));
      }
    }

    /* Test MatSolve() */
    if (testMatSolve) {
      for (nsolve = 0; nsolve < 2; nsolve++) {
        PetscCall(VecSetRandom(x, rand));
        PetscCall(VecCopy(x, u));
        PetscCall(MatMult(A, x, b));

        PetscCall(PetscPrintf(PETSC_COMM_WORLD, "   %" PetscInt_FMT "-the MatSolve \n", nsolve));
        PetscCall(MatSolve(F, b, x));

        /* Check the error */
        PetscCall(VecAXPY(u, -1.0, x)); /* u <- (-1.0)x + u */
        PetscCall(VecNorm(u, NORM_2, &norm));
        if (norm > tol) {
          PetscReal resi;
          PetscCall(MatMult(A, x, u));    /* u = A*x */
          PetscCall(VecAXPY(u, -1.0, b)); /* u <- (-1.0)b + u */
          PetscCall(VecNorm(u, NORM_2, &resi));
          PetscCall(PetscPrintf(PETSC_COMM_WORLD, "MatSolve: Norm of error %g, resi %g, numfact %" PetscInt_FMT "\n", (double)norm, (double)resi, nfact));
        }
      }
    }

    /* Test MatSolveTranspose() */
    if (testMatSolveTranspose) {
      for (nsolve = 0; nsolve < 2; nsolve++) {
        PetscCall(VecSetRandom(x, rand));
        PetscCall(VecCopy(x, u));
        PetscCall(MatMultTranspose(A, x, b));

        PetscCall(PetscPrintf(PETSC_COMM_WORLD, "   %" PetscInt_FMT "-the MatSolveTranspose\n", nsolve));
        PetscCall(MatSolveTranspose(F, b, x));

        /* Check the error */
        PetscCall(VecAXPY(u, -1.0, x)); /* u <- (-1.0)x + u */
        PetscCall(VecNorm(u, NORM_2, &norm));
        if (norm > tol) {
          PetscReal resi;
          PetscCall(VecAXPY(u, -1.0, b)); /* u <- (-1.0)b + u */
          PetscCall(VecNorm(u, NORM_2, &resi));
          PetscCall(PetscPrintf(PETSC_COMM_WORLD, "MatSolveTranspose: Norm of error %g, resi %g, numfact %" PetscInt_FMT "\n", (double)norm, (double)resi, nfact));
        }
      }
    }
  }

  /* Free data structures */
  PetscCall(MatDestroy(&A));
  PetscCall(MatDestroy(&C));
  PetscCall(MatDestroy(&F));
  PetscCall(MatDestroy(&X));
  PetscCall(MatDestroy(&RHS));
  PetscCall(MatDestroy(&RHS1));

  PetscCall(PetscRandomDestroy(&rand));
  PetscCall(ISDestroy(&perm));
  PetscCall(ISDestroy(&iperm));
  PetscCall(VecDestroy(&x));
  PetscCall(VecDestroy(&b));
  PetscCall(VecDestroy(&u));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   test:
      requires: datafilespath !complex double !defined(PETSC_USE_64BIT_INDICES)
      args: -f ${DATAFILESPATH}/matrices/medium -mat_solver_type petsc
      output_file: output/ex125.out

   test:
      suffix: 2
      args: -mat_solver_type petsc
      output_file: output/ex125.out

   test:
      suffix: mkl_pardiso
      requires: mkl_pardiso datafilespath !complex double !defined(PETSC_USE_64BIT_INDICES)
      args: -f ${DATAFILESPATH}/matrices/small -mat_solver_type mkl_pardiso

   test:
      suffix: mkl_pardiso_2
      requires: mkl_pardiso
      args: -mat_solver_type mkl_pardiso
      output_file: output/ex125_mkl_pardiso.out

   test:
      suffix: mumps
      requires: mumps datafilespath !complex double !defined(PETSC_USE_64BIT_INDICES)
      args: -f ${DATAFILESPATH}/matrices/small -mat_solver_type mumps
      output_file: output/ex125_mumps_seq.out

   test:
      suffix: mumps_2
      nsize: 3
      requires: mumps datafilespath !complex double !defined(PETSC_USE_64BIT_INDICES)
      args: -f ${DATAFILESPATH}/matrices/small -mat_solver_type mumps
      output_file: output/ex125_mumps_par.out

   test:
      suffix: mumps_3
      requires: mumps
      args: -mat_solver_type mumps
      output_file: output/ex125_mumps_seq.out

   test:
      suffix: mumps_4
      nsize: 3
      requires: mumps
      args: -mat_solver_type mumps
      output_file: output/ex125_mumps_par.out

   test:
      suffix: mumps_5
      nsize: 3
      requires: mumps
      args: -mat_solver_type mumps -cholesky
      output_file: output/ex125_mumps_par_cholesky.out

   test:
      suffix: superlu
      requires: datafilespath double !complex !defined(PETSC_USE_64BIT_INDICES) superlu
      args: -f ${DATAFILESPATH}/matrices/medium -mat_solver_type superlu
      output_file: output/ex125_superlu.out

   test:
      suffix: superlu_dist
      nsize: {{1 3}}
      requires: datafilespath double !complex !defined(PETSC_USE_64BIT_INDICES) superlu_dist
      args: -f ${DATAFILESPATH}/matrices/small -mat_solver_type superlu_dist -mat_superlu_dist_rowperm NOROWPERM
      output_file: output/ex125_superlu_dist.out

   test:
      suffix: superlu_dist_2
      nsize: {{1 3}}
      requires: superlu_dist !complex
      args: -n 36 -mat_solver_type superlu_dist -mat_superlu_dist_rowperm NOROWPERM
      output_file: output/ex125_superlu_dist.out

   test:
      suffix: superlu_dist_3
      nsize: {{1 3}}
      requires: superlu_dist !complex
      requires: datafilespath double !complex !defined(PETSC_USE_64BIT_INDICES) superlu_dist
      args: -f ${DATAFILESPATH}/matrices/medium -mat_solver_type superlu_dist -mat_superlu_dist_rowperm NOROWPERM
      output_file: output/ex125_superlu_dist_nonsymmetric.out

   test:
      suffix: superlu_dist_complex
      nsize: 3
      requires: datafilespath double superlu_dist complex !defined(PETSC_USE_64BIT_INDICES)
      args: -f ${DATAFILESPATH}/matrices/farzad_B_rhs -mat_solver_type superlu_dist
      output_file: output/ex125_superlu_dist_complex.out

   test:
      suffix: superlu_dist_complex_2
      nsize: 3
      requires: superlu_dist complex
      args: -mat_solver_type superlu_dist
      output_file: output/ex125_superlu_dist_complex_2.out

   test:
      suffix: cusparse
      requires: cuda datafilespath !complex double !defined(PETSC_USE_64BIT_INDICES)
      #todo: fix the bug with cholesky
      #args: -mat_type aijcusparse -f ${DATAFILESPATH}/matrices/small -mat_solver_type cusparse -cholesky {{0 1}separate output}
      args: -mat_type aijcusparse -f ${DATAFILESPATH}/matrices/small -mat_solver_type cusparse -cholesky {{0}separate output}

   test:
      suffix: cusparse_2
      requires: cuda
      args: -mat_type aijcusparse -mat_solver_type cusparse -cholesky {{0 1}separate output}

TEST*/

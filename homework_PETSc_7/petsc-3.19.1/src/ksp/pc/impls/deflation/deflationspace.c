#include <../src/ksp/pc/impls/deflation/deflation.h> /*I "petscksp.h" I*/

PetscScalar db2[] = {0.7071067811865476, 0.7071067811865476};

PetscScalar db4[] = {-0.12940952255092145, 0.22414386804185735, 0.836516303737469, 0.48296291314469025};

PetscScalar db8[] = {-0.010597401784997278, 0.032883011666982945, 0.030841381835986965, -0.18703481171888114, -0.02798376941698385, 0.6308807679295904, 0.7148465705525415, 0.23037781330885523};

PetscScalar db16[] = {-0.00011747678400228192, 0.0006754494059985568,  -0.0003917403729959771, -0.00487035299301066,  0.008746094047015655, 0.013981027917015516, -0.04408825393106472, -0.01736930100202211,
                      0.128747426620186,       0.00047248457399797254, -0.2840155429624281,    -0.015829105256023893, 0.5853546836548691,   0.6756307362980128,   0.3128715909144659,   0.05441584224308161};

PetscScalar biorth22[] = {0.0, -0.1767766952966369, 0.3535533905932738, 1.0606601717798214, 0.3535533905932738, -0.1767766952966369};

PetscScalar meyer[] = {0.0, -1.009999956941423e-12, 8.519459636796214e-09, -1.111944952595278e-08, -1.0798819539621958e-08, 6.066975741351135e-08, -1.0866516536735883e-07, 8.200680650386481e-08, 1.1783004497663934e-07, -5.506340565252278e-07, 1.1307947017916706e-06, -1.489549216497156e-06, 7.367572885903746e-07, 3.20544191334478e-06, -1.6312699734552807e-05, 6.554305930575149e-05, -0.0006011502343516092, -0.002704672124643725, 0.002202534100911002, 0.006045814097323304, -0.006387718318497156, -0.011061496392513451, 0.015270015130934803, 0.017423434103729693, -0.03213079399021176, -0.024348745906078023, 0.0637390243228016, 0.030655091960824263, -0.13284520043622938, -0.035087555656258346, 0.44459300275757724, 0.7445855923188063, 0.44459300275757724, -0.035087555656258346, -0.13284520043622938, 0.030655091960824263, 0.0637390243228016, -0.024348745906078023, -0.03213079399021176, 0.017423434103729693, 0.015270015130934803, -0.011061496392513451, -0.006387718318497156, 0.006045814097323304, 0.002202534100911002, -0.002704672124643725, -0.0006011502343516092, 6.554305930575149e-05, -1.6312699734552807e-05, 3.20544191334478e-06, 7.367572885903746e-07, -1.489549216497156e-06, 1.1307947017916706e-06, -5.506340565252278e-07, 1.1783004497663934e-07, 8.200680650386481e-08, -1.0866516536735883e-07, 6.066975741351135e-08, -1.0798819539621958e-08, -1.111944952595278e-08, 8.519459636796214e-09, -1.009999956941423e-12};

static PetscErrorCode PCDeflationCreateSpaceWave(MPI_Comm comm, PetscInt m, PetscInt n, PetscInt M, PetscInt N, PetscInt ncoeffs, PetscScalar *coeffs, PetscBool trunc, Mat *H)
{
  Mat      defl;
  PetscInt i, j, k, ilo, ihi, *Iidx;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(ncoeffs, &Iidx));

  PetscCall(MatCreate(comm, &defl));
  PetscCall(MatSetSizes(defl, m, n, M, N));
  PetscCall(MatSetUp(defl));
  PetscCall(MatSeqAIJSetPreallocation(defl, ncoeffs, NULL));
  PetscCall(MatMPIAIJSetPreallocation(defl, ncoeffs, NULL, ncoeffs, NULL));
  PetscCall(MatSetOption(defl, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_TRUE));
  PetscCall(MatSetOption(defl, MAT_NO_OFF_PROC_ENTRIES, PETSC_TRUE));

  /* Alg 735 Taswell: fvecmat */
  k = ncoeffs - 2;
  if (trunc) k = k / 2;

  PetscCall(MatGetOwnershipRange(defl, &ilo, &ihi));
  for (i = 0; i < ncoeffs; i++) {
    Iidx[i] = i + ilo * 2 - k;
    if (Iidx[i] >= N) Iidx[i] = PETSC_MIN_INT;
  }
  for (i = ilo; i < ihi; i++) {
    PetscCall(MatSetValues(defl, 1, &i, ncoeffs, Iidx, coeffs, INSERT_VALUES));
    for (j = 0; j < ncoeffs; j++) {
      Iidx[j] += 2;
      if (Iidx[j] >= N) Iidx[j] = PETSC_MIN_INT;
    }
  }

  PetscCall(MatAssemblyBegin(defl, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(defl, MAT_FINAL_ASSEMBLY));

  PetscCall(PetscFree(Iidx));
  *H = defl;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCDeflationGetSpaceHaar(PC pc, Mat *W, PetscInt size)
{
  Mat          A, defl;
  PetscInt     i, j, len, ilo, ihi, *Iidx, m, M;
  PetscScalar *col, val;

  PetscFunctionBegin;
  /* Haar basis wavelet, level=size */
  len = pow(2, size);
  PetscCall(PetscMalloc2(len, &col, len, &Iidx));
  val = 1. / pow(2, size / 2.);
  for (i = 0; i < len; i++) col[i] = val;

  PetscCall(PCGetOperators(pc, NULL, &A));
  PetscCall(MatGetLocalSize(A, &m, NULL));
  PetscCall(MatGetSize(A, &M, NULL));
  PetscCall(MatCreate(PetscObjectComm((PetscObject)A), &defl));
  PetscCall(MatSetSizes(defl, m, PETSC_DECIDE, M, PetscCeilInt(M, len)));
  PetscCall(MatSetUp(defl));
  PetscCall(MatSeqAIJSetPreallocation(defl, size, NULL));
  PetscCall(MatMPIAIJSetPreallocation(defl, size, NULL, size, NULL));
  PetscCall(MatSetOption(defl, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_TRUE));

  PetscCall(MatGetOwnershipRangeColumn(defl, &ilo, &ihi));
  for (i = 0; i < len; i++) Iidx[i] = i + ilo * len;
  if (M % len && ihi == PetscCeilInt(M, len)) ihi -= 1;
  for (i = ilo; i < ihi; i++) {
    PetscCall(MatSetValues(defl, len, Iidx, 1, &i, col, INSERT_VALUES));
    for (j = 0; j < len; j++) Iidx[j] += len;
  }
  if (M % len && ihi + 1 == PetscCeilInt(M, len)) {
    len = M % len;
    val = 1. / pow(pow(2, len), 0.5);
    for (i = 0; i < len; i++) col[i] = val;
    PetscCall(MatSetValues(defl, len, Iidx, 1, &ihi, col, INSERT_VALUES));
  }

  PetscCall(MatAssemblyBegin(defl, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(defl, MAT_FINAL_ASSEMBLY));

  PetscCall(PetscFree2(col, Iidx));
  *W = defl;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCDeflationGetSpaceWave(PC pc, Mat *W, PetscInt size, PetscInt ncoeffs, PetscScalar *coeffs, PetscBool trunc)
{
  Mat      A, *H, defl;
  PetscInt i, m, M, Mdefl, Ndefl;
  MPI_Comm comm;

  PetscFunctionBegin;
  PetscCall(PetscObjectGetComm((PetscObject)pc, &comm));
  PetscCall(PetscMalloc1(size, &H));
  PetscCall(PCGetOperators(pc, &A, NULL));
  PetscCall(MatGetLocalSize(A, &m, NULL));
  PetscCall(MatGetSize(A, &M, NULL));
  Mdefl = M;
  Ndefl = M;
  for (i = 0; i < size; i++) {
    if (Mdefl % 2) {
      if (trunc) Mdefl = (PetscInt)PetscCeilReal(Mdefl / 2.);
      else Mdefl = (PetscInt)PetscFloorReal((ncoeffs + Mdefl - 1) / 2.);
    } else Mdefl = Mdefl / 2;
    PetscCall(PCDeflationCreateSpaceWave(comm, PETSC_DECIDE, m, Mdefl, Ndefl, ncoeffs, coeffs, trunc, &H[i]));
    PetscCall(MatGetLocalSize(H[i], &m, NULL));
    Ndefl = Mdefl;
  }
  PetscCall(MatCreateComposite(comm, size, H, &defl));
  PetscCall(MatCompositeSetType(defl, MAT_COMPOSITE_MULTIPLICATIVE));
  *W = defl;

  for (i = 0; i < size; i++) PetscCall(MatDestroy(&H[i]));
  PetscCall(PetscFree(H));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCDeflationGetSpaceAggregation(PC pc, Mat *W)
{
  Mat          A, defl;
  PetscInt     i, ilo, ihi, *Iidx, M;
  PetscMPIInt  m;
  PetscScalar *col;
  MPI_Comm     comm;

  PetscFunctionBegin;
  PetscCall(PCGetOperators(pc, &A, NULL));
  PetscCall(MatGetOwnershipRangeColumn(A, &ilo, &ihi));
  PetscCall(MatGetSize(A, &M, NULL));
  PetscCall(PetscObjectGetComm((PetscObject)A, &comm));
  PetscCallMPI(MPI_Comm_size(comm, &m));
  PetscCall(MatCreate(comm, &defl));
  PetscCall(MatSetSizes(defl, ihi - ilo, 1, M, m));
  PetscCall(MatSetUp(defl));
  PetscCall(MatSeqAIJSetPreallocation(defl, 1, NULL));
  PetscCall(MatMPIAIJSetPreallocation(defl, 1, NULL, 0, NULL));
  PetscCall(MatSetOption(defl, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_TRUE));
  PetscCall(MatSetOption(defl, MAT_NO_OFF_PROC_ENTRIES, PETSC_TRUE));

  PetscCall(PetscMalloc2(ihi - ilo, &col, ihi - ilo, &Iidx));
  for (i = ilo; i < ihi; i++) {
    Iidx[i - ilo] = i;
    col[i - ilo]  = 1;
  }
  PetscCallMPI(MPI_Comm_rank(comm, &m));
  i = m;
  PetscCall(MatSetValues(defl, ihi - ilo, Iidx, 1, &i, col, INSERT_VALUES));

  PetscCall(MatAssemblyBegin(defl, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(defl, MAT_FINAL_ASSEMBLY));

  PetscCall(PetscFree2(col, Iidx));
  *W = defl;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PCDeflationComputeSpace(PC pc)
{
  Mat           defl;
  PetscBool     transp = PETSC_TRUE;
  PC_Deflation *def    = (PC_Deflation *)pc->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc, PC_CLASSID, 1);
  PetscCheck(def->spacesize >= 1, PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_WRONG, "Wrong PCDeflation space size specified: %" PetscInt_FMT, def->spacesize);
  switch (def->spacetype) {
  case PC_DEFLATION_SPACE_HAAR:
    transp = PETSC_FALSE;
    PetscCall(PCDeflationGetSpaceHaar(pc, &defl, def->spacesize));
    break;
  case PC_DEFLATION_SPACE_DB2:
    PetscCall(PCDeflationGetSpaceWave(pc, &defl, def->spacesize, 2, db2, PetscNot(def->extendsp)));
    break;
  case PC_DEFLATION_SPACE_DB4:
    PetscCall(PCDeflationGetSpaceWave(pc, &defl, def->spacesize, 4, db4, PetscNot(def->extendsp)));
    break;
  case PC_DEFLATION_SPACE_DB8:
    PetscCall(PCDeflationGetSpaceWave(pc, &defl, def->spacesize, 8, db8, PetscNot(def->extendsp)));
    break;
  case PC_DEFLATION_SPACE_DB16:
    PetscCall(PCDeflationGetSpaceWave(pc, &defl, def->spacesize, 16, db16, PetscNot(def->extendsp)));
    break;
  case PC_DEFLATION_SPACE_BIORTH22:
    PetscCall(PCDeflationGetSpaceWave(pc, &defl, def->spacesize, 6, biorth22, PetscNot(def->extendsp)));
    break;
  case PC_DEFLATION_SPACE_MEYER:
    PetscCall(PCDeflationGetSpaceWave(pc, &defl, def->spacesize, 62, meyer, PetscNot(def->extendsp)));
    break;
  case PC_DEFLATION_SPACE_AGGREGATION:
    transp = PETSC_FALSE;
    PetscCall(PCDeflationGetSpaceAggregation(pc, &defl));
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)pc), PETSC_ERR_ARG_WRONG, "Wrong PCDeflationSpaceType specified");
  }

  PetscCall(PCDeflationSetSpace(pc, defl, transp));
  PetscCall(MatDestroy(&defl));
  PetscFunctionReturn(PETSC_SUCCESS);
}

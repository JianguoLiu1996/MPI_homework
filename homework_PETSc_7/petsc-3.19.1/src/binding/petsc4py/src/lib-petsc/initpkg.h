static
PetscErrorCode PetscInitializePackageAll(void)
{
  PetscFunctionBegin;
  PetscCall(PetscSysInitializePackage());
  PetscCall(PetscDrawInitializePackage());
  PetscCall(PetscViewerInitializePackage());
  PetscCall(PetscRandomInitializePackage());
  PetscCall(PetscDeviceInitializePackage());
  PetscCall(ISInitializePackage());
  PetscCall(AOInitializePackage());
  PetscCall(PFInitializePackage());
  PetscCall(PetscSFInitializePackage());
  PetscCall(VecInitializePackage());
  PetscCall(MatInitializePackage());
  PetscCall(PCInitializePackage());
  PetscCall(KSPInitializePackage());
  PetscCall(SNESInitializePackage());
  PetscCall(TaoInitializePackage());
  PetscCall(TSInitializePackage());
  PetscCall(PetscPartitionerInitializePackage());
  PetscCall(DMInitializePackage());
  PetscCall(PetscDSInitializePackage());
  PetscCall(PetscFEInitializePackage());
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  Local variables:
  c-basic-offset: 2
  indent-tabs-mode: nil
  End:
*/

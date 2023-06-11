/*
      Objects which encapsulate mesh adaptation operation
*/
#ifndef PETSCDMADAPTOR_H
#define PETSCDMADAPTOR_H

#include <petscdm.h>
#include <petscconvest.h>

/* SUBMANSEC = DM */

/*S
  DMAdaptor - An object that constructs a `DMLabel` or metric `Vec` that can be used to modify the `DM` based on error estimators or other criteria

  Level: developer

.seealso: `DM`, `DMAdaptorCreate()`, `DMAdaptorSetSolver()`, `DMAdaptorGetSolver()`, `DMAdaptorSetSequenceLength()`, `DMAdaptorGetSequenceLength()`, `DMAdaptorSetFromOptions()`,
          `DMAdaptorSetUp()`, `DMAdaptorAdapt()`, `DMAdaptorDestroy()`, `DMAdaptorGetTransferFunction()`, `PetscConvEstCreate()`, `PetscConvEstDestroy()`
S*/
typedef struct _p_DMAdaptor *DMAdaptor;

PETSC_EXTERN PetscErrorCode DMAdaptorCreate(MPI_Comm, DMAdaptor *);
PETSC_EXTERN PetscErrorCode DMAdaptorDestroy(DMAdaptor *);
PETSC_EXTERN PetscErrorCode DMAdaptorView(DMAdaptor, PetscViewer);
PETSC_EXTERN PetscErrorCode DMAdaptorSetFromOptions(DMAdaptor);
PETSC_EXTERN PetscErrorCode DMAdaptorGetSolver(DMAdaptor, SNES *);
PETSC_EXTERN PetscErrorCode DMAdaptorSetSolver(DMAdaptor, SNES);
PETSC_EXTERN PetscErrorCode DMAdaptorGetSequenceLength(DMAdaptor, PetscInt *);
PETSC_EXTERN PetscErrorCode DMAdaptorSetSequenceLength(DMAdaptor, PetscInt);
PETSC_EXTERN PetscErrorCode DMAdaptorSetUp(DMAdaptor);
PETSC_EXTERN PetscErrorCode DMAdaptorGetTransferFunction(DMAdaptor, PetscErrorCode (**)(DMAdaptor, DM, Vec, DM, Vec, void *));
PETSC_EXTERN PetscErrorCode DMAdaptorSetTransferFunction(DMAdaptor, PetscErrorCode (*)(DMAdaptor, DM, Vec, DM, Vec, void *));
PETSC_EXTERN PetscErrorCode DMAdaptorAdapt(DMAdaptor, Vec, DMAdaptationStrategy, DM *, Vec *);

#endif

static char help[] = "Simple wrapper object to solve DAE of the form:\n\
                             \\dot{U} = f(U,V)\n\
                             F(U,V) = 0\n\n";

#include <petscts.h>

/* ----------------------------------------------------------------------------*/

typedef struct _p_TSDAESimple *TSDAESimple;
struct _p_TSDAESimple {
  MPI_Comm comm;
  PetscErrorCode (*setfromoptions)(TSDAESimple, PetscOptionItems *);
  PetscErrorCode (*solve)(TSDAESimple, Vec);
  PetscErrorCode (*destroy)(TSDAESimple);
  Vec U, V;
  PetscErrorCode (*f)(PetscReal, Vec, Vec, Vec, void *);
  PetscErrorCode (*F)(PetscReal, Vec, Vec, Vec, void *);
  void *fctx, *Fctx;
  void *data;
};

PetscErrorCode TSDAESimpleCreate(MPI_Comm comm, TSDAESimple *tsdae)
{
  PetscFunctionBeginUser;
  PetscCall(PetscNew(tsdae));
  (*tsdae)->comm = comm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSetRHSFunction(TSDAESimple tsdae, Vec U, PetscErrorCode (*f)(PetscReal, Vec, Vec, Vec, void *), void *ctx)
{
  PetscFunctionBeginUser;
  tsdae->f = f;
  tsdae->U = U;
  PetscCall(PetscObjectReference((PetscObject)U));
  tsdae->fctx = ctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSetIFunction(TSDAESimple tsdae, Vec V, PetscErrorCode (*F)(PetscReal, Vec, Vec, Vec, void *), void *ctx)
{
  PetscFunctionBeginUser;
  tsdae->F = F;
  tsdae->V = V;
  PetscCall(PetscObjectReference((PetscObject)V));
  tsdae->Fctx = ctx;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleDestroy(TSDAESimple *tsdae)
{
  PetscFunctionBeginUser;
  PetscCall((*(*tsdae)->destroy)(*tsdae));
  PetscCall(VecDestroy(&(*tsdae)->U));
  PetscCall(VecDestroy(&(*tsdae)->V));
  PetscCall(PetscFree(*tsdae));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSolve(TSDAESimple tsdae, Vec Usolution)
{
  PetscFunctionBeginUser;
  PetscCall((*tsdae->solve)(tsdae, Usolution));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSetFromOptions(TSDAESimple tsdae)
{
  PetscFunctionBeginUser;
  PetscCall((*tsdae->setfromoptions)(PetscOptionsObject, tsdae));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* ----------------------------------------------------------------------------*/
/*
      Integrates the system by integrating the reduced ODE system and solving the
   algebraic constraints at each stage with a separate SNES solve.
*/

typedef struct {
  PetscReal t;
  TS        ts;
  SNES      snes;
  Vec       U;
} TSDAESimple_Reduced;

/*
   Defines the RHS function that is passed to the time-integrator.

   Solves F(U,V) for V and then computes f(U,V)

*/
PetscErrorCode TSDAESimple_Reduced_TSFunction(TS ts, PetscReal t, Vec U, Vec F, void *actx)
{
  TSDAESimple          tsdae = (TSDAESimple)actx;
  TSDAESimple_Reduced *red   = (TSDAESimple_Reduced *)tsdae->data;

  PetscFunctionBeginUser;
  red->t = t;
  red->U = U;
  PetscCall(SNESSolve(red->snes, NULL, tsdae->V));
  PetscCall((*tsdae->f)(t, U, tsdae->V, F, tsdae->fctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   Defines the nonlinear function that is passed to the nonlinear solver

*/
PetscErrorCode TSDAESimple_Reduced_SNESFunction(SNES snes, Vec V, Vec F, void *actx)
{
  TSDAESimple          tsdae = (TSDAESimple)actx;
  TSDAESimple_Reduced *red   = (TSDAESimple_Reduced *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall((*tsdae->F)(red->t, red->U, V, F, tsdae->Fctx));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSolve_Reduced(TSDAESimple tsdae, Vec U)
{
  TSDAESimple_Reduced *red = (TSDAESimple_Reduced *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall(TSSolve(red->ts, U));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSetFromOptions_Reduced(TSDAESimple tsdae, PetscOptionItems *PetscOptionsObject)
{
  TSDAESimple_Reduced *red = (TSDAESimple_Reduced *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall(TSSetFromOptions(red->ts));
  PetscCall(SNESSetFromOptions(red->snes));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleDestroy_Reduced(TSDAESimple tsdae)
{
  TSDAESimple_Reduced *red = (TSDAESimple_Reduced *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall(TSDestroy(&red->ts));
  PetscCall(SNESDestroy(&red->snes));
  PetscCall(PetscFree(red));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSetUp_Reduced(TSDAESimple tsdae)
{
  TSDAESimple_Reduced *red;
  Vec                  tsrhs;

  PetscFunctionBeginUser;
  PetscCall(PetscNew(&red));
  tsdae->data = red;

  tsdae->setfromoptions = TSDAESimpleSetFromOptions_Reduced;
  tsdae->solve          = TSDAESimpleSolve_Reduced;
  tsdae->destroy        = TSDAESimpleDestroy_Reduced;

  PetscCall(TSCreate(tsdae->comm, &red->ts));
  PetscCall(TSSetProblemType(red->ts, TS_NONLINEAR));
  PetscCall(TSSetType(red->ts, TSEULER));
  PetscCall(TSSetExactFinalTime(red->ts, TS_EXACTFINALTIME_STEPOVER));
  PetscCall(VecDuplicate(tsdae->U, &tsrhs));
  PetscCall(TSSetRHSFunction(red->ts, tsrhs, TSDAESimple_Reduced_TSFunction, tsdae));
  PetscCall(VecDestroy(&tsrhs));

  PetscCall(SNESCreate(tsdae->comm, &red->snes));
  PetscCall(SNESSetOptionsPrefix(red->snes, "tsdaesimple_"));
  PetscCall(SNESSetFunction(red->snes, NULL, TSDAESimple_Reduced_SNESFunction, tsdae));
  PetscCall(SNESSetJacobian(red->snes, NULL, NULL, SNESComputeJacobianDefault, tsdae));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* ----------------------------------------------------------------------------*/

/*
      Integrates the system by integrating directly the entire DAE system
*/

typedef struct {
  TS         ts;
  Vec        UV, UF, VF;
  VecScatter scatterU, scatterV;
} TSDAESimple_Full;

/*
   Defines the RHS function that is passed to the time-integrator.

   f(U,V)
   0

*/
PetscErrorCode TSDAESimple_Full_TSRHSFunction(TS ts, PetscReal t, Vec UV, Vec F, void *actx)
{
  TSDAESimple       tsdae = (TSDAESimple)actx;
  TSDAESimple_Full *full  = (TSDAESimple_Full *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall(VecSet(F, 0.0));
  PetscCall(VecScatterBegin(full->scatterU, UV, tsdae->U, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterEnd(full->scatterU, UV, tsdae->U, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterBegin(full->scatterV, UV, tsdae->V, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterEnd(full->scatterV, UV, tsdae->V, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall((*tsdae->f)(t, tsdae->U, tsdae->V, full->UF, tsdae->fctx));
  PetscCall(VecScatterBegin(full->scatterU, full->UF, F, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(VecScatterEnd(full->scatterU, full->UF, F, INSERT_VALUES, SCATTER_FORWARD));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   Defines the nonlinear function that is passed to the nonlinear solver

   \dot{U}
   F(U,V)

*/
PetscErrorCode TSDAESimple_Full_TSIFunction(TS ts, PetscReal t, Vec UV, Vec UVdot, Vec F, void *actx)
{
  TSDAESimple       tsdae = (TSDAESimple)actx;
  TSDAESimple_Full *full  = (TSDAESimple_Full *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall(VecCopy(UVdot, F));
  PetscCall(VecScatterBegin(full->scatterU, UV, tsdae->U, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterEnd(full->scatterU, UV, tsdae->U, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterBegin(full->scatterV, UV, tsdae->V, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterEnd(full->scatterV, UV, tsdae->V, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall((*tsdae->F)(t, tsdae->U, tsdae->V, full->VF, tsdae->Fctx));
  PetscCall(VecScatterBegin(full->scatterV, full->VF, F, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(VecScatterEnd(full->scatterV, full->VF, F, INSERT_VALUES, SCATTER_FORWARD));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSolve_Full(TSDAESimple tsdae, Vec U)
{
  TSDAESimple_Full *full = (TSDAESimple_Full *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall(VecSet(full->UV, 1.0));
  PetscCall(VecScatterBegin(full->scatterU, U, full->UV, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(VecScatterEnd(full->scatterU, U, full->UV, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(TSSolve(full->ts, full->UV));
  PetscCall(VecScatterBegin(full->scatterU, full->UV, U, INSERT_VALUES, SCATTER_REVERSE));
  PetscCall(VecScatterEnd(full->scatterU, full->UV, U, INSERT_VALUES, SCATTER_REVERSE));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSetFromOptions_Full(TSDAESimple tsdae, PetscOptionItems *PetscOptionsObject)
{
  TSDAESimple_Full *full = (TSDAESimple_Full *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall(TSSetFromOptions(full->ts));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleDestroy_Full(TSDAESimple tsdae)
{
  TSDAESimple_Full *full = (TSDAESimple_Full *)tsdae->data;

  PetscFunctionBeginUser;
  PetscCall(TSDestroy(&full->ts));
  PetscCall(VecDestroy(&full->UV));
  PetscCall(VecDestroy(&full->UF));
  PetscCall(VecDestroy(&full->VF));
  PetscCall(VecScatterDestroy(&full->scatterU));
  PetscCall(VecScatterDestroy(&full->scatterV));
  PetscCall(PetscFree(full));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode TSDAESimpleSetUp_Full(TSDAESimple tsdae)
{
  TSDAESimple_Full *full;
  Vec               tsrhs;
  PetscInt          nU, nV, UVstart;
  IS                is;

  PetscFunctionBeginUser;
  PetscCall(PetscNew(&full));
  tsdae->data = full;

  tsdae->setfromoptions = TSDAESimpleSetFromOptions_Full;
  tsdae->solve          = TSDAESimpleSolve_Full;
  tsdae->destroy        = TSDAESimpleDestroy_Full;

  PetscCall(TSCreate(tsdae->comm, &full->ts));
  PetscCall(TSSetProblemType(full->ts, TS_NONLINEAR));
  PetscCall(TSSetType(full->ts, TSROSW));
  PetscCall(TSSetExactFinalTime(full->ts, TS_EXACTFINALTIME_STEPOVER));
  PetscCall(VecDuplicate(tsdae->U, &full->UF));
  PetscCall(VecDuplicate(tsdae->V, &full->VF));

  PetscCall(VecGetLocalSize(tsdae->U, &nU));
  PetscCall(VecGetLocalSize(tsdae->V, &nV));
  PetscCall(VecCreateMPI(tsdae->comm, nU + nV, PETSC_DETERMINE, &tsrhs));
  PetscCall(VecDuplicate(tsrhs, &full->UV));

  PetscCall(VecGetOwnershipRange(tsrhs, &UVstart, NULL));
  PetscCall(ISCreateStride(tsdae->comm, nU, UVstart, 1, &is));
  PetscCall(VecScatterCreate(tsdae->U, NULL, tsrhs, is, &full->scatterU));
  PetscCall(ISDestroy(&is));
  PetscCall(ISCreateStride(tsdae->comm, nV, UVstart + nU, 1, &is));
  PetscCall(VecScatterCreate(tsdae->V, NULL, tsrhs, is, &full->scatterV));
  PetscCall(ISDestroy(&is));

  PetscCall(TSSetRHSFunction(full->ts, tsrhs, TSDAESimple_Full_TSRHSFunction, tsdae));
  PetscCall(TSSetIFunction(full->ts, NULL, TSDAESimple_Full_TSIFunction, tsdae));
  PetscCall(VecDestroy(&tsrhs));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* ----------------------------------------------------------------------------*/

/*
   Simple example:   f(U,V) = U + V

*/
PetscErrorCode f(PetscReal t, Vec U, Vec V, Vec F, void *ctx)
{
  PetscFunctionBeginUser;
  PetscCall(VecWAXPY(F, 1.0, U, V));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
   Simple example: F(U,V) = U - V

*/
PetscErrorCode F(PetscReal t, Vec U, Vec V, Vec F, void *ctx)
{
  PetscFunctionBeginUser;
  PetscCall(VecWAXPY(F, -1.0, V, U));
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  TSDAESimple tsdae;
  Vec         U, V, Usolution;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));
  PetscCall(TSDAESimpleCreate(PETSC_COMM_WORLD, &tsdae));

  PetscCall(VecCreateMPI(PETSC_COMM_WORLD, 1, PETSC_DETERMINE, &U));
  PetscCall(VecCreateMPI(PETSC_COMM_WORLD, 1, PETSC_DETERMINE, &V));
  PetscCall(TSDAESimpleSetRHSFunction(tsdae, U, f, NULL));
  PetscCall(TSDAESimpleSetIFunction(tsdae, V, F, NULL));

  PetscCall(VecDuplicate(U, &Usolution));
  PetscCall(VecSet(Usolution, 1.0));

  /*  PetscCall(TSDAESimpleSetUp_Full(tsdae)); */
  PetscCall(TSDAESimpleSetUp_Reduced(tsdae));

  PetscCall(TSDAESimpleSetFromOptions(tsdae));
  PetscCall(TSDAESimpleSolve(tsdae, Usolution));
  PetscCall(TSDAESimpleDestroy(&tsdae));

  PetscCall(VecDestroy(&U));
  PetscCall(VecDestroy(&Usolution));
  PetscCall(VecDestroy(&V));
  PetscCall(PetscFinalize());
  return 0;
}

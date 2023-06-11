/*
  Code for timestepping with additive Runge-Kutta IMEX method

  Notes:
  The general system is written as

  F(t,U,Udot) = G(t,U)

  where F represents the stiff part of the physics and G represents the non-stiff part.

*/
#include <petsc/private/tsimpl.h> /*I   "petscts.h"   I*/
#include <petscdm.h>

static TSARKIMEXType  TSARKIMEXDefault = TSARKIMEX3;
static PetscBool      TSARKIMEXRegisterAllCalled;
static PetscBool      TSARKIMEXPackageInitialized;
static PetscErrorCode TSExtrapolate_ARKIMEX(TS, PetscReal, Vec);

typedef struct _ARKTableau *ARKTableau;
struct _ARKTableau {
  char      *name;
  PetscInt   order;                /* Classical approximation order of the method */
  PetscInt   s;                    /* Number of stages */
  PetscBool  stiffly_accurate;     /* The implicit part is stiffly accurate*/
  PetscBool  FSAL_implicit;        /* The implicit part is FSAL*/
  PetscBool  explicit_first_stage; /* The implicit part has an explicit first stage*/
  PetscInt   pinterp;              /* Interpolation order */
  PetscReal *At, *bt, *ct;         /* Stiff tableau */
  PetscReal *A, *b, *c;            /* Non-stiff tableau */
  PetscReal *bembedt, *bembed;     /* Embedded formula of order one less (order-1) */
  PetscReal *binterpt, *binterp;   /* Dense output formula */
  PetscReal  ccfl;                 /* Placeholder for CFL coefficient relative to forward Euler */
};
typedef struct _ARKTableauLink *ARKTableauLink;
struct _ARKTableauLink {
  struct _ARKTableau tab;
  ARKTableauLink     next;
};
static ARKTableauLink ARKTableauList;

typedef struct {
  ARKTableau   tableau;
  Vec         *Y;            /* States computed during the step */
  Vec         *YdotI;        /* Time derivatives for the stiff part */
  Vec         *YdotRHS;      /* Function evaluations for the non-stiff part */
  Vec         *Y_prev;       /* States computed during the previous time step */
  Vec         *YdotI_prev;   /* Time derivatives for the stiff part for the previous time step*/
  Vec         *YdotRHS_prev; /* Function evaluations for the non-stiff part for the previous time step*/
  Vec          Ydot0;        /* Holds the slope from the previous step in FSAL case */
  Vec          Ydot;         /* Work vector holding Ydot during residual evaluation */
  Vec          Z;            /* Ydot = shift(Y-Z) */
  PetscScalar *work;         /* Scalar work */
  PetscReal    scoeff;       /* shift = scoeff/dt */
  PetscReal    stage_time;
  PetscBool    imex;
  PetscBool    extrapolate; /* Extrapolate initial guess from previous time-step stage values */
  TSStepStatus status;

  /* context for sensitivity analysis */
  Vec *VecsDeltaLam;   /* Increment of the adjoint sensitivity w.r.t IC at stage */
  Vec *VecsSensiTemp;  /* Vectors to be multiplied with Jacobian transpose */
  Vec *VecsSensiPTemp; /* Temporary Vectors to store JacobianP-transpose-vector product */
} TS_ARKIMEX;
/*MC
     TSARKIMEXARS122 - Second order ARK IMEX scheme.

     This method has one explicit stage and one implicit stage.

     Options Database Key:
.      -ts_arkimex_type ars122 - set arkimex type to ars122

     Level: advanced

     References:
.    * -  U. Ascher, S. Ruuth, R. J. Spiteri, Implicit explicit Runge Kutta methods for time dependent Partial Differential Equations. Appl. Numer. Math. 25, (1997).

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEXA2 - Second order ARK IMEX scheme with A-stable implicit part.

     This method has an explicit stage and one implicit stage, and has an A-stable implicit scheme. This method was provided by Emil Constantinescu.

     Options Database Key:
.      -ts_arkimex_type a2 - set arkimex type to a2

     Level: advanced

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEXL2 - Second order ARK IMEX scheme with L-stable implicit part.

     This method has two implicit stages, and L-stable implicit scheme.

     Options Database Key:
.      -ts_arkimex_type l2 - set arkimex type to l2

     Level: advanced

    References:
.   * -  L. Pareschi, G. Russo, Implicit Explicit Runge Kutta schemes and applications to hyperbolic systems with relaxations. Journal of Scientific Computing Volume: 25, Issue: 1, October, 2005.

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEX1BEE - First order backward Euler represented as an ARK IMEX scheme with extrapolation as error estimator. This is a 3-stage method.

     This method is aimed at starting the integration of implicit DAEs when explicit first-stage ARK methods are used.

     Options Database Key:
.      -ts_arkimex_type 1bee - set arkimex type to 1bee

     Level: advanced

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEX2C - Second order ARK IMEX scheme with L-stable implicit part.

     This method has one explicit stage and two implicit stages. The implicit part is the same as in TSARKIMEX2D and TSARKIMEX2E, but the explicit part has a larger stability region on the negative real axis. This method was provided by Emil Constantinescu.

     Options Database Key:
.      -ts_arkimex_type 2c - set arkimex type to 2c

     Level: advanced

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEX2D - Second order ARK IMEX scheme with L-stable implicit part.

     This method has one explicit stage and two implicit stages. The stability function is independent of the explicit part in the infinity limit of the implicit component. This method was provided by Emil Constantinescu.

     Options Database Key:
.      -ts_arkimex_type 2d - set arkimex type to 2d

     Level: advanced

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEX2E - Second order ARK IMEX scheme with L-stable implicit part.

     This method has one explicit stage and two implicit stages. It is is an optimal method developed by Emil Constantinescu.

     Options Database Key:
.      -ts_arkimex_type 2e - set arkimex type to 2e

    Level: advanced

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEXPRSSP2 - Second order SSP ARK IMEX scheme.

     This method has three implicit stages.

     References:
.    * -  L. Pareschi, G. Russo, Implicit Explicit Runge Kutta schemes and applications to hyperbolic systems with relaxations. Journal of Scientific Computing Volume: 25, Issue: 1, October, 2005.

     This method is referred to as SSP2-(3,3,2) in https://arxiv.org/abs/1110.4375

     Options Database Key:
.      -ts_arkimex_type prssp2 - set arkimex type to prssp2

     Level: advanced

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEX3 - Third order ARK IMEX scheme with L-stable implicit part.

     This method has one explicit stage and three implicit stages.

     Options Database Key:
.      -ts_arkimex_type 3 - set arkimex type to 3

     Level: advanced

     References:
.    * -  Kennedy and Carpenter 2003.

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEXARS443 - Third order ARK IMEX scheme.

     This method has one explicit stage and four implicit stages.

     Options Database Key:
.      -ts_arkimex_type ars443 - set arkimex type to ars443

     Level: advanced

     References:
+    * -  U. Ascher, S. Ruuth, R. J. Spiteri, Implicit explicit Runge Kutta methods for time dependent Partial Differential Equations. Appl. Numer. Math. 25, (1997).
-    * -  This method is referred to as ARS(4,4,3) in https://arxiv.org/abs/1110.4375

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEXBPR3 - Third order ARK IMEX scheme.

     This method has one explicit stage and four implicit stages.

     Options Database Key:
.      -ts_arkimex_type bpr3 - set arkimex type to bpr3

     Level: advanced

     References:
.    * - This method is referred to as ARK3 in https://arxiv.org/abs/1110.4375

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEX4 - Fourth order ARK IMEX scheme with L-stable implicit part.

     This method has one explicit stage and four implicit stages.

     Options Database Key:
.      -ts_arkimex_type 4 - set arkimex type to4

     Level: advanced

     References:
.    * - Kennedy and Carpenter 2003.

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/
/*MC
     TSARKIMEX5 - Fifth order ARK IMEX scheme with L-stable implicit part.

     This method has one explicit stage and five implicit stages.

     Options Database Key:
.      -ts_arkimex_type 5 - set arkimex type to 5

     Level: advanced

     References:
.    * - Kennedy and Carpenter 2003.

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEXSetType()`
M*/

/*@C
  TSARKIMEXRegisterAll - Registers all of the additive Runge-Kutta implicit-explicit methods in `TSARKIMEX`

  Not Collective, but should be called by all processes which will need the schemes to be registered

  Level: advanced

.seealso: [](chapter_ts), `TS`, `TSARKIMEX`, `TSARKIMEXRegisterDestroy()`
@*/
PetscErrorCode TSARKIMEXRegisterAll(void)
{
  PetscFunctionBegin;
  if (TSARKIMEXRegisterAllCalled) PetscFunctionReturn(PETSC_SUCCESS);
  TSARKIMEXRegisterAllCalled = PETSC_TRUE;

  {
    const PetscReal A[3][3] =
      {
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        {0.0, 0.5, 0.0}
    },
                    At[3][3] = {{1.0, 0.0, 0.0}, {0.0, 0.5, 0.0}, {0.0, 0.5, 0.5}}, b[3] = {0.0, 0.5, 0.5}, bembedt[3] = {1.0, 0.0, 0.0};
    PetscCall(TSARKIMEXRegister(TSARKIMEX1BEE, 2, 3, &At[0][0], b, NULL, &A[0][0], b, NULL, bembedt, bembedt, 1, b, NULL));
  }
  {
    const PetscReal A[2][2] =
      {
        {0.0, 0.0},
        {0.5, 0.0}
    },
                    At[2][2] = {{0.0, 0.0}, {0.0, 0.5}}, b[2] = {0.0, 1.0}, bembedt[2] = {0.5, 0.5};
    /* binterpt[2][2] = {{1.0,-1.0},{0.0,1.0}};  second order dense output has poor stability properties and hence it is not currently in use*/
    PetscCall(TSARKIMEXRegister(TSARKIMEXARS122, 2, 2, &At[0][0], b, NULL, &A[0][0], b, NULL, bembedt, bembedt, 1, b, NULL));
  }
  {
    const PetscReal A[2][2] =
      {
        {0.0, 0.0},
        {1.0, 0.0}
    },
                    At[2][2] = {{0.0, 0.0}, {0.5, 0.5}}, b[2] = {0.5, 0.5}, bembedt[2] = {0.0, 1.0};
    /* binterpt[2][2] = {{1.0,-0.5},{0.0,0.5}}  second order dense output has poor stability properties and hence it is not currently in use*/
    PetscCall(TSARKIMEXRegister(TSARKIMEXA2, 2, 2, &At[0][0], b, NULL, &A[0][0], b, NULL, bembedt, bembedt, 1, b, NULL));
  }
  {
    /* const PetscReal us2 = 1.0-1.0/PetscSqrtReal((PetscReal)2.0);    Direct evaluation: 0.2928932188134524755992. Used below to ensure all values are available at compile time   */
    const PetscReal A[2][2] =
      {
        {0.0, 0.0},
        {1.0, 0.0}
    },
                    At[2][2] = {{0.2928932188134524755992, 0.0}, {1.0 - 2.0 * 0.2928932188134524755992, 0.2928932188134524755992}}, b[2] = {0.5, 0.5}, bembedt[2] = {0.0, 1.0}, binterpt[2][2] = {{(0.2928932188134524755992 - 1.0) / (2.0 * 0.2928932188134524755992 - 1.0), -1 / (2.0 * (1.0 - 2.0 * 0.2928932188134524755992))}, {1 - (0.2928932188134524755992 - 1.0) / (2.0 * 0.2928932188134524755992 - 1.0), -1 / (2.0 * (1.0 - 2.0 * 0.2928932188134524755992))}}, binterp[2][2] = {{1.0, -0.5}, {0.0, 0.5}};
    PetscCall(TSARKIMEXRegister(TSARKIMEXL2, 2, 2, &At[0][0], b, NULL, &A[0][0], b, NULL, bembedt, bembedt, 2, binterpt[0], binterp[0]));
  }
  {
    /* const PetscReal s2 = PetscSqrtReal((PetscReal)2.0),  Direct evaluation: 1.414213562373095048802. Used below to ensure all values are available at compile time   */
    const PetscReal A[3][3] =
      {
        {0,                           0,   0},
        {2 - 1.414213562373095048802, 0,   0},
        {0.5,                         0.5, 0}
    },
                    At[3][3] = {{0, 0, 0}, {1 - 1 / 1.414213562373095048802, 1 - 1 / 1.414213562373095048802, 0}, {1 / (2 * 1.414213562373095048802), 1 / (2 * 1.414213562373095048802), 1 - 1 / 1.414213562373095048802}}, bembedt[3] = {(4. - 1.414213562373095048802) / 8., (4. - 1.414213562373095048802) / 8., 1 / (2. * 1.414213562373095048802)}, binterpt[3][2] = {{1.0 / 1.414213562373095048802, -1.0 / (2.0 * 1.414213562373095048802)}, {1.0 / 1.414213562373095048802, -1.0 / (2.0 * 1.414213562373095048802)}, {1.0 - 1.414213562373095048802, 1.0 / 1.414213562373095048802}};
    PetscCall(TSARKIMEXRegister(TSARKIMEX2C, 2, 3, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembedt, 2, binterpt[0], NULL));
  }
  {
    /* const PetscReal s2 = PetscSqrtReal((PetscReal)2.0),  Direct evaluation: 1.414213562373095048802. Used below to ensure all values are available at compile time   */
    const PetscReal A[3][3] =
      {
        {0,                           0,    0},
        {2 - 1.414213562373095048802, 0,    0},
        {0.75,                        0.25, 0}
    },
                    At[3][3] = {{0, 0, 0}, {1 - 1 / 1.414213562373095048802, 1 - 1 / 1.414213562373095048802, 0}, {1 / (2 * 1.414213562373095048802), 1 / (2 * 1.414213562373095048802), 1 - 1 / 1.414213562373095048802}}, bembedt[3] = {(4. - 1.414213562373095048802) / 8., (4. - 1.414213562373095048802) / 8., 1 / (2. * 1.414213562373095048802)}, binterpt[3][2] = {{1.0 / 1.414213562373095048802, -1.0 / (2.0 * 1.414213562373095048802)}, {1.0 / 1.414213562373095048802, -1.0 / (2.0 * 1.414213562373095048802)}, {1.0 - 1.414213562373095048802, 1.0 / 1.414213562373095048802}};
    PetscCall(TSARKIMEXRegister(TSARKIMEX2D, 2, 3, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembedt, 2, binterpt[0], NULL));
  }
  { /* Optimal for linear implicit part */
    /* const PetscReal s2 = PetscSqrtReal((PetscReal)2.0),  Direct evaluation: 1.414213562373095048802. Used below to ensure all values are available at compile time   */
    const PetscReal A[3][3] =
      {
        {0,                                     0,                                     0},
        {2 - 1.414213562373095048802,           0,                                     0},
        {(3 - 2 * 1.414213562373095048802) / 6, (3 + 2 * 1.414213562373095048802) / 6, 0}
    },
                    At[3][3] = {{0, 0, 0}, {1 - 1 / 1.414213562373095048802, 1 - 1 / 1.414213562373095048802, 0}, {1 / (2 * 1.414213562373095048802), 1 / (2 * 1.414213562373095048802), 1 - 1 / 1.414213562373095048802}}, bembedt[3] = {(4. - 1.414213562373095048802) / 8., (4. - 1.414213562373095048802) / 8., 1 / (2. * 1.414213562373095048802)}, binterpt[3][2] = {{1.0 / 1.414213562373095048802, -1.0 / (2.0 * 1.414213562373095048802)}, {1.0 / 1.414213562373095048802, -1.0 / (2.0 * 1.414213562373095048802)}, {1.0 - 1.414213562373095048802, 1.0 / 1.414213562373095048802}};
    PetscCall(TSARKIMEXRegister(TSARKIMEX2E, 2, 3, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembedt, 2, binterpt[0], NULL));
  }
  { /* Optimal for linear implicit part */
    const PetscReal A[3][3] =
      {
        {0,   0,   0},
        {0.5, 0,   0},
        {0.5, 0.5, 0}
    },
                    At[3][3] = {{0.25, 0, 0}, {0, 0.25, 0}, {1. / 3, 1. / 3, 1. / 3}};
    PetscCall(TSARKIMEXRegister(TSARKIMEXPRSSP2, 2, 3, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, NULL, NULL, 0, NULL, NULL));
  }
  {
    const PetscReal A[4][4] =
      {
        {0,                                0,                                0,                                 0},
        {1767732205903. / 2027836641118.,  0,                                0,                                 0},
        {5535828885825. / 10492691773637., 788022342437. / 10882634858940.,  0,                                 0},
        {6485989280629. / 16251701735622., -4246266847089. / 9704473918619., 10755448449292. / 10357097424841., 0}
    },
                    At[4][4] = {{0, 0, 0, 0}, {1767732205903. / 4055673282236., 1767732205903. / 4055673282236., 0, 0}, {2746238789719. / 10658868560708., -640167445237. / 6845629431997., 1767732205903. / 4055673282236., 0}, {1471266399579. / 7840856788654., -4482444167858. / 7529755066697., 11266239266428. / 11593286722821., 1767732205903. / 4055673282236.}}, bembedt[4] = {2756255671327. / 12835298489170., -10771552573575. / 22201958757719., 9247589265047. / 10645013368117., 2193209047091. / 5459859503100.}, binterpt[4][2] = {{4655552711362. / 22874653954995., -215264564351. / 13552729205753.}, {-18682724506714. / 9892148508045., 17870216137069. / 13817060693119.}, {34259539580243. / 13192909600954., -28141676662227. / 17317692491321.}, {584795268549. / 6622622206610., 2508943948391. / 7218656332882.}};
    PetscCall(TSARKIMEXRegister(TSARKIMEX3, 3, 4, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembedt, 2, binterpt[0], NULL));
  }
  {
    const PetscReal A[5][5] =
      {
        {0,        0,       0,      0,       0},
        {1. / 2,   0,       0,      0,       0},
        {11. / 18, 1. / 18, 0,      0,       0},
        {5. / 6,   -5. / 6, .5,     0,       0},
        {1. / 4,   7. / 4,  3. / 4, -7. / 4, 0}
    },
                    At[5][5] = {{0, 0, 0, 0, 0}, {0, 1. / 2, 0, 0, 0}, {0, 1. / 6, 1. / 2, 0, 0}, {0, -1. / 2, 1. / 2, 1. / 2, 0}, {0, 3. / 2, -3. / 2, 1. / 2, 1. / 2}}, *bembedt = NULL;
    PetscCall(TSARKIMEXRegister(TSARKIMEXARS443, 3, 5, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembedt, 0, NULL, NULL));
  }
  {
    const PetscReal A[5][5] =
      {
        {0,      0,      0,      0, 0},
        {1,      0,      0,      0, 0},
        {4. / 9, 2. / 9, 0,      0, 0},
        {1. / 4, 0,      3. / 4, 0, 0},
        {1. / 4, 0,      3. / 5, 0, 0}
    },
                    At[5][5] = {{0, 0, 0, 0, 0}, {.5, .5, 0, 0, 0}, {5. / 18, -1. / 9, .5, 0, 0}, {.5, 0, 0, .5, 0}, {.25, 0, .75, -.5, .5}}, *bembedt = NULL;
    PetscCall(TSARKIMEXRegister(TSARKIMEXBPR3, 3, 5, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembedt, 0, NULL, NULL));
  }
  {
    const PetscReal A[6][6] =
      {
        {0,                               0,                                 0,                                 0,                                0,              0},
        {1. / 2,                          0,                                 0,                                 0,                                0,              0},
        {13861. / 62500.,                 6889. / 62500.,                    0,                                 0,                                0,              0},
        {-116923316275. / 2393684061468., -2731218467317. / 15368042101831., 9408046702089. / 11113171139209.,  0,                                0,              0},
        {-451086348788. / 2902428689909., -2682348792572. / 7519795681897.,  12662868775082. / 11960479115383., 3355817975965. / 11060851509271., 0,              0},
        {647845179188. / 3216320057751.,  73281519250. / 8382639484533.,     552539513391. / 3454668386233.,    3354512671639. / 8306763924573.,  4040. / 17871., 0}
    },
                    At[6][6] = {{0, 0, 0, 0, 0, 0}, {1. / 4, 1. / 4, 0, 0, 0, 0}, {8611. / 62500., -1743. / 31250., 1. / 4, 0, 0, 0}, {5012029. / 34652500., -654441. / 2922500., 174375. / 388108., 1. / 4, 0, 0}, {15267082809. / 155376265600., -71443401. / 120774400., 730878875. / 902184768., 2285395. / 8070912., 1. / 4, 0}, {82889. / 524892., 0, 15625. / 83664., 69875. / 102672., -2260. / 8211, 1. / 4}}, bembedt[6] = {4586570599. / 29645900160., 0, 178811875. / 945068544., 814220225. / 1159782912., -3700637. / 11593932., 61727. / 225920.}, binterpt[6][3] = {{6943876665148. / 7220017795957., -54480133. / 30881146., 6818779379841. / 7100303317025.},  {0, 0, 0},
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    {7640104374378. / 9702883013639., -11436875. / 14766696., 2173542590792. / 12501825683035.}, {-20649996744609. / 7521556579894., 174696575. / 18121608., -31592104683404. / 5083833661969.},
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    {8854892464581. / 2390941311638., -12120380. / 966161., 61146701046299. / 7138195549469.},   {-11397109935349. / 6675773540249., 3843. / 706., -17219254887155. / 4939391667607.}};
    PetscCall(TSARKIMEXRegister(TSARKIMEX4, 4, 6, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembedt, 3, binterpt[0], NULL));
  }
  {
    const PetscReal A[8][8] =
      {
        {0,                                  0,                              0,                                 0,                                  0,                               0,                                 0,                               0},
        {41. / 100,                          0,                              0,                                 0,                                  0,                               0,                                 0,                               0},
        {367902744464. / 2072280473677.,     677623207551. / 8224143866563., 0,                                 0,                                  0,                               0,                                 0,                               0},
        {1268023523408. / 10340822734521.,   0,                              1029933939417. / 13636558850479.,  0,                                  0,                               0,                                 0,                               0},
        {14463281900351. / 6315353703477.,   0,                              66114435211212. / 5879490589093.,  -54053170152839. / 4284798021562.,  0,                               0,                                 0,                               0},
        {14090043504691. / 34967701212078.,  0,                              15191511035443. / 11219624916014., -18461159152457. / 12425892160975., -281667163811. / 9011619295870., 0,                                 0,                               0},
        {19230459214898. / 13134317526959.,  0,                              21275331358303. / 2942455364971.,  -38145345988419. / 4862620318723.,  -1. / 8,                         -1. / 8,                           0,                               0},
        {-19977161125411. / 11928030595625., 0,                              -40795976796054. / 6384907823539., 177454434618887. / 12078138498510., 782672205425. / 8267701900261.,  -69563011059811. / 9646580694205., 7356628210526. / 4942186776405., 0}
    },
                    At[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {41. / 200., 41. / 200., 0, 0, 0, 0, 0, 0}, {41. / 400., -567603406766. / 11931857230679., 41. / 200., 0, 0, 0, 0, 0}, {683785636431. / 9252920307686., 0, -110385047103. / 1367015193373., 41. / 200., 0, 0, 0, 0}, {3016520224154. / 10081342136671., 0, 30586259806659. / 12414158314087., -22760509404356. / 11113319521817., 41. / 200., 0, 0, 0}, {218866479029. / 1489978393911., 0, 638256894668. / 5436446318841., -1179710474555. / 5321154724896., -60928119172. / 8023461067671., 41. / 200., 0, 0}, {1020004230633. / 5715676835656., 0, 25762820946817. / 25263940353407., -2161375909145. / 9755907335909., -211217309593. / 5846859502534., -4269925059573. / 7827059040749., 41. / 200, 0}, {-872700587467. / 9133579230613., 0, 0, 22348218063261. / 9555858737531., -1143369518992. / 8141816002931., -39379526789629. / 19018526304540., 32727382324388. / 42900044865799., 41. / 200.}}, bembedt[8] = {-975461918565. / 9796059967033., 0, 0, 78070527104295. / 32432590147079., -548382580838. / 3424219808633., -33438840321285. / 15594753105479., 3629800801594. / 4656183773603., 4035322873751. / 18575991585200.}, binterpt[8][3] = {{-17674230611817. / 10670229744614., 43486358583215. / 12773830924787., -9257016797708. / 5021505065439.}, {0, 0, 0}, {0, 0, 0}, {65168852399939. / 7868540260826., -91478233927265. / 11067650958493., 26096422576131. / 11239449250142.}, {15494834004392. / 5936557850923., -79368583304911. / 10890268929626., 92396832856987. / 20362823103730.}, {-99329723586156. / 26959484932159., -12239297817655. / 9152339842473., 30029262896817. / 10175596800299.}, {-19024464361622. / 5461577185407., 115839755401235. / 10719374521269., -26136350496073. / 3983972220547.}, {-6511271360970. / 6095937251113., 5843115559534. / 2180450260947., -5289405421727. / 3760307252460.}};
    PetscCall(TSARKIMEXRegister(TSARKIMEX5, 5, 8, &At[0][0], NULL, NULL, &A[0][0], NULL, NULL, bembedt, bembedt, 3, binterpt[0], NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSARKIMEXRegisterDestroy - Frees the list of schemes that were registered by `TSARKIMEXRegister()`.

   Not Collective

   Level: advanced

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXRegister()`, `TSARKIMEXRegisterAll()`
@*/
PetscErrorCode TSARKIMEXRegisterDestroy(void)
{
  ARKTableauLink link;

  PetscFunctionBegin;
  while ((link = ARKTableauList)) {
    ARKTableau t   = &link->tab;
    ARKTableauList = link->next;
    PetscCall(PetscFree6(t->At, t->bt, t->ct, t->A, t->b, t->c));
    PetscCall(PetscFree2(t->bembedt, t->bembed));
    PetscCall(PetscFree2(t->binterpt, t->binterp));
    PetscCall(PetscFree(t->name));
    PetscCall(PetscFree(link));
  }
  TSARKIMEXRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  TSARKIMEXInitializePackage - This function initializes everything in the `TSARKIMEX` package. It is called
  from `TSInitializePackage()`.

  Level: developer

.seealso: [](chapter_ts), `PetscInitialize()`, `TSARKIMEXFinalizePackage()`
@*/
PetscErrorCode TSARKIMEXInitializePackage(void)
{
  PetscFunctionBegin;
  if (TSARKIMEXPackageInitialized) PetscFunctionReturn(PETSC_SUCCESS);
  TSARKIMEXPackageInitialized = PETSC_TRUE;
  PetscCall(TSARKIMEXRegisterAll());
  PetscCall(PetscRegisterFinalize(TSARKIMEXFinalizePackage));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  TSARKIMEXFinalizePackage - This function destroys everything in the `TSARKIMEX` package. It is
  called from `PetscFinalize()`.

  Level: developer

.seealso: [](chapter_ts), `PetscFinalize()`, `TSARKIMEXInitializePackage()`
@*/
PetscErrorCode TSARKIMEXFinalizePackage(void)
{
  PetscFunctionBegin;
  TSARKIMEXPackageInitialized = PETSC_FALSE;
  PetscCall(TSARKIMEXRegisterDestroy());
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   TSARKIMEXRegister - register a `TSARKIMEX` scheme by providing the entries in the Butcher tableau and optionally embedded approximations and interpolation

   Not Collective, but the same schemes should be registered on all processes on which they will be used

   Input Parameters:
+  name - identifier for method
.  order - approximation order of method
.  s - number of stages, this is the dimension of the matrices below
.  At - Butcher table of stage coefficients for stiff part (dimension s*s, row-major)
.  bt - Butcher table for completing the stiff part of the step (dimension s; NULL to use the last row of At)
.  ct - Abscissa of each stiff stage (dimension s, NULL to use row sums of At)
.  A - Non-stiff stage coefficients (dimension s*s, row-major)
.  b - Non-stiff step completion table (dimension s; NULL to use last row of At)
.  c - Non-stiff abscissa (dimension s; NULL to use row sums of A)
.  bembedt - Stiff part of completion table for embedded method (dimension s; NULL if not available)
.  bembed - Non-stiff part of completion table for embedded method (dimension s; NULL to use bembedt if provided)
.  pinterp - Order of the interpolation scheme, equal to the number of columns of binterpt and binterp
.  binterpt - Coefficients of the interpolation formula for the stiff part (dimension s*pinterp)
-  binterp - Coefficients of the interpolation formula for the non-stiff part (dimension s*pinterp; NULL to reuse binterpt)

   Level: advanced

   Note:
   Several `TSARKIMEX` methods are provided, this function is only needed to create new methods.

.seealso: [](chapter_ts), `TSARKIMEX`, `TSType`, `TS`
@*/
PetscErrorCode TSARKIMEXRegister(TSARKIMEXType name, PetscInt order, PetscInt s, const PetscReal At[], const PetscReal bt[], const PetscReal ct[], const PetscReal A[], const PetscReal b[], const PetscReal c[], const PetscReal bembedt[], const PetscReal bembed[], PetscInt pinterp, const PetscReal binterpt[], const PetscReal binterp[])
{
  ARKTableauLink link;
  ARKTableau     t;
  PetscInt       i, j;

  PetscFunctionBegin;
  PetscCall(TSARKIMEXInitializePackage());
  PetscCall(PetscNew(&link));
  t = &link->tab;
  PetscCall(PetscStrallocpy(name, &t->name));
  t->order = order;
  t->s     = s;
  PetscCall(PetscMalloc6(s * s, &t->At, s, &t->bt, s, &t->ct, s * s, &t->A, s, &t->b, s, &t->c));
  PetscCall(PetscArraycpy(t->At, At, s * s));
  PetscCall(PetscArraycpy(t->A, A, s * s));
  if (bt) PetscCall(PetscArraycpy(t->bt, bt, s));
  else
    for (i = 0; i < s; i++) t->bt[i] = At[(s - 1) * s + i];
  if (b) PetscCall(PetscArraycpy(t->b, b, s));
  else
    for (i = 0; i < s; i++) t->b[i] = t->bt[i];
  if (ct) PetscCall(PetscArraycpy(t->ct, ct, s));
  else
    for (i = 0; i < s; i++)
      for (j = 0, t->ct[i] = 0; j < s; j++) t->ct[i] += At[i * s + j];
  if (c) PetscCall(PetscArraycpy(t->c, c, s));
  else
    for (i = 0; i < s; i++)
      for (j = 0, t->c[i] = 0; j < s; j++) t->c[i] += A[i * s + j];
  t->stiffly_accurate = PETSC_TRUE;
  for (i = 0; i < s; i++)
    if (t->At[(s - 1) * s + i] != t->bt[i]) t->stiffly_accurate = PETSC_FALSE;
  t->explicit_first_stage = PETSC_TRUE;
  for (i = 0; i < s; i++)
    if (t->At[i] != 0.0) t->explicit_first_stage = PETSC_FALSE;
  /*def of FSAL can be made more precise*/
  t->FSAL_implicit = (PetscBool)(t->explicit_first_stage && t->stiffly_accurate);
  if (bembedt) {
    PetscCall(PetscMalloc2(s, &t->bembedt, s, &t->bembed));
    PetscCall(PetscArraycpy(t->bembedt, bembedt, s));
    PetscCall(PetscArraycpy(t->bembed, bembed ? bembed : bembedt, s));
  }

  t->pinterp = pinterp;
  PetscCall(PetscMalloc2(s * pinterp, &t->binterpt, s * pinterp, &t->binterp));
  PetscCall(PetscArraycpy(t->binterpt, binterpt, s * pinterp));
  PetscCall(PetscArraycpy(t->binterp, binterp ? binterp : binterpt, s * pinterp));
  link->next     = ARKTableauList;
  ARKTableauList = link;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
 The step completion formula is

 x1 = x0 - h bt^T YdotI + h b^T YdotRHS

 This function can be called before or after ts->vec_sol has been updated.
 Suppose we have a completion formula (bt,b) and an embedded formula (bet,be) of different order.
 We can write

 x1e = x0 - h bet^T YdotI + h be^T YdotRHS
     = x1 + h bt^T YdotI - h b^T YdotRHS - h bet^T YdotI + h be^T YdotRHS
     = x1 - h (bet - bt)^T YdotI + h (be - b)^T YdotRHS

 so we can evaluate the method with different order even after the step has been optimistically completed.
*/
static PetscErrorCode TSEvaluateStep_ARKIMEX(TS ts, PetscInt order, Vec X, PetscBool *done)
{
  TS_ARKIMEX  *ark = (TS_ARKIMEX *)ts->data;
  ARKTableau   tab = ark->tableau;
  PetscScalar *w   = ark->work;
  PetscReal    h;
  PetscInt     s = tab->s, j;

  PetscFunctionBegin;
  switch (ark->status) {
  case TS_STEP_INCOMPLETE:
  case TS_STEP_PENDING:
    h = ts->time_step;
    break;
  case TS_STEP_COMPLETE:
    h = ts->ptime - ts->ptime_prev;
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)ts), PETSC_ERR_PLIB, "Invalid TSStepStatus");
  }
  if (order == tab->order) {
    if (ark->status == TS_STEP_INCOMPLETE) {
      if (!ark->imex && tab->stiffly_accurate) { /* Only the stiffly accurate implicit formula is used */
        PetscCall(VecCopy(ark->Y[s - 1], X));
      } else { /* Use the standard completion formula (bt,b) */
        PetscCall(VecCopy(ts->vec_sol, X));
        for (j = 0; j < s; j++) w[j] = h * tab->bt[j];
        PetscCall(VecMAXPY(X, s, w, ark->YdotI));
        if (ark->imex) { /* Method is IMEX, complete the explicit formula */
          for (j = 0; j < s; j++) w[j] = h * tab->b[j];
          PetscCall(VecMAXPY(X, s, w, ark->YdotRHS));
        }
      }
    } else PetscCall(VecCopy(ts->vec_sol, X));
    if (done) *done = PETSC_TRUE;
    PetscFunctionReturn(PETSC_SUCCESS);
  } else if (order == tab->order - 1) {
    if (!tab->bembedt) goto unavailable;
    if (ark->status == TS_STEP_INCOMPLETE) { /* Complete with the embedded method (bet,be) */
      PetscCall(VecCopy(ts->vec_sol, X));
      for (j = 0; j < s; j++) w[j] = h * tab->bembedt[j];
      PetscCall(VecMAXPY(X, s, w, ark->YdotI));
      for (j = 0; j < s; j++) w[j] = h * tab->bembed[j];
      PetscCall(VecMAXPY(X, s, w, ark->YdotRHS));
    } else { /* Rollback and re-complete using (bet-be,be-b) */
      PetscCall(VecCopy(ts->vec_sol, X));
      for (j = 0; j < s; j++) w[j] = h * (tab->bembedt[j] - tab->bt[j]);
      PetscCall(VecMAXPY(X, tab->s, w, ark->YdotI));
      for (j = 0; j < s; j++) w[j] = h * (tab->bembed[j] - tab->b[j]);
      PetscCall(VecMAXPY(X, s, w, ark->YdotRHS));
    }
    if (done) *done = PETSC_TRUE;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
unavailable:
  if (done) *done = PETSC_FALSE;
  else
    SETERRQ(PetscObjectComm((PetscObject)ts), PETSC_ERR_SUP, "ARKIMEX '%s' of order %" PetscInt_FMT " cannot evaluate step at order %" PetscInt_FMT ". Consider using -ts_adapt_type none or a different method that has an embedded estimate.", tab->name,
            tab->order, order);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSARKIMEXTestMassIdentity(TS ts, PetscBool *id)
{
  Vec         Udot, Y1, Y2;
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  PetscReal   norm;

  PetscFunctionBegin;
  PetscCall(VecDuplicate(ts->vec_sol, &Udot));
  PetscCall(VecDuplicate(ts->vec_sol, &Y1));
  PetscCall(VecDuplicate(ts->vec_sol, &Y2));
  PetscCall(TSComputeIFunction(ts, ts->ptime, ts->vec_sol, Udot, Y1, ark->imex));
  PetscCall(VecSetRandom(Udot, NULL));
  PetscCall(TSComputeIFunction(ts, ts->ptime, ts->vec_sol, Udot, Y2, ark->imex));
  PetscCall(VecAXPY(Y2, -1.0, Y1));
  PetscCall(VecAXPY(Y2, -1.0, Udot));
  PetscCall(VecNorm(Y2, NORM_2, &norm));
  if (norm < 100.0 * PETSC_MACHINE_EPSILON) {
    *id = PETSC_TRUE;
  } else {
    *id = PETSC_FALSE;
    PetscCall(PetscInfo((PetscObject)ts, "IFunction(Udot = random) - IFunction(Udot = 0) is not near Udot, %g, suspect mass matrix implied in IFunction() is not the identity as required\n", (double)norm));
  }
  PetscCall(VecDestroy(&Udot));
  PetscCall(VecDestroy(&Y1));
  PetscCall(VecDestroy(&Y2));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSRollBack_ARKIMEX(TS ts)
{
  TS_ARKIMEX      *ark = (TS_ARKIMEX *)ts->data;
  ARKTableau       tab = ark->tableau;
  const PetscInt   s   = tab->s;
  const PetscReal *bt = tab->bt, *b = tab->b;
  PetscScalar     *w     = ark->work;
  Vec             *YdotI = ark->YdotI, *YdotRHS = ark->YdotRHS;
  PetscInt         j;
  PetscReal        h;

  PetscFunctionBegin;
  switch (ark->status) {
  case TS_STEP_INCOMPLETE:
  case TS_STEP_PENDING:
    h = ts->time_step;
    break;
  case TS_STEP_COMPLETE:
    h = ts->ptime - ts->ptime_prev;
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)ts), PETSC_ERR_PLIB, "Invalid TSStepStatus");
  }
  for (j = 0; j < s; j++) w[j] = -h * bt[j];
  PetscCall(VecMAXPY(ts->vec_sol, s, w, YdotI));
  for (j = 0; j < s; j++) w[j] = -h * b[j];
  PetscCall(VecMAXPY(ts->vec_sol, s, w, YdotRHS));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSStep_ARKIMEX(TS ts)
{
  TS_ARKIMEX      *ark = (TS_ARKIMEX *)ts->data;
  ARKTableau       tab = ark->tableau;
  const PetscInt   s   = tab->s;
  const PetscReal *At = tab->At, *A = tab->A, *ct = tab->ct, *c = tab->c;
  PetscScalar     *w = ark->work;
  Vec             *Y = ark->Y, *YdotI = ark->YdotI, *YdotRHS = ark->YdotRHS, Ydot = ark->Ydot, Ydot0 = ark->Ydot0, Z = ark->Z;
  PetscBool        extrapolate = ark->extrapolate;
  TSAdapt          adapt;
  SNES             snes;
  PetscInt         i, j, its, lits;
  PetscInt         rejections = 0;
  PetscBool        stageok, accept = PETSC_TRUE;
  PetscReal        next_time_step = ts->time_step;

  PetscFunctionBegin;
  if (ark->extrapolate && !ark->Y_prev) {
    PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->Y_prev));
    PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->YdotI_prev));
    PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->YdotRHS_prev));
  }

  if (!ts->steprollback) {
    if (ts->equation_type >= TS_EQ_IMPLICIT) { /* Save the initial slope for the next step */
      PetscCall(VecCopy(YdotI[s - 1], Ydot0));
    }
    if (ark->extrapolate && !ts->steprestart) { /* Save the Y, YdotI, YdotRHS for extrapolation initial guess */
      for (i = 0; i < s; i++) {
        PetscCall(VecCopy(Y[i], ark->Y_prev[i]));
        PetscCall(VecCopy(YdotRHS[i], ark->YdotRHS_prev[i]));
        PetscCall(VecCopy(YdotI[i], ark->YdotI_prev[i]));
      }
    }
  }

  if (ts->equation_type >= TS_EQ_IMPLICIT && tab->explicit_first_stage && ts->steprestart) {
    TS ts_start;
    if (PetscDefined(USE_DEBUG)) {
      PetscBool id = PETSC_FALSE;
      PetscCall(TSARKIMEXTestMassIdentity(ts, &id));
      PetscCheck(id, PetscObjectComm((PetscObject)ts), PETSC_ERR_ARG_INCOMP, "This scheme requires an identity mass matrix, however the TSIFunction you provide does not utilize an identity mass matrix");
    }
    PetscCall(TSClone(ts, &ts_start));
    PetscCall(TSSetSolution(ts_start, ts->vec_sol));
    PetscCall(TSSetTime(ts_start, ts->ptime));
    PetscCall(TSSetMaxSteps(ts_start, ts->steps + 1));
    PetscCall(TSSetMaxTime(ts_start, ts->ptime + ts->time_step));
    PetscCall(TSSetExactFinalTime(ts_start, TS_EXACTFINALTIME_STEPOVER));
    PetscCall(TSSetTimeStep(ts_start, ts->time_step));
    PetscCall(TSSetType(ts_start, TSARKIMEX));
    PetscCall(TSARKIMEXSetFullyImplicit(ts_start, PETSC_TRUE));
    PetscCall(TSARKIMEXSetType(ts_start, TSARKIMEX1BEE));

    PetscCall(TSRestartStep(ts_start));
    PetscCall(TSSolve(ts_start, ts->vec_sol));
    PetscCall(TSGetTime(ts_start, &ts->ptime));
    PetscCall(TSGetTimeStep(ts_start, &ts->time_step));

    { /* Save the initial slope for the next step */
      TS_ARKIMEX *ark_start = (TS_ARKIMEX *)ts_start->data;
      PetscCall(VecCopy(ark_start->YdotI[ark_start->tableau->s - 1], Ydot0));
    }
    ts->steps++;

    /* Set the correct TS in SNES */
    /* We'll try to bypass this by changing the method on the fly */
    {
      PetscCall(TSGetSNES(ts, &snes));
      PetscCall(TSSetSNES(ts, snes));
    }
    PetscCall(TSDestroy(&ts_start));
  }

  ark->status = TS_STEP_INCOMPLETE;
  while (!ts->reason && ark->status != TS_STEP_COMPLETE) {
    PetscReal t = ts->ptime;
    PetscReal h = ts->time_step;
    for (i = 0; i < s; i++) {
      ark->stage_time = t + h * ct[i];
      PetscCall(TSPreStage(ts, ark->stage_time));
      if (At[i * s + i] == 0) { /* This stage is explicit */
        PetscCheck(i == 0 || ts->equation_type < TS_EQ_IMPLICIT, PetscObjectComm((PetscObject)ts), PETSC_ERR_SUP, "Explicit stages other than the first one are not supported for implicit problems");
        PetscCall(VecCopy(ts->vec_sol, Y[i]));
        for (j = 0; j < i; j++) w[j] = h * At[i * s + j];
        PetscCall(VecMAXPY(Y[i], i, w, YdotI));
        for (j = 0; j < i; j++) w[j] = h * A[i * s + j];
        PetscCall(VecMAXPY(Y[i], i, w, YdotRHS));
      } else {
        ark->scoeff = 1. / At[i * s + i];
        /* Ydot = shift*(Y-Z) */
        PetscCall(VecCopy(ts->vec_sol, Z));
        for (j = 0; j < i; j++) w[j] = h * At[i * s + j];
        PetscCall(VecMAXPY(Z, i, w, YdotI));
        for (j = 0; j < i; j++) w[j] = h * A[i * s + j];
        PetscCall(VecMAXPY(Z, i, w, YdotRHS));
        if (extrapolate && !ts->steprestart) {
          /* Initial guess extrapolated from previous time step stage values */
          PetscCall(TSExtrapolate_ARKIMEX(ts, c[i], Y[i]));
        } else {
          /* Initial guess taken from last stage */
          PetscCall(VecCopy(i > 0 ? Y[i - 1] : ts->vec_sol, Y[i]));
        }
        PetscCall(TSGetSNES(ts, &snes));
        PetscCall(SNESSolve(snes, NULL, Y[i]));
        PetscCall(SNESGetIterationNumber(snes, &its));
        PetscCall(SNESGetLinearSolveIterations(snes, &lits));
        ts->snes_its += its;
        ts->ksp_its += lits;
        PetscCall(TSGetAdapt(ts, &adapt));
        PetscCall(TSAdaptCheckStage(adapt, ts, ark->stage_time, Y[i], &stageok));
        if (!stageok) {
          /* We are likely rejecting the step because of solver or function domain problems so we should not attempt to
           * use extrapolation to initialize the solves on the next attempt. */
          extrapolate = PETSC_FALSE;
          goto reject_step;
        }
      }
      if (ts->equation_type >= TS_EQ_IMPLICIT) {
        if (i == 0 && tab->explicit_first_stage) {
          PetscCheck(tab->stiffly_accurate, PetscObjectComm((PetscObject)ts), PETSC_ERR_SUP, "TSARKIMEX %s is not stiffly accurate and therefore explicit-first stage methods cannot be used if the equation is implicit because the slope cannot be evaluated",
                     ark->tableau->name);
          PetscCall(VecCopy(Ydot0, YdotI[0])); /* YdotI = YdotI(tn-1) */
        } else {
          PetscCall(VecAXPBYPCZ(YdotI[i], -ark->scoeff / h, ark->scoeff / h, 0, Z, Y[i])); /* YdotI = shift*(X-Z) */
        }
      } else {
        if (i == 0 && tab->explicit_first_stage) {
          PetscCall(VecZeroEntries(Ydot));
          PetscCall(TSComputeIFunction(ts, t + h * ct[i], Y[i], Ydot, YdotI[i], ark->imex)); /* YdotI = -G(t,Y,0)   */
          PetscCall(VecScale(YdotI[i], -1.0));
        } else {
          PetscCall(VecAXPBYPCZ(YdotI[i], -ark->scoeff / h, ark->scoeff / h, 0, Z, Y[i])); /* YdotI = shift*(X-Z) */
        }
        if (ark->imex) {
          PetscCall(TSComputeRHSFunction(ts, t + h * c[i], Y[i], YdotRHS[i]));
        } else {
          PetscCall(VecZeroEntries(YdotRHS[i]));
        }
      }
      PetscCall(TSPostStage(ts, ark->stage_time, i, Y));
    }

    ark->status = TS_STEP_INCOMPLETE;
    PetscCall(TSEvaluateStep_ARKIMEX(ts, tab->order, ts->vec_sol, NULL));
    ark->status = TS_STEP_PENDING;
    PetscCall(TSGetAdapt(ts, &adapt));
    PetscCall(TSAdaptCandidatesClear(adapt));
    PetscCall(TSAdaptCandidateAdd(adapt, tab->name, tab->order, 1, tab->ccfl, (PetscReal)tab->s, PETSC_TRUE));
    PetscCall(TSAdaptChoose(adapt, ts, ts->time_step, NULL, &next_time_step, &accept));
    ark->status = accept ? TS_STEP_COMPLETE : TS_STEP_INCOMPLETE;
    if (!accept) { /* Roll back the current step */
      PetscCall(TSRollBack_ARKIMEX(ts));
      ts->time_step = next_time_step;
      goto reject_step;
    }

    ts->ptime += ts->time_step;
    ts->time_step = next_time_step;
    break;

  reject_step:
    ts->reject++;
    accept = PETSC_FALSE;
    if (!ts->reason && ++rejections > ts->max_reject && ts->max_reject >= 0) {
      ts->reason = TS_DIVERGED_STEP_REJECTED;
      PetscCall(PetscInfo(ts, "Step=%" PetscInt_FMT ", step rejections %" PetscInt_FMT " greater than current TS allowed, stopping solve\n", ts->steps, rejections));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}
/*
  This adjoint step function assumes the partitioned ODE system has an identity mass matrix and thus can be represented in the form
  Udot = H(t,U) + G(t,U)
  This corresponds to F(t,U,Udot) = Udot-H(t,U).

  The complete adjoint equations are
  (shift*I - dHdu) lambda_s[i]   = 1/at[i][i] (
    (b_i dGdU + bt[i] dHdU) lambda_{n+1} + dGdU \sum_{j=i+1}^s a[j][i] lambda_s[j]
    + dHdU \sum_{j=i+1}^s at[j][i] lambda_s[j]),  i = s-1,...,0
  lambda_n = lambda_{n+1} + \sum_{j=1}^s lambda_s[j]
  mu_{n+1}[i]   = h (at[i][i] dHdP lambda_s[i]
    + (b_i dGdP + bt[i] dHdP) lambda_{n+1} + dGdP \sum_{j=i+1}^s a[j][i] lambda_s[j]
    + dHdP \sum_{j=i+1}^s at[j][i] lambda_s[j]), i = s-1,...,0
  mu_n = mu_{n+1} + \sum_{j=1}^s mu_{n+1}[j]
  where shift = 1/(at[i][i]*h)

  If at[i][i] is 0, the first equation falls back to
  lambda_s[i] = h (
    (b_i dGdU + bt[i] dHdU) lambda_{n+1} + dGdU \sum_{j=i+1}^s a[j][i] lambda_s[j]
    + dHdU \sum_{j=i+1}^s at[j][i] lambda_s[j])

*/
static PetscErrorCode TSAdjointStep_ARKIMEX(TS ts)
{
  TS_ARKIMEX      *ark = (TS_ARKIMEX *)ts->data;
  ARKTableau       tab = ark->tableau;
  const PetscInt   s   = tab->s;
  const PetscReal *At = tab->At, *A = tab->A, *ct = tab->ct, *c = tab->c, *b = tab->b, *bt = tab->bt;
  PetscScalar     *w = ark->work;
  Vec             *Y = ark->Y, Ydot = ark->Ydot, *VecsDeltaLam = ark->VecsDeltaLam, *VecsSensiTemp = ark->VecsSensiTemp, *VecsSensiPTemp = ark->VecsSensiPTemp;
  Mat              Jex, Jim, Jimpre;
  PetscInt         i, j, nadj;
  PetscReal        t                 = ts->ptime, stage_time_ex;
  PetscReal        adjoint_time_step = -ts->time_step; /* always positive since ts->time_step is negative */
  KSP              ksp;

  PetscFunctionBegin;
  ark->status = TS_STEP_INCOMPLETE;
  PetscCall(SNESGetKSP(ts->snes, &ksp));
  PetscCall(TSGetRHSMats_Private(ts, &Jex, NULL));
  PetscCall(TSGetIJacobian(ts, &Jim, &Jimpre, NULL, NULL));

  for (i = s - 1; i >= 0; i--) {
    ark->stage_time = t - adjoint_time_step * (1.0 - ct[i]);
    stage_time_ex   = t - adjoint_time_step * (1.0 - c[i]);
    if (At[i * s + i] == 0) { // This stage is explicit
      ark->scoeff = 0.;
    } else {
      ark->scoeff = -1. / At[i * s + i]; // this makes shift=ark->scoeff/ts->time_step positive since ts->time_step is negative
    }
    PetscCall(TSComputeSNESJacobian(ts, Y[i], Jim, Jimpre));
    PetscCall(TSComputeRHSJacobian(ts, stage_time_ex, Y[i], Jex, Jex));
    if (ts->vecs_sensip) {
      PetscCall(TSComputeIJacobianP(ts, ark->stage_time, Y[i], Ydot, ark->scoeff / adjoint_time_step, ts->Jacp, PETSC_TRUE)); // get dFdP (-dHdP), Ydot not really used since mass matrix is identity
      PetscCall(TSComputeRHSJacobianP(ts, stage_time_ex, Y[i], ts->Jacprhs));                                                 // get dGdP
    }
    for (nadj = 0; nadj < ts->numcost; nadj++) {
      /* Build RHS (stored in VecsDeltaLam) for first-order adjoint */
      if (s - i - 1 > 0) {
        /* Temp = -\sum_{j=i+1}^s at[j][i] lambda_s[j] */
        for (j = i + 1; j < s; j++) w[j - i - 1] = -At[j * s + i];
        PetscCall(VecSet(VecsSensiTemp[nadj], 0));
        PetscCall(VecMAXPY(VecsSensiTemp[nadj], s - i - 1, w, &VecsDeltaLam[nadj * s + i + 1]));
        /* (shift I - dHdU) Temp */
        PetscCall(MatMultTranspose(Jim, VecsSensiTemp[nadj], VecsDeltaLam[nadj * s + i]));
        /* cancel out shift Temp where shift=-scoeff/h */
        PetscCall(VecAXPY(VecsDeltaLam[nadj * s + i], ark->scoeff / adjoint_time_step, VecsSensiTemp[nadj]));
        if (ts->vecs_sensip) {
          /* - dHdP Temp */
          PetscCall(MatMultTranspose(ts->Jacp, VecsSensiTemp[nadj], VecsSensiPTemp[nadj]));
          /* mu_n += h dHdP Temp */
          PetscCall(VecAXPY(ts->vecs_sensip[nadj], -adjoint_time_step, VecsSensiPTemp[nadj]));
        }
        /* Temp = \sum_{j=i+1}^s a[j][i] lambda_s[j] */
        for (j = i + 1; j < s; j++) w[j - i - 1] = A[j * s + i];
        PetscCall(VecSet(VecsSensiTemp[nadj], 0));
        PetscCall(VecMAXPY(VecsSensiTemp[nadj], s - i - 1, w, &VecsDeltaLam[nadj * s + i + 1]));
        /* dGdU Temp */
        PetscCall(MatMultTransposeAdd(Jex, VecsSensiTemp[nadj], VecsDeltaLam[nadj * s + i], VecsDeltaLam[nadj * s + i]));
        if (ts->vecs_sensip) {
          /* dGdP Temp */
          PetscCall(MatMultTranspose(ts->Jacprhs, VecsSensiTemp[nadj], VecsSensiPTemp[nadj]));
          /* mu_n += h dGdP Temp */
          PetscCall(VecAXPY(ts->vecs_sensip[nadj], adjoint_time_step, VecsSensiPTemp[nadj]));
        }
      } else {
        PetscCall(VecSet(VecsDeltaLam[nadj * s + i], 0)); // make sure it is initialized
      }
      if (bt[i]) { // bt[i] dHdU lambda_{n+1}
        /* (shift I - dHdU)^T lambda_{n+1} */
        PetscCall(MatMultTranspose(Jim, ts->vecs_sensi[nadj], VecsSensiTemp[nadj]));
        /* -bt[i] (shift I - dHdU)^T lambda_{n+1} */
        PetscCall(VecAXPY(VecsDeltaLam[nadj * s + i], -bt[i], VecsSensiTemp[nadj]));
        /* cancel out -bt[i] shift lambda_{n+1} */
        PetscCall(VecAXPY(VecsDeltaLam[nadj * s + i], -bt[i] * ark->scoeff / adjoint_time_step, ts->vecs_sensi[nadj]));
        if (ts->vecs_sensip) {
          /* -dHdP lambda_{n+1} */
          PetscCall(MatMultTranspose(ts->Jacp, ts->vecs_sensi[nadj], VecsSensiPTemp[nadj]));
          /* mu_n += h bt[i] dHdP lambda_{n+1} */
          PetscCall(VecAXPY(ts->vecs_sensip[nadj], -bt[i] * adjoint_time_step, VecsSensiPTemp[nadj]));
        }
      }
      if (b[i]) { // b[i] dGdU lambda_{n+1}
        /* dGdU lambda_{n+1} */
        PetscCall(MatMultTranspose(Jex, ts->vecs_sensi[nadj], VecsSensiTemp[nadj]));
        /* b[i] dGdU lambda_{n+1} */
        PetscCall(VecAXPY(VecsDeltaLam[nadj * s + i], b[i], VecsSensiTemp[nadj]));
        if (ts->vecs_sensip) {
          /* dGdP lambda_{n+1} */
          PetscCall(MatMultTranspose(ts->Jacprhs, ts->vecs_sensi[nadj], VecsSensiPTemp[nadj]));
          /* mu_n += h b[i] dGdP lambda_{n+1} */
          PetscCall(VecAXPY(ts->vecs_sensip[nadj], b[i] * adjoint_time_step, VecsSensiPTemp[nadj]));
        }
      }
      /* Build LHS for first-order adjoint */
      if (At[i * s + i] == 0) { // This stage is explicit
        PetscCall(VecScale(VecsDeltaLam[nadj * s + i], adjoint_time_step));
      } else {
        KSP                ksp;
        KSPConvergedReason kspreason;
        PetscCall(SNESGetKSP(ts->snes, &ksp));
        PetscCall(KSPSetOperators(ksp, Jim, Jimpre));
        PetscCall(VecScale(VecsDeltaLam[nadj * s + i], 1. / At[i * s + i]));
        PetscCall(KSPSolveTranspose(ksp, VecsDeltaLam[nadj * s + i], VecsDeltaLam[nadj * s + i]));
        PetscCall(KSPGetConvergedReason(ksp, &kspreason));
        if (kspreason < 0) {
          ts->reason = TSADJOINT_DIVERGED_LINEAR_SOLVE;
          PetscCall(PetscInfo(ts, "Step=%" PetscInt_FMT ", %" PetscInt_FMT "th cost function, transposed linear solve fails, stopping 1st-order adjoint solve\n", ts->steps, nadj));
        }
        if (ts->vecs_sensip) {
          /* -dHdP lambda_s[i] */
          PetscCall(MatMultTranspose(ts->Jacp, VecsDeltaLam[nadj * s + i], VecsSensiPTemp[nadj]));
          /* mu_n += h at[i][i] dHdP lambda_s[i] */
          PetscCall(VecAXPY(ts->vecs_sensip[nadj], -At[i * s + i] * adjoint_time_step, VecsSensiPTemp[nadj]));
        }
      }
    }
  }
  for (j = 0; j < s; j++) w[j] = 1.0;
  for (nadj = 0; nadj < ts->numcost; nadj++) // no need to do this for mu's
    PetscCall(VecMAXPY(ts->vecs_sensi[nadj], s, w, &VecsDeltaLam[nadj * s]));
  ark->status = TS_STEP_COMPLETE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSInterpolate_ARKIMEX(TS ts, PetscReal itime, Vec X)
{
  TS_ARKIMEX      *ark = (TS_ARKIMEX *)ts->data;
  PetscInt         s = ark->tableau->s, pinterp = ark->tableau->pinterp, i, j;
  PetscReal        h;
  PetscReal        tt, t;
  PetscScalar     *bt, *b;
  const PetscReal *Bt = ark->tableau->binterpt, *B = ark->tableau->binterp;

  PetscFunctionBegin;
  PetscCheck(Bt && B, PetscObjectComm((PetscObject)ts), PETSC_ERR_SUP, "TSARKIMEX %s does not have an interpolation formula", ark->tableau->name);
  switch (ark->status) {
  case TS_STEP_INCOMPLETE:
  case TS_STEP_PENDING:
    h = ts->time_step;
    t = (itime - ts->ptime) / h;
    break;
  case TS_STEP_COMPLETE:
    h = ts->ptime - ts->ptime_prev;
    t = (itime - ts->ptime) / h + 1; /* In the interval [0,1] */
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)ts), PETSC_ERR_PLIB, "Invalid TSStepStatus");
  }
  PetscCall(PetscMalloc2(s, &bt, s, &b));
  for (i = 0; i < s; i++) bt[i] = b[i] = 0;
  for (j = 0, tt = t; j < pinterp; j++, tt *= t) {
    for (i = 0; i < s; i++) {
      bt[i] += h * Bt[i * pinterp + j] * tt;
      b[i] += h * B[i * pinterp + j] * tt;
    }
  }
  PetscCall(VecCopy(ark->Y[0], X));
  PetscCall(VecMAXPY(X, s, bt, ark->YdotI));
  PetscCall(VecMAXPY(X, s, b, ark->YdotRHS));
  PetscCall(PetscFree2(bt, b));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSExtrapolate_ARKIMEX(TS ts, PetscReal c, Vec X)
{
  TS_ARKIMEX      *ark = (TS_ARKIMEX *)ts->data;
  PetscInt         s = ark->tableau->s, pinterp = ark->tableau->pinterp, i, j;
  PetscReal        h, h_prev, t, tt;
  PetscScalar     *bt, *b;
  const PetscReal *Bt = ark->tableau->binterpt, *B = ark->tableau->binterp;

  PetscFunctionBegin;
  PetscCheck(Bt && B, PetscObjectComm((PetscObject)ts), PETSC_ERR_SUP, "TSARKIMEX %s does not have an interpolation formula", ark->tableau->name);
  PetscCall(PetscCalloc2(s, &bt, s, &b));
  h      = ts->time_step;
  h_prev = ts->ptime - ts->ptime_prev;
  t      = 1 + h / h_prev * c;
  for (j = 0, tt = t; j < pinterp; j++, tt *= t) {
    for (i = 0; i < s; i++) {
      bt[i] += h * Bt[i * pinterp + j] * tt;
      b[i] += h * B[i * pinterp + j] * tt;
    }
  }
  PetscCheck(ark->Y_prev, PetscObjectComm((PetscObject)ts), PETSC_ERR_SUP, "Stages from previous step have not been stored");
  PetscCall(VecCopy(ark->Y_prev[0], X));
  PetscCall(VecMAXPY(X, s, bt, ark->YdotI_prev));
  PetscCall(VecMAXPY(X, s, b, ark->YdotRHS_prev));
  PetscCall(PetscFree2(bt, b));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*------------------------------------------------------------*/

static PetscErrorCode TSARKIMEXTableauReset(TS ts)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  ARKTableau  tab = ark->tableau;

  PetscFunctionBegin;
  if (!tab) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscFree(ark->work));
  PetscCall(VecDestroyVecs(tab->s, &ark->Y));
  PetscCall(VecDestroyVecs(tab->s, &ark->YdotI));
  PetscCall(VecDestroyVecs(tab->s, &ark->YdotRHS));
  PetscCall(VecDestroyVecs(tab->s, &ark->Y_prev));
  PetscCall(VecDestroyVecs(tab->s, &ark->YdotI_prev));
  PetscCall(VecDestroyVecs(tab->s, &ark->YdotRHS_prev));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSReset_ARKIMEX(TS ts)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;

  PetscFunctionBegin;
  PetscCall(TSARKIMEXTableauReset(ts));
  PetscCall(VecDestroy(&ark->Ydot));
  PetscCall(VecDestroy(&ark->Ydot0));
  PetscCall(VecDestroy(&ark->Z));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSAdjointReset_ARKIMEX(TS ts)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  ARKTableau  tab = ark->tableau;

  PetscFunctionBegin;
  PetscCall(VecDestroyVecs(tab->s * ts->numcost, &ark->VecsDeltaLam));
  PetscCall(VecDestroyVecs(ts->numcost, &ark->VecsSensiTemp));
  PetscCall(VecDestroyVecs(ts->numcost, &ark->VecsSensiPTemp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSARKIMEXGetVecs(TS ts, DM dm, Vec *Z, Vec *Ydot)
{
  TS_ARKIMEX *ax = (TS_ARKIMEX *)ts->data;

  PetscFunctionBegin;
  if (Z) {
    if (dm && dm != ts->dm) {
      PetscCall(DMGetNamedGlobalVector(dm, "TSARKIMEX_Z", Z));
    } else *Z = ax->Z;
  }
  if (Ydot) {
    if (dm && dm != ts->dm) {
      PetscCall(DMGetNamedGlobalVector(dm, "TSARKIMEX_Ydot", Ydot));
    } else *Ydot = ax->Ydot;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSARKIMEXRestoreVecs(TS ts, DM dm, Vec *Z, Vec *Ydot)
{
  PetscFunctionBegin;
  if (Z) {
    if (dm && dm != ts->dm) PetscCall(DMRestoreNamedGlobalVector(dm, "TSARKIMEX_Z", Z));
  }
  if (Ydot) {
    if (dm && dm != ts->dm) PetscCall(DMRestoreNamedGlobalVector(dm, "TSARKIMEX_Ydot", Ydot));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  This defines the nonlinear equation that is to be solved with SNES
  G(U) = F[t0+Theta*dt, U, (U-U0)*shift] = 0
*/
static PetscErrorCode SNESTSFormFunction_ARKIMEX(SNES snes, Vec X, Vec F, TS ts)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  DM          dm, dmsave;
  Vec         Z, Ydot;
  PetscReal   shift = ark->scoeff / ts->time_step;

  PetscFunctionBegin;
  PetscCall(SNESGetDM(snes, &dm));
  PetscCall(TSARKIMEXGetVecs(ts, dm, &Z, &Ydot));
  PetscCall(VecAXPBYPCZ(Ydot, -shift, shift, 0, Z, X)); /* Ydot = shift*(X-Z) */
  dmsave = ts->dm;
  ts->dm = dm;

  PetscCall(TSComputeIFunction(ts, ark->stage_time, X, Ydot, F, ark->imex));

  ts->dm = dmsave;
  PetscCall(TSARKIMEXRestoreVecs(ts, dm, &Z, &Ydot));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode SNESTSFormJacobian_ARKIMEX(SNES snes, Vec X, Mat A, Mat B, TS ts)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  DM          dm, dmsave;
  Vec         Ydot;
  PetscReal   shift = ark->scoeff / ts->time_step;

  PetscFunctionBegin;
  PetscCall(SNESGetDM(snes, &dm));
  PetscCall(TSARKIMEXGetVecs(ts, dm, NULL, &Ydot));
  /* ark->Ydot has already been computed in SNESTSFormFunction_ARKIMEX (SNES guarantees this) */
  dmsave = ts->dm;
  ts->dm = dm;

  PetscCall(TSComputeIJacobian(ts, ark->stage_time, X, Ydot, shift, A, B, ark->imex));

  ts->dm = dmsave;
  PetscCall(TSARKIMEXRestoreVecs(ts, dm, NULL, &Ydot));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSGetStages_ARKIMEX(TS ts, PetscInt *ns, Vec *Y[])
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;

  PetscFunctionBegin;
  if (ns) *ns = ark->tableau->s;
  if (Y) *Y = ark->Y;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMCoarsenHook_TSARKIMEX(DM fine, DM coarse, void *ctx)
{
  PetscFunctionBegin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMRestrictHook_TSARKIMEX(DM fine, Mat restrct, Vec rscale, Mat inject, DM coarse, void *ctx)
{
  TS  ts = (TS)ctx;
  Vec Z, Z_c;

  PetscFunctionBegin;
  PetscCall(TSARKIMEXGetVecs(ts, fine, &Z, NULL));
  PetscCall(TSARKIMEXGetVecs(ts, coarse, &Z_c, NULL));
  PetscCall(MatRestrict(restrct, Z, Z_c));
  PetscCall(VecPointwiseMult(Z_c, rscale, Z_c));
  PetscCall(TSARKIMEXRestoreVecs(ts, fine, &Z, NULL));
  PetscCall(TSARKIMEXRestoreVecs(ts, coarse, &Z_c, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMSubDomainHook_TSARKIMEX(DM dm, DM subdm, void *ctx)
{
  PetscFunctionBegin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMSubDomainRestrictHook_TSARKIMEX(DM dm, VecScatter gscat, VecScatter lscat, DM subdm, void *ctx)
{
  TS  ts = (TS)ctx;
  Vec Z, Z_c;

  PetscFunctionBegin;
  PetscCall(TSARKIMEXGetVecs(ts, dm, &Z, NULL));
  PetscCall(TSARKIMEXGetVecs(ts, subdm, &Z_c, NULL));

  PetscCall(VecScatterBegin(gscat, Z, Z_c, INSERT_VALUES, SCATTER_FORWARD));
  PetscCall(VecScatterEnd(gscat, Z, Z_c, INSERT_VALUES, SCATTER_FORWARD));

  PetscCall(TSARKIMEXRestoreVecs(ts, dm, &Z, NULL));
  PetscCall(TSARKIMEXRestoreVecs(ts, subdm, &Z_c, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSARKIMEXTableauSetUp(TS ts)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  ARKTableau  tab = ark->tableau;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(tab->s, &ark->work));
  PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->Y));
  PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->YdotI));
  PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->YdotRHS));
  if (ark->extrapolate) {
    PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->Y_prev));
    PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->YdotI_prev));
    PetscCall(VecDuplicateVecs(ts->vec_sol, tab->s, &ark->YdotRHS_prev));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSSetUp_ARKIMEX(TS ts)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  DM          dm;
  SNES        snes;

  PetscFunctionBegin;
  PetscCall(TSARKIMEXTableauSetUp(ts));
  PetscCall(VecDuplicate(ts->vec_sol, &ark->Ydot));
  PetscCall(VecDuplicate(ts->vec_sol, &ark->Ydot0));
  PetscCall(VecDuplicate(ts->vec_sol, &ark->Z));
  PetscCall(TSGetDM(ts, &dm));
  PetscCall(DMCoarsenHookAdd(dm, DMCoarsenHook_TSARKIMEX, DMRestrictHook_TSARKIMEX, ts));
  PetscCall(DMSubDomainHookAdd(dm, DMSubDomainHook_TSARKIMEX, DMSubDomainRestrictHook_TSARKIMEX, ts));
  PetscCall(TSGetSNES(ts, &snes));
  PetscFunctionReturn(PETSC_SUCCESS);
}
/*------------------------------------------------------------*/

static PetscErrorCode TSAdjointSetUp_ARKIMEX(TS ts)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  ARKTableau  tab = ark->tableau;

  PetscFunctionBegin;
  PetscCall(VecDuplicateVecs(ts->vecs_sensi[0], tab->s * ts->numcost, &ark->VecsDeltaLam));
  PetscCall(VecDuplicateVecs(ts->vecs_sensi[0], ts->numcost, &ark->VecsSensiTemp));
  if (ts->vecs_sensip) { PetscCall(VecDuplicateVecs(ts->vecs_sensip[0], ts->numcost, &ark->VecsSensiPTemp)); }
  if (PetscDefined(USE_DEBUG)) {
    PetscBool id = PETSC_FALSE;
    PetscCall(TSARKIMEXTestMassIdentity(ts, &id));
    PetscCheck(id, PetscObjectComm((PetscObject)ts), PETSC_ERR_ARG_INCOMP, "Adjoint ARKIMEX requires an identity mass matrix, however the TSIFunction you provide does not utilize an identity mass matrix");
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSSetFromOptions_ARKIMEX(TS ts, PetscOptionItems *PetscOptionsObject)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "ARKIMEX ODE solver options");
  {
    ARKTableauLink link;
    PetscInt       count, choice;
    PetscBool      flg;
    const char   **namelist;
    for (link = ARKTableauList, count = 0; link; link = link->next, count++)
      ;
    PetscCall(PetscMalloc1(count, (char ***)&namelist));
    for (link = ARKTableauList, count = 0; link; link = link->next, count++) namelist[count] = link->tab.name;
    PetscCall(PetscOptionsEList("-ts_arkimex_type", "Family of ARK IMEX method", "TSARKIMEXSetType", (const char *const *)namelist, count, ark->tableau->name, &choice, &flg));
    if (flg) PetscCall(TSARKIMEXSetType(ts, namelist[choice]));
    PetscCall(PetscFree(namelist));

    flg = (PetscBool)!ark->imex;
    PetscCall(PetscOptionsBool("-ts_arkimex_fully_implicit", "Solve the problem fully implicitly", "TSARKIMEXSetFullyImplicit", flg, &flg, NULL));
    ark->imex = (PetscBool)!flg;
    PetscCall(PetscOptionsBool("-ts_arkimex_initial_guess_extrapolate", "Extrapolate the initial guess for the stage solution from stage values of the previous time step", "", ark->extrapolate, &ark->extrapolate, NULL));
  }
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSView_ARKIMEX(TS ts, PetscViewer viewer)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;
  PetscBool   iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) {
    ARKTableau    tab = ark->tableau;
    TSARKIMEXType arktype;
    char          buf[512];
    PetscBool     flg;

    PetscCall(TSARKIMEXGetType(ts, &arktype));
    PetscCall(TSARKIMEXGetFullyImplicit(ts, &flg));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  ARK IMEX %s\n", arktype));
    PetscCall(PetscFormatRealArray(buf, sizeof(buf), "% 8.6f", tab->s, tab->ct));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  Stiff abscissa       ct = %s\n", buf));
    PetscCall(PetscFormatRealArray(buf, sizeof(buf), "% 8.6f", tab->s, tab->c));
    PetscCall(PetscViewerASCIIPrintf(viewer, "Fully implicit: %s\n", flg ? "yes" : "no"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "Stiffly accurate: %s\n", tab->stiffly_accurate ? "yes" : "no"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "Explicit first stage: %s\n", tab->explicit_first_stage ? "yes" : "no"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "FSAL property: %s\n", tab->FSAL_implicit ? "yes" : "no"));
    PetscCall(PetscViewerASCIIPrintf(viewer, "  Nonstiff abscissa     c = %s\n", buf));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSLoad_ARKIMEX(TS ts, PetscViewer viewer)
{
  SNES    snes;
  TSAdapt adapt;

  PetscFunctionBegin;
  PetscCall(TSGetAdapt(ts, &adapt));
  PetscCall(TSAdaptLoad(adapt, viewer));
  PetscCall(TSGetSNES(ts, &snes));
  PetscCall(SNESLoad(snes, viewer));
  /* function and Jacobian context for SNES when used with TS is always ts object */
  PetscCall(SNESSetFunction(snes, NULL, NULL, ts));
  PetscCall(SNESSetJacobian(snes, NULL, NULL, NULL, ts));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  TSARKIMEXSetType - Set the type of `TSARKIMEX` scheme

  Logically Collective

  Input Parameters:
+  ts - timestepping context
-  arktype - type of `TSARKIMEX` scheme

  Options Database Key:
.  -ts_arkimex_type <1bee,a2,l2,ars122,2c,2d,2e,prssp2,3,bpr3,ars443,4,5> - set `TSARKIMEX` scheme type

  Level: intermediate

.seealso: [](chapter_ts), `TSARKIMEXGetType()`, `TSARKIMEX`, `TSARKIMEXType`, `TSARKIMEX1BEE`, `TSARKIMEXA2`, `TSARKIMEXL2`, `TSARKIMEXARS122`, `TSARKIMEX2C`, `TSARKIMEX2D`, `TSARKIMEX2E`, `TSARKIMEXPRSSP2`,
          `TSARKIMEX3`, `TSARKIMEXBPR3`, `TSARKIMEXARS443`, `TSARKIMEX4`, `TSARKIMEX5`
@*/
PetscErrorCode TSARKIMEXSetType(TS ts, TSARKIMEXType arktype)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID, 1);
  PetscValidCharPointer(arktype, 2);
  PetscTryMethod(ts, "TSARKIMEXSetType_C", (TS, TSARKIMEXType), (ts, arktype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  TSARKIMEXGetType - Get the type of `TSARKIMEX` scheme

  Logically Collective

  Input Parameter:
.  ts - timestepping context

  Output Parameter:
.  arktype - type of `TSARKIMEX` scheme

  Level: intermediate

.seealso: [](chapter_ts), `TSARKIMEX`c, `TSARKIMEXGetType()`
@*/
PetscErrorCode TSARKIMEXGetType(TS ts, TSARKIMEXType *arktype)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID, 1);
  PetscUseMethod(ts, "TSARKIMEXGetType_C", (TS, TSARKIMEXType *), (ts, arktype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  TSARKIMEXSetFullyImplicit - Solve both parts of the equation implicitly, including the part that is normally solved explicitly

  Logically Collective

  Input Parameters:
+  ts - timestepping context
-  flg - `PETSC_TRUE` for fully implicit

  Level: intermediate

.seealso: [](chapter_ts), `TSARKIMEX`, `TSARKIMEXGetType()`, `TSARKIMEXGetFullyImplicit()`
@*/
PetscErrorCode TSARKIMEXSetFullyImplicit(TS ts, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID, 1);
  PetscValidLogicalCollectiveBool(ts, flg, 2);
  PetscTryMethod(ts, "TSARKIMEXSetFullyImplicit_C", (TS, PetscBool), (ts, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  TSARKIMEXGetFullyImplicit - Inquires if both parts of the equation are solved implicitly

  Logically Collective

  Input Parameter:
.  ts - timestepping context

  Output Parameter:
.  flg - `PETSC_TRUE` for fully implicit

  Level: intermediate

.seealso: [](chapter_ts), `TSARKIMEXGetType()`, `TSARKIMEXSetFullyImplicit()`
@*/
PetscErrorCode TSARKIMEXGetFullyImplicit(TS ts, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ts, TS_CLASSID, 1);
  PetscValidBoolPointer(flg, 2);
  PetscUseMethod(ts, "TSARKIMEXGetFullyImplicit_C", (TS, PetscBool *), (ts, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSARKIMEXGetType_ARKIMEX(TS ts, TSARKIMEXType *arktype)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;

  PetscFunctionBegin;
  *arktype = ark->tableau->name;
  PetscFunctionReturn(PETSC_SUCCESS);
}
static PetscErrorCode TSARKIMEXSetType_ARKIMEX(TS ts, TSARKIMEXType arktype)
{
  TS_ARKIMEX    *ark = (TS_ARKIMEX *)ts->data;
  PetscBool      match;
  ARKTableauLink link;

  PetscFunctionBegin;
  if (ark->tableau) {
    PetscCall(PetscStrcmp(ark->tableau->name, arktype, &match));
    if (match) PetscFunctionReturn(PETSC_SUCCESS);
  }
  for (link = ARKTableauList; link; link = link->next) {
    PetscCall(PetscStrcmp(link->tab.name, arktype, &match));
    if (match) {
      if (ts->setupcalled) PetscCall(TSARKIMEXTableauReset(ts));
      ark->tableau = &link->tab;
      if (ts->setupcalled) PetscCall(TSARKIMEXTableauSetUp(ts));
      ts->default_adapt_type = ark->tableau->bembed ? TSADAPTBASIC : TSADAPTNONE;
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }
  SETERRQ(PetscObjectComm((PetscObject)ts), PETSC_ERR_ARG_UNKNOWN_TYPE, "Could not find '%s'", arktype);
}

static PetscErrorCode TSARKIMEXSetFullyImplicit_ARKIMEX(TS ts, PetscBool flg)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;

  PetscFunctionBegin;
  ark->imex = (PetscBool)!flg;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSARKIMEXGetFullyImplicit_ARKIMEX(TS ts, PetscBool *flg)
{
  TS_ARKIMEX *ark = (TS_ARKIMEX *)ts->data;

  PetscFunctionBegin;
  *flg = (PetscBool)!ark->imex;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode TSDestroy_ARKIMEX(TS ts)
{
  PetscFunctionBegin;
  PetscCall(TSReset_ARKIMEX(ts));
  if (ts->dm) {
    PetscCall(DMCoarsenHookRemove(ts->dm, DMCoarsenHook_TSARKIMEX, DMRestrictHook_TSARKIMEX, ts));
    PetscCall(DMSubDomainHookRemove(ts->dm, DMSubDomainHook_TSARKIMEX, DMSubDomainRestrictHook_TSARKIMEX, ts));
  }
  PetscCall(PetscFree(ts->data));
  PetscCall(PetscObjectComposeFunction((PetscObject)ts, "TSARKIMEXGetType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ts, "TSARKIMEXSetType_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ts, "TSARKIMEXSetFullyImplicit_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)ts, "TSARKIMEXGetFullyImplicit_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* ------------------------------------------------------------ */
/*MC
      TSARKIMEX - ODE and DAE solver using additive Runge-Kutta IMEX schemes

  These methods are intended for problems with well-separated time scales, especially when a slow scale is strongly
  nonlinear such that it is expensive to solve with a fully implicit method. The user should provide the stiff part
  of the equation using `TSSetIFunction()` and the non-stiff part with `TSSetRHSFunction()`.

  Level: beginner

  Notes:
  The default is `TSARKIMEX3`, it can be changed with `TSARKIMEXSetType()` or -ts_arkimex_type

  If the equation is implicit or a DAE, then `TSSetEquationType()` needs to be set accordingly. Refer to the manual for further information.

  Methods with an explicit stage can only be used with ODE in which the stiff part G(t,X,Xdot) has the form Xdot + Ghat(t,X).

  Consider trying `TSROSW` if the stiff part is linear or weakly nonlinear.

.seealso: [](chapter_ts), `TSCreate()`, `TS`, `TSSetType()`, `TSARKIMEXSetType()`, `TSARKIMEXGetType()`, `TSARKIMEXSetFullyImplicit()`, `TSARKIMEXGetFullyImplicit()`,
          `TSARKIMEX1BEE`, `TSARKIMEX2C`, `TSARKIMEX2D`, `TSARKIMEX2E`, `TSARKIMEX3`, `TSARKIMEXL2`, `TSARKIMEXA2`, `TSARKIMEXARS122`,
          `TSARKIMEX4`, `TSARKIMEX5`, `TSARKIMEXPRSSP2`, `TSARKIMEXARS443`, `TSARKIMEXBPR3`, `TSARKIMEXType`, `TSARKIMEXRegister()`, `TSType`
M*/
PETSC_EXTERN PetscErrorCode TSCreate_ARKIMEX(TS ts)
{
  TS_ARKIMEX *ark;

  PetscFunctionBegin;
  PetscCall(TSARKIMEXInitializePackage());

  ts->ops->reset          = TSReset_ARKIMEX;
  ts->ops->adjointreset   = TSAdjointReset_ARKIMEX;
  ts->ops->destroy        = TSDestroy_ARKIMEX;
  ts->ops->view           = TSView_ARKIMEX;
  ts->ops->load           = TSLoad_ARKIMEX;
  ts->ops->setup          = TSSetUp_ARKIMEX;
  ts->ops->adjointsetup   = TSAdjointSetUp_ARKIMEX;
  ts->ops->step           = TSStep_ARKIMEX;
  ts->ops->interpolate    = TSInterpolate_ARKIMEX;
  ts->ops->evaluatestep   = TSEvaluateStep_ARKIMEX;
  ts->ops->rollback       = TSRollBack_ARKIMEX;
  ts->ops->setfromoptions = TSSetFromOptions_ARKIMEX;
  ts->ops->snesfunction   = SNESTSFormFunction_ARKIMEX;
  ts->ops->snesjacobian   = SNESTSFormJacobian_ARKIMEX;
  ts->ops->getstages      = TSGetStages_ARKIMEX;
  ts->ops->adjointstep    = TSAdjointStep_ARKIMEX;

  ts->usessnes = PETSC_TRUE;

  PetscCall(PetscNew(&ark));
  ts->data  = (void *)ark;
  ark->imex = PETSC_TRUE;

  ark->VecsDeltaLam   = NULL;
  ark->VecsSensiTemp  = NULL;
  ark->VecsSensiPTemp = NULL;

  PetscCall(PetscObjectComposeFunction((PetscObject)ts, "TSARKIMEXGetType_C", TSARKIMEXGetType_ARKIMEX));
  PetscCall(PetscObjectComposeFunction((PetscObject)ts, "TSARKIMEXSetType_C", TSARKIMEXSetType_ARKIMEX));
  PetscCall(PetscObjectComposeFunction((PetscObject)ts, "TSARKIMEXSetFullyImplicit_C", TSARKIMEXSetFullyImplicit_ARKIMEX));
  PetscCall(PetscObjectComposeFunction((PetscObject)ts, "TSARKIMEXGetFullyImplicit_C", TSARKIMEXGetFullyImplicit_ARKIMEX));

  PetscCall(TSARKIMEXSetType(ts, TSARKIMEXDefault));
  PetscFunctionReturn(PETSC_SUCCESS);
}

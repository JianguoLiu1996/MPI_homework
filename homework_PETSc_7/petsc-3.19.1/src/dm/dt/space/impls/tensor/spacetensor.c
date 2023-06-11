#include <petsc/private/petscfeimpl.h> /*I "petscfe.h" I*/

static PetscErrorCode PetscSpaceTensorCreateSubspace(PetscSpace space, PetscInt Nvs, PetscInt Ncs, PetscSpace *subspace)
{
  PetscInt    degree;
  const char *prefix;
  const char *name;
  char        subname[PETSC_MAX_PATH_LEN];

  PetscFunctionBegin;
  PetscCall(PetscSpaceGetDegree(space, &degree, NULL));
  PetscCall(PetscObjectGetOptionsPrefix((PetscObject)space, &prefix));
  PetscCall(PetscSpaceCreate(PetscObjectComm((PetscObject)space), subspace));
  PetscCall(PetscSpaceSetType(*subspace, PETSCSPACEPOLYNOMIAL));
  PetscCall(PetscSpaceSetNumVariables(*subspace, Nvs));
  PetscCall(PetscSpaceSetNumComponents(*subspace, Ncs));
  PetscCall(PetscSpaceSetDegree(*subspace, degree, PETSC_DETERMINE));
  PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*subspace, prefix));
  PetscCall(PetscObjectAppendOptionsPrefix((PetscObject)*subspace, "tensorcomp_"));
  if (((PetscObject)space)->name) {
    PetscCall(PetscObjectGetName((PetscObject)space, &name));
    PetscCall(PetscSNPrintf(subname, PETSC_MAX_PATH_LEN - 1, "%s tensor component", name));
    PetscCall(PetscObjectSetName((PetscObject)*subspace, subname));
  } else PetscCall(PetscObjectSetName((PetscObject)*subspace, "tensor component"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceSetFromOptions_Tensor(PetscSpace sp, PetscOptionItems *PetscOptionsObject)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)sp->data;
  PetscInt           Ns, Nc, i, Nv, deg;
  PetscBool          uniform = PETSC_TRUE;

  PetscFunctionBegin;
  PetscCall(PetscSpaceGetNumVariables(sp, &Nv));
  if (!Nv) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscSpaceGetNumComponents(sp, &Nc));
  PetscCall(PetscSpaceTensorGetNumSubspaces(sp, &Ns));
  PetscCall(PetscSpaceGetDegree(sp, &deg, NULL));
  if (Ns > 1) {
    PetscSpace s0;

    PetscCall(PetscSpaceTensorGetSubspace(sp, 0, &s0));
    for (i = 1; i < Ns; i++) {
      PetscSpace si;

      PetscCall(PetscSpaceTensorGetSubspace(sp, i, &si));
      if (si != s0) {
        uniform = PETSC_FALSE;
        break;
      }
    }
  }
  Ns = (Ns == PETSC_DEFAULT) ? PetscMax(Nv, 1) : Ns;
  PetscOptionsHeadBegin(PetscOptionsObject, "PetscSpace tensor options");
  PetscCall(PetscOptionsBoundedInt("-petscspace_tensor_spaces", "The number of subspaces", "PetscSpaceTensorSetNumSubspaces", Ns, &Ns, NULL, 0));
  PetscCall(PetscOptionsBool("-petscspace_tensor_uniform", "Subspaces are identical", "PetscSpaceTensorSetFromOptions", uniform, &uniform, NULL));
  PetscOptionsHeadEnd();
  PetscCheck(Ns >= 0 && (Nv <= 0 || Ns != 0), PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_OUTOFRANGE, "Cannot have a tensor space made up of %" PetscInt_FMT " spaces", Ns);
  PetscCheck(Nv <= 0 || Ns <= Nv, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_OUTOFRANGE, "Cannot have a tensor space with %" PetscInt_FMT " subspaces over %" PetscInt_FMT " variables", Ns, Nv);
  if (Ns != tens->numTensSpaces) PetscCall(PetscSpaceTensorSetNumSubspaces(sp, Ns));
  if (uniform) {
    PetscInt   Nvs = Nv / Ns;
    PetscInt   Ncs;
    PetscSpace subspace;

    PetscCheck(Nv % Ns == 0, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_WRONG, "Cannot use %" PetscInt_FMT " uniform subspaces for %" PetscInt_FMT " variable space", Ns, Nv);
    Ncs = (PetscInt)PetscPowReal((PetscReal)Nc, 1. / Ns);
    PetscCheck(Nc % PetscPowInt(Ncs, Ns) == 0, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_WRONG, "Cannot use %" PetscInt_FMT " uniform subspaces for %" PetscInt_FMT " component space", Ns, Nc);
    PetscCall(PetscSpaceTensorGetSubspace(sp, 0, &subspace));
    if (!subspace) PetscCall(PetscSpaceTensorCreateSubspace(sp, Nvs, Ncs, &subspace));
    else PetscCall(PetscObjectReference((PetscObject)subspace));
    PetscCall(PetscSpaceSetFromOptions(subspace));
    for (i = 0; i < Ns; i++) PetscCall(PetscSpaceTensorSetSubspace(sp, i, subspace));
    PetscCall(PetscSpaceDestroy(&subspace));
  } else {
    for (i = 0; i < Ns; i++) {
      PetscSpace subspace;

      PetscCall(PetscSpaceTensorGetSubspace(sp, i, &subspace));
      if (!subspace) {
        char tprefix[128];

        PetscCall(PetscSpaceTensorCreateSubspace(sp, 1, 1, &subspace));
        PetscCall(PetscSNPrintf(tprefix, 128, "%d_", (int)i));
        PetscCall(PetscObjectAppendOptionsPrefix((PetscObject)subspace, tprefix));
      } else PetscCall(PetscObjectReference((PetscObject)subspace));
      PetscCall(PetscSpaceSetFromOptions(subspace));
      PetscCall(PetscSpaceTensorSetSubspace(sp, i, subspace));
      PetscCall(PetscSpaceDestroy(&subspace));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceTensorView_Ascii(PetscSpace sp, PetscViewer v)
{
  PetscSpace_Tensor *tens    = (PetscSpace_Tensor *)sp->data;
  PetscBool          uniform = PETSC_TRUE;
  PetscInt           Ns      = tens->numTensSpaces, i, n;

  PetscFunctionBegin;
  for (i = 1; i < Ns; i++) {
    if (tens->tensspaces[i] != tens->tensspaces[0]) {
      uniform = PETSC_FALSE;
      break;
    }
  }
  if (uniform) PetscCall(PetscViewerASCIIPrintf(v, "Tensor space of %" PetscInt_FMT " subspaces (all identical)\n", Ns));
  else PetscCall(PetscViewerASCIIPrintf(v, "Tensor space of %" PetscInt_FMT " subspaces\n", Ns));
  n = uniform ? 1 : Ns;
  for (i = 0; i < n; i++) {
    PetscCall(PetscViewerASCIIPushTab(v));
    PetscCall(PetscSpaceView(tens->tensspaces[i], v));
    PetscCall(PetscViewerASCIIPopTab(v));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceView_Tensor(PetscSpace sp, PetscViewer viewer)
{
  PetscBool iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &iascii));
  if (iascii) PetscCall(PetscSpaceTensorView_Ascii(sp, viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceSetUp_Tensor(PetscSpace sp)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)sp->data;
  PetscInt           Nc, Nv, Ns;
  PetscBool          uniform = PETSC_TRUE;
  PetscInt           deg, maxDeg;
  PetscInt           Ncprod;

  PetscFunctionBegin;
  if (tens->setupCalled) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscSpaceGetNumVariables(sp, &Nv));
  PetscCall(PetscSpaceGetNumComponents(sp, &Nc));
  PetscCall(PetscSpaceTensorGetNumSubspaces(sp, &Ns));
  if (Ns == PETSC_DEFAULT) {
    Ns = Nv;
    PetscCall(PetscSpaceTensorSetNumSubspaces(sp, Ns));
  }
  if (!Ns) {
    SETERRQ(PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_OUTOFRANGE, "Cannot have zero subspaces");
  } else {
    PetscSpace s0 = NULL;

    PetscCheck(Nv <= 0 || Ns <= Nv, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_OUTOFRANGE, "Cannot have a tensor space with %" PetscInt_FMT " subspaces over %" PetscInt_FMT " variables", Ns, Nv);
    PetscCall(PetscSpaceTensorGetSubspace(sp, 0, &s0));
    for (PetscInt i = 1; i < Ns; i++) {
      PetscSpace si;

      PetscCall(PetscSpaceTensorGetSubspace(sp, i, &si));
      if (si != s0) {
        uniform = PETSC_FALSE;
        break;
      }
    }
    if (uniform) {
      PetscInt Nvs = Nv / Ns;
      PetscInt Ncs;

      PetscCheck(Nv % Ns == 0, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_WRONG, "Cannot use %" PetscInt_FMT " uniform subspaces for %" PetscInt_FMT " variable space", Ns, Nv);
      Ncs = (PetscInt)(PetscPowReal((PetscReal)Nc, 1. / Ns));
      PetscCheck(Nc % PetscPowInt(Ncs, Ns) == 0, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_WRONG, "Cannot use %" PetscInt_FMT " uniform subspaces for %" PetscInt_FMT " component space", Ns, Nc);
      if (!s0) PetscCall(PetscSpaceTensorCreateSubspace(sp, Nvs, Ncs, &s0));
      else PetscCall(PetscObjectReference((PetscObject)s0));
      PetscCall(PetscSpaceSetUp(s0));
      for (PetscInt i = 0; i < Ns; i++) PetscCall(PetscSpaceTensorSetSubspace(sp, i, s0));
      PetscCall(PetscSpaceDestroy(&s0));
      Ncprod = PetscPowInt(Ncs, Ns);
    } else {
      PetscInt Nvsum = 0;

      Ncprod = 1;
      for (PetscInt i = 0; i < Ns; i++) {
        PetscInt   Nvs, Ncs;
        PetscSpace si = NULL;

        PetscCall(PetscSpaceTensorGetSubspace(sp, i, &si));
        if (!si) PetscCall(PetscSpaceTensorCreateSubspace(sp, 1, 1, &si));
        else PetscCall(PetscObjectReference((PetscObject)si));
        PetscCall(PetscSpaceSetUp(si));
        PetscCall(PetscSpaceTensorSetSubspace(sp, i, si));
        PetscCall(PetscSpaceGetNumVariables(si, &Nvs));
        PetscCall(PetscSpaceGetNumComponents(si, &Ncs));
        PetscCall(PetscSpaceDestroy(&si));

        Nvsum += Nvs;
        Ncprod *= Ncs;
      }

      PetscCheck(Nvsum == Nv, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_WRONG, "Sum of subspace variables %" PetscInt_FMT " does not equal the number of variables %" PetscInt_FMT, Nvsum, Nv);
      PetscCheck(Nc % Ncprod == 0, PetscObjectComm((PetscObject)sp), PETSC_ERR_ARG_WRONG, "Product of subspace components %" PetscInt_FMT " does not divide the number of components %" PetscInt_FMT, Ncprod, Nc);
    }
    if (Ncprod != Nc) {
      PetscInt    Ncopies = Nc / Ncprod;
      PetscInt    Nv      = sp->Nv;
      const char *prefix;
      const char *name;
      char        subname[PETSC_MAX_PATH_LEN];
      PetscSpace  subsp;

      PetscCall(PetscSpaceCreate(PetscObjectComm((PetscObject)sp), &subsp));
      PetscCall(PetscObjectGetOptionsPrefix((PetscObject)sp, &prefix));
      PetscCall(PetscObjectSetOptionsPrefix((PetscObject)subsp, prefix));
      PetscCall(PetscObjectAppendOptionsPrefix((PetscObject)subsp, "sumcomp_"));
      if (((PetscObject)sp)->name) {
        PetscCall(PetscObjectGetName((PetscObject)sp, &name));
        PetscCall(PetscSNPrintf(subname, PETSC_MAX_PATH_LEN - 1, "%s sum component", name));
        PetscCall(PetscObjectSetName((PetscObject)subsp, subname));
      } else PetscCall(PetscObjectSetName((PetscObject)subsp, "sum component"));
      PetscCall(PetscSpaceSetType(subsp, PETSCSPACETENSOR));
      PetscCall(PetscSpaceSetNumVariables(subsp, Nv));
      PetscCall(PetscSpaceSetNumComponents(subsp, Ncprod));
      PetscCall(PetscSpaceTensorSetNumSubspaces(subsp, Ns));
      for (PetscInt i = 0; i < Ns; i++) {
        PetscSpace ssp;

        PetscCall(PetscSpaceTensorGetSubspace(sp, i, &ssp));
        PetscCall(PetscSpaceTensorSetSubspace(subsp, i, ssp));
      }
      PetscCall(PetscSpaceSetUp(subsp));
      PetscCall(PetscSpaceSetType(sp, PETSCSPACESUM));
      PetscCall(PetscSpaceSumSetNumSubspaces(sp, Ncopies));
      for (PetscInt i = 0; i < Ncopies; i++) PetscCall(PetscSpaceSumSetSubspace(sp, i, subsp));
      PetscCall(PetscSpaceDestroy(&subsp));
      PetscCall(PetscSpaceSetUp(sp));
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }
  deg    = PETSC_MAX_INT;
  maxDeg = 0;
  for (PetscInt i = 0; i < Ns; i++) {
    PetscSpace si;
    PetscInt   iDeg, iMaxDeg;

    PetscCall(PetscSpaceTensorGetSubspace(sp, i, &si));
    PetscCall(PetscSpaceGetDegree(si, &iDeg, &iMaxDeg));
    deg = PetscMin(deg, iDeg);
    maxDeg += iMaxDeg;
  }
  sp->degree        = deg;
  sp->maxDegree     = maxDeg;
  tens->uniform     = uniform;
  tens->setupCalled = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceDestroy_Tensor(PetscSpace sp)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)sp->data;
  PetscInt           Ns, i;

  PetscFunctionBegin;
  Ns = tens->numTensSpaces;
  if (tens->heightsubspaces) {
    PetscInt d;

    /* sp->Nv is the spatial dimension, so it is equal to the number
     * of subspaces on higher co-dimension points */
    for (d = 0; d < sp->Nv; ++d) PetscCall(PetscSpaceDestroy(&tens->heightsubspaces[d]));
  }
  PetscCall(PetscFree(tens->heightsubspaces));
  for (i = 0; i < Ns; i++) PetscCall(PetscSpaceDestroy(&tens->tensspaces[i]));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscSpaceTensorSetSubspace_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscSpaceTensorGetSubspace_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscSpaceTensorSetNumSubspaces_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscSpaceTensorGetNumSubspaces_C", NULL));
  PetscCall(PetscFree(tens->tensspaces));
  PetscCall(PetscFree(tens));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceGetDimension_Tensor(PetscSpace sp, PetscInt *dim)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)sp->data;
  PetscInt           i, Ns, d;

  PetscFunctionBegin;
  PetscCall(PetscSpaceSetUp(sp));
  Ns = tens->numTensSpaces;
  d  = 1;
  for (i = 0; i < Ns; i++) {
    PetscInt id;

    PetscCall(PetscSpaceGetDimension(tens->tensspaces[i], &id));
    d *= id;
  }
  *dim = d;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceEvaluate_Tensor(PetscSpace sp, PetscInt npoints, const PetscReal points[], PetscReal B[], PetscReal D[], PetscReal H[])
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)sp->data;
  DM                 dm   = sp->dm;
  PetscInt           Nc   = sp->Nc;
  PetscInt           Nv   = sp->Nv;
  PetscInt           Ns;
  PetscReal         *lpoints, *sB = NULL, *sD = NULL, *sH = NULL;
  PetscInt           pdim;

  PetscFunctionBegin;
  if (!tens->setupCalled) {
    PetscCall(PetscSpaceSetUp(sp));
    PetscCall(PetscSpaceEvaluate(sp, npoints, points, B, D, H));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  Ns = tens->numTensSpaces;
  PetscCall(PetscSpaceGetDimension(sp, &pdim));
  PetscCall(DMGetWorkArray(dm, npoints * Nv, MPIU_REAL, &lpoints));
  if (B || D || H) PetscCall(DMGetWorkArray(dm, npoints * pdim * Nc, MPIU_REAL, &sB));
  if (D || H) PetscCall(DMGetWorkArray(dm, npoints * pdim * Nc * Nv, MPIU_REAL, &sD));
  if (H) PetscCall(DMGetWorkArray(dm, npoints * pdim * Nc * Nv * Nv, MPIU_REAL, &sH));
  if (B) {
    for (PetscInt i = 0; i < npoints * pdim * Nc; i++) B[i] = 1.;
  }
  if (D) {
    for (PetscInt i = 0; i < npoints * pdim * Nc * Nv; i++) D[i] = 1.;
  }
  if (H) {
    for (PetscInt i = 0; i < npoints * pdim * Nc * Nv * Nv; i++) H[i] = 1.;
  }
  for (PetscInt s = 0, d = 0, vstep = 1, cstep = 1; s < Ns; s++) {
    PetscInt sNv, sNc, spdim;
    PetscInt vskip, cskip;

    PetscCall(PetscSpaceGetNumVariables(tens->tensspaces[s], &sNv));
    PetscCall(PetscSpaceGetNumComponents(tens->tensspaces[s], &sNc));
    PetscCall(PetscSpaceGetDimension(tens->tensspaces[s], &spdim));
    PetscCheck(!(pdim % vstep) && !(pdim % spdim), PETSC_COMM_SELF, PETSC_ERR_PLIB, "Bad tensor loop: Nv %" PetscInt_FMT ", Ns %" PetscInt_FMT ", pdim %" PetscInt_FMT ", s %" PetscInt_FMT ", vstep %" PetscInt_FMT ", spdim %" PetscInt_FMT, Nv, Ns, pdim, s, vstep, spdim);
    PetscCheck(!(Nc % cstep) && !(Nc % sNc), PETSC_COMM_SELF, PETSC_ERR_PLIB, "Bad tensor loop: Nv %" PetscInt_FMT ", Ns %" PetscInt_FMT ", Nc %" PetscInt_FMT ", s %" PetscInt_FMT ", cstep %" PetscInt_FMT ", sNc %" PetscInt_FMT, Nv, Ns, Nc, s, cstep, spdim);
    vskip = pdim / (vstep * spdim);
    cskip = Nc / (cstep * sNc);
    for (PetscInt p = 0; p < npoints; ++p) {
      for (PetscInt i = 0; i < sNv; i++) lpoints[p * sNv + i] = points[p * Nv + d + i];
    }
    PetscCall(PetscSpaceEvaluate(tens->tensspaces[s], npoints, lpoints, sB, sD, sH));
    if (B) {
      for (PetscInt k = 0; k < vskip; k++) {
        for (PetscInt si = 0; si < spdim; si++) {
          for (PetscInt j = 0; j < vstep; j++) {
            PetscInt i = (k * spdim + si) * vstep + j;

            for (PetscInt l = 0; l < cskip; l++) {
              for (PetscInt sc = 0; sc < sNc; sc++) {
                for (PetscInt m = 0; m < cstep; m++) {
                  PetscInt c = (l * sNc + sc) * cstep + m;

                  for (PetscInt p = 0; p < npoints; p++) B[(pdim * p + i) * Nc + c] *= sB[(spdim * p + si) * sNc + sc];
                }
              }
            }
          }
        }
      }
    }
    if (D) {
      for (PetscInt k = 0; k < vskip; k++) {
        for (PetscInt si = 0; si < spdim; si++) {
          for (PetscInt j = 0; j < vstep; j++) {
            PetscInt i = (k * spdim + si) * vstep + j;

            for (PetscInt l = 0; l < cskip; l++) {
              for (PetscInt sc = 0; sc < sNc; sc++) {
                for (PetscInt m = 0; m < cstep; m++) {
                  PetscInt c = (l * sNc + sc) * cstep + m;

                  for (PetscInt der = 0; der < Nv; der++) {
                    if (der >= d && der < d + sNv) {
                      for (PetscInt p = 0; p < npoints; p++) D[((pdim * p + i) * Nc + c) * Nv + der] *= sD[((spdim * p + si) * sNc + sc) * sNv + der - d];
                    } else {
                      for (PetscInt p = 0; p < npoints; p++) D[((pdim * p + i) * Nc + c) * Nv + der] *= sB[(spdim * p + si) * sNc + sc];
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    if (H) {
      for (PetscInt k = 0; k < vskip; k++) {
        for (PetscInt si = 0; si < spdim; si++) {
          for (PetscInt j = 0; j < vstep; j++) {
            PetscInt i = (k * spdim + si) * vstep + j;

            for (PetscInt l = 0; l < cskip; l++) {
              for (PetscInt sc = 0; sc < sNc; sc++) {
                for (PetscInt m = 0; m < cstep; m++) {
                  PetscInt c = (l * sNc + sc) * cstep + m;

                  for (PetscInt der = 0; der < Nv; der++) {
                    for (PetscInt der2 = 0; der2 < Nv; der2++) {
                      if (der >= d && der < d + sNv && der2 >= d && der2 < d + sNv) {
                        for (PetscInt p = 0; p < npoints; p++) H[(((pdim * p + i) * Nc + c) * Nv + der) * Nv + der2] *= sH[(((spdim * p + si) * sNc + sc) * sNv + der - d) * sNv + der2 - d];
                      } else if (der >= d && der < d + sNv) {
                        for (PetscInt p = 0; p < npoints; p++) H[(((pdim * p + i) * Nc + c) * Nv + der) * Nv + der2] *= sD[((spdim * p + si) * sNc + sc) * sNv + der - d];
                      } else if (der2 >= d && der2 < d + sNv) {
                        for (PetscInt p = 0; p < npoints; p++) H[(((pdim * p + i) * Nc + c) * Nv + der) * Nv + der2] *= sD[((spdim * p + si) * sNc + sc) * sNv + der2 - d];
                      } else {
                        for (PetscInt p = 0; p < npoints; p++) H[(((pdim * p + i) * Nc + c) * Nv + der) * Nv + der2] *= sB[(spdim * p + si) * sNc + sc];
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    d += sNv;
    vstep *= spdim;
    cstep *= sNc;
  }
  if (H) PetscCall(DMRestoreWorkArray(dm, npoints * pdim * Nc * Nv * Nv, MPIU_REAL, &sH));
  if (D || H) PetscCall(DMRestoreWorkArray(dm, npoints * pdim * Nc * Nv, MPIU_REAL, &sD));
  if (B || D || H) PetscCall(DMRestoreWorkArray(dm, npoints * pdim * Nc, MPIU_REAL, &sB));
  PetscCall(DMRestoreWorkArray(dm, npoints * Nv, MPIU_REAL, &lpoints));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceTensorSetNumSubspaces - Set the number of spaces in the tensor product space

  Input Parameters:
+ sp  - the function space object
- numTensSpaces - the number of spaces

  Level: intermediate

  Note:
  The name NumSubspaces is misleading because it is actually setting the number of defining spaces of the tensor product space, not a number of Subspaces of it

.seealso: `PETSCSPACETENSOR`, `PetscSpace`, `PetscSpaceTensorGetNumSubspaces()`, `PetscSpaceSetDegree()`, `PetscSpaceSetNumVariables()`
@*/
PetscErrorCode PetscSpaceTensorSetNumSubspaces(PetscSpace sp, PetscInt numTensSpaces)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscTryMethod(sp, "PetscSpaceTensorSetNumSubspaces_C", (PetscSpace, PetscInt), (sp, numTensSpaces));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceTensorGetNumSubspaces - Get the number of spaces in the tensor product space

  Input Parameter:
. sp  - the function space object

  Output Parameter:
. numTensSpaces - the number of spaces

  Level: intermediate

 Note:
  The name NumSubspaces is misleading because it is actually getting the number of defining spaces of the tensor product space, not a number of Subspaces of it

.seealso: `PETSCSPACETENSOR`, `PetscSpace`, `PetscSpaceTensorSetNumSubspaces()`, `PetscSpaceSetDegree()`, `PetscSpaceSetNumVariables()`
@*/
PetscErrorCode PetscSpaceTensorGetNumSubspaces(PetscSpace sp, PetscInt *numTensSpaces)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscValidIntPointer(numTensSpaces, 2);
  PetscTryMethod(sp, "PetscSpaceTensorGetNumSubspaces_C", (PetscSpace, PetscInt *), (sp, numTensSpaces));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceTensorSetSubspace - Set a space in the tensor product space

  Input Parameters:
+ sp    - the function space object
. s     - The space number
- subsp - the number of spaces

  Level: intermediate

  Note:
  The name SetSubspace is misleading because it is actually setting one of the defining spaces of the tensor product space, not a Subspace of it

.seealso: `PETSCSPACETENSOR`, `PetscSpace`, `PetscSpaceTensorGetSubspace()`, `PetscSpaceSetDegree()`, `PetscSpaceSetNumVariables()`
@*/
PetscErrorCode PetscSpaceTensorSetSubspace(PetscSpace sp, PetscInt s, PetscSpace subsp)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  if (subsp) PetscValidHeaderSpecific(subsp, PETSCSPACE_CLASSID, 3);
  PetscTryMethod(sp, "PetscSpaceTensorSetSubspace_C", (PetscSpace, PetscInt, PetscSpace), (sp, s, subsp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscSpaceTensorGetSubspace - Get a space in the tensor product space

  Input Parameters:
+ sp - the function space object
- s  - The space number

  Output Parameter:
. subsp - the `PetscSpace`

  Level: intermediate

  Note:
  The name GetSubspace is misleading because it is actually getting one of the defining spaces of the tensor product space, not a Subspace of it

.seealso: `PETSCSPACETENSOR`, `PetscSpace`, `PetscSpaceTensorSetSubspace()`, `PetscSpaceSetDegree()`, `PetscSpaceSetNumVariables()`
@*/
PetscErrorCode PetscSpaceTensorGetSubspace(PetscSpace sp, PetscInt s, PetscSpace *subsp)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscValidPointer(subsp, 3);
  PetscTryMethod(sp, "PetscSpaceTensorGetSubspace_C", (PetscSpace, PetscInt, PetscSpace *), (sp, s, subsp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceTensorSetNumSubspaces_Tensor(PetscSpace space, PetscInt numTensSpaces)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)space->data;
  PetscInt           Ns;

  PetscFunctionBegin;
  PetscCheck(!tens->setupCalled, PetscObjectComm((PetscObject)space), PETSC_ERR_ARG_WRONGSTATE, "Cannot change number of subspaces after setup called");
  Ns = tens->numTensSpaces;
  if (numTensSpaces == Ns) PetscFunctionReturn(PETSC_SUCCESS);
  if (Ns >= 0) {
    PetscInt s;

    for (s = 0; s < Ns; s++) PetscCall(PetscSpaceDestroy(&tens->tensspaces[s]));
    PetscCall(PetscFree(tens->tensspaces));
  }
  Ns = tens->numTensSpaces = numTensSpaces;
  PetscCall(PetscCalloc1(Ns, &tens->tensspaces));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceTensorGetNumSubspaces_Tensor(PetscSpace space, PetscInt *numTensSpaces)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)space->data;

  PetscFunctionBegin;
  *numTensSpaces = tens->numTensSpaces;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceTensorSetSubspace_Tensor(PetscSpace space, PetscInt s, PetscSpace subspace)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)space->data;
  PetscInt           Ns;

  PetscFunctionBegin;
  PetscCheck(!tens->setupCalled, PetscObjectComm((PetscObject)space), PETSC_ERR_ARG_WRONGSTATE, "Cannot change subspace after setup called");
  Ns = tens->numTensSpaces;
  PetscCheck(Ns >= 0, PetscObjectComm((PetscObject)space), PETSC_ERR_ARG_WRONGSTATE, "Must call PetscSpaceTensorSetNumSubspaces() first");
  PetscCheck(s >= 0 && s < Ns, PetscObjectComm((PetscObject)space), PETSC_ERR_ARG_OUTOFRANGE, "Invalid subspace number %" PetscInt_FMT, s);
  PetscCall(PetscObjectReference((PetscObject)subspace));
  PetscCall(PetscSpaceDestroy(&tens->tensspaces[s]));
  tens->tensspaces[s] = subspace;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceGetHeightSubspace_Tensor(PetscSpace sp, PetscInt height, PetscSpace *subsp)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)sp->data;
  PetscInt           Nc, dim, order, i;
  PetscSpace         bsp;

  PetscFunctionBegin;
  PetscCall(PetscSpaceGetNumVariables(sp, &dim));
  PetscCheck(tens->uniform && tens->numTensSpaces == dim, PETSC_COMM_SELF, PETSC_ERR_ARG_INCOMP, "Can only get a generic height subspace of a uniform tensor space of 1d spaces.");
  PetscCall(PetscSpaceGetNumComponents(sp, &Nc));
  PetscCall(PetscSpaceGetDegree(sp, &order, NULL));
  PetscCheck(height <= dim && height >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Asked for space at height %" PetscInt_FMT " for dimension %" PetscInt_FMT " space", height, dim);
  if (!tens->heightsubspaces) PetscCall(PetscCalloc1(dim, &tens->heightsubspaces));
  if (height <= dim) {
    if (!tens->heightsubspaces[height - 1]) {
      PetscSpace  sub;
      const char *name;

      PetscCall(PetscSpaceTensorGetSubspace(sp, 0, &bsp));
      PetscCall(PetscSpaceCreate(PetscObjectComm((PetscObject)sp), &sub));
      PetscCall(PetscObjectGetName((PetscObject)sp, &name));
      PetscCall(PetscObjectSetName((PetscObject)sub, name));
      PetscCall(PetscSpaceSetType(sub, PETSCSPACETENSOR));
      PetscCall(PetscSpaceSetNumComponents(sub, Nc));
      PetscCall(PetscSpaceSetDegree(sub, order, PETSC_DETERMINE));
      PetscCall(PetscSpaceSetNumVariables(sub, dim - height));
      PetscCall(PetscSpaceTensorSetNumSubspaces(sub, dim - height));
      for (i = 0; i < dim - height; i++) PetscCall(PetscSpaceTensorSetSubspace(sub, i, bsp));
      PetscCall(PetscSpaceSetUp(sub));
      tens->heightsubspaces[height - 1] = sub;
    }
    *subsp = tens->heightsubspaces[height - 1];
  } else {
    *subsp = NULL;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceTensorGetSubspace_Tensor(PetscSpace space, PetscInt s, PetscSpace *subspace)
{
  PetscSpace_Tensor *tens = (PetscSpace_Tensor *)space->data;
  PetscInt           Ns;

  PetscFunctionBegin;
  Ns = tens->numTensSpaces;
  PetscCheck(Ns >= 0, PetscObjectComm((PetscObject)space), PETSC_ERR_ARG_WRONGSTATE, "Must call PetscSpaceTensorSetNumSubspaces() first");
  PetscCheck(s >= 0 && s < Ns, PetscObjectComm((PetscObject)space), PETSC_ERR_ARG_OUTOFRANGE, "Invalid subspace number %" PetscInt_FMT, s);
  *subspace = tens->tensspaces[s];
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscSpaceInitialize_Tensor(PetscSpace sp)
{
  PetscFunctionBegin;
  sp->ops->setfromoptions    = PetscSpaceSetFromOptions_Tensor;
  sp->ops->setup             = PetscSpaceSetUp_Tensor;
  sp->ops->view              = PetscSpaceView_Tensor;
  sp->ops->destroy           = PetscSpaceDestroy_Tensor;
  sp->ops->getdimension      = PetscSpaceGetDimension_Tensor;
  sp->ops->evaluate          = PetscSpaceEvaluate_Tensor;
  sp->ops->getheightsubspace = PetscSpaceGetHeightSubspace_Tensor;
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscSpaceTensorGetNumSubspaces_C", PetscSpaceTensorGetNumSubspaces_Tensor));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscSpaceTensorSetNumSubspaces_C", PetscSpaceTensorSetNumSubspaces_Tensor));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscSpaceTensorGetSubspace_C", PetscSpaceTensorGetSubspace_Tensor));
  PetscCall(PetscObjectComposeFunction((PetscObject)sp, "PetscSpaceTensorSetSubspace_C", PetscSpaceTensorSetSubspace_Tensor));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
  PETSCSPACETENSOR = "tensor" - A `PetscSpace` object that encapsulates a tensor product space.
                     A tensor product is created of the components of the subspaces as well.

  Level: intermediate

.seealso: `PetscSpace`, `PetscSpaceType`, `PetscSpaceCreate()`, `PetscSpaceSetType()`, `PetscSpaceTensorGetSubspace()`, `PetscSpaceTensorSetSubspace()`,
          `PetscSpaceTensorGetNumSubspaces()`, `PetscSpaceTensorSetNumSubspaces()`
M*/

PETSC_EXTERN PetscErrorCode PetscSpaceCreate_Tensor(PetscSpace sp)
{
  PetscSpace_Tensor *tens;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sp, PETSCSPACE_CLASSID, 1);
  PetscCall(PetscNew(&tens));
  sp->data = tens;

  tens->numTensSpaces = PETSC_DEFAULT;

  PetscCall(PetscSpaceInitialize_Tensor(sp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#include <petsc/private/dmpleximpl.h>   /*I      "petscdmplex.h"   I*/
#include <petsc/private/matorderimpl.h> /*I      "petscmat.h"      I*/
#include <petsc/private/dmlabelimpl.h>

static PetscErrorCode DMPlexCreateOrderingClosure_Static(DM dm, PetscInt numPoints, const PetscInt pperm[], PetscInt **clperm, PetscInt **invclperm)
{
  PetscInt *perm, *iperm;
  PetscInt  depth, d, pStart, pEnd, fStart, fMax, fEnd, p;

  PetscFunctionBegin;
  PetscCall(DMPlexGetDepth(dm, &depth));
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  PetscCall(PetscMalloc1(pEnd - pStart, &perm));
  PetscCall(PetscMalloc1(pEnd - pStart, &iperm));
  for (p = pStart; p < pEnd; ++p) iperm[p] = -1;
  for (d = depth; d > 0; --d) {
    PetscCall(DMPlexGetDepthStratum(dm, d, &pStart, &pEnd));
    PetscCall(DMPlexGetDepthStratum(dm, d - 1, &fStart, &fEnd));
    fMax = fStart;
    for (p = pStart; p < pEnd; ++p) {
      const PetscInt *cone;
      PetscInt        point, coneSize, c;

      if (d == depth) {
        perm[p]         = pperm[p];
        iperm[pperm[p]] = p;
      }
      point = perm[p];
      PetscCall(DMPlexGetConeSize(dm, point, &coneSize));
      PetscCall(DMPlexGetCone(dm, point, &cone));
      for (c = 0; c < coneSize; ++c) {
        const PetscInt oldc = cone[c];
        const PetscInt newc = iperm[oldc];

        if (newc < 0) {
          perm[fMax]  = oldc;
          iperm[oldc] = fMax++;
        }
      }
    }
    PetscCheck(fMax == fEnd, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Number of depth %" PetscInt_FMT " faces %" PetscInt_FMT " does not match permuted number %" PetscInt_FMT, d, fEnd - fStart, fMax - fStart);
  }
  *clperm    = perm;
  *invclperm = iperm;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexGetOrdering - Calculate a reordering of the mesh

  Collective

  Input Parameters:
+ dm - The DMPlex object
. otype - type of reordering, see `MatOrderingType`
- label - [Optional] Label used to segregate ordering into sets, or `NULL`

  Output Parameter:
. perm - The point permutation as an `IS`, `perm`[old point number] = new point number

  Level: intermediate

  Note:
  The label is used to group sets of points together by label value. This makes it easy to reorder a mesh which
  has different types of cells, and then loop over each set of reordered cells for assembly.

.seealso: `DMPLEX`, `DMPlexPermute()`, `MatOrderingType`, `MatGetOrdering()`
@*/
PetscErrorCode DMPlexGetOrdering(DM dm, MatOrderingType otype, DMLabel label, IS *perm)
{
  PetscInt  numCells = 0;
  PetscInt *start = NULL, *adjacency = NULL, *cperm, *clperm = NULL, *invclperm = NULL, *mask, *xls, pStart, pEnd, c, i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(perm, 4);
  PetscCall(DMPlexCreateNeighborCSR(dm, 0, &numCells, &start, &adjacency));
  PetscCall(PetscMalloc3(numCells, &cperm, numCells, &mask, numCells * 2, &xls));
  if (numCells) {
    /* Shift for Fortran numbering */
    for (i = 0; i < start[numCells]; ++i) ++adjacency[i];
    for (i = 0; i <= numCells; ++i) ++start[i];
    PetscCall(SPARSEPACKgenrcm(&numCells, start, adjacency, cperm, mask, xls));
  }
  PetscCall(PetscFree(start));
  PetscCall(PetscFree(adjacency));
  /* Shift for Fortran numbering */
  for (c = 0; c < numCells; ++c) --cperm[c];
  /* Segregate */
  if (label) {
    IS              valueIS;
    const PetscInt *valuesTmp;
    PetscInt       *values;
    PetscInt        numValues, numPoints = 0;
    PetscInt       *sperm, *vsize, *voff, v;

    // Can't directly sort the valueIS, since it is a view into the DMLabel
    PetscCall(DMLabelGetValueIS(label, &valueIS));
    PetscCall(ISGetLocalSize(valueIS, &numValues));
    PetscCall(ISGetIndices(valueIS, &valuesTmp));
    PetscCall(PetscCalloc4(numCells, &sperm, numValues, &values, numValues, &vsize, numValues + 1, &voff));
    PetscCall(PetscArraycpy(values, valuesTmp, numValues));
    PetscCall(PetscSortInt(numValues, values));
    PetscCall(ISRestoreIndices(valueIS, &valuesTmp));
    PetscCall(ISDestroy(&valueIS));
    for (v = 0; v < numValues; ++v) {
      PetscCall(DMLabelGetStratumSize(label, values[v], &vsize[v]));
      if (v < numValues - 1) voff[v + 2] += vsize[v] + voff[v + 1];
      numPoints += vsize[v];
    }
    PetscCheck(numPoints == numCells, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Label only covers %" PetscInt_FMT " cells != %" PetscInt_FMT " total cells", numPoints, numCells);
    for (c = 0; c < numCells; ++c) {
      const PetscInt oldc = cperm[c];
      PetscInt       val, vloc;

      PetscCall(DMLabelGetValue(label, oldc, &val));
      PetscCheck(val != -1, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Cell %" PetscInt_FMT " not present in label", oldc);
      PetscCall(PetscFindInt(val, numValues, values, &vloc));
      PetscCheck(vloc >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Value %" PetscInt_FMT " not present label", val);
      sperm[voff[vloc + 1]++] = oldc;
    }
    for (v = 0; v < numValues; ++v) PetscCheck(voff[v + 1] - voff[v] == vsize[v], PETSC_COMM_SELF, PETSC_ERR_PLIB, "Number of %" PetscInt_FMT " values found is %" PetscInt_FMT " != %" PetscInt_FMT, values[v], voff[v + 1] - voff[v], vsize[v]);
    PetscCall(PetscArraycpy(cperm, sperm, numCells));
    PetscCall(PetscFree4(sperm, values, vsize, voff));
  }
  /* Construct closure */
  PetscCall(DMPlexCreateOrderingClosure_Static(dm, numCells, cperm, &clperm, &invclperm));
  PetscCall(PetscFree3(cperm, mask, xls));
  PetscCall(PetscFree(clperm));
  /* Invert permutation */
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)dm), pEnd - pStart, invclperm, PETSC_OWN_POINTER, perm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexGetOrdering1D - Reorder the vertices so that the mesh is in a line

  Collective

  Input Parameter:
. dm - The `DMPLEX` object

  Output Parameter:
. perm - The point permutation as an `IS`, `perm`[old point number] = new point number

  Level: intermediate

.seealso: `DMPLEX`, `DMPlexGetOrdering()`, `DMPlexPermute()`, `MatGetOrdering()`
@*/
PetscErrorCode DMPlexGetOrdering1D(DM dm, IS *perm)
{
  PetscInt       *points;
  const PetscInt *support, *cone;
  PetscInt        dim, pStart, pEnd, cStart, cEnd, c, vStart, vEnd, v, suppSize, lastCell = 0;

  PetscFunctionBegin;
  PetscCall(DMGetDimension(dm, &dim));
  PetscCheck(dim == 1, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONG, "Input mesh must be one dimensional, not %" PetscInt_FMT, dim);
  PetscCall(DMPlexGetChart(dm, &pStart, &pEnd));
  PetscCall(DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd));
  PetscCall(DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd));
  PetscCall(PetscMalloc1(pEnd - pStart, &points));
  for (c = cStart; c < cEnd; ++c) points[c] = c;
  for (v = vStart; v < vEnd; ++v) points[v] = v;
  for (v = vStart; v < vEnd; ++v) {
    PetscCall(DMPlexGetSupportSize(dm, v, &suppSize));
    PetscCall(DMPlexGetSupport(dm, v, &support));
    if (suppSize == 1) {
      lastCell = support[0];
      break;
    }
  }
  if (v < vEnd) {
    PetscInt pos = cEnd;

    points[v] = pos++;
    while (lastCell >= cStart) {
      PetscCall(DMPlexGetCone(dm, lastCell, &cone));
      if (cone[0] == v) v = cone[1];
      else v = cone[0];
      PetscCall(DMPlexGetSupport(dm, v, &support));
      PetscCall(DMPlexGetSupportSize(dm, v, &suppSize));
      if (suppSize == 1) {
        lastCell = -1;
      } else {
        if (support[0] == lastCell) lastCell = support[1];
        else lastCell = support[0];
      }
      points[v] = pos++;
    }
    PetscCheck(pos == pEnd, PetscObjectComm((PetscObject)dm), PETSC_ERR_ARG_WRONG, "Last vertex was %" PetscInt_FMT ", not %" PetscInt_FMT, pos, pEnd);
  }
  PetscCall(ISCreateGeneral(PetscObjectComm((PetscObject)dm), pEnd - pStart, points, PETSC_OWN_POINTER, perm));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexRemapCoordinates_Private(IS perm, PetscSection cs, Vec coordinates, PetscSection *csNew, Vec *coordinatesNew)
{
  PetscScalar    *coords, *coordsNew;
  const PetscInt *pperm;
  PetscInt        pStart, pEnd, p;
  const char     *name;

  PetscFunctionBegin;
  PetscCall(PetscSectionPermute(cs, perm, csNew));
  PetscCall(VecDuplicate(coordinates, coordinatesNew));
  PetscCall(PetscObjectGetName((PetscObject)coordinates, &name));
  PetscCall(PetscObjectSetName((PetscObject)*coordinatesNew, name));
  PetscCall(VecGetArray(coordinates, &coords));
  PetscCall(VecGetArray(*coordinatesNew, &coordsNew));
  PetscCall(PetscSectionGetChart(*csNew, &pStart, &pEnd));
  PetscCall(ISGetIndices(perm, &pperm));
  for (p = pStart; p < pEnd; ++p) {
    PetscInt dof, off, offNew, d;

    PetscCall(PetscSectionGetDof(*csNew, p, &dof));
    PetscCall(PetscSectionGetOffset(cs, p, &off));
    PetscCall(PetscSectionGetOffset(*csNew, pperm[p], &offNew));
    for (d = 0; d < dof; ++d) coordsNew[offNew + d] = coords[off + d];
  }
  PetscCall(ISRestoreIndices(perm, &pperm));
  PetscCall(VecRestoreArray(coordinates, &coords));
  PetscCall(VecRestoreArray(*coordinatesNew, &coordsNew));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexPermute - Reorder the mesh according to the input permutation

  Collective

  Input Parameters:
+ dm - The `DMPLEX` object
- perm - The point permutation, `perm`[old point number] = new point number

  Output Parameter:
. pdm - The permuted `DM`

  Level: intermediate

.seealso: `DMPLEX`, `MatPermute()`
@*/
PetscErrorCode DMPlexPermute(DM dm, IS perm, DM *pdm)
{
  DM_Plex    *plex = (DM_Plex *)dm->data, *plexNew;
  PetscInt    dim, cdim;
  const char *name;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidHeaderSpecific(perm, IS_CLASSID, 2);
  PetscValidPointer(pdm, 3);
  PetscCall(DMCreate(PetscObjectComm((PetscObject)dm), pdm));
  PetscCall(DMSetType(*pdm, DMPLEX));
  PetscCall(PetscObjectGetName((PetscObject)dm, &name));
  PetscCall(PetscObjectSetName((PetscObject)*pdm, name));
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMSetDimension(*pdm, dim));
  PetscCall(DMGetCoordinateDim(dm, &cdim));
  PetscCall(DMSetCoordinateDim(*pdm, cdim));
  PetscCall(DMCopyDisc(dm, *pdm));
  if (dm->localSection) {
    PetscSection section, sectionNew;

    PetscCall(DMGetLocalSection(dm, &section));
    PetscCall(PetscSectionPermute(section, perm, &sectionNew));
    PetscCall(DMSetLocalSection(*pdm, sectionNew));
    PetscCall(PetscSectionDestroy(&sectionNew));
  }
  plexNew = (DM_Plex *)(*pdm)->data;
  /* Ignore ltogmap, ltogmapb */
  /* Ignore sf, sectionSF */
  /* Ignore globalVertexNumbers, globalCellNumbers */
  /* Reorder labels */
  {
    PetscInt numLabels, l;
    DMLabel  label, labelNew;

    PetscCall(DMGetNumLabels(dm, &numLabels));
    for (l = 0; l < numLabels; ++l) {
      PetscCall(DMGetLabelByNum(dm, l, &label));
      PetscCall(DMLabelPermute(label, perm, &labelNew));
      PetscCall(DMAddLabel(*pdm, labelNew));
      PetscCall(DMLabelDestroy(&labelNew));
    }
    PetscCall(DMGetLabel(*pdm, "depth", &(*pdm)->depthLabel));
    if (plex->subpointMap) PetscCall(DMLabelPermute(plex->subpointMap, perm, &plexNew->subpointMap));
  }
  if ((*pdm)->celltypeLabel) {
    DMLabel ctLabel;

    // Reset label for fast lookup
    PetscCall(DMPlexGetCellTypeLabel(*pdm, &ctLabel));
    PetscCall(DMLabelMakeAllInvalid_Internal(ctLabel));
  }
  /* Reorder topology */
  {
    const PetscInt *pperm;
    PetscInt        n, pStart, pEnd, p;

    PetscCall(PetscSectionDestroy(&plexNew->coneSection));
    PetscCall(PetscSectionPermute(plex->coneSection, perm, &plexNew->coneSection));
    PetscCall(PetscSectionGetStorageSize(plexNew->coneSection, &n));
    PetscCall(PetscMalloc1(n, &plexNew->cones));
    PetscCall(PetscMalloc1(n, &plexNew->coneOrientations));
    PetscCall(ISGetIndices(perm, &pperm));
    PetscCall(PetscSectionGetChart(plex->coneSection, &pStart, &pEnd));
    for (p = pStart; p < pEnd; ++p) {
      PetscInt dof, off, offNew, d;

      PetscCall(PetscSectionGetDof(plexNew->coneSection, pperm[p], &dof));
      PetscCall(PetscSectionGetOffset(plex->coneSection, p, &off));
      PetscCall(PetscSectionGetOffset(plexNew->coneSection, pperm[p], &offNew));
      for (d = 0; d < dof; ++d) {
        plexNew->cones[offNew + d]            = pperm[plex->cones[off + d]];
        plexNew->coneOrientations[offNew + d] = plex->coneOrientations[off + d];
      }
    }
    PetscCall(PetscSectionDestroy(&plexNew->supportSection));
    PetscCall(PetscSectionPermute(plex->supportSection, perm, &plexNew->supportSection));
    PetscCall(PetscSectionGetStorageSize(plexNew->supportSection, &n));
    PetscCall(PetscMalloc1(n, &plexNew->supports));
    PetscCall(PetscSectionGetChart(plex->supportSection, &pStart, &pEnd));
    for (p = pStart; p < pEnd; ++p) {
      PetscInt dof, off, offNew, d;

      PetscCall(PetscSectionGetDof(plexNew->supportSection, pperm[p], &dof));
      PetscCall(PetscSectionGetOffset(plex->supportSection, p, &off));
      PetscCall(PetscSectionGetOffset(plexNew->supportSection, pperm[p], &offNew));
      for (d = 0; d < dof; ++d) plexNew->supports[offNew + d] = pperm[plex->supports[off + d]];
    }
    PetscCall(ISRestoreIndices(perm, &pperm));
  }
  /* Remap coordinates */
  {
    DM           cdm, cdmNew;
    PetscSection cs, csNew;
    Vec          coordinates, coordinatesNew;

    PetscCall(DMGetCoordinateSection(dm, &cs));
    PetscCall(DMGetCoordinatesLocal(dm, &coordinates));
    PetscCall(DMPlexRemapCoordinates_Private(perm, cs, coordinates, &csNew, &coordinatesNew));
    PetscCall(DMSetCoordinateSection(*pdm, PETSC_DETERMINE, csNew));
    PetscCall(DMSetCoordinatesLocal(*pdm, coordinatesNew));
    PetscCall(PetscSectionDestroy(&csNew));
    PetscCall(VecDestroy(&coordinatesNew));

    PetscCall(DMGetCellCoordinateDM(dm, &cdm));
    if (cdm) {
      PetscCall(DMGetCoordinateDM(*pdm, &cdm));
      PetscCall(DMClone(cdm, &cdmNew));
      PetscCall(DMSetCellCoordinateDM(*pdm, cdmNew));
      PetscCall(DMDestroy(&cdmNew));
      PetscCall(DMGetCellCoordinateSection(dm, &cs));
      PetscCall(DMGetCellCoordinatesLocal(dm, &coordinates));
      PetscCall(DMPlexRemapCoordinates_Private(perm, cs, coordinates, &csNew, &coordinatesNew));
      PetscCall(DMSetCellCoordinateSection(*pdm, PETSC_DETERMINE, csNew));
      PetscCall(DMSetCellCoordinatesLocal(*pdm, coordinatesNew));
      PetscCall(PetscSectionDestroy(&csNew));
      PetscCall(VecDestroy(&coordinatesNew));
    }
  }
  PetscCall(DMPlexCopy_Internal(dm, PETSC_TRUE, PETSC_TRUE, *pdm));
  (*pdm)->setupcalled = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexReorderSetDefault_Plex(DM dm, DMPlexReorderDefaultFlag reorder)
{
  DM_Plex *mesh = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  mesh->reorderDefault = reorder;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexReorderSetDefault - Set flag indicating whether the DM should be reordered by default

  Logically Collective

  Input Parameters:
+ dm        - The `DM`
- reorder   - Flag for reordering

  Level: intermediate

.seealso: `DMPlexReorderGetDefault()`
@*/
PetscErrorCode DMPlexReorderSetDefault(DM dm, DMPlexReorderDefaultFlag reorder)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscTryMethod(dm, "DMPlexReorderSetDefault_C", (DM, DMPlexReorderDefaultFlag), (dm, reorder));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode DMPlexReorderGetDefault_Plex(DM dm, DMPlexReorderDefaultFlag *reorder)
{
  DM_Plex *mesh = (DM_Plex *)dm->data;

  PetscFunctionBegin;
  *reorder = mesh->reorderDefault;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  DMPlexReorderGetDefault - Get flag indicating whether the DM should be reordered by default

  Not Collective

  Input Parameter:
. dm      - The `DM`

  Output Parameter:
. reorder - Flag for reordering

  Level: intermediate

.seealso: `DMPlexReorderSetDefault()`
@*/
PetscErrorCode DMPlexReorderGetDefault(DM dm, DMPlexReorderDefaultFlag *reorder)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(reorder, 2);
  PetscUseMethod(dm, "DMPlexReorderGetDefault_C", (DM, DMPlexReorderDefaultFlag *), (dm, reorder));
  PetscFunctionReturn(PETSC_SUCCESS);
}

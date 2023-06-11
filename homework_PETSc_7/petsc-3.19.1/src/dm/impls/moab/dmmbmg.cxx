#include <petsc/private/dmmbimpl.h>
#include <petscdmmoab.h>
#include <MBTagConventions.hpp>
#include <moab/NestedRefine.hpp>

/*@C
  DMMoabGenerateHierarchy - Generate a multi-level uniform refinement hierarchy
  by succesively refining a coarse mesh, already defined in the `DM` object
  provided by the user.

  Collective

  Input Parameter:
. dmb  - The `DMMOAB` object

  Output Parameters:
+ nlevels   - The number of levels of refinement needed to generate the hierarchy
- ldegrees  - The degree of refinement at each level in the hierarchy

  Level: beginner

@*/
PetscErrorCode DMMoabGenerateHierarchy(DM dm, PetscInt nlevels, PetscInt *ldegrees)
{
  DM_Moab                        *dmmoab;
  moab::ErrorCode                 merr;
  PetscInt                       *pdegrees, ilevel;
  std::vector<moab::EntityHandle> hsets;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  dmmoab = (DM_Moab *)(dm)->data;

  if (!ldegrees) {
    PetscCall(PetscMalloc1(nlevels, &pdegrees));
    for (ilevel = 0; ilevel < nlevels; ilevel++) pdegrees[ilevel] = 2; /* default = Degree 2 refinement */
  } else pdegrees = ldegrees;

  /* initialize set level refinement data for hierarchy */
  dmmoab->nhlevels = nlevels;

  /* Instantiate the nested refinement class */
#ifdef MOAB_HAVE_MPI
  dmmoab->hierarchy = new moab::NestedRefine(dynamic_cast<moab::Core *>(dmmoab->mbiface), dmmoab->pcomm, dmmoab->fileset);
#else
  dmmoab->hierarchy = new moab::NestedRefine(dynamic_cast<moab::Core *>(dmmoab->mbiface), NULL, dmmoab->fileset);
#endif

  PetscCall(PetscMalloc1(nlevels + 1, &dmmoab->hsets));

  /* generate the mesh hierarchy */
  merr = dmmoab->hierarchy->generate_mesh_hierarchy(nlevels, pdegrees, hsets, false);
  MBERRNM(merr);

#ifdef MOAB_HAVE_MPI
  if (dmmoab->pcomm->size() > 1) {
    merr = dmmoab->hierarchy->exchange_ghosts(hsets, dmmoab->nghostrings);
    MBERRNM(merr);
  }
#endif

  /* copy the mesh sets for nested refinement hierarchy */
  dmmoab->hsets[0] = hsets[0];
  for (ilevel = 1; ilevel <= nlevels; ilevel++) {
    dmmoab->hsets[ilevel] = hsets[ilevel];

#ifdef MOAB_HAVE_MPI
    merr = dmmoab->pcomm->assign_global_ids(hsets[ilevel], dmmoab->dim, 0, false, true, false);
    MBERRNM(merr);
#endif

    /* Update material and other geometric tags from parent to child sets */
    merr = dmmoab->hierarchy->update_special_tags(ilevel, hsets[ilevel]);
    MBERRNM(merr);
  }

  hsets.clear();
  if (!ldegrees) PetscCall(PetscFree(pdegrees));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMRefineHierarchy_Moab - Generate a multi-level `DM` hierarchy
  by succesively refining a coarse mesh.

  Collective

  Input Parameter:
. dm  - The `DMMOAB` object

  Output Parameters:
+ nlevels   - The number of levels of refinement needed to generate the hierarchy
- dmf  - The DM objects after successive refinement of the hierarchy

  Level: beginner

@*/
PETSC_EXTERN PetscErrorCode DMRefineHierarchy_Moab(DM dm, PetscInt nlevels, DM dmf[])
{
  PetscInt i;

  PetscFunctionBegin;

  PetscCall(DMRefine(dm, PetscObjectComm((PetscObject)dm), &dmf[0]));
  for (i = 1; i < nlevels; i++) PetscCall(DMRefine(dmf[i - 1], PetscObjectComm((PetscObject)dm), &dmf[i]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMCoarsenHierarchy_Moab - Generate a multi-level `DM` hierarchy
  by succesively coarsening a refined mesh.

  Collective

  Input Parameter:
. dm  - The `DMMOAB` object

  Output Parameters:
+ nlevels   - The number of levels of refinement needed to generate the hierarchy
- dmc  - The `DM` objects after successive coarsening of the hierarchy

  Level: beginner

@*/
PETSC_EXTERN PetscErrorCode DMCoarsenHierarchy_Moab(DM dm, PetscInt nlevels, DM dmc[])
{
  PetscInt i;

  PetscFunctionBegin;

  PetscCall(DMCoarsen(dm, PetscObjectComm((PetscObject)dm), &dmc[0]));
  for (i = 1; i < nlevels; i++) PetscCall(DMCoarsen(dmc[i - 1], PetscObjectComm((PetscObject)dm), &dmc[i]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PETSC_EXTERN PetscErrorCode DMMoab_Compute_NNZ_From_Connectivity(DM, PetscInt *, PetscInt *, PetscInt *, PetscInt *, PetscBool);

/*@C
  DMCreateInterpolation_Moab - Generate the interpolation operators to transform
  operators (matrices, vectors) from parent level to child level as defined by
  the `DM` inputs provided by the user.

  Collective

  Input Parameters:
+ dm1  - The `DMMOAB` object
- dm2  - the second, finer `DMMOAB` object

  Output Parameters:
+ interpl  - The interpolation operator for transferring data between the levels
- vec      - The scaling vector (optional)

  Level: developer

@*/
PETSC_EXTERN PetscErrorCode DMCreateInterpolation_Moab(DM dmp, DM dmc, Mat *interpl, Vec *vec)
{
  DM_Moab        *dmbp, *dmbc;
  moab::ErrorCode merr;
  PetscInt        dim;
  PetscReal       factor;
  PetscInt        innz, *nnz, ionz, *onz;
  PetscInt        nlsizp, nlsizc, nlghsizp, ngsizp, ngsizc;
  const PetscBool use_consistent_bases = PETSC_TRUE;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dmp, DM_CLASSID, 1);
  PetscValidHeaderSpecific(dmc, DM_CLASSID, 2);
  dmbp     = (DM_Moab *)(dmp)->data;
  dmbc     = (DM_Moab *)(dmc)->data;
  nlsizp   = dmbp->nloc;                  // *dmb1->numFields;
  nlsizc   = dmbc->nloc;                  // *dmb2->numFields;
  ngsizp   = dmbp->n;                     // *dmb1->numFields;
  ngsizc   = dmbc->n;                     // *dmb2->numFields;
  nlghsizp = (dmbp->nloc + dmbp->nghost); // *dmb1->numFields;

  // Columns = Parent DoFs ;  Rows = Child DoFs
  // Interpolation matrix: \sum_{i=1}^P Owned(Child) * (Owned(Parent) + Ghosted(Parent))
  // Size: nlsizc * nlghsizp
  PetscCall(PetscInfo(NULL, "Creating interpolation matrix %" PetscInt_FMT " X %" PetscInt_FMT " to apply transformation between levels %" PetscInt_FMT " -> %" PetscInt_FMT ".\n", ngsizc, nlghsizp, dmbp->hlevel, dmbc->hlevel));

  PetscCall(DMGetDimension(dmp, &dim));

  /* allocate the nnz, onz arrays based on block size and local nodes */
  PetscCall(PetscCalloc2(nlsizc, &nnz, nlsizc, &onz));

  /* Loop through the local elements and compute the relation between the current parent and the refined_level. */
  for (moab::Range::iterator iter = dmbc->vowned->begin(); iter != dmbc->vowned->end(); iter++) {
    const moab::EntityHandle vhandle = *iter;
    /* define local variables */
    moab::EntityHandle              parent;
    std::vector<moab::EntityHandle> adjs;
    moab::Range                     found;

    /* store the vertex DoF number */
    const int ldof = dmbc->lidmap[vhandle - dmbc->seqstart];

    /* Get adjacency information for current vertex - i.e., all elements of dimension (dim) that connects
       to the current vertex. We can then decipher if a vertex is ghosted or not and compute the
       non-zero pattern accordingly. */
    merr = dmbc->hierarchy->get_adjacencies(vhandle, dmbc->dim, adjs);
    MBERRNM(merr);

    /* loop over vertices and update the number of connectivity */
    for (unsigned jter = 0; jter < adjs.size(); jter++) {
      const moab::EntityHandle jhandle = adjs[jter];

      /* Get the relation between the current (coarse) parent and its corresponding (finer) children elements */
      merr = dmbc->hierarchy->child_to_parent(jhandle, dmbc->hlevel, dmbp->hlevel, &parent);
      MBERRNM(merr);

      /* Get connectivity information in canonical ordering for the local element */
      std::vector<moab::EntityHandle> connp;
      merr = dmbp->hierarchy->get_connectivity(parent, dmbp->hlevel, connp);
      MBERRNM(merr);

      for (unsigned ic = 0; ic < connp.size(); ++ic) {
        /* loop over each element connected to the adjacent vertex and update as needed */
        /* find the truly user-expected layer of ghosted entities to decipher NNZ pattern */
        if (found.find(connp[ic]) != found.end()) continue;                    /* make sure we don't double count shared vertices */
        if (dmbp->vghost->find(connp[ic]) != dmbp->vghost->end()) onz[ldof]++; /* update out-of-proc onz */
        else nnz[ldof]++;                                                      /* else local vertex */
        found.insert(connp[ic]);
      }
    }
  }

  for (int i = 0; i < nlsizc; i++) nnz[i] += 1; /* self count the node */

  ionz = onz[0];
  innz = nnz[0];
  for (int tc = 0; tc < nlsizc; tc++) {
    // check for maximum allowed sparsity = fully dense
    nnz[tc] = std::min(nlsizp, nnz[tc]);
    onz[tc] = std::min(ngsizp - nlsizp, onz[tc]);

    PetscCall(PetscInfo(NULL, "  %d: NNZ = %d, ONZ = %d\n", tc, nnz[tc], onz[tc]));

    innz = (innz < nnz[tc] ? nnz[tc] : innz);
    ionz = (ionz < onz[tc] ? onz[tc] : ionz);
  }

  /* create interpolation matrix */
  PetscCall(MatCreate(PetscObjectComm((PetscObject)dmc), interpl));
  PetscCall(MatSetSizes(*interpl, nlsizc, nlsizp, ngsizc, ngsizp));
  PetscCall(MatSetType(*interpl, MATAIJ));
  PetscCall(MatSetFromOptions(*interpl));

  PetscCall(MatSeqAIJSetPreallocation(*interpl, innz, nnz));
  PetscCall(MatMPIAIJSetPreallocation(*interpl, innz, nnz, ionz, onz));

  /* clean up temporary memory */
  PetscCall(PetscFree2(nnz, onz));

  /* set up internal matrix data-structures */
  PetscCall(MatSetUp(*interpl));

  /* Define variables for assembly */
  std::vector<moab::EntityHandle> children;
  std::vector<moab::EntityHandle> connp, connc;
  std::vector<PetscReal>          pcoords, ccoords, values_phi;

  if (use_consistent_bases) {
    const moab::EntityHandle ehandle = dmbp->elocal->front();

    merr = dmbp->hierarchy->parent_to_child(ehandle, dmbp->hlevel, dmbc->hlevel, children);
    MBERRNM(merr);

    /* Get connectivity and coordinates of the parent vertices */
    merr = dmbp->hierarchy->get_connectivity(ehandle, dmbp->hlevel, connp);
    MBERRNM(merr);
    merr = dmbc->mbiface->get_connectivity(&children[0], children.size(), connc);
    MBERRNM(merr);

    std::vector<PetscReal> natparam(3 * connc.size(), 0.0);
    pcoords.resize(connp.size() * 3);
    ccoords.resize(connc.size() * 3);
    values_phi.resize(connp.size() * connc.size());
    /* Get coordinates for connectivity entities in canonical order for both coarse and finer levels */
    merr = dmbp->hierarchy->get_coordinates(&connp[0], connp.size(), dmbp->hlevel, &pcoords[0]);
    MBERRNM(merr);
    merr = dmbc->hierarchy->get_coordinates(&connc[0], connc.size(), dmbc->hlevel, &ccoords[0]);
    MBERRNM(merr);

    /* Set values: For each DOF in coarse grid cell, set the contribution or PHI evaluated at each fine grid DOF point */
    for (unsigned tc = 0; tc < connc.size(); tc++) {
      const PetscInt offset = tc * 3;

      /* Scale ccoords relative to pcoords */
      PetscCall(DMMoabPToRMapping(dim, connp.size(), &pcoords[0], &ccoords[offset], &natparam[offset], &values_phi[connp.size() * tc]));
    }
  } else {
    factor = std::pow(2.0 /*degree_P_for_refinement*/, (dmbc->hlevel - dmbp->hlevel) * dmbp->dim * 1.0);
  }

  /* TODO: Decipher the correct non-zero pattern. There is still some issue with onz allocation */
  PetscCall(MatSetOption(*interpl, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_FALSE));

  /* Loop through the remaining vertices. These vertices appear only on the current refined_level. */
  for (moab::Range::iterator iter = dmbp->elocal->begin(); iter != dmbp->elocal->end(); iter++) {
    const moab::EntityHandle ehandle = *iter;

    /* Get the relation between the current (coarse) parent and its corresponding (finer) children elements */
    children.clear();
    connc.clear();
    merr = dmbp->hierarchy->parent_to_child(ehandle, dmbp->hlevel, dmbc->hlevel, children);
    MBERRNM(merr);

    /* Get connectivity and coordinates of the parent vertices */
    merr = dmbp->hierarchy->get_connectivity(ehandle, dmbp->hlevel, connp);
    MBERRNM(merr);
    merr = dmbc->mbiface->get_connectivity(&children[0], children.size(), connc);
    MBERRNM(merr);

    pcoords.resize(connp.size() * 3);
    ccoords.resize(connc.size() * 3);
    /* Get coordinates for connectivity entities in canonical order for both coarse and finer levels */
    merr = dmbp->hierarchy->get_coordinates(&connp[0], connp.size(), dmbp->hlevel, &pcoords[0]);
    MBERRNM(merr);
    merr = dmbc->hierarchy->get_coordinates(&connc[0], connc.size(), dmbc->hlevel, &ccoords[0]);
    MBERRNM(merr);

    std::vector<int> dofsp(connp.size()), dofsc(connc.size());
    /* TODO: specific to scalar system - use GetDofs */
    PetscCall(DMMoabGetDofsBlocked(dmp, connp.size(), &connp[0], &dofsp[0]));
    PetscCall(DMMoabGetDofsBlocked(dmc, connc.size(), &connc[0], &dofsc[0]));

    /* Compute the actual interpolation weights when projecting solution/residual between levels */
    if (use_consistent_bases) {
      /* Use the cached values of natural parameteric coordinates and basis pre-evaluated.
         We are making an assumption here that UMR used in GMG to generate the hierarchy uses
         the same template for all elements; This will fail for mixed element meshes (TRI/QUAD).

         TODO: Fix the above assumption by caching data for families (especially for Tets and mixed meshes)
      */

      /* Set values: For each DOF in coarse grid cell, set the contribution or PHI evaluated at each fine grid DOF point */
      for (unsigned tc = 0; tc < connc.size(); tc++) {
        /* TODO: Check if we should be using INSERT_VALUES instead */
        PetscCall(MatSetValues(*interpl, 1, &dofsc[tc], connp.size(), &dofsp[0], &values_phi[connp.size() * tc], ADD_VALUES));
      }
    } else {
      /* Compute the interpolation weights by determining distance of 1-ring
         neighbor vertices from current vertex

         This should be used only when FEM basis is not used for the discretization.
         Else, the consistent interface to compute the basis function for interpolation
         between the levels should be evaluated correctly to preserve convergence of GMG.
         Shephard's basis will be terrible for any unsmooth problems.
      */
      values_phi.resize(connp.size());
      for (unsigned tc = 0; tc < connc.size(); tc++) {
        PetscReal normsum = 0.0;
        for (unsigned tp = 0; tp < connp.size(); tp++) {
          values_phi[tp] = 0.0;
          for (unsigned k = 0; k < 3; k++) values_phi[tp] += std::pow(pcoords[tp * 3 + k] - ccoords[k + tc * 3], dim);
          if (values_phi[tp] < 1e-12) {
            values_phi[tp] = 1e12;
          } else {
            //values_phi[tp] = std::pow(values_phi[tp], -1.0/dim);
            values_phi[tp] = std::pow(values_phi[tp], -1.0);
            normsum += values_phi[tp];
          }
        }
        for (unsigned tp = 0; tp < connp.size(); tp++) {
          if (values_phi[tp] > 1e11) values_phi[tp] = factor * 0.5 / connp.size();
          else values_phi[tp] = factor * values_phi[tp] * 0.5 / (connp.size() * normsum);
        }
        PetscCall(MatSetValues(*interpl, 1, &dofsc[tc], connp.size(), &dofsp[0], &values_phi[0], ADD_VALUES));
      }
    }
  }
  if (vec) *vec = NULL;
  PetscCall(MatAssemblyBegin(*interpl, MAT_FINAL_ASSEMBLY));
  PetscCall(MatAssemblyEnd(*interpl, MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMCreateInjection_Moab - Generate a multi-level uniform refinement hierarchy
  by succesively refining a coarse mesh, already defined in the `DM` object
  provided by the user.

  Collective

  Input Parameter:
. dmb  - The `DMMOAB` object

  Output Parameters:
+ nlevels   - The number of levels of refinement needed to generate the hierarchy
- ldegrees  - The degree of refinement at each level in the hierarchy

  Level: beginner

@*/
PETSC_EXTERN PetscErrorCode DMCreateInjection_Moab(DM dm1, DM dm2, VecScatter *ctx)
{
  //DM_Moab        *dmmoab;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm1, DM_CLASSID, 1);
  PetscValidHeaderSpecific(dm2, DM_CLASSID, 2);
  //dmmoab = (DM_Moab*)(dm1)->data;

  PetscCall(PetscPrintf(PETSC_COMM_WORLD, "[DMCreateInjection_Moab] :: Placeholder\n"));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMMoab_UMR_Private(DM dm, MPI_Comm comm, PetscBool refine, DM *dmref)
{
  PetscInt        i, dim;
  DM              dm2;
  moab::ErrorCode merr;
  DM_Moab        *dmb = (DM_Moab *)dm->data, *dd2;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(dmref, 4);

  if ((dmb->hlevel == dmb->nhlevels && refine) || (dmb->hlevel == 0 && !refine)) {
    if (dmb->hlevel + 1 > dmb->nhlevels && refine) {
      PetscCall(PetscInfo(NULL, "Invalid multigrid refinement hierarchy level specified (%" PetscInt_FMT "). MOAB UMR max levels = %" PetscInt_FMT ". Creating a NULL object.\n", dmb->hlevel + 1, dmb->nhlevels));
    }
    if (dmb->hlevel - 1 < 0 && !refine) PetscCall(PetscInfo(NULL, "Invalid multigrid coarsen hierarchy level specified (%" PetscInt_FMT "). Creating a NULL object.\n", dmb->hlevel - 1));
    *dmref = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  PetscCall(DMMoabCreate(PetscObjectComm((PetscObject)dm), &dm2));
  dd2 = (DM_Moab *)dm2->data;

  dd2->mbiface = dmb->mbiface;
#ifdef MOAB_HAVE_MPI
  dd2->pcomm = dmb->pcomm;
#endif
  dd2->icreatedinstance = PETSC_FALSE;
  dd2->nghostrings      = dmb->nghostrings;

  /* set the new level based on refinement/coarsening */
  if (refine) {
    dd2->hlevel = dmb->hlevel + 1;
  } else {
    dd2->hlevel = dmb->hlevel - 1;
  }

  /* Copy the multilevel hierarchy pointers in MOAB */
  dd2->hierarchy = dmb->hierarchy;
  dd2->nhlevels  = dmb->nhlevels;
  PetscCall(PetscMalloc1(dd2->nhlevels + 1, &dd2->hsets));
  for (i = 0; i <= dd2->nhlevels; i++) dd2->hsets[i] = dmb->hsets[i];
  dd2->fileset = dd2->hsets[dd2->hlevel];

  /* do the remaining initializations for DMMoab */
  dd2->bs                = dmb->bs;
  dd2->numFields         = dmb->numFields;
  dd2->rw_dbglevel       = dmb->rw_dbglevel;
  dd2->partition_by_rank = dmb->partition_by_rank;
  PetscCall(PetscStrncpy(dd2->extra_read_options, dmb->extra_read_options, sizeof(dd2->extra_read_options)));
  PetscCall(PetscStrncpy(dd2->extra_write_options, dmb->extra_write_options, sizeof(dd2->extra_write_options)));
  dd2->read_mode  = dmb->read_mode;
  dd2->write_mode = dmb->write_mode;

  /* set global ID tag handle */
  PetscCall(DMMoabSetLocalToGlobalTag(dm2, dmb->ltog_tag));

  merr = dd2->mbiface->tag_get_handle(MATERIAL_SET_TAG_NAME, dd2->material_tag);
  MBERRNM(merr);

  PetscCall(DMSetOptionsPrefix(dm2, ((PetscObject)dm)->prefix));
  PetscCall(DMGetDimension(dm, &dim));
  PetscCall(DMSetDimension(dm2, dim));

  /* allow overloaded (user replaced) operations to be inherited by refinement clones */
  dm2->ops->creatematrix = dm->ops->creatematrix;

  /* copy fill information if given */
  PetscCall(DMMoabSetBlockFills(dm2, dmb->dfill, dmb->ofill));

  /* copy vector type information */
  PetscCall(DMSetMatType(dm2, dm->mattype));
  PetscCall(DMSetVecType(dm2, dm->vectype));
  dd2->numFields = dmb->numFields;
  if (dmb->numFields) PetscCall(DMMoabSetFieldNames(dm2, dmb->numFields, dmb->fieldNames));

  PetscCall(DMSetFromOptions(dm2));

  /* recreate Dof numbering for the refined DM and make sure the distribution is correctly populated */
  PetscCall(DMSetUp(dm2));

  *dmref = dm2;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMRefine_Moab - Generate a multi-level uniform refinement hierarchy
  by succesively refining a coarse mesh, already defined in the `DM` object
  provided by the user.

  Collective

  Input Parameters:
+ dm  - The `DMMOAB` object
- comm - the communicator to contain the new DM object (or `MPI_COMM_NULL`)

  Output Parameter:
. dmf - the refined `DM`, or `NULL`

  Level: developer

  Note:
  If no refinement was done, the return value is `NULL`

@*/
PETSC_EXTERN PetscErrorCode DMRefine_Moab(DM dm, MPI_Comm comm, DM *dmf)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);

  PetscCall(DMMoab_UMR_Private(dm, comm, PETSC_TRUE, dmf));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  DMCoarsen_Moab - Generate a multi-level uniform refinement hierarchy
  by succesively refining a coarse mesh, already defined in the `DM` object
  provided by the user.

  Collective

  Input Parameters:
+ dm  - The `DMMOAB` object
- comm - the communicator to contain the new `DM` object (or `MPI_COMM_NULL`)

  Output Parameter:
. dmf - the coarsened `DM`, or `NULL`

  Level: developer

  Note:
  If no coarsening was done, the return value is `NULL`

@*/
PETSC_EXTERN PetscErrorCode DMCoarsen_Moab(DM dm, MPI_Comm comm, DM *dmc)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscCall(DMMoab_UMR_Private(dm, comm, PETSC_FALSE, dmc));
  PetscFunctionReturn(PETSC_SUCCESS);
}

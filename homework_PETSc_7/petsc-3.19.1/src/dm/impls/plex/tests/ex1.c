static char help[] = "Tests various DMPlex routines to construct, refine and distribute a mesh.\n\n";

#include <petscdmplex.h>
#include <petscdmplextransform.h>
#include <petscsf.h>

enum {
  STAGE_LOAD,
  STAGE_DISTRIBUTE,
  STAGE_REFINE,
  STAGE_OVERLAP
};

typedef struct {
  PetscLogEvent createMeshEvent;
  PetscLogStage stages[4];
  /* Domain and mesh definition */
  PetscInt  dim;     /* The topological mesh dimension */
  PetscInt  overlap; /* The cell overlap to use during partitioning */
  PetscBool testp4est[2];
  PetscBool redistribute;
  PetscBool final_ref;         /* Run refinement at the end */
  PetscBool final_diagnostics; /* Run diagnostics on the final mesh */
} AppCtx;

PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscFunctionBegin;
  options->dim               = 2;
  options->overlap           = 0;
  options->testp4est[0]      = PETSC_FALSE;
  options->testp4est[1]      = PETSC_FALSE;
  options->redistribute      = PETSC_FALSE;
  options->final_ref         = PETSC_FALSE;
  options->final_diagnostics = PETSC_TRUE;

  PetscOptionsBegin(comm, "", "Meshing Problem Options", "DMPLEX");
  PetscCall(PetscOptionsRangeInt("-dim", "The topological mesh dimension", "ex1.c", options->dim, &options->dim, NULL, 1, 3));
  PetscCall(PetscOptionsBoundedInt("-overlap", "The cell overlap for partitioning", "ex1.c", options->overlap, &options->overlap, NULL, 0));
  PetscCall(PetscOptionsBool("-test_p4est_seq", "Test p4est with sequential base DM", "ex1.c", options->testp4est[0], &options->testp4est[0], NULL));
  PetscCall(PetscOptionsBool("-test_p4est_par", "Test p4est with parallel base DM", "ex1.c", options->testp4est[1], &options->testp4est[1], NULL));
  PetscCall(PetscOptionsBool("-test_redistribute", "Test redistribution", "ex1.c", options->redistribute, &options->redistribute, NULL));
  PetscCall(PetscOptionsBool("-final_ref", "Run uniform refinement on the final mesh", "ex1.c", options->final_ref, &options->final_ref, NULL));
  PetscCall(PetscOptionsBool("-final_diagnostics", "Run diagnostics on the final mesh", "ex1.c", options->final_diagnostics, &options->final_diagnostics, NULL));
  PetscOptionsEnd();

  PetscCall(PetscLogEventRegister("CreateMesh", DM_CLASSID, &options->createMeshEvent));
  PetscCall(PetscLogStageRegister("MeshLoad", &options->stages[STAGE_LOAD]));
  PetscCall(PetscLogStageRegister("MeshDistribute", &options->stages[STAGE_DISTRIBUTE]));
  PetscCall(PetscLogStageRegister("MeshRefine", &options->stages[STAGE_REFINE]));
  PetscCall(PetscLogStageRegister("MeshOverlap", &options->stages[STAGE_OVERLAP]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode CreateMesh(MPI_Comm comm, AppCtx *user, DM *dm)
{
  PetscInt    dim           = user->dim;
  PetscBool   testp4est_seq = user->testp4est[0];
  PetscBool   testp4est_par = user->testp4est[1];
  PetscMPIInt rank, size;

  PetscFunctionBegin;
  PetscCall(PetscLogEventBegin(user->createMeshEvent, 0, 0, 0, 0));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCall(PetscLogStagePush(user->stages[STAGE_LOAD]));
  PetscCall(DMCreate(comm, dm));
  PetscCall(DMSetType(*dm, DMPLEX));
  PetscCall(DMPlexDistributeSetDefault(*dm, PETSC_FALSE));
  PetscCall(DMSetFromOptions(*dm));
  PetscCall(DMLocalizeCoordinates(*dm));

  PetscCall(DMViewFromOptions(*dm, NULL, "-init_dm_view"));
  PetscCall(DMGetDimension(*dm, &dim));

  if (testp4est_seq) {
    PetscCheck(PetscDefined(HAVE_P4EST), PETSC_COMM_WORLD, PETSC_ERR_SUP, "Reconfigure PETSc with --download-p4est");
    DM dmConv = NULL;

    PetscCall(DMPlexCheck(*dm));
    PetscCall(DMPlexSetRefinementUniform(*dm, PETSC_TRUE));
    PetscCall(DMPlexSetTransformType(*dm, DMPLEXREFINETOBOX));
    PetscCall(DMRefine(*dm, PETSC_COMM_WORLD, &dmConv));
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, NULL));
    if (dmConv) {
      PetscCall(DMDestroy(dm));
      *dm = dmConv;
    }
    PetscCall(DMViewFromOptions(*dm, NULL, "-initref_dm_view"));
    PetscCall(DMPlexCheck(*dm));

    /* For topologically periodic meshes, we first localize coordinates,
       and then remove any information related with the
       automatic computation of localized vertices.
       This way, refinement operations and conversions to p4est
       will preserve the shape of the domain in physical space */
    PetscCall(DMSetPeriodicity(*dm, NULL, NULL, NULL));

    PetscCall(DMConvert(*dm, dim == 2 ? DMP4EST : DMP8EST, &dmConv));
    if (dmConv) {
      PetscCall(PetscObjectSetOptionsPrefix((PetscObject)dmConv, "conv_seq_1_"));
      PetscCall(DMSetFromOptions(dmConv));
      PetscCall(DMDestroy(dm));
      *dm = dmConv;
    }
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, "conv_seq_1_"));
    PetscCall(DMSetUp(*dm));
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
    PetscCall(DMConvert(*dm, DMPLEX, &dmConv));
    if (dmConv) {
      PetscCall(PetscObjectSetOptionsPrefix((PetscObject)dmConv, "conv_seq_2_"));
      PetscCall(DMPlexDistributeSetDefault(dmConv, PETSC_FALSE));
      PetscCall(DMSetFromOptions(dmConv));
      PetscCall(DMDestroy(dm));
      *dm = dmConv;
    }
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, "conv_seq_2_"));
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, NULL));
  }

  PetscCall(PetscLogStagePop());
  if (!testp4est_seq) {
    PetscCall(PetscLogStagePush(user->stages[STAGE_DISTRIBUTE]));
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_pre_dist_view"));
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, "dist_"));
    PetscCall(DMSetFromOptions(*dm));
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, NULL));
    PetscCall(PetscLogStagePop());
    PetscCall(DMViewFromOptions(*dm, NULL, "-distributed_dm_view"));
  }
  PetscCall(PetscLogStagePush(user->stages[STAGE_REFINE]));
  PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, "ref_"));
  PetscCall(DMSetFromOptions(*dm));
  PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, NULL));
  PetscCall(PetscLogStagePop());

  if (testp4est_par) {
    PetscCheck(PetscDefined(HAVE_P4EST), PETSC_COMM_WORLD, PETSC_ERR_SUP, "Reconfigure PETSc with --download-p4est");
    DM dmConv = NULL;

    PetscCall(DMPlexCheck(*dm));
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_tobox_view"));
    PetscCall(DMPlexSetRefinementUniform(*dm, PETSC_TRUE));
    PetscCall(DMPlexSetTransformType(*dm, DMPLEXREFINETOBOX));
    PetscCall(DMRefine(*dm, PETSC_COMM_WORLD, &dmConv));
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, NULL));
    if (dmConv) {
      PetscCall(DMDestroy(dm));
      *dm = dmConv;
    }
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_tobox_view"));
    PetscCall(DMPlexCheck(*dm));

    PetscCall(DMConvert(*dm, dim == 2 ? DMP4EST : DMP8EST, &dmConv));
    if (dmConv) {
      PetscCall(PetscObjectSetOptionsPrefix((PetscObject)dmConv, "conv_par_1_"));
      PetscCall(DMSetFromOptions(dmConv));
      PetscCall(DMDestroy(dm));
      *dm = dmConv;
    }
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, "conv_par_1_"));
    PetscCall(DMSetUp(*dm));
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
    PetscCall(DMConvert(*dm, DMPLEX, &dmConv));
    if (dmConv) {
      PetscCall(PetscObjectSetOptionsPrefix((PetscObject)dmConv, "conv_par_2_"));
      PetscCall(DMPlexDistributeSetDefault(dmConv, PETSC_FALSE));
      PetscCall(DMSetFromOptions(dmConv));
      PetscCall(DMDestroy(dm));
      *dm = dmConv;
    }
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, "conv_par_2_"));
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
    PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*dm, NULL));
  }

  /* test redistribution of an already distributed mesh */
  if (user->redistribute) {
    DM       distributedMesh;
    PetscSF  sf;
    PetscInt nranks;

    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_pre_redist_view"));
    PetscCall(DMPlexDistribute(*dm, 0, NULL, &distributedMesh));
    if (distributedMesh) {
      PetscCall(DMGetPointSF(distributedMesh, &sf));
      PetscCall(PetscSFSetUp(sf));
      PetscCall(DMGetNeighbors(distributedMesh, &nranks, NULL));
      PetscCall(MPIU_Allreduce(MPI_IN_PLACE, &nranks, 1, MPIU_INT, MPI_MIN, PetscObjectComm((PetscObject)*dm)));
      PetscCall(PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_(PetscObjectComm((PetscObject)*dm)), "Minimum number of neighbors: %" PetscInt_FMT "\n", nranks));
      PetscCall(DMDestroy(dm));
      *dm = distributedMesh;
    }
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_post_redist_view"));
  }

  if (user->overlap) {
    DM overlapMesh = NULL;

    /* Add the overlap to refined mesh */
    PetscCall(PetscLogStagePush(user->stages[STAGE_OVERLAP]));
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_pre_overlap_view"));
    PetscCall(DMPlexDistributeOverlap(*dm, user->overlap, NULL, &overlapMesh));
    if (overlapMesh) {
      PetscInt overlap;
      PetscCall(DMPlexGetOverlap(overlapMesh, &overlap));
      PetscCall(PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD, "Overlap: %" PetscInt_FMT "\n", overlap));
      PetscCall(DMDestroy(dm));
      *dm = overlapMesh;
    }
    PetscCall(DMViewFromOptions(*dm, NULL, "-dm_post_overlap_view"));
    PetscCall(PetscLogStagePop());
  }
  if (user->final_ref) {
    DM refinedMesh = NULL;

    PetscCall(DMPlexSetRefinementUniform(*dm, PETSC_TRUE));
    PetscCall(DMRefine(*dm, comm, &refinedMesh));
    if (refinedMesh) {
      PetscCall(DMDestroy(dm));
      *dm = refinedMesh;
    }
  }

  PetscCall(PetscObjectSetName((PetscObject)*dm, "Generated Mesh"));
  PetscCall(DMViewFromOptions(*dm, NULL, "-dm_view"));
  if (user->final_diagnostics) PetscCall(DMPlexCheck(*dm));
  PetscCall(PetscLogEventEnd(user->createMeshEvent, 0, 0, 0, 0));
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  DM     dm;
  AppCtx user;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  PetscCall(ProcessOptions(PETSC_COMM_WORLD, &user));
  PetscCall(CreateMesh(PETSC_COMM_WORLD, &user, &dm));
  PetscCall(DMDestroy(&dm));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

  # CTetGen 0-1
  test:
    suffix: 0
    requires: ctetgen
    args: -dm_coord_space 0 -dm_plex_dim 3 -dim 3 -dm_plex_interpolate 0 -ctetgen_verbose 4 -dm_view ascii::ascii_info_detail -info :~sys
  test:
    suffix: 1
    requires: ctetgen
    args: -dm_coord_space 0 -dm_plex_dim 3 -dim 3 -dm_plex_interpolate 0 -ctetgen_verbose 4 -dm_refine_volume_limit_pre 0.0625 -dm_view ascii::ascii_info_detail -info :~sys

  # 2D LaTex and ASCII output 2-9
  test:
    suffix: 2
    requires: triangle
    args: -dm_plex_interpolate 0 -dm_view ascii::ascii_latex
  test:
    suffix: 3
    requires: triangle
    args: -ref_dm_refine 1 -dm_view ascii::ascii_info_detail
  test:
    suffix: 4
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -ref_dm_refine 1 -dist_dm_distribute -petscpartitioner_type simple -dm_view ascii::ascii_info_detail
  test:
    suffix: 5
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -ref_dm_refine 1 -dist_dm_distribute -petscpartitioner_type simple -dm_view ascii::ascii_latex
  test:
    suffix: 6
    args: -dm_coord_space 0 -dm_plex_simplex 0 -dm_view ascii::ascii_info_detail
  test:
    suffix: 7
    args: -dm_coord_space 0 -dm_plex_simplex 0 -ref_dm_refine 1 -dm_view ascii::ascii_info_detail
  test:
    suffix: 8
    nsize: 2
    args: -dm_plex_simplex 0 -ref_dm_refine 1 -dist_dm_distribute -petscpartitioner_type simple -dm_view ascii::ascii_latex
  test:
    suffix: box_2d_latex_xper
    nsize: 1
    args: -dm_plex_simplex 0 -dm_plex_box_faces 5,5 -dm_plex_box_bd periodic,none \
          -dist_dm_distribute -petscpartitioner_type simple -dm_view ascii::ascii_latex -dm_plex_view_edges 0

  # 1D ASCII output
  testset:
    args: -dm_coord_space 0 -dm_plex_dim 1 -dm_view ascii::ascii_info_detail -dm_plex_check_all
    test:
      suffix: 1d_0
      args:
    test:
      suffix: 1d_1
      args: -ref_dm_refine 2
    test:
      suffix: 1d_2
      args: -dm_plex_box_faces 5 -dm_plex_box_bd periodic

  # Parallel refinement tests with overlap
  test:
    suffix: refine_overlap_1d
    nsize: 2
    args: -dm_plex_dim 1 -dim 1 -dm_plex_box_faces 4 -dm_plex_box_faces 4 -ref_dm_refine 1 -overlap {{0 1 2}separate output} -dist_dm_distribute -petscpartitioner_type simple -dm_view ascii::ascii_info
  test:
    suffix: refine_overlap_2d
    requires: triangle
    nsize: {{2 8}separate output}
    args: -dm_coord_space 0 -ref_dm_refine 1 -dist_dm_distribute -petscpartitioner_type simple -overlap {{0 1 2}separate output} -dm_view ascii::ascii_info

  # Parallel extrusion tests
  test:
    suffix: 1d_extruded
    args: -dm_plex_dim 1 -dm_plex_box_faces 5 -dm_extrude 3 -dm_plex_check_all -dm_view draw

  test:
    # This test needs a non-tensor prism so we can make a coordinate space
    suffix: spheresurface_extruded
    nsize : 4
    args: -dm_plex_shape sphere -dm_extrude 3 -dm_plex_transform_extrude_use_tensor 0 \
          -dist_dm_distribute -petscpartitioner_type simple \
          -dm_plex_check_all -dm_view ::ascii_info_detail -dm_plex_view_coord_system spherical

  test:
    # This test needs a non-tensor prism so we can make a coordinate space
    suffix: spheresurface_extruded_symmetric
    nsize : 4
    args: -dm_plex_shape sphere -dm_extrude 3 -dm_plex_transform_extrude_use_tensor 0 -dm_plex_transform_extrude_symmetric \
          -dist_dm_distribute -petscpartitioner_type simple \
          -dm_plex_check_all -dm_view ::ascii_info_detail -dm_plex_view_coord_system spherical

  test:
    # Test with a tensor prism which cannot have a coordinate space
    suffix: spheresurface_extruded_nocoord
    nsize : 4
    args: -dm_coord_space 0 -dm_plex_shape sphere -dm_extrude 3 \
          -dist_dm_distribute -petscpartitioner_type simple \
          -dm_plex_check_all -dm_view ::ascii_info_detail -dm_plex_view_coord_system spherical

  # Parallel simple partitioner tests
  test:
    suffix: part_simple_0
    requires: triangle
    nsize: 2
    args: -dm_coord_space 0 -dm_plex_interpolate 0 -dist_dm_distribute -petscpartitioner_type simple -dist_partition_view -dm_view ascii::ascii_info_detail
  test:
    suffix: part_simple_1
    requires: triangle
    nsize: 8
    args: -dm_coord_space 0 -ref_dm_refine 1 -dist_dm_distribute -petscpartitioner_type simple -dist_partition_view -dm_view ascii::ascii_info_detail

  # Parallel partitioner tests
  test:
    suffix: part_parmetis_0
    requires: parmetis
    nsize: 2
    args: -dm_plex_simplex 0 -ref_dm_refine 1 -dist_dm_distribute -petscpartitioner_type parmetis -dm_view -petscpartitioner_view -test_redistribute -dm_plex_csr_alg {{mat graph overlap}} -dm_pre_redist_view ::load_balance -dm_post_redist_view ::load_balance -petscpartitioner_view_graph
  test:
    suffix: part_ptscotch_0
    requires: ptscotch
    nsize: 2
    args: -dm_plex_simplex 0 -dist_dm_distribute -petscpartitioner_type ptscotch -petscpartitioner_view -petscpartitioner_ptscotch_strategy quality -test_redistribute -dm_plex_csr_alg {{mat graph overlap}} -dm_pre_redist_view ::load_balance -dm_post_redist_view ::load_balance -petscpartitioner_view_graph
  test:
    suffix: part_ptscotch_1
    requires: ptscotch
    nsize: 8
    args: -dm_plex_simplex 0 -ref_dm_refine 1 -dist_dm_distribute -petscpartitioner_type ptscotch -petscpartitioner_view -petscpartitioner_ptscotch_imbalance 0.1

  # CGNS reader tests 10-11 (need to find smaller test meshes)
  test:
    suffix: cgns_0
    requires: cgns
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/tut21.cgns -dm_view

  # ExodusII reader tests
  testset:
    args: -dm_plex_boundary_label boundary -dm_plex_check_all -dm_view
    test:
      suffix: exo_0
      requires: exodusii
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/sevenside-quad.exo
    test:
      suffix: exo_1
      requires: exodusii
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/sevenside-quad-15.exo
    test:
      suffix: exo_2
      requires: exodusii
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/squaremotor-30.exo
    test:
      suffix: exo_3
      requires: exodusii
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/blockcylinder-50.exo
    test:
      suffix: exo_4
      requires: exodusii
     args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/simpleblock-100.exo

  # Gmsh mesh reader tests
  testset:
    args: -dm_coord_space 0 -dm_view

    test:
      suffix: gmsh_0
      requires: !single
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/doublet-tet.msh
    test:
      suffix: gmsh_1
      requires: !single
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square.msh
    test:
      suffix: gmsh_2
      requires: !single
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_bin.msh
    test:
      suffix: gmsh_3
      nsize: 3
      requires: !single
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square.msh -dist_dm_distribute -petscpartitioner_type simple
    test:
      suffix: gmsh_4
      nsize: 3
      requires: !single
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_bin.msh -dist_dm_distribute -petscpartitioner_type simple
    test:
      suffix: gmsh_5
      requires: !single
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_quad.msh
    # TODO: it seems the mesh is not a valid gmsh (inverted cell)
    test:
      suffix: gmsh_6
      requires: !single
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_bin_physnames.msh -final_diagnostics 0
    test:
      suffix: gmsh_7
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/mesh-3d-box-innersphere_bin.msh -dm_view ::ascii_info_detail -dm_plex_check_all
    test:
      suffix: gmsh_8
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/mesh-3d-box-innersphere.msh -dm_view ::ascii_info_detail -dm_plex_check_all
  testset:
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic_bin.msh -dm_view ::ascii_info_detail -dm_plex_check_all
    test:
      suffix: gmsh_9
    test:
      suffix: gmsh_9_periodic_0
      args: -dm_plex_gmsh_periodic 0
  testset:
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic.msh -dm_view ::ascii_info_detail -dm_plex_check_all
    test:
      suffix: gmsh_10
    test:
      suffix: gmsh_10_periodic_0
      args: -dm_plex_gmsh_periodic 0
  testset:
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic.msh -dm_view ::ascii_info_detail -dm_plex_check_all -ref_dm_refine 1
    test:
      suffix: gmsh_11
    test:
      suffix: gmsh_11_periodic_0
      args: -dm_plex_gmsh_periodic 0
  # TODO: it seems the mesh is not a valid gmsh (inverted cell)
  test:
    suffix: gmsh_12
    nsize: 4
    requires: !single mpiio
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_bin_physnames.msh -viewer_binary_mpiio -dist_dm_distribute -petscpartitioner_type simple -dm_view -final_diagnostics 0
  test:
    suffix: gmsh_13_hybs2t
    nsize: 4
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_triquad.msh -dist_dm_distribute -petscpartitioner_type simple -dm_view -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox -dm_plex_check_all
  test:
    suffix: gmsh_14_ext
    requires: !single
    args: -dm_coord_space 0 -dm_extrude 2 -dm_plex_transform_extrude_thickness 1.5 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_bin.msh -dm_view -dm_plex_check_all
  test:
    suffix: gmsh_14_ext_s2t
    requires: !single
    args: -dm_coord_space 0 -dm_extrude 2 -dm_plex_transform_extrude_thickness 1.5 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_bin.msh -dm_view -dm_plex_check_all -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox
  test:
    suffix: gmsh_15_hyb3d
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_tetwedge.msh -dm_view -dm_plex_check_all
  test:
    suffix: gmsh_15_hyb3d_vtk
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_tetwedge.msh -dm_view vtk: -dm_plex_gmsh_hybrid -dm_plex_check_all
  test:
    suffix: gmsh_15_hyb3d_s2t
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_tetwedge.msh -dm_view -dm_plex_check_all -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox
  test:
    suffix: gmsh_16_spheresurface
    nsize : 4
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3 -dm_plex_check_all -dm_view -dist_dm_distribute -petscpartitioner_type simple
  test:
    suffix: gmsh_16_spheresurface_s2t
    nsize : 4
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3 -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox -dm_plex_check_all -dm_view -dist_dm_distribute -petscpartitioner_type simple
  test:
    suffix: gmsh_16_spheresurface_extruded
    nsize : 4
    args: -dm_coord_space 0 -dm_extrude 3 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3 -dm_plex_check_all -dm_view -dist_dm_distribute -petscpartitioner_type simple
  test:
    suffix: gmsh_16_spheresurface_extruded_s2t
    nsize : 4
    args: -dm_coord_space 0 -dm_extrude 3 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3 -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox -dm_plex_check_all -dm_view -dist_dm_distribute -petscpartitioner_type simple
  test:
    suffix: gmsh_17_hyb3d_interp_ascii
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_hexwedge.msh -dm_view -dm_plex_check_all
  test:
    suffix: exodus_17_hyb3d_interp_ascii
    requires: exodusii
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_hexwedge.exo -dm_view -dm_plex_check_all

  # Legacy Gmsh v22/v40 ascii/binary reader tests
  testset:
    output_file: output/ex1_gmsh_3d_legacy.out
    args: -dm_coord_space 0 -dm_view ::ascii_info_detail -dm_plex_check_all
    test:
      suffix: gmsh_3d_ascii_v22
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-ascii.msh2
    test:
      suffix: gmsh_3d_ascii_v40
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-ascii.msh4
    test:
      suffix: gmsh_3d_binary_v22
      # Could not remake binary to remove extra face labeling
      output_file: output/ex1_gmsh_3d_legacy_v22_bin.out
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary.msh2
    test:
      suffix: gmsh_3d_binary_v40
      requires: long64
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary.msh4

  # Gmsh v41 ascii/binary reader tests
  testset: # 32bit mesh, sequential
    args: -dm_coord_space 0 -dm_view ::ascii_info_detail -dm_plex_check_all -dm_plex_gmsh_mark_vertices
    output_file: output/ex1_gmsh_3d_32.out
    test:
      suffix: gmsh_3d_ascii_v41_32
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-ascii-32.msh
    test:
      suffix: gmsh_3d_binary_v41_32
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary-32.msh
    test:
      suffix: gmsh_3d_binary_v41_32_mpiio
      requires: defined(PETSC_HAVE_MPIIO)
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary-32.msh -viewer_binary_mpiio
  test:
    suffix: gmsh_quad_8node
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-qua-8node.msh \
          -dm_view -dm_plex_check_all -dm_plex_gmsh_mark_vertices
  test:
    suffix: gmsh_hex_20node
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-hex-20node.msh \
          -dm_view -dm_plex_check_all -dm_plex_gmsh_mark_vertices
  testset:  # 32bit mesh, parallel
    args: -dm_coord_space 0 -dist_dm_distribute -petscpartitioner_type simple -dm_view ::ascii_info_detail -dm_plex_check_all -dm_plex_gmsh_mark_vertices
    nsize: 2
    output_file: output/ex1_gmsh_3d_32_np2.out
    test:
      suffix: gmsh_3d_ascii_v41_32_np2
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-ascii-32.msh
    test:
      suffix: gmsh_3d_binary_v41_32_np2
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary-32.msh
    test:
      suffix: gmsh_3d_binary_v41_32_np2_mpiio
      requires: defined(PETSC_HAVE_MPIIO)
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary-32.msh -viewer_binary_mpiio
  testset: # 64bit mesh, sequential
    args: -dm_coord_space 0 -dm_view ::ascii_info_detail -dm_plex_check_all -dm_plex_gmsh_mark_vertices
    output_file: output/ex1_gmsh_3d_64.out
    test:
      suffix: gmsh_3d_ascii_v41_64
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-ascii-64.msh
    test:
      suffix: gmsh_3d_binary_v41_64
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary-64.msh
    test:
      suffix: gmsh_3d_binary_v41_64_mpiio
      requires: defined(PETSC_HAVE_MPIIO)
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary-64.msh -viewer_binary_mpiio
  testset:  # 64bit mesh, parallel
    args: -dm_coord_space 0 -dist_dm_distribute -petscpartitioner_type simple -dm_view ::ascii_info_detail -dm_plex_check_all -dm_plex_gmsh_mark_vertices
    nsize: 2
    output_file: output/ex1_gmsh_3d_64_np2.out
    test:
      suffix: gmsh_3d_ascii_v41_64_np2
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-ascii-64.msh
    test:
      suffix: gmsh_3d_binary_v41_64_np2
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary-64.msh
    test:
      suffix: gmsh_3d_binary_v41_64_np2_mpiio
      requires: defined(PETSC_HAVE_MPIIO)
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/gmsh-3d-binary-64.msh -viewer_binary_mpiio

  # Fluent mesh reader tests
  # TODO: Geometry checks fail
  test:
    suffix: fluent_0
    requires: !complex
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square.cas -dm_view -final_diagnostics 0
  test:
    suffix: fluent_1
    nsize: 3
    requires: !complex
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square.cas -dist_dm_distribute -petscpartitioner_type simple -dm_view -final_diagnostics 0
  test:
    suffix: fluent_2
    requires: !complex
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/cube_5tets_ascii.cas -dm_view -final_diagnostics 0
  test:
    suffix: fluent_3
    requires: !complex
    TODO: Fails on non-linux: fseek(), fileno() ? https://gitlab.com/petsc/petsc/merge_requests/2206#note_238166382
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/cube_5tets.cas -dm_view -final_diagnostics 0

  # Med mesh reader tests, including parallel file reads
  test:
    suffix: med_0
    requires: med
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square.med -dm_view
  test:
    suffix: med_1
    requires: med
    nsize: 3
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square.med -dist_dm_distribute -petscpartitioner_type simple -dm_view
  test:
    suffix: med_2
    requires: med
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/cylinder.med -dm_view
  test:
    suffix: med_3
    requires: med
    TODO: MED
    nsize: 3
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/cylinder.med -dist_dm_distribute -petscpartitioner_type simple -dm_view

  # Test shape quality
  test:
    suffix: test_shape
    requires: ctetgen
    args: -dm_plex_dim 3 -dim 3 -dm_refine_hierarchy 3 -dm_plex_check_all -dm_plex_check_cell_shape

  # Test simplex to tensor conversion
  test:
    suffix: s2t2
    requires: triangle
    args: -dm_coord_space 0 -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox -dm_refine_volume_limit_pre 0.0625 -dm_view ascii::ascii_info_detail

  test:
    suffix: s2t3
    requires: ctetgen
    args: -dm_coord_space 0 -dm_plex_dim 3 -dim 3 -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox -dm_refine_volume_limit_pre 0.0625 -dm_view ascii::ascii_info_detail

  # Test cylinder
  testset:
    args: -dm_plex_shape cylinder -dm_plex_check_all -dm_view
    test:
      suffix: cylinder
      args: -ref_dm_refine 1
    test:
      suffix: cylinder_per
      args: -dm_plex_cylinder_bd periodic -ref_dm_refine 1 -ref_dm_refine_remap 0
    test:
      suffix: cylinder_wedge
      args: -dm_coord_space 0 -dm_plex_interpolate 0 -dm_plex_cell tensor_triangular_prism -dm_view vtk:
    test:
      suffix: cylinder_wedge_int
      output_file: output/ex1_cylinder_wedge.out
      args: -dm_coord_space 0 -dm_plex_cell tensor_triangular_prism -dm_view vtk:

  test:
    suffix: box_2d
    args: -dm_plex_simplex 0 -ref_dm_refine 2 -dm_plex_check_all -dm_view

  test:
    suffix: box_2d_per
    args: -dm_plex_simplex 0 -ref_dm_refine 2 -dm_plex_check_all -dm_view

  test:
    suffix: box_2d_per_unint
    args: -dm_coord_space 0 -dm_plex_simplex 0 -dm_plex_interpolate 0 -dm_plex_box_faces 3,3 -dm_plex_box_faces 3,3 -dm_plex_check_all -dm_view ::ascii_info_detail

  test:
    suffix: box_3d
    args: -dm_plex_dim 3 -dim 3 -dm_plex_simplex 0 -ref_dm_refine 3 -dm_plex_check_all -dm_view

  test:
    requires: triangle
    suffix: box_wedge
    args: -dm_coord_space 0 -dm_plex_dim 3 -dim 3 -dm_plex_simplex 0 -dm_plex_cell tensor_triangular_prism -dm_view vtk: -dm_plex_check_all

  testset:
    requires: triangle
    args: -dm_coord_space 0 -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_cell tensor_triangular_prism -dm_plex_box_faces 2,3,1 -dm_view -dm_plex_check_all -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox
    test:
      suffix: box_wedge_s2t
    test:
      nsize: 3
      args: -dist_dm_distribute -petscpartitioner_type simple
      suffix: box_wedge_s2t_parallel

  # Test GLVis output
  testset:
    args: -dm_coord_space 0 -dm_plex_interpolate 0
    test:
      suffix: glvis_2d_tet
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic.msh -dm_plex_gmsh_periodic 0 -dm_view glvis:
    test:
      suffix: glvis_2d_tet_per
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic.msh -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary 0
    test:
      suffix: glvis_3d_tet
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/mesh-3d-box-innersphere_bin.msh -dm_plex_gmsh_periodic 0 -dm_view glvis:
  testset:
    args: -dm_coord_space 0
    test:
      suffix: glvis_2d_tet_per_mfem
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic.msh -viewer_glvis_dm_plex_enable_boundary -viewer_glvis_dm_plex_enable_mfem -dm_view glvis:
    test:
      suffix: glvis_2d_quad
      args: -dm_plex_simplex 0 -dm_plex_box_faces 3,3 -dm_view glvis:
    test:
      suffix: glvis_2d_quad_per
      args: -dm_plex_simplex 0 -dm_plex_box_faces 3,3 -dm_plex_box_bd periodic,periodic -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary
    test:
      suffix: glvis_2d_quad_per_shift
      args: -dm_plex_simplex 0 -dm_plex_box_faces 3,3 -dm_plex_box_bd periodic,periodic -dm_plex_box_lower -1,-1 -dm_plex_box_upper 1,1 -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary
    test:
      suffix: glvis_2d_quad_per_mfem
      args: -dm_plex_simplex 0 -dm_plex_box_faces 3,3 -dm_plex_box_bd periodic,periodic -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary -viewer_glvis_dm_plex_enable_mfem
    test:
      suffix: glvis_3d_tet_per
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/mesh-3d-box-innersphere_bin.msh -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary
    test:
      suffix: glvis_3d_tet_per_mfem
      TODO: broken
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/mesh-3d-box-innersphere_bin.msh -viewer_glvis_dm_plex_enable_mfem -dm_view glvis:
    test:
      suffix: glvis_3d_hex
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_faces 3,3,3 -dm_view glvis:
    test:
      suffix: glvis_3d_hex_per
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_faces 3,3,3 -dm_plex_box_bd periodic,periodic,periodic -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary 0
    test:
      suffix: glvis_3d_hex_per_mfem
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_faces 3,3,3 -dm_plex_box_bd periodic,periodic,periodic -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary -viewer_glvis_dm_plex_enable_mfem
    test:
      suffix: glvis_2d_hyb
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_triquad.msh -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary
    test:
      suffix: glvis_3d_hyb
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_tetwedge.msh -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary
    test:
      suffix: glvis_3d_hyb_s2t
      args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_3d_cube.msh -dm_view glvis: -viewer_glvis_dm_plex_enable_boundary -ref_dm_refine 1 -ref_dm_plex_transform_type refine_tobox -dm_plex_check_all

  # Test P4EST
  testset:
    requires: p4est
    args: -dm_coord_space 0 -dm_view -test_p4est_seq -conv_seq_2_dm_plex_check_all -conv_seq_1_dm_forest_minimum_refinement 1
    test:
      suffix: p4est_periodic
      args: -dm_plex_simplex 0 -dm_plex_box_bd periodic,periodic -dm_plex_box_faces 3,5 -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 2 -conv_seq_1_dm_p4est_refine_pattern hash
    test:
      suffix: p4est_periodic_3d
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_bd periodic,periodic,none -dm_plex_box_faces 3,5,4 -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 2 -conv_seq_1_dm_p4est_refine_pattern hash
    test:
      suffix: p4est_gmsh_periodic
      args: -dm_coord_space 0 -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 1 -conv_seq_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic.msh
    test:
      suffix: p4est_gmsh_surface
      args: -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 1 -conv_seq_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3
    test:
      suffix: p4est_gmsh_surface_parallel
      nsize: 2
      args: -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 1 -conv_seq_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3 -petscpartitioner_type simple -dm_view ::load_balance
    test:
      suffix: p4est_hyb_2d
      args: -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 1 -conv_seq_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_triquad.msh
    test:
      suffix: p4est_hyb_3d
      args: -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 1 -conv_seq_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_tetwedge.msh
    test:
      requires: ctetgen
      suffix: p4est_s2t_bugfaces_3d
      args: -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 0 -dm_plex_dim 3 -dm_plex_box_faces 1,1
    test:
      suffix: p4est_bug_overlapsf
      nsize: 3
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_faces 2,2,1 -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 1 -conv_seq_1_dm_p4est_refine_pattern hash -petscpartitioner_type simple
    test:
      suffix: p4est_redistribute
      nsize: 3
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_faces 2,2,1 -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 1 -conv_seq_1_dm_p4est_refine_pattern hash -petscpartitioner_type simple -test_redistribute -dm_plex_csr_alg {{mat graph overlap}} -dm_view ::load_balance
    test:
      suffix: p4est_gmsh_s2t_3d
      args: -conv_seq_1_dm_forest_initial_refinement 1 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/doublet-tet.msh
    test:
      suffix: p4est_gmsh_s2t_3d_hash
      args: -conv_seq_1_dm_forest_initial_refinement 1 -conv_seq_1_dm_forest_maximum_refinement 2 -conv_seq_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/doublet-tet.msh
    test:
      requires: long_runtime
      suffix: p4est_gmsh_periodic_3d
      args: -dm_coord_space 0 -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 1 -conv_seq_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/mesh-3d-box-innersphere.msh

  testset:
    requires: p4est
    nsize: 6
    args: -dm_coord_space 0 -test_p4est_par -conv_par_2_dm_plex_check_all -conv_par_1_dm_forest_minimum_refinement 1 -conv_par_1_dm_forest_partition_overlap 0 -dist_dm_distribute
    test:
      TODO: interface cones do not conform
      suffix: p4est_par_periodic
      args: -dm_plex_simplex 0 -dm_plex_box_bd periodic,periodic -dm_plex_box_faces 3,5 -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 2 -conv_par_1_dm_p4est_refine_pattern hash
    test:
      TODO: interface cones do not conform
      suffix: p4est_par_periodic_3d
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_bd periodic,periodic,periodic -dm_plex_box_faces 3,5,4 -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 2 -conv_par_1_dm_p4est_refine_pattern hash
    test:
      TODO: interface cones do not conform
      suffix: p4est_par_gmsh_periodic
      args: -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 1 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic.msh
    test:
      suffix: p4est_par_gmsh_surface
      args: -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 1 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3
    test:
      suffix: p4est_par_gmsh_s2t_3d
      args: -conv_par_1_dm_forest_initial_refinement 1 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/doublet-tet.msh
    test:
      TODO: interface cones do not conform
      suffix: p4est_par_gmsh_s2t_3d_hash
      args: -conv_par_1_dm_forest_initial_refinement 1 -conv_par_1_dm_forest_maximum_refinement 2 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/doublet-tet.msh
    test:
      requires: long_runtime
      suffix: p4est_par_gmsh_periodic_3d
      args: -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 1 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/mesh-3d-box-innersphere.msh

  testset:
    requires: p4est
    nsize: 6
    args: -dm_coord_space 0 -test_p4est_par -conv_par_2_dm_plex_check_all -conv_par_1_dm_forest_minimum_refinement 1 -conv_par_1_dm_forest_partition_overlap 1 -dist_dm_distribute -petscpartitioner_type simple
    test:
      suffix: p4est_par_ovl_periodic
      args: -dm_plex_simplex 0 -dm_plex_box_bd periodic,periodic -dm_plex_box_faces 3,5 -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 2 -conv_par_1_dm_p4est_refine_pattern hash
    #TODO Mesh cell 201 is inverted, vol = 0. (FVM Volume. Is it correct? -> Diagnostics disabled)
    test:
      suffix: p4est_par_ovl_periodic_3d
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_bd periodic,periodic,none -dm_plex_box_faces 3,5,4 -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 2 -conv_par_1_dm_p4est_refine_pattern hash -final_diagnostics 0
    test:
      suffix: p4est_par_ovl_gmsh_periodic
      args: -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 1 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/square_periodic.msh
    test:
      suffix: p4est_par_ovl_gmsh_surface
      args: -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 1 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3
    test:
      suffix: p4est_par_ovl_gmsh_s2t_3d
      args: -conv_par_1_dm_forest_initial_refinement 1 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/doublet-tet.msh
    test:
      suffix: p4est_par_ovl_gmsh_s2t_3d_hash
      args: -conv_par_1_dm_forest_initial_refinement 1 -conv_par_1_dm_forest_maximum_refinement 2 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/doublet-tet.msh
    test:
      requires: long_runtime
      suffix: p4est_par_ovl_gmsh_periodic_3d
      args: -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 1 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/mesh-3d-box-innersphere.msh
    test:
      suffix: p4est_par_ovl_hyb_2d
      args: -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 1 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_triquad.msh
    test:
      suffix: p4est_par_ovl_hyb_3d
      args: -conv_par_1_dm_forest_initial_refinement 0 -conv_par_1_dm_forest_maximum_refinement 1 -conv_par_1_dm_p4est_refine_pattern hash -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_tetwedge.msh

  test:
    TODO: broken
    requires: p4est
    nsize: 2
    suffix: p4est_bug_labels_noovl
    args: -test_p4est_seq -dm_plex_check_all -dm_forest_minimum_refinement 0 -dm_forest_partition_overlap 1 -dm_plex_simplex 0 -dm_plex_box_faces 3,3 -dm_forest_initial_refinement 0 -dm_forest_maximum_refinement 2 -dm_p4est_refine_pattern hash -dist_dm_distribute -petscpartitioner_type simple -dm_forest_print_label_error

  test:
    requires: p4est
    nsize: 2
    suffix: p4est_bug_distribute_overlap
    args: -dm_coord_space 0 -test_p4est_seq -conv_seq_2_dm_plex_check_all -conv_seq_1_dm_forest_minimum_refinement 0 -conv_seq_1_dm_forest_partition_overlap 0 -dm_plex_simplex 0 -dm_plex_box_faces 3,3 -conv_seq_1_dm_forest_initial_refinement 0 -conv_seq_1_dm_forest_maximum_refinement 2 -conv_seq_1_dm_p4est_refine_pattern hash -petscpartitioner_type simple -overlap 1 -dm_view ::load_balance
    args: -dm_post_overlap_view

  test:
    suffix: ref_alfeld2d_0
    requires: triangle
    args: -dm_plex_box_faces 5,3 -dm_view -dm_plex_check_all -ref_dm_refine 1 -ref_dm_plex_transform_type refine_alfeld -final_diagnostics
  test:
    suffix: ref_alfeld3d_0
    requires: ctetgen
    args: -dm_plex_dim 3 -dm_plex_box_faces 5,1,1 -dm_view -dm_plex_check_all -ref_dm_refine 1 -ref_dm_plex_transform_type refine_alfeld -final_diagnostics

  # Boundary layer refiners
  test:
    suffix: ref_bl_1
    args: -dm_plex_dim 1 -dm_plex_simplex 0 -dm_plex_box_faces 5,1 -dm_view -dm_plex_check_all 0 -ref_dm_refine 1 -ref_dm_plex_transform_type refine_boundary_layer -dm_extrude 2 -final_diagnostics -ref_dm_plex_transform_bl_splits 3
  test:
    suffix: ref_bl_2_tri
    requires: triangle
    args: -dm_coord_space 0 -dm_plex_box_faces 5,3 -dm_view -dm_plex_check_all 0 -ref_dm_refine 1 -ref_dm_plex_transform_type refine_boundary_layer -dm_extrude 3 -final_diagnostics -ref_dm_plex_transform_bl_splits 4
  test:
    suffix: ref_bl_3_quad
    args: -dm_plex_simplex 0 -dm_plex_box_faces 5,1 -dm_view -dm_plex_check_all 0 -ref_dm_refine 1 -ref_dm_plex_transform_type refine_boundary_layer -dm_extrude 3 -final_diagnostics -ref_dm_plex_transform_bl_splits 4
  test:
    suffix: ref_bl_spheresurface_extruded
    nsize : 4
    args: -dm_coord_space 0 -dm_extrude 3 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/surfacesphere_bin.msh -dm_plex_gmsh_spacedim 3 -dm_plex_check_all -dm_view -dist_dm_distribute -petscpartitioner_type simple -final_diagnostics -ref_dm_refine 1 -ref_dm_plex_transform_type refine_boundary_layer -ref_dm_plex_transform_bl_splits 2
  test:
    suffix: ref_bl_3d_hyb
    nsize : 4
    args: -dm_coord_space 0 -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/hybrid_3d_cube.msh -dm_plex_check_all -dm_view -dist_dm_distribute -petscpartitioner_type simple -final_diagnostics -ref_dm_refine 1 -ref_dm_plex_transform_type refine_boundary_layer -ref_dm_plex_transform_bl_splits 4 -ref_dm_plex_transform_bl_height_factor 3.1

  testset:
    args: -dm_plex_shape sphere -dm_plex_check_all -dm_view
    test:
      suffix: sphere_0
      args:
    test:
      suffix: sphere_1
      args: -ref_dm_refine 2
    test:
      suffix: sphere_2
      args: -dm_plex_simplex 0
    test:
      suffix: sphere_3
      args: -dm_plex_simplex 0 -ref_dm_refine 2

  test:
    suffix: ball_0
    requires: ctetgen
    args: -dm_plex_dim 3 -dm_plex_shape ball -dm_plex_check_all -dm_view

  test:
    suffix: ball_1
    requires: ctetgen
    args: -dm_plex_dim 3 -dm_plex_shape ball -bd_dm_refine 2 -dm_plex_check_all -dm_view

  test:
    suffix: schwarz_p_extrude
    args: -dm_plex_shape schwarz_p -dm_plex_tps_extent 1,1,1 -dm_plex_tps_layers 1 -dm_plex_tps_thickness .2 -dm_view

  test:
    suffix: pyr_mixed_0
    args: -dm_plex_filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/pyr_tet.msh -dm_plex_check_all -dm_view

  test:
    suffix: hypercubic_0
    args: -dm_plex_dim 2 -dm_plex_shape hypercubic -dm_plex_box_faces 3,3 -dm_plex_check_all -dm_view

  test:
    suffix: hypercubic_1
    args: -dm_plex_dim 3 -dm_plex_shape hypercubic -dm_plex_box_faces 3,3,3 -dm_plex_check_all -dm_view

  test:
    suffix: hypercubic_2
    args: -dm_plex_dim 4 -dm_plex_shape hypercubic -dm_plex_box_faces 3,3,3,3 -dm_view \
          -dm_plex_check_symmetry -dm_plex_check_skeleton -dm_plex_check_faces -dm_plex_check_pointsf -final_diagnostics 0

  test:
    suffix: hypercubic_3
    args: -dm_plex_dim 5 -dm_plex_shape hypercubic -dm_plex_box_faces 3,3,3,3,3 -dm_view \
          -dm_plex_check_symmetry -dm_plex_check_skeleton -dm_plex_check_faces -dm_plex_check_pointsf -final_diagnostics 0

  test:
    suffix: hypercubic_4
    args: -dm_plex_dim 6 -dm_plex_shape hypercubic -dm_plex_box_faces 3,3,3,3,3,3 -dm_view \
          -dm_plex_check_symmetry -dm_plex_check_skeleton -dm_plex_check_faces -dm_plex_check_pointsf -final_diagnostics 0
TEST*/

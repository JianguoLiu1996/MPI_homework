static char help[] = "Load and save the mesh and fields to HDF5 and ExodusII\n\n";

#include <petsc/private/dmpleximpl.h>
#include <petscviewerhdf5.h>
#include <petscsf.h>

typedef struct {
  PetscBool         compare;                      /* Compare the meshes using DMPlexEqual() */
  PetscBool         compare_labels;               /* Compare labels in the meshes using DMCompareLabels() */
  PetscBool         distribute;                   /* Distribute the mesh */
  PetscBool         interpolate;                  /* Generate intermediate mesh elements */
  char              fname[PETSC_MAX_PATH_LEN];    /* Mesh filename */
  char              ofname[PETSC_MAX_PATH_LEN];   /* Output mesh filename */
  char              meshname[PETSC_MAX_PATH_LEN]; /* Mesh name */
  PetscViewerFormat format;                       /* Format to write and read */
  PetscBool         second_write_read;            /* Write and read for the 2nd time */
  PetscBool         use_low_level_functions;      /* Use low level functions for viewing and loading */
} AppCtx;

static PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscFunctionBeginUser;
  options->compare                 = PETSC_FALSE;
  options->compare_labels          = PETSC_FALSE;
  options->distribute              = PETSC_TRUE;
  options->interpolate             = PETSC_FALSE;
  options->fname[0]                = '\0';
  options->meshname[0]             = '\0';
  options->format                  = PETSC_VIEWER_DEFAULT;
  options->second_write_read       = PETSC_FALSE;
  options->use_low_level_functions = PETSC_FALSE;
  PetscCall(PetscStrncpy(options->ofname, "ex55.h5", sizeof(options->ofname)));

  PetscOptionsBegin(comm, "", "Meshing Problem Options", "DMPLEX");
  PetscCall(PetscOptionsBool("-compare", "Compare the meshes using DMPlexEqual()", "ex55.c", options->compare, &options->compare, NULL));
  PetscCall(PetscOptionsBool("-compare_labels", "Compare labels in the meshes using DMCompareLabels()", "ex55.c", options->compare_labels, &options->compare_labels, NULL));
  PetscCall(PetscOptionsBool("-distribute", "Distribute the mesh", "ex55.c", options->distribute, &options->distribute, NULL));
  PetscCall(PetscOptionsBool("-interpolate", "Generate intermediate mesh elements", "ex55.c", options->interpolate, &options->interpolate, NULL));
  PetscCall(PetscOptionsString("-filename", "The mesh file", "ex55.c", options->fname, options->fname, sizeof(options->fname), NULL));
  PetscCall(PetscOptionsString("-ofilename", "The output mesh file", "ex55.c", options->ofname, options->ofname, sizeof(options->ofname), NULL));
  PetscCall(PetscOptionsString("-meshname", "The mesh file", "ex55.c", options->meshname, options->meshname, sizeof(options->meshname), NULL));
  PetscCall(PetscOptionsEnum("-format", "Format to write and read", "ex55.c", PetscViewerFormats, (PetscEnum)options->format, (PetscEnum *)&options->format, NULL));
  PetscCall(PetscOptionsBool("-second_write_read", "Write and read for the 2nd time", "ex55.c", options->second_write_read, &options->second_write_read, NULL));
  PetscCall(PetscOptionsBool("-use_low_level_functions", "Use low level functions for viewing and loading", "ex55.c", options->use_low_level_functions, &options->use_low_level_functions, NULL));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CheckDistributed(DM dm, PetscBool expectedDistributed)
{
  PetscMPIInt size;
  PetscBool   distributed;
  const char  YES[] = "DISTRIBUTED";
  const char  NO[]  = "NOT DISTRIBUTED";

  PetscFunctionBeginUser;
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)dm), &size));
  if (size < 2) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(DMPlexIsDistributed(dm, &distributed));
  PetscCheck(distributed == expectedDistributed, PetscObjectComm((PetscObject)dm), PETSC_ERR_PLIB, "Expected DM being %s but actually is %s", expectedDistributed ? YES : NO, distributed ? YES : NO);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CheckInterpolated(DM dm, PetscBool expectedInterpolated)
{
  DMPlexInterpolatedFlag iflg;
  PetscBool              interpolated;
  const char             YES[] = "INTERPOLATED";
  const char             NO[]  = "NOT INTERPOLATED";

  PetscFunctionBeginUser;
  PetscCall(DMPlexIsInterpolatedCollective(dm, &iflg));
  interpolated = (PetscBool)(iflg == DMPLEX_INTERPOLATED_FULL);
  PetscCheck(interpolated == expectedInterpolated, PetscObjectComm((PetscObject)dm), PETSC_ERR_PLIB, "Expected DM being %s but actually is %s", expectedInterpolated ? YES : NO, interpolated ? YES : NO);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode CheckDistributedInterpolated(DM dm, PetscViewer v, AppCtx *user)
{
  PetscViewerFormat format;
  PetscBool         distributed, interpolated;

  PetscFunctionBeginUser;
  PetscCall(PetscViewerGetFormat(v, &format));
  switch (format) {
  case PETSC_VIEWER_HDF5_XDMF:
  case PETSC_VIEWER_HDF5_VIZ: {
    distributed  = PETSC_TRUE;
    interpolated = PETSC_FALSE;
  }; break;
  case PETSC_VIEWER_HDF5_PETSC:
  case PETSC_VIEWER_DEFAULT:
  case PETSC_VIEWER_NATIVE: {
    DMPlexStorageVersion version;

    PetscCall(PetscViewerHDF5GetDMPlexStorageVersionReading(v, &version));
    distributed  = (PetscBool)(version->major >= 3);
    interpolated = user->interpolate;
  }; break;
  default: {
    distributed  = PETSC_FALSE;
    interpolated = user->interpolate;
  }
  }
  PetscCall(CheckDistributed(dm, distributed));
  PetscCall(CheckInterpolated(dm, interpolated));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMPlexWriteAndReadHDF5(DM dm, const char filename[], const char prefix[], AppCtx *user, DM *dm_new)
{
  DM          dmnew;
  const char  savedName[]  = "Mesh";
  const char  loadedName[] = "Mesh_new";
  PetscViewer v;

  PetscFunctionBeginUser;
  PetscCall(PetscViewerHDF5Open(PetscObjectComm((PetscObject)dm), filename, FILE_MODE_WRITE, &v));
  PetscCall(PetscViewerPushFormat(v, user->format));
  PetscCall(PetscObjectSetName((PetscObject)dm, savedName));
  if (user->use_low_level_functions) {
    PetscCall(DMPlexTopologyView(dm, v));
    PetscCall(DMPlexCoordinatesView(dm, v));
    PetscCall(DMPlexLabelsView(dm, v));
  } else {
    PetscCall(DMView(dm, v));
  }
  PetscCall(PetscViewerFileSetMode(v, FILE_MODE_READ));
  PetscCall(DMCreate(PETSC_COMM_WORLD, &dmnew));
  PetscCall(DMSetType(dmnew, DMPLEX));
  PetscCall(DMPlexDistributeSetDefault(dmnew, PETSC_FALSE));
  PetscCall(PetscObjectSetName((PetscObject)dmnew, savedName));
  PetscCall(DMSetOptionsPrefix(dmnew, prefix));
  if (user->use_low_level_functions) {
    PetscSF sfXC;

    PetscCall(DMPlexTopologyLoad(dmnew, v, &sfXC));
    PetscCall(DMPlexCoordinatesLoad(dmnew, v, sfXC));
    PetscCall(DMPlexLabelsLoad(dmnew, v, sfXC));
    PetscCall(PetscSFDestroy(&sfXC));
  } else {
    PetscCall(DMLoad(dmnew, v));
  }
  PetscCall(CheckDistributedInterpolated(dmnew, v, user));
  PetscCall(PetscObjectSetName((PetscObject)dmnew, loadedName));
  PetscCall(PetscViewerPopFormat(v));
  PetscCall(PetscViewerDestroy(&v));
  *dm_new = dmnew;
  PetscFunctionReturn(PETSC_SUCCESS);
}

int main(int argc, char **argv)
{
  DM               dm, dmnew;
  PetscPartitioner part;
  AppCtx           user;
  PetscBool        flg;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, NULL, help));
  PetscCall(ProcessOptions(PETSC_COMM_WORLD, &user));
  PetscCall(DMPlexCreateFromFile(PETSC_COMM_WORLD, user.fname, user.meshname, user.interpolate, &dm));
  PetscCall(DMSetOptionsPrefix(dm, "orig_"));
  PetscCall(DMViewFromOptions(dm, NULL, "-dm_view"));
  PetscCall(CheckInterpolated(dm, user.interpolate));

  if (user.distribute) {
    DM dmdist;

    PetscCall(DMPlexGetPartitioner(dm, &part));
    PetscCall(PetscPartitionerSetType(part, PETSCPARTITIONERSIMPLE));
    PetscCall(PetscPartitionerSetFromOptions(part));
    PetscCall(DMPlexDistribute(dm, 0, NULL, &dmdist));
    if (dmdist) {
      PetscCall(DMDestroy(&dm));
      dm = dmdist;
      PetscCall(CheckDistributed(dm, PETSC_TRUE));
      PetscCall(CheckInterpolated(dm, user.interpolate));
    }
  }

  PetscCall(DMSetOptionsPrefix(dm, NULL));
  PetscCall(DMPlexDistributeSetDefault(dm, PETSC_FALSE));
  PetscCall(DMSetFromOptions(dm));
  PetscCall(DMViewFromOptions(dm, NULL, "-dm_view"));

  PetscCall(DMPlexWriteAndReadHDF5(dm, user.ofname, "new_", &user, &dmnew));

  if (user.second_write_read) {
    PetscCall(DMDestroy(&dm));
    dm = dmnew;
    PetscCall(DMPlexWriteAndReadHDF5(dm, user.ofname, "new_", &user, &dmnew));
  }

  PetscCall(DMViewFromOptions(dmnew, NULL, "-dm_view"));

  /* This currently makes sense only for sequential meshes. */
  if (user.compare) {
    PetscCall(DMPlexEqual(dmnew, dm, &flg));
    PetscCheck(flg, PETSC_COMM_WORLD, PETSC_ERR_ARG_INCOMP, "DMs are not equal");
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "DMs equal\n"));
  }
  if (user.compare_labels) {
    PetscCall(DMCompareLabels(dmnew, dm, NULL, NULL));
    PetscCall(PetscPrintf(PETSC_COMM_WORLD, "DMLabels equal\n"));
  }

  PetscCall(DMDestroy(&dm));
  PetscCall(DMDestroy(&dmnew));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST
  build:
    requires: hdf5
  # Idempotence of saving/loading
  #   Have to replace Exodus file, which is creating uninterpolated edges
  test:
    suffix: 0
    TODO: broken
    requires: exodusii
    args: -filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/Rect-tri3.exo -dm_view ascii::ascii_info_detail
    args: -format hdf5_petsc -compare
  test:
    suffix: 1
    TODO: broken
    requires: exodusii parmetis !defined(PETSC_USE_64BIT_INDICES)
    nsize: 2
    args: -filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/Rect-tri3.exo -dm_view ascii::ascii_info_detail
    args: -petscpartitioner_type parmetis
    args: -format hdf5_petsc -new_dm_view ascii::ascii_info_detail

  testset:
    requires: exodusii
    args: -filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/blockcylinder-50.exo
    test:
      suffix: 2
      nsize: {{1 2 4 8}separate output}
      args: -format {{default hdf5_petsc}separate output}
      args: -dm_plex_view_hdf5_storage_version {{1.0.0 2.0.0 3.0.0}}
      args: -interpolate {{0 1}separate output}
    test:
      suffix: 2a
      nsize: {{1 2 4 8}separate output}
      args: -format {{hdf5_xdmf hdf5_viz}separate output}

  test:
    suffix: 3
    requires: exodusii
    args: -filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/blockcylinder-50.exo -compare -compare_labels
    args: -dm_plex_view_hdf5_storage_version {{1.0.0 2.0.0 3.0.0}}

  # Load HDF5 file in XDMF format in parallel, write, read dm1, write, read dm2, and compare dm1 and dm2
  testset:
    suffix: 4
    requires: !complex
    args: -filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/blockcylinder-50.h5 -dm_plex_create_from_hdf5_xdmf
    args: -distribute 0 -second_write_read -compare
    test:
      suffix: hdf5_petsc
      nsize: {{1 2}}
      args: -format hdf5_petsc -compare_labels
      args: -dm_plex_view_hdf5_storage_version {{1.0.0 2.0.0 3.0.0}}
    test:
      suffix: hdf5_xdmf
      nsize: {{1 3 8}}
      args: -format hdf5_xdmf

  # Use low level functions, DMPlexTopologyView()/Load(), DMPlexCoordinatesView()/Load(), and DMPlexLabelsView()/Load()
  # TODO: The output is very long so keeping just 1.0.0 version. This test should be redesigned or removed.
  test:
    suffix: 5
    requires: exodusii
    nsize: 2
    args: -filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/blockcylinder-50.exo
    args: -dm_view ascii::ascii_info_detail
    args: -new_dm_view ascii::ascii_info_detail
    args: -format hdf5_petsc -use_low_level_functions {{0 1}}
    args: -dm_plex_view_hdf5_storage_version 1.0.0

  testset:
    suffix: 6
    requires: hdf5 !complex datafilespath
    nsize: {{1 3}}
    args: -dm_plex_check_symmetry -dm_plex_check_skeleton -dm_plex_check_geometry
    args: -filename ${DATAFILESPATH}/meshes/cube-hexahedra-refined.h5 -dm_plex_create_from_hdf5_xdmf -dm_plex_hdf5_topology_path /cells -dm_plex_hdf5_geometry_path /coordinates
    args: -format hdf5_petsc -second_write_read -compare -compare_labels
    args: -interpolate {{0 1}} -distribute {{0 1}}
    args: -dm_plex_view_hdf5_storage_version {{1.0.0 2.0.0 3.0.0}}

  testset:
    # the same data and settings as dm_impls_plex_tests-ex18_9%
    suffix: 9
    requires: hdf5 !complex datafilespath
    nsize: {{1 2 4}}
    args: -dm_plex_check_symmetry -dm_plex_check_skeleton -dm_plex_check_geometry
    args: -filename ${DATAFILESPATH}/meshes/cube-hexahedra-refined.h5 -dm_plex_create_from_hdf5_xdmf -dm_plex_hdf5_topology_path /cells -dm_plex_hdf5_geometry_path /coordinates
    args: -format {{hdf5_petsc hdf5_xdmf}} -second_write_read -compare
    args: -dm_plex_view_hdf5_storage_version 3.0.0
    test:
      suffix: hdf5_seqload
      args: -distribute
      args: -interpolate {{0 1}}
      args: -dm_plex_hdf5_force_sequential
    test:
      suffix: hdf5_seqload_metis
      requires: parmetis
      args: -distribute -petscpartitioner_type parmetis
      args: -interpolate 1
      args: -dm_plex_hdf5_force_sequential
    test:
      suffix: hdf5
      args: -interpolate 1
    test:
      suffix: hdf5_repart
      requires: parmetis
      args: -distribute -petscpartitioner_type parmetis
      args: -interpolate 1
    test:
      TODO: Parallel partitioning of uninterpolated meshes not supported
      suffix: hdf5_repart_ppu
      requires: parmetis
      args: -distribute -petscpartitioner_type parmetis
      args: -interpolate 0

  # reproduce PetscSFView() crash - fixed, left as regression test
  test:
    suffix: new_dm_view
    requires: exodusii
    nsize: 2
    args: -filename ${wPETSC_DIR}/share/petsc/datafiles/meshes/TwoQuads.exo -new_dm_view ascii:ex5_new_dm_view.log:ascii_info_detail
    args: -dm_plex_view_hdf5_storage_version {{1.0.0 2.0.0 3.0.0}}

  # test backward compatibility with petsc_hdf5 format version 1.0.0, serial idempotence
  testset:
    suffix: 10-v3.16.0-v1.0.0
    requires: hdf5 !complex datafilespath
    args: -dm_plex_check_all -compare -compare_labels
    args: -dm_plex_view_hdf5_storage_version {{1.0.0 2.0.0 3.0.0}} -use_low_level_functions {{0 1}}
    test:
      suffix: a
      args: -filename ${DATAFILESPATH}/meshes/hdf5-petsc/petsc-v3.16.0/v1.0.0/annulus-20.h5
    test:
      suffix: b
      TODO: broken
      args: -filename ${DATAFILESPATH}/meshes/hdf5-petsc/petsc-v3.16.0/v1.0.0/barycentricallyrefinedcube.h5
    test:
      suffix: c
      args: -filename ${DATAFILESPATH}/meshes/hdf5-petsc/petsc-v3.16.0/v1.0.0/blockcylinder-50.h5
    test:
      suffix: d
      args: -filename ${DATAFILESPATH}/meshes/hdf5-petsc/petsc-v3.16.0/v1.0.0/cube-hexahedra-refined.h5
    test:
      suffix: e
      args: -filename ${DATAFILESPATH}/meshes/hdf5-petsc/petsc-v3.16.0/v1.0.0/hybrid_hexwedge.h5
    test:
      suffix: f
      args: -filename ${DATAFILESPATH}/meshes/hdf5-petsc/petsc-v3.16.0/v1.0.0/square.h5

TEST*/

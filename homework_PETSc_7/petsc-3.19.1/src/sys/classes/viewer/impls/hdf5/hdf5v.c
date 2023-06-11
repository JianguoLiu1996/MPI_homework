#include <petsc/private/viewerhdf5impl.h> /*I "petscviewerhdf5.h" I*/

static PetscErrorCode PetscViewerHDF5Traverse_Internal(PetscViewer, const char[], PetscBool, PetscBool *, H5O_type_t *);
static PetscErrorCode PetscViewerHDF5HasAttribute_Internal(PetscViewer, const char[], const char[], PetscBool *);

/*@C
  PetscViewerHDF5GetGroup - Get the current HDF5 group name (full path), set with `PetscViewerHDF5PushGroup()`/`PetscViewerHDF5PopGroup()`.

  Not Collective

  Input Parameters:
+ viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`
- path - (Optional) The path relative to the pushed group

  Output Parameter:
. abspath - The absolute HDF5 path (group)

  Level: intermediate

  Notes:
  If path starts with '/', it is taken as an absolute path overriding currently pushed group, else path is relative to the current pushed group.
  `NULL` or empty path means the current pushed group.

  The output abspath is newly allocated so needs to be freed.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5OpenGroup()`, `PetscViewerHDF5WriteGroup()`
@*/
PetscErrorCode PetscViewerHDF5GetGroup(PetscViewer viewer, const char path[], char *abspath[])
{
  size_t      len;
  PetscBool   relative = PETSC_FALSE;
  const char *group;
  char        buf[PETSC_MAX_PATH_LEN] = "";

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (path) PetscValidCharPointer(path, 2);
  PetscValidPointer(abspath, 3);
  PetscCall(PetscViewerHDF5GetGroup_Internal(viewer, &group));
  PetscCall(PetscStrlen(path, &len));
  relative = (PetscBool)(!len || path[0] != '/');
  if (relative) {
    PetscCall(PetscStrncpy(buf, group, sizeof(buf)));
    if (!group || len) PetscCall(PetscStrlcat(buf, "/", sizeof(buf)));
    PetscCall(PetscStrlcat(buf, path, sizeof(buf)));
    PetscCall(PetscStrallocpy(buf, abspath));
  } else {
    PetscCall(PetscStrallocpy(path, abspath));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5CheckNamedObject_Internal(PetscViewer viewer, PetscObject obj)
{
  PetscBool has;

  PetscFunctionBegin;
  PetscCall(PetscViewerHDF5HasObject(viewer, obj, &has));
  if (!has) {
    char *group;
    PetscCall(PetscViewerHDF5GetGroup(viewer, NULL, &group));
    SETERRQ(PetscObjectComm((PetscObject)viewer), PETSC_ERR_FILE_UNEXPECTED, "Object (dataset) \"%s\" not stored in group %s", obj->name, group);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerSetFromOptions_HDF5(PetscViewer v, PetscOptionItems *PetscOptionsObject)
{
  PetscBool         flg  = PETSC_FALSE, set;
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)v->data;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "HDF5 PetscViewer Options");
  PetscCall(PetscOptionsBool("-viewer_hdf5_base_dimension2", "1d Vectors get 2 dimensions in HDF5", "PetscViewerHDF5SetBaseDimension2", hdf5->basedimension2, &hdf5->basedimension2, NULL));
  PetscCall(PetscOptionsBool("-viewer_hdf5_sp_output", "Force data to be written in single precision", "PetscViewerHDF5SetSPOutput", hdf5->spoutput, &hdf5->spoutput, NULL));
  PetscCall(PetscOptionsBool("-viewer_hdf5_collective", "Enable collective transfer mode", "PetscViewerHDF5SetCollective", flg, &flg, &set));
  if (set) PetscCall(PetscViewerHDF5SetCollective(v, flg));
  flg = PETSC_FALSE;
  PetscCall(PetscOptionsBool("-viewer_hdf5_default_timestepping", "Set default timestepping state", "PetscViewerHDF5SetDefaultTimestepping", flg, &flg, &set));
  if (set) PetscCall(PetscViewerHDF5SetDefaultTimestepping(v, flg));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerView_HDF5(PetscViewer v, PetscViewer viewer)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)v->data;
  PetscBool         flg;

  PetscFunctionBegin;
  if (hdf5->filename) PetscCall(PetscViewerASCIIPrintf(viewer, "Filename: %s\n", hdf5->filename));
  PetscCall(PetscViewerASCIIPrintf(viewer, "Vectors with blocksize 1 saved as 2D datasets: %s\n", PetscBools[hdf5->basedimension2]));
  PetscCall(PetscViewerASCIIPrintf(viewer, "Enforce single precision storage: %s\n", PetscBools[hdf5->spoutput]));
  PetscCall(PetscViewerHDF5GetCollective(v, &flg));
  PetscCall(PetscViewerASCIIPrintf(viewer, "MPI-IO transfer mode: %s\n", flg ? "collective" : "independent"));
  PetscCall(PetscViewerASCIIPrintf(viewer, "Default timestepping: %s\n", PetscBools[hdf5->defTimestepping]));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileClose_HDF5(PetscViewer viewer)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscCall(PetscFree(hdf5->filename));
  if (hdf5->file_id) PetscCallHDF5(H5Fclose, (hdf5->file_id));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFlush_HDF5(PetscViewer viewer)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  if (hdf5->file_id) PetscCallHDF5(H5Fflush, (hdf5->file_id, H5F_SCOPE_LOCAL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerDestroy_HDF5(PetscViewer viewer)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscCallHDF5(H5Pclose, (hdf5->dxpl_id));
  PetscCall(PetscViewerFileClose_HDF5(viewer));
  while (hdf5->groups) {
    PetscViewerHDF5GroupList *tmp = hdf5->groups->next;

    PetscCall(PetscFree(hdf5->groups->name));
    PetscCall(PetscFree(hdf5->groups));
    hdf5->groups = tmp;
  }
  PetscCall(PetscFree(hdf5));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileSetName_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileGetName_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileSetMode_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerFileGetMode_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerHDF5SetBaseDimension2_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerHDF5SetSPOutput_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerHDF5SetCollective_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerHDF5GetCollective_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerHDF5GetDefaultTimestepping_C", NULL));
  PetscCall(PetscObjectComposeFunction((PetscObject)viewer, "PetscViewerHDF5SetDefaultTimestepping_C", NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileSetMode_HDF5(PetscViewer viewer, PetscFileMode type)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  hdf5->btype = type;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileGetMode_HDF5(PetscViewer viewer, PetscFileMode *type)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  *type = hdf5->btype;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5SetBaseDimension2_HDF5(PetscViewer viewer, PetscBool flg)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  hdf5->basedimension2 = flg;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     PetscViewerHDF5SetBaseDimension2 - Vectors of 1 dimension (i.e. bs/dof is 1) will be saved in the HDF5 file with a
       dimension of 2.

    Logically Collective

  Input Parameters:
+  viewer - the `PetscViewer`; if it is a `PETSCVIEWERHDF5` then this command is ignored
-  flg - if `PETSC_TRUE` the vector will always have at least a dimension of 2 even if that first dimension is of size 1

  Options Database Key:
.  -viewer_hdf5_base_dimension2 - turns on (true) or off (false) using a dimension of 2 in the HDF5 file even if the bs/dof of the vector is 1

  Level: intermediate

  Note:
  Setting this option allegedly makes code that reads the HDF5 in easier since they do not have a "special case" of a bs/dof
  of one when the dimension is lower. Others think the option is crazy.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, PetscViewerFileSetMode()`, `PetscViewerCreate()`, `PetscViewerSetType()`, `PetscViewerBinaryOpen()`
@*/
PetscErrorCode PetscViewerHDF5SetBaseDimension2(PetscViewer viewer, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscTryMethod(viewer, "PetscViewerHDF5SetBaseDimension2_C", (PetscViewer, PetscBool), (viewer, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     PetscViewerHDF5GetBaseDimension2 - Vectors of 1 dimension (i.e. bs/dof is 1) will be saved in the HDF5 file with a
       dimension of 2.

    Logically Collective

  Input Parameter:
.  viewer - the `PetscViewer`, must be `PETSCVIEWERHDF5`

  Output Parameter:
.  flg - if `PETSC_TRUE` the vector will always have at least a dimension of 2 even if that first dimension is of size 1

  Level: intermediate

  Note:
  Setting this option allegedly makes code that reads the HDF5 in easier since they do not have a "special case" of a bs/dof
  of one when the dimension is lower. Others think the option is crazy.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerFileSetMode()`, `PetscViewerCreate()`, `PetscViewerSetType()`, `PetscViewerBinaryOpen()`
@*/
PetscErrorCode PetscViewerHDF5GetBaseDimension2(PetscViewer viewer, PetscBool *flg)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  *flg = hdf5->basedimension2;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5SetSPOutput_HDF5(PetscViewer viewer, PetscBool flg)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  hdf5->spoutput = flg;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     PetscViewerHDF5SetSPOutput - Data is written to disk in single precision even if PETSc is
       compiled with double precision `PetscReal`.

    Logically Collective

  Input Parameters:
+  viewer - the PetscViewer; if it is a `PETSCVIEWERHDF5` then this command is ignored
-  flg - if `PETSC_TRUE` the data will be written to disk with single precision

  Options Database Key:
.  -viewer_hdf5_sp_output - turns on (true) or off (false) output in single precision

  Level: intermediate

  Note:
    Setting this option does not make any difference if PETSc is compiled with single precision
         in the first place. It does not affect reading datasets (HDF5 handle this internally).

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerFileSetMode()`, `PetscViewerCreate()`, `PetscViewerSetType()`, `PetscViewerBinaryOpen()`,
          `PetscReal`, `PetscViewerHDF5GetSPOutput()`
@*/
PetscErrorCode PetscViewerHDF5SetSPOutput(PetscViewer viewer, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscTryMethod(viewer, "PetscViewerHDF5SetSPOutput_C", (PetscViewer, PetscBool), (viewer, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
     PetscViewerHDF5GetSPOutput - Data is written to disk in single precision even if PETSc is
       compiled with double precision `PetscReal`.

    Logically Collective

  Input Parameter:
.  viewer - the PetscViewer, must be of type `PETSCVIEWERHDF5`

  Output Parameter:
.  flg - if `PETSC_TRUE` the data will be written to disk with single precision

  Level: intermediate

.seealso: [](sec_viewers), `PetscViewerFileSetMode()`, `PetscViewerCreate()`, `PetscViewerSetType()`, `PetscViewerBinaryOpen()`,
          `PetscReal`, `PetscViewerHDF5SetSPOutput()`
@*/
PetscErrorCode PetscViewerHDF5GetSPOutput(PetscViewer viewer, PetscBool *flg)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  *flg = hdf5->spoutput;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5SetCollective_HDF5(PetscViewer viewer, PetscBool flg)
{
  PetscFunctionBegin;
  /* H5FD_MPIO_COLLECTIVE is wrong in hdf5 1.10.2, and is the same as H5FD_MPIO_INDEPENDENT in earlier versions
     - see e.g. https://gitlab.cosma.dur.ac.uk/swift/swiftsim/issues/431 */
#if H5_VERSION_GE(1, 10, 3) && defined(H5_HAVE_PARALLEL)
  {
    PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;
    PetscCallHDF5(H5Pset_dxpl_mpio, (hdf5->dxpl_id, flg ? H5FD_MPIO_COLLECTIVE : H5FD_MPIO_INDEPENDENT));
  }
#else
  if (flg) PetscCall(PetscPrintf(PetscObjectComm((PetscObject)viewer), "Warning: PetscViewerHDF5SetCollective(viewer,PETSC_TRUE) is ignored for HDF5 versions prior to 1.10.3 or if built without MPI support\n"));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5SetCollective - Use collective MPI-IO transfer mode for HDF5 reads and writes.

  Logically Collective; flg must contain common value

  Input Parameters:
+ viewer - the `PetscViewer`; if it is not `PETSCVIEWERHDF5` then this command is ignored
- flg - `PETSC_TRUE` for collective mode; `PETSC_FALSE` for independent mode (default)

  Options Database Key:
. -viewer_hdf5_collective - turns on (true) or off (false) collective transfers

  Level: intermediate

  Note:
  Collective mode gives the MPI-IO layer underneath HDF5 a chance to do some additional collective optimizations and hence can perform better.
  However, this works correctly only since HDF5 1.10.3 and if HDF5 is installed for MPI; hence, we ignore this setting for older versions.

  Developer Note:
  In the HDF5 layer, `PETSC_TRUE` / `PETSC_FALSE` means `H5Pset_dxpl_mpio()` is called with `H5FD_MPIO_COLLECTIVE` / `H5FD_MPIO_INDEPENDENT`, respectively.
  This in turn means use of MPI_File_{read,write}_all /  MPI_File_{read,write} in the MPI-IO layer, respectively.
  See HDF5 documentation and MPI-IO documentation for details.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5GetCollective()`, `PetscViewerCreate()`, `PetscViewerSetType()`, `PetscViewerHDF5Open()`
@*/
PetscErrorCode PetscViewerHDF5SetCollective(PetscViewer viewer, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidLogicalCollectiveBool(viewer, flg, 2);
  PetscTryMethod(viewer, "PetscViewerHDF5SetCollective_C", (PetscViewer, PetscBool), (viewer, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5GetCollective_HDF5(PetscViewer viewer, PetscBool *flg)
{
#if defined(H5_HAVE_PARALLEL)
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;
  H5FD_mpio_xfer_t  mode;
#endif

  PetscFunctionBegin;
#if !defined(H5_HAVE_PARALLEL)
  *flg = PETSC_FALSE;
#else
  PetscCallHDF5(H5Pget_dxpl_mpio, (hdf5->dxpl_id, &mode));
  *flg = (mode == H5FD_MPIO_COLLECTIVE) ? PETSC_TRUE : PETSC_FALSE;
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5GetCollective - Return flag whether collective MPI-IO transfer mode is used for HDF5 reads and writes.

  Not Collective

  Input Parameter:
. viewer - the `PETSCVIEWERHDF5` `PetscViewer`

  Output Parameter:
. flg - the flag

  Level: intermediate

  Note:
  This setting works correctly only since HDF5 1.10.3 and if HDF5 was installed for MPI. For older versions, `PETSC_FALSE` will be always returned.
  For more details, see `PetscViewerHDF5SetCollective()`.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5SetCollective()`, `PetscViewerCreate()`, `PetscViewerSetType()`, `PetscViewerHDF5Open()`
@*/
PetscErrorCode PetscViewerHDF5GetCollective(PetscViewer viewer, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidBoolPointer(flg, 2);

  PetscUseMethod(viewer, "PetscViewerHDF5GetCollective_C", (PetscViewer, PetscBool *), (viewer, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileSetName_HDF5(PetscViewer viewer, const char name[])
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;
  hid_t             plist_id;

  PetscFunctionBegin;
  if (hdf5->file_id) PetscCallHDF5(H5Fclose, (hdf5->file_id));
  if (hdf5->filename) PetscCall(PetscFree(hdf5->filename));
  PetscCall(PetscStrallocpy(name, &hdf5->filename));
  /* Set up file access property list with parallel I/O access */
  PetscCallHDF5Return(plist_id, H5Pcreate, (H5P_FILE_ACCESS));
#if defined(H5_HAVE_PARALLEL)
  PetscCallHDF5(H5Pset_fapl_mpio, (plist_id, PetscObjectComm((PetscObject)viewer), MPI_INFO_NULL));
#endif
  /* Create or open the file collectively */
  switch (hdf5->btype) {
  case FILE_MODE_READ:
    if (PetscDefined(USE_DEBUG)) {
      PetscMPIInt rank;
      PetscBool   flg;

      PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)viewer), &rank));
      if (rank == 0) {
        PetscCall(PetscTestFile(hdf5->filename, 'r', &flg));
        PetscCheck(flg, PETSC_COMM_SELF, PETSC_ERR_FILE_OPEN, "File %s requested for reading does not exist", hdf5->filename);
      }
      PetscCallMPI(MPI_Barrier(PetscObjectComm((PetscObject)viewer)));
    }
    PetscCallHDF5Return(hdf5->file_id, H5Fopen, (name, H5F_ACC_RDONLY, plist_id));
    break;
  case FILE_MODE_APPEND:
  case FILE_MODE_UPDATE: {
    PetscBool flg;
    PetscCall(PetscTestFile(hdf5->filename, 'r', &flg));
    if (flg) PetscCallHDF5Return(hdf5->file_id, H5Fopen, (name, H5F_ACC_RDWR, plist_id));
    else PetscCallHDF5Return(hdf5->file_id, H5Fcreate, (name, H5F_ACC_EXCL, H5P_DEFAULT, plist_id));
    break;
  }
  case FILE_MODE_WRITE:
    PetscCallHDF5Return(hdf5->file_id, H5Fcreate, (name, H5F_ACC_TRUNC, H5P_DEFAULT, plist_id));
    break;
  case FILE_MODE_UNDEFINED:
    SETERRQ(PetscObjectComm((PetscObject)viewer), PETSC_ERR_ORDER, "Must call PetscViewerFileSetMode() before PetscViewerFileSetName()");
  default:
    SETERRQ(PetscObjectComm((PetscObject)viewer), PETSC_ERR_SUP, "Unsupported file mode %s", PetscFileModes[hdf5->btype]);
  }
  PetscCheck(hdf5->file_id >= 0, PETSC_COMM_SELF, PETSC_ERR_LIB, "H5Fcreate failed for %s", name);
  PetscCallHDF5(H5Pclose, (plist_id));
  PetscCall(PetscViewerHDF5ResetAttachedDMPlexStorageVersion(viewer));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFileGetName_HDF5(PetscViewer viewer, const char **name)
{
  PetscViewer_HDF5 *vhdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  *name = vhdf5->filename;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerSetUp_HDF5(PetscViewer viewer)
{
  /*
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5*) viewer->data;
  */

  PetscFunctionBegin;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5SetDefaultTimestepping_HDF5(PetscViewer viewer, PetscBool flg)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  hdf5->defTimestepping = flg;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5GetDefaultTimestepping_HDF5(PetscViewer viewer, PetscBool *flg)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  *flg = hdf5->defTimestepping;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5SetDefaultTimestepping - Set the flag for default timestepping

  Logically Collective

  Input Parameters:
+ viewer - the `PetscViewer`; if it is not `PETSCVIEWERHDF5` then this command is ignored
- flg    - if `PETSC_TRUE` we will assume that timestepping is on

  Options Database Key:
. -viewer_hdf5_default_timestepping - turns on (true) or off (false) default timestepping

  Level: intermediate

  Note:
  If the timestepping attribute is not found for an object, then the default timestepping is used

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5GetDefaultTimestepping()`, `PetscViewerHDF5PushTimestepping()`, `PetscViewerHDF5GetTimestep()`
@*/
PetscErrorCode PetscViewerHDF5SetDefaultTimestepping(PetscViewer viewer, PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscTryMethod(viewer, "PetscViewerHDF5SetDefaultTimestepping_C", (PetscViewer, PetscBool), (viewer, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5GetDefaultTimestepping - Get the flag for default timestepping

  Not Collective

  Input Parameter:
. viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`

  Output Parameter:
. flg    - if `PETSC_TRUE` we will assume that timestepping is on

  Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5SetDefaultTimestepping()`, `PetscViewerHDF5PushTimestepping()`, `PetscViewerHDF5GetTimestep()`
@*/
PetscErrorCode PetscViewerHDF5GetDefaultTimestepping(PetscViewer viewer, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscUseMethod(viewer, "PetscViewerHDF5GetDefaultTimestepping_C", (PetscViewer, PetscBool *), (viewer, flg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PETSCVIEWERHDF5 - A viewer that writes to an HDF5 file

  Level: beginner

.seealso: [](sec_viewers), `PetscViewerHDF5Open()`, `PetscViewerStringSPrintf()`, `PetscViewerSocketOpen()`, `PetscViewerDrawOpen()`, `PETSCVIEWERSOCKET`,
          `PetscViewerCreate()`, `PetscViewerASCIIOpen()`, `PetscViewerBinaryOpen()`, `PETSCVIEWERBINARY`, `PETSCVIEWERDRAW`, `PETSCVIEWERSTRING`,
          `PetscViewerMatlabOpen()`, `VecView()`, `DMView()`, `PetscViewerMatlabPutArray()`, `PETSCVIEWERASCII`, `PETSCVIEWERMATLAB`,
          `PetscViewerFileSetName()`, `PetscViewerFileSetMode()`, `PetscViewerFormat`, `PetscViewerType`, `PetscViewerSetType()`
M*/

PETSC_EXTERN PetscErrorCode PetscViewerCreate_HDF5(PetscViewer v)
{
  PetscViewer_HDF5 *hdf5;

  PetscFunctionBegin;
#if !defined(H5_HAVE_PARALLEL)
  {
    PetscMPIInt size;
    PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)v), &size));
    PetscCheck(size <= 1, PetscObjectComm((PetscObject)v), PETSC_ERR_SUP, "Cannot use parallel HDF5 viewer since the given HDF5 does not support parallel I/O (H5_HAVE_PARALLEL is unset)");
  }
#endif

  PetscCall(PetscNew(&hdf5));

  v->data                = (void *)hdf5;
  v->ops->destroy        = PetscViewerDestroy_HDF5;
  v->ops->setfromoptions = PetscViewerSetFromOptions_HDF5;
  v->ops->setup          = PetscViewerSetUp_HDF5;
  v->ops->view           = PetscViewerView_HDF5;
  v->ops->flush          = PetscViewerFlush_HDF5;
  hdf5->btype            = FILE_MODE_UNDEFINED;
  hdf5->filename         = NULL;
  hdf5->timestep         = -1;
  hdf5->groups           = NULL;

  PetscCallHDF5Return(hdf5->dxpl_id, H5Pcreate, (H5P_DATASET_XFER));

  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileSetName_C", PetscViewerFileSetName_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileGetName_C", PetscViewerFileGetName_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileSetMode_C", PetscViewerFileSetMode_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerFileGetMode_C", PetscViewerFileGetMode_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerHDF5SetBaseDimension2_C", PetscViewerHDF5SetBaseDimension2_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerHDF5SetSPOutput_C", PetscViewerHDF5SetSPOutput_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerHDF5SetCollective_C", PetscViewerHDF5SetCollective_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerHDF5GetCollective_C", PetscViewerHDF5GetCollective_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerHDF5GetDefaultTimestepping_C", PetscViewerHDF5GetDefaultTimestepping_HDF5));
  PetscCall(PetscObjectComposeFunction((PetscObject)v, "PetscViewerHDF5SetDefaultTimestepping_C", PetscViewerHDF5SetDefaultTimestepping_HDF5));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscViewerHDF5Open - Opens a file for HDF5 input/output as a `PETSCVIEWERHDF5` `PetscViewer`

   Collective

   Input Parameters:
+  comm - MPI communicator
.  name - name of file
-  type - type of file

   Output Parameter:
.  hdf5v - `PetscViewer` for HDF5 input/output to use with the specified file

  Options Database Keys:
+  -viewer_hdf5_base_dimension2 - turns on (true) or off (false) using a dimension of 2 in the HDF5 file even if the bs/dof of the vector is 1
-  -viewer_hdf5_sp_output - forces (if true) the viewer to write data in single precision independent on the precision of PetscReal

   Level: beginner

   Notes:
   Reading is always available, regardless of the mode. Available modes are
.vb
  FILE_MODE_READ - open existing HDF5 file for read only access, fail if file does not exist [H5Fopen() with H5F_ACC_RDONLY]
  FILE_MODE_WRITE - if file exists, fully overwrite it, else create new HDF5 file [H5FcreateH5Fcreate() with H5F_ACC_TRUNC]
  FILE_MODE_APPEND - if file exists, keep existing contents [H5Fopen() with H5F_ACC_RDWR], else create new HDF5 file [H5FcreateH5Fcreate() with H5F_ACC_EXCL]
  FILE_MODE_UPDATE - same as FILE_MODE_APPEND
.ve

   In case of `FILE_MODE_APPEND` / `FILE_MODE_UPDATE`, any stored object (dataset, attribute) can be selectively overwritten if the same fully qualified name (/group/path/to/object) is specified.

   This PetscViewer should be destroyed with PetscViewerDestroy().

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerASCIIOpen()`, `PetscViewerPushFormat()`, `PetscViewerDestroy()`, `PetscViewerHDF5SetBaseDimension2()`,
          `PetscViewerHDF5SetSPOutput()`, `PetscViewerHDF5GetBaseDimension2()`, `VecView()`, `MatView()`, `VecLoad()`,
          `MatLoad()`, `PetscFileMode`, `PetscViewer`, `PetscViewerSetType()`, `PetscViewerFileSetMode()`, `PetscViewerFileSetName()`
@*/
PetscErrorCode PetscViewerHDF5Open(MPI_Comm comm, const char name[], PetscFileMode type, PetscViewer *hdf5v)
{
  PetscFunctionBegin;
  PetscCall(PetscViewerCreate(comm, hdf5v));
  PetscCall(PetscViewerSetType(*hdf5v, PETSCVIEWERHDF5));
  PetscCall(PetscViewerFileSetMode(*hdf5v, type));
  PetscCall(PetscViewerFileSetName(*hdf5v, name));
  PetscCall(PetscViewerSetFromOptions(*hdf5v));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscViewerHDF5GetFileId - Retrieve the file id, this file ID then can be used in direct HDF5 calls

  Not Collective

  Input Parameter:
. viewer - the `PetscViewer`

  Output Parameter:
. file_id - The file id

  Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`
@*/
PetscErrorCode PetscViewerHDF5GetFileId(PetscViewer viewer, hid_t *file_id)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (file_id) *file_id = hdf5->file_id;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscViewerHDF5PushGroup - Set the current HDF5 group for output

  Not Collective

  Input Parameters:
+ viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`
- name - The group name

  Level: intermediate

  Notes:
  This is designed to mnemonically resemble the Unix cd command.
.vb
  If name begins with '/', it is interpreted as an absolute path fully replacing current group, otherwise it is taken as relative to the current group.
  `NULL`, empty string, or any sequence of all slashes (e.g. "///") is interpreted as the root group "/".
  "." means the current group is pushed again.
.ve

  Example:
  Suppose the current group is "/a".
.vb
  If name is `NULL`, empty string, or a sequence of all slashes (e.g. "///"), then the new group will be "/".
  If name is ".", then the new group will be "/a".
  If name is "b", then the new group will be "/a/b".
  If name is "/b", then the new group will be "/b".
.ve

  Developer Note:
  The root group "/" is internally stored as `NULL`.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`, `PetscViewerHDF5OpenGroup()`, `PetscViewerHDF5WriteGroup()`
@*/
PetscErrorCode PetscViewerHDF5PushGroup(PetscViewer viewer, const char name[])
{
  PetscViewer_HDF5         *hdf5 = (PetscViewer_HDF5 *)viewer->data;
  PetscViewerHDF5GroupList *groupNode;
  size_t                    i, len;
  char                      buf[PETSC_MAX_PATH_LEN];
  const char               *gname;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (name) PetscValidCharPointer(name, 2);
  PetscCall(PetscStrlen(name, &len));
  gname = NULL;
  if (len) {
    if (len == 1 && name[0] == '.') {
      /* use current name */
      gname = (hdf5->groups && hdf5->groups->name) ? hdf5->groups->name : NULL;
    } else if (name[0] == '/') {
      /* absolute */
      for (i = 1; i < len; i++) {
        if (name[i] != '/') {
          gname = name;
          break;
        }
      }
    } else {
      /* relative */
      const char *parent = (hdf5->groups && hdf5->groups->name) ? hdf5->groups->name : "";
      PetscCall(PetscSNPrintf(buf, sizeof(buf), "%s/%s", parent, name));
      gname = buf;
    }
  }
  PetscCall(PetscNew(&groupNode));
  PetscCall(PetscStrallocpy(gname, (char **)&groupNode->name));
  groupNode->next = hdf5->groups;
  hdf5->groups    = groupNode;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5PopGroup - Return the current HDF5 group for output to the previous value

  Not Collective

  Input Parameter:
. viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`

  Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5GetGroup()`, `PetscViewerHDF5OpenGroup()`, `PetscViewerHDF5WriteGroup()`
@*/
PetscErrorCode PetscViewerHDF5PopGroup(PetscViewer viewer)
{
  PetscViewer_HDF5         *hdf5 = (PetscViewer_HDF5 *)viewer->data;
  PetscViewerHDF5GroupList *groupNode;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCheck(hdf5->groups, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_WRONGSTATE, "HDF5 group stack is empty, cannot pop");
  groupNode    = hdf5->groups;
  hdf5->groups = hdf5->groups->next;
  PetscCall(PetscFree(groupNode->name));
  PetscCall(PetscFree(groupNode));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerHDF5GetGroup_Internal(PetscViewer viewer, const char *name[])
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidPointer(name, 2);
  if (hdf5->groups) *name = hdf5->groups->name;
  else *name = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscViewerHDF5OpenGroup - Open the HDF5 group with the name (full path) returned by `PetscViewerHDF5GetGroup()`,
  and return this group's ID and file ID.
  If `PetscViewerHDF5GetGroup()` yields NULL, then group ID is file ID.

  Not Collective

  Input Parameters:
+ viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`
- path - (Optional) The path relative to the pushed group

  Output Parameters:
+ fileId - The HDF5 file ID
- groupId - The HDF5 group ID

  Level: intermediate

  Note:
  If path starts with '/', it is taken as an absolute path overriding currently pushed group, else path is relative to the current pushed group.
  `NULL` or empty path means the current pushed group.

  If the viewer is writable, the group is created if it doesn't exist yet.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`, `PetscViewerHDF5WriteGroup()`
@*/
PetscErrorCode PetscViewerHDF5OpenGroup(PetscViewer viewer, const char path[], hid_t *fileId, hid_t *groupId)
{
  hid_t       file_id;
  H5O_type_t  type;
  const char *fileName  = NULL;
  char       *groupName = NULL;
  PetscBool   writable, has;

  PetscFunctionBegin;
  PetscCall(PetscViewerWritable(viewer, &writable));
  PetscCall(PetscViewerHDF5GetFileId(viewer, &file_id));
  PetscCall(PetscViewerFileGetName(viewer, &fileName));
  PetscCall(PetscViewerHDF5GetGroup(viewer, path, &groupName));
  PetscCall(PetscViewerHDF5Traverse_Internal(viewer, groupName, writable, &has, &type));
  if (!has) {
    PetscCheck(writable, PetscObjectComm((PetscObject)viewer), PETSC_ERR_FILE_UNEXPECTED, "Group %s does not exist and file %s is not open for writing", groupName, fileName);
    SETERRQ(PetscObjectComm((PetscObject)viewer), PETSC_ERR_LIB, "HDF5 failed to create group %s although file %s is open for writing", groupName, fileName);
  }
  PetscCheck(type == H5O_TYPE_GROUP, PetscObjectComm((PetscObject)viewer), PETSC_ERR_FILE_UNEXPECTED, "Path %s in file %s resolves to something which is not a group", groupName, fileName);
  PetscCallHDF5Return(*groupId, H5Gopen2, (file_id, groupName, H5P_DEFAULT));
  PetscCall(PetscFree(groupName));
  *fileId = file_id;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscViewerHDF5WriteGroup - Ensure the HDF5 group exists in the HDF5 file

  Not Collective

  Input Parameters:
+ viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`
- path - (Optional) The path relative to the pushed group

  Level: intermediate

  Note:
  If path starts with '/', it is taken as an absolute path overriding currently pushed group, else path is relative to the current pushed group.
  `NULL` or empty path means the current pushed group.

  This will fail if the viewer is not writable.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`, `PetscViewerHDF5OpenGroup()`
@*/
PetscErrorCode PetscViewerHDF5WriteGroup(PetscViewer viewer, const char path[])
{
  hid_t fileId, groupId;

  PetscFunctionBegin;
  PetscCall(PetscViewerHDF5OpenGroup(viewer, path, &fileId, &groupId)); // make sure group is actually created
  PetscCallHDF5(H5Gclose, (groupId));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5PushTimestepping - Activate timestepping mode for subsequent HDF5 reading and writing.

  Not Collective

  Input Parameter:
. viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`

  Level: intermediate

  Notes:
  On first `PetscViewerHDF5PushTimestepping()`, the initial time step is set to 0.
  Next timesteps can then be set using `PetscViewerHDF5IncrementTimestep()` or `PetscViewerHDF5SetTimestep()`.
  Current timestep value determines which timestep is read from or written to any dataset on the next HDF5 I/O operation [e.g. `VecView()`].
  Use `PetscViewerHDF5PopTimestepping()` to deactivate timestepping mode; calling it by the end of the program is NOT mandatory.
  Current timestep is remembered between `PetscViewerHDF5PopTimestepping()` and the next `PetscViewerHDF5PushTimestepping()`.

  If a dataset was stored with timestepping, it can be loaded only in the timestepping mode again.
  Loading a timestepped dataset with timestepping disabled, or vice-versa results in an error.

  Developer note:
  Timestepped HDF5 dataset has an extra dimension and attribute "timestepping" set to true.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PopTimestepping()`, `PetscViewerHDF5IsTimestepping()`, `PetscViewerHDF5SetTimestep()`, `PetscViewerHDF5IncrementTimestep()`, `PetscViewerHDF5GetTimestep()`
@*/
PetscErrorCode PetscViewerHDF5PushTimestepping(PetscViewer viewer)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCheck(!hdf5->timestepping, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_WRONGSTATE, "Timestepping is already pushed");
  hdf5->timestepping = PETSC_TRUE;
  if (hdf5->timestep < 0) hdf5->timestep = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5PopTimestepping - Deactivate timestepping mode for subsequent HDF5 reading and writing.

  Not Collective

  Input Parameter:
. viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`

  Level: intermediate

  Note:
  See `PetscViewerHDF5PushTimestepping()` for details.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushTimestepping()`, `PetscViewerHDF5IsTimestepping()`, `PetscViewerHDF5SetTimestep()`, `PetscViewerHDF5IncrementTimestep()`, `PetscViewerHDF5GetTimestep()`
@*/
PetscErrorCode PetscViewerHDF5PopTimestepping(PetscViewer viewer)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCheck(hdf5->timestepping, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_WRONGSTATE, "Timestepping has not been pushed yet. Call PetscViewerHDF5PushTimestepping() first");
  hdf5->timestepping = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5IsTimestepping - Ask the viewer whether it is in timestepping mode currently.

  Not Collective

  Input Parameter:
. viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`

  Output Parameter:
. flg - is timestepping active?

  Level: intermediate

  Note:
  See `PetscViewerHDF5PushTimestepping()` for details.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushTimestepping()`, `PetscViewerHDF5PopTimestepping()`, `PetscViewerHDF5SetTimestep()`, `PetscViewerHDF5IncrementTimestep()`, `PetscViewerHDF5GetTimestep()`
@*/
PetscErrorCode PetscViewerHDF5IsTimestepping(PetscViewer viewer, PetscBool *flg)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  *flg = hdf5->timestepping;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5IncrementTimestep - Increments current timestep for the HDF5 output. Fields are stacked in time.

  Not Collective

  Input Parameter:
. viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`

  Level: intermediate

  Note:
  This can be called only if the viewer is in timestepping mode. See `PetscViewerHDF5PushTimestepping()` for details.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushTimestepping()`, `PetscViewerHDF5SetTimestep()`, `PetscViewerHDF5GetTimestep()`
@*/
PetscErrorCode PetscViewerHDF5IncrementTimestep(PetscViewer viewer)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCheck(hdf5->timestepping, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_WRONGSTATE, "Timestepping has not been pushed yet. Call PetscViewerHDF5PushTimestepping() first");
  ++hdf5->timestep;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5SetTimestep - Set the current timestep for the HDF5 output. Fields are stacked in time.

  Logically Collective

  Input Parameters:
+ viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`
- timestep - The timestep

  Level: intermediate

  Note:
  This can be called only if the viewer is in timestepping mode. See `PetscViewerHDF5PushTimestepping()` for details.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushTimestepping()`, `PetscViewerHDF5IncrementTimestep()`, `PetscViewerHDF5GetTimestep()`
@*/
PetscErrorCode PetscViewerHDF5SetTimestep(PetscViewer viewer, PetscInt timestep)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidLogicalCollectiveInt(viewer, timestep, 2);
  PetscCheck(timestep >= 0, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_WRONGSTATE, "Timestep %" PetscInt_FMT " is negative", timestep);
  PetscCheck(hdf5->timestepping, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_WRONGSTATE, "Timestepping has not been pushed yet. Call PetscViewerHDF5PushTimestepping() first");
  hdf5->timestep = timestep;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
  PetscViewerHDF5GetTimestep - Get the current timestep for the HDF5 output. Fields are stacked in time.

  Not Collective

  Input Parameter:
. viewer - the `PetscViewer` of type `PETSCVIEWERHDF5`

  Output Parameter:
. timestep - The timestep

  Level: intermediate

  Note:
  This can be called only if the viewer is in the timestepping mode. See `PetscViewerHDF5PushTimestepping()` for details.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5PushTimestepping()`, `PetscViewerHDF5IncrementTimestep()`, `PetscViewerHDF5SetTimestep()`
@*/
PetscErrorCode PetscViewerHDF5GetTimestep(PetscViewer viewer, PetscInt *timestep)
{
  PetscViewer_HDF5 *hdf5 = (PetscViewer_HDF5 *)viewer->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidIntPointer(timestep, 2);
  PetscCheck(hdf5->timestepping, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_WRONGSTATE, "Timestepping has not been pushed yet. Call PetscViewerHDF5PushTimestepping() first");
  *timestep = hdf5->timestep;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscDataTypeToHDF5DataType - Converts the PETSc name of a datatype to its HDF5 name.

  Not Collective

  Input Parameter:
. ptype - the PETSc datatype name (for example `PETSC_DOUBLE`)

  Output Parameter:
. mtype - the HDF5  datatype

  Level: advanced

.seealso: [](sec_viewers), `PetscDataType`, `PetscHDF5DataTypeToPetscDataType()`
@*/
PetscErrorCode PetscDataTypeToHDF5DataType(PetscDataType ptype, hid_t *htype)
{
  PetscFunctionBegin;
  if (ptype == PETSC_INT)
#if defined(PETSC_USE_64BIT_INDICES)
    *htype = H5T_NATIVE_LLONG;
#else
    *htype = H5T_NATIVE_INT;
#endif
  else if (ptype == PETSC_DOUBLE) *htype = H5T_NATIVE_DOUBLE;
  else if (ptype == PETSC_LONG) *htype = H5T_NATIVE_LONG;
  else if (ptype == PETSC_SHORT) *htype = H5T_NATIVE_SHORT;
  else if (ptype == PETSC_ENUM) *htype = H5T_NATIVE_INT;
  else if (ptype == PETSC_BOOL) *htype = H5T_NATIVE_INT;
  else if (ptype == PETSC_FLOAT) *htype = H5T_NATIVE_FLOAT;
  else if (ptype == PETSC_CHAR) *htype = H5T_NATIVE_CHAR;
  else if (ptype == PETSC_BIT_LOGICAL) *htype = H5T_NATIVE_UCHAR;
  else if (ptype == PETSC_STRING) *htype = H5Tcopy(H5T_C_S1);
  else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Unsupported PETSc datatype");
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscHDF5DataTypeToPetscDataType - Finds the PETSc name of a datatype from its HDF5 name

  Not Collective

  Input Parameter:
. htype - the HDF5 datatype (for example `H5T_NATIVE_DOUBLE`, ...)

  Output Parameter:
. ptype - the PETSc datatype name (for example `PETSC_DOUBLE`)

  Level: advanced

.seealso: [](sec_viewers), `PetscDataType`, `PetscHDF5DataTypeToPetscDataType()`
@*/
PetscErrorCode PetscHDF5DataTypeToPetscDataType(hid_t htype, PetscDataType *ptype)
{
  PetscFunctionBegin;
#if defined(PETSC_USE_64BIT_INDICES)
  if (htype == H5T_NATIVE_INT) *ptype = PETSC_LONG;
  else if (htype == H5T_NATIVE_LLONG) *ptype = PETSC_INT;
#else
  if (htype == H5T_NATIVE_INT) *ptype = PETSC_INT;
#endif
  else if (htype == H5T_NATIVE_DOUBLE) *ptype = PETSC_DOUBLE;
  else if (htype == H5T_NATIVE_LONG) *ptype = PETSC_LONG;
  else if (htype == H5T_NATIVE_SHORT) *ptype = PETSC_SHORT;
  else if (htype == H5T_NATIVE_FLOAT) *ptype = PETSC_FLOAT;
  else if (htype == H5T_NATIVE_CHAR) *ptype = PETSC_CHAR;
  else if (htype == H5T_NATIVE_UCHAR) *ptype = PETSC_CHAR;
  else if (htype == H5T_C_S1) *ptype = PETSC_STRING;
  else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Unsupported HDF5 datatype");
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 PetscViewerHDF5WriteAttribute - Write an attribute

  Collective

  Input Parameters:
+ viewer - The `PETSCVIEWERHDF5` viewer
. parent - The parent dataset/group name
. name   - The attribute name
. datatype - The attribute type
- value    - The attribute value

  Level: advanced

  Note:
  If parent starts with '/', it is taken as an absolute path overriding currently pushed group, else parent is relative to the current pushed group. NULL means the current pushed group.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5WriteObjectAttribute()`, `PetscViewerHDF5ReadAttribute()`, `PetscViewerHDF5HasAttribute()`,
          `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`
@*/
PetscErrorCode PetscViewerHDF5WriteAttribute(PetscViewer viewer, const char parent[], const char name[], PetscDataType datatype, const void *value)
{
  char     *parentAbsPath;
  hid_t     h5, dataspace, obj, attribute, dtype;
  PetscBool has;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (parent) PetscValidCharPointer(parent, 2);
  PetscValidCharPointer(name, 3);
  PetscValidLogicalCollectiveEnum(viewer, datatype, 4);
  PetscValidPointer(value, 5);
  PetscCall(PetscViewerHDF5GetGroup(viewer, parent, &parentAbsPath));
  PetscCall(PetscViewerHDF5Traverse_Internal(viewer, parentAbsPath, PETSC_TRUE, NULL, NULL));
  PetscCall(PetscViewerHDF5HasAttribute_Internal(viewer, parentAbsPath, name, &has));
  PetscCall(PetscDataTypeToHDF5DataType(datatype, &dtype));
  if (datatype == PETSC_STRING) {
    size_t len;
    PetscCall(PetscStrlen((const char *)value, &len));
    PetscCallHDF5(H5Tset_size, (dtype, len + 1));
  }
  PetscCall(PetscViewerHDF5GetFileId(viewer, &h5));
  PetscCallHDF5Return(dataspace, H5Screate, (H5S_SCALAR));
  PetscCallHDF5Return(obj, H5Oopen, (h5, parentAbsPath, H5P_DEFAULT));
  if (has) {
    PetscCallHDF5Return(attribute, H5Aopen_name, (obj, name));
  } else {
    PetscCallHDF5Return(attribute, H5Acreate2, (obj, name, dtype, dataspace, H5P_DEFAULT, H5P_DEFAULT));
  }
  PetscCallHDF5(H5Awrite, (attribute, dtype, value));
  if (datatype == PETSC_STRING) PetscCallHDF5(H5Tclose, (dtype));
  PetscCallHDF5(H5Aclose, (attribute));
  PetscCallHDF5(H5Oclose, (obj));
  PetscCallHDF5(H5Sclose, (dataspace));
  PetscCall(PetscFree(parentAbsPath));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 PetscViewerHDF5WriteObjectAttribute - Write an attribute to the dataset matching the given `PetscObject` by name

  Collective

  Input Parameters:
+ viewer   - The `PETSCVIEWERHDF5` viewer
. obj      - The object whose name is used to lookup the parent dataset, relative to the current group.
. name     - The attribute name
. datatype - The attribute type
- value    - The attribute value

  Level: advanced

  Note:
  This fails if the path current_group/object_name doesn't resolve to a dataset (the path doesn't exist or is not a dataset).
  You might want to check first if it does using `PetscViewerHDF5HasObject()`.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5WriteAttribute()`, `PetscViewerHDF5ReadObjectAttribute()`, `PetscViewerHDF5HasObjectAttribute()`,
          `PetscViewerHDF5HasObject()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`
@*/
PetscErrorCode PetscViewerHDF5WriteObjectAttribute(PetscViewer viewer, PetscObject obj, const char name[], PetscDataType datatype, const void *value)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidHeader(obj, 2);
  PetscValidCharPointer(name, 3);
  PetscValidPointer(value, 5);
  PetscCall(PetscViewerHDF5CheckNamedObject_Internal(viewer, obj));
  PetscCall(PetscViewerHDF5WriteAttribute(viewer, obj->name, name, datatype, value));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 PetscViewerHDF5ReadAttribute - Read an attribute

  Collective

  Input Parameters:
+ viewer - The `PETSCVIEWERHDF5` viewer
. parent - The parent dataset/group name
. name   - The attribute name
. datatype - The attribute type
- defaultValue - The pointer to the default value

  Output Parameter:
. value    - The pointer to the read HDF5 attribute value

  Level: advanced

  Notes:
  If defaultValue is `NULL` and the attribute is not found, an error occurs.

  If defaultValue is not `NULL` and the attribute is not found, `defaultValue` is copied to value.

  The pointers `defaultValue` and value can be the same; for instance
.vb
  flg = PETSC_FALSE;
  PetscCall(`PetscViewerHDF5ReadAttribute`(viewer,name,"attr",PETSC_BOOL,&flg,&flg));
.ve
  is valid, but make sure the default value is initialized.

  If the datatype is `PETSC_STRING`, the output string is newly allocated so one must `PetscFree()` it when no longer needed.

  If parent starts with '/', it is taken as an absolute path overriding currently pushed group, else parent is relative to the current pushed group. `NULL` means the current pushed group.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5ReadObjectAttribute()`, `PetscViewerHDF5WriteAttribute()`, `PetscViewerHDF5HasAttribute()`, `PetscViewerHDF5HasObject()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`
@*/
PetscErrorCode PetscViewerHDF5ReadAttribute(PetscViewer viewer, const char parent[], const char name[], PetscDataType datatype, const void *defaultValue, void *value)
{
  char     *parentAbsPath;
  hid_t     h5, obj, attribute, dtype;
  PetscBool has;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (parent) PetscValidCharPointer(parent, 2);
  PetscValidCharPointer(name, 3);
  if (defaultValue) PetscValidPointer(defaultValue, 5);
  PetscValidPointer(value, 6);
  PetscCall(PetscDataTypeToHDF5DataType(datatype, &dtype));
  PetscCall(PetscViewerHDF5GetGroup(viewer, parent, &parentAbsPath));
  PetscCall(PetscViewerHDF5Traverse_Internal(viewer, parentAbsPath, PETSC_FALSE, &has, NULL));
  if (has) PetscCall(PetscViewerHDF5HasAttribute_Internal(viewer, parentAbsPath, name, &has));
  if (!has) {
    if (defaultValue) {
      if (defaultValue != value) {
        if (datatype == PETSC_STRING) {
          PetscCall(PetscStrallocpy(*(char **)defaultValue, (char **)value));
        } else {
          size_t len;
          PetscCallHDF5ReturnNoCheck(len, H5Tget_size, (dtype));
          PetscCall(PetscMemcpy(value, defaultValue, len));
        }
      }
      PetscCall(PetscFree(parentAbsPath));
      PetscFunctionReturn(PETSC_SUCCESS);
    } else SETERRQ(PetscObjectComm((PetscObject)viewer), PETSC_ERR_FILE_UNEXPECTED, "Attribute %s/%s does not exist and default value not provided", parentAbsPath, name);
  }
  PetscCall(PetscViewerHDF5GetFileId(viewer, &h5));
  PetscCallHDF5Return(obj, H5Oopen, (h5, parentAbsPath, H5P_DEFAULT));
  PetscCallHDF5Return(attribute, H5Aopen_name, (obj, name));
  if (datatype == PETSC_STRING) {
    size_t len;
    hid_t  atype;
    PetscCallHDF5Return(atype, H5Aget_type, (attribute));
    PetscCallHDF5ReturnNoCheck(len, H5Tget_size, (atype));
    PetscCall(PetscMalloc((len + 1) * sizeof(char), value));
    PetscCallHDF5(H5Tset_size, (dtype, len + 1));
    PetscCallHDF5(H5Aread, (attribute, dtype, *(char **)value));
  } else {
    PetscCallHDF5(H5Aread, (attribute, dtype, value));
  }
  PetscCallHDF5(H5Aclose, (attribute));
  /* H5Oclose can be used to close groups, datasets, or committed datatypes */
  PetscCallHDF5(H5Oclose, (obj));
  PetscCall(PetscFree(parentAbsPath));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 PetscViewerHDF5ReadObjectAttribute - Read an attribute from the dataset matching the given `PetscObject` by name

  Collective

  Input Parameters:
+ viewer   - The `PETSCVIEWERHDF5` viewer
. obj      - The object whose name is used to lookup the parent dataset, relative to the current group.
. name     - The attribute name
- datatype - The attribute type

  Output Parameter:
. value    - The attribute value

  Level: advanced

  Note:
  This fails if current_group/object_name doesn't resolve to a dataset (the path doesn't exist or is not a dataset).
  You might want to check first if it does using `PetscViewerHDF5HasObject()`.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5ReadAttribute()` `PetscViewerHDF5WriteObjectAttribute()`, `PetscViewerHDF5HasObjectAttribute()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`
@*/
PetscErrorCode PetscViewerHDF5ReadObjectAttribute(PetscViewer viewer, PetscObject obj, const char name[], PetscDataType datatype, void *defaultValue, void *value)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidHeader(obj, 2);
  PetscValidCharPointer(name, 3);
  PetscValidPointer(value, 6);
  PetscCall(PetscViewerHDF5CheckNamedObject_Internal(viewer, obj));
  PetscCall(PetscViewerHDF5ReadAttribute(viewer, obj->name, name, datatype, defaultValue, value));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscViewerHDF5Traverse_Inner_Internal(hid_t h5, const char name[], PetscBool createGroup, PetscBool *exists_)
{
  htri_t exists;
  hid_t  group;

  PetscFunctionBegin;
  PetscCallHDF5Return(exists, H5Lexists, (h5, name, H5P_DEFAULT));
  if (exists) PetscCallHDF5Return(exists, H5Oexists_by_name, (h5, name, H5P_DEFAULT));
  if (!exists && createGroup) {
    PetscCallHDF5Return(group, H5Gcreate2, (h5, name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    PetscCallHDF5(H5Gclose, (group));
    exists = PETSC_TRUE;
  }
  *exists_ = (PetscBool)exists;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5Traverse_Internal(PetscViewer viewer, const char name[], PetscBool createGroup, PetscBool *has, H5O_type_t *otype)
{
  const char rootGroupName[] = "/";
  hid_t      h5;
  PetscBool  exists = PETSC_FALSE;
  PetscInt   i;
  int        n;
  char     **hierarchy;
  char       buf[PETSC_MAX_PATH_LEN] = "";

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (name) PetscValidCharPointer(name, 2);
  else name = rootGroupName;
  if (has) {
    PetscValidBoolPointer(has, 4);
    *has = PETSC_FALSE;
  }
  if (otype) {
    PetscValidIntPointer(otype, 5);
    *otype = H5O_TYPE_UNKNOWN;
  }
  PetscCall(PetscViewerHDF5GetFileId(viewer, &h5));

  /*
     Unfortunately, H5Oexists_by_name() fails if any object in hierarchy is missing.
     Hence, each of them needs to be tested separately:
     1) whether it's a valid link
     2) whether this link resolves to an object
     See H5Oexists_by_name() documentation.
  */
  PetscCall(PetscStrToArray(name, '/', &n, &hierarchy));
  if (!n) {
    /*  Assume group "/" always exists in accordance with HDF5 >= 1.10.0. See H5Lexists() documentation. */
    if (has) *has = PETSC_TRUE;
    if (otype) *otype = H5O_TYPE_GROUP;
    PetscCall(PetscStrToArrayDestroy(n, hierarchy));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  for (i = 0; i < n; i++) {
    PetscCall(PetscStrlcat(buf, "/", sizeof(buf)));
    PetscCall(PetscStrlcat(buf, hierarchy[i], sizeof(buf)));
    PetscCall(PetscViewerHDF5Traverse_Inner_Internal(h5, buf, createGroup, &exists));
    if (!exists) break;
  }
  PetscCall(PetscStrToArrayDestroy(n, hierarchy));

  /* If the object exists, get its type */
  if (exists && otype) {
    H5O_info_t info;

    /* We could use H5Iget_type() here but that would require opening the object. This way we only need its name. */
    PetscCallHDF5(H5Oget_info_by_name, (h5, name, &info, H5P_DEFAULT));
    *otype = info.type;
  }
  if (has) *has = exists;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 PetscViewerHDF5HasGroup - Check whether the current (pushed) group exists in the HDF5 file

  Collective

  Input Parameters:
+ viewer - The `PETSCVIEWERHDF5` viewer
- path - (Optional) The path relative to the pushed group

  Output Parameter:
. has - Flag for group existence

  Level: advanced

  Notes:
  If path starts with '/', it is taken as an absolute path overriding currently pushed group, else path is relative to the current pushed group.
  `NULL` or empty path means the current pushed group.

  If path exists but is not a group, `PETSC_FALSE` is returned.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5HasAttribute()`, `PetscViewerHDF5HasDataset()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`, `PetscViewerHDF5OpenGroup()`
@*/
PetscErrorCode PetscViewerHDF5HasGroup(PetscViewer viewer, const char path[], PetscBool *has)
{
  H5O_type_t type;
  char      *abspath;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (path) PetscValidCharPointer(path, 2);
  PetscValidBoolPointer(has, 3);
  PetscCall(PetscViewerHDF5GetGroup(viewer, path, &abspath));
  PetscCall(PetscViewerHDF5Traverse_Internal(viewer, abspath, PETSC_FALSE, NULL, &type));
  *has = (PetscBool)(type == H5O_TYPE_GROUP);
  PetscCall(PetscFree(abspath));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 PetscViewerHDF5HasDataset - Check whether a given dataset exists in the HDF5 file

  Collective

  Input Parameters:
+ viewer - The `PETSCVIEWERHDF5` viewer
- path - The dataset path

  Output Parameter:
. has - Flag whether dataset exists

  Level: advanced

  Notes:
  If path starts with '/', it is taken as an absolute path overriding currently pushed group, else path is relative to the current pushed group.

  If `path` is `NULL` or empty, has is set to `PETSC_FALSE`.

  If `path` exists but is not a dataset, has is set to `PETSC_FALSE` as well.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5HasObject()`, `PetscViewerHDF5HasAttribute()`, `PetscViewerHDF5HasGroup()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`
@*/
PetscErrorCode PetscViewerHDF5HasDataset(PetscViewer viewer, const char path[], PetscBool *has)
{
  H5O_type_t type;
  char      *abspath;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (path) PetscValidCharPointer(path, 2);
  PetscValidBoolPointer(has, 3);
  PetscCall(PetscViewerHDF5GetGroup(viewer, path, &abspath));
  PetscCall(PetscViewerHDF5Traverse_Internal(viewer, abspath, PETSC_FALSE, NULL, &type));
  *has = (PetscBool)(type == H5O_TYPE_DATASET);
  PetscCall(PetscFree(abspath));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
 PetscViewerHDF5HasObject - Check whether a dataset with the same name as given object exists in the HDF5 file under current group

  Collective

  Input Parameters:
+ viewer - The `PETSCVIEWERHDF5` viewer
- obj    - The named object

  Output Parameter:
. has    - Flag for dataset existence

  Level: advanced

  Notes:
  If the object is unnamed, an error occurs.

  If the path current_group/object_name exists but is not a dataset, has is set to `PETSC_FALSE` as well.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5HasDataset()`, `PetscViewerHDF5HasAttribute()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`
@*/
PetscErrorCode PetscViewerHDF5HasObject(PetscViewer viewer, PetscObject obj, PetscBool *has)
{
  size_t len;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidHeader(obj, 2);
  PetscValidBoolPointer(has, 3);
  PetscCall(PetscStrlen(obj->name, &len));
  PetscCheck(len, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_WRONG, "Object must be named");
  PetscCall(PetscViewerHDF5HasDataset(viewer, obj->name, has));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 PetscViewerHDF5HasAttribute - Check whether an attribute exists

  Collective

  Input Parameters:
+ viewer - The `PETSCVIEWERHDF5` viewer
. parent - The parent dataset/group name
- name   - The attribute name

  Output Parameter:
. has    - Flag for attribute existence

  Level: advanced

  Note:
  If parent starts with '/', it is taken as an absolute path overriding currently pushed group, else parent is relative to the current pushed group. `NULL` means the current pushed group.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5HasObjectAttribute()`, `PetscViewerHDF5WriteAttribute()`, `PetscViewerHDF5ReadAttribute()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`
@*/
PetscErrorCode PetscViewerHDF5HasAttribute(PetscViewer viewer, const char parent[], const char name[], PetscBool *has)
{
  char *parentAbsPath;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  if (parent) PetscValidCharPointer(parent, 2);
  PetscValidCharPointer(name, 3);
  PetscValidBoolPointer(has, 4);
  PetscCall(PetscViewerHDF5GetGroup(viewer, parent, &parentAbsPath));
  PetscCall(PetscViewerHDF5Traverse_Internal(viewer, parentAbsPath, PETSC_FALSE, has, NULL));
  if (*has) PetscCall(PetscViewerHDF5HasAttribute_Internal(viewer, parentAbsPath, name, has));
  PetscCall(PetscFree(parentAbsPath));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
 PetscViewerHDF5HasObjectAttribute - Check whether an attribute is attached to the dataset matching the given `PetscObject` by name

  Collective

  Input Parameters:
+ viewer - The `PETSCVIEWERHDF5` viewer
. obj    - The object whose name is used to lookup the parent dataset, relative to the current group.
- name   - The attribute name

  Output Parameter:
. has    - Flag for attribute existence

  Level: advanced

  Note:
  This fails if current_group/object_name doesn't resolve to a dataset (the path doesn't exist or is not a dataset).
  You might want to check first if it does using `PetscViewerHDF5HasObject()`.

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerHDF5HasAttribute()`, `PetscViewerHDF5WriteObjectAttribute()`, `PetscViewerHDF5ReadObjectAttribute()`, `PetscViewerHDF5HasObject()`, `PetscViewerHDF5PushGroup()`, `PetscViewerHDF5PopGroup()`, `PetscViewerHDF5GetGroup()`
@*/
PetscErrorCode PetscViewerHDF5HasObjectAttribute(PetscViewer viewer, PetscObject obj, const char name[], PetscBool *has)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidHeader(obj, 2);
  PetscValidCharPointer(name, 3);
  PetscValidBoolPointer(has, 4);
  PetscCall(PetscViewerHDF5CheckNamedObject_Internal(viewer, obj));
  PetscCall(PetscViewerHDF5HasAttribute(viewer, obj->name, name, has));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerHDF5HasAttribute_Internal(PetscViewer viewer, const char parent[], const char name[], PetscBool *has)
{
  hid_t  h5;
  htri_t hhas;

  PetscFunctionBegin;
  PetscCall(PetscViewerHDF5GetFileId(viewer, &h5));
  PetscCallHDF5Return(hhas, H5Aexists_by_name, (h5, parent, name, H5P_DEFAULT));
  *has = hhas ? PETSC_TRUE : PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  The variable Petsc_Viewer_HDF5_keyval is used to indicate an MPI attribute that
  is attached to a communicator, in this case the attribute is a PetscViewer.
*/
PetscMPIInt Petsc_Viewer_HDF5_keyval = MPI_KEYVAL_INVALID;

/*@C
  PETSC_VIEWER_HDF5_ - Creates an `PETSCVIEWERHDF5` `PetscViewer` shared by all processors in a communicator.

  Collective

  Input Parameter:
. comm - the MPI communicator to share the `PETSCVIEWERHDF5` `PetscViewer`

  Options Database Key:
. -viewer_hdf5_filename <name> - name of the HDF5 file

  Environmental variable:
. `PETSC_VIEWER_HDF5_FILENAME` - name of the HDF5 file

  Level: intermediate

  Note:
  Unlike almost all other PETSc routines, `PETSC_VIEWER_HDF5_()` does not return
  an error code.  The HDF5 `PetscViewer` is usually used in the form
$       XXXView(XXX object, PETSC_VIEWER_HDF5_(comm));

.seealso: [](sec_viewers), `PETSCVIEWERHDF5`, `PetscViewerHDF5Open()`, `PetscViewerCreate()`, `PetscViewerDestroy()`
@*/
PetscViewer PETSC_VIEWER_HDF5_(MPI_Comm comm)
{
  PetscErrorCode ierr;
  PetscMPIInt    mpi_ierr;
  PetscBool      flg;
  PetscViewer    viewer;
  char           fname[PETSC_MAX_PATH_LEN];
  MPI_Comm       ncomm;

  PetscFunctionBegin;
  ierr = PetscCommDuplicate(comm, &ncomm, NULL);
  if (ierr) {
    ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
    PetscFunctionReturn(NULL);
  }
  if (Petsc_Viewer_HDF5_keyval == MPI_KEYVAL_INVALID) {
    mpi_ierr = MPI_Comm_create_keyval(MPI_COMM_NULL_COPY_FN, MPI_COMM_NULL_DELETE_FN, &Petsc_Viewer_HDF5_keyval, NULL);
    if (mpi_ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
      PetscFunctionReturn(NULL);
    }
  }
  mpi_ierr = MPI_Comm_get_attr(ncomm, Petsc_Viewer_HDF5_keyval, (void **)&viewer, (int *)&flg);
  if (mpi_ierr) {
    ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
    PetscFunctionReturn(NULL);
  }
  if (!flg) { /* PetscViewer not yet created */
    ierr = PetscOptionsGetenv(ncomm, "PETSC_VIEWER_HDF5_FILENAME", fname, PETSC_MAX_PATH_LEN, &flg);
    if (ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
      PetscFunctionReturn(NULL);
    }
    if (!flg) {
      ierr = PetscStrncpy(fname, "output.h5", sizeof(fname));
      if (ierr) {
        ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
        PetscFunctionReturn(NULL);
      }
    }
    ierr = PetscViewerHDF5Open(ncomm, fname, FILE_MODE_WRITE, &viewer);
    if (ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
      PetscFunctionReturn(NULL);
    }
    ierr = PetscObjectRegisterDestroy((PetscObject)viewer);
    if (ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
      PetscFunctionReturn(NULL);
    }
    mpi_ierr = MPI_Comm_set_attr(ncomm, Petsc_Viewer_HDF5_keyval, (void *)viewer);
    if (mpi_ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
      PetscFunctionReturn(NULL);
    }
  }
  ierr = PetscCommDestroy(&ncomm);
  if (ierr) {
    ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_HDF5_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
    PetscFunctionReturn(NULL);
  }
  PetscFunctionReturn(viewer);
}


#include <petsc/private/petscimpl.h> /*I    "petscsys.h"   I*/
#include <petscviewer.h>

/*@C
   PetscObjectSetName - Sets a string name associated with a PETSc object.

   Not Collective

   Input Parameters:
+  obj - the Petsc variable
         Thus must be cast with a (`PetscObject`), for example,
         `PetscObjectSetName`((`PetscObject`)mat,name);
-  name - the name to give obj

   Level: advanced

   Note:
    If this routine is not called then the object may end up being name by `PetscObjectName()`.

.seealso: `PetscObjectGetName()`, `PetscObjectName()`
@*/
PetscErrorCode PetscObjectSetName(PetscObject obj, const char name[])
{
  PetscFunctionBegin;
  PetscValidHeader(obj, 1);
  PetscCall(PetscFree(obj->name));
  PetscCall(PetscStrallocpy(name, &obj->name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
      PetscObjectPrintClassNamePrefixType - used in the `XXXView()` methods to display information about the class, name, prefix and type of an object

   Input Parameters:
+     obj - the PETSc object
-     viewer - ASCII viewer where the information is printed, function does nothing if the viewer is not `PETSCVIEWERASCII` type

   Level: developer

   Notes:
   If the viewer format is `PETSC_VIEWER_ASCII_MATLAB` then the information is printed after a % symbol
   so that MATLAB will treat it as a comment.

   If the viewer format is `PETSC_VIEWER_ASCII_VTK*`, `PETSC_VIEWER_ASCII_LATEX`, or
   `PETSC_VIEWER_ASCII_MATRIXMARKET` then don't print header information
   as these formats can't process it.

   Developer Note:
   The flag donotPetscObjectPrintClassNamePrefixType is useful to prevent double printing of the information when recursion is used to actually print the object.

.seealso: `PetscObjectSetName()`, `PetscObjectName()`
@*/
PetscErrorCode PetscObjectPrintClassNamePrefixType(PetscObject obj, PetscViewer viewer)
{
  PetscMPIInt       size;
  PetscViewerFormat format;
  PetscBool         flg;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &flg));
  if (obj->donotPetscObjectPrintClassNamePrefixType) PetscFunctionReturn(PETSC_SUCCESS);
  if (!flg) PetscFunctionReturn(PETSC_SUCCESS);

  PetscCall(PetscViewerGetFormat(viewer, &format));
  if (format == PETSC_VIEWER_ASCII_VTK_DEPRECATED || format == PETSC_VIEWER_ASCII_VTK_CELL_DEPRECATED || format == PETSC_VIEWER_ASCII_VTK_COORDS_DEPRECATED || format == PETSC_VIEWER_ASCII_MATRIXMARKET || format == PETSC_VIEWER_ASCII_LATEX || format == PETSC_VIEWER_ASCII_GLVIS)
    PetscFunctionReturn(PETSC_SUCCESS);

  if (format == PETSC_VIEWER_ASCII_MATLAB) PetscCall(PetscViewerASCIIPrintf(viewer, "%%"));
  PetscCallMPI(MPI_Comm_size(PetscObjectComm(obj), &size));
  PetscCall(PetscViewerASCIIPrintf(viewer, "%s Object:%s%s%s%s%s %d MPI process%s\n", obj->class_name, obj->name ? " " : "", obj->name ? obj->name : "", obj->prefix ? " (" : "", obj->prefix ? obj->prefix : "", obj->prefix ? ")" : "", size, size > 1 ? "es" : ""));
  if (format == PETSC_VIEWER_ASCII_MATLAB) PetscCall(PetscViewerASCIIPrintf(viewer, "%%"));
  if (obj->type_name) {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  type: %s\n", obj->type_name));
  } else {
    PetscCall(PetscViewerASCIIPrintf(viewer, "  type not yet set\n"));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscObjectName - Gives an object a name if it does not have one

   Collective

   Input Parameter:
.  obj - the Petsc variable
         Thus must be cast with a (`PetscObject`), for example,
         `PetscObjectName`((`PetscObject`)mat,name);

   Level: developer

   Notes:
   This is used in a small number of places when an object NEEDS a name, for example when it is saved to MATLAB with that variable name.

   Use `PetscObjectSetName()` to set the name of an object to what you want. The SAWs viewer requires that no two published objects
   share the same name.

   Developer Note:
   This needs to generate the exact same string on all ranks that share the object. The current algorithm may not always work.

.seealso: `PetscObjectGetName()`, `PetscObjectSetName()`
@*/
PetscErrorCode PetscObjectName(PetscObject obj)
{
  PetscCommCounter *counter;
  PetscMPIInt       flg;
  char              name[64];

  PetscFunctionBegin;
  PetscValidHeader(obj, 1);
  if (!obj->name) {
    union
    {
      MPI_Comm comm;
      void    *ptr;
      char     raw[sizeof(MPI_Comm)];
    } ucomm;
    PetscCallMPI(MPI_Comm_get_attr(obj->comm, Petsc_Counter_keyval, (void *)&counter, &flg));
    PetscCheck(flg, PETSC_COMM_SELF, PETSC_ERR_ARG_CORRUPT, "Bad MPI communicator supplied; must be a PETSc communicator");
    ucomm.ptr  = NULL;
    ucomm.comm = obj->comm;
    PetscCallMPI(MPI_Bcast(ucomm.raw, sizeof(MPI_Comm), MPI_BYTE, 0, obj->comm));
    /* If the union has extra bytes, their value is implementation-dependent, but they will normally be what we set last
     * in 'ucomm.ptr = NULL'.  This output is always implementation-defined (and varies from run to run) so the union
     * abuse acceptable. */
    PetscCall(PetscSNPrintf(name, 64, "%s_%p_%" PetscInt_FMT, obj->class_name, ucomm.ptr, counter->namecount++));
    PetscCall(PetscStrallocpy(name, &obj->name));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscObjectChangeTypeName(PetscObject obj, const char type_name[])
{
  PetscFunctionBegin;
  PetscValidHeader(obj, 1);
  PetscCall(PetscFree(obj->type_name));
  PetscCall(PetscStrallocpy(type_name, &obj->type_name));
  /* Clear all the old subtype callbacks so they can't accidentally be called (shouldn't happen anyway) */
  PetscCall(PetscMemzero(obj->fortrancallback[PETSC_FORTRAN_CALLBACK_SUBTYPE], obj->num_fortrancallback[PETSC_FORTRAN_CALLBACK_SUBTYPE] * sizeof(PetscFortranCallback)));
  PetscFunctionReturn(PETSC_SUCCESS);
}

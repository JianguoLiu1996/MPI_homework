
#include <petsc/private/viewerimpl.h> /*I  "petscsys.h"  */
#include <petscviewersaws.h>

/*@C
    PetscViewerSAWsOpen - Opens an SAWs `PetscViewer`.

    Collective; No Fortran Support

    Input Parameter:
.   comm - the MPI communicator

    Output Parameter:
.   lab - the `PetscViewer`

    Options Database Keys:
+   -saws_port <port number> - port number where you are running SAWs client
.   -xxx_view saws - publish the object xxx
-   -xxx_saws_block - blocks the program at the end of a critical point (for `KSP` and `SNES` it is the end of a solve) until
                    the user unblocks the problem with an external tool that access the object with SAWS

    Level: advanced

    Notes:
    Unlike other viewers that only access the object being viewed on the call to XXXView(object,viewer) the SAWs viewer allows
    one to view the object asynchronously as the program continues to run. One can remove SAWs access to the object with a call to
    `PetscObjectSAWsViewOff()`.

    Information about the SAWs is available via https://bitbucket.org/saws/saws

.seealso: [](sec_viewers), `PetscViewerDestroy()`, `PetscViewerStringSPrintf()`, `PETSC_VIEWER_SAWS_()`, `PetscObjectSAWsBlock()`,
          `PetscObjectSAWsViewOff()`, `PetscObjectSAWsTakeAccess()`, `PetscObjectSAWsGrantAccess()`
@*/
PetscErrorCode PetscViewerSAWsOpen(MPI_Comm comm, PetscViewer *lab)
{
  PetscFunctionBegin;
  PetscCall(PetscViewerCreate(comm, lab));
  PetscCall(PetscViewerSetType(*lab, PETSCVIEWERSAWS));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscObjectViewSAWs - View the base portion of any object with an SAWs viewer

   Collective

   Input Parameters:
+  obj - the `PetscObject` variable. Thus must be cast with a (`PetscObject`), for example, `PetscObjectSetName`((`PetscObject`)mat,name);
-  viewer - the SAWs viewer

   Level: advanced

   Note:
   The object must have already been named before calling this routine since naming an
   object can be collective.

   Developer Note:
   Currently this is called only on rank zero of `PETSC_COMM_WORLD`

.seealso: [](sec_viewers), `PetscViewer`, `PetscObject`, `PetscObjectSetName()`, `PetscObjectSAWsViewOff()`
@*/
PetscErrorCode PetscObjectViewSAWs(PetscObject obj, PetscViewer viewer)
{
  char        dir[1024];
  PetscMPIInt rank;

  PetscFunctionBegin;
  PetscValidHeader(obj, 1);
  if (obj->amsmem) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCallMPI(MPI_Comm_rank(PETSC_COMM_WORLD, &rank));
  PetscCheck(rank == 0, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Should only be being called on rank zero");
  PetscCheck(obj->name, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Object must already have been named");

  obj->amsmem = PETSC_TRUE;
  PetscCall(PetscSNPrintf(dir, 1024, "/PETSc/Objects/%s/Class", obj->name));
  PetscCallSAWs(SAWs_Register, (dir, &obj->class_name, 1, SAWs_READ, SAWs_STRING));
  PetscCall(PetscSNPrintf(dir, 1024, "/PETSc/Objects/%s/Type", obj->name));
  PetscCallSAWs(SAWs_Register, (dir, &obj->type_name, 1, SAWs_READ, SAWs_STRING));
  PetscCall(PetscSNPrintf(dir, 1024, "/PETSc/Objects/%s/__Id", obj->name));
  PetscCallSAWs(SAWs_Register, (dir, &obj->id, 1, SAWs_READ, SAWs_INT));
  PetscFunctionReturn(PETSC_SUCCESS);
}


/*
       Provides the calling sequences for all the basic PetscDraw routines.
*/
#include <petsc/private/drawimpl.h> /*I "petscdraw.h" I*/

/*@
   PetscDrawSetCoordinates - Sets the application coordinates of the corners of
   the window (or page).

   Not Collective

   Input Parameters:
+  draw - the drawing object
-  xl,yl,xr,yr - the coordinates of the lower left corner and upper
                 right corner of the drawing region.

   Level: advanced

.seealso: `PetscDraw`, `PetscDrawGetCoordinates()`
@*/
PetscErrorCode PetscDrawSetCoordinates(PetscDraw draw, PetscReal xl, PetscReal yl, PetscReal xr, PetscReal yr)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  draw->coor_xl = xl;
  draw->coor_yl = yl;
  draw->coor_xr = xr;
  draw->coor_yr = yr;
  PetscTryTypeMethod(draw, setcoordinates, xl, yl, xr, yr);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawGetCoordinates - Gets the application coordinates of the corners of
   the window (or page).

   Not Collective

   Input Parameter:
.  draw - the drawing object

   Output Parameters:
+  xl - the horizontal coordinate of the lower left corner of the drawing region.
.  yl - the vertical coordinate of the lower left corner of the drawing region.
.  xr - the horizontal coordinate of the upper right corner of the drawing region.
-  yr - the vertical coordinate of the upper right corner of the drawing region.

   Level: advanced

.seealso: `PetscDraw`, `PetscDrawSetCoordinates()`
@*/
PetscErrorCode PetscDrawGetCoordinates(PetscDraw draw, PetscReal *xl, PetscReal *yl, PetscReal *xr, PetscReal *yr)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidRealPointer(xl, 2);
  PetscValidRealPointer(yl, 3);
  PetscValidRealPointer(xr, 4);
  PetscValidRealPointer(yr, 5);
  *xl = draw->coor_xl;
  *yl = draw->coor_yl;
  *xr = draw->coor_xr;
  *yr = draw->coor_yr;
  PetscFunctionReturn(PETSC_SUCCESS);
}

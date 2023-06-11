
#include <../src/sys/classes/viewer/impls/draw/vdraw.h> /*I "petscdraw.h" I*/
#include <petscviewer.h>                                /*I "petscviewer.h" I*/

static PetscErrorCode PetscViewerDestroy_Draw(PetscViewer v)
{
  PetscInt          i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw *)v->data;

  PetscFunctionBegin;
  PetscCheck(!vdraw->singleton_made, PETSC_COMM_SELF, PETSC_ERR_ORDER, "Destroying PetscViewer without first restoring singleton");
  for (i = 0; i < vdraw->draw_max; i++) {
    PetscCall(PetscDrawAxisDestroy(&vdraw->drawaxis[i]));
    PetscCall(PetscDrawLGDestroy(&vdraw->drawlg[i]));
    PetscCall(PetscDrawDestroy(&vdraw->draw[i]));
  }
  PetscCall(PetscFree(vdraw->display));
  PetscCall(PetscFree(vdraw->title));
  PetscCall(PetscFree3(vdraw->draw, vdraw->drawlg, vdraw->drawaxis));
  PetscCall(PetscFree(vdraw->bounds));
  PetscCall(PetscFree(vdraw->drawtype));
  PetscCall(PetscFree(v->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscViewerFlush_Draw(PetscViewer v)
{
  PetscInt          i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw *)v->data;

  PetscFunctionBegin;
  for (i = 0; i < vdraw->draw_max; i++) {
    if (vdraw->draw[i]) PetscCall(PetscDrawFlush(vdraw->draw[i]));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscViewerDrawGetDraw - Returns `PetscDraw` object from `PetscViewer` object.
    This `PetscDraw` object may then be used to perform graphics using
    `PetscDraw` commands.

    Collective

    Input Parameters:
+   viewer - the `PetscViewer` (created with `PetscViewerDrawOpen()` of type `PETSCVIEWERDRAW`)
-   windownumber - indicates which subwindow (usually 0)

    Output Parameter:
.   draw - the draw object

    Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewerDrawGetLG()`, `PetscViewerDrawGetAxis()`, `PetscViewerDrawOpen()`
@*/
PetscErrorCode PetscViewerDrawGetDraw(PetscViewer viewer, PetscInt windownumber, PetscDraw *draw)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidLogicalCollectiveInt(viewer, windownumber, 2);
  if (draw) PetscValidPointer(draw, 3);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  PetscCheck(isdraw, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Must be draw type PetscViewer");
  PetscCheck(windownumber >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Window number cannot be negative");
  vdraw = (PetscViewer_Draw *)viewer->data;

  windownumber += vdraw->draw_base;
  if (windownumber >= vdraw->draw_max) {
    /* allocate twice as many slots as needed */
    PetscInt       draw_max = vdraw->draw_max;
    PetscDraw     *tdraw    = vdraw->draw;
    PetscDrawLG   *drawlg   = vdraw->drawlg;
    PetscDrawAxis *drawaxis = vdraw->drawaxis;

    vdraw->draw_max = 2 * windownumber;

    PetscCall(PetscCalloc3(vdraw->draw_max, &vdraw->draw, vdraw->draw_max, &vdraw->drawlg, vdraw->draw_max, &vdraw->drawaxis));
    PetscCall(PetscArraycpy(vdraw->draw, tdraw, draw_max));
    PetscCall(PetscArraycpy(vdraw->drawlg, drawlg, draw_max));
    PetscCall(PetscArraycpy(vdraw->drawaxis, drawaxis, draw_max));
    PetscCall(PetscFree3(tdraw, drawlg, drawaxis));
  }

  if (!vdraw->draw[windownumber]) {
    char *title = vdraw->title, tmp_str[128];
    if (windownumber) {
      PetscCall(PetscSNPrintf(tmp_str, sizeof(tmp_str), "%s:%" PetscInt_FMT, vdraw->title ? vdraw->title : "", windownumber));
      title = tmp_str;
    }
    PetscCall(PetscDrawCreate(PetscObjectComm((PetscObject)viewer), vdraw->display, title, PETSC_DECIDE, PETSC_DECIDE, vdraw->w, vdraw->h, &vdraw->draw[windownumber]));
    if (vdraw->drawtype) PetscCall(PetscDrawSetType(vdraw->draw[windownumber], vdraw->drawtype));
    PetscCall(PetscDrawSetPause(vdraw->draw[windownumber], vdraw->pause));
    PetscCall(PetscDrawSetOptionsPrefix(vdraw->draw[windownumber], ((PetscObject)viewer)->prefix));
    PetscCall(PetscDrawSetFromOptions(vdraw->draw[windownumber]));
  }
  if (draw) *draw = vdraw->draw[windownumber];
  if (draw) PetscValidHeaderSpecific(*draw, PETSC_DRAW_CLASSID, 3);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscViewerDrawBaseAdd - add to the base integer that is added to the windownumber passed to `PetscViewerDrawGetDraw()`

    Logically Collective

    Input Parameters:
+  viewer - the `PetscViewer` (created with `PetscViewerDrawOpen()`)
-   windownumber - how much to add to the base

    Level: developer

    Note:
    A `PETSCVIEWERDRAW` may have multiple `PetscDraw` subwindows, this increases the number of the subwindow that is returned with `PetscViewerDrawGetDraw()`

.seealso: [](sec_viewers), `PetscViewerDrawGetLG()`, `PetscViewerDrawGetAxis()`, `PetscViewerDrawOpen()`, `PetscViewerDrawGetDraw()`, `PetscViewerDrawBaseSet()`
@*/
PetscErrorCode PetscViewerDrawBaseAdd(PetscViewer viewer, PetscInt windownumber)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidLogicalCollectiveInt(viewer, windownumber, 2);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  PetscCheck(isdraw, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Must be draw type PetscViewer");
  vdraw = (PetscViewer_Draw *)viewer->data;

  PetscCheck(windownumber + vdraw->draw_base >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Resulting base %" PetscInt_FMT " cannot be negative", windownumber + vdraw->draw_base);
  vdraw->draw_base += windownumber;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscViewerDrawBaseSet - sets the base integer that is added to the windownumber passed to `PetscViewerDrawGetDraw()`

    Logically Collective

    Input Parameters:
+   viewer - the `PetscViewer` (created with `PetscViewerDrawOpen()`)
-   windownumber - value to set the base

    Level: developer

    Note:
    A `PETSCVIEWERDRAW` may have multiple `PetscDraw` subwindows, this increases the number of the subwindow that is returned with `PetscViewerDrawGetDraw()`

.seealso: [](sec_viewers), `PetscViewerDrawGetLG()`, `PetscViewerDrawGetAxis()`, `PetscViewerDrawOpen()`, `PetscViewerDrawGetDraw()`, `PetscViewerDrawBaseAdd()`
@*/
PetscErrorCode PetscViewerDrawBaseSet(PetscViewer viewer, PetscInt windownumber)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidLogicalCollectiveInt(viewer, windownumber, 2);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  PetscCheck(isdraw, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Must be draw type PetscViewer");
  vdraw = (PetscViewer_Draw *)viewer->data;

  PetscCheck(windownumber >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Resulting base %" PetscInt_FMT " cannot be negative", windownumber);
  vdraw->draw_base = windownumber;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscViewerDrawGetDrawLG - Returns a `PetscDrawLG` object from `PetscViewer` object of type `PETSCVIEWERDRAW`.
    This `PetscDrawLG` object may then be used to perform graphics using `PetscDrawLG` commands.

    Collective

    Input Parameters:
+   viewer - the `PetscViewer` (created with `PetscViewerDrawOpen()`)
-   windownumber - indicates which subwindow (usually 0)

    Output Parameter:
.   draw - the draw line graph object

    Level: intermediate

    Note:
    A `PETSCVIEWERDRAW` may have multiple `PetscDraw` subwindows

.seealso: [](sec_viewers), `PetscDrawLG`, `PetscViewerDrawGetDraw()`, `PetscViewerDrawGetAxis()`, `PetscViewerDrawOpen()`
@*/
PetscErrorCode PetscViewerDrawGetDrawLG(PetscViewer viewer, PetscInt windownumber, PetscDrawLG *drawlg)
{
  PetscBool         isdraw;
  PetscViewer_Draw *vdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidLogicalCollectiveInt(viewer, windownumber, 2);
  PetscValidPointer(drawlg, 3);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  PetscCheck(isdraw, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Must be draw type PetscViewer");
  PetscCheck(windownumber >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Window number cannot be negative");
  vdraw = (PetscViewer_Draw *)viewer->data;

  if (windownumber + vdraw->draw_base >= vdraw->draw_max || !vdraw->draw[windownumber + vdraw->draw_base]) PetscCall(PetscViewerDrawGetDraw(viewer, windownumber, NULL));
  if (!vdraw->drawlg[windownumber + vdraw->draw_base]) {
    PetscCall(PetscDrawLGCreate(vdraw->draw[windownumber + vdraw->draw_base], 1, &vdraw->drawlg[windownumber + vdraw->draw_base]));
    PetscCall(PetscDrawLGSetFromOptions(vdraw->drawlg[windownumber + vdraw->draw_base]));
  }
  *drawlg = vdraw->drawlg[windownumber + vdraw->draw_base];
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscViewerDrawGetDrawAxis - Returns a `PetscDrawAxis` object from a `PetscViewer` object of type `PETSCVIEWERDRAW`.
    This `PetscDrawAxis` object may then be used to perform graphics using `PetscDrawAxis` commands.

    Collective

    Input Parameters:
+   viewer - the `PetscViewer` (created with `PetscViewerDrawOpen()`)
-   windownumber - indicates which subwindow (usually 0)

    Output Parameter:
.   drawaxis - the draw axis object

    Level: advanced

    Note:
    A `PETSCVIEWERDRAW` may have multiple `PetscDraw` subwindows

.seealso: [](sec_viewers), `PetscViewerDrawGetDraw()`, `PetscViewerDrawGetLG()`, `PetscViewerDrawOpen()`
@*/
PetscErrorCode PetscViewerDrawGetDrawAxis(PetscViewer viewer, PetscInt windownumber, PetscDrawAxis *drawaxis)
{
  PetscBool         isdraw;
  PetscViewer_Draw *vdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscValidLogicalCollectiveInt(viewer, windownumber, 2);
  PetscValidPointer(drawaxis, 3);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  PetscCheck(isdraw, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Must be draw type PetscViewer");
  PetscCheck(windownumber >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Window number cannot be negative");
  vdraw = (PetscViewer_Draw *)viewer->data;

  if (windownumber + vdraw->draw_base >= vdraw->draw_max || !vdraw->draw[windownumber + vdraw->draw_base]) PetscCall(PetscViewerDrawGetDraw(viewer, windownumber, NULL));
  if (!vdraw->drawaxis[windownumber + vdraw->draw_base]) PetscCall(PetscDrawAxisCreate(vdraw->draw[windownumber + vdraw->draw_base], &vdraw->drawaxis[windownumber + vdraw->draw_base]));
  *drawaxis = vdraw->drawaxis[windownumber + vdraw->draw_base];
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerDrawResize(PetscViewer v, int w, int h)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) PetscFunctionReturn(PETSC_SUCCESS);
  vdraw = (PetscViewer_Draw *)v->data;

  if (w >= 1) vdraw->w = w;
  if (h >= 1) vdraw->h = h;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerDrawSetInfo(PetscViewer v, const char display[], const char title[], int x, int y, int w, int h)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) PetscFunctionReturn(PETSC_SUCCESS);
  vdraw = (PetscViewer_Draw *)v->data;

  PetscCall(PetscStrallocpy(display, &vdraw->display));
  PetscCall(PetscStrallocpy(title, &vdraw->title));
  if (w >= 1) vdraw->w = w;
  if (h >= 1) vdraw->h = h;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerDrawSetDrawType(PetscViewer v, PetscDrawType drawtype)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) PetscFunctionReturn(PETSC_SUCCESS);
  vdraw = (PetscViewer_Draw *)v->data;

  PetscCall(PetscFree(vdraw->drawtype));
  PetscCall(PetscStrallocpy(drawtype, (char **)&vdraw->drawtype));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerDrawGetDrawType(PetscViewer v, PetscDrawType *drawtype)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERDRAW, &isdraw));
  PetscCheck(isdraw, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Must be draw type PetscViewer");
  vdraw = (PetscViewer_Draw *)v->data;

  *drawtype = vdraw->drawtype;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerDrawSetTitle(PetscViewer v, const char title[])
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) PetscFunctionReturn(PETSC_SUCCESS);
  vdraw = (PetscViewer_Draw *)v->data;

  PetscCall(PetscFree(vdraw->title));
  PetscCall(PetscStrallocpy(title, &vdraw->title));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerDrawGetTitle(PetscViewer v, const char *title[])
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERDRAW, &isdraw));
  PetscCheck(isdraw, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Must be draw type PetscViewer");
  vdraw = (PetscViewer_Draw *)v->data;

  *title = vdraw->title;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscViewerDrawOpen - Opens a `PetscDraw` window for use as a `PetscViewer` with type `PETSCVIEWERDRAW`. If you want to
   do graphics in this window, you must call `PetscViewerDrawGetDraw()` and
   perform the graphics on the `PetscDraw` object.

   Collective

   Input Parameters:
+  comm - communicator that will share window
.  display - the X display on which to open, or `NULL` for the local machine
.  title - the title to put in the title bar, or `NULL` for no title
.  x - horizontal screen coordinate of the upper left corner of window, or use `PETSC_DECIDE`
.  y - vertical screen coordinate of the upper left corner of window, or use `PETSC_DECIDE`
.  w - window width in pixels, or may use `PETSC_DECIDE` or `PETSC_DRAW_FULL_SIZE`, `PETSC_DRAW_HALF_SIZE`,`PETSC_DRAW_THIRD_SIZE`, `PETSC_DRAW_QUARTER_SIZE`
-  h - window height in pixels, or may use `PETSC_DECIDE` or `PETSC_DRAW_FULL_SIZE`, `PETSC_DRAW_HALF_SIZE`,`PETSC_DRAW_THIRD_SIZE`, `PETSC_DRAW_QUARTER_SIZE`

   Output Parameter:
. viewer - the `PetscViewer`

   Format Options:
+  `PETSC_VIEWER_DRAW_BASIC` - displays with basic format
-  `PETSC_VIEWER_DRAW_LG`    - displays using a line graph

   Options Database Keys:
+  -draw_type - use x or null
.  -nox - Disables all x-windows output
.  -display <name> - Specifies name of machine for the X display
.  -geometry <x,y,w,h> - allows setting the window location and size
-  -draw_pause <pause> - Sets time (in seconds) that the
     program pauses after PetscDrawPause() has been called
     (0 is default, -1 implies until user input).

   Level: beginner

   Fortran Note:
   Whenever indicating null character data in a Fortran code,
   `PETSC_NULL_CHARACTER` must be employed; using NULL is not
   correct for character data!  Thus, `PETSC_NULL_CHARACTER` can be
   used for the display and title input parameters.

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscDrawCreate()`, `PetscViewerDestroy()`, `PetscViewerDrawGetDraw()`, `PetscViewerCreate()`, `PETSC_VIEWER_DRAW_`,
          `PETSC_VIEWER_DRAW_WORLD`, `PETSC_VIEWER_DRAW_SELF`
@*/
PetscErrorCode PetscViewerDrawOpen(MPI_Comm comm, const char display[], const char title[], int x, int y, int w, int h, PetscViewer *viewer)
{
  PetscFunctionBegin;
  PetscCall(PetscViewerCreate(comm, viewer));
  PetscCall(PetscViewerSetType(*viewer, PETSCVIEWERDRAW));
  PetscCall(PetscViewerDrawSetInfo(*viewer, display, title, x, y, w, h));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#include <petsc/private/drawimpl.h>

PetscErrorCode PetscViewerGetSubViewer_Draw(PetscViewer viewer, MPI_Comm comm, PetscViewer *sviewer)
{
  PetscMPIInt       rank;
  PetscInt          i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw *)viewer->data, *svdraw;

  PetscFunctionBegin;
  PetscCheck(!vdraw->singleton_made, PETSC_COMM_SELF, PETSC_ERR_ORDER, "Trying to get SubViewer without first restoring previous");
  /* only processor zero can use the PetscViewer draw singleton */
  if (sviewer) *sviewer = NULL;
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)viewer), &rank));
  if (rank == 0) {
    PetscMPIInt flg;
    PetscDraw   draw, sdraw;

    PetscCallMPI(MPI_Comm_compare(PETSC_COMM_SELF, comm, &flg));
    PetscCheck(flg == MPI_IDENT || flg == MPI_CONGRUENT, PETSC_COMM_SELF, PETSC_ERR_SUP, "PetscViewerGetSubViewer() for PETSCVIEWERDRAW requires a singleton MPI_Comm");
    PetscCall(PetscViewerCreate(comm, sviewer));
    PetscCall(PetscViewerSetType(*sviewer, PETSCVIEWERDRAW));
    svdraw             = (PetscViewer_Draw *)(*sviewer)->data;
    (*sviewer)->format = viewer->format;
    for (i = 0; i < vdraw->draw_max; i++) { /* XXX this is wrong if svdraw->draw_max (initially 5) < vdraw->draw_max */
      if (vdraw->draw[i]) PetscCall(PetscDrawGetSingleton(vdraw->draw[i], &svdraw->draw[i]));
    }
    PetscCall(PetscViewerDrawGetDraw(viewer, 0, &draw));
    PetscCall(PetscViewerDrawGetDraw(*sviewer, 0, &sdraw));
    if (draw->savefilename) {
      PetscCall(PetscDrawSetSave(sdraw, draw->savefilename));
      sdraw->savefilecount  = draw->savefilecount;
      sdraw->savesinglefile = draw->savesinglefile;
      sdraw->savemoviefps   = draw->savemoviefps;
      sdraw->saveonclear    = draw->saveonclear;
      sdraw->saveonflush    = draw->saveonflush;
    }
    if (draw->savefinalfilename) PetscCall(PetscDrawSetSaveFinalImage(sdraw, draw->savefinalfilename));
  } else {
    PetscDraw draw;
    PetscCall(PetscViewerDrawGetDraw(viewer, 0, &draw));
  }
  vdraw->singleton_made = PETSC_TRUE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerRestoreSubViewer_Draw(PetscViewer viewer, MPI_Comm comm, PetscViewer *sviewer)
{
  PetscMPIInt       rank;
  PetscInt          i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw *)viewer->data, *svdraw;

  PetscFunctionBegin;
  PetscCheck(vdraw->singleton_made, PETSC_COMM_SELF, PETSC_ERR_ORDER, "Trying to restore a singleton that was not gotten");
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)viewer), &rank));
  if (rank == 0) {
    PetscDraw draw, sdraw;

    PetscCall(PetscViewerDrawGetDraw(viewer, 0, &draw));
    PetscCall(PetscViewerDrawGetDraw(*sviewer, 0, &sdraw));
    if (draw->savefilename) {
      draw->savefilecount = sdraw->savefilecount;
      PetscCallMPI(MPI_Bcast(&draw->savefilecount, 1, MPIU_INT, 0, PetscObjectComm((PetscObject)draw)));
    }
    svdraw = (PetscViewer_Draw *)(*sviewer)->data;
    for (i = 0; i < vdraw->draw_max; i++) {
      if (vdraw->draw[i] && svdraw->draw[i]) PetscCall(PetscDrawRestoreSingleton(vdraw->draw[i], &svdraw->draw[i]));
    }
    PetscCall(PetscFree3(svdraw->draw, svdraw->drawlg, svdraw->drawaxis));
    PetscCall(PetscFree((*sviewer)->data));
    PetscCall(PetscHeaderDestroy(sviewer));
  } else {
    PetscDraw draw;

    PetscCall(PetscViewerDrawGetDraw(viewer, 0, &draw));
    if (draw->savefilename) PetscCallMPI(MPI_Bcast(&draw->savefilecount, 1, MPIU_INT, 0, PetscObjectComm((PetscObject)draw)));
  }

  vdraw->singleton_made = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerSetFromOptions_Draw(PetscViewer v, PetscOptionItems *PetscOptionsObject)
{
  PetscReal bounds[16];
  PetscInt  nbounds = 16;
  PetscBool flg;

  PetscFunctionBegin;
  PetscOptionsHeadBegin(PetscOptionsObject, "Draw PetscViewer Options");
  PetscCall(PetscOptionsRealArray("-draw_bounds", "Bounds to put on plots axis", "PetscViewerDrawSetBounds", bounds, &nbounds, &flg));
  if (flg) PetscCall(PetscViewerDrawSetBounds(v, nbounds / 2, bounds));
  PetscOptionsHeadEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscViewerView_Draw(PetscViewer viewer, PetscViewer v)
{
  PetscDraw         draw;
  PetscInt          i;
  PetscViewer_Draw *vdraw = (PetscViewer_Draw *)viewer->data;
  PetscBool         iascii;

  PetscFunctionBegin;
  PetscCall(PetscObjectTypeCompare((PetscObject)v, PETSCVIEWERASCII, &iascii));
  if (iascii) PetscCall(PetscViewerASCIIPrintf(v, "Draw viewer is of type %s\n", vdraw->drawtype));
  /*  If the PetscViewer has just been created then no vdraw->draw yet
      exists so this will not actually call the viewer on any draws. */
  for (i = 0; i < vdraw->draw_base; i++) {
    if (vdraw->draw[i]) {
      PetscCall(PetscViewerDrawGetDraw(viewer, i, &draw));
      PetscCall(PetscDrawView(draw, v));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*MC
   PETSCVIEWERDRAW - A viewer that generates graphics, either to the screen or a file

  Level: beginner

.seealso: [](sec_viewers), `PetscViewerDrawOpen()`, `PetscViewerDrawGetDraw()`, `PETSC_VIEWER_DRAW_()`, `PETSC_VIEWER_DRAW_SELF`, `PETSC_VIEWER_DRAW_WORLD`,
          `PetscViewerCreate()`, `PetscViewerASCIIOpen()`, `PetscViewerBinaryOpen()`, `PETSCVIEWERBINARY`,
          `PetscViewerMatlabOpen()`, `VecView()`, `DMView()`, `PetscViewerMatlabPutArray()`, `PETSCVIEWERASCII`, `PETSCVIEWERMATLAB`,
          `PetscViewerFileSetName()`, `PetscViewerFileSetMode()`, `PetscViewerFormat`, `PetscViewerType`, `PetscViewerSetType()`
M*/
PETSC_EXTERN PetscErrorCode PetscViewerCreate_Draw(PetscViewer viewer)
{
  PetscViewer_Draw *vdraw;

  PetscFunctionBegin;
  PetscCall(PetscNew(&vdraw));
  viewer->data = (void *)vdraw;

  viewer->ops->flush            = PetscViewerFlush_Draw;
  viewer->ops->view             = PetscViewerView_Draw;
  viewer->ops->destroy          = PetscViewerDestroy_Draw;
  viewer->ops->setfromoptions   = PetscViewerSetFromOptions_Draw;
  viewer->ops->getsubviewer     = PetscViewerGetSubViewer_Draw;
  viewer->ops->restoresubviewer = PetscViewerRestoreSubViewer_Draw;

  /* these are created on the fly if requested */
  vdraw->draw_max  = 5;
  vdraw->draw_base = 0;
  vdraw->w         = PETSC_DECIDE;
  vdraw->h         = PETSC_DECIDE;

  PetscCall(PetscCalloc3(vdraw->draw_max, &vdraw->draw, vdraw->draw_max, &vdraw->drawlg, vdraw->draw_max, &vdraw->drawaxis));
  vdraw->singleton_made = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscViewerDrawClear - Clears a `PetscDraw` graphic associated with a `PetscViewer`.

    Not Collective

    Input Parameter:
.  viewer - the `PetscViewer`

    Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewerDrawOpen()`, `PetscViewerDrawGetDraw()`,
@*/
PetscErrorCode PetscViewerDrawClear(PetscViewer viewer)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;
  PetscInt          i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) PetscFunctionReturn(PETSC_SUCCESS);
  vdraw = (PetscViewer_Draw *)viewer->data;

  for (i = 0; i < vdraw->draw_max; i++) {
    if (vdraw->draw[i]) PetscCall(PetscDrawClear(vdraw->draw[i]));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscViewerDrawGetPause - Gets the pause value (how long to pause before an image is changed)  in the `PETSCVIEWERDRAW` `PetscViewer`

    Not Collective

    Input Parameter:
.  viewer - the `PetscViewer`

    Output Parameter:
.  pause - the pause value

    Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewerDrawOpen()`, `PetscViewerDrawGetDraw()`,
@*/
PetscErrorCode PetscViewerDrawGetPause(PetscViewer viewer, PetscReal *pause)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;
  PetscInt          i;
  PetscDraw         draw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) {
    *pause = 0.0;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  vdraw = (PetscViewer_Draw *)viewer->data;

  for (i = 0; i < vdraw->draw_max; i++) {
    if (vdraw->draw[i]) {
      PetscCall(PetscDrawGetPause(vdraw->draw[i], pause));
      PetscFunctionReturn(PETSC_SUCCESS);
    }
  }
  /* none exist yet so create one and get its pause */
  PetscCall(PetscViewerDrawGetDraw(viewer, 0, &draw));
  PetscCall(PetscDrawGetPause(draw, pause));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscViewerDrawSetPause - Sets a pause for each `PetscDraw` in the `PETSCVIEWERDRAW` `PetscViewer`

    Not Collective

    Input Parameters:
+  viewer - the `PetscViewer`
-  pause - the pause value

    Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewerDrawOpen()`, `PetscViewerDrawGetDraw()`,
@*/
PetscErrorCode PetscViewerDrawSetPause(PetscViewer viewer, PetscReal pause)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;
  PetscInt          i;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) PetscFunctionReturn(PETSC_SUCCESS);
  vdraw = (PetscViewer_Draw *)viewer->data;

  vdraw->pause = pause;
  for (i = 0; i < vdraw->draw_max; i++) {
    if (vdraw->draw[i]) PetscCall(PetscDrawSetPause(vdraw->draw[i], pause));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscViewerDrawSetHold - Holds previous image when drawing new image

    Not Collective

    Input Parameters:
+  viewer - the `PetscViewer`
-  hold - `PETSC_TRUE` indicates to hold the previous image

    Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewerDrawOpen()`, `PetscViewerDrawGetDraw()`,
@*/
PetscErrorCode PetscViewerDrawSetHold(PetscViewer viewer, PetscBool hold)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) PetscFunctionReturn(PETSC_SUCCESS);
  vdraw = (PetscViewer_Draw *)viewer->data;

  vdraw->hold = hold;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
    PetscViewerDrawGetHold - Checks if the `PETSCVIEWERDRAW` `PetscViewer` holds previous image when drawing new image

    Not Collective

    Input Parameter:
.  viewer - the `PetscViewer`

    Output Parameter:
.  hold - indicates to hold or not

    Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewerDrawOpen()`, `PetscViewerDrawGetDraw()`,
@*/
PetscErrorCode PetscViewerDrawGetHold(PetscViewer viewer, PetscBool *hold)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) {
    *hold = PETSC_FALSE;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  vdraw = (PetscViewer_Draw *)viewer->data;

  *hold = vdraw->hold;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    The variable Petsc_Viewer_Draw_keyval is used to indicate an MPI attribute that
  is attached to a communicator, in this case the attribute is a PetscViewer.
*/
PetscMPIInt Petsc_Viewer_Draw_keyval = MPI_KEYVAL_INVALID;

/*@C
    PETSC_VIEWER_DRAW_ - Creates a window `PETSCVIEWERDRAW` `PetscViewer` shared by all processors
                     in a communicator.

     Collective

     Input Parameter:
.    comm - the MPI communicator to share the window `PetscViewer`

     Level: intermediate

     Note:
     Unlike almost all other PETSc routines, `PETSC_VIEWER_DRAW_()` does not return
     an error code.  The window is usually used in the form
$       XXXView(XXX object,PETSC_VIEWER_DRAW_(comm));

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewer`, `PETSC_VIEWER_DRAW_WORLD`, `PETSC_VIEWER_DRAW_SELF`, `PetscViewerDrawOpen()`,
@*/
PetscViewer PETSC_VIEWER_DRAW_(MPI_Comm comm)
{
  PetscErrorCode ierr;
  PetscMPIInt    flag, mpi_ierr;
  PetscViewer    viewer;
  MPI_Comm       ncomm;

  PetscFunctionBegin;
  ierr = PetscCommDuplicate(comm, &ncomm, NULL);
  if (ierr) {
    ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_DRAW_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
    PetscFunctionReturn(NULL);
  }
  if (Petsc_Viewer_Draw_keyval == MPI_KEYVAL_INVALID) {
    mpi_ierr = MPI_Comm_create_keyval(MPI_COMM_NULL_COPY_FN, MPI_COMM_NULL_DELETE_FN, &Petsc_Viewer_Draw_keyval, NULL);
    if (mpi_ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_DRAW_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
      PetscFunctionReturn(NULL);
    }
  }
  mpi_ierr = MPI_Comm_get_attr(ncomm, Petsc_Viewer_Draw_keyval, (void **)&viewer, &flag);
  if (mpi_ierr) {
    ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_DRAW_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
    PetscFunctionReturn(NULL);
  }
  if (!flag) { /* PetscViewer not yet created */
    ierr = PetscViewerDrawOpen(ncomm, NULL, NULL, PETSC_DECIDE, PETSC_DECIDE, 300, 300, &viewer);
    if (ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_DRAW_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
      PetscFunctionReturn(NULL);
    }
    ierr = PetscObjectRegisterDestroy((PetscObject)viewer);
    if (ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_DRAW_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
      PetscFunctionReturn(NULL);
    }
    mpi_ierr = MPI_Comm_set_attr(ncomm, Petsc_Viewer_Draw_keyval, (void *)viewer);
    if (mpi_ierr) {
      ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_DRAW_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_INITIAL, " ");
      PetscFunctionReturn(NULL);
    }
  }
  ierr = PetscCommDestroy(&ncomm);
  if (ierr) {
    ierr = PetscError(PETSC_COMM_SELF, __LINE__, "PETSC_VIEWER_DRAW_", __FILE__, PETSC_ERR_PLIB, PETSC_ERROR_REPEAT, " ");
    PetscFunctionReturn(NULL);
  }
  PetscFunctionReturn(viewer);
}

/*@
    PetscViewerDrawSetBounds - sets the upper and lower bounds to be used in plotting

    Collective

    Input Parameters:
+   viewer - the Petsc`Viewer` (created with `PetscViewerDrawOpen()`)
.   nbounds - number of plots that can be made with this viewer, for example the dof passed to `DMDACreate()`
-   bounds - the actual bounds, the size of this is 2*nbounds, the values are stored in the order min F_0, max F_0, min F_1, max F_1, .....

    Options Database Key:
.   -draw_bounds  minF0,maxF0,minF1,maxF1 - the lower left and upper right bounds

    Level: intermediate

    Note:
    this determines the colors used in 2d contour plots generated with VecView() for `DMDA` in 2d. Any values in the vector below or above the
      bounds are moved to the bound value before plotting. In this way the color index from color to physical value remains the same for all plots generated with
      this viewer. Otherwise the color to physical value meaning changes with each new image if this is not set.

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewerDrawGetLG()`, `PetscViewerDrawGetAxis()`, `PetscViewerDrawOpen()`
@*/
PetscErrorCode PetscViewerDrawSetBounds(PetscViewer viewer, PetscInt nbounds, const PetscReal *bounds)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) PetscFunctionReturn(PETSC_SUCCESS);
  vdraw = (PetscViewer_Draw *)viewer->data;

  vdraw->nbounds = nbounds;
  PetscCall(PetscFree(vdraw->bounds));
  PetscCall(PetscMalloc1(2 * nbounds, &vdraw->bounds));
  PetscCall(PetscArraycpy(vdraw->bounds, bounds, 2 * nbounds));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
    PetscViewerDrawGetBounds - gets the upper and lower bounds to be used in plotting set with `PetscViewerDrawSetBounds()`

    Collective

    Input Parameter:
.   viewer - the `PetscViewer` (created with `PetscViewerDrawOpen()`)

    Output Parameters:
+   nbounds - number of plots that can be made with this viewer, for example the dof passed to `DMDACreate()`
-   bounds - the actual bounds, the size of this is 2*nbounds, the values are stored in the order min F_0, max F_0, min F_1, max F_1, .....

    Level: intermediate

.seealso: [](sec_viewers), `PETSCVIEWERDRAW`, `PetscViewerDrawGetLG()`, `PetscViewerDrawGetAxis()`, `PetscViewerDrawOpen()`, `PetscViewerDrawSetBounds()`
@*/
PetscErrorCode PetscViewerDrawGetBounds(PetscViewer viewer, PetscInt *nbounds, const PetscReal **bounds)
{
  PetscViewer_Draw *vdraw;
  PetscBool         isdraw;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 1);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
  if (!isdraw) {
    if (nbounds) *nbounds = 0;
    if (bounds) *bounds = NULL;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  vdraw = (PetscViewer_Draw *)viewer->data;

  if (nbounds) *nbounds = vdraw->nbounds;
  if (bounds) *bounds = vdraw->bounds;
  PetscFunctionReturn(PETSC_SUCCESS);
}

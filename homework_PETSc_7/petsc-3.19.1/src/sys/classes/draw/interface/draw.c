
#include <petsc/private/drawimpl.h> /*I "petscdraw.h" I*/
#include <petscviewer.h>

PetscClassId PETSC_DRAW_CLASSID;

static PetscBool PetscDrawPackageInitialized = PETSC_FALSE;
/*@C
  PetscDrawFinalizePackage - This function destroys everything in the Petsc interface to the `PetscDraw` package. It is
  called from `PetscFinalize()`.

  Level: developer

.seealso: `PetscDraw`, `PetscFinalize()`
@*/
PetscErrorCode PetscDrawFinalizePackage(void)
{
  PetscFunctionBegin;
  PetscCall(PetscFunctionListDestroy(&PetscDrawList));
  PetscDrawPackageInitialized = PETSC_FALSE;
  PetscDrawRegisterAllCalled  = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscInitializeDrawPackage - This function initializes everything in the `PetscDraw` package. It is called
  from PetscDLLibraryRegister_petsc() when using dynamic libraries, and on the call to `PetscInitialize()`
  when using shared or static libraries.

  Level: developer

.seealso: `PetscDraw`, `PetscInitialize()`
@*/
PetscErrorCode PetscDrawInitializePackage(void)
{
  char      logList[256];
  PetscBool opt, pkg;

  PetscFunctionBegin;
  if (PetscDrawPackageInitialized) PetscFunctionReturn(PETSC_SUCCESS);
  PetscDrawPackageInitialized = PETSC_TRUE;
  /* Register Classes */
  PetscCall(PetscClassIdRegister("Draw", &PETSC_DRAW_CLASSID));
  PetscCall(PetscClassIdRegister("Draw Axis", &PETSC_DRAWAXIS_CLASSID));
  PetscCall(PetscClassIdRegister("Line Graph", &PETSC_DRAWLG_CLASSID));
  PetscCall(PetscClassIdRegister("Histogram", &PETSC_DRAWHG_CLASSID));
  PetscCall(PetscClassIdRegister("Bar Graph", &PETSC_DRAWBAR_CLASSID));
  PetscCall(PetscClassIdRegister("Scatter Plot", &PETSC_DRAWSP_CLASSID));
  /* Register Constructors */
  PetscCall(PetscDrawRegisterAll());
  /* Process Info */
  {
    PetscClassId classids[6];

    classids[0] = PETSC_DRAW_CLASSID;
    classids[1] = PETSC_DRAWAXIS_CLASSID;
    classids[2] = PETSC_DRAWLG_CLASSID;
    classids[3] = PETSC_DRAWHG_CLASSID;
    classids[4] = PETSC_DRAWBAR_CLASSID;
    classids[5] = PETSC_DRAWSP_CLASSID;
    PetscCall(PetscInfoProcessClass("draw", 6, classids));
  }
  /* Process summary exclusions */
  PetscCall(PetscOptionsGetString(NULL, NULL, "-log_exclude", logList, sizeof(logList), &opt));
  if (opt) {
    PetscCall(PetscStrInList("draw", logList, ',', &pkg));
    if (pkg) {
      PetscCall(PetscLogEventExcludeClass(PETSC_DRAW_CLASSID));
      PetscCall(PetscLogEventExcludeClass(PETSC_DRAWAXIS_CLASSID));
      PetscCall(PetscLogEventExcludeClass(PETSC_DRAWLG_CLASSID));
      PetscCall(PetscLogEventExcludeClass(PETSC_DRAWHG_CLASSID));
      PetscCall(PetscLogEventExcludeClass(PETSC_DRAWBAR_CLASSID));
      PetscCall(PetscLogEventExcludeClass(PETSC_DRAWSP_CLASSID));
    }
  }
  /* Register package finalizer */
  PetscCall(PetscRegisterFinalize(PetscDrawFinalizePackage));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawResizeWindow - Allows one to resize a window from a program.

   Collective

   Input Parameters:
+  draw - the window
.  w - the new width of the window
-  h - the new height of the window

   Level: intermediate

.seealso: `PetscDraw`, `PetscDrawCheckResizedWindow()`
@*/
PetscErrorCode PetscDrawResizeWindow(PetscDraw draw, int w, int h)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidLogicalCollectiveInt(draw, w, 2);
  PetscValidLogicalCollectiveInt(draw, h, 3);
  PetscTryTypeMethod(draw, resizewindow, w, h);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawGetWindowSize - Gets the size of the window.

   Not Collective

   Input Parameter:
.  draw - the window

   Output Parameters:
+  w - the window width
-  h - the window height

   Level: intermediate

.seealso: `PetscDraw`, `PetscDrawResizeWindow()`, `PetscDrawCheckResizedWindow()`
@*/
PetscErrorCode PetscDrawGetWindowSize(PetscDraw draw, int *w, int *h)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  if (w) PetscValidPointer(w, 2);
  if (h) PetscValidPointer(h, 3);
  if (w) *w = draw->w;
  if (h) *h = draw->h;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawCheckResizedWindow - Checks if the user has resized the window.

   Collective

   Input Parameter:
.  draw - the window

   Level: advanced

.seealso: `PetscDraw`, `PetscDrawResizeWindow()`
@*/
PetscErrorCode PetscDrawCheckResizedWindow(PetscDraw draw)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscTryTypeMethod(draw, checkresizedwindow);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawGetTitle - Gets pointer to title of a `PetscDraw` context.

   Not Collective

   Input Parameter:
.  draw - the graphics context

   Output Parameter:
.  title - the title

   Level: intermediate

.seealso: `PetscDraw`, `PetscDrawSetTitle()`
@*/
PetscErrorCode PetscDrawGetTitle(PetscDraw draw, const char *title[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidPointer(title, 2);
  *title = draw->title;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawSetTitle - Sets the title of a `PetscDraw` context.

   Collective

   Input Parameters:
+  draw - the graphics context
-  title - the title

   Level: intermediate

   Notes:
   The title is positioned in the windowing system title bar for the window. Hence it will not be saved with -draw_save
   in the image.

   A copy of the string is made, so you may destroy the
   title string after calling this routine.

   You can use `PetscDrawAxisSetLabels()` to indicate a title within the window

.seealso: `PetscDraw`, `PetscDrawGetTitle()`, `PetscDrawAppendTitle()`
@*/
PetscErrorCode PetscDrawSetTitle(PetscDraw draw, const char title[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidCharPointer(title, 2);
  PetscCall(PetscFree(draw->title));
  PetscCall(PetscStrallocpy(title, &draw->title));
  PetscTryTypeMethod(draw, settitle, draw->title);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawAppendTitle - Appends to the title of a `PetscDraw` context.

   Collective

   Input Parameters:
+  draw - the graphics context
-  title - the title

   Level: advanced

   Note:
   A copy of the string is made, so you may destroy the
   title string after calling this routine.

.seealso: `PetscDraw`, `PetscDrawSetTitle()`, `PetscDrawGetTitle()`
@*/
PetscErrorCode PetscDrawAppendTitle(PetscDraw draw, const char title[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  if (title) PetscValidCharPointer(title, 2);
  if (!title || !title[0]) PetscFunctionReturn(PETSC_SUCCESS);

  if (draw->title) {
    size_t len1, len2, new_len;
    PetscCall(PetscStrlen(draw->title, &len1));
    PetscCall(PetscStrlen(title, &len2));
    new_len = len1 + len2 + 1;
    PetscCall(PetscRealloc(new_len * sizeof(*(draw->title)), &draw->title));
    PetscCall(PetscStrncpy(draw->title + len1, title, len2 + 1));
  } else {
    PetscCall(PetscStrallocpy(title, &draw->title));
  }
  PetscTryTypeMethod(draw, settitle, draw->title);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDrawDestroy_Private(PetscDraw draw)
{
  PetscFunctionBegin;
  if (!draw->ops->save && !draw->ops->getimage) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscDrawSaveMovie(draw));
  if (draw->savefinalfilename) {
    draw->savesinglefile = PETSC_TRUE;
    PetscCall(PetscDrawSetSave(draw, draw->savefinalfilename));
    PetscCall(PetscDrawSave(draw));
  }
  PetscCall(PetscBarrier((PetscObject)draw));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawDestroy - Deletes a draw context.

   Collective

   Input Parameter:
.  draw - the drawing context

   Level: beginner

.seealso: `PetscDraw`, `PetscDrawCreate()`
@*/
PetscErrorCode PetscDrawDestroy(PetscDraw *draw)
{
  PetscFunctionBegin;
  if (!*draw) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific(*draw, PETSC_DRAW_CLASSID, 1);
  if (--((PetscObject)(*draw))->refct > 0) PetscFunctionReturn(PETSC_SUCCESS);

  if ((*draw)->pause == -2) {
    (*draw)->pause = -1;
    PetscCall(PetscDrawPause(*draw));
  }

  /* if memory was published then destroy it */
  PetscCall(PetscObjectSAWsViewOff((PetscObject)*draw));

  PetscCall(PetscDrawDestroy_Private(*draw));

  if ((*draw)->ops->destroy) PetscCall((*(*draw)->ops->destroy)(*draw));
  PetscCall(PetscDrawDestroy(&(*draw)->popup));
  PetscCall(PetscFree((*draw)->title));
  PetscCall(PetscFree((*draw)->display));
  PetscCall(PetscFree((*draw)->savefilename));
  PetscCall(PetscFree((*draw)->saveimageext));
  PetscCall(PetscFree((*draw)->savemovieext));
  PetscCall(PetscFree((*draw)->savefinalfilename));
  PetscCall(PetscHeaderDestroy(draw));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawGetPopup - Creates a popup window associated with a `PetscDraw` window.

   Collective

   Input Parameter:
.  draw - the original window

   Output Parameter:
.  popup - the new popup window

   Level: advanced

.seealso: `PetscDraw`, `PetscDrawScalePopup()`, `PetscDrawCreate()`
@*/
PetscErrorCode PetscDrawGetPopup(PetscDraw draw, PetscDraw *popup)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidPointer(popup, 2);

  if (draw->popup) *popup = draw->popup;
  else if (draw->ops->getpopup) {
    PetscUseTypeMethod(draw, getpopup, popup);
    if (*popup) {
      PetscCall(PetscObjectSetOptionsPrefix((PetscObject)*popup, "popup_"));
      (*popup)->pause = 0.0;
      PetscCall(PetscDrawSetFromOptions(*popup));
    }
  } else *popup = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscDrawSetDisplay - Sets the display where a `PetscDraw` object will be displayed

  Input Parameters:
+ draw - the drawing context
- display - the X windows display

  Level: advanced

.seealso: `PetscDraw`, `PetscDrawOpenX()`, `PetscDrawCreate()`
@*/
PetscErrorCode PetscDrawSetDisplay(PetscDraw draw, const char display[])
{
  PetscFunctionBegin;
  PetscCall(PetscFree(draw->display));
  PetscCall(PetscStrallocpy(display, &draw->display));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawSetDoubleBuffer - Sets a window to be double buffered.

   Logically Collective

   Input Parameter:
.  draw - the drawing context

   Level: intermediate

.seealso: `PetscDraw`, `PetscDrawOpenX()`, `PetscDrawCreate()`
@*/
PetscErrorCode PetscDrawSetDoubleBuffer(PetscDraw draw)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscTryTypeMethod(draw, setdoublebuffer);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawGetSingleton - Gain access to a `PetscDraw` object as if it were owned
        by the one process.

   Collective

   Input Parameter:
.  draw - the original window

   Output Parameter:
.  sdraw - the singleton window

   Level: advanced

.seealso: `PetscDraw`, `PetscDrawRestoreSingleton()`, `PetscViewerGetSingleton()`, `PetscViewerRestoreSingleton()`
@*/
PetscErrorCode PetscDrawGetSingleton(PetscDraw draw, PetscDraw *sdraw)
{
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidPointer(sdraw, 2);

  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)draw), &size));
  if (size == 1) {
    PetscCall(PetscObjectReference((PetscObject)draw));
    *sdraw = draw;
  } else {
    if (draw->ops->getsingleton) {
      PetscUseTypeMethod(draw, getsingleton, sdraw);
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Cannot get singleton for this type %s of draw object", ((PetscObject)draw)->type_name);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawRestoreSingleton - Remove access to a `PetscDraw` object obtained with `PetscDrawGetSingleton()`
        by the one process.

   Collective

   Input Parameters:
+  draw - the original window
-  sdraw - the singleton window

   Level: advanced

.seealso: `PetscDraw`, `PetscDrawGetSingleton()`, `PetscViewerGetSingleton()`, `PetscViewerRestoreSingleton()`
@*/
PetscErrorCode PetscDrawRestoreSingleton(PetscDraw draw, PetscDraw *sdraw)
{
  PetscMPIInt size;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidPointer(sdraw, 2);
  PetscValidHeaderSpecific(*sdraw, PETSC_DRAW_CLASSID, 2);

  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)draw), &size));
  if (size == 1) {
    if (draw == *sdraw) {
      PetscCall(PetscObjectDereference((PetscObject)draw));
      *sdraw = NULL;
    } else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Cannot restore singleton, it is not the parent draw");
  } else PetscUseTypeMethod(draw, restoresingleton, sdraw);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscDrawSetVisible - Sets if the drawing surface (the 'window') is visible on its display.

  Input Parameters:
+ draw - the drawing window
- visible - if the surface should be visible

  Level: intermediate

.seealso: `PetscDraw`
@*/
PetscErrorCode PetscDrawSetVisible(PetscDraw draw, PetscBool visible)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscTryTypeMethod(draw, setvisible, visible);
  PetscFunctionReturn(PETSC_SUCCESS);
}


/*
       Provides the registration process for PETSc PetscDraw routines
*/
#include <petsc/private/drawimpl.h> /*I "petscdraw.h" I*/
#include <petscviewer.h>            /*I "petscviewer.h" I*/
#if defined(PETSC_HAVE_SAWS)
  #include <petscviewersaws.h>
#endif

/*
   Contains the list of registered PetscDraw routines
*/
PetscFunctionList PetscDrawList = NULL;

/*@C
   PetscDrawView - Prints the `PetscDraw` data structure.

   Collective

   Input Parameters:
+  indraw - the `PetscDraw` context
-  viewer - visualization context

   See PetscDrawSetFromOptions() for options database keys

   Note:
   The available visualization contexts include
+     `PETSC_VIEWER_STDOUT_SELF` - standard output (default)
-     `PETSC_VIEWER_STDOUT_WORLD` - synchronized standard
         output where only the first processor opens
         the file.  All other processors send their
         data to the first processor to print.

   The user can open an alternative visualization context with
   `PetscViewerASCIIOpen()` - output to a specified file.

   Level: beginner

.seealso: `PetscDraw`, `PetscViewerASCIIOpen()`, `PetscViewer`
@*/
PetscErrorCode PetscDrawView(PetscDraw indraw, PetscViewer viewer)
{
  PetscBool isdraw;
#if defined(PETSC_HAVE_SAWS)
  PetscBool issaws;
#endif

  PetscFunctionBegin;
  PetscValidHeaderSpecific(indraw, PETSC_DRAW_CLASSID, 1);
  if (!viewer) PetscCall(PetscViewerASCIIGetStdout(PetscObjectComm((PetscObject)indraw), &viewer));
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  PetscCheckSameComm(indraw, 1, viewer, 2);

  PetscCall(PetscObjectPrintClassNamePrefixType((PetscObject)indraw, viewer));
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERDRAW, &isdraw));
#if defined(PETSC_HAVE_SAWS)
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERSAWS, &issaws));
#endif
  if (isdraw) {
    PetscDraw draw;
    char      str[36];
    PetscReal x, y, bottom, h;

    PetscCall(PetscViewerDrawGetDraw(viewer, 0, &draw));
    PetscCall(PetscDrawGetCurrentPoint(draw, &x, &y));
    PetscCall(PetscStrncpy(str, "PetscDraw: ", sizeof(str)));
    PetscCall(PetscStrlcat(str, ((PetscObject)indraw)->type_name, sizeof(str)));
    PetscCall(PetscDrawStringBoxed(draw, x, y, PETSC_DRAW_RED, PETSC_DRAW_BLACK, str, NULL, &h));
    bottom = y - h;
    PetscCall(PetscDrawPushCurrentPoint(draw, x, bottom));
#if defined(PETSC_HAVE_SAWS)
  } else if (issaws) {
    PetscMPIInt rank;

    PetscCall(PetscObjectName((PetscObject)indraw));
    PetscCallMPI(MPI_Comm_rank(PETSC_COMM_WORLD, &rank));
    if (!((PetscObject)indraw)->amsmem && rank == 0) PetscCall(PetscObjectViewSAWs((PetscObject)indraw, viewer));
#endif
  } else PetscTryTypeMethod(indraw, view, viewer);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawViewFromOptions - View a `PetscDraw` from the option database

   Collective

   Input Parameters:
+  A - the `PetscDraw` context
.  obj - Optional object
-  name - command line option

   Level: intermediate
.seealso: `PetscDraw`, `PetscDrawView`, `PetscObjectViewFromOptions()`, `PetscDrawCreate()`
@*/
PetscErrorCode PetscDrawViewFromOptions(PetscDraw A, PetscObject obj, const char name[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(A, PETSC_DRAW_CLASSID, 1);
  PetscCall(PetscObjectViewFromOptions((PetscObject)A, obj, name));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawCreate - Creates a graphics context.

   Collective

   Input Parameters:
+  comm - MPI communicator
.  display - X display when using X Windows
.  title - optional title added to top of window
.  x - horizonatl coordinate of lower left corner of window or `PETSC_DECIDE`
.  y - vertical coordinate of lower left corner of window or `PETSC_DECIDE`
.  w - width of window, `PETSC_DECIDE`, `PETSC_DRAW_HALF_SIZE`, `PETSC_DRAW_FULL_SIZE`, `PETSC_DRAW_THIRD_SIZE` or `PETSC_DRAW_QUARTER_SIZE`
-  h - height of window, `PETSC_DECIDE`, `PETSC_DRAW_HALF_SIZE`, `PETSC_DRAW_FULL_SIZE`, `PETSC_DRAW_THIRD_SIZE` or `PETSC_DRAW_QUARTER_SIZE`

   Output Parameter:
.  draw - location to put the `PetscDraw` context

   Level: beginner

.seealso: `PetscDrawSetType()`, `PetscDrawSetFromOptions()`, `PetscDrawDestroy()`, `PetscDrawSetType()`, `PetscDrawLGCreate()`, `PetscDrawSPCreate()`,
          `PetscDrawViewPortsCreate()`, `PetscDrawViewPortsSet()`, `PetscDrawAxisCreate()`, `PetscDrawHGCreate()`, `PetscDrawBarCreate()`,
          `PetscViewerDrawGetDraw()`, `PetscDrawSetFromOptions()`, `PetscDrawSetSave()`, `PetscDrawSetSaveMovie()`, `PetscDrawSetSaveFinalImage()`,
          `PetscDrawOpenX()`, `PetscDrawOpenImage()`, `PetscDrawIsNull()`, `PetscDrawGetPopup()`, `PetscDrawCheckResizedWindow()`, `PetscDrawResizeWindow()`,
          `PetscDrawGetWindowSize()`, `PetscDrawLine()`, `PetscDrawArrow()`, `PetscDrawLineSetWidth()`, `PetscDrawLineGetWidth()`, `PetscDrawMarker()`,
          `PetscDrawPoint()`, `PetscDrawRectangle()`, `PetscDrawTriangle()`, `PetscDrawEllipse()`, `PetscDrawString()`, `PetscDrawStringCentered()`,
          `PetscDrawStringBoxed()`, `PetscDrawStringVertical()`, `PetscDrawSetViewPort()`, `PetscDrawGetViewPort()`,
          `PetscDrawSplitViewPort()`, `PetscDrawSetTitle()`, `PetscDrawAppendTitle()`, `PetscDrawGetTitle()`, `PetscDrawSetPause()`, `PetscDrawGetPause()`,
          `PetscDrawPause()`, `PetscDrawSetDoubleBuffer()`, `PetscDrawClear()`, `PetscDrawFlush()`, `PetscDrawGetSingleton()`, `PetscDrawGetMouseButton()`,
          `PetscDrawZoom()`, `PetscDrawGetBoundingBox()`
@*/
PetscErrorCode PetscDrawCreate(MPI_Comm comm, const char display[], const char title[], int x, int y, int w, int h, PetscDraw *indraw)
{
  PetscDraw draw;
  PetscReal dpause = 0.0;
  PetscBool flag;

  PetscFunctionBegin;
  PetscCall(PetscDrawInitializePackage());
  *indraw = NULL;
  PetscCall(PetscHeaderCreate(draw, PETSC_DRAW_CLASSID, "Draw", "Graphics", "Draw", comm, PetscDrawDestroy, PetscDrawView));

  draw->data = NULL;
  PetscCall(PetscStrallocpy(display, &draw->display));
  PetscCall(PetscStrallocpy(title, &draw->title));
  draw->x       = x;
  draw->y       = y;
  draw->w       = w;
  draw->h       = h;
  draw->pause   = 0.0;
  draw->coor_xl = 0.0;
  draw->coor_xr = 1.0;
  draw->coor_yl = 0.0;
  draw->coor_yr = 1.0;
  draw->port_xl = 0.0;
  draw->port_xr = 1.0;
  draw->port_yl = 0.0;
  draw->port_yr = 1.0;
  draw->popup   = NULL;

  PetscCall(PetscOptionsGetReal(NULL, NULL, "-draw_pause", &dpause, &flag));
  if (flag) draw->pause = dpause;

  draw->savefilename   = NULL;
  draw->saveimageext   = NULL;
  draw->savemovieext   = NULL;
  draw->savefilecount  = 0;
  draw->savesinglefile = PETSC_FALSE;
  draw->savemoviefps   = PETSC_DECIDE;

  PetscCall(PetscDrawSetCurrentPoint(draw, .5, .9));

  draw->boundbox_xl = .5;
  draw->boundbox_xr = .5;
  draw->boundbox_yl = .9;
  draw->boundbox_yr = .9;

  *indraw = draw;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawSetType - Builds graphics object for a particular implementation

   Collective

   Input Parameters:
+  draw      - the graphics context
-  type      - for example, `PETSC_DRAW_X`

   Options Database Key:
.  -draw_type  <type> - Sets the type; use -help for a list of available methods (for instance, x)

   Level: intermediate

   Note:
   See `PetscDrawSetFromOptions()` for additional options database keys

   See "petsc/include/petscdraw.h" for available methods (for instance,
   `PETSC_DRAW_X`, `PETSC_DRAW_TIKZ` or `PETSC_DRAW_IMAGE`)

.seealso: `PetscDraw`, `PETSC_DRAW_X`, `PETSC_DRAW_TIKZ`, `PETSC_DRAW_IMAGE`, `PetscDrawSetFromOptions()`, `PetscDrawCreate()`, `PetscDrawDestroy()`, `PetscDrawType`
@*/
PetscErrorCode PetscDrawSetType(PetscDraw draw, PetscDrawType type)
{
  PetscBool match;
  PetscBool flg = PETSC_FALSE;
  PetscErrorCode (*r)(PetscDraw);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidCharPointer(type, 2);

  PetscCall(PetscObjectTypeCompare((PetscObject)draw, type, &match));
  if (match) PetscFunctionReturn(PETSC_SUCCESS);

  /*  User requests no graphics */
  PetscCall(PetscOptionsHasName(((PetscObject)draw)->options, NULL, "-nox", &flg));

  /*
     This is not ideal, but it allows codes to continue to run if X graphics
   was requested but is not installed on this machine. Mostly this is for
   testing.
   */
#if !defined(PETSC_HAVE_X)
  if (!flg) {
    PetscCall(PetscStrcmp(type, PETSC_DRAW_X, &match));
    if (match) {
      PetscBool dontwarn = PETSC_TRUE;
      flg                = PETSC_TRUE;
      PetscCall(PetscOptionsHasName(NULL, NULL, "-nox_warning", &dontwarn));
      if (!dontwarn) PetscCall((*PetscErrorPrintf)("PETSc installed without X Windows on this machine\nproceeding without graphics\n"));
    }
  }
#endif
  if (flg) {
    PetscCall(PetscStrcmp(type, "tikz", &flg));
    if (!flg) type = PETSC_DRAW_NULL;
  }

  PetscCall(PetscStrcmp(type, PETSC_DRAW_NULL, &match));
  if (match) {
    PetscCall(PetscOptionsHasName(NULL, NULL, "-draw_double_buffer", NULL));
    PetscCall(PetscOptionsHasName(NULL, NULL, "-draw_virtual", NULL));
    PetscCall(PetscOptionsHasName(NULL, NULL, "-draw_fast", NULL));
    PetscCall(PetscOptionsHasName(NULL, NULL, "-draw_ports", NULL));
    PetscCall(PetscOptionsHasName(NULL, NULL, "-draw_coordinates", NULL));
  }

  PetscCall(PetscFunctionListFind(PetscDrawList, type, &r));
  PetscCheck(r, PETSC_COMM_SELF, PETSC_ERR_ARG_UNKNOWN_TYPE, "Unknown PetscDraw type given: %s", type);
  PetscTryTypeMethod(draw, destroy);
  PetscCall(PetscMemzero(draw->ops, sizeof(struct _PetscDrawOps)));
  PetscCall(PetscObjectChangeTypeName((PetscObject)draw, type));
  PetscCall((*r)(draw));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawGetType - Gets the `PetscDraw` type as a string from the `PetscDraw` object.

   Not Collective

   Input Parameter:
.  draw - Krylov context

   Output Parameter:
.  name - name of PetscDraw method

   Level: advanced

.seealso: `PetscDraw`, `PetscDrawType`, `PetscDrawSetType()`, `PetscDrawCreate()
@*/
PetscErrorCode PetscDrawGetType(PetscDraw draw, PetscDrawType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscValidPointer(type, 2);
  *type = ((PetscObject)draw)->type_name;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawRegister - Adds a method to the graphics package.

   Not Collective

   Input Parameters:
+  name_solver - name of a new user-defined graphics class
-  routine_create - routine to create method context

   Level: developer

   Note:
   `PetscDrawRegister()` may be called multiple times to add several user-defined graphics classes

   Sample usage:
.vb
   PetscDrawRegister("my_draw_type", MyDrawCreate);
.ve

   Then, your specific graphics package can be chosen with the procedural interface via
$     PetscDrawSetType(ksp,"my_draw_type")
   or at runtime via the option
$     -draw_type my_draw_type

.seealso: `PetscDraw`, `PetscDrawRegisterAll()`, `PetscDrawRegisterDestroy()`, `PetscDrawType`, `PetscDrawSetType()`
@*/
PetscErrorCode PetscDrawRegister(const char *sname, PetscErrorCode (*function)(PetscDraw))
{
  PetscFunctionBegin;
  PetscCall(PetscDrawInitializePackage());
  PetscCall(PetscFunctionListAdd(&PetscDrawList, sname, function));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawSetOptionsPrefix - Sets the prefix used for searching for all
   `PetscDraw` options in the database.

   Logically Collective

   Input Parameters:
+  draw - the draw context
-  prefix - the prefix to prepend to all option names

   Level: advanced

.seealso: `PetscDraw`, `PetscDrawSetFromOptions()`, `PetscDrawCreate()`
@*/
PetscErrorCode PetscDrawSetOptionsPrefix(PetscDraw draw, const char prefix[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscCall(PetscObjectSetOptionsPrefix((PetscObject)draw, prefix));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawSetFromOptions - Sets the graphics type from the options database.
      Defaults to a PETSc X Windows graphics.

   Collective

   Input Parameter:
.     draw - the graphics context

   Options Database Keys:
+   -nox - do not use X graphics (ignore graphics calls, but run program correctly)
.   -nox_warning - when X Windows support is not installed this prevents the warning message from being printed
.   -draw_pause <pause amount> -- -1 indicates wait for mouse input, -2 indicates pause when window is to be destroyed
.   -draw_marker_type - <x,point>
.   -draw_save [optional filename] - (X Windows only) saves each image before it is cleared to a file
.   -draw_save_final_image [optional filename] - (X Windows only) saves the final image displayed in a window
.   -draw_save_movie - converts image files to a movie  at the end of the run. See PetscDrawSetSave()
.   -draw_save_single_file - saves each new image in the same file, normally each new image is saved in a new file with 'filename/filename_%d.ext'
.   -draw_save_on_clear - saves an image on each clear, mainly for debugging
-   -draw_save_on_flush - saves an image on each flush, mainly for debugging

   Level: intermediate

   Note:
    Must be called after `PetscDrawCreate()` before the `PetscDraw` is used.

.seealso: `PetscDraw`, `PetscDrawCreate()`, `PetscDrawSetType()`, `PetscDrawSetSave()`, `PetscDrawSetSaveFinalImage()`, `PetscDrawPause()`, `PetscDrawSetPause()`
@*/
PetscErrorCode PetscDrawSetFromOptions(PetscDraw draw)
{
  PetscBool   flg, nox;
  char        vtype[256];
  const char *def;
#if !defined(PETSC_USE_WINDOWS_GRAPHICS) && !defined(PETSC_HAVE_X)
  PetscBool warn;
#endif

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);

  PetscCall(PetscDrawRegisterAll());

  if (((PetscObject)draw)->type_name) def = ((PetscObject)draw)->type_name;
  else {
    PetscCall(PetscOptionsHasName(((PetscObject)draw)->options, NULL, "-nox", &nox));
    def = PETSC_DRAW_NULL;
#if defined(PETSC_USE_WINDOWS_GRAPHICS)
    if (!nox) def = PETSC_DRAW_WIN32;
#elif defined(PETSC_HAVE_X)
    if (!nox) def = PETSC_DRAW_X;
#else
    PetscCall(PetscOptionsHasName(NULL, NULL, "-nox_warning", &warn));
    if (!nox && !warn) PetscCall((*PetscErrorPrintf)("PETSc installed without X Windows or Microsoft Graphics on this machine\nproceeding without graphics\n"));
#endif
  }
  PetscObjectOptionsBegin((PetscObject)draw);
  PetscCall(PetscOptionsFList("-draw_type", "Type of graphical output", "PetscDrawSetType", PetscDrawList, def, vtype, 256, &flg));
  if (flg) {
    PetscCall(PetscDrawSetType(draw, vtype));
  } else if (!((PetscObject)draw)->type_name) {
    PetscCall(PetscDrawSetType(draw, def));
  }
  PetscCall(PetscOptionsName("-nox", "Run without graphics", "None", &nox));
  {
    char      filename[PETSC_MAX_PATH_LEN];
    char      movieext[32];
    PetscBool image, movie;
    PetscCall(PetscSNPrintf(filename, sizeof(filename), "%s%s", draw->savefilename ? draw->savefilename : "", draw->saveimageext ? draw->saveimageext : ""));
    PetscCall(PetscSNPrintf(movieext, sizeof(movieext), "%s", draw->savemovieext ? draw->savemovieext : ""));
    PetscCall(PetscOptionsString("-draw_save", "Save graphics to image file", "PetscDrawSetSave", filename, filename, sizeof(filename), &image));
    PetscCall(PetscOptionsString("-draw_save_movie", "Make a movie from saved images", "PetscDrawSetSaveMovie", movieext, movieext, sizeof(movieext), &movie));
    PetscCall(PetscOptionsInt("-draw_save_movie_fps", "Set frames per second in saved movie", PETSC_FUNCTION_NAME, draw->savemoviefps, &draw->savemoviefps, NULL));
    PetscCall(PetscOptionsBool("-draw_save_single_file", "Each new image replaces previous image in file", PETSC_FUNCTION_NAME, draw->savesinglefile, &draw->savesinglefile, NULL));
    if (image) PetscCall(PetscDrawSetSave(draw, filename));
    if (movie) PetscCall(PetscDrawSetSaveMovie(draw, movieext));
    PetscCall(PetscOptionsString("-draw_save_final_image", "Save final graphics to image file", "PetscDrawSetSaveFinalImage", filename, filename, sizeof(filename), &image));
    if (image) PetscCall(PetscDrawSetSaveFinalImage(draw, filename));
    PetscCall(PetscOptionsBool("-draw_save_on_clear", "Save graphics to file on each clear", PETSC_FUNCTION_NAME, draw->saveonclear, &draw->saveonclear, NULL));
    PetscCall(PetscOptionsBool("-draw_save_on_flush", "Save graphics to file on each flush", PETSC_FUNCTION_NAME, draw->saveonflush, &draw->saveonflush, NULL));
  }
  PetscCall(PetscOptionsReal("-draw_pause", "Amount of time that program pauses after plots", "PetscDrawSetPause", draw->pause, &draw->pause, NULL));
  PetscCall(PetscOptionsEnum("-draw_marker_type", "Type of marker to use on plots", "PetscDrawSetMarkerType", PetscDrawMarkerTypes, (PetscEnum)draw->markertype, (PetscEnum *)&draw->markertype, NULL));

  /* process any options handlers added with PetscObjectAddOptionsHandler() */
  PetscCall(PetscObjectProcessOptionsHandlers((PetscObject)draw, PetscOptionsObject));

  PetscCall(PetscDrawViewFromOptions(draw, NULL, "-draw_view"));
  PetscOptionsEnd();
  PetscFunctionReturn(PETSC_SUCCESS);
}

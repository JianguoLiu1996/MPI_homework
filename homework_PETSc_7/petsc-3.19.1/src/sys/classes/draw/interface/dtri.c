
#include <petsc/private/drawimpl.h> /*I "petscdraw.h" I*/

/*@
   PetscDrawTriangle - draws a triangle  onto a drawable.

   Not Collective

   Input Parameters:
+  draw - the drawing context
.  x1 - coordinate of the first vertex
.  y1 - coordinate of the first vertex
.  x2 - coordinate of the second vertex
.  y2 - coordinate of the second vertex
.  x3 - coordinate of the third vertex
.  y3 - coordinate of the third vertex
.  c1 - color of the first vertex
.  c2 - color of the second vertex
-  c3 - color of the third vertext

   Level: beginner

.seealso: `PetscDraw`, `PetscDrawLine()`, `PetscDrawRectangle()`, `PetscDrawEllipse()`, `PetscDrawMarker()`, `PetscDrawPoint()`, `PetscDrawArrow()`
@*/
PetscErrorCode PetscDrawTriangle(PetscDraw draw, PetscReal x1, PetscReal y_1, PetscReal x2, PetscReal y2, PetscReal x3, PetscReal y3, int c1, int c2, int c3)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscUseTypeMethod(draw, triangle, x1, y_1, x2, y2, x3, y3, c1, c2, c3);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawScalePopup - draws a contour scale window.

   Collective

   Input Parameters:
+  popup - the window (often a window obtained via `PetscDrawGetPopup()`
.  min - minimum value being plotted
-  max - maximum value being plotted

   Level: intermediate

   Note:
    All processors that share the draw MUST call this routine

.seealso: `PetscDraw`, `PetscDrawGetPopup()`, `PetscDrawTensorContour()`
@*/
PetscErrorCode PetscDrawScalePopup(PetscDraw popup, PetscReal min, PetscReal max)
{
  PetscBool   isnull;
  PetscReal   xl = 0.0, yl = 0.0, xr = 1.0, yr = 1.0;
  PetscMPIInt rank;
  int         i;
  char        string[32];

  PetscFunctionBegin;
  if (!popup) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidHeaderSpecific(popup, PETSC_DRAW_CLASSID, 1);
  PetscCall(PetscDrawIsNull(popup, &isnull));
  if (isnull) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCallMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)popup), &rank));

  PetscCall(PetscDrawCheckResizedWindow(popup));
  PetscCall(PetscDrawClear(popup));
  PetscCall(PetscDrawSetTitle(popup, "Contour Scale"));
  PetscCall(PetscDrawSetCoordinates(popup, xl, yl, xr, yr));
  PetscDrawCollectiveBegin(popup);
  if (rank == 0) {
    for (i = 0; i < 10; i++) {
      int c = PetscDrawRealToColor((PetscReal)i / 9, 0, 1);
      PetscCall(PetscDrawRectangle(popup, xl, yl, xr, yr, c, c, c, c));
      yl += 0.1;
    }
    for (i = 0; i < 10; i++) {
      PetscReal value = min + i * (max - min) / 9;
      /* look for a value that should be zero, but is not due to round-off */
      if (PetscAbsReal(value) < 1.e-10 && max - min > 1.e-6) value = 0.0;
      PetscCall(PetscSNPrintf(string, sizeof(string), "%18.16e", (double)value));
      PetscCall(PetscDrawString(popup, 0.2, 0.02 + i / 10.0, PETSC_DRAW_BLACK, string));
    }
  }
  PetscDrawCollectiveEnd(popup);
  PetscCall(PetscDrawFlush(popup));
  PetscCall(PetscDrawSave(popup));
  PetscFunctionReturn(PETSC_SUCCESS);
}

typedef struct {
  int        m, n;
  PetscReal *x, *y, min, max, *v;
  PetscBool  showgrid;
} ZoomCtx;

static PetscErrorCode PetscDrawTensorContour_Zoom(PetscDraw win, void *dctx)
{
  int      i;
  ZoomCtx *ctx = (ZoomCtx *)dctx;

  PetscFunctionBegin;
  PetscCall(PetscDrawTensorContourPatch(win, ctx->m, ctx->n, ctx->x, ctx->y, ctx->min, ctx->max, ctx->v));
  if (ctx->showgrid) {
    for (i = 0; i < ctx->m; i++) PetscCall(PetscDrawLine(win, ctx->x[i], ctx->y[0], ctx->x[i], ctx->y[ctx->n - 1], PETSC_DRAW_BLACK));
    for (i = 0; i < ctx->n; i++) PetscCall(PetscDrawLine(win, ctx->x[0], ctx->y[i], ctx->x[ctx->m - 1], ctx->y[i], PETSC_DRAW_BLACK));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDrawTensorContour - draws a contour plot for a two-dimensional array

   Collective on draw, but draw must be sequential

   Input Parameters:
+   draw  - the draw context
.   m,n   - the global number of mesh points in the x and y directions
.   xi    - the locations of the global mesh points in the horizontal direction (optional, use `NULL` to indicate uniform spacing on [0,1])
.   yi    - the locations of the global mesh points in the vertical direction (optional, use `NULL` to indicate uniform spacing on [0,1])
-   V     - the values

   Options Database Keys:
+  -draw_x_shared_colormap - Indicates use of private colormap
-  -draw_contour_grid - draws grid contour

   Level: intermediate

.seealso: `PetscDraw`, `PetscDrawTensorContourPatch()`, `PetscDrawScalePopup()`
@*/
PetscErrorCode PetscDrawTensorContour(PetscDraw draw, int m, int n, const PetscReal xi[], const PetscReal yi[], PetscReal *v)
{
  int         N = m * n;
  PetscBool   isnull;
  PetscDraw   popup;
  int         xin = 1, yin = 1, i;
  PetscMPIInt size;
  PetscReal   h;
  ZoomCtx     ctx;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  PetscCall(PetscDrawIsNull(draw, &isnull));
  if (isnull) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCallMPI(MPI_Comm_size(PetscObjectComm((PetscObject)draw), &size));
  PetscCheck(size <= 1, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "May only be used with single processor PetscDraw");
  PetscCheck(N > 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "n %d and m %d must be positive", m, n);

  ctx.v   = v;
  ctx.m   = m;
  ctx.n   = n;
  ctx.max = ctx.min = v[0];
  for (i = 0; i < N; i++) {
    if (ctx.max < ctx.v[i]) ctx.max = ctx.v[i];
    if (ctx.min > ctx.v[i]) ctx.min = ctx.v[i];
  }
  if (ctx.max - ctx.min < 1.e-7) {
    ctx.min -= 5.e-8;
    ctx.max += 5.e-8;
  }

  /* PetscDraw the scale window */
  PetscCall(PetscDrawGetPopup(draw, &popup));
  PetscCall(PetscDrawScalePopup(popup, ctx.min, ctx.max));

  ctx.showgrid = PETSC_FALSE;
  PetscCall(PetscOptionsGetBool(((PetscObject)draw)->options, NULL, "-draw_contour_grid", &ctx.showgrid, NULL));

  /* fill up x and y coordinates */
  if (!xi) {
    xin = 0;
    PetscCall(PetscMalloc1(ctx.m, &ctx.x));
    h        = 1.0 / (ctx.m - 1);
    ctx.x[0] = 0.0;
    for (i = 1; i < ctx.m; i++) ctx.x[i] = ctx.x[i - 1] + h;
  } else ctx.x = (PetscReal *)xi;

  if (!yi) {
    yin = 0;
    PetscCall(PetscMalloc1(ctx.n, &ctx.y));
    h        = 1.0 / (ctx.n - 1);
    ctx.y[0] = 0.0;
    for (i = 1; i < ctx.n; i++) ctx.y[i] = ctx.y[i - 1] + h;
  } else ctx.y = (PetscReal *)yi;

  PetscCall(PetscDrawZoom(draw, PetscDrawTensorContour_Zoom, &ctx));

  if (!xin) PetscCall(PetscFree(ctx.x));
  if (!yin) PetscCall(PetscFree(ctx.y));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscDrawTensorContourPatch - draws a rectangular patch of a contour plot
   for a two-dimensional array.

   Not Collective

   Input Parameters:
+  draw - the draw context
.  m,n - the number of local mesh points in the x and y direction
.  x - the horizontal locations of the local mesh points
.  y - the vertical locations of the local mesh points
.  min - the minimum value in the entire contour
.  max - the maximum value in the entire contour
-  v - the data

   Options Database Key:
.  -draw_x_shared_colormap - Activates private colormap

   Level: advanced

   Note:
   This is a lower level support routine, usually the user will call
   `PetscDrawTensorContour()`.

.seealso: `PetscDraw`, `PetscDrawTensorContour()`
@*/
PetscErrorCode PetscDrawTensorContourPatch(PetscDraw draw, int m, int n, PetscReal *x, PetscReal *y, PetscReal min, PetscReal max, PetscReal *v)
{
  int       c1, c2, c3, c4, i, j;
  PetscReal x1, x2, x3, x4, y_1, y2, y3, y4;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw, PETSC_DRAW_CLASSID, 1);
  /* PetscDraw the contour plot patch */
  for (j = 0; j < n - 1; j++) {
    for (i = 0; i < m - 1; i++) {
      x1  = x[i];
      y_1 = y[j];
      c1  = PetscDrawRealToColor(v[i + j * m], min, max);
      x2  = x[i + 1];
      y2  = y_1;
      c2  = PetscDrawRealToColor(v[i + j * m + 1], min, max);
      x3  = x2;
      y3  = y[j + 1];
      c3  = PetscDrawRealToColor(v[i + j * m + 1 + m], min, max);
      x4  = x1;
      y4  = y3;
      c4  = PetscDrawRealToColor(v[i + j * m + m], min, max);

      PetscCall(PetscDrawTriangle(draw, x1, y_1, x2, y2, x3, y3, c1, c2, c3));
      PetscCall(PetscDrawTriangle(draw, x1, y_1, x3, y3, x4, y4, c1, c3, c4));
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

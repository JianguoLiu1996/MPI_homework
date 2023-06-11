/*
    Defines the operations for the TikZ PetscDraw implementation.
*/

#include <petsc/private/drawimpl.h> /*I  "petscsys.h" I*/

typedef struct {
  char     *filename;
  FILE     *fd;
  PetscBool written; /* something has been written to the current frame */
} PetscDraw_TikZ;

#define TikZ_BEGIN_DOCUMENT \
  "\\documentclass{beamer}\n\n\
\\usepackage{tikz}\n\
\\usepackage{pgflibraryshapes}\n\
\\usetikzlibrary{backgrounds}\n\
\\usetikzlibrary{arrows}\n\
\\newenvironment{changemargin}[2]{%%\n\
  \\begin{list}{}{%%\n\
    \\setlength{\\topsep}{0pt}%%\n\
    \\setlength{\\leftmargin}{#1}%%\n\
    \\setlength{\\rightmargin}{#2}%%\n\
    \\setlength{\\listparindent}{\\parindent}%%\n\
    \\setlength{\\itemindent}{\\parindent}%%\n\
    \\setlength{\\parsep}{\\parskip}%%\n\
  }%%\n\
  \\item[]}{\\end{list}}\n\n\
\\begin{document}\n"

#define TikZ_BEGIN_FRAME \
  "\\begin{frame}{}\n\
\\begin{changemargin}{-1cm}{0cm}\n\
\\begin{center}\n\
\\begin{tikzpicture}[scale = 10.00,font=\\fontsize{8}{8}\\selectfont]\n"

#define TikZ_END_FRAME \
  "\\end{tikzpicture}\n\
\\end{center}\n\
\\end{changemargin}\n\
\\end{frame}\n"

#define TikZ_END_DOCUMENT "\\end{document}\n"

static PetscErrorCode PetscDrawDestroy_TikZ(PetscDraw draw)
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;

  PetscFunctionBegin;
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, TikZ_END_FRAME));
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, TikZ_END_DOCUMENT));
  PetscCall(PetscFClose(PetscObjectComm((PetscObject)draw), win->fd));
  PetscCall(PetscFree(win->filename));
  PetscCall(PetscFree(draw->data));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static const char *TikZColors[] = {"white", "black", "red", "green", "cyan", "blue", "magenta", NULL, NULL, "orange", "violet", "brown", "pink", NULL, "yellow", NULL};

static inline const char *TikZColorMap(int cl)
{
  return ((cl < 16) ? (TikZColors[cl] ? TikZColors[cl] : "black") : "black");
}

/*
     These macros transform from the users coordinates to the (0,0) -> (1,1) coordinate system
*/
#define XTRANS(draw, x) (double)(((draw)->port_xl + (((x - (draw)->coor_xl) * ((draw)->port_xr - (draw)->port_xl)) / ((draw)->coor_xr - (draw)->coor_xl))))
#define YTRANS(draw, y) (double)(((draw)->port_yl + (((y - (draw)->coor_yl) * ((draw)->port_yr - (draw)->port_yl)) / ((draw)->coor_yr - (draw)->coor_yl))))

static PetscErrorCode PetscDrawClear_TikZ(PetscDraw draw)
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;
  PetscBool       written;

  PetscFunctionBegin;
  /* often PETSc generates unneeded clears, we want avoid creating empty pictures for them */
  PetscCall(MPIU_Allreduce(&win->written, &written, 1, MPIU_BOOL, MPI_LOR, PetscObjectComm((PetscObject)(draw))));
  if (!written) PetscFunctionReturn(PETSC_SUCCESS);
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, TikZ_END_FRAME));
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, TikZ_BEGIN_FRAME));
  win->written = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDrawLine_TikZ(PetscDraw draw, PetscReal xl, PetscReal yl, PetscReal xr, PetscReal yr, int cl)
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;

  PetscFunctionBegin;
  win->written = PETSC_TRUE;
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, "\\draw [%s] (%g,%g) --(%g,%g);\n", TikZColorMap(cl), XTRANS(draw, xl), YTRANS(draw, yl), XTRANS(draw, xr), YTRANS(draw, yr)));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDrawRectangle_TikZ(PetscDraw draw, PetscReal xl, PetscReal yl, PetscReal xr, PetscReal yr, int c1, int c2, int c3, int c4)
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;

  PetscFunctionBegin;
  win->written = PETSC_TRUE;
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, "\\fill [bottom color=%s,top color=%s] (%g,%g) rectangle (%g,%g);\n", TikZColorMap(c1), TikZColorMap(c4), XTRANS(draw, xl), YTRANS(draw, yl), XTRANS(draw, xr), YTRANS(draw, yr)));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDrawTriangle_TikZ(PetscDraw draw, PetscReal x1, PetscReal y1, PetscReal x2, PetscReal y2, PetscReal x3, PetscReal y3, int c1, int c2, int c3)
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;

  PetscFunctionBegin;
  win->written = PETSC_TRUE;
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, "\\fill [color=%s] (%g,%g) -- (%g,%g) -- (%g,%g) -- cycle;\n", TikZColorMap(c1), XTRANS(draw, x1), YTRANS(draw, y1), XTRANS(draw, x2), YTRANS(draw, y2), XTRANS(draw, x3), YTRANS(draw, y3)));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDrawEllipse_TikZ(PetscDraw draw, PetscReal x, PetscReal y, PetscReal a, PetscReal b, int c)
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;
  PetscReal       rx, ry;

  PetscFunctionBegin;
  win->written = PETSC_TRUE;
  rx           = a / 2 * (draw->port_xr - draw->port_xl) / (draw->coor_xr - draw->coor_xl);
  ry           = b / 2 * (draw->port_yr - draw->port_yl) / (draw->coor_yr - draw->coor_yl);
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, "\\fill [color=%s] (%g,%g) circle [x radius=%g,y radius=%g];\n", TikZColorMap(c), XTRANS(draw, x), YTRANS(draw, y), (double)rx, (double)ry));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDrawString_TikZ(PetscDraw draw, PetscReal xl, PetscReal yl, int cl, const char text[])
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;

  PetscFunctionBegin;
  win->written = PETSC_TRUE;
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, "\\node [above right, %s] at (%g,%g) {%s};\n", TikZColorMap(cl), XTRANS(draw, xl), YTRANS(draw, yl), text));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDrawStringVertical_TikZ(PetscDraw draw, PetscReal xl, PetscReal yl, int cl, const char text[])
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;
  size_t          len;
  PetscReal       width;

  PetscFunctionBegin;
  win->written = PETSC_TRUE;
  PetscCall(PetscStrlen(text, &len));
  PetscCall(PetscDrawStringGetSize(draw, &width, NULL));
  yl = yl - len * width * (draw->coor_yr - draw->coor_yl) / (draw->coor_xr - draw->coor_xl);
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, "\\node [rotate=90, %s] at (%g,%g) {%s};\n", TikZColorMap(cl), XTRANS(draw, xl), YTRANS(draw, yl), text));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
    Does not handle multiline strings correctly
*/
static PetscErrorCode PetscDrawStringBoxed_TikZ(PetscDraw draw, PetscReal xl, PetscReal yl, int cl, int ct, const char text[], PetscReal *w, PetscReal *h)
{
  PetscDraw_TikZ *win = (PetscDraw_TikZ *)draw->data;
  size_t          len;

  PetscFunctionBegin;
  win->written = PETSC_TRUE;
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, "\\draw (%g,%g) node [rectangle, draw, align=center, inner sep=1ex] {%s};\n", XTRANS(draw, xl), YTRANS(draw, yl), text));

  /* make up totally bogus height and width of box */
  PetscCall(PetscStrlen(text, &len));
  if (w) *w = .07 * len;
  if (h) *h = .07;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode PetscDrawStringGetSize_TikZ(PetscDraw draw, PetscReal *x, PetscReal *y)
{
  PetscFunctionBegin;
  if (x) *x = .014 * (draw->coor_xr - draw->coor_xl) / ((draw->port_xr - draw->port_xl));
  if (y) *y = .05 * (draw->coor_yr - draw->coor_yl) / ((draw->port_yr - draw->port_yl));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static struct _PetscDrawOps DvOps = {NULL, NULL, PetscDrawLine_TikZ, NULL, NULL, NULL, NULL, PetscDrawString_TikZ, PetscDrawStringVertical_TikZ, NULL, PetscDrawStringGetSize_TikZ, NULL, PetscDrawClear_TikZ, PetscDrawRectangle_TikZ, PetscDrawTriangle_TikZ, PetscDrawEllipse_TikZ, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, PetscDrawDestroy_TikZ, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, PetscDrawStringBoxed_TikZ, NULL};

PETSC_EXTERN PetscErrorCode PetscDrawCreate_TikZ(PetscDraw draw)
{
  PetscDraw_TikZ *win;

  PetscFunctionBegin;
  PetscCall(PetscMemcpy(draw->ops, &DvOps, sizeof(DvOps)));
  PetscCall(PetscNew(&win));

  draw->data = (void *)win;

  if (draw->title) {
    PetscCall(PetscStrallocpy(draw->title, &win->filename));
  } else {
    const char *fname;
    PetscCall(PetscObjectGetName((PetscObject)draw, &fname));
    PetscCall(PetscStrallocpy(fname, &win->filename));
  }
  PetscCall(PetscFOpen(PetscObjectComm((PetscObject)draw), win->filename, "w", &win->fd));
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, TikZ_BEGIN_DOCUMENT));
  PetscCall(PetscFPrintf(PetscObjectComm((PetscObject)draw), win->fd, TikZ_BEGIN_FRAME));

  win->written = PETSC_FALSE;
  PetscFunctionReturn(PETSC_SUCCESS);
}

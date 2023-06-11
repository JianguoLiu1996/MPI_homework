#include <petscsys.h>

struct _PetscSegBufferLink {
  struct _PetscSegBufferLink *tail;
  size_t                      alloc;
  size_t                      used;
  size_t                      tailused;
  union
  { /* Dummy types to ensure alignment */
    PetscReal dummy_real;
    PetscInt  dummy_int;
    char      array[1]; /* This array is over-allocated for the size of the link */
  } u;
};

/* Segmented (extendable) array implementation */
struct _n_PetscSegBuffer {
  struct _PetscSegBufferLink *head;
  size_t                      unitbytes;
};

static PetscErrorCode PetscSegBufferAlloc_Private(PetscSegBuffer seg, size_t count)
{
  size_t                      alloc;
  struct _PetscSegBufferLink *newlink, *s;

  PetscFunctionBegin;
  s = seg->head;
  /* Grow at least fast enough to hold next item, like Fibonacci otherwise (up to 1MB chunks) */
  alloc = PetscMax(s->used + count, PetscMin(1000000 / seg->unitbytes + 1, s->alloc + s->tailused));
  PetscCall(PetscMalloc(offsetof(struct _PetscSegBufferLink, u) + alloc * seg->unitbytes, &newlink));
  PetscCall(PetscMemzero(newlink, offsetof(struct _PetscSegBufferLink, u)));

  newlink->tailused = s->used + s->tailused;
  newlink->tail     = s;
  newlink->alloc    = alloc;
  seg->head         = newlink;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSegBufferCreate - create a segmented buffer

   Not Collective

   Input Parameters:
+  unitbytes - number of bytes that each entry will contain
-  expected - expected/typical number of entries

   Output Parameter:
.  seg - `PetscSegBuffer` object

   Level: developer

.seealso: `PetscSegBufferGet()`, `PetscSegBufferExtractAlloc()`, `PetscSegBufferExtractTo()`, `PetscSegBufferExtractInPlace()`, `PetscSegBufferDestroy()`,
          `PetscSegBuffer`
@*/
PetscErrorCode PetscSegBufferCreate(size_t unitbytes, size_t expected, PetscSegBuffer *seg)
{
  struct _PetscSegBufferLink *head;

  PetscFunctionBegin;
  PetscCall(PetscNew(seg));
  PetscCall(PetscMalloc(offsetof(struct _PetscSegBufferLink, u) + expected * unitbytes, &head));
  PetscCall(PetscMemzero(head, offsetof(struct _PetscSegBufferLink, u)));

  head->alloc       = expected;
  (*seg)->unitbytes = unitbytes;
  (*seg)->head      = head;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSegBufferGet - get new buffer space from a segmented buffer

   Not Collective

   Input Parameters:
+  seg - `PetscSegBuffer` buffer
-  count - number of entries needed

   Output Parameter:
.  buf - address of new buffer for contiguous data

   Level: developer

.seealso: `PetscSegBufferCreate()`, `PetscSegBufferExtractAlloc()`, `PetscSegBufferExtractTo()`, `PetscSegBufferExtractInPlace()`, `PetscSegBufferDestroy()`,
          `PetscSegBuffer`
@*/
PetscErrorCode PetscSegBufferGet(PetscSegBuffer seg, size_t count, void *buf)
{
  struct _PetscSegBufferLink *s;

  PetscFunctionBegin;
  s = seg->head;
  if (PetscUnlikely(s->used + count > s->alloc)) PetscCall(PetscSegBufferAlloc_Private(seg, count));
  s             = seg->head;
  *(char **)buf = &s->u.array[s->used * seg->unitbytes];
  s->used += count;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSegBufferDestroy - destroy segmented buffer

   Not Collective

   Input Parameter:
.  seg - address of segmented buffer object

   Level: developer

.seealso: `PetscSegBuffer`, `PetscSegBufferCreate()`
@*/
PetscErrorCode PetscSegBufferDestroy(PetscSegBuffer *seg)
{
  struct _PetscSegBufferLink *s;

  PetscFunctionBegin;
  if (!*seg) PetscFunctionReturn(PETSC_SUCCESS);
  for (s = (*seg)->head; s;) {
    struct _PetscSegBufferLink *tail = s->tail;
    PetscCall(PetscFree(s));
    s = tail;
  }
  PetscCall(PetscFree(*seg));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSegBufferExtractTo - extract contiguous data to provided buffer and reset segmented buffer

   Not Collective

   Input Parameters:
+  seg - segmented buffer
-  contig - allocated buffer to hold contiguous data

   Level: developer

.seealso: `PetscSegBufferCreate()`, `PetscSegBufferGet()`, `PetscSegBufferDestroy()`, `PetscSegBufferExtractAlloc()`, `PetscSegBufferExtractInPlace()`,
          `PetscSegBuffer`
@*/
PetscErrorCode PetscSegBufferExtractTo(PetscSegBuffer seg, void *contig)
{
  size_t                      unitbytes;
  struct _PetscSegBufferLink *s, *t;
  char                       *ptr;

  PetscFunctionBegin;
  unitbytes = seg->unitbytes;
  s         = seg->head;
  ptr       = ((char *)contig) + s->tailused * unitbytes;
  PetscCall(PetscMemcpy(ptr, s->u.array, s->used * unitbytes));
  for (t = s->tail; t;) {
    struct _PetscSegBufferLink *tail = t->tail;
    ptr -= t->used * unitbytes;
    PetscCall(PetscMemcpy(ptr, t->u.array, t->used * unitbytes));
    PetscCall(PetscFree(t));
    t = tail;
  }
  PetscCheck(ptr == contig, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Tail count does not match");
  s->used     = 0;
  s->tailused = 0;
  s->tail     = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSegBufferExtractAlloc - extract contiguous data to new allocation and reset segmented buffer

   Not Collective

   Input Parameter:
.  seg - `PetscSegBuffer` buffer

   Output Parameter:
.  contiguous - address of new array containing contiguous data, caller frees with `PetscFree()`

   Level: developer

   Developer Note:
   'seg' argument is a pointer so that implementation could reallocate, though this is not currently done

.seealso: `PetscSegBufferCreate()`, `PetscSegBufferGet()`, `PetscSegBufferDestroy()`, `PetscSegBufferExtractTo()`, `PetscSegBufferExtractInPlace()`,
          `PetscSegBuffer`
@*/
PetscErrorCode PetscSegBufferExtractAlloc(PetscSegBuffer seg, void *contiguous)
{
  struct _PetscSegBufferLink *s;
  void                       *contig;

  PetscFunctionBegin;
  s = seg->head;

  PetscCall(PetscMalloc((s->used + s->tailused) * seg->unitbytes, &contig));
  PetscCall(PetscSegBufferExtractTo(seg, contig));
  *(void **)contiguous = contig;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSegBufferExtractInPlace - extract in-place contiguous representation of data and reset segmented buffer for reuse

   Not Collective

   Input Parameter:
.  seg - `PetscSegBuffer` object

   Output Parameter:
.  contig - address of pointer to contiguous memory, may be `NULL`

   Level: developer

.seealso: `PetscSegBuffer`, `PetscSegBufferExtractAlloc()`, `PetscSegBufferExtractTo()`
@*/
PetscErrorCode PetscSegBufferExtractInPlace(PetscSegBuffer seg, void *contig)
{
  struct _PetscSegBufferLink *head;

  PetscFunctionBegin;
  head = seg->head;
  if (PetscUnlikely(head->tail)) {
    PetscSegBuffer newseg;

    PetscCall(PetscSegBufferCreate(seg->unitbytes, head->used + head->tailused, &newseg));
    PetscCall(PetscSegBufferExtractTo(seg, newseg->head->u.array));
    seg->head    = newseg->head;
    newseg->head = head;
    PetscCall(PetscSegBufferDestroy(&newseg));
    head = seg->head;
  }
  if (contig) *(char **)contig = head->u.array;
  head->used = 0;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSegBufferGetSize - get currently used size of a `PetscSegBuffer`

   Not Collective

   Input Parameter:
.  seg - `PetscSegBuffer` object

   Output Parameter:
.  usedsize - number of used units

   Level: developer

.seealso: `PetscSegBuffer`, `PetscSegBufferExtractAlloc()`, `PetscSegBufferExtractTo()`, `PetscSegBufferCreate()`, `PetscSegBufferGet()`
@*/
PetscErrorCode PetscSegBufferGetSize(PetscSegBuffer seg, size_t *usedsize)
{
  PetscFunctionBegin;
  *usedsize = seg->head->tailused + seg->head->used;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscSegBufferUnuse - return some unused entries obtained with an overzealous `PetscSegBufferGet()`

   Not Collective

   Input Parameters:
+  seg - `PetscSegBuffer` object
-  unused - number of unused units

   Level: developer

.seealso: `PetscSegBuffer`, `PetscSegBufferCreate()`, `PetscSegBufferGet()`
@*/
PetscErrorCode PetscSegBufferUnuse(PetscSegBuffer seg, size_t unused)
{
  struct _PetscSegBufferLink *head;

  PetscFunctionBegin;
  head = seg->head;
  PetscCheck(head->used >= unused, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Attempt to return more unused entries (%zu) than previously gotten (%zu)", unused, head->used);
  head->used -= unused;
  PetscFunctionReturn(PETSC_SUCCESS);
}

#include <petscsys.h> /*I  "petscsys.h"  I*/
#include <petsc/private/petscimpl.h>

static inline int Compare_PetscMPIInt_Private(const void *left, const void *right, PETSC_UNUSED void *ctx)
{
  PetscMPIInt l = *(PetscMPIInt *)left, r = *(PetscMPIInt *)right;
  return (l < r) ? -1 : (l > r);
}

static inline int Compare_PetscInt_Private(const void *left, const void *right, PETSC_UNUSED void *ctx)
{
  PetscInt l = *(PetscInt *)left, r = *(PetscInt *)right;
  return (l < r) ? -1 : (l > r);
}

static inline int Compare_PetscReal_Private(const void *left, const void *right, PETSC_UNUSED void *ctx)
{
  PetscReal l = *(PetscReal *)left, r = *(PetscReal *)right;
  return (l < r) ? -1 : (l > r);
}

#define MIN_GALLOP_CONST_GLOBAL 8
static PetscInt MIN_GALLOP_GLOBAL = MIN_GALLOP_CONST_GLOBAL;
typedef int (*CompFunc)(const void *, const void *, void *);

/* Mostly to force clang uses the builtins, but can't hurt to have gcc compilers also do the same */
#if !defined(PETSC_USE_DEBUG) && defined(__GNUC__)
static inline void COPYSWAPPY(char *a, char *b, char *t, size_t size)
{
  __builtin_memcpy(t, b, size);
  __builtin_memmove(b, a, size);
  __builtin_memcpy(a, t, size);
  return;
}

static inline void COPYSWAPPY2(char *al, char *ar, size_t asize, char *bl, char *br, size_t bsize, char *t)
{
  __builtin_memcpy(t, ar, asize);
  __builtin_memmove(ar, al, asize);
  __builtin_memcpy(al, t, asize);
  __builtin_memcpy(t, br, bsize);
  __builtin_memmove(br, bl, bsize);
  __builtin_memcpy(bl, t, bsize);
  return;
}

static inline void Petsc_memcpy(char *dest, const char *src, size_t size)
{
  __builtin_memcpy(dest, src, size);
  return;
}

static inline void Petsc_memcpy2(char *adest, const char *asrc, size_t asize, char *bdest, const char *bsrc, size_t bsize)
{
  __builtin_memcpy(adest, asrc, asize);
  __builtin_memcpy(bdest, bsrc, bsize);
  return;
}

static inline void Petsc_memmove(char *dest, const char *src, size_t size)
{
  __builtin_memmove(dest, src, size);
  return;
}

static inline void Petsc_memmove2(char *adest, const char *asrc, size_t asize, char *bdest, const char *bsrc, size_t bsize)
{
  __builtin_memmove(adest, asrc, asize);
  __builtin_memmove(bdest, bsrc, bsize);
  return;
}
#else
static inline void COPYSWAPPY(char *a, char *b, char *t, size_t size)
{
  PetscFunctionBegin;
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(t, b, size));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemmove(b, a, size));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(a, t, size));
  PetscFunctionReturnVoid();
}

static inline void COPYSWAPPY2(char *al, char *ar, size_t asize, char *bl, char *br, size_t bsize, char *t)
{
  PetscFunctionBegin;
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(t, ar, asize));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemmove(ar, al, asize));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(al, t, asize));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(t, br, bsize));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemmove(br, bl, bsize));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(bl, t, bsize));
  PetscFunctionReturnVoid();
}

static inline void Petsc_memcpy(char *dest, const char *src, size_t size)
{
  PetscFunctionBegin;
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(dest, src, size));
  PetscFunctionReturnVoid();
}

static inline void Petsc_memcpy2(char *adest, const char *asrc, size_t asize, char *bdest, const char *bsrc, size_t bsize)
{
  PetscFunctionBegin;
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(adest, asrc, asize));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemcpy(bdest, bsrc, bsize));
  PetscFunctionReturnVoid();
}

static inline void Petsc_memmove(char *dest, const char *src, size_t size)
{
  PetscFunctionBegin;
  PetscCallAbort(PETSC_COMM_SELF, PetscMemmove(dest, src, size));
  PetscFunctionReturnVoid();
}

static inline void Petsc_memmove2(char *adest, const char *asrc, size_t asize, char *bdest, const char *bsrc, size_t bsize)
{
  PetscFunctionBegin;
  PetscCallAbort(PETSC_COMM_SELF, PetscMemmove(adest, asrc, asize));
  PetscCallAbort(PETSC_COMM_SELF, PetscMemmove(bdest, bsrc, bsize));
  PetscFunctionReturnVoid();
}
#endif

/* Start left look right. Looking for e.g. B[0] in A or mergelo. l inclusive, r inclusive. Returns first m such that arr[m] >
 x. Output also inclusive.

 NOTE: Both gallopsearch functions CANNOT distinguish between inserting AFTER the entire array vs inserting at entry n!! For example for an array:

   {0,1,2,3,4,5}

   when looking to insert "5" this routine will return *m = 6, but when looking to insert "6" it will ALSO return *m = 6.
  */
static inline PetscErrorCode PetscGallopSearchLeft_Private(const char *arr, size_t size, CompFunc cmp, void *ctx, PetscInt l, PetscInt r, const char *x, PetscInt *m)
{
  PetscInt last = l, k = 1, mid, cur = l + 1;

  PetscFunctionBegin;
  *m = l;
  PetscAssert(r >= l, PETSC_COMM_SELF, PETSC_ERR_PLIB, "r %" PetscInt_FMT " < l %" PetscInt_FMT " in PetscGallopSearchLeft", r, l);
  if ((*cmp)(x, arr + r * size, ctx) >= 0) {
    *m = r;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if ((*cmp)(x, (arr) + l * size, ctx) < 0 || PetscUnlikely(!(r - l))) PetscFunctionReturn(PETSC_SUCCESS);
  while (PETSC_TRUE) {
    if (PetscUnlikely(cur > r)) {
      cur = r;
      break;
    }
    if ((*cmp)(x, (arr) + cur * size, ctx) < 0) break;
    last = cur;
    cur += (k <<= 1) + 1;
    ++k;
  }
  /* standard binary search but take last 0 mid 0 cur 1 into account*/
  while (cur > last + 1) {
    mid = last + ((cur - last) >> 1);
    if ((*cmp)(x, (arr) + mid * size, ctx) < 0) {
      cur = mid;
    } else {
      last = mid;
    }
  }
  *m = cur;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Start right look left. Looking for e.g. A[-1] in B or mergehi. l inclusive, r inclusive. Returns last m such that arr[m]
 < x. Output also inclusive */
static inline PetscErrorCode PetscGallopSearchRight_Private(const char *arr, size_t size, CompFunc cmp, void *ctx, PetscInt l, PetscInt r, const char *x, PetscInt *m)
{
  PetscInt last = r, k = 1, mid, cur = r - 1;

  PetscFunctionBegin;
  *m = r;
  PetscAssert(r >= l, PETSC_COMM_SELF, PETSC_ERR_PLIB, "r %" PetscInt_FMT " < l %" PetscInt_FMT " in PetscGallopSearchRight", r, l);
  if ((*cmp)(x, arr + l * size, ctx) <= 0) {
    *m = l;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if ((*cmp)(x, (arr) + r * size, ctx) > 0 || PetscUnlikely(!(r - l))) PetscFunctionReturn(PETSC_SUCCESS);
  while (PETSC_TRUE) {
    if (PetscUnlikely(cur < l)) {
      cur = l;
      break;
    }
    if ((*cmp)(x, (arr) + cur * size, ctx) > 0) break;
    last = cur;
    cur -= (k <<= 1) + 1;
    ++k;
  }
  /* standard binary search but take last r-1 mid r-1 cur r-2 into account*/
  while (last > cur + 1) {
    mid = last - ((last - cur) >> 1);
    if ((*cmp)(x, (arr) + mid * size, ctx) > 0) {
      cur = mid;
    } else {
      last = mid;
    }
  }
  *m = cur;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Mergesort where size of left half <= size of right half, so mergesort is done left to right. Arr should be pointer to
 complete array, left is first index of left array, mid is first index of right array, right is last index of right
 array */
static inline PetscErrorCode PetscTimSortMergeLo_Private(char *arr, char *tarr, size_t size, CompFunc cmp, void *ctx, PetscInt left, PetscInt mid, PetscInt right)
{
  PetscInt       i = 0, j = mid, k = left, gallopleft = 0, gallopright = 0;
  const PetscInt llen = mid - left;

  PetscFunctionBegin;
  Petsc_memcpy(tarr, arr + (left * size), llen * size);
  while ((i < llen) && (j <= right)) {
    if ((*cmp)(tarr + (i * size), arr + (j * size), ctx) < 0) {
      Petsc_memcpy(arr + (k * size), tarr + (i * size), size);
      ++k;
      gallopright = 0;
      if (++i < llen && ++gallopleft >= MIN_GALLOP_GLOBAL) {
        PetscInt l1, l2, diff1, diff2;
        do {
          if (MIN_GALLOP_GLOBAL > 1) --MIN_GALLOP_GLOBAL;
          /* search temp for right[j], can move up to that of temp into arr immediately */
          PetscCall(PetscGallopSearchLeft_Private(tarr, size, cmp, ctx, i, llen - 1, arr + (j * size), &l1));
          diff1 = l1 - i;
          Petsc_memcpy(arr + (k * size), tarr + (i * size), diff1 * size);
          k += diff1;
          i = l1;
          /* search right for temp[i], can move up to that many of right into arr */
          PetscCall(PetscGallopSearchLeft_Private(arr, size, cmp, ctx, j, right, tarr + (i * size), &l2));
          diff2 = l2 - j;
          Petsc_memmove((arr) + k * size, (arr) + j * size, diff2 * size);
          k += diff2;
          j = l2;
          if (i >= llen || j > right) break;
        } while (diff1 > MIN_GALLOP_GLOBAL || diff2 > MIN_GALLOP_GLOBAL);
        ++MIN_GALLOP_GLOBAL;
      }
    } else {
      Petsc_memmove(arr + (k * size), arr + (j * size), size);
      ++k;
      gallopleft = 0;
      if (++j <= right && ++gallopright >= MIN_GALLOP_GLOBAL) {
        PetscInt l1, l2, diff1, diff2;
        do {
          if (MIN_GALLOP_GLOBAL > 1) --MIN_GALLOP_GLOBAL;
          /* search right for temp[i], can move up to that many of right into arr */
          PetscCall(PetscGallopSearchLeft_Private(arr, size, cmp, ctx, j, right, tarr + (i * size), &l2));
          diff2 = l2 - j;
          Petsc_memmove(arr + (k * size), arr + (j * size), diff2 * size);
          k += diff2;
          j = l2;
          /* search temp for right[j], can copy up to that of temp into arr immediately */
          PetscCall(PetscGallopSearchLeft_Private(tarr, size, cmp, ctx, i, llen - 1, arr + (j * size), &l1));
          diff1 = l1 - i;
          Petsc_memcpy(arr + (k * size), tarr + (i * size), diff1 * size);
          k += diff1;
          i = l1;
          if (i >= llen || j > right) break;
        } while (diff1 > MIN_GALLOP_GLOBAL || diff2 > MIN_GALLOP_GLOBAL);
        ++MIN_GALLOP_GLOBAL;
      }
    }
  }
  if (i < llen) Petsc_memcpy(arr + (k * size), tarr + (i * size), (llen - i) * size);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscTimSortMergeLoWithArray_Private(char *arr, char *atarr, size_t asize, char *barr, char *btarr, size_t bsize, CompFunc cmp, void *ctx, PetscInt left, PetscInt mid, PetscInt right)
{
  PetscInt       i = 0, j = mid, k = left, gallopleft = 0, gallopright = 0;
  const PetscInt llen = mid - left;

  PetscFunctionBegin;
  Petsc_memcpy2(atarr, arr + (left * asize), llen * asize, btarr, barr + (left * bsize), llen * bsize);
  while ((i < llen) && (j <= right)) {
    if ((*cmp)(atarr + (i * asize), arr + (j * asize), ctx) < 0) {
      Petsc_memcpy2(arr + (k * asize), atarr + (i * asize), asize, barr + (k * bsize), btarr + (i * bsize), bsize);
      ++k;
      gallopright = 0;
      if (++i < llen && ++gallopleft >= MIN_GALLOP_GLOBAL) {
        PetscInt l1, l2, diff1, diff2;
        do {
          if (MIN_GALLOP_GLOBAL > 1) --MIN_GALLOP_GLOBAL;
          /* search temp for right[j], can move up to that of temp into arr immediately */
          PetscCall(PetscGallopSearchLeft_Private(atarr, asize, cmp, ctx, i, llen - 1, arr + (j * asize), &l1));
          diff1 = l1 - i;
          Petsc_memcpy2(arr + (k * asize), atarr + (i * asize), diff1 * asize, barr + (k * bsize), btarr + (i * bsize), diff1 * bsize);
          k += diff1;
          i = l1;
          /* search right for temp[i], can move up to that many of right into arr */
          PetscCall(PetscGallopSearchLeft_Private(arr, asize, cmp, ctx, j, right, atarr + (i * asize), &l2));
          diff2 = l2 - j;
          Petsc_memmove2(arr + (k * asize), arr + (j * asize), diff2 * asize, barr + (k * bsize), barr + (j * bsize), diff2 * bsize);
          k += diff2;
          j = l2;
          if (i >= llen || j > right) break;
        } while (diff1 > MIN_GALLOP_GLOBAL || diff2 > MIN_GALLOP_GLOBAL);
        ++MIN_GALLOP_GLOBAL;
      }
    } else {
      Petsc_memmove2(arr + (k * asize), arr + (j * asize), asize, barr + (k * bsize), barr + (j * bsize), bsize);
      ++k;
      gallopleft = 0;
      if (++j <= right && ++gallopright >= MIN_GALLOP_GLOBAL) {
        PetscInt l1, l2, diff1, diff2;
        do {
          if (MIN_GALLOP_GLOBAL > 1) --MIN_GALLOP_GLOBAL;
          /* search right for temp[i], can move up to that many of right into arr */
          PetscCall(PetscGallopSearchLeft_Private(arr, asize, cmp, ctx, j, right, atarr + (i * asize), &l2));
          diff2 = l2 - j;
          Petsc_memmove2(arr + (k * asize), arr + (j * asize), diff2 * asize, barr + (k * bsize), barr + (j * bsize), diff2 * bsize);
          k += diff2;
          j = l2;
          /* search temp for right[j], can copy up to that of temp into arr immediately */
          PetscCall(PetscGallopSearchLeft_Private(atarr, asize, cmp, ctx, i, llen - 1, arr + (j * asize), &l1));
          diff1 = l1 - i;
          Petsc_memcpy2(arr + (k * asize), atarr + (i * asize), diff1 * asize, barr + (k * bsize), btarr + (i * bsize), diff1 * bsize);
          k += diff1;
          i = l1;
          if (i >= llen || j > right) break;
        } while (diff1 > MIN_GALLOP_GLOBAL || diff2 > MIN_GALLOP_GLOBAL);
        ++MIN_GALLOP_GLOBAL;
      }
    }
  }
  if (i < llen) Petsc_memcpy2(arr + (k * asize), atarr + (i * asize), (llen - i) * asize, barr + (k * bsize), btarr + (i * bsize), (llen - i) * bsize);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Mergesort where size of right half < size of left half, so mergesort is done right to left. Arr should be pointer to
 complete array, left is first index of left array, mid is first index of right array, right is last index of right
 array */
static inline PetscErrorCode PetscTimSortMergeHi_Private(char *arr, char *tarr, size_t size, CompFunc cmp, void *ctx, PetscInt left, PetscInt mid, PetscInt right)
{
  PetscInt       i = right - mid, j = mid - 1, k = right, gallopleft = 0, gallopright = 0;
  const PetscInt rlen = right - mid + 1;

  PetscFunctionBegin;
  Petsc_memcpy(tarr, (arr) + mid * size, rlen * size);
  while ((i >= 0) && (j >= left)) {
    if ((*cmp)((tarr) + i * size, (arr) + j * size, ctx) > 0) {
      Petsc_memcpy((arr) + k * size, (tarr) + i * size, size);
      --k;
      gallopleft = 0;
      if (--i >= 0 && ++gallopright >= MIN_GALLOP_GLOBAL) {
        PetscInt l1, l2, diff1, diff2;
        do {
          if (MIN_GALLOP_GLOBAL > 1) --MIN_GALLOP_GLOBAL;
          /* search temp for left[j], can copy up to that many of temp into arr */
          PetscCall(PetscGallopSearchRight_Private(tarr, size, cmp, ctx, 0, i, (arr) + j * size, &l1));
          diff1 = i - l1;
          Petsc_memcpy((arr) + (k - diff1 + 1) * size, (tarr) + (l1 + 1) * size, diff1 * size);
          k -= diff1;
          i = l1;
          /* search left for temp[i], can move up to that many of left up arr */
          PetscCall(PetscGallopSearchRight_Private(arr, size, cmp, ctx, left, j, (tarr) + i * size, &l2));
          diff2 = j - l2;
          Petsc_memmove((arr) + (k - diff2 + 1) * size, (arr) + (l2 + 1) * size, diff2 * size);
          k -= diff2;
          j = l2;
          if (i < 0 || j < left) break;
        } while (diff1 > MIN_GALLOP_GLOBAL || diff2 > MIN_GALLOP_GLOBAL);
        ++MIN_GALLOP_GLOBAL;
      }
    } else {
      Petsc_memmove((arr) + k * size, (arr) + j * size, size);
      --k;
      gallopright = 0;
      if (--j >= left && ++gallopleft >= MIN_GALLOP_GLOBAL) {
        PetscInt l1, l2, diff1, diff2;
        do {
          if (MIN_GALLOP_GLOBAL > 1) --MIN_GALLOP_GLOBAL;
          /* search left for temp[i], can move up to that many of left up arr */
          PetscCall(PetscGallopSearchRight_Private(arr, size, cmp, ctx, left, j, (tarr) + i * size, &l2));
          diff2 = j - l2;
          Petsc_memmove((arr) + (k - diff2 + 1) * size, (arr) + (l2 + 1) * size, diff2 * size);
          k -= diff2;
          j = l2;
          /* search temp for left[j], can copy up to that many of temp into arr */
          PetscCall(PetscGallopSearchRight_Private(tarr, size, cmp, ctx, 0, i, (arr) + j * size, &l1));
          diff1 = i - l1;
          Petsc_memcpy((arr) + (k - diff1 + 1) * size, (tarr) + (l1 + 1) * size, diff1 * size);
          k -= diff1;
          i = l1;
          if (i < 0 || j < left) break;
        } while (diff1 > MIN_GALLOP_GLOBAL || diff2 > MIN_GALLOP_GLOBAL);
        ++MIN_GALLOP_GLOBAL;
      }
    }
  }
  if (i >= 0) Petsc_memcpy((arr) + left * size, tarr, (i + 1) * size);
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscTimSortMergeHiWithArray_Private(char *arr, char *atarr, size_t asize, char *barr, char *btarr, size_t bsize, CompFunc cmp, void *ctx, PetscInt left, PetscInt mid, PetscInt right)
{
  PetscInt       i = right - mid, j = mid - 1, k = right, gallopleft = 0, gallopright = 0;
  const PetscInt rlen = right - mid + 1;

  PetscFunctionBegin;
  Petsc_memcpy2(atarr, (arr) + mid * asize, rlen * asize, btarr, (barr) + mid * bsize, rlen * bsize);
  while ((i >= 0) && (j >= left)) {
    if ((*cmp)((atarr) + i * asize, (arr) + j * asize, ctx) > 0) {
      Petsc_memcpy2((arr) + k * asize, (atarr) + i * asize, asize, (barr) + k * bsize, (btarr) + i * bsize, bsize);
      --k;
      gallopleft = 0;
      if (--i >= 0 && ++gallopright >= MIN_GALLOP_GLOBAL) {
        PetscInt l1, l2, diff1, diff2;
        do {
          if (MIN_GALLOP_GLOBAL > 1) --MIN_GALLOP_GLOBAL;
          /* search temp for left[j], can copy up to that many of temp into arr */
          PetscCall(PetscGallopSearchRight_Private(atarr, asize, cmp, ctx, 0, i, (arr) + j * asize, &l1));
          diff1 = i - l1;
          Petsc_memcpy2((arr) + (k - diff1 + 1) * asize, (atarr) + (l1 + 1) * asize, diff1 * asize, (barr) + (k - diff1 + 1) * bsize, (btarr) + (l1 + 1) * bsize, diff1 * bsize);
          k -= diff1;
          i = l1;
          /* search left for temp[i], can move up to that many of left up arr */
          PetscCall(PetscGallopSearchRight_Private(arr, asize, cmp, ctx, left, j, (atarr) + i * asize, &l2));
          diff2 = j - l2;
          Petsc_memmove2((arr) + (k - diff2 + 1) * asize, (arr) + (l2 + 1) * asize, diff2 * asize, (barr) + (k - diff2 + 1) * bsize, (barr) + (l2 + 1) * bsize, diff2 * bsize);
          k -= diff2;
          j = l2;
          if (i < 0 || j < left) break;
        } while (diff1 > MIN_GALLOP_GLOBAL || diff2 > MIN_GALLOP_GLOBAL);
        ++MIN_GALLOP_GLOBAL;
      }
    } else {
      Petsc_memmove2((arr) + k * asize, (arr) + j * asize, asize, (barr) + k * bsize, (barr) + j * bsize, bsize);
      --k;
      gallopright = 0;
      if (--j >= left && ++gallopleft >= MIN_GALLOP_GLOBAL) {
        PetscInt l1, l2, diff1, diff2;
        do {
          if (MIN_GALLOP_GLOBAL > 1) --MIN_GALLOP_GLOBAL;
          /* search left for temp[i], can move up to that many of left up arr */
          PetscCall(PetscGallopSearchRight_Private(arr, asize, cmp, ctx, left, j, (atarr) + i * asize, &l2));
          diff2 = j - l2;
          Petsc_memmove2((arr) + (k - diff2 + 1) * asize, (arr) + (l2 + 1) * asize, diff2 * asize, (barr) + (k - diff2 + 1) * bsize, (barr) + (l2 + 1) * bsize, diff2 * bsize);
          k -= diff2;
          j = l2;
          /* search temp for left[j], can copy up to that many of temp into arr */
          PetscCall(PetscGallopSearchRight_Private(atarr, asize, cmp, ctx, 0, i, (arr) + j * asize, &l1));
          diff1 = i - l1;
          Petsc_memcpy2((arr) + (k - diff1 + 1) * asize, (atarr) + (l1 + 1) * asize, diff1 * asize, (barr) + (k - diff1 + 1) * bsize, (btarr) + (l1 + 1) * bsize, diff1 * bsize);
          k -= diff1;
          i = l1;
          if (i < 0 || j < left) break;
        } while (diff1 > MIN_GALLOP_GLOBAL || diff2 > MIN_GALLOP_GLOBAL);
        ++MIN_GALLOP_GLOBAL;
      }
    }
  }
  if (i >= 0) Petsc_memcpy2((arr) + left * asize, atarr, (i + 1) * asize, (barr) + left * bsize, btarr, (i + 1) * bsize);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* Left is inclusive lower bound of array slice, start is start location of unsorted section, right is inclusive upper
 bound of array slice. If unsure of where unsorted section starts or if entire length is unsorted pass start = left */
static inline PetscErrorCode PetscInsertionSort_Private(char *arr, char *tarr, size_t size, CompFunc cmp, void *ctx, PetscInt left, PetscInt start, PetscInt right)
{
  PetscInt i = start == left ? start + 1 : start;

  PetscFunctionBegin;
  for (; i <= right; ++i) {
    PetscInt j = i - 1;
    Petsc_memcpy(tarr, arr + (i * size), size);
    while ((j >= left) && ((*cmp)(tarr, (arr) + j * size, ctx) < 0)) {
      COPYSWAPPY(arr + (j + 1) * size, arr + j * size, tarr + size, size);
      --j;
    }
    Petsc_memcpy((arr) + (j + 1) * size, tarr, size);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscInsertionSortWithArray_Private(char *arr, char *atarr, size_t asize, char *barr, char *btarr, size_t bsize, CompFunc cmp, void *ctx, PetscInt left, PetscInt start, PetscInt right)
{
  PetscInt i = start == left ? start + 1 : start;

  PetscFunctionBegin;
  for (; i <= right; ++i) {
    PetscInt j = i - 1;
    Petsc_memcpy2(atarr, arr + (i * asize), asize, btarr, barr + (i * bsize), bsize);
    while ((j >= left) && ((*cmp)(atarr, arr + (j * asize), ctx) < 0)) {
      COPYSWAPPY2(arr + (j + 1) * asize, arr + j * asize, asize, barr + (j + 1) * bsize, barr + j * bsize, bsize, atarr + asize);
      --j;
    }
    Petsc_memcpy2(arr + (j + 1) * asize, atarr, asize, barr + (j + 1) * bsize, btarr, bsize);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* See PetscInsertionSort_Private */
static inline PetscErrorCode PetscBinaryInsertionSort_Private(char *arr, char *tarr, size_t size, CompFunc cmp, void *ctx, PetscInt left, PetscInt start, PetscInt right)
{
  PetscInt i = start == left ? start + 1 : start;

  PetscFunctionBegin;
  for (; i <= right; ++i) {
    PetscInt l = left, r = i;
    Petsc_memcpy(tarr, arr + (i * size), size);
    do {
      const PetscInt m = l + ((r - l) >> 1);
      if ((*cmp)(tarr, arr + (m * size), ctx) < 0) {
        r = m;
      } else {
        l = m + 1;
      }
    } while (l < r);
    Petsc_memmove(arr + ((l + 1) * size), arr + (l * size), (i - l) * size);
    Petsc_memcpy(arr + (l * size), tarr, size);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscBinaryInsertionSortWithArray_Private(char *arr, char *atarr, size_t asize, char *barr, char *btarr, size_t bsize, CompFunc cmp, void *ctx, PetscInt left, PetscInt start, PetscInt right)
{
  PetscInt i = start == left ? start + 1 : start;

  PetscFunctionBegin;
  for (; i <= right; ++i) {
    PetscInt l = left, r = i;
    Petsc_memcpy2(atarr, arr + (i * asize), asize, btarr, barr + (i * bsize), bsize);
    do {
      const PetscInt m = l + ((r - l) >> 1);
      if ((*cmp)(atarr, arr + (m * asize), ctx) < 0) {
        r = m;
      } else {
        l = m + 1;
      }
    } while (l < r);
    Petsc_memmove2(arr + ((l + 1) * asize), arr + (l * asize), (i - l) * asize, barr + ((l + 1) * bsize), barr + (l * bsize), (i - l) * bsize);
    Petsc_memcpy2(arr + (l * asize), atarr, asize, barr + (l * bsize), btarr, bsize);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

typedef struct {
  PetscInt size;
  PetscInt start;
#if defined(__CRAY_AARCH64) /* segfaults with Cray compilers for aarch64 on a64FX */
} PetscTimSortStack;
#else
} PetscTimSortStack PETSC_ATTRIBUTEALIGNED(2 * sizeof(PetscInt));
#endif

typedef struct {
  char *ptr PETSC_ATTRIBUTEALIGNED(PETSC_MEMALIGN);
  size_t    size;
  size_t    maxsize;
} PetscTimSortBuffer;

static inline PetscErrorCode PetscTimSortResizeBuffer_Private(PetscTimSortBuffer *buff, size_t newSize)
{
  PetscFunctionBegin;
  if (PetscLikely(newSize <= buff->size)) PetscFunctionReturn(PETSC_SUCCESS);
  {
    /* Can't be larger than n, there is merit to simply allocating buff to n to begin with */
    size_t newMax = PetscMin(newSize * newSize, buff->maxsize);
    PetscCall(PetscFree(buff->ptr));
    PetscCall(PetscMalloc1(newMax, &buff->ptr));
    buff->size = newMax;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscTimSortForceCollapse_Private(char *arr, size_t size, CompFunc cmp, void *ctx, PetscTimSortBuffer *buff, PetscTimSortStack *stack, PetscInt stacksize)
{
  PetscFunctionBegin;
  for (; stacksize; --stacksize) {
    /* A = stack[i-1], B = stack[i], if A[-1] <= B[0] means sorted */
    if ((*cmp)(arr + (stack[stacksize].start - 1) * size, arr + (stack[stacksize].start) * size, ctx) > 0) {
      PetscInt l, m = stack[stacksize].start, r;
      /* Search A for B[0] insertion */
      PetscCall(PetscGallopSearchLeft_Private(arr, size, cmp, ctx, stack[stacksize - 1].start, stack[stacksize].start - 1, (arr) + (stack[stacksize].start) * size, &l));
      /* Search B for A[-1] insertion */
      PetscCall(PetscGallopSearchRight_Private(arr, size, cmp, ctx, stack[stacksize].start, stack[stacksize].start + stack[stacksize].size - 1, (arr) + (stack[stacksize].start - 1) * size, &r));
      if (m - l <= r - m) {
        PetscCall(PetscTimSortResizeBuffer_Private(buff, (m - l + 1) * size));
        PetscCall(PetscTimSortMergeLo_Private(arr, buff->ptr, size, cmp, ctx, l, m, r));
      } else {
        PetscCall(PetscTimSortResizeBuffer_Private(buff, (r - m + 1) * size));
        PetscCall(PetscTimSortMergeHi_Private(arr, buff->ptr, size, cmp, ctx, l, m, r));
      }
    }
    /* Update A with merge */
    stack[stacksize - 1].size += stack[stacksize].size;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscTimSortForceCollapseWithArray_Private(char *arr, size_t asize, char *barr, size_t bsize, CompFunc cmp, void *ctx, PetscTimSortBuffer *abuff, PetscTimSortBuffer *bbuff, PetscTimSortStack *stack, PetscInt stacksize)
{
  PetscFunctionBegin;
  for (; stacksize; --stacksize) {
    /* A = stack[i-1], B = stack[i], if A[-1] <= B[0] means sorted */
    if ((*cmp)(arr + (stack[stacksize].start - 1) * asize, arr + (stack[stacksize].start) * asize, ctx) > 0) {
      PetscInt l, m = stack[stacksize].start, r;
      /* Search A for B[0] insertion */
      PetscCall(PetscGallopSearchLeft_Private(arr, asize, cmp, ctx, stack[stacksize - 1].start, stack[stacksize].start - 1, (arr) + (stack[stacksize].start) * asize, &l));
      /* Search B for A[-1] insertion */
      PetscCall(PetscGallopSearchRight_Private(arr, asize, cmp, ctx, stack[stacksize].start, stack[stacksize].start + stack[stacksize].size - 1, (arr) + (stack[stacksize].start - 1) * asize, &r));
      if (m - l <= r - m) {
        PetscCall(PetscTimSortResizeBuffer_Private(abuff, (m - l + 1) * asize));
        PetscCall(PetscTimSortResizeBuffer_Private(bbuff, (m - l + 1) * bsize));
        PetscCall(PetscTimSortMergeLoWithArray_Private(arr, abuff->ptr, asize, barr, bbuff->ptr, bsize, cmp, ctx, l, m, r));
      } else {
        PetscCall(PetscTimSortResizeBuffer_Private(abuff, (r - m + 1) * asize));
        PetscCall(PetscTimSortResizeBuffer_Private(bbuff, (r - m + 1) * bsize));
        PetscCall(PetscTimSortMergeHiWithArray_Private(arr, abuff->ptr, asize, barr, bbuff->ptr, bsize, cmp, ctx, l, m, r));
      }
    }
    /* Update A with merge */
    stack[stacksize - 1].size += stack[stacksize].size;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscTimSortMergeCollapse_Private(char *arr, size_t size, CompFunc cmp, void *ctx, PetscTimSortBuffer *buff, PetscTimSortStack *stack, PetscInt *stacksize)
{
  PetscInt i = *stacksize;

  PetscFunctionBegin;
  while (i) {
    PetscInt l, m, r, itemp = i;

    if (i == 1) {
      /* A = stack[i-1], B = stack[i] */
      if (stack[i - 1].size < stack[i].size) {
        /* if A[-1] <= B[0] then sorted */
        if ((*cmp)(arr + (stack[i].start - 1) * size, arr + (stack[i].start) * size, ctx) > 0) {
          m = stack[i].start;
          /* Search A for B[0] insertion */
          PetscCall(PetscGallopSearchLeft_Private(arr, size, cmp, ctx, stack[i - 1].start, stack[i].start - 1, (arr) + stack[i].start * size, &l));
          /* Search B for A[-1] insertion */
          PetscCall(PetscGallopSearchRight_Private(arr, size, cmp, ctx, stack[i].start, stack[i].start + stack[i].size - 1, arr + (stack[i].start - 1) * size, &r));
          if (m - l <= r - m) {
            PetscCall(PetscTimSortResizeBuffer_Private(buff, (m - l + 1) * size));
            PetscCall(PetscTimSortMergeLo_Private(arr, buff->ptr, size, cmp, ctx, l, m, r));
          } else {
            PetscCall(PetscTimSortResizeBuffer_Private(buff, (r - m + 1) * size));
            PetscCall(PetscTimSortMergeHi_Private(arr, buff->ptr, size, cmp, ctx, l, m, r));
          }
        }
        /* Update A with merge */
        stack[i - 1].size += stack[i].size;
        --i;
      }
    } else {
      /* i > 2, i.e. C exists
       A = stack[i-2], B = stack[i-1], C = stack[i]; */
      if (stack[i - 2].size <= stack[i - 1].size + stack[i].size) {
        if (stack[i - 2].size < stack[i].size) {
          /* merge B into A, but if A[-1] <= B[0] then already sorted */
          if ((*cmp)(arr + (stack[i - 1].start - 1) * size, arr + (stack[i - 1].start) * size, ctx) > 0) {
            m = stack[i - 1].start;
            /* Search A for B[0] insertion */
            PetscCall(PetscGallopSearchLeft_Private(arr, size, cmp, ctx, stack[i - 2].start, stack[i - 1].start - 1, (arr) + (stack[i - 1].start) * size, &l));
            /* Search B for A[-1] insertion */
            PetscCall(PetscGallopSearchRight_Private(arr, size, cmp, ctx, stack[i - 1].start, stack[i - 1].start + stack[i - 1].size - 1, (arr) + (stack[i - 1].start - 1) * size, &r));
            if (m - l <= r - m) {
              PetscCall(PetscTimSortResizeBuffer_Private(buff, (m - l + 1) * size));
              PetscCall(PetscTimSortMergeLo_Private(arr, buff->ptr, size, cmp, ctx, l, m, r));
            } else {
              PetscCall(PetscTimSortResizeBuffer_Private(buff, (r - m + 1) * size));
              PetscCall(PetscTimSortMergeHi_Private(arr, buff->ptr, size, cmp, ctx, l, m, r));
            }
          }
          /* Update A with merge */
          stack[i - 2].size += stack[i - 1].size;
          /* Push C up the stack */
          stack[i - 1].start = stack[i].start;
          stack[i - 1].size  = stack[i].size;
        } else {
        /* merge C into B */
        mergeBC:
          /* If B[-1] <= C[0] then... you know the drill */
          if ((*cmp)(arr + (stack[i].start - 1) * size, arr + (stack[i].start) * size, ctx) > 0) {
            m = stack[i].start;
            /* Search B for C[0] insertion */
            PetscCall(PetscGallopSearchLeft_Private(arr, size, cmp, ctx, stack[i - 1].start, stack[i].start - 1, arr + stack[i].start * size, &l));
            /* Search C for B[-1] insertion */
            PetscCall(PetscGallopSearchRight_Private(arr, size, cmp, ctx, stack[i].start, stack[i].start + stack[i].size - 1, (arr) + (stack[i].start - 1) * size, &r));
            if (m - l <= r - m) {
              PetscCall(PetscTimSortResizeBuffer_Private(buff, (m - l + 1) * size));
              PetscCall(PetscTimSortMergeLo_Private(arr, buff->ptr, size, cmp, ctx, l, m, r));
            } else {
              PetscCall(PetscTimSortResizeBuffer_Private(buff, (r - m + 1) * size));
              PetscCall(PetscTimSortMergeHi_Private(arr, buff->ptr, size, cmp, ctx, l, m, r));
            }
          }
          /* Update B with merge */
          stack[i - 1].size += stack[i].size;
        }
        --i;
      } else if (stack[i - 1].size <= stack[i].size) {
        /* merge C into B */
        goto mergeBC;
      }
    }
    if (itemp == i) break;
  }
  *stacksize = i;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscTimSortMergeCollapseWithArray_Private(char *arr, size_t asize, char *barr, size_t bsize, CompFunc cmp, void *ctx, PetscTimSortBuffer *abuff, PetscTimSortBuffer *bbuff, PetscTimSortStack *stack, PetscInt *stacksize)
{
  PetscInt i = *stacksize;

  PetscFunctionBegin;
  while (i) {
    PetscInt l, m, r, itemp = i;

    if (i == 1) {
      /* A = stack[i-1], B = stack[i] */
      if (stack[i - 1].size < stack[i].size) {
        /* if A[-1] <= B[0] then sorted */
        if ((*cmp)(arr + (stack[i].start - 1) * asize, arr + (stack[i].start) * asize, ctx) > 0) {
          m = stack[i].start;
          /* Search A for B[0] insertion */
          PetscCall(PetscGallopSearchLeft_Private(arr, asize, cmp, ctx, stack[i - 1].start, stack[i].start - 1, (arr) + stack[i].start * asize, &l));
          /* Search B for A[-1] insertion */
          PetscCall(PetscGallopSearchRight_Private(arr, asize, cmp, ctx, stack[i].start, stack[i].start + stack[i].size - 1, arr + (stack[i].start - 1) * asize, &r));
          if (m - l <= r - m) {
            PetscCall(PetscTimSortResizeBuffer_Private(abuff, (m - l + 1) * asize));
            PetscCall(PetscTimSortResizeBuffer_Private(bbuff, (m - l + 1) * bsize));
            PetscCall(PetscTimSortMergeLoWithArray_Private(arr, abuff->ptr, asize, barr, bbuff->ptr, bsize, cmp, ctx, l, m, r));
          } else {
            PetscCall(PetscTimSortResizeBuffer_Private(abuff, (r - m + 1) * asize));
            PetscCall(PetscTimSortResizeBuffer_Private(bbuff, (r - m + 1) * bsize));
            PetscCall(PetscTimSortMergeHiWithArray_Private(arr, abuff->ptr, asize, barr, bbuff->ptr, bsize, cmp, ctx, l, m, r));
          }
        }
        /* Update A with merge */
        stack[i - 1].size += stack[i].size;
        --i;
      }
    } else {
      /* i > 2, i.e. C exists
       A = stack[i-2], B = stack[i-1], C = stack[i]; */
      if (stack[i - 2].size <= stack[i - 1].size + stack[i].size) {
        if (stack[i - 2].size < stack[i].size) {
          /* merge B into A, but if A[-1] <= B[0] then already sorted */
          if ((*cmp)(arr + (stack[i - 1].start - 1) * asize, arr + (stack[i - 1].start) * asize, ctx) > 0) {
            m = stack[i - 1].start;
            /* Search A for B[0] insertion */
            PetscCall(PetscGallopSearchLeft_Private(arr, asize, cmp, ctx, stack[i - 2].start, stack[i - 1].start - 1, (arr) + (stack[i - 1].start) * asize, &l));
            /* Search B for A[-1] insertion */
            PetscCall(PetscGallopSearchRight_Private(arr, asize, cmp, ctx, stack[i - 1].start, stack[i - 1].start + stack[i - 1].size - 1, (arr) + (stack[i - 1].start - 1) * asize, &r));
            if (m - l <= r - m) {
              PetscCall(PetscTimSortResizeBuffer_Private(abuff, (m - l + 1) * asize));
              PetscCall(PetscTimSortResizeBuffer_Private(bbuff, (m - l + 1) * bsize));
              PetscCall(PetscTimSortMergeLoWithArray_Private(arr, abuff->ptr, asize, barr, bbuff->ptr, bsize, cmp, ctx, l, m, r));
            } else {
              PetscCall(PetscTimSortResizeBuffer_Private(abuff, (r - m + 1) * asize));
              PetscCall(PetscTimSortResizeBuffer_Private(bbuff, (r - m + 1) * bsize));
              PetscCall(PetscTimSortMergeHiWithArray_Private(arr, abuff->ptr, asize, barr, bbuff->ptr, bsize, cmp, ctx, l, m, r));
            }
          }
          /* Update A with merge */
          stack[i - 2].size += stack[i - 1].size;
          /* Push C up the stack */
          stack[i - 1].start = stack[i].start;
          stack[i - 1].size  = stack[i].size;
        } else {
        /* merge C into B */
        mergeBC:
          /* If B[-1] <= C[0] then... you know the drill */
          if ((*cmp)(arr + (stack[i].start - 1) * asize, arr + (stack[i].start) * asize, ctx) > 0) {
            m = stack[i].start;
            /* Search B for C[0] insertion */
            PetscCall(PetscGallopSearchLeft_Private(arr, asize, cmp, ctx, stack[i - 1].start, stack[i].start - 1, arr + stack[i].start * asize, &l));
            /* Search C for B[-1] insertion */
            PetscCall(PetscGallopSearchRight_Private(arr, asize, cmp, ctx, stack[i].start, stack[i].start + stack[i].size - 1, (arr) + (stack[i].start - 1) * asize, &r));
            if (m - l <= r - m) {
              PetscCall(PetscTimSortResizeBuffer_Private(abuff, (m - l + 1) * asize));
              PetscCall(PetscTimSortResizeBuffer_Private(bbuff, (m - l + 1) * bsize));
              PetscCall(PetscTimSortMergeLoWithArray_Private(arr, abuff->ptr, asize, barr, bbuff->ptr, bsize, cmp, ctx, l, m, r));
            } else {
              PetscCall(PetscTimSortResizeBuffer_Private(abuff, (r - m + 1) * asize));
              PetscCall(PetscTimSortResizeBuffer_Private(bbuff, (r - m + 1) * bsize));
              PetscCall(PetscTimSortMergeHiWithArray_Private(arr, abuff->ptr, asize, barr, bbuff->ptr, bsize, cmp, ctx, l, m, r));
            }
          }
          /* Update B with merge */
          stack[i - 1].size += stack[i].size;
        }
        --i;
      } else if (stack[i - 1].size <= stack[i].size) {
        /* merge C into B */
        goto mergeBC;
      }
    }
    if (itemp == i) break;
  }
  *stacksize = i;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* March sequentially through the array building up a "run" of weakly increasing or strictly decreasing contiguous
 elements. Decreasing runs are reversed by swapping. If the run is less than minrun, artificially extend it via either
 binary insertion sort or regulat insertion sort */
static inline PetscErrorCode PetscTimSortBuildRun_Private(char *arr, char *tarr, size_t size, CompFunc cmp, void *ctx, PetscInt n, PetscInt minrun, PetscInt runstart, PetscInt *runend)
{
  const PetscInt re = PetscMin(runstart + minrun, n - 1);
  PetscInt       ri = runstart;

  PetscFunctionBegin;
  if (PetscUnlikely(runstart == n - 1)) {
    *runend = runstart;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  /* guess whether run is ascending or descending and tally up the longest consecutive run. essentially a coinflip for random data */
  if ((*cmp)((arr) + (ri + 1) * size, (arr) + ri * size, ctx) < 0) {
    ++ri;
    while (ri < n - 1) {
      if ((*cmp)((arr) + (ri + 1) * size, (arr) + ri * size, ctx) >= 0) break;
      ++ri;
    }
    {
      PetscInt lo = runstart, hi = ri;
      for (; lo < hi; ++lo, --hi) COPYSWAPPY(arr + lo * size, arr + hi * size, tarr, size);
    }
  } else {
    ++ri;
    while (ri < n - 1) {
      if ((*cmp)((arr) + (ri + 1) * size, (arr) + ri * size, ctx) < 0) break;
      ++ri;
    }
  }
  if (ri < re) {
    /* the attempt failed, this section likely contains random data. If ri got close to minrun (within 50%) then we try
     binary search */
    if (ri - runstart <= minrun >> 1) {
      ++MIN_GALLOP_GLOBAL; /* didn't get close hedge our bets against random data */
      PetscCall(PetscInsertionSort_Private(arr, tarr, size, cmp, ctx, runstart, ri, re));
    } else {
      PetscCall(PetscBinaryInsertionSort_Private(arr, tarr, size, cmp, ctx, runstart, ri, re));
    }
    *runend = re;
  } else *runend = ri;
  PetscFunctionReturn(PETSC_SUCCESS);
}

static inline PetscErrorCode PetscTimSortBuildRunWithArray_Private(char *arr, char *atarr, size_t asize, char *barr, char *btarr, size_t bsize, CompFunc cmp, void *ctx, PetscInt n, PetscInt minrun, PetscInt runstart, PetscInt *runend)
{
  const PetscInt re = PetscMin(runstart + minrun, n - 1);
  PetscInt       ri = runstart;

  PetscFunctionBegin;
  if (PetscUnlikely(runstart == n - 1)) {
    *runend = runstart;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  /* guess whether run is ascending or descending and tally up the longest consecutive run. essentially a coinflip for random data */
  if ((*cmp)((arr) + (ri + 1) * asize, arr + (ri * asize), ctx) < 0) {
    ++ri;
    while (ri < n - 1) {
      if ((*cmp)((arr) + (ri + 1) * asize, (arr) + ri * asize, ctx) >= 0) break;
      ++ri;
    }
    {
      PetscInt lo = runstart, hi = ri;
      for (; lo < hi; ++lo, --hi) COPYSWAPPY2(arr + lo * asize, arr + hi * asize, asize, barr + lo * bsize, barr + hi * bsize, bsize, atarr);
    }
  } else {
    ++ri;
    while (ri < n - 1) {
      if ((*cmp)((arr) + (ri + 1) * asize, (arr) + ri * asize, ctx) < 0) break;
      ++ri;
    }
  }
  if (ri < re) {
    /* the attempt failed, this section likely contains random data. If ri got close to minrun (within 50%) then we try
     binary search */
    if (ri - runstart <= minrun >> 1) {
      ++MIN_GALLOP_GLOBAL; /* didn't get close hedge our bets against random data */
      PetscCall(PetscInsertionSortWithArray_Private(arr, atarr, asize, barr, btarr, bsize, cmp, ctx, runstart, ri, re));
    } else {
      PetscCall(PetscBinaryInsertionSortWithArray_Private(arr, atarr, asize, barr, btarr, bsize, cmp, ctx, runstart, ri, re));
    }
    *runend = re;
  } else *runend = ri;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscTimSort - Sorts an array in place in increasing order using Tim Peters adaptive sorting algorithm.

  Not Collective

  Input Parameters:
+ n    - number of values
. arr  - array to be sorted
. size - size in bytes of the datatype held in arr
. cmp  - function pointer to comparison function
- ctx  - optional context to be passed to comparison function, NULL if not needed

  Output Parameter:
. arr  - sorted array

  Notes:
  Timsort makes the assumption that input data is already likely partially ordered, or that it contains contiguous
 sections (termed 'runs') where the data is locally ordered (but not necessarily globally ordered). It therefore aims to
 select slices of the array in such a way that resulting mergesorts operate on near perfectly length-balanced arrays. To
 do so it repeatedly triggers attempts throughout to merge adjacent runs.

  Should one run continuously "win" a comparison the algorithm begins the "gallop" phase. It will aggressively search
  the "winner" for the location of the "losers" next entry (and vice versa) to copy all preceding elements into place in
  bulk. However if the data is truly unordered (as is the case with random data) the immense gains possible from these
  searches are expected __not__ to repay their costs. While adjacent arrays are almost all nearly the same size, they
  likely all contain similar data.

  Sample usage:
  The comparison function must follow the `qsort()` comparison function paradigm, returning the sign of the difference
  between its arguments. If left < right : return -1, if left == right : return 0, if left > right : return 1. The user
  may also
 change or reverse the order of the sort by flipping the above. Note that stability of the sort is only guaranteed if
 the comparison function forms a valid trigraph. For example when sorting an array of type "my_type" in increasing
  order

.vb
  int my_increasing_comparison_function(const void *left, const void *right, void *ctx) {
    my_type l = *(my_type *) left, r = *(my_type *) right;
    return (l < r) ? -1 : (l > r);
  }
.ve
  Note the context is unused here but you may use it to pass and subsequently access whatever information required
  inside the comparison function. The context pointer will unaltered except for any changes made inside the comparison function.
  Then pass the function
.vb
  PetscTimSort(n, arr, sizeof(arr[0]), my_increasing_comparison_function, ctx)
.ve

  Fortran Notes:
  To use this from fortran you must write a comparison subroutine with 4 arguments which accepts left, right, ctx and
  returns result. For example
.vb
 subroutine CompareIntegers(left,right,ctx,result)
   implicit none

   PetscInt,intent(in) :: left, right
   type(UserCtx)       :: ctx
   integer,intent(out) :: result

   if (left < right) then
     result = -1
   else if (left == right) then
     result = 0
   else
     result = 1
   end if
   return
 end subroutine CompareIntegers
.ve

  References:
. * - Tim Peters. https://bugs.python.org/file4451/timsort.txt

  Level: developer

.seealso: `PetscTimSortWithArray()`, `PetscIntSortSemiOrdered()`, `PetscRealSortSemiOrdered()`, `PetscMPIIntSortSemiOrdered()`
@*/
PetscErrorCode PetscTimSort(PetscInt n, void *arr, size_t size, int (*cmp)(const void *, const void *, void *), void *ctx)
{
  PetscInt           stacksize = 0, minrun, runstart = 0, runend = 0;
  PetscTimSortStack  runstack[128];
  PetscTimSortBuffer buff;
  /* stacksize  = log_phi(n) = log_2(n)/log_2(phi), so 128 is enough for ~5.614e26 elements.
   It is so unlikely that this limit is reached that this is __never__ checked for */

  PetscFunctionBegin;
  /* Compute minrun. Minrun should be (32, 65) such that N/minrun
   is a power of 2 or one plus a power of 2 */
  {
    PetscInt t = n, r = 0;
    /* r becomes 1 if the least significant bits contain at least one off bit */
    while (t >= 64) {
      r |= t & 1;
      t >>= 1;
    }
    minrun = t + r;
  }
  if (PetscDefined(USE_DEBUG)) {
    PetscCall(PetscInfo(NULL, "minrun = %" PetscInt_FMT "\n", minrun));
    if (n < 64) {
      PetscCall(PetscInfo(NULL, "n %" PetscInt_FMT " < 64, consider using PetscSortInt() instead\n", n));
    } else PetscCheck(!(minrun < 32) && !(minrun > 65), PETSC_COMM_SELF, PETSC_ERR_PLIB, "Calculated minrun %" PetscInt_FMT " not in range (32,65)", minrun);
  }
  PetscCall(PetscMalloc1((size_t)minrun * size, &buff.ptr));
  buff.size         = (size_t)minrun * size;
  buff.maxsize      = (size_t)n * size;
  MIN_GALLOP_GLOBAL = MIN_GALLOP_CONST_GLOBAL;
  while (runstart < n) {
    /* Check if additional entries are at least partially ordered and build natural run */
    PetscCall(PetscTimSortBuildRun_Private((char *)arr, buff.ptr, size, cmp, ctx, n, minrun, runstart, &runend));
    runstack[stacksize].start = runstart;
    runstack[stacksize].size  = runend - runstart + 1;
    PetscCall(PetscTimSortMergeCollapse_Private((char *)arr, size, cmp, ctx, &buff, runstack, &stacksize));
    ++stacksize;
    runstart = runend + 1;
  }
  /* Have been inside while, so discard last stacksize++ */
  --stacksize;
  PetscCall(PetscTimSortForceCollapse_Private((char *)arr, size, cmp, ctx, &buff, runstack, stacksize));
  PetscCall(PetscFree(buff.ptr));
  MIN_GALLOP_GLOBAL = MIN_GALLOP_CONST_GLOBAL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscTimSortWithArray - Sorts an array in place in increasing order using Tim Peters adaptive sorting algorithm and
  reorders a second array to match the first. The arrays need not be the same type.

  Not Collective

  Input Parameters:
+ n     - number of values
. asize - size in bytes of the datatype held in arr
. bsize - size in bytes of the datatype held in barr
. cmp   - function pointer to comparison function
- ctx   - optional context to be passed to comparison function, NULL if not needed

  Input/Output Parameters:
+ arr  - array to be sorted, on output it is sorted
- barr - array to be reordered, on output it is reordered

  Notes:
  The arrays need not be of the same type, however barr MUST contain at least as many elements as arr and the two CANNOT
  overlap.

  Timsort makes the assumption that input data is already likely partially ordered, or that it contains contiguous
  sections (termed 'runs') where the data is locally ordered (but not necessarily globally ordered). It therefore aims
 to select slices of the array in such a way that resulting mergesorts operate on near perfectly length-balanced
 arrays. To do so it repeatedly triggers attempts throughout to merge adjacent runs.

  Should one run continuously "win" a comparison the algorithm begins the "gallop" phase. It will aggressively search
  the "winner" for the location of the "losers" next entry (and vice versa) to copy all preceding elements into place in
  bulk. However if the data is truly unordered (as is the case with random data) the immense gains possible from these
  searches are expected __not__ to repay their costs. While adjacent arrays are almost all nearly the same size, they
  likely all contain similar data.

  Sample usage:
  The comparison function must follow the `qsort()` comparison function paradigm, returning the sign of the difference
  between its arguments. If left < right : return -1, if left == right : return 0, if left > right : return 1. The user
  may also change or reverse the order of the sort by flipping the above. Note that stability of the sort is only
  guaranteed if the comparison function forms a valid trigraph. For example when sorting an array of type "my_type" in
  increasing order

.vb
  int my_increasing_comparison_function(const void *left, const void *right, void *ctx) {
    my_type l = *(my_type *) left, r = *(my_type *) right;
    return (l < r) ? -1 : (l > r);
  }
.ve
  Note the context is unused here but you may use it to pass and subsequently access whatever information required
  inside the comparison function. The context pointer will unaltered except for any changes made inside the comparison function.
  Then pass the function
.vb
  PetscTimSortWithArray(n, arr, sizeof(arr[0]), barr, sizeof(barr[0]), my_increasing_comparison_function, ctx)
.ve

  Fortran Notes:
  To use this from fortran you must write a comparison subroutine with 4 arguments which accepts left, right, ctx and
  returns result. For example
.vb
 subroutine CompareIntegers(left,right,ctx,result)
   implicit none

   PetscInt,intent(in) :: left, right
   type(UserCtx)       :: ctx
   integer,intent(out) :: result

   if (left < right) then
     result = -1
   else if (left == right) then
     result = 0
   else
     result = 1
   end if
   return
 end subroutine CompareIntegers
.ve

  References:
. * - Tim Peters. https://bugs.python.org/file4451/timsort.txt

  Level: developer

.seealso: `PetscTimSort()`, `PetscIntSortSemiOrderedWithArray()`, `PetscRealSortSemiOrderedWithArrayInt()`, `PetscMPIIntSortSemiOrderedWithArray()`
@*/
PetscErrorCode PetscTimSortWithArray(PetscInt n, void *arr, size_t asize, void *barr, size_t bsize, int (*cmp)(const void *, const void *, void *), void *ctx)
{
  PetscInt           stacksize = 0, minrun, runstart = 0, runend = 0;
  PetscTimSortStack  runstack[128];
  PetscTimSortBuffer abuff, bbuff;
  /* stacksize  = log_phi(n) = log_2(n)/log_2(phi), so 128 is enough for ~5.614e26 elements.
   It is so unlikely that this limit is reached that this is __never__ checked for */

  PetscFunctionBegin;
  /* Compute minrun. Minrun should be (32, 65) such that N/minrun
   is a power of 2 or one plus a power of 2 */
  {
    PetscInt t = n, r = 0;
    /* r becomes 1 if the least significant bits contain at least one off bit */
    while (t >= 64) {
      r |= t & 1;
      t >>= 1;
    }
    minrun = t + r;
  }
  if (PetscDefined(USE_DEBUG)) {
    PetscCall(PetscInfo(NULL, "minrun = %" PetscInt_FMT "\n", minrun));
    PetscCheck(n < 64 || (minrun >= 32 && minrun <= 65), PETSC_COMM_SELF, PETSC_ERR_PLIB, "Calculated minrun %" PetscInt_FMT " not in range (32,65)", minrun);
  }
  PetscCall(PetscMalloc1((size_t)minrun * asize, &abuff.ptr));
  abuff.size    = (size_t)minrun * asize;
  abuff.maxsize = (size_t)n * asize;
  PetscCall(PetscMalloc1((size_t)minrun * bsize, &bbuff.ptr));
  bbuff.size        = (size_t)minrun * bsize;
  bbuff.maxsize     = (size_t)n * bsize;
  MIN_GALLOP_GLOBAL = MIN_GALLOP_CONST_GLOBAL;
  while (runstart < n) {
    /* Check if additional entries are at least partially ordered and build natural run */
    PetscCall(PetscTimSortBuildRunWithArray_Private((char *)arr, abuff.ptr, asize, (char *)barr, bbuff.ptr, bsize, cmp, ctx, n, minrun, runstart, &runend));
    runstack[stacksize].start = runstart;
    runstack[stacksize].size  = runend - runstart + 1;
    PetscCall(PetscTimSortMergeCollapseWithArray_Private((char *)arr, asize, (char *)barr, bsize, cmp, ctx, &abuff, &bbuff, runstack, &stacksize));
    ++stacksize;
    runstart = runend + 1;
  }
  /* Have been inside while, so discard last stacksize++ */
  --stacksize;
  PetscCall(PetscTimSortForceCollapseWithArray_Private((char *)arr, asize, (char *)barr, bsize, cmp, ctx, &abuff, &bbuff, runstack, stacksize));
  PetscCall(PetscFree(abuff.ptr));
  PetscCall(PetscFree(bbuff.ptr));
  MIN_GALLOP_GLOBAL = MIN_GALLOP_CONST_GLOBAL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscIntSortSemiOrdered - Sorts an array of `PetscInt` in place in increasing order.

   Not Collective

   Input Parameters:
+  n   - number of values
-  arr - array of integers

   Output Parameter:
.  arr - sorted array of integers

   Notes:
   If the array is less than 64 entries long `PetscSortInt()` is automatically used.

   This function serves as an alternative to `PetscSortInt()`. While this function works for any array of integers it is
   significantly faster if the array is not totally random. There are exceptions to this and so it is __highly__
   recommended that the user benchmark their code to see which routine is fastest.

   Level: intermediate

.seealso: `PetscTimSort()`, `PetscSortInt()`, `PetscSortIntWithPermutation()`
@*/
PetscErrorCode PetscIntSortSemiOrdered(PetscInt n, PetscInt arr[])
{
  PetscFunctionBegin;
  if (n <= 1) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidIntPointer(arr, 2);
  if (n < 64) {
    PetscCall(PetscSortInt(n, arr));
  } else {
    PetscCall(PetscTimSort(n, arr, sizeof(PetscInt), Compare_PetscInt_Private, NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscIntSortSemiOrderedWithArray - Sorts an array of `PetscInt` in place in increasing order and reorders a second
   `PetscInt` array to match the first.

   Not Collective

   Input Parameter:
.  n   - number of values

   Input/Output Parameters:
+  arr1 - array of integers to be sorted, modified on output
-  arr2 - array of integers to be reordered, modified on output

   Notes:
   The arrays CANNOT overlap.

   This function serves as an alternative to PetscSortIntWithArray(). While this function works for any array of integers it is
   significantly faster if the array is not totally random. There are exceptions to this and so it is __highly__
   recommended that the user benchmark their code to see which routine is fastest.

   Level: intermediate

.seealso: `PetscTimSortWithArray()`, `PetscSortIntWithArray()`, `PetscSortIntWithPermutation()`
@*/
PetscErrorCode PetscIntSortSemiOrderedWithArray(PetscInt n, PetscInt arr1[], PetscInt arr2[])
{
  PetscFunctionBegin;
  if (n <= 1) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidIntPointer(arr1, 2);
  PetscValidIntPointer(arr2, 3);
  /* cannot export out to PetscIntSortWithArray here since it isn't stable */
  PetscCall(PetscTimSortWithArray(n, arr1, sizeof(PetscInt), arr2, sizeof(PetscInt), Compare_PetscInt_Private, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscMPIIntSortSemiOrdered - Sorts an array of `PetscMPIInt` in place in increasing order.

   Not Collective

   Input Parameters:
+  n   - number of values
-  arr - array of `PetscMPIInt`

   Output Parameter:
.  arr - sorted array of integers

   Notes:
   If the array is less than 64 entries long `PetscSortMPIInt()` is automatically used.

   This function serves as an alternative to `PetscSortMPIInt()`. While this function works for any array of `PetscMPIInt` it is
   significantly faster if the array is not totally random. There are exceptions to this and so it is __highly__
   recommended that the user benchmark their code to see which routine is fastest.

   Level: intermediate

.seealso: `PetscTimSort()`, `PetscSortMPIInt()`
@*/
PetscErrorCode PetscMPIIntSortSemiOrdered(PetscInt n, PetscMPIInt arr[])
{
  PetscFunctionBegin;
  if (n <= 1) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidIntPointer(arr, 2);
  if (n < 64) {
    PetscCall(PetscSortMPIInt(n, arr));
  } else {
    PetscCall(PetscTimSort(n, arr, sizeof(PetscMPIInt), Compare_PetscMPIInt_Private, NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscMPIIntSortSemiOrderedWithArray - Sorts an array of `PetscMPIInt` in place in increasing order and reorders a second `PetscMPIInt`
   array to match the first.

   Not Collective

   Input Parameter:
.  n   - number of values

   Input/Output Parameters:
.  arr1 - array of integers to be sorted, modified on output
-  arr2 - array of integers to be reordered, modified on output

   Notes:
   The arrays CANNOT overlap.

   This function serves as an alternative to `PetscSortMPIIntWithArray()`. While this function works for any array of integers it is
   significantly faster if the array is not totally random. There are exceptions to this and so it is __highly__
   recommended that the user benchmark their code to see which routine is fastest.

   Level: intermediate

.seealso: `PetscTimSortWithArray()`, `PetscSortMPIIntWithArray()`, `PetscSortMPIIntWithPermutation()`
@*/
PetscErrorCode PetscMPIIntSortSemiOrderedWithArray(PetscInt n, PetscMPIInt arr1[], PetscMPIInt arr2[])
{
  PetscFunctionBegin;
  if (n <= 1) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidIntPointer(arr1, 2);
  PetscValidIntPointer(arr2, 3);
  /* cannot export out to PetscMPIIntSortWithArray here since it isn't stable */
  PetscCall(PetscTimSortWithArray(n, arr1, sizeof(PetscMPIInt), arr2, sizeof(PetscMPIInt), Compare_PetscMPIInt_Private, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscRealSortSemiOrdered - Sorts an array of `PetscReal` in place in increasing order.

   Not Collective

   Input Parameters:
+  n   - number of values
-  arr - array of `PetscReal`

   Output Parameter:
.  arr - sorted array of integers

   Notes:
   If the array is less than 64 entries long `PetscSortReal()` is automatically used.

   This function serves as an alternative to `PetscSortReal()`. While this function works for any array of `PetscReal` it is
   significantly faster if the array is not totally random. There are exceptions to this and so it is __highly__
   recommended that the user benchmark their code to see which routine is fastest.

   Level: intermediate

.seealso: `PetscTimSort()`, `PetscSortReal()`, `PetscSortRealWithPermutation()`
@*/
PetscErrorCode PetscRealSortSemiOrdered(PetscInt n, PetscReal arr[])
{
  PetscFunctionBegin;
  if (n <= 1) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidRealPointer(arr, 2);
  if (n < 64) {
    PetscCall(PetscSortReal(n, arr));
  } else {
    PetscCall(PetscTimSort(n, arr, sizeof(PetscReal), Compare_PetscReal_Private, NULL));
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscRealSortSemiOrderedWithArrayInt - Sorts an array of `PetscReal` in place in increasing order and reorders a second
   array of `PetscInt` to match the first.

   Not Collective

   Input Parameter:
.  n   - number of values

   Input/Output Parameters:
.  arr1 - array of `PetscReal` to be sorted, modified on output
-  arr2 - array of `PetscInt` to be reordered, modified on output

   Notes:
   This function serves as an alternative to `PetscSortRealWithArray()`. While this function works for any array of `PetscReal` it is
   significantly faster if the array is not totally random. There are exceptions to this and so it is __highly__
   recommended that the user benchmark their code to see which routine is fastest.

   Level: intermediate

.seealso: `PetscTimSortWithArray()`, `PetscSortRealWithArrayInt()`, `PetscSortRealWithPermutation()`
@*/
PetscErrorCode PetscRealSortSemiOrderedWithArrayInt(PetscInt n, PetscReal arr1[], PetscInt arr2[])
{
  PetscFunctionBegin;
  if (n <= 1) PetscFunctionReturn(PETSC_SUCCESS);
  PetscValidRealPointer(arr1, 2);
  PetscValidIntPointer(arr2, 3);
  /* cannot export out to PetscRealSortWithArrayInt here since it isn't stable */
  PetscCall(PetscTimSortWithArray(n, arr1, sizeof(PetscReal), arr2, sizeof(PetscInt), Compare_PetscReal_Private, NULL));
  PetscFunctionReturn(PETSC_SUCCESS);
}

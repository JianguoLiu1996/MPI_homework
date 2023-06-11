
/*
   This file contains simple binary read/write routines.
 */

#include <petscsys.h>
#include <petscbt.h>
#include <errno.h>
#include <fcntl.h>
#if defined(PETSC_HAVE_UNISTD_H)
  #include <unistd.h>
#endif
#if defined(PETSC_HAVE_IO_H)
  #include <io.h>
#endif
#if !defined(PETSC_HAVE_O_BINARY)
  #define O_BINARY 0
#endif

const char *const PetscFileModes[] = {"READ", "WRITE", "APPEND", "UPDATE", "APPEND_UPDATE", "PetscFileMode", "PETSC_FILE_", NULL};

/* --------------------------------------------------------- */
/*
  PetscByteSwapEnum - Swap bytes in a  PETSc Enum

*/
PetscErrorCode PetscByteSwapEnum(PetscEnum *buff, PetscInt n)
{
  PetscInt  i, j;
  PetscEnum tmp = ENUM_DUMMY;
  char     *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff + j);
    for (i = 0; i < (PetscInt)sizeof(PetscEnum); i++) ptr2[i] = ptr1[sizeof(PetscEnum) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(PetscEnum); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  PetscByteSwapBool - Swap bytes in a  PETSc Bool

*/
PetscErrorCode PetscByteSwapBool(PetscBool *buff, PetscInt n)
{
  PetscInt  i, j;
  PetscBool tmp = PETSC_FALSE;
  char     *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff + j);
    for (i = 0; i < (PetscInt)sizeof(PetscBool); i++) ptr2[i] = ptr1[sizeof(PetscBool) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(PetscBool); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  PetscByteSwapInt - Swap bytes in a  PETSc integer (which may be 32 or 64 bits)

*/
PetscErrorCode PetscByteSwapInt(PetscInt *buff, PetscInt n)
{
  PetscInt i, j, tmp = 0;
  char    *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff + j);
    for (i = 0; i < (PetscInt)sizeof(PetscInt); i++) ptr2[i] = ptr1[sizeof(PetscInt) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(PetscInt); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  PetscByteSwapInt64 - Swap bytes in a  PETSc integer (64 bits)

*/
PetscErrorCode PetscByteSwapInt64(PetscInt64 *buff, PetscInt n)
{
  PetscInt   i, j;
  PetscInt64 tmp = 0;
  char      *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff + j);
    for (i = 0; i < (PetscInt)sizeof(PetscInt64); i++) ptr2[i] = ptr1[sizeof(PetscInt64) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(PetscInt64); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/* --------------------------------------------------------- */
/*
  PetscByteSwapShort - Swap bytes in a short
*/
PetscErrorCode PetscByteSwapShort(short *buff, PetscInt n)
{
  PetscInt i, j;
  short    tmp;
  char    *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff + j);
    for (i = 0; i < (PetscInt)sizeof(short); i++) ptr2[i] = ptr1[sizeof(short) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(short); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}
/*
  PetscByteSwapLong - Swap bytes in a long
*/
PetscErrorCode PetscByteSwapLong(long *buff, PetscInt n)
{
  PetscInt i, j;
  long     tmp;
  char    *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff + j);
    for (i = 0; i < (PetscInt)sizeof(long); i++) ptr2[i] = ptr1[sizeof(long) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(long); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}
/* --------------------------------------------------------- */
/*
  PetscByteSwapReal - Swap bytes in a PetscReal
*/
PetscErrorCode PetscByteSwapReal(PetscReal *buff, PetscInt n)
{
  PetscInt  i, j;
  PetscReal tmp, *buff1 = (PetscReal *)buff;
  char     *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff1 + j);
    for (i = 0; i < (PetscInt)sizeof(PetscReal); i++) ptr2[i] = ptr1[sizeof(PetscReal) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(PetscReal); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}
/* --------------------------------------------------------- */
/*
  PetscByteSwapScalar - Swap bytes in a PetscScalar
  The complex case is dealt with with an array of PetscReal, twice as long.
*/
PetscErrorCode PetscByteSwapScalar(PetscScalar *buff, PetscInt n)
{
  PetscInt  i, j;
  PetscReal tmp, *buff1 = (PetscReal *)buff;
  char     *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
#if defined(PETSC_USE_COMPLEX)
  n *= 2;
#endif
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff1 + j);
    for (i = 0; i < (PetscInt)sizeof(PetscReal); i++) ptr2[i] = ptr1[sizeof(PetscReal) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(PetscReal); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}
/* --------------------------------------------------------- */
/*
  PetscByteSwapDouble - Swap bytes in a double
*/
PetscErrorCode PetscByteSwapDouble(double *buff, PetscInt n)
{
  PetscInt i, j;
  double   tmp, *buff1 = (double *)buff;
  char    *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff1 + j);
    for (i = 0; i < (PetscInt)sizeof(double); i++) ptr2[i] = ptr1[sizeof(double) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(double); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*
  PetscByteSwapFloat - Swap bytes in a float
*/
PetscErrorCode PetscByteSwapFloat(float *buff, PetscInt n)
{
  PetscInt i, j;
  float    tmp, *buff1 = (float *)buff;
  char    *ptr1, *ptr2 = (char *)&tmp;

  PetscFunctionBegin;
  for (j = 0; j < n; j++) {
    ptr1 = (char *)(buff1 + j);
    for (i = 0; i < (PetscInt)sizeof(float); i++) ptr2[i] = ptr1[sizeof(float) - 1 - i];
    for (i = 0; i < (PetscInt)sizeof(float); i++) ptr1[i] = ptr2[i];
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode PetscByteSwap(void *data, PetscDataType pdtype, PetscInt count)
{
  PetscFunctionBegin;
  if (pdtype == PETSC_INT) PetscCall(PetscByteSwapInt((PetscInt *)data, count));
  else if (pdtype == PETSC_ENUM) PetscCall(PetscByteSwapEnum((PetscEnum *)data, count));
  else if (pdtype == PETSC_BOOL) PetscCall(PetscByteSwapBool((PetscBool *)data, count));
  else if (pdtype == PETSC_SCALAR) PetscCall(PetscByteSwapScalar((PetscScalar *)data, count));
  else if (pdtype == PETSC_REAL) PetscCall(PetscByteSwapReal((PetscReal *)data, count));
  else if (pdtype == PETSC_COMPLEX) PetscCall(PetscByteSwapReal((PetscReal *)data, 2 * count));
  else if (pdtype == PETSC_INT64) PetscCall(PetscByteSwapInt64((PetscInt64 *)data, count));
  else if (pdtype == PETSC_DOUBLE) PetscCall(PetscByteSwapDouble((double *)data, count));
  else if (pdtype == PETSC_FLOAT) PetscCall(PetscByteSwapFloat((float *)data, count));
  else if (pdtype == PETSC_SHORT) PetscCall(PetscByteSwapShort((short *)data, count));
  else if (pdtype == PETSC_LONG) PetscCall(PetscByteSwapLong((long *)data, count));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscBinaryRead - Reads from a binary file.

   Not Collective

   Input Parameters:
+  fd - the file descriptor
.  num  - the maximum number of items to read
-  type - the type of items to read (`PETSC_INT`, `PETSC_REAL`, `PETSC_SCALAR`, etc.)

   Output Parameters:
+  data - the buffer
-  count - the number of items read, optional

   Level: developer

   Notes:
   If count is not provided and the number of items read is less than
   the maximum number of items to read, then this routine errors.

   `PetscBinaryRead()` uses byte swapping to work on all machines; the files
   are written to file ALWAYS using big-endian ordering. On little-endian machines the numbers
   are converted to the little-endian format when they are read in from the file.
   When PETSc is ./configure with --with-64-bit-indices the integers are written to the
   file as 64 bit integers, this means they can only be read back in when the option `--with-64-bit-indices`
   is used.

.seealso: `PetscBinaryWrite()`, `PetscBinaryOpen()`, `PetscBinaryClose()`, `PetscViewerBinaryGetDescriptor()`, `PetscBinarySynchronizedWrite()`,
          `PetscBinarySynchronizedRead()`, `PetscBinarySynchronizedSeek()`
@*/
PetscErrorCode PetscBinaryRead(int fd, void *data, PetscInt num, PetscInt *count, PetscDataType type)
{
  size_t typesize, m = (size_t)num, n = 0, maxblock = 65536;
  char  *p = (char *)data;
#if defined(PETSC_USE_REAL___FLOAT128)
  PetscBool readdouble = PETSC_FALSE;
  double   *pdouble;
#endif
  void *ptmp  = data;
  char *fname = NULL;

  PetscFunctionBegin;
  if (count) *count = 0;
  PetscCheck(num >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Trying to read a negative amount of data %" PetscInt_FMT, num);
  if (!num) PetscFunctionReturn(PETSC_SUCCESS);

  if (type == PETSC_FUNCTION) {
    m     = 64;
    type  = PETSC_CHAR;
    fname = (char *)malloc(m * sizeof(char));
    p     = (char *)fname;
    ptmp  = (void *)fname;
    PetscCheck(fname, PETSC_COMM_SELF, PETSC_ERR_MEM, "Cannot allocate space for function name");
  }
  if (type == PETSC_BIT_LOGICAL) m = PetscBTLength(m);

  PetscCall(PetscDataTypeGetSize(type, &typesize));

#if defined(PETSC_USE_REAL___FLOAT128)
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-binary_read_double", &readdouble, NULL));
  /* If using __float128 precision we still read in doubles from file */
  if ((type == PETSC_REAL || type == PETSC_COMPLEX) && readdouble) {
    PetscInt cnt = num * ((type == PETSC_REAL) ? 1 : 2);
    PetscCall(PetscMalloc1(cnt, &pdouble));
    p = (char *)pdouble;
    typesize /= 2;
  }
#endif

  m *= typesize;

  while (m) {
    size_t len = (m < maxblock) ? m : maxblock;
    int    ret = (int)read(fd, p, len);
    if (ret < 0 && errno == EINTR) continue;
    if (!ret && len > 0) break; /* Proxy for EOF */
    PetscCheck(ret >= 0, PETSC_COMM_SELF, PETSC_ERR_FILE_READ, "Error reading from file, errno %d", errno);
    m -= (size_t)ret;
    p += ret;
    n += (size_t)ret;
  }
  PetscCheck(!m || count, PETSC_COMM_SELF, PETSC_ERR_FILE_READ, "Read past end of file");

  num = (PetscInt)(n / typesize); /* Should we require `n % typesize == 0` ? */
  if (count) *count = num;        /* TODO: This is most likely wrong for PETSC_BIT_LOGICAL */

#if defined(PETSC_USE_REAL___FLOAT128)
  if ((type == PETSC_REAL || type == PETSC_COMPLEX) && readdouble) {
    PetscInt   i, cnt = num * ((type == PETSC_REAL) ? 1 : 2);
    PetscReal *preal = (PetscReal *)data;
    if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwapDouble(pdouble, cnt));
    for (i = 0; i < cnt; i++) preal[i] = pdouble[i];
    PetscCall(PetscFree(pdouble));
    PetscFunctionReturn(PETSC_SUCCESS);
  }
#endif

  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(ptmp, type, num));

  if (type == PETSC_FUNCTION) {
#if defined(PETSC_SERIALIZE_FUNCTIONS)
    PetscCall(PetscDLSym(NULL, fname, (void **)data));
#else
    *(void **)data = NULL;
#endif
    free(fname);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscBinaryWrite - Writes to a binary file.

   Not Collective

   Input Parameters:
+  fd     - the file
.  p      - the buffer
.  n      - the number of items to write
-  type   - the type of items to read (`PETSC_INT`, `PETSC_REAL` or `PETSC_SCALAR`)

   Level: advanced

   Notes:
   `PetscBinaryWrite()` uses byte swapping to work on all machines; the files
   are written using big-endian ordering to the file. On little-endian machines the numbers
   are converted to the big-endian format when they are written to disk.
   When PETSc is ./configure with --with-64-bit-indices the integers are written to the
   file as 64 bit integers, this means they can only be read back in when the option `--with-64-bit-indices`
   is used.

   If running with `__float128` precision the output is in `__float128` unless one uses the `-binary_write_double` option

   The buffer `p` should be read-write buffer, and not static data.
   This way, byte-swapping is done in-place, and then the buffer is
   written to the file.

   This routine restores the original contents of the buffer, after
   it is written to the file. This is done by byte-swapping in-place
   the second time.

   Because byte-swapping may be done on the values in data it cannot be declared const

.seealso: `PetscBinaryRead()`, `PetscBinaryOpen()`, `PetscBinaryClose()`, `PetscViewerBinaryGetDescriptor()`, `PetscBinarySynchronizedWrite()`,
          `PetscBinarySynchronizedRead()`, `PetscBinarySynchronizedSeek()`
@*/
PetscErrorCode PetscBinaryWrite(int fd, const void *p, PetscInt n, PetscDataType type)
{
  const char *pp = (char *)p;
  int         err, wsize;
  size_t      m = (size_t)n, maxblock = 65536;
  const void *ptmp  = p;
  char       *fname = NULL;
#if defined(PETSC_USE_REAL___FLOAT128)
  PetscBool  writedouble = PETSC_FALSE;
  double    *ppp;
  PetscReal *pv;
  PetscInt   i;
#endif
  PetscDataType wtype = type;

  PetscFunctionBegin;
  PetscCheck(n >= 0, PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Trying to write a negative amount of data %" PetscInt_FMT, n);
  if (!n) PetscFunctionReturn(PETSC_SUCCESS);

  if (type == PETSC_FUNCTION) {
#if defined(PETSC_SERIALIZE_FUNCTIONS)
    const char *fnametmp;
#endif
    m     = 64;
    fname = (char *)malloc(m * sizeof(char));
    PetscCheck(fname, PETSC_COMM_SELF, PETSC_ERR_MEM, "Cannot allocate space for function name");
#if defined(PETSC_SERIALIZE_FUNCTIONS)
    PetscCheck(n <= 1, PETSC_COMM_SELF, PETSC_ERR_SUP, "Can only binary view a single function at a time");
    PetscCall(PetscFPTFind(*(void **)p, &fnametmp));
    PetscCall(PetscStrncpy(fname, fnametmp, m));
#else
    PetscCall(PetscStrncpy(fname, "", m));
#endif
    wtype = PETSC_CHAR;
    pp    = (char *)fname;
    ptmp  = (void *)fname;
  }

#if defined(PETSC_USE_REAL___FLOAT128)
  PetscCall(PetscOptionsGetBool(NULL, NULL, "-binary_write_double", &writedouble, NULL));
  /* If using __float128 precision we still write in doubles to file */
  if ((type == PETSC_SCALAR || type == PETSC_REAL || type == PETSC_COMPLEX) && writedouble) {
    wtype = PETSC_DOUBLE;
    PetscCall(PetscMalloc1(n, &ppp));
    pv = (PetscReal *)pp;
    for (i = 0; i < n; i++) ppp[i] = (double)pv[i];
    pp   = (char *)ppp;
    ptmp = (char *)ppp;
  }
#endif

  if (wtype == PETSC_INT) m *= sizeof(PetscInt);
  else if (wtype == PETSC_SCALAR) m *= sizeof(PetscScalar);
#if defined(PETSC_HAVE_COMPLEX)
  else if (wtype == PETSC_COMPLEX) m *= sizeof(PetscComplex);
#endif
  else if (wtype == PETSC_REAL) m *= sizeof(PetscReal);
  else if (wtype == PETSC_DOUBLE) m *= sizeof(double);
  else if (wtype == PETSC_FLOAT) m *= sizeof(float);
  else if (wtype == PETSC_SHORT) m *= sizeof(short);
  else if (wtype == PETSC_LONG) m *= sizeof(long);
  else if (wtype == PETSC_CHAR) m *= sizeof(char);
  else if (wtype == PETSC_ENUM) m *= sizeof(PetscEnum);
  else if (wtype == PETSC_BOOL) m *= sizeof(PetscBool);
  else if (wtype == PETSC_INT64) m *= sizeof(PetscInt64);
  else if (wtype == PETSC_BIT_LOGICAL) m = PetscBTLength(m) * sizeof(char);
  else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Unknown type");

  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap((void *)ptmp, wtype, n));

  while (m) {
    wsize = (m < maxblock) ? m : maxblock;
    err   = write(fd, pp, wsize);
    if (err < 0 && errno == EINTR) continue;
    PetscCheck(err == wsize, PETSC_COMM_SELF, PETSC_ERR_FILE_WRITE, "Error writing to file total size %d err %d wsize %d", (int)n, (int)err, (int)wsize);
    m -= wsize;
    pp += wsize;
  }

  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap((void *)ptmp, wtype, n));

  if (type == PETSC_FUNCTION) free(fname);
#if defined(PETSC_USE_REAL___FLOAT128)
  if ((type == PETSC_SCALAR || type == PETSC_REAL || type == PETSC_COMPLEX) && writedouble) PetscCall(PetscFree(ppp));
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscBinaryOpen - Opens a PETSc binary file.

   Not Collective

   Input Parameters:
+  name - filename
-  mode - open mode of binary file, one of `FILE_MODE_READ`, `FILE_MODE_WRITE`, `FILE_MODE_APPEND``

   Output Parameter:
.  fd - the file

   Level: advanced

   Note:
    Files access with PetscBinaryRead()` and `PetscBinaryWrite()` are ALWAYS written in
   big-endian format. This means the file can be accessed using `PetscBinaryOpen()` and
   `PetscBinaryRead()` and `PetscBinaryWrite()` on any machine.

.seealso: `PetscBinaryRead()`, `PetscBinaryWrite()`, `PetscFileMode`, `PetscViewerFileSetMode()`, `PetscViewerBinaryGetDescriptor()`,
          `PetscBinarySynchronizedWrite()`, `PetscBinarySynchronizedRead()`, `PetscBinarySynchronizedSeek()`
@*/
PetscErrorCode PetscBinaryOpen(const char name[], PetscFileMode mode, int *fd)
{
  PetscFunctionBegin;
  switch (mode) {
  case FILE_MODE_READ:
    *fd = open(name, O_BINARY | O_RDONLY, 0);
    break;
  case FILE_MODE_WRITE:
    *fd = open(name, O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, 0666);
    break;
  case FILE_MODE_APPEND:
    *fd = open(name, O_BINARY | O_WRONLY | O_APPEND, 0);
    break;
  default:
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP, "Unsupported file mode %s", PetscFileModes[mode]);
  }
  PetscCheck(*fd != -1, PETSC_COMM_SELF, PETSC_ERR_FILE_OPEN, "Cannot open file %s for %s", name, PetscFileModes[mode]);
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@
   PetscBinaryClose - Closes a PETSc binary file.

   Not Collective

   Output Parameter:
.  fd - the file

   Level: advanced

.seealso: `PetscBinaryRead()`, `PetscBinaryWrite()`, `PetscBinaryOpen()`, `PetscBinarySynchronizedWrite()`, `PetscBinarySynchronizedRead()`,
          `PetscBinarySynchronizedSeek()`
@*/
PetscErrorCode PetscBinaryClose(int fd)
{
  PetscFunctionBegin;
  PetscCheck(!close(fd), PETSC_COMM_SELF, PETSC_ERR_SYS, "close() failed on file descriptor");
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscBinarySeek - Moves the file pointer on a PETSc binary file.

   Not Collective

   Input Parameters:
+  fd - the file
.  off - number of bytes to move. Use `PETSC_BINARY_INT_SIZE`, `PETSC_BINARY_SCALAR_SIZE`,
            etc. in your calculation rather than sizeof() to compute byte lengths.
-  whence - see `PetscBinarySeekType` for possible values

   Output Parameter:
.   offset - new offset in file

   Level: developer

   Note:
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine. Hence you CANNOT use `sizeof()`
   to determine the offset or location.

.seealso: `PetscBinaryRead()`, `PetscBinarySeekType`, `PetscBinaryWrite()`, `PetscBinaryOpen()`, `PetscBinarySynchronizedWrite()`, `PetscBinarySynchronizedRead()`,
          `PetscBinarySynchronizedSeek()`
@*/
PetscErrorCode PetscBinarySeek(int fd, off_t off, PetscBinarySeekType whence, off_t *offset)
{
  int iwhence = 0;

  PetscFunctionBegin;
  if (whence == PETSC_BINARY_SEEK_SET) iwhence = SEEK_SET;
  else if (whence == PETSC_BINARY_SEEK_CUR) iwhence = SEEK_CUR;
  else if (whence == PETSC_BINARY_SEEK_END) iwhence = SEEK_END;
  else SETERRQ(PETSC_COMM_SELF, PETSC_ERR_ARG_OUTOFRANGE, "Unknown seek location");
#if defined(PETSC_HAVE_LSEEK)
  *offset = lseek(fd, off, iwhence);
#elif defined(PETSC_HAVE__LSEEK)
  *offset = _lseek(fd, (long)off, iwhence);
#else
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "System does not have a way of seeking on a file");
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscBinarySynchronizedRead - Reads from a binary file.

   Collective

   Input Parameters:
+  comm - the MPI communicator
.  fd - the file descriptor
.  num  - the maximum number of items to read
-  type - the type of items to read (`PETSC_INT`, `PETSC_REAL`, `PETSC_SCALAR`, etc.)

   Output Parameters:
+  data - the buffer
-  count - the number of items read, optional

   Level: developer

   Notes:
   Does a `PetscBinaryRead()` followed by an `MPI_Bcast()`

   If count is not provided and the number of items read is less than
   the maximum number of items to read, then this routine errors.

   `PetscBinarySynchronizedRead()` uses byte swapping to work on all machines.
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine.

.seealso: `PetscBinaryWrite()`, `PetscBinaryOpen()`, `PetscBinaryClose()`, `PetscBinaryRead()`, `PetscBinarySynchronizedWrite()`,
          `PetscBinarySynchronizedSeek()`
@*/
PetscErrorCode PetscBinarySynchronizedRead(MPI_Comm comm, int fd, void *data, PetscInt num, PetscInt *count, PetscDataType type)
{
  PetscMPIInt  rank, size;
  MPI_Datatype mtype;
  PetscInt     ibuf[2] = {0, 0};
  char        *fname   = NULL;
  void        *fptr    = NULL;

  PetscFunctionBegin;
  if (type == PETSC_FUNCTION) {
    num   = 64;
    type  = PETSC_CHAR;
    fname = (char *)malloc(num * sizeof(char));
    fptr  = data;
    data  = (void *)fname;
    PetscCheck(fname, PETSC_COMM_SELF, PETSC_ERR_MEM, "Cannot allocate space for function name");
  }

  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  PetscCallMPI(MPI_Comm_size(comm, &size));
  if (rank == 0) ibuf[0] = PetscBinaryRead(fd, data, num, count ? &ibuf[1] : NULL, type);
  PetscCallMPI(MPI_Bcast(ibuf, 2, MPIU_INT, 0, comm));
  PetscCall((PetscErrorCode)ibuf[0]);

  /* skip MPI call on potentially huge amounts of data when running with one process; this allows the amount of data to basically unlimited in that case */
  if (size > 1) {
    PetscCall(PetscDataTypeToMPIDataType(type, &mtype));
    PetscCallMPI(MPI_Bcast(data, count ? ibuf[1] : num, mtype, 0, comm));
  }
  if (count) *count = ibuf[1];

  if (type == PETSC_FUNCTION) {
#if defined(PETSC_SERIALIZE_FUNCTIONS)
    PetscCall(PetscDLLibrarySym(PETSC_COMM_SELF, &PetscDLLibrariesLoaded, NULL, fname, (void **)fptr));
#else
    *(void **)fptr = NULL;
#endif
    free(fname);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscBinarySynchronizedWrite - writes to a binary file.

   Collective

   Input Parameters:
+  comm - the MPI communicator
.  fd - the file
.  n  - the number of items to write
.  p - the buffer
-  type - the type of items to write (`PETSC_INT`, `PETSC_REAL` or `PETSC_SCALAR`)

   Level: developer

   Notes:
   MPI rank 0 does a `PetscBinaryWrite()`

   `PetscBinarySynchronizedWrite()` uses byte swapping to work on all machines.
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine.

   Because byte-swapping may be done on the values in data it cannot be declared const

   WARNING:
   This is NOT like `PetscSynchronizedFPrintf()`! This routine ignores calls on all but MPI rank 0,
   while `PetscSynchronizedFPrintf()` has all processes print their strings in order.

.seealso: `PetscBinaryWrite()`, `PetscBinaryOpen()`, `PetscBinaryClose()`, `PetscBinaryRead()`, `PetscBinarySynchronizedRead()`,
          `PetscBinarySynchronizedSeek()`
@*/
PetscErrorCode PetscBinarySynchronizedWrite(MPI_Comm comm, int fd, const void *p, PetscInt n, PetscDataType type)
{
  PetscMPIInt rank;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  if (rank == 0) PetscCall(PetscBinaryWrite(fd, p, n, type));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscBinarySynchronizedSeek - Moves the file pointer on a PETSc binary file.

   Input Parameters:
+  fd - the file
.  whence -  see `PetscBinarySeekType` for possible values
-  off    - number of bytes to move. Use `PETSC_BINARY_INT_SIZE`, `PETSC_BINARY_SCALAR_SIZE`,
            etc. in your calculation rather than `sizeof()` to compute byte lengths.

   Output Parameter:
.   offset - new offset in file

   Level: developer

   Note:
   Integers are stored on the file as 32 long, regardless of whether
   they are stored in the machine as 32 or 64, this means the same
   binary file may be read on any machine. Hence you CANNOT use `sizeof()`
   to determine the offset or location.

.seealso: `PetscBinaryRead()`, `PetscBinarySeekType`, `PetscBinaryWrite()`, `PetscBinaryOpen()`, `PetscBinarySynchronizedWrite()`, `PetscBinarySynchronizedRead()`,
          `PetscBinarySynchronizedSeek()`
@*/
PetscErrorCode PetscBinarySynchronizedSeek(MPI_Comm comm, int fd, off_t off, PetscBinarySeekType whence, off_t *offset)
{
  PetscMPIInt rank;

  PetscFunctionBegin;
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  if (rank == 0) PetscCall(PetscBinarySeek(fd, off, whence, offset));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#if defined(PETSC_HAVE_MPIIO)

  #if defined(PETSC_USE_PETSC_MPI_EXTERNAL32)
/*
      MPICH does not provide the external32 representation for MPI_File_set_view() so we need to provide the functions.
    These are set into MPI in PetscInitialize() via MPI_Register_datarep()

    Note I use PetscMPIInt for the MPI error codes since that is what MPI uses (instead of the standard PetscErrorCode)

    The next three routines are not used because MPICH does not support their use

*/
PETSC_EXTERN PetscMPIInt PetscDataRep_extent_fn(MPI_Datatype datatype, MPI_Aint *file_extent, void *extra_state)
{
  MPI_Aint    ub;
  PetscMPIInt ierr;

  ierr = MPI_Type_get_extent(datatype, &ub, file_extent);
  return ierr;
}

PETSC_EXTERN PetscMPIInt PetscDataRep_read_conv_fn(void *userbuf, MPI_Datatype datatype, PetscMPIInt count, void *filebuf, MPI_Offset position, void *extra_state)
{
  PetscDataType pdtype;
  PetscMPIInt   ierr;
  size_t        dsize;

  PetscCall(PetscMPIDataTypeToPetscDataType(datatype, &pdtype));
  PetscCall(PetscDataTypeGetSize(pdtype, &dsize));

  /* offset is given in units of MPI_Datatype */
  userbuf = ((char *)userbuf) + dsize * position;

  PetscCall(PetscMemcpy(userbuf, filebuf, count * dsize));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(userbuf, pdtype, count));
  return ierr;
}

PetscMPIInt PetscDataRep_write_conv_fn(void *userbuf, MPI_Datatype datatype, PetscMPIInt count, void *filebuf, MPI_Offset position, void *extra_state)
{
  PetscDataType pdtype;
  PetscMPIInt   ierr;
  size_t        dsize;

  PetscCall(PetscMPIDataTypeToPetscDataType(datatype, &pdtype));
  PetscCall(PetscDataTypeGetSize(pdtype, &dsize));

  /* offset is given in units of MPI_Datatype */
  userbuf = ((char *)userbuf) + dsize * position;

  PetscCall(PetscMemcpy(filebuf, userbuf, count * dsize));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(filebuf, pdtype, count));
  return ierr;
}
  #endif

PetscErrorCode MPIU_File_write_all(MPI_File fd, void *data, PetscMPIInt cnt, MPI_Datatype dtype, MPI_Status *status)
{
  PetscDataType pdtype;

  PetscFunctionBegin;
  PetscCall(PetscMPIDataTypeToPetscDataType(dtype, &pdtype));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscCallMPI(MPI_File_write_all(fd, data, cnt, dtype, status));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MPIU_File_read_all(MPI_File fd, void *data, PetscMPIInt cnt, MPI_Datatype dtype, MPI_Status *status)
{
  PetscDataType pdtype;

  PetscFunctionBegin;
  PetscCall(PetscMPIDataTypeToPetscDataType(dtype, &pdtype));
  PetscCallMPI(MPI_File_read_all(fd, data, cnt, dtype, status));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MPIU_File_write_at(MPI_File fd, MPI_Offset off, void *data, PetscMPIInt cnt, MPI_Datatype dtype, MPI_Status *status)
{
  PetscDataType pdtype;

  PetscFunctionBegin;
  PetscCall(PetscMPIDataTypeToPetscDataType(dtype, &pdtype));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscCallMPI(MPI_File_write_at(fd, off, data, cnt, dtype, status));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MPIU_File_read_at(MPI_File fd, MPI_Offset off, void *data, PetscMPIInt cnt, MPI_Datatype dtype, MPI_Status *status)
{
  PetscDataType pdtype;

  PetscFunctionBegin;
  PetscCall(PetscMPIDataTypeToPetscDataType(dtype, &pdtype));
  PetscCallMPI(MPI_File_read_at(fd, off, data, cnt, dtype, status));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MPIU_File_write_at_all(MPI_File fd, MPI_Offset off, void *data, PetscMPIInt cnt, MPI_Datatype dtype, MPI_Status *status)
{
  PetscDataType pdtype;

  PetscFunctionBegin;
  PetscCall(PetscMPIDataTypeToPetscDataType(dtype, &pdtype));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscCallMPI(MPI_File_write_at_all(fd, off, data, cnt, dtype, status));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode MPIU_File_read_at_all(MPI_File fd, MPI_Offset off, void *data, PetscMPIInt cnt, MPI_Datatype dtype, MPI_Status *status)
{
  PetscDataType pdtype;

  PetscFunctionBegin;
  PetscCall(PetscMPIDataTypeToPetscDataType(dtype, &pdtype));
  PetscCallMPI(MPI_File_read_at_all(fd, off, data, cnt, dtype, status));
  if (!PetscBinaryBigEndian()) PetscCall(PetscByteSwap(data, pdtype, cnt));
  PetscFunctionReturn(PETSC_SUCCESS);
}

#endif

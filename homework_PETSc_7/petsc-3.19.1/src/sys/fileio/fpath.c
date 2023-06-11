
#include <petscsys.h>
#if defined(PETSC_HAVE_PWD_H)
  #include <pwd.h>
#endif

/*@C
   PetscGetFullPath - Given a filename, returns the fully qualified file name.

   Not Collective

   Input Parameters:
+  path     - pathname to qualify
.  fullpath - pointer to buffer to hold full pathname
-  flen     - size of fullpath

   Level: developer

.seealso: `PetscGetRelativePath()`
@*/
PetscErrorCode PetscGetFullPath(const char path[], char fullpath[], size_t flen)
{
  size_t    ln;
  PetscBool flg;

  PetscFunctionBegin;
  if (path[0] == '/') {
    PetscCall(PetscStrncmp("/tmp_mnt/", path, 9, &flg));
    if (flg) PetscCall(PetscStrncpy(fullpath, path + 8, flen));
    else PetscCall(PetscStrncpy(fullpath, path, flen));
    fullpath[flen - 1] = 0;
    PetscFunctionReturn(PETSC_SUCCESS);
  }
  if (path[0] == '.' && path[1] == '/') {
    PetscCall(PetscGetWorkingDirectory(fullpath, flen));
    PetscCall(PetscStrlcat(fullpath, path + 1, flen));
    PetscFunctionReturn(PETSC_SUCCESS);
  }

  PetscCall(PetscStrncpy(fullpath, path, flen));
  fullpath[flen - 1] = 0;
  /* Remove the various "special" forms (~username/ and ~/) */
  if (fullpath[0] == '~') {
    char tmppath[PETSC_MAX_PATH_LEN], *rest;
    if (fullpath[1] == '/') {
      PetscCall(PetscGetHomeDirectory(tmppath, PETSC_MAX_PATH_LEN));
      rest = fullpath + 2;
    } else {
#if defined(PETSC_HAVE_PWD_H)
      struct passwd *pwde;
      char          *p, *name;

      /* Find username */
      name = fullpath + 1;
      p    = name;
      while (*p && *p != '/') p++;
      *p   = 0;
      rest = p + 1;
      pwde = getpwnam(name);
      if (!pwde) PetscFunctionReturn(PETSC_SUCCESS);

      PetscCall(PetscStrncpy(tmppath, pwde->pw_dir, sizeof(tmppath)));
#else
      PetscFunctionReturn(PETSC_SUCCESS);
#endif
    }
    PetscCall(PetscStrlen(tmppath, &ln));
    if (tmppath[ln - 1] != '/') PetscCall(PetscStrlcat(tmppath + ln - 1, "/", sizeof(tmppath) - ln + 1));
    PetscCall(PetscStrlcat(tmppath, rest, sizeof(tmppath)));
    PetscCall(PetscStrncpy(fullpath, tmppath, flen));
    fullpath[flen - 1] = 0;
  } else {
    PetscCall(PetscGetWorkingDirectory(fullpath, flen));
    PetscCall(PetscStrlen(fullpath, &ln));
    PetscCall(PetscStrncpy(fullpath + ln, "/", flen - ln));
    fullpath[flen - 1] = 0;
    PetscCall(PetscStrlen(fullpath, &ln));
    if (path[0] == '.' && path[1] == '/') {
      PetscCall(PetscStrlcat(fullpath, path + 2, flen));
    } else {
      PetscCall(PetscStrlcat(fullpath, path, flen));
    }
    fullpath[flen - 1] = 0;
  }

  /* Remove the automounter part of the path */
  PetscCall(PetscStrncmp(fullpath, "/tmp_mnt/", 9, &flg));
  if (flg) {
    char tmppath[PETSC_MAX_PATH_LEN];
    PetscCall(PetscStrncpy(tmppath, fullpath + 8, sizeof(tmppath)));
    PetscCall(PetscStrncpy(fullpath, tmppath, flen));
  }
  /* We could try to handle things like the removal of .. etc */
  PetscFunctionReturn(PETSC_SUCCESS);
}

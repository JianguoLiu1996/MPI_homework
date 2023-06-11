/*
   Low-level routines for managing dynamic link libraries (DLLs).
*/

#include <petscconf.h>
#if defined(PETSC__GNU_SOURCE)
  #if !defined(_GNU_SOURCE)
    #define _GNU_SOURCE 1
  #endif
#endif

#include <petsc/private/petscimpl.h>

#if defined(PETSC_HAVE_WINDOWS_H)
  #include <windows.h>
#endif
#if defined(PETSC_HAVE_DLFCN_H)
  #include <dlfcn.h>
#endif

#if defined(PETSC_HAVE_WINDOWS_H)
typedef HMODULE dlhandle_t;
typedef FARPROC dlsymbol_t;
#elif defined(PETSC_HAVE_DLFCN_H)
typedef void *dlhandle_t;
typedef void *dlsymbol_t;
#else
typedef void *dlhandle_t;
typedef void *dlsymbol_t;
#endif

/*@C
   PetscDLOpen - opens a dynamic library

   Not Collective

   Input Parameters:
+    name - name of library
-    mode - options on how to open library

   Output Parameter:
.    handle - opaque pointer to be used with `PetscDLSym()`

   Level: developer

.seealso: `PetscDLClose()`, `PetscDLSym()`, `PetscDLAddr()`
@*/
PetscErrorCode PetscDLOpen(const char name[], PetscDLMode mode, PetscDLHandle *handle)
{
  PETSC_UNUSED int dlflags1, dlflags2; /* There are some preprocessor paths where these variables are set, but not used */
  dlhandle_t       dlhandle;

  PetscFunctionBegin;
  PetscValidCharPointer(name, 1);
  PetscValidPointer(handle, 3);

  dlflags1 = 0;
  dlflags2 = 0;
  dlhandle = (dlhandle_t)0;
  *handle  = (PetscDLHandle)0;

  /*
     --- LoadLibrary ---
  */
#if defined(PETSC_HAVE_WINDOWS_H) && defined(PETSC_HAVE_LOADLIBRARY)
  dlhandle = LoadLibrary(name);
  if (!dlhandle) {
    /* TODO: Seem to need fixing, why not just return with an error with SETERRQ() */
  #if defined(PETSC_HAVE_GETLASTERROR)
    DWORD erc;
    char *buff = NULL;
    erc        = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, erc, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buff, 0, NULL);
    PetscCall(PetscError(PETSC_COMM_SELF, __LINE__, PETSC_FUNCTION_NAME, __FILE__, PETSC_ERR_FILE_OPEN, PETSC_ERROR_REPEAT, "Unable to open dynamic library:\n  %s\n  Error message from LoadLibrary() %s\n", name, buff));
    LocalFree(buff);
    PetscFunctionReturn(PETSC_SUCCESS);
  #else
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_FILE_OPEN, "Unable to open dynamic library:\n  %s\n  Error message from LoadLibrary() %s", name, "unavailable");
  #endif
  }

  /*
     --- dlopen ---
  */
#elif defined(PETSC_HAVE_DLFCN_H) && defined(PETSC_HAVE_DLOPEN)
  /*
      Mode indicates symbols required by symbol loaded with dlsym()
     are only loaded when required (not all together) also indicates
     symbols required can be contained in other libraries also opened
     with dlopen()
  */
  #if defined(PETSC_HAVE_RTLD_LAZY)
  dlflags1 = RTLD_LAZY;
  #endif
  #if defined(PETSC_HAVE_RTLD_NOW)
  if (mode & PETSC_DL_NOW) dlflags1 = RTLD_NOW;
  #endif
  #if defined(PETSC_HAVE_RTLD_GLOBAL)
  dlflags2 = RTLD_GLOBAL;
  #endif
  #if defined(PETSC_HAVE_RTLD_LOCAL)
  if (mode & PETSC_DL_LOCAL) dlflags2 = RTLD_LOCAL;
  #endif
  #if defined(PETSC_HAVE_DLERROR)
  dlerror(); /* clear any previous error */
  #endif
  dlhandle = dlopen(name, dlflags1 | dlflags2);
  if (!dlhandle) {
  #if defined(PETSC_HAVE_DLERROR)
    const char *errmsg = dlerror();
  #else
    const char *errmsg = "unavailable";
  #endif
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_FILE_OPEN, "Unable to open dynamic library:\n  %s\n  Error message from dlopen() %s", name, errmsg);
  }
  /*
     --- unimplemented ---
  */
#else
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "Cannot use dynamic libraries on this platform");
#endif

  *handle = (PetscDLHandle)dlhandle;
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   PetscDLClose -  closes a dynamic library

   Not Collective

  Input Parameter:
.   handle - the handle for the library obtained with `PetscDLOpen()`

  Level: developer

.seealso: `PetscDLOpen()`, `PetscDLSym()`, `PetscDLAddr()`
@*/
PetscErrorCode PetscDLClose(PetscDLHandle *handle)
{
  PetscFunctionBegin;
  PetscValidPointer(handle, 1);

  /*
     --- FreeLibrary ---
  */
#if defined(PETSC_HAVE_WINDOWS_H)
  #if defined(PETSC_HAVE_FREELIBRARY)
  if (FreeLibrary((dlhandle_t)*handle) == 0) {
    #if defined(PETSC_HAVE_GETLASTERROR)
    char *buff = NULL;
    DWORD erc  = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, erc, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buff, 0, NULL);
    PetscCall(PetscErrorPrintf("Error closing dynamic library:\n  Error message from FreeLibrary() %s\n", buff));
    LocalFree(buff);
    #else
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error closing dynamic library:\n  Error message from FreeLibrary() %s", "unavailable");
    #endif
  }
  #endif /* !PETSC_HAVE_FREELIBRARY */

  /*
     --- dclose ---
  */
#elif defined(PETSC_HAVE_DLFCN_H)
  #if defined(PETSC_HAVE_DLCLOSE)
    #if defined(PETSC_HAVE_DLERROR)
  dlerror(); /* clear any previous error */
    #endif
  if (dlclose((dlhandle_t)*handle) < 0) {
    #if defined(PETSC_HAVE_DLERROR)
    const char *errmsg = dlerror();
    #else
    const char *errmsg = "unavailable";
    #endif
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error closing dynamic library:\n  Error message from dlclose() %s", errmsg);
  }
  #endif /* !PETSC_HAVE_DLCLOSE */

  /*
     --- unimplemented ---
  */
#else
  SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "Cannot use dynamic libraries on this platform");
#endif

  *handle = NULL;
  PetscFunctionReturn(PETSC_SUCCESS);
}

// clang-format off

/*@C
   PetscDLSym - finds a symbol in a dynamic library

   Not Collective

   Input Parameters:
+   handle - obtained with `PetscDLOpen()` or NULL
-   symbol - name of symbol

   Output Parameter:
.   value - pointer to the function, NULL if not found

   Level: developer

  Note:
   If handle is NULL, the symbol is looked for in the main executable's dynamic symbol table.
   In order to be dynamically loadable, the symbol has to be exported as such.  On many UNIX-like
   systems this requires platform-specific linker flags.

.seealso: `PetscDLClose()`, `PetscDLOpen()`, `PetscDLAddr()`
@*/
PetscErrorCode PetscDLSym(PetscDLHandle handle, const char symbol[], void **value)
{
  PETSC_UNUSED dlhandle_t dlhandle;
  dlsymbol_t              dlsymbol;

  PetscFunctionBegin;
  PetscValidCharPointer(symbol, 2);
  PetscValidPointer(value, 3);

  dlhandle = (dlhandle_t)0;
  dlsymbol = (dlsymbol_t)0;
  *value   = (void *)0;

  /*
     --- GetProcAddress ---
  */
  #if defined(PETSC_HAVE_WINDOWS_H)
    #if defined(PETSC_HAVE_GETPROCADDRESS)
      if (handle) dlhandle = (dlhandle_t)handle;
      else dlhandle = (dlhandle_t)GetCurrentProcess();
      dlsymbol = (dlsymbol_t)GetProcAddress(dlhandle, symbol);
      #if defined(PETSC_HAVE_SETLASTERROR)
        SetLastError((DWORD)0); /* clear any previous error */
      #endif /* PETSC_HAVE_SETLASTERROR */
    #endif /* !PETSC_HAVE_GETPROCADDRESS */

  /*
     --- dlsym ---
  */
  #elif defined(PETSC_HAVE_DLFCN_H) /* PETSC_HAVE_WINDOWS_H */
    #if defined(PETSC_HAVE_DLSYM)
      if (handle) dlhandle = (dlhandle_t)handle;
      else {
        #if defined(PETSC_HAVE_DLOPEN)
          /* Attempt to retrieve the main executable's dlhandle. */
          {
            int dlflags1 = 0, dlflags2 = 0;
            #if defined(PETSC_HAVE_RTLD_LAZY)
              dlflags1 = RTLD_LAZY;
            #endif /* PETSC_HAVE_RTLD_LAZY */
            #if defined(PETSC_HAVE_RTLD_NOW)
              if (!dlflags1) {
                dlflags1 = RTLD_NOW;
              }
            #endif /* PETSC_HAVE_RTLD_NOW */
            #if defined(PETSC_HAVE_RTLD_LOCAL)
              dlflags2 = RTLD_LOCAL;
            #endif /* PETSC_HAVE_RTLD_LOCAL */
            #if defined(PETSC_HAVE_RTLD_GLOBAL)
              if (!dlflags2) {
                dlflags2 = RTLD_GLOBAL;
              }
            #endif /* PETSC_HAVE_RTLD_GLOBAL */
            #if defined(PETSC_HAVE_DLERROR)
              if (!(PETSC_RUNNING_ON_VALGRIND)) { dlerror(); /* clear any previous error; valgrind does not like this */ }
            #endif /* PETSC_HAVE_DLERROR */
            #if defined(PETSC_HAVE_RTLD_DEFAULT)
              dlhandle = RTLD_DEFAULT;
            #else /* PETSC_HAVE_RTLD_DEFAULT */
              /* Attempt to open the main executable as a dynamic library. */
              dlhandle = dlopen(NULL, dlflags1 | dlflags2);
              #if defined(PETSC_HAVE_DLERROR)
                {
                  const char *e = (const char *)dlerror();
                  PetscCheck(!e, PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG, "Error opening main executable as a dynamic library:\n  Error message from dlopen(): '%s'", e);
                }
              #endif /* PETSC_HAVE_DLERROR */
            #endif /* PETSC_HAVE_RTLD_DEFAULT */
          }
        #endif /* PETSC_HAVE_DLOPEN */
      }
      #if defined(PETSC_HAVE_DLERROR)
        dlerror(); /* clear any previous error */
      #endif /* PETSC_HAVE_DLERROR */
      dlsymbol = (dlsymbol_t)dlsym(dlhandle, symbol);
    #else /* PETSC_HAVE_DLSYM */
      SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "Cannot use dynamic libraries on this platform");
    #endif /* PETSC_HAVE_DLSYM */
  #else /* PETSC_HAVE_DLFCN_H */
    SETERRQ(PETSC_COMM_SELF, PETSC_ERR_SUP_SYS, "Cannot use dynamic libraries on this platform");
  #endif /* PETSC_HAVE_WINDOWS_H */
  // clang-format on

  *value = *((void **)&dlsymbol);

#if defined(PETSC_SERIALIZE_FUNCTIONS)
  if (*value) PetscCall(PetscFPTAdd(*value, symbol));
#endif /* PETSC_SERIALIZE_FUNCTIONS */
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
  PetscDLAddr - find the name of a symbol in a dynamic library

  Not Collective

  Input Parameters:
+ handle - obtained with `PetscDLOpen()` or NULL
- func   - pointer to the function, NULL if not found

  Output Parameter:
. name   - name of symbol, or NULL if name lookup is not supported.

  Level: developer

  Notes:
  The caller must free the returned name.

  In order to be dynamically loadable, the symbol has to be exported as such.  On many UNIX-like
  systems this requires platform-specific linker flags.

.seealso: `PetscDLClose()`, `PetscDLSym()`, `PetscDLOpen()`
@*/
PetscErrorCode PetscDLAddr(void (*func)(void), char **name)
{
  PetscFunctionBegin;
  PetscValidPointer(name, 2);
  *name = NULL;
#if defined(PETSC_HAVE_DLADDR) && !(defined(__cray__) && defined(__clang__))
  dlerror(); /* clear any previous error */
  {
    Dl_info info;

    PetscCheck(dladdr(*(void **)&func, &info), PETSC_COMM_SELF, PETSC_ERR_LIB, "Failed to lookup symbol: %s", dlerror());
  #ifdef PETSC_HAVE_CXX
    PetscCall(PetscDemangleSymbol(info.dli_sname, name));
  #else
    PetscCall(PetscStrallocpy(info.dli_sname, name));
  #endif
  }
#endif
  PetscFunctionReturn(PETSC_SUCCESS);
}

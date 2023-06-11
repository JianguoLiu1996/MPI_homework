#if !defined(INCLUDED_PETSCCONF_H)
#define INCLUDED_PETSCCONF_H

#define PETSC_ARCH "arch-linux-c-debug"
#define PETSC_ATTRIBUTEALIGNED(size) __attribute((aligned(size)))
#define PETSC_BLASLAPACK_UNDERSCORE 1
#define PETSC_CLANGUAGE_C 1
#define PETSC_CXX_RESTRICT __restrict
#define PETSC_DEPRECATED_ENUM(why) __attribute__((deprecated(why)))
#define PETSC_DEPRECATED_FUNCTION(why) __attribute__((deprecated(why)))
#define PETSC_DEPRECATED_MACRO(why) _Pragma(why)
#define PETSC_DEPRECATED_TYPEDEF(why) __attribute__((deprecated(why)))
#define PETSC_DIR "/home/jianguoliu/Documents/MPI_homework/homework_PETSc_7/petsc-3.19.1"
#define PETSC_DIR_SEPARATOR '/'
#define PETSC_FORTRAN_CHARLEN_T int
#define PETSC_FORTRAN_TYPE_INITIALIZE  = -2
#define PETSC_FUNCTION_NAME_C __func__
#define PETSC_FUNCTION_NAME_CXX __func__
#define PETSC_HAVE_ACCESS 1
#define PETSC_HAVE_ATOLL 1
#define PETSC_HAVE_ATTRIBUTEALIGNED 1
#define PETSC_HAVE_BUILTIN_EXPECT 1
#define PETSC_HAVE_BZERO 1
#define PETSC_HAVE_C99_COMPLEX 1
#define PETSC_HAVE_CLOCK 1
#define PETSC_HAVE_CXX 1
#define PETSC_HAVE_CXX_ATOMIC 1
#define PETSC_HAVE_CXX_COMPLEX 1
#define PETSC_HAVE_CXX_COMPLEX_FIX 1
#define PETSC_HAVE_CXX_DIALECT_CXX11 1
#define PETSC_HAVE_CXX_DIALECT_CXX14 1
#define PETSC_HAVE_CXX_DIALECT_CXX17 1
#define PETSC_HAVE_DLADDR 1
#define PETSC_HAVE_DLCLOSE 1
#define PETSC_HAVE_DLERROR 1
#define PETSC_HAVE_DLFCN_H 1
#define PETSC_HAVE_DLOPEN 1
#define PETSC_HAVE_DLSYM 1
#define PETSC_HAVE_DOUBLE_ALIGN_MALLOC 1
#define PETSC_HAVE_DRAND48 1
#define PETSC_HAVE_DYNAMIC_LIBRARIES 1
#define PETSC_HAVE_ERF 1
#define PETSC_HAVE_EXECUTABLE_EXPORT 1
#define PETSC_HAVE_FCNTL_H 1
#define PETSC_HAVE_FENV_H 1
#define PETSC_HAVE_FE_VALUES 1
#define PETSC_HAVE_FLOAT_H 1
#define PETSC_HAVE_FORK 1
#define PETSC_HAVE_FORTRAN 1
#define PETSC_HAVE_FORTRAN_FLUSH 1
#define PETSC_HAVE_FORTRAN_FREE_LINE_LENGTH_NONE 1
#define PETSC_HAVE_FORTRAN_GET_COMMAND_ARGUMENT 1
#define PETSC_HAVE_FORTRAN_TYPE_STAR 1
#define PETSC_HAVE_FORTRAN_UNDERSCORE 1
#define PETSC_HAVE_GETCWD 1
#define PETSC_HAVE_GETDOMAINNAME 1
#define PETSC_HAVE_GETHOSTBYNAME 1
#define PETSC_HAVE_GETHOSTNAME 1
#define PETSC_HAVE_GETPAGESIZE 1
#define PETSC_HAVE_GETRUSAGE 1
#define PETSC_HAVE_IMMINTRIN_H 1
#define PETSC_HAVE_INTTYPES_H 1
#define PETSC_HAVE_ISINF 1
#define PETSC_HAVE_ISNAN 1
#define PETSC_HAVE_ISNORMAL 1
#define PETSC_HAVE_LGAMMA 1
#define PETSC_HAVE_LOG2 1
#define PETSC_HAVE_LSEEK 1
#define PETSC_HAVE_MALLOC_H 1
#define PETSC_HAVE_MEMMOVE 1
#define PETSC_HAVE_MKSTEMP 1
#define PETSC_HAVE_MMAP 1
#define PETSC_HAVE_MPIEXEC_ENVIRONMENTAL_VARIABLE OMP
#define PETSC_HAVE_MPIIO 1
#define PETSC_HAVE_MPI_COMBINER_CONTIGUOUS 1
#define PETSC_HAVE_MPI_COMBINER_DUP 1
#define PETSC_HAVE_MPI_COMBINER_NAMED 1
#define PETSC_HAVE_MPI_F90MODULE 1
#define PETSC_HAVE_MPI_F90MODULE_VISIBILITY 1
#define PETSC_HAVE_MPI_FEATURE_DYNAMIC_WINDOW 1
#define PETSC_HAVE_MPI_GET_ACCUMULATE 1
#define PETSC_HAVE_MPI_GET_LIBRARY_VERSION 1
#define PETSC_HAVE_MPI_INIT_THREAD 1
#define PETSC_HAVE_MPI_INT64_T 1
#define PETSC_HAVE_MPI_LONG_DOUBLE 1
#define PETSC_HAVE_MPI_NEIGHBORHOOD_COLLECTIVES 1
#define PETSC_HAVE_MPI_NONBLOCKING_COLLECTIVES 1
#define PETSC_HAVE_MPI_ONE_SIDED 1
#define PETSC_HAVE_MPI_PROCESS_SHARED_MEMORY 1
#define PETSC_HAVE_MPI_REDUCE_LOCAL 1
#define PETSC_HAVE_MPI_REDUCE_SCATTER_BLOCK 1
#define PETSC_HAVE_MPI_RGET 1
#define PETSC_HAVE_MPI_WIN_CREATE 1
#define PETSC_HAVE_NANOSLEEP 1
#define PETSC_HAVE_NETDB_H 1
#define PETSC_HAVE_NETINET_IN_H 1
#define PETSC_HAVE_OMPI_MAJOR_VERSION 2
#define PETSC_HAVE_OMPI_MINOR_VERSION 1
#define PETSC_HAVE_OMPI_RELEASE_VERSION 1
#define PETSC_HAVE_PACKAGES ":blaslapack:mathlib:mpi:pthread:regex:x11:"
#define PETSC_HAVE_POPEN 1
#define PETSC_HAVE_POSIX_MEMALIGN 1
#define PETSC_HAVE_PTHREAD 1
#define PETSC_HAVE_PWD_H 1
#define PETSC_HAVE_RAND 1
#define PETSC_HAVE_READLINK 1
#define PETSC_HAVE_REALPATH 1
#define PETSC_HAVE_REAL___FLOAT128 1
#define PETSC_HAVE_REGEX 1
#define PETSC_HAVE_RTLD_GLOBAL 1
#define PETSC_HAVE_RTLD_LAZY 1
#define PETSC_HAVE_RTLD_LOCAL 1
#define PETSC_HAVE_RTLD_NOW 1
#define PETSC_HAVE_SETJMP_H 1
#define PETSC_HAVE_SLEEP 1
#define PETSC_HAVE_SNPRINTF 1
#define PETSC_HAVE_SOCKET 1
#define PETSC_HAVE_SO_REUSEADDR 1
#define PETSC_HAVE_STDATOMIC_H 1
#define PETSC_HAVE_STDINT_H 1
#define PETSC_HAVE_STRCASECMP 1
#define PETSC_HAVE_STRINGS_H 1
#define PETSC_HAVE_STRUCT_SIGACTION 1
#define PETSC_HAVE_SYS_PARAM_H 1
#define PETSC_HAVE_SYS_PROCFS_H 1
#define PETSC_HAVE_SYS_RESOURCE_H 1
#define PETSC_HAVE_SYS_SOCKET_H 1
#define PETSC_HAVE_SYS_TIMES_H 1
#define PETSC_HAVE_SYS_TIME_H 1
#define PETSC_HAVE_SYS_TYPES_H 1
#define PETSC_HAVE_SYS_UTSNAME_H 1
#define PETSC_HAVE_SYS_WAIT_H 1
#define PETSC_HAVE_TAU_PERFSTUBS 1
#define PETSC_HAVE_TGAMMA 1
#define PETSC_HAVE_TIME 1
#define PETSC_HAVE_TIME_H 1
#define PETSC_HAVE_UNAME 1
#define PETSC_HAVE_UNISTD_H 1
#define PETSC_HAVE_USLEEP 1
#define PETSC_HAVE_VA_COPY 1
#define PETSC_HAVE_VSNPRINTF 1
#define PETSC_HAVE_X 1
#define PETSC_HAVE_XMMINTRIN_H 1
#define PETSC_INTPTR_T intptr_t
#define PETSC_INTPTR_T_FMT "#" PRIxPTR
#define PETSC_IS_COLORING_MAX USHRT_MAX
#define PETSC_IS_COLORING_VALUE_TYPE short
#define PETSC_IS_COLORING_VALUE_TYPE_F integer2
#define PETSC_LEVEL1_DCACHE_LINESIZE 64
#define PETSC_LIB_DIR "/home/jianguoliu/Documents/MPI_homework/homework_PETSc_7/petsc-3.19.1/arch-linux-c-debug/lib"
#define PETSC_MAX_PATH_LEN 4096
#define PETSC_MEMALIGN 16
#define PETSC_MPICC_SHOW "gcc -I/usr/lib/x86_64-linux-gnu/openmpi/include/openmpi -I/usr/lib/x86_64-linux-gnu/openmpi/include/openmpi/opal/mca/event/libevent2022/libevent -I/usr/lib/x86_64-linux-gnu/openmpi/include/openmpi/opal/mca/event/libevent2022/libevent/include -I/usr/lib/x86_64-linux-gnu/openmpi/include -pthread -L/usr//lib -L/usr/lib/x86_64-linux-gnu/openmpi/lib -lmpi"
#define PETSC_MPIU_IS_COLORING_VALUE_TYPE MPI_UNSIGNED_SHORT
#define PETSC_OMAKE "/usr/bin/make --no-print-directory"
#define PETSC_PREFETCH_HINT_NTA _MM_HINT_NTA
#define PETSC_PREFETCH_HINT_T0 _MM_HINT_T0
#define PETSC_PREFETCH_HINT_T1 _MM_HINT_T1
#define PETSC_PREFETCH_HINT_T2 _MM_HINT_T2
#define PETSC_PYTHON_EXE "/usr/bin/python3"
#define PETSC_Prefetch(a,b,c) _mm_prefetch((const char*)(a),(c))
#define PETSC_REPLACE_DIR_SEPARATOR '\\'
#define PETSC_SIGNAL_CAST  
#define PETSC_SIZEOF_INT 4
#define PETSC_SIZEOF_LONG 8
#define PETSC_SIZEOF_LONG_LONG 8
#define PETSC_SIZEOF_SIZE_T 8
#define PETSC_SIZEOF_VOID_P 8
#define PETSC_SLSUFFIX "so"
#define PETSC_UINTPTR_T uintptr_t
#define PETSC_UINTPTR_T_FMT "#" PRIxPTR
#define PETSC_UNUSED __attribute((unused))
#define PETSC_USE_AVX512_KERNELS 1
#define PETSC_USE_BACKWARD_LOOP 1
#define PETSC_USE_CTABLE 1
#define PETSC_USE_DEBUG 1
#define PETSC_USE_DEBUGGER "gdb"
#define PETSC_USE_DMLANDAU_2D 1
#define PETSC_USE_INFO 1
#define PETSC_USE_ISATTY 1
#define PETSC_USE_LOG 1
#define PETSC_USE_PROC_FOR_SIZE 1
#define PETSC_USE_REAL_DOUBLE 1
#define PETSC_USE_SHARED_LIBRARIES 1
#define PETSC_USE_SINGLE_LIBRARY 1
#define PETSC_USE_SOCKET_VIEWER 1
#define PETSC_USE_VISIBILITY_C 1
#define PETSC_USE_VISIBILITY_CXX 1
#define PETSC_USING_64BIT_PTR 1
#define PETSC_USING_F2003 1
#define PETSC_USING_F90FREEFORM 1
#define PETSC__BSD_SOURCE 1
#define PETSC__DEFAULT_SOURCE 1
#define PETSC__GNU_SOURCE 1
#endif

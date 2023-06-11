import config.package
import os

class Configure(config.package.CMakePackage):
  def __init__(self, framework):
    config.package.CMakePackage.__init__(self, framework)
    self.minversion       = '6.3.0'
    self.version          = '8.1.2'
    self.versionname      = 'SUPERLU_DIST_MAJOR_VERSION.SUPERLU_DIST_MINOR_VERSION.SUPERLU_DIST_PATCH_VERSION'
    self.gitcommit        = 'v'+self.version
    self.download         = ['git://https://github.com/xiaoyeli/superlu_dist','https://github.com/xiaoyeli/superlu_dist/archive/'+self.gitcommit+'.tar.gz']
    self.functions        = ['set_default_options_dist']
    self.includes         = ['superlu_ddefs.h']
    self.liblist          = [['libsuperlu_dist.a']]
    # SuperLU_DIST CMake requires working MPI_C_COMPILER (i.e. mpicc) but that is not provided with Microsoft and Intel Windows compilers and Microsoft Windows MPI
    # self.downloadonWindows= 1
    self.hastests         = 1
    self.hastestsdatafiles= 1
    self.precisions       = ['double','single']
    self.buildLanguages   = ['Cxx']
    self.minCmakeVersion  = (3,18,1)
    return

  def setupDependencies(self, framework):
    config.package.CMakePackage.setupDependencies(self, framework)
    self.blasLapack     = framework.require('config.packages.BlasLapack',self)
    self.parmetis       = framework.require('config.packages.parmetis',self)
    self.mpi            = framework.require('config.packages.MPI',self)
    self.cuda           = framework.require('config.packages.cuda',self)
    self.hip            = framework.require('config.packages.hip',self)
    self.openmp         = framework.require('config.packages.openmp',self)
    self.odeps          = [self.parmetis,self.cuda,self.hip,self.openmp]
    self.deps           = [self.mpi,self.blasLapack]
    return

  def formCMakeConfigureArgs(self):
    args = config.package.CMakePackage.formCMakeConfigureArgs(self)
    if self.openmp.found:
      self.usesopenmp = 'yes'
    else:
      args.append('-DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=TRUE')
    if self.cuda.found:
      # SuperLU_DIST C/C++ sources also include CUDA includes so add cuda.include to CFLAGS/CXXFLAGS
      for place,item in enumerate(args):
        if item.find('CMAKE_C_FLAGS') >= 0 or item.find('CMAKE_CXX_FLAGS') >= 0:
          args[place]=item[:-1]+' '+self.headers.toString(self.cuda.include)+' -DDEBUGlevel=0 -DPRNTlevel=0"'
      args.append('-DTPL_ENABLE_CUDALIB=TRUE')
      args.append('-DTPL_CUDA_LIBRARIES="'+self.libraries.toString(self.cuda.dlib)+'"')
      with self.Language('CUDA'):
        # already set in package.py so could be removed, but why are MPI include paths listed here
        args.append('-DCMAKE_CUDA_FLAGS="'+self.getCompilerFlags()+' '+self.mpi.includepaths+' '+self.headers.toString(self.cuda.include)+' -DDEBUGlevel=0 -DPRNTlevel=0"')

    if self.hip.found:
      args.append('-DTPL_ENABLE_HIPLIB=TRUE')
      with self.Language('HIP'):
        for place,item in enumerate(args):
          if item.find('CMAKE_C_FLAGS') >= 0 or item.find('CMAKE_CXX_FLAGS') >= 0:
            args[place]=item[:-1]+' '+' -DDEBUGlevel=0 -DPRNTlevel=0"'
        hipArchFlags = '--amdgpu-target=' + self.hip.hipArch
        args.append('-DHIP_HIPCC_FLAGS="'+hipArchFlags+' '+self.getCompilerFlags()+' '+self.mpi.includepaths+' '+self.headers.toString(self.hip.include)+' -DDEBUGlevel=0 -DPRNTlevel=0"')

    args.append('-DUSE_XSDK_DEFAULTS=YES')
    args.append('-DTPL_BLAS_LIBRARIES="'+self.libraries.toString(self.blasLapack.dlib)+'"')
    args.append('-DTPL_LAPACK_LIBRARIES="'+self.libraries.toString(self.blasLapack.dlib)+'"')
    if self.parmetis.found:
      args.append('-DTPL_PARMETIS_INCLUDE_DIRS="'+';'.join(self.parmetis.dinclude)+'"')
      args.append('-DTPL_PARMETIS_LIBRARIES="'+self.libraries.toString(self.parmetis.dlib)+'"')
    else:
      args.append('-DTPL_ENABLE_PARMETISLIB=FALSE')

    if self.getDefaultIndexSize() == 64:
      args.append('-DXSDK_INDEX_SIZE=64')

    if hasattr(self.compilers, 'FC'):
      args.append('-DXSDK_ENABLE_Fortran=ON')
    else:
      args.append('-DXSDK_ENABLE_Fortran=OFF')

    args.append('-Denable_tests=0')
    args.append('-Denable_examples=0')
    empty = not ('MSYSTEM' in os.environ and 'HAVE_MSMPI' in self.mpi.defines)
    if empty and 'download-superlu_dist-cmake-arguments' in self.argDB and self.argDB['download-superlu_dist-cmake-arguments']:
      empty = (not '-DMPI_GUESS_LIBRARY_NAME=' in self.argDB['download-superlu_dist-cmake-arguments'])
    if empty:
      args.append('-DMPI_C_COMPILE_FLAGS:STRING=""')
      args.append('-DMPI_C_INCLUDE_PATH:STRING=""')
      args.append('-DMPI_C_HEADER_DIR:STRING=""')
      args.append('-DMPI_C_LIBRARIES:STRING=""')
    return args

  def configureLibrary(self):
    config.package.Package.configureLibrary(self)
    self.pushLanguage('C')
    oldFlags = self.compilers.CPPFLAGS # Disgusting save and restore
    self.compilers.CPPFLAGS += ' '+self.headers.toString(self.dinclude)
    if self.defaultIndexSize == 64:
      if not self.checkCompile('#include "superlu_ddefs.h"','#if !defined(_LONGINT)\n#error "No longint"\n#endif\n'):
        raise RuntimeError('PETSc is being configured using --with-64-bit-indices but SuperLU_DIST library is built for 32 bit integers.\n\
Suggest using --download-superlu_dist')
    else:
      if not self.checkCompile('#include "superlu_ddefs.h"','#if defined(_LONGINT)\n#error "longint is defined"\n#endif\n'):
        raise RuntimeError('PETSc is being configured without using --with-64-bit-indices but SuperLU_DIST library is built for 64 bit integers.\n\
Suggest using --download-superlu_dist')
    self.compilers.CPPFLAGS = oldFlags
    self.popLanguage()

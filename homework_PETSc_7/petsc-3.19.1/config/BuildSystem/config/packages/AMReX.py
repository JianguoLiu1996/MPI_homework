import config.package
import os

class Configure(config.package.CMakePackage):
  def __init__(self, framework):
    config.package.CMakePackage.__init__(self, framework)
    self.versionname       = 'AMREX_GIT_VERSION'
    self.gitcommit         = '22.10'
    self.download          = ['git://https://github.com/AMReX-Codes/amrex.git']
    self.includes          = ['AMReX.H']
    self.liblist           = [['libamrex.a']]
    self.versioninclude    = 'AMReX_Config.H'
    self.functions         = ['amrex_fi_init']
    self.hastests          = 1
    self.hastestsdatafiles = 1
    self.precisions        = ['double']
    self.buildLanguages    = ['Cxx']
    self.minCxxVersion     = 'c++14'
    self.builtafterpetsc   = 1
    self.minCmakeVersion   = (3,14,0)
    return

  def setupHelp(self, help):
    import nargs
    config.package.CMakePackage.setupHelp(self, help)

  def setupDependencies(self, framework):
    config.package.CMakePackage.setupDependencies(self, framework)
    self.blasLapack     = framework.require('config.packages.BlasLapack',self)
    self.mpi            = framework.require('config.packages.MPI',self)
    #  requires HYPRE install because AMReX CMake requires HYPRE in path to compile AMReX PETSc code
    #  Src/Extern/PETSc/AMReX_PETSc.cpp:10:10: fatal error: 'AMReX_HypreABec_F.H' file not found
    self.hypre          = framework.require('config.packages.hypre',self)
    self.cuda           = framework.require('config.packages.cuda',self)
    self.hip            = framework.require('config.packages.hip',self)
    self.sycl           = framework.require('config.packages.sycl',self)
    self.openmp         = framework.require('config.packages.openmp',self)
    self.odeps          = [self.mpi,self.blasLapack,self.cuda,self.hip,self.sycl,self.openmp]
    self.deps           = [self.hypre,self.mpi,self.blasLapack]
    return

  def formCMakeConfigureArgs(self):
    args = config.package.CMakePackage.formCMakeConfigureArgs(self)
    args.append('-DAMReX_EB=YES')
    args.append('-DAMReX_LINEAR_SOLVERS=YES')
    args.append('-DAMReX_PARTICLES=YES')
    args.append('-DAMReX_PETSC=YES')
    args.append('-DAMReX_HYPRE=YES')
    if hasattr(self.compilers, 'FC'):
      args.append('-DAMReX_FORTRAN_INTERFACES=YES')

    GPUBackend = ''
    if self.cuda.found:
      GPUBackend = 'CUDA'
      # Prefer cmake options instead of -DAMReX_CUDA_ARCH
      args.append('-D'+self.cuda.cmakeArchProperty())
    elif self.hip.found:
      GPUBackend = 'HIP'
      args.append('-DCMAKE_HIP_ARCHITECTURES="'+self.hip.hipArch+'"')
    elif self.sycl.found:
      GPUBackend = 'SYCL'

    if GPUBackend:
      args.append('-DAMReX_GPU_BACKEND='+GPUBackend)

    if self.argDB['prefix'] and not 'package-prefix-hash' in self.argDB:
      args.append('-DPETSC_DIR='+os.path.abspath(os.path.expanduser(self.argDB['prefix'])))
      args.append('-DPETSC_ARCH=""')
      args.append('-DHYPRE_ROOT='+os.path.abspath(os.path.expanduser(self.argDB['prefix'])))
    else:
      args.append('-DPETSC_DIR='+os.path.join(self.petscdir.dir))
      args.append('-DPETSC_ARCH='+self.arch)
      args.append('-DHYPRE_ROOT='+os.path.join(self.petscdir.dir,self.arch))
    if self.argDB['prefix']:
      args.append('-DHYPRE_ROOT='+os.path.abspath(os.path.expanduser(self.argDB['prefix'])))
    else:
      args.append('-DHYPRE_ROOT='+os.path.join(self.petscdir.dir,self.arch))
    return args

  def Install(self):
    import os

    output,err,ret  = config.package.Package.executeShellCommand('git describe --abbrev=12 --dirty --always --tags', cwd=self.packageDir)
    if not err and not ret:
      self.foundversion = output

    args = self.formCMakeConfigureArgs()
    if self.download and self.argDB['download-'+self.downloadname.lower()+'-cmake-arguments']:
       args.append(self.argDB['download-'+self.downloadname.lower()+'-cmake-arguments'])
    args = ' '.join(args)
    conffile = os.path.join(self.packageDir,self.package+'.petscconf')
    fd = open(conffile, 'w')
    fd.write(args)
    fd.close()

    if not self.installNeeded(conffile):
      self.addMakeRule('amrex-build','')
      self.addMakeRule('amrex-install','')
      return self.installDir
    if not self.cmake.found:
      raise RuntimeError('CMake not found, needed to build '+self.PACKAGE+'. Rerun configure with --download-cmake.')

    # effectively, this is 'make clean'
    folder = os.path.join(self.packageDir, 'petsc-build')
    if os.path.isdir(folder):
      import shutil
      shutil.rmtree(folder)
    os.mkdir(folder)

    if not hasattr(self.framework, 'packages'):
      self.framework.packages = []
    self.framework.packages.append(self)

    # these checks are usually done in configureLibrary
    if self.argDB['prefix'] and not 'package-prefix-hash' in self.argDB:
      self.directory = os.path.abspath(os.path.expanduser(self.argDB['prefix']))
      self.include_a = '-I'+os.path.join(os.path.abspath(os.path.expanduser(self.argDB['prefix'])),'include')
      self.lib_a = [os.path.join(os.path.abspath(os.path.expanduser(self.argDB['prefix'])),'lib',self.liblist[0][0])]
    else:
      self.directory = self.petscdir.dir
      self.include_a = '-I'+os.path.join(self.petscdir.dir,self.arch,'include')
      self.lib_a = [os.path.join(self.petscdir.dir,self.arch,'lib',self.liblist[0][0])]
    self.found_a     = 1
    self.addDefine('HAVE_AMREX', 1)
    self.addMakeMacro('AMREX_LIB',' '.join(map(self.libraries.getLibArgument, self.lib_a)))
    self.addMakeMacro('AMREX_INCLUDE',self.include_a)

    # if installing prefix location then need to set new value for PETSC_DIR/PETSC_ARCH
    if self.argDB['prefix'] and not 'package-prefix-hash' in self.argDB:
       carg = 'PETSC_DIR='+os.path.abspath(os.path.expanduser(self.argDB['prefix']))+' PETSC_ARCH="" '
       prefix = os.path.abspath(os.path.expanduser(self.argDB['prefix']))
    else:
       prefix = os.path.join(self.petscdir.dir,self.arch)
       carg = ''

    self.addDefine('HAVE_AMREX',1)
    self.addMakeMacro('AMREX','yes')
    self.addMakeRule('amrexbuild','', \
                       ['@echo "*** Building amrex ***"',\
                          '@${RM} ${PETSC_DIR}/${PETSC_ARCH}/lib/petsc/conf/amrex.errorflg',\
                          '@cd '+os.path.join(self.packageDir,'petsc-build')+' && \\\n\
           '+carg+' '+self.cmake.cmake+' .. '+args+'  > ${PETSC_DIR}/${PETSC_ARCH}/lib/petsc/conf/amrex.log 2>&1 &&'+\
           self.make.make_jnp+' '+self.makerulename+'  >> ${PETSC_DIR}/${PETSC_ARCH}/lib/petsc/conf/amrex.log 2>&1  || \\\n\
             (echo "**************************ERROR*************************************" && \\\n\
             echo "Error building amrex. Check ${PETSC_DIR}/${PETSC_ARCH}/lib/petsc/conf/amrex.log" && \\\n\
             echo "********************************************************************" && \\\n\
             touch ${PETSC_DIR}/${PETSC_ARCH}/lib/petsc/conf/amrex.errorflg && \\\n\
             exit 1)'])
    self.addMakeRule('amrexinstall','', \
                       ['@echo "*** Installing amrex ***"',\
                          '@(cd '+os.path.join(self.packageDir,'petsc-build')+' && \\\n\
           '+'${OMAKE} install) >> ${PETSC_DIR}/${PETSC_ARCH}/lib/petsc/conf/amrex.log 2>&1 || \\\n\
             (echo "**************************ERROR*************************************" && \\\n\
             echo "Error installing amrex. Check ${PETSC_DIR}/${PETSC_ARCH}/lib/petsc/conf/amrex.log" && \\\n\
             echo "********************************************************************" && \\\n\
             exit 1)'])
    if self.argDB['prefix'] and not 'package-prefix-hash' in self.argDB:
      self.addMakeRule('amrex-build','')
      # the build must be done at install time because PETSc shared libraries must be in final location before building amrex
      self.addMakeRule('amrex-install','amrexbuild amrexinstall')
    else:
      self.addMakeRule('amrex-build','amrexbuild amrexinstall')
      self.addMakeRule('amrex-install','')
    return self.installDir

  def alternateConfigureLibrary(self):
    '''Adds rules for building AMReX to PETSc makefiles'''
    self.addMakeRule('amrex-build','')
    self.addMakeRule('amrex-install','')

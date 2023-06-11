import config.package

class Configure(config.package.Package):
  def __init__(self, framework):
    config.package.Package.__init__(self, framework)
    #disable version checking
    #self.minversion             = '4'
    #self.version                = '4.0.0'
    #self.versionname            = 'MFEM_VERSION_STRING'
    #self.versioninclude         = 'mfem/config.hpp'
    self.gitcommit              = 'v4.4' # tags do not include subminor
    self.download               = ['git://https://github.com/mfem/mfem.git']
    self.linkedbypetsc          = 0
    self.downloadonWindows      = 1
    self.buildLanguages         = ['Cxx']
    self.skippackagewithoptions = 1
    self.builtafterpetsc        = 1
    self.noMPIUni               = 1
    return

  def setupHelp(self, help):
    import nargs
    config.package.Package.setupHelp(self, help)
    help.addArgument('MFEM', '-download-mfem-ghv-cxx=<prog>', nargs.Arg(None, None, 'CXX Front-end compiler to compile get_hypre_version'))
    return

  def setupDependencies(self, framework):
    config.package.Package.setupDependencies(self, framework)
    self.hypre  = framework.require('config.packages.hypre',self)
    self.mpi    = framework.require('config.packages.MPI',self)
    self.metis  = framework.require('config.packages.metis',self)
    self.slepc  = framework.require('config.packages.slepc',self)
    self.ceed   = framework.require('config.packages.libceed',self)
    self.cuda   = framework.require('config.packages.cuda',self)
    self.openmp = framework.require('config.packages.openmp',self)
    self.deps   = [self.mpi,self.hypre,self.metis]
    self.odeps  = [self.slepc,self.ceed,self.cuda,self.openmp]
    return

  def Install(self):
#    return self.installDir
#
#  TODO: In order to use postProcess, we need to fix package.py and add these lines
#  in configureLibrary if builtafterpetsc is true. However, these caused duplicated entries
#  in the petscconf.h macros. Not sure if PETSC_HAVE_XXX will conflict when building XXX after petsc
#+        if not hasattr(self.framework, 'packages'):
#+          self.framework.packages = []
#+        self.framework.packages.append(self)

#  def postProcess(self):
    import os

    buildDir = os.path.join(self.packageDir,'petsc-build')
    configDir = os.path.join(buildDir,'config')
    if not os.path.exists(configDir):
      os.makedirs(configDir)

    if self.framework.argDB['prefix']:
      PETSC_DIR  = os.path.abspath(os.path.expanduser(self.argDB['prefix']))
      PETSC_ARCH = ''
      prefix     = os.path.abspath(os.path.expanduser(self.argDB['prefix']))
    else:
      PETSC_DIR  = self.petscdir.dir
      PETSC_ARCH = self.arch
      prefix     = os.path.join(self.petscdir.dir,self.arch)

    PETSC_OPT = self.headers.toStringNoDupes([os.path.join(PETSC_DIR,'include'),os.path.join(PETSC_DIR,PETSC_ARCH,'include')])

    self.pushLanguage('Cxx')
    cxx = self.getCompiler()
    cxxflags = self.getCompilerFlags()
    cxxflags = cxxflags.replace('-fvisibility=hidden','') # MFEM is currently broken with -fvisibility=hidden
    # MFEM uses the macro MFEM_BUILD_DIR that builds a path by combining the directory plus other stuff but if the
    # directory name contains  "-linux" this is converted by CPP to the value 1 since that is defined in Linux header files
    # unless the -std=C++11 or -std=C++14 flag is used; we want to support MFEM without this flag
    if '-linux' in self.packageDir:
      cxxflags += ' -Dlinux=linux'
    self.popLanguage()
    if 'download-mfem-ghv-cxx' in self.argDB and self.argDB['download-mfem-ghv-cxx']:
      ghv = self.argDB['download-mfem-ghv-cxx']
    else:
      ghv = cxx

    # On CRAY with shared libraries, libmfem.so is linked as
    # $ cc -shared -o libmfem.so ...a bunch of .o files.... ...libraries.... -dynamic
    # The -dynamic at the end makes cc think it is creating an executable
    ldflags = self.setCompilers.LDFLAGS.replace('-dynamic','')

    strip_rpath=''
    if self.cuda.found:
      strip_rpath=' | sed "s/-Wl,-rpath,/-Xlinker=-rpath,/g"'
      if self.openmp.found:
        ldflags = ldflags.replace(self.openmp.ompflag,'')

    makedepend = ''
    with open(os.path.join(configDir,'user.mk'),'w') as g:
      g.write('PREFIX = '+prefix+'\n')
      g.write('MPICXX = '+cxx+'\n')
      g.write('export GHV_CXX = '+ghv+'\n')
      g.write('CXXFLAGS = '+cxxflags+'\n')
      if self.argDB['with-shared-libraries']:
        g.write('SHARED = YES\n')
        g.write('STATIC = NO\n')
      else:
        g.write('SHARED = NO\n')
        g.write('STATIC = YES\n')
      g.write('AR = '+self.setCompilers.AR+'\n')
      g.write('ARFLAGS = '+self.setCompilers.AR_FLAGS+'\n')
      g.write('LDFLAGS = '+ldflags+'\n')
      if self.cuda.found:
        g.write('LDFLAGS := $(addprefix -Xlinker ,$(LDFLAGS))\n')
      g.write('MFEM_USE_MPI = YES\n')
      g.write('MFEM_MPIEXEC = '+self.mpi.getMakeMacro('MPIEXEC')+'\n')
      g.write('MFEM_USE_METIS_5 = YES\n')
      g.write('MFEM_USE_METIS = YES\n')
      g.write('MFEM_USE_PETSC = YES\n')
      g.write('HYPRE_OPT = '+self.headers.toString(self.hypre.include)+'\n')
      g.write('HYPRE_LIB = '+self.libraries.toString(self.hypre.lib)+'\n')
      g.write('METIS_OPT = '+self.headers.toString(self.metis.include)+'\n')
      g.write('METIS_LIB = '+self.libraries.toString(self.metis.lib)+'\n')
      if self.cuda.found:
        g.write('HYPRE_LIB := $(subst -Wl,-Xlinker=,$(HYPRE_LIB))\n')
        g.write('METIS_LIB := $(subst -Wl,-Xlinker=,$(METIS_LIB))\n')
      g.write('PETSC_VARS = '+prefix+'/lib/petsc/conf/petscvariables\n')
      g.write('PETSC_OPT = '+PETSC_OPT+'\n')
      # MFEM's config/defaults.mk overwrites these
      g.write('PETSC_DIR = '+PETSC_DIR+'\n')
      g.write('PETSC_ARCH = '+PETSC_ARCH+'\n')
      # Adding all externals should not be needed when PETSc is a shared library, but it is no harm.
      # When the HYPRE library is built statically, we need to resolve blas symbols
      # It would be nice to have access to the conf variables during postProcess, and access petsclib and other variables, instead of using a shell here
      # but I do not know how to do so
      petscext = '$(shell sed -n "s/PETSC_EXTERNAL_LIB_BASIC = *//p" $(PETSC_VARS)'+strip_rpath+')'

      if self.argDB['with-single-library']:
        petsclib = '-L'+prefix+'/lib -lpetsc'
      else:
        petsclib = '-L'+prefix+'/lib -lpetsctao -lpetscts -lpetscsnes -lpetscksp -lpetscdm -lpetscmat -lpetscvec -lpetscsys'
      if self.argDB['with-shared-libraries']:
        if self.cuda.found:
          petscrpt = '-Xlinker=-rpath,'+prefix+'/lib'
        else:
          petscrpt = '-Wl,-rpath,'+prefix+'/lib'
      else:
        petscrpt = ''
      g.write('PETSC_LIB = '+petscrpt+' '+petsclib+' '+petscext+'\n')
      if self.slepc.found:
        g.write('MFEM_USE_SLEPC = YES\n')
        g.write('SLEPC_OPT = '+PETSC_OPT+'\n')
        g.write('SLEPC_DIR = '+PETSC_DIR+'\n')
        g.write('SLEPC_ARCH = '+PETSC_ARCH+'\n')
        g.write('SLEPC_VARS = '+prefix+'/lib/slepc/conf/slepc_variables\n')
        slepclib = '-L'+prefix+'/lib -lslepc'
        slepcext = ''
        g.write('SLEPC_LIB = '+petscrpt+' '+slepclib+' '+slepcext+' $(PETSC_LIB)\n')
        if self.argDB['prefix']:
          makedepend = 'slepc-install'
        else:
          makedepend = 'slepc-build'
      if self.ceed.found:
        g.write('MFEM_USE_CEED = YES\n')
        g.write('CEED_DIR = '+self.ceed.directory+'\n')
        g.write('CEED_OPT = '+self.headers.toString(self.ceed.include)+'\n')
        g.write('CEED_LIB = '+self.libraries.toString(self.ceed.lib)+'\n')
        if self.cuda.found:
          g.write('CEED_LIB := $(subst -Wl,-Xlinker=,$(CEED_LIB))\n')

      if self.cuda.found:
        self.pushLanguage('CUDA')
        petscNvcc = self.getCompiler()
        cudaFlags = self.getCompilerFlags()
        self.popLanguage()
        g.write('MFEM_USE_CUDA = YES\n')
        g.write('CUDA_CXX = '+petscNvcc+'\n')
        if hasattr(self.cuda, 'cudaArch'):
          g.write(self.cuda.cmakeArchProperty()+'\n')
        g.write('CXXFLAGS := '+cudaFlags+' $(addprefix -Xcompiler ,$(CXXFLAGS))\n')
      g.close()

    self.addDefine('HAVE_MFEM',1)
    self.addMakeMacro('MFEM','yes')
    self.addMakeRule('mfembuild',makedepend, \
                       ['@echo "*** Building MFEM ***"',\
                          '@${RM} ${PETSC_ARCH}/lib/petsc/conf/mfem.errorflg',\
                          '@(cd '+buildDir+' && \\\n\
           ${OMAKE} -f '+self.packageDir+'/makefile config MFEM_DIR='+self.packageDir+' && \\\n\
           ${OMAKE} clean && \\\n\
           '+self.make.make_jnp+') > ${PETSC_ARCH}/lib/petsc/conf/mfem.log 2>&1 || \\\n\
             (echo "**************************ERROR*************************************" && \\\n\
             echo "Error building MFEM. Check ${PETSC_ARCH}/lib/petsc/conf/mfem.log" && \\\n\
             echo "********************************************************************" && \\\n\
             touch ${PETSC_ARCH}/lib/petsc/conf/mfem.errorflg && \\\n\
             exit 1)'])
    self.addMakeRule('mfeminstall','', \
                       ['@echo "*** Installing MFEM ***"',\
                          '@(cd '+buildDir+' && \\\n\
           '+'${OMAKE} install) >> ${PETSC_ARCH}/lib/petsc/conf/mfem.log 2>&1 || \\\n\
             (echo "**************************ERROR*************************************" && \\\n\
             echo "Error installing MFEM. Check ${PETSC_ARCH}/lib/petsc/conf/mfem.log" && \\\n\
             echo "********************************************************************" && \\\n\
             exit 1)'])

    if self.argDB['prefix']:
      self.addMakeRule('mfem-build','')
      self.addMakeRule('mfem-install','mfembuild mfeminstall')
    else:
      self.addMakeRule('mfem-build','mfembuild mfeminstall')
      self.addMakeRule('mfem-install','')

    exampleDir = os.path.join(self.packageDir,'examples')
    self.logClearRemoveDirectory()
    self.logPrintBox('MFEM examples are available at '+exampleDir)
    self.logResetRemoveDirectory()

    return self.installDir

  def alternateConfigureLibrary(self):
    self.addMakeRule('mfem-build','')
    self.addMakeRule('mfem-install','')

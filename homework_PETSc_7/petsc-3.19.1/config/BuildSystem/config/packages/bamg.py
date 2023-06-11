import config.package

class Configure(config.package.Package):
  def __init__(self, framework):
    config.package.Package.__init__(self, framework)
    self.gitcommit              = 'a02ea7456e23e63ae9e90f6d3a9403c91ecd5688' # sync with 3.14.0
    self.download               = ['git://https://gitlab.com/knepley/bamg.git','https://gitlab.com/knepley/bamg/archive/'+self.gitcommit+'.tar.gz']
    self.functions              = []
    self.includes               = []
    self.useddirectly           = 0
    self.linkedbypetsc          = 0
    self.builtafterpetsc        = 1
    return

  def setupDependencies(self, framework):
    config.package.Package.setupDependencies(self, framework)
    self.python          = framework.require('config.packages.python',self)
    self.setCompilers    = framework.require('config.setCompilers',self)
    self.sharedLibraries = framework.require('PETSc.options.sharedLibraries',self)
    self.mathlib         = framework.require('config.packages.mathlib',self)
    self.mpi             = framework.require('config.packages.MPI',self)
    self.blasLapack      = framework.require('config.packages.BlasLapack',self)
    self.slepc           = framework.require('config.packages.slepc',self)
    self.parch           = framework.require('PETSc.options.arch',self)
    self.scalartypes     = framework.require('PETSc.options.scalarTypes',self)
    self.deps            = [self.blasLapack,self.mathlib,self.mpi,self.slepc]

    # Must force --have-petsc4py into SLEPc configure arguments so it does not test PETSc before BAMG is built
    if self.argDB['download-'+self.downloadname.lower()]:
      if 'download-slepc-configure-arguments' in self.argDB:
        if not '--have-petsc4py' in self.argDB['download-slepc-configure-arguments']:
          self.argDB['download-slepc-configure-arguments'] = self.argDB['download-slepc-configure-arguments']+' --have-petsc4py'
      else:
        self.argDB['download-slepc-configure-arguments'] = '--have-petsc4py'
    return

  def Install(self):
    import os
    if self.checkSharedLibrariesEnabled():
      # if installing prefix location then need to set new value for PETSC_DIR/PETSC_ARCH
      if self.argDB['prefix'] and not 'package-prefix-hash' in self.argDB:
         iarch = 'installed-'+self.parch.nativeArch
         if self.scalartypes.scalartype != 'real':
           iarch += '-' + self.scalartypes.scalartype
         carg = 'BAMG_DIR='+self.packageDir+' PETSC_DIR='+os.path.abspath(os.path.expanduser(self.argDB['prefix']))+' PETSC_ARCH="" '
         barg = 'BAMG_DIR='+self.packageDir+' PETSC_DIR='+os.path.abspath(os.path.expanduser(self.argDB['prefix']))+' PETSC_ARCH='+iarch+' '
         prefix = os.path.abspath(os.path.expanduser(self.argDB['prefix']))
      else:
         carg = ' BAMG_DIR='+self.packageDir+' '
         barg = ' BAMG_DIR='+self.packageDir+' SLEPC_DIR='+self.slepc.installDir+' '
         prefix = os.path.join(self.petscdir.dir,self.arch)
      if not hasattr(self.framework, 'packages'):
        self.framework.packages = []
      self.framework.packages.append(self)
      # SLEPc dependency
      slepcbuilddep = 'slepc-install slepc-build'
      oldFlags = self.compilers.CPPFLAGS

      self.addMakeMacro('BAMG','yes')
      self.addMakeRule('bamgbuild',slepcbuilddep, \
                         ['@echo "*** Building PETSc BAMG ***"',\
                            '@${RM} ${PETSC_ARCH}/lib/petsc/conf/bamg.errorflg',\
                            '@(cd '+self.packageDir+' && \\\n\
             '+carg+self.python.pyexe+' ./configure --prefix='+prefix+' --with-clean && \\\n\
               mkdir -p ${PETSC_ARCH}/tests && \\\n\
               touch ${PETSC_ARCH}/tests/testfiles && \\\n\
             '+barg+'${OMAKE} '+barg+') > ${PETSC_ARCH}/lib/petsc/conf/bamg.log 2>&1 || \\\n\
               (echo "**************************ERROR*************************************" && \\\n\
               echo "Error building bamg. Check ${PETSC_ARCH}/lib/petsc/conf/bamg.log" && \\\n\
               echo "********************************************************************" && \\\n\
               touch ${PETSC_ARCH}/lib/petsc/conf/bamg.errorflg && \\\n\
               exit 1)'])
      self.addMakeRule('bamginstall','', \
                         ['@echo "*** Installing PETSc BAMG ***"',\
                            '@(cd '+self.packageDir+' && \\\n\
             '+barg+'${OMAKE} install '+barg+') >> ${PETSC_ARCH}/lib/petsc/conf/bamg.log 2>&1 || \\\n\
               (echo "**************************ERROR*************************************" && \\\n\
               echo "Error installing bamg. Check ${PETSC_ARCH}/lib/petsc/conf/bamg.log" && \\\n\
               echo "********************************************************************" && \\\n\
               exit 1)'])
      if self.argDB['prefix'] and not 'package-prefix-hash' in self.argDB:
        self.addMakeRule('bamg-build','')
        # the build must be done at install time because PETSc shared libraries must be in final location before building ba
        self.addMakeRule('bamg-install','bamgbuild bamginstall')
      else:
        self.addMakeRule('bamg-build','bamgbuild bamginstall')
        self.addMakeRule('bamg-install','')
    else:
      self.addMakeRule('bamg-build','')
      self.addMakeRule('bamg-install','')
      self.logPrintWarning('Skipping BAMG installation, remove --with-shared-libraries=0')
    return self.installDir

  def alternateConfigureLibrary(self):
    self.addMakeRule('bamg-build','')
    self.addMakeRule('bamg-install','')

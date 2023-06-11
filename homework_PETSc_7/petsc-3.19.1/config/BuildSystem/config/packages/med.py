import config.package

class Configure(config.package.CMakePackage):
  def __init__(self, framework):
    config.package.CMakePackage.__init__(self, framework)
    self.gitcommit         = '17bebeb8306e6c1dd380f0a8f0be19f2ed9b04e5' # maint-4.0.0 on May 31, 2021
    self.download          = ['git://https://bitbucket.org/petsc/pkg-med.git','https://bitbucket.org/petsc/pkg-med/get/'+self.gitcommit+'.tar.gz']
    self.downloaddirnames  = ['petsc-pkg-med']
    self.functions         = ['MEDfileOpen']
    self.includes          = ['med.h']
    self.liblist           = [['libmedC.a','libmed.a'],['libmedC.a']]
    self.needsMath         = 1
    self.precisions        = ['double'];
    self.buildLanguages    = ['Cxx']    
    return

  def setupDependencies(self, framework):
    config.package.CMakePackage.setupDependencies(self, framework)
    self.sharedLibraries = framework.require('PETSc.options.sharedLibraries', self)
    self.mpi            = framework.require('config.packages.MPI', self)
    self.hdf5           = framework.require('config.packages.hdf5', self)
    self.mathlib        = framework.require('config.packages.mathlib',self)
    self.deps           = [self.mpi, self.hdf5, self.mathlib]
    return

  def formCMakeConfigureArgs(self):
    args = config.package.CMakePackage.formCMakeConfigureArgs(self)
    args.append('-DHDF5_ROOT_DIR=%s' % self.hdf5.directory)
    if not self.checkSharedLibrariesEnabled():
      args.append('-DMEDFILE_BUILD_STATIC_LIBS=OFF')
      args.append('-DMEDFILE_BUILD_SHARED_LIBS=ON')
    else:
      args.append('-DMEDFILE_BUILD_STATIC_LIBS=ON')
      args.append('-DMEDFILE_BUILD_SHARED_LIBS=OFF')
    args.append('-DMEDFILE_BUILD_TESTS=OFF')
    args.append('-DMEDFILE_INSTALL_DOC=OFF')
    args.append('-DMEDFILE_BUILD_PYTHON=OFF')
    for place,item in enumerate(args):
      if 'CMAKE_C_FLAGS' in item or 'CMAKE_CXX_FLAGS' in item:
        args[place]=item[:-1]+' -DH5_USE_18_API"'

    return args


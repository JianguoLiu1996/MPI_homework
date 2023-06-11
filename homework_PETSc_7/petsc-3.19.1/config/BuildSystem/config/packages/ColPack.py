import config.package

class Configure(config.package.CMakePackage):
  def __init__(self, framework):
    config.package.CMakePackage.__init__(self, framework)
    self.gitcommit       = '043e3cd'
    self.download        = ['git://https://github.com/caidao22/ColPack.git']
    self.includes        = ['ColPack/ColPackHeaders.h']
    self.liblist         = [['libColPack.a']]
    self.functionsCxx    = [1,'void current_time();','current_time()']
    self.buildLanguages  = ['Cxx']
    self.precisions      = ['double']
    self.complex         = 0
    self.cmakelistsdir   = 'build/cmake'
    self.minCmakeVersion = (3,4,0)
    return

  def setupDependencies(self, framework):
    config.package.CMakePackage.setupDependencies(self, framework)
    self.openmp = framework.require('config.packages.openmp',self)
    self.odeps = [self.openmp]
    return

  def formCMakeConfigureArgs(self):
    import os
    args = config.package.CMakePackage.formCMakeConfigureArgs(self)
    # args.append('--enable-examples=no')  #  this option doesn't work to prevent processing examples
    if self.openmp.found:
      args.append('-DENABLE_OPENMP=ON')
    else:
      args.append('-DENABLE_OPENMP=OFF')
    return args

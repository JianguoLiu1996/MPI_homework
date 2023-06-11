import config.package

class Configure(config.package.Package):
  def __init__(self, framework):
    config.package.Package.__init__(self, framework)
    self.versionname       = 'VERSION'
    self.gitcommit         = 'v1.99p2'
    self.download          = ['git://https://bitbucket.org/petsc/pkg-party.git',
                              'https://bitbucket.org/petsc/pkg-party/get/'+self.gitcommit+'.tar.gz']
    self.downloaddirnames  = ['petsc-pkg-party','PARTY']
    self.functions         = ['party_lib']
    self.includes          = ['party_lib.h']
    self.liblist           = [['libparty.a']]
    self.license           = 'http://www2.cs.uni-paderborn.de/cs/robsy/party.html'
    self.requires32bitint  = 1
    return

  def versionToStandardForm(self,ver):
    import re
    return re.compile(r'[=A-Za-z]([\.0-9]*),').search(ver).group(1)

  def Install(self):
    import os

    g = open(os.path.join(self.packageDir,'make.inc'),'w')
    self.pushLanguage('C')
    g.write('CC = '+self.getCompiler()+' '+self.updatePackageCFlags(self.getCompilerFlags())+'\n')
    self.popLanguage()
    g.close()

    if self.installNeeded('make.inc'):
      try:
        self.logPrintBox('Compiling party; this may take several minutes')
        output,err,ret  = config.package.Package.executeShellCommand('cd '+os.path.join(self.packageDir,'src')+' && make clean && make all && cd .. && mkdir -p '+os.path.join(self.installDir,self.libdir)+'&& cp -f *.a '+os.path.join(self.installDir,self.libdir,'')+' && mkdir -p '+os.path.join(self.installDir,self.includedir)+' && cp -f party_lib.h '+os.path.join(self.installDir,self.includedir,''), timeout=2500, log = self.log)
      except RuntimeError as e:
        raise RuntimeError('Error running make on PARTY: '+str(e))
      self.postInstall(output+err,'make.inc')
    return self.installDir

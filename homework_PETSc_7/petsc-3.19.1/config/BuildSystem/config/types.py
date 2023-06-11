import config.base

import os

class Configure(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = ''
    self.substPrefix  = ''
    self.sizes = {}
    self.c99_complex = 0
    self.cxx_complex = 0
    return

  def setupHelp(self, help):
    import nargs
    help.addArgument('Visibility', '-with-visibility=<bool>', nargs.ArgBool(None, 1, 'Use compiler visibility flags to limit symbol visibility'))
    return

  def setupDependencies(self, framework):
    config.base.Configure.setupDependencies(self, framework)
    self.compilers = framework.require('config.compilers', self)
    self.headers   = framework.require('config.headers', self)
    return

  def check(self, typeName, defaultType = None, includes = []):
    '''Checks that "typeName" exists, and if not defines it to "defaultType" if given'''
    self.log.write('Checking for type: '+typeName+'\n')
    include = '''
#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
%s
    ''' % ('\n'.join(['#include<%s>' % inc for inc in includes]))
    found = self.checkCompile(include,typeName+' a;\n(void)a')
    if not found and defaultType:
      self.addTypedef(defaultType, typeName)
    else:
      self.log.write(typeName+' found\n')
    return found

  def check_struct_sigaction(self):
    '''Checks if "struct sigaction" exists in signal.h. This check is for C89 check.'''
    if self.check('struct sigaction', includes = ['signal.h']):
      self.addDefine('HAVE_STRUCT_SIGACTION',1)
    return

  def check__int64(self):
    '''Checks if __int64 exists. This is primarily for windows.'''
    if self.check('__int64'):
      self.addDefine('HAVE___INT64',1)
    return

  def checkSizeTypes(self):
    '''Checks for types associated with sizes, such as size_t.'''
    self.check('size_t', 'int')
    return

  def checkIntegerTypes(self):
    '''Checks for types associated with integers, such as int32_t.'''
    self.check('int32_t', 'int')
    return

  def checkFileTypes(self):
    '''Checks for types associated with files, such as mode_t, off_t, etc.'''
    self.check('mode_t', 'int')
    self.check('off_t', 'int')
    return

  def checkPID(self):
    '''Checks for pid_t, and defines it if necessary'''
    return self.check('pid_t', 'int')

  def checkC99Complex(self):
    '''Check for complex numbers in in C99 std
       Note that since PETSc source code uses _Complex we test specifically for that, not complex'''
    includes = '#include <complex.h>\n'
    body     = 'double _Complex x;\n x = I;\n(void)x'
    if not self.checkCompile(includes, body): return    # checkLink can succeed even if checkCompile fails
    if self.checkLink(includes, body):
      # recheck with _GNU_SOURCE active (Issues with crayCC on aarch64)
      includes = '#define _GNU_SOURCE\n#include <complex.h>\n'
      if not self.checkCompile(includes, body): return
      self.addDefine('HAVE_C99_COMPLEX', 1)
      self.c99_complex = 1
    return

  def checkCxxComplex(self):
    '''Check for complex numbers in namespace std'''
    self.pushLanguage('C++')
    includes = '#include <complex>\n'
    body     = 'std::complex<double> x;\n(void)x'
    if self.checkLink(includes, body):
      self.addDefine('HAVE_CXX_COMPLEX', 1)
      self.cxx_complex = 1
    self.popLanguage()
    return

  def checkSizeof(self, typeName, typeSizes, otherInclude = None, lang='C', save=True, codeBegin=''):
    '''Determines the size of type "typeName", and defines SIZEOF_"typeName" to be the size'''
    self.log.write('Checking for size of type: ' + typeName + '\n')
    typename = typeName.replace(' ', '-').replace('*', 'p')
    includes = '''
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>'''
    mpiFix = '''
#define MPICH_IGNORE_CXX_SEEK
#define MPICH_SKIP_MPICXX 1
#define OMPI_SKIP_MPICXX 1\n'''
    if otherInclude:
      if otherInclude == 'mpi.h':
        includes += mpiFix
      includes += '#include <' + otherInclude + '>\n'
    size = None
    checkName = typeName
    if typeName == 'enum':
      checkName = 'enum{ENUM_DUMMY}'
    with self.Language(lang):
      for s in typeSizes:
        body = 'char assert_sizeof[(sizeof({0})=={1})*2-1];'.format(checkName, s)
        if self.checkCompile(includes, body, codeBegin=codeBegin, codeEnd='\n'):
          size = s
          break
    if size is None:
      raise RuntimeError('Unable to determine size of {0} not found'.format(typeName))
    if save:
      self.sizes[typename] = size
      self.addDefine('SIZEOF_'+typename.replace('-', '_').upper(), str(size))
    return size

  def checkVisibility(self):
    if not self.argDB['with-shared-libraries']:
      self.argDB['with-visibility'] = 0
      self.log.write('Disabled visibility attributes due to static build')
    elif self.argDB['with-visibility']:
      self.pushLanguage('C')
      if self.checkCompile('','__attribute__((visibility ("default"))) int foo(void);'):
        self.addDefine('USE_VISIBILITY_C',1)
      else:
        self.log.write('Cannot use visibility attributes with C')
        self.argDB['with-visibility'] = 0
      self.popLanguage()
      if hasattr(self.compilers, 'CXX'):
        self.pushLanguage('C++')
        if self.checkCompile('','__attribute__((visibility ("default"))) int foo(void);'):
          self.addDefine('USE_VISIBILITY_CXX',1)
        else:
          self.log.write('Cannot use visibility attributes with C++')
        self.popLanguage()
    else:
      self.log.write('User turned off visibility attributes')

  def checkMaxPathLen(self):
    '''Sets PETSC_MAX_PATH_LEN depending on values available on the system'''
    import re
    HASHLINESPACE = ' *(?:\n#.*\n *)*'
    self.log.write('Determining PETSC_MAX_PATH_LEN\n')
    include = ''
    if self.headers.haveHeader('sys/param.h'):
      include = (include + '\n#include <sys/param.h>')
    if self.headers.haveHeader('sys/types.h'):
      include = (include + '\n#include <sys/types.h>')
    length = include + '''
#if defined(MAXPATHLEN)
#  define PETSC_MAX_PATH_LEN MAXPATHLEN
#elif defined(MAX_PATH)
#  define PETSC_MAX_PATH_LEN MAX_PATH
#elif defined(_MAX_PATH)
#  define PETSC_MAX_PATH_LEN _MAX_PATH
#else
#  define PETSC_MAX_PATH_LEN 4096
#endif
#define xstr(s) str(s)
#define str(s) #s
char petsc_max_path_len[] = xstr(PETSC_MAX_PATH_LEN);
'''
    MaxPathLength = 'unknown'
    if self.checkCompile(length):
      buf = self.outputPreprocess(length)
      try:
        MaxPathLength = re.compile('\n'+r'char petsc_max_path_len\s?\[\s?\] = '+HASHLINESPACE+'\"([0-9]+)\"'+HASHLINESPACE+';').search(buf).group(1)
      except:
        raise RuntimeError('Unable to determine PETSC_MAX_PATH_LEN')
    if MaxPathLength == 'unknown' or not MaxPathLength.isdigit():
      raise RuntimeError('Unable to determine PETSC_MAX_PATH_LEN')
    else:
      self.addDefine('MAX_PATH_LEN',MaxPathLength)


  def configure(self):
    self.executeTest(self.check_struct_sigaction)
    self.executeTest(self.check__int64)
    self.executeTest(self.checkSizeTypes)
    self.executeTest(self.checkFileTypes)
    self.executeTest(self.checkIntegerTypes)
    self.executeTest(self.checkPID)
    self.executeTest(self.checkC99Complex)
    if hasattr(self.compilers, 'CXX'):
      self.executeTest(self.checkCxxComplex)
    for t, sizes in {'void *': (8, 4),
                     'int': (4, 8, 2),
                     'long': (8, 4),
                     'long long': (8,),
                     'size_t': (8, 4)}.items():
      self.executeTest(self.checkSizeof, args=[t, sizes])
    if self.sizes['void-p'] == 8:
      self.addDefine('USING_64BIT_PTR',1)
    self.executeTest(self.checkVisibility)
    self.executeTest(self.checkMaxPathLen)
    return

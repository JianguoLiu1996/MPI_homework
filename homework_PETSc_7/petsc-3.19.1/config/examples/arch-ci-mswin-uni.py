#!/usr/bin/env python3

import os
petsc_hash_pkgs=os.path.join(os.getenv('HOME'),'petsc-hash-pkgs')

if __name__ == '__main__':
  import sys
  import os
  sys.path.insert(0, os.path.abspath('config'))
  import configure
  configure_options = [
    '--package-prefix-hash='+petsc_hash_pkgs,
    '--download-f2cblaslapack',
    '--with-cc=cl',
    '--with-shared-libraries=1',
    '--with-cxx=0',
    '--with-fc=0',
    '--with-mpi=0',
    '--with-strict-petscerrorcode',
  ]
  configure.petsc_configure(configure_options)


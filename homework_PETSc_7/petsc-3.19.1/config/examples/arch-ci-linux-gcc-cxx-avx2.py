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
    '--with-debugging=0',
    '--with-clanguage=cxx',
    'COPTFLAGS=-mavx2 -g -O',
    'CXXOPTFLAGS=-mavx2 -g -O',
    'FOPTFLAGS=-mavx2 -g -O',
    '--with-mpi-dir=/nfs/gce/projects/petsc/soft/u22.04/gcc-avx2/mpich-4.0.2',
    '--with-blaslapack-dir=/nfs/gce/projects/petsc/soft/u22.04/gcc-avx2/fblaslapack-3.4.2-p3',
    '--with-memalign=64',
    '--download-metis=1',
    '--download-parmetis=1',
    '--download-superlu_dist=1',
    '--with-strict-petscerrorcode',
    '--with-coverage',
  ]
  configure.petsc_configure(configure_options)

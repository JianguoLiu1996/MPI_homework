#!/usr/bin/env python3

import os
petsc_hash_pkgs=os.path.join(os.getenv('HOME'),'petsc-hash-pkgs')

configure_options = [
  '--package-prefix-hash='+petsc_hash_pkgs,
  '--with-make-test-np=18', # for petsc-gpu-01 - attempt to run 2 simultaneous valgrind jobs
  'COPTFLAGS=-g -O',
  'FOPTFLAGS=-g -O',
  'CXXOPTFLAGS=-g -O',
  #'--download-mpich=1',
  #'--with-hwloc=0', # make sure mpich is built without hwloc - as it doesn't work with valgrind
  '--download-fblaslapack=1',
  '--download-hypre=1',
  '--download-metis=1',
  '--download-parmetis=1',
  '--download-ptscotch=1',
  '--download-suitesparse=1',
  '--download-triangle=1',
  '--download-superlu=1',
  '--download-superlu_dist=1',
  '--download-scalapack=1',
  '--download-strumpack=1',
  '--download-mumps=1',
  # '--download-elemental=1', # disabled since its maxCxxVersion is c++14, but Kokkos-4.0's minCxxVersion is c++17
  #'--download-spai=1', valgrind leaks here will probably not get fixed in the near future
  '--download-parms=1',
  #'--download-moab=1',
  '--download-chaco=1',
  '--download-revolve=1',
  '--download-cams=1',
  '--download-codipack=1',
  '--download-adblaslapack=1',
  '--download-p4est=1',
  '--download-zlib=1',
  '--download-h2opus=1',
  '--download-thrust=1',
  '--download-kokkos=1',
  '--download-kokkos-kernels=1',
  '--with-strict-petscerrorcode',
  '--with-coverage',
  ]

if __name__ == '__main__':
  import sys,os
  sys.path.insert(0,os.path.abspath('config'))
  import configure
  configure.petsc_configure(configure_options)

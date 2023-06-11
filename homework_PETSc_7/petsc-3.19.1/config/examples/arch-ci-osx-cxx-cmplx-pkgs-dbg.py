#!/usr/bin/env python3

import os
petsc_hash_pkgs=os.path.join(os.getenv('HOME'),'petsc-hash-pkgs')

configure_options = [
  '--package-prefix-hash='+petsc_hash_pkgs,
  #'--with-cc=clang',
  #'--with-cxx=clang++',
  #'--with-fc=gfortran', # https://brew.sh/
  'COPTFLAGS=-g -Og',
  'FOPTFLAGS=-g -Og',
  'CXXOPTFLAGS=-g -Og',

  'CXXFLAGS=-Wall -Wwrite-strings -Wno-strict-aliasing -Wno-unknown-pragmas -fstack-protector -fvisibility=hidden -Wno-deprecated',
  '--with-clanguage=cxx',
  '--with-scalar-type=complex',

  #'-download-fblaslapack=1',
  #'--download-mpich=1',
  '--download-metis=1',
  '--download-parmetis=1',
  '--download-ptscotch=1',
  '--download-bison=1',
  '--download-triangle=1',
  '--download-fftw=1',
  '--download-superlu=1',
  '--download-superlu_dist=1',
  '--download-scalapack=1',
  '--download-mumps=1',
  '--download-exodusii=1',
  '--download-netcdf=1',
  '--download-pnetcdf',
  '--download-hdf5',
  '--download-ssl=1',
  '--download-mpfr=1',
  '--download-gmp=1',
  '--download-eigen=1',
  '--download-grid=1',
  '--download-zlib=1',
  '--with-petsc4py=1',
  '--download-mpi4py=1',
  '--download-elemental=1',
  '--with-mpi-f90module-visibility=0',
  #'--download-sundials2=1',
  #'--download-hypre=1',
  #'--download-suitesparse=1',
  #'--download-chaco=1',
  #'--download-spai=1',
  #'--with-coverage',
  '--with-strict-petscerrorcode',
  ]

if __name__ == '__main__':
  import sys,os
  sys.path.insert(0,os.path.abspath('config'))
  import configure
  configure.petsc_configure(configure_options)

#!/usr/bin/env python3

import os
petsc_hash_pkgs=os.path.join(os.getenv('HOME'),'petsc-hash-pkgs')

configure_options = [
  '--package-prefix-hash='+petsc_hash_pkgs,
  #'--with-cc=clang',
  #'--with-cxx=clang++',
  #'--with-fc=gfortran', # https://brew.sh/

  'CXXFLAGS=-Wall -Wwrite-strings -Wno-strict-aliasing -Wno-unknown-pragmas -fstack-protector -Wno-deprecated -fno-stack-check',
  '--with-clanguage=cxx',
  '--with-debugging=0',
  '--debugLevel=4',

  #'--prefix=petsc-install', temporarily disable for gitlab-ci

  #'-download-fblaslapack=1',
  #'--download-mpich=1',
  #'--download-mpich-device=ch3:sock',
  #'--download-mpich-configure-arguments=--disable-two-level-namespace', # workaround for AMREX build failure with MPICH 4.0.1 and above
  '--download-metis=1',
  '--download-parmetis=1',
  '--download-bison=1',
  '--download-ptscotch=1',
  '--download-triangle=1',
  '--download-superlu=1',
  '--download-superlu_dist=1',
  '--download-scalapack=1',
  '--download-mumps=1',
  '--download-parms=1',
  '--download-hdf5=1',
  '--download-med=1',
  '--download-sundials2=1',
  '--download-hypre=1',
  '--download-amrex=1',
  '--download-cmake=1',
  '--download-suitesparse=1',
  '--download-chaco=1',
  '--download-spai=1',
  # '--download-moab=1', # disabled since its maxCxxVersion is c++14, but Kokkos-4.0's minCxxVersion is c++17
  #'--download-saws', #needs /usr/bin/python [missing in newer MacOS]
  '--download-revolve=1',
  '--download-cams=1',
  '--download-ctetgen=1',
  '--download-tetgen=1',
  '--download-mfem=1',
  '--download-glvis=1',
  '--download-hpddm=1',
  '--with-opengl=1',
  '--download-adolc=1',
  '--download-colpack=1',
  '--download-mmg=1',
  '--download-parmmg=1',
  '--download-kokkos=1',
  '--download-kokkos-kernels=1',
  '--download-htool=1',
  '--with-strict-petscerrorcode',
  ]

if __name__ == '__main__':
  import sys,os
  sys.path.insert(0,os.path.abspath('config'))
  import configure
  configure.petsc_configure(configure_options)

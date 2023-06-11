#!/usr/bin/env python3

import os

configure_options = [
  '--package-prefix-hash=/Volumes/Scratch/svcpetsc/petsc-hash-pkgs',
  '--with-mpi-dir=/Volumes/Scratch/svcpetsc/soft/mpich-4.0.1',
  '--with-64-bit-indices=1',
  '--with-clanguage=cxx',
  'CXXFLAGS=-Wall -Wwrite-strings -Wno-strict-aliasing -Wno-unknown-pragmas -fstack-protector -fno-stack-check -Wno-deprecated -fvisibility=hidden',
  'COPTFLAGS=-g -O',
  'FOPTFLAGS=-g -O',
  'CXXOPTFLAGS=-g -O',
  '--with-petsc4py=1',
  '--download-mpi4py=1',
  '--download-make=1',
  '--download-slepc=1',
  '--download-f2cblaslapack=1',
  '--with-f2cblaslapack-fp16-bindings=1',
  #'--with-coverage',
  '--with-strict-petscerrorcode',
  ]

if __name__ == '__main__':
  import sys,os
  sys.path.insert(0,os.path.abspath('config'))
  import configure
  configure.petsc_configure(configure_options)

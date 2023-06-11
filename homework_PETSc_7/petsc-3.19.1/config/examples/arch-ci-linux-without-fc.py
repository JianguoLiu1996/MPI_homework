#!/usr/bin/env python3

import os
petsc_hash_pkgs=os.path.join(os.getenv('HOME'),'petsc-hash-pkgs')

configure_options = [
  '--package-prefix-hash='+petsc_hash_pkgs,
  '--with-fc=0',
  'COPTFLAGS=-g -O',
  #'FOPTFLAGS=-g -O',
  'CXXOPTFLAGS=-g -O',
  '--download-boost=1',
  '--download-f2cblaslapack=1',
  '--with-f2cblaslapack-float128-bindings=1',
  '--download-hpddm=1',
  '--with-shared-libraries=1',
  '--with-strict-petscerrorcode',
  '--with-coverage',
  ]

if __name__ == '__main__':
  import sys,os
  sys.path.insert(0,os.path.abspath('config'))
  import configure
  configure.petsc_configure(configure_options)

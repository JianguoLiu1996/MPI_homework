#!/usr/bin/python3
if __name__ == '__main__':
  import sys
  import os
  sys.path.insert(0, os.path.abspath('config'))
  import configure
  configure_options = [
    'PETSC_ARCH=test',
  ]
  configure.petsc_configure(configure_options)

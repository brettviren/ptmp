#/usr/bin/env python3
'''
pip install -e .
'''

from setuptools import setup
#from distutils.core import setup


setup(name = 'ptmp',
      version = '0.0.2',
      description = 'Prototype Trigger Message Passing',
      author = 'Brett Viren',
      author_email = 'brett.viren@gmail.com',
      license = 'GPLv2',
      url = 'http://github.com/brettviren/ptmp',
      package_dir = {'':'python'},
      #packages = ['ptmp', 'ptmp.data', 'ptmp.ptmp_pb2'],
      py_modules = ['ptmp', 'ptmp.main', 'ptmp.spy', 'ptmp.data', 'ptmp.ptmp_pb2', 'ptmp.helpers', 'ptmp.zgraph'],
      install_requires = [l for l in open("requirements.txt").readlines() if l.strip()],
      # # implicitly depends on ROOT
      entry_points = {
          'console_scripts': [
              'ptmpy = ptmp.main:main',
              'ptmp-spy = ptmp.spy:main',
          ]
      }
              
  )


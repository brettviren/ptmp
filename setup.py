#from setuptools import setup
from distutils.core import setup


setup(name = 'ptmp',
      version = '0.0.2',        # fixme, get it from git
      description = 'Prototype Trigger Message Passing',
      author = 'Brett Viren',
      author_email = 'brett.viren@gmail.com',
      license = 'GPLv2',
      url = 'http://github.com/brettviren/ptmp',
      package_dir = {'':'python'},
      #packages = ['ptmp', 'ptmp.data', 'ptmp.ptmp_pb2'],
      py_modules = ['ptmp', 'ptmp.data', 'ptmp.ptmp_pb2'],
      # These are just what were developed against.  Older versions may be okay.
      install_requires=[
          'protobuf'
      ],
      # # implicitly depends on ROOT
      # entry_points = {
      #     'console_scripts': [
      #         ]
      # }
              
  )


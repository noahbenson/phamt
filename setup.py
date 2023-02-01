#! /usr/bin/env python
################################################################################

import os
from setuptools import (setup, Extension)
from setuptools.command.build_ext import build_ext
from sysconfig import get_config_var

# Depending on the C compiler, we have different options.
class BuildExt(build_ext):
    compile_flags = {"msvc": ["/EHsc", "/std:c11", "/GL"],
                     "unix": ["-std=c11", "-O3"]}
    def build_extensions(self):
        opts = self.compile_flags.get(self.compiler.compiler_type, [])
        for ext in self.extensions:
            ext.extra_compile_args = opts
        build_ext.build_extensions(self)

# Get the version from __init__.py
with open(os.path.join('phamt', '__init__.py'), 'rt') as fl:
    lns = fl.readlines()
version = next(ln for ln in lns if "__version__ = " in ln)
version = version.split('"')[1]

setup(
    name='phamt',
    version=version,
    description='C implementation of a Persistent Hash Array Mapped Trie',
    keywords='persistent immutable functional', 
    author='Noah C. Benson',
    author_email='nben@uw.edu',
    url='https://github.com/noahbenson/phamt/',
    download_url='https://github.com/noahbenson/phamt/',
    license='MIT',
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX',
        'Operating System :: Unix',
        'Operating System :: MacOS',
        'Programming Language :: C',
        'Programming Language :: Python :: Implementation :: CPython',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Topic :: Software Development',
        'Topic :: Software Development :: Libraries',
        'Topic :: Software Development :: Libraries :: Python Modules'],
    packages=['phamt', 'phamt.test'],
    ext_modules=[
        Extension('phamt.c_core',
                  ['phamt/phamt.c'],
                  depends=['phamt/phamt.h'],
                  include_dirs=["phamt"],
                  language="c")],
    package_data={'': ['LICENSE.txt']},
    zip_safe=False,
    include_package_data=True,
    install_requires=[],
    cmdclass={'build_ext': BuildExt})

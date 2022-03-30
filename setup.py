#! /usr/bin/env python
####################################################################################################

from setuptools import (setup, Extension)
from setuptools.command.build_ext import build_ext
from sysconfig import get_config_var

# Depending on the C compiler, we have different options.
class BuildExt(build_ext):
    compile_flags = {"msvc": ["/EHsc", "/std:c99", "/GL"],
                     "unix": ["-std=c99", "-O3"]}
    def build_extensions(self):
        opts = self.compile_flags.get(self.compiler.compiler_type, [])
        for ext in self.extensions:
            ext.extra_compile_args = opts
        build_ext.build_extensions(self)


setup(
    name='phamt',
    version='0.0.1',
    description='C implementation of a Persistent Hash Array Mapped Trie',
    keywords='persistent immutable functional',
    author='Noah C. Benson',
    author_email='nben@uw.edu',
    url='https://github.com/noahbenson/phamt/',
    license='MIT',
    packages=['phamt', 'phamt.test'],
    ext_modules=[
        Extension('phamt.core',
                  ['phamt/phamt.c'],
                  depends=['phamt/phamt.h'],
                  include_dirs=["phamt"],
                  language="c")],
    package_data={'': ['LICENSE.txt']},
    include_package_data=True,
    install_requires=[],
    cmdclass={'build_ext': BuildExt})

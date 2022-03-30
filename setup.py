#! /usr/bin/env python
####################################################################################################

from setuptools import (setup, Extension)

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
        Extension('phamt.c_core',
                  ['phamt/phamt.c'],
                  depends=['phamt/phamt.h'],
                  include_dirs=["phamt"],
                  extra_compile_args=["-O3"])],
    package_data={'': ['LICENSE.txt']},
    include_package_data=True,
    install_requires=[])

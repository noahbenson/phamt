#! /usr/bin/env python
####################################################################################################

from setuptools import (setup, Extension)
from sysconfig import get_config_var

# Depending on the C compiler, we have different options.
cc = get_config_var('CC')
if cc == 'cl.exe' or cc.endswith('/cl.exe') or cc.endswith('\\cl.exe'):
    cc_opts = ["/GL", "/std:c99"]
else:
    cc_opts = ["-O3", "-std=c99"]

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
                  language="c",
                  extra_compile_args=cc_opts)],
    package_data={'': ['LICENSE.txt']},
    include_package_data=True,
    install_requires=[])

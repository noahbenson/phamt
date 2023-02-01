# -*- coding: utf-8 -*-
################################################################################
# phamt/__init__.py
# Initialization file for the PHAMT library.
# By Noah C. Benson

"""Persistent and Transient Hash Array Mapped Trie data structures for Python.
"""

try:              from .c_core  import (PHAMT, THAMT)
except Exception: from .py_core import (PHAMT, THAMT)

__version__ = "0.1.5"


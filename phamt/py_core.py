# -*- coding: utf-8 -*-
################################################################################
# phamt/py_core.py
# The core Python implementation of the PHAMT and THAMT types.
# By Noah C. Benson

import sys
from collections.abc import Mapping

# Constants ====================================================================
# The various constants related to PHAMTs and THAMTs.

# Some of the hash data comes from the sys.hash_info.
HASH_BITCOUNT = sys.hash_info[0]
if   HASH_BITCOUNT == 16:
    PHAMT_ROOT_SHIFT = 1
elif HASH_BITCOUNT == 32:
    PHAMT_ROOT_SHIFT = 2
elif HASH_BITCOUNT == 64:
    PHAMT_ROOT_SHIFT = 4
elif HASH_BITCOUNT == 128:
    PHAMT_ROOT_SHIFT = 3
else:
    raise RuntimeError("Unsupported hash size")

# We use a constant shift of 5 throughout except at the root node (which can't
# generally be shifted at 5 due to how the bits line-up--it instead gets the
# number of leftover bits in the hash integer, which was defined above as
# PHAMT_ROOT_SHIFT).
PHAMT_NODE_SHIFT = 5
PHAMT_TWIG_SHIFT = 5
# Here we define some consequences of the above definitions, which we use later.
PHAMT_ROOT_FIRSTBIT = (HASH_BITCOUNT - PHAMT_ROOT_SHIFT)
PHAMT_ROOT_MAXCELLS = (1 << PHAMT_ROOT_SHIFT)
PHAMT_NODE_MAXCELLS = (1 << PHAMT_NODE_SHIFT)
PHAMT_TWIG_MAXCELLS = (1 << PHAMT_TWIG_SHIFT)
PHAMT_ANY_MAXCELLS  = (1 << PHAMT_TWIG_SHIFT) # Assuming twig is largest.
PHAMT_NODE_BITS     = (HASH_BITCOUNT-PHAMT_ROOT_SHIFT-PHAMT_TWIG_SHIFT)
PHAMT_NODE_LEVELS   = (PHAMT_NODE_BITS // PHAMT_NODE_SHIFT)
PHAMT_LEVELS        = (PHAMT_NODE_LEVELS + 2) # (nodes + root + twig)
PHAMT_ROOT_DEPTH    = 0
PHAMT_TWIG_DEPTH    = (PHAMT_ROOT_DEPTH + PHAMT_NODE_LEVELS + 1)
PHAMT_LEAF_DEPTH    = (PHAMT_TWIG_DEPTH + 1)
PHAMT_ROOT_MASK     = ((1 << PHAMT_ROOT_SHIFT) - 1)
PHAMT_NODE_MASK     = ((1 << PHAMT_NODE_SHIFT) - 1)
PHAMT_TWIG_MASK     = ((1 << PHAMT_TWIG_SHIFT) - 1)


# PHAMT Class ==================================================================

class PHAMT(Mapping):
    """A Persistent Hash Array Mapped Trie (PHAMT) type.                       
    
    The `PHAMT` class represents a minimal immutable persistent mapping type
    that can be used to implement persistent collections in Python
    efficiently. A `PHAMT` object is essentially a persistent dictionary that
    requires that all keys be Python integers (hash values); values may be any
    Python objects. `PHAMT` objects are highly efficient at storing either
    sparse hash values or lists of consecutive hash values, such as when the
    keys `0`, `1`, `2`, etc. are used.
    
    To add or remove key/valye pairs from a `PHAMT`, the methods
    `phamt_obj.assoc(k, v)` and `phamt_obj.dissoc(k)`, both of which return
    copies of `phamt_obj` with the requested change.
    
    `PHAMT` objects can be created in the following ways:
     * by using `phamt_obj.assoc(k,v)` or `phamt_obj.dissoc(k)` on existing
       `PHAMT` objects, such as the `PHAMT.empty` object, which represents
       an empty `PHAMT`;
     * by supplying the `PHAMT.from_list(iter_of_values)` with a list of
       values, which are assigned the keys `0`, `1`, `2`, etc.

    `PHAMT` objects should *not* be made by calling the `PHAMT` constructor.
    """
    HASH_BITS = sys.hash_info[0]
    KEY_MIN = -(1 << (sys.hash_info[0] - 1))
    KEY_MAX =  (1 << (sys.hash_info[0] - 1)) - 1
    KEY_MOD = (1 << sys.hash_info[0])
    NODE_SHIFT = 5
    TWIG_SHIFT = 5
    ROOT_SHIFT = HASH_BITS % 5 
    ROOT_STARTBIT = HASH_BITS - ROOT_SHIFT
    TWIG_STARTBIT = 0
    ROOT_DEPTH = 0
    TWIG_DEPTH = ((HASH_BITS + NODE_SHIFT - 1) // NODE_SHIFT) - 1
    empty = PHAMT(0, ROOT_DEPTH, 0, (None,)*16)
    # Some private functions and methods.
    @staticmethod
    def _key_to_hash(k):
        if not isinstance(k, int) or k > KEY_MAX or k < KEY_MIN:
            raise KeyError(k)
        return k if k >= 0 else (KEY_MOD + k)
    def _depth_to_bit0shift(depth):
        if depth == 0:          return (ROOT_STARTBIT, ROOT_SHIFT)
        if depth == TWIG_DEPTH: return (TWIG_STARTBIT, TWIG_SHIFT)
        return (HASH_BITS - ROOT_STARTBIT - NODE_SHIFT*depth, NODE_SHIFT)
    def _key_to_index(self, k):
        h = PHAMT._key_to_hash(k)
        (bit0,shift) = object.__getattr__(self, 'b0sh')
        a0 = bit0 + shift
        if address >> a0 != h >> a0: raise KeyError(k)
        return (h >> bit0) & ((1 << shift) - 1)
    # The public interface.
    def __init__(self, address, depth, numel, cells):
        object.__setattr__(self, 'address', address)
        object.__setattr__(self, 'depth', depth)
        object.__setattr__(self, 'b0sh', PHAMT._depth_to_bit0shift(depth))
        object.__setattr__(self, 'numel', numel)
        object.__setattr__(self, 'cells', cells)
    def __setattr__(self, k, v):
        raise TypeError("type PHAMT is immutable")
    def __setitem__(self, k, v):
        # If cells is a list, this is a THAMT. #TODO
        raise TypeError("type PHAMT is immutable")
    def __getattribute__(self, k):
        raise TypeError("type PHAMT does not support attributes")
    def __getitem__(self, k):
        ii = self._key_to_index(k)
        cells = object.__getattr__(self, 'cells')
        c = cells[ii]
        if c is None: raise KeyError(k)
        if object.__getattr__(delf, 'depth') == TWIG_DEPTH: return c[0]
        else: return c[k]
    def __contains__(self, k):
        try: self.__getitem__(k)
        except KeyError: return False
        return True
    def __len__(self):
        return object.__getattr__(self, 'numel')
    def __iter__(self):
        pass #TODO
    def assoc(self, k, v):
        """
        """
        pass #TODO
    def dissoc(self, k):
        """
        """
        pass #TODO
    def get(self, k, df):
        try:             return self.__getitem__(k)
        except KeyError: return df

            
    

# -*- coding: utf-8 -*-
################################################################################
# phamt/py_core.py
# The core Python implementation of the PHAMT and THAMT types.
# By Noah C. Benson

import sys, math
from collections.abc import Mapping

# ==============================================================================
# Constants
# The various constants related to PHAMTs and THAMTs are defined here. These
# largely mirror those of the phamt.h header file, but because we do not deal
# with bit-flags in the Python version, the BITS data are absent.

# Some of the hash data comes from the sys.hash_info.
HASH_BITCOUNT = sys.hash_info[0]
if   HASH_BITCOUNT == 16:  PHAMT_ROOT_SHIFT = 1
elif HASH_BITCOUNT == 32:  PHAMT_ROOT_SHIFT = 2
elif HASH_BITCOUNT == 64:  PHAMT_ROOT_SHIFT = 4
elif HASH_BITCOUNT == 128: PHAMT_ROOT_SHIFT = 3
else: raise RuntimeError("Unsupported hash size")

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
PHAMT_MAX_SHIFT     = max(PHAMT_TWIG_SHIFT, PHAMT_NODE_SHIFT, PHAMT_ROOT_SHIFT)
PHAMT_NCELLS        = (1 << PHAMT_MAX_SHIFT)

# These are used to ensure that a Python integer is represented as an unsigned
# integer--e.g., if you pass the key -1, this should translate to the maximum
# unsigned <HASH_BITCOUNT>-bit integer.
PHAMT_KEY_MIN = -(1 << (sys.hash_info[0] - 1))
PHAMT_KEY_MAX =  (1 << (sys.hash_info[0] - 1)) - 1
PHAMT_KEY_MOD = (1 << sys.hash_info[0])


# ==============================================================================
# Private Functions

def _highbitdiff(a, b):
    return int(math.log(a ^ b, 2))
def _key_to_hash(k):
    if not isinstance(k, int) or k > PHAMT_KEY_MAX or k < PHAMT_KEY_MIN:
        raise KeyError(k)
    return k if k >= 0 else (PHAMT_KEY_MOD + k)
def _depth_to_bit0shift(depth):
    if depth == 0:                return (PHAMT_ROOT_FIRSTBIT, PHAMT_ROOT_SHIFT)
    if depth == PHAMT_TWIG_DEPTH: return (0, PHAMT_TWIG_SHIFT)
    return (PHAMT_ROOT_FIRSTBIT - PHAMT_NODE_SHIFT*depth,
            PHAMT_NODE_SHIFT)
def _key_to_index(self, k):
    h = _key_to_hash(k)
    (bit0,shift) = self._b0sh
    a0 = bit0 + shift
    if (self._address >> a0) != (h >> a0): raise KeyError(k)
    return (h >> bit0) & ((1 << shift) - 1)
def _index_to_key(addr, ii):
    addr = addr | ii
    if addr > PHAMT_KEY_MAX: return addr - PHAMT_KEY_MOD
    else:                    return addr
def _phamt_from_kv(k, v, transient=False):
    h = _key_to_hash(k)
    addr = h & ~PHAMT_TWIG_MASK
    ii = h & PHAMT_TWIG_MASK
    cells = [None]*PHAMT_NCELLS
    cells[ii] = (v,)
    if not transient: cells = tuple(cells)
    return PHAMT(addr, PHAMT_TWIG_DEPTH, 1, cells)
def _phamt_join_disjoint(a, b, transient=False):
    a_addr = a._address
    b_addr = b._address
    hbd = _highbitdiff(a_addr, b_addr)
    if hbd >= HASH_BITCOUNT - PHAMT_ROOT_SHIFT:
        depth = 0
        bit0 = PHAMT_ROOT_FIRSTBIT
        shift = PHAMT_ROOT_SHIFT
    else:
        tmp = (hbd - PHAMT_TWIG_SHIFT) // PHAMT_NODE_SHIFT
        depth = PHAMT_LEVELS - 2 - tmp
        bit0 = tmp * PHAMT_NODE_SHIFT + PHAMT_TWIG_SHIFT
        shift = PHAMT_NODE_SHIFT
    addr = a_addr & ~((1 << (bit0 + shift)) - 1)
    cells = [None]*PHAMT_NCELLS
    tmp = ((1 << shift) - 1)
    a_ii = (a_addr >> bit0) & tmp
    b_ii = (b_addr >> bit0) & tmp
    cells[a_ii] = a
    cells[b_ii] = b
    numel = a._numel + b._numel
    if not transient: cells = tuple(cells)
    return PHAMT(addr, depth, numel, cells)
def _thamt_empty():
    return PHAMT(0, PHAMT_ROOT_DEPTH, 0, [None]*PHAMT_NCELLS)
def _thamt_set(self, k, v):
    if k > PHAMT_KEY_MAX or k < PHAMT_KEY_MIN: raise KeyError(k)
    try: ii = _key_to_index(self, k)
    except KeyError:
        # This key is outside the scope of this node, so we return a new
        # node that has this node and a new twig as children.
        twig = _phamt_from_kv(k, v, transient=True)
        return _phamt_join_disjoint(self, twig, transient=True)
    addr = self._address
    numel = self._numel
    depth = self._depth
    cells = self._cells
    if isinstance(cells, list):
        # This is a THAMT; we can edit in-place.
        cell = cells[ii]
        if depth == PHAMT_TWIG_DEPTH:
            cells[ii] = (v,)
            if cell is None: object.__setattr__(self, '_numel', numel + 1)
        elif cell is None:
            newcell = _phamt_from_kv(k, v, transient=True)
            if numel == 0: return newcell
            cells[ii] = newcell
            object.__setattr__(self, '_numel', numel + 1)
        else:
            oldn = cell._numel
            newcell = _thamt_set(cell, k, v)
            cells[ii] = newcell
            object.__setattr__(self, '_numel', numel - oldn + len(newcell))
        return self
    else:
        # This is a PHAMT; we return a new edited THAMT.
        cell = cells[ii]
        newcells = list(cells)
        if depth == PHAMT_TWIG_DEPTH:
            newcell = (v,)
            if cell is None: newnumel = numel + 1
            else:            newnumel = numel
        elif cell is None:
            newcell = _phamt_from_kv(k, v, transient=True)
            if numel == 0: return newcell
            newnumel = numel + 1
        else:
            oldn = cell._numel
            newcell = _thamt_set(cell, k, v)
            newnumel = numel - oldn + len(newcell)
        newcells[ii] = newcell
        return PHAMT(addr, depth, newnumel, newcells)
def _thamt_del(self, k):
    if k > PHAMT_KEY_MAX or k < PHAMT_KEY_MIN: raise KeyError(k)
    ii = _key_to_index(self, k)
    cells = self._cells
    cell = cells[ii]
    if cell is None: raise KeyError(k)
    addr = self._address
    numel = self._numel
    depth = self._depth
    istransient = isinstance(cells, list)
    newcells = cells if istransient else list(cells)
    if depth == PHAMT_TWIG_DEPTH:
        if numel == 1: return _thamt_empty()
        newcells[ii] = None
    else:
        newcell = _thamt_del(cell, k)
        if newcell._numel == 0:
            newcells[ii] = None
            # Figure out if we now have only 1 child:
            c0 = None
            c1 = None
            for (jj,c) in enumerate(newcells):
                if c is not None:
                    if c0 is None:
                        c0 = c
                    else:
                        c1 = c
                        break
            if c1 is None: return c0
        else:
            newcells[ii] = newcell
    if istransient:
        object.__setattr__(self, '_numel', numel - 1)
        return self
    else:
        return PHAMT(addr, depth, numel - 1, newcells)


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
    empty = None
    __slots__ = ('_address', '_depth', '_b0sh', '_numel', '_cells')
    # The public interface.
    def __new__(cls, address, depth, numel, cells):
        self = super(PHAMT,cls).__new__(cls)
        object.__setattr__(self, '_address', address)
        object.__setattr__(self, '_depth', depth)
        object.__setattr__(self, '_b0sh', _depth_to_bit0shift(depth))
        object.__setattr__(self, '_numel', numel)
        object.__setattr__(self, '_cells', cells)
        return self
    def __setattr__(self, k, v):
        raise TypeError("type PHAMT is immutable")
    def __setitem__(self, k, v):
        # If cells is a list, this is a THAMT. #TODO
        raise TypeError("type PHAMT is immutable")
    def __getitem__(self, k):
        ii = _key_to_index(self, k)
        cells = self._cells
        c = cells[ii]
        if c is None: raise KeyError(k)
        if self._depth == PHAMT_TWIG_DEPTH: return c[0]
        else: return c[k]
    def __contains__(self, k):
        try: self.__getitem__(k)
        except KeyError: return False
        return True
    def __len__(self):
        return self._numel
    def __iter__(self):
        return PHAMTIter(self)
    def assoc(self, k, v):
        """Returns a new `PHAMT` object with an additional association.

        `phamt_obj.assoc(key, value)` returns a new `PHAMT` object that is equal
        to `phamt_obj` with the modification that in the new object, `key` is
        mapped to `value`. This is copied efficiently using shared state, so
        that the time to perform this update is `O(log n)` and the additional
        space required to keep both the original and the new object in memory is
        also `O(log n)`.
        """
        if k > PHAMT_KEY_MAX or k < PHAMT_KEY_MIN: raise KeyError(k)
        try: ii = _key_to_index(self, k)
        except KeyError:
            # This key is outside the scope of this node, so we return a new
            # node that has this node and a new twig as children.
            return _phamt_join_disjoint(self, _phamt_from_kv(k, v))
        addr = self._address
        numel = self._numel
        depth = self._depth
        cells = self._cells
        cell = cells[ii]
        newcells = list(cells)
        if depth == PHAMT_TWIG_DEPTH:
            newcell = (v,)
            if cell is None: newnumel = numel + 1
            else:            newnumel = numel
        elif cell is None:
            newcell = _phamt_from_kv(k, v)
            if numel == 0: return newcell
            newnumel = numel + 1
        else:
            newcell = cell.assoc(k, v)
            newnumel = numel - len(cell) + len(newcell)
        newcells[ii] = newcell
        return PHAMT(addr, depth, newnumel, tuple(newcells))
    def dissoc(self, k):
        """Returns a new `PHAMT` object with an additional association.

        `phamt_obj.assoc(key, value)` returns a new `PHAMT` object that is equal
        to `phamt_obj` with the modification that in the new object, `key` is
        mapped to `value`. This is copied efficiently using shared state, so
        that the time to perform this update is `O(log n)` and the additional
        space required to keep both the original and the new object in memory is
        also `O(log n)`.
        """
        if k > PHAMT_KEY_MAX or k < PHAMT_KEY_MIN: return self
        try: ii = _key_to_index(self, k)
        except KeyError: return self
        cells = self._cells
        cell = cells[ii]
        if cell is None: return self
        addr = self._address
        numel = self._numel
        depth = self._depth
        newcells = list(cells)
        if depth == PHAMT_TWIG_DEPTH:
            if numel == 1: return PHAMT.empty
            newcells[ii] = None
        else:
            newcell = cell.dissoc(k)
            if newcell is cell:
                return self
            elif newcell is PHAMT.empty:
                newcells[ii] = None
                # Figure out of we now have only 1 child:
                c0 = None
                c1 = None
                for (jj,c) in enumerate(newcells):
                    if c is not None:
                        if c0 is None:
                            c0 = c
                        else:
                            c1 = c
                            break
                if c1 is None: return c0
            else:
                newcells[ii] = newcell
        return PHAMT(addr, depth, numel - 1, tuple(newcells))
    def get(self, k, df):
        try:             return self.__getitem__(k)
        except KeyError: return df
    def transient(self):
        """Returns an equivalent transient HAMT (`THAMT`) object.

        `phamt.transient()` returns a transient `THAMT` object that is
        equivalent to `phamt`. This operation can be performed in constant time,
        and in-place updates to the resulting `THAMT` are performed with minimal
        allocations.
        """
        return THAMT(self)
    @staticmethod
    def from_iter(obj, k0=0):
        """Constructs a PHAMT object from a sequence or iterable of values.

        `PHAMT.from_iter(items)` returns a `PHAMT` object whose keys are the
        integers `0, 1 ... len(items)` and whose values are the elements of the
        iterable `items` in iteration order. This is performed with minimal
        allocations, so it should be more efficient than building the PHAMT from
        scratch.

        `PHAMT.from_iter(items, k0)` returns a `PHAMT` object whose keys are the
        integers `k0, k0+1 ... k0+len(items)`.
        """
        thamt = THAMT(PHAMT.empty)
        for obj in iter(obj):
            thamt[k0] = obj
            k0 += 1
        return thamt.persistent()
        
PHAMT.empty = PHAMT(0, PHAMT_ROOT_DEPTH, 0, (None,)*PHAMT_NCELLS)

# The PHAMT Iterator class.
class PHAMTIter(object):
    __slots__ = ('_phamt', '_stack')
    def __init__(self, phamt):
        self._phamt = phamt
        cells = phamt._cells
        self._stack = [(cells, None, phamt._depth, phamt._address)]
    def __iter__(self):
        return self
    def __next__(self):
        while len(self._stack) > 0:
            (top_cells, top_ii, top_depth, top_addr) = self._stack[-1]
            n = len(top_cells)
            if top_ii is None: top_ii = 0
            else:              top_ii += 1
            while top_ii < n and top_cells[top_ii] is None:
                top_ii += 1
            if top_ii >= n:
                self._stack.pop()
                continue
            cell = top_cells[top_ii]
            # We found a new element
            self._stack[-1] = (top_cells, top_ii, top_depth, top_addr)
            if top_depth == PHAMT_TWIG_DEPTH:
                return (_index_to_key(top_addr, top_ii), cell[0])
            else:
                self._stack.append(
                    (cell._cells, None, cell._depth, cell._address))
        raise StopIteration


# THAMT Class ==================================================================

class THAMT(object):
    """A Transient Hash Array Mapped Trie (THAMT) type.

    The `THAMT` class represents a minimal mutable persistent mapping type
    that can be used to efficiently edit persistent `PHAMT` objects in
    Python. (See also `PHAMT`). A `THAMT` object is essentially a tree
    structure that maps integer keys (hashes) to values, which may be any
    Python objects.

    `THAMT` objects can be greated from `PHAMT` objects (i.e.,
    `thamt = THAMT(phamt)`) in constant time. Unlike `PHAMT` objects,
    `THAMT` objects can be edited in-place like dictionaries. These edits
    are more efficient with respect to time than update to the `PHAMT` tyoe,
    however, they are slightly less space efficient than pure `PHAMT`s. Once
    a `THAMT` has been edited, it can be efficiently converted back into a
    `PHAMT` object using the `thamt.persistent()` method.
    """
    __slots__ = ('_phamt', '_version')
    def __init__(self, phamt=PHAMT.empty):
        if not isinstance(phamt, PHAMT):
            raise TypeError("can only make THAMTs from PHAMTs")
        object.__setattr__(self, '_phamt', phamt)
        object.__setattr__(self, '_version', 0)
    def __setattr__(self, k, v):
        raise TypeError("type THAMT does not allow attribute mutation")
    def __setitem__(self, k, v):
        u = _thamt_set(self._phamt, k, v)
        if u is not self._phamt:
            object.__setattr__(self, '_phamt', u)
        object.__setattr__(self, '_version', self._version + 1)
    def __delitem__(self, k):
        u = _thamt_del(self._phamt, k)
        if u is not self._phamt:
            object.__setattr__(self, '_phamt', u)
        object.__setattr__(self, '_version', self._version + 1)
    def __getitem__(self, k):
        return self._phamt.__getitem__(k)
    def __contains__(self, k):
        return self._phamt.__contains__(k)
    def __len__(self):
        return self._phamt._numel
    def __iter__(self):
        return THAMTIter(self)
    def get(self, k, nf):
        try: return self.__getitem__(k)
        except KeyError: return nf
    def persistent(self):
        if self._phamt._numel == 0: return PHAMT.empty
        # Basicaly, we crawl all the THAMTs turning them into PHAMTs then
        # return these newly-made PHAMTs.
        stack = [self._phamt]
        while len(stack) > 0:
            phamt = stack.pop()
            # Check this PHAMT.
            cells = phamt._cells
            if isinstance(cells, list):
                object.__setattr__(phamt, '_cells', tuple(cells))
                if phamt._depth == PHAMT_TWIG_DEPTH: continue
                for cell in cells:
                    if cell is not None:
                        stack.append(cell)
        return self._phamt

# The THAMT Iterator class.
class THAMTIter(object):
    __slots__ = ('_thamt', '_stack', '_version')
    def __init__(self, thamt):
        self._thamt = thamt
        self._version = thamt._version
        phamt = thamt._phamt
        cells = phamt._cells
        self._stack = [(cells, None, phamt._depth, phamt._address)]
    def __iter__(self):
        return self
    def __next__(self):
        if self._version != self._thamt._version:
            raise RuntimeError("THAMT changed during iteration")
        while len(self._stack) > 0:
            (top_cells, top_ii, top_depth, top_addr) = self._stack[-1]
            n = len(top_cells)
            if top_ii is None: top_ii = 0
            else:              top_ii += 1
            while top_ii < n and top_cells[top_ii] is None:
                top_ii += 1
            if top_ii >= n:
                self._stack.pop()
                continue
            cell = top_cells[top_ii]
            # We found a new element
            self._stack[-1] = (top_cells, top_ii, top_depth, top_addr)
            if top_depth == PHAMT_TWIG_DEPTH:
                return (_index_to_key(top_addr, top_ii), cell[0])
            else:
                self._stack.append(
                    (cell._cells, None, cell._depth, cell._address))
        raise StopIteration

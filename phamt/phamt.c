////////////////////////////////////////////////////////////////////////////////
// phamt/phamt.c
// Implemntation of the core phamt C data structures.
// by Noah C. Benson

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <Python.h>
#include "phamt.h"

//------------------------------------------------------------------------------
// Some configuration

// If we want to print debug statements, we can define the following:
//#define __PHAMT_DEBUG
// The consequences: (some functions for debugging).
#ifdef __PHAMT_DEBUG
#  define dbgmsg(...) (fprintf(stderr, __VA_ARGS__))
#  define dbgnode(prefix, u) \
     dbgmsg("%s node={addr=(%p, %u, %u, %u),\n"                    \
            "%s       numel=%u, bits=%p,\n"                        \
            "%s       flags={pyobj=%u, firstn=%u}}\n",             \
            (prefix),                                              \
            (void*)(u)->address, (u)->addr_depth,                  \
            (u)->addr_startbit, (u)->addr_shift,                   \
            (prefix),                                              \
            (unsigned)(u)->numel, (void*)((intptr_t)(u)->bits),    \
            (prefix),                                              \
            (u)->flag_pyobject, (u)->flag_firstn)
#  define dbgci(prefix, ci)                                       \
     dbgmsg("%s ci={found=%u, beneath=%u, cell=%u, bit=%u}\n",    \
            (prefix), (ci).is_found, (ci).is_beneath,             \
            (ci).cellindex, (ci).bitindex)
#else
#  define dbgmsg(...)
#  define dbgnode(prefix, u)
#  define dbgci(prefix, ci)
#endif
#define MAX_64BIT 0xffffffffffffffff
// Possibly, the uint128_t isn't defined, but could be...
#ifndef uint128_t
#  if defined ULLONG_MAX && (ULLONG_MAX >> 64 == MAX_64BIT)
   typedef unsigned long long uint128_t;
#  endif
#endif
// Handy constant values.
#define HASH_ZERO ((hash_t)0)
#define HASH_ONE  ((hash_t)1)
// The bits type is also defined here.
typedef uint32_t bits_t;
#define BITS_BITCOUNT 32
#define BITS_MAX 0xffffffff
// Number of bits in the total addressable hash-space of a PHAMT.
// Only certain values are supported. The popcount, clz, and ctz functions are
// defined below.
#define popcount_bits popcount32
#define clz_bits      clz32
#define ctz_bits      ctz32
#if   (HASH_BITCOUNT == 16)
#     define popcount_hash    popcount16
#     define clz_hash         clz16
#     define ctz_hash         ctz16
#elif (HASH_BITCOUNT == 32)
#     define popcount_hash    popcount32
#     define clz_hash         clz32
#     define ctz_hash         ctz32
#elif (HASH_BITCOUNT == 64)
#     define popcount_hash    popcount64
#     define clz_hash         clz64
#     define ctz_hash         ctz64
#elif (HASH_BITCOUNT == 128)
#     define popcount_hash    popcount128
#     define clz_hash         clz128
#     define ctz_hash         ctz128
#else
#     error unhandled size for hash_t
#endif
#define BITS_ZERO           ((bits_t)0)
#define BITS_ONE            ((bits_t)1)
#define PHAMT_DOCSTRING     (                                                  \
   "A Persistent Hash Array Mapped Trie (PHAMT) type.\n"                       \
   "\n"                                                                        \
   "The `PHAMT` class represents a minimal immutable persistent mapping type\n"\
   "that can be used to implement persistent collections in Python\n"          \
   "efficiently. A `PHAMT` object is essentially a persistent dictionary\n"    \
   "that requires that all keys be Python integers (hash values); values may\n"\
   "be any Python objects. `PHAMT` objects are highly efficient at storing\n"  \
   "either sparse hash values or lists of consecutive hash values, such as\n"  \
   "when the keys `0`, `1`, `2`, etc. are used.\n"                             \
   "\n"                                                                        \
   "To add or remove key/valye pairs from a `PHAMT`, the methods\n"            \
   "`phamt_obj.assoc(k, v)` and `phamt_obj.dissoc(k)`, both of which return\n" \
   "copies of `phamt_obj` with the requested change.\n"                        \
   "\n"                                                                        \
   "`PHAMT` objects can be created in the following ways:\n"                   \
   " * by using `phamt_obj.assic(k,v)` or `phamt_obj.dissoc(k)` on existing\n" \
   "   `PHAMT` objects, such as the `PHAMT.empty` object, which represents\n"  \
   "   an empty `PHAMT`;\n"                                                    \
   " * by supplying the `PHAMT.from_list(iter_of_values)` with a list of\n"    \
   "   values, which are assigned the keys `0`, `1`, `2`, etc.\n"              )


//------------------------------------------------------------------------------
// Utility functions.
// Functions for making masks and counting bits.

// popcount(bits)
// Returns the number of set bits in the given bits_t unsigned integer.
#if   defined (__builtin_popcount) && (UINT_MAX == BITS_MAX)
#     define popcount32 __builtin_popcount
#elif defined (__builtin_popcountl) && (ULONG_MAX == BITS_MAX)
#     define popcount32 __builtin_popcountl
#elif defined (ULLONG_MAX) && defined (__builtin_popcountll) && (ULLONG_MAX == BITS_MAX)
#     define popcount32 __builtin_popcountll
#else
      inline uint32_t popcount32(uint32_t w)
      {
         w = w - ((w >> 1) & 0x55555555);
         w = (w & 0x33333333) + ((w >> 2) & 0x33333333);
         w = (w + (w >> 4)) & 0x0F0F0F0F;
         return (w * 0x01010101) >> 24;
      }
#endif
inline uint16_t popcount16(uint16_t w)
{
   return popcount32((uint32_t)w);
}
#if   defined (__builtin_popcount) && (UINT_MAX == MAX_64BIT)
#     define popcount64 __builtin_popcount
#elif defined (__builtin_popcountl) && (ULONG_MAX == MAX_64BIT)
#     define popcount64 __builtin_popcountl
#elif defined (ULLONG_MAX) && defined (__builtin_popcountll) && (ULLONG_MAX == MAX_64BBIT)
#     define popcount64 __builtin_popcountll
#else
      inline uint64_t popcount64(uint64_t w)
      {
         return popcount32((uint32_t)w) + popcount32((uint32_t)(w >> 32));
      }
#endif
#ifdef uint128_t
      inline uint64_t popcount128(uint128_t w)
      {
         return (popcount32((uint32_t)w) +
                 popcount32((uint32_t)(w >> 32)) +
                 popcount32((uint32_t)(w >> 64)) +
                 popcount32((uint32_t)(w >> 96)))
      }
#endif
// clz(bits)
// Returns the number of leading zeros in the bits.
inline uint32_t clz32(uint32_t v)
{
   v = v | (v >> 1);
   v = v | (v >> 2);
   v = v | (v >> 4);
   v = v | (v >> 8);
   v = v | (v >> 16);
   return popcount32(~v);
}
inline uint16_t clz16(uint16_t w)
{
   return clz32((uint32_t)w) - 16;
}
inline uint64_t clz64(uint64_t w)
{
   uint32_t c = clz32((uint32_t)(w >> 32));
   return (c == 32 ? 32 + clz32((uint32_t)w) : c);
}
#ifdef uint128_t
      inline uint64_t clz128(uint128_t w)
      {
         uint32_t c = clz32((uint32_t)(w >> 96));
         if (c < 32) return c;
         c = clz32((uint32_t)(w >> 64));
         if (c < 32) return c + 32;
         c = clz32((uint32_t)(w >> 32));
         if (c < 32) return c + 64;
         return clz32((uint32_t)w) + 96;
      }
#endif
// ctz(bits)
// Returns the number of trailing zeros in the bits.
inline uint32_t ctz32(uint32_t v)
{
   static const int deBruijn_values[32] = {
      0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
   };
   return deBruijn_values[((uint32_t)((v & -v) * 0x077CB531U)) >> 27];
}
inline uint16_t ctz16(uint16_t w)
{
   return ctz32((uint32_t)w);
}
inline uint64_t ctz64(uint64_t w)
{
   uint32_t c = ctz32((uint32_t)w);
   return (c == 32 ? 32 + ctz32((uint32_t)(w >> 32)) : c);
}
#ifdef uint128_t
      inline uint128_t ctz128(uint128_t w)
      {
         uint32_t c = ctz32((uint32_t)w);
         if (c < 32) return c;
         c = ctz32((uint32_t)(w >> 32));
         if (c < 32) return c + 32;
         c = ctz32((uint32_t)(w >> 64));
         if (c < 32) return c + 64;
         return ctz32((uint32_t)(w >> 96)) + 96;
      }
#endif
// lowmask(bitno)
// Yields a mask of all bits above the given bit number set to false and all
// bits below that number set to true. The bit itself is set to false. Bits are
// indexed starting at 0.
// lowmask(bitno) is equal to ~highmask(bitno).
inline bits_t lowmask_bits(bits_t bitno)
{
   return ((BITS_ONE << bitno) - BITS_ONE);
}
inline hash_t lowmask_hash(hash_t bitno)
{
   return ((HASH_ONE << bitno) - HASH_ONE);
}
// The mask of the bits that represent the depth of the node-id in a PTree.
// const bits_t PHAMT_DEPTH_MASK = lowmask_bits(PHAMT_TWIG_SHIFT);
#define PHAMT_DEPTH_MASK_BITS ((BITS_ONE << PHAMT_TWIG_SHIFT) - BITS_ONE)
#define PHAMT_DEPTH_MASK_HASH ((HASH_ONE << PHAMT_TWIG_SHIFT) - HASH_ONE)
// highmask(bitno)
// Yields a mask of all bits above the given bit number set to true and all
// bits below that number set to true. The bit itself is set to true. Bits are
// indexed starting at 0.
// highmask(bitno) is equal to ~lowmask(bitno).
inline bits_t highmask_bits(bits_t bitno)
{
   return ~((BITS_ONE << bitno) - BITS_ONE);
}
inline hash_t highmask_hash(hash_t bitno)
{
   return ~((HASH_ONE << bitno) - HASH_ONE);
}
// is_firstn(bits)
// True if the first n bits (and only those bits) are set (for any n) and False
// otherwise.
inline uint8_t is_firstn(bits_t bits)
{
   return lowmask_bits(BITS_BITCOUNT - clz_bits(bits)) == bits;
}

// ptree_depth(nodeaddr)
// Yields the depth of the node with the given node address. This depth is in
// the theoretical complete tree, not in the reified tree represented in memory.
inline hash_t phamt_depth(hash_t nodeid)
{
   return nodeid & PHAMT_DEPTH_MASK_HASH;
}
// depth_to_startbit(depth)
// Yields the first bit in the hash type for the given depth.
inline hash_t depth_to_startbit(hash_t depth)
{
   if (depth == PHAMT_TWIG_DEPTH)
      return 0;
   else if (depth == 0)
      return PHAMT_ROOT_FIRSTBIT;
   else
      return PHAMT_ROOT_FIRSTBIT - depth*PHAMT_NODE_SHIFT;
}
// depth_to_shift(depth)
// Yields the first bit in the hash type for the given depth.
inline hash_t depth_to_shift(hash_t depth)
{
   if (depth == PHAMT_TWIG_DEPTH)
      return PHAMT_TWIG_SHIFT;
   else if (depth == 0)
      return PHAMT_ROOT_SHIFT;
   else
      return PHAMT_NODE_SHIFT;
}
// phamt_startbit(nodeid)
// Yields the first has bit for the given ptree node id.
inline hash_t phamt_startbit(hash_t nodeid)
{
   return depth_to_startbit(phamt_depth(nodeid));
}
// phamt_depthmask(depth)
// Yields the mask that includes the address space for all nodes at or below the
// given depth.
inline hash_t phamt_depthmask(hash_t depth)
{
   if (depth == PHAMT_TWIG_DEPTH)
      return (HASH_ONE << PHAMT_TWIG_SHIFT) - HASH_ONE;
   else if (depth == 0)
      return HASH_MAX;
   else
      return ((HASH_ONE << (PHAMT_ROOT_FIRSTBIT - (depth-1)*PHAMT_NODE_SHIFT))
              - HASH_ONE);
}
// phamt_nodemask(nodeid)
// Yields the lowmask for the node's top bit.
inline hash_t phamt_nodemask(hash_t nodeid)
{
   return phamt_depthmask(phamt_depth(nodeid));
}
// phamt_shift(nodeid)
// Yields the bitshift for the given ptree node id.
inline hash_t phamt_shift(hash_t nodeid)
{
   return depth_to_shift(phamt_depth(nodeid));
}
// phamt_minleaf(nodeid)
// Yields the minimum child leaf index associated with the given nodeid.
inline hash_t phamt_minleaf(hash_t nodeid)
{
   return nodeid & ~phamt_nodemask(nodeid);
}
// phamt_maxleaf(nodeid)
// Yields the maximum child leaf index assiciated with the given nodeid.
inline hash_t phamt_maxleaf(hash_t nodeid)
{
   return nodeid | phamt_nodemask(nodeid);
}
// phamt_id(minleaf, depth)
// Yields the node-id for the node whose minimum leaf and depth are given.
inline hash_t phamt_id(hash_t minleaf, hash_t depth)
{
   return minleaf | depth;
}
// phamt_parentid(nodeid0)
// Yields the node-id of the parent of the given node. Note that node 0 (the
// tree's theoretical root) has no parent. If given a node id of 0, this
// function will return an arbitrary large number.
inline hash_t phamt_parentid(hash_t nodeid0)
{
   hash_t d = phamt_depth(nodeid0) - 1;
   return phamt_id(nodeid0 & ~phamt_depthmask(d), d);
}
// phamt_isbeneath(nodeid, leafid)
// Yields true if the given leafid can be found beneath the given node-id.
inline hash_t phamt_isbeneath(hash_t nodeid, hash_t leafid)
{
   hash_t mask = phamt_nodemask(nodeid);
   return leafid <= (nodeid | mask) && leafid >= (nodeid & ~mask);
}
// phamt_isbeneath_mask(nodeid, leafid, mask)
// Yields true if the given leafid can be found beneath the given node-id.
// Same as above, but if you already have the mask.
inline hash_t phamt_isbeneath_mask(hash_t nodeid, hash_t leafid, hash_t mask)
{
   return leafid <= (nodeid | mask) && leafid >= (nodeid & ~mask);
}
// phamt_highbitdiff(id1, id2)
// Yields the highest bit that is different between id1 and id2.
inline bits_t phamt_highbitdiff_bits(bits_t id1, bits_t id2)
{
   return BITS_BITCOUNT - clz_bits(id1 ^ id2) - 1;
}
inline hash_t phamt_highbitdiff_hash(hash_t id1, hash_t id2)
{
   return HASH_BITCOUNT - clz_hash(id1 ^ id2) - 1;
}

//------------------------------------------------------------------------------
// Core PHAMT implementation.
struct PHAMT {
   // The Python stuff.
   PyObject_VAR_HEAD

   // The node's address in the PHAMT.
   hash_t address;
   // The number of leaves beneath this node.
   hash_t numel;
   // The bitmask of children.
   bits_t bits;

   // What follows is a set of meta-data that also manages to fill in the other
   // 32 bits of the 64-bit block that startd with bits.
   //
   // The PHAMT's first bit (this is enough for 256-bit integer hashes).
   bits_t addr_startbit : 8;
   // The PHAMT's depth.
   bits_t addr_depth : 8;
   // The PHAMT's shif).
   bits_t addr_shift : 5;
   // Whether the PHAMT is transient or not.
   bits_t flag_transient : 1;
   // Whether the PHAMT stores Python objects (1) or C objects (0).
   bits_t flag_pyobject : 1;
   // Whether the PHAMT stores all of its (n) cells in its first n nodes.
   bits_t flag_firstn : 1;
   // The remaining bits are just empty for now.
   bits_t _empty : 8;
   
   // And the variable-length list of children.
   void* cells[];
};
// A type foor passing information about cells.
typedef struct cellindex_data {
   uint8_t bitindex;   // the bit index of the node
   uint8_t cellindex;  // the cell index of the node
   uint8_t is_beneath; // whether the key is beneath this node
   uint8_t is_found;   // whether the bit for the key is set
} cellindex_t;
#define PHAMT_SIZE sizeof(struct PHAMT)
static PHAMT_t PHAMT_EMPTY = NULL;
static PHAMT_t PHAMT_EMPTY_CTYPE = NULL;
#define RETURN_EMPTY ({Py_INCREF(PHAMT_EMPTY); return PHAMT_EMPTY;})
#define RETURN_EMPTY_CTYPE ({Py_INCREF(PHAMT_EMPTY_CTYPE); return PHAMT_EMPTY_CTYPE;})
// We're going to define these constructors later when we have defined the type.
static PHAMT_t phamt_new(unsigned ncells);
static PHAMT_t phamt_copy_chgcell(PHAMT_t node, cellindex_t ci, void* val);
static PHAMT_t phamt_copy_addcell(PHAMT_t node, cellindex_t ci, void* val);
static PHAMT_t phamt_copy_delcell(PHAMT_t node, cellindex_t ci);
// Additional constructors that are part of the Python interface.
static PyObject* phamt_py_from_list(PyObject* self, PyObject *const *args, Py_ssize_t nargs);

// Get the number of cells (not the number of elements).
inline bits_t phamt_cellcount(PHAMT_t u)
{
   return (bits_t)Py_SIZE(u);
}
// phamt_cellindex(node, leafid)
// Yields a cellindex_t structure that indicates whether and where the leafid is
// with respect to node.
inline cellindex_t phamt_cellindex(PHAMT_t node, hash_t leafid)
{
   cellindex_t ci;
   hash_t mask = phamt_depthmask(node->addr_depth);
   ci.is_beneath = phamt_isbeneath_mask(node->address, leafid, mask);
   // Grab the index out of the leaf id.
   ci.bitindex = ((leafid >> node->addr_startbit) & 
                  lowmask_hash(node->addr_shift));
   // Get the cellindex.
   ci.cellindex = (node->flag_firstn
                   ? ci.bitindex
                   : popcount_bits(node->bits & lowmask_bits(ci.bitindex)));
   // is_found depends on whether the bit is set.
   ci.is_found = (ci.is_beneath
                  ? ((node->bits & (BITS_ONE << ci.bitindex)) != 0)
                  : 0);
   return ci;
}
// phamt_cellkey(ptree, childidx)
// Yields the leafid (a hash_t value) of the key that goes with the particular
// child index that is given. This only works correctly for twig nodes.
inline hash_t phamt_cellkey(hash_t id, hash_t k)
{
   return phamt_minleaf(id) | k;
}

// phamt_lookup(node, k)
// Yields the leaf value for the hash k. If no such key is in the phamt, then
// NULL is returned.
// This function does not deal at all with INCREF or DECREF, so before returning
// anything returned from this function back to Python, be sure to INCREF it.
void* phamt_lookup(PHAMT_t node, hash_t k)
{
   cellindex_t ci;
   uint8_t depth;
   dbgmsg("[phamt_lookup] call: key=%p\n", (void*)k);
   do {
      ci = phamt_cellindex(node, k);
      dbgnode("[phamt_lookup]      ", node);      
      dbgci(  "[phamt_lookup]      ", ci);
      if (!ci.is_found) return NULL;
      depth = node->addr_depth;
      node = (PHAMT_t)node->cells[ci.cellindex];
   } while (depth != PHAMT_TWIG_DEPTH);
   dbgmsg("[phamt_lookup]       return %p\n", (void*)node);
   return (void*)node;
}
// phamt_assoc(node, k, v)
// Yields a copy of the given PHAMT with the new key associated. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_assoc(PHAMT_t node, hash_t k, void* v)
{
   cellindex_t ci;
   PHAMT_t kv, u;
   // If the node is empty, we just return a new node. This auto-updates the
   // refcount for v.
   if (node->numel == 0)
      return phamt_from_kv(k, v, node->flag_pyobject);
   // Get the cell-index.
   ci = phamt_cellindex(node, k);
   // Is the key beneath this node or not?
   if (ci.is_found) {
      // This means that the bit is set, so we need to either update a leaf or
      // continue to search down for a twig.
      if (node->addr_depth == PHAMT_TWIG_DEPTH) {
         // We'e replacing a leaf. Go ahead and alloc a copy (this increases
         // the refcount for everything, so we need to deref the old child.
         void* obj = node->cells[ci.cellindex];
         if (obj == v) {
            Py_INCREF(node);
            return node;
         }
         u = phamt_copy_chgcell(node, ci, v);
      } else {
         // We pass control on down and return a copy of ourselves on the way
         // back up.
         PHAMT_t oldcell = node->cells[ci.cellindex];
         PHAMT_t newcell = phamt_assoc(oldcell, k, v);
         if (oldcell == newcell) {
            Py_DECREF(newcell);
            Py_INCREF(node);
            return node;
         }
         // We make a copy, which increments the refcounts, but we'll have to
         // deref the oldcell due to this.
         u = phamt_copy_chgcell(node, ci, (void*)newcell);
         u->numel += newcell->numel - oldcell->numel;
         // We aren't returning newcell, so we no longer hold this reference.
         Py_DECREF(newcell);
         // The caller now inherits the reference for u.
      }
   } else if (ci.is_beneath) {
      // The key is beneath this node, so we will dig down until we find the
      // correct place for this key.
      // Okay, is the node a twig or not?
      if (node->addr_depth == PHAMT_TWIG_DEPTH) {
         // We're adding a new leaf. This updates refcounts for everything
         // except the replaced cell.
         u = phamt_copy_addcell(node, ci, (void*)v);
         u->numel = node->numel + 1;
      } else {
         // We are not a twig.
         // We are adding a twig node containing just the leaf below us. This
         // copy function auto-updates the refcounts for all children.
         kv = phamt_from_kv(k, v, node->flag_pyobject);
         u = phamt_copy_addcell(node, ci, (void*)kv);
         ++(u->numel);
         Py_DECREF((PyObject*)kv);
      }
   } else {
      uint8_t bit0, shift, newdepth;
      hash_t h;
      // The key isn't beneath this node, so we need to make a higher up node
      // that links to both the key and node.
      // The key and value will go in their own node, regardless.
      kv = phamt_from_kv(k, v, node->flag_pyobject);
      // What's the highest bit at which they differ?
      h = highmask_hash(node->addr_startbit + node->addr_shift);
      h = phamt_highbitdiff_hash(node->address, k & h);
      if (h <= HASH_BITCOUNT - PHAMT_ROOT_SHIFT) {
         // We're allocating a new non-root node.
         bit0 = (h - PHAMT_TWIG_SHIFT) / PHAMT_NODE_SHIFT;
         newdepth = PHAMT_LEVELS - 2 - bit0;
         bit0 = bit0*PHAMT_NODE_SHIFT + PHAMT_TWIG_SHIFT;
         shift = PHAMT_NODE_SHIFT;
      } else {
         // We're allocating a new root node.
         newdepth = 0;
         bit0 = HASH_BITCOUNT - PHAMT_ROOT_SHIFT;
         shift = PHAMT_ROOT_SHIFT;
      }
      // Go ahead and allocate the new node.
      u = phamt_new(2);
      u->address = node->address & highmask_hash(bit0+shift);
      u->numel = 1 + node->numel;
      u->flag_pyobject = node->flag_pyobject;
      u->flag_transient = 0;
      u->addr_shift = shift;
      u->addr_startbit = bit0;
      u->addr_depth = newdepth;
      // We use h to store the new minleaf value.
      u->bits = 0;
      h = lowmask_hash(shift);
      u->bits |= BITS_ONE << (h & (node->address >> bit0));
      u->bits |= BITS_ONE << (h & (k >> bit0));
      if (k < node->address) {
         u->cells[0] = (void*)kv;
         u->cells[1] = (void*)node;
      } else {
         u->cells[0] = (void*)node;
         u->cells[1] = (void*)kv;
      }
      u->flag_firstn = is_firstn(u->bits);
      // We refcount node (from u), and pass kv's refcount ownership to u.
      Py_INCREF(node);
   }
   // We need to register the new node u with the garbage collector.
   PyObject_GC_Track((PyObject*)u);
   // Return the new PHAMT object!
   return (PHAMT_t)u;
}
PHAMT_t thamt_assoc(PHAMT_t node, hash_t k, PyObject* v)
{
   return NULL; // #TODO
}
// phamt_dissoc(node, k)
// Yields a copy of the given PHAMT with the given key removed. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_dissoc(PHAMT_t node, hash_t k)
{
   PHAMT_t u;
   cellindex_t ci = phamt_cellindex(node, k);
   // If the node lies outside of this node or the bit isn't set, there's
   // nothing to do.
   if (!ci.is_found) {
      Py_INCREF(node);
      return node;
   }
   // There's something to delete; start by determining if we are or are not a
   // twig node:
   if (node->addr_depth == PHAMT_TWIG_DEPTH) {
      // If we are deleting the only element, we return a new empty node.
      if (node->numel == 1) return phamt_empty_like(node);
      // Otherwise, we need to realloc this node. This takes care of reference
      // count increments.
      u = phamt_copy_delcell(node, ci);
      --(u->numel);
   } else {
      PHAMT_t newcell, *oldcell = node->cells[ci.cellindex];
      newcell = (PHAMT_t)phamt_dissoc((PHAMT_t)oldcell, k);
      if (newcell == (PHAMT_t)oldcell) {
         Py_DECREF(newcell);
         Py_INCREF(node);
         return node;
      }
      if (newcell->numel == 0) {
         // So newcell is the empty node; if we're now empty, we can avoid the
         // refcounting on the empty by just passing newcell along.
         if (node->numel == 1)
            return newcell;
         // Additionally, if this would leave us with exactly 1 bit set,
         // we should just return that child isntead (it is sufficient!).
         if (popcount_bits(node->bits & ~(BITS_ONE << ci.bitindex)) == 1) {
            node = node->cells[!ci.cellindex];
            Py_INCREF(node);
            return node;
         }
         // Otherwise, we're deleting the subnode. This takes care of refcounts.
         u = phamt_copy_delcell(node, ci);
         --(u->numel);
      } else {
         // we're replacing the subnode. This duplicates the refcounts for
         // all items, including the oldcell, so we decref that one.
         u = phamt_copy_chgcell(node, ci, newcell);
         u->numel += newcell->numel - ((PHAMT_t)oldcell)->numel;
         Py_DECREF(newcell);
      }
   }
   // We need to register the new node u with the garbage collector.
   PyObject_GC_Track((PyObject*)u);
   return (PHAMT_t)u;
}


//------------------------------------------------------------------------------
// Python Type and Module Code
// This section contains all the code necessary to setup the Python PHAMT type
// and register the module with Python.
// Many of the functions below are just wrappers around the functions above.

static PyObject* phamt_py_assoc(PyObject* self, PyObject* varargs)
{
   hash_t h;
   PyObject* key, *val;
   if (!PyArg_ParseTuple(varargs, "OO:assoc", &key, &val))
      return NULL;
   if (!PyLong_Check(key)) {
      PyErr_SetString(PyExc_TypeError, "PHAMT keys must be integers");
      return NULL;
   }
   h = (hash_t)PyLong_AsSsize_t(key);
   return (PyObject*)phamt_assoc((PHAMT_t)self, h, val);
}
static PyObject* phamt_py_dissoc(PyObject* self, PyObject* varargs)
{
   hash_t h;
   PyObject* key;
   if (!PyArg_ParseTuple(varargs, "O:dissoc", &key))
      return NULL;
   if (!PyLong_Check(key)) {
      PyErr_SetString(PyExc_TypeError, "PHAMT keys must be integers");
      return NULL;
   }
   h = (hash_t)PyLong_AsSsize_t(key);
   return (PyObject*)phamt_dissoc((PHAMT_t)self, h);
}
static PyObject* phamt_py_get(PyObject* self, PyObject* varargs)
{
   hash_t h;
   PyObject* key, *res, *dv;
   Py_ssize_t sz = PyTuple_Size(varargs);
   if (sz == 1) {
      if (!PyArg_ParseTuple(varargs, "O:get", &key))
         return NULL;
      dv = NULL;
   } else if (sz == 2) {
      if (!PyArg_ParseTuple(varargs, "OO:get", &key, &dv))
         return NULL;
   } else {
      PyErr_SetString(PyExc_ValueError, "get requires 1 or 2 arguments");
      return NULL;
   }
   if (!PyLong_Check(key)) {
      PyErr_SetString(PyExc_TypeError, "PHAMT keys must be integers");
      return NULL;
   }
   h = (hash_t)PyLong_AsSsize_t(key);
   res = (PyObject*)phamt_lookup((PHAMT_t)self, h);
   if (res) {
      Py_INCREF(res);
      return res;
   } else if (dv) {
      Py_INCREF(dv);
      return dv;
   } else {
      Py_RETURN_NONE;
   }
}
static PyObject *PHAMT_py_getitem(PyObject *type, PyObject *item)
{
   Py_INCREF(type);
   return type;
}
static int phamt_tp_contains(PHAMT_t self, PyObject* key)
{
   hash_t h;
   if (!PyLong_Check(key)) return 0;
   h = (hash_t)PyLong_AsSsize_t(key);
   key = phamt_lookup(self, h);
   return (key? 1 : 0);
}
static PyObject* phamt_tp_subscript(PHAMT_t self, PyObject* key)
{
   PyObject* val;
   hash_t h;
   if (!PyLong_Check(key)) {
      PyErr_SetObject(PyExc_KeyError, key);
      return NULL;
   }
   h = (hash_t)PyLong_AsSsize_t(key);
   val = phamt_lookup(self, h);
   if (val)
      Py_INCREF(val);
   else
      PyErr_SetObject(PyExc_KeyError, key);
   return (PyObject*)val;
}
static Py_ssize_t phamt_tp_len(PHAMT_t self)
{
   return (Py_ssize_t)self->numel;
}
static PyObject *phamt_tp_iter(PHAMT_t self)
{
   return NULL; // #TODO
}
static void phamt_tp_dealloc(PHAMT_t self)
{
   PyTypeObject* tp;
   PyObject* tmp;
   bits_t ii, ncells;
   tp = Py_TYPE(self);
   ncells = phamt_cellcount(self);
   PyObject_GC_UnTrack(self);
   // Walk through the children, dereferencing them
   if (self->addr_depth < PHAMT_TWIG_DEPTH || self->flag_pyobject) {
      for (ii = 0; ii < ncells; ++ii) {
         tmp = self->cells[ii];
         self->cells[ii] = NULL;
         Py_DECREF(tmp);
      }
   }
   tp->tp_free(self);
}
static PyObject* phamt_tp_richcompare(PyObject* self, PyObject *other, int op)
{
   return NULL; // #TODO
}
static int phamt_tp_traverse(PHAMT_t self, visitproc visit, void *arg)
{
   hash_t ii, ncells;
   PyTypeObject* tp;
   tp = Py_TYPE(self);
   Py_VISIT(tp);
   if (self->addr_depth == PHAMT_TWIG_DEPTH && !self->flag_pyobject)
      return 0;
   ncells = phamt_cellcount(self);
   for (ii = 0; ii < ncells; ++ii) {
      Py_VISIT(((PHAMT_t)self)->cells[ii]);
   }
   return 0;
}
static PyObject* phamt_tp_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
   // #TODO
   Py_INCREF(PHAMT_EMPTY);
   return (PyObject*)PHAMT_EMPTY;
}
static PyObject* phamt_py_repr(PHAMT_t self)
{
   dbgnode("[phamt_py_repr]", self);
   return PyUnicode_FromFormat("<PHAMT:n=%u>", (unsigned)self->numel);
}
static void phamt_module_free(void* mod)
{
   PHAMT_t tmp = PHAMT_EMPTY;
   PHAMT_EMPTY = NULL;
   Py_DECREF(tmp);
   tmp = PHAMT_EMPTY_CTYPE;
   PHAMT_EMPTY_CTYPE = NULL;
   Py_DECREF(tmp);
}

static PyMethodDef PHAMT_methods[] = {
   {"assoc", (PyCFunction)phamt_py_assoc, METH_VARARGS, NULL},
   {"dissoc", (PyCFunction)phamt_py_dissoc, METH_VARARGS, NULL},
   {"get", (PyCFunction)phamt_py_get, METH_VARARGS, NULL},
   //{"update", (PyCFunction)phamt_py_update, METH_VARARGS | METH_KEYWORDS, NULL},
   {"__class_getitem__", (PyCFunction)PHAMT_py_getitem, METH_O|METH_CLASS, NULL},
   // For JSON/pickle serialization.
   {"from_list", (PyCFunction)phamt_py_from_list, METH_FASTCALL,
    ("Constructs a PHAMT object from a sequence or iterable of values, which"
     " are assigned the keys 0, 1, 2, etc. in iteration order.")},
   {NULL, NULL, 0, NULL}
};
static PySequenceMethods PHAMT_as_sequence = {
   0,                             // sq_length
   0,                             // sq_concat
   0,                             // sq_repeat
   0,                             // sq_item
   0,                             // sq_slice
   0,                             // sq_ass_item
   0,                             // sq_ass_slice
   (objobjproc)phamt_tp_contains, // sq_contains
   0,                             // sq_inplace_concat
   0,                             // sq_inplace_repeat
};
static PyMappingMethods PHAMT_as_mapping = {
   (lenfunc)phamt_tp_len,          // mp_length
   (binaryfunc)phamt_tp_subscript, // mp_subscript
   NULL
};
static PyTypeObject PHAMT_type = {
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "phamt.core.PHAMT",
   .tp_doc = PyDoc_STR(PHAMT_DOCSTRING),
   .tp_basicsize = PHAMT_SIZE,
   .tp_itemsize = sizeof(void*),
   .tp_methods = PHAMT_methods,
   .tp_as_mapping = &PHAMT_as_mapping,
   .tp_as_sequence = &PHAMT_as_sequence,
   .tp_iter = (getiterfunc)phamt_tp_iter,
   .tp_dealloc = (destructor)phamt_tp_dealloc,
   .tp_getattro = PyObject_GenericGetAttr,
   .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
   .tp_richcompare = phamt_tp_richcompare,
   .tp_traverse = (traverseproc)phamt_tp_traverse,
   // PHAMTs, like tuples, can't make ref links because they are 100% immutable.
   //.tp_clear = (inquiry)phamt_tp_clear,
   .tp_new = phamt_tp_new,
   .tp_repr = (reprfunc)phamt_py_repr,
   .tp_str = (reprfunc)phamt_py_repr,
};
static struct PyModuleDef phamt_pymodule = {
   PyModuleDef_HEAD_INIT,
   "core",
   NULL,
   -1,
   NULL,
   NULL,
   NULL,
   NULL,
   phamt_module_free
};
PyMODINIT_FUNC PyInit_core(void)
{
   PyObject* m = PyModule_Create(&phamt_pymodule);
   if (m == NULL) return NULL;
   // Initialize the PHAMT_type a tp_dict.
   if (PyType_Ready(&PHAMT_type) < 0) return NULL;
   Py_INCREF(&PHAMT_type);
   // Get the Empty PHAMT ready.
   PHAMT_EMPTY = (PHAMT_t)PyObject_GC_NewVar(struct PHAMT, &PHAMT_type, 0);
   if (!PHAMT_EMPTY) return NULL;
   PHAMT_EMPTY->address = 0;
   PHAMT_EMPTY->numel = 0;
   PHAMT_EMPTY->bits = 0;
   PHAMT_EMPTY->flag_transient = 0;
   PHAMT_EMPTY->flag_firstn = 0;
   PHAMT_EMPTY->flag_pyobject = 1;
   PHAMT_EMPTY->addr_startbit = HASH_BITCOUNT - PHAMT_ROOT_SHIFT;
   PHAMT_EMPTY->addr_shift = PHAMT_ROOT_SHIFT;
   PHAMT_EMPTY->addr_depth = 0;
   PyObject_GC_Track(PHAMT_EMPTY);
   PyDict_SetItemString(PHAMT_type.tp_dict, "empty", (PyObject*)PHAMT_EMPTY);
   // Also the Empty non-Python-object PHAMT for use with C code.
   PHAMT_EMPTY_CTYPE = (PHAMT_t)PyObject_GC_NewVar(struct PHAMT, &PHAMT_type, 0);
   if (!PHAMT_EMPTY_CTYPE) return NULL;
   PHAMT_EMPTY_CTYPE->address = 0;
   PHAMT_EMPTY_CTYPE->numel = 0;
   PHAMT_EMPTY_CTYPE->bits = 0;
   PHAMT_EMPTY_CTYPE->flag_transient = 0;
   PHAMT_EMPTY_CTYPE->flag_firstn = 0;
   PHAMT_EMPTY_CTYPE->flag_pyobject = 0;
   PHAMT_EMPTY_CTYPE->addr_startbit = HASH_BITCOUNT - PHAMT_ROOT_SHIFT;
   PHAMT_EMPTY_CTYPE->addr_shift = PHAMT_ROOT_SHIFT;
   PHAMT_EMPTY_CTYPE->addr_depth = 0;
   PyObject_GC_Track(PHAMT_EMPTY_CTYPE);
   // We don't add this one to the type's dictionary--it's for C use only.
   // The PHAMT type.
   if (PyModule_AddObject(m, "PHAMT", (PyObject*)&PHAMT_type) < 0) {
      Py_DECREF(&PHAMT_type);
      return NULL;
   }
   // Debugging things that are useful to print.
   dbgmsg("Initialized PHAMT C API.\n"
          "    PHAMT_SIZE: %u\n",
          (unsigned)PHAMT_SIZE);
   // Return the module!
   return m;
}

//------------------------------------------------------------------------------
// Constructors
// All of the constructors in this section make an attemt to correctly handle
// the ref-counting associated with copying nodes. Please be careful with this!

// phamt_new(ncells)
// Create a new PHAMT with a single key-value pair.
static PHAMT_t phamt_new(unsigned ncells)
{
   return (PHAMT_t)PyObject_GC_NewVar(struct PHAMT, &PHAMT_type, ncells);
}
// phamt_empty_like(node)
// Returns the empty PHAMT (caller obtains the reference).
// The empty PHAMT is like the given node in terms of flags (excepting
// the transient flag).
// This function increments the empty object's refcount.
PHAMT_t phamt_empty_like(PHAMT_t like)
{
   if (like == NULL || like->flag_pyobject)
      RETURN_EMPTY;
   else
      RETURN_EMPTY_CTYPE;
}
PHAMT_t phamt_empty(void)
{
   RETURN_EMPTY;
}
PHAMT_t phamt_empty_ctype(void)
{
   RETURN_EMPTY_CTYPE;
}
// phamt_from_kv(k, v, flag_pyobjbect)
// Create a new PHAMT node that holds a single key-value pair.
// The returned node is fully initialized and has had the
// PyObject_GC_Track() function already called for it.
PHAMT_t phamt_from_kv(hash_t k, void* v, uint8_t flag_pyobject)
{
   PHAMT_t node = phamt_new(1);
   node->bits = (BITS_ONE << (k & PHAMT_TWIG_MASK));
   node->address = k & ~PHAMT_TWIG_MASK;
   node->numel = 1;
   dbgmsg("[phamt_from_kv] %p -> %p (%s)\n", (void*)k, v,
          flag_pyobject ? "pyobject" : "ctype");
   node->flag_pyobject = flag_pyobject;
   node->flag_firstn = (node->bits == 1);
   node->flag_transient = 0;
   node->addr_depth = PHAMT_TWIG_DEPTH;
   node->addr_shift = PHAMT_TWIG_SHIFT;
   node->addr_startbit = 0;
   node->cells[0] = (void*)v;
   // Update that refcount and notify the GC tracker!
   if (flag_pyobject) Py_INCREF(v);
   PyObject_GC_Track((PyObject*)node);
   // Otherwise, that's all!
   return node;
}
// phamt_copy_chgcell(node)
// Creates an exact copy of the given node with a single element replaced,
// and increases all the relevant reference counts for the node's cells,
// including val.
static PHAMT_t phamt_copy_chgcell(PHAMT_t node, cellindex_t ci, void* val)
{
   PHAMT_t u;
   bits_t ncells = Py_SIZE(node);
   u = phamt_new(ncells);
   u->address = node->address;
   u->bits = node->bits;
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = node->flag_firstn;
   u->flag_transient = 0;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   memcpy(u->cells, node->cells, sizeof(void*)*ncells);
   // Change the relevant cell.
   u->cells[ci.cellindex] = val;
   // Increase the refcount for all these cells!
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject) {
      bits_t ii;
      for (ii = 0; ii < ncells; ++ii)
         Py_INCREF((PyObject*)u->cells[ii]);
   }
   return u;
}
// phamt_copy_addcell(node, cellinfo)
// Creates a copy of the given node with a new cell inserted at the appropriate
// position and the bits value updated; increases all the relevant
// reference counts for the node's cells. Does not update numel or initiate the
// new cell bucket itself.
// The refcount on val is incremented.
static PHAMT_t phamt_copy_addcell(PHAMT_t node, cellindex_t ci, void* val)
{
   PHAMT_t u;
   bits_t ncells = phamt_cellcount(node);
   dbgnode("[phamt_copy_addcell]", node);
   dbgci("[phamt_copy_addcell]", ci);
   u = phamt_new(ncells + 1);
   u->address = node->address;
   u->bits = node->bits | (BITS_ONE << ci.bitindex);
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = is_firstn(u->bits);
   u->flag_transient = 0;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   // If node and u have different firstn flags, then cellindex may not be
   // correct here.
   if (u->flag_firstn != node->flag_firstn)
      ci = phamt_cellindex(u, ((hash_t)ci.bitindex) << node->addr_startbit); 
   memcpy(u->cells, node->cells, sizeof(void*)*ci.cellindex);
   memcpy(u->cells + ci.cellindex + 1,
          node->cells + ci.cellindex,
          sizeof(void*)*(ncells - ci.cellindex));
   u->cells[ci.cellindex] = val;
   // Increase the refcount for all these cells!
   ++ncells;
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject) {
      bits_t ii;
      for (ii = 0; ii < ncells; ++ii)
         Py_INCREF((PyObject*)u->cells[ii]);
   }
   return u;
}
// phamt_copy_delcell(node, cellinfo)
// Creates a copy of the given node with a cell deleted at the appropriate
// position and the bits value updated; increases all the relevant
// reference counts for the node's cells. Does not update numel.
// Behavior is undefined if there is not a bit set for the ci.
static PHAMT_t phamt_copy_delcell(PHAMT_t node, struct cellindex_data ci)
{
   PHAMT_t u;
   bits_t ncells = phamt_cellcount(node) - 1;
   if (ncells == 0) return phamt_empty_like(node);
   u = phamt_new(ncells);
   u->address = node->address;
   u->bits = node->bits & ~(BITS_ONE << ci.bitindex);
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = is_firstn(u->bits);
   u->flag_transient = 0;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   memcpy(u->cells, node->cells, sizeof(void*)*ci.cellindex);
   memcpy(u->cells + ci.cellindex,
          node->cells + ci.cellindex + 1,
          sizeof(void*)*(ncells - ci.cellindex));
   // Increase the refcount for all these cells!
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject) {
      bits_t ii;
      for (ii = 0; ii < ncells; ++ii)
         Py_INCREF((PyObject*)u->cells[ii]);
   }
   return u;
}
static PyObject* phamt_py_from_list(PyObject* self, PyObject *const *args, Py_ssize_t nargs)
{
   return NULL; // #TODO
}
void* phamt_first(PHAMT_t node, struct PHAMTIter* iter)
{
   uint8_t d;
   if (node->numel == 0) {
      iter->found = 0;
      return NULL;
   }
   // Just dig down the first child as far as possible
   d = 0;
   iter->node[0] = NULL;
   do {
      iter->updepth[node->addr_depth] = d;
      d = node->addr_depth;
      iter->node[d] = node;
      iter->cellindex[d] = 0;
      node = (PHAMT_t)node->cells[0];
   } while (d < PHAMT_TWIG_DEPTH);
   iter->found = 1;
   return node;
}
void* phamt_next(PHAMT_t node, struct PHAMTIter* iter)
{
   uint8_t d, ci;
   if (!iter->found) return NULL;
   // We always start at twig depth; 
   d = PHAMT_TWIG_DEPTH;
   node = iter->node[d];
   do {
      ci = iter->cellindex[d];
      ++ci;
      if (ci < phamt_cellcount(node)) {
         // We've found a point at which we can descend.
         iter->cellindex[d] = ci;
         node = node->cells[ci];
         while (d < PHAMT_TWIG_DEPTH) {
            iter->updepth[node->addr_depth] = d;
            d = node->addr_depth;
            iter->node[d] = node;
            iter->cellindex[d] = 0;
            node = (PHAMT_t)node->cells[0];
         }
         return node;
      } else if (d == 0) {
         break;
      } else {
         d = iter->updepth[d];
         node = iter->node[d];
      }
   } while (node);
   // If we reach this point, we didn't find anything.
   iter->found = 0;
   return NULL;
}


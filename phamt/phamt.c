////////////////////////////////////////////////////////////////////////////////
// phamt/phamt.c
// Implemntation of the core phamt C data structures.
// by Noah C. Benson

// (This is used by the phamt.h header file.)
#define __phamt_phamt_c_290301a044d09f4211d799b982b25688

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <Python.h>
#include "phamt.h"

// Some configuration ----------------------------------------------------------

#define MAX_64BIT 0xffffffffffffffff
// Possibly, the uint128_t isn't defined, but could be...
#ifndef uint128_t
#  if defined ULLONG_MAX && (ULLONG_MAX >> 64 == MAX_64BIT)
typedef unsigned long long uint128_t;
#  endif
#endif
// Handy constant values.
const hash_t HASH_ZERO = ((hash_t)0);
const hash_t HASH_ONE = ((hash_t)1);
// The bits type is also defined here.
typedef uint32_t bits_t;
#define BITS_BITCOUNT 32
#define BITS_MAX 0xffffffff
// We use a constant shift of 5 throughout except at the root node (which can't
// be shifted at 5 due to how the bits line-up.
#define PHAMT_NODE_SHIFT 5
#define PHAMT_TWIG_SHIFT 5
// Number of bits in the total addressable hash-space of a PHAMT.
// Only certain values are supported. The popcount, clz, and ctz functions are
// defined below.
#define popcount_bits popcount32
#define clz_bits      clz32
#define ctz_bits      ctz32
#if   (HASH_MAX >> 16 == 0)
#     define HASH_BITCOUNT    16
#     define PHAMT_ROOT_SHIFT 1
#     define popcount_hash    popcount16
#     define clz_hash         clz16
#     define ctz_hash         ctz16
#elif (HASH_MAX >> 32 == 0)
#     define HASH_BITCOUNT    32
#     define PHAMT_ROOT_SHIFT 2
#     define popcount_hash    popcount32
#     define clz_hash         clz32
#     define ctz_hash         ctz32
#elif (HASH_MAX >> 64 == 0)
#     define HASH_BITCOUNT    64
#     define PHAMT_ROOT_SHIFT 4
#     define popcount_hash    popcount64
#     define clz_hash         clz64
#     define ctz_hash         ctz64
#elif (HASH_MAX >> 128 == 0)
#     define HASH_BITCOUNT    128
#     define PHAMT_ROOT_SHIFT 3
#     define popcount_hash    popcount128
#     define clz_hash         clz128
#     define ctz_hash         ctz128
#else
#     error unhandled size for hash_t
#endif
#define PHAMT_ROOT_FIRSTBIT (HASH_BITCOUNT - PHAMT_ROOT_SHIFT)
#define PHAMT_ROOT_NCHILD   (1 << PHAMT_ROOT_SHIFT)
#define PHAMT_NODE_NCHILD   (1 << PHAMT_NODE_SHIFT)
#define PHAMT_TWIG_NCHILD   (1 << PHAMT_TWIG_SHIFT)
// We need to make sure the value we get for the bits_t is safe!
#define PHAMT_NODE_BITS     (HASH_BITCOUNT - PHAMT_ROOT_SHIFT - PHAMT_TWIG_SHIFT)
#define PHAMT_NODE_LEVELS   (PHAMT_NODE_BITS / PHAMT_NODE_SHIFT)
#define PHAMT_LEVELS        (PHAMT_NODE_LEVELS + 2)
#define PHAMT_ROOT_DEPTH    0
#define PHAMT_TWIG_DEPTH    (PHAMT_ROOT_DEPTH + PHAMT_NODE_LEVELS + 1)
#define PHAMT_LEAF_DEPTH    (PHAMT_TWIG_DEPTH + 1)
const bits_t BITS_ZERO    = (bits_t)0;
const bits_t BITS_ONE     = (bits_t)1;
#define PHAMT_DOCSTRING                                                         \
   ("A Persistent Hash Array Mapped Trie (PHAMT) type.\n"                       \
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
    "   `PHAMT` objects, such as the `PHAMT.EMPTY` object, which represents\n"  \
    "   an empty `PHAMT`;\n"                                                    \
    " * by supplying the `PHAMT.from_kvs(iter_of_kv_pairs)` with an iterable\n" \
    "   collection of key-value tuples; or\n"                                   \
    " * by supplying the `PHAMT.from_seq(iter_of_values)` with a list of\n"     \
    "   values, which are assigned the keys `0`, `1`, `2`, etc.\n")


//------------------------------------------------------------------------------
// Utility functions.
// Functions for making masks.

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
// (1) Core PHAMT implementation.
struct PHAMT {
   // The Python stuff.
   PyObject_VAR_HEAD
   // The node's address in the PHAMT.
   hash_t address;
   // The bitmask of children.
   bits_t bits;
   // The number of leaves beneath this node.
   hash_t numel;
   // And the variable-length list of children.
   void* cells[];
};
#define PHAMT_SIZE sizeof(struct PHAMT)
static PHAMT_t PHAMT_EMPTY = NULL;
#define RETURN_EMPTY ({Py_INCREF(PHAMT_EMPTY); return PHAMT_EMPTY;})
// A type foor passing information about cells.
typedef struct cellindex_data {
   uint8_t bitindex;
   uint8_t cellindex;
} cellindex_t;

// We're going to define these constructors later when we have defined the type.
PHAMT_t phamt_new(unsigned ncells);
PHAMT_t phamt_from_kv(hash_t k, PyObject* v);
PHAMT_t phamt_copy_chgcell(PHAMT_t node, cellindex_t ci, void* val);
PHAMT_t phamt_copy_addcell(PHAMT_t node, cellindex_t ci, void* val);
PHAMT_t phamt_copy_delcell(PHAMT_t node, cellindex_t ci);
// Additional constructors that are part of the Python interface.
PyObject* phamt_from_list(PyObject* self, PyObject *const *args, Py_ssize_t nargs);

// Get the number of cells (not the number of elements).
inline bits_t phamt_cellcount(PHAMT_t u)
{
   return popcount_bits(u->bits);
}
// phamt_cellindex(nodeid, bits, leafid, cellindex_data_ptr)
// Yields a boolean indicating whether a node containing the given leafid or the
// leaf itself is a child of the given node, and sets the bitindex an cellindex
// fields of the cellindex_data_ptr referent. In this structure, bitindex is the
// index into the node's bits integer, and cellindex is the index into the
// node's cell array where that child is or would be found. If the leafid is
// outside of the given subtreee (i.e., it cannot exist beneath this node) then
// the cellindex returned returned is 0, but the bitindex will still match the
// appropriate shift for the node's depth.
inline hash_t phamt_cellindex(hash_t id, bits_t bits, hash_t leafid,
                              struct cellindex_data* ci) {
   // Check that the leaf is below this leaf.
   hash_t bit0, shift, depth = phamt_depth(id);
   bit0 = depth_to_startbit(depth);
   shift = depth_to_shift(depth);
   // Grab the index out of the leaf id.
   ci->bitindex = (leafid >> bit0) & lowmask_hash(shift);
   if (phamt_isbeneath(id, leafid)) {
      // Get the cellindex.
      ci->cellindex = popcount_bits(bits & lowmask_bits(ci->bitindex));
      // The return value just depends on whether the bit is set.
      return bits & (BITS_ONE << ci->bitindex);
   } else {
      ci->cellindex = 0xff;
      return 0;
   }
}
inline hash_t phamt_cellindex_nocheck(hash_t id, bits_t bits, hash_t leafid,
                                      struct cellindex_data* ci) {
   hash_t bit0, shift, depth = phamt_depth(id);
   bit0 = depth_to_startbit(depth);
   shift = depth_to_shift(depth);
   // Grab the index out of the leaf id. (We don't need leafid after
   // this, so we upcycle it into is_present.)
   ci->bitindex = (leafid >> bit0) & lowmask_hash(shift);
   // Get the cellindex.
   ci->cellindex = popcount_bits(bits & lowmask_bits(ci->bitindex));
   // The return value just depends on whether the bit is set.
   return bits & (BITS_ONE << ci->bitindex);
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
PyObject* phamt_lookup(PHAMT_t node, hash_t k)
{
   struct cellindex_data ci;
   hash_t r;
   if (!node->bits) return NULL;
   do {
      r = phamt_cellindex(node->address, node->bits, k, &ci);
      if (!r) return NULL;
      r = phamt_depth(node->address);
      node = (PHAMT_t)node->cells[ci.cellindex];
   } while (r != PHAMT_TWIG_DEPTH);
   return (PyObject*)node;
}
// phamt_assoc(node, k, v)
// Yields a copy of the given PHAMT with the new key associated. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_assoc(PHAMT_t node, hash_t k, PyObject* v)
{
   struct cellindex_data ci;
   PHAMT_t kv, u;
   hash_t h, bit0, shift, depth, newdepth;
   // If the node is empty, we just return a new node. This auto-updates the
   // refcount for v.
   if (node->numel == 0) return phamt_from_kv(k, v);
   depth = phamt_depth(node->address);
   bit0 = depth_to_startbit(depth);
   shift = depth_to_shift(depth);
   // Is the key beneath this node or not?
   h = phamt_cellindex(node->address, node->bits, k, &ci);
   if (h) {
      // This means that the bit is set, so we need to either update a leaf or
      // continue to search down for a twig.
      if (depth == PHAMT_TWIG_DEPTH) {
         // We'e replacing a leaf. Go ahead and alloc a copy (this increases
         // the refcount for everything, so we need to deref the old child.
         PyObject* obj = (PyObject*)node->cells[ci.cellindex];
         if (obj == v) {
            Py_INCREF(node);
            return node;
         }
         u = phamt_copy_chgcell(node, ci, (void*)v);
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
   } else if (ci.cellindex < 0xff) {
      // The key is beneath this node, so we will dig down until we find the
      // correct place for this key.
      // Okay, is the node a twig or not?
      if (depth == PHAMT_TWIG_DEPTH) {
         // We're adding a new leaf. This updates refcounts for everything
         // except the replaced cell.
         u = phamt_copy_addcell(node, ci, (void*)v);
         u->numel = node->numel + 1;
      } else {
         // We are not a twig.
         // We are adding a twig node containing just the leaf below us. This
         // copy function auto-updates the refcounts for all children.
         kv = phamt_from_kv(k, v);
         u = phamt_copy_addcell(node, ci, (void*)kv);
         ++(u->numel);
         Py_DECREF((PyObject*)kv);
      }
   } else {
      // The key isn't beneath this node, so we need to make a higher up node
      // that links to both the key and node.
      // The key and value will go in their own node, regardless.
      kv = phamt_from_kv(k, v);
      // What's the highest bit at which they differ?
      h = highmask_hash(bit0 + shift);
      h = phamt_highbitdiff_hash(node->address & h, k & h);
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
      u->numel = 1 + node->numel;
      // We use h to store the new minleaf value.
      h = node->address & ~((HASH_ONE << (bit0 + shift)) - HASH_ONE);
      u->address = phamt_id(h, newdepth);
      u->bits = 0;
      h = (HASH_ONE << shift) - HASH_ONE;
      u->bits |= BITS_ONE << (h & (node->address >> bit0));
      u->bits |= BITS_ONE << (h & (k >> bit0));
      if (k < node->address) {
         u->cells[0] = (void*)kv;
         u->cells[1] = (void*)node;
      } else {
         u->cells[0] = (void*)node;
         u->cells[1] = (void*)kv;
      }
      // We refcount node (from u), and pass kv's refcount ownership to u.
      Py_INCREF(node);
   }
   // We need to register the new node u with the garbage collector.
   PyObject_GC_Track((PyObject*)u);
   // Return the new PHAMT object!
   return (PHAMT_t)u;
}
// phamt_dissoc(node, k)
// Yields a copy of the given PHAMT with the given key removed. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_dissoc(PHAMT_t node, hash_t k)
{
   PHAMT_t u;
   struct cellindex_data ci;
   hash_t depth, h = phamt_cellindex(node->address, node->bits, k, &ci);
   // If the node lies outside of this node or the bit isn't set, there's
   // nothing to do.
   if (!h) {
      Py_INCREF(node);
      return node;
   }
   // There's something to delete; start by determining if we are or are not a
   // twig node:
   depth = phamt_depth(node->address);
   if (depth == PHAMT_TWIG_DEPTH) {
      // If we are deleting the only element, we return a new empty node.
      if (node->numel == 1) RETURN_EMPTY;
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
         if (node->numel == 1) return newcell;
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
static PyObject* phamt_py_items(PyObject* self)
{
   return NULL; // #TODO
}
static PyObject* phamt_py_keys(PyObject* self)
{
   return NULL; // #TODO
}
static PyObject* phamt_py_values(PyObject* self)
{
   return NULL; // #TODO
}
static PyObject *PHAMT_py_getitem(PyObject *type, PyObject *item)
{
   Py_INCREF(type);
   return type;
}
static PyObject* phamt_py_reduce(PyObject* self)
{
   return NULL; // #TODO
}
static PyObject* phamt_py_dump(PyObject* self)
{
   return NULL; // #TODO
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
static void phamt_tp_dealloc(PyObject* self)
{
   PyTypeObject* tp;
   PyObject* tmp;
   bits_t ii, ncells;
   tp = Py_TYPE(self);
   ncells = phamt_cellcount((PHAMT_t)self);
   PyObject_GC_UnTrack(self);
   // Walk through the children, dereferencing them
   for (ii = 0; ii < ncells; ++ii) {
      tmp = ((PHAMT_t)self)->cells[ii];
      ((PHAMT_t)self)->cells[ii] = NULL;
      Py_DECREF(tmp);
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
   ncells = phamt_cellcount(self);
   for (ii = 0; ii < ncells; ++ii) {
      if (self->cells[ii])
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
static PyObject* phamt_tp_init(PHAMT_t self, PyObject *args, PyObject *kw)
{
   return 0; // #TODO
}
static PyObject* phamt_py_hash(PyObject* self)
{
   return NULL; // #TODO
}
#if 0
// This function is for debugging purposes only!
// There is a potential buffer overflow error.
static void phamt_format(PHAMT_t node, char* buf)
{
   bits_t ci, bi, ncells, bits;
   hash_t depth;
   depth = phamt_depth(node->address);
   ncells = phamt_cellcount(node);
   bits = node->bits;
   for (ci = 0; ci < ncells; ++ci) {
      bi = ctz_bits(bits);
      bits &= ~(BITS_ONE << bi);
      if (depth == PHAMT_TWIG_DEPTH) {
         sprintf(buf, "%s%s%u ",
                 buf, (*buf ? ", " : "<|"),
                 (unsigned)bi);
      } else {
         sprintf(buf, "%s%s%u:",
                 buf, (*buf ? ", " : "<|"),
                 (unsigned)bi);
         phamt_format(node->cells[ci], buf + strlen(buf));
      }
   }
   if (ncells == 0) sprintf(buf, "<|");
   sprintf(buf, "%s; d=%u, id=%p|>",
           buf, (unsigned)depth, (void*)(node->address & ~PHAMT_DEPTH_MASK_HASH));
}
static PyObject* phamt_py_repr(PHAMT_t self)
{
   static char buf[1024*16];
   *buf = 0;
   phamt_format(self, buf);
   return PyUnicode_FromFormat("%s", buf);
}
#else
static PyObject* phamt_py_repr(PHAMT_t self)
{
   return PyUnicode_FromFormat("<PHAMT:n=%u>", (unsigned)self->numel);
}
#endif
static void phamt_module_free(void* mod)
{
   PHAMT_t tmp = PHAMT_EMPTY;
   PHAMT_EMPTY = NULL;
   Py_DECREF(tmp);
}

static PyMethodDef PHAMT_methods[] = {
   {"assoc", (PyCFunction)phamt_py_assoc, METH_VARARGS, NULL},
   {"dissoc", (PyCFunction)phamt_py_dissoc, METH_VARARGS, NULL},
   {"get", (PyCFunction)phamt_py_get, METH_VARARGS, NULL},
   {"items", (PyCFunction)phamt_py_items, METH_NOARGS, NULL},
   {"keys", (PyCFunction)phamt_py_keys, METH_NOARGS, NULL},
   {"values", (PyCFunction)phamt_py_values, METH_NOARGS, NULL},
   //{"update", (PyCFunction)phamt_py_update, METH_VARARGS | METH_KEYWORDS, NULL},
   {"__class_getitem__", (PyCFunction)PHAMT_py_getitem, METH_O|METH_CLASS, NULL},
   // For JSON/pickle serialization.
   {"__reduce__", (PyCFunction)phamt_py_reduce, METH_NOARGS, NULL},
   {"__dump__", (PyCFunction)phamt_py_dump, METH_NOARGS, NULL},
   {"from_list", (PyCFunction)phamt_from_list, METH_FASTCALL,
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
   .tp_init = (initproc)phamt_tp_init,
   .tp_hash = (hashfunc)phamt_py_hash,
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
   PHAMT_EMPTY = phamt_new(0);
   PHAMT_EMPTY->address = 0;
   PHAMT_EMPTY->numel = 0;
   PHAMT_EMPTY->bits = 0;
   PyObject_GC_Track(PHAMT_EMPTY);
   // Add empty to the type's dictionary.
   PyDict_SetItemString(PHAMT_type.tp_dict, "empty", (PyObject*)PHAMT_EMPTY);
   // The PHAMT type.
   if (PyModule_AddObject(m, "PHAMT", (PyObject*)&PHAMT_type) < 0) {
      Py_DECREF(&PHAMT_type);
      return NULL;
   }
   // Return the module!
   return m;
}

//------------------------------------------------------------------------------
// Constructors
// All of the constructors in this section make an attemt to correctly handle
// the ref-counting associated with copying nodes. Please be careful with this!

// phamt_new(ncells)
// Create a new PHAMT with a single key-value pair.
PHAMT_t phamt_new(unsigned ncells)
{
   PHAMT_t u;
   if (ncells == 0) RETURN_EMPTY;
   u = (PHAMT_t)PyObject_GC_NewVar(struct PHAMT, &PHAMT_type, ncells);
   if (!u) return NULL;
   //Py_SET_SIZE(u, ncells);
   return u;
}
// phamt_empty()
// Returns the empty PHAMT (caller obtains the reference).
PHAMT_t phamt_empty(void)
{
   RETURN_EMPTY;
}
// phamt_from_kv(k, v)
// Create a new PHAMT node that holds a single key-value pair.
// The returned node is fully initialized and has had the
// PyObject_GC_Track() function already called for it.
PHAMT_t phamt_from_kv(hash_t k, PyObject* v)
{
   PHAMT_t node = phamt_new(1);
   bits_t twigmask = (BITS_ONE << PHAMT_TWIG_SHIFT) - BITS_ONE;
   node->bits = (BITS_ONE << (k & twigmask));
   node->address = phamt_id(k & ~PHAMT_DEPTH_MASK_HASH, PHAMT_TWIG_DEPTH);
   node->numel = 1;
   node->cells[0] = (void*)v;
   // Update that refcount and notify the GC tracker!
   Py_INCREF(v);
   PyObject_GC_Track((PyObject*)node);
   // Otherwise, that's all!
   return node;
}
// phamt_copy_chgcell(node)
// Creates an exact copy of the given node with a single element replaced,
// and increases all the relevant reference counts for the node's cells,
// including val.
PHAMT_t phamt_copy_chgcell(PHAMT_t node, struct cellindex_data ci, void* val)
{
   PHAMT_t u;
   bits_t ii, ncells;
   ncells = phamt_cellcount(node);
   u = phamt_new(ncells);
   u->address = node->address;
   u->bits = node->bits;
   u->numel = node->numel;
   memcpy(u->cells, node->cells, sizeof(void*)*ncells);
   // Change the relevant cell.
   u->cells[ci.cellindex] = val;
   // Increase the refcount for all these cells!
   for (ii = 0; ii < ncells; ++ii)
      Py_INCREF((PyObject*)u->cells[ii]);
   return u;
}
// phamt_copy_addcell(node, cellinfo)
// Creates a copy of the given node with a new cell inserted at the appropriate
// position and the bits value updated; increases all the relevant
// reference counts for the node's cells. Does not update numel or initiate the
// new cell bucket itself.
// The refcount on val is incremented.
PHAMT_t phamt_copy_addcell(PHAMT_t node, struct cellindex_data ci, void* val)
{
   PHAMT_t u;
   bits_t ii, ncells;
   ncells = phamt_cellcount(node);
   u = phamt_new(ncells + 1);
   u->address = node->address;
   u->bits = node->bits | (BITS_ONE << ci.bitindex);
   u->numel = node->numel;
   memcpy(u->cells, node->cells, sizeof(void*)*ci.cellindex);
   memcpy(u->cells + ci.cellindex + 1,
          node->cells + ci.cellindex,
          sizeof(void*)*(ncells - ci.cellindex));
   u->cells[ci.cellindex] = val;
   // Increase the refcount for all these cells!
   ++ncells;
   for (ii = 0; ii < ncells; ++ii)
      Py_INCREF((PyObject*)u->cells[ii]);
   return u;
}
// phamt_copy_delcell(node, cellinfo)
// Creates a copy of the given node with a cell deleted at the appropriate
// position and the bits value updated; increases all the relevant
// reference counts for the node's cells. Does not update numel.
// Behavior is undefined if there is not a bit set for the ci.
PHAMT_t phamt_copy_delcell(PHAMT_t node, struct cellindex_data ci)
{
   PHAMT_t u;
   bits_t ii, ncells;
   ncells = phamt_cellcount(node) - 1;
   if (ncells == 0) RETURN_EMPTY;
   u = phamt_new(ncells);
   u->address = node->address;
   u->bits = node->bits & ~(BITS_ONE << ci.bitindex);
   u->numel = node->numel;
   memcpy(u->cells, node->cells, sizeof(void*)*ci.cellindex);
   memcpy(u->cells + ci.cellindex,
          node->cells + ci.cellindex + 1,
          sizeof(void*)*(ncells - ci.cellindex));
   // Increase the refcount for all these cells!
   for (ii = 0; ii < ncells; ++ii)
      Py_INCREF((PyObject*)u->cells[ii]);
   // Set the bits flag appropriately.
   u->bits &= ~(BITS_ONE << ci.bitindex);
   return u;
}

//------------------------------------------------------------------------------
// Constructors (from an interator or array).
PyObject* phamt_from_list(PyObject* self, PyObject *const *args, Py_ssize_t nargs)
{
   return NULL; // #TODO
}

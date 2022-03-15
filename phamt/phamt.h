////////////////////////////////////////////////////////////////////////////////
// phamt/phamt.h
// Definitions for the core phamt C data structures.
// by Noah C. Benson

#ifndef __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210
#define __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210

//------------------------------------------------------------------------------
// Type Declaration and Configuration

// The type of the hash.
// Python itself uses signed integers for hashes (Py_hash_t), but we want to
// make sure our code uses unsigned integers internally.
// In this case, the size_t type is the same size as Py_hash_t (which is just
// defined from Py_ssize_t, which in turn is defined from ssize_t), but size_t
// is also unsigned, so we use it.
typedef size_t hash_t;
#define HASH_MAX SIZE_MAX
// Figure out what size the hash actually is and define some things based on it.
#define MAX_16BIT  0xffff
#define MAX_32BIT  0xffffffff
#define MAX_64BIT  0xffffffffffffffff
#define MAX_128BIT 0xffffffffffffffffffffffffffffffff
#if   (HASH_MAX == MAX_16BIT)
#     define HASH_BITCOUNT 16
#     define PHAMT_ROOT_SHIFT 1
#elif (HASH_MAX == MAX_32BIT)
#     define HASH_BITCOUNT 32
#     define PHAMT_ROOT_SHIFT 2
#elif (HASH_MAX == MAX_64BIT)
#     define HASH_BITCOUNT 64
#     define PHAMT_ROOT_SHIFT 4
#elif (HASH_MAX == MAX_128BIT)
#     define HASH_BITCOUNT 128
#     define PHAMT_ROOT_SHIFT 3
#else
#     error unhandled size for hash_t
#endif
// Handy constant values.
#define HASH_ZERO     ((hash_t)0)
#define HASH_ONE      ((hash_t)1)
// The bits type is also defined here.
typedef uint32_t bits_t;
#define BITS_BITCOUNT 32
#define BITS_MAX      0xffffffff
#define BITS_ZERO     ((bits_t)0)
#define BITS_ONE      ((bits_t)1)
// We use a constant shift of 5 throughout except at the root node (which can't
// be shifted at 5 due to how the bits line-up.
#define PHAMT_NODE_SHIFT 5
#define PHAMT_TWIG_SHIFT 5
// Some consequences of the above definitions
#define PHAMT_ROOT_FIRSTBIT (HASH_BITCOUNT - PHAMT_ROOT_SHIFT)
#define PHAMT_ROOT_NCHILD   (1 << PHAMT_ROOT_SHIFT)
#define PHAMT_NODE_NCHILD   (1 << PHAMT_NODE_SHIFT)
#define PHAMT_TWIG_NCHILD   (1 << PHAMT_TWIG_SHIFT)
#define PHAMT_NODE_BITS     (HASH_BITCOUNT - PHAMT_ROOT_SHIFT - PHAMT_TWIG_SHIFT)
#define PHAMT_NODE_LEVELS   (PHAMT_NODE_BITS / PHAMT_NODE_SHIFT)
#define PHAMT_LEVELS        (PHAMT_NODE_LEVELS + 2)
#define PHAMT_ROOT_DEPTH    0
#define PHAMT_TWIG_DEPTH    (PHAMT_ROOT_DEPTH + PHAMT_NODE_LEVELS + 1)
#define PHAMT_LEAF_DEPTH    (PHAMT_TWIG_DEPTH + 1)
#define PHAMT_TWIG_MASK     ((HASH_ONE << PHAMT_TWIG_SHIFT) - HASH_ONE)
// The typedef for the PHAMT exists here, but the type isn't defined in the
// header--this is just to encapsulate the immutable data.
typedef struct PHAMT* PHAMT_t;
// Additionally an index and iterator type for the PHAMT. This is not a python
// iterator--it is a C iterator type.
typedef struct {
   uint8_t bitindex;   // the bit index of the node
   uint8_t cellindex;  // the cell index of the node
   uint8_t is_beneath; // whether the key is beneath this node
   uint8_t is_found;   // whether the bit for the key is set
} PHAMTIndex_t;
typedef struct {
   PHAMT_t      node;  // The node that this location refers to.
   PHAMTIndex_t index; // The cell-index that this location refers to.
} PHAMTLoc_t;
typedef struct {
   // PHAMT_LEVELS is guaranteed to be enough space for any search; however
   // we need to store some information about the original query and/or the
   // starting point, so we actually allocate PHAMT_LEVELS+1; (the final level
   // is just stored in the variable start).
   // The steps along the path include both a node and an index each; in the
   // indices, however, we slightly re-interpret the meaning of a few members,
   // particularly is_beneath:
   //  - steps[d].node is the node at depth d on the search (if there is no
   //    depth d, then the values at steps[d] are all undefined).
   //  - steps[d].index.is_found is either 0 or 1. If the subindex was found
   //    at this depth (i.e., the requested element is beneath the node at depth
   //    d), then is_found is 1; if the subindex is not beneath this node at
   //    all, then is_found is 0; otherwise, if the requested element is beneath
   //    the node at depth d but is not found in it, then is_found is 0, but the
   //    edit_depth of the overall path will not be equal to the min_depth (see
   //    below).
   //  - steps[d].index.is_beneath is the depth one level up from the depth d
   //    in the original node/tree.
   //  - If steps[d].index.is_beneath is greater than PHAMT_TWIG_DEPTH (0xff),
   //    then the node is the root of the tree.
   PHAMTLoc_t steps[PHAMT_LEVELS];
   // The additional data stores some info about the search:
   //  - min_depth is the depth if the *first* node on the path (i.e., the node
   //    in which the search was initiated).
   //  - max_depth is the depth of the *final* node on the path. The requested
   //    node may not be beneath this node if edit_depth is not equal to
   //    min_depth.
   //  - edit_depth is the depth at which the first edit to the path should be
   //    made if the intention is to add the node to the path. This is always
   //    equal to min_depth except in the case that the value being searched for
   //    is disjoint from the node at the min_depth, indicating that the search
   //    reached a depth at which the value was not beneath the subnode.
   //  - value_found is 1 if the element requested (when the path was created)
   //    was found in the original node and 0 if the requested element was not
   //    found.
   uint8_t min_depth;
   uint8_t max_depth;
   uint8_t edit_depth;
   uint8_t value_found;
} PHAMTPath_t;
// PHAMTIndex_t, PHAMTLoc_t and PHAMTPath_t are not pointers, since they should
// usually be allocated on the stack.
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
#define PHAMT_SIZE sizeof(struct PHAMT)
// Possibly, the uint128_t isn't defined, but could be...
#ifndef uint128_t
#  if defined ULLONG_MAX && (ULLONG_MAX > MAX_64BIT) && (ULLONG_MAX >> 64 == MAX_64BIT)
   typedef unsigned long long uint128_t;
#  endif
#endif
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
// Private Utility functions.
// Functions for making masks and counting bits mostly.

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
// Get the number of cells (not the number of elements).
inline bits_t phamt_cellcount(PHAMT_t u)
{
   return (bits_t)Py_SIZE(u);
}
// phamt_cellindex(node, leafid)
// Yields a PHAMTIndex_t structure that indicates whether and where the leafid is
// with respect to node.
inline PHAMTIndex_t phamt_cellindex(PHAMT_t node, hash_t leafid)
{
   PHAMTIndex_t ci;
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


//------------------------------------------------------------------------------
// Public API
// These functions are part of the public PHAMT C API; they can be used to
// create and edit PHAMTs.

// phamt_empty()
// Returns the empty PHAMT object; caller obtains the reference.
// This function increments the empty object's refcount.
PHAMT_t phamt_empty(void);
// phamt_empty()
// Returns the empty CTYPE PHAMT object; caller obtains the reference. PHAMTs
// made using C types don't reference-count their values, so, while they are
// Python objects, it is not safe to use them with Python code. Rather, they are
// intended to support internal PHAMTs for persistent data structures
// implemented in C that need to store, for example, ints as values.
// This function increments the empty object's refcount.
PHAMT_t phamt_empty_ctype(void);
// phamt_empty_like(node)
// Returns the empty PHAMT (caller obtains the reference).
// The empty PHAMT is like the given node in terms of flags (excepting
// the transient flag).
// This function increments the empty object's refcount.
PHAMT_t phamt_empty_like(PHAMT_t like);
// phamt_from_kv(k, v)
// Create a new PHAMT node that holds a single key-value pair.
// The returned node is fully initialized and has had the
// PyObject_GC_Track() function already called for it.
// The argument flag_pyobject should be 1 if v is a Python object and 0 if
// it is not (this determines whether the resulting PHAMT is a Python PHAMT
// or a c-type PHAMT).
PHAMT_t phamt_from_kv(hash_t k, void* v, uint8_t flag_pyobject);
// phamt_lookup(node, k)
// Yields the leaf value for the hash k. If no such key is in the phamt, then
// NULL is returned.
// This function does not deal at all with INCREF or DECREF, so before returning
// anything returned from this function back to Python, be sure to INCREF it.
// If the pointer found is provided, then it is set to 1 if the key k was found
// and 0 if it was not; this allows for disambiguation when NULL is a valid
// value (for a ctype PHAMT).
void* phamt_lookup(PHAMT_t node, hash_t k, int* found);
// phamt_find(node, k, path)
// Finds and returns the value associated with the given key k in the given
// node. Update the given path-object in order to indicate where in the node
// the key lies.
void* phamt_find(PHAMT_t node, hash_t k, PHAMTPath_t* path);
// phamt_assoc(node, k, v)
// Yields a copy of the given PHAMT with the new key associated. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_assoc(PHAMT_t node, hash_t k, void* v);
// phamt_dissoc(node, k)
// Yields a copy of the given PHAMT with the given key removed. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_dissoc(PHAMT_t node, hash_t k);
// phamt_apply(node, h, fn, arg)
// Applies the given function to the value with the given hash h. The function
// is called as fn(uint8_t found, void** value, void* arg); it will always be
// safe to set *value. If found is 0, then the node was not found, and if it is
// 1 then it was found. If fn returns 0, then the hash should be removed from
// the PHAMT; if 1 then the value stored in *value should be added or should
// replace the value mapped to h.
// The updated PHAMT is returned.
typedef uint8_t (*phamtfn_t)(uint8_t found, void** value, void* arg);
PHAMT_t phamt_apply(PHAMT_t node, hash_t k, phamtfn_t fn, void* arg);
// phamt_first(node, iter)
// Returns the first item in the phamt node and sets the iterator accordingly.
// If the depth in the iter is ever set to 0 when this function returns, that
// means that there are no items to iterate (the return value will also be
// NULL in this case). No refcounting is performed by this function.
void* phamt_first(PHAMT_t node, PHAMTPath_t* iter);
// phamt_next(node, iter)
// Returns the next item in the phamt node and updates the iterator accordingly.
// If the depth in the iter is ever set to 0 when this function returns, that
// means that there are no items to iterate (the return value will also be
// NULL in this case). No refcounting is performed by this function.
void* phamt_next(PHAMT_t node, PHAMTPath_t* iter);


#endif // ifndef __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210

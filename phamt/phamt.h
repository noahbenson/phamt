////////////////////////////////////////////////////////////////////////////////
// phamt/phamt.h
// Definitions for the core phamt C data structures.
// by Noah C. Benson

#ifndef __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210
#define __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210

//==============================================================================
// Required header files.

#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif


//==============================================================================
// Configuration.
// This library was written with a 64-bit unsigned integer hash type in mind.
// However, Python does not guarantee that the C compiler and backend will use
// 64 bits; accordingly, we have to perform some amount of configuration
// regarding the size of integers and the functions/macros that handle them.

//------------------------------------------------------------------------------
// The docstring:
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
// hash_t and bits_t
// This subsection of the configuration defines data related to the hash and
// bits types as well as configurations related to the PHAMT type's nodes (e.g.,
// which bits go with which node depth).

// Python itself uses signed integers for hashes (Py_hash_t), but we want to
// make sure our code uses unsigned integers internally.
// In this case, the size_t type is the same size as Py_hash_t (which is just
// defined from Py_ssize_t, which in turn is defined from ssize_t), but size_t
// is also unsigned, so we use it.
typedef size_t hash_t;
// The max value of the hash is the same as the max value of a site_t integer.
#define HASH_MAX SIZE_MAX
// These are useful constants: 0 and 1 for the hash type.
#define HASH_ZERO ((hash_t)0)
#define HASH_ONE  ((hash_t)1)

// We now need to figure out what size the hash actually is and define some
// values based on it its size.
//  - The MAX_*BIT values are defined then undefined later in this file (they
//    are just used for clarity in this configuration section.
#define MAX_16BIT  0xffff
#define MAX_32BIT  0xffffffff
#define MAX_64BIT  0xffffffffffffffff
#define MAX_128BIT 0xffffffffffffffffffffffffffffffff
//  - Check what size the hash is by comparing to the above max values. We
//    define the HASH_BITCOUNT and PHAMT_ROOT_SHIFT based on this. The root
//    shift is the remainder of the bitcount divided by 5.
#if   (HASH_MAX == MAX_16BIT)
#   define HASH_BITCOUNT 16
#   define PHAMT_ROOT_SHIFT 1
#elif (HASH_MAX == MAX_32BIT)
#   define HASH_BITCOUNT 32
#   define PHAMT_ROOT_SHIFT 2
#elif (HASH_MAX == MAX_64BIT)
#   define HASH_BITCOUNT 64
#   define PHAMT_ROOT_SHIFT 4
#elif (HASH_MAX == MAX_128BIT)
#   define HASH_BITCOUNT 128
#   define PHAMT_ROOT_SHIFT 3
#else
#   error unhandled size for hash_t
#endif

// We also define the bits type. Because we use a max shift of 5, the bits type
// can always be a 32-bit unsigned integer.
typedef uint32_t bits_t;
#define BITS_BITCOUNT 32
#define BITS_MAX      (0xffffffff)
#define BITS_ZERO     ((bits_t)0)
#define BITS_ONE      ((bits_t)1)

// We use a constant shift of 5 throughout except at the root node (which can't
// generally be shifted at 5 due to how the bits line-up--it instead gets the
// number of leftover bits in the hash integer, which was defined above as
// PHAMT_ROOT_SHIFT).
#define PHAMT_NODE_SHIFT 5
#define PHAMT_TWIG_SHIFT 5
// Here we define some consequences of the above definitions, which we use
#define PHAMT_ROOT_FIRSTBIT (HASH_BITCOUNT - PHAMT_ROOT_SHIFT)
#define PHAMT_ROOT_MAXCELLS (1 << PHAMT_ROOT_SHIFT)
#define PHAMT_NODE_MAXCELLS (1 << PHAMT_NODE_SHIFT)
#define PHAMT_TWIG_MAXCELLS (1 << PHAMT_TWIG_SHIFT)
#define PHAMT_NODE_BITS     (HASH_BITCOUNT - PHAMT_ROOT_SHIFT - PHAMT_TWIG_SHIFT)
#define PHAMT_NODE_LEVELS   (PHAMT_NODE_BITS / PHAMT_NODE_SHIFT)
#define PHAMT_LEVELS        (PHAMT_NODE_LEVELS + 2) // (nodes + root + twig)
#define PHAMT_ROOT_DEPTH    0
#define PHAMT_TWIG_DEPTH    (PHAMT_ROOT_DEPTH + PHAMT_NODE_LEVELS + 1)
#define PHAMT_LEAF_DEPTH    (PHAMT_TWIG_DEPTH + 1)
#define PHAMT_ROOT_MASK     ((HASH_ONE << PHAMT_ROOT_SHIFT) - HASH_ONE)
#define PHAMT_NODE_MASK     ((HASH_ONE << PHAMT_NODE_SHIFT) - HASH_ONE)
#define PHAMT_TWIG_MASK     ((HASH_ONE << PHAMT_TWIG_SHIFT) - HASH_ONE)

//------------------------------------------------------------------------------
// Bit Operations.
// We need to define functions for performing popcount, clz, and ctz on the hash
// and bits types. Since these types aren't guaranteed to be a single size, we
// instead do some preprocessor magic to define two versions of each of these:
// a _hash and _bits version (e.g., popcount_hash, popcount_bits).

// For starters, it's possible that the uint128_t isn't defined explicitly but
// could be... if this is the case, we can go ahead and define it.
#ifndef uint128_t
#  if defined (ULLONG_MAX)                \
       && (ULLONG_MAX > MAX_64BIT)        \
       && (ULLONG_MAX >> 64 == MAX_64BIT)
      typedef unsigned long long uint128_t;
#  endif
#endif

// popcount(bits):
// Returns the number of set bits in the given bits_t unsigned integer.
// If the gcc-defined builtin popcount is available, we should use it
// because it is likely faster. However, the builtin version is
// defined interms of C types (and not interms of the number of bits
// in the type) so we need to do some preprocessor magic to make sure
// we are using the correct version of the builtin popcount.
#if defined (__builtin_popcount) && (UINT_MAX == BITS_MAX)
#   define popcount32 __builtin_popcount
#elif defined (__builtin_popcountl) && (ULONG_MAX == BITS_MAX)
#   define popcount32 __builtin_popcountl
#elif defined (ULLONG_MAX)               \
       && defined (__builtin_popcountll) \
       && (ULLONG_MAX == BITS_MAX)
#   define popcount32 __builtin_popcountll
#else
    static inline uint32_t popcount32(uint32_t w)
    {
       w = w - ((w >> 1) & 0x55555555);
       w = (w & 0x33333333) + ((w >> 2) & 0x33333333);
       w = (w + (w >> 4)) & 0x0F0F0F0F;
       return (w * 0x01010101) >> 24;
    }
#endif
// The 16-bit version can always just use the 32-bit version, above.
static inline uint16_t popcount16(uint16_t w)
{
   return popcount32((uint32_t)w);
}
// The 64-bit version will either need to use the 32-bit version or will be
// defined from a builtin gcc version.
#if   defined (__builtin_popcount) && (UINT_MAX == MAX_64BIT)
#   define popcount64 __builtin_popcount
#elif defined (__builtin_popcountl) && (ULONG_MAX == MAX_64BIT)
#   define popcount64 __builtin_popcountl
#elif defined (ULLONG_MAX)               \
       && defined (__builtin_popcountll) \
       && (ULLONG_MAX == MAX_64BBIT)
#   define popcount64 __builtin_popcountll
#else
    static inline uint64_t popcount64(uint64_t w)
    {
       return popcount32((uint32_t)w) + popcount32((uint32_t)(w >> 32));
    }
#endif
// It's unlikely there will need to be a 128-bit popcount for this library, but
// we'll just use the 32-bit version for the sake of completeness.
#ifdef uint128_t
    static inline uint64_t popcount128(uint128_t w)
    {
       return (popcount32((uint32_t)w) +
               popcount32((uint32_t)(w >> 32)) +
               popcount32((uint32_t)(w >> 64)) +
               popcount32((uint32_t)(w >> 96)))
    }
#endif

// clz(bits)
// Returns the number of leading zeros in the bits. For clz, we do not use the
// builtin functions, even if available, because they return the value 0 for the
// input of 0; this library expects an in put of 0 to return the number of bits
// in the type. Instead, we use following clever implementation of clz followed
// by some extensions of it to other integer sizes.
static inline uint32_t clz32(uint32_t v)
{
   v = v | (v >> 1);
   v = v | (v >> 2);
   v = v | (v >> 4);
   v = v | (v >> 8);
   v = v | (v >> 16);
   return popcount32(~v);
}
static inline uint16_t clz16(uint16_t w)
{
   return clz32((uint32_t)w) - 16;
}
static inline uint64_t clz64(uint64_t w)
{
   uint32_t c = clz32((uint32_t)(w >> 32));
   return (c == 32 ? 32 + clz32((uint32_t)w) : c);
}
#ifdef uint128_t
    static inline uint64_t clz128(uint128_t w)
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
// Returns the number of trailing zeros in the bits. Like with clz, we define
// our own implementation and use it with other integer sizes.
static inline uint32_t ctz32(uint32_t v)
{
   static const int deBruijn_values[32] = {
      0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
   };
   return deBruijn_values[((uint32_t)((v & -v) * 0x077CB531U)) >> 27];
}
static inline uint16_t ctz16(uint16_t w)
{
   return ctz32((uint32_t)w);
}
static inline uint64_t ctz64(uint64_t w)
{
   uint32_t c = ctz32((uint32_t)w);
   return (c == 32 ? 32 + ctz32((uint32_t)(w >> 32)) : c);
}
#ifdef uint128_t
    static inline uint128_t ctz128(uint128_t w)
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

// Finally, now that we have functions for each type defined, we can finally
// define the _hash and _bits functions.
// The bits type is actually always of size 32, so it's easy to define.
#define popcount_bits popcount32
#define clz_bits      clz32
#define ctz_bits      ctz32
// The hash functions depend on the size of the hash bitcount, though.
#if   (HASH_BITCOUNT == 16)
#   define popcount_hash    popcount16
#   define clz_hash         clz16
#   define ctz_hash         ctz16
#elif (HASH_BITCOUNT == 32)
#   define popcount_hash    popcount32
#   define clz_hash         clz32
#   define ctz_hash         ctz32
#elif (HASH_BITCOUNT == 64)
#   define popcount_hash    popcount64
#   define clz_hash         clz64
#   define ctz_hash         ctz64
#elif (HASH_BITCOUNT == 128)
#   define popcount_hash    popcount128
#   define clz_hash         clz128
#   define ctz_hash         ctz128
#else
#   error unhandled size for hash_t
#endif

// At this point, we're done with the MAX_* types, so we can undefine them.
#undef MAX_16BIT
#undef MAX_32BIT
#undef MAX_64BIT
#undef MAX_128BIT


//==============================================================================
// Type definitions.
// In this section, we define the PHAMT_t, PHAMT_index_t, PHAMT_loc_t, and
// PHAMT_path_t types.

// The PHAMT_t type is the type of a PHAMT (equivalently, of a PHAMT node).
// Note that PHAMT_t is a pointer while other types below are not; this is
// because you should always pass pointers of PHAMTs but you should pass the
// other types by value and/or allocate them on the stack.
typedef struct PHAMT {
   // The Python stuff.
   PyObject_VAR_HEAD
   // The node's address in the PHAMT.
   hash_t address;
   // The number of leaves beneath this node.
   hash_t numel;
   // The bitmask of children.
   bits_t bits;
   // What follows, between the addr_startbit and _empty members, is a
   // set of meta-data that also manages to fill in the other 32 bits
   // of the 64-bit block that started with bits.
   // v-----------------------------------------------------------------v
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
   // ^-----------------------------------------------------------------^
   // And the variable-length list of children.
   void* cells[];
} *PHAMT_t;
// The size of a PHAMT:
#define PHAMT_SIZE sizeof(struct PHAMT)

// The PHAMT_index_t type specifies how a particular hash value relates to a
// node in the PHAMT.
typedef struct {
   uint8_t bitindex;   // the bit index of the node
   uint8_t cellindex;  // the cell index of the node
   uint8_t is_beneath; // whether the key is beneath this node
   uint8_t is_found;   // whether the bit for the key is set
} PHAMT_index_t;

// The PHAMT_loc_t specifies a pairing of a node and an index.
typedef struct {
   PHAMT_t      node;  // The node that this location refers to.
   PHAMT_index_t index; // The cell-index that this location refers to.
} PHAMT_loc_t;

// The PHAMT_path_t specifies the meta-data of a search or iteration over a
// PHAMT object.
typedef struct {
   // PHAMT_LEVELS is guaranteed to be enough space for any search.
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
   //    then the node is the root of the tree (i.e., d is equal to min_depth,
   //    below).
   PHAMT_loc_t steps[PHAMT_LEVELS];
   // These additional data store some general information about the search:
   //  - min_depth is the depth if the *first* node on the path (i.e., the node
   //    in which the search was initiated).
   uint8_t min_depth;
   //  - max_depth is the depth of the *final* node on the path. The requested
   //    node may not be beneath this node if edit_depth is not equal to
   //    min_depth.
   uint8_t max_depth;
   //  - edit_depth is the depth at which the first edit to the path should be
   //    made if the intention is to add the node to the path. This is always
   //    equal to min_depth except in the case that the value being searched for
   //    is disjoint from the node at the min_depth, indicating that the search
   //    reached a depth at which the value was not beneath the subnode.
   uint8_t edit_depth;
   //  - value_found is 1 if the element requested (when the path was created)
   //    was found in the original node and 0 if the requested element was not
   //    found.
   uint8_t value_found;
} PHAMT_path_t;


//==============================================================================
// Debugging Code.
// This section contains macros that either do or do not print debugging
// messages to standard error, depending on whether the symbol __PHAMT_DEBUG is
// defined.
// In other words, if we want to print debug statements, we can define the
// following before including this header file:
// #define __PHAMT_DEBUG

#ifdef __PHAMT_DEBUG
#  include <stdio.h>
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
   static inline void dbgpath(const char* prefix, PHAMT_path_t* path)
   {
      char buf[1024];
      uint8_t d = path->max_depth;
      PHAMT_loc_t* loc = &path->steps[d];
      PHAMT_t node = path->steps[path->min_depth].node;
      fprintf(stderr, "%s path [%u, %u, %u, %u]\n", prefix,
              (unsigned)path->min_depth, (unsigned)path->edit_depth,
              (unsigned)path->max_depth, (unsigned)path->value_found);
      do {
         loc = path->steps + d;
         sprintf(buf, "%s path     %2u:", prefix, (unsigned)d);
         dbgnode(buf, loc->node);
         dbgci(buf, loc->index);
         d = loc->index.is_beneath;
      } while (loc->node != node);
   }
#else
#  define dbgmsg(...)
#  define dbgnode(prefix, u)
#  define dbgci(prefix, ci)
#  define dbgpath(prefix, path)
#endif


//==============================================================================
// Inline Utility functions.
// These are mostly functions for making masks and counting bits, for use with
// PHAMT nodes.

// lowmask(bitno)
// Yields a mask of all bits above the given bit number set to false and all
// bits below that number set to true. The bit itself is set to false. Bits are
// indexed starting at 0.
// lowmask(bitno) is equal to ~highmask(bitno).
static inline bits_t lowmask_bits(bits_t bitno)
{
   return ((BITS_ONE << bitno) - BITS_ONE);
}
static inline hash_t lowmask_hash(hash_t bitno)
{
   return ((HASH_ONE << bitno) - HASH_ONE);
}
// highmask(bitno)
// Yields a mask of all bits above the given bit number set to true and all
// bits below that number set to true. The bit itself is set to true. Bits are
// indexed starting at 0.
// highmask(bitno) is equal to ~lowmask(bitno).
static inline bits_t highmask_bits(bits_t bitno)
{
   return ~((BITS_ONE << bitno) - BITS_ONE);
}
static inline hash_t highmask_hash(hash_t bitno)
{
   return ~((HASH_ONE << bitno) - HASH_ONE);
}
// highbitdiff(id1, id2)
// Yields the highest bit that is different between id1 and id2.
static inline bits_t highbitdiff_bits(bits_t id1, bits_t id2)
{
   return BITS_BITCOUNT - clz_bits(id1 ^ id2) - 1;
}
static inline hash_t highbitdiff_hash(hash_t id1, hash_t id2)
{
   return HASH_BITCOUNT - clz_hash(id1 ^ id2) - 1;
}
// firstn(bits)
// True if the first n bits (and only those bits) are set (for any n) and False
// otherwise.
static inline uint8_t firstn_bits(bits_t bits)
{
   return lowmask_bits(BITS_BITCOUNT - clz_bits(bits)) == bits;
}
// phamt_depthmask(depth)
// Yields the mask that includes the address space for all nodes at or below the
// given depth.
static inline hash_t phamt_depthmask(hash_t depth)
{
   if (depth == PHAMT_TWIG_DEPTH)
      return PHAMT_TWIG_MASK;
   else if (depth == 0)
      return HASH_MAX;
   else
      return ((HASH_ONE << (PHAMT_ROOT_FIRSTBIT - (depth-1)*PHAMT_NODE_SHIFT))
              - HASH_ONE);
}
// phamt_minleaf(nodeid)
// Yields the minimum child leaf index associated with the given nodeid.
static inline hash_t phamt_minleaf(hash_t nodeid)
{
   return nodeid;
}
// phamt_maxleaf(nodeid)
// Yields the maximum child leaf index assiciated with the given nodeid.
static inline hash_t phamt_maxleaf(hash_t nodeid, hash_t depth)
{
   return nodeid | phamt_depthmask(depth);
}
// phamt_isbeneath(nodeid, leafid)
// Yields true if the given leafid can be found beneath the given node-id.
static inline hash_t phamt_isbeneath(hash_t nodeid, hash_t depth, hash_t leafid)
{
   return leafid >= nodeid && leafid <= (nodeid | phamt_depthmask(depth));
}
// phamt_cellcount(node)
// Get the number of cells in the PHAMT node (not the number of elements).
static inline bits_t phamt_cellcount(PHAMT_t u)
{
   return (bits_t)Py_SIZE(u);
}
// phamt_cellindex(node, leafid)
// Yields a PHAMT_index_t structure that indicates whether and where the leafid
// is with respect to node.
static inline PHAMT_index_t phamt_cellindex(PHAMT_t node, hash_t leafid)
{
   PHAMT_index_t ci;
   ci.is_beneath = phamt_isbeneath(node->address, node->addr_depth, leafid);
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


//==============================================================================
// Public API
// These functions are part of the public PHAMT C API; they can be used to
// create and edit PHAMTs.

//------------------------------------------------------------------------------
// Creating PHAMT objects.

// phamt_empty()
// Returns the empty PHAMT object; caller obtains the reference.
// This function increments the empty object's refcount.
// Unlike phamt_empty_ctype(), this returns a PHAMT that tracks Python objects.
PHAMT_t phamt_empty(void);
// phamt_empty_ctype()
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
static inline PHAMT_t phamt_empty_like(PHAMT_t like)
{
   if (like == NULL || like->flag_pyobject) return phamt_empty();
   else return phamt_empty_ctype();
}
// _phamt_new(ncells)
// Create a new PHAMT with a size of ncells. This object is not initialized
// beyond Python's initialization, and it has not been added to the garbage
// collector, so it should not be used in general except by the phamt core
// functions themselves.
PHAMT_t _phamt_new(unsigned ncells);
// phamt_from_kv(k, v)
// Create a new PHAMT node that holds a single key-value pair.
// The returned node is fully initialized and has had the
// PyObject_GC_Track() function already called for it.
// The argument flag_pyobject should be 1 if v is a Python object and 0 if
// it is not (this determines whether the resulting PHAMT is a Python PHAMT
// or a c-type PHAMT).
static inline PHAMT_t phamt_from_kv(hash_t k, void* v, uint8_t flag_pyobject)
{
   PHAMT_t node = _phamt_new(1);
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
static inline PHAMT_t _phamt_copy_chgcell(PHAMT_t node, PHAMT_index_t ci,
                                         void* val)
{
   PHAMT_t u;
   bits_t ncells = Py_SIZE(node);
   u = _phamt_new(ncells);
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
// _phamt_copy_addcell(node, cellinfo)
// Creates a copy of the given node with a new cell inserted at the appropriate
// position and the bits value updated; increases all the relevant
// reference counts for the node's cells. Does not update numel or initiate the
// new cell bucket itself.
// The refcount on val is incremented.
static inline PHAMT_t _phamt_copy_addcell(PHAMT_t node, PHAMT_index_t ci,
                                         void* val)
{
   PHAMT_t u;
   bits_t ncells = phamt_cellcount(node);
   dbgnode("[_phamt_copy_addcell]", node);
   dbgci("[_phamt_copy_addcell]", ci);
   u = _phamt_new(ncells + 1);
   u->address = node->address;
   u->bits = node->bits | (BITS_ONE << ci.bitindex);
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = firstn_bits(u->bits);
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
// _phamt_copy_delcell(node, cellinfo)
// Creates a copy of the given node with a cell deleted at the appropriate
// position and the bits value updated; increases all the relevant
// reference counts for the node's cells. Does not update numel.
// Behavior is undefined if there is not a bit set for the ci.
static inline PHAMT_t _phamt_copy_delcell(PHAMT_t node, PHAMT_index_t ci)
{
   PHAMT_t u;
   bits_t ncells = phamt_cellcount(node) - 1;
   if (ncells == 0) return phamt_empty_like(node);
   u = _phamt_new(ncells);
   u->address = node->address;
   u->bits = node->bits & ~(BITS_ONE << ci.bitindex);
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = firstn_bits(u->bits);
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
// _phamt_join_disjoint(node1, node2)
// Yields a single PHAMT that has as children the two PHAMTs node1 and node2.
// The nodes must be disjoint--i.e., node1 is not a subnode of node2 and node2
// is not a subnode of node1. Both nodes must have the same pyobject flag.
// This function does not update the references of either node, so this must be
// accounted for by the caller (in other words, make sure to INCREF both nodes
// before calling this function). The return value has a refcount of 1.
static inline PHAMT_t _phamt_join_disjoint(PHAMT_t a, PHAMT_t b)
{
   PHAMT_t u;
   uint8_t bit0, shift, newdepth;
   hash_t h;
   // What's the highest bit at which they differ?
   h = highbitdiff_hash(a->address, b->address);
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
   u = _phamt_new(2);
   u->address = a->address & highmask_hash(bit0 + shift);
   u->numel = a->numel + b->numel;
   u->flag_pyobject = a->flag_pyobject;
   u->flag_transient = 0;
   u->addr_shift = shift;
   u->addr_startbit = bit0;
   u->addr_depth = newdepth;
   // We use h to store the new minleaf value.
   u->bits = 0;
   h = lowmask_hash(shift);
   u->bits |= BITS_ONE << (h & (a->address >> bit0));
   u->bits |= BITS_ONE << (h & (b->address >> bit0));
   if (a->address < b->address) {
      u->cells[0] = (void*)a;
      u->cells[1] = (void*)b;
   } else {
      u->cells[0] = (void*)b;
      u->cells[1] = (void*)a;
   }
   u->flag_firstn = firstn_bits(u->bits);
   // We need to register the new node u with the garbage collector.
   PyObject_GC_Track((PyObject*)u);
   // That's all.
   return u;
}

//------------------------------------------------------------------------------
// Lookup and finding functions.

// phamt_lookup(node, k)
// Yields the leaf value for the hash k. If no such key is in the phamt, then
// NULL is returned.
// This function does not deal at all with INCREF or DECREF, so before returning
// anything returned from this function back to Python, be sure to INCREF it.
// If the pointer found is provided, then it is set to 1 if the key k was found
// and 0 if it was not; this allows for disambiguation when NULL is a valid
// value (for a ctype PHAMT).
static inline void* phamt_lookup(PHAMT_t node, hash_t k, int* found)
{
   PHAMT_index_t ci;
   uint8_t depth;
   int dummy;
   if (!found) found = &dummy;
   dbgmsg("[phamt_lookup] call: key=%p\n", (void*)k);
   do {
      ci = phamt_cellindex(node, k);
      dbgnode("[phamt_lookup]      ", node);      
      dbgci(  "[phamt_lookup]      ", ci);
      if (!ci.is_found) {
         *found = 0;
         return NULL;
      }
      depth = node->addr_depth;
      node = (PHAMT_t)node->cells[ci.cellindex];
   } while (depth != PHAMT_TWIG_DEPTH);
   dbgmsg("[phamt_lookup]       return %p\n", (void*)node);
   *found = 1;
   return (void*)node;
}
// phamt_find(node, k, path)
// Finds and returns the value associated with the given key k in the given
// node. Update the given path-object in order to indicate where in the node
// the key lies.
static inline void* phamt_find(PHAMT_t node, hash_t k, PHAMT_path_t* path)
{
   PHAMT_loc_t* loc;
   uint8_t depth, updepth = 0xff;
   path->min_depth = node->addr_depth;
   do {
      depth = node->addr_depth;
      loc = path->steps + depth;
      loc->node = node;
      loc->index = phamt_cellindex(node, k);
      if (!loc->index.is_found) {
         path->max_depth = depth;
         path->edit_depth = (loc->index.is_beneath ? depth : updepth);
         path->value_found = 0;
         loc->index.is_found = 0;
         loc->index.is_beneath = updepth;
         return NULL;
      }
      loc->index.is_beneath = updepth;
      updepth = depth;
      node = (PHAMT_t)node->cells[loc->index.cellindex];
   } while (depth != PHAMT_TWIG_DEPTH);
   // If we reach this point, node is the correct/found value.
   path->max_depth = PHAMT_TWIG_DEPTH;
   path->edit_depth = PHAMT_TWIG_DEPTH;
   path->value_found = 1;
   return (void*)node;
}

//------------------------------------------------------------------------------
// Editing Functions (assoc'ing and dissoc'ing).

// _phamt_assoc_path(path, k, newval)
// Performs an assoc operation in which the PHAMT leaf referenced by the given
// path and hash-value k (i.e., path was found via phamt_find(node, k, path)).
static inline PHAMT_t _phamt_assoc_path(PHAMT_path_t* path, hash_t k,
                                       void* newval)
{
   uint8_t dnumel = 1 - path->value_found, depth = path->max_depth;
   PHAMT_loc_t* loc = path->steps + depth;
   PHAMT_t u, node = path->steps[path->min_depth].node;
   dbgmsg("[_phamt_assoc] start: %p\n", (void*)k);
   dbgpath("[_phamt_assoc]  ", path);
   // The first step in this function is to handle all the quick cases (like
   // assoc'ing to the empty PHAMT) and to get the replacement node for the
   // deepest node in the path (u).
   if (path->value_found) {
      // We'e replacing a leaf. Check that there's reason to.
      void* curval = loc->node->cells[loc->index.cellindex];
      if (curval == newval) {
         Py_INCREF(node);
         return node;
      }
      // Go ahead and alloc a copy.
      u = _phamt_copy_chgcell(loc->node, loc->index, newval);
      PyObject_GC_Track((PyObject*)u);
   } else if (depth != path->edit_depth) {
      // The key isn't beneath the deepest node; we need to join a new twig
      // with the disjoint deep node.
      u = phamt_from_kv(k, newval, node->flag_pyobject);
      Py_INCREF(loc->node); // The new parent node gets this ref.
      u = _phamt_join_disjoint(loc->node, u);
   } else if (depth == PHAMT_TWIG_DEPTH) {
      // We're adding a new leaf. This updates refcounts for everything
      // except the replaced cell (correctly).
      u = _phamt_copy_addcell(loc->node, loc->index, newval);
      ++(u->numel);
      PyObject_GC_Track((PyObject*)u);
   } else if (node->numel == 0) {
      // We are assoc'ing to the empty node, so just return a new key-val twig.
      return phamt_from_kv(k, newval, node->flag_pyobject);
   } else {
      // We are adding a new twig to an internal node.
      node = phamt_from_kv(k, newval, node->flag_pyobject);
      // The key is beneath this node, so we insert u into it.
      u = _phamt_copy_addcell(loc->node, loc->index, node);
      Py_DECREF(node);
      ++(u->numel);
      PyObject_GC_Track((PyObject*)u);
   }
   // At this point, u is the replacement node for loc->node, which is the
   // deepest node in the path.
   // We now step up through the path, rebuilding the nodes.
   while (depth != path->min_depth) {
      depth = loc->index.is_beneath;
      loc = path->steps + depth;
      node = u;
      u = _phamt_copy_chgcell(loc->node, loc->index, u);
      Py_DECREF(node);
      u->numel += dnumel;
      PyObject_GC_Track((PyObject*)u);
   }
   // At the end of this loop, u is the replacement node, and should be ready.
   return u;
}
// _phamt_dissoc_path(path, k, newval)
// Performs a dissoc operation in which the PHAMT leaf referenced by the given
// path and hash-value k (i.e., path was found via phamt_find(node, k, path)).
static inline PHAMT_t _phamt_dissoc_path(PHAMT_path_t* path)
{
   PHAMT_loc_t* loc;
   PHAMT_t u, node = path->steps[path->min_depth].node;
   uint8_t depth = path->max_depth;
   dbgpath("[_phamt_dissoc]", path);
   if (!path->value_found) {
      // The item isn't there; just return the node unaltered.
      Py_INCREF(node);
      return node;
   }
   loc = path->steps + depth;
   if (loc->node->numel == 1) {
      // We need to just remove this node; however, we know that the parent node
      // won't need this same treatment because only twig nodes can have exactly
      // 1 child--otherwise the node gets simplified.
      if (path->min_depth == depth)
         return phamt_empty_like(loc->node);
      depth = loc->index.is_beneath;
      loc = path->steps + depth;
      // Now, we want to delcell at loc, but if loc has n=2, then we instead
      // just want to pass up the other twig.
      if (phamt_cellcount(loc->node) == 2) {
         u = loc->node->cells[loc->index.cellindex ? 0 : 1];
         Py_INCREF(u);
         if (depth == path->min_depth)
            return u;
      } else  {
         u = _phamt_copy_delcell(loc->node, loc->index);
         --(u->numel);
         PyObject_GC_Track((PyObject*)u);
      }
   } else {
      u = _phamt_copy_delcell(loc->node, loc->index);
      --(u->numel);
      PyObject_GC_Track((PyObject*)u);
   }
   // At this point, u is the replacement node for loc->node, which is the
   // deepest node in the path.
   // We now step up through the path, rebuilding the nodes.
   while (depth > path->min_depth) {
      depth = loc->index.is_beneath;
      loc = path->steps + depth;
      node = u;
      u = _phamt_copy_chgcell(loc->node, loc->index, u);
      Py_DECREF(node);
      --(u->numel);
      PyObject_GC_Track((PyObject*)u);
   }
   // At the end of this loop, u is the replacement node, and should be ready.
   return u;
}
// phamt_assoc(node, k, v)
// Yields a copy of the given PHAMT with the new key associated. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
static inline PHAMT_t phamt_assoc(PHAMT_t node, hash_t k, void* v)
{
   PHAMT_path_t path;
   phamt_find(node, k, &path);
   return _phamt_assoc_path(&path, k, v);
}
// phamt_dissoc(node, k)
// Yields a copy of the given PHAMT with the given key removed. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
static inline PHAMT_t phamt_dissoc(PHAMT_t node, hash_t k)
{
   PHAMT_path_t path;
   phamt_find(node, k, &path);
   return _phamt_dissoc_path(&path);
}
// _phamt_update(path, k, newval, remove)
// An update function that either assoc's k => newval (if remove is 0) into the
// node referenced by the given path, or removes the hash k from the PHAMT (if
// remove is 1).
// The path must be found via phamt_find(node, k, path).
static inline PHAMT_t _phamt_update(PHAMT_path_t* path, hash_t k, void* newval,
                                    uint8_t remove)
{
   if (remove) return _phamt_assoc_path(path, k, newval);
   else        return _phamt_dissoc_path(path);
}
// phamt_apply(node, h, fn, arg)
// Applies the given function to the value with the given hash h. The function
// is called as fn(uint8_t found, void** value, void* arg); it will always be
// safe to set *value. If found is 0, then the node was not found, and if it is
// 1 then it was found. If fn returns 0, then the hash should be removed from
// the PHAMT; if 1 then the value stored in *value should be added or should
// replace the value mapped to h.
// The updated PHAMT is returned.
typedef uint8_t (*phamtfn_t)(uint8_t found, void** value, void* arg);
static inline PHAMT_t phamt_apply(PHAMT_t node, hash_t k,
                                  phamtfn_t fn, void* arg)
{
   uint8_t rval;
   PHAMT_path_t path;
   void* val = phamt_find(node, k, &path);
   rval = (*fn)(path.value_found, &val, arg);
   return _phamt_update(&path, k, val, rval);
}

//------------------------------------------------------------------------------
// Iteration functions.

// _phamt_digfirst(node, path)
// Finds the first value stored under the given node at the given path by
// digging down into the 0'th cell of each node until we reach a twig/leaf.
static inline void* _phamt_digfirst(PHAMT_t node, PHAMT_path_t* path)
{
   PHAMT_loc_t* loc;
   uint8_t last_depth = path->steps[node->addr_depth].index.is_beneath;
   // Just dig down into the first children as far as possible
   do {
      loc = path->steps + node->addr_depth;
      loc->node = node;
      loc->index.cellindex = 0;
      loc->index.bitindex = ctz_bits(node->bits);
      loc->index.is_beneath = last_depth;
      loc->index.is_found = 1;
      last_depth = node->addr_depth;
      node = (PHAMT_t)node->cells[0];
   } while (last_depth < PHAMT_TWIG_DEPTH);
   path->value_found = 1;
   path->max_depth = PHAMT_TWIG_DEPTH;
   path->edit_depth = PHAMT_TWIG_DEPTH;
   return node;
}
// phamt_first(node, iter)
// Returns the first item in the phamt node and sets the iterator accordingly.
// If the depth in the iter is ever set to 0 when this function returns, that
// means that there are no items to iterate (the return value will also be
// NULL in this case). No refcounting is performed by this function.
static inline void* phamt_first(PHAMT_t node, PHAMT_path_t* path)
{
   path->min_depth = node->addr_depth;
   // Check that this node isn't empty.
   if (node->numel == 0) {
      path->value_found = 0;
      path->max_depth = 0;
      path->edit_depth = 0;
      return NULL;
   }
   // Otherwise, digfirst will take care of things.
   path->steps[node->addr_depth].index.is_beneath = 0xff;
   return _phamt_digfirst(node, path);
}
// phamt_next(node, iter)
// Returns the next item in the phamt node and updates the iterator accordingly.
// If the depth in the iter is ever set to 0 when this function returns, that
// means that there are no items to iterate (the return value will also be
// NULL in this case). No refcounting is performed by this function.
static inline void* phamt_next(PHAMT_t node0, PHAMT_path_t* path)
{
   PHAMT_t node;
   uint8_t d, ci;
   bits_t mask;
   PHAMT_loc_t* loc;
   // We should always return from twig depth, but we can start at whatever
   // depth the path gives us, in case someone has a path pointing to the middle
   // of a phamt somewhere.
   d = path->max_depth;
   do {
      loc = path->steps + d;
      ci = loc->index.cellindex + 1;
      if (ci < phamt_cellcount(loc->node)) {
         // We've found a point at which we can descend.
         loc->index.cellindex = ci;
         mask = highmask_bits(loc->index.bitindex);
         loc->index.bitindex = ctz_bits(loc->node->bits & mask);
         // We can dig for the rest.
         node = loc->node->cells[ci];
         if (d < PHAMT_TWIG_DEPTH)
            node = _phamt_digfirst(node, path);
         return node;
      } else if (d == 0) {
         break;
      } else {
         d = loc->index.is_beneath;
      }
   } while (d <= PHAMT_TWIG_DEPTH);
   // If we reach this point, we didn't find anything.
   path->value_found = 0;
   path->max_depth = 0;
   path->edit_depth = 0;
   path->min_depth = 0;
   return NULL;
}

#ifdef __cplusplus
}
#endif

#endif // ifndef __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210

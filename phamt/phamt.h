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
// The docstrings:
#define PHAMT_DOCSTRING (                                                      \
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
   " * by using `phamt_obj.assoc(k,v)` or `phamt_obj.dissoc(k)` on existing\n" \
   "   `PHAMT` objects, such as the `PHAMT.empty` object, which represents\n"  \
   "   an empty `PHAMT`;\n"                                                    \
   " * by supplying the `PHAMT.from_iter(iter_of_values)` with a list of\n"    \
   "   values, which are assigned the keys `0`, `1`, `2`, etc.\n"              )
#define PHAMT_ASSOC_DOCSTRING (                                                \
   "Returns a new `PHAMT` object with an additional association.\n"            \
   "\n"                                                                        \
   "`phamt_obj.assoc(key, value)` returns a new `PHAMT` object that is equal\n"\
   "to `phamt_obj` with the modification that in the new object, `key` is\n"   \
   "mapped to `value`. This is copied efficiently using shared state, so\n"    \
   "that the time to perform this update is `O(log n)` and the additional\n"   \
   "space required to keep both the original and the new object in memory is\n"\
   "also `O(log n)`.\n")
#define PHAMT_DISSOC_DOCSTRING (                                               \
   "Returns a new `PHAMT` object with an additional association.\n"            \
   "\n"                                                                        \
   "`phamt_obj.assoc(key, value)` returns a new `PHAMT` object that is equal\n"\
   "to `phamt_obj` with the modification that in the new object, `key` is\n"   \
   "mapped to `value`. This is copied efficiently using shared state, so\n"    \
   "that the time to perform this update is `O(log n)` and the additional\n"   \
   "space required to keep both the original and the new object in memory is\n"\
   "also `O(log n)`.\n")
#define PHAMT_FROM_ITER_DOCSTRING (                                            \
   "Constructs a PHAMT object from a sequence or iterable of values.\n"        \
   "\n"                                                                        \
   "`PHAMT.from_iter(items)` returns a `PHAMT` object whose keys are the\n"    \
   "integers `0, 1 ... len(items)` and whose values are the elements of the\n" \
   "iterable `items` in iteration order. This is performed with minimal\n"     \
   "allocations, so it should be more efficient than building the PHAMT from\n"\
   "scratch.\n"                                                                \
   "\n"                                                                        \
   "`PHAMT.from_iter(items, k0)` returns a `PHAMT` object whose keys are the\n"\
   "integers `k0, k0+1 ... k0+len(items)`.\n")
#define PHAMT_TRANSIENT_DOCSTRING (                                            \
   "Returns an equivalent transient HAMT (`THAMT`) object.\n"                  \
   "\n"                                                                        \
   "`phamt.transient()` returns a transient `THAMT` object that is\n"          \
   "equivalent to `phamt`. This operation can be performed in constant time,\n"\
   "and in-place updates to the resulting `THAMT` are performed with minimal\n"\
   "allocations.\n")
#define THAMT_DOCSTRING (                                                      \
   "A Transient Hash Array Mapped Trie (THAMT) type.\n"                        \
   "\n"                                                                        \
   "The `THAMT` class represents a minimal mutable persistent mapping type\n"  \
   "that can be used to efficiently edit persistent `PHAMT` objects in\n"      \
   "Python. (See also `PHAMT`). A `THAMT` object is essentially a tree\n"      \
   "structure that maps integer keys (hashes) to values, which may be any\n"   \
   "Python objects.\n"                                                         \
   "\n"                                                                        \
   "`THAMT` objects can be greated from `PHAMT` objects (i.e.,\n"              \
   "`thamt = THAMT(phamt)`) in constant time. Unlike `PHAMT` objects,\n"       \
   "`THAMT` objects can be edited in-place like dictionaries. These edits\n"   \
   "are more efficient with respect to time than update to the `PHAMT` tyoe,\n"\
   "however, they are slightly less space efficient than pure `PHAMT`s. Once\n"\
   "a `THAMT` has been edited, it can be efficiently converted back into a\n"  \
   "`PHAMT` object using the `thamt.persistent()` method.\n")
#define THAMT_PERSISTENT_DOCSTRING (                                           \
   "Returns an equivalent persistent HAMT (`PHAMT`) object.\n"                 \
   "\n"                                                                        \
   "`thamt.persistent()` returns a persistent `PHAMT` object that is\n"        \
   "equivalent to `thamt`. This operation can be performed very efficiently\n" \
   "as it requires no allocations.\n")

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
#define PHAMT_ANY_MAXCELLS  (1 << PHAMT_TWIG_SHIFT) // Assuming twig is largest.
#define PHAMT_NODE_BITS     (HASH_BITCOUNT-PHAMT_ROOT_SHIFT-PHAMT_TWIG_SHIFT)
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
   // Whether the PHAMT has allocated all cells, even empty ones.
   bits_t flag_full : 1;
   // The remaining bits are just empty for now.
   bits_t _empty : 7;
   // ^-----------------------------------------------------------------^
   // And finally the variable-length list of children.
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
   PHAMT_t       node;  // The node that this location refers to.
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

// The PHAMT iterator type for Python.
typedef struct PHAMT_iter {
   // The Python data.
   PyObject_HEAD
   // All the iterator needs is to know is the path of iteration so far.
   PHAMT_path_t path;
}* PHAMT_iter_t;

// The THAMT type for Python.
// THAMTs are just thin layers around PHAMTs; note that the PHAMT type already
// has all the machinery for dealing with transients via the flag_transient bit,
// the THAMT type as Python sees
// Note that the thamt_* and _thamt_* functions in this file deal only with the
// PHAMT type--specifically with the PHAMTs that are wrapped by THAMTs. These
// may have the transient bit set, and so might be mutated in place. The actual
// THAMT_t type that this struct is for is only part of the Python interface to
// THAMTs.
typedef struct THAMT {
   // The Python data.
   PyObject_HEAD
   // The PHAMT that we wrap. This may be pesistent or transient--the idea is
   // that once we start updating it, we replace it with transient nodes and
   // mutate them directly in further updates. When a THAMT is persisted, we
   // just flip the transient bit on all the non-persistent nodes and return
   // this value.
   PHAMT_t phamt;
   // THAMTs track a version number specifically so that iterators don't get
   // screwed up when the THAMT changes underneath them.
   hash_t version;
}* THAMT_t;

// The PHAMT iterator type for Python.
typedef struct THAMT_iter {
   // The Python data.
   PyObject_HEAD
   // All the iterator needs is to know is the path of iteration so far...
   PHAMT_path_t path;
   // And the THAMT on which it is operating.
   THAMT_t thamt;
   // Transient iterators also need to know the version of the THAMT they
   // started iterating over so that know if the THAMT was changed. In
   // persistent iterators, this is ignored.
   hash_t version;
}* THAMT_iter_t;


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
            "%s       flags={pyobj=%u, firstn=%u, full=%u}}\n",    \
            (prefix),                                              \
            (void*)(u)->address, (u)->addr_depth,                  \
            (u)->addr_startbit, (u)->addr_shift,                   \
            (prefix),                                              \
            (unsigned)(u)->numel, (void*)((intptr_t)(u)->bits),    \
            (prefix),                                              \
            (u)->flag_pyobject, (u)->flag_firstn, (u)->flag_full)
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
   return ~((BITS_ONE << bitno) - BITS_ONE) * (bitno != BITS_BITCOUNT);
}
static inline hash_t highmask_hash(hash_t bitno)
{
   return ~((HASH_ONE << bitno) - HASH_ONE) * (bitno != HASH_BITCOUNT);
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
   //if (depth == PHAMT_TWIG_DEPTH)
   //   return PHAMT_TWIG_MASK;
   //else if (depth == 0)
   //   return HASH_MAX;
   //else
   //   return ((HASH_ONE << (PHAMT_ROOT_FIRSTBIT - (depth-1)*PHAMT_NODE_SHIFT))
   //           - HASH_ONE);
   // No-branch version of the above code block.
   uint8_t t = depth == PHAMT_TWIG_DEPTH,
           r = depth == PHAMT_ROOT_DEPTH,
           n = (t | r) == 0;
   hash_t h = (PHAMT_ROOT_FIRSTBIT - (depth-1)*PHAMT_NODE_SHIFT);
   h = ((HASH_ONE << h) - HASH_ONE);
   return ( (r * HASH_MAX)
          | (t * PHAMT_TWIG_MASK)
          | (n * h));
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
// phamt_maxcells(depth)
// Get the maximum number of cells at a particula depth.
static inline bits_t phamt_maxcells(bits_t depth)
{
   //return (depth == PHAMT_ROOT_DEPTH
   //        ? PHAMT_ROOT_MAXCELLS
   //        : node->addr_depth == PHAMT_TWIG_DEPTH
   //        ? PHAMT_TWIG_MAXCELLS
   //        : PHAMT_NODE_MAXCELLS)
   // No-branching version of the above:
   uint8_t r = (depth == PHAMT_ROOT_DEPTH),
           t = (depth == PHAMT_TWIG_DEPTH),
           n = ((t|r) == 0);
   return ( (r * PHAMT_ROOT_MAXCELLS)
          | (t * PHAMT_TWIG_MAXCELLS)
          | (n * PHAMT_NODE_MAXCELLS));
      
}
// phamt_cellcount(node)
// Get the number of cells in the PHAMT node (not the number of elements).
static inline bits_t phamt_cellcount(PHAMT_t u)
{
   return (u->flag_full ? popcount_bits(u->bits) : Py_SIZE(u));
}
// phamt_cellcapacity(node)
// Get the number of allocated cells in this node.
static inline bits_t phamt_cellcapacity(PHAMT_t u)
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
   ci.bitindex = ((leafid >> node->addr_startbit)
                  & lowmask_hash(node->addr_shift));
   // Get the cellindex.
   ci.cellindex = (node->flag_firstn | node->flag_full
                   ? ci.bitindex
                   : popcount_bits(node->bits & lowmask_bits(ci.bitindex)));
   // is_found depends on whether the bit is set. We use multiplication by
   // is_beneath here in order to avoid a branch (as in this commented block):
   //ci.is_found = (ci.is_beneath
   //               ? ((node->bits & (BITS_ONE << ci.bitindex)) != 0)
   //               : 0);
   ci.is_found = ci.is_beneath*((node->bits & (BITS_ONE << ci.bitindex)) != 0);
   return ci;
}
// phamt_cellfirst(node)
// Yields the PHAMT_index_t for the first cell in the PHAMT node.
// The index's is_found will be 0 if the node is empty.
// The index's is_beneath will always be equal to is_found.
static inline PHAMT_index_t phamt_firstcell(PHAMT_t node)
{
   PHAMT_index_t ii;
   ii.is_found   = (node->numel > 0);
   ii.is_beneath = ii.is_found;
   if (node->flag_firstn) {
      ii.bitindex  = 0;
      ii.cellindex = 0;
   } else {
      //ii.bitindex = ctz_bits(node->bits);
      //if (node->flag_full)
      //   ii.cellindex = ii.bitindex;
      //else
      //   ii.cellindex = 0;
      // The above code-block, without branches.
      ii.bitindex  = ctz_bits(node->bits);
      ii.cellindex = node->flag_full * ii.bitindex;
   }
   return ii;
}
// phamt_cellnext(node)
// Yields the next PHAMT_index_t for the next cell in the PHAMT node after the
// cell whose index is given.
static inline PHAMT_index_t phamt_nextcell(PHAMT_t node, PHAMT_index_t ii)
{
   bits_t b = node->bits & highmask_bits(ii.bitindex);
   ii.bitindex = ctz_bits(b);
   //if (node->flag_full)
   //   ii.cellindex = ii.bitindex;
   //else
   //   ++ii.cellindex;
   // no-branching version of the above block:
   ii.cellindex = ( ((!node->flag_full) * (ii.cellindex + 1))
                  | (( node->flag_full) * (ii.bitindex)));
   ii.is_found = (b > 0);
   ii.is_beneath = ii.is_found;
   return ii;
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
   node->flag_full = 0;
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
   bits_t ncells = phamt_cellcount(node);
   dbgnode("[_phamt_copy_chgcell]", node);
   dbgci("[_phamt_copy_chgcell]", ci);
   u = _phamt_new(ncells);
   u->address = node->address;
   u->bits = node->bits;
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = node->flag_firstn;
   u->flag_full = ncells == phamt_maxcells(node->addr_depth);
   u->flag_transient = 0;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   if (node->flag_full) {
      // We need to copy from a fully-allocated node to a compact one.
      bits_t b, bi, ii = 0, flag = BITS_ONE << ci.bitindex;
      for (b = u->bits; b & flag; b &= ~(BITS_ONE << bi)) {
         bi = ctz_bits(b);
         u->cells[ii++] = node->cells[bi];
      }
      // The loop above copies over the changed cell, so we overwrite it.
      u->cells[ii - 1] = val;
      for (; b; b &= ~(BITS_ONE << bi)) {
         bi = ctz_bits(b);
         u->cells[ii++] = node->cells[bi];
      }
   } else {
      memcpy(u->cells, node->cells, sizeof(void*)*ncells);
      // Change the relevant cell.
      u->cells[ci.cellindex] = val;
   }
   // Increase the refcount for all these cells!
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject) {
      bits_t ii;
      for (ii = 0; ii < ncells; ++ii)
         Py_INCREF((PyObject*)u->cells[ii]);
   }
   PyObject_GC_Track((PyObject*)u);
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
   u->flag_full = ncells + 1 == phamt_maxcells(node->addr_depth);
   u->flag_transient = 0;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   // If node and u have different firstn flags, then cellindex may not be
   // correct here.
   if (u->flag_firstn != node->flag_firstn)
      ci = phamt_cellindex(u, ((hash_t)ci.bitindex) << node->addr_startbit);
   // If node is a full node, we can't use memcpy.
   if (node->flag_full) {
      bits_t b, bi, ii;
      for (b = u->bits, ii = 0; b; b &= ~(BITS_ONE << bi), ++ii) {
         bi = ctz_bits(b);
         u->cells[ii] = node->cells[bi];
      }
      // We have a new cellindex now.
      ci.cellindex = popcount_bits(u->bits & lowmask_bits(ci.bitindex));
   } else {
      memcpy(u->cells, node->cells, sizeof(void*)*ci.cellindex);
      memcpy(u->cells + ci.cellindex + 1,
             node->cells + ci.cellindex,
             sizeof(void*)*(ncells - ci.cellindex));
   }
   u->cells[ci.cellindex] = val;
   // Increase the refcount for all these cells!
   ++ncells;
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject) {
      bits_t ii;
      for (ii = 0; ii < ncells; ++ii)
         Py_INCREF((PyObject*)u->cells[ii]);
   }
   PyObject_GC_Track((PyObject*)u);
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
   u->flag_full = 0;
   u->flag_transient = 0;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   if (node->flag_full) {
      bits_t b, bi, ci;
      for (b = u->bits, ci = 0; b; b &= ~(BITS_ONE << bi)) {
         bi = ctz_bits(b);
         u->cells[ci++] = node->cells[bi];
      }
   } else {
      memcpy(u->cells, node->cells, sizeof(void*)*ci.cellindex);
      memcpy(u->cells + ci.cellindex,
             node->cells + ci.cellindex + 1,
             sizeof(void*)*(ncells - ci.cellindex));
   }
   // Increase the refcount for all these cells!
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject) {
      bits_t ii;
      for (ii = 0; ii < ncells; ++ii)
         Py_INCREF((PyObject*)u->cells[ii]);
   }
   PyObject_GC_Track((PyObject*)u);
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
   if (h < HASH_BITCOUNT - PHAMT_ROOT_SHIFT) {
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
   u->flag_full = 0;
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
// THAMT constructors.
// These functions are identical to the PHAMT constructors just above, except
// that they allocate THAMT nodes--i.e., PHAMTS with their flag_transient bits
// set. These nodes always have full arrays allocated in order to accomodate
// future edits.
static inline PHAMT_t _thamt_empty(uint8_t pyobject)
{
   // We have to allocate empty thampts!
   PHAMT_t node = _phamt_new(PHAMT_ANY_MAXCELLS);
   node->address = 0;
   node->bits = 0;
   node->numel = 0;
   node->address = 0;
   // The firstn flag is not really relevant, but...
   node->flag_firstn = 0;
   node->flag_transient = 1;
   node->flag_full = 1;
   node->flag_pyobject = pyobject;
   // All empty nodes must be the root depth.
   node->addr_depth    = PHAMT_ROOT_DEPTH;
   node->addr_startbit = PHAMT_ROOT_FIRSTBIT;
   node->addr_shift    = PHAMT_ROOT_SHIFT;
   node->address = 0;
   // Add to the garbage collector:
   PyObject_GC_Track((PyObject*)node);
   // That's it--node is ready!
   return node;
}
static inline PHAMT_t _thamt_from_kv(hash_t k, void* v, uint8_t flag_pyobject)
{
   PHAMT_t node = _phamt_new(PHAMT_ANY_MAXCELLS);
   uint8_t cellindex = k & PHAMT_TWIG_MASK;
   node->bits = (BITS_ONE << cellindex);
   node->address = k & ~PHAMT_TWIG_MASK;
   node->numel = 1;
   dbgmsg("[thamt_from_kv] %p -> %p (%s)\n", (void*)k, v,
          flag_pyobject ? "pyobject" : "ctype");
   node->flag_pyobject = flag_pyobject;
   node->flag_firstn = (cellindex == 0);
   node->flag_full = 1;
   node->flag_transient = 1;
   node->addr_depth = PHAMT_TWIG_DEPTH;
   node->addr_shift = PHAMT_TWIG_SHIFT;
   node->addr_startbit = 0;
   node->cells[cellindex] = (void*)v;
   // Update that refcount and notify the GC tracker!
   if (flag_pyobject) Py_INCREF(v);
   PyObject_GC_Track((PyObject*)node);
   // Otherwise, that's all!
   return node;
}
// _thamt_refcount_cells(node)
// Incrememnts the refcount of each cell in the given node.
// This does not check the pyobject flag.
static inline void _thamt_refcount_cells(PHAMT_t node)
{
   bits_t b, bi;
   for (b = node->bits; b; b &= ~(BITS_ONE << bi)) {
      bi = ctz_bits(b);
      Py_INCREF((PyObject*)node->cells[bi]);
   }
}
static inline PHAMT_t _thamt_copy_chgcell(PHAMT_t node, PHAMT_index_t ci,
                                          void* val)
{
   PHAMT_t u;
   bits_t ncells;
   if (node->flag_transient) {
      // We don't need to allocate anything--we just change in place.
      if (node->flag_pyobject || node->addr_depth != PHAMT_TWIG_DEPTH) {
         Py_DECREF((PyObject*)node->cells[ci.bitindex]);
         Py_INCREF((PyObject*)val);
      }
      node->cells[ci.bitindex] = val;
      Py_INCREF(node);
      return node;
   }
   // Otherwise, we need to do an allocation, much like with phamts.
   ncells = phamt_maxcells(node->addr_depth);
   u = _phamt_new(PHAMT_ANY_MAXCELLS);
   u->address = node->address;
   u->bits = node->bits;
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = node->flag_firstn;
   u->flag_full = 1;
   u->flag_transient = 1;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   // Copy over the cells. This depends on node's format.
   if (node->flag_full) {
      memcpy(u->cells, node->cells, sizeof(void*)*ncells);
   } else if (node->flag_firstn) {
      memcpy(u->cells, node->cells, sizeof(void*)*phamt_cellcount(node));
   } else {
      bits_t b, bi, ii = 0;
      for (b = u->bits; b; b &= ~(BITS_ONE << bi)) {
         bi = ctz_bits(b);
         u->cells[bi] = node->cells[ii++];
      }
   }
   u->cells[ci.bitindex] = val;
   // Increase the refcount for all these cells!
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject)
      _thamt_refcount_cells(u);
   PyObject_GC_Track((PyObject*)u);
   return u;
}
static inline PHAMT_t _thamt_copy_addcell(PHAMT_t node, PHAMT_index_t ci,
                                          void* val)
{
   PHAMT_t u;
   bits_t ncells = phamt_cellcount(node),
          maxcells = phamt_maxcells(node->addr_depth);
   dbgnode("[_thamt_copy_addcell]", node);
   dbgci("[_thamt_copy_addcell]", ci);
   if (node->flag_transient) {
      // We don't need to allocate anything--we just change in place.
      node->cells[ci.bitindex] = val;
      node->bits |= (BITS_ONE << ci.bitindex);
      node->flag_firstn = firstn_bits(node->bits);
      if (node->flag_pyobject || node->addr_depth < PHAMT_TWIG_DEPTH)
         Py_INCREF((PyObject*)val);
      Py_INCREF(node);
      return node;
   }
   // Otherwise, we need to do an allocation, much like with phamts.
   u = _phamt_new(PHAMT_ANY_MAXCELLS);
   u->address = node->address;
   u->bits = node->bits | (BITS_ONE << ci.bitindex);
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = firstn_bits(u->bits);
   u->flag_full = 1;
   u->flag_transient = 1;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   if (node->flag_full) {
      memcpy(u->cells, node->cells, sizeof(void*)*maxcells);
   } else if (node->flag_firstn) {
      memcpy(u->cells, node->cells, sizeof(void*)*ncells);
   } else {
      bits_t b, bi, ii = 0;
      for (b = node->bits; b; b &= ~(BITS_ONE << bi)) {
         bi = ctz_bits(b);
         u->cells[bi] = node->cells[ii++];
      }
   }
   // Increase the refcount for all these cells!
   u->cells[ci.bitindex] = val;
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject)
      _thamt_refcount_cells(u);
   PyObject_GC_Track((PyObject*)u);
   return u;
}
static inline PHAMT_t _thamt_copy_delcell(PHAMT_t node, PHAMT_index_t ci)
{
   PHAMT_t u;
   bits_t ncells, maxcells;
   if (node->flag_transient) {
      // We don't need to allocate anything--we just change the bit.
      if (node->flag_pyobject || node->addr_depth < PHAMT_TWIG_DEPTH)
         Py_DECREF(node->cells[ci.bitindex]);
      node->bits &= ~(BITS_ONE << ci.bitindex);
      node->flag_firstn = firstn_bits(node->bits);
      Py_INCREF(node);
      return node;
   }
   ncells = phamt_cellcount(node);
   maxcells = phamt_maxcells(node->addr_depth);
   // Otherwise, we need to do an allocation, much like with phamts.
   // We don't check for ncells == 0 because we're actually fine making a new
   // empty transient node.
   u = _phamt_new(PHAMT_ANY_MAXCELLS);
   u->address = node->address;
   u->bits = node->bits & ~(BITS_ONE << ci.bitindex);
   u->numel = node->numel;
   u->flag_pyobject = node->flag_pyobject;
   u->flag_firstn = firstn_bits(u->bits);
   u->flag_full = 1;
   u->flag_transient = 1;
   u->addr_depth = node->addr_depth;
   u->addr_shift = node->addr_shift;
   u->addr_startbit = node->addr_startbit;
   // In some cases, we go ahead and copy the old value over and just unset the
   // bit (plus unref the object if necessary). It doesn't hurt us to have the
   // old value sitting there.
   if (node->flag_full) {
      memcpy(u->cells, node->cells, sizeof(void*)*maxcells);
   } else if (node->flag_firstn) {
      memcpy(u->cells, node->cells, sizeof(void*)*ncells);
   } else {
      bits_t b, bi, ii = 0;
      for (b = node->bits; b; b &= ~(BITS_ONE << bi)) {
         bi = ctz_bits(b);
         if (bi != ci.bitindex)
            u->cells[bi] = node->cells[ii];
         ++ii;
      }
   }
   // Increase the refcount for all these cells!
   if (u->addr_depth < PHAMT_TWIG_DEPTH || u->flag_pyobject)
      _thamt_refcount_cells(u);
   PyObject_GC_Track((PyObject*)u);
   return u;
}
static inline PHAMT_t _thamt_join_disjoint(PHAMT_t a, PHAMT_t b)
{
   PHAMT_t u;
   uint8_t bit0, shift, newdepth, ii;
   hash_t h;
   // What's the highest bit at which they differ?
   h = highbitdiff_hash(a->address, b->address);
   if (h < HASH_BITCOUNT - PHAMT_ROOT_SHIFT) {
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
   u = _phamt_new(PHAMT_ANY_MAXCELLS);
   u->address = a->address & highmask_hash(bit0 + shift);
   u->numel = a->numel + b->numel;
   u->flag_pyobject = a->flag_pyobject;
   u->flag_full = 1;
   u->flag_transient = 1;
   u->addr_shift = shift;
   u->addr_startbit = bit0;
   u->addr_depth = newdepth;
   // We use h to store the new minleaf value.
   u->bits = 0;
   h = lowmask_hash(shift);
   ii = h & (a->address >> bit0);
   u->bits |= BITS_ONE << ii;
   u->cells[ii] = a;
   ii = h & (b->address >> bit0);
   u->bits |= BITS_ONE << ii;
   u->cells[ii] = b;
   u->flag_firstn = u->bits == 3;
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
   uint8_t ii, depth = path->max_depth;
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
         // We want to grab the other node that isn't on the path; this will
         // depend on whether the node is fully specified or not.
         if (loc->node->flag_full) {
            ii = ctz_bits(loc->node->bits & ~(BITS_ONE << loc->index.bitindex));
            u = loc->node->cells[ii];
         } else {
            // The cellindex will be 0 or 1; we just want the other value.
            u = loc->node->cells[1 - loc->index.cellindex];
         }
         Py_INCREF(u);
         if (depth == path->min_depth)
            return u;
      } else  {
         u = _phamt_copy_delcell(loc->node, loc->index);
         --(u->numel);
      }
   } else {
      u = _phamt_copy_delcell(loc->node, loc->index);
      --(u->numel);
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
      loc->index = phamt_firstcell(node);
      loc->index.is_beneath = last_depth;
      last_depth = node->addr_depth;
      node = (PHAMT_t)node->cells[loc->index.cellindex];
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
   uint8_t d = node->addr_depth;
   path->min_depth = d;
   path->steps[d].node = node;
   path->steps[d].index.is_beneath = 0xff;
   // Check that this node isn't empty.
   if (node->numel == 0) {
      path->value_found = 0;
      path->max_depth = 0;
      path->edit_depth = 0;
      return NULL;
   }
   // Otherwise, digfirst will take care of things.
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
   uint8_t d;
   bits_t mask, b;
   PHAMT_loc_t* loc;
   // We should always return from twig depth, but we can start at whatever
   // depth the path gives us, in case someone has a path pointing to the middle
   // of a phamt somewhere.
   d = path->max_depth;
   while (d <= PHAMT_TWIG_DEPTH) {
      loc = path->steps + d;
      // Get the next bitindex, assuming there are more.
      mask = highmask_bits(loc->index.bitindex) << 1;
      b = loc->node->bits & mask;
      if (b) {
         // We've found a point at which we can descend.
         loc->index.bitindex = ctz_bits(b);
         if (loc->node->flag_full)
            loc->index.cellindex = loc->index.bitindex;
         else
            ++(loc->index.cellindex);
         // We can dig for the rest.
         node = loc->node->cells[loc->index.cellindex];
         if (d < PHAMT_TWIG_DEPTH) {
            path->steps[node->addr_depth].index.is_beneath = d;
            node = _phamt_digfirst(node, path);
         }
         return node;
      } else if (d == path->min_depth) {
         break;
      } else {
         d = loc->index.is_beneath;
      }
   }
   // If we reach this point, we didn't find anything.
   path->value_found = 0;
   path->max_depth = 0xff;
   path->edit_depth = 0;
   path->min_depth = 0;
   return NULL;
}

//------------------------------------------------------------------------------
// THAMT functions.
// Any thamt_* function is equivalent to the phamt_* function defined above with
// the exception that it mutates nodes in-place when their flag_transient bits are
// set (you must set this bit yourself when creating a new PHAMT if you want to
// treat an object as transient). Persistent nodes are never mutated by the
// thamt_* functions, and it is safe to pass PHAMTs to the thampt functions--
// they will just return THAMTs (i.e., PHAMTs with their flag_transient bits set).
// All return values of these thamt functions (except thamt_persist) will have
// at least some subnodes that have the transient bit set; this should be fixed
// by calling thamt_persist(thamt), which flips all of these bits, turning the
// THAMT into a PHAMT.

static inline PHAMT_t _thamt_assoc_path(PHAMT_path_t* path, hash_t k,
                                        void* newval)
{
   uint8_t dnumel = 1 - path->value_found, depth = path->max_depth;
   PHAMT_loc_t* loc = path->steps + depth;
   PHAMT_t u, node = path->steps[path->min_depth].node;
   dbgmsg("[_thamt_assoc] start: %p\n", (void*)k);
   dbgpath("[_thamt_assoc]  ", path);
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
      // Go ahead and change it; we also manage the refcount if need.
      u = _thamt_copy_chgcell(loc->node, loc->index, newval);
   } else if (depth != path->edit_depth) {
      // The key isn't beneath the deepest node; we need to join a new twig
      // with the disjoint deep node.
      u = _thamt_from_kv(k, newval, node->flag_pyobject);
      Py_INCREF(loc->node); // The new parent node gets this ref.
      u = _thamt_join_disjoint(loc->node, u);
   } else if (depth == PHAMT_TWIG_DEPTH) {
      // We're adding a new leaf. This updates refcounts for everything
      // except the replaced cell (correctly).
      u = _thamt_copy_addcell(loc->node, loc->index, newval);
      ++(u->numel);
   } else if (node->numel == 0) {
      // We are assoc'ing to the empty node, so just return a new key-val twig.
      return _thamt_from_kv(k, newval, node->flag_pyobject);
   } else {
      // We are adding a new twig to an internal node.
      node = _thamt_from_kv(k, newval, node->flag_pyobject);
      // The key is beneath this node, so we insert u into it.
      u = _thamt_copy_addcell(loc->node, loc->index, node);
      Py_DECREF(node);
      ++(u->numel);
   }
   // At this point, u is the replacement node for loc->node, which is the
   // deepest node in the path.
   // We now step up through the path, rebuilding the nodes.
   while (depth != path->min_depth) {
      if (loc->node == u) {
         // We just edited a transient, so we can simplify the rest of this.
         Py_DECREF(u);
         do {
            depth = loc->index.is_beneath;
            loc = path->steps + depth;
            u = loc->node;
            u->numel += dnumel;
         } while (depth != path->min_depth);
         Py_INCREF(u);
      } else {
         depth = loc->index.is_beneath;
         loc = path->steps + depth;
         node = _thamt_copy_chgcell(loc->node, loc->index, u);
         Py_DECREF(u);
         u = node;
         u->numel += dnumel;
      }
   }
   // At the end of this loop, u is the replacement node, and should be ready.
   return u;
}
// _thamt_clear(node)
// Clears the given node (updating refcounts) and returns it. This does not
// check if node has the transient flag set, so it is an error to pass a non-
// transient PHAMT to this function.
static inline void _thamt_clear(PHAMT_t node)
{
   bits_t b, bi;
   // Decref the Python objects.
   if (node->flag_pyobject || node->addr_depth != PHAMT_TWIG_DEPTH) {
      for (b = node->bits; b; b &= ~(BITS_ONE << bi)) {
         bi = ctz_bits(b);
         Py_DECREF(node->cells[bi]);
      }
   }
   // Set the bits and numel to 0.
   node->bits = 0;
   node->numel = 0;
   // The firstn flag is no longer really relevant, but...
   node->flag_firstn = 0;
   // All empty nodes must be the root depth.
   node->addr_depth    = PHAMT_ROOT_DEPTH;
   node->addr_startbit = PHAMT_ROOT_FIRSTBIT;
   node->addr_shift    = PHAMT_ROOT_SHIFT;
   node->address = 0;
   // That's it--node is cleared.
}
static inline PHAMT_t _thamt_dissoc_path(PHAMT_path_t* path)
{
   PHAMT_loc_t* loc;
   PHAMT_t u, node = path->steps[path->min_depth].node;
   uint8_t ii, depth = path->max_depth;
   dbgpath("[_thamt_dissoc]", path);
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
      if (path->min_depth == depth) {
         if (node->flag_transient) {
            // Clear out the node and return it.
            _thamt_clear(node);
            Py_INCREF(node);
            return node;
         } else
            return _thamt_empty(loc->node->flag_pyobject);
      }
      depth = loc->index.is_beneath;
      loc = path->steps + depth;
      // Now, we want to delcell at loc, but if loc has n=2, then we instead
      // just want to pass up the other subhamt.
      if (phamt_cellcount(loc->node) == 2) {
         // We want to grab the other node that isn't on the path; this will
         // depend on whether the node is fully specified or not.
         if (loc->node->flag_full) {
            ii = ctz_bits(loc->node->bits & ~(BITS_ONE << loc->index.bitindex));
            u = loc->node->cells[ii];
         } else {
            // The cellindex will be 0 or 1; we just want the other value.
            u = loc->node->cells[1 - loc->index.cellindex];
         }
         Py_INCREF(u);
      } else {
         u = _thamt_copy_delcell(loc->node, loc->index);
         --(u->numel);
      }
   } else {
      u = _thamt_copy_delcell(loc->node, loc->index);
      --(u->numel);
   }
   // At this point, u is the replacement node for loc->node, which is the
   // deepest node in the path.
   // We now step up through the path, rebuilding the nodes.
   while (depth != path->min_depth) {
      if (loc->node == u) {
         // We just edited a transient, so we can simplify the rest of this.
         Py_DECREF(u);
         do {
            depth = loc->index.is_beneath;
            loc = path->steps + depth;
            u = loc->node;
            --(u->numel);
         } while (depth != path->min_depth);
         Py_INCREF(u);
      } else {
         depth = loc->index.is_beneath;
         loc = path->steps + depth;
         node = _thamt_copy_chgcell(loc->node, loc->index, u);
         Py_DECREF(u);
         u = node;
         --(u->numel);
      }
   }
   // At the end of this loop, u is the replacement node, and should be ready.
   return u;
}
static inline PHAMT_t thamt_assoc(PHAMT_t node, hash_t k, void* v)
{
   PHAMT_path_t path;
   phamt_find(node, k, &path);
   return _thamt_assoc_path(&path, k, v);
}
static inline PHAMT_t thamt_dissoc(PHAMT_t node, hash_t k)
{
   PHAMT_path_t path;
   phamt_find(node, k, &path);
   return _thamt_dissoc_path(&path);
}
static inline PHAMT_t _thamt_update(PHAMT_path_t* path, hash_t k, void* newval,
                                    uint8_t remove)
{
   if (remove) return _thamt_assoc_path(path, k, newval);
   else        return _thamt_dissoc_path(path);
}
static inline PHAMT_t thamt_apply(PHAMT_t node, hash_t k,
                                  phamtfn_t fn, void* arg)
{
   uint8_t rval;
   PHAMT_path_t path;
   void* val = phamt_find(node, k, &path);
   rval = (*fn)(path.value_found, &val, arg);
   return _thamt_update(&path, k, val, rval);
}
// thampt_persist(thamt)
// Flips all of the flag_transient bits in the thamt object and returns it.
// This does not change the refcount of node.
static inline PHAMT_t thamt_persist(PHAMT_t node)
{
   uint8_t d = node->addr_depth;
   PHAMT_t u;
   PHAMT_path_t path;
   PHAMT_loc_t* loc;
   if (node->numel == 0) return phamt_empty_like(node);
   // We're going to return node at the end, so go ahead and incref it.
   Py_INCREF(node);
   if (!node->flag_transient) return node;
   // Note the starting depth and start position in general.
   path.min_depth = node->addr_depth;
   loc = path.steps + d;
   loc->index.is_beneath = 0xff;
   loc->node = node;
   // Now, iterate, starting here.
   while (1) {
      // Upon starting this loop, we are encountering the node at the given
      // depth for the first time.
      if (loc->node->flag_transient) {
         // Unset the transient bit.
         loc->node->flag_transient = 0;
         // Unless we're a twig node, we need to recurse on our children.
         if (d < PHAMT_TWIG_DEPTH) {
            // We need to recurse on this node's children; add this node to the
            // stack (path).
            d = loc->index.is_beneath;
            loc->index = phamt_firstcell(loc->node);
            loc->index.is_beneath = d; // Preserve the pevious depth!
            u = loc->node->cells[loc->index.cellindex];
            loc = path.steps + u->addr_depth;
            loc->index.is_beneath = d;
            loc->node = u;
            d = u->addr_depth;
            continue;
         }
         // Otherwise, we need to pop up the stack, so we fall through to the
         // end of this loop.
      }
      // If we reach this point, then u is either not transient, or it was a
      // twig that has now been made non-transient. Either way, we need to
      // pop up the stack (path) to find the next node in our search.
      do {
         d = loc->index.is_beneath;
         if (d > PHAMT_TWIG_DEPTH) return node;
         // Check the next node at this depth.
         loc = path.steps + d;
         d = loc->index.is_beneath;
         loc->index = phamt_nextcell(loc->node, loc->index);
         loc->index.is_beneath = d; // Preserve the pevious depth!
      } while (!loc->index.is_found);
      u = loc->node->cells[loc->index.cellindex];
      loc = path.steps + u->addr_depth;
      loc->node = u;
      loc->index.is_beneath = d;
      d = u->addr_depth;
   }
   return node;
}

// Undefine the debug statements now.
/*
#undef dbgmsg
#undef dbgci
#undef dbgpath
#undef dbgnode
*/

#ifdef __cplusplus
}
#endif

#endif // ifndef __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210

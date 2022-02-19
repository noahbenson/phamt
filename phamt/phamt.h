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
#if   (HASH_MAX >> 16 == 0)
#     define HASH_BITCOUNT 16
#     define PHAMT_ROOT_SHIFT 1
#elif (HASH_MAX >> 32 == 0)
#     define HASH_BITCOUNT 32
#     define PHAMT_ROOT_SHIFT 2
#elif (HASH_MAX >> 64 == 0)
#     define HASH_BITCOUNT 64
#     define PHAMT_ROOT_SHIFT 4
#elif (HASH_MAX >> 128 == 0)
#     define HASH_BITCOUNT 128
#     define PHAMT_ROOT_SHIFT 3
#else
#     error unhandled size for hash_t
#endif
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
// Additionally an iterator type for the PHAMT. This is not a python iterator--
// it is a C iterator type.
struct PHAMTIter {
   // This is guaranteed to be enough space for the search
   PHAMT_t node[PHAMT_LEAF_DEPTH];
   uint8_t cellindex[PHAMT_LEAF_DEPTH];
   uint8_t updepth[PHAMT_LEAF_DEPTH];
   // Whether there were more items. If the found field is 1 when the iterator
   // functions (phamt_first or phamt_next) return, then the end of the PHAMT
   // was reached.
   uint8_t found;
};


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
void* phamt_lookup(PHAMT_t node, hash_t k);
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
// phamt_first(node, iter)
// Returns the first item in the phamt node and sets the iterator accordingly.
// If the depth in the iter is ever set to 0 when this function returns, that
// means that there are no items to iterate (the return value will also be
// NULL in this case). No refcounting is performed by this function.
void* phamt_first(PHAMT_t node, struct PHAMTIter* iter);
// phamt_next(node, iter)
// Returns the next item in the phamt node and updates the iterator accordingly.
// If the depth in the iter is ever set to 0 when this function returns, that
// means that there are no items to iterate (the return value will also be
// NULL in this case). No refcounting is performed by this function.
void* phamt_next(PHAMT_t node, struct PHAMTIter* iter);


#endif // ifndef __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210

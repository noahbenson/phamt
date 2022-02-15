////////////////////////////////////////////////////////////////////////////////
// phamt/phamt.h
// Definitions for the core phamt C data structures.
// by Noah C. Benson

#ifndef __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210
#define __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210

//------------------------------------------------------------------------------
// Type Declarations

// The type of the hash.
// Python itself uses signed integers for hashes (Py_hash_t), but we want to
// make sure our code uses unsigned integers internally.
// In this case, the size_t type is the same size as Py_hash_t (which is just
// defined from Py_ssize_t, which in turn is defined from ssize_t), but size_t
// is also unsigned, so we use it.
typedef size_t hash_t;
#define HASH_MAX SIZE_MAX
// The typedef for the PHAMT exists here, but the type isn't defined in the
// header--this is just to encapsulate the immutable data.
typedef struct PHAMT* PHAMT_t;

//------------------------------------------------------------------------------
// Public API
// These functions are part of the public PHAMT C API; they can be used to
// create and edit PHAMTs.

// phamt_empty()
// Returns the empty PHAMT object; caller obtains the reference.
PHAMT_t phamt_empty(void);
// phamt_from_kv(k, v)
// Create a new PHAMT node that holds a single key-value pair.
// The returned node is fully initialized and has had the
// PyObject_GC_Track() function already called for it.
PHAMT_t phamt_from_kv(hash_t k, PyObject* v);
// phamt_lookup(node, k)
// Yields the leaf value for the hash k. If no such key is in the phamt, then
// NULL is returned.
// This function does not deal at all with INCREF or DECREF, so before returning
// anything returned from this function back to Python, be sure to INCREF it.
PyObject* phamt_lookup(PHAMT_t node, hash_t k);
// phamt_assoc(node, k, v)
// Yields a copy of the given PHAMT with the new key associated. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_assoc(PHAMT_t node, hash_t k, PyObject* v);
// phamt_dissoc(node, k)
// Yields a copy of the given PHAMT with the given key removed. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_dissoc(PHAMT_t node, hash_t k);

#endif // ifndef __phamt_phamt_h_754b8b4b82e87484dd015341f7e9d210

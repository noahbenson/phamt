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
// Debugging Code.

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
   static inline void dbgpath(const char* prefix, PHAMTPath_t* path)
   {
      char buf[1024];
      uint8_t d = path->max_depth;
      PHAMTLoc_t* loc = &path->steps[d];
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


//------------------------------------------------------------------------------
// Global Variables and Function Declarations.

static PHAMT_t PHAMT_EMPTY = NULL;
static PHAMT_t PHAMT_EMPTY_CTYPE = NULL;
#define RETURN_EMPTY ({Py_INCREF(PHAMT_EMPTY); return PHAMT_EMPTY;})
#define RETURN_EMPTY_CTYPE ({Py_INCREF(PHAMT_EMPTY_CTYPE); return PHAMT_EMPTY_CTYPE;})

// We're going to define these constructors later when we have defined the type.
static PHAMT_t phamt_new(unsigned ncells);
static PHAMT_t phamt_copy_chgcell(PHAMT_t node, PHAMTIndex_t ci, void* val);
static PHAMT_t phamt_copy_addcell(PHAMT_t node, PHAMTIndex_t ci, void* val);
static PHAMT_t phamt_copy_delcell(PHAMT_t node, PHAMTIndex_t ci);
// Additional constructors that are part of the Python interface.
static PyObject* phamt_py_from_list(PyObject* self, PyObject *const *args, Py_ssize_t nargs);


//------------------------------------------------------------------------------
// Public API Functions and their inline support functions.

// phamt_join_disjoint(node1, node2)
// Yields a single PHAMT that has as children the two PHAMTs node1 and node2.
// The nodes must be disjoint--i.e., node1 is not a subnode of node2 and node2
// is not a subnode of node1. Both nodes must have the same pyobject flag.
// This function does not update the references of either node, so this must be
// accounted for by the caller (in other words, the returned node takes the
// references PHAMTs to it). The return value has a refcount of 1.
static inline PHAMT_t phamt_join_disjoint(PHAMT_t a, PHAMT_t b)
{
   PHAMT_t u;
   uint8_t bit0, shift, newdepth;
   hash_t h;
   // What's the highest bit at which they differ?
   h = phamt_highbitdiff_hash(a->address, b->address);
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
   u->flag_firstn = is_firstn(u->bits);
   // We need to register the new node u with the garbage collector.
   PyObject_GC_Track((PyObject*)u);
   // That's all.
   return u;
}
static inline void* _phamt_lookup(PHAMT_t node, hash_t k, int* found)
{
   PHAMTIndex_t ci;
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
void* phamt_lookup(PHAMT_t node, hash_t k, int* found)
{
   return _phamt_lookup(node, k, found);
}
static inline void* _phamt_find(PHAMT_t node, hash_t k, PHAMTPath_t* path)
{
   PHAMTLoc_t* loc;
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
void* phamt_find(PHAMT_t node, hash_t k, PHAMTPath_t* path)
{
   return _phamt_find(node, k, path);
}
static inline PHAMT_t _phamt_assoc_path(PHAMTPath_t* path, hash_t k, void* newval)
{
   uint8_t dnumel = 1 - path->value_found, depth = path->max_depth;
   PHAMTLoc_t* loc = path->steps + depth;
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
      u = phamt_copy_chgcell(loc->node, loc->index, newval);
      PyObject_GC_Track((PyObject*)u);
   } else if (depth != path->edit_depth) {
      // The key isn't beneath the deepest node; we need to join a new twig
      // with the disjoint deep node.
      u = phamt_from_kv(k, newval, node->flag_pyobject);
      Py_INCREF(loc->node); // The new parent node gets this ref.
      u = phamt_join_disjoint(loc->node, u);
   } else if (depth == PHAMT_TWIG_DEPTH) {
      // We're adding a new leaf. This updates refcounts for everything
      // except the replaced cell (correctly).
      u = phamt_copy_addcell(loc->node, loc->index, newval);
      ++(u->numel);
      PyObject_GC_Track((PyObject*)u);
   } else if (node->numel == 0) {
      // We are assoc'ing to the empty node, so just return a new key-val twig.
      return phamt_from_kv(k, newval, node->flag_pyobject);
   } else {
      // We are adding a new twig to an internal node.
      node = phamt_from_kv(k, newval, node->flag_pyobject);
      // The key is beneath this node, so we insert u into it.
      u = phamt_copy_addcell(loc->node, loc->index, node);
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
      u = phamt_copy_chgcell(loc->node, loc->index, u);
      Py_DECREF(node);
      u->numel += dnumel;
      PyObject_GC_Track((PyObject*)u);
   }
   // At the end of this loop, u is the replacement node, and should be ready.
   return u;
}
static inline PHAMT_t _phamt_dissoc_path(PHAMTPath_t* path)
{
   PHAMTLoc_t* loc;
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
         u = phamt_copy_delcell(loc->node, loc->index);
         --(u->numel);
         PyObject_GC_Track((PyObject*)u);
      }
   } else {
      u = phamt_copy_delcell(loc->node, loc->index);
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
      u = phamt_copy_chgcell(loc->node, loc->index, u);
      Py_DECREF(node);
      --(u->numel);
      PyObject_GC_Track((PyObject*)u);
   }
   // At the end of this loop, u is the replacement node, and should be ready.
   return u;
}
PHAMT_t phamt_update(PHAMTPath_t* path, hash_t k, void* newval, uint8_t remove)
{
   if (remove) return _phamt_assoc_path(path, k, newval);
   else        return _phamt_dissoc_path(path);
}
// phamt_assoc(node, k, v)
// Yields a copy of the given PHAMT with the new key associated. All return
// values and touches objects should be correctly reference-tracked, and this
// function's return-value has been reference-incremented for the caller.
PHAMT_t phamt_assoc(PHAMT_t node, hash_t k, void* v)
{
   PHAMTPath_t path;
   phamt_find(node, k, &path);
   return _phamt_assoc_path(&path, k, v);
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
   PHAMTPath_t path;
   phamt_find(node, k, &path);
   return _phamt_dissoc_path(&path);
}
// phamt_apply(node, h, fn, arg)
// Applies the given function to the value with the given hash h. The function
// is called as fn(uint8_t found, void** value, void* arg); it will always be
// safe to set *value. If found is 0, then the node was not found, and if it is
// 1 then it was found. If fn returns 0, then the hash should be removed from
// the PHAMT; if 1 then the value stored in *value should be added or should
// replace the value mapped to h.
// The updated PHAMT is returned.
PHAMT_t phamt_apply(PHAMT_t node, hash_t k, phamtfn_t fn, void* arg)
{
   uint8_t rval;
   PHAMTPath_t path;
   void* val = phamt_find(node, k, &path);
   rval = (*fn)(path.value_found, &val, arg);
   if (rval) return _phamt_assoc_path(&path, k, val);
   else      return _phamt_dissoc_path(&path);
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
   int found;
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
   res = (PyObject*)phamt_lookup((PHAMT_t)self, h, &found);
   if (found) {
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
   int found;
   if (!PyLong_Check(key)) return 0;
   h = (hash_t)PyLong_AsSsize_t(key);
   key = phamt_lookup(self, h, &found);
   return found;
}
static PyObject* phamt_tp_subscript(PHAMT_t self, PyObject* key)
{
   PyObject* val;
   int found;
   hash_t h;
   if (!PyLong_Check(key)) {
      PyErr_SetObject(PyExc_KeyError, key);
      return NULL;
   }
   h = (hash_t)PyLong_AsSsize_t(key);
   val = phamt_lookup(self, h, &found);
   // We assume here that self is a pyobject PHAMT; if Python has access to a
   // ctype PHAMT then something has gone wrong already.
   if (found)
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
          "    PHAMT_SIZE:      %u\n"
          "    PHAMTPath SIZE: %u\n",
          (unsigned)PHAMT_SIZE, (unsigned)sizeof(PHAMTPath_t));
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
static PHAMT_t phamt_copy_chgcell(PHAMT_t node, PHAMTIndex_t ci, void* val)
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
static PHAMT_t phamt_copy_addcell(PHAMT_t node, PHAMTIndex_t ci, void* val)
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
static PHAMT_t phamt_copy_delcell(PHAMT_t node, PHAMTIndex_t ci)
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
static inline void* _phamt_digfirst(PHAMT_t node, PHAMTPath_t* path)
{
   PHAMTLoc_t* loc;
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
void* phamt_first(PHAMT_t node, PHAMTPath_t* path)
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
void* phamt_next(PHAMT_t node0, PHAMTPath_t* path)
{
   PHAMT_t node;
   uint8_t d, ci;
   bits_t mask;
   PHAMTLoc_t* loc;
   // We should always return from twig depth, but we can start at whatever
   // depth the path gives us, in case someone has a path pointing to the middle
   // of a phamt somewhere.
   d = path->min_depth;
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


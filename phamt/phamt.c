////////////////////////////////////////////////////////////////////////////////
// phamt/phamt.c
// Implemntation of the core phamt C data structures.
// by Noah C. Benson

// This line may be commented out to enable debugging statements in the PHAMT
// code. These are mostly sprinkled throughout the phamt.h header file in the
// various inline functions defined there.
//#define __PHAMT_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <Python.h>
#include "phamt.h"


//==============================================================================
// Function Declarations.
// This section declares all this file's functions up-font, excepting the module
// init function, which comes at the very end of the file.

//------------------------------------------------------------------------------
// PHAMT methods

static PyObject*  py_phamt_assoc(PHAMT_t self, PyObject* varargs);
static PyObject*  py_phamt_dissoc(PHAMT_t self, PyObject* varargs);
static PyObject*  py_phamt_transient(PHAMT_t self);
static PyObject*  py_phamt_get(PHAMT_t self, PyObject* varargs);
static int        py_phamt_contains(PHAMT_t self, PyObject* key);
static PyObject*  py_phamt_subscript(PHAMT_t self, PyObject* key);
static Py_ssize_t py_phamt_len(PHAMT_t self);
static PyObject*  py_phamt_iter(PHAMT_t self);
static void       py_phamt_dealloc(PHAMT_t self);
static int        py_phamt_traverse(PHAMT_t self, visitproc visit, void *arg);
static int        py_phamt_clear(PHAMT_t self);
static PyObject*  py_phamt_repr(PHAMT_t self);

//------------------------------------------------------------------------------
// PHAMT_iter Methods

static void      py_phamtiter_dealloc(PHAMT_iter_t self);
static int       py_phamtiter_traverse(PHAMT_iter_t self, visitproc visit,
                                       void *arg);
static int       py_phamtiter_clear(PHAMT_iter_t self);
static PyObject* py_phamtiter_repr(PHAMT_iter_t self);
static PyObject* py_phamtiter_iter(PHAMT_iter_t self);
static PyObject* py_phamtiter_next(PHAMT_iter_t self);

//------------------------------------------------------------------------------
// PHAMT-type methods (i.e., classmethods)

static PyObject* py_PHAMT_getitem(PyObject *type, PyObject *item);
static PyObject* py_PHAMT_from_iter(PyObject* self, PyObject *const *args,
                                    Py_ssize_t nargs);

//------------------------------------------------------------------------------
// THAMT methods

static PyObject*  py_thamt_get(THAMT_t self, PyObject* varargs);
static PyObject*  py_thamt_persistent(THAMT_t self);
static int        py_thamt_contains(THAMT_t self, PyObject* key);
static PyObject*  py_thamt_subscript(THAMT_t self, PyObject* key);
static int        py_thamt_ass_subscript(THAMT_t self, PyObject *key,
                                         PyObject *v);
static Py_ssize_t py_thamt_len(THAMT_t self);
static PyObject*  py_thamt_iter(THAMT_t self);
static void       py_thamt_dealloc(THAMT_t self);
static int        py_thamt_traverse(THAMT_t self, visitproc visit, void *arg);
static int        py_thamt_clear(THAMT_t self);
static PyObject*  py_thamt_repr(THAMT_t self);
static PyObject*  py_thamt_new(PyTypeObject *subtype, PyObject *args,
                               PyObject *kwds);

//------------------------------------------------------------------------------
// THAMT_iter Methods

static void      py_thamtiter_dealloc(THAMT_iter_t self);
static int       py_thamtiter_traverse(THAMT_iter_t self, visitproc visit,
                                       void *arg);
static int       py_thamtiter_clear(THAMT_iter_t self);
static PyObject* py_thamtiter_repr(THAMT_iter_t self);
static PyObject* py_thamtiter_iter(THAMT_iter_t self);
static PyObject* py_thamtiter_next(THAMT_iter_t self);

//------------------------------------------------------------------------------
// THAMT-type methods (i.e., classmethods)

static PyObject* py_THAMT_getitem(PyObject *type, PyObject *item);

//------------------------------------------------------------------------------
// Module-level Functions

static void py_phamtmod_free(void* mod);


//==============================================================================
// Static Variables
// This section defines all the variables that are local to this file (and thus
// to the phamt.c_core module's scope, in effect). These are mostly used or
// initialized in the PyCore_Init() function below.

//------------------------------------------------------------------------------
// The Empty PHAMTs

// The empty (Python object) PHAMT.
static PHAMT_t PHAMT_EMPTY = NULL;
// The empty (C type) PHAMT.
static PHAMT_t PHAMT_EMPTY_CTYPE = NULL;

//------------------------------------------------------------------------------
// Python Data Structures
// These values represent data structures that define the Python-C interface for
// the phamt.c_core module.

// PHAMTs ......................................................................
// The PHAMT class methods.
static PyMethodDef PHAMT_methods[] = {
   {"get",               (PyCFunction)py_phamt_get, METH_VARARGS, NULL},
   {"__class_getitem__", (PyCFunction)py_PHAMT_getitem, METH_O|METH_CLASS, NULL},
   {"assoc",             (PyCFunction)py_phamt_assoc, METH_VARARGS,
                         PyDoc_STR(PHAMT_ASSOC_DOCSTRING)},
   {"dissoc",            (PyCFunction)py_phamt_dissoc, METH_VARARGS,
                         PyDoc_STR(PHAMT_DISSOC_DOCSTRING)},
   {"transient",         (PyCFunction)py_phamt_transient, METH_NOARGS,
                         PyDoc_STR(PHAMT_TRANSIENT_DOCSTRING)},
   {"from_iter",         (PyCFunction)py_PHAMT_from_iter,
                         METH_FASTCALL|METH_CLASS,
                         PyDoc_STR(PHAMT_FROM_ITER_DOCSTRING)},
   {NULL, NULL, 0, NULL}
};
// The PHAMT implementation of the sequence interface.
static PySequenceMethods PHAMT_as_sequence = {
   0,                             // sq_length
   0,                             // sq_concat
   0,                             // sq_repeat
   0,                             // sq_item
   0,                             // sq_slice
   0,                             // sq_ass_item
   0,                             // sq_ass_slice
   (objobjproc)py_phamt_contains, // sq_contains
   0,                             // sq_inplace_concat
   0,                             // sq_inplace_repeat
};
// The PHAMT implementation of the Mapping interface.
static PyMappingMethods PHAMT_as_mapping = {
   (lenfunc)py_phamt_len,          // mp_length
   (binaryfunc)py_phamt_subscript, // mp_subscript
   NULL
};
// The PHAMT Type object data.
static PyTypeObject PHAMT_type = {
   //PyVarObject_HEAD_INIT(&PyType_Type, 0)
   PyVarObject_HEAD_INIT(NULL, 0)
   "phamt.c_core.PHAMT",
   .tp_doc = PyDoc_STR(PHAMT_DOCSTRING),
   .tp_basicsize = PHAMT_SIZE,
   .tp_itemsize = sizeof(void*),
   .tp_methods = PHAMT_methods,
   .tp_as_mapping = &PHAMT_as_mapping,
   .tp_as_sequence = &PHAMT_as_sequence,
   .tp_iter = (getiterfunc)py_phamt_iter,
   .tp_dealloc = (destructor)py_phamt_dealloc,
   .tp_getattro = PyObject_GenericGetAttr,
   .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
   .tp_traverse = (traverseproc)py_phamt_traverse,
   .tp_clear = (inquiry)py_phamt_clear,
   .tp_repr = (reprfunc)py_phamt_repr,
   .tp_str = (reprfunc)py_phamt_repr
};
// The PHAMT_iter Type object data.
static PyTypeObject PHAMT_iter_type = {
   //PyVarObject_HEAD_INIT(&PyType_Type, 0)
   PyVarObject_HEAD_INIT(NULL, 0)
   .tp_name = "phamt.c_core.PHAMT_iter",
   .tp_basicsize = sizeof(struct PHAMT_iter),
   .tp_itemsize = 0,
   .tp_dealloc = (destructor)py_phamtiter_dealloc,
   .tp_repr = (reprfunc)py_phamtiter_repr,
   .tp_str = (reprfunc)py_phamtiter_repr,
   .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
   .tp_traverse = (traverseproc)py_phamtiter_traverse,
   .tp_clear = (inquiry)py_phamtiter_clear,
   .tp_iter = (getiterfunc)py_phamtiter_iter,
   .tp_iternext = (iternextfunc)py_phamtiter_next,
};

// THAMTs ......................................................................
// The THAMT class methods.
static PyMethodDef THAMT_methods[] = {
   {"get",               (PyCFunction)py_thamt_get,        METH_VARARGS,
                         NULL},
   {"persistent",        (PyCFunction)py_thamt_persistent, METH_NOARGS,
                         THAMT_PERSISTENT_DOCSTRING},
   {"__class_getitem__", (PyCFunction)py_THAMT_getitem,    METH_O|METH_CLASS,
                         NULL},
   {NULL, NULL, 0, NULL}
};
// The THAMT implementation of the sequence interface.
static PySequenceMethods THAMT_as_sequence = {
   0,                             // sq_length
   0,                             // sq_concat
   0,                             // sq_repeat
   0,                             // sq_item
   0,                             // sq_slice
   0,                             // sq_ass_item
   0,                             // sq_ass_slice
   (objobjproc)py_thamt_contains, // sq_contains
   0,                             // sq_inplace_concat
   0,                             // sq_inplace_repeat
};
// The THAMT implementation of the Mapping interface.
static PyMappingMethods THAMT_as_mapping = {
   (lenfunc)py_thamt_len,                // mp_length
   (binaryfunc)py_thamt_subscript,       // mp_subscript
   (objobjargproc)py_thamt_ass_subscript // mp_ass_subscript
};
// The THAMT Type object data.
static PyTypeObject THAMT_type = {
   //PyVarObject_HEAD_INIT(&PyType_Type, 0)
   PyVarObject_HEAD_INIT(NULL, 0)
   "phamt.c_core.THAMT",
   .tp_doc = PyDoc_STR(THAMT_DOCSTRING),
   .tp_basicsize = sizeof(struct THAMT),
   .tp_itemsize = 0,
   .tp_methods = THAMT_methods,
   .tp_as_mapping = &THAMT_as_mapping,
   .tp_as_sequence = &THAMT_as_sequence,
   .tp_iter = (getiterfunc)py_thamt_iter,
   .tp_dealloc = (destructor)py_thamt_dealloc,
   .tp_getattro = PyObject_GenericGetAttr,
   .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
   .tp_traverse = (traverseproc)py_thamt_traverse,
   // PHAMTs, like tuples, can't make ref links because they are 100% immutable.
   .tp_clear = (inquiry)py_thamt_clear,
   .tp_new = (newfunc)py_thamt_new,
   .tp_repr = (reprfunc)py_thamt_repr,
   .tp_str = (reprfunc)py_thamt_repr,
};
// The THAMT_iter Type object data.
static PyTypeObject THAMT_iter_type = {
   //PyVarObject_HEAD_INIT(&PyType_Type, 0)
   PyVarObject_HEAD_INIT(NULL, 0)
   .tp_name = "phamt.c_core.THAMT_iter",
   .tp_basicsize = sizeof(struct THAMT_iter),
   .tp_itemsize = 0,
   .tp_dealloc = (destructor)py_thamtiter_dealloc,
   .tp_repr = (reprfunc)py_thamtiter_repr,
   .tp_str = (reprfunc)py_thamtiter_repr,
   .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
   .tp_traverse = (traverseproc)py_thamtiter_traverse,
   .tp_clear = (inquiry)py_thamtiter_clear,
   .tp_iter = (getiterfunc)py_thamtiter_iter,
   .tp_iternext = (iternextfunc)py_thamtiter_next,
};

// The phamt.c_core module data.
static struct PyModuleDef phamt_pymodule = {
   PyModuleDef_HEAD_INIT,
   "c_core",
   NULL,
   -1,
   NULL,
   NULL,
   NULL,
   NULL,
   py_phamtmod_free
};


//==============================================================================
// Python-C Interface Code 
// This section contains the implementatin of the PHAMT methods and the PHAMT
// type functions for the Python-C interface.

//------------------------------------------------------------------------------
// PHAMT methods

static PyObject* py_phamt_assoc(PHAMT_t self, PyObject* varargs)
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
   return (PyObject*)phamt_assoc(self, h, val);
}
static PyObject* py_phamt_dissoc(PHAMT_t self, PyObject* varargs)
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
   return (PyObject*)phamt_dissoc(self, h);
}
static PyObject* py_phamt_transient(PHAMT_t self)
{
   THAMT_t u = (THAMT_t)PyObject_GC_NewVar(struct THAMT, &THAMT_type, 0);
   Py_INCREF(self);
   u->phamt = self;
   u->version = 0;
   PyObject_GC_Track((PyObject*)u);
   return (PyObject*)u;
}
static PyObject* py_phamt_get(PHAMT_t self, PyObject* varargs)
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
   res = (PyObject*)phamt_lookup(self, h, &found);
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
static int py_phamt_contains(PHAMT_t self, PyObject* key)
{
   hash_t h;
   int found;
   if (!PyLong_Check(key)) return 0;
   h = (hash_t)PyLong_AsSsize_t(key);
   key = phamt_lookup(self, h, &found);
   return found;
}
static PyObject* py_phamt_subscript(PHAMT_t self, PyObject* key)
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
static Py_ssize_t py_phamt_len(PHAMT_t self)
{
   return (Py_ssize_t)self->numel;
}
static PyObject *py_phamt_iter(PHAMT_t self)
{
   PHAMT_iter_t it = (PHAMT_iter_t)PyObject_GC_NewVar(struct PHAMT_iter,
                                                      &PHAMT_iter_type, 0);
   uint8_t d = self->addr_depth;
   Py_INCREF(self);
   it->path.steps[d].node = self;
   it->path.min_depth = d;
   it->path.value_found = 0xff; // indicates we haven't started.
   PyObject_GC_Track(it);
   return (PyObject*)it;
}
static void py_phamt_dealloc(PHAMT_t self)
{
   PyTypeObject* tp = Py_TYPE(self);
   // Untrack ourself.
   PyObject_GC_UnTrack(self);
   // Clear the children.
   py_phamt_clear(self);
   // Free the node.
   tp->tp_free(self);
}
static int py_phamt_traverse(PHAMT_t self, visitproc visit, void *arg)
{
   hash_t ii, ncells;
   PyTypeObject* tp;
   tp = Py_TYPE(self);
   Py_VISIT(tp);
   if (self->addr_depth == PHAMT_TWIG_DEPTH && !self->flag_pyobject)
      return 0;
   if (self->flag_full) {
      for (ncells = self->bits; ncells; ncells &= ~(BITS_ONE << ii)) {
         ii = ctz_bits(ncells);
         Py_VISIT(((PHAMT_t)self)->cells[ii]);
      }
   } else {
      ncells = phamt_cellcount(self);
      for (ii = 0; ii < ncells; ++ii) {
         Py_VISIT(((PHAMT_t)self)->cells[ii]);
      }
   }
   return 0;
}
static int py_phamt_clear(PHAMT_t self)
{
   bits_t ii, ncells;
   // Walk through the children, clearing them.
   if (self->addr_depth < PHAMT_TWIG_DEPTH || self->flag_pyobject) {
      if (self->flag_full) {
         // Use ncells as the iteration variable since we won't need it.
         for (ncells = self->bits; ncells; ncells &= ~(BITS_ONE << ii)) {
            ii = ctz_bits(ncells);
            Py_CLEAR(self->cells[ii]);
         }
      } else {
         ncells = phamt_cellcount(self);
         for (ii = 0; ii < ncells; ++ii) {
            Py_CLEAR(self->cells[ii]);
         }
      }
   }
   return 0;
}
static PyObject* py_phamt_repr(PHAMT_t self)
{
   dbgnode("[py_phamt_repr]", self);
   return PyUnicode_FromFormat("<PHAMT:n=%u>", (unsigned)self->numel);
}

//------------------------------------------------------------------------------
// PHAMT Constructors
// These are constructors intended for use in the C-API, not thee Python
// constructors.

// phamt_empty()
// Returns the empty PHAMT--this is not static because we want it to be
// available to other C modules. (It is in fact declared in the header file.)
PHAMT_t phamt_empty(void)
{
   Py_INCREF(PHAMT_EMPTY);
   return PHAMT_EMPTY;
}
// phamt_empty_ctype()
// Returns the empty PHAMT whose objects must be C-types.
PHAMT_t phamt_empty_ctype(void)
{
   Py_INCREF(PHAMT_EMPTY_CTYPE);
   return PHAMT_EMPTY_CTYPE;
}
// _phamt_new(ncells)
// Returns a newly allocated PHAMT object with the given number of cells. The
// PHAMT has a refcount of 1 but it's PHAMT data are not initialized.
PHAMT_t _phamt_new(unsigned ncells)
{
   return (PHAMT_t)PyObject_GC_NewVar(struct PHAMT, &PHAMT_type, ncells);
}

//------------------------------------------------------------------------------
// PHAMT_iter Methods

static void py_phamtiter_dealloc(PHAMT_iter_t self)
{
   PyTypeObject* tp;
   tp = Py_TYPE(self);
   // Untrack ourself.
   PyObject_GC_UnTrack(self);
   // Clear the children.
   py_phamtiter_clear(self);
   // Free the object.
   tp->tp_free(self);
}
static int py_phamtiter_traverse(PHAMT_iter_t self, visitproc visit, void *arg)
{
   Py_VISIT(Py_TYPE(self));
   Py_VISIT(self->path.steps[self->path.min_depth].node);
   return 0;
}
static int py_phamtiter_clear(PHAMT_iter_t self)
{
   Py_CLEAR(self->path.steps[self->path.min_depth].node);   
   return 0;
}
static PyObject* py_phamtiter_repr(PHAMT_iter_t self)
{
   return PyUnicode_FromString("<PHAMT_iter>");
}
static PyObject* py_phamtiter_iter(PHAMT_iter_t self)
{
   Py_INCREF(self);
   return (PyObject*)self;
}
static PyObject* py_phamtiter_next(PHAMT_iter_t self)
{
   PHAMT_loc_t* loc;
   PHAMT_t node;
   void* val;
   hash_t key;
   // Depending on whether iteration hasn't started, has alerady ended, or is
   // ongoing, we handle this differently.
   loc = self->path.steps + self->path.min_depth;
   node = loc->node;
   if (self->path.value_found == 0xff) 
      val = phamt_first(node, &self->path);
   else if (self->path.value_found)
      val = phamt_next(node, &self->path);
   // If there aren't any more, raise the stop-iteration exception.
   if (!self->path.value_found) {
      PyErr_SetNone(PyExc_StopIteration);
      return NULL;
   }
   // Otherwise, make a tuple and return it. The key can be derived from the
   // path.
   loc = self->path.steps + self->path.max_depth;
   key = loc->node->address | (hash_t)loc->index.bitindex;
   return Py_BuildValue("(nO)", (Py_ssize_t)key, val);
}

//------------------------------------------------------------------------------
// THAMT Methods

static PyObject* py_thamt_get(THAMT_t self, PyObject* varargs)
{
   return py_phamt_get(self->phamt, varargs);
}
static PyObject* py_thamt_persistent(THAMT_t self)
{
   return (PyObject*)thamt_persist(self->phamt);
}
static int py_thamt_contains(THAMT_t self, PyObject* key)
{
   return py_phamt_contains(self->phamt, key);
}
static PyObject* py_thamt_subscript(THAMT_t self, PyObject* key)
{
   return py_phamt_subscript(self->phamt, key);
}
static int py_thamt_ass_subscript(THAMT_t self, PyObject* key, PyObject* val)
{
   PHAMT_t u;
   PHAMT_path_t path;
   hash_t h;
   if (!PyLong_Check(key)) {
      PyErr_SetObject(PyExc_KeyError, key);
      return -1;
   }
   h = (hash_t)PyLong_AsSsize_t(key);
   u = self->phamt;
   if (val) {
      self->phamt = thamt_assoc(self->phamt, h, val);
   } else {
      // Find the location we're going to delete.
      phamt_find(self->phamt, h, &path);
      // We need to raise a key error when h is not found.
      if (!path.value_found) {
         PyErr_SetObject(PyExc_KeyError, key);
         return -1;
      }
      self->phamt = _thamt_dissoc_path(&path);
   }
   Py_DECREF(u);
   ++(self->version);
   return 0;
}
static Py_ssize_t py_thamt_len(THAMT_t self)
{
   return (Py_ssize_t)self->phamt->numel;
}
static PyObject *py_thamt_iter(THAMT_t self)
{
   THAMT_iter_t it = (THAMT_iter_t)PyObject_GC_NewVar(struct THAMT_iter,
                                                      &THAMT_iter_type, 0);
   uint8_t d = self->phamt->addr_depth;
   Py_INCREF(self);
   it->thamt = self;
   it->version = self->version;
   it->path.steps[d].node = self->phamt;
   it->path.min_depth = d;
   it->path.value_found = 0xff; // indicates we haven't started.
   PyObject_GC_Track(it);
   return (PyObject*)it;
}
static void py_thamt_dealloc(THAMT_t self)
{
   PyTypeObject* tp = Py_TYPE(self);
   // Untrack ourself.
   PyObject_GC_UnTrack(self);
   // Clear the children.
   py_thamt_clear(self);
   // Free the node.
   tp->tp_free(self);
}
static int py_thamt_traverse(THAMT_t self, visitproc visit, void *arg)
{
   PyTypeObject* tp = Py_TYPE(self);
   Py_VISIT(tp);
   Py_VISIT(self->phamt);
   return 0;
}
static int py_thamt_clear(THAMT_t self)
{
   Py_CLEAR(self->phamt);
   return 0;
}
static PyObject* py_thamt_repr(THAMT_t self)
{
   dbgnode("[py_thamt_repr]", self->phamt);
   return PyUnicode_FromFormat("<THAMT:n=%u>", (unsigned)self->phamt->numel);
}
static PyObject* py_thamt_new(PyTypeObject *subtype, PyObject *args,
                              PyObject *kw)
{
   PyObject* tmp;
   PHAMT_t p;
   THAMT_t u;
   if (kw && PyDict_Size(kw) > 0) {
      PyErr_SetString(PyExc_TypeError, "THAMT() takes no keyword arguments");
      return NULL;
   }
   Py_ssize_t sz = PyTuple_Size(args);
   if (sz == 1) {
      if (!PyArg_ParseTuple(args, "O:get", &tmp))
         return NULL;
      if (Py_TYPE(tmp) != &PHAMT_type) {
         PyErr_SetString(PyExc_TypeError, "THAMT() argument must be a PHAMT");
         return NULL;
      }
      p = (PHAMT_t)tmp;
   } else if (sz == 0) {
      p = PHAMT_EMPTY;
   } else {
      PyErr_SetString(PyExc_ValueError, "THAMT() requires 0 or 1 arguments");
      return NULL;
   }
   u = (THAMT_t)PyObject_GC_NewVar(struct THAMT, &THAMT_type, 0);
   Py_INCREF(p);
   u->phamt = p;
   u->version = 0;
   PyObject_GC_Track((PyObject*)u);
   return (PyObject*)u;
}

//------------------------------------------------------------------------------
// THAMT_iter Methods

static void py_thamtiter_dealloc(THAMT_iter_t self)
{
   PyTypeObject* tp;
   tp = Py_TYPE(self);
   // Untrack ourself.
   PyObject_GC_UnTrack(self);
   // Clear the children.
   py_thamtiter_clear(self);
   // Free the object.
   tp->tp_free(self);
}
static int py_thamtiter_traverse(THAMT_iter_t self, visitproc visit, void *arg)
{
   Py_VISIT(Py_TYPE(self));
   Py_VISIT(self->thamt);
   return 0;
}
static int py_thamtiter_clear(THAMT_iter_t self)
{
   Py_CLEAR(self->thamt);
   return 0;
}
static PyObject* py_thamtiter_repr(THAMT_iter_t self)
{
   return PyUnicode_FromString("<THAMT_iter>");
}
static PyObject* py_thamtiter_iter(THAMT_iter_t self)
{
   Py_INCREF(self);
   return (PyObject*)self;
}
static PyObject* py_thamtiter_next(THAMT_iter_t self)
{
   PHAMT_loc_t* loc;
   PHAMT_t node;
   void* val;
   hash_t key;
   // If the  of the iterator doesn't match the version of the THAMT,
   // that's a runtime error.
   if (self->version != self->thamt->version) {
      PyErr_SetString(PyExc_RuntimeError,
                      "THAMT updated during iteration");
      return NULL;
   }
   // Depending on whether iteration hasn't started, has alerady ended, or is
   // ongoing, we handle this differently.
   node = self->thamt->phamt;
   if (self->path.value_found == 0xff)
      val = phamt_first(node, &self->path);
   else if (self->path.value_found)
      val = phamt_next(node, &self->path);
   // If there aren't any more, raise the stop-iteration exception.
   if (!self->path.value_found) {
      PyErr_SetNone(PyExc_StopIteration);
      return NULL;
   }
   // Otherwise, make a tuple and return it. The key can be derived from the
   // path.
   loc = self->path.steps + self->path.max_depth;
   key = loc->node->address | (hash_t)loc->index.bitindex;
   dbgpath("[thamtiter_next]", &self->path);
   return Py_BuildValue("(nO)", (Py_ssize_t)key, val);
}

//------------------------------------------------------------------------------
// PHAMT-Type Methods

static PyObject *py_PHAMT_getitem(PyObject *type, PyObject *item)
{
   Py_INCREF(type);
   return type;
}
// PHAMT.from_iter(list)
// Returns a new PHAMT whose keys are 0, 1, 2... and whose values are the items
// in the given list in order.
static PyObject* py_PHAMT_from_iter(PyObject* self, PyObject *const *args,
                                    Py_ssize_t nargs)
{
   PyObject* arg, *it;
   PHAMT_t thamt;
   hash_t k = 0;
   if (nargs > 2 || nargs < 1) {
      PyErr_SetString(PyExc_ValueError, "PHAMT.from_iter requires 1 or 2 args");
      return NULL;
   } else if (nargs == 2) {
      arg = args[1];
      if (!PyLong_Check(arg)) {
         PyErr_SetString(PyExc_ValueError, "k0 must be a long integer");
         return NULL;
      }
      k = (hash_t)PyLong_AsSsize_t(arg);
   }
   arg = args[0];
   // Get the iterator:
   it = PyObject_GetIter(arg);
   if (it == NULL)
      return NULL;
   // We start with the empty PHAMT and build up the entire THAMT.
   thamt = PHAMT_EMPTY;
   while ((arg = PyIter_Next(it))) {
      thamt = thamt_assoc(thamt, k++, arg);
      Py_DECREF(arg);
   }
   // We're done with the iterator now.
   Py_DECREF(it);
   // If there was an error, we just return NULL too propogate it.
   if (PyErr_Occurred())
      return NULL;
   // Otherwise, we juust need to return the persistent PHAMT.
   return (PyObject*)thamt_persist(thamt);
}

//------------------------------------------------------------------------------
// THAMT-Type Methods

static PyObject *py_THAMT_getitem(PyObject *type, PyObject *item)
{
   Py_INCREF(type);
   return type;
}

//------------------------------------------------------------------------------
// Functions for the phamt.c_core Module

// Free the module when it is unloaded.
static void py_phamtmod_free(void* mod)
{
   PHAMT_t tmp = PHAMT_EMPTY;
   PHAMT_EMPTY = NULL;
   Py_DECREF(tmp);
   tmp = PHAMT_EMPTY_CTYPE;
   PHAMT_EMPTY_CTYPE = NULL;
   Py_DECREF(tmp);
}
// The moodule's initialization function.
PyMODINIT_FUNC PyInit_c_core(void)
{
   PyObject* m = PyModule_Create(&phamt_pymodule);
   if (m == NULL) return NULL;
   // Initialize the PHAMT_type a tp_dict.
   if (PyType_Ready(&PHAMT_type) < 0) return NULL;
   Py_INCREF(&PHAMT_type);
   // Same for the others.
   if (PyType_Ready(&PHAMT_iter_type) < 0) return NULL;
   Py_INCREF(&PHAMT_iter_type);
   if (PyType_Ready(&THAMT_type) < 0) return NULL;
   Py_INCREF(&THAMT_type);
   if (PyType_Ready(&THAMT_iter_type) < 0) return NULL;
   Py_INCREF(&THAMT_iter_type);
   // Get the Empty PHAMT ready.
   PHAMT_EMPTY = (PHAMT_t)PyObject_GC_NewVar(struct PHAMT, &PHAMT_type, 0);
   if (!PHAMT_EMPTY) return NULL;
   PHAMT_EMPTY->address = 0;
   PHAMT_EMPTY->numel = 0;
   PHAMT_EMPTY->bits = 0;
   PHAMT_EMPTY->flag_transient = 0;
   PHAMT_EMPTY->flag_firstn = 0;
   PHAMT_EMPTY->flag_full = 0;
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
   PHAMT_EMPTY_CTYPE->flag_full = 0;
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
   // The THAMT type.
   if (PyModule_AddObject(m, "THAMT", (PyObject*)&THAMT_type) < 0) {
      Py_DECREF(&THAMT_type);
      return NULL;
   }
   // Debugging things that are useful to print.
   dbgmsg("Initialized PHAMT C API.\n"
          "    PHAMT size:      %u\n"
          "    PHAMT path SIZE: %u\n",
          (unsigned)PHAMT_SIZE, 
          (unsigned)sizeof(PHAMT_path_t));
   // Return the module!
   return m;
}

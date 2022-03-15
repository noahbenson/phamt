////////////////////////////////////////////////////////////////////////////////
// phamt/phamt.c
// Implemntation of the core phamt C data structures.
// by Noah C. Benson

//#define __PHAMT_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <Python.h>
#include "phamt.h"


//------------------------------------------------------------------------------
// Global Variables and Function Declarations.

static PHAMT_t PHAMT_EMPTY = NULL;
static PHAMT_t PHAMT_EMPTY_CTYPE = NULL;
#define RETURN_EMPTY         \
   ({Py_INCREF(PHAMT_EMPTY); \
     return PHAMT_EMPTY;})
#define RETURN_EMPTY_CTYPE         \
   ({Py_INCREF(PHAMT_EMPTY_CTYPE); \
     return PHAMT_EMPTY_CTYPE;})

// Additional constructors that are part of the Python interface.
static PyObject* phamt_py_from_list(PyObject* self, PyObject *const *args, Py_ssize_t nargs);


//------------------------------------------------------------------------------
// Public API Functions and their inline support functions.

PHAMT_t thamt_assoc(PHAMT_t node, hash_t k, PyObject* v)
{
   return NULL; // #TODO
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
   {"__class_getitem__", (PyCFunction)PHAMT_py_getitem, METH_O|METH_CLASS, NULL},
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
          (unsigned)PHAMT_SIZE, (unsigned)sizeof(PHAMT_path_t));
   // Return the module!
   return m;
}


//------------------------------------------------------------------------------
// Constructors
// All of the constructors in this section make an attemt to correctly handle
// the ref-counting associated with copying nodes. Please be careful with this!

PHAMT_t phamt_empty(void)
{
   RETURN_EMPTY;
}
PHAMT_t phamt_empty_ctype(void)
{
   RETURN_EMPTY_CTYPE;
}
PHAMT_t _phamt_new(unsigned ncells)
{
   return (PHAMT_t)PyObject_GC_NewVar(struct PHAMT, &PHAMT_type, ncells);
}
static PyObject* phamt_py_from_list(PyObject* self, PyObject *const *args, Py_ssize_t nargs)
{
   return NULL; // #TODO
}


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

// PHAMT methods
static PyObject*  py_phamt_assoc(PyObject* self, PyObject* varargs);
static PyObject*  py_phamt_dissoc(PyObject* self, PyObject* varargs);
static PyObject*  py_phamt_get(PyObject* self, PyObject* varargs);
static int        py_phamt_contains(PHAMT_t self, PyObject* key);
static PyObject*  py_phamt_subscript(PHAMT_t self, PyObject* key);
static Py_ssize_t py_phamt_len(PHAMT_t self);
static PyObject*  py_phamt_iter(PHAMT_t self);
static void       py_phamt_dealloc(PHAMT_t self);
static int        py_phamt_traverse(PHAMT_t self, visitproc visit, void *arg);
static PyObject*  py_phamt_repr(PHAMT_t self);
static PyObject*  py_phamt_new(PyTypeObject *subtype, PyObject *args,
                               PyObject *kwds);
// PHAMT type methods (i.e., classmethods)
static PyObject* py_PHAMT_getitem(PyObject *type, PyObject *item);
static PyObject* py_PHAMT_from_list(PyObject* self, PyObject *const *args,
                                    Py_ssize_t nargs);
// Module-level functions
static void py_phamtmod_free(void* mod);


//==============================================================================
// Static Variables
// This section defines all the variables that are local to this file (and thus
// to the phamt.core module's scope, in effect). These are mostly used or
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
// the phamt.core module.

// The PHAMT class methods.
static PyMethodDef PHAMT_methods[] = {
   {"get",               (PyCFunction)py_phamt_get, METH_VARARGS, NULL},
   {"__class_getitem__", (PyCFunction)py_PHAMT_getitem, METH_O|METH_CLASS, NULL},
   {"assoc",             (PyCFunction)py_phamt_assoc, METH_VARARGS,
                         PyDoc_STR(PHAMT_ASSOC_DOCSTRING)},
   {"dissoc",            (PyCFunction)py_phamt_dissoc, METH_VARARGS,
                         PyDoc_STR(PHAMT_DISSOC_DOCSTRING)},
   {"from_list",         (PyCFunction)py_PHAMT_from_list, METH_FASTCALL,
                         PyDoc_STR(PHAMT_FROM_LIST_DOCSTRING)},
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
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "phamt.core.PHAMT",
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
   // PHAMTs, like tuples, can't make ref links because they are 100% immutable.
   //.tp_clear = (inquiry)py_phamt_clear,
   .tp_new = py_phamt_new,
   .tp_repr = (reprfunc)py_phamt_repr,
   .tp_str = (reprfunc)py_phamt_repr,
};
// The phamt.core module data.
static struct PyModuleDef phamt_pymodule = {
   PyModuleDef_HEAD_INIT,
   "core",
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

static PyObject* py_phamt_assoc(PyObject* self, PyObject* varargs)
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
static PyObject* py_phamt_dissoc(PyObject* self, PyObject* varargs)
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
static PyObject* py_phamt_get(PyObject* self, PyObject* varargs)
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
   return NULL; // #TODO
}
static void py_phamt_dealloc(PHAMT_t self)
{
   PyTypeObject* tp;
   PyObject* tmp;
   bits_t ii, ncells;
   tp = Py_TYPE(self);
   PyObject_GC_UnTrack(self);
   // Walk through the children, dereferencing them
   if (self->addr_depth < PHAMT_TWIG_DEPTH || self->flag_pyobject) {
      if (self->flag_full) {
         // Use ncells as the iteration variable since we won't need it.
         for (ncells = self->bits; ncells; ncells &= ~(BITS_ONE << ii)) {
            ii = ctz_bits(ncells);
            tmp = self->cells[ii];
            self->cells[ii] = NULL;
            Py_DECREF(tmp);
         }
      } else {
         ncells = phamt_cellcount(self);
         for (ii = 0; ii < ncells; ++ii) {
            tmp = self->cells[ii];
            self->cells[ii] = NULL;
            Py_DECREF(tmp);
         }
      }
   }
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
static PyObject* py_phamt_repr(PHAMT_t self)
{
   dbgnode("[py_phamt_repr]", self);
   return PyUnicode_FromFormat("<PHAMT:n=%u>", (unsigned)self->numel);
}
static PyObject* py_phamt_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
   // #TODO
   Py_INCREF(PHAMT_EMPTY);
   return (PyObject*)PHAMT_EMPTY;
}

//------------------------------------------------------------------------------
// PHAMT Constructors
// These are constructors intended for use in the C-API, not thee Python
// constructors, which are included above (see py_phamt_new).

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
// PHAMT Type Methods

static PyObject *py_PHAMT_getitem(PyObject *type, PyObject *item)
{
   Py_INCREF(type);
   return type;
}
// PHAMT.from_list(list)
// Returns a new PHAMT whose keys are 0, 1, 2... and whose values are the items
// in the given list in order.
static PyObject* py_PHAMT_from_list(PyObject* self, PyObject *const *args,
                                    Py_ssize_t nargs)
{
   return NULL; // #TODO
}

//------------------------------------------------------------------------------
// Functions for the phamt.core Module

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
   // Debugging things that are useful to print.
   dbgmsg("Initialized PHAMT C API.\n"
          "    PHAMT size:      %u\n"
          "    PHAMT path SIZE: %u\n",
          (unsigned)PHAMT_SIZE, (unsigned)sizeof(PHAMT_path_t));
   // Return the module!
   return m;
}

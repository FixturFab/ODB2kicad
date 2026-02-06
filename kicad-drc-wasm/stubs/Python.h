#pragma once
// Minimal Python.h stub for WASM build - no Python support

typedef struct _object PyObject;
typedef struct _typeobject PyTypeObject;
typedef long Py_ssize_t;

#define Py_None ((PyObject*)0)
#define Py_True ((PyObject*)1)
#define Py_False ((PyObject*)2)

#define PyObject_HEAD int ob_refcnt; PyTypeObject *ob_type;

inline void Py_Initialize() {}
inline int Py_IsInitialized() { return 0; }
inline void Py_Finalize() {}
inline int PyRun_SimpleString(const char*) { return -1; }
inline PyObject* PyImport_ImportModule(const char*) { return nullptr; }
inline void Py_XDECREF(PyObject*) {}
inline void Py_DECREF(PyObject*) {}
inline void Py_INCREF(PyObject*) {}
inline PyObject* PyUnicode_FromString(const char*) { return nullptr; }
inline const char* PyUnicode_AsUTF8(PyObject*) { return ""; }

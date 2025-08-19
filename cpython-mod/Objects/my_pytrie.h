#ifndef PY_MYMOD_H
#define PY_MYMOD_H
#include "Python.h"

typedef struct
{
  Py_UCS4 ch;
  /*
   * Es un dict
   * Clave: el ch
   * Valor: el *PyTrieNode encapsulado
   */
  PyObject *children;
  int is_terminal;
  int freed;
  
} PyTrieNode;

typedef struct {
  PyObject_HEAD

  /*
   * También es un dict
   * Clave: el ch
   * Valor: el *PyTrieNode encapsulado
   */
  PyObject *root;
  PyObject *strs; // lista con las palabras
} PyTrieObject;

void
PyTrie_dealloc(PyTrieObject *self);

extern PyTypeObject PyTrie_Type;
extern PyMethodDef PyTrie_methods[];

int PyTrie_init(PyTrieObject *self, PyObject *args, PyObject *kwds);

PyObject* PyTrie_insert(PyObject *self, PyObject *args);

PyObject* PyTrie_starts_with(PyObject* self, PyObject* args);

PyObject *
PyTrie_search(PyObject* self, PyObject* args);

PyObject *
PyTrie_delete(PyObject* self, PyObject *args);

int trie_contains(PyObject *self, PyObject *key);

Py_ssize_t
trie_len(PyObject *self);

PyObject *
trie_iter(PyObject *self);

PyObject *trie_str(PyObject *self);

#endif

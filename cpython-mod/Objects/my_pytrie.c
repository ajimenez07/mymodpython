#include <Python.h>
#include "my_pytrie.h" /* solo si realmente tienes un header tuyo */

PyMethodDef PyTrie_methods[] = {
    { "insert", (PyCFunction) PyTrie_insert, METH_VARARGS,
      "Inserta una cadena en el trie." },

    { "starts_with", (PyCFunction) PyTrie_starts_with, METH_VARARGS,
      "Devuelve true si hay ese prefijo en el trie." },

    { "search", (PyCFunction) PyTrie_search, METH_VARARGS,
      "Devuelve true si la palabra completa está en el trie y marcada como "
      "terminal." },

    { "delete", (PyCFunction) PyTrie_delete, METH_VARARGS,
      "Elimina una palabra del árbol" },

    { NULL, NULL, 0, NULL } /* Sentinel */
};

static PySequenceMethods trie_sequence_methods = { .sq_concat = 0,
                                                   .sq_repeat = 0,
                                                   .sq_item = 0,
                                                   .sq_ass_item = 0,
                                                   .sq_length = trie_len,
                                                   .sq_contains =
                                                       trie_contains,
                                                   .sq_inplace_concat = 0,
                                                   .sq_inplace_repeat = 0 };

PyTypeObject PyTrie_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0).tp_base = &PyBaseObject_Type,
    .tp_name = "trie",
    .tp_basicsize = sizeof(PyTrieObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | _Py_TPFLAGS_STATIC_BUILTIN,
    .tp_doc = "Trie object",
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) PyTrie_init,
    .tp_dealloc = (destructor) PyTrie_dealloc,
    .tp_methods = PyTrie_methods,
    .tp_as_sequence = &trie_sequence_methods,
    .tp_iter = trie_iter,
    .tp_str = (reprfunc) trie_str

};

static PyObject *print_trie_test(PyObject *self, PyObject *args) {
    return PyUnicode_FromString("¡Trie module funciona!");
}

static PyMethodDef MyModMethods[] = { { "print_trie_test", print_trie_test,
                                        METH_NOARGS, "Imprimir para test" },
                                      { NULL, NULL, 0, NULL } };

static struct PyModuleDef my_mod_module = { PyModuleDef_HEAD_INIT, "my_mod",
                                            "Módulo AST personalizado", -1,
                                            MyModMethods };

static PyTrieNode *py_trie_node_create(Py_UCS4 ch) {
    PyTrieNode *node = PyMem_Malloc(sizeof(PyTrieNode));
    if (!node)
        return NULL;

    node->ch = ch;
    node->children = PyDict_New();
    node->is_terminal = 0;
    node->freed = 0;

    return node;
}

static void py_trie_free_node(PyTrieNode *n) {
    if (!n)
        return;
    if (n->freed)
        return;
    n->freed = 1;

    PyObject *children = n->children;
    n->children = NULL;
    Py_XDECREF(children);
    PyMem_Free(n);
}

int PyTrie_init(PyTrieObject *self, PyObject *args, PyObject *kwds) {
    self->root = PyDict_New();
    if (!self->root) {
        PyErr_NoMemory();
        return -1;
    }

    self->strs = PyList_New(0);
    if (!self->strs) {
        PyErr_NoMemory();
        return -1;
    }

    return 0;
}

void PyTrie_dealloc(PyTrieObject *self) {
    Py_XDECREF(self->root);
    self->root = NULL;
    Py_TYPE(self)->tp_free((PyObject *) self);
}

PyMODINIT_FUNC PyInit_my_mod(void) {
    PyObject *m = PyModule_Create(&my_mod_module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&PyTrie_Type);
    if (PyModule_AddObject(m, "trie", (PyObject *) &PyTrie_Type) < 0) {
        Py_DECREF(&PyTrie_Type);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}

static void py_trie_node_capsule_destructor(PyObject *capsule) {
    PyTrieNode *n = PyCapsule_GetPointer(capsule, "PyTrieNode");
    if (!n) {
        PyErr_Clear();
        return;
    }
    py_trie_free_node(n);
}

/*
 *--------------------
 * FUNCIONES DEL TRIE
 *--------------------
 *
 */

/* aquí insertamos el str en el tree una vez se llama a trie.insert() */
static int finish_insert(PyTrieObject *self, const char *str) {
    PyTrieNode *current = NULL;
    if (str[0] == '\0') {
        return 0;
    }

    /* iteramos sobre todos los carácteres, creando un nodo si no tiene uno
     * para su respectiva letra */
    for (size_t i = 0; str[i] != '\0'; i++) {
        PyObject *children;
        if (current == NULL)
            children = self->root;
        else
            children = current->children;

        unsigned char ch = (unsigned char) str[i];
        PyObject *key = PyLong_FromUnsignedLong((unsigned long) ch);
        PyObject *value = PyDict_GetItem(children, key);
        PyTrieNode *child_node = NULL;

        if (!value) {
            /* creamos nodo */
            PyTrieNode *new = py_trie_node_create((Py_UCS4) ch);

            /* encapsulamos */
            PyObject *new_cap = PyCapsule_New(new, "PyTrieNode",
                                              py_trie_node_capsule_destructor);
            if (!new_cap) {
                Py_DECREF(new->children);
                PyMem_Free(new);
                Py_DECREF(key);
                return -1;
            }

            /* insertamos el nuevo nodo en el dict */
            if (PyDict_SetItem(children, key, new_cap) < 0) {
                Py_DECREF(new_cap);
                Py_DECREF(key);
                return -1;
            }
            Py_DECREF(new_cap);
            Py_DECREF(key);
            child_node = new;
        } else {
            child_node = PyCapsule_GetPointer(value, "PyTrieNode");

            if (!child_node) {
                Py_DECREF(key);
                return -1;
            }
            Py_DECREF(key);
        }

        current = child_node;
        /* continuamos*/
    }
    current->is_terminal = 1;

    return 0;
}

/* extrae, pensando que solo hay 1 arg, un string de la tupla */
static const char *extract_one_arg_str(const char *fn_name, PyObject *args) {
    PyObject *obj;
    if (!PyArg_UnpackTuple(args, fn_name, 1, 1, &obj)) {
        return NULL;
    }

    if (!PyUnicode_Check(obj)) {
        PyErr_SetString(PyExc_TypeError, "Se esperaba una cadena (str)");
        return NULL;
    }

    const char *str = PyUnicode_AsUTF8(obj);
    /* lo hacemos explícitamente por claridad */
    if (str == NULL) {
        return NULL;
    }

    return str;
}

PyObject *PyTrie_insert(PyObject *self, PyObject *args) {
    const char *str = extract_one_arg_str("insert", args);

    if (str == NULL) {
        return NULL;
    }

    /* self debe ser PyTrieObject */
    if (!PyObject_TypeCheck(self, &PyTrie_Type)) {
        PyErr_SetString(PyExc_TypeError, "Se esperaba un objeto de tipo trie");
        return NULL;
    }

    /* finalizamos... */
    PyTrieObject *trie = (PyTrieObject *) self;
    if (finish_insert(trie, str) < 0)
        return NULL;

    /* añadimos a la lista interna */
    PyObject *py_word = PyUnicode_FromString(str);

    if (py_word == NULL) {
        return NULL;
    }

    if (PyList_Append(trie->strs, py_word) < 0) {
        return NULL;
    }

    /* permitimos chaining */
    Py_INCREF(self);
    return self;
}

/* devuele el último trie de la última letra si la palabra está, si no, NULL*/
PyTrieNode *is_in_trie(PyTrieObject *self, const char *str) {
    if (str[0] == '\0') {
        return 0;
    }

    PyTrieNode *current = NULL;

    /* iteramos sobre los caracs para ver si cada uno de ellos tiene su nodo */
    for (size_t i = 0; str[i] != '\0'; i++) {
        /*esto es un poco copia y pega de finish_insert*/
        PyObject *children;
        if (current == NULL)
            children = self->root;
        else
            children = current->children;

        unsigned char ch = (unsigned char) str[i];
        PyObject *key = PyLong_FromUnsignedLong((unsigned long) ch);
        PyObject *value = PyDict_GetItem(children, key);

        if (!value) {
            return NULL;
        }

        PyTrieNode *child_node = PyCapsule_GetPointer(value, "PyTrieNode");

        current = child_node;
    }

    return current;
}

PyObject *PyTrie_starts_with(PyObject *self, PyObject *args) {
    const char *str = extract_one_arg_str("insert", args);

    if (str == NULL) {
        return NULL;
    }

    /* ahora comrpobamos que la palabra esté en el trie */
    PyTrieObject *trie = (PyTrieObject *) self;
    if (is_in_trie(trie, str)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

PyObject *PyTrie_search(PyObject *self, PyObject *args) {
    const char *str = extract_one_arg_str("search", args);

    if (str == NULL) {
        return NULL;
    }

    PyTrieObject *trie = (PyTrieObject *) self;

    PyObject *py_word = PyUnicode_FromString(str);
    if (py_word == NULL) {
        return NULL;
    }

    int contains = PySequence_Contains(trie->strs, py_word);

    Py_DECREF(py_word);

    if (contains == 1) {
        Py_RETURN_TRUE;
    } else if (contains == 0) {
        Py_RETURN_FALSE;
    } else {
        return NULL;
    }
}

PyObject *PyTrie_delete(PyObject *self, PyObject *args) {
    const char *str = extract_one_arg_str("delete", args);
    if (str == NULL) {
        return NULL;
    }

    if (!PyTrie_search(self, args)) {
        Py_RETURN_FALSE;
    }

    PyTrieObject *trie = (PyTrieObject *) self;

    PyTrieNode *current = NULL;
    PyObject *children = trie->root;

    PyObject *path = PyList_New(0);
    PyObject *keys = PyList_New(0);
    if (!path || !keys) {
        Py_XDECREF(path);
        Py_XDECREF(keys);
        return NULL;
    }

    /* primero añadimos los nodos a una lista provisional */
    for (size_t i = 0; str[i] != '\0'; i++) {
        unsigned char ch = (unsigned char) str[i];
        PyObject *key = PyLong_FromUnsignedLong(ch);
        if (!key)
            goto fail;

        PyObject *value = PyDict_GetItem(children, key);
        if (!value) {
            Py_DECREF(key);
            goto not_found;
        }

        PyTrieNode *node = PyCapsule_GetPointer(value, "PyTrieNode");
        if (!node) {
            Py_DECREF(key);
            goto fail;
        }

        Py_INCREF(value);
        if (PyList_Append(path, value) < 0 || PyList_Append(keys, key) < 0) {
            Py_DECREF(key);
            goto fail;
        }

        current = node;
        children = node->children;
    }

    if (!current || !current->is_terminal)
        goto not_found;

    /* nos desmarcamos de los que no tienen subnodos y son terminales */
    current->is_terminal = 0;

    Py_ssize_t depth = PyList_Size(path);
    for (Py_ssize_t i = depth - 1; i >= 0; i--) {
        PyObject *capsule = PyList_GetItem(path, i);
        PyTrieNode *node = PyCapsule_GetPointer(capsule, "PyTrieNode");
        if (!node || node->is_terminal || PyDict_Size(node->children) > 0)
            break;

        PyObject *key = PyList_GetItem(keys, i);
        PyObject *parent_dict =
            (i == 0) ? trie->root
                     : ((PyTrieNode *) PyCapsule_GetPointer(
                            PyList_GetItem(path, i - 1), "PyTrieNode"))
                           ->children;

        PyDict_DelItem(parent_dict, key);
    }

    /* finalmente, eliminamos de la lista interna */
    PyObject *py_word = PyUnicode_FromString(str);
    if (py_word == NULL) {
        return NULL;
    }

    Py_ssize_t index = PySequence_Index(trie->strs, py_word);
    if (index == -1) {
        Py_DECREF(py_word);
        Py_RETURN_FALSE;
    }

    if (PySequence_DelItem(trie->strs, index) == -1) {
        Py_DECREF(py_word);
        return NULL;
    }

    Py_DECREF(py_word);
    Py_DECREF(path);
    Py_DECREF(keys);
    Py_RETURN_TRUE;

not_found:
    Py_DECREF(path);
    Py_DECREF(keys);
    Py_RETURN_FALSE;

fail:
    Py_DECREF(path);
    Py_DECREF(keys);
    return NULL;
}

/* en contains usamos search() internamente */
int trie_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(PyExc_TypeError, "Se esperaba una cadena");
        return -1;
    }

    PyObject *result = PyObject_CallMethod(self, "search", "O", key);
    if (result == NULL) {
        return -1;
    }

    int is_present = PyObject_IsTrue(result);
    Py_DECREF(result);

    return is_present;
}

Py_ssize_t trie_len(PyObject *self) {
    PyTrieObject *trie = (PyTrieObject *) self;
    return PyList_Size(trie->strs);
}

PyObject *trie_iter(PyObject *self) {
    PyTrieObject *trie = (PyTrieObject *) self;
    return PyObject_GetIter(trie->strs);
}

static int build_trie_lines_from_dict(PyObject *dict, PyObject *lines,
                                      const char *prefix, int is_last) {
    if (!dict || !PyDict_Check(dict) || !lines || !PyList_Check(lines))
        return -1;

    // Recogemos las claves en una lista para poder ordenarlas y saber el
    // último
    PyObject *keys = PyDict_Keys(dict);
    if (!keys)
        return -1;
    if (PyList_Sort(keys) < 0) {
        Py_DECREF(keys);
        return -1;
    }

    Py_ssize_t nkeys = PyList_Size(keys);
    for (Py_ssize_t i = 0; i < nkeys; i++) {
        PyObject *key = PyList_GetItem(keys, i);     // borrowed
        PyObject *value = PyDict_GetItem(dict, key); // borrowed

        PyTrieNode *node = PyCapsule_GetPointer(value, "PyTrieNode");
        if (!node) {
            Py_DECREF(keys);
            return -1;
        }

        int last_child = (i == nkeys - 1);

        // Construímos prefijo con ramificaciones
        PyObject *char_repr = PyUnicode_FromOrdinal(node->ch);
        if (!char_repr) {
            Py_DECREF(keys);
            return -1;
        }

        const char *chr_utf8 = PyUnicode_AsUTF8(char_repr);
        if (!chr_utf8) {
            Py_DECREF(char_repr);
            Py_DECREF(keys);
            return -1;
        }

        const char *branch = last_child ? "└── " : "├── ";

        PyObject *line = PyUnicode_FromFormat("%s%s%s", prefix, branch,
                                              node->is_terminal ? "[T] " : "");
        if (!line) {
            Py_DECREF(char_repr);
            Py_DECREF(keys);
            return -1;
        }

        // Añadimos el carácter del nodo
        PyObject *full_line = PyUnicode_FromFormat("%U%s", line, chr_utf8);
        Py_DECREF(line);
        Py_DECREF(char_repr);
        if (!full_line) {
            Py_DECREF(keys);
            return -1;
        }

        if (PyList_Append(lines, full_line) < 0) {
            Py_DECREF(full_line);
            Py_DECREF(keys);
            return -1;
        }
        Py_DECREF(full_line);

        // Nuevo prefijo para hijos: añade tubería o espacios
        char new_prefix[256];
        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix,
                 last_child ? "    " : "│   ");

        if (node->children && PyDict_Check(node->children)) {
            if (build_trie_lines_from_dict(node->children, lines, new_prefix,
                                           last_child) < 0) {
                Py_DECREF(keys);
                return -1;
            }
        }
    }

    Py_DECREF(keys);
    return 0;
}

PyObject *trie_str(PyObject *self) {
    PyTrieObject *trie = (PyTrieObject *) self;
    PyObject *lines = PyList_New(0);
    if (!lines)
        return PyUnicode_FromString("<trie error>");

    PyObject *root_line = PyUnicode_FromString("(root)");
    if (!root_line || PyList_Append(lines, root_line) < 0) {
        Py_XDECREF(root_line);
        Py_DECREF(lines);
        return PyUnicode_FromString("<trie error>");
    }
    Py_DECREF(root_line);

    if (trie->root && PyDict_Check(trie->root)) {
        if (build_trie_lines_from_dict(trie->root, lines, "", 1) < 0) {
            // Añadimos un aviso legible al listado
            PyObject *err_line =
                PyUnicode_FromString("└── <error al recorrer trie>");
            if (err_line) {
                PyList_Append(lines, err_line);
                Py_DECREF(err_line);
            }
        }
    } else {
        // Si root no es un dict válido, también lo indicamos
        PyObject *err_line = PyUnicode_FromString("└── <raíz inválida>");
        if (err_line) {
            PyList_Append(lines, err_line);
            Py_DECREF(err_line);
        }
    }

    PyObject *sep = PyUnicode_FromString("\n");
    if (!sep) {
        Py_DECREF(lines);
        return PyUnicode_FromString("<trie error>");
    }

    PyObject *joined = PyUnicode_Join(sep, lines);
    Py_DECREF(sep);
    Py_DECREF(lines);

    if (!joined)
        return PyUnicode_FromString("<trie error>");

    return joined;
}

# CPython — Modificación Personal

Me apetecía hacer un fork de **CPython** y añadirle una estructura built-in nueva (tipo list, dict, set, etc.). No es nada serio,
simplemente para diversión personal. La estructura que he añadido es un trie de strings.

## Cómo funciona?

Un trie es una estructura de datos donde cada carácter representa un nodo más. 
Para hacerlo, he implementado las siguientes funciones:

- **insert**: Inserta un string en el árbol.
- **starts_with**: Devuelve `True` si el prefijo dado está presente.
- **search**: Devuelve `True` si la palabra (no prefijo) está presente.
- **delete**: Elimina la palabra indicada.

También he implementado los métodos internos **__iter__**, **__contains__**,
**__len__** y **__str__**

Todo esto modificando el intérprete de Python desde C.

## Cómo se ha implementado?

En el directorio `Objects/` he creado dos archivos (un header y un src) para las funcionalidades. 
Los respectivos archivos son `my_pytrie.h` y `my_pytrie.c`. Pero estos archivos por si solos no hacen nada. Hay
que registrar el tipo.

En `Python/bltinmodule.c`.. he usado la siguiente macro para marcarlo como built-in:

  ```c
  SETBUILTIN("trie", &PyTrie_Type);
  ```
De esta manera marcamos el tipo como built-in, pero aún falta ponerlo como estático para que no haga falta importarlo.

Para marcarlo como tipo estático en `Objects/object.c` he añadido `&PyTrie_Type` al array `static_types[]`, 
junto a otros tipos como `&PyList_Type` y `&PyDict_Type`.

Luego hace falta que el tipo esté marcado con la bandera `_Py_TPFLAGS_STATIC_BUILTIN` 
para que se inicialice automáticamente en `_PyTypes_InitTypes()` durante el arranque del intérprete.

## Test

Para probar si funcionaba he hecho un unittest (créditos a la IA) en `Lib/test/test_trie.py`

```python

import unittest

class TestTrieMethods(unittest.TestCase):

    def setUp(self):
        # Crear instancia del trie
        self.trie = trie()
        # Poblar con algunas palabras para los tests
        self.words = ["hola", "hilo", "sol", "sombra"]
        for w in self.words:
            self.trie.insert(w)

    def test_insert_and_search(self):
        # Palabras existentes
        for w in self.words:
            self.assertTrue(self.trie.search(w))
        # Palabra inexistente
        self.assertFalse(self.trie.search("holaquetal"))

    def test_starts_with(self):
        self.assertTrue(self.trie.starts_with("ho"))
        self.assertTrue(self.trie.starts_with("so"))
        self.assertFalse(self.trie.starts_with("za"))

    def test_delete(self):
        # Borrar una palabra existente
        self.assertTrue(self.trie.delete("hola"))
        self.assertFalse(self.trie.search("hola"))
        # Borrar palabra inexistente → debería devolver False
        self.assertFalse(self.trie.delete("inexistente"))

    def test_trie_contains_protocol(self):
        # __contains__ mapea a trie_contains
        self.assertIn("hilo", self.trie)
        self.assertNotIn("hilarante", self.trie)

    def test_trie_len_protocol(self):
        # __len__ mapea a trie_len
        self.assertEqual(len(self.trie), len(self.words))
        self.trie.insert("extra")
        self.assertEqual(len(self.trie), len(self.words) + 1)

    def test_trie_iter_protocol(self):
        # Iterador devuelve todas las palabras (en algún orden)
        collected = set(self.trie)
        for w in self.words:
            self.assertIn(w, collected)

    def test_trie_str_output(self):
        # La representación incluye las etiquetas clave
        output = str(self.trie)
        self.assertIn("(root)", output)
        # Algún carácter esperado
        self.assertRegex(output, r"[hs]")  # letras iniciales
        # Marcadores de terminal
        self.assertIn("[T]", output)


if __name__ == "__main__":
    unittest.main()

```
## Compilación y test

```bash
./configure
./configure     --with-pydebug     CFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer"     LDFLAGS="-fsanitize=address" # para debug
make -j$(nproc)
./python Lib/test/test_trie.py

```
## Licencia
Este proyecto es un fork de [CPython](https://github.com/python/cpython)  
Licencia: Python Software Foundation License (PSF)  
Consulta el archivo LICENSE para más detalles.


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


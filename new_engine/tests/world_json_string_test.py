"""Compile the production JSON-string helper and parse its actual C++ output."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which('g++'), 'Standalone C++ compiler required')
class WorldJsonStringTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='valwood-json-string-')
        cls.binary = Path(cls.temp.name)/'fixture'
        subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT),
                        str(ROOT/'tests/world_json_string_test.cpp'), '-o', str(cls.binary)],
                       check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def quote(self, value):
        return subprocess.check_output([str(self.binary), '--quote'], input=value.encode('utf-8')).decode('utf-8')

    def test_actual_report_windows_path_reproduces_old_failure_and_roundtrips(self):
        path = r'C:\users\computer\Temp\valwood-studio-ve97zr/0.txt'
        with self.assertRaises(json.JSONDecodeError):
            json.loads('{"authored_sources":[{"path":"'+path+'","instances":283}]}')
        repaired = json.loads('{"authored_sources":[{"path":'+self.quote(path)+',"instances":283}]}')
        self.assertEqual(repaired['authored_sources'][0], dict(path=path, instances=283))

    def test_cpp_report_keeps_field_values_and_json_types(self):
        value = json.loads(subprocess.check_output([str(self.binary)]))
        self.assertEqual(value['authored_sources'][0]['path'], r'C:\users\computer\Temp\valwood-studio-ve97zr/0.txt')
        self.assertEqual(value['authored_sources'][0]['instances'], 283)
        self.assertIs(value['enabled'], True)
        self.assertEqual(value['utf8'], 'C:\\Users\\Élodie\\城\\house.glb')
        self.assertEqual(value['empty'], '')
        self.assertIn('\x00', value['controls'])
        self.assertEqual(value['scenic'], 'rainforest-milestone')
        self.assertEqual(value['view'], 'arrival')
        self.assertEqual(value['id'], 'silverpine_waystation_return')

    def test_every_ascii_control_quote_slash_and_utf8_byte_is_preserved(self):
        value = ''.join(map(chr, range(32)))+'\x7f"\\/é城😀'
        encoded = self.quote(value)
        self.assertFalse(any(ord(c)<32 for c in encoded))
        self.assertEqual(json.loads(encoded), value)

    def test_quotes_cannot_inject_another_json_field(self):
        value = '\\"}, "gl_error": 1282, "forged": "'
        self.assertEqual(json.loads('{"path":'+self.quote(value)+'}'), {'path': value})


if __name__ == '__main__':
    unittest.main()

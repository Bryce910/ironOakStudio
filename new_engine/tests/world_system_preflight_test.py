import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from check_valwood_system import assess

class PreflightTest(unittest.TestCase):
    def test_compatibility_and_missing_dependencies(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root)
            doc = {'architecture': 'Advanced Micro Devices X86-64', 'minimum_glibc': '2.34',
                   'libraries': [{'soname': 'libSDL2.so', 'bundled': True},
                                 {'soname': 'libGL.so.1', 'bundled': False}]}
            (path / 'native-dependencies.json').write_text(json.dumps(doc))
            def run(**kw):
                options = dict(system='Linux', machine='x86_64', libc=('glibc', '2.39'), cache='libGL.so.1', loader=lambda name: (_ for _ in ()).throw(OSError(name)))
                options.update(kw)
                return assess(path, **options)
            self.assertFalse(run()['passed'])
            (path / 'lib').mkdir()
            (path / 'lib/libSDL2.so').touch()
            self.assertTrue(run()['passed'])
            self.assertTrue(run()['requires_real_launch'])
            self.assertFalse(run(system='Windows')['passed'])
            self.assertFalse(run(machine='arm64')['passed'])
            self.assertFalse(run(libc=('glibc', '2.31'))['passed'])
            self.assertFalse(run(libc=('musl', '1.2'))['passed'])
            self.assertFalse(run(cache='')['passed'])
            self.assertTrue(run(cache='', loader=lambda name: object())['passed'])

if __name__ == '__main__':
    unittest.main()

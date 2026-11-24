"""Compile the production codec/state and replay the real tiny route files on CPU."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which('g++'), 'Standalone C++ compiler required')
class WorldPlaytestSliceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='valwood-playtest-slice-')
        cls.binary = Path(cls.temp.name)/'fixture'
        subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT),
                        str(ROOT/'tests/world_playtest_slice_test.cpp'), '-o', str(cls.binary)],
                       check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_fixture(self, *args):
        result = subprocess.run([str(self.binary), *map(str,args)], check=True,
                                capture_output=True, text=True)
        self.assertIn('PLAYTEST_SLICE_PASS', result.stdout)
        return result.stdout

    def test_strict_codec_atomic_rejection_and_ordered_height_aware_progress(self):
        self.run_fixture()

    def test_actual_city_route_loads_roundtrips_and_completes_in_order(self):
        output = self.run_fixture(ROOT/'docs/playtest-slices/2026-11-02/city/runtime/playtest.route')
        self.assertIn('ROUTE_PASS 213 nodes 5 milestones', output)

    def test_actual_jungle_route_loads_roundtrips_and_completes_in_order(self):
        output = self.run_fixture(ROOT/'docs/playtest-slices/2026-11-02/jungle/runtime/playtest.route')
        self.assertIn('ROUTE_PASS 28 nodes', output)


if __name__ == '__main__':
    unittest.main()

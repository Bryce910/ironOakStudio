"""Exercise source VAT decoding and independently check the cooked bank."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import cook_volumetric_blood as tool


def exr_fixture(path):
    def attribute(name, kind, value):
        return name.encode()+b'\0'+kind.encode()+b'\0'+struct.pack('<I', len(value))+value
    channels = b''.join(name+b'\0'+struct.pack('<iB3xii', 2, 0, 1, 1) for name in (b'B', b'G', b'R'))+b'\0'
    header = struct.pack('<II', 20000630, 2)
    header += attribute('channels', 'chlist', channels)
    header += attribute('compression', 'compression', b'\0')
    for key in ('dataWindow', 'displayWindow'):
        header += attribute(key, 'box2i', struct.pack('<4i', 0, 0, 1, 1))
    header += attribute('lineOrder', 'lineOrder', b'\0')
    header += attribute('pixelAspectRatio', 'float', struct.pack('<f', 1))
    header += attribute('screenWindowCenter', 'v2f', struct.pack('<2f', 0, 0))
    header += attribute('screenWindowWidth', 'float', struct.pack('<f', 1))+b'\0'
    expected = np.array([[[.1, .3, .5], [.2, .4, .6]], [[.7, .9, .2], [.8, 1., .3]]], dtype='<f4')
    chunks = [struct.pack('<ii', y, 24)+expected[y, :, [2, 1, 0]].tobytes() for y in range(2)]
    start = len(header)+16
    path.write_bytes(header+struct.pack('<2Q', start, start+len(chunks[0]))+b''.join(chunks))
    return expected


class DecoderTest(unittest.TestCase):
    def test_gamma_branch_and_bottom_left_point_sampling(self):
        self.assertAlmostEqual(float(tool.gamma(np.array([.003]))[0]), .03876, places=7)
        self.assertAlmostEqual(float(tool.gamma(np.array([.25]))[0]), .53709873, places=7)
        pixels = np.arange(12).reshape(2, 2, 3)
        uv = np.array([[.25, .75], [.75, .25]])
        np.testing.assert_array_equal(tool.sample_vat(pixels, uv, 0), [pixels[0, 0], pixels[1, 1]])
        np.testing.assert_array_equal(tool.sample_vat(pixels, uv, -.5), [pixels[1, 0], pixels[0, 1]])

    @unittest.skipUnless(shutil.which('ffmpeg'), 'CPU EXR decoder unavailable')
    def test_cpu_exr_decoder_preserves_channel_order_and_float_values(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'fixture.exr'
            expected = exr_fixture(path)
            self.assertEqual(tool.exr_shape(path), (2, 2))
            np.testing.assert_allclose(tool.read_exr(path), expected, atol=1e-7)

    def test_ascii_fbx_triangle_uv_seams_and_invalid_topology(self):
        text = '; FBX 7.5\nMappingInformationType: "ByPolygonVertex"\n'
        text += 'Vertices: *9 { a: 0,0,0,1,0,0,0,1,0 }\n'
        text += 'PolygonVertexIndex: *3 { a: 2,1,-1 }\n'
        text += 'UV: *6 { a: .1,.2,.3,.4,.5,.6 }\nUVIndex: *3 { a: 0,1,2 }\n'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'mesh.fbx'
            path.write_text(text)
            uv, indices = tool.mesh_addressing(path)
            np.testing.assert_allclose(uv, [[.1, .2], [.3, .4], [.5, .6]])
            np.testing.assert_array_equal(indices, [0, 1, 2])
            path.write_text(text.replace('2,1,-1', '2,-2,0'))
            with self.assertRaisesRegex(ValueError, 'triangle topology'):
                tool.mesh_addressing(path)

    def test_nonfinite_or_truncated_fbx_data_is_refused(self):
        for text in ('Vertices: *3 { a: 1,2 }', 'Vertices: *3 { a: 1,nan,3 }'):
            with self.assertRaises(ValueError):
                tool.fbx_array(text, 'Vertices')


class ActualBankTest(unittest.TestCase):
    def test_original_three_effects_match_renderer_binary_contract(self):
        self.check_bank('runtime-v1', .20, ((9600, 3200, 81, 2.3), (9600, 3200, 74, 2.3), (10500, 3500, 95, 2.0)))

    def test_heavier_fans_preserve_original_129_frame_effect(self):
        self.check_bank('runtime-v2', .48, ((12000, 4000, 79, 2.0), (9600, 3200, 99, 2.5), (15000, 5000, 129, 2.8)))

    def check_bank(self, name, presentation_scale, expected_effects):
        root = Path(__file__).resolve().parents[1]
        folder = root/'assets/purchased/volumetric_blood'/name
        if not folder.exists():
            self.skipTest('Purchased bank not staged')
        manifest = json.loads((folder/'BLOOD_ASSETS.json').read_text())
        provenance = json.loads((folder/'provenance.json').read_text())
        self.assertEqual(manifest['source_archive_sha256'], '78a37065dcaf5ef2e103beb05ad85708680a4b35ff777ab9f907a470fdbe7bdf')
        self.assertEqual(manifest['presentation_scale'], presentation_scale)
        self.assertFalse(provenance['package_code_executed'])
        for item in manifest['assets']:
            self.assertEqual(hashlib.sha256((folder/item['file']).read_bytes()).hexdigest(), item['sha256'])
        data = (folder/'blood.bank').read_bytes()
        self.assertEqual(data[:8], b'VWBLOOD1')
        version, count = struct.unpack_from('<II', data, 8)
        self.assertEqual((version, count), (1, 3))
        offset = 16
        geometry_bytes = 0
        for expected in expected_effects:
            vertex_count, index_count, frame_count, width, height, duration, *extra = struct.unpack_from('<5If6f', data, offset)
            offset += struct.calcsize('<5If6f')
            self.assertEqual((vertex_count, index_count//3, frame_count), expected[:3])
            self.assertAlmostEqual(duration, expected[3], places=6)
            self.assertTrue(all(0 <= color <= 1 for color in extra[:4]))
            self.assertTrue(all(0 < size < 20 for size in extra[4:]))
            indices = np.frombuffer(data, '<u4', index_count, offset)
            offset += index_count*4
            self.assertLess(indices.max(), vertex_count)
            vertices = np.frombuffer(data, '<f4', frame_count*vertex_count*6, offset).reshape(frame_count, vertex_count, 6)
            offset += frame_count*vertex_count*24
            geometry_bytes += index_count*4+frame_count*vertex_count*24
            self.assertTrue(np.isfinite(vertices).all())
            np.testing.assert_allclose(np.linalg.norm(vertices[:, :, 3:], axis=2), 1, atol=2e-6)
            self.assertGreater(np.abs(vertices[frame_count//2, :, :3]-vertices[0, :, :3]).max(), .2)
            self.assertGreater(vertices[:, :, 2].max(), 2)
            # Actual baked gravity remains vertical; yaw normalization does not
            # flatten, normalize or erase the authored trajectory.
            self.assertLess(vertices[:, :, 1].min(), -1)
            triangles = vertices[frame_count//5, indices, :3].reshape(-1, 3, 3)
            normal = vertices[frame_count//5, indices, 3:].reshape(-1, 3, 3).mean(1)
            face = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
            self.assertGreater(np.einsum('ij,ij->', face, normal), 0)
            rgba = np.frombuffer(data, np.uint8, width*height*4, offset).reshape(height, width, 4)
            offset += width*height*4
            self.assertTrue(np.any(rgba[:, :, 3] == 0))
            self.assertTrue(np.any(rgba[:, :, 3] > 128))
        self.assertEqual(offset, len(data))
        self.assertLessEqual(geometry_bytes, 96*1024**2)


if __name__ == '__main__':
    unittest.main()

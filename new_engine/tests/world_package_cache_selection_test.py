#!/usr/bin/env python3
"""CPU-only encoded-source identity proof for nested managed BC7 selection."""
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from world_asset_database import Source
import world_package_cache_selection as selection


def glb(images=(b'first-encoded-image',), *, document=None, binary=None):
    if document is None:
        binary = b''.join(images)
        views = []; at = 0
        for image in images:
            views.append({'buffer': 0, 'byteOffset': at, 'byteLength': len(image)})
            at += len(image)
        document = {'asset': {'version': '2.0'}, 'buffers': [{'byteLength': len(binary)}],
                    'bufferViews': views,
                    'images': [{'bufferView': i, 'mimeType': 'image/png'} for i in range(len(images))]}
    encoded = json.dumps(document, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    chunks = struct.pack('<II', len(encoded), 0x4e4f534a) + encoded
    if binary is not None:
        binary += b'\0' * (-len(binary) % 4)
        chunks += struct.pack('<II', len(binary), 0x004e4942) + binary
    return struct.pack('<III', 0x46546c67, 2, 12 + len(chunks)) + chunks


def cache(encoded=b'unrelated-encoded-image', *, format_=1):
    # Real v1 layout: one 1x1 mip, 40-byte descriptor, original encoded source,
    # and one opaque BC7 block. Selection deliberately does not certify CRC/GPU quality.
    header = bytearray(64)
    header[:8] = b'VWBC7\0\r\n'
    struct.pack_into('<8I', header, 8, 1, 1, format_, 64, 1, 1, 1, 3 if format_ == 3 else 1)
    struct.pack_into('<QQ', header, 40, len(encoded), 64 + 40 + len(encoded) + 16)
    descriptor = struct.pack('<IIQ4fII', 1, 1, 16, 0, 0, 0, 0, 0, 0)
    return bytes(header) + descriptor + encoded + b'\x02' + bytes(15)


class CacheSelectionTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='valwood-cache-selection-')
        self.root = Path(self.temp.name)
        self.sources = {}
        self.revision = 'models/imports/oak/' + 'a' * 24

    def tearDown(self):
        self.temp.cleanup()

    def add(self, relative, raw=None, *, link=None):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if link is not None:
            os.link(self.root / link, path)
        else:
            path.write_bytes(raw)
        info = path.stat(); data = path.read_bytes()
        self.sources[relative] = Source(relative, path, path.resolve(), (info.st_dev, info.st_ino),
                                        hashlib.sha256(data).hexdigest(), len(data))
        return relative

    def model(self, raw=None, *, revision=None, name='near.glb'):
        return self.add((revision or self.revision) + '/' + name, raw or glb())

    def block(self, encoded=b'unrelated-encoded-image', *, revision=None, name='0123456789abcdef.vbc7', raw=None):
        return self.add((revision or self.revision) + '/texture_blocks/' + name,
                        cache(encoded) if raw is None else raw)

    def selected(self, deps=None):
        return selection.select_unused_revision_caches(self.sources, deps or {})

    def test_only_strict_nested_unrelated_cache_is_removed(self):
        self.model(); unused = self.block()
        for name in ('texture_blocks/0123456789abcdef.vbc7',
                     'models/ordinary/texture_blocks/0123456789abcdef.vbc7',
                     self.revision + '/texture_blocks/not-a-cache-name.vbc7',
                     self.revision + '/other/0123456789abcdef.vbc7'):
            self.add(name, cache())
        before = dict(self.sources)
        self.assertEqual(self.selected(), {unused})
        self.assertEqual(self.sources, before)
        self.assertTrue(all(source.lexical.is_file() for source in self.sources.values()))

    def test_matching_srgb_linear_and_normal_sources_are_retained(self):
        image = b'first-encoded-image'; self.model()
        self.block(image)
        self.block(image, name='0123456789abcdef.linear.vbc7', raw=cache(image, format_=2))
        self.block(image, name='1111111111111111.linear.vbc7', raw=cache(image, format_=3))
        self.assertEqual(self.selected(), set())

    def test_all_images_and_all_lods_form_revision_union(self):
        self.model(glb((b'near-a', b'near-b')))
        self.model(glb((b'mid-c',)), name='mid.glb')
        for i, image in enumerate((b'near-a', b'near-b', b'mid-c')):
            self.block(image, name=f'{i:016x}.vbc7')
        unused = self.block()
        self.assertEqual(self.selected(), {unused})

    def test_revision_scope_does_not_cross_contaminate(self):
        self.model(glb((b'a',)))
        other = 'models/imports/oak/' + 'b' * 24
        self.model(glb((b'b',)), revision=other)
        first = self.block(b'b')
        self.block(b'b', revision=other)
        self.assertEqual(self.selected(), {first})

    def test_explicit_dependency_always_protects(self):
        self.model(); unused = self.block()
        self.assertEqual(self.selected({'recipe.json': [unused]}), set())

    def test_missing_glb_and_loose_gltf_keep_scope(self):
        self.block(); self.assertEqual(self.selected(), set())
        self.model(); self.add(self.revision + '/extra.gltf', b'{}')
        self.assertEqual(self.selected(), set())

    def test_unsupported_glb_image_or_buffer_keeps_all_revision_caches(self):
        base = {'asset': {'version': '2.0'}, 'buffers': [{'byteLength': 4}],
                'bufferViews': [{'buffer': 0, 'byteLength': 4}],
                'images': [{'bufferView': 0}]}
        cases = []
        for uri in ('data:image/png;base64,eA==', 'external.png'):
            doc = copy.deepcopy(base); doc['images'] = [{'uri': uri}]; cases.append(doc)
        doc = copy.deepcopy(base); doc['buffers'][0]['uri'] = 'data:application/octet-stream;base64,eA=='
        cases.append(doc)
        doc = copy.deepcopy(base); doc['images'][0]['extensions'] = {'future': {}}; cases.append(doc)
        doc = copy.deepcopy(base); doc['bufferViews'][0]['byteLength'] = 9; cases.append(doc)
        doc = copy.deepcopy(base); doc['bufferViews'][0]['buffer'] = False; cases.append(doc)
        for doc in cases:
            with self.subTest(document=doc):
                self.sources.clear(); self.model(glb(document=doc, binary=b'abcd')); self.block()
                self.assertEqual(self.selected(), set())

    def test_one_bad_glb_keeps_whole_scope_and_bad_lengths_are_conservative(self):
        self.model(); self.model(b'not-a-glb', name='mid.glb'); self.block()
        self.assertEqual(self.selected(), set())
        for raw in (glb()[:-1], glb()[:12], b''):
            with self.subTest(raw=raw):
                self.sources.clear(); self.add(self.revision + '/near.glb', raw); self.block()
                self.assertEqual(self.selected(), set())

    def test_unknown_cache_headers_and_truncation_are_kept(self):
        self.model(); valid = cache()
        bad = [b'unknown-cache', valid[:-1]]
        for offset, value in ((8, 9), (12, 9), (16, 9), (20, 68), (32, 0), (36, 0), (64, 2)):
            raw = bytearray(valid); struct.pack_into('<I', raw, offset, value); bad.append(bytes(raw))
        for i, raw in enumerate(bad):
            self.block(raw=raw, name=f'{i:016x}.vbc7')
        self.assertEqual(self.selected(), set())

    def test_stored_encoded_length_is_part_of_identity(self):
        self.model(glb((b'first-encoded-image',)))
        shorter = self.block(b'first-encoded-imag')
        self.assertEqual(self.selected(), {shorter})

    def test_no_image_glb_can_prove_known_cache_unused(self):
        self.model(glb(document={'asset': {'version': '2.0'}}))
        unused = self.block()
        self.assertEqual(self.selected(), {unused})

    def test_image_free_glb_with_invalid_binary_bounds_is_kept(self):
        document = {'asset': {'version': '2.0'}, 'buffers': [{'byteLength': 12}],
                    'bufferViews': [{'buffer': 0, 'byteLength': 12}]}
        self.model(glb(document=document, binary=b'abcd')); self.block()
        self.assertEqual(self.selected(), set())
        document['buffers'][0]['byteLength'] = 4
        self.sources.clear(); self.model(glb(document=document, binary=b'abcd')); self.block()
        self.assertEqual(self.selected(), set())

    def test_shared_cache_source_is_hashed_once_per_inode(self):
        self.model(); first = self.block()
        other = 'models/imports/beech/' + 'b' * 24
        self.model(revision=other)
        second = self.add(other + '/texture_blocks/0123456789abcdef.vbc7', link=first)
        with mock.patch.object(selection, '_hash_region', wraps=selection._hash_region) as digest:
            self.assertEqual(self.selected(), {first, second})
        self.assertEqual(digest.call_count, 1)

    def test_modified_glb_or_retargeted_source_is_not_pruned(self):
        model = self.model(); self.block()
        self.sources[model].lexical.write_bytes(b'x' * self.sources[model].size)
        self.assertEqual(self.selected(), set())
        self.sources.clear(); self.model(); block = self.block()
        source = self.sources[block]
        replacement = self.root / 'replacement-cache'
        replacement.write_bytes(cache())
        os.replace(replacement, source.lexical)
        self.assertEqual(self.selected(), set())


if __name__ == '__main__':
    unittest.main()

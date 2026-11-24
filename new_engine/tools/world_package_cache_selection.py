"""Conservatively select unused nested managed-revision BC7 cache paths.

This is a packaging optimization, never a cache validator or a geometry filter.
The caller opts in after retaining complete managed-revision dependency closure.
Inputs are ProjectInventory.Source records; neither input mapping is mutated.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import struct

_REVISION = re.compile(r"^(models/imports/[A-Za-z0-9_-]+/[0-9a-f]{24})/")
_CACHE = re.compile(r"^(models/imports/[A-Za-z0-9_-]+/[0-9a-f]{24})/texture_blocks/[0-9a-f]{16}(?:\.linear)?\.vbc7$")
_MAX_GLB = 512 * 1024 * 1024
_MAX_JSON = 16 * 1024 * 1024
_MAX_ENCODED = 64 * 1024 * 1024
_MAX_CACHE = 192 * 1024 * 1024


def _integer(value):
    return type(value) is int


def _source_key(source):
    """Verified inventory identity; whole-file SHA also separates edited inputs."""
    return tuple(source.identity), source.size, source.source_hash


def _embedded_image_digests(source):
    """Return complete encoded image identities, or None for uncertain GLBs."""
    try:
        if source.size < 20 or source.size > _MAX_GLB:
            return None
        raw = source.read_verified()
        if len(raw) != source.size:
            return None
        magic, version, total = struct.unpack_from('<III', raw)
        if magic != 0x46546c67 or version != 2 or total != len(raw):
            return None
        chunks = []; offset = 12
        while offset < len(raw):
            if offset + 8 > len(raw):
                return None
            length, kind = struct.unpack_from('<II', raw, offset)
            offset += 8
            if length % 4 or length > len(raw) - offset:
                return None
            chunks.append((kind, offset, length)); offset += length
        if not chunks or chunks[0][0] != 0x4e4f534a or chunks[0][2] > _MAX_JSON:
            return None
        if len(chunks) > 2 or (len(chunks) == 2 and chunks[1][0] != 0x004e4942):
            return None
        _, start, size = chunks[0]
        document = json.loads(raw[start:start + size])
        if not isinstance(document, dict) or document.get('asset', {}).get('version') != '2.0':
            return None
        images = document.get('images', [])
        buffers = document.get('buffers', [])
        views = document.get('bufferViews', [])
        if not all(isinstance(value, list) for value in (images, buffers, views)):
            return None
        if len(buffers) > 1 or any(not isinstance(b, dict) or 'uri' in b for b in buffers):
            return None
        if not buffers:
            return set() if not images and not views and len(chunks) == 1 else None
        if len(chunks) != 2:
            return None
        _, binary_at, binary_size = chunks[1]
        declared = buffers[0].get('byteLength')
        if not _integer(declared) or declared < 0 or not declared <= binary_size <= declared + 3:
            return None
        # Unknown geometry still retains its complete revision scope. Check the
        # enclosing views as well as image views, including an image-free GLB.
        for view in views:
            if not isinstance(view, dict) or not _integer(view.get('buffer')) or view['buffer'] != 0:
                return None
            offset = view.get('byteOffset', 0); length = view.get('byteLength')
            if (not _integer(offset) or not _integer(length) or offset < 0 or length <= 0
                    or offset > declared - length):
                return None
        identities = set(); binary = memoryview(raw)[binary_at:binary_at + binary_size]
        for image in images:
            # External/data URI image semantics are deliberately not inferred.
            if not isinstance(image, dict) or 'uri' in image or image.get('extensions'):
                return None
            index = image.get('bufferView')
            if not _integer(index) or not 0 <= index < len(views):
                return None
            view = views[index]
            if (not isinstance(view, dict) or not _integer(view.get('buffer'))
                    or view.get('buffer') != 0 or 'byteStride' in view):
                return None
            offset = view.get('byteOffset', 0); length = view.get('byteLength')
            if (not _integer(offset) or not _integer(length) or offset < 0
                    or length <= 0 or length > _MAX_ENCODED or offset > declared - length):
                return None
            identities.add((length, hashlib.sha256(binary[offset:offset + length]).digest()))
        return identities
    except (OSError, ValueError, TypeError, AttributeError, struct.error, OverflowError):
        return None


def _hash_region(stream, length):
    """Accelerated streaming SHA256; no bytewise Python fingerprint loops."""
    digest = hashlib.sha256()
    while length:
        data = stream.read(min(length, 1024 * 1024))
        if not data:
            return None
        digest.update(data); length -= len(data)
    return digest.digest()


def _cache_source_digest(source, memo):
    """Read only the known header/table and original encoded-source region."""
    # Lazy import avoids a cycle when ProjectInventory imports this helper.
    from world_asset_database import open_regular_nofollow
    try:
        if source.lexical.resolve(strict=True) != source.resolved:
            return None
        with open_regular_nofollow(source.resolved) as stream:
            before = os.fstat(stream.fileno())
            if ((before.st_dev, before.st_ino) != tuple(source.identity)
                    or before.st_size != source.size):
                return None
            stamp = (before.st_dev, before.st_ino, before.st_size,
                     before.st_mtime_ns, before.st_ctime_ns, source.source_hash)
            if stamp in memo:
                return memo[stamp]
            memo[stamp] = None
            header = stream.read(64)
            if len(header) != 64 or header[:8] != b'VWBC7\0\r\n':
                return None
            version, recipe, format_, header_size, width, height, levels, flags = struct.unpack_from('<8I', header, 8)
            length, total = struct.unpack_from('<QQ', header, 40)
            valid_flags = (flags == 1 if format_ in (1, 2) else
                           ((flags & 3) == 3 and not (flags & 0xfc000000)
                            and ((flags >> 2) & 4095) <= 2048
                            and ((flags >> 14) & 4095) <= 768))
            if (version != 1 or recipe != 1 or format_ not in (1, 2, 3)
                    or header_size != 64 or not valid_flags
                    or not 1 <= width <= 16384 or not 1 <= height <= 16384
                    or width * height > 64 * 1024 * 1024
                    or not 1 <= levels <= 15 or not 1 <= length <= _MAX_ENCODED
                    or total != before.st_size or total > _MAX_CACHE):
                return None
            source_at = 64 + levels * 40
            if source_at > total or length > total - source_at:
                return None
            table = stream.read(levels * 40)
            if len(table) != levels * 40:
                return None
            w, h = width, height; block_total = 0
            for level in range(levels):
                lw, lh, block_size = struct.unpack_from('<IIQ', table, level * 40)
                expected = ((w + 3) // 4) * ((h + 3) // 4) * 16
                if (lw, lh, block_size) != (w, h, expected):
                    return None
                if (w, h) == (1, 1) and level != levels - 1:
                    return None
                block_total += block_size; w, h = max(1, w // 2), max(1, h // 2)
            if (lw, lh) != (1, 1) or source_at + length + block_total != total:
                return None
            digest = _hash_region(stream, length)
            after = os.fstat(stream.fileno())
            if (digest is None or (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns)
                    != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns)):
                return None
            memo[stamp] = (length, digest)
            return memo[stamp]
    except (OSError, ValueError, TypeError, AttributeError, struct.error, OverflowError):
        return None


def select_unused_revision_caches(sources, deps) -> set[str]:
    """Return only proven-unused nested managed-revision cache paths.

    `sources` maps portable project paths to verified inventory Source objects.
    `deps` maps owners to explicit dependency path iterables. References always
    win. No GLB, geometry, nonmanaged/global cache, unknown format, or uncertain
    image consumer is selected. A revision must contain at least one supported
    GLB, and every GLB in it must expose all images as bounded embedded views.
    """
    protected = {reference for references in deps.values() for reference in references}
    revisions = {}; cache_paths = {}
    for name in sources:
        match = _REVISION.match(name)
        if match:
            revisions.setdefault(match[1], []).append(name)
        match = _CACHE.fullmatch(name)
        if match and name not in protected:
            cache_paths.setdefault(match[1], []).append(name)
    unused = set(); glb_memo = {}; cache_memo = {}
    for revision, paths in cache_paths.items():
        names = revisions[revision]
        # A loose glTF is an unsupported additional image consumer in scope.
        if any(name.lower().endswith('.gltf') for name in names):
            continue
        glbs = [name for name in names if name.lower().endswith('.glb')]
        if not glbs:
            continue
        images = set(); certain = True
        for name in glbs:
            source = sources[name]
            key = _source_key(source)
            if key not in glb_memo:
                glb_memo[key] = _embedded_image_digests(source)
            identities = glb_memo[key]
            if identities is None:
                certain = False; break
            images.update(identities)
        if not certain:
            continue
        for name in paths:
            identity = _cache_source_digest(sources[name], cache_memo)
            if identity is not None and identity not in images:
                unused.add(name)
    return unused

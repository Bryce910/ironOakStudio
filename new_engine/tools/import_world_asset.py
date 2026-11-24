#!/usr/bin/env python3
"""Copy bounded embedded static GLBs into immutable project revisions.

Never changes purchased sources. The engine validates materials/geometry before
calling this cooker and validates actual GPU loads before committing the scene.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import tempfile


def prepare(path, units):
    raw = path.read_bytes()
    if len(raw) > 128 * 1024 * 1024 or len(raw) < 20:
        raise ValueError("GLB is empty or exceeds128 MiB")
    magic, version, length = struct.unpack_from("<III", raw)
    if magic != 0x46546C67 or version != 2 or length != len(raw):
        raise ValueError("Invalid GLB2 header")
    chunks = []
    offset = 12
    while offset < len(raw):
        size, kind = struct.unpack_from("<II", raw, offset)
        offset += 8
        if size % 4 or size > len(raw) - offset:
            raise ValueError("Invalid chunk range")
        chunks.append((kind, raw[offset:offset + size]))
        offset += size
    if len(chunks) != 2 or chunks[0][0] != 0x4E4F534A or chunks[1][0] != 0x004E4942:
        raise ValueError("Embedded JSON/BIN GLB required")
    doc = json.loads(chunks[0][1])
    if doc.get("skins") or len(doc.get("nodes", [])) > 512 or len(doc.get("images", [])) > 128:
        raise ValueError("Use the character pipeline for rigs; static GLB limits apply")
    if any(v.get("uri") for v in doc.get("buffers", []) + doc.get("images", [])):
        raise ValueError("External buffers/images must be embedded")
    if units == 1:
        return raw
    if doc.get("animations"):
        raise ValueError("Bake animated node transforms before converting units")
    nodes = doc.get("nodes", [])
    children = {child for node in nodes for child in node.get("children", [])}
    for index, node in enumerate(nodes):
        if index in children:
            continue
        if "matrix" in node:
            matrix = node["matrix"]
            if len(matrix) != 16:
                raise ValueError("Invalid root matrix")
            for k in (0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14):
                matrix[k] *= units
        else:
            node["scale"] = [v * units for v in node.get("scale", [1, 1, 1])]
            node["translation"] = [v * units for v in node.get("translation", [0, 0, 0])]
    encoded = json.dumps(doc, separators=(",", ":"), allow_nan=False).encode()
    encoded += b" " * (-len(encoded) % 4)
    body = struct.pack("<II", len(encoded), 0x4E4F534A) + encoded
    body += struct.pack("<II", len(chunks[1][1]), 0x004E4942) + chunks[1][1]
    return struct.pack("<III", magic, version, len(body) + 12) + body


def cook(project, alias, sources, units, immutable_links=False):
    if immutable_links and units != 1:
        raise ValueError("Immutable hardlinks require unmodified metre-unit GLBs")
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,95}", alias):
        raise ValueError("Alias uses letters, digits, underscores and hyphens")
    if not math.isfinite(units) or not .001 <= units <= 1000:
        raise ValueError("Unit multiplier outside0.001–1000")
    if sources[2] and not sources[1]:
        raise ValueError("LOD2 requires LOD1")
    data = [prepare(path, units) for path in sources if path]
    digest = hashlib.sha256(b"Valwood static GLB revision1\0" + b"".join(
        struct.pack("<Q", len(value)) + value for value in data)).hexdigest()[:24]
    root = (project.resolve() / "models" / "imports").resolve()
    if not root.is_relative_to(project.resolve()):
        raise ValueError("Managed import folder escapes the project")
    target = root / alias / digest
    if not target.resolve().is_relative_to(root):
        raise ValueError("Managed revision escapes the import folder")
    target.mkdir(parents=True, exist_ok=True)
    for suffix, value, source in zip((".glb", "_mid.glb", "_far.glb"), data, (p for p in sources if p)):
        path = target / ("asset" + suffix)
        if path.exists():
            if path.read_bytes() != value:
                raise ValueError("Existing revision has different contents")
            continue
        if immutable_links:
            if units != 1:
                raise ValueError("Immutable hardlinks require unmodified metre-unit GLBs")
            import os
            # Explicit source-showcase option only. The caller owns these
            # immutable candidates; managed revisions never edit them in place.
            os.link(Path(source).resolve(), path)
            continue
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=target, prefix=".import-", delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(value)
                stream.flush()
                import os
                os.fsync(stream.fileno())
            # Windows disallows renaming a still-open NamedTemporaryFile.
            temporary.replace(path)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    # Both filenames point at the same immutable source bytes. Tree/purchased
    # aliases use _near; ordinary static props use the unsuffixed name.
    near = target / "asset_near.glb"
    if not near.exists():
        import os
        os.link(target / "asset.glb", near)
    return f"imports/{alias}/{digest}/asset"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--alias", required=True)
    parser.add_argument("--near", type=Path, required=True)
    parser.add_argument("--mid", type=Path)
    parser.add_argument("--far", type=Path)
    parser.add_argument("--units", type=float, default=1)
    parser.add_argument("--result", type=Path, required=True)
    args = parser.parse_args()
    try:
        stem = cook(args.project, args.alias, (args.near, args.mid, args.far), args.units)
        args.result.write_text("OK " + stem + "\n")
    except (ValueError, OSError, KeyError, IndexError, struct.error) as error:
        args.result.write_text("Import failed: " + str(error) + "\n")
        raise SystemExit(1)


if __name__ == "__main__":
    main()

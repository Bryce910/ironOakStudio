#!/usr/bin/env python3
"""Finalize a derived knight GLB for Valwood, preserving geometry and motion.

Source materials are restored from the balanced derivative manifest. Only
exactly constant LINEAR TRS animation samplers are reduced; no approximate
key reduction, retargeting, coordinate correction or geometric edits occur.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import io
import json
from pathlib import Path
import struct
import sys

import numpy as np
from PIL import Image

ENGINE = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ENGINE / "assets/purchased/modular_knights/derived/materials-balanced/materials.json"
DTYPES = {5120: np.dtype("i1"), 5121: np.dtype("u1"), 5122: np.dtype("<i2"), 5123: np.dtype("<u2"), 5125: np.dtype("<u4"), 5126: np.dtype("<f4")}
WIDTHS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    raw = path.read_bytes()
    if len(raw) < 20 or struct.unpack_from("<4sII", raw) != (b"glTF", 2, len(raw)):
        raise ValueError("Requires a complete GLB2 input")
    chunks = []
    offset = 12
    while offset < len(raw):
        if offset + 8 > len(raw):
            raise ValueError("Truncated GLB chunk header")
        length, kind = struct.unpack_from("<II", raw, offset)
        if length % 4 or offset + 8 + length > len(raw):
            raise ValueError("Invalid GLB chunk length")
        chunks.append((kind, raw[offset + 8:offset + 8 + length]))
        offset += 8 + length
    if len(chunks) != 2 or chunks[0][0] != 0x4E4F534A or chunks[1][0] != 0x004E4942:
        raise ValueError("Requires exactly one JSON and one BIN GLB chunk")
    doc = json.loads(chunks[0][1])
    if len(doc.get("buffers", [])) != 1 or "uri" in doc["buffers"][0]:
        raise ValueError("Requires one embedded buffer")
    length = doc["buffers"][0]["byteLength"]
    if length > len(chunks[1][1]):
        raise ValueError("Declared buffer exceeds GLB binary payload")
    return doc, bytearray(chunks[1][1][:length]), raw


def encode(doc, binary):
    doc["buffers"] = [{"byteLength": len(binary)}]
    header = json.dumps(doc, separators=(",", ":"), ensure_ascii=False, allow_nan=False).encode()
    header += b" " * (-len(header) % 4)
    payload = bytes(binary) + b"\0" * (-len(binary) % 4)
    length = 12 + 8 + len(header) + 8 + len(payload)
    return struct.pack("<4sII", b"glTF", 2, length) + struct.pack("<II", len(header), 0x4E4F534A) + header + struct.pack("<II", len(payload), 0x004E4942) + payload


def array(doc, binary, index):
    accessor = doc["accessors"][index]
    if "sparse" in accessor or "bufferView" not in accessor:
        raise ValueError("Sparse/missing buffer-view accessor unsupported")
    dtype = DTYPES[accessor["componentType"]]
    width = WIDTHS[accessor["type"]]
    view = doc["bufferViews"][accessor["bufferView"]]
    if view.get("buffer", 0) != 0 or view.get("extensions"):
        raise ValueError("External/compressed buffer views unsupported")
    stride = view.get("byteStride", dtype.itemsize * width)
    relative_offset = accessor.get("byteOffset", 0)
    start = view.get("byteOffset", 0) + relative_offset
    count = accessor["count"]
    span = (count - 1) * stride + dtype.itemsize * width if count else 0
    if count < 1 or stride < dtype.itemsize * width or relative_offset + span > view["byteLength"] or start + span > len(binary):
        raise ValueError("Unsafe accessor bounds/stride")
    return np.ndarray((count, width), dtype=dtype, buffer=binary, offset=start, strides=(stride, dtype.itemsize)).copy()


def append_view(doc, binary, payload):
    binary.extend(b"\0" * (-len(binary) % 4))
    index = len(doc.setdefault("bufferViews", []))
    doc["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(payload)})
    binary.extend(payload)
    return index


def append_accessor(doc, binary, values, original):
    values = np.ascontiguousarray(values)
    accessor = copy.deepcopy(original)
    accessor.update(bufferView=append_view(doc, binary, values.tobytes()), byteOffset=0, count=len(values))
    accessor.pop("min", None)
    accessor.pop("max", None)
    if values.shape[1] == 1:
        accessor["min"] = [float(values.min())]
        accessor["max"] = [float(values.max())]
    index = len(doc["accessors"])
    doc["accessors"].append(accessor)
    return index


def geometry_signature(doc, binary):
    records = []
    for mesh in doc.get("meshes", []):
        primitives = []
        for primitive in mesh.get("primitives", []):
            record = {"mode": primitive.get("mode", 4), "material": primitive.get("material"), "attributes": {}}
            for semantic, index in sorted(primitive["attributes"].items()):
                a = doc["accessors"][index]
                record["attributes"][semantic] = {"data_sha256": sha(array(doc, binary, index).tobytes()), "type": a["type"], "componentType": a["componentType"], "normalized": a.get("normalized", False)}
            if "indices" in primitive:
                record["indices"] = sha(array(doc, binary, primitive["indices"]).tobytes())
            primitives.append(record)
        records.append(primitives)
    return records


def skin_signature(doc, binary):
    return [{"joints": skin["joints"], "skeleton": skin.get("skeleton"), "inverse_bind_sha256": sha(array(doc, binary, skin["inverseBindMatrices"]).tobytes()) if "inverseBindMatrices" in skin else None} for skin in doc.get("skins", [])]


def material_patch(doc, binary, manifest, preserve_materials=()):
    by_name = {}
    for path, row in manifest.items():
        if row["name"] in by_name:
            raise ValueError(f"Manifest has ambiguous exported material name: {row['name']}")
        by_name[row["name"]] = (path, row)
    sampler = {"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}
    sampler_index = len(doc.setdefault("samplers", []))
    doc["samplers"].append(sampler)
    cache = {}
    images = doc.setdefault("images", [])
    textures = doc.setdefault("textures", [])
    normal_reports, material_reports = [], []

    def embed(path):
        encoded = Path(path).read_bytes()
        digest = sha(encoded)
        if digest in cache:
            return cache[digest]
        with Image.open(io.BytesIO(encoded)) as image:
            if max(image.size) > 4096:
                raise ValueError(f"Texture exceeds runtime maximum: {path}")
            mime = {"PNG": "image/png", "JPEG": "image/jpeg"}.get(image.format)
            if mime is None:
                raise ValueError(f"Unsupported derivative image format: {path}")
        index = len(images)
        images.append({"name": Path(path).name, "bufferView": append_view(doc, binary, encoded), "mimeType": mime})
        texture = len(textures)
        textures.append({"source": index, "sampler": sampler_index})
        cache[digest] = texture
        return texture

    preserve_materials = set(preserve_materials)
    exported_names = {material.get("name") for material in doc.get("materials", [])}
    missing_preserved = preserve_materials - exported_names
    if missing_preserved:
        raise ValueError(f"Explicit preserved material name is absent from export: {sorted(missing_preserved)}")
    for material in doc.get("materials", []):
        name = material.get("name")
        if name not in by_name:
            if name not in preserve_materials:
                raise ValueError(f"Export material has no exact manifest name match: {name}")
            # This opt-in is only for explicitly named, plain fixed-PBR
            # procedural materials. It must never bypass source image bindings,
            # unsupported shader features or normal/alpha semantics.
            if texture_references(material) or material.get("extensions"):
                raise ValueError(f"Explicit preserved material must be texture-free fixed PBR: {name}")
            pbr = material.get("pbrMetallicRoughness", {})
            color = pbr.get("baseColorFactor", [1, 1, 1, 1])
            values = color + [pbr.get("metallicFactor", 1), pbr.get("roughnessFactor", 1)]
            if len(color) != 4 or not np.all(np.isfinite(values)) or any(value < 0 or value > 1 for value in values):
                raise ValueError(f"Invalid fixed PBR factors in explicitly preserved material: {name}")
            if material.get("alphaMode", "OPAQUE") != "OPAQUE":
                raise ValueError(f"Explicit preserved procedural material must be opaque: {name}")
            material_reports.append({"name": name, "source_material": None, "preserved_by_explicit_name": True,
                                     "scope": "Texture-free fixed PBR derivative preserved exactly as exported",
                                     "base_color_factor": color, "metallic_factor": pbr.get("metallicFactor", 1),
                                     "roughness_factor": pbr.get("roughnessFactor", 1)})
            continue
        path, row = by_name[name]
        if row["alpha_mode"] not in ("OPAQUE", "MASK"):
            raise ValueError(f"Unsupported manifest alpha mode: {path}")
        color = row["base_factorRGBA"]
        factors = color + [row["metallic_factor"], row["roughness_factor"], row["alpha_cutoff"]]
        if len(color) != 4 or not np.all(np.isfinite(factors)) or any(value < 0 or value > 1 for value in factors):
            raise ValueError(f"Manifest PBR factors must be finite and within [0,1]: {path}")
        pbr = material.setdefault("pbrMetallicRoughness", {})
        pbr["baseColorFactor"] = row["base_factorRGBA"]
        pbr["metallicFactor"] = row["metallic_factor"]
        pbr["roughnessFactor"] = row["roughness_factor"]
        pbr["baseColorTexture"] = {"index": embed(row["base"]), "texCoord": 0}
        orm = embed(row["orm"])
        pbr["metallicRoughnessTexture"] = {"index": orm, "texCoord": 0}
        # AO strength was applied to the derivative once, never apply it twice.
        material["occlusionTexture"] = {"index": orm, "texCoord": 0, "strength": 1}
        if row["normal"]:
            material["normalTexture"] = {"index": embed(row["normal"]), "texCoord": 0, "scale": row["normal_scale"]}
        else:
            material.pop("normalTexture", None)
        material["alphaMode"] = row["alpha_mode"]
        material["doubleSided"] = row["double_sided"]
        if row["alpha_mode"] == "MASK":
            material["alphaCutoff"] = row["alpha_cutoff"]
        else:
            material.pop("alphaCutoff", None)
        flip = row.get("source_bindings", {}).get("_BumpMap", {}).get("flip_green_channel", False)
        normal_reports.append({"material": name, "source_flip_green_enabled": flip, "derivative_already_applied_source_flip": bool(flip), "additional_export_green_flip": False})
        material_reports.append({"name": name, "source_material": path, "alpha_mode": row["alpha_mode"], "normal_scale": row["normal_scale"], "AO_and_metallic_roughness_share_one_texture": True, "source_warnings": row.get("warnings", []), "runtime_override": row.get("runtime_override")})
    return material_reports, normal_reports


def optimize_constants(doc, binary):
    reductions, durations = [], []
    for ai, animation in enumerate(doc.get("animations", [])):
        used = {channel["sampler"] for channel in animation["channels"]}
        paths = {}
        for channel in animation["channels"]:
            paths.setdefault(channel["sampler"], set()).add(channel["target"]["path"])
        duration_before = max(float(array(doc, binary, animation["samplers"][index]["input"])[-1, 0]) for index in used)
        for si in used:
            sampler = animation["samplers"][si]
            if sampler.get("interpolation", "LINEAR") != "LINEAR" or not paths[si] <= {"translation", "rotation", "scale"}:
                continue
            times = array(doc, binary, sampler["input"])
            values = array(doc, binary, sampler["output"])
            if len(times) <= 2 or len(values) != len(times) or not np.all(values == values[0]):
                continue
            old_input, old_output = sampler["input"], sampler["output"]
            sampler["input"] = append_accessor(doc, binary, times[[0, -1]], doc["accessors"][old_input])
            sampler["output"] = append_accessor(doc, binary, values[[0, -1]], doc["accessors"][old_output])
            reductions.append({"animation": animation.get("name", str(ai)), "sampler": si, "paths": sorted(paths[si]), "old_keys": len(times), "new_keys": 2, "first_time": float(times[0, 0]), "last_time": float(times[-1, 0]), "constant_values_exactly_equal": True})
        duration_after = max(float(array(doc, binary, animation["samplers"][index]["input"])[-1, 0]) for index in used)
        if duration_after != duration_before:
            raise ValueError("Constant optimization changed animation duration")
        durations.append({"name": animation.get("name", str(ai)), "before": duration_before, "after": duration_after})
    return reductions, durations


def accessor_references(doc):
    references = []
    for mesh in doc.get("meshes", []):
        for primitive in mesh["primitives"]:
            references.extend((primitive["attributes"], name) for name in primitive["attributes"])
            if "indices" in primitive:
                references.append((primitive, "indices"))
    for skin in doc.get("skins", []):
        if "inverseBindMatrices" in skin:
            references.append((skin, "inverseBindMatrices"))
    for animation in doc.get("animations", []):
        for sampler in animation["samplers"]:
            references.extend([(sampler, "input"), (sampler, "output")])
    return references


def texture_references(value):
    references = []
    if isinstance(value, dict):
        for name, item in value.items():
            if name.endswith("Texture") and isinstance(item, dict) and "index" in item:
                references.append((item, "index"))
            else:
                references.extend(texture_references(item))
    elif isinstance(value, list):
        for item in value:
            references.extend(texture_references(item))
    return references


def compact(doc, binary):
    # Keep remaining unknown extension index semantics out of compaction.
    for view in doc.get("bufferViews", []):
        if view.get("extensions"):
            raise ValueError("Compressed/extension buffer-view compaction unsupported")
    refs = texture_references(doc.get("materials", []))
    used_textures = sorted({obj[key] for obj, key in refs})
    texture_map = {old: new for new, old in enumerate(used_textures)}
    for obj, key in refs:
        obj[key] = texture_map[obj[key]]
    doc["textures"] = [doc["textures"][index] for index in used_textures]
    used_images = sorted({texture["source"] for texture in doc["textures"]})
    image_map = {old: new for new, old in enumerate(used_images)}
    for texture in doc["textures"]:
        texture["source"] = image_map[texture["source"]]
    doc["images"] = [doc["images"][index] for index in used_images]
    refs = accessor_references(doc)
    used_accessors = sorted({obj[key] for obj, key in refs})
    accessor_map = {old: new for new, old in enumerate(used_accessors)}
    for obj, key in refs:
        obj[key] = accessor_map[obj[key]]
    doc["accessors"] = [doc["accessors"][index] for index in used_accessors]
    refs = [(accessor, "bufferView") for accessor in doc["accessors"]]
    refs += [(image, "bufferView") for image in doc["images"]]
    used_views = sorted({obj[key] for obj, key in refs})
    view_map = {old: new for new, old in enumerate(used_views)}
    for obj, key in refs:
        obj[key] = view_map[obj[key]]
    old_views = doc["bufferViews"]
    doc["bufferViews"] = []
    compacted = bytearray()
    for index in used_views:
        old = old_views[index]
        start, length = old.get("byteOffset", 0), old["byteLength"]
        if start < 0 or length < 0 or start + length > len(binary):
            raise ValueError("Invalid original buffer-view bounds")
        new_index = append_view(doc, compacted, binary[start:start + length])
        for key, value in old.items():
            if key not in ("buffer", "byteOffset", "byteLength"):
                doc["bufferViews"][new_index][key] = value
    return compacted


def validate(doc, binary):
    nodes, skins, meshes = doc.get("nodes", []), doc.get("skins", []), doc.get("meshes", [])
    if not 1 <= len(nodes) <= 256 or len(skins) != 1 or len(meshes) != 1:
        raise ValueError("Runtime requires <=256 nodes, exactly one skin and one mesh")
    mesh_nodes = [node for node in nodes if "mesh" in node]
    if len(mesh_nodes) != 1 or mesh_nodes[0].get("skin") != 0 or mesh_nodes[0]["mesh"] != 0:
        raise ValueError("Runtime requires exactly one skinned mesh node")
    joints = skins[0]["joints"]
    if not 1 <= len(joints) <= 128 or len(set(joints)) != len(joints) or any(j < 0 or j >= len(nodes) for j in joints):
        raise ValueError("Skin exceeds runtime joint budget or has invalid joints")
    if any("weights" in node for node in nodes) or any("weights" in mesh for mesh in meshes):
        raise ValueError("Morph weights unsupported")
    vertices, indices = 0, 0
    primitives = meshes[0]["primitives"]
    if not 1 <= len(primitives) <= 128:
        raise ValueError("Primitive budget exceeded")
    for primitive in primitives:
        attributes = primitive["attributes"]
        if primitive.get("mode", 4) != 4 or primitive.get("targets") or primitive.get("extensions"):
            raise ValueError("Requires uncompressed, morph-free triangle primitives")
        if "POSITION" not in attributes or "JOINTS_0" not in attributes or "WEIGHTS_0" not in attributes or any(name in attributes for name in ("JOINTS_1", "WEIGHTS_1")):
            raise ValueError("Requires POSITION/JOINTS_0/WEIGHTS_0 with at most four influences")
        positions = array(doc, binary, attributes["POSITION"])
        joint_values = array(doc, binary, attributes["JOINTS_0"])
        weight_accessor = doc["accessors"][attributes["WEIGHTS_0"]]
        weights = array(doc, binary, attributes["WEIGHTS_0"])
        if joint_values.shape != (len(positions), 4) or weights.shape != joint_values.shape:
            raise ValueError("Skin accessor shape mismatch")
        if weights.dtype.kind in "ui":
            if not weight_accessor.get("normalized"):
                raise ValueError("Integer skin weights must be normalized")
            weights = weights.astype(np.float64) / np.iinfo(weights.dtype).max
        if not np.all(np.isfinite(positions)) or not np.all(np.isfinite(weights)) or np.any(weights < 0) or np.any(weights > 1) or np.any(np.abs(weights.sum(axis=1) - 1) > .02) or np.any(joint_values >= len(joints)):
            raise ValueError("Invalid vertex/joint/weight values")
        vertices += len(positions)
        if "indices" in primitive:
            faces = array(doc, binary, primitive["indices"])
            if np.any(faces >= len(positions)):
                raise ValueError("Triangle index outside vertex range")
            indices += len(faces)
            if len(faces) % 3:
                raise ValueError("Non-triangle index count")
        else:
            indices += len(positions)
    if vertices > 200000 or indices > 600000:
        raise ValueError("Runtime vertex/index budget exceeded")
    if len(doc.get("animations", [])) > 64:
        raise ValueError("Runtime animation clip budget exceeded")
    keys = 0
    for animation in doc.get("animations", []):
        channels = animation.get("channels", [])
        if not channels or len(channels) > 768:
            raise ValueError("Runtime animation channel budget exceeded")
        targets = set()
        for channel in channels:
            target = channel["target"]
            pair = (target["node"], target["path"])
            if pair in targets or pair[0] < 0 or pair[0] >= len(nodes) or "matrix" in nodes[pair[0]] or pair[1] not in ("translation", "rotation", "scale"):
                raise ValueError("Invalid/duplicate animation target")
            targets.add(pair)
            sampler = animation["samplers"][channel["sampler"]]
            times = array(doc, binary, sampler["input"])
            values = array(doc, binary, sampler["output"])
            interpolation = sampler.get("interpolation", "LINEAR")
            multiplier = 3 if interpolation == "CUBICSPLINE" else 1
            if interpolation not in ("LINEAR", "STEP", "CUBICSPLINE") or times.shape[1] != 1 or len(times) > 16384 or len(values) != len(times) * multiplier or values.shape[1] != (4 if pair[1] == "rotation" else 3):
                raise ValueError("Unsupported animation sampler shape/key count")
            if not np.all(np.isfinite(times)) or not np.all(np.isfinite(values)) or np.any(times < 0) or np.any(np.diff(times[:, 0]) <= 0):
                raise ValueError("Invalid animation time/value data")
            keys += len(times)
    if keys > 262144:
        raise ValueError(f"Runtime total animation key budget exceeded: {keys}")
    if len(doc.get("images", [])) > 128 or len(doc.get("materials", [])) > 128:
        raise ValueError("Runtime image/material count budget exceeded")
    image_bytes, decoded_bytes = 0, 0
    for image in doc.get("images", []):
        if "uri" in image or "bufferView" not in image:
            raise ValueError("Runtime requires embedded images")
        view = doc["bufferViews"][image["bufferView"]]
        payload = binary[view["byteOffset"]:view["byteOffset"] + view["byteLength"]]
        image_bytes += len(payload)
        with Image.open(io.BytesIO(payload)) as source:
            width, height = source.size
        if min(width, height) < 1 or max(width, height) > 4096:
            raise ValueError("Runtime image dimension budget exceeded")
        while True:
            decoded_bytes += width * height * 4
            if width == height == 1:
                break
            width, height = max(1, width // 2), max(1, height // 2)
    if decoded_bytes > 256 * 1024 * 1024:
        raise ValueError(f"Runtime decoded RGBA8 full-mip texture budget exceeded: {decoded_bytes}")
    return {"nodes": len(nodes), "skins": 1, "joints": len(joints), "skinned_mesh_nodes": 1, "vertices": vertices, "indices": indices, "triangles": indices // 3, "primitives": len(primitives), "clips": len(doc.get("animations", [])), "total_channel_keys": keys, "images": len(doc.get("images", [])), "embedded_image_bytes": image_bytes, "estimated_RGBA8_full_mips_bytes": decoded_bytes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--materials", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--preserve-material", action="append", default=[], metavar="EXACT_NAME",
                        help="Explicitly allow one unmapped, opaque, texture-free fixed-PBR material; repeat per exact material name")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        raise ValueError("Use a separate output GLB; the Blender export is preserved")
    purchased_source = (ENGINE / "assets/purchased/modular_knights/source").resolve()
    if purchased_source in args.output.resolve().parents:
        raise ValueError("Cannot write to purchased source assets")
    doc, binary, original = load(args.input)
    geometry_before, skin_before, nodes_before = geometry_signature(doc, binary), skin_signature(doc, binary), copy.deepcopy(doc["nodes"])
    manifest = json.loads(args.materials.read_text())
    materials, normal_flips = material_patch(doc, binary, manifest, args.preserve_material)
    reductions, durations = optimize_constants(doc, binary)
    before_compaction = len(binary)
    binary = compact(doc, binary)
    if geometry_before != geometry_signature(doc, binary) or skin_before != skin_signature(doc, binary) or nodes_before != doc["nodes"]:
        raise ValueError("Finalization changed geometry, inverse bind matrices or node transforms")
    limits = validate(doc, binary)
    output = encode(doc, binary)
    if len(output) > 32 * 1024 * 1024:
        raise ValueError(f"Final GLB exceeds runtime 32MiB file budget: {len(output)}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_bytes(output)
    temporary.replace(args.output)
    report = {
        "passed": True, "scope": "Standalone derived asset material finalization and exactly constant LINEAR TRS storage optimization; no gameplay integration",
        "input": str(args.input.resolve()), "input_bytes": len(original), "input_sha256": sha(original),
        "output": str(args.output.resolve()), "output_bytes": len(output), "output_sha256": sha(output),
        "materials_manifest": str(args.materials.resolve()), "materials_manifest_sha256": sha(args.materials.read_bytes()),
        "source_geometry_attribute_and_index_values_unchanged": True, "inverse_bind_values_unchanged": True, "node_transforms_unchanged": True,
        "constant_linear_channels_only": True, "approximate_animation_simplification": False,
        "constant_samplers_reduced": len(reductions), "constant_reductions": reductions, "clip_durations": durations,
        "binary_bytes_before_compaction": before_compaction, "binary_bytes_after_compaction": len(binary),
        "runtime_limits": limits, "materials": materials, "normal_green_semantics": normal_flips,
        "explicit_preserved_material_names": args.preserve_material,
        "unused_manifest_material_paths": sorted(path for path, row in manifest.items() if row["name"] not in {material.get("name") for material in doc.get("materials", [])}),
        "normal_convention": "Final material binds source-derived glTF tangent normals directly. Source TextureImporter.flipGreenChannel is applied by the material converter once; finalization performs no additional green flip.",
        "limitations": ["Budget checks are followed by the actual engine importer/CPU inspector and GPU preview; they do not prove retarget quality or gameplay animation flow."]}
    destination = args.report or args.output.with_suffix(".report.json")
    destination.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": True, "output": str(args.output.resolve()), "output_bytes": len(output), "constant_samplers_reduced": len(reductions), "report": str(destination.resolve()), **limits}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, IndexError, TypeError, struct.error) as error:
        print(f"finalize_modular_knight_glb: {error}", file=sys.stderr)
        sys.exit(1)

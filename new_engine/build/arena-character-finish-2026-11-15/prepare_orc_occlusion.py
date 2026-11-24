#!/usr/bin/env python3
"""Repair zeroed palette/grayscale Orc AO in an exact-byte override bank.

Source GLBs, original texture PNGs, and all source artwork remain read-only.
The existing loader resolves external PNG URIs and applies this exact encoded
byte identity override before ARM upload and any large-encounter size cap.
"""
from __future__ import annotations

import ctypes
import hashlib
import json
from pathlib import Path
import shlex
import struct

import numpy as np
from PIL import Image

STAGE = Path(__file__).resolve().parent
ENGINE = STAGE.parents[1]
BANK = STAGE / "character-textures"
ROSTER = STAGE / "arena-project/arena_orcs_roster"
REPORT = STAGE / "ORC_OCCLUSION_REVIEW.json"


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def hasher():
    library = ctypes.CDLL(str(ENGINE / "build/arena-material-quality-2026-11-15/review/hash.so"))
    library.hash.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    library.hash.restype = ctypes.c_uint64
    return lambda data: int(library.hash(data, len(data)))


def manifest(path):
    lines = path.read_text().splitlines()
    assert lines[0] == "VALWOOD_CHARACTER_TEXTURES 1"
    result = {}
    for line in lines[1:]:
        words = shlex.split(line)
        assert len(words) == 11
        identity = (int(words[0]), int(words[1]))
        assert identity not in result
        result[identity] = [(words[i], int(words[i+1]), int(words[i+2]))
                            for i in (2, 5, 8)]
    return result


def glb_document(path):
    with path.open("rb") as stream:
        header = stream.read(20)
        assert header[:4] == b"glTF" and header[16:20] == b"JSON"
        data = stream.read(struct.unpack_from("<I", header, 12)[0])
        return json.loads(data), hashlib.sha256(data).hexdigest()


def linear(values):
    return np.where(values <= .04045, values / 12.92,
                    ((values + .055) / 1.055) ** 2.4)


def resize(values, size):
    return np.asarray(Image.fromarray(values.astype(np.float32), "F").resize(
        size, Image.Resampling.LANCZOS), np.float32)


def quant(values):
    return np.rint(np.clip(values, 0, 1) * 255).astype(np.uint8)


def run():
    fnv = hasher()
    entries = manifest(BANK / "textures.manifest")
    accepted = manifest(ENGINE / "build/arena-material-quality-2026-11-15/character-textures/textures.manifest")
    head_report = json.loads((STAGE / "HEAD_OCCLUSION_REVIEW.json").read_text())
    assert head_report["passed"] and len(accepted) == 586
    old_entries = {identity: entries[identity] for identity in accepted}
    assert set(accepted).issubset(entries)
    original_inputs = {}
    def pin(path):
        path = Path(path).resolve()
        original_inputs[str(path)] = sha(path)
    pin(STAGE / "HEAD_OCCLUSION_REVIEW.json")
    pin(ENGINE / "build/arena-material-quality-2026-11-15/character-textures/textures.manifest")
    source_code = STAGE / "source-v1/engine/world_models.cpp"
    code = source_code.read_text()
    function = code[code.index("static GLuint material_texture("):]
    assert function.index("world_model_image_bytes::read(") < function.index("world_character_textures::replace(") < function.index("texture_cache_acquire(")
    pin(source_code)
    repairs = []
    identities = set()
    correspondence = []
    for number in range(1, 7):
        character = f"orc{number:02d}"
        folder = ROSTER / character
        mapping = folder / "ORC_TEXTURE_MAPPING.json"
        pin(mapping)
        data = json.loads(mapping.read_text())
        affected = []
        for material in data["materials"]:
            conversion = material["conversions"].get("arm")
            source = material["source_images"].get("occlusion")
            if not conversion or not source:
                continue
            original = Path(conversion["path"])
            before = np.asarray(Image.open(original).convert("RGB"))
            if before[:, :, 0].max() != 0:
                continue
            assert before[:, :, 0].min() == 0
            raw = original.read_bytes()
            assert hashlib.sha256(raw).hexdigest() == conversion["sha256"]
            identity = (fnv(raw), len(raw))
            assert identity not in identities and identity not in accepted
            identities.add(identity)
            pin(original)
            pin(source["path"])
            pin(source["meta_path"])
            pin(material["source_mat"])
            with Image.open(source["path"]) as image:
                assert image.mode in ("P", "L")
                source_mode = image.mode
                source_size = image.size
                # Fix the historical helper's P/L bands check: expand the
                # authored palette/grayscale first, then ask for actual G.
                green = np.asarray(image.convert("RGBA").getchannel("G"),
                                   np.float32) / 255
            assert green.max() > 0 and green.min() < green.max()
            if source["import_flags"]["srgb"]:
                green = linear(green)
            strength = material["unity_floats"].get("_OcclusionStrength", 1)
            outputs = []
            for level in ("512", "1024", "2048"):
                height, width = before.shape[:2]
                if level == "512":
                    factor = min(1., 512 / max(width, height))
                    width, height = round(width * factor), round(height * factor)
                assert width <= source_size[0] and height <= source_size[1]
                assert width <= before.shape[1] and height <= before.shape[0]
                dimensions = (width, height)
                expected = before.copy() if dimensions == (before.shape[1], before.shape[0]) else np.dstack([
                    quant(resize(before[:, :, channel].astype(np.float32) / 255, dimensions))
                    for channel in range(3)])
                ao = quant(1 - strength * (1 - resize(green, dimensions)))
                after = expected.copy()
                after[:, :, 0] = ao
                relative = f"{level}/orc-ao-{identity[0]:016x}-{identity[1]}.png"
                path = BANK / relative
                assert not path.is_symlink()
                Image.fromarray(after, "RGB").save(path, optimize=False)
                decoded = np.asarray(Image.open(path).convert("RGB"))
                assert np.array_equal(decoded[:, :, 0], ao)
                assert np.array_equal(decoded[:, :, 1:], expected[:, :, 1:])
                if level != "512":
                    assert np.array_equal(decoded[:, :, 1:], before[:, :, 1:])
                encoded = path.read_bytes()
                outputs.append((relative, len(encoded), fnv(encoded)))
            entries[identity] = outputs
            affected.append((int(material["material_index"]), original.resolve(), identity))
            repairs.append(dict(character=character, material=material["unity_name"],
                original_texture=str(original), source_ao=str(Path(source["path"])),
                source_mode=source_mode, source_dimensions=list(source_size),
                source_srgb_decode=bool(source["import_flags"]["srgb"]),
                occlusion_strength=strength, identity=list(identity),
                original_ao_min=0, original_ao_max=0,
                corrected_ao_min=int(ao.min()), corrected_ao_max=int(ao.max()),
                corrected_ao_mean=float(ao.mean()),
                variants=[dict(path=p, bytes=count, fnv1a64=check,
                    dimensions=list(Image.open(BANK / p).size)) for p, count, check in outputs],
                medium_high_roughness_and_metal_pixel_identical=True,
                low_roughness_and_metal="separate linear-data float Lanczos reduction of original decoded1K channels; no artistic change",
                no_bitmap_flip=True, no_upscaling=True))
        assert affected, character
        frames = sorted((folder / "body").glob("combat_body_*.glb"))
        assert frames
        frame_receipts = []
        for frame in frames:
            document, document_sha = glb_document(frame)
            for material_index, original, identity in affected:
                material = document["materials"][material_index]
                texture_index = material["pbrMetallicRoughness"]["metallicRoughnessTexture"]["index"]
                assert material["occlusionTexture"]["index"] == texture_index
                assert material["occlusionTexture"].get("strength", 1) == 1
                image = document["images"][document["textures"][texture_index]["source"]]
                # Real Orc GLBs use external exact PNG URIs, not an embedded
                # bufferView. The loader feeds the same encoded bytes to FNV.
                assert "uri" in image and "bufferView" not in image
                assert (frame.parent / image["uri"]).resolve() == original
                source_bytes = original.read_bytes()
                assert (fnv(source_bytes), len(source_bytes)) == identity
            frame_receipts.append(dict(path=str(frame), json_sha256=document_sha))
        pin(frames[0])
        correspondence.append(dict(character=character, body_frames_checked=len(frames),
            external_png_uri_identity_matches=True, embedded_png_buffer_views=False,
            matched_materials=len(affected), representative_glb_sha256=sha(frames[0]),
            document_receipts=frame_receipts))
    assert len(repairs) == len(identities) == 16
    assert len(entries) == 602
    assert {identity: entries[identity] for identity in accepted} == old_entries
    lines = ["VALWOOD_CHARACTER_TEXTURES 1"]
    for identity, variants in sorted(entries.items()):
        lines.append(f"{identity[0]} {identity[1]} " + " ".join(
            f'"{path}" {count} {check}' for path, count, check in variants))
    (BANK / "textures.manifest").write_text("\n".join(lines) + "\n")
    assert manifest(BANK / "textures.manifest") == entries
    checked_bytes = checked_files = 0
    for variants in entries.values():
        for relative, count, check in variants:
            path = BANK / relative
            assert path.is_file() and not path.is_symlink()
            raw = path.read_bytes()
            assert len(raw) == count and fnv(raw) == check
            with Image.open(path) as image:
                image.verify()
            checked_files += 1
            checked_bytes += len(raw)
    for row in head_report["replaced_head01_orm_variants"]:
        assert sha(BANK / row["path"]) == row["replacement_sha256"]
    for path, check in original_inputs.items():
        assert sha(path) == check, path
    report = dict(passed=True, repaired_materials=16, added_source_identities=16,
        preserved_source_identities=586, total_source_identities=602,
        checked_manifest_variants=checked_files, verified_encoded_bytes=checked_bytes,
        original_materials_glbs_source_art_and_accepted_assets_modified=False,
        head01_repairs_preserved=True,
        formula="sourceRGBA.G /255; source-sRGB decode when flagged; float Lanczos resize;1-strength*(1-AO);clamp/round8bit; source decoded G/B retained",
        source_image_orientation="Pillow decoder source orientation; no additional bitmapflip",
        loader_override="external PNG bytes read by world_model_image_bytes::read then exactFNV/length replaced by world_character_textures::replace beforeARMupload and NPCedgecap",
        repairs=repairs, actual_glb_correspondence=correspondence,
        immutable_inputs_sha256=original_inputs,
        output_manifest_sha256=sha(BANK / "textures.manifest"))
    REPORT.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(passed=True, added_source_identities=16,
        total_source_identities=602, checked_manifest_variants=checked_files,
        verified_encoded_bytes=checked_bytes, report=str(REPORT))))


if __name__ == "__main__":
    run()

#!/usr/bin/env python3
"""Repair the owned Head01 AO reference in a new, independently checked bank.

Never modifies the purchased source, accepted bank, or any model. The source
material points its AO slot at a nonexistent GUID although its same-head AO
image is included in the pack. Its ORM's smoothness/metal channels are retained.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

import numpy as np
from PIL import Image


STAGE = Path(__file__).resolve().parent
ENGINE = STAGE.parents[1]
ACCEPTED = ENGINE / "build/arena-material-quality-2026-11-15/character-textures"
SOURCE = ENGINE / "assets/purchased/modular_knights/source/Assets/_TheTalesFactory/Modular Knights/Textures/Body/Head01/Textures/KnightHead01_Occlusion.png"
SOURCE_MATERIAL = SOURCE.parent.parent / "KnightHead01.mat"
MATERIALS = ENGINE / "build/arena-character-roster-2026-11-14/knights_roster/materials/materials-gltf.json"
BANK = STAGE / "character-textures"
REPORT = STAGE / "HEAD_OCCLUSION_REVIEW.json"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def hasher():
    # Reuse the accepted CPU-only FNV helper without altering it.
    path = ACCEPTED.parent / "review/hash.so"
    library = ctypes.CDLL(str(path))
    library.hash.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    library.hash.restype = ctypes.c_uint64
    return lambda data: int(library.hash(data, len(data)))


def parse_manifest(path):
    lines = path.read_text().splitlines()
    assert lines[0] == "VALWOOD_CHARACTER_TEXTURES 1"
    entries = []
    for line in lines[1:]:
        words = shlex.split(line)
        assert len(words) == 11
        identity = (int(words[0]), int(words[1]))
        variants = [(words[i], int(words[i+1]), int(words[i+2]))
                    for i in (2, 5, 8)]
        entries.append((identity, variants))
    assert len(entries) == 586 and len({i for i, _ in entries}) == 586
    return entries


def run(mode):
    fingerprint = hasher()
    inputs = {str(path): sha(path) for path in
              (SOURCE, SOURCE.with_suffix(".png.meta"), SOURCE_MATERIAL,
               MATERIALS, ACCEPTED / "textures.manifest")}
    rows = json.loads(MATERIALS.read_text())
    matches = [(key, value) for key, value in rows.items()
               if "/Body/Head01/" in key]
    assert len(matches) == 1
    key, material = matches[0]
    old_orm = Path(material["orm"])
    old_bytes = old_orm.read_bytes()
    repair_identity = (fingerprint(old_bytes), len(old_bytes))
    assert repair_identity == (1112535854094684086, 153447)
    assert material["source_bindings"]["_OcclusionMap"].get("missing_optional_reference")
    assert material["source_parameters"]["_OcclusionStrength"] == 1
    source_image = Image.open(SOURCE)
    assert source_image.size == (4096, 4096)
    # This is the converter's exact channel/layout convention: raw G samples,
    # independent LANCZOS reduction, no sRGB conversion and no vertical flip.
    source_green = source_image.getchannel("G")
    entries = parse_manifest(ACCEPTED / "textures.manifest")
    BANK.mkdir(exist_ok=True)
    replacements = []
    lines = ["VALWOOD_CHARACTER_TEXTURES 1"]
    unchanged_count = 0
    file_total = 0
    for identity, variants in entries:
        output_variants = []
        for relative, byte_count, old_hash in variants:
            original = ACCEPTED / relative
            destination = BANK / relative
            destination.parent.mkdir(exist_ok=True)
            assert original.is_file() and not original.is_symlink()
            assert original.stat().st_size == byte_count
            if identity == repair_identity:
                before = np.asarray(Image.open(original).convert("RGB"))
                height, width = before.shape[:2]
                assert width <= source_image.width and height <= source_image.height
                ao = np.asarray(source_green.resize((width, height), Image.Resampling.LANCZOS))
                after = before.copy()
                assert np.all(before[:, :, 0] == 255)
                after[:, :, 0] = ao
                if destination.is_symlink():
                    destination.unlink()
                Image.fromarray(after, "RGB").save(destination, optimize=False)
                decoded = np.asarray(Image.open(destination).convert("RGB"))
                assert np.array_equal(decoded[:, :, 1:], before[:, :, 1:])
                assert np.array_equal(decoded[:, :, 0], ao)
                assert Image.open(destination).size == Image.open(original).size
                data = destination.read_bytes()
                output_variants.append((relative, len(data), fingerprint(data)))
                replacements.append(dict(path=relative, dimensions=[width, height],
                    original_sha256=sha(original), replacement_sha256=sha(destination),
                    unchanged_roughness_and_metal=True, ao_matches_source_green=True,
                    ao_min=int(ao.min()), ao_max=int(ao.max()),
                    ao_mean=float(ao.mean()), originally_flat_white=True,
                    vertically_flipped=False, upscaled=False))
            else:
                if not destination.exists() and not destination.is_symlink():
                    if mode == "symlink":
                        destination.symlink_to(original)
                    else:
                        # reflink=auto may share immutable extents, never inodes.
                        subprocess.run(["cp", "--reflink=auto", "--", str(original),
                                        str(destination)], check=True)
                if mode == "copy":
                    assert not destination.is_symlink()
                    assert destination.stat().st_ino != original.stat().st_ino
                else:
                    assert destination.is_symlink() and destination.resolve() == original
                output_variants.append((relative, byte_count, old_hash))
                unchanged_count += 1
            file_total += 1
        lines.append(f"{identity[0]} {identity[1]} " + " ".join(
            f'"{p}" {count} {check}' for p, count, check in output_variants))
    assert len(replacements) == 3 and unchanged_count == 1755
    (BANK / "textures.manifest").write_text("\n".join(lines) + "\n")
    actual = parse_manifest(BANK / "textures.manifest")
    assert [identity for identity, _ in entries] == [identity for identity, _ in actual]
    # Every manifest byte count and FNV checksum is checked, not only repairs.
    verified_bytes = 0
    for _, variants in actual:
        for relative, byte_count, check in variants:
            file = BANK / relative
            data = file.read_bytes()
            assert len(data) == byte_count and fingerprint(data) == check, relative
            with Image.open(file) as image:
                image.verify()
            verified_bytes += len(data)
    for path, original_hash in inputs.items():
        assert sha(path) == original_hash, f"Input changed: {path}"
    assert sha(old_orm) == hashlib.sha256(old_bytes).hexdigest()
    report = dict(passed=True, registered_source_identities=len(actual),
        checked_manifest_files=file_total, unchanged_variants=unchanged_count,
        storage_mode=mode, runtime_symlink_policy_compatible=mode == "copy",
        replaced_head01_orm_variants=replacements,
        exact_material_key=key,
        source_occlusion_texture=str(SOURCE),
        orientation="same raw top-to-bottom G samples as established material converter; no flips",
        source_ao_reference="missing GUID a0e44fa62daf1964bba4f9936f879c56; included matching-head AO GUID5540691ae6fe3304e98dbb76d21dfbb5 is assigned to source ParallaxMap",
        source_material_and_owned_textures_modified=False,
        accepted_bank_modified=False, verified_manifest_encoded_bytes=verified_bytes,
        original_inputs_sha256=inputs,
        original_orm_sha256=hashlib.sha256(old_bytes).hexdigest(),
        output_manifest_sha256=sha(BANK / "textures.manifest"))
    REPORT.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(passed=True, bank=str(BANK), report=str(REPORT),
        registered_source_identities=len(actual), checked_manifest_files=file_total,
        replaced_variants=3, storage_mode=mode)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--unchanged", choices=("copy", "symlink"), required=True)
    args = parser.parse_args()
    run(args.unchanged)

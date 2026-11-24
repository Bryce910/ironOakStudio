#!/usr/bin/env python3
"""Give every owned Orc map bounded, exact-identity quality variants.

The accepted character bank and original source art remain immutable. High
retains the exact current Orc texture bytes, including their established green
channel correction. Lower normal tiers store the mean tangent-space vector,
not a renormalized vector: the actor shader uses its length to retain variance
as roughness and prevent shimmering highlights.
"""
from __future__ import annotations

import ctypes
import hashlib
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess

import numpy as np
from PIL import Image

STAGE = Path(__file__).resolve().parent
ENGINE = STAGE.parents[1]
BASE = ENGINE / "build/arena-character-finish-2026-11-15"
BANK = STAGE / "character-textures"
ROSTER = BASE / "arena-project/arena_orcs_roster"
REPORT = STAGE / "ORC_TEXTURE_REVIEW.json"
LEVELS = ("512", "1024", "2048")


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def fingerprint():
    library = ctypes.CDLL(str(ENGINE / "build/arena-material-quality-2026-11-15/review/hash.so"))
    library.hash.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    library.hash.restype = ctypes.c_uint64
    return lambda raw: int(library.hash(raw, len(raw)))


def manifest(path):
    lines = path.read_text().splitlines()
    assert lines[0] == "VALWOOD_CHARACTER_TEXTURES 1"
    result = {}
    for line in lines[1:]:
        words = shlex.split(line)
        assert len(words) == 11
        identity = (int(words[0]), int(words[1]))
        assert identity not in result
        result[identity] = [(words[i], int(words[i + 1]), int(words[i + 2]))
                            for i in (2, 5, 8)]
    return result


def linear(values):
    return np.where(values <= .04045, values / 12.92,
                    ((values + .055) / 1.055) ** 2.4)


def encoded(values):
    values = np.clip(values, 0, 1)
    return np.where(values <= .0031308, values * 12.92,
                    1.055 * values ** (1 / 2.4) - .055)


def resize(values, dimensions, method=Image.Resampling.LANCZOS):
    return np.asarray(Image.fromarray(values.astype(np.float32), "F").resize(
        dimensions, method), np.float32)


def quant(values):
    return np.rint(np.clip(values, 0, 1) * 255).astype(np.uint8)


def image_variant(raw, role, dimensions):
    with Image.open(raw) as image:
        # All cooked Orc colors are sRGB, while ARM and normals are linear.
        # Preserve original alpha coverage, never reinterpret it as smoothness.
        if role == "albedo":
            a = np.asarray(image.convert("RGBA"), np.float32) / 255
            alpha = np.clip(resize(a[:, :, 3], dimensions), 0, 1)
            rgb = linear(a[:, :, :3])
            premultiplied = np.dstack([resize(rgb[:, :, i] * a[:, :, 3], dimensions)
                                      for i in range(3)])
            color = encoded(premultiplied / np.maximum(alpha[:, :, None], 1e-7))
            out = quant(np.dstack((color, alpha)))
            if image.mode == "RGB":
                return Image.fromarray(out[:, :, :3], "RGB"), None
            return Image.fromarray(out, "RGBA"), None
        a = np.asarray(image.convert("RGB"), np.float32) / 255
        if role == "normal":
            vector = a * 2 - 1
            # Area integration preserves the real mean vector. A unit-length
            # normalization here would erase the unresolved source variance.
            average = np.dstack([resize(vector[:, :, i], dimensions,
                                       Image.Resampling.BOX) for i in range(3)])
            length = np.linalg.norm(average, axis=2)
            average /= np.maximum(length[:, :, None], 1)
            out = quant(average * .5 + .5)
            decoded = out.astype(np.float32) / 127.5 - 1
            receipt = dict(filter="area BOX on linear tangent vectors",
                           vectors_renormalized=False,
                           source_green_correction_changed=False,
                           only_quantization_overshoot_bounded=True,
                           mean_vector_length_before_quantization=float(np.linalg.norm(average, axis=2).mean()),
                           mean_vector_length_after_quantization=float(np.linalg.norm(decoded, axis=2).mean()),
                           minimum_vector_length_after_quantization=float(np.linalg.norm(decoded, axis=2).min()),
                           maximum_component_quantization_error=float(np.abs(decoded - average).max()))
            assert receipt["maximum_component_quantization_error"] <= 1 / 255 + 1e-6
            return Image.fromarray(out, "RGB"), receipt
        assert role == "arm"
        out = quant(np.dstack([resize(a[:, :, i], dimensions) for i in range(3)]))
        return Image.fromarray(out, "RGB"), None


def glb_json(path):
    with path.open("rb") as stream:
        header = stream.read(20)
        assert header[:4] == b"glTF" and header[16:20] == b"JSON"
        data = stream.read(struct.unpack_from("<I", header, 12)[0])
        return json.loads(data), hashlib.sha256(data).hexdigest()


def mip_bytes(dimensions):
    width, height = dimensions
    total = 0
    while True:
        total += width * height * 4
        if width == height == 1:
            return total
        width, height = max(1, width // 2), max(1, height // 2)


def main():
    fnv = fingerprint()
    accepted = manifest(BASE / "character-textures/textures.manifest")
    assert len(accepted) == 602
    if not BANK.exists():
        # Reflinks copy independently on supported filesystems. No image
        # symlinks or hardlinks: the native asset policy rejects symlinks and
        # no mutable output may alias an accepted input inode.
        subprocess.run(["cp", "--reflink=auto", "-a",
                        str(BASE / "character-textures"), str(BANK)], check=True)
    assert BANK.is_dir() and not BANK.is_symlink()
    entries = dict(accepted)
    inputs = {}

    def pin(path):
        path = Path(path).resolve()
        inputs[str(path)] = sha(path)

    pin(BASE / "character-textures/textures.manifest")
    pin(BASE / "ORC_OCCLUSION_REVIEW.json")
    source_maps = {}
    records = []
    actual_glbs = []
    resolved_texture_identities = {}
    all_materials = 0
    for mapping in sorted(ROSTER.glob("*/ORC_TEXTURE_MAPPING.json")):
        pin(mapping)
        data = json.loads(mapping.read_text())
        character = mapping.parent.name
        materials = []
        for material in data["materials"]:
            all_materials += 1
            mapped = {}
            for role, conversion in material["conversions"].items():
                assert role in ("albedo", "normal", "arm")
                source = Path(conversion["path"])
                raw = source.read_bytes()
                assert hashlib.sha256(raw).hexdigest() == conversion["sha256"]
                identity = (fnv(raw), len(raw))
                previous = source_maps.get(identity)
                if previous:
                    assert previous["role"] == role
                else:
                    with Image.open(source) as image:
                        assert list(image.size) == conversion["size"]
                        dimensions = image.size
                    source_maps[identity] = dict(path=source, role=role,
                        dimensions=dimensions, original_sha256=conversion["sha256"],
                        already_registered=identity in accepted,
                        original_normal_green_policy=material["normal_green_policy"],
                        characters=set())
                    pin(source)
                source_maps[identity]["characters"].add(character)
                mapped[role] = dict(identity=list(identity), original_path=str(source))
            for source in material["source_images"].values():
                assert Path(source["path"]).is_file()
                assert sha(source["path"]) == source["sha256"]
                assert sha(source["meta_path"]) == source["meta_sha256"]
                pin(source["path"])
                pin(source["meta_path"])
            pin(material["source_mat"])
            assert sha(material["source_mat"]) == material["source_mat_sha256"]
            materials.append(dict(index=material["material_index"], name=material["unity_name"],
                                  roles=mapped, normal_green_policy=material["normal_green_policy"]))
        records.append(dict(character=character, materials=materials))
        frames = sorted((mapping.parent / "body").glob("combat_body_*.glb"))
        assert frames
        receipts = []
        for frame in frames:
            document, check = glb_json(frame)
            for material in materials:
                actual = document["materials"][material["index"]]
                pbr = actual["pbrMetallicRoughness"]
                for role, mapped in material["roles"].items():
                    view = actual["normalTexture"] if role == "normal" else pbr[
                        "baseColorTexture" if role == "albedo" else "metallicRoughnessTexture"]
                    image = document["images"][document["textures"][view["index"]]["source"]]
                    assert "uri" in image and "bufferView" not in image
                    # Gnur's initial import was subsequently copied into the
                    # complete roster. Its pathname differs; exact encoded
                    # bytes, which key the real native override, must agree.
                    actual_path = (frame.parent / image["uri"]).resolve()
                    if actual_path not in resolved_texture_identities:
                        raw = actual_path.read_bytes()
                        resolved_texture_identities[actual_path] = (fnv(raw), len(raw))
                        pin(actual_path)
                    assert resolved_texture_identities[actual_path] == tuple(mapped["identity"]), (frame, material["index"], role, actual_path)
            receipts.append(dict(path=str(frame), json_sha256=check))
        pin(frames[0])
        actual_glbs.append(dict(character=character, frames=len(frames),
                               all_material_texture_uris_match=True, documents=receipts))
    assert len(records) == 14 and all_materials == 41 and len(source_maps) == 87
    assert sum(row["already_registered"] for row in source_maps.values()) == 16
    textures = []
    profile_totals = {level: dict(orc_images=0, encoded_bytes=0, rgba8_with_mips_upper_bound_bytes=0)
                      for level in LEVELS}
    for identity, row in sorted(source_maps.items()):
        source = row["path"]
        width, height = row["dimensions"]
        variants = []
        variant_reports = []
        if not row["already_registered"]:
            for level in LEVELS:
                maximum = int(level)
                scale = min(1., maximum / max(width, height))
                dimensions = (round(width * scale), round(height * scale))
                relative = f"{level}/orc-{row['role']}-{identity[0]:016x}-{identity[1]}.png"
                output = BANK / relative
                assert not output.is_symlink()
                output.parent.mkdir(exist_ok=True)
                normal_receipt = None
                if dimensions == row["dimensions"]:
                    output.write_bytes(source.read_bytes())
                else:
                    image, normal_receipt = image_variant(source, row["role"], dimensions)
                    image.save(output, optimize=False)
                    # Independently recompute the exact expected decoded image.
                    expected, _ = image_variant(source, row["role"], dimensions)
                    assert np.array_equal(np.asarray(Image.open(output)), np.asarray(expected))
                raw = output.read_bytes()
                variants.append((relative, len(raw), fnv(raw)))
                with Image.open(output) as image:
                    assert image.size == dimensions
                assert max(dimensions) <= int(level)
                assert dimensions[0] <= width and dimensions[1] <= height
                variant_reports.append(dict(level=level, path=relative, dimensions=list(dimensions),
                    original_encoded_bytes_preserved=dimensions == row["dimensions"],
                    sha256=hashlib.sha256(raw).hexdigest(), normal_filter=normal_receipt))
            entries[identity] = variants
            assert variant_reports[-1]["original_encoded_bytes_preserved"]
            assert variant_reports[-1]["sha256"] == row["original_sha256"]
        else:
            assert row["role"] == "arm"
            variants = accepted[identity]
            variant_reports = [dict(level=level, path=relative,
                dimensions=list(Image.open(BANK / relative).size),
                inherited_repaired_ao_unchanged=True, sha256=sha(BANK / relative))
                for level, (relative, _, _) in zip(LEVELS, variants)]
        for level, (relative, size, check) in zip(LEVELS, variants):
            image = Image.open(BANK / relative)
            totals = profile_totals[level]
            totals["orc_images"] += 1
            totals["encoded_bytes"] += size
            totals["rgba8_with_mips_upper_bound_bytes"] += mip_bytes(image.size)
        textures.append(dict(identity=list(identity), role=row["role"],
            original_path=str(source), original_sha256=row["original_sha256"],
            original_dimensions=list(row["dimensions"]), characters=sorted(row["characters"]),
            previous_registered=row["already_registered"], variants=variant_reports))
    assert len(entries) == 673 and len(entries) - len(accepted) == 71
    assert {key: entries[key] for key in accepted} == accepted
    checked_files = checked_bytes = 0
    for identity, variants in entries.items():
        for relative, count, check in variants:
            output = BANK / relative
            assert not output.is_symlink() and output.is_file()
            raw = output.read_bytes()
            assert len(raw) == count and fnv(raw) == check
            with Image.open(output) as image:
                image.verify()
            if identity in accepted:
                original = BASE / "character-textures" / relative
                assert sha(output) == sha(original)
                assert (output.stat().st_dev, output.stat().st_ino) != (original.stat().st_dev, original.stat().st_ino)
            checked_files += 1
            checked_bytes += len(raw)
    for path, check in inputs.items():
        assert sha(path) == check, path
    lines = ["VALWOOD_CHARACTER_TEXTURES 1"]
    for identity, variants in sorted(entries.items()):
        lines.append(f"{identity[0]} {identity[1]} " + " ".join(
            f'"{path}" {count} {check}' for path, count, check in variants))
    (BANK / "textures.manifest").write_text("\n".join(lines) + "\n")
    assert manifest(BANK / "textures.manifest") == entries
    link = STAGE / "arena-project/character-textures"
    assert link.is_symlink()
    temporary = link.with_name(link.name + ".prepared")
    if temporary.is_symlink():
        temporary.unlink()
    temporary.symlink_to(BANK)
    os.replace(temporary, link)
    report = dict(passed=True, owned_orc_characters=14, material_records=41,
        unique_orc_texture_identities=87, roles={role: sum(row["role"] == role for row in textures)
            for role in ("albedo", "normal", "arm")},
        preserved_bank_identities=602, preserved_ao_repair_identities=16,
        added_texture_identities=71, total_bank_identities=len(entries),
        all_profiles_verified_files=checked_files, all_profiles_encoded_bytes=checked_bytes,
        all_accepted_variants_pixel_and_encoded_byte_identical=True,
        no_symlink_images_or_hardlinks_to_accepted_bank=True,
        high_original_color_normal_and_arm_bytes_preserved=True,
        original_source_art_and_glbs_modified=False, no_upscaling=True,
        no_normal_green_flip_changes=True,
        profiles={"Low": "<=512 all maps", "Medium": "<=1024 all maps",
                  "High": "Original <=2048 color/normal and <=1024 ARM; exact original bytes except inherited repaired AO"},
        resize_formulas={"color": "sRGB decode; linear-light Lanczos on alpha-premultiplied RGB; alpha Lanczos; divide by coverage; encode sRGB; preserve source alpha",
            "normal": "Area BOX mean of signed tangent-space vectors; no renormalization; bound only >1 quantization overshoot; preserve variance for shader roughness AA",
            "arm": "Linear data-channel Lanczos; clamp [0,1]; no gamma transforms or channel substitutions",
            "highest": "Exact original encoded PNG for every newly registered map; repaired AO exact accepted variant bytes"},
        profile_totals=profile_totals,
        profile_totals_scope="Whole-roster distinct textures, not simultaneous residency; native crowd caps retained",
        source_materials=records, textures=textures, glb_material_correspondence=actual_glbs,
        immutable_inputs_sha256=inputs, output_manifest_sha256=sha(BANK / "textures.manifest"),
        script_sha256=sha(__file__))
    REPORT.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("passed", "owned_orc_characters",
        "unique_orc_texture_identities", "preserved_bank_identities", "added_texture_identities",
        "total_bank_identities", "all_profiles_verified_files", "all_profiles_encoded_bytes", "profile_totals")}), flush=True)


if __name__ == "__main__":
    main()

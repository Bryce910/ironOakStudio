#!/usr/bin/env python3
"""Derive compact glTF materials from the purchased pack's Unity Standard data.

All source assets are read-only. Output is an explicit mapping by Unity .mat
path for the separately authored geometry conversion, not gameplay integration.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes.util import find_library
import hashlib
import json
import math
from pathlib import Path
import re
import sys

import numpy as np
from PIL import Image
import yaml


ENGINE = Path(__file__).resolve().parents[1]
PACK = ENGINE / "assets/purchased/modular_knights"


def unity_documents(path: Path):
    # Unity's class tags are document metadata, not object constructors. Removing
    # only those markers lets safe_load_all retain every real YAML document,
    # including Material records that follow an unrelated MonoBehaviour.
    text = path.read_text(encoding="utf-8-sig")
    text = re.sub(r"^%TAG[^\n]*\n", "", text, flags=re.M)
    text = re.sub(r"^--- !u!\d+ &[^\n]*", "---", text, flags=re.M)
    return list(yaml.safe_load_all(text))


def named_properties(rows):
    return {key: value for row in rows or [] for key, value in row.items()}


def finite_number(value, name):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError(f"Non-finite material property {name}")
    return value


def unit_number(value, name):
    result = finite_number(value, name)
    if not 0 <= result <= 1:
        raise ValueError(f"Material property {name} must be between 0 and 1, got {result}")
    return result


def tiff_pixels(path: Path):
    """Decode source samples with libtiff, preserving unspecified extra data.

    Source metallic/smoothness TIFFs contain both RGB+unspecified-fourth and
    grayscale+unspecified-second layouts. Generic image readers discard or
    reject them; Unity uses that final sample as authored smoothness.
    """
    library = find_library("tiff")
    if not library:
        raise ValueError("Source TIFF data decoding requires system libtiff")
    tiff = ctypes.CDLL(library)
    tiff.TIFFOpen.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    tiff.TIFFOpen.restype = ctypes.c_void_p
    tiff.TIFFClose.argtypes = [ctypes.c_void_p]
    tiff.TIFFGetField.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    tiff.TIFFGetField.restype = ctypes.c_int
    tiff.TIFFScanlineSize.argtypes = [ctypes.c_void_p]
    tiff.TIFFScanlineSize.restype = ctypes.c_ssize_t
    tiff.TIFFReadScanline.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint16]
    tiff.TIFFReadScanline.restype = ctypes.c_int
    handle = tiff.TIFFOpen(str(path).encode(), b"r")
    if not handle:
        raise ValueError(f"Cannot open source TIFF: {path}")
    try:
        def field(tag, kind=ctypes.c_uint16, default=None):
            value = kind()
            if not tiff.TIFFGetField(handle, tag, ctypes.byref(value)):
                if default is not None:
                    return default
                raise ValueError(f"Missing TIFF tag {tag}: {path}")
            return value.value
        width = field(256, ctypes.c_uint32)
        height = field(257, ctypes.c_uint32)
        bits = field(258, default=8)
        samples = field(277, default=1)
        planar = field(284, default=1)
        photo = field(262)
        orientation = field(274, default=1)
        if bits != 8 or samples not in (1, 2, 3, 4) or photo not in (0, 1, 2) or planar != 1 or orientation != 1:
            raise ValueError(f"Unsupported source TIFF sample layout (bits={bits}, samples={samples}, photo={photo}, planar={planar}, orientation={orientation}): {path}")
        row_bytes = tiff.TIFFScanlineSize(handle)
        if width < 1 or height < 1 or max(width, height) > 16384 or row_bytes < width * samples:
            raise ValueError(f"Unsafe source TIFF dimensions/scanline: {path}")
        raw = np.empty((height, row_bytes), dtype=np.uint8)
        for y in range(height):
            if tiff.TIFFReadScanline(handle, raw[y].ctypes.data_as(ctypes.c_void_p), y, 0) < 0:
                raise ValueError(f"Cannot decode source TIFF row {y}: {path}")
        data = raw[:, :width * samples].reshape(height, width, samples)
        rgba = np.full((height, width, 4), 255, dtype=np.uint8)
        if photo in (0, 1):
            gray = data[:, :, 0] if photo == 1 else 255 - data[:, :, 0]
            rgba[:, :, :3] = gray[:, :, None]
            if samples == 2:
                rgba[:, :, 3] = data[:, :, 1]
        elif samples >= 3:
            rgba[:, :, :3] = data[:, :, :3]
            if samples == 4:
                rgba[:, :, 3] = data[:, :, 3]
        else:
            raise ValueError(f"RGB TIFF lacks RGB samples: {path}")
        return Image.fromarray(rgba, "RGBA"), samples
    finally:
        tiff.TIFFClose(handle)


def load_pixels(path: Path):
    """Keep the fourth data channel in Photoshop TIFFs with ExtraSamples=0.

    Pillow/libtiff normally labels that layout RGBX, discarding the authored
    smoothness channel. Decode those exact four-sample TIFF tiles as RGBA.
    This changes decoding only and never modifies the purchased file.
    """
    if path.suffix.lower() in (".tif", ".tiff"):
        result, samples = tiff_pixels(path)
        has_alpha = samples in (2, 4)
        return result, {
            "dimensions": list(result.size), "decoded_mode": result.mode,
            "has_source_alpha": has_alpha, "recovered_tiff_fourth_channel": samples == 4,
            "recovered_tiff_extra_sample": has_alpha, "source_samples_per_pixel": samples,
            "channel_ranges": [list(bounds) for bounds in result.getextrema()],
            "decoder": "libtiff raw source samples; unspecified extra sample retained as smoothness/alpha",
        }
    image = Image.open(path)
    recovered = False
    image.load()
    has_alpha = "A" in image.getbands() or "transparency" in image.info
    result = image.convert("RGBA")
    metadata = {
        "dimensions": list(image.size), "decoded_mode": image.mode,
        "has_source_alpha": has_alpha, "recovered_tiff_fourth_channel": recovered,
        "channel_ranges": [list(bounds) for bounds in result.getextrema()],
    }
    image.close()
    return result, metadata


def fit(image: Image.Image, side: int):
    if max(image.size) <= side:
        return image.copy()
    scale = side / max(image.size)
    return image.resize((max(1, round(image.width * scale)), max(1, round(image.height * scale))), Image.Resampling.LANCZOS)


def png_array(values, path: Path):
    Image.fromarray(np.clip(np.rint(values * 255), 0, 255).astype(np.uint8), "RGB").save(path, optimize=True)


class Converter:
    def __init__(self, source: Path, inventory: Path, output: Path, profile="balanced"):
        self.source = source.resolve()
        data = json.loads(inventory.read_text())
        self.guids = {entry["guid"]: entry["path"] for entry in data["entries"] if "path" in entry}
        self.output = output.resolve()
        self.profile = profile
        if self.output == self.source or self.source in self.output.parents:
            raise ValueError("Output directory must be outside purchased source assets")
        self.output.mkdir(parents=True, exist_ok=True)

    def source_path(self, unity_path: str):
        path = (self.source / unity_path).resolve()
        if self.source not in path.parents:
            raise ValueError(f"Source path escapes the purchased source directory: {unity_path}")
        if not path.is_file():
            raise ValueError(f"Missing purchased source: {unity_path}")
        return path

    def material(self, unity_path: str):
        documents = unity_documents(self.source_path(unity_path))
        records = [d["Material"] for d in documents if isinstance(d, dict) and "Material" in d]
        if len(records) != 1:
            raise ValueError(f"Expected one Material document: {unity_path}")
        material = records[0]
        properties = material.get("m_SavedProperties", {})
        textures = named_properties(properties.get("m_TexEnvs"))
        floats = named_properties(properties.get("m_Floats"))
        floats.update(named_properties(properties.get("m_Ints")))
        colors = named_properties(properties.get("m_Colors"))
        warnings = []
        source_bindings = {}

        def texture(name, *, optional_missing=False):
            env = textures.get(name) or {}
            ref = env.get("m_Texture") or {}
            guid = ref.get("guid")
            row = {"guid": guid, "file_id": ref.get("fileID", 0),
                   "scale": env.get("m_Scale", {"x": 1, "y": 1}),
                   "offset": env.get("m_Offset", {"x": 0, "y": 0})}
            source_bindings[name] = row
            if not guid or not str(guid).strip("0"):
                return None
            if guid not in self.guids:
                if optional_missing:
                    warnings.append(f"{name}: source GUID {guid} is outside the purchased package; using AO=1")
                    row["missing_optional_reference"] = True
                    return None
                raise ValueError(f"{unity_path}: {name} source GUID {guid} is missing from package inventory")
            path = self.source_path(self.guids[guid])
            row["unity_path"] = self.guids[guid]
            row["absolute_path"] = str(path)
            if row["scale"] != {"x": 1, "y": 1} or row["offset"] != {"x": 0, "y": 0}:
                raise ValueError(f"{unity_path}: non-default {name} UV transform needs explicit geometry support")
            image, metadata = load_pixels(path)
            row.update(metadata)
            meta_path = path.with_suffix(path.suffix + ".meta")
            importer = {}
            if meta_path.is_file():
                for doc in unity_documents(meta_path):
                    if isinstance(doc, dict) and "TextureImporter" in doc:
                        importer = doc["TextureImporter"]
            row["flip_green_channel"] = bool(importer.get("flipGreenChannel", (importer.get("bumpmap") or {}).get("flipGreenChannel", 0)))
            if importer.get("bumpmap", {}).get("convertToNormalMap") and name == "_BumpMap":
                raise ValueError(f"{unity_path}: height-to-normal source import is unsupported")
            return image

        base_source = texture("_MainTex")
        metallic_source = texture("_MetallicGlossMap")
        normal_source = texture("_BumpMap")
        ao_source = texture("_OcclusionMap", optional_missing=True)
        keywords = set(material.get("m_ValidKeywords") or [])
        emission_reference = ((textures.get("_EmissionMap") or {}).get("m_Texture") or {})
        emission_color = colors.get("_EmissionColor") or {"r": 0, "g": 0, "b": 0, "a": 1}
        if "_EMISSION" in keywords and (emission_reference.get("fileID") or any(float(emission_color.get(c, 0)) for c in "rgb")):
            warnings.append("Source emission is not included in this base/normal/ORM derivative; source emission binding retained for follow-up")
            source_bindings["_EmissionMap"] = textures.get("_EmissionMap")
            source_bindings["_EmissionColor"] = emission_color
        tint = colors.get("_Color") or {"r": 1, "g": 1, "b": 1, "a": 1}
        factor = [unit_number(tint.get(c, 1), f"_Color.{c}") for c in "rgba"]
        mode = int(floats.get("_Mode", 0))
        alpha_enabled = bool(floats.get("_AlphaCutoffEnable", 0))
        alpha_mode = "MASK" if mode in (1, 2, 3) or alpha_enabled or "_ALPHATEST_ON" in keywords else "OPAQUE"
        cutoff = unit_number(floats.get("_Cutoff", floats.get("_AlphaCutoff", .5)), "_Cutoff") if alpha_mode == "MASK" else 0.0
        if mode in (2, 3):
            warnings.append(f"Unity Standard alpha mode {mode} uses blending; explicitly downgraded to MASK at authored cutoff {cutoff}, because current engine does not support BLEND")
        double_sided = bool(floats.get("_DoubleSidedEnable", 0)) or int(floats.get("_Cull", floats.get("_CullMode", 2))) == 0
        slug = re.sub(r"[^A-Za-z0-9_-]+", "_", Path(unity_path).stem).strip("_")
        stem = slug + "-" + hashlib.sha256(unity_path.encode()).hexdigest()[:12]
        is_head = "/Body/Head" in unity_path
        base_limit = 2048 if self.profile == "high" or is_head else 1024
        normal_limit = 1024 if self.profile == "high" or is_head else 512
        orm_limit = 1024 if self.profile == "high" else 512
        base = fit(base_source, base_limit) if base_source else Image.new("RGBA", (1, 1), (255, 255, 255, 255))
        base_path = self.output / (stem + ("-base.png" if alpha_mode == "MASK" else "-base.jpg"))
        if alpha_mode == "MASK":
            base.save(base_path, optimize=True)
        else:
            base.convert("RGB").save(base_path, quality=94, subsampling=0, optimize=True)
        normal_path = None
        normal_size = None
        if normal_source:
            normal = np.asarray(fit(normal_source, normal_limit).convert("RGB"), dtype=np.float32) / 255 * 2 - 1
            if source_bindings["_BumpMap"]["flip_green_channel"]:
                normal[:, :, 1] *= -1
            length = np.linalg.norm(normal, axis=2, keepdims=True)
            normal = np.divide(normal, np.maximum(length, 1e-8))
            normal[length[:, :, 0] < 1e-8] = [0, 0, 1]
            normal_path = self.output / (stem + "-normal.png")
            normal_size = list(normal.shape[1::-1])
            png_array(normal * .5 + .5, normal_path)
        smooth_channel = int(floats.get("_SmoothnessTextureChannel", 0))
        if smooth_channel not in (0, 1):
            raise ValueError(f"{unity_path}: unsupported _SmoothnessTextureChannel={smooth_channel}")
        smooth_scale = unit_number(floats.get("_GlossMapScale", 1), "_GlossMapScale")
        scalar_smoothness = unit_number(floats.get("_Glossiness", .5), "_Glossiness")
        scalar_metallic = unit_number(floats.get("_Metallic", 0), "_Metallic")
        ao_strength = unit_number(floats.get("_OcclusionStrength", 1), "_OcclusionStrength")
        sizing = metallic_source or ao_source or (base_source if smooth_channel == 1 else None)
        orm_size = fit(sizing, orm_limit).size if sizing else (1, 1)
        metallic = np.full(orm_size[::-1], scalar_metallic, np.float32)
        smoothness = np.full_like(metallic, scalar_smoothness)
        if metallic_source:
            # Resize data channels independently: RGBA image resizing normally
            # premultiplies RGB by alpha, which is wrong for metallic/smoothness.
            metallic = np.asarray(metallic_source.getchannel("R").resize(orm_size, Image.Resampling.LANCZOS), dtype=np.float32) / 255
            smoothness = np.asarray(metallic_source.getchannel("A").resize(orm_size, Image.Resampling.LANCZOS), dtype=np.float32) / 255 * smooth_scale
        if smooth_channel == 1:
            alpha = base_source.getchannel("A").resize(orm_size, Image.Resampling.LANCZOS) if base_source else Image.new("L", orm_size, 255)
            smoothness = np.asarray(alpha, dtype=np.float32) / 255 * smooth_scale
        ao = np.ones_like(metallic)
        if ao_source:
            ao = 1 - ao_strength + ao_strength * np.asarray(ao_source.getchannel("G").resize(orm_size, Image.Resampling.LANCZOS), dtype=np.float32) / 255
        orm_path = self.output / (stem + "-orm.png")
        png_array(np.stack((ao, 1 - smoothness, metallic), axis=2), orm_path)
        return {
            "name": material.get("m_Name", Path(unity_path).stem),
            "base": str(base_path), "normal": str(normal_path) if normal_path else None,
            "orm": str(orm_path), "base_factorRGBA": factor,
            "normal_scale": finite_number(floats.get("_BumpScale", 1), "_BumpScale"),
            "metallic_factor": 1, "roughness_factor": 1,
            "alpha_mode": alpha_mode, "alpha_cutoff": cutoff, "double_sided": double_sided,
            "source_bindings": source_bindings,
            "source_parameters": {key: floats.get(key) for key in ["_Mode", "_Cutoff", "_AlphaCutoffEnable", "_Metallic", "_Glossiness", "_GlossMapScale", "_SmoothnessTextureChannel", "_BumpScale", "_OcclusionStrength"]},
            "derived_dimensions": {"base": list(base.size), "normal": normal_size, "orm": list(orm_size)},
            "profile": self.profile,
            "conversion": "Unity Standard R=metal/A=smoothness -> glTF R=AO/G=roughness/B=metal; factors retained, no artistic roughness changes",
            "warnings": warnings,
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selection", type=Path, required=True, help="JSON containing pieces:[{mesh_name,material_paths:[UnityPath,...]}]")
    parser.add_argument("--output-dir", type=Path, default=PACK / "derived/materials")
    parser.add_argument("--profile", choices=("balanced", "high"), default="balanced", help="balanced preserves a 2K face with 1K armor base/512 data maps; high uses 2K/1K/1K everywhere")
    parser.add_argument("--source-root", type=Path, default=PACK / "source")
    parser.add_argument("--inventory", type=Path, default=PACK / "package_inventory.json")
    args = parser.parse_args()
    selection = json.loads(args.selection.read_text())
    material_paths = sorted({material for piece in selection["pieces"] for material in piece["material_paths"]})
    if not material_paths:
        raise ValueError("Selection contains no material paths")
    converter = Converter(args.source_root, args.inventory, args.output_dir, args.profile)
    rows = {}
    for path in material_paths:
        rows[path] = converter.material(path)
        print(f"Converted {path}: {rows[path]['alpha_mode']}", flush=True)
        for warning in rows[path]["warnings"]:
            print(f"  warning: {warning}", flush=True)
    destination = converter.output / "materials.json"
    temporary = destination.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(rows, indent=2) + "\n")
    temporary.replace(destination)
    print(f"Wrote {len(rows)} source material bindings to {destination}")
    images = {Path(row[field]) for row in rows.values() for field in ("base", "normal", "orm") if row[field]}
    file_bytes = sum(path.stat().st_size for path in images)
    decoded_bytes = 0
    for path in images:
        with Image.open(path) as image:
            width, height = image.size
        while True:
            decoded_bytes += width * height * 4
            if width == height == 1:
                break
            width, height = max(1, width // 2), max(1, height // 2)
    print(f"profile={args.profile} unique_image_files={len(images)} total_file_bytes={file_bytes} decoded_RGBA8_full_mips_bytes={decoded_bytes}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, yaml.YAMLError) as error:
        print(f"prepare_modular_knight_materials: {error}", file=sys.stderr)
        sys.exit(1)

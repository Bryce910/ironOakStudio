#!/usr/bin/env python3
"""Decode original Finward EXRs with Blender's CPU image loader, without rendering.

blender -b -t 2 --python tools/prepare_finward_scene_lightmaps.py -- \
  --scene-export assets/purchased/finward_house_props/scene_export/scene.json \
  --source-root docs/opening-finward-reference/2026-11-03/source --output NEW_DIR

Original EXRs are linear HDR data, not RGBM GPU transport images. No albedo,
exposure, gamma curve or PI multiplier is baked into the cached irradiance.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import sys

import bpy
import numpy as np


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scene-export', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--mode-probe', type=Path, required=True)
    parser.add_argument('--unity-entity-lighting', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    source = args.source_root.resolve(strict=True)
    document = json.loads(args.scene_export.read_text())
    probe = json.loads(args.mode_probe.read_text())
    if probe.get('mode_numeric') != 1:
        raise ValueError('This cook requires the proven CombinedDirectional source mode')
    shader_text = args.unity_entity_lighting.read_text()
    if 'return encodedIlluminance.rgb;' not in shader_text or 'direction.xyz - 0.5' not in shader_text:
        raise ValueError('Local official Unity lighting decode contract changed')
    if args.output.exists() or args.output.is_symlink():
        raise ValueError('Choose a NEW lightmap cache directory')
    args.output.mkdir(parents=True)
    records = []
    for fallback_index, lightmap in enumerate(document.get('lightmaps', [])):
        value = lightmap.get('color', '')
        if not value:
            raise ValueError('Lightmap atlas is missing its original color image')
        relative = PurePosixPath(value)
        if relative.is_absolute() or '..' in relative.parts or relative.parts[:2] != ('Assets', 'HouseProps'):
            raise ValueError('Unsafe purchased lightmap path')
        path = (source / str(relative)).resolve(strict=True)
        if not path.is_relative_to(source) or path.suffix.lower() != '.exr':
            raise ValueError('Expected an original purchased floating-point EXR')
        index = int(lightmap.get('index', fallback_index))
        image = bpy.data.images.load(str(path), check_existing=False)
        try:
            original_space = image.colorspace_settings.name
            # Pixel data is irradiance, never display-referred color. This
            # also excludes Blender view/exposure/display transformations.
            image.colorspace_settings.name = 'Non-Color'
            width, height = image.size
            channels = image.channels
            if not image.is_float or channels < 3 or width * height > 8192 ** 2:
                raise ValueError('Unreviewed source lightmap format')
            pixels = np.empty(width * height * channels, dtype=np.float32)
            image.pixels.foreach_get(pixels)
            rgb = pixels.reshape(height, width, channels)[:, :, :3].copy()
            if not np.isfinite(rgb).all():
                raise ValueError('Nonfinite source lightmap sample')
            raw_min, raw_max = float(rgb.min()), float(rgb.max())
            negative = int((rgb < 0).sum())
            above = int((rgb > 65504).sum())
            np.clip(rgb, 0, 65504, out=rgb)
            target = args.output / ('lightmap-%03d.npy' % index)
            np.save(target, rgb, allow_pickle=False)
            direction = lightmap.get('direction') or None
            direction_path = (source / direction).resolve(strict=True) if direction else None
            if direction_path and not direction_path.is_relative_to(source):
                raise ValueError('Directional lightmap escaped its source root')
            records.append(dict(index=index, source=str(path), sha256=sha(path),
                                cache=str(target.resolve()), cache_sha256=sha(target),
                                width=width, height=height, channels=channels,
                                original_colorspace=original_space, decoder_colorspace='Non-Color',
                                cache_rows='row0=UnityUVv0 (bottom)', dtype='float32',
                                raw_rgb_range=[raw_min, raw_max],
                                rgb_mean=rgb.mean((0, 1), dtype=np.float64).tolist(),
                                negative_components_clipped=negative,
                                above65504_components_clipped=above,
                                direction_source=str(direction_path) if direction_path else None,
                                direction_sha256=sha(direction_path) if direction_path else None,
                                source_export_record=lightmap))
            print('FINWARD EXR', index, width, height, 'range', raw_min, raw_max, flush=True)
        finally:
            bpy.data.images.remove(image)
    if not records or len({r['index'] for r in records}) != len(records):
        raise ValueError('Expected unique source lightmap atlas indices')
    manifest = dict(version=1, source_scene=str(args.scene_export.resolve()),
                    source_scene_sha256=sha(args.scene_export),
                    lightmaps_mode=document.get('lightmaps_mode'),
                    lightmaps_mode_numeric=probe['mode_numeric'],
                    lightmaps_mode_name='CombinedDirectional',
                    lightmaps_mode_probe=dict(path=str(args.mode_probe.resolve()), sha256=sha(args.mode_probe)),
                    active_color_space=document.get('active_color_space'),
                    active_color_space_scope=document.get('active_color_space_scope', 'private conversion project only; publisher settings unavailable'),
                    official_unity_shader=dict(path=str(args.unity_entity_lighting.resolve()), sha256=sha(args.unity_entity_lighting)),
                    decode='Unity FULL_HDR DecodeLightmap identity RGB from original linear floating-point EXR; no RGBM expansion',
                    directional_decode='color*(dot(unit source Unity world normal, direction.xyz-.5)+.5)/max(direction.w,.0001); source PNG is non-color UNORM',
                    decode_multiplier=1, gamma_decode=False, pi_multiplier=False,
                    no_rendering=True, source_files_modified=False,
                    cache_rows='Blender image pixels: bottom row is Unity UVv0',
                    attribute_contract='_VALWOOD_BAKED_LIGHT FLOAT VEC4; RGB linear diffuse irradiance/PI, A validity',
                    atlas=records)
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()

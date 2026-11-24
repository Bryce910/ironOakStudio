#!/usr/bin/env python3
"""Small, dry domestic materials for the opening house.

This helper only writes its output directory. The existing Poly Haven wood scan
is read without alteration; paint, ceramic tile, and their subtle wear are
deterministic procedural textures. Native packing is AO / roughness / metallic.
No geometry, source material, source image, or project is changed here.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SIZE = 1024


def _noise(size: int, seed: int, blur: float) -> np.ndarray:
    """Periodic Gaussian-filtered noise, without a texture seam."""
    random = np.random.default_rng(seed).normal(size=(size, size))
    frequencies = np.fft.fftfreq(size)
    radius = frequencies[:, None] ** 2 + frequencies[None, :] ** 2
    result = np.fft.ifft2(np.fft.fft2(random) * np.exp(-radius * blur * blur)).real
    return result / max(float(result.std()), 1e-8)


def _rgb(values: np.ndarray) -> np.ndarray:
    return np.clip(np.rint(values), 0, 255).astype(np.uint8)


def _normal(height: np.ndarray, strength: float) -> np.ndarray:
    # OpenGL tangent-space normal: positive green is the texture's upward axis.
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    result = np.stack((-dx, dy, np.ones_like(height)), axis=-1)
    result /= np.linalg.norm(result, axis=2, keepdims=True)
    return _rgb(result * 127.5 + 127.5)


def _write_material(output: Path, name: str, base: np.ndarray,
                    normal: np.ndarray, roughness: np.ndarray,
                    tile: float, description: str, source: str) -> dict:
    arm = np.zeros((*roughness.shape, 3), dtype=np.uint8)
    arm[:, :, 0] = 255  # No counterfeit baked shadows in broad room surfaces.
    arm[:, :, 1] = _rgb(roughness)
    result = {}
    for suffix, values in [('base', base), ('normal', normal), ('arm', arm)]:
        path = output / f'{name}_{suffix}.png'
        image = Image.fromarray(values)
        # Fine paint does not warrant a 1K allocation. Reserve 1K for the floor
        # and tile joints, keeping the complete set under 55 MiB with mipmaps.
        if name not in {'floor', 'tile'}:
            image = image.resize((512, 512), Image.Resampling.LANCZOS)
        image.save(path, optimize=True)
        result[suffix] = path
    result.update(rgb=[1., 1., 1.], roughness=1., tile=tile,
                  description=description, source=source)
    return result


def build_materials(output_dir: Path | str) -> dict[str, dict]:
    """Return {name: {base, normal, arm, rgb, roughness, tile, ...}}.

    Paths are Path objects for direct use with build_opening_prologue.Glb.material.
    Use rgb=[1,1,1] so the intended paint is not darkened a second time. Existing
    Glb.material normal strength .55 is already accounted for in these maps.
    ``tile`` is the physical repeat length in meters for Glb.box.
    """
    output = Path(output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    materials = {}
    micro = _noise(SIZE, 803, .75)
    medium = _noise(SIZE, 804, 16.)
    broad = _noise(SIZE, 805, 90.)
    painted = medium * .65 + micro * .4 + broad * .55

    # Modest rental-house paint: ordinary wear, never repeating ruin/mold stains.
    paint_specs = [
        ('wall', [205., 201., 190.], 2.4, 232.,
         'Warm off-white painted plaster; fine roller texture and gentle age.'),
        ('lower_wall', [190., 187., 174.], 2.4, 235.,
         'Slightly darker warm paint for localized lower-wall wear.'),
        ('ceiling', [215., 214., 207.], 3.2, 244.,
         'Neutral matte ceiling, with almost invisible surface variation.'),
        ('trim', [216., 212., 200.], 1.0, 215.,
         'Old satin white painted skirting and window/door casing.'),
    ]
    for name, color, tile, rough, description in paint_specs:
        intensity = .42 if name == 'ceiling' else (.55 if name == 'trim' else 1.)
        base = _rgb(np.array(color)[None, None, :] + painted[:, :, None] * intensity)
        normal = _normal(micro * .055 + medium * .035, .18)
        if name == 'trim':
            # Negligible grain belongs to painted trim, not to the ceiling.
            base = _rgb(base.astype(float) + _noise(SIZE, 810, 3.)[:, :, None] * .25)
        materials[name] = _write_material(
            output, name, base, normal, np.full((SIZE, SIZE), rough) + medium * .8,
            tile, description, 'Procedural, authored for this project; no external source')

    # Keep scanned grain and joints, remove the old saturated orange varnish.
    wood_source = ROOT / 'ground/wood_floors/wood_cabinet_worn_long_diff_1k.jpg'
    wood_normal = ROOT / 'ground/wood_floors/wood_cabinet_worn_long_nor_gl_1k.jpg'
    original = np.array(Image.open(wood_source).convert('RGB').resize((SIZE, SIZE)), dtype=float)
    luma = original @ np.array([.2126, .7152, .0722])
    luma = (luma - float(luma.mean())) * .44
    # A dry, modest oak floor rather than high-gloss red cabinet lacquer.
    base = _rgb(np.array([142., 128., 108.])[None, None, :] + luma[:, :, None])
    raw_normal = np.array(Image.open(wood_normal).convert('RGB').resize((SIZE, SIZE)), dtype=float)
    vector = raw_normal / 127.5 - 1.
    vector[:, :, :2] *= .22
    vector[:, :, 2] = np.maximum(vector[:, :, 2], .5)
    vector /= np.linalg.norm(vector, axis=2, keepdims=True)
    normal = _rgb(vector * 127.5 + 127.5)
    materials['floor'] = _write_material(
        output, 'floor', base, normal, np.full((SIZE, SIZE), 233.) + medium * 2.,
        2.4, 'Muted dry oak grain; restrained small joints and no lacquer sheen.',
        'Poly Haven CC0 wood_cabinet_worn_long, existing local scan; recolored and normal strength reduced')

    # Cheap 30 cm domestic ceramic/linoleum squares, not castle paving.
    yy, xx = np.indices((SIZE, SIZE))
    tile_index_x = xx // 256
    tile_index_y = yy // 256
    edge_x = np.minimum(xx % 256, 255 - xx % 256)
    edge_y = np.minimum(yy % 256, 255 - yy % 256)
    edge_distance = np.minimum(edge_x, edge_y)
    grout = np.clip((3. - edge_distance) / 3., 0., 1.)
    offsets = np.array([[0., 2., -1., 1.], [-2., 0., 1., -1.],
                        [1., -1., 2., 0.], [0., 1., -2., 1.]])
    variation = medium * 1.2 + broad * 1.1 + micro * .25
    tile_color = np.array([177., 177., 166.])[None, None, :] + (
        variation + offsets[tile_index_y, tile_index_x])[:, :, None]
    base = _rgb(tile_color * (1. - grout[:, :, None]) +
                np.array([134., 134., 124.])[None, None, :] * grout[:, :, None])
    normal = _normal(-grout * .22 + micro * .008, .34)
    materials['tile'] = _write_material(
        output, 'tile', base, normal, 223. + medium + grout * 15.,
        1.2, 'Simple 30 cm warm gray kitchen tiles, narrow grout and matte glaze.',
        'Procedural, authored for this project; no external source')

    # A few localized scuffs can be placed once at floor level. Do not apply it
    # across every wall; the wall itself is intentionally quiet and believable.
    materials['wear'] = _write_material(
        output, 'wear', _rgb(np.array([181., 177., 164.])[None, None, :] +
                             broad[:, :, None] * 1.3 + painted[:, :, None] * 1.2),
        _normal(micro * .07, .2), np.full((SIZE, SIZE), 242.),
        2.4, 'Subtle localized wear paint; optional patches near floor/utility corners.',
        'Procedural, authored for this project; no external source')

    manifest = {
        'version': 1,
        'packing': 'RGB = ambient occlusion, roughness, metallic',
        'metallic': 0,
        'source_inputs': {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [wood_source, wood_normal]
        },
        'source_url': 'https://polyhaven.com/a/wood_cabinet_worn_long',
        'source_license': 'CC0; scan was already present locally; no download performed',
        'maps': {},
        'materials': {},
    }
    for name, material in materials.items():
        manifest['materials'][name] = {
            k: (str(v.name) if isinstance(v, Path) else v)
            for k, v in material.items()
        }
        for suffix in ['base', 'normal', 'arm']:
            path = material[suffix]
            with Image.open(path) as image:
                manifest['maps'][path.name] = {
                    'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                    'width': image.width, 'height': image.height,
                    'decoded_bytes': image.width * image.height * 4,
                    'file_bytes': path.stat().st_size,
                }
    manifest['decoded_bytes_total'] = sum(x['decoded_bytes'] for x in manifest['maps'].values())
    manifest['compressed_bytes_total'] = sum(x['file_bytes'] for x in manifest['maps'].values())
    (output / 'materials.json').write_text(json.dumps(manifest, indent=2) + '\n')
    swatches = Image.new('RGB', (1024, 584), '#292b2b')
    labels = ImageDraw.Draw(swatches)
    for i, name in enumerate(materials):
        x, y = (i % 4) * 256, (i // 4) * 292
        with Image.open(materials[name]['base']) as image:
            image = image.resize((256, 256), Image.Resampling.LANCZOS)
            swatches.paste(image, (x, y + 28))
        labels.text((x + 8, y + 8), name.replace('_', ' '), fill='#ffffff')
    swatches.save(output / 'swatches.jpg', quality=94)
    return materials


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=ROOT / 'build/opening-apartment-refresh/materials')
    args = parser.parse_args()
    materials = build_materials(args.output)
    manifest = json.loads((args.output / 'materials.json').read_text())
    print(json.dumps({'materials': list(materials), 'output': str(args.output),
                      'decoded_bytes_total': manifest['decoded_bytes_total'],
                      'compressed_bytes_total': manifest['compressed_bytes_total']}, indent=2))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Bounded offline cook of the user's purchased BEP particle sprite artwork.

    python3 tools/cook_bep_particles.py
    python3 tools/cook_bep_particles.py --validate
    python3 tools/cook_bep_particles.py --self-test

No source scripts, Unity assets, or Blender scenes are executed. Only four
explicit ZIP members are read; source archive bytes remain unchanged. VFX1 is
four little-endian uint32 values (magic,width,height,layers), followed by tightly
packed RGBA8 layers. Layer rows run bottom-to-top for OpenGL v=0 at the base.
RGB stores sRGB-encoded *linear premultiplied* radiance; alpha is linear coverage.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import struct
import zipfile

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = Path('/home/computer/Downloads/Buildings&PropsTextures.zip')
OUTPUT = ROOT / 'projects/valwood_world/particles'
SOURCE = ROOT / 'assets/purchased/bep_particles/source'
MAGIC = 0x31584656
RECIPE = 1
MEMBERS = {
    'Torchfire_Sheet.png': (1350, 2048),
    'Campfire_Sheet.png': (2700, 4096),
    'Fire_Smoke.png': (128, 128),
    'Spark_Sharp.png': (64, 64),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def srgb_decode(value):
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)


def srgb_encode(value):
    return np.where(value <= .0031308, value * 12.92, 1.055 * value ** (1 / 2.4) - .055)


def premultiplied_resize(image, size):
    rgba = np.asarray(image.convert('RGBA'), dtype=np.float32) / 255
    rgba[:, :, :3] = srgb_decode(rgba[:, :, :3]) * rgba[:, :, 3:4]
    channels = [np.asarray(Image.fromarray(rgba[:, :, c]).resize(size, Image.Resampling.LANCZOS),
                           dtype=np.float32) for c in range(4)]
    linear = np.clip(np.stack(channels, axis=2), 0, 1)
    # Lanczos ringing must not produce radiance in zero-coverage padding.
    linear[:, :, :3] = np.minimum(linear[:, :, :3], linear[:, :, 3:4])
    linear[:, :, :3] = srgb_encode(linear[:, :, :3])
    encoded = np.uint8(np.clip(np.rint(linear * 255), 0, 255))
    encoded[:, :, :3][encoded[:, :, 3] == 0] = 0
    # Source sheets have top-left image origin; GL flame geometry uses y=0 at
    # its fuel mouth. Frame order itself remains top-left row-major.
    return encoded[::-1].copy()


def encode_vfx(layers):
    assert layers and len(layers) <= 256
    height, width, channels = layers[0].shape
    assert channels == 4 and 1 <= width <= 512 and 1 <= height <= 512
    assert all(a.shape == (height, width, 4) and a.dtype == np.uint8 for a in layers)
    return struct.pack('<4I', MAGIC, width, height, len(layers)) + b''.join(a.tobytes() for a in layers)


def decode_vfx(raw):
    assert len(raw) >= 16
    magic, width, height, layers = struct.unpack_from('<4I', raw)
    assert magic == MAGIC and 1 <= width <= 512 and 1 <= height <= 512 and 1 <= layers <= 256
    assert len(raw) == 16 + width * height * layers * 4
    return np.frombuffer(raw, np.uint8, offset=16).reshape(layers, height, width, 4)


def read_sources():
    assert ARCHIVE.is_file(), str(ARCHIVE)
    images, records, original_bytes = {}, [], {}
    with zipfile.ZipFile(ARCHIVE) as archive:
        total = 0
        for name, dimensions in MEMBERS.items():
            member = 'Buildings&PropsTextures/' + name
            entry = archive.getinfo(member)
            assert not entry.is_dir() and entry.file_size <= 8 * 1024 * 1024
            assert (entry.external_attr >> 16) & 0o170000 != 0o120000
            total += entry.file_size
            assert total <= 8 * 1024 * 1024
            raw = archive.read(entry)  # ZIP CRC is checked by zipfile.
            image = Image.open(io.BytesIO(raw))
            assert image.size == dimensions and image.mode == 'RGBA'
            image.load()
            images[name], original_bytes[name] = image, raw
            records.append(dict(member=member, bytes=len(raw), crc32=f'{entry.CRC:08x}',
                                sha256=sha(raw), dimensions=list(image.size), mode=image.mode))
    assert np.asarray(images['Campfire_Sheet.png'])[:, :, 3].min() == 255
    return images, records, original_bytes


def build():
    images, records, originals = read_sources()
    fire = []
    for name in ['Torchfire_Sheet.png', 'Campfire_Sheet.png']:
        image = images[name]
        tile_w, tile_h = image.width // 9, image.height // 8
        assert image.width == tile_w * 9 and image.height == tile_h * 8
        for row in range(8):
            for column in range(9):
                tile = image.crop((column * tile_w, row * tile_h,
                                   (column + 1) * tile_w, (row + 1) * tile_h))
                fire.append(premultiplied_resize(tile, (128, 256)))
    files = {
        'purchased_fire.vfx': encode_vfx(fire),
        'purchased_smoke.vfx': encode_vfx([premultiplied_resize(images['Fire_Smoke.png'], (128, 128))]),
    }
    for name, raw in files.items():
        decoded = decode_vfx(raw)
        assert decoded.shape == ((144, 256, 128, 4) if 'fire' in name else (1, 128, 128, 4))
        assert np.all(decoded[:, :, :, :3][decoded[:, :, :, 3] == 0] == 0)
    provenance = dict(
        schema_version=1, recipe=RECIPE,
        source='User-provided purchased BIG Environment Pack sprite artwork; private project use.',
        source_archive=str(ARCHIVE), archive_bytes=ARCHIVE.stat().st_size,
        archive_sha256=None,
        archive_sha256_note='Not computed: bounded member inspection avoids reading the full1.6GB archive. Individual source members are CRC-checked and SHA256-recorded.',
        source_members=records,
        license='User identifies this content as purchased. No separate license/EULA/README exists in the supplied archive. No open-license or redistribution rights are asserted.',
        source_preservation='Original ZIP unchanged; selected original PNG bytes retained under assets/purchased/bep_particles/source.',
        layout=dict(fire_layers=144, torch_layers=[0, 71], campfire_layers=[72, 143],
                    source_grid=[9, 8], source_frame_order='Top-left row-major, inferred from the visibly regular9x8 sheet; no authored timing metadata supplied.',
                    layer_pixel_rows='Bottom-to-top: first RGBA row is GL v=0 at flame base.',
                    authored_fps=None, playback_rate='Runtime adaptation23.625fps:72frames x21loops per64second phase. No FPS value is asserted from source.'),
        color=dict(rgb='sRGB decode, multiply by original alpha in linear light, per-channel float Lanczos resize, clamp RGB<=coverage, sRGB encode and round to RGBA8.',
                   alpha='Linear original coverage, Lanczos resized and clamped; not radiance.',
                   campfire='Source alpha is uniformly255 over black RGB. Runtime must use additive/premultiplied radiance, never source-alpha black cards.',
                   mipmaps='Runtime creates independent texture-array layer mips; no cross-frame atlas filtering.'),
        runtime_format=dict(magic='VFX1', magic_uint=MAGIC, header='4 little-endian uint32: magic,width,height,layers',
                            data='Tightly packed RGBA8 contiguous layers; no compression or trailing bytes.'),
        outputs=[dict(file=name, bytes=len(raw), sha256=sha(raw), dimensions=list(decode_vfx(raw).shape)[1:3][::-1],
                      layers=decode_vfx(raw).shape[0]) for name, raw in files.items()],
        spark='Original Spark_Sharp.png preserved and inspected; this recipe does not add a runtime spark texture. Existing bounded analytic ember geometry remains available.',
        changes=['144 fire layers resampled to128x256; smoke to128x128.',
                 'No full archive extraction, model import, source script execution, engine settings mutation, or new render target.'])
    files['purchased_particles_provenance.json'] = (json.dumps(provenance, indent=2) + '\n').encode()
    return files, originals


def self_test():
    # Independent energy witness: fully white source at128/255 alpha must decode
    # to that alpha in linear RGB, rather than the much darker sRGB-alpha mix.
    one = Image.new('RGBA', (1, 1), (255, 255, 255, 128))
    a = premultiplied_resize(one, (1, 1))[0, 0]
    assert a[3] == 128 and abs(float(srgb_decode(a[0] / 255)) - 128 / 255) < .004
    hidden = premultiplied_resize(Image.new('RGBA', (4, 4), (255, 255, 255, 0)), (9, 7))
    assert not np.any(hidden)
    # Source top is red and bottom is blue: GL v=0 must receive blue.
    image = Image.new('RGBA', (2, 2), (255, 0, 0, 255))
    image.putpixel((0, 1), (0, 0, 255, 255)); image.putpixel((1, 1), (0, 0, 255, 255))
    a = premultiplied_resize(image, (2, 2))
    assert tuple(a[0, 0]) == (0, 0, 255, 255) and tuple(a[1, 0]) == (255, 0, 0, 255)
    raw = encode_vfx([a, a])
    assert len(raw) == 16 + 2 * 2 * 2 * 4 and np.array_equal(decode_vfx(raw)[0], a)
    for invalid in [raw[:-1], raw + b'\0', b'BAD!' + raw[4:]]:
        try:
            decode_vfx(invalid)
        except AssertionError:
            pass
        else:
            raise AssertionError('Invalid VFX accepted')
    print('PASS VFX cook: linear-premultiplied energy, transparent-white padding neutral, GL row origin, exact header/layer bytes, invalid/trailing payload rejection')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validate', action='store_true', help='Rebuild bounded inputs in memory and compare every generated/source byte.')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    self_test()
    if args.self_test:
        return
    files, originals = build()
    if args.validate:
        for name, raw in files.items():
            assert (OUTPUT / name).read_bytes() == raw, 'Cook differs: ' + name
        for name, raw in originals.items():
            assert (SOURCE / name).read_bytes() == raw, 'Preserved source differs: ' + name
        print('PASS purchased particle cook: byte-exact deterministic144-layer fire, single smoke layer, four preserved source PNGs/provenance')
        return
    # Complete validation occurs before any output mutation.
    for name, raw in originals.items():
        target = SOURCE / name
        if target.exists():
            assert target.read_bytes() == raw, 'Existing source changed: ' + name
    OUTPUT.mkdir(parents=True, exist_ok=True)
    SOURCE.mkdir(parents=True, exist_ok=True)
    for name, raw in originals.items():
        target = SOURCE / name
        if not target.exists():
            target.write_bytes(raw)
    for name, raw in files.items():
        (OUTPUT / name).write_bytes(raw)
        print('WROTE', OUTPUT / name, len(raw), sha(raw))


if __name__ == '__main__':
    main()

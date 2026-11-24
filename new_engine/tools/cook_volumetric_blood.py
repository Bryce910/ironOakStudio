#!/usr/bin/env python3
"""Offline conversion of purchased FBX+EXR vertex animation; no Unity import.

Reads only the data-only inspector bank. FFmpeg is used as a CPU EXR decoder;
package scripts are never imported, evaluated, compiled or executed.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import subprocess

import numpy as np
from PIL import Image, ImageDraw

from inspect_volumetric_blood import sha256_file, strict_source_path


MAGIC = b'VWBLOOD1'
SOURCE_PREFIX = 'Assets/KriptoFX/VolumetricBloodFX/'
VARIANTS = ('Blood1', 'Blood3', 'Blood4')
PRESENTATION_SCALE = .20


def fbx_array(text, name, integer=False):
    matches = list(re.finditer(r'\b'+re.escape(name)+r': \*(\d+)\s*\{\s*a:\s*(.*?)\s*\}', text, re.S))
    if len(matches) != 1:
        raise ValueError(f'Expected one ASCII FBX {name} array')
    declared = int(matches[0][1])
    if declared <= 0 or declared > 200000:
        raise ValueError('FBX array exceeds bounded effect topology')
    raw = matches[0][2].replace('\n', '').replace('\r', '')
    values = np.fromstring(raw, sep=',', dtype=np.float64)
    if len(values) != declared or not np.isfinite(values).all():
        raise ValueError('Invalid/truncated FBX array')
    if integer:
        if not np.equal(values, np.floor(values)).all():
            raise ValueError('Nonintegral FBX indices')
        return values.astype(np.int32)
    return values


def mesh_addressing(path):
    if path.stat().st_size > 8*1024**2:
        raise ValueError('Oversized source FBX')
    text = path.read_text(encoding='utf-8-sig')
    if not text.startswith('; FBX') or 'MappingInformationType: "ByPolygonVertex"' not in text:
        raise ValueError('Unsupported FBX mapping')
    vertices = fbx_array(text, 'Vertices').reshape(-1, 3)
    corners = fbx_array(text, 'PolygonVertexIndex', True)
    if len(corners)%3 or not np.all(corners.reshape(-1, 3)[:, :2] >= 0) or not np.all(corners[2::3] < 0):
        raise ValueError('Only original triangle topology is supported')
    ids = np.where(corners < 0, -corners-1, corners)
    if ids.min() < 0 or ids.max() >= len(vertices):
        raise ValueError('FBX vertex index out of bounds')
    uv = fbx_array(text, 'UV').reshape(-1, 2)
    uv_ids = fbx_array(text, 'UVIndex', True)
    if len(uv_ids) != len(corners) or uv_ids.min() < 0 or uv_ids.max() >= len(uv):
        raise ValueError('FBX UV indices out of bounds')
    # Preserve polygon-corner order, including UV seams, as independent vertices.
    return uv[uv_ids], np.arange(len(corners), dtype='<u4')


def exr_shape(path):
    with path.open('rb') as stream:
        magic, version = struct.unpack('<II', stream.read(8))
        if magic != 20000630 or version != 2:
            raise ValueError('Only ordinary scanline EXR2 data is accepted')
        def zstring():
            value = bytearray()
            while True:
                c = stream.read(1)
                if c == b'\0':
                    return value.decode('ascii')
                if not c or len(value) >= 255:
                    raise ValueError('Invalid EXR header string')
                value.extend(c)
        attributes = {}
        while True:
            name = zstring()
            if not name:
                break
            kind = zstring()
            size = struct.unpack('<I', stream.read(4))[0]
            if size > 1024**2 or stream.tell()+size > 2*1024**2:
                raise ValueError('Oversized EXR header')
            value = stream.read(size)
            if len(value) != size or name in attributes:
                raise ValueError('Truncated/duplicate EXR header')
            attributes[name] = (kind, value)
        if attributes.get('dataWindow', ('', b''))[0] != 'box2i':
            raise ValueError('Missing EXR data window')
        x0, y0, x1, y1 = struct.unpack('<4i', attributes['dataWindow'][1])
        width, height = x1-x0+1, y1-y0+1
        if (x0, y0) != (0, 0) or not 1 <= width <= 4096 or not 1 <= height <= 4096 or width*height > 4*1024**2:
            raise ValueError('EXR source dimensions exceed limits')
        return width, height


def read_exr(path):
    width, height = exr_shape(path)
    command = ['ffmpeg', '-v', 'error', '-nostdin', '-threads', '1', '-i', str(path),
               '-frames:v', '1', '-threads', '1', '-f', 'rawvideo', '-pix_fmt', 'gbrpf32le', 'pipe:1']
    result = subprocess.run(command, capture_output=True, timeout=60, check=False)
    if result.returncode or len(result.stdout) != width*height*12:
        raise ValueError('CPU EXR decoder failed: '+result.stderr.decode(errors='replace')[:1000])
    pixels = np.frombuffer(result.stdout, dtype='<f4').reshape(3, height, width)[[2, 0, 1]].transpose(1, 2, 0).copy()
    if not np.isfinite(pixels).all():
        raise ValueError('Nonfinite source EXR pixels')
    return pixels


def gamma(values):
    values = np.maximum(values, 0)
    return np.where(values <= .0031308, values*12.92, 1.055*np.power(values, 1/2.4)-.055)


def sample_vat(pixels, uv, offset):
    height, width = pixels.shape[:2]
    x = np.floor(np.mod(uv[:, 0], 1)*width).astype(np.int64)%width
    y = np.floor(np.mod(uv[:, 1]+offset, 1)*height).astype(np.int64)%height
    # Unity UV origin is bottom-left; source EXR rows and FFmpeg are top-down.
    return pixels[height-1-y, x]


def field_vector(text, name, labels='xyz'):
    match = re.search(r'^\s*'+re.escape(name)+r': \{([^}]+)\}', text, re.M)
    if not match:
        raise ValueError(f'Missing vector {name}')
    fields = dict(re.findall(r'(\w+): ([^,}]+)', match[1]))
    value = np.array([float(fields[key]) for key in labels], dtype=np.float64)
    if not np.isfinite(value).all():
        raise ValueError('Nonfinite source vector')
    return value


def reference(text, field):
    match = re.search(re.escape(field)+r': \{fileID: [^,}]+, guid: ([0-9a-f]{32}),', text)
    if not match:
        raise ValueError(f'Missing GUID reference {field}')
    return match[1]


def documents(text):
    parts = re.split(r'^--- !u!(\d+) &(-?\d+)\r?\n', text, flags=re.M)
    return {int(parts[i+1]): (int(parts[i]), parts[i+2]) for i in range(1, len(parts), 3)}


def owner(text):
    return int(re.search(r'm_GameObject: \{fileID: (-?\d+)\}', text)[1])


def transform_matrix(body):
    q = field_vector(body, 'm_LocalRotation', 'xyzw')
    q /= np.linalg.norm(q)
    x, y, z, w = q
    rotation = np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                         [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                         [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
    result = np.eye(4)
    result[:3, :3] = rotation@np.diag(field_vector(body, 'm_LocalScale'))
    result[:3, 3] = field_vector(body, 'm_LocalPosition')
    return result


def float_property(material, name):
    matches = re.findall(r'^\s*- '+re.escape(name)+r': ([^\r\n{}]+)$', material, re.M)
    if len(matches) != 1:
        raise ValueError(f'Expected one scalar {name}')
    return float(matches[0])


def texture_reference(material, name):
    match = re.search(r'^\s*- '+re.escape(name)+r':\s*\n\s*m_Texture: \{fileID: [^,}]+, guid: ([0-9a-f]{32}),', material, re.M)
    if not match:
        raise ValueError(f'Missing texture {name}')
    return match[1]


def silhouette(path, frames, duration, scale):
    image = Image.new('RGB', (1200, 430), '#141a20')
    draw = ImageDraw.Draw(image)
    for col, age in enumerate((.1, .3, .7)):
        frame = frames[min(len(frames)-1, round(age/duration*(len(frames)-1))), :, :3]*scale
        cx, cy, pixels = col*400+70, 195, 230
        draw.line((col*400+20, cy, col*400+380, cy), fill='#41505c')
        draw.ellipse((cx-3, cy-3, cx+3, cy+3), fill='white')
        for x, y, z in frame:
            px, py = cx+z*pixels, cy-y*pixels
            draw.point((int(px), int(py)), fill='#bb393e')
        draw.text((col*400+20, 20), f'{age:.1f}s  +Z forward / Y up  scale {scale}', fill='white')
        draw.text((col*400+20, 390), 'CPU source geometry projection; not an engine screenshot', fill='#a7b2bb')
    image.save(path)


def cook(source_bank, inventory_path, output, previews, variant_names=VARIANTS, presentation_scale=PRESENTATION_SCALE):
    source_bank, output = Path(source_bank).absolute(), Path(output).absolute()
    if output.exists() or output.is_symlink():
        raise FileExistsError('Refusing existing runtime bank')
    report = json.loads(Path(inventory_path).read_text())
    extraction = json.loads((source_bank/'extraction-manifest.json').read_text())
    if extraction['package_sha256'] != report['sha256']:
        raise ValueError('Inspector archive hashes differ')
    entries = {item['guid']: item for item in report['entries']}
    extracted = {item['path']: item for item in extraction['extracted']}
    used = {}
    def source(path):
        path = str(strict_source_path(path))
        item = extracted[path]
        file = source_bank/path
        if file.is_symlink() or not file.is_file() or sha256_file(file) != item['sha256']:
            raise ValueError(f'Changed source bank file: {path}')
        used[path] = {'sha256': item['sha256'], 'bytes': item['bytes'], 'guid': item['guid']}
        return file
    def by_guid(guid):
        return source(entries[guid]['path'])
    # Source code is inspected as text, but no purchased code is executed.
    source(SOURCE_PREFIX+'Shaders/BFX_Blood.shader')
    source(SOURCE_PREFIX+'Shaders/BFX_Decal_Plane.shader')
    if not 1 <= len(variant_names) <= 3 or not .02 <= presentation_scale <= 2:
        raise ValueError('Bounded variants and presentation scale required')
    variants, records = [], []
    for name in variant_names:
        if not re.fullmatch(r'Blood(?:[1-9]|2_Left|2_Right)', name):
            raise ValueError('Unsupported source variant name')
        prefab = source(SOURCE_PREFIX+'Prefabs/'+name+'.prefab')
        docs = documents(prefab.read_text(encoding='utf-8-sig'))
        filters = [body for kind, body in docs.values() if kind == 33 and 'fileID: 4300000' in body]
        if len(filters) != 1:
            raise ValueError('Expected one original fluid mesh')
        fluid_owner = owner(filters[0])
        renderer = next(body for kind, body in docs.values() if kind == 23 and owner(body) == fluid_owner)
        animation = next(body for kind, body in docs.values() if kind == 114 and owner(body) == fluid_owner)
        transforms = {key: body for key, (kind, body) in docs.items() if kind == 4}
        fluid_transform = next(key for key, body in transforms.items() if owner(body) == fluid_owner)
        def world_transform(key, visited=None):
            visited = set() if visited is None else visited
            if key in visited or len(visited) > 16:
                raise ValueError('Cyclic/deep prefab hierarchy')
            visited.add(key)
            body = transforms[key]
            matrix = transform_matrix(body)
            parent = int(re.search(r'm_Father: \{fileID: (-?\d+)\}', body)[1])
            return world_transform(parent, visited)@matrix if parent else matrix
        transform = world_transform(fluid_transform)
        material_path = by_guid(re.search(r'guid: ([0-9a-f]{32}), type: 2', renderer)[1])
        material = material_path.read_text(encoding='utf-8-sig')
        mesh_path = by_guid(reference(filters[0], 'm_Mesh'))
        uv, indices = mesh_addressing(mesh_path)
        pos_path = by_guid(texture_reference(material, '_posTex'))
        normal_path = by_guid(texture_reference(material, '_nTex'))
        positions, normals = read_exr(pos_path), read_exr(normal_path)
        if positions.shape != normals.shape:
            raise ValueError('Mismatching original VAT dimensions')
        count = int(re.search(r'FramesCount: (\S+)', animation)[1])+1
        duration = float(re.search(r'TimeLimit: (\S+)', animation)[1])
        offset = float(re.search(r'OffsetFrames: (\S+)', animation)[1])
        if not 2 <= count <= 256 or not .1 <= duration <= 10 or offset != 0:
            raise ValueError('Unsupported original animation timing')
        minimum, maximum = float_property(material, '_boundingMin'), float_property(material, '_boundingMax')
        height_offset = field_vector(material, '- _HeightOffset', 'rgb')
        raw_frames = np.empty((count, len(uv), 6), dtype='<f4')
        linear = np.diag([1., 1., -1.])@transform[:3, :3]
        translate = np.diag([1., 1., -1.])@transform[:3, 3]
        normal_matrix = np.linalg.inv(linear).T
        for frame in range(count):
            source_offset = math.ceil(-(frame+offset+1.1))/count+1/count
            p = gamma(sample_vat(positions, uv, source_offset))*(maximum-minimum)+minimum
            p[:, 0] *= -1
            p = p[:, [0, 2, 1]]+height_offset
            n = gamma(sample_vat(normals, uv, source_offset))[:, [0, 2, 1]]*2-1
            n[:, 0] *= -1
            raw_frames[frame, :, :3] = p@linear.T+translate
            raw_frames[frame, :, 3:] = n@normal_matrix.T
        direction = raw_frames[max(1, round((count-1)*.15)), :, :3].mean(0)
        angle = math.atan2(float(direction[0]), float(direction[2]))
        ca, sa = math.cos(-angle), math.sin(-angle)
        canonical = np.array([[ca, 0, sa], [0, 1, 0], [-sa, 0, ca]])
        raw_frames[:, :, :3] = raw_frames[:, :, :3]@canonical.T
        raw_frames[:, :, 3:] = raw_frames[:, :, 3:]@canonical.T
        lengths = np.linalg.norm(raw_frames[:, :, 3:], axis=2, keepdims=True)
        raw_frames[:, :, 3:] /= np.maximum(lengths, 1e-10)
        if not np.isfinite(raw_frames).all() or np.max(np.abs(raw_frames[:, :, :3])) > 100:
            raise ValueError('Decoded effect exceeds finite source bounds')
        test = raw_frames[max(1, count//5)]
        triangles = test[:, :3].reshape(-1, 3, 3)
        cross = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
        signed = np.einsum('ij,ij->i', cross, test[:, 3:].reshape(-1, 3, 3).mean(1))
        reverse = np.sum(signed) < 0
        if reverse:
            indices = indices.reshape(-1, 3)[:, [0, 2, 1]].copy().reshape(-1)
        decal_renderer = next(body for kind, body in docs.values() if kind == 23 and owner(body) != fluid_owner)
        decal_owner = owner(decal_renderer)
        decal_transform = world_transform(next(key for key, body in transforms.items() if owner(body) == decal_owner))
        decal_material = by_guid(re.search(r'guid: ([0-9a-f]{32}), type: 2', decal_renderer)[1]).read_text(encoding='utf-8-sig')
        mask_path = by_guid(texture_reference(decal_material, '_CutoutTex'))
        norm_alpha_path = by_guid(texture_reference(decal_material, '_MainTex'))
        mask_image = Image.open(mask_path).convert('RGBA')
        normal_image = Image.open(norm_alpha_path).convert('RGBA')
        common_size = tuple(max(a, b) for a, b in zip(mask_image.size, normal_image.size))
        if max(common_size) > 2048:
            raise ValueError('Unsupported source decal dimensions')
        mask = np.asarray(mask_image.resize(common_size, Image.Resampling.BILINEAR), dtype=np.float32)/255
        norm_alpha = np.asarray(normal_image.resize(common_size, Image.Resampling.BILINEAR), dtype=np.float32)/255
        alpha = np.clip(norm_alpha[:, :, 3]*2, 0, 1)*np.clip(mask[:, :, 0]*20, 0, 1)
        shade = 1-.75*mask[:, :, 2]*alpha
        decal = np.empty_like(mask, dtype=np.uint8)
        decal[:, :, :3] = np.round(shade[:, :, None]*255).astype(np.uint8)
        decal[:, :, 3] = np.round(alpha*255).astype(np.uint8)
        if not np.any(decal[:, :, 3] == 0) or not np.any(decal[:, :, 3] > 128):
            raise ValueError('Source decal mask lost its cutout')
        # Rotate source footprint axes into the canonical horizontal basis.
        # These source jets point along X; their ground rectangle follows X.
        decal = np.ascontiguousarray(np.rot90(decal))
        size = np.linalg.norm(decal_transform[:3, :3], axis=0)[[2, 0]]
        color = field_vector(material, '- _Color', 'rgba')
        color[:3] *= .65  # Explicit BFX_Blood.shader linear-render branch.
        variants.append((raw_frames, indices, duration, color, size, decal))
        records.append({'name': name, 'duration': duration, 'frame_count': count,
            'vertex_count': len(uv), 'triangle_count': len(indices)//3,
            'vat_dimensions': list(positions.shape[:2][::-1]),
            'source_prefab_transform': transform.tolist(), 'canonical_yaw_radians': -angle,
            'reversed_winding_to_match_decoded_normals': bool(reverse),
            'source_bounds': [raw_frames[:, :, :3].min((0, 1)).tolist(), raw_frames[:, :, :3].max((0, 1)).tolist()],
            'sample_bounds': {str(t): [raw_frames[round(t/duration*(count-1)), :, :3].min(0).tolist(), raw_frames[round(t/duration*(count-1)), :, :3].max(0).tolist()] for t in (.1, .3, .7)},
            'color_linear': color.tolist(), 'decal_size_xz': size.tolist(),
            'decal_alpha_nonzero_fraction': float(np.count_nonzero(decal[:, :, 3])/decal[:, :, 3].size)})
    output.mkdir(parents=True)
    try:
        bank = output/'blood.bank'
        with bank.open('xb') as stream:
            stream.write(MAGIC+struct.pack('<II', 1, len(variants)))
            for frames, indices, duration, color, size, decal in variants:
                height, width = decal.shape[:2]
                stream.write(struct.pack('<5If6f', frames.shape[1], len(indices), len(frames), width, height, duration, *color, *size))
                stream.write(indices.astype('<u4').tobytes())
                stream.write(frames.astype('<f4').tobytes())
                stream.write(decal.tobytes())
        if bank.stat().st_size > 96*1024**2:
            raise ValueError('Cooked bank exceeds fixed renderer asset budget')
        provenance = {'schema': 'valwood.volumetric-blood-provenance', 'version': 1,
            'source_archive_sha256': report['sha256'], 'bank_sha256': sha256_file(bank),
            'presentation_scale': presentation_scale, 'source_files': used, 'effects': records,
            'decoding': {'geometry': 'Original ASCII FBX polygon corners/UVs; fixed triangle topology.',
                'vat': 'Point sample repeat UV (u,v+ceil(-(frame+1.1))/N+1/N); EXR top-down to Unity bottom-up; exact BFX linear-to-gamma; scalar bounds; X sign and XZY swizzle; HeightOffset; full prefab hierarchy; Unity Z reflection; yaw only to +Z emission.',
                'timing': 'N=FramesCount+1, original linear AnimationSpeed curve, duration=TimeLimit. Stored frames indexed age/duration*(N-1).',
                'color': 'Serialized _Color RGB multiplied .65 per original BFX linear-render branch; normals decoded and normalized.',
                'decal': 'Original mask RGB and normal-map alpha; alpha=clamp(normalA*2)*clamp(maskR*20) at source cutout0. RGB retains maskB darkening. Original rectangle dimensions rotated to canonical axes.',
                'adaptation': f'Runtime ground-only planar stains replace Unity screen-depth/bone decals and height scripts. Extra {presentation_scale:.2f} presentation scale is separate from unchanged authored prefab-meter frames.'},
            'package_code_executed': False, 'unity_imported': False,
            'format': 'VWBLOOD1 LE u32 version,variants; each u32 vertex,index,frame,maskW,maskH; f32 duration,colorRGBA[4],decalSizeXZ[2]; u32 indices[]; f32 frames[][vertex][xyz,nxyz]; u8 decalRGBA[]'}
        (output/'provenance.json').write_text(json.dumps(provenance, indent=2)+'\n')
        manifest = {'schema': 'valwood.volumetric-blood-assets', 'version': 1, 'ready': True,
            'source_archive_sha256': report['sha256'], 'presentation_scale': presentation_scale,
            'effects': records, 'assets': [{'file': path.name, 'sha256': sha256_file(path)} for path in (bank, output/'provenance.json')]}
        (output/'BLOOD_ASSETS.json').write_text(json.dumps(manifest, indent=2)+'\n')
        if previews:
            previews = Path(previews)
            previews.mkdir(parents=True, exist_ok=True)
            for record, variant in zip(records, variants):
                silhouette(previews/(record['name']+'-cpu-projection.png'), variant[0], variant[2], presentation_scale)
        return manifest
    except BaseException:
        shutil.rmtree(output)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-bank', type=Path, required=True)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--previews', type=Path)
    parser.add_argument('--variants', nargs='+', default=VARIANTS)
    parser.add_argument('--presentation-scale', type=float, default=PRESENTATION_SCALE)
    args = parser.parse_args()
    result = cook(args.source_bank, args.inventory, args.output, args.previews, args.variants, args.presentation_scale)
    print(json.dumps({'ready': result['ready'], 'assets': result['assets'],
                      'effects': [{k: effect[k] for k in ('name', 'frame_count', 'vertex_count', 'triangle_count', 'duration')} for effect in result['effects']]}, indent=2))


if __name__ == '__main__':
    main()

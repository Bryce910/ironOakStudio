#!/usr/bin/env python3
"""Cook selected purchased Finward prefab exports and their actual URP materials.

Inputs are immutable Unity JSON/bin exports and the purchased .mat/TIFF/meta
files. No Unity/source code is run here. Output is a NEW directory of static
embedded GLBs and source/geometry/material reports, suitable for the existing
opening accumulator. No accepted project, save, collider or runtime is edited.

The material GUID matches official URP Lit. Active URP maps/keywords, importer
colour space, scalar smoothness, AO strength and base ST are used; retained but
inactive HDRP remaps are documentary only. TIFF ExtraSamples=0 is interpreted
as a fourth *data* channel by changing that tag in an in-memory decoding copy.
Otherwise Pillow silently discards the actual smoothness/opacity channel.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import tempfile

import numpy as np
from PIL import Image
import yaml

from build_opening_prologue import Glb, read_glb, array


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'assets/purchased/finward_house_props'
SHADER_GUID = '933532a4fcc9baf4fa0491de14d08ed7'
SHADER_SOURCES = {
    'guid': 'https://raw.githubusercontent.com/Unity-Technologies/Graphics/6000.3/staging/Packages/com.unity.render-pipelines.universal/Shaders/Lit.shader.meta',
    'semantics': 'https://raw.githubusercontent.com/Unity-Technologies/Graphics/6000.3/staging/Packages/com.unity.render-pipelines.universal/Shaders/LitInput.hlsl',
}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + '\n')


def unity_documents(path):
    text = re.sub(r'^%.*\n', '', path.read_text(), flags=re.M)
    text = re.sub(r'^--- !u!\d+ &-?\d+', '---', text, flags=re.M)
    return [item for item in yaml.safe_load_all(text) if item]


def srgb_to_linear(value):
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)


def linear_to_srgb(value):
    return np.where(value <= .0031308, value * 12.92, 1.055 * np.maximum(value, 0) ** (1 / 2.4) - .055)


def resize(value, side):
    height, width = value.shape[:2]
    ratio = min(1., side / max(height, width))
    size = (max(1, round(width * ratio)), max(1, round(height * ratio)))
    if size == (width, height):
        return value.copy()
    return np.stack([np.asarray(Image.fromarray(value[:, :, channel], 'F').resize(size, Image.Resampling.LANCZOS))
                     for channel in range(value.shape[2])], axis=2)


def decode_source_image(path):
    """Preserve four TIFF samples, including an unspecified data extra sample."""
    raw = path.read_bytes()
    patched = False
    with Image.open(io.BytesIO(raw)) as original:
        samples = int(original.tag_v2.get(277, len(original.getbands()))) if original.format == 'TIFF' else len(original.getbands())
        extras = tuple(original.tag_v2.get(338, ())) if original.format == 'TIFF' else ()
    if samples == 4 and extras == (0,):
        work = bytearray(raw)
        order = '<' if work[:2] == b'II' else '>' if work[:2] == b'MM' else None
        if order is None or struct.unpack_from(order + 'H', work, 2)[0] != 42:
            raise ValueError('Unreviewed TIFF layout: ' + str(path))
        offset = struct.unpack_from(order + 'I', work, 4)[0]
        count = struct.unpack_from(order + 'H', work, offset)[0]
        for index in range(count):
            at = offset + 2 + index * 12
            tag, kind, length = struct.unpack_from(order + 'HHI', work, at)
            if tag == 338:
                if kind != 3 or length != 1 or struct.unpack_from(order + 'H', work, at + 8)[0] != 0:
                    raise ValueError('Unreviewed TIFF extra-sample declaration')
                struct.pack_into(order + 'H', work, at + 8, 2)
                patched = True
                break
        if not patched:
            raise ValueError('TIFF fourth data sample could not be preserved')
        raw = bytes(work)
    with Image.open(io.BytesIO(raw)) as image:
        image.load()
        if samples == 4 and image.mode != 'RGBA':
            raise ValueError('TIFF decoder lost the fourth sample: ' + str(path))
        result = np.asarray(image.convert('RGBA'), dtype=np.float32) / 255.
    return result, dict(samples=samples, extra_samples=list(extras),
                        extra_sample_decode_copy_patched=patched,
                        original_file_unchanged=True,
                        rgba_minimum=result.min(axis=(0, 1)).tolist(),
                        rgba_maximum=result.max(axis=(0, 1)).tolist(),
                        rgba_mean=result.mean(axis=(0, 1), dtype=np.float64).tolist())


class Materials:
    def __init__(self, output, size):
        self.output = output
        self.size = size
        self.source = DATA / 'source'
        inventory = json.loads((DATA / 'INVENTORY.json').read_text())
        self.paths = {entry['guid']: entry['path'] for entry in inventory['entries']}
        self.records = {}
        self.inputs = {}
        self.images = {}

    def source_path(self, path):
        value = PurePosixPath(path)
        if value.is_absolute() or '..' in value.parts or not value.parts or value.parts[0] != 'Assets':
            raise ValueError('Unsafe purchased source path')
        result = self.source / str(value)
        if not result.resolve(strict=True).is_relative_to(self.source.resolve()):
            raise ValueError('Purchased source path escaped its source root')
        self.inputs[str(result.relative_to(DATA))] = sha(result)
        return result

    def sample(self, info, role, records):
        guid = info.get('m_Texture', {}).get('guid')
        if not guid:
            return None
        if guid not in self.paths:
            raise ValueError('Unresolved purchased texture GUID: ' + guid)
        path = self.source_path(self.paths[guid])
        meta_path = path.with_name(path.name + '.meta')
        self.inputs[str(meta_path.relative_to(DATA))] = sha(meta_path)
        meta = yaml.safe_load(meta_path.read_text())['TextureImporter']
        value, decode = decode_source_image(path)
        srgb = bool(meta.get('mipmaps', {}).get('sRGBTexture', 1))
        if role == 'normal':
            if srgb or meta.get('textureType') != 1 or meta.get('bumpmap', {}).get('convertToNormalMap'):
                raise ValueError('Unreviewed source normal importer semantics: ' + str(path))
            if meta.get('flipGreenChannel', 0):
                value[:, :, 1] = 1. - value[:, :, 1]
            # Engine tangents are regenerated from the exported UV0, whose V
            # axis is reflected below. With C=diag(1,1,-1), T'=CT but B'=-CB;
            # the sampled tangent-space Y must therefore change sign too.
            # A headless native Unity Mesh.tangents probe confirms this on
            # all 10,564 source couch triangles. Compose this with, rather
            # than replacing, any original importer flipGreenChannel flag.
            value[:, :, 1] = 1. - value[:, :, 1]
        elif srgb:
            value[:, :, :3] = srgb_to_linear(value[:, :, :3])
        records[role] = dict(path=str(path.relative_to(DATA)), sha256=sha(path),
                             meta_sha256=sha(meta_path), source_srgb=srgb,
                             source_settings=meta.get('textureSettings', {}), decode=decode)
        if role == 'normal':
            records[role].update(source_importer_flip_green=bool(meta.get('flipGreenChannel', 0)),
                                 native_uv_v_reflection_green_inverted=True)
        return value

    def store_image(self, role, value, side):
        value = resize(value, side)
        if role == 'normal':
            normal = value[:, :, :3] * 2. - 1.
            normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-8)
            value = normal * .5 + .5
        encoded = io.BytesIO()
        Image.fromarray(np.uint8(np.clip(value, 0, 1) * 255. + .5)).save(encoded, format='PNG', compress_level=6)
        raw = encoded.getvalue()
        digest = hashlib.sha256(raw).hexdigest()
        relative = 'textures/' + digest + '.png'
        path = self.output / relative
        path.parent.mkdir(exist_ok=True)
        if path.exists():
            if path.read_bytes() != raw:
                raise ValueError('Cooked image hash collision')
        else:
            path.write_bytes(raw)
        self.images[relative] = dict(sha256=digest, bytes=len(raw), width=value.shape[1],
                                     height=value.shape[0], role=role,
                                     estimated_rgba8_mips_bytes=round(value.shape[0] * value.shape[1] * 4 * 4 / 3))
        return relative

    def make(self, path):
        if path in self.records:
            return self.records[path]
        source = self.source_path(path)
        material = next(item['Material'] for item in unity_documents(source) if 'Material' in item)
        if material['m_Shader'].get('guid') != SHADER_GUID:
            raise ValueError('Unreviewed Finward source shader: ' + path)
        saved = material['m_SavedProperties']
        floats = {key: value for row in saved.get('m_Floats', []) for key, value in row.items()}
        colors = {key: value for row in saved.get('m_Colors', []) for key, value in row.items()}
        tex = {key: value for row in saved.get('m_TexEnvs', []) for key, value in row.items()}
        keywords = set(material.get('m_ValidKeywords', material.get('m_ShaderKeywords', '').split()))
        unsupported = keywords.intersection({'_SPECULAR_SETUP', '_DETAIL_MULX2', '_DETAIL_SCALED', '_PARALLAXMAP', '_CLEARCOAT', '_CLEARCOATMAP'})
        if unsupported:
            raise ValueError('Finward shader features need explicit baking: ' + str(unsupported))
        sources = {}
        base = self.sample(tex.get('_BaseMap', {}), 'base', sources)
        if base is None:
            base = np.ones((1, 1, 4), np.float32)
        normal = self.sample(tex.get('_BumpMap', {}), 'normal', sources) if '_NORMALMAP' in keywords else None
        mask = self.sample(tex.get('_MetallicGlossMap', {}), 'metallic_gloss', sources) if '_METALLICSPECGLOSSMAP' in keywords else None
        occlusion = self.sample(tex.get('_OcclusionMap', {}), 'occlusion', sources) if '_OCCLUSIONMAP' in keywords else None
        smoothness = float(floats.get('_Smoothness', .5))
        size = (min(self.size, 512), min(self.size, 512))
        smooth_albedo = '_SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A' in keywords
        if smooth_albedo:
            smooth = resize(base[:, :, 3:4], size[0])[:, :, 0] * smoothness
        elif mask is not None:
            smooth = resize(mask[:, :, 3:4], size[0])[:, :, 0] * smoothness
        else:
            smooth = np.full(size, smoothness, np.float32)
        shape = smooth.shape
        def channel(value, index):
            if value is None:
                return None
            return np.asarray(Image.fromarray(value[:, :, index], 'F').resize(shape[::-1], Image.Resampling.LANCZOS))
        metal = channel(mask, 0) if mask is not None else np.full(shape, floats.get('_Metallic', 0), np.float32)
        ao = np.ones(shape, np.float32)
        if occlusion is not None:
            strength = float(floats.get('_OcclusionStrength', 1))
            ao = 1. + strength * (channel(occlusion, 1) - 1.)
        arm = np.stack((ao, 1. - smooth, metal), axis=2)
        # Re-encode sampled linear colour for glTF's sRGB colour textures;
        # source alpha remains linear and is never inferred from RGB.
        base[:, :, :3] = linear_to_srgb(base[:, :, :3])
        if smooth_albedo:
            base[:, :, 3] = 1.
        paths = dict(base=self.store_image('base', base, self.size),
                     arm=self.store_image('arm', arm, min(self.size, 512)))
        if normal is not None:
            paths['normal'] = self.store_image('normal', normal[:, :, :3], self.size)
        color = colors.get('_BaseColor', dict(r=1, g=1, b=1, a=1))
        output_material = dict(name='Finward ' + material['m_Name'],
            pbrMetallicRoughness=dict(baseColorFactor=[float(color[c]) for c in 'rgba'],
                                     roughnessFactor=1., metallicFactor=1.),
            alphaMode='BLEND' if floats.get('_Surface', 0) else 'MASK' if floats.get('_AlphaClip', 0) else 'OPAQUE',
            doubleSided=floats.get('_Cull', 2) == 0)
        if output_material['alphaMode'] == 'MASK':
            output_material['alphaCutoff'] = float(floats.get('_Cutoff', .5))
        if normal is not None:
            output_material['normalTexture'] = dict(scale=float(floats.get('_BumpScale', 1)))
        if '_EMISSION' in keywords:
            emission = self.sample(tex.get('_EmissionMap', {}), 'emission', sources)
            ecolor = colors.get('_EmissionColor', dict(r=0, g=0, b=0))
            output_material['emissiveFactor'] = [float(ecolor[c]) for c in 'rgb']
            if emission is not None:
                paths['emission'] = self.store_image('emission', linear_to_srgb(emission[:, :, :3]), self.size)
        base_info = tex.get('_BaseMap', {})
        scale = base_info.get('m_Scale', dict(x=1, y=1))
        offset = base_info.get('m_Offset', dict(x=0, y=0))
        result = dict(source=path, source_sha256=sha(source), name=material['m_Name'],
                      shader_guid=SHADER_GUID, active_keywords=sorted(keywords),
                      material=output_material, paths=paths, sources=sources,
                      uv_scale=[float(scale[c]) for c in 'xy'], uv_offset=[float(offset[c]) for c in 'xy'],
                      active_scalars={k: floats.get(k) for k in ('_Metallic', '_Smoothness', '_BumpScale', '_OcclusionStrength', '_SmoothnessTextureChannel', '_Surface', '_AlphaClip', '_Cutoff', '_Cull')},
                      inactive_hdrp_remaps_ignored={k: floats[k] for k in ('_AORemapMin', '_AORemapMax', '_SmoothnessRemapMin', '_SmoothnessRemapMax') if k in floats},
                      native_arm_statistics=dict(minimum=np.clip(arm, 0, 1).min(axis=(0, 1)).tolist(), maximum=np.clip(arm, 0, 1).max(axis=(0, 1)).tolist(), mean=np.clip(arm, 0, 1).mean(axis=(0, 1), dtype=np.float64).tolist()),
                      semantics='Active URP Lit: mapped metallic R directly; smoothness from map A (or active albedo-A keyword) times _Smoothness; AO=lerp(1,occlusion G,strength). All texture samples use transformed BaseMap UV0. Importer sRGB RGB decoding honored; alpha stays linear.')
        self.records[path] = result
        print('MATERIAL', result['name'], 'roughness mean', round(result['native_arm_statistics']['mean'][1], 4), flush=True)
        return result

    def append(self, g, path):
        info = self.make(path)
        material = copy.deepcopy(info['material'])
        pbr = material['pbrMetallicRoughness']
        for role, relative in info['paths'].items():
            # These bounded, decoded and resized PNGs were already prepared
            # once for this material. Embed their exact bytes, without another
            # expensive image encode for each prefab and each identical LOD.
            cache = getattr(g, '_finward_prepared_textures', None)
            if cache is None:
                cache = g._finward_prepared_textures = {}
            if relative not in cache:
                raw = (self.output / relative).read_bytes()
                g.doc['images'].append(dict(bufferView=g.view(raw), mimeType='image/png'))
                g.doc['textures'].append(dict(sampler=0, source=len(g.doc['images']) - 1))
                cache[relative] = len(g.doc['textures']) - 1
            index = cache[relative]
            if role == 'base':
                pbr['baseColorTexture'] = dict(index=index)
            elif role == 'arm':
                pbr['metallicRoughnessTexture'] = dict(index=index)
                material['occlusionTexture'] = dict(index=index)
            elif role == 'normal':
                material['normalTexture']['index'] = index
            elif role == 'emission':
                material['emissiveTexture'] = dict(index=index)
        g.doc['materials'].append(material)
        return len(g.doc['materials']) - 1, info


def load_export(path):
    doc = json.loads(path.read_text())
    if doc.get('stride') != 56 or doc.get('version') != 1:
        raise ValueError('Unreviewed Unity furniture binary schema')
    blob = path.with_suffix('.bin').read_bytes()
    parts = []
    for meta in doc['parts']:
        if not meta.get('renderer_active', True) or not meta.get('renderer_enabled', True):
            continue
        vertex = np.ndarray((meta['vertices'], 14), '<f4', blob, meta['vertex_offset']).copy()
        indices = np.ndarray((meta['indices'],), '<i4', blob, meta['index_offset']).copy()
        if not len(vertex) or not len(indices) or len(indices) % 3 or not np.isfinite(vertex).all() or np.any(indices < 0) or indices.max() >= len(vertex):
            raise ValueError('Invalid native furniture geometry: ' + str(path))
        if not meta.get('uv0_present', True) or meta.get('uv0_default', False):
            raise ValueError('Furniture source has missing/default UV0')
        used, indices = np.unique(indices, return_inverse=True)
        parts.append((meta, vertex[used], indices.reshape(-1, 3)))
    return doc, parts


def cook_export(path, output, materials):
    doc, parts = load_export(path)
    near = [vertex[:, :3] for meta, vertex, index in parts if meta['lod'] == 0]
    if not near:
        raise ValueError('No source LOD0 in furniture export')
    points = np.concatenate(near)
    low, high = points.min(0), points.max(0)
    origin = np.array([(low[0] + high[0]) * .5, low[1], (low[2] + high[2]) * .5])
    records = []
    for lod, suffix in ((0, 'near'), (1, 'mid')):
        g = Glb()
        mapping = {}
        source_parts = []
        for meta, vertex, indices in parts:
            if meta['lod'] != lod:
                continue
            key = meta['material']
            if key not in mapping:
                mapping[key] = materials.append(g, key)
            material, info = mapping[key]
            position = vertex[:, :3] - origin
            normal = vertex[:, 3:6].copy()
            position[:, 2] *= -1
            normal[:, 2] *= -1
            normal /= np.maximum(np.linalg.norm(normal, axis=1, keepdims=True), 1e-10)
            uv = vertex[:, 6:8] * info['uv_scale'] + info['uv_offset']
            uv[:, 1] = 1. - uv[:, 1]
            # Native Unity export retains original index order. A single
            # reflected Z axis needs the corresponding winding reflection.
            indices = indices[:, [0, 2, 1]]
            cross = np.cross(position[indices[:, 1]] - position[indices[:, 0]], position[indices[:, 2]] - position[indices[:, 0]])
            nonzero = np.linalg.norm(cross, axis=1) > 1e-12
            dropped = int((~nonzero).sum())
            agreement = np.einsum('ij,ij->i', cross, normal[indices].sum(axis=1))
            opposing = int((agreement[nonzero] < -1e-9).sum())
            if opposing / max(1, int(nonzero.sum())) > .02:
                raise ValueError('Furniture winding/normal basis disagreement: ' + meta['hierarchy'])
            indices = indices[nonzero].reshape(-1)
            g.mesh(position, normal, uv, indices, material)
            source_parts.append(dict(name=meta['name'], hierarchy=meta.get('hierarchy'), material=key,
                                     mesh_source=meta.get('mesh_source'), source_lod=meta.get('source_lod'),
                                     triangles=len(indices) // 3, degenerate_source_triangles_removed=dropped,
                                     source_opposing_normal_triangles_retained=opposing))
        if not source_parts:
            raise ValueError('Missing authored LOD in furniture export')
        filename = doc['name'] + '_' + suffix + '.glb'
        destination = output / filename
        destination.write_bytes(g.finish())
        # Decode the actual completed GLB rather than trusting writer counters.
        loaded, binary = read_glb(destination)
        placed = np.concatenate([array(loaded, binary, p['attributes']['POSITION']) for p in loaded['meshes'][0]['primitives']])
        records.append(dict(lod=lod, file=filename, sha256=sha(destination), bytes=destination.stat().st_size,
                            primitives=len(loaded['meshes'][0]['primitives']), images=len(loaded['images']),
                            triangles=sum(p['triangles'] for p in source_parts),
                            minimum=placed.min(0).tolist(), maximum=placed.max(0).tolist(), parts=source_parts))
    print('COOKED', doc['name'], [record['triangles'] for record in records], flush=True)
    return dict(name=doc['name'], source_prefab=doc['source'], native_export=path.name,
                native_export_sha256=sha(path), native_binary_sha256=sha(path.with_suffix('.bin')),
                source_minimum=low.tolist(), source_maximum=high.tolist(),
                dimensions=(high - low).tolist(), source_origin_unity=origin.tolist(),
                coordinates='Source Unity root-relative metres -> RH Y-up glTF by Z reflection and winding reversal; base ST then V=1-V; grounded and XZ centered from source LOD0; mounting/source-origin Y retained in report',
                lod_policy=doc.get('lod_policy'), models=records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exports', type=Path, default=DATA / 'exported')
    parser.add_argument('--output', type=Path, default=DATA / 'cooked-v2')
    parser.add_argument('--size', type=int, choices=(512, 1024, 2048), default=1024)
    parser.add_argument('--only', action='append', default=[])
    args = parser.parse_args()
    if args.output.exists() or args.output.is_symlink():
        raise ValueError('Choose a NEW cook output directory')
    paths = sorted(path for path in args.exports.glob('*.json') if path.with_suffix('.bin').is_file())
    if args.only:
        paths = [path for path in paths if path.stem in args.only]
        if {path.stem for path in paths} != set(args.only):
            raise ValueError('Requested furniture export is missing')
    if not paths:
        raise ValueError('No native furniture exports are ready')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='.finward-cook-', dir=args.output.parent))
    try:
        materials = Materials(temporary, args.size)
        assets = [cook_export(path, temporary, materials) for path in paths]
        for relative, expected in materials.inputs.items():
            if sha(DATA / relative) != expected:
                raise ValueError('Purchased input changed during cook: ' + relative)
        write_json(temporary / 'material-report.json', materials.records)
        write_json(temporary / 'catalog.json', assets)
        write_json(temporary / 'COOK_REVIEW.json', dict(passed=True,
            source_product='House Props &Furniture Vol.2, Unity265523; user-purchased proprietary assets',
            source_inputs=materials.inputs, source_inputs_unchanged=True,
            shader_sources=SHADER_SOURCES, shader_guid=SHADER_GUID,
            assets=len(assets), models=sum(len(item['models']) for item in assets),
            texture_size_cap=args.size, arm_size_cap=min(args.size, 512),
            unique_images=materials.images,
            estimated_unique_rgba8_mips_bytes=sum(item['estimated_rgba8_mips_bytes'] for item in materials.images.values()),
            near_triangles=sum(item['models'][0]['triangles'] for item in assets),
            geometry='All active selected prefab child parts and source LOD0/last authored LOD retained; only zero-area triangles removed; no lossy mesh simplification',
            normal_basis='C=reflectZ and UV=(u,1-v), recomputed engine T=CT_source and B=-CB_source; normal green inverted after honoring source importer flipGreenChannel. Confirmed by NORMAL_BASIS_PROBE.json actual Unity Mesh.tangents.',
            runtime_project_changed=False, gpu_used=False,
            limitations=['CPU source cook and geometry/material inspection only; native placement, collision and visual acceptance pending',
                         'Source transparent BLEND flags retained; current native world renderer approximates BLEND with alpha cutoff, so glass appearance requires visual review',
                         'Source HDR emission factor may exceed1 as supported by the existing native loader; no Unity light, postprocessing, reflection or shadow pipeline is imported']))
        temporary.replace(args.output)
        print('READY', args.output, len(assets), 'prefabs', len(materials.records), 'materials', flush=True)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Turn the original rent notice toward the opening camera in a new project.

Only the four TEXCOORD_0 pairs belonging to the original household-bill
material change. The GLB container, JSON, geometry, embedded images, other UVs
and every other byte remain intact. The accepted v11 project is never edited.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import tempfile


ROOT = Path(__file__).resolve().parents[1]
PROJECT_BANK = ROOT / 'build/opening-finward-reference'
SOURCE_SHA256 = '3bb95395550683b777976b1cb2ea0283dabc7a383dbfb74249de45c6370aae10'
MATERIAL_NAME = 'Valwood | original household bills'
MODELS = ('home_story.glb', 'home_story_mid.glb')
UV_BEFORE = ((0., 0.), (0., 1.), (.5, 1.), (.5, 0.))
UV_AFTER = ((.5, 1.), (.5, 0.), (0., 0.), (0., 1.))


def _sha(raw):
    return hashlib.sha256(raw).hexdigest()


def _ranges(indices):
    result = []
    for index in indices:
        if result and result[-1]['end_exclusive'] == index:
            result[-1]['end_exclusive'] += 1
        else:
            result.append(dict(start=index, end_exclusive=index+1))
    return result


def _plan(path):
    raw = path.read_bytes()
    if _sha(raw) != SOURCE_SHA256:
        raise ValueError('Expected the exact unmodified v11 original story GLB: '+str(path))
    magic, version, length = struct.unpack_from('<III', raw)
    json_length, json_kind = struct.unpack_from('<II', raw, 12)
    binary_length, binary_kind = struct.unpack_from('<II', raw, 20+json_length)
    binary_start = 28+json_length
    if ((magic, version, length) != (0x46546c67, 2, len(raw)) or
            json_kind != 0x4e4f534a or binary_kind != 0x004e4942 or
            binary_start+binary_length != len(raw)):
        raise ValueError('Expected a bounded JSON + BIN GLB container')
    doc = json.loads(raw[20:20+json_length])
    if len(doc['buffers']) != 1 or 'uri' in doc['buffers'][0]:
        raise ValueError('Expected a single embedded GLB buffer')
    materials = [i for i, item in enumerate(doc['materials'])
                 if item.get('name') == MATERIAL_NAME]
    if len(materials) != 1:
        raise ValueError('Expected one original household-bill material')
    material = materials[0]
    primitives = [(mi, pi, p) for mi, mesh in enumerate(doc['meshes'])
                  for pi, p in enumerate(mesh['primitives'])
                  if p.get('material') == material]
    if len(primitives) != 1:
        raise ValueError('Expected one original household-bill primitive')
    mesh_index, primitive_index, primitive = primitives[0]
    accessor_index = primitive['attributes']['TEXCOORD_0']
    accessor = doc['accessors'][accessor_index]
    if (accessor.get('componentType') != 5126 or accessor.get('type') != 'VEC2' or
            accessor.get('count') != 4 or accessor.get('normalized', False) or
            'sparse' in accessor):
        raise ValueError('Expected four unnormalized float32 UV pairs')
    references = [(mi, pi, semantic) for mi, mesh in enumerate(doc['meshes'])
                  for pi, p in enumerate(mesh['primitives'])
                  for semantic, index in p.get('attributes', {}).items()
                  if index == accessor_index]
    if references != [(mesh_index, primitive_index, 'TEXCOORD_0')]:
        raise ValueError('The bill UV accessor must not be shared')
    view_index = accessor['bufferView']
    view = doc['bufferViews'][view_index]
    if (view.get('buffer', 0) != 0 or view.get('byteStride', 8) != 8 or
            view['byteLength'] != 32 or accessor.get('byteOffset', 0) != 0):
        raise ValueError('Expected a private tightly packed 32-byte UV buffer view')
    begin = binary_start+view.get('byteOffset', 0)
    end = begin+32
    if not binary_start <= begin < end <= binary_start+binary_length:
        raise ValueError('Bill UV range escaped the embedded buffer')
    # No other accessor or embedded image may alias any byte of this view.
    for index, other in enumerate(doc['accessors']):
        if index == accessor_index:
            continue
        other_view = doc['bufferViews'][other['bufferView']]
        low = binary_start+other_view.get('byteOffset', 0)
        high = low+other_view['byteLength']
        if max(begin, low) < min(end, high):
            raise ValueError('Another accessor aliases the bill UV byte range')
    images = []
    for index, image in enumerate(doc.get('images', [])):
        image_view = doc['bufferViews'][image['bufferView']]
        low = binary_start+image_view.get('byteOffset', 0)
        high = low+image_view['byteLength']
        if max(begin, low) < min(end, high):
            raise ValueError('An embedded image aliases the bill UV byte range')
        images.append(dict(index=index, buffer_view=image['bufferView'],
                           bytes=high-low, sha256=_sha(raw[low:high])))
    before_uv = tuple(struct.iter_unpack('<ff', raw[begin:end]))
    if before_uv != UV_BEFORE:
        raise ValueError('Expected the original first atlas tile and vertex order')
    payload = struct.pack('<8f', *(value for uv in UV_AFTER for value in uv))
    patched = raw[:begin]+payload+raw[end:]
    differences = [i for i, (a, b) in enumerate(zip(raw, patched)) if a != b]
    if (not differences or any(not begin <= i < end for i in differences) or
            len(raw) != len(patched) or raw[:begin] != patched[:begin] or
            raw[end:] != patched[end:]):
        raise ValueError('Patch changed bytes outside the original bill UV accessor')
    evidence = dict(
        model=path.name, before_sha256=_sha(raw), after_sha256=_sha(patched),
        file_bytes_before=len(raw), file_bytes_after=len(patched),
        mesh=mesh_index, primitive=primitive_index, material=material,
        material_name=MATERIAL_NAME, accessor=accessor_index, buffer_view=view_index,
        component_type=5126, accessor_type='VEC2', vertices=4,
        rotation_degrees=180, atlas_tile=dict(minimum=[0., 0.], maximum=[.5, 1.]),
        uv_before=[list(uv) for uv in before_uv], uv_after=[list(uv) for uv in UV_AFTER],
        allowed_file_byte_range=dict(start=begin, end_exclusive=end),
        changed_byte_count=len(differences), changed_file_byte_ranges=_ranges(differences),
        unchanged_bytes_outside_uv_sha256=_sha(raw[:begin]+raw[end:]),
        json_chunk_sha256=_sha(raw[20:20+json_length]), embedded_images=images,
        proof=dict(all_changes_inside_bill_uv=True, json_bytes_unchanged=True,
                   positions_normals_indices_unchanged=True, materials_unchanged=True,
                   images_unchanged=True, all_other_uvs_unchanged=True,
                   raw_geometry_bounds_unchanged=True, accessor_counts_unchanged=True,
                   gpu_allocation_sizes_unchanged=True))
    return raw, patched, evidence


def rotate_bill(project):
    """Patch both private story variants and return exact byte-difference proof.

    The target must be a new v12-or-later project under the reference build
    bank. Exact source SHA guards make this deliberately single-use; rerunning
    refuses the already-rotated input rather than turning the notice back.
    """
    project = Path(project).absolute()
    if (project.is_symlink() or not project.is_dir() or
            project.resolve(strict=True) != project or project.name != 'project' or
            project.parent.parent != PROJECT_BANK):
        raise ValueError('Choose a new ordinary private reference build project')
    match = re.fullmatch(r'v(\d+)', project.parent.name)
    if match is None or int(match[1]) < 12:
        raise ValueError('Accepted v11 and earlier projects cannot be patched')
    bank = project/'models/opening'
    if bank.is_symlink() or not bank.is_dir() or bank.resolve(strict=True) != bank:
        raise ValueError('Opening model bank must be private and contain no directory links')
    plans = []
    for name in MODELS:
        path = bank/name
        if path.is_symlink() or not path.is_file():
            raise ValueError('Story variants must be ordinary private files: '+str(path))
        plans.append((path, *_plan(path)))
    staged = []
    published = []
    try:
        # Validate and stage both variants before publishing either one. Rename
        # replaces any hardlink inode, so an older build can never be modified.
        for path, before, after, evidence in plans:
            if path.read_bytes() != before:
                raise ValueError('Story GLB changed while planning: '+str(path))
            with tempfile.NamedTemporaryFile(prefix='.'+path.name+'-', dir=bank,
                                             delete=False) as stream:
                temporary = Path(stream.name)
                staged.append(temporary)
                stream.write(after)
                stream.flush()
                os.fsync(stream.fileno())
            temporary.chmod(path.stat().st_mode & 0o777)
        for (path, before, after, evidence), temporary in zip(plans, staged):
            if path.read_bytes() != before:
                raise ValueError('Story GLB changed before publication: '+str(path))
            os.replace(temporary, path)
            published.append((path, before))
            if path.read_bytes() != after:
                raise ValueError('Story GLB did not match planned publication: '+str(path))
    except BaseException:
        for path, before in reversed(published):
            with tempfile.NamedTemporaryFile(prefix='.'+path.name+'-restore-', dir=bank,
                                             delete=False) as stream:
                restore = Path(stream.name)
                staged.append(restore)
                stream.write(before)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(restore, path)
        raise
    finally:
        for temporary in staged:
            temporary.unlink(missing_ok=True)
    return dict(schema='valwood.opening-cinematic-bill-uv', version=1, passed=True,
                project=str(project), operation='Rotate only original bill UVs inside first atlas tile',
                models=[entry[3] for entry in plans],
                scope='Exact GLB byte-range proof; native screenshot review remains required.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(rotate_bill(args.project), indent=2))


if __name__ == '__main__':
    main()

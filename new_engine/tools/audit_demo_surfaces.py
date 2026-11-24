#!/usr/bin/env python3
"""Read-only source/LOD/material audit. Never cooks or changes purchased files."""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image
from build_purchased_village import glb
from validate_purchased_village_fidelity import array

ROOT = Path(__file__).resolve().parents[1]


def audit(path):
    doc, binary = glb(path)
    images = []
    for image in doc.get('images', []):
        view = doc['bufferViews'][image['bufferView']]
        start = view.get('byteOffset', 0)
        encoded = binary[start:start + view['byteLength']]
        with Image.open(io.BytesIO(encoded)) as im:
            w, h = im.size
        images.append(dict(name=image.get('name', ''), width=w, height=h,
                           sha256=hashlib.sha256(encoded).hexdigest(),
                           rgba8_mips_bytes=sum(max(1, w >> m) * max(1, h >> m) * 4
                                                for m in range(int(math.log2(max(w, h))) + 1))))
    records = []
    for mesh in doc.get('meshes', []):
        for primitive in mesh.get('primitives', []):
            if primitive.get('mode', 4) != 4:
                continue
            m = doc.get('materials', [])[primitive['material']]
            xyz = array(doc, binary, primitive['attributes']['POSITION'], 3)
            indices = array(doc, binary, primitive['indices'], 1).reshape(-1, 3)
            triangles = xyz[indices].astype(np.float64)
            areas = np.linalg.norm(np.cross(triangles[:, 1] - triangles[:, 0],
                                            triangles[:, 2] - triangles[:, 0]), axis=1) * .5
            row = dict(material=m.get('name', ''), triangles=len(indices),
                       area_m2=float(areas.sum()), alpha=m.get('alphaMode', 'OPAQUE'), maps={})
            pbr = m.get('pbrMetallicRoughness', {})
            slots = {'color_srgb': pbr.get('baseColorTexture'),
                     'normal_linear': m.get('normalTexture'),
                     'arm_linear': pbr.get('metallicRoughnessTexture'),
                     'emission_srgb': m.get('emissiveTexture')}
            for role, texture in slots.items():
                if texture:
                    image = doc['textures'][texture['index']]['source']
                    row['maps'][role] = dict(image=image, dimensions=[images[image]['width'], images[image]['height']],
                                             texcoord=texture.get('texCoord', 0))
            if 'TEXCOORD_0' in primitive['attributes'] and 'color_srgb' in row['maps']:
                uv = array(doc, binary, primitive['attributes']['TEXCOORD_0'], 2)[indices].astype(np.float64)
                u, v = uv[:, 1] - uv[:, 0], uv[:, 2] - uv[:, 0]
                uv_area = np.abs(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]) * .5
                tiling = m.get('extensions', {}).get('VALWOOD_materials_facade', {}).get('parameters', [1]*8)[5]
                w, h = row['maps']['color_srgb']['dimensions']
                row['albedo_texels_per_m_area_rms'] = math.sqrt(float(uv_area[areas > 1e-10].sum()) * w * h * tiling**2 / max(1e-10, float(areas.sum())))
            records.append(row)
    return dict(path=str(path.relative_to(ROOT)), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                images=images, primitives=records,
                density_note='Mesh-local metres before instance scale; rendered texels/m divide by instance scale. Base image UV0 including facade primary tiling.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    models = ROOT/'projects/valwood_world/models'
    names = [f'purchased_village_house_{h}_{lod}.glb' for h in 'abcdef' for lod in ('near', 'mid')]
    names += [f'purchased_village_{h}_{lod}.glb' for h in ('castle', 'wall') for lod in ('near', 'mid')]
    names += ['village_tavern_near.glb', 'village_tavern_mid.glb', 'bep_nature_fern_near.glb', 'bep_nature_fern_mid.glb']
    rows = [audit(models/name) for name in names if (models/name).exists()]
    unique = {image['sha256']: image for row in rows for image in row['images']}
    correction = ROOT/'assets/purchased/medieval_village/source-facades/channel-correction-validation.json'
    report = dict(schema=1, read_only=True, records=rows,
                  unique_images=len(unique), unique_decoded_rgba8_mips_mib=sum(im['rgba8_mips_bytes'] for im in unique.values())/1024**2,
                  source_channel_history=json.loads(correction.read_text()),
                  limitations=['Texel density is a geometry/material measurement, not a screenshot quality score.',
                               'No lighting/exposure or compression changes are applied by this audit.',
                               'Historical mid plaster normal/ARM channel blend requires a geometry-preserving material recook.'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(f'Audited {len(rows)} models, {len(unique)} unique images; no source/model writes')
    for row in rows:
        if '_near' in row['path']:
            print(Path(row['path']).name, ', '.join(f"{m['material']}: {m.get('albedo_texels_per_m_area_rms', 0):.1f} texels/m" for m in row['primitives']))


if __name__ == '__main__':
    main()

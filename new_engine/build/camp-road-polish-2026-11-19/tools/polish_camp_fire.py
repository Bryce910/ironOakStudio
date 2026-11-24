"""Stage a restrained hearth using existing purchased meshes and flame sheets."""
from pathlib import Path
import copy, hashlib, json, struct

N = Path(__file__).resolve().parents[1]
P = N / 'world-project'
OUT = N / 'assets/camp-road/fire'
REPORT = N / 'review/fire'

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def read(p):
    raw = p.read_bytes()
    length, kind = struct.unpack_from('<II', raw, 12)
    assert kind == 0x4e4f534a
    return json.loads(raw[20:20+length]), raw[20+length:]

def write(p, doc, binary_chunk):
    data = json.dumps(doc, separators=(',', ':')).encode()
    data += b' ' * (-len(data) % 4)
    p.write_bytes(struct.pack('<III', 0x46546c67, 2, 20 + len(data) + len(binary_chunk)) +
                  struct.pack('<II', len(data), 0x4e4f534a) + data + binary_chunk)

def main():
    assert not (N / 'UNIFIED_READY.json').exists()
    OUT.mkdir(parents=True, exist_ok=True)
    REPORT.mkdir(parents=True, exist_ok=True)
    models = []
    for kind in ('camp', 'roast'):
        for lod in ('near', 'mid'):
            source = P / 'models/opening' / f'arrival_{kind}_{lod}.glb'
            before, binary = read(source)
            doc = copy.deepcopy(before)
            changes = []
            for material in doc['materials']:
                name = material.get('name', '')
                original = copy.deepcopy(material)
                if kind == 'roast':
                    material['pbrMetallicRoughness'].update(baseColorFactor=[.78, .64, .49, 1], roughnessFactor=.72)
                elif name == 'starting_firepit worn material':
                    material['pbrMetallicRoughness']['baseColorFactor'] = [.72, .70, .66, 1]
                elif name == 'Low live coals':
                    material['emissiveFactor'] = [.46, .054, .006]
                if material != original:
                    changes.append(dict(name=name, before=original, after=copy.deepcopy(material)))
            assert len(changes) == (1 if kind == 'roast' else 2)
            stem = 'camp_hearth_polish_20261119' if kind == 'camp' else 'camp_roast_polish_20261119'
            dest = OUT / f'{stem}_{lod}.glb'
            assert not dest.exists(), 'Retain previous asset and use a fresh iteration'
            write(dest, doc, binary)
            after, payload = read(dest)
            assert after == doc and payload == binary
            # Every geometry, transform, image and texture binding stays exact.
            restored = copy.deepcopy(after)
            restored['materials'] = before['materials']
            assert restored == before
            alias = P / 'models/opening' / dest.name
            assert not alias.exists() and not alias.is_symlink()
            alias.symlink_to(dest)
            models.append(dict(source=str(source), source_sha256=sha(source), asset=str(dest),
                               project_alias=str(alias.relative_to(P)), after_sha256=sha(dest),
                               material_changes=changes, binary_geometry_and_images_exact=True))
    flames = P / 'showcase.flames'
    original = flames.read_bytes()
    rows = original.decode().splitlines()
    assert len(rows) == 2 and rows[0] == 'VALWOOD_SOURCE_FLAMES 1'
    fields = rows[1].split()
    assert abs(float(fields[0]) + 11988) < .001 and abs(float(fields[2]) - 5172) < .001
    fields[4], fields[5] = '.36', '.60'
    updated = (rows[0] + '\n' + ' '.join(fields) + '\n').encode()
    (REPORT / 'showcase.flames.before').write_bytes(original)
    flames.write_bytes(updated)
    result = dict(passed=True, native_visual_review_required=True, models=models,
                  flame_change=dict(relative='showcase.flames', before_sha256=hashlib.sha256(original).hexdigest(),
                                    after_sha256=sha(flames), before=rows[1], after=' '.join(fields)),
                  shader_unchanged=True, geometry_collision_and_texture_payloads_unchanged=True,
                  intention='Separate a compact orange flame from darker fuel, dim glowing coals and warmer roast material; retain purchased animated fire frames.')
    (REPORT / 'FIRE_ASSETS_REVIEW.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(passed=True, models=len(models), flame_width=.36, flame_intensity=.60)))

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Source-preserving, zero-purchase finish for the selected courtyard/path."""
from pathlib import Path
import copy, hashlib, io, json, math, random, sys, tempfile
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_visual_upgrade_assets as assets
from compose_visual_upgrade import line, TRAIL
from build_starting_area import Atlas, segment_distance

PROJECT = ROOT / 'projects/valwood_visual'
OUT = PROJECT / 'models/environment_finish'
REPORT = ROOT / 'docs/environment-finish/2026-10-30'
WOOD = ROOT / 'assets/polyhaven/castle_village/brown_planks_08'
STONE = ROOT / 'assets/polyhaven/milestone/cobblestone_floor_08/textures'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def maps():
    cache = OUT / 'maps'
    cache.mkdir(parents=True, exist_ok=True)
    result = {}
    for key, path in {
        'wood_base': WOOD/'diff.jpg', 'wood_normal': WOOD/'nor_gl.jpg',
        'wood_arm': WOOD/'arm.jpg',
        'stone_base': STONE/'cobblestone_floor_08_diff_1k.jpg',
        'stone_normal': STONE/'cobblestone_floor_08_nor_gl_1k.jpg',
        'stone_arm': STONE/'cobblestone_floor_08_arm_2k.jpg',
    }.items():
        image = Image.open(path).convert('RGB')
        # Shared 1K color, 512px linear maps; no 4K texture expansion.
        limit = 1024 if key.endswith('base') else 512
        image.thumbnail((limit, limit), Image.Resampling.LANCZOS)
        if key.endswith('normal'):
            values = np.asarray(image, dtype=np.float32)/127.5-1
            values /= np.maximum(np.linalg.norm(values, axis=2, keepdims=True), 1e-6)
            image = Image.fromarray(np.uint8(np.rint(np.clip(values*.5+.5, 0, 1)*255)))
        target = cache / (key+'.png')
        image.save(target)
        result[key] = target
    return result


def finish_model(source, target, selected, material_maps):
    doc, blob, raw = assets.read_glb(source)
    doc = copy.deepcopy(doc)
    data = bytearray(blob)
    image_ids = {}

    def texture(key):
        if key not in image_ids:
            encoded = material_maps[key].read_bytes()
            data.extend(b'\0'*(-len(data) % 4))
            view = len(doc['bufferViews'])
            doc['bufferViews'].append(dict(buffer=0, byteOffset=len(data), byteLength=len(encoded)))
            data.extend(encoded)
            image_id = len(doc.setdefault('images', []))
            doc['images'].append(dict(bufferView=view, mimeType='image/png'))
            texture_id = len(doc.setdefault('textures', []))
            sampler = len(doc.setdefault('samplers', []))
            doc['samplers'].append(dict(magFilter=9729, minFilter=9987, wrapS=10497, wrapT=10497))
            doc['textures'].append(dict(source=image_id, sampler=sampler))
            image_ids[key] = texture_id
        return dict(index=image_ids[key])

    for material in doc['materials']:
        kind = selected.get(material.get('name'))
        if not kind:
            continue
        pbr = material.setdefault('pbrMetallicRoughness', {})
        pbr.update(baseColorFactor=[.78, .76, .69, 1] if kind=='wood' else [.88, .89, .84, 1],
                   roughnessFactor=.92, metallicFactor=0,
                   baseColorTexture=texture(kind+'_base'),
                   metallicRoughnessTexture=texture(kind+'_arm'))
        material['normalTexture'] = dict(**texture(kind+'_normal'), scale=.75)
        material['occlusionTexture'] = dict(**texture(kind+'_arm'), strength=.8)
        material['name'] += ' | CC0 complete PBR'
    # Drop superseded texture pages instead of carrying an unused second set.
    texture_refs=[]
    def references(node):
        if isinstance(node,dict):
            for key,value in node.items():
                if key.endswith('Texture') and isinstance(value,dict) and 'index' in value:
                    texture_refs.append(value)
                else: references(value)
        elif isinstance(node,list):
            for value in node: references(value)
    references(doc['materials'])
    used_textures=sorted({v['index'] for v in texture_refs})
    texture_mapping={old:i for i,old in enumerate(used_textures)}
    for value in texture_refs:value['index']=texture_mapping[value['index']]
    doc['textures']=[doc['textures'][i] for i in used_textures]
    used_images=sorted({v['source'] for v in doc['textures']})
    image_mapping={old:i for i,old in enumerate(used_images)}
    old_image_views={v['bufferView'] for v in doc['images']}
    for value in doc['textures']:value['source']=image_mapping[value['source']]
    doc['images']=[doc['images'][i] for i in used_images]
    retained_image_views={v['bufferView'] for v in doc['images']}
    # Original mesh, skin, UV, index and animation views remain byte-identical.
    assert data[:len(blob)] == blob
    packed=bytearray();views=[];mapping={}
    for i,view in enumerate(doc['bufferViews']):
        if i in old_image_views and i not in retained_image_views:continue
        item=copy.deepcopy(view);item['byteOffset']=len(packed)
        packed.extend(data[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']])
        packed.extend(b'\0'*(-len(packed)%4));mapping[i]=len(views);views.append(item)
    for accessor in doc.get('accessors',[]):
        if 'bufferView' in accessor:accessor['bufferView']=mapping[accessor['bufferView']]
    for image in doc['images']:image['bufferView']=mapping[image['bufferView']]
    doc['bufferViews']=views
    doc.setdefault('extras', {})['environment_finish_source_sha256'] = hashlib.sha256(raw).hexdigest()
    assets.save_glb(target, doc, packed)
    return dict(path=str(target.relative_to(PROJECT)), source=str(source.relative_to(ROOT)),
                sha256=digest(target), geometry_unchanged=True)


def geometry(material_maps):
    assets.OUT = OUT
    records = []
    # Narrow drainage rills ground the wall without blocking the training court.
    model = assets.Model()
    stone = model.material('CC0 rough drainage stone', [.58, .60, .54], .96,
        maps={k:material_maps['stone_'+v] for k,v in [('base','base'),('normal','normal'),('arm','arm')]})
    soil = model.material('Earth retained in drainage rill', [.055,.043,.028], .98)
    model.box(soil, (-5.9,-.012,-.21), (5.9,.008,.21))
    rng = random.Random(302603)
    for i in range(24):
        x = -6+i*.5
        for side in [-1,1]:
            model.box(stone, (x+.014,.005,side*.28-.09),
                      (x+.485,.05+rng.random()*.025,side*.28+.09))
    for i in range(30):
        x,z=rng.uniform(-5.8,5.8),rng.uniform(-.17,.17)
        model.box(stone,(x,.005,z),(x+rng.uniform(.035,.11),.02+rng.random()*.025,z+.04))
    records.append(model.save('court_drain'))
    # Small branch/stone groups add ground-scale silhouettes, using opaque
    # geometry instead of floating rectangular leaf cards.
    model = assets.Model()
    wood = model.material('CC0 fallen weathered wood', [.32,.29,.23], .98,
        maps={k:material_maps['wood_'+v] for k,v in [('base','base'),('normal','normal'),('arm','arm')]})
    rock = model.material('CC0 forest pebbles', [.43,.47,.39], .97,
        maps={k:material_maps['stone_'+v] for k,v in [('base','base'),('normal','normal'),('arm','arm')]})
    for i in range(4):
        x,z=rng.uniform(-.75,.75),rng.uniform(-.42,.42)
        model.box(wood,(x,.012,z),(x+rng.uniform(.32,.72),.035+rng.random()*.02,z+.035),
                  yaw=rng.uniform(-1.2,1.2),pivot=(x,0,z))
    for i in range(6):
        x,z=rng.uniform(-.85,.85),rng.uniform(-.5,.5)
        r=rng.uniform(.025,.075)
        # Flattened irregular octahedron: the footprint never becomes a cube.
        p=[(x-r,0,z),(x,0,z-r*.8),(x+r*1.2,0,z),(x,0,z+r),(x,.06+r*.4,z)]
        for j in range(4):
            a,b,c=p[j],p[(j+1)%4],p[4]
            normal=np.cross(np.asarray(b)-a,np.asarray(c)-a);normal/=np.linalg.norm(normal)
            group=model.parts.setdefault(rock,[[],[],[],[]]);offset=len(group[0])
            group[0].extend([a,b,c]);group[1].extend([normal]*3)
            group[2].extend([(0,0),(1,0),(.5,1)]);group[3].append((offset,offset+1,offset+2))
    records.append(model.save('forest_debris'))
    return records


def compose():
    document = PROJECT/'world.studio'
    # Replace only this recipe's records; later authored quarters must survive.
    prefixes=('asset finish_drain ', 'asset finish_debris ', 'material milestone_leaf ', 'material milestone_canopy ', '# Environment material finish')
    text = '\n'.join(l for l in document.read_text().splitlines() if not l.startswith(prefixes)).rstrip()
    text = text.replace('asset milestone_bed visual_milestone/court_bed ',
                        'asset milestone_bed environment_finish/court_bed ')
    for name in ['open','glazed','slit']:
        text=text.replace('visual_upgrade/window_'+name+' ', 'environment_finish/window_'+name+' ')
    text += '\n'
    text += 'asset finish_drain environment_finish/court_drain 1 0 0 100 40 1\n'
    text += 'asset finish_debris environment_finish/forest_debris 1 0 0 35 15 1\n'
    # Keep consistent material tint on the photographed broad-leaf plants.
    text += 'material milestone_leaf .94 1.0 .90 1.04 .92 1\n'
    text += 'material milestone_canopy .98 1.0 .95 1.03 .96 1\n'
    document.write_text(text)
    placed=[]
    for x in [-1362,-1350,-1338,-1326]:
        placed.append(dict(alias='finish_drain', x=x,z=-10263.85,scale=1,yaw=0,
                           burial=0,elevation=0,zone='wall drainage'))
    with tempfile.TemporaryDirectory(prefix='valwood-material-author-') as temp:
        atlas=Atlas(temp)
        for i,(x,z) in enumerate([(-26734,9731),(-26730,9739),(-26728,9728),
              (-26722,9730),(-26720,9722),(-26724,9718),(-26716,9711),(-26714,9704)]):
            gap=min(segment_distance((x,z),a,b) for a,b in zip(TRAIL,TRAIL[1:]))
            if gap<=2.0:
                continue
            samples=[atlas.sample(x+dx,z+dz) for dx,dz in [(0,0),(-.8,0),(.8,0),(0,-.5),(0,.5)]]
            if max(s.height for s in samples)-min(s.height for s in samples)>.23:
                continue
            placed.append(dict(alias='finish_debris',x=x,z=z,scale=.8,yaw=(i*53+180)%360-180,
                               burial=.008,elevation=0,zone='path edge debris'))
    scenery=PROJECT/'scenery/demo_completion_instances.txt'
    before, marker, after=scenery.read_text().partition('# Environment material finish')
    next_layer=after.find('\n# ') if marker else -1
    retained=after[next_layer:].lstrip('\n') if next_layer>=0 else ''
    scenery.write_text(before.rstrip()+'\n# Environment material finish\n'+'\n'.join(line(p) for p in placed)+'\n'+retained)
    return placed


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    REPORT.mkdir(parents=True, exist_ok=True)
    material_maps=maps()
    records=[]
    for name in ['open','glazed','slit']:
        records.append(finish_model(PROJECT/'models/visual_upgrade'/('window_'+name+'.glb'),
            OUT/('window_'+name+'.glb'), {'CC0 brown_planks_08':'wood',
            'Existing scanned village stone':'stone'},material_maps))
    records.append(finish_model(PROJECT/'models/visual_milestone/court_bed.glb',OUT/'court_bed.glb',
        {'Weathered limestone bed edge':'stone'},material_maps))
    records.append(finish_model(PROJECT/'models/visual_milestone/watch_banner.glb',OUT/'watch_banner.glb',
        {'Weathered banner pole':'wood'},material_maps))
    proxy=PROJECT/'models/village_banner.glb'
    if proxy.is_symlink():proxy.unlink()
    assert not proxy.exists(), 'Keep independently edited models intact'
    proxy.symlink_to(OUT/'watch_banner.glb')
    geometry_records=geometry(material_maps)
    placed=compose()
    result=dict(spend_usd=0,source_assets_unchanged=True,models=records,
        detail_models=geometry_records,placements=placed,
        textures={str(p.relative_to(PROJECT)):digest(p) for p in material_maps.values()},
        sources=['https://polyhaven.com/a/brown_planks_08',
                 'https://polyhaven.com/a/cobblestone_floor_08',
                 'https://polyhaven.com/a/forest_leaves_03'],
        license='https://polyhaven.com/license')
    (REPORT/'assets.json').write_text(json.dumps(result,indent=2)+'\n')
    (PROJECT/'assets/environment_finish').mkdir(exist_ok=True)
    (PROJECT/'assets/environment_finish/provenance.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Finished',len(records),'PBR derivatives;',len(placed),'small ground details; $0',flush=True)


if __name__=='__main__':main()

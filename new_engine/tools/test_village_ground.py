#!/usr/bin/env python3
"""Independent material-history semantics and real-section placement checks."""
import argparse,copy,hashlib,json,math
from pathlib import Path
import numpy as np
from PIL import Image
from build_village_ground import ROOT,PROJECT,SOURCE,HERO_SOURCE,MANIFEST,OUTPUT,section_segments,authored_market_history

parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--input-dir',type=Path,help='Check staged mask/proof without publishing them');args=parser.parse_args()
manifest=args.input_dir/MANIFEST.name if args.input_dir else MANIFEST
output=args.input_dir/OUTPUT.name if args.input_dir else OUTPUT
config=json.loads(SOURCE.read_text());proof=json.loads(manifest.read_text())
pixels=np.asarray(Image.open(output).convert('RGBA'));assert pixels.shape==(2049,2049,4)
assert hashlib.sha256(pixels.tobytes()).hexdigest()==proof['pixel_sha256']
assert pixels.nbytes==2049*2049*4 and proof['gpu_mip_bytes']<22*1048576
assert not np.any(pixels[[0,-1]]) and not np.any(pixels[:,[0,-1]])
def sample(x,z):
    xmin,zmin,xmax,zmax=config['bounds_local_xz'];u=(x-xmin)/(xmax-xmin)*(config['side']-1);v=(z-zmin)/(zmax-zmin)*(config['side']-1)
    ix,iz=int(u),int(v);tx,tz=u-ix,v-iz
    return ((pixels[iz,ix].astype(float)*(1-tx)+pixels[iz,ix+1]*tx)*(1-tz)+(pixels[iz+1,ix].astype(float)*(1-tx)+pixels[iz+1,ix+1]*tx)*tz)/255

# Independent authored locations: traffic remains clean away from the path;
# the well's wet working area is not a global wetness/noise overlay.
assert sample(17,0)[1]>.7 and sample(15,0)[1]<.01
assert sample(18.7,39.2)[2]>.65 and sample(24,41)[2]<.01
assert sample(39.6,4)[3]>.15
# The gate's open passage has no false whole-bounds contact strip.
assert sample(0,-55)[0]<.02 and sample(5.5,-55)[0]>.35
sections={r['label']:r for r in proof['grounded_sections']}
assert 'dress_cooper_stack_0' not in sections and 'lane_entry_delivery_crate_0' in sections
assert proof['skipped_elevated_contacts']>=8
scene=json.loads((PROJECT/'scenery/castle_village_manifest.json').read_text())
record=next(r for r in scene['placements'] if r['label']=='lane_west_merchant')
model=scene['models'][record['alias']]['bounds_y_up'];section=sections['lane_west_merchant']['section_bounds_local_xz']
assert section[2]-section[0]<(model[1][0]-model[0][0])*record['scale']*.85
assert all(r['world_height_m']==.1 and r['section_segments']>0 for r in sections.values())

# The hero street has separate foot wear, wheel dirt, damp drainage and broken
# paving-edge deposits. This prevents a uniform burnish overlay from passing.
assert sample(-70,54.3)[1]>.7 and sample(-70,53)[1]<.2
assert sample(-65,55.3)[0]>.25 and sample(-65,55.3)[2]>.15
assert sample(-70,52.35)[2]>.5 and sample(-70,57.7)[2]>.45
assert sample(-75,52.1)[3]>.5 and sample(-75,54.3)[3]<.05
hero=proof['hero_market'];recipe=json.loads(HERO_SOURCE.read_text())
assert hero['recipe_sha256']==hashlib.sha256(HERO_SOURCE.read_bytes()).hexdigest()
assert hero['outside_changed_pixels']==0 and 4000<hero['changed_pixels']<10000
assert all(n>1000 for n in hero['channel_changed_pixels'])
assert hero['feature_counts']=={'footways':4,'wheel_runs':2,'gutters':4,'edge_deposits':6}
assert hero['new_samplers']==hero['new_gpu_bytes']==0 and hero['cpu_height_changed'] is False
expected_approaches=[]
for section in sections.values():
    house=section['type']=='foundation' and section['label'].startswith('demo_home_market_row_')
    shop=section['alias'].startswith('purchased_shop_')
    sx0,sz0,sx1,sz1=section['section_bounds_local_xz'];cx=(sx0+sx1)*.5
    face_z=sz1 if (sz0+sz1)*.5<55 else sz0
    if (house or shop) and -82<cx<-38 and 47.5<face_z<62.5:expected_approaches.append(section['label'])
assert len(expected_approaches)>=6
assert {s['label'] for s in hero['frontage_approaches']}==set(expected_approaches)
for approach in hero['frontage_approaches']:
    section=sections[approach['label']];sx0,sz0,sx1,sz1=section['section_bounds_local_xz']
    px,pz=approach['points'][0]
    assert abs(px-(sx0+sx1)*.5)<1e-6
    assert abs(pz-(sz1 if (sz0+sz1)*.5<55 else sz0))<1e-6

# A separate synthetic field proves locality and preservation of existing
# stronger soil contacts without depending on the published image's hash.
tx,tz=np.meshgrid(np.linspace(-85,-35,401,dtype=np.float32),np.linspace(44,66,177,dtype=np.float32))
original=[np.full_like(tx,v) for v in [.63,.42,.13,.07]]
modified,_=authored_market_history(original,tx,tz,[],recipe)
outside=(tx<=-82)|(tx>=-38)|(tz<=47.5)|(tz>=62.5)
assert all(np.array_equal(before[outside],after[outside]) for before,after in zip(original,modified))
assert np.all(modified[0]>=original[0])
assert all(np.isfinite(channel).all() for channel in modified)
for mutation in [lambda r:r.update(bounds_local_xz=[-90,40,-30,70]),lambda r:r['gutters'][0].update(width=.01),lambda r:r.update(border_fade_m=0)]:
    invalid=copy.deepcopy(recipe);mutation(invalid)
    try:authored_market_history(original,tx,tz,[],invalid)
    except AssertionError:pass
    else:raise AssertionError('Invalid material-history feature accepted')

def box(lo,hi):
    vertices=[(x,y,z) for x in [lo[0],hi[0]] for y in [lo[1],hi[1]] for z in [lo[2],hi[2]]]
    faces=[(0,1,3),(0,3,2),(4,6,7),(4,7,5),(0,4,5),(0,5,1),(2,3,7),(2,7,6),(0,2,6),(0,6,4),(1,5,7),(1,7,3)]
    return [[vertices[i] for i in face] for face in faces]
# Analytic gate model: disconnected piers plus a very wide elevated beam.
# Ground sections preserve the gap and entirely exclude the overhang.
geometry=box((-3,0,-1),(-2,3,1))+box((2,0,-1),(3,3,1))+box((-7,4,-2),(7,5,2))
segments=section_segments(geometry,.1)
assert len(segments)==16
assert all(abs(p[0])>=2 and abs(p[0])<=3 for segment in segments for p in segment)
assert section_segments(geometry,7)==[]
print('PASS village ground: source low-height sections exclude eaves/elevated cargo and retain gate gaps; district traffic/well/drainage/edge channels; bounded hero wheel/foot/drainage/edge wear +',len(expected_approaches),'real-section house/shop frontage approaches; zero changes outside hero region;2049-square field/<22MiB mips; deterministic field')

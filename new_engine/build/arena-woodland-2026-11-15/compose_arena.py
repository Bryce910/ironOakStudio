#!/usr/bin/env python3
"""Owned-asset arena dressing. Exact combat floor and source assets stay intact."""
from pathlib import Path
import shutil,math,random,struct,json,hashlib,ctypes as C
N=Path(__file__).resolve().parent;E=N.parents[1];BASE=E/'build/arena-material-quality-2026-11-15';P=N/'arena-project'
if not P.exists():
 P.mkdir()
 for s in (BASE/'arena-project').iterdir():
  t=P/s.name
  if s.name in ('saves','studio-layout.ini','world.studio.workspace'):continue
  if s.name in ('scenery','ground','environments','shaders'):shutil.copytree(s,t)
  elif s.is_symlink() or s.is_dir():t.symlink_to(s.resolve(),target_is_directory=s.is_dir())
  else:shutil.copy2(s,t)
 (N/'runtime-native-v1').symlink_to(BASE/'runtime-native-v1',target_is_directory=True)
 (N/'previous-launchers').mkdir()
 for name in ('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):shutil.copy2(E.parent/name,N/'previous-launchers'/name)
from PIL import Image
Image.open(P/'ground/terrain_sand.png').convert('RGB').resize((1024,1024),Image.Resampling.LANCZOS).save(P/'scenery/arena_floor.bmp')
atlas=C.CDLL(str(N/'review/libarena_atlas.so'));atlas.world_atlas_init.restype=C.c_bool;assert atlas.world_atlas_init();atlas.world_atlas_height.argtypes=[C.c_double,C.c_double];atlas.world_atlas_height.restype=C.c_float
r=random.Random(65183);rows=[];records=[]
def terrain(x,z):
 d=max(abs(x),abs(z))
 # Seam-free and exactly flat around the fight floor, seating and entry.
 fade=max(0,min(1,(d-26)/16));fade=fade*fade*(3-2*fade)
 road=1-math.exp(-((x/4.2)**4)) if z<0 else 1
 wave=2.5+1.8*math.sin(x*.054+1.2)*math.cos(z*.063)+1.5*math.sin(z*.043-.6)
 inner=64+fade*road*wave
 blend=max(0,min(1,(d-88)/424));blend=blend*blend*(3-2*blend)
 return inner if not blend else inner*(1-blend)+atlas.world_atlas_height(x-1800,z-10500)*blend
side=513
values=[terrain(x,z) for z in range(-512,513,2) for x in range(-512,513,2)]
(P/'ground/showcase.height').write_bytes(struct.pack('<8sIIdddd',b'VWHGT1\0\0',side,side,-2312.,-11012.,2.,2.)+struct.pack('<'+'f'*len(values),*values))
def put(a,x,z,scale=1,yaw=None,y=None,sx=1,sy=1,sz=1,wind=None):
 y=terrain(x,z) if y is None else y;yaw=r.uniform(-180,180)if yaw is None else yaw
 row=f'{a} {(x-1800)/100800+.5:.16g} {(z-10500)/(100800*2818/5009)+.5:.16g} {yaw:.6g} {scale:.6g} 0 0 0 0 0 {sx:.6g} {sy:.6g} {sz:.6g} {y:.7g}'
 if wind:row+=f' wind {wind[0]} {wind[1]}'
 assert len(row)<=256
 rows.append(row);records.append(dict(alias=a,x=x,z=z,y=y,scale=scale,yaw=yaw,sx=sx,sy=sy,sz=sz))
# Near framing trees are spaced and the south entrance is a clear lane.
roots=[(-22,-15),(-25,5),(-19,28),(23,-9),(26,13),(18,34),(-33,-29),(32,-32)]
for i,(x,z)in enumerate(roots):put('bep_nature_oak'if i%2==0 else'bep_nature_beech',x,z,r.uniform(.72,.98),wind=(.65,.7))
# Uneven clusters avoid both a rigid ring and a solid leaf wall at the boundary.
for sector in range(40):
 angle=sector*math.tau/40+r.uniform(-.09,.09);rad=r.uniform(29,90)
 for j in range(r.randint(4,6)):
  x=math.sin(angle)*rad+r.uniform(-7,7);z=math.cos(angle)*rad+r.uniform(-7,7)
  if z<0 and abs(x)<8:continue
  if any(math.hypot(x-a,z-b)<4.0 for a,b in roots):continue
  roots.append((x,z));put('bep_nature_beech'if r.random()<.4 else'bep_nature_oak',x,z,r.uniform(.58,1.0),wind=(.65,.7))
# Ground detail is restricted to outside the ring and the entry/viewing apron.
for i in range(4500):
 x=r.uniform(-65,65);z=r.uniform(-65,65)
 if max(abs(x),abs(z))<24 or (z<0 and abs(x)<4.5):continue
 patch=.5+.5*math.sin(x*.23)*math.cos(z*.19)
 if r.random()>patch*.95:continue
 a=r.choices(['bep_nature_grass_long','bep_nature_grass_broad','bep_nature_fern'],[6,3,1])[0]
 put(a,x,z,r.uniform(.4,.8),wind=(.8,.7))
for x,z in roots[:70]:
 for i in range(5):
  a=x+r.uniform(-3.8,3.8);b=z+r.uniform(-3.8,3.8)
  if max(abs(a),abs(b))<23 or (b<0 and abs(a)<5):continue
  put('bep_nature_bush_a'if i%2 else'bep_nature_bush_b',a,b,r.uniform(.7,1.12),wind=(.5,.7))
# Dress the near apron as well as the wider woodland; leave working lanes clear.
def apron_clear(x,z):
 if max(abs(x),abs(z))<13.4:return False
 if z< -12 and abs(x)<6.3:return False
 if -11.5<x<11.5 and 12<z<20.5:return False
 if -24.8<x< -15.2 and -6.5<z<6.5:return False
 if 15.2<x<24.8 and -6.5<z<5:return False
 if -21.5<x< -16.2 and 6<z<14.5:return False
 if 19<x<27 and -21<z< -13:return False
 return True
for ix in range(-23,24):
 for iz in range(-23,24):
  x=ix+r.uniform(-.35,.35);z=iz+r.uniform(-.35,.35)
  if not apron_clear(x,z)or r.random()>.6:continue
  put(r.choice(['bep_nature_grass_long','bep_nature_grass_broad','bep_nature_fern']),x,z,r.uniform(.55,.9),wind=(.8,.7))
for x,z in[(-15.5,-8),(-15.5,8),(15.5,-8),(15.5,8),(-12,22),(12,22),(-18,21),(18,21),(-11,-16),(11,-16)]:
 for j in range(3):
  a=x+r.uniform(-1.2,1.2);b=z+r.uniform(-1.2,1.2)
  if apron_clear(a,b):put('showcase_shrub'if j%2 else'showcase_shrub_b',a,b,r.uniform(.9,1.2),wind=(.5,.7))

# Low grasses and wildflowers soften the retaining wall without entering it.
for side_sign in(-1,1):
 for i in range(40):
  x=side_sign*r.uniform(12.8,15);z=r.uniform(-11.5,11.5)
  put('bep_nature_grass_long',x,z,r.uniform(.25,.48),wind=(.8,.6))
 for x,z in [(side_sign*26,-21),(side_sign*30,18),(side_sign*22,30)]:
  for i in range(3):put('rock0'if i%2 else'rock1',x+r.uniform(-2,2),z+r.uniform(-2,2),r.uniform(.75,1.5),y=terrain(x,z)-.2)
# Northern timber spectator terrace: three shallow tiers, visible from combat.
base=(BASE/'arena-project/world.studio').read_text().replace('structure_ids 8','structure_ids 11')
extra=[]
for tier in range(3):
 z=14+2.25*tier;h=64.45+.6*tier
 extra.append(f'structure {8+tier} 1 "" 3 2.25 3 0.35 1.1 3 0 1 0 0 2 -1810 {h} {z-10500} -1790 {h} {z-10500}')
 for x in(-7.5,-3.8,0,3.8,7.5):put('bep_prop_bench',x,z,1.1,180,y=h)
base=base.replace('entities ', '\n'.join(extra)+'\nentities ',1)
(P/'world.studio').write_text(base)
# Merchant/quartermaster shelters and training equipment on the broad aprons.
put('castle_residents_shelter',-20,0,.68,90)
put('purchased_shop_a',20,-1,1, -90)
for x,z,yaw in[(-17,-3,90),(17,-4,-90)]:
 put('castle_weapon_rack',x,z,.85,yaw)
 put('purchased_barrel',x-1.7,z+2,.8,0)
 put('purchased_crate',x+1.1,z+2.2,.8,15)
put('village_cart',23,-17,.85,35)
put('castle_archery_target',-19,9,.85,90)
put('castle_archery_target',-19,12,.85,90)
# Entry court and banner poles. Existing native banners remain inside the arena.
for x in(-4.8,4.8):
 put('purchased_prop_flag_a'if x<0 else'purchased_prop_flag_b',x,-17,1.5,0,sy=2.3)
 put('purchased_barrel',x,-20,.8,0)
for side_sign in(-1,1):
 for z in(-24,-26.8,-29.6):put('bep_prop_fence',side_sign*4.7,z,.6,90)
put('bep_prop_bench',-7,-23,1,25)
put('bep_prop_bench',7,-23,1,-25)
# Each required source stays valid; use nine useful perimeter supply objects.
files=sorted((P/'scenery').glob('*instances.txt'))
for i,f in enumerate(files):
 f.write_text('# Woodland tournament grounds: deterministic owned-asset placements.\n'+'\n'.join(rows[i::len(files)])+'\n')
(N/'PLACEMENTS.json').write_text(json.dumps(records,indent=2)+'\n')
(N/'LAYOUT.json').write_text(json.dumps(dict(center=[-1800,64,-10500],clear_floor=[-12,-12,12,12],tree_count=len(roots),placed_objects=len(records),heightfield=[513,513],combat_changed=False,source_asset_bank=str((P/'models').resolve()),seed=65183),indent=2)+'\n')
print(json.loads((N/'LAYOUT.json').read_text()))

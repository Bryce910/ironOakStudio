#!/usr/bin/env python3
"""Baked canopy-aware scenery. No runtime sampling, new colliders or shader edits."""
from pathlib import Path
import collections,hashlib,json,math,shlex,struct,shutil
import numpy as np

N=Path(__file__).resolve().parents[1];BASE=N.with_name('arena-scenery-2026-11-18');WORLD=N/'world-project'
ORDER=['authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets','bep_environment','demo_finish','demo_polish']
def rand(x,z,salt=0):
 b=f'{x:.5f}/{z:.5f}/{salt}'.encode();return int.from_bytes(hashlib.blake2b(b,digest_size=8).digest(),'little')/2**64
def smooth(a,b,x):
 t=np.clip((x-a)/(b-a),0,1);return t*t*(3-2*t)
def records(p):
 out=[]
 for name in ORDER:
  f=p/'scenery'/(name+'_instances.txt')
  for line_number,line in enumerate(f.read_text().splitlines()):
   a=line.split()
   if not a or a[0].startswith('#'):continue
   x=(float(a[1])-.5)*100800;z=(float(a[2])-.5)*(100800*2818/5009)
   out.append(dict(file=f.name,line=line_number,text=line,a=a,alias=a[0],x=x,z=z,scale=float(a[4]),yaw=math.radians(float(a[3]))))
 return out
def bounds(path):
 b=path.read_bytes();length=struct.unpack_from('<I',b,12)[0];d=json.loads(b[20:20+length]);acc=[d['accessors'][q['attributes']['POSITION']] for m in d['meshes'] for q in m['primitives']]
 return np.min([a['min'] for a in acc],axis=0),np.max([a['max'] for a in acc],axis=0)
class Canopy:
 def __init__(self,rows,models):
  self.trees=[]
  for r in rows:
   if r['alias'] not in models:continue
   lo,hi=models[r['alias']];a=r['a'];sx=float(a[10]) if len(a)>13 else 1;sz=float(a[12]) if len(a)>13 else 1;s=r['scale']
   # Broad crown envelope, not the frame's moving shadow. Overlap reduces
   # sky access; an isolated crown retains some dappled-light transmission.
   rx=(hi[0]-lo[0])*.48*s*sx;rz=(hi[2]-lo[2])*.48*s*sz;cx=(hi[0]+lo[0])*.5*s*sx;cz=(hi[2]+lo[2])*.5*s*sz;c=math.cos(r['yaw']);t=math.sin(r['yaw'])
   self.trees.append(dict(x=r['x']+c*cx+t*cz,z=r['z']-t*cx+c*cz,root_x=r['x'],root_z=r['z'],rx=max(1,rx),rz=max(1,rz),c=c,s=t,root_radius=.55+s*.65))
 def sample(self,x,z):
  light=1.;root=1e9
  for t in self.trees:
   dx=x-t['x'];dz=z-t['z']
   if abs(dx)>t['rx']+t['rz']+2 or abs(dz)>t['rx']+t['rz']+2:continue
   q=math.hypot((t['c']*dx-t['s']*dz)/t['rx'],(t['s']*dx+t['c']*dz)/t['rz'])
   if q<1.1:light*=1-.78*float(1-smooth(.28,1.1,q))
   root=min(root,math.hypot(x-t['root_x'],z-t['root_z'])-t['root_radius'])
  return 1-light,root

road=[]
for line in (WORLD/'opening/road-traffic.settings').read_text().splitlines():
 a=line.split()
 if a and a[0]=='point':road.append((float(a[1]),float(a[3])))
def road_distance(x,z):
 result=1e9
 for a,b in zip(road,road[1:]):
  dx=b[0]-a[0];dz=b[1]-a[1];t=max(0,min(1,((x-a[0])*dx+(z-a[1])*dz)/(dx*dx+dz*dz)));result=min(result,math.hypot(x-a[0]-t*dx,z-a[1]-t*dz))
 return result
def source_unbound(studio):
 assert not any(s.startswith(('override ','group ')) for s in studio.splitlines()),'Preserve saved source references before recomposition'
 for line in studio.splitlines():
  if line.startswith('entities '):
   a=shlex.split(json.loads(line[9:]));assert a[0].startswith('VALWOOD_') or a[3]!='2','Registered source model needs explicit identity migration'

def compose(mode):
 p=N/(mode+'-project');origin=BASE/'arena-project' if mode=='arena' else N/'baseline-world';studio=(origin/'world.studio').read_text();source_unbound(studio);all_rows=records(origin)
 if mode=='arena':
  models={name:bounds(p/'models'/(name+'_near.glb')) for name in ['bep_nature_oak','bep_nature_beech']}
 else:
  manifest=json.loads((p/'assets/seldonne-detail/art-manifest-final.json').read_text())
  models={name:(np.array(manifest['models'][name]['bounds_min']),np.array(manifest['models'][name]['bounds_max'])) for name in ['nm_city_region_oak','nm_city_region_beech']}
 canopy=Canopy(all_rows,models);changes={};stats=collections.Counter();samples=[]
 for r in all_rows:
  a=r['a'];alias=r['alias'];x=r['x'];z=r['z']
  local=mode=='arena' or (abs(x+11960)<135 and 5075<z<5525)
  if not local or alias not in ('bep_nature_grass_long','bep_nature_grass_broad','opening_verge_grass_long','opening_verge_grass_broad','bep_nature_clover'):continue
  shade,root=canopy.sample(x,z);band='deep' if shade>=.68 else 'partial' if shade>=.30 else 'sun';stats[band+'_before']+=1
  grass='grass' in alias;retain=(.9-.78*float(smooth(.18,.82,shade))) if grass else (.95-.6*float(smooth(.60,.95,shade)))
  # Broad, coherent patch boundaries plus stable per-root variation. No
  # flicker/pop as the sun, camera or quality preset changes.
  patch=.5+.5*math.sin(x*.24+math.sin(z*.13))*math.cos(z*.19+x*.025)
  retain*=.78+.22*patch
  if root<.35:retain=0
  keep=rand(x,z,1)<retain
  replacement=None
  if not keep and grass and .38<shade<.93 and root>1 and rand(x,z,2)<.22:
   allowed=(max(abs(x+1800),abs(z+10500))>15 if mode=='arena' else road_distance(x,z)>5.5 and math.hypot(x+11988,z-5172)>13)
   if allowed:
    # Source fern includes a visible-root datum above its synthetic pivot.
    # Correct that datum rather than leaving its stems floating in the air.
    a=a[:];a[0]='bep_nature_fern';s=.38+rand(x,z,3)*.20;a[4]=f'{s:.8g}'
    if len(a)>13:
     sy=float(a[11]);pitch=math.radians(float(a[8]));roll=math.radians(float(a[9]));a[13]=f'{float(a[13])-.3516409695148468*s*sy*math.cos(pitch)*math.cos(roll):.12g}'
    replacement=' '.join(a);stats['shade_ferns_added']+=1
  if replacement is not None:changes[(r['file'],r['line'])]=replacement;action='fern'
  elif not keep:changes[(r['file'],r['line'])]=None;stats[band+'_removed']+=1;action='litter_gap'
  elif grass and shade>.3:
   a=a[:];a[4]=f'{r["scale"]*(1-.28*shade):.9g}';changes[(r['file'],r['line'])]=' '.join(a);stats['shorter_shade_grass']+=1;stats[band+'_retained']+=1;action='short_grass'
  else:stats[band+'_retained']+=1;action='keep'
  if mode=='world' and grass and keep and alias.startswith('bep_nature_grass_'):
   # These existing local aliases use byte-identical PNT/UV/image data and
   # the same75/18m LOD ranges. They opt out of costly per-tuft shadow draws
   # and enable the existing rooted interactive grass-wind family.
   old=changes.get((r['file'],r['line']),r['text']);new_alias=alias.replace('bep_nature_grass_','opening_verge_grass_')
   changes[(r['file'],r['line'])]=new_alias+' '+old.split(' ',1)[1];stats['grass_using_existing_lightweight_family']+=1
  samples.append(dict(alias=alias,x=x,z=z,shade=round(shade,5),root_clearance=round(root,4),band=band,action=action))
 for name in ORDER:
  f=origin/'scenery'/(name+'_instances.txt');out=[]
  for i,line in enumerate(f.read_text().splitlines()):
   value=changes.get((f.name,i),line)
   if value is not None:out.append(value)
  (p/'scenery'/f.name).write_text('\n'.join(out)+'\n')
 # Existing grass-paint masks also affect the procedural blade layer. A
 # bounded set of soft, persistent canopy masks keeps that layer from filling
 # the exposed litter gaps back in. Preserve existing town/route masks.
 existing_masks=sum(line.startswith('foliage ') for line in studio.splitlines())
 cx,cz=(-1800,-10500) if mode=='arena' else (-11988,5172)
 selected_masks=[]
 for t in sorted(canopy.trees,key=lambda t:math.hypot(t['x']-cx,t['z']-cz)):
  if len(selected_masks)>=16-existing_masks:break
  if math.hypot(t['x']-cx,t['z']-cz)>(65 if mode=='arena' else 90):continue
  if canopy.sample(t['x'],t['z'])[0]<.68:continue
  if any(math.hypot(t['x']-s['x'],t['z']-s['z'])<4 for s in selected_masks):continue
  selected_masks.append(t);radius=max(2.2,min(6,math.sqrt(t['rx']*t['rz'])*.53));studio+=f'foliage {t["x"]:.10g} {t["z"]:.10g} {radius:.6g} 0.06 0\n'
 stats['procedural_grass_shade_masks']=len(selected_masks)
 if mode=='arena':
  # Add local leaf beds beneath crowns, using the existing litter material.
  # The material follows terrain; no coplanar decal surfaces are introduced.
  payload=[json.loads(s[9:]) for s in studio.splitlines() if s.startswith('surfaces ')]
  candidates=[t for t in canopy.trees if math.hypot(t['root_x']+1800,t['root_z']+10500)<55]
  selected=[]
  for t in sorted(candidates,key=lambda t:math.hypot(t['root_x']+1800,t['root_z']+10500)):
   if all(math.hypot(t['root_x']-v['root_x'],t['root_z']-v['root_z'])>5 for v in selected):selected.append(t)
   if len(selected)==34:break
  for t in selected:
   radius=max(2.5,min(8.5,math.sqrt(t['rx']*t['rz'])*.75));payload.append(f'paint {t["root_x"]:.9g} {t["root_z"]:.9g} {radius:.6g} 0.86 1 0')
  studio='\n'.join(line for line in studio.splitlines() if not line.startswith('surfaces '))+'\n'+'\n'.join('surfaces '+json.dumps(line) for line in payload)+'\n';stats['canopy_litter_patches']=len(selected)
 else:
  # The opening is already at the256-stroke material budget. Recompose only
  # its small leaf islands beneath the actual nearby crowns, retaining all
  # broad woodland beds, path earth, riverbed and town materials unchanged.
  local_trees=sorted([t for t in canopy.trees if math.hypot(t['x']-cx,t['z']-cz)<40],key=lambda t:math.hypot(t['x']-cx,t['z']-cz));used=[];out=[]
  for line in studio.splitlines():
   if line.startswith('surfaces '):
    record=json.loads(line[9:]);v=record.split()
    if v[0]=='paint' and int(v[5]) in (1,7) and float(v[3])<9 and math.hypot(float(v[1])-cx,float(v[2])-cz)<40:
     t=next((t for t in local_trees if all(math.hypot(t['x']-u['x'],t['z']-u['z'])>3 for u in used)),None)
     if t:
      used.append(t);radius=max(3,min(9,math.sqrt(t['rx']*t['rz'])*.9));record=f'paint {t["x"]:.10g} {t["z"]:.10g} {radius:.6g} 0.86 7 0';line='surfaces '+json.dumps(record)
   out.append(line)
  studio='\n'.join(out)+'\n';stats['existing_litter_islands_recomposed']=len(used)
  payload=[json.loads(s[9:]) for s in studio.splitlines() if s.startswith('surfaces ')]
  surface_dir=p/'surfaces'
  if surface_dir.is_symlink():
   old=surface_dir.resolve();surface_dir.unlink();surface_dir.mkdir()
   for line in payload:
    if line.startswith('image '):
     relative=shlex.split(line)[2];target=p/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(old/Path(relative).relative_to('surfaces'),target)
  local_material=surface_dir/'ecology_20261118';local_material.mkdir(exist_ok=True)
  for name in ['color','normal']:shutil.copy2(BASE/'downloads/brown_mud'/(name+'.jpg'),local_material/(name+'.jpg'))
  assert sum(line.startswith('image ') for line in payload)==12
  payload[1:1]=['image "Dry campsite earth color" "surfaces/ecology_20261118/color.jpg" 0','image "Dry campsite earth normal" "surfaces/ecology_20261118/normal.jpg" 1']
  # Image indices follow serialized order. Append the new images instead so
  # all existing river, stone and leaf material references stay exact.
  extra=payload[1:3];del payload[1:3];payload+=extra
  out=[]
  for line in payload:
   if line.startswith('layer "Compacted camp earth"'):
    line='layer "Compacted camp earth" 12 13 -1 0.72 0.65 0.54 1.3 0.99 0 0.75 0'
   elif line.startswith('layer "Arrival detailed woodland litter"'):
    line='layer "Arrival detailed woodland litter" 2 3 4 0.67 0.61 0.46 2.4 0.98 0 1 0'
   out.append(line)
  studio='\n'.join(line for line in studio.splitlines() if not line.startswith('surfaces '))+'\n'+'\n'.join('surfaces '+json.dumps(line) for line in out)+'\n'
  for name in ['long','broad']:studio+=f'material opening_verge_grass_{name} 0.90 0.91 0.80 1.05 1 1\n'
  stats['existing_material_layers_retained']=8;stats['additional_ground_images']=2
 (p/'world.studio').write_text(studio)
 report=dict(mode=mode,trees=len(canopy.trees),changes=len(changes),statistics=dict(stats),classification='Persistent crown-envelope sky access with overlapping canopies; not instantaneous rendered shadows or simulated plant growth.',physics_and_tree_records_unchanged=True,source_index_references_absent=True)
 (N/(mode.upper()+'_ECOLOGY.json')).write_text(json.dumps(report,indent=2)+'\n');(N/(mode.upper()+'_FIELD.json')).write_text(json.dumps(samples,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':
 compose('arena');compose('world')

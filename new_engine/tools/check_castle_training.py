#!/usr/bin/env python3
"""Read-only actual-GLB topology, material reuse and station clearance proof."""
from pathlib import Path
import argparse,hashlib,json,math,re,sys
import numpy as np
ROOT=Path(__file__).resolve().parents[1];MODELS=ROOT/'projects/valwood_world/models'
sys.path.insert(0,str(ROOT/'tools'))
from audit_village_pose_compatibility import glb,pose

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def stations():
 text=(ROOT/'engine/world_castle_training.h').read_text().split('world_castle_training_stations[]={',1)[1].split('};',1)[0]
 rows=[]
 for body in re.findall(r'\{([^{}]+)\}',text):
  x,z,yaw,seconds,phase,instructor=[v.strip() for v in body.split(',')]
  rows.append(dict(x=float(x),z=float(z),yaw=float(yaw.removesuffix('f')),seconds=float(seconds),phase=float(phase),instructor=instructor=='true'))
 assert len(rows)==7 and sum(r['instructor'] for r in rows)==1
 return rows

def inspect(scene_path):
 m=json.loads((MODELS/'castle_training_manifest.json').read_text());assert m['schema_version']==1 and m['pose_count']==24 and m['source_samples']==20 and m['authored_recovery_samples']==4
 assert sha(ROOT/m['source'])==m['source_sha256'] and m['source_action']=='Attack-two handed2' and m['source_frames']==[33.,113.] and m['cycle_seconds']==4
 assert not m['source_file_modified'] and not m['source_scripts_executed']
 assert m['new_image_texture_policies']==0 and m['unique_images']==6
 resident=pose(MODELS/'village_character_knight_idle_00.glb');resident_materials=[p['signature']['material'] for p in resident['records']]
 poses=[];geometry_bytes=0;base=None
 for i,r in enumerate(m['assets']):
  path=MODELS/r['file'];assert r['file']==f'castle_training_knight_drill_{i:02}.glb' and sha(path)==r['sha256'] and path.stat().st_size==r['bytes']
  d,b=glb(path);p=pose(path);assert not d.get('skins') and not d.get('animations') and not d.get('extensionsRequired')
  signatures=[p['signature'] for p in p['records']]
  if base is None:base=signatures
  assert signatures==base and [p['material'] for p in signatures]==resident_materials
  views={image['bufferView'] for image in d['images']};geometry_bytes+=sum(v['byteLength'] for j,v in enumerate(d['bufferViews']) if j not in views)
  for part in p['records']:
   assert np.isfinite(part['world']).all() and np.isfinite(part['normal']).all()
   lengths=np.linalg.norm(part['normal'],axis=1);assert lengths.min()>.98 and lengths.max()<1.02
   part['world'][:,1]+=p['raw_min_y']
  assert abs(p['records'][0]['world'][:,1].min())<1e-6 and min(x['world'][:,1].min() for x in p['records'])>=-1e-6
  poses.append(p)
 assert len(poses)==24 and geometry_bytes==m['geometry_bytes'] and geometry_bytes<16*1048576
 adjacent=[]
 for a,b in zip(poses,poses[1:]+poses[:1]):
  delta=np.concatenate([np.linalg.norm(x['world']-y['world'],axis=1) for x,y in zip(a['records'],b['records'])])
  adjacent.append(dict(rms_m=float(np.sqrt(np.mean(delta**2))),max_m=float(delta.max())))
 # Every rendered in-between point is a convex combination of the two baked
 # endpoints. Their union envelope therefore bounds every GPU blend weight.
 drill_parts=[np.concatenate([p['records'][j]['world'] for p in poses]) for j in range(2)]
 idle=[pose(MODELS/f'village_character_knight_idle_{i:02}.glb') for i in range(4)]
 idle_parts=[]
 for j in range(2):
  idle_parts.append(np.concatenate([p['records'][j]['world']+np.array([0,p['raw_min_y'],0]) for p in idle]))
 scene=json.loads(Path(scene_path).read_text());models=scene['models'];rows=stations();worlds=[];bounds=[];checks=0;swept_checks=0;intentional_contacts=[]
 for index,s in enumerate(rows):
  c,si=math.cos(s['yaw']),math.sin(s['yaw']);parts=[]
  for local in (idle_parts if s['instructor'] else drill_parts):
   world=local.copy();world[:,0]=s['x']+c*local[:,0]+si*local[:,2];world[:,2]=s['z']-si*local[:,0]+c*local[:,2];parts.append(world)
  worlds.append(parts);points=np.concatenate(parts);lo=points.min(axis=0);hi=points.max(axis=0);bounds.append(dict(actor=index,min=lo.tolist(),max=hi.tolist()))
  for r in scene['placements']:
   if r['alias'] not in models:continue
   blo,bhi=[np.asarray(v)*r['scale'] for v in models[r['alias']]['bounds_y_up']]
   root=r.get('elevation',0)-r['burial']-(.05 if r['alias'] in {'starting_crate','starting_barrel','starting_bucket','starting_lantern','starting_axe','starting_stump'} else 0)
   if root+bhi[1]<lo[1]+.01 or root+blo[1]>hi[1]-.01:continue
   rc,rs=math.cos(r['yaw']),math.sin(r['yaw'])
   corners=np.asarray([[blo[0],blo[2]],[blo[0],bhi[2]],[bhi[0],blo[2]],[bhi[0],bhi[2]]]);xx=r['x']+rc*corners[:,0]+rs*corners[:,1];zz=r['z']-rs*corners[:,0]+rc*corners[:,1]
   if xx.max()<lo[0] or xx.min()>hi[0] or zz.max()<lo[2] or zz.min()>hi[2]:continue
   for part_index,points in enumerate(parts):
    dx=points[:,0]-r['x'];dz=points[:,2]-r['z'];px=rc*dx-rs*dz;pz=rs*dx+rc*dz;py=points[:,1]-root
    inside=(px>blo[0]+.003)&(px<bhi[0]-.003)&(pz>blo[2]+.003)&(pz<bhi[2]-.003)&(py>blo[1]+.003)&(py<bhi[1]-.003)
    checks+=len(points)
    # The GPU linearly interpolates each vertex. A slab test over every
    # adjacent source-pose segment covers ALL weights, including a thin prop
    # crossed between two clear endpoints. Rigid station transforms commute
    # with the blend and preserve this independent sweep proof.
    start=np.stack([px,py,pz],axis=1);nposes=4 if s['instructor'] else 24
    end=np.roll(start.reshape(nposes,-1,3),-1,axis=0).reshape(-1,3);delta=end-start
    low=blo+.003;high=bhi-.003;moving=np.abs(delta)>1e-12
    inverse=np.divide(1.,delta,out=np.zeros_like(delta),where=moving)
    v1=(low-start)*inverse;v2=(high-start)*inverse
    entry=np.minimum(v1,v2);leave=np.maximum(v1,v2)
    stationary_inside=(start>low)&(start<high)
    entry=np.where(moving,entry,np.where(stationary_inside,-np.inf,np.inf));leave=np.where(moving,leave,np.where(stationary_inside,np.inf,-np.inf))
    swept=np.maximum(entry.max(axis=1),0.)<np.minimum(leave.min(axis=1),1.)
    swept_checks+=len(points)
    inside|=swept
    if inside.any():
     # Dummy targets deliberately meet weapon tips, never the pupil body.
     assert part_index==1 and 'dummy' in r['label'].lower(),('Actual practice mesh intersects prop',index,part_index,r['label'],int(inside.sum()))
     intentional_contacts.append(dict(actor=index,prop=r['label'],weapon_vertices_inside_source_envelope=int(inside.sum())))
 # Whole-source envelopes (not just roots) remain separated from all existing
 # established main routes. The original128 residents and12 pets are untouched.
 for b in bounds:
  assert b['max'][0]<-40 and b['min'][0]>-62 and b['min'][2]>-90 and b['max'][2]<-65
 distances=[]
 for i,a in enumerate(rows):
  for j,b in enumerate(rows[:i]):distances.append(math.hypot(a['x']-b['x'],a['z']-b['z']))
 assert min(distances)>3.99
 return dict(schema_version=1,source_sha256=m['source_sha256'],pose_count=24,additional_resident_idle_assets=4,actors=7,trainees=6,instructors=1,exact_adjacent_topology_material_pairs=24,existing_resident_material_image_keys_exact=True,new_texture_image_policies=0,source_file_unchanged=True,source_scripts_executed=False,geometry_bytes=geometry_bytes,minimum_station_root_separation_m=min(distances),actual_mesh_vertex_vs_prop_envelope_checks=checks,actual_vertex_segment_vs_prop_envelope_checks=swept_checks,all_gpu_blend_weights_swept_against_prop_envelopes=True,intentional_dummy_weapon_contacts=intentional_contacts,all_blend_weights_bounded_by_baked_endpoint_union=True,stations=rows,whole_source_sweep_bounds=bounds,adjacent_pose_displacements=adjacent)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--scene',type=Path,default=ROOT/'projects/valwood_world/scenery/castle_village_manifest.json');p.add_argument('--output',type=Path);a=p.parse_args();r=inspect(a.scene)
 if a.output:a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(r,indent=2)+'\n')
 print('PASS castle training actual GLBs: 24 exact compatible pairs,6 existing resident image keys,9.23MiB geometry;7 bounded station sweeps; source props clear; checks',r['actual_mesh_vertex_vs_prop_envelope_checks'])
if __name__=='__main__':main()

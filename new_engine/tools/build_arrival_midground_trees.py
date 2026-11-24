#!/usr/bin/env python3
"""Stage fourteen grounded existing canopy instances outside the old map edge."""
from pathlib import Path
import copy,json,math
import numpy as np
from scipy.spatial import cKDTree
from visual_ground_finish import GLB,sha
from countryside_contact import root_points
from build_village_showcase_project import decompose
ROOT=Path(__file__).resolve().parents[1];PROJECT=ROOT/'build/opening-finward-reference/v15/project';OUT=ROOT/'docs/arrival-redesign/2026-11-04/terrain/midground-v1';RING=ROOT/'docs/arrival-redesign/2026-11-04/terrain/candidate-v4'
CENTER=np.array([-1486.37664868499,18.75,-10224.9106110642]);WIDTH=100800.;DEPTH=WIDTH*2818/5009
class RingSampler:
 def __init__(self,lod):
  g=GLB(RING/'models'/('arrival_border_'+lod+'.glb'));p=g.doc['meshes'][0]['primitives'][0];v=g.read(p['attributes']['POSITION']).astype(float)+[-1310.4,12,-10157.584747454583];i=g.read(p['indices']).reshape(-1,3);self.t=v[i];self.tree=cKDTree(self.t[:,:,[0,2]].mean(1))
 def sample(self,p):
  q=np.asarray(p);_,index=self.tree.query(q,k=24);t=self.t[index];a,b,c=t[:,:,0],t[:,:,1],t[:,:,2];den=(b[:,:,2]-c[:,:,2])*(a[:,:,0]-c[:,:,0])+(c[:,:,0]-b[:,:,0])*(a[:,:,2]-c[:,:,2]);w=((b[:,:,2]-c[:,:,2])*(q[:,None,0]-c[:,:,0])+(c[:,:,0]-b[:,:,0])*(q[:,None,1]-c[:,:,2]))/den;u=((c[:,:,2]-a[:,:,2])*(q[:,None,0]-c[:,:,0])+(a[:,:,0]-c[:,:,0])*(q[:,None,1]-c[:,:,2]))/den;v=1-w-u;valid=(w>=-1e-6)&(u>=-1e-6)&(v>=-1e-6);h=w*a[:,:,1]+u*b[:,:,1]+v*c[:,:,1];out=np.max(np.where(valid,h,-np.inf),axis=1);assert np.all(np.isfinite(out));return out

def main():
 assert not OUT.exists();OUT.mkdir(parents=True)
 meta=json.loads((PROJECT/'city_showcase.json').read_text());cat=json.loads(Path(meta['active_geometry_catalog']).read_text());assets={a.get('native',{}).get('alias'):copy.deepcopy(a)for a in cat['assets']if a.get('native',{}).get('alias')in['bep_nature_oak','bep_nature_beech']}
 defs={s[1]:s[2]for line in(PROJECT/'world.studio').read_text().splitlines()if len(s:=line.split())>3 and s[0]=='asset'}
 for alias,a in assets.items():a['files']=[str(PROJECT/'models'/(defs[alias]+'_near.glb')),str(PROJECT/'models'/(defs[alias]+'_mid.glb'))]
 roots={alias:root_points(a,{})for alias,a in assets.items()};near=RingSampler('near');mid=RingSampler('mid');records=[];rows=[]
 sites=[(-55,-38,.63,23,'oak'),(-60,-31,.37,141,'beech'),(-83,-52,.72,259,'oak'),(-68,-74,.43,81,'beech'),(-106,-22,.80,191,'oak'),(-82,-4,.41,322,'beech'),(-57,15,.62,58,'oak'),(-61,23,.34,221,'beech'),(-91,42,.77,108,'oak'),(-107,24,.40,274,'beech'),(-72,65,.66,167,'oak'),(-45,74,.39,37,'beech'),(-52,-80,.61,296,'oak'),(-93,-72,.44,94,'beech')]
 for x,z,s,yaw,kind in sites:
  alias='bep_nature_'+kind;a=assets[alias];angle=math.radians(yaw);c,si=math.cos(angle),math.sin(angle);rot=np.array([[c,0,si],[0,1,0],[-si,0,c]])*s;r=roots[alias];world=CENTER+np.array([x,0,z]);world[1]=near.sample(world[None,[0,2]])[0];p=r@rot.T+world;e=p[:,1]-near.sample(p[:,[0,2]]);world[1]-=max(float(e.max())+.025,0)
  p=r@rot.T+world;e=p[:,1]-near.sample(p[:,[0,2]]);em=p[:,1]-mid.sample(p[:,[0,2]])
  m=np.eye(4);m[:3,:3]=rot;m[:3,3]=world+rot@np.asarray(a['pivot_offset_native']);yy,pi,ro,sc,ex=decompose(m);wx,wy,wz=m[:3,3]
  row=f'{alias} {wx/WIDTH+.5:.17g} {wz/DEPTH+.5:.17g} {yy:.12g} {sc:.12g} 0 0 0 {pi:.12g} {ro:.12g} '+' '.join(format(float(v),'.12g')for v in ex)+f' {wy:.12g}';rows.append(row)
  records.append(dict(alias=alias,local_xz=[x,z],distance_to_camp=float(np.hypot(x,z)),world_root=world.tolist(),scale=s,yaw=yaw,near_root_error_range=[float(e.min()),float(e.max())],mid_root_error_range=[float(em.min()),float(em.max())],projected_height_m=float(a['bounds'][1][1]*s),row=row))
 sources=[]
 for a in assets.values():sources +=[dict(path=p,sha256=sha(p))for p in a['files']]
 report=dict(passed=True,scope='Fourteen asymmetrical west/northwest/southwest midground canopy trees; existing owned oak/beech only. Actual integrated candidate ring HIGH/MID triangles sampled at all root sites; no terrain or collision model changes.',records=records,new_instances=len(records),new_assets=0,new_texture_bytes=0,terrain_manifest=str(RING/'TERRAIN_REVIEW.json'),terrain_manifest_sha256=sha(RING/'TERRAIN_REVIEW.json'),sources=sources,source_project_unchanged=True,visual_acceptance_required=True,recipe_sha256=sha(__file__))
 (OUT/'instances.txt').write_text('# Layered midground canopy; append after existing arrival woodland edge.\n'+'\n'.join(rows)+'\n');(OUT/'MIDGROUND_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(passed=True,new_instances=len(rows),rows=str(OUT/'instances.txt')),indent=2))
if __name__=='__main__':main()

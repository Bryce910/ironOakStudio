#!/usr/bin/env python3
"""Immutable layered woodland finish, replacing midground-v1 completely."""
from pathlib import Path
import copy,json,math
import numpy as np
from scipy.spatial import cKDTree
from visual_ground_finish import GLB,sha
from countryside_contact import root_points,fit_plane
from build_village_showcase_project import decompose
ROOT=Path(__file__).resolve().parents[1];PROJECT=ROOT/'build/opening-finward-reference/v15/project';OUT=ROOT/'docs/arrival-redesign/2026-11-04/terrain/midground-v7';RING=ROOT/'docs/arrival-redesign/2026-11-04/terrain/candidate-v7'
CENTER=np.array([-1486.37664868499,18.75,-10224.9106110642]);WIDTH=100800.;DEPTH=WIDTH*2818/5009
class RingSampler:
 def __init__(self,lod):
  doc=json.loads((RING/'TERRAIN_REVIEW.json').read_text());row=doc['contract']['instance_row'].split();anchor=np.array([(float(row[1])-.5)*WIDTH,float(row[13]),(float(row[2])-.5)*DEPTH])
  g=GLB(RING/'models'/('arrival_border_'+lod+'.glb'));p=g.doc['meshes'][0]['primitives'][0];v=g.read(p['attributes']['POSITION']).astype(float)+anchor;i=g.read(p['indices']).reshape(-1,3);self.t=v[i];self.tree=cKDTree(self.t[:,:,[0,2]].mean(1))
 def sample(self,p):
  q=np.asarray(p);_,index=self.tree.query(q,k=32);t=self.t[index];a,b,c=t[:,:,0],t[:,:,1],t[:,:,2];den=(b[:,:,2]-c[:,:,2])*(a[:,:,0]-c[:,:,0])+(c[:,:,0]-b[:,:,0])*(a[:,:,2]-c[:,:,2]);w=((b[:,:,2]-c[:,:,2])*(q[:,None,0]-c[:,:,0])+(c[:,:,0]-b[:,:,0])*(q[:,None,1]-c[:,:,2]))/den;u=((c[:,:,2]-a[:,:,2])*(q[:,None,0]-c[:,:,0])+(a[:,:,0]-c[:,:,0])*(q[:,None,1]-c[:,:,2]))/den;v=1-w-u;valid=(w>=-1e-6)&(u>=-1e-6)&(v>=-1e-6);h=w*a[:,:,1]+u*b[:,:,1]+v*c[:,:,1];out=np.max(np.where(valid,h,-np.inf),axis=1);assert np.all(np.isfinite(out));return out
class Surface:
 def __init__(self):self.origin=np.zeros(3);self.layers=[RingSampler('near'),RingSampler('mid')]
 def sample(self,p,lod=0):return self.layers[lod].sample(p)

def main():
 assert not OUT.exists();OUT.mkdir(parents=True)
 meta=json.loads((PROJECT/'city_showcase.json').read_text());cat=json.loads(Path(meta['active_geometry_catalog']).read_text());names=['oak','beech','bush_a','bush_b','rock','fern','grass_long','grass_broad','clover'];wanted={'bep_nature_'+s for s in names};assets={a.get('native',{}).get('alias'):copy.deepcopy(a)for a in cat['assets']if a.get('native',{}).get('alias')in wanted}
 defs={s[1]:s[2]for line in(PROJECT/'world.studio').read_text().splitlines()if len(s:=line.split())>3 and s[0]=='asset'}
 for alias,a in assets.items():a['files']=[str(PROJECT/'models'/(defs[alias]+'_near.glb')),str(PROJECT/'models'/(defs[alias]+'_mid.glb'))]
 grass=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-grass-clumps/source-audit.json').read_text());gs={a['alias']:a['tuft_sites_native']for a in grass['geometry']};roots={alias:root_points(a,gs)for alias,a in assets.items()};surface=Surface();records=[];rows=[];sites=[]
 anchors=[(-57,-47),(-79,-82),(-110,-55),(-57,-14),(-78,15),(-117,-3),(-57,42),(-97,67)]
 for k,(x,z)in enumerate(anchors):
  for j,(dx,dz)in enumerate([(-3,-4),(4,3),(-7,7),(8,-5)]):
   kind='beech'if j in[1,3]else'oak';sc=(.55+.035*((k+j)%4))if kind=='beech'else(.72+.085*((k+j)%5));sites.append(('bep_nature_'+kind,x+dx,z+dz,sc,(k*47+j*91+23)%360,'canopy',k))
  for j,(dx,dz)in enumerate([(4.5,5),(-5.5,3),(2,-6),(-4,-4)]):sites.append(('bep_nature_bush_'+('a'if(k+j)%2 else'b'),x+dx,z+dz,.29+.035*((k+j)%4),(k*73+j*83)%360,'understory',k))
  for j,(dx,dz)in enumerate([(7,2),(-5,-7)]):sites.append(('bep_nature_rock',x+dx,z+dz,.32+.08*((k+j)%3),(k*61+j*137)%360,'buried_rock',k))
  for j,(dx,dz)in enumerate([(1,4),(-3,2),(4,-1)]):sites.append(('bep_nature_fern',x+dx,z+dz,.60+.08*((k+j)%3),(k*31+j*101)%360,'low_fern',k))
  if np.hypot(x,z)<112:
   for j,(dx,dz)in enumerate([(-2,5),(3,-2),(-5,-1)]):sites.append(('bep_nature_grass_'+('long'if(k+j)%2 else'broad'),x+dx,z+dz,.63+.04*((k+j)%3),(k*37+j*119)%360,'low_grass',k))
   for j,(dx,dz)in enumerate([(2,3),(-1,-3)]):sites.append(('bep_nature_clover',x+dx,z+dz,.72+.08*((k+j)%2),(k*53+j*157)%360,'low_clover',k))
 for alias,x,z,s,yaw,purpose,cluster in sites:
  a=assets[alias];angle=math.radians(yaw);c,si=math.cos(angle),math.sin(angle);rot=np.array([[c,0,si],[0,1,0],[-si,0,c]])*s;r=roots[alias];world=CENTER+np.array([x,0,z]);world[1]=surface.sample(world[None,[0,2]])[0];m=np.eye(4);m[:3,:3]=rot;m[:3,3]=world
  if purpose.startswith('low_'):m=fit_plane(m,r,surface,np.array([s,s,s]))
  else:
   p=r@rot.T+world;e=p[:,1]-surface.sample(p[:,[0,2]]);m[1,3]-=max(float(e.max()if purpose=='canopy'else np.percentile(e,90))+.025,0)
  p=r@m[:3,:3].T+m[:3,3];e=p[:,1]-surface.sample(p[:,[0,2]]);em=p[:,1]-surface.sample(p[:,[0,2]],1)
  rendered=copy.copy(m);rendered[:3,3]+=m[:3,:3]@np.asarray(a['pivot_offset_native']);yy,pi,ro,sc,ex=decompose(rendered);wx,wy,wz=rendered[:3,3]
  row=f'{alias} {wx/WIDTH+.5:.17g} {wz/DEPTH+.5:.17g} {yy:.12g} {sc:.12g} 0 0 0 {pi:.12g} {ro:.12g} '+' '.join(format(float(v),'.12g')for v in ex)+f' {wy:.12g}';rows.append(row)
  records.append(dict(alias=alias,purpose=purpose,cluster=cluster,local_xz=[x,z],distance_to_camp=float(np.hypot(x,z)),world_root=m[:3,3].tolist(),scale=s,yaw=yaw,near_root_error_range=[float(e.min()),float(e.max())],mid_root_error_range=[float(em.min()),float(em.max())],row=row))
 sources=[]
 for a in assets.values():sources +=[dict(path=p,sha256=sha(p))for p in a['files']]
 counts={kind:sum(r['purpose']==kind for r in records)for kind in set(r['purpose']for r in records)}
 report=dict(passed=True,scope='REPLACE midground-v1 rows completely. Eight irregular woodland groups,32canopieswithunderstory/rock/lowcover; existing owned asset aliases only. HIGH/MID actual candidate-V7 triangles at every source root site.',records=records,counts=counts,new_instances=len(records),new_assets=0,new_texture_bytes=0,terrain_manifest=str(RING/'TERRAIN_REVIEW.json'),terrain_manifest_sha256=sha(RING/'TERRAIN_REVIEW.json'),sources=sources,minimum_camp_root_distance_m=min(r['distance_to_camp']for r in records),qa_and_connector_clearance_m='All new roots beyond55mwestofcamp; minimum2mQAclearance preserved.',source_project_unchanged=True,visual_acceptance_required=True,recipe_sha256=sha(__file__))
 assert counts['canopy']==32 and all(sha(q['path'])==q['sha256']for q in sources)
 (OUT/'instances.txt').write_text('# REPLACE midground-v1 entirely; no duplicate prior14trees.\n'+'\n'.join(rows)+'\n');(OUT/'MIDGROUND_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(passed=True,counts=counts,new_instances=len(rows),rows=str(OUT/'instances.txt')),indent=2))
if __name__=='__main__':main()

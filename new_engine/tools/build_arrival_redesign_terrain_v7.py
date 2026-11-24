#!/usr/bin/env python3
"""Immutable camp soil and continuous textured countryside border candidate."""
from pathlib import Path
import argparse, copy, json, subprocess
import numpy as np
from scipy.ndimage import map_coordinates
from visual_ground_finish import GLB, append_view, write_glb, sha, segment_distance, MARKER
from build_city_countryside import readfield,field,sample
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/arrival-redesign/2026-11-04/terrain'
CENTER=np.array([-1486.37664868499,-10224.9106110642])
W=100800.;D=W*2818/5009


def smooth(t):
 t=np.clip(t,0,1);return t*t*(3-2*t)

def camp_wear(world):
 p=np.asarray(world,float)-CENTER
 angle=np.arctan2(p[:,1],p[:,0]);radius=np.linalg.norm(p,axis=1)
 edge=2.25+.29*np.sin(angle*3+.7)+.18*np.sin(angle*5-1.1)
 hearth=1-smooth((radius-edge)/1.30)
 # Modest overlapping foot traffic, never a perfect8m bare stamp.
 lanes=np.array([[[0,1.3],[0,4.8]], [[-1.,.5],[-2.15,-.65]],[[.8,.5],[2.2,-.65]],[[.1,-1.3],[.1,-3.2]], [[-1.2,-1.3],[-3.3,-3.2]]])+CENTER
 d=segment_distance(world,lanes)
 lane=(1-smooth((d-.40)/.85))*.72
 low=np.array([-5.95,-5.8]);high=np.array([-1.2,-2.2]);exterior=np.maximum(np.maximum(low-p,p-high),0)
 shelter=(1-smooth(np.linalg.norm(exterior,axis=1)/.8))*.62
 connector=np.asarray(json.loads((ROOT/'docs/arrival-camp/2026-11-04/art/ART_CANDIDATE.json').read_text())['connection_world'])[:,[0,2]]
 connector_lane=(1-smooth((segment_distance(world,np.stack((connector[:-1],connector[1:]),axis=1))-.80)/1.1))*.88
 return np.maximum.reduce([hearth,lane,shelter,connector_lane]).clip(0,1).astype('<f4')

def add_array(doc,binary,values,kind,index=False):
 values=np.asarray(values,dtype='<u4'if index else'<f4')
 binary,view=append_view(doc,binary,values.tobytes(),target=34963 if index else 34962)
 a=dict(bufferView=view,componentType=5125 if index else 5126,count=len(values),type=kind)
 if not index:
  flat=values.reshape(len(values),-1);a.update(min=flat.min(0).tolist(),max=flat.max(0).tolist())
 idx=len(doc['accessors']);doc['accessors'].append(a);return binary,idx

def rewrite_camp(project,previous,out):
 rows=[];replacements=[];sources=[]
 origin=np.array(json.loads((project/'city_showcase.json').read_text())['origin'])[[0,2]]
 for line in(project/'world.studio').read_text().splitlines():
  s=line.split()
  if len(s)<4 or s[0]!='asset' or s[1]!='bep_country_terrain_0_1':continue
  alias,prefix=s[1:3]
  for suffix,lod in[('.glb','near'),('_mid.glb','mid')]:
   rel=Path('models')/(prefix+suffix);source=project/rel;oldsource=previous/rel
   original=GLB(source);old=GLB(oldsource);doc=copy.deepcopy(original.doc);binary=original.binary;changes=[]
   for mi,mesh in enumerate(doc['meshes']):
    for pi,primitive in enumerate(mesh['primitives']):
     if '_VALWOOD_PAINT'not in primitive['attributes']:continue
     before=original.read(primitive['attributes']['_VALWOOD_PAINT']).ravel()
     uv=original.read(primitive['attributes']['TEXCOORD_0']);world=uv.astype(float)*4+origin
     inherited=old.read(old.doc['meshes'][mi]['primitives'][pi]['attributes']['_VALWOOD_PAINT']).ravel()
     distance=np.linalg.norm(world-CENTER,axis=1)
     retention=smooth((distance-9.)/3.)
     after=np.maximum(inherited*retention,camp_wear(world))
     binary,acc=add_array(doc,binary,after,'SCALAR');primitive['attributes']['_VALWOOD_PAINT']=acc
     changes.append(dict(vertices=len(after),changed=int(np.sum(before!=after)),old_bare=int(np.sum(before>.95)),new_bare=int(np.sum(after>.95))))
   dest=out/'models'/alias/('asset_'+lod+'.glb');write_glb(dest,doc,binary);check=GLB(dest)
   assert check.binary[:len(original.binary)]==original.binary
   for mi,(a,b)in enumerate(zip(original.doc['meshes'],check.doc['meshes'])):
    for ap,bp in zip(a['primitives'],b['primitives']):
     assert ap['indices']==bp['indices']
     for k,v in ap['attributes'].items():
      if k!='_VALWOOD_PAINT':assert v==bp['attributes'][k]and np.array_equal(original.read(v),check.read(v))
   for rel2 in[rel]+([Path('models')/(prefix+'_near.glb')]if lod=='near'else[]):replacements.append(dict(relative_path=str(rel2),candidate=str(dest),sha256=sha(dest)))
   sources.append(dict(path=str(source),sha256=sha(source),previous_path=str(oldsource),previous_sha256=sha(oldsource)))
   rows.append(dict(alias=alias,lod=lod,output=str(dest),sha256=sha(dest),changes=changes,geometry_material_image_exact=True))
 return rows,replacements,sources


def boundary_edges(project,old_origin,old_step):
 lo=old_origin;hi=lo+old_step*1024;edges={};sources=[]
 defs={s[1]:s[2]for line in(project/'world.studio').read_text().splitlines()if len(s:=line.split())>3 and s[0]=='asset'}
 records={}
 for path in(project/'scenery').glob('*.txt'):
  for line in path.read_text().splitlines():
   s=line.split()
   if s and s[0].startswith('bep_country_terrain_'):records[s[0]]=s
 for side,axis,bound in [('w',0,lo[0]),('e',0,hi[0]),('s',1,lo[1]),('n',1,hi[1])]:
  sites=[]
  for alias,s in records.items():
   xi,zi=map(int,alias.split('_')[-2:])
   if not((side=='w'and xi==0)or(side=='e'and xi==3)or(side=='s'and zi==0)or(side=='n'and zi==3)):continue
   path=project/'models'/(defs[alias]+'.glb');g=GLB(path);translation=np.array([(float(s[1])-.5)*W,float(s[13]),(float(s[2])-.5)*D]);sources.append(dict(path=str(path),sha256=sha(path)))
   for mesh in g.doc['meshes']:
    for pr in mesh['primitives']:
     pos=g.read(pr['attributes']['POSITION']).astype(float)+translation
     mask=np.abs(pos[:,[0,2]][:,axis]-bound)<1e-4
     if mask.any():sites.extend(pos[mask][:,[2 if axis==0 else 0,1]].tolist())
  a=np.array(sites);order=np.argsort(a[:,0]);a=a[order];keys=np.round(a[:,0],7);_,index=np.unique(keys,return_index=True);a=a[index];edges[side]=a
 return edges,sources

def grid_ring(lo,hi,oldlo,oldhi,stride,step,edges):
 x=np.linspace(lo[0],hi[0],2048//stride+1);z=np.linspace(lo[1],hi[1],2048//stride+1);xx,zz=np.meshgrid(x,z);p=np.column_stack((xx.ravel(),zz.ravel())).tolist();ids=np.arange(len(p)).reshape(len(z),len(x));a=ids[:-1,:-1].ravel();b=ids[:-1,1:].ravel();c=ids[1:,:-1].ravel();d=ids[1:,1:].ravel();tri=np.concatenate((np.stack((a,c,b),1),np.stack((b,c,d),1)))
 centers=np.asarray(p)[tri].mean(1);tri=tri[~(np.all(centers>oldlo,axis=1)&np.all(centers<oldhi,axis=1))]
 lookup={tuple(np.round(q,7)):i for i,q in enumerate(p)}
 for side,axis,bound in [('w',0,oldlo[0]),('e',0,oldhi[0]),('s',1,oldlo[1]),('n',1,oldhi[1])]:
  other=1-axis;common=np.unique(np.r_[edges[side][:,0],np.asarray(p)[np.abs(np.asarray(p)[:,axis]-bound)<1e-5,other]])
  common=common[(common>=oldlo[other]-1e-5)&(common<=oldhi[other]+1e-5)];common=np.clip(common,oldlo[other],oldhi[other]);common=np.unique(common)
  common_ids=[]
  for v in common:
   q=[0.,0.];q[axis]=bound;q[other]=v;k=tuple(np.round(q,7))
   if k not in lookup:lookup[k]=len(p);p.append(q)
   common_ids.append(lookup[k])
  pp=np.asarray(p);on=np.abs(pp[:,axis]-bound)<1e-5;on&=(pp[:,other]>=oldlo[other]-1e-5)&(pp[:,other]<=oldhi[other]+1e-5);split=on[tri].sum(1)==2;keep=[tri[~split]];fans=[]
  for face in tri[split]:
   border=face[on[face]];opposite=int(face[~on[face]][0]);left,right=np.sort(pp[border,other]);chain=np.asarray(common_ids)[(common>=left-1e-5)&(common<=right+1e-5)]
   for l,r in zip(chain,chain[1:]):
    face2=np.array([opposite,l,r]);v=pp[face2]
    if np.cross(v[1]-v[0],v[2]-v[0])>0:face2=face2[[0,2,1]]
    fans.append(face2)
  if fans:keep.append(np.array(fans))
  tri=np.concatenate(keep)
 pp=np.asarray(p);area=np.cross(pp[tri[:,1]]-pp[tri[:,0]],pp[tri[:,2]]-pp[tri[:,0]]);tri=tri[np.abs(area)>1e-7];area=area[np.abs(area)>1e-7];tri[area>0]=tri[area>0][:,[0,2,1]]
 used,inv=np.unique(tri,return_inverse=True);return np.asarray(p)[used],inv.reshape(-1,3)


def stage(project,previous,out):
 out.mkdir(parents=True,exist_ok=True)
 assert not(out/'TERRAIN_REVIEW.json').exists(),'Use fresh immutable candidate'
 paint,replacements,sources=rewrite_camp(project,previous,out)
 old,oldlo,step=readfield(project/'ground/countryside.height');assert old.shape==(1025,1025)
 oldhi=oldlo+step*1024;lo=oldlo-step*512;hi=oldhi+step*512
 sampler=DEFAULT/'sample_atlas_grid'
 raw=subprocess.check_output([str(sampler),str(lo[0]),str(lo[1]),str(step[0]),str(step[1]),'2049','2049'])
 atlas=np.frombuffer(raw,dtype='<f4').reshape(2049,2049)
 xx,zz=np.meshgrid(lo[0]+np.arange(2049)*step[0],lo[1]+np.arange(2049)*step[1]);p=np.stack((xx,zz),-1)
 nearest=np.clip(p,oldlo,oldhi);ext=sample(old,nearest,oldlo,step)
 # The extrapolation carries the boundary slope for the first few metres, then
 # settles. Quintic edge weight has zero first/second derivatives at both ends.
 gradz,gradx=np.gradient(old,*step[::-1]);gx=sample(gradx,nearest,oldlo,step);gz=sample(gradz,nearest,oldlo,step)
 delta=p-nearest;distance=np.linalg.norm(delta,axis=-1);edgegrad=np.clip(gx,-.20,.20)*delta[:,:,0]+np.clip(gz,-.20,.20)*delta[:,:,1]
 extension=ext+edgegrad*np.exp(-distance/22.)
 t=np.clip(distance/np.min(step*512),0,1);blend=t*t*t*(t*(t*6-15)+10)
 expanded=(extension*(1-blend)+atlas*blend).astype('<f4')
 # Final bounded absolute-height ridge. Smooth maximum keeps the new
 # gradient a convex combination of the source and hill gradients, rather
 # than adding a steep hill to an already sloping base.
 local=p-CENTER;west=-local[:,:,0];zzlocal=local[:,:,1]
 axis=100.+2.*np.sin(zzlocal*.04)
 axis_height=sample(expanded,np.stack((CENTER[0]-axis,p[:,:,1]),axis=-1),lo,step)
 peak=np.minimum(axis_height+16.,39.+np.cos(zzlocal*.045))
 hill=peak-.0065*(west-axis)**2-.003*np.maximum(np.abs(zzlocal)-80.,0.)**2
 difference=hill-expanded
 bump=np.where(difference<=0.,0.,np.where(difference<1.,difference*difference/2.,difference-.5))
 # C1 cap within the last40cm keeps the nominal16m rise below16m exactly.
 bump=np.where(bump<=15.8,bump,16.-np.maximum(16.2-bump,0.)**2/.8)
 # Two offset crest lobes and a shallow saddle break the even silhouette.
 # Their compact cross-slope envelopes leave the steep front shoulder alone.
 lobes=3.7*np.exp(-((west-112.)/24.)**2-((zzlocal+42.)/26.)**2)
 lobes+=3.1*np.exp(-((west-100.)/24.)**2-((zzlocal-38.)/30.)**2)
 lobes-=1.0*np.exp(-((west-103.)/28.)**2-((zzlocal-1.)/23.)**2)
 outer_distance=np.minimum(p-lo,hi-p).min(axis=2)
 crest_finish=lobes*smooth(distance/16.)*smooth(outer_distance/22.)
 bump=np.maximum(bump+crest_finish,0.)
 assert float(bump[512:1537,512:1537].max())==0.
 expanded+=bump.astype('<f4');expanded[512:1537,512:1537]=old
 gz,gx=np.gradient(expanded,*step[::-1]);edited=bump>1.e-4
 maximum_edited_slope_degrees=float(np.rad2deg(np.arctan(np.hypot(gx,gz)[edited].max())))
 assert maximum_edited_slope_degrees<=35.,maximum_edited_slope_degrees
 assert np.array_equal(expanded[512:1537,512:1537],old)
 assert np.array_equal(expanded[[0,-1]],atlas[[0,-1]])and np.array_equal(expanded[:,[0,-1]],atlas[:,[0,-1]])
 field(out/'ground/countryside.height',expanded,lo,step)
 # This coverage raster remains1025² to satisfy the shared GPU array contract;
 # constant full ownership has no resampling/edge ambiguity.
 field(out/'ground/countryside.coverage',np.ones((1025,1025),dtype='<f4'),lo,step*2)
 for rel in['ground/countryside.height','ground/countryside.coverage']:
  q=out/rel;replacements.append(dict(relative_path=rel,candidate=str(q),sha256=sha(q)));sources.append(dict(path=str(project/rel),sha256=sha(project/rel)))
 edges,edge_sources=boundary_edges(project,oldlo,step);sources+=edge_sources
 source=GLB(Path(next(r['path']for r in edge_sources if 'bep_country_terrain_0_1/'in r['path'])))
 origin=np.array(json.loads((project/'city_showcase.json').read_text())['origin'])[[0,2]]
 anchor=(oldlo+oldhi)/2;datum=float(np.floor(expanded.min())-1);rows=[];max_inner=0
 for label,stride in[('near',8),('mid',8)]:
  positions,tri=grid_ring(lo,hi,oldlo,oldhi,stride,step,edges)
  h=sample(expanded,positions,lo,step)+.05
  for side,axis,bound in [('w',0,oldlo[0]),('e',0,oldhi[0]),('s',1,oldlo[1]),('n',1,oldhi[1])]:
   other=1-axis;mask=(np.abs(positions[:,axis]-bound)<1e-5)&(positions[:,other]>=oldlo[other]-1e-5)&(positions[:,other]<=oldhi[other]+1e-5)
   expected=np.interp(positions[mask,other],edges[side][:,0],edges[side][:,1]);h[mask]=expected;max_inner=max(max_inner,float(np.max(abs(h[mask]-expected))))
  # Taper the existing five-centimetre render offset into exact atlas at the
  # outer boundary, so the outer rendered surface meets ordinary atlas terrain.
  outer=np.minimum(positions-lo,hi-positions).min(1);h-=.05*(1-smooth(outer/8.))
  eps=.43;gx=(sample(expanded,positions+[eps,0],lo,step)-sample(expanded,positions-[eps,0],lo,step))/(2*eps);gz=(sample(expanded,positions+[0,eps],lo,step)-sample(expanded,positions-[0,eps],lo,step))/(2*eps)
  n=np.column_stack((-gx,np.ones(len(gx)),-gz));n/=np.linalg.norm(n,axis=1)[:,None];tan=np.column_stack((np.ones(len(gx)),gx,np.zeros(len(gx))));tan/=np.linalg.norm(tan,axis=1)[:,None];tan=np.column_stack((tan,-np.ones(len(gx))))
  pos=np.column_stack((positions[:,0]-anchor[0],h-datum,positions[:,1]-anchor[1]));uv=(positions-origin)/4
  # Broad sparse soil/moss transitions break up the old uniform lime field.
  noise=.5+.22*np.sin(positions[:,0]*.053+positions[:,1]*.039)+.17*np.sin(positions[:,0]*.021-positions[:,1]*.087)
  weight=(.02+.20*smooth((noise-.45)/.5)).astype('<f4')
  # Fade to original grass at the exact shared inner rim.
  inside_distance=np.linalg.norm(positions-np.clip(positions,oldlo,oldhi),axis=1);weight*=smooth(inside_distance/12.)
  doc=copy.deepcopy(source.doc);binary=source.binary;attrs={}
  for k,a,typ in [('POSITION',pos,'VEC3'),('NORMAL',n,'VEC3'),('TEXCOORD_0',uv,'VEC2'),('TANGENT',tan,'VEC4'),('_VALWOOD_PAINT',weight,'SCALAR')]:binary,attrs[k]=add_array(doc,binary,a,typ)
  binary,indices=add_array(doc,binary,tri.ravel(),'SCALAR',True)
  mat=next(i for i,m in enumerate(doc['materials'])if m.get('name')=='grassground')
  doc['materials'][mat]['extensions']['VALWOOD_materials_facade']['parameters'][-1]=7.876
  primitive=dict(attributes=attrs,indices=indices,material=mat)
  # A tiny degenerate datum at origin keeps importer grounding common to LODs.
  binary,dp=add_array(doc,binary,np.zeros((3,3)),'VEC3');binary,dn=add_array(doc,binary,np.tile([0,1,0],(3,1)),'VEC3');binary,di=add_array(doc,binary,np.arange(3),'SCALAR',True)
  dmat=next(i for i,m in enumerate(doc['materials'])if m.get('name')=='Countryside_Pivot_Datum')
  doc['meshes']=[dict(name='Arrival countryside border',primitives=[primitive,dict(attributes=dict(POSITION=dp,NORMAL=dn),indices=di,material=dmat)])];doc['nodes']=[dict(name='Arrival countryside border',mesh=0)];doc['scenes']=[dict(nodes=[0])];doc['scene']=0
  dest=out/'models'/('arrival_border_'+label+'.glb');write_glb(dest,doc,binary)
  q=GLB(dest);cross=np.cross(pos[tri[:,1]]-pos[tri[:,0]],pos[tri[:,2]]-pos[tri[:,0]]);assert np.all(cross[:,1]>0)
  rows.append(dict(lod=label,path=str(dest),sha256=sha(dest),triangles=len(tri),vertices=len(pos),bytes=dest.stat().st_size,rendered_bounds=[pos.min(0).tolist(),pos.max(0).tolist()]))
 for row in rows:
  names=['models/opening/arrival_border_'+row['lod']+'.glb']+(['models/opening/arrival_border.glb']if row['lod']=='near'else[])
  replacements +=[dict(relative_path=n,candidate=row['path'],sha256=row['sha256'])for n in names]
 contract=dict(alias='bep_opening_arrival_border',studio_row='asset bep_opening_arrival_border opening/arrival_border 2 0 0 1600 260 1',instance_row=f'bep_opening_arrival_border {anchor[0]/W+.5:.17g} {anchor[1]/D+.5:.17g} 0 1 0 0 0 0 0 1 1 1 {datum:.17g}',physics='No new proxies; expanded fallback heightfield supplies the ordinary terrain floor.')
 report=dict(passed=True,diagnosis='Camp44mfromwestedgeof440x460m authored terrain. At rectangleedge authoredsurface~20m abruptly gives way to atlas~24–25m and genericgrass. Exactrectanglecoverage exposedthis4–6m wallandhorizontalbands.',source_project=str(project),sources=sources,original_lattice_bit_exact=True,original_field_node_offset=[512,512],new_field_dimensions=[2049,2049],new_field_bounds=[*lo,*hi],field_outer_border_atlas_node_error_m=0,render_inner_boundary_error_m=max_inner,original_meshes_and_physics_unchanged=True,paint=paint,models=rows,replacements=replacements,contract=contract,ridge_maximum_height_delta_m=float(bump.max()),maximum_edited_slope_degrees=maximum_edited_slope_degrees,original_rectangle_height_delta_m=0.,ring_facade_marker=7.876,mid_geometry_policy='Exact near lattice retained in MID to keep all ridge vegetation contacts identical;100k static triangles per tier,~3.4MB additional MID geometry, no new textures.',required_shader_patch='ring-grass-shader-patch.json',recipe_sha256=sha(__file__),visual_acceptance='Root must verify normaleye360degreesandgameplayframing; CPUgeometryaloneisnotvisualacceptance.')
 assert all(sha(r['path'])==r['sha256']for r in sources)
 (out/'TERRAIN_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in['passed','contract','models']},indent=2))

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--project',type=Path,default=ROOT/'build/opening-finward-reference/v15/project');p.add_argument('--previous',type=Path,default=ROOT/'build/opening-finward-reference/v14/project');p.add_argument('--output',type=Path,default=DEFAULT/'candidate-v7');a=p.parse_args();stage(a.project.resolve(),a.previous.resolve(),a.output.resolve())

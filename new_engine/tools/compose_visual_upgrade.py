#!/usr/bin/env python3
"""Compose a measured castle and jungle pocket in the isolated visual project."""
from pathlib import Path
import ctypes as C,copy,hashlib,json,math,random,sys,tempfile
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from build_visual_upgrade_assets import read_glb,OUT,PROJECT,MODELS,REPORT
from build_starting_area import Atlas,glb_bounds,segment_distance
from build_cinematic_diversity import Writer
WIDTH=100800.;DEPTH=WIDTH*2818/5009
FILES=['authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets','bep_environment','demo_finish','demo_polish','demo_completion']
TRAIL=[(-26737,9736),(-26727,9735.4967059),(-26710,9700.4967059),(-26720,9660.4967059),(-26707,9666.4967059),(-26700,9658.4967059),(-26694,9658.4967059)]
def accessor(d,blob,index):
 a=d['accessors'][index];v=d['bufferViews'][a['bufferView']];components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];dtype=np.dtype({5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]);stride=v.get('byteStride',dtype.itemsize*components)
 return np.ndarray((a['count'],components),dtype=dtype,buffer=blob,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(stride,dtype.itemsize)).copy()
def node_matrix(n):
 if 'matrix'in n:return np.asarray(n['matrix']).reshape(4,4).T
 x,y,z,w=n.get('rotation',[0,0,0,1]);m=np.eye(4);m[:3,:3]=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])@np.diag(n.get('scale',[1,1,1]));m[:3,3]=n.get('translation',[0,0,0]);return m

def geometry(path):
 d,b,_=read_glb(path);parts=[]
 def visit(index,parent):
  n=d['nodes'][index];m=parent@node_matrix(n)
  if 'mesh'in n:
   for p in d['meshes'][n['mesh']]['primitives']:
    a=p['attributes'];pos=accessor(d,b,a['POSITION']);pos=(np.c_[pos,np.ones(len(pos))]@m.T)[:,:3];normal=accessor(d,b,a['NORMAL'])@np.linalg.inv(m[:3,:3]);normal/=np.maximum(np.linalg.norm(normal,axis=1,keepdims=True),1e-8);uv=accessor(d,b,a['TEXCOORD_0']);idx=accessor(d,b,p['indices']).reshape(-1,3);parts.append([pos,normal,uv,idx,p.get('material',0)])
  for child in n.get('children',[]):visit(child,m)
 for n in d['scenes'][d.get('scene',0)]['nodes']:visit(n,np.eye(4))
 floor=min(p[0][:,1].min()for p in parts)
 for p in parts:p[0][:,1]-=floor
 return d,b,parts

def grass_clusters():
 rng=random.Random(36219);placements=[(0,0,0,1)]
 for i in range(9):
  a=i*2.399963;r=.3+math.sqrt((i+.5)/9)*.88;placements.append((math.cos(a)*r,math.sin(a)*r,rng.random()*math.tau,.57+rng.random()*.31))
 for lod,label in [('near','grass_patch'),('mid','grass_patch_mid')]:
  source=MODELS/f'bep_nature_grass_broad_{lod}.glb';d,b,parts=geometry(source);images=copy.deepcopy(d['images'])
  for i,image in enumerate(images):
   v=d['bufferViews'][image['bufferView']];data=b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']];path=OUT/f'grass_source_{i}.png';path.write_bytes(data);image.pop('bufferView',None);image['uri']=str(path)
  writer=Writer(copy.deepcopy(d['materials']),copy.deepcopy(d['textures']),images);writer.g['samplers']=copy.deepcopy(d.get('samplers',writer.g['samplers']));groups={}
  for x,z,yaw,scale in placements:
   c,s=math.cos(yaw),math.sin(yaw);rotation=np.array([[c,0,s],[0,1,0],[-s,0,c]])
   for pos,n,uv,idx,material in parts:
    p=pos@rotation.T*scale+[x,0,z];normal=n@rotation.T;g=groups.setdefault(material,[[],[],[],[]]);offset=sum(len(k)for k in g[0]);g[0].append(p);g[1].append(normal);g[2].append(uv);g[3].append(idx+offset)
  for material,g in groups.items():writer.mesh(label+str(material),np.concatenate(g[0]),np.concatenate(g[1]),np.concatenate(g[2]),np.concatenate(g[3]),material)
  writer.save(OUT/(label+'.glb'),embedded=True)
 print('GRASS measured clumps',len(placements),'near/mid geometry retained from owned foliage',flush=True)

def rows():
 result=[];counts={}
 for name in FILES:
  p=PROJECT/'scenery'/f'{name}_instances.txt';text=p.read_text().split('# Visual upgrade authored layer')[0];counts[name]=0
  for number,line in enumerate(text.splitlines(),1):
   fields=line.split('#')[0].split()
   if not fields:continue
   alias=fields[0];u,v,yaw,scale,burial=map(float,fields[1:6]);elevation=float(fields[6])if len(fields)>6 else 0
   result.append(dict(alias=alias,x=(u-.5)*WIDTH,z=(v-.5)*DEPTH,yaw=yaw,scale=scale,burial=burial,elevation=elevation,file=p.name,line=number));counts[name]+=1
 return result,counts

def line(r):return f"{r['alias']} {r['x']/WIDTH+.5:.15f} {r['z']/DEPTH+.5:.15f} {r['yaw']:.6f} {r['scale']:.6f} {r.get('burial',0):.6f} {r.get('elevation',0):.6f}"
def facades(path):
 _,_,parts=geometry(path);tri=np.concatenate([p[0][p[3]]for p in parts]);cross=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);area=np.linalg.norm(cross,axis=1)*.5;n=cross/np.maximum(2*area[:,None],1e-9);out=[]
 for axis in [0,2]:
  for sign in [-1,1]:
   selected=(n[:,axis]*sign>.98)&(np.abs(n[:,1])<.05)&(area>.015)&(tri[:,:,1].mean(axis=1)>.6)&(tri[:,:,1].mean(axis=1)<4.5)
   ts=tri[selected];aa=area[selected]
   if not len(ts):continue
   planes=np.rint(ts[:,:,axis].mean(axis=1)*20)/20;weights={p:float(aa[planes==p].sum())for p in np.unique(planes)};plane=max(weights,key=weights.get);points=ts[planes==plane].reshape(-1,3);lo=points.min(axis=0);hi=points.max(axis=0)
   if hi[1]-lo[1]>1.8 and hi[2-axis]-lo[2-axis]>1.9:out.append((axis,sign,float(plane),lo,hi))
 return out

def compose():
 grass_clusters();original,counts=rows();budget=8192-len(original);records=[];rejected=[];cache={};rng=random.Random(816299)
 with tempfile.TemporaryDirectory(prefix='valwood-visual-author-')as temporary:
  atlas=Atlas(temporary);atlas.api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.POINTER(C.c_float),C.POINTER(C.c_float)];atlas.api.world_atlas_local_water.restype=C.c_bool
  def add(alias,x,z,yaw=0,scale=1,burial=0,elevation=0,**extra):
   assert len(records)<budget,'Scene record budget exceeded';r=dict(alias=alias,x=x,z=z,yaw=yaw,scale=scale,burial=burial,elevation=elevation,**extra);records.append(r);return r
  # Measured facade planes, rather than maximum roof/eave envelopes.
  buildings=sorted((r for r in original if r['alias']in ['purchased_house_a','purchased_house_b','purchased_house_c','purchased_house_d','purchased_house_e','purchased_house_f']and math.hypot(r['x']+1380,r['z']+10250)<160),key=lambda r:math.hypot(r['x']+1380,r['z']+10250))[:8]
  for index,parent in enumerate(buildings):
   stem=parent['alias'].replace('purchased_house','purchased_village_house')+'_near';path=MODELS/(stem+'.glb');cache.setdefault(stem,facades(path));c,s=math.cos(math.radians(parent['yaw'])),math.sin(math.radians(parent['yaw']))
   for axis,sign,plane,lo,hi in cache[stem][:2]:
    along=2-axis;span=hi[along]-lo[along];center=(lo[along]+hi[along])*.5-span*.22;local=[0,0,0];local[axis]=plane;local[along]=center
    nx,nz=(sign,0)if axis==0 else(0,sign);wx=c*nx+s*nz;wz=-s*nx+c*nz
    x=parent['x']+(c*local[0]+s*local[2])*parent['scale']+wx*.035;z=parent['z']+(-s*local[0]+c*local[2])*parent['scale']+wz*.035;scale=min(.94,parent['scale']*.95)
    bottom=min(max(lo[1]+.9,1.4),hi[1]-1.75)*parent['scale'];elevation=bottom-parent['burial']-.13*scale
    if not .5<elevation<3:continue
    add('visual_window_open'if index%3 else'visual_window_glazed',x,z,math.degrees(math.atan2(wx,wz)),scale,elevation=elevation,zone='castle_facade',parent=f"{parent['file']}:{parent['line']}",wall_plane=[axis,sign,plane])
  walls=sorted((r for r in original if r['alias']=='purchased_wall'and math.hypot(r['x']+1359,r['z']+10280)<70),key=lambda r:math.hypot(r['x']+1359,r['z']+10280))[:4]
  for parent in walls:
   path=MODELS/'purchased_castle_wall.glb'
   # Resolve the actual model stem from the existing cooker inventory when named differently.
   if not path.exists():path=MODELS/'purchased_village_wall_near.glb'
   if not path.exists():continue
   cache.setdefault(str(path),facades(path));c,s=math.cos(math.radians(parent['yaw'])),math.sin(math.radians(parent['yaw']));options=[]
   for axis,sign,plane,lo,hi in cache[str(path)]:
    nx,nz=(sign,0)if axis==0 else(0,sign);normal=(c*nx+s*nz,-s*nx+c*nz);score=normal[0]*(-1359-parent['x'])+normal[1]*(-10280-parent['z']);options.append((score,axis,plane,lo,hi,normal))
   if not options:continue
   _,axis,plane,lo,hi,normal=max(options,key=lambda v:v[0]);local=[0,0,0];local[axis]=plane;local[2-axis]=(lo[2-axis]+hi[2-axis])*.5;scale=.85
   add('visual_window_slit',parent['x']+(c*local[0]+s*local[2])*parent['scale']+normal[0]*.035,parent['z']+(-s*local[0]+c*local[2])*parent['scale']+normal[1]*.035,math.degrees(math.atan2(*normal)),scale,elevation=2.15-parent['burial']-.13*scale,zone='castle_wall',parent=f"{parent['file']}:{parent['line']}")
  def path_gap(x,z):return min(segment_distance((x,z),a,b)for a,b in zip(TRAIL,TRAIL[1:]))
  def plant(alias,x,z,scale,zone):
   radius={'visual_grass':1.7,'bep_nature_palm':.5,'showcase_roots':2.1,'showcase_shrub':.9,'bep_nature_fern':.8,'rock0':1.3,'showcase_sorrel':.65}[alias]*scale
   if path_gap(x,z)<radius+1.35 or math.hypot(x+26688,z-9670.4967)<radius+17 or math.hypot(x+26715,z-9615.4967)<radius+2.5:return False
   heights=[]
   for dx,dz in [(0,0),(-.6,0),(.6,0),(0,-.6),(0,.6)]:
    xx=x+dx*radius;zz=z+dz*radius;sample=atlas.sample(xx,zz)
    if sample.land<.7 or atlas.api.world_atlas_local_water(xx,zz,None,None):return False
    heights.append(sample.height)
   if max(heights)-min(heights)>(.65 if alias=='visual_grass' else 1.1):return False
   add(alias,x,z,rng.uniform(-180,180),scale,burial=.015 if alias!='showcase_roots'else .18,zone=zone,ground_height_range=heights,path_gap=path_gap(x,z),root_clearance=radius);return True
  # Broad leaves at intermediate height, with open paths and encounter reserve.
  for x,z in [(-26745,9725),(-26699,9719),(-26736,9698),(-26694,9683),(-26740,9665),(-26743,9600),(-26762,9634),(-26779,9617)]:plant('bep_nature_palm',x,z,2.2+rng.random()*.7,'canopy')
  centers=[(-26739,9721),(-26720,9713),(-26728,9688),(-26710,9683),(-26732,9655),(-26701,9644),(-26745,9597),(-26763,9603),(-26768,9628),(-26747,9640),(-26720,9598),(-26722,9635),(-26734,9591),(-26747,9593),(-26748,9637)]
  for index,(cx,cz)in enumerate(centers):
   for j in range(24):
    a=rng.random()*math.tau;r=1.7+math.sqrt(rng.random())*4;x=cx+math.cos(a)*r;z=cz+math.sin(a)*r
    if plant('visual_grass',x,z,.58+rng.random()*.3,'clustered_grass'):break
   # A second/third patch makes growth coherent rather than uniform isolated tufts.
   for k in range(4):
    for attempt in range(12):
     a=rng.random()*math.tau;r=2+math.sqrt(rng.random())*6;x=cx+math.cos(a)*r;z=cz+math.sin(a)*r
     if plant('visual_grass',x,z,.55+rng.random()*.37,'clustered_grass'):break
   for alias,scale in [('showcase_shrub',.75),('bep_nature_fern',.85),('showcase_sorrel',.7)]:
    for attempt in range(12):
     x=cx+rng.uniform(-5,5);z=cz+rng.uniform(-5,5)
     if plant(alias,x,z,scale,'layered_understory'):break
   if index%2==0:
    for attempt in range(12):
     x=cx+rng.uniform(-5,5);z=cz+rng.uniform(-5,5)
     if plant('showcase_roots',x,z,.65,'exposed_roots'):break
  # Close bank tufts frame the water without covering its reflective centre.
  for x,z in [(-26719,9605),(-26719,9609),(-26720,9612),(-26720,9617),(-26719,9621),(-26719,9625),(-26721,9603),(-26721,9627),(-26724,9600),(-26724,9630),(-26728,9597),(-26729,9633)]:
   plant('visual_grass',x,z,.6,'foreground_bank')
  assert len(records)<=budget
  # Source supports remain untouched. The added layer uses the existing tenth source.
  p=PROJECT/'scenery/demo_completion_instances.txt';base=p.read_text().split('# Visual upgrade authored layer')[0].rstrip();p.write_text(base+'\n# Visual upgrade authored layer\n'+'\n'.join(line(r)for r in records)+'\n')
  doc=PROJECT/'world.studio';text=doc.read_text().split('# Visual upgrade assets')[0];text='\n'.join('look .43 1.05 1.18 1.08 15.4 -24 .27 .06 .7 .3'if l.startswith('look ')else l for l in text.splitlines() if not l.startswith('asset visual_'))
  text+='\n'
  for style in ['open','glazed','slit']:text+=f'asset visual_window_{style} visual_upgrade/window_{style} 1 0 0 190 45 1\n'
  text+='asset visual_grass visual_upgrade/grass_patch 2 0 1 65 18 1\n';doc.write_text(text)
  graphics=PROJECT/'graphics.settings';values={'auto_quality':'1','render_scale':'1.0','min_render_scale':'0.7'};graphics.write_text('\n'.join(k+'='+values[k] if (k:=l.split('=',1)[0])in values else l for l in graphics.read_text().splitlines())+'\n')
  result=dict(seed=816299,base_records=sum(counts.values()),source_counts=counts,added=len(records),total_records=sum(counts.values())+len(records),record_cap=8192,placements=records,terrain_geometry_modified=False,walk_corridor_reserve_m=1.35,new_grass_clumps_per_patch=10,unmodified_authoring='Source world and prior presentation; this recipe edits only projects/valwood_visual')
  (REPORT/'composition.json').write_text(json.dumps(result,indent=2)+'\n');print('COMPOSED',len(records),'new placements; total',result['total_records'],'of8192',flush=True)
if __name__=='__main__':compose()

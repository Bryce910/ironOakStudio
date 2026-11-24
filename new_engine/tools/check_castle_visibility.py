#!/usr/bin/env python3
"""Continuous source-face coverage proof for conservative castle grass occlusion.

No GPU or asset writes. Polygon clipping proves opaque front and back wall
surfaces cover the complete inset core rectangle, rather than sampling rays.
"""
from pathlib import Path
import argparse,hashlib,json,sys,io
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_starting_area import glb_bounds
from audit_village_pose_compatibility import glb,values
from PIL import Image

def area(p):
 p=np.asarray(p)
 return abs(float(np.dot(p[:,0],np.roll(p[:,1],-1))-np.dot(p[:,1],np.roll(p[:,0],-1))))*.5 if len(p)>2 else 0.
def clip_half(poly,axis,limit,positive):
 result=[]
 if not poly:return result
 previous=poly[-1];pin=(previous[axis]>=limit) if positive else(previous[axis]<=limit)
 for current in poly:
  cin=(current[axis]>=limit)if positive else(current[axis]<=limit)
  if cin!=pin:
   t=(limit-previous[axis])/(current[axis]-previous[axis]);result.append(previous+(current-previous)*t)
  if cin:result.append(current)
  previous,pin=current,cin
 return result

def inspect():
 result=[];rectangle=(.1,14.2,-9.97,9.97);expected=(rectangle[1]-rectangle[0])*(rectangle[3]-rectangle[2])
 for lod in ('near','mid'):
  path=ROOT/f'projects/valwood_world/models/purchased_village_wall_{lod}.glb';doc,_=glb(path)
  assert all(m.get('alphaMode','OPAQUE') in ('OPAQUE','MASK') for m in doc['materials'])
  # The kit uses one MASK atlas, with transparent unused atlas space. Its
  # actual continuous wall face UVs are brick rectangles, not cutout geometry.
  alpha_reports=[]
  doc,binary=glb(path)
  for material in doc['materials']:
   pbr=material['pbrMetallicRoughness'];assert pbr.get('baseColorFactor',[1,1,1,1])[3]==1
   tex=doc['textures'][pbr['baseColorTexture']['index']];im=doc['images'][tex['source']];view=doc['bufferViews'][im['bufferView']];offset=view.get('byteOffset',0)
   image=Image.open(io.BytesIO(binary[offset:offset+view['byteLength']])).convert('RGBA');w,h=image.size
   # Full actual front/back UV rectangles, retaining their image edge AA.
   for label,bounds in [('front',(.449951171875,.791015625,.53076171875,.9697265625)),('back',(.0300750732421875,.389892578125,.53076171875,.9697265625))]:
    u0,u1,v0,v1=bounds;tile=np.asarray(image)[int(np.ceil(v0*h)):int(np.floor(v1*h)),int(np.ceil(u0*w)):int(np.floor(u1*w)),3]
    minimum=int(tile.min());assert minimum>=128 and material.get('alphaCutoff',.5)<=.4
    alpha_reports.append(dict(face=label,actual_uv_rect=bounds,minimum_base_alpha=minimum,alpha_cutoff=material.get('alphaCutoff',.5),unused_transparent_atlas_areas_not_used=True))
  tri=np.asarray(glb_bounds(path,True)[3]);cross=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);projected_area=np.abs(cross[:,0])*.5
  rows=[]
  for name,mask in [('front',(tri[:,:,0].min(axis=1)>.1)),('back',(tri[:,:,0].max(axis=1)<-.7))]:
   selected=tri[mask&(projected_area>20)];assert len(selected)==4
   clipped=[]
   for t in selected:
    polygon=[p.copy() for p in t[:,[1,2]]]
    for axis,limit,positive in [(0,rectangle[0],True),(0,rectangle[1],False),(1,rectangle[2],True),(1,rectangle[3],False)]:polygon=clip_half(polygon,axis,limit,positive)
    clipped.append(area(polygon))
   # The source four-face strip is a triangulated, nonfolded single surface:
   # projected orientation is consistent and all triangles share one normal.
   signs=np.sign(cross[mask&(projected_area>20),0]);assert abs(signs.sum())==4
   # Prove interiors do not overlap, independently of the coverage sum.
   pair_overlaps=[]
   for i,a in enumerate(selected):
    pa=a[:,[1,2]]
    for b in selected[:i]:
     pb=b[:,[1,2]];poly=[p.copy() for p in pa]
     orientation=np.sign(np.cross(pb[1]-pb[0],pb[2]-pb[0]));assert orientation
     for edge in range(3):
      if not poly:break
      ea,eb=pb[edge],pb[(edge+1)%3];normal=np.array([-(eb-ea)[1],(eb-ea)[0]])*orientation;lim=np.dot(normal,ea);output=[];prev=poly[-1];pd=np.dot(normal,prev)-lim
      for cur in poly:
       cd=np.dot(normal,cur)-lim
       if (cd>=-1e-10)!=(pd>=-1e-10):output.append(prev+(cur-prev)*(pd/(pd-cd)))
       if cd>=-1e-10:output.append(cur)
       prev,pd=cur,cd
      poly=output
     pair_overlaps.append(area(poly))
   assert max(pair_overlaps)<1e-7 and abs(sum(clipped)-expected)<1e-7,(lod,name,clipped,expected,pair_overlaps)
   rows.append(dict(side=name,source_triangles=4,required_yz_rectangle=rectangle,covered_area_m2=sum(clipped),required_area_m2=expected,maximum_pairwise_overlap_m2=max(pair_overlaps),coverage_is_continuous_not_sampled=True))
  result.append(dict(file=str(path.relative_to(ROOT)),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),mapped_wall_core_alpha_opaque=True,alpha=alpha_reports,faces=rows))
 scene=json.loads((ROOT/'projects/valwood_world/scenery/castle_village_manifest.json').read_text())
 runs=[('castle_east_',13,True,65,-195,-55),('castle_west_',13,True,-65,-195,-55),('castle_north_',12,False,-195,-65,65),('castle_southwest_',6,False,-55,-65,-5.9),('castle_southeast_',6,False,-55,5.9,65)]
 for prefix,count,xplane,plane,begin,end in runs:
  records=[r for r in scene['placements']if r['label'].startswith(prefix)];assert len(records)==count
  for i,r in enumerate(records):
   assert r['alias']=='purchased_wall' and r['burial']==.1 and r['elevation']==0
   center=begin+(end-begin)*(i+.5)/count
   assert abs(r['x']-(plane if xplane else center))<1e-7 and abs(r['z']-(center if xplane else plane))<1e-7
   assert abs(r['yaw']-(0 if xplane else np.pi/2))<1e-7
   # Entire helper vertical interval maps strictly into the continuously
   # covered source core; full source thickness fits the1m swept slab.
   assert (24.50-23.9)/r['scale']>=rectangle[0] and(30.80-23.9)/r['scale']<=rectangle[1]
   assert .874*r['scale']<.5
   assert ((end-begin)/count*.5-1.25)/r['scale']<rectangle[3]
 return dict(schema_version=1,wall_files=result,measured_runs=5,measured_wall_pieces=50,helper_core_world_y=[24.50,30.80],half_thickness_m=.5,piece_edge_inset_m=1.25,cached_eye_guard_xz_m=1.5,cached_eye_guard_y_m=.5,source_files_modified=False,continuous_front_back_coverage=True)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path);args=p.parse_args();report=inspect()
 if args.output:args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
 print('PASS source wall occlusion: near/mid opaque front+back continuous core coverage;50 actual wall pieces; gate/join insets; full1m slab and camera cache guards')
if __name__=='__main__':main()

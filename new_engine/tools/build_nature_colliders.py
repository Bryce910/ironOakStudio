#!/usr/bin/env python3
"""Measured ground-level collision for the purchased citadel assemblies."""
from pathlib import Path
import ctypes,json,math,subprocess
import numpy as np
from compose_visual_upgrade import geometry
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'assets/purchased/nature_castle/citadel'

def rectangles(mask):
 mask=mask.copy();result=[]
 for z in range(mask.shape[0]):
  x=0
  while x<mask.shape[1]:
   if not mask[z,x]:x+=1;continue
   right=x+1
   while right<mask.shape[1]and mask[z,right]:right+=1
   end=z+1
   while end<mask.shape[0]and mask[end,x:right].all():end+=1
   mask[z:end,x:right]=0;result.append((x,right,z,end));x=right
 return result

def main():
 lib=ROOT.parent/'.local-tools/unity-source/nature_collision_raster.so'
 subprocess.run(['g++','-O2','-shared','-fPIC',str(ROOT/'tools/nature_collision_raster.cpp'),'-o',str(lib)],check=True)
 native=ctypes.CDLL(str(lib));native.nature_collision_raster.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_float,ctypes.c_float,ctypes.c_float,ctypes.c_int,ctypes.c_int,ctypes.c_float,ctypes.c_float,ctypes.c_void_p]
 catalog=json.loads((DATA/'catalog.json').read_text());lines=['// Generated from purchased visible meshes; ground-level doorways remain open.','static const VillageCollisionBox nature_collision_boxes[]={'];report={}
 for asset in catalog:
  name=asset['name']
  if name in ['floor','hall_vault','altar_roof','roof','tower_roof','parapet','torch','sculpture','stair','hall_stairs','bridge_pier']:continue
  _,_,parts=geometry(ROOT/asset['files'][0]);positions=np.concatenate([p[0]for p in parts]);lo=positions.min(0);hi=positions.max(0);cell=.30
  w=math.ceil((hi[0]-lo[0])/cell);d=math.ceil((hi[2]-lo[2])/cell);mask=np.zeros((d,w),np.uint8)
  for pos,n,uv,index,material in parts:
   p=np.ascontiguousarray(pos,dtype=np.float32);idx=np.ascontiguousarray(index.ravel(),dtype=np.int32)
   native.nature_collision_raster(p.ctypes.data,idx.ctypes.data,len(idx),lo[0],lo[2],cell,w,d,.24,2.5,mask.ctypes.data)
  rects=rectangles(mask);boxes=[]
  for x0,x1,z0,z1 in rects:
   box=[lo[0]+x0*cell,lo[0]+x1*cell,lo[2]+z0*cell,lo[2]+z1*cell,0.,2.8]
   lines.append('{"nm_'+name+'",'+','.join(f'{v:.6f}f'for v in box)+'},');boxes.append(box)
  report[name]={'boxes':boxes,'cell_m':cell,'occupied_square_metres':float(mask.sum()*cell*cell)}
 lines.append('};')
 (ROOT/'engine/world_nature_colliders.inc').write_text('\n'.join(lines)+'\n')
 (DATA/'collision.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Measured',len(report),'ground-level assets;',sum(len(v['boxes'])for v in report.values()),'collision boxes')

if __name__=='__main__':main()

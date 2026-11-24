"""Idempotent cinematic dressing upgrade; preserve authored objects and heightfield.
Original scene files are backed up by the engine pass in backups/cinematic-pass.
"""
from pathlib import Path
import re,math,random,struct
ROOT=Path(__file__).resolve().parents[1];P=ROOT/'projects/seldonne'
res=512;heights=struct.unpack('<'+str(res*res)+'f',(P/'terrain_sculpt.bin').read_bytes())
def height(x,z):
 u=max(0,min(res-1,(x+250)/500*(res-1)));v=max(0,min(res-1,(z+250)/500*(res-1)))
 ix,iz=int(u),int(v);jx,jz=min(ix+1,res-1),min(iz+1,res-1);fx,fz=u-ix,v-iz
 return 3+(heights[iz*res+ix]*(1-fx)+heights[iz*res+jx]*fx)*(1-fz)+(heights[jz*res+ix]*(1-fx)+heights[jz*res+jx]*fx)*fz
text=(P/'props_custom.txt').read_text()
# Existing low-fidelity plants retain their placement and identity.
text=text.replace('models/chestnut.glb','models/silver_fir.glb').replace('models/canopy_tree.glb','models/silver_fir.glb')
text=text.replace('models/fern.glb','models/woodland_fern_1.glb').replace('models/boulders.glb','models/mossy_boulder_0.glb')
marker='# Cinematic woodland dressing v1'
if marker in text:text=text[:text.index(marker)]
text+=marker+'\n';uid=70000;rng=random.Random(9381)
def place(model,x,z,scale,yaw=0):
 global text,uid
 uid+=1;text+=f'PROP models/{model}.glb {x:.3f} {height(x,z):.3f} {z:.3f} 0 {yaw:.2f} 0 {scale:.3f} 0 0 0 1 0 0 2.5 90 {uid}\nEND\n'
# Frame the arrival, leave the road and village silhouettes open.
for x,z,s in [(-32,43,1.25),(-30,9,1.10),(5,38,1.18),(14,23,1.08),(-46,-26,1.27),(30,-53,1.3),(-47,37,1.15),(20,50,.95)]:place('silver_fir',x,z,s,rng.uniform(0,360))
for i,(x,z,s) in enumerate([(-22,31,1.0),(-31,33,.9),(2,34,1.2),(7,22,.8),(-26,12,.9),(-39,-8,1.0),(31,-11,.9),(31,-36,1.1),(-37,48,.8)]):
 place('mossy_boulder_'+str(i%3),x,z,s,rng.uniform(0,360))
# Mixed-height clusters around trees and stones create an undergrowth layer.
for cx,cz in [(-25,34),(-29,20),(10,28),(20,42),(-37,-1),(-22,31),(7,22),(-26,12),(-32,43),(5,38),(-37,48),(-39,-8)]:
 for i in range(6):
  angle=rng.uniform(0,math.tau);r=rng.uniform(1.2,4.5);x=cx+math.cos(angle)*r;z=cz+math.sin(angle)*r
  if abs(x+14)<4 and z>20:continue
  place('woodland_fern_'+str(i%3),x,z,rng.uniform(.72,1.25),rng.uniform(0,360))
(P/'props_custom.txt').write_text(text)
print('Added woodland dressing; total prop records:',text.count('PROP '))

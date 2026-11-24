#!/usr/bin/env python3
"""Plan forge-wing detours from measured model collision, then rehearse natively."""
from pathlib import Path
import argparse,collections,hashlib,heapq,json,math,re
import numpy as np
from compose_visual_upgrade import geometry

ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'assets/purchased/nature_castle/citadel';DOC=ROOT/'docs/stronghold/2026-11-01/citadel'
X=-43344.;Z=-23987.824156518269

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--check-route',action='store_true');args=parser.parse_args()
 text=(DOC/'candidate.world.studio').read_text().splitlines();definitions={};props=[]
 for line in text:
  f=line.split()
  if f[0]=='asset':definitions[f[1]]=(f[2],int(f[4]))
  if f[0]=='prop':props.append((f[1],list(map(float,f[2:]))))
 measured=json.loads((DATA/'collision.json').read_text());cache={};resolution=.35
 xmin,zmin=-130.,-245.;width=math.ceil(260/resolution)+1;depth=math.ceil(550/resolution)+1
 occupied=np.zeros((depth,width),bool);obstacles=[];label=''
 def stamp(x0,x1,z0,z1,r=.53):
  ax=max(0,math.floor((x0-r-xmin)/resolution));bx=min(width,math.ceil((x1+r-xmin)/resolution)+1)
  az=max(0,math.floor((z0-r-zmin)/resolution));bz=min(depth,math.ceil((z1+r-zmin)/resolution)+1)
  if ax<bx and az<bz:occupied[az:bz,ax:bx]=True;obstacles.append((ax,bx,az,bz,label))
 for alias,values in props:
  label=alias
  x,z,yaw,scale,burial,elevation,offset,pitch,roll,sx,sy,sz=values
  x-=X;z-=Z
  if offset>3 or abs(x)>140 or z>310 or z< -250:continue
  stem,collision=definitions.get(alias,(alias,2 if alias=='starting_barrel'else 1))
  if not collision:continue
  if stem not in cache:
   path=ROOT/'projects/valwood_stronghold/models'/(stem+'.glb')
   if not path.exists():path=path.with_name(path.stem+'_near.glb')
   _,_,parts=geometry(path);points=np.concatenate([p[0]for p in parts]);cache[stem]=(points.min(0),points.max(0))
  lo,hi=cache[stem]
  if alias.startswith('nm_')and collision==4 and sx==sy==sz==1:
   boxes=measured.get(alias[3:],{}).get('boxes',[])
   if not boxes:boxes=[[lo[0],hi[0],lo[2],hi[2],lo[1],hi[1]]]
  elif collision in [2,3]:
   radius=.3 if collision==3 else max(abs(lo[0]),abs(hi[0]),abs(lo[2]),abs(hi[2]))
   boxes=[[-radius,radius,-radius,radius,0,hi[1]]]
  else:boxes=[[lo[0],hi[0],lo[2],hi[2],lo[1],hi[1]]]
  angle=math.radians(yaw);c=math.cos(angle);s=math.sin(angle)
  for box in boxes:
   points=[(a*scale*sx,b*scale*sz)for a in box[:2]for b in box[2:4]]
   corners=[(x+c*a+s*b,z-s*a+c*b)for a,b in points]
   stamp(min(p[0]for p in corners),max(p[0]for p in corners),min(p[1]for p in corners),max(p[1]for p in corners))
 # Static residents and the swept strips of the existing hall patrols.
 header=(ROOT/'engine/world_stronghold_data.h').read_text();body=re.search(r'stronghold_stations\[\]=\{\n(.*?)\n\};',header,re.S)[1]
 for line in body.splitlines():
  label='resident'
  f=line.strip('{} ,').split(',');ax=float(f[1])-X;az=float(f[2])-Z;bx=float(f[6])-X;bz=float(f[7])-Z
  stamp(min(ax,bx),max(ax,bx),min(az,bz),max(az,bz),1)
 def cell(x,z):return(round((z-zmin)/resolution),round((x-xmin)/resolution))
 def world(c):return[xmin+c[1]*resolution,zmin+c[0]*resolution]
 if args.check_route:
  route=json.loads((DOC/'layout.json').read_text())['residents']['route_local_xz'];blocked=[]
  for i,(a,b)in enumerate(zip(route,route[1:])):
   steps=max(1,math.ceil(math.dist(a,b)/.1));bad=[]
   for t in range(steps+1):
    p=[a[j]+(b[j]-a[j])*t/steps for j in range(2)]
    if occupied[cell(*p)]:bad.append(p)
   if bad:
    cz,cx=cell(*bad[0]);labels=sorted(set(o[4]for o in obstacles if o[0]<=cx<o[1]and o[2]<=cz<o[3]))
    blocked.append({'segment':i,'from':a,'to':b,'first_occupied':bad[0],'last_occupied':bad[-1],'obstacles':labels})
  print(json.dumps({'conservative_grid_occupied_segments':blocked},indent=2));return
 start=cell(0,-55.2);assert not occupied[start]
 def plan(side):
  goal=cell(side*53,-101);queue=[(0,start)];cost={start:0};parent={start:None};found=None
  while queue:
   _,at=heapq.heappop(queue)
   if at==goal:found=at;break
   for dz,dx in [(0,1),(1,0),(0,-1),(-1,0)]:
    nxt=(at[0]+dz,at[1]+dx)
    if not(0<=nxt[0]<depth and 0<=nxt[1]<width)or occupied[nxt]:continue
    value=cost[at]+1
    if value>=cost.get(nxt,1e99):continue
    cost[nxt]=value;parent[nxt]=at;heuristic=abs(nxt[0]-goal[0])+abs(nxt[1]-goal[1]);heapq.heappush(queue,(value+heuristic,nxt))
  if found is None:raise ValueError(f'Forge wing {side} is disconnected')
  path=[]
  while found is not None:path.append(found);found=parent[found]
  path.reverse();compressed=[path[0]]
  def clear(a,b):
   steps=max(abs(a[0]-b[0]),abs(a[1]-b[1]))*4+1
   return all(not occupied[round(a[0]+(b[0]-a[0])*t/steps),round(a[1]+(b[1]-a[1])*t/steps)]for t in range(steps+1))
  at=0
  while at<len(path)-1:
   nxt=len(path)-1
   while nxt>at+1 and not clear(path[at],path[nxt]):nxt-=1
   compressed.append(path[nxt]);at=nxt
  return[[round(v,3)for v in world(c)]for c in compressed]
 result={'version':2,'source_document_sha256':hashlib.sha256((DOC/'candidate.world.studio').read_bytes()).hexdigest(),'collision_cell_m':resolution,'left':plan(-1),'right':plan(1),'native_validation_required':True}
 (DOC/'wing-routes.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

if __name__=='__main__':main()

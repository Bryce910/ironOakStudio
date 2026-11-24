#!/usr/bin/env python3
"""CPU-only, actual native Jolt audit of installed source-city walking lanes.

Includes positive open-lane queries and a deliberately blocked house crossing.
No game launch, GPU work, scene/save edits or claim of whole-city navigation.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,tempfile,struct
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def native_checker():
    folder=Path(tempfile.mkdtemp(prefix='valwood-city-collision-check-'));obj=folder/'walk.o';exe=folder/'check'
    subprocess.run(['gcc','-std=c99','-O2','-I.','-c','engine/world_walk.c','-o',str(obj)],cwd=ROOT,check=True)
    subprocess.run(['g++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-I.','tools/check_city_collision.cpp','engine/world_physics.o',str(obj),'build/physics/libJolt.a','-pthread','-o',str(exe)],cwd=ROOT,check=True)
    return exe

def sample(path,x,z):
    raw=path.read_bytes();magic,cols,rows,xmin,zmin,sx,sz=struct.unpack_from('<8sIIdddd',raw)
    assert magic==b'VWHGT1\0\0';values=np.frombuffer(raw,dtype='<f4',offset=48).reshape(rows,cols)
    gx,gz=(x-xmin)/sx,(z-zmin)/sz
    assert 0<=gx<=cols-1 and 0<=gz<=rows-1
    ix,iz=min(cols-2,int(gx)),min(rows-2,int(gz));tx,tz=gx-ix,gz-iz
    return float((values[iz,ix]*(1-tx)+values[iz,ix+1]*tx)*(1-tz)+(values[iz+1,ix]*(1-tx)+values[iz+1,ix+1]*tx)*tz)

def exact_ground(vertices,triangles,x,z):
    v=vertices[triangles];a,b,c=v[:,0,[0,2]],v[:,1,[0,2]],v[:,2,[0,2]]
    den=(b[:,1]-c[:,1])*(a[:,0]-c[:,0])+(c[:,0]-b[:,0])*(a[:,1]-c[:,1]);safe=np.where(np.abs(den)>1e-12,den,np.nan)
    w=((b[:,1]-c[:,1])*(x-c[:,0])+(c[:,0]-b[:,0])*(z-c[:,1]))/safe
    u=((c[:,1]-a[:,1])*(x-c[:,0])+(a[:,0]-c[:,0])*(z-c[:,1]))/safe;q=1-w-u
    valid=(w>=-1e-7)&(u>=-1e-7)&(q>=-1e-7)
    if not valid.any():raise ValueError('Spawn is outside actual source ground triangles')
    return float((w*v[:,0,1]+u*v[:,1,1]+q*v[:,2,1])[valid].max())

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--project',type=Path,default=ROOT/'projects/valwood_city_showcase');p.add_argument('--output',type=Path);args=p.parse_args();project=args.project.resolve()
    meta=json.loads((project/'city_showcase.json').read_text());origin=np.array(meta['origin']);config=dict(row.split('=',1)for row in (project/'project.valwood').read_text().splitlines()if'='in row);sx,sz,yaw=map(float,config['spawn'].split())
    folder=(args.output or project/'qa/collision').resolve();folder.mkdir(parents=True,exist_ok=True);checker=native_checker()
    routes=[('statue_lane',[(12,-5),(12,5)],True),('square_cross_lane',[(12,5),(4,5)],True),('market_lane',[(4,5),(4,15)],True),('blocked_house_crossing',[(15,-8),(15,3)],False)]
    spawn=[sx-origin[0],sz-origin[2]]
    routes += [('spawn_'+name,[spawn,[spawn[0]+dx,spawn[1]+dz]],True)for name,dx,dz in [('east',1,0),('west',-1,0),('north',0,-1),('south',0,1)]]
    report=[];good=True
    for name,points,expected_clear in routes:
        path=folder/(name+'.walk');path.write_text('VALWOOD_CITY_WALK 1\n'+''.join(f'p{i} {origin[0]+x:.14g} {origin[2]+z:.14g}\n'for i,(x,z)in enumerate(points)))
        r=subprocess.run([str(checker),str(project/'city.physics'),str(project/'ground/showcase.height'),str(path)],capture_output=True,text=True)
        result=json.loads(r.stdout)if r.stdout.strip().startswith('{')else dict(error=r.stderr)
        passed=(r.returncode==0)if expected_clear else(r.returncode==1 and result.get('blocked',0)>0 and result.get('slope_failures',1)==0 and result.get('outside_source_ground',1)==0)
        report.append(dict(name=name,expected_clear=expected_clear,passed=passed,result=result,route=str(path),diagnostic=r.stderr));good &=passed
    source=ROOT.parent/'.local-tools/village-source/source-ground-native.npz';ground=np.load(source);native_height=sample(project/'ground/showcase.height',sx,sz);source_height=exact_ground(ground['vertices'],ground['triangles'],*spawn)+origin[1]
    ground_ok=abs(source_height-native_height-.05)<.025;good &=ground_ok
    result=dict(kind='Bounded CPU source-city walking audit; actual native Jolt and terrain slope queries',passed=bool(good),project=str(project),source_catalog=meta['active_geometry_catalog'],source_catalog_sha256=meta['catalog_sha256'],collision_sha256=digest(project/'city.physics'),height_sha256=digest(project/'ground/showcase.height'),source_ground_sha256=digest(source),spawn=dict(world=[sx,native_height+1.75,sz],yaw_radians=yaw,source_xz=spawn,source_ground_y=source_height,walking_support_y=native_height,below_visual_ground_m=source_height-native_height,support_matches=bool(ground_ok)),routes=report,scope='Three connected source square/market lanes and startup clearance. Whole-city interiors, high castle backdrops and complete game quests are not audited.')
    (folder/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit(0 if good else 1)
if __name__=='__main__':main()

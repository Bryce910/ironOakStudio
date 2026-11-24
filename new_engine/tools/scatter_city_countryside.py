"""Source-family canopy and composed layered groundcover for the city outskirts."""
import copy,json,math
from pathlib import Path
from collections import Counter
import numpy as np
from build_city_countryside import ROOT,PRIVATE,BOUNDS,native_points

def add_dressing(catalog,land,path):
    data=json.loads(Path(path).read_text());rock=ROOT/'assets/purchased/bep_reforged/countryside-rock/catalog.json'
    if rock.exists():data['assets']+=json.loads(rock.read_text())['assets']
    catalog['assets']+=copy.deepcopy(data['assets']);lookup={a['native']['alias']:a for a in data['assets']};counts=Counter();rng=np.random.default_rng(11022026);records=catalog['placements']
    # Conservative roof/trunk guards include the explicitly relocated edge house.
    source_assets={a['name']:a for a in catalog['assets']};boxes=[]
    for p in catalog['placements']:
        if not p['asset'].startswith(('SM_House_','SM_Boundary_')):continue
        a=source_assets[p['asset']];lo,hi=np.asarray(a['bounds']);m=np.asarray(p['matrix_native']).reshape(4,4).T.copy();m[:3,3]+=m[:3,:3]@np.asarray(a['pivot_offset_native']);v=np.array([[x,y,z,1]for x in [lo[0],hi[0]]for y in [lo[1],hi[1]]for z in [lo[2],hi[2]]])@m.T;boxes.append((v[:,[0,2]].min(0),v[:,[0,2]].max(0)))
    def valid(p,margin=0):
        if not (BOUNDS[0]+5<p[0]<BOUNDS[2]-5 and BOUNDS[1]+5<p[1]<BOUNDS[3]-5):return False
        if np.all(p>=land.lo-margin)&np.all(p<=land.hi+margin):return False
        if np.linalg.norm((p-np.array([-134.5,202.6]))/np.array([90,90]))<1:return False
        return not any(np.all(p>=lo-margin)&np.all(p<=hi+margin)for lo,hi in boxes)
    def insert(alias,p,scale,yaw,source=None,grounded=False):
        scale=max(.16,scale)
        a=lookup[alias];m=np.eye(4);cs,sn=math.cos(yaw),math.sin(yaw);x=np.array([cs,0,-sn]);normal=np.array([0.,1,0.])
        if grounded:
            eps=.25;dx=float(land.height(p+[eps,0])-land.height(p-[eps,0]))/(2*eps);dz=float(land.height(p+[0,eps])-land.height(p-[0,eps]))/(2*eps);normal=np.array([-dx,1,-dz]);normal/=np.linalg.norm(normal);x-=normal*np.dot(x,normal);x/=np.linalg.norm(x)
        m[:3,:3]=np.stack((x,normal,np.cross(x,normal)),axis=1)*scale;m[[0,2],3]=p;m[1,3]=float(land.height(p))-land.origin[1]+(.6*scale if alias=='bep_nature_rock'else.015)
        if land.origin[1]+m[1,3]+a['pivot_offset_native'][1]*scale<.1:return False
        row=dict(asset=a['name'],matrix_native=m.T.flatten().tolist(),authored_extension='Layered BEP countryside dressing')
        if source:row['source_canopy']=source
        records.append(row);counts[alias]+=1;return True
    # Retain actual artist tree root distribution for the selected crop, using
    # the verified native representative of each recovered botanical family.
    terrain=json.loads((PRIVATE/'terrain-Isle_01.json').read_text());s=land.source;roots=[]
    def canopy_clear(p):
        # A broad sunny meadow frames the castle; groves occupy its edges.
        if np.linalg.norm((p-np.array([-151.,-29.]))/np.array([39.,27.]))<1:return False
        return True
    native=native_points(s['tree_position_local_xyz'][:,[0,2]])
    for i in rng.permutation(len(native)):
        p=native[i];proto=int(s['tree_prototype_index'][i]);name=terrain['tree_prototypes'][proto]['prefab'].get('path','')
        alias='bep_nature_oak'if'/Trees/Oak/'in name else('bep_nature_beech'if'/Trees/Beech/'in name else None)
        if not alias or not valid(p,12) or not canopy_clear(p):continue
        d,_=land.road_tree.query(p)
        if d<11 or any(np.linalg.norm(p-q)<14 for q in roots):continue
        scale=float(np.clip(s['tree_height_scale'][i],.8,1.1))*(.43 if alias.endswith('beech')else.68)
        if insert(alias,p,scale,float(s['tree_rotation_radians'][i]),dict(index=int(i),prototype=name,variant='Verified family representative; source root retained, Y adapted to terrain')):roots.append(p)
        if len(roots)>=130:break
    # Fill immediate approach groves where source canopy fell on excluded town.
    for _ in range(1500):
        if len(roots)>=145:break
        p=rng.uniform([-202,-100],[-67,95]);d,_=land.road_tree.query(p)
        if not valid(p,12)or not canopy_clear(p)or d<11 or d>60 or any(np.linalg.norm(p-q)<13 for q in roots):continue
        alias='bep_nature_oak'if rng.random()<.65 else'bep_nature_beech';scale=rng.uniform(.55,.80)if alias.endswith('oak')else rng.uniform(.32,.50)
        if insert(alias,p,scale,rng.uniform(0,math.tau)):roots.append(p)
    goals=[('grass',4050),('clover',650),('fern',550),('bush',210),('rock',90)]
    for kind,goal in goals:
        done=0
        for _ in range(goal*35):
            if done>=goal:break
            # Most density surrounds the walkable woodland loop, with outer
            # meadow pockets to soften the distant terrain and village edges.
            p=rng.uniform([-209,-112],[-48,107])if rng.random()<.88 else rng.uniform(BOUNDS[:2],BOUNDS[2:])
            dist,_=land.road_tree.query(p)
            if not valid(p,.4)or dist<3.2 or (dist>65 and rng.random()<.90):continue
            if kind in ['fern','bush']:
                # Shade plants belong to groves; the central glade stays grass.
                if not canopy_clear(p) or not roots or min(np.linalg.norm(p-q)for q in roots)>12:continue
            if kind=='grass':alias='bep_nature_grass_long'if rng.random()<.55 else'bep_nature_grass_broad';scale=rng.uniform(.72,1.35)
            elif kind=='clover':alias='bep_nature_clover';scale=rng.uniform(.7,1.6)
            elif kind=='fern':alias='bep_nature_fern';scale=rng.uniform(.65,1.25)
            elif kind=='bush':
                if dist<5:continue
                alias='bep_nature_bush_a'if rng.random()<.55 else'bep_nature_bush_b';scale=rng.uniform(.12,.26)
            else:
                alias='bep_nature_rock';scale=rng.uniform(.28,.85)
                if alias not in lookup:break
            if insert(alias,p,scale,rng.uniform(0,math.tau),grounded=kind in ['grass','clover','fern']):done+=1
    if len(records)>8192:raise ValueError('Countryside exceeded bounded scenery record budget')
    if max(counts.values(),default=0)>4096:raise ValueError('Countryside exceeded per-alias buffer')
    catalog['countryside']['dressing']=dict(counts=dict(counts),records=sum(counts.values()),source_asset_catalog=str(Path(path).resolve()),sunny_meadow=dict(center=[-151,-29],radii=[39,27]),minimum_tree_spacing_m=13,canopy='Source root distribution/family representative adapted to this scene; new source-based fill groves',groundcover='New clustered and layered dressing, not original source placement claims',seed=11022026,road_clearance_m=3.2)


def fit_existing_contacts(catalog,project):
    """Keep every existing XZ/identity; fit soil contacts without rerandomizing."""
    from countryside_contact import TerrainSurfaces,correct
    from build_city_countryside import readfield,sample
    project=Path(project);origin=np.asarray(json.loads((project/'city_showcase.json').read_text())['origin'])
    primary,po,ps=readfield(project/'ground/showcase.height');country,co,cs=readfield(project/'ground/countryside.height')
    def fallback(points):
        p=np.asarray(points);inside=np.all(p>=po,axis=-1)&np.all(p<=po+ps*(np.array(primary.shape)[::-1]-1),axis=-1)
        h=sample(country,p,co,cs);h[inside]=sample(primary,p[inside],po,ps);return h
    terrain=TerrainSurfaces(catalog,origin,fallback)
    audit=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-grass-clumps/source-audit.json').read_text())
    return correct(catalog,terrain,audit)


if __name__=='__main__':
    import argparse,hashlib
    parser=argparse.ArgumentParser(description='Stage existing countryside soil-contact corrections; no project or source edits')
    parser.add_argument('--catalog',type=Path,required=True);parser.add_argument('--project',type=Path,default=ROOT/'projects/valwood_city_showcase');parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    if (args.output.resolve()/'catalog.json')==args.catalog.resolve():parser.error('Output must be an isolated candidate directory')
    catalog=json.loads(args.catalog.read_text());candidate,report=fit_existing_contacts(catalog,args.project)
    report['source_catalog_sha256']=hashlib.sha256(args.catalog.read_bytes()).hexdigest();report['source_catalog']=str(args.catalog.resolve())
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'catalog.json').write_text(json.dumps(candidate,indent=2,allow_nan=False)+'\n');(args.output/'contact-report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps({k:v for k,v in report.items()if k not in ['before','after','changes','scaled']},indent=2))

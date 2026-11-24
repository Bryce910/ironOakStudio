#!/usr/bin/env python3
"""Stage two bounded Grandstone collision sectors from purchased source meshes.

No rendered model, material, project, save or original source is changed. Walls
use original selected MID geometry with near-only structural fallback; original
HIGH floor/stair/carpet triangles retain the visible walking surface. Cosmetic
metal overlays, decals and roof ornaments do not become snagging
colliders. Each source LOD object contributes exactly one mesh. The exterior
staircase is kept in full rather than ending collision midway down the flight.
"""
from pathlib import Path
import argparse
import collections
import functools
import hashlib
import json
import os
import re
import tempfile
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT.parent/'.local-tools/unity-source/showcase-native/scene.json'
ORIGIN=np.array([-43344.-500,1130.8125,-23987.82415651827+500])
PROFILES={
    'approach':dict(lo=[347,27,475],hi=[432,101,552],
        regions=[dict(lo=[347,79,506],hi=[375,100,551]),
                 dict(lo=[370,27,475],hi=[430,85,541],prefix='Stairs/Fence Generator/'),
                 # The purchased scene places a small rock on exterior step37.
                 # Keep its physical surface so players step on/around it.
                 dict(lo=[374.5,50,479],hi=[382,58,487],prefix='Spawned Rocks/')],
        spawn=[356.26292896294,83.13901521075,538.92477398462],yaw=1.81577498992176,
        title='Snowbound Barbican approach'),
    'hall':dict(lo=[222,149,603],hi=[257,169,660],
        regions=[dict(lo=[222,149,603],hi=[257,155.2,660]),
                 dict(lo=[224,155,607],hi=[254,169,624])],
        spawn=[239.06292255267,152.16500852518,615.14478441963],yaw=.094837700895,
        title='First Temple hall'),
}

def atomic(path,data):
    fd,name=tempfile.mkstemp(prefix='.'+path.name+'-',dir=path.parent)
    try:
        with os.fdopen(fd,'wb')as out:out.write(data);out.flush();os.fsync(out.fileno())
        os.replace(name,path)
    finally:
        Path(name).unlink(missing_ok=True)

def sha(data):return hashlib.sha256(data).hexdigest()

# These render overlays repeat the solid wall/door surface or hang above it.
# Their often thin single-sided triangles must not become capsule snags.
COSMETIC=re.compile(r'forged_|ornament|decal|emission',re.I)
VERTEX_LIMIT=147456
TRIANGLE_LIMIT=294912

class Source:
    def __init__(self,path):
        self.path=path;self.raw=path.read_bytes();self.doc=json.loads(self.raw)
        self.meshes={row['hash']:row for row in self.doc['meshes']};self.read_hashes={}
    @functools.lru_cache(None)
    def mesh(self,key):
        m=self.meshes[key];raw=(self.path.parent/m['binary']).read_bytes();self.read_hashes[key]=sha(raw)
        v=np.ndarray((m['vertices'],16),'<f4',raw).copy()
        idx=np.concatenate([np.ndarray((s['indices'],),'<i4',raw,s['index_offset'])for s in m['submeshes']]).reshape(-1,3).copy()
        if not np.isfinite(v[:,:6]).all()or np.any(idx<0)or np.any(idx>=len(v)):raise ValueError('Invalid source mesh '+key)
        return v,idx
    def selected(self):
        units=collections.defaultdict(list)
        for p in self.doc['placements']:
            if p['group']in ('Lava Rivers and Lakes','Emission','Background Mountains'):continue
            key=(p['group'],re.sub(r'(?i)LOD[ _]?[0-9]+','LOD',p['hierarchy']),tuple(round(float(x),5)for x in p['matrix']))
            units[key].append(p)
        out=[]
        for key,rows in units.items():
            near=[p for p in rows if p['lod_mask']&1];mid=[p for p in rows if p['lod_mask']&2]
            floor=any(word in key[1].lower()for word in ('floor_plane','lake polygon','stone_stairs','carpet'))
            candidates=near if floor and near else mid or near
            def detail(p):
                match=re.search(r'(?i)LOD[ _]?([0-9]+)',p['hierarchy'])
                return int(match.group(1)) if match else 0
            # A mask3 row may contain both LOD0 and LOD1. Taking every matching
            # row creates coincident, differently tessellated colliders.
            if candidates and not COSMETIC.search(candidates[0]['name']):
                out.append(sorted(candidates,key=detail,reverse=not floor)[0])
        return out
    def sector(self,selected,profile):
        regions=profile.get('regions',[profile]);parts=[];included=[]
        for p in selected:
            a=np.array([p['bounds_min'][k]for k in 'xyz']);b=np.array([p['bounds_max'][k]for k in 'xyz'])
            active=[r for r in regions if (not r.get('prefix') or p['hierarchy'].startswith(r['prefix']))
                    and not np.any(b<np.array(r['lo'])) and not np.any(a>np.array(r['hi']))]
            if not active:continue
            v,idx=self.mesh(p['mesh']);m=np.array(p['matrix']).reshape(4,4,order='F')
            positions=v[:,:3]@m[:3,:3].T+m[:3,3]
            t=positions[idx];keep=np.zeros(len(t),dtype=bool)
            for region in active:
                keep|=np.all(t.max(1)>=np.array(region['lo']),axis=1)&np.all(t.min(1)<=np.array(region['hi']),axis=1)
            idx=idx[keep];t=t[keep]
            if not len(t):continue
            # Native mirrors UnityZ. Orient each face against the actual
            # transformed source normals before welding its positions.
            t=t*np.array([1,1,-1]);n=(v[:,3:6]@np.linalg.inv(m[:3,:3]))*np.array([1,1,-1])
            cross=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]);good=np.linalg.norm(cross,axis=1)>1.e-8
            t=t[good];idx=idx[good];cross=cross[good]
            flip=np.einsum('ij,ij->i',cross,n[idx].sum(1))<0
            t[flip]=t[flip][:,[0,2,1]]
            if not len(t):continue
            included.append(dict(hierarchy=p['hierarchy'],mesh=p['mesh'],lod_mask=p['lod_mask'],triangles=len(t)))
            parts.append(t)
        if not parts:raise ValueError('Empty source sector')
        triangles=np.concatenate(parts)
        vertices,inverse=np.unique(np.round(triangles.reshape(-1,3),5),axis=0,return_inverse=True)
        indices=inverse.reshape(-1,3)
        t=vertices[indices];valid=np.linalg.norm(np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]),axis=1)>1.e-8
        indices=indices[valid]
        # Duplicate source faces provide no additional collision. Prefer an
        # upward copy for coincident two-sided floor sheets.
        t=vertices[indices];up=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0])[:,1]
        order=np.argsort(-up,kind='stable');_,unique=np.unique(np.sort(indices[order],axis=1),axis=0,return_index=True)
        indices=indices[order[unique]]
        # Drop unreferenced positions left by degenerate/duplicate face removal.
        used,remap=np.unique(indices,return_inverse=True);vertices=vertices[used];indices=remap.reshape(-1,3)
        if len(vertices)>VERTEX_LIMIT or len(indices)>TRIANGLE_LIMIT:
            raise ValueError(f'Native geometry budget exceeded: {len(vertices)} vertices, {len(indices)} triangles')
        return vertices,indices,included

def physics_bytes(name,vertices,indices):
    rows=['VALWOOD_PHYSICS 1','gravity 0 -9.81 0']+[f'layer {i} 255'for i in range(8)]
    x,y,z=ORIGIN
    rows.append(f'body 1 "Grandstone {name} source triangles" 3 0 0 0 {x:.15g} {y:.15g} {z:.15g} 0 0 0 1 .5 .5 .5 .6 .01 1000 0 1 0 0 0 0 0 0 1')
    rows.extend(f'vertex 1 {x:.8f} {y:.8f} {z:.8f}'for x,y,z in vertices)
    rows.extend(f'triangle 1 {a} {b} {c}'for a,b,c in indices)
    data=('\n'.join(rows)+'\n').encode()
    if len(data)>16*1024**2:raise ValueError('Native physics document exceeds16MiB')
    return data

def settings_bytes(name,profile):
    lo=np.array(profile['lo']);hi=np.array(profile['hi']);spawn=np.array(profile['spawn'])*[1,1,-1]+ORIGIN
    lower=lo*np.array([1,1,-1])+ORIGIN;upper=hi*np.array([1,1,-1])+ORIGIN
    bounds=[lower[0],upper[0],upper[2],lower[2],lower[1],upper[1]]
    text='VALWOOD_PLAYTEST_WALK 1\nphysics "'+name+'.physics"\nbounds '+' '.join(f'{v:.15g}'for v in bounds)+'\n'
    text+='spawn '+' '.join(f'{v:.15g}'for v in (*spawn,profile['yaw'],-.03))+'\n'
    return text.encode(),spawn.tolist(),bounds

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=SOURCE)
    parser.add_argument('--output',type=Path,default=ROOT/'docs/playtest-slices/2026-11-02/grandstone/runtime')
    args=parser.parse_args(argv);output=args.output.resolve()
    if output.is_relative_to(ROOT/'projects')or output.is_relative_to(args.source.parent.resolve()):raise ValueError('Choose a new analysis/playtest output, not an existing project/source')
    output.mkdir(parents=True,exist_ok=True);source=Source(args.source);selected=source.selected();reports={}
    for name,profile in PROFILES.items():
        vertices,indices,included=source.sector(selected,profile)
        physics=physics_bytes(name,vertices,indices);settings,spawn,bounds=settings_bytes(name,profile)
        atomic(output/(name+'.physics'),physics);atomic(output/(name+'.settings'),settings)
        reports[name]=dict(name=profile['title'],vertices=len(vertices),triangles=len(indices),physics_bytes=len(physics),physics_sha256=sha(physics),settings_sha256=sha(settings),spawn_world_feet=spawn,bounds_world=bounds,source_bounds=[profile['lo'],profile['hi']],source_regions=profile['regions'],source_components=included)
        print(name,len(vertices),'vertices',len(indices),'triangles',len(physics),'bytes',flush=True)
    report=dict(version=1,kind='Two bounded original-source collision sectors; explicit gate interaction transition',source_scene=str(args.source.resolve()),source_scene_sha256=sha(source.raw),source_mesh_sha256=source.read_hashes,geometry_policy='One selectedMID source mesh per wall/door/rail object; exactHIGH floors/stairs/carpet; near-only structural fallback; source normals set CCW winding;5micrometre coordinate weld; cosmetic metal/decal/ornament overlays omitted from collision only; complete exterior staircase and hall side stairrooms; no decimation or renderer asset edits',sectors=reports,native_cpu_validation='Required before gameplay claim',source_files_unchanged=True,physics_budget_per_sector=dict(vertices=VERTEX_LIMIT,triangles=TRIANGLE_LIMIT,document_bytes=16*1024**2))
    atomic(output/'cook-report.json',(json.dumps(report,indent=2,allow_nan=False)+'\n').encode())
    return 0

if __name__=='__main__':raise SystemExit(main())

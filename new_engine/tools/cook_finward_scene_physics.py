#!/usr/bin/env python3
"""Aggregate the supplied demo's original collider geometry into bounded meshes.

Original building/floor/window and major furniture proxies are retained.
Sub-35 cm isolated clutter is omitted from collision, not rendering. Boxes keep
their exact oriented source corners; three authored nonconvex mesh colliders
retain their triangles. Curved garbage-bag/lamp proxies are tessellated within
5 mm radial error. No purchased source, game project or accepted runtime edits.
"""
from __future__ import annotations
import argparse
import collections
import functools
import hashlib
import json
import math
from pathlib import Path
import shutil
import tempfile
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT/'assets/purchased/finward_house_props/scene_export/scene.json'
DEFAULT_OUTPUT = ROOT/'assets/purchased/finward_house_props/scene-collision-v2'
VERTEX_LIMIT, TRIANGLE_LIMIT, BODY_LIMIT = 147456, 294912, 16
C = np.array([1.,1.,-1.])


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def vector(value):return np.array([value[key] for key in 'xyz'],dtype=np.float64)


def oriented(triangles, center):
    normals=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    flip=np.einsum('ij,ij->i',normals,triangles.mean(1)-center)<0
    triangles[flip]=triangles[flip][:,[0,2,1]]
    return triangles


def box_triangles(collider):
    points=np.array([vector(v) for v in collider['oriented_corners']])
    if points.shape!=(8,3):raise ValueError('Box is missing eight source corners')
    quads=[[0,1,3,2],[4,6,7,5],[0,4,5,1],[2,3,7,6],[0,2,6,4],[1,5,7,3]]
    index=np.array([[a,b,c] for a,b,c,d in quads]+[[a,c,d] for a,b,c,d in quads])
    return oriented(points[index],points.mean(0))


def curved_triangles(collider):
    matrix=np.asarray(collider['matrix'],dtype=float).reshape(4,4,order='F')
    center=matrix[:3,:3]@vector(collider['center'])+matrix[:3,3]
    scales=np.linalg.norm(matrix[:3,:3],axis=0)
    direction=collider['direction'] if collider['type']=='CapsuleCollider' else 1
    axis=matrix[:3,direction]/scales[direction]
    perpendicular=[i for i in range(3) if i!=direction]
    radius=collider['radius']*float(max(scales[perpendicular]) if collider['type']=='CapsuleCollider' else max(scales))
    half_segment=max(0.,collider['height']*scales[direction]*.5-radius) if collider['type']=='CapsuleCollider' else 0.
    reference=np.array([1.,0,0]) if abs(axis[0])<.8 else np.array([0.,0,1.])
    u=np.cross(axis,reference);u/=np.linalg.norm(u);v=np.cross(axis,u)
    points=[];rings=13;segments=16
    for ring in range(rings):
        angle=-math.pi/2+math.pi*ring/(rings-1)
        cap_offset=half_segment*(-1 if angle<0 else 1)
        # Include both equators for a capsule's straight cylindrical section.
        if ring==rings//2 and half_segment:
            offsets=[-half_segment,half_segment]
        else:offsets=[cap_offset]
        for offset in offsets:
            for segment in range(segments):
                azimuth=2*math.pi*segment/segments
                points.append(center+axis*(radius*math.sin(angle)+offset)+
                              radius*math.cos(angle)*(u*math.cos(azimuth)+v*math.sin(azimuth)))
    points=np.array(points);rows=len(points)//segments;triangles=[]
    for row in range(rows-1):
        for segment in range(segments):
            nxt=(segment+1)%segments;a=row*segments+segment;b=row*segments+nxt
            c=(row+1)*segments+nxt;d=(row+1)*segments+segment
            triangles.extend([[a,b,c],[a,c,d]])
    return oriented(points[np.asarray(triangles)],center)


class Source:
    def __init__(self,path):
        self.path=path;self.hash=sha(path);self.doc=json.loads(path.read_bytes())
        self.meshes={m['hash']:m for m in self.doc['meshes']};self.read_hashes={}
    @functools.lru_cache(None)
    def mesh(self,key):
        meta=self.meshes[key];path=self.path.parent/meta['binary'];raw=path.read_bytes()
        self.read_hashes[meta['binary']]=hashlib.sha256(raw).hexdigest()
        vertices=np.ndarray((meta['vertices'],16),'<f4',raw)[:,:3].copy()
        indices=np.concatenate([np.ndarray((s['indices'],),'<i4',raw,s['index_offset'])
                                for s in meta['submeshes']]).reshape(-1,3)
        if not np.isfinite(vertices).all()or np.any(indices<0)or np.any(indices>=len(vertices)):
            raise ValueError('Invalid original mesh collider geometry')
        return vertices,indices
    def triangles(self,collider):
        kind=collider['type']
        if kind=='BoxCollider':return box_triangles(collider)
        if kind in {'SphereCollider','CapsuleCollider'}:return curved_triangles(collider)
        if kind=='MeshCollider':
            if collider['convex']:raise ValueError('Convex collider needs a source hull bake')
            vertices,indices=self.mesh(collider['mesh'])
            matrix=np.array(collider['matrix']).reshape(4,4,order='F')
            points=vertices@matrix[:3,:3].T+matrix[:3,3]
            return points[indices]
        raise ValueError('Unsupported original collider type: '+kind)


def welded(parts):
    triangles=np.concatenate(parts)*C
    # Reflecting Z reverses winding; retain source outward/upward surfaces.
    triangles=triangles[:,[0,2,1]]
    vertices,inverse=np.unique(np.round(triangles.reshape(-1,3),5),axis=0,return_inverse=True)
    indices=inverse.reshape(-1,3);triangles=vertices[indices]
    valid=np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1)>1e-10
    indices=indices[valid];triangles=vertices[indices]
    up=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])[:,1]
    order=np.argsort(-up,kind='stable')
    _,keep=np.unique(np.sort(indices[order],axis=1),axis=0,return_index=True)
    indices=indices[order[keep]]
    used,remap=np.unique(indices,return_inverse=True)
    return vertices[used],remap.reshape(-1,3)


def build(source_path,output,origin,clutter_size=.35,keep_background=False):
    source_path=Path(source_path).resolve(strict=True);output=Path(output).resolve()
    if output.exists()or output.is_symlink():raise ValueError('Choose a NEW collision output directory')
    if output.is_relative_to(ROOT/'projects')or output.is_relative_to(source_path.parent):
        raise ValueError('Collision output must be outside accepted projects and source export')
    origin=np.asarray(origin,dtype=float)
    if origin.shape!=(3,)or not np.isfinite(origin).all():raise ValueError('Finite XYZ origin required')
    source=Source(source_path);active=[];skipped=[]
    for index,collider in enumerate(source.doc['colliders']):
        if collider['hierarchy']=='Scene/Cube' and not keep_background:
            skipped.append(dict(index=index,hierarchy=collider['hierarchy'],reason='non-house 200 m background support omitted together with its unresolved-material renderer'))
        elif not collider['enabled']or not collider['active']or collider['is_trigger']:
            skipped.append(dict(index=index,hierarchy=collider['hierarchy'],reason='disabled/inactive/trigger'))
        elif collider['has_rigidbody']:
            raise ValueError('Unexpected source rigid body requires an explicit interaction policy')
        else:active.append((index,collider))
    roots=collections.defaultdict(list)
    for index,collider in active:
        key=collider['prefab_root_hierarchy']or collider['hierarchy'];roots[key].append(collider)
    tiny=set()
    for root,colliders in roots.items():
        if not root.startswith('Props/'):continue
        lo=np.min([vector(c['bounds_min'])for c in colliders],axis=0)
        hi=np.max([vector(c['bounds_max'])for c in colliders],axis=0)
        if float(max(hi-lo))<clutter_size:tiny.add(root)
    groups=collections.defaultdict(list);included=[]
    for index,collider in active:
        root=collider['prefab_root_hierarchy']or collider['hierarchy']
        if root in tiny:
            skipped.append(dict(index=index,hierarchy=collider['hierarchy'],reason='isolated prop below clutter threshold',root=root))
            continue
        triangles=source.triangles(collider)
        center=(vector(collider['bounds_min'])+vector(collider['bounds_max']))*.5
        if collider['hierarchy']=='Scene/Cube':key='background_support'
        else:key='strip_'+str(int(math.floor((center[0]+12)/4))).zfill(2)
        groups[key].append(triangles)
        included.append(dict(index=index,hierarchy=collider['hierarchy'],root=root,type=collider['type'],
                             source_prefab=collider['source_prefab'],group=key,triangles=len(triangles)))
    if len(groups)>BODY_LIMIT:raise ValueError('Source groups exceed the bounded static-mesh body budget')
    rows=['VALWOOD_PHYSICS 1','gravity 0 -9.81 0']+[f'layer {i} 255'for i in range(8)]
    records=[];total_vertices=0;total_triangles=0
    for body_id,(name,parts)in enumerate(sorted(groups.items()),1):
        vertices,indices=welded(parts);total_vertices+=len(vertices);total_triangles+=len(indices)
        rows.append(f'body {body_id} "Finward source {name}" 3 0 0 0 '+
                    ' '.join(format(float(v),'.15g')for v in origin)+' 0 0 0 1 .5 .5 .5 .6 .01 1000 0 1 0 0 0 0 0 0 1')
        rows.extend(f'vertex {body_id} {x:.5f} {y:.5f} {z:.5f}'for x,y,z in vertices)
        rows.extend(f'triangle {body_id} {a} {b} {c}'for a,b,c in indices)
        records.append(dict(id=body_id,name=name,vertices=len(vertices),triangles=len(indices),
                            bounds_min=vertices.min(0).tolist(),bounds_max=vertices.max(0).tolist()))
    if total_vertices>VERTEX_LIMIT or total_triangles>TRIANGLE_LIMIT:
        raise ValueError('Global source collision vertex/triangle budget exceeded')
    payload=('\n'.join(rows)+'\n').encode()
    if len(payload)>16*1024**2:raise ValueError('Physics document exceeds the default 16 MiB budget')
    if sha(source_path)!=source.hash:raise ValueError('Source export changed during collision conversion')
    for name,expected in source.read_hashes.items():
        if sha(source_path.parent/name)!=expected:raise ValueError('Source collider mesh changed during conversion')
    report=dict(passed=True,source=str(source_path),source_sha256=source.hash,
                source_mesh_sha256=source.read_hashes,origin_engine=origin.tolist(),
                coordinates='Unity source world X/Y retained, Z reflected; all triangles baked before identity body poses plus requested origin',
                bodies=records,total_vertices=total_vertices,total_triangles=total_triangles,
                limits=dict(bodies=BODY_LIMIT,vertices=VERTEX_LIMIT,triangles=TRIANGLE_LIMIT,document_bytes=16*1024**2),
                physics_bytes=len(payload),physics_sha256=hashlib.sha256(payload).hexdigest(),
                source_collider_count=len(source.doc['colliders']),included=included,skipped=skipped,
                clutter_threshold_m=clutter_size,tiny_prop_roots=sorted(tiny),
                non_house_background_support_retained=keep_background,
                mesh_policy='Exact source oriented box surfaces and nonconvex mesh collider triangles; curved source proxies tessellated to16azimuth/12latitude segments; 10micrometre positional weld and duplicate/degenerate removal',
                source_and_runtime_unchanged=True,
                acceptance='Native physics parser/world initialization and real controller traversal still required; no route/spawn policy authored here')
    output.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.finward-collision-',dir=output.parent))
    try:
        (stage/'scene.physics').write_bytes(payload)
        (stage/'COLLISION_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
        if output.exists():raise ValueError('Collision output appeared during staging')
        stage.rename(output)
    finally:
        if stage.exists():shutil.rmtree(stage)
    return dict(passed=True,output=str(output),bodies=len(records),vertices=total_vertices,
                triangles=total_triangles,physics_bytes=len(payload),source_unchanged=True,
                omitted_tiny_prop_roots=len(tiny))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=DEFAULT_SOURCE)
    parser.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    parser.add_argument('--origin',nargs=3,type=float,default=[0,0,0])
    parser.add_argument('--clutter-size',type=float,default=.35)
    parser.add_argument('--keep-background',action='store_true',help='Explicit reference-only option; runtime normally omits the unresolved non-house background cube')
    args=parser.parse_args()
    try:result=build(args.source,args.output,args.origin,args.clutter_size,args.keep_background)
    except (ValueError,KeyError,OSError)as error:parser.exit(1,'Finward collision conversion failed: '+str(error)+'\n')
    print(json.dumps(result,indent=2))

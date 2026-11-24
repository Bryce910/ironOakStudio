#!/usr/bin/env python3
"""Independent, CPU-only route/scenery evidence. Never changes project data.

Reads real POSITION streams and node transforms, not accessor min/max or an
authoring receipt. Uses the production height-region sampler, with explicit
limits: derived terrain, collision and observed GPU timing need native tests.
All generated artifacts are confined to review/route-budget.
"""
from __future__ import annotations

import argparse
import collections
import ctypes
import hashlib
import io
import json
import math
import re
import shlex
import struct
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image

N = Path(__file__).resolve().parents[1]
OUT = N / "review/route-budget"
FILES = ("world.studio", "scenery/bep_environment_instances.txt",
         "scenery/demo_polish_instances.txt", "scenery/starting_area_instances.txt")
SCENERY = ("authored_instances", "starting_area_instances", "rainforest_instances",
           "castle_village_instances", "tanthoon_bandit_camp_instances",
           "castle_streets_instances", "bep_environment_instances",
           "demo_finish_instances", "demo_polish_instances")
WIDTH, DEPTH = 100800., 100800. * 2818 / 5009


def sha(path):
    with Path(path).open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def cross(a, b, c):
    return float((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))


def hull(points):
    p = sorted(set(map(tuple, np.asarray(points))))
    if len(p) <= 2:
        return np.asarray(p)
    low, high = [], []
    for target, seq in ((low, p), (high, p[::-1])):
        for v in seq:
            while len(target) >= 2 and cross(target[-2], target[-1], v) <= 0:
                target.pop()
            target.append(v)
    return np.asarray(low[:-1]+high[:-1])


def point_segment(p, a, b):
    d = b-a
    t = np.clip(np.dot(p-a, d)/max(float(np.dot(d, d)), 1e-20), 0., 1.)
    return float(np.linalg.norm(p-a-t*d))


def segments(a, b, c, d):
    # Orientation alone needs bounding boxes for collinear disjoint segments.
    if (max(min(a[0], b[0]), min(c[0], d[0])) <= min(max(a[0], b[0]), max(c[0], d[0]))+1e-10
            and max(min(a[1], b[1]), min(c[1], d[1])) <= min(max(a[1], b[1]), max(c[1], d[1]))+1e-10
            and cross(a,b,c)*cross(a,b,d) <= 1e-20
            and cross(c,d,a)*cross(c,d,b) <= 1e-20):
        return 0.
    return min(point_segment(a,c,d), point_segment(b,c,d),
               point_segment(c,a,b), point_segment(d,a,b))


def polygon_route(poly, route):
    if not len(poly):
        raise ValueError("Empty actual mesh footprint")
    a=np.asarray([v[0] for v in route])[:,None,:];b=np.asarray([v[1] for v in route])[:,None,:]
    c=np.asarray(poly)[None,:,:];d=np.roll(c,-1,axis=1)
    def orient(u,v):return u[...,0]*v[...,1]-u[...,1]*v[...,0]
    if len(poly)>=3 and np.any(np.all(orient(d-c,a-c)>=-1e-10,axis=1)):return 0.
    boxes=np.all(np.maximum(np.minimum(a,b),np.minimum(c,d))<=np.minimum(np.maximum(a,b),np.maximum(c,d))+1e-10,axis=2)
    crossing=boxes&(orient(b-a,c-a)*orient(b-a,d-a)<=1e-20)&(orient(d-c,a-c)*orient(d-c,b-c)<=1e-20)
    if crossing.any():return 0.
    def distances(p,u,v):
        direction=v-u;t=np.clip(np.sum((p-u)*direction,axis=2)/np.maximum(np.sum(direction*direction,axis=2),1e-20),0.,1.)
        return np.sum((p-u-t[...,None]*direction)**2,axis=2)
    return math.sqrt(min(float(distances(p,u,v).min()) for p,u,v in [(a,c,d),(b,c,d),(c,a,b),(d,a,b)]))


def rotation(yaw, pitch, roll):
    y, p, r = np.radians([yaw,pitch,roll])
    ry=np.array([[math.cos(y),0,math.sin(y)],[0,1,0],[-math.sin(y),0,math.cos(y)]])
    rx=np.array([[1,0,0],[0,math.cos(p),-math.sin(p)],[0,math.sin(p),math.cos(p)]])
    rz=np.array([[math.cos(r),-math.sin(r),0],[math.sin(r),math.cos(r),0],[0,0,1]])
    return ry @ rx @ rz


def node_matrix(node):
    if "matrix" in node:
        return np.asarray(node["matrix"],dtype=float).reshape(4,4,order="F")
    x,y,z,w=node.get("rotation",[0,0,0,1])
    q=np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],
                [2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],
                [2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]])
    m=np.eye(4);m[:3,:3]=q @ np.diag(node.get("scale",[1,1,1]));m[:3,3]=node.get("translation",[0,0,0])
    return m


def model(path,with_triangles=False):
    raw=path.read_bytes();magic,version,size=struct.unpack_from("<III",raw)
    assert magic==0x46546c67 and version==2 and size==len(raw), path
    length,kind=struct.unpack_from("<II",raw,12);assert kind==0x4e4f534a
    doc=json.loads(raw[20:20+length]);offset=20+length
    length,kind=struct.unpack_from("<II",raw,offset);assert kind==0x004e4942
    binary=memoryview(raw)[offset+8:offset+8+length]
    assert not doc.get("skins"), "Expected static scenery"
    points=[];triangles=0;material_ids=set();vertex_count=0;topology=[];parts=[]
    def visit(index,parent,ancestors):
        nonlocal triangles,vertex_count
        assert index not in ancestors, "Cyclic scene"
        node=doc["nodes"][index];matrix=parent @ node_matrix(node)
        if "mesh" in node:
            for primitive in doc["meshes"][node["mesh"]]["primitives"]:
                assert primitive.get("mode",4)==4 and "POSITION" in primitive["attributes"]
                ac=doc["accessors"][primitive["attributes"]["POSITION"]]
                assert ac["type"]=="VEC3" and ac["componentType"]==5126 and not ac.get("sparse")
                view=doc["bufferViews"][ac["bufferView"]]
                data=np.ndarray((ac["count"],3),dtype="<f4",buffer=binary,
                    offset=view.get("byteOffset",0)+ac.get("byteOffset",0),strides=(view.get("byteStride",12),4))
                points.append(data.astype(float) @ matrix[:3,:3].T + matrix[:3,3])
                parts.append((doc.get("materials",[])[primitive["material"]].get("name","") if "material" in primitive else "",points[-1]))
                if with_triangles:
                    if 'indices' in primitive:
                        aci=doc['accessors'][primitive['indices']];vi=doc['bufferViews'][aci['bufferView']];dtype={5121:'u1',5123:'<u2',5125:'<u4'}[aci['componentType']]
                        ix=np.frombuffer(binary,dtype=dtype,count=aci['count'],offset=vi.get('byteOffset',0)+aci.get('byteOffset',0))
                    else:ix=np.arange(len(data))
                    topology.append(points[-1][ix.reshape(-1,3)])
                vertex_count+=len(data)
                triangles+=(doc["accessors"][primitive["indices"]]["count"] if "indices" in primitive else len(data))//3
                if "material" in primitive:material_ids.add(primitive["material"])
        for child in node.get("children",[]):visit(child,matrix,ancestors|{index})
    if doc.get("scenes"):
        roots=doc["scenes"][doc.get("scene",0)]["nodes"]
    else:
        children={c for n in doc["nodes"] for c in n.get("children",[])}
        roots=[i for i in range(len(doc["nodes"])) if i not in children]
    for index in roots:visit(index,np.eye(4),set())
    p=np.vstack(points);assert np.isfinite(p).all();minimum_y=p[:,1].min();p[:,1]-=minimum_y
    triangle_points=np.concatenate(topology) if with_triangles else None
    if with_triangles:triangle_points[:,:,1]-=minimum_y
    image_ids=set()
    for i in material_ids:
        material=doc.get("materials",[])[i];pbr=material.get("pbrMetallicRoughness",{})
        for obj in [pbr.get("baseColorTexture"),pbr.get("metallicRoughnessTexture"),material.get("normalTexture"),material.get("emissiveTexture")]:
            if obj:image_ids.add(doc["textures"][obj["index"]]["source"])
    images={}
    for i in image_ids:
        im=doc["images"][i]
        if "bufferView" in im:
            v=doc["bufferViews"][im["bufferView"]];data=bytes(binary[v.get("byteOffset",0):v.get("byteOffset",0)+v["byteLength"]])
        else:
            uri=im["uri"];assert not Path(uri).is_absolute() and ":" not in uri
            image_path=(path.parent/uri).resolve();data=image_path.read_bytes()
        with Image.open(io.BytesIO(data)) as decoded:w,h=decoded.size
        key=hashlib.sha256(data).hexdigest();a,b=w,h;total=0
        while True:
            total+=a*b*4
            if a==b==1:break
            a=max(1,a//2);b=max(1,b//2)
        images[key]={"width":w,"height":h,"rgba8_mip_chain_bytes":total}
    return dict(points=p,parts=[(name,q-np.array([0.,minimum_y,0.])) for name,q in parts],triangles=triangles,triangle_points=triangle_points,vertices=vertex_count,
                geometry_bytes=vertex_count*68+triangles*3*4,images=images,
                sha256=hashlib.sha256(raw).hexdigest(),minimum=p.min(0),maximum=p.max(0))


def definitions(project,source):
    result={}
    builtin_text=(source/"engine/world_scenery.cpp").read_text()
    builtin_rows={}
    for row in re.findall(r'^\s*\{("[^"]+",\s*"[^"]+",[^\n]+)\}',builtin_text,re.M):
        values=[v.strip() for v in row.split(',')]
        builtin_rows[values[0].strip('"')]=values
    pattern=r'\{"([^"\\]+)",\s*"([^"\\]+)",\s*(true|false),\s*[^,]+,\s*([^,]+),\s*(\d+),\s*(\d+),\s*Grounding::(\w+),\s*Collision::(\w+)'
    for name,stem,tree,distance,vegetation,lods,grounding,collision in re.findall(pattern,builtin_text):
        q=builtin_rows.get(name,[])
        result[name]=dict(stem=stem,tree=tree=="true",lods=int(lods),distance=float(distance.rstrip("f")),split=float(q[11].rstrip('f')) if len(q)>11 else 80.,named_near=len(q)>14 and q[14]=='true',collision=collision,vegetation=int(vegetation),grounding=grounding)
    for line in (project/"world.studio").read_text().splitlines():
        q=shlex.split(line)
        if q and q[0]=="asset":
            name,stem,lods,collision,vegetation,distance,split=q[1:8]
            result[name]=dict(stem=stem,lods=int(lods),tree=int(vegetation)==2,distance=float(distance),split=float(split),named_near=result.get(name,{}).get('named_near',False),collision=int(collision),vegetation=int(vegetation),grounding="Root" if int(vegetation) else "Support")
    return result


def placements(project):
    out=[]
    for stem in SCENERY:
        path=project/"scenery"/(stem+".txt")
        if not path.exists():continue
        for index,line in enumerate(path.read_text().splitlines()):
            q=line.split()
            if not q or q[0].startswith("#"):continue
            if "wind" in q:q=q[:q.index("wind")]
            values=list(map(float,q[1:]));u,v,yaw,scale,burial=values[:5];extra=values[5:]
            assert len(extra) in [0,1,2,7,8],(path,index)
            # Defaults are positional, including explicit pivot at slot7.
            defaults=[0.,0.,0.,0.,1.,1.,1.,float("nan")]
            defaults[:len(extra)]=extra
            elevation,offset,pitch,roll,sx,sy,sz,pivot=defaults
            out.append(dict(alias=q[0],x=(u-.5)*WIDTH,z=(v-.5)*DEPTH,yaw=yaw,scale=scale,
                burial=burial,elevation=elevation,offset=offset,pitch=pitch,roll=roll,
                sx=sx,sy=sy,sz=sz,pivot=pivot,file="scenery/"+stem+".txt",line=index+1,raw=line))
    return out


def load_height(project,source):
    # Generated review helper invokes the actual production header sampler.
    cpp=OUT/"sample-height.cpp";lib=OUT/"sample-height.so"
    text='''#include "engine/world_height_region.h"
#include "engine/world_surface_field.h"
#include <fstream>
#include <sstream>
#include <iomanip>
static WorldHeightRegion r;
static world_surface::Data surfaces;
static world_surface::PaintField paint;
static world_surface::TerrainField terrain;
static std::string why;
static float base(double x,double z,void *){float h;if(!world_height_region_sample(&r,x,z,&h))return NAN;return h;}
extern "C" const char *error(){return why.c_str();}
extern "C" int load(const char *p,const char *studio){char e[256];if(world_height_region_load(&r,p,e,sizeof e)!=1){why=e;return 0;}
 std::ifstream f(studio);std::string line,text;
 while(std::getline(f,line))if(line.rfind("surfaces ",0)==0){std::istringstream row(line.substr(9));std::string record;if(!(row>>std::quoted(record))){why="Bad surface quote";return 0;}text+=record+'\\n';}
 return world_surface::decode(text,surfaces,why)&&world_surface::bake_paint(surfaces,paint,why)&&world_surface::bake_terrain(surfaces,base,nullptr,terrain,why);}
extern "C" int sample_many(unsigned n,const double *xz,float *out,int derived){for(unsigned i=0;i<n;i++){if(!world_height_region_sample(&r,xz[2*i],xz[2*i+1],out+i))return 0;if(derived)out[i]+=world_surface::terrain_offset(terrain,xz[2*i],xz[2*i+1]);}return 1;}
extern "C" unsigned count(unsigned n){switch(n){case 0:return surfaces.images.size();case 1:return surfaces.layers.size();case 2:return surfaces.paint.size();case 3:return surfaces.ribbons.size();case 4:return paint.tiles.size();case 5:return terrain.tiles.size();default:return 0;}}
'''
    if not cpp.exists() or cpp.read_text()!=text:cpp.write_text(text)
    temporary=lib.with_name("sample-height.build.so")
    command=["g++","-std=c++17","-shared","-fPIC","-O2","-I"+str(source),str(cpp),str(source/"engine/world_surface_authoring.cpp"),str(source/"engine/world_surface_field.cpp"),"-o",str(temporary)]
    required=[cpp,source/"engine/world_height_region.h",source/"engine/world_surface_authoring.cpp",source/"engine/world_surface_authoring.h",source/"engine/world_surface_field.cpp",source/"engine/world_surface_field.h",source/"engine/world_river_network.h"]
    pins={str(p):sha(p) for p in required};receipt=OUT/"sample-height-build.json"
    old=json.loads(receipt.read_text()) if receipt.exists() else {}
    if not lib.exists() or old.get('inputs_sha256')!=pins or old.get('binary_sha256')!=sha(lib):
        result=subprocess.run(command,capture_output=True,text=True);assert result.returncode==0,result.stderr
        temporary.replace(lib)
        receipt.write_text(json.dumps(dict(inputs_sha256=pins,binary_sha256=sha(lib),command=command),indent=2)+'\n')
    dll=ctypes.CDLL(str(lib));dll.load.argtypes=[ctypes.c_char_p,ctypes.c_char_p];dll.sample_many.argtypes=[ctypes.c_uint,ctypes.POINTER(ctypes.c_double),ctypes.POINTER(ctypes.c_float),ctypes.c_int];dll.error.restype=ctypes.c_char_p
    assert dll.load(str(project/"ground/showcase.height").encode(),str(project/"world.studio").encode())==1,dll.error().decode()
    def sample(xz,derived=True):
        data=np.ascontiguousarray(xz,dtype=np.float64).reshape(-1,2);values=np.empty(len(data),dtype=np.float32)
        assert dll.sample_many(len(data),data.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),values.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),int(derived)),"Footprint outside source height region"
        return values
    stats=dict(zip(["images","layers","paint_records","ribbons","paint_tiles","terrain_tiles"],[dll.count(i) for i in range(6)]))
    return sample,command,stats


def source_path(project,d,lod):
    suffix=("_near","_mid","_far")[lod] if d["tree"] else (("_near" if d.get('named_near') else "") if lod==0 else "_mid")
    return project/"models"/(d["stem"]+suffix+".glb")


def baseline_snapshot(base):
    folder=OUT/"baseline-files";folder.mkdir(exist_ok=True)
    pins={}
    for rel in FILES:
        p=folder/rel;p.parent.mkdir(parents=True,exist_ok=True)
        if not p.exists():p.write_bytes((base/"world-project"/rel).read_bytes())
        assert p.read_bytes()==(base/"world-project"/rel).read_bytes(),"Baseline changed since budget snapshot: "+rel
        pins[rel]=sha(p)
    return pins


def self_test():
    assert abs(segments(np.array([0.,0]),np.array([1.,0]),np.array([2.,0]),np.array([3.,0]))-1)<1e-12
    assert segments(np.array([0.,0]),np.array([1.,1]),np.array([0.,1]),np.array([1.,0]))==0
    box=hull([[0,0],[1,0],[1,1],[0,1],[.5,.5]])
    assert polygon_route(box,[(np.array([.2,.2]),np.array([.8,.8]))])==0
    assert abs(polygon_route(box,[(np.array([2.,.2]),np.array([2.,.8]))])-1)<1e-12
    assert np.allclose(rotation(90,0,0)@np.array([1.,0,0]),[0,0,-1])
    n=node_matrix({"translation":[1,2,3],"scale":[2,3,4]})
    assert np.allclose(n@np.array([1.,1,1,1]),[3,5,7,1])
    random=np.random.default_rng(20261119)
    for _ in range(40):
        poly=hull(random.normal(size=(12,2)));route=[(random.normal(size=2)+[3,0],random.normal(size=2)+[3,0])]
        expected=min(segments(route[0][0],route[0][1],p,poly[(i+1)%len(poly)]) for i,p in enumerate(poly))
        if all(cross(poly[i],poly[(i+1)%len(poly)],route[0][0])>=-1e-10 for i in range(len(poly))):expected=0.
        assert abs(polygon_route(poly,route)-expected)<1e-10
    return 46


def supported_roast(project,before,after,defs):
    receipt=N/'review/scenery/ROAST_PLACEMENT.json';r=json.loads(receipt.read_text())
    assert r['passed'] and r['applied'] and r['geometry_materials_collision_and_all_other_rows_preserved']
    assert after['alias']=='nm_city_region_roast' and after['file']==r['file'] and after['line']==r['line']==439
    assert before['raw']==r['before'] and after['raw']==r['after']
    old,new=before['raw'].split(),after['raw'].split();assert len(old)==len(new)==14
    assert all(x==y for i,(x,y) in enumerate(zip(old,new)) if i!=13)
    path=project/r['file'];rows=path.read_text().splitlines(keepends=True)
    assert sha(path)==r['after_sha256'];rows[r['line']-1]=r['before']+'\n'
    assert hashlib.sha256(''.join(rows).encode()).hexdigest()==r['before_sha256']
    camp=next(p for p in placements(project) if p['alias']=='nm_city_region_camp')
    assert camp['raw'].split()[1:13]==new[1:13]
    camp_path=source_path(project,defs['nm_city_region_camp'],0);roast_path=source_path(project,defs['nm_city_region_roast'],0)
    assert sha(camp_path)==r['source_camp_sha256'] and sha(roast_path)==r['source_roast_sha256']
    cg=model(camp_path);rg=model(roast_path,True)
    axis_y=r['shaft_center_world_y']-camp['pivot']
    # Actual vertices on the retained 50mm shaft must span its2.29m length.
    q=np.vstack([points for name,points in cg['parts'] if name=='Charcoal and dry splits']);shaft=q[q[:,1]>1.1]
    assert abs((shaft[:,1].min()+shaft[:,1].max())*.5-axis_y)<1e-6 and abs((shaft[:,2].min()+shaft[:,2].max())*.5)<1e-6
    assert len(shaft)>=24 and abs(np.ptp(shaft[:,0])-2.29)<1e-5
    roast_local_y=r['shaft_center_world_y']-after['pivot']
    assert abs(roast_local_y-(rg['minimum'][1]+rg['maximum'][1])*.5)<1e-6
    start=np.array([-2.,roast_local_y,0.]);direction=np.array([1.,0,0]);hits=[]
    for triangle in rg['triangle_points']:
        matrix=np.column_stack((direction,-(triangle[1]-triangle[0]),-(triangle[2]-triangle[0])))
        if abs(np.linalg.det(matrix))<1e-10:continue
        distance,u,v=np.linalg.solve(matrix,triangle[0]-start)
        if distance>0 and u>=-1e-8 and v>=-1e-8 and u+v<=1+1e-8:hits.append(float(start[0]+distance))
    assert len(hits)>=2 and min(hits)<-.04 and max(hits)>.04
    assert abs(min(hits)-min(r['central_axis_actual_triangle_crossings_local_x']))<1e-6 and abs(max(hits)-max(r['central_axis_actual_triangle_crossings_local_x']))<1e-6
    return dict(passed=True,placement_changes=1,only_y_token_changed=True,shaft_crosses_actual_roast_triangles=True,
        measured_triangle_crossings_local_x=sorted(hits),raised_m=after['pivot']-before['pivot'],
        receipt_sha256=sha(receipt),source_camp_sha256=sha(camp_path),source_roast_sha256=sha(roast_path))


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--snapshot-only",action="store_true");a=parser.parse_args()
    assert N.name=="camp-road-polish-2026-11-19" and not (N/"UNIFIED_READY.json").exists(),"Published generations are immutable"
    OUT.mkdir(exist_ok=True);source=N/"source-v1";project=N/"world-project"
    base=Path(json.loads((N/"BASELINE.json").read_text())["baseline"])
    snapshot=baseline_snapshot(base)
    if a.snapshot_only:print(json.dumps(dict(baseline=str(base),snapshot_sha256=snapshot),indent=2));return
    input_paths=[project/"world.studio",project/"ground/showcase.height",project/"opening/first-journey.routes.json",project/"opening/road-traffic.settings",project/"opening/arrival-camp.settings",project/"opening/first-journey.town-people.settings"]+[project/"scenery"/(s+".txt") for s in SCENERY]
    initial_input_pins={str(p):sha(p) for p in input_paths if p.is_file()}
    tests=self_test();errors=[];warnings=[];defs=definitions(project,source);before_defs=definitions(base/"world-project",source)
    cover=(source/"engine/world_cover.h").read_text()
    near_begin=float(re.search(r'#define WORLD_TREE_NEAR_FADE_BEGIN ([\d.]+)f',cover)[1])
    near_end=float(re.search(r'#define WORLD_TREE_NEAR_FADE_END ([\d.]+)f',cover)[1])
    before=placements(base/"world-project");after=placements(project);by_key={(p['file'],p['line']):p for p in before}
    changed=[p for p in after if (p['file'],p['line']) not in by_key or p['raw']!=by_key[(p['file'],p['line'])]['raw']]
    new_details=sum((p['file'],p['line']) not in by_key for p in changed)
    if len(changed)!=608 or new_details!=607:errors.append('Expected607 new details plus exactly1 existing supported-roast correction')
    counts_before=collections.Counter(p['alias'] for p in before);counts_after=collections.Counter(p['alias'] for p in after)
    cap=int(re.search(r'WORLD_SOURCE_RECORD_LIMIT=(\d+)',(source/"engine/world_authoring_limits.h").read_text())[1])
    if len(after)>cap:errors.append("Actual source instance cap exceeded")
    if max(counts_after.values())>4096:errors.append("Actual per-alias instance cap exceeded")
    for rel in ["ground/showcase.height","opening/first-journey.routes.json","opening/road-traffic.settings"]:
        if sha(project/rel)!=sha(base/"world-project"/rel):errors.append("Protected terrain/route source changed: "+rel)
    old_lines=(OUT/"baseline-files/world.studio").read_text().splitlines();new_lines=(project/"world.studio").read_text().splitlines()
    protected=lambda lines:[line for line in lines if line.startswith(("terrain ","tile ","water ","structure ")) or (line.startswith("surfaces ") and shlex.split(json.loads(line[9:]))[0] in ["ribbon","point","river","river_point"])]
    if protected(old_lines)!=protected(new_lines):errors.append("Derived terrain/structure records changed")
    paths=json.loads((project/"opening/first-journey.routes.json").read_text())["paths"]
    route=[(np.array(a)[[0,2]],np.array(b)[[0,2]]) for points in paths.values() for a,b in zip(points[:-1],points[1:])]
    traffic=np.array([list(map(float,q[1:4])) for line in (project/"opening/road-traffic.settings").read_text().splitlines() if (q:=line.split()) and q[0]=="point"])
    road=[(a[[0,2]],b[[0,2]]) for a,b in zip(traffic[:-1],traffic[1:])]
    npc_points=[]
    for rel in ["opening/arrival-camp.settings","opening/first-journey.town-people.settings"]:
        for line in (project/rel).read_text().splitlines():
            q=line.split()
            if q and q[0]=='actor':npc_points.append((rel+':'+q[1],np.array([float(q[2]),float(q[4])])))
    samples=[]
    for a,b in road:
        count=max(1,math.ceil(np.linalg.norm(b-a)/20))
        samples.extend(a+(b-a)*i/count for i in range(count))
    samples.append(road[-1][1]);samples=np.asarray(samples)
    cache={};input_pins=dict(initial_input_pins)
    def geometry(d,lod,owner=project):
        path=source_path(owner,d,lod)
        if path not in cache:cache[path]=model(path);input_pins[str(path)]=cache[path]['sha256']
        return cache[path]
    ground,compile_command,surface_stats=load_height(project,source);changed_results=[];roast_review=None
    for p in changed:
        if p['alias']=='nm_city_region_roast':
            assert roast_review is None
            roast_review=supported_roast(project,by_key[(p['file'],p['line'])],p,defs)
            continue
        if (p['file'],p['line']) in by_key:errors.append('Unreviewed existing scenery row changed: '+p['file']+':'+str(p['line']))
        d=defs[p['alias']];g=geometry(d,0);matrix=rotation(p['yaw'],p['pitch'],p['roll'])@np.diag([p['scale']*p['sx'],p['scale']*p['sy'],p['scale']*p['sz']]);v=g['points']@matrix.T
        if not math.isfinite(p['pivot']):
            errors.append("Changed placement lacks explicit grounded pivot: "+p['alias']);continue
        v+=np.array([p['x'],p['pivot'],p['z']]);poly=hull(v[:,[0,2]])
        lane=polygon_route(poly,road);walk=polygon_route(poly,route)
        npc_label,npc_clearance=min(((label,polygon_route(poly,[(point,point)])) for label,point in npc_points),key=lambda value:value[1])
        low=g['points'][:,1]<=max(.015,float(g['maximum'][1])*.04)
        basal=v[low];derived=ground(basal[:,[0,2]]);raw=ground(basal[:,[0,2]],False);residual=basal[:,1]-derived
        all_residual=v[:,1]-ground(v[:,[0,2]])
        posed_height=float(v[:,1].max()-v[:,1].min())
        burial_ratio=max(0.,-float(residual.min()))/max(posed_height,.001)
        row=dict(file=p['file'],line=p['line'],alias=p['alias'],actual_world_minimum=v.min(0).tolist(),actual_world_maximum=v.max(0).tolist(),road_centerline_clearance_m=lane,journey_centerline_clearance_m=walk,nearest_static_npc=npc_label,static_npc_footprint_clearance_m=npc_clearance,basal_vertices=len(basal),derived_terrain_offset_max_abs_m=float(np.abs(derived-raw).max()),lowest_stem_burial_fraction_of_mesh_height=burial_ratio,visible_height_above_local_ground_m=float(all_residual.max()),mesh_vertices_above_ground_fraction=float(np.mean(all_residual>0)),basal_ground_residual_m=dict(min=float(residual.min()),p05=float(np.quantile(residual,.05)),median=float(np.median(residual)),max=float(residual.max())),noncolliding=d['collision'] in [0,'None'])
        changed_results.append(row)
        if not row['noncolliding']:errors.append("Patch changes colliding placement: "+p['alias'])
        if walk<.37:errors.append("Changed mesh covers player route capsule: "+p['alias'])
        if npc_clearance<.37:errors.append("Changed mesh intrudes on static NPC body: "+p['alias'])
        if lane<2.75:warnings.append(f"{p['file']}:{p['line']} visible footprint is within the nominal 2.75m road half-width")
        # Source ferns contain three low stem-tip vertices. Forcing these tips
        # onto soil would float the visible root mass; report their burial and
        # apply the loader's 60%-of-height burial ceiling rather than an invented
        # 12cm absolute limit. Positive gaps still fail independently.
        if float(np.quantile(residual,.05))>.08:errors.append(f"Changed mesh basal gap exceeds8cm: {p['file']}:{p['line']}")
        if burial_ratio>.60:errors.append(f"Changed mesh burial exceeds60% of height: {p['file']}:{p['line']}")
        if float(all_residual.max())<.02:errors.append(f"Changed mesh almost entirely hidden under terrain: {p['file']}:{p['line']}")
    (OUT/"MESH_CLEARANCE_REVIEW.json").write_text(json.dumps(dict(passed=not errors,changed_instances=len(changed),rows=changed_results,errors=errors,production_surface_stats=surface_stats),indent=2)+'\n')
    def budget(rows,definitions_,owner):
        stations=[dict(x=float(s[0]),z=float(s[1]),potential_draw_instances=0,triangles=0,geometry_bytes_unique=0,rgba8_texture_mips_unique=0) for s in samples]
        used=[set() for _ in samples];images=[{} for _ in samples];local_aliases=set()
        for p in rows:
            # 300m root margin is only a cheap reject; actual transformed bounds
            # below make final range/LOD decisions. The scope excludes huge city
            # source batches whose roots do not lie in this local route region.
            if min(point_segment(np.array([p['x'],p['z']]),a,b) for a,b in road)>300:continue
            d=definitions_[p['alias']];local_aliases.add(p['alias'])
            for lod in range(d['lods']):
                g=geometry(d,lod,owner);corners=np.array([[g['minimum'][0] if not k&1 else g['maximum'][0],g['minimum'][1] if not k&2 else g['maximum'][1],g['minimum'][2] if not k&4 else g['maximum'][2]] for k in range(8)])
                m=rotation(p['yaw'],p['pitch'],p['roll'])@np.diag([p['scale']*p['sx'],p['scale']*p['sy'],p['scale']*p['sz']]);xz=(corners@m.T)[:,[0,2]]+np.array([p['x'],p['z']]);lo=xz.min(0);hi=xz.max(0)
                distance=np.linalg.norm(samples-np.clip(samples,lo,hi),axis=1)
                if d['lods']==1:active=distance<d['distance']
                elif d['tree'] and d['lods']==3:active=(distance<[near_end,265,1680][lod])&(distance>=[0,near_begin,215][lod])
                else:active=(distance<(d['split']+4 if lod==0 else d['distance']))&(distance>=(0 if lod==0 else d['split']-4))
                path=source_path(owner,d,lod)
                for i in np.nonzero(active)[0]:
                    stations[i]['potential_draw_instances']+=1;stations[i]['triangles']+=g['triangles'];used[i].add(path);images[i].update(g['images'])
        for i,s in enumerate(stations):
            s['geometry_bytes_unique']=sum(cache[p]['geometry_bytes'] for p in used[i]);s['rgba8_texture_mips_unique']=sum(im['rgba8_mip_chain_bytes'] for im in images[i].values())
        return dict(stations=stations,local_aliases=sorted(local_aliases),max_potential_draw_instances=max(s['potential_draw_instances'] for s in stations),max_potential_triangles=max(s['triangles'] for s in stations),max_unique_geometry_bytes=max(s['geometry_bytes_unique'] for s in stations),max_unique_rgba8_texture_mip_bytes=max(s['rgba8_texture_mips_unique'] for s in stations))
    old_budget=budget(before,before_defs,base/"world-project");new_budget=budget(after,defs,project)
    # Changes in actual files or transforms must remain visible in the report;
    # no blind 44FPS average or invented VRAM measurement is used as acceptance.
    for stem in SCENERY:
        p=project/"scenery"/(stem+".txt")
        if p.exists():input_pins[str(p)]=sha(p)
    for p,digest in initial_input_pins.items():
        if sha(p)!=digest:errors.append("Candidate data changed during independent review; rerun after freeze: "+p)
    for rel in ["engine/world_height_region.h","engine/world_model_cpu.cpp","engine/world_transform_math.h","engine/world_scenery.cpp","engine/world_cover.h","engine/world_surface_authoring.cpp","engine/world_surface_authoring.h","engine/world_surface_field.cpp","engine/world_surface_field.h","engine/world_river_network.h","engine/world_authoring_limits.h"]:input_pins[str(source/rel)]=sha(source/rel)
    for path in [Path(__file__).resolve(),OUT/"sample-height.cpp",OUT/"sample-height.so",OUT/"sample-height-build.json"]:input_pins[str(path)]=sha(path)
    if roast_review:
        for path in [N/'review/scenery/ROAST_PLACEMENT.json',N/'review/scenery/ROAST_PLACEMENT_PUBLICATION.json']:input_pins[str(path)]=sha(path)
    report=dict(schema="valwood.camp-road-budget-independent.v1",passed=not errors,gpu_used=False,project_or_live_modified=False,source="Actual GLB POSITION/node streams, production height-region sampler and actual surface decode/paint/terrain bake",candidate=str(N),baseline=str(base),self_tests=tests,baseline_snapshot_sha256=snapshot,inputs_sha256=input_pins,production_height_helper_compile_command=compile_command,production_surface_stats=surface_stats,source_height_and_routes_unchanged=not any('source changed' in e for e in errors),global_instance_count_before=len(before),global_instance_count_after=len(after),global_instance_delta=len(after)-len(before),alias_count_delta=dict(counts_after-counts_before),source_instance_cap=cap,changed_instances=len(changed),changed_mesh_review=changed_results,budget_before=old_budget,budget_after=new_budget,budget_delta={key:new_budget[key]-old_budget[key] for key in ['max_potential_draw_instances','max_potential_triangles','max_unique_geometry_bytes','max_unique_rgba8_texture_mip_bytes']},errors=errors,warnings=warnings,limitations=["360-degree conservative source-AABB range/LOD potential, not observed GPU draw counts, frustum/occlusion results or frame time.","RGBA8 mip-chain sums are uncompressed source-image bounds, not observed VRAM; compression, shared sampler roles, streaming and other renderer resources are not included.","Source-root local scope: 300m from traffic polyline; huge spatial source batches with remote roots are excluded.","Basal geometry uses the production source height sampler plus actual baked surface offsets. No authored sculpt tiles are added by this patch; arbitrary runtime terrain edits are outside scope.","Footprint convex hull is conservative across gaps/leaves. It does not replace native production capsule/actor/river clearance testing.","No full-game or ordinary-play frame-pacing claim; mixed-route profiler includes loading, modals and QA readbacks."])
    report.update(new_detail_instances_reviewed=len(changed_results),supported_roast_placement=roast_review,
        supported_roast_placement_changes=1 if roast_review else 0,
        concurrent_review_note='CPU-only review may overlap candidate native run; BLAS/OMP thread caps are1. No performance-gain claim or correction of native timing intervals.')
    path=OUT/"CPU_BUDGET_REVIEW.json";path.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps({k:report[k] for k in ['passed','changed_instances','budget_delta','errors']},indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__=="__main__":main()

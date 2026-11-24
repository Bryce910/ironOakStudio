#!/usr/bin/env python3
"""Cook a lowered castle drawbridge with resident purchased wood PBR pixels.

The bridge root is on the dry OUTER south approach (0,225). Its separate walk floor
leaves the shared atlas bed submerged below it. This tool writes only its new
two mesh files, collision include and provenance; existing models are read-only.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import math
import struct
import sys
import numpy as np

from build_castle_village_assets import Builder
from build_source_market_canopies import encoded_glb
from audit_village_pose_compatibility import glb,values
from build_starting_area import glb_bounds

ROOT=Path(__file__).resolve().parents[1]
MODELS=ROOT/'projects/valwood_world/models'
SOURCE=ROOT/'assets/purchased/medieval_village/prepared'
STEM='castle_drawbridge'
PROVENANCE=ROOT/'assets/authored/castle_drawbridge/PROVENANCE.json'
ALIAS=dict(name='castle_drawbridge',stem=STEM,grounding='Foundation',collision='Drawbridge',
    lods=2,lod_split_m=65,draw_distance_m=550,shadow_proxy='castle_drawbridge_mid.glb',
    position_xz=[0,225],scale=1,burial=.70,elevation=0,max_burial=.75)
COLLIDERS=[[-7.5,-6.65,-12.5,-.35,-.30,4.65],[6.65,7.5,-12.5,-.35,-.30,4.65]]
VIEWS=[dict(id='castle-drawbridge',position_xz=[-13,225],target_xz=[0,211],pitch=-.06),
       dict(id='castle-creek',position_xz=[224.5,-112],target_xz=[219,-108],pitch=-.18)]

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def apply(author):
    """Append the independently validated raised crossing after ground layers."""
    if hasattr(author,'_castle_creek_bridge_metadata'):return copy.deepcopy(author._castle_creek_bridge_metadata)
    # The scene recipe can run as __main__ or be imported by acceptance tools.
    # Register against the actual Author owner so both paths use one alias map.
    owner=sys.modules[type(author).__module__]
    owner.ALIASES['castle_drawbridge']=STEM
    owner.NON_SOLID.add('castle_drawbridge')
    before=copy.deepcopy(author.records)
    label='castle_creek_drawbridge'
    assert label not in {r['label']for r in before}
    author.model('castle_drawbridge',True)
    author.add('castle_drawbridge',0,225,0,1,.70,label,0)
    author._castle_creek_bridge_baseline=before
    author._castle_creek_bridge_metadata=dict(schema_version=1,
        recipe_file=str(Path(__file__).relative_to(ROOT)),recipe_sha256=sha(Path(__file__)),
        terrain_helper_file='engine/world_castle_creek.h',terrain_helper_sha256=sha(ROOT/'engine/world_castle_creek.h'),
        provenance_file=str(PROVENANCE.relative_to(ROOT)),provenance_sha256=sha(PROVENANCE),
        collision_file='engine/world_castle_drawbridge_colliders.inc',collision_sha256=sha(ROOT/'engine/world_castle_drawbridge_colliders.inc'),
        labels=[label],existing_records_preserved=len(before),water_shape='closed creek outside outer town walls',
        water_rectangle_local=[-225,-223,225,223],water_surface_y=22.9,
        outside_outer_town_walls=True,enclosure_wall_centerline_xz=[-205,-205,205,205],
        outer_tower_source_radius_m=7.92*1.28,water_path_points=22,
        water_mask_policy='shared closed path envelope plus scene depth; entire outer town enclosure remains dry',
        deck_surface_y=24.02,source_root_dry=True,lowered_static_bridge=True,
        views=copy.deepcopy(VIEWS),new_gpu_image_keys=0,collision_policy='two compound side rail/chain volumes; deck has separate walk floor')
    return copy.deepcopy(author._castle_creek_bridge_metadata)

def validate_placement(author):
    from build_village_demo_dressing import baseline_equal
    before=author._castle_creek_bridge_baseline
    assert baseline_equal(author.records[:len(before)],before),'Bridge changed an existing scene record'
    records=[r for r in author.records if r['label']=='castle_creek_drawbridge']
    assert len(records)==1
    r=records[0]
    assert r['alias']=='castle_drawbridge' and abs(r['x'])<1e-6 and abs(r['z']-225)<1e-6
    assert abs(r['yaw'])<1e-6 and r['scale']==1 and r['burial']==.70 and r['elevation']==0
    proof=validate()
    return dict(bridge_instances=1,source_model_triangles=[a['triangles']for a in proof['assets']],
        compound_collision_boxes=2,walkable_width_m=13,actual_water_below_deck=True,
        new_gpu_image_keys=0,existing_records_preserved=len(before))

def ring(builder,center,tangent,normal,rx,ry,tube=.027,segments=10,sides=4):
    """Elliptical welded chain link; alternate real link planes along the cable."""
    c=np.asarray(center);u=np.asarray(tangent);v=np.asarray(normal);w=np.cross(u,v)
    for i in range(segments):
        a,b=i*math.tau/segments,(i+1)*math.tau/segments
        def p(t,k):
            radial=u*math.cos(t)+v*math.sin(t);radial/=np.linalg.norm(radial)
            return tuple(c+u*(rx*math.cos(t))+v*(ry*math.sin(t))+tube*(radial*math.cos(k)+w*math.sin(k)))
        for j in range(sides):
            s,t=j*math.tau/sides,(j+1)*math.tau/sides
            builder.face('iron',[p(a,s),p(b,s),p(b,t),p(a,t)])

def geometry(lod):
    b=Builder(lod)
    # Staggered board ends, individual gaps and recessed nails survive close
    # walking views; rails, hinge hardware and under-deck bearers cast shadows.
    boards=38 if lod=='near'else 19
    length=12.5;step=length/boards
    for i in range(boards):
        z0=-12.5+i*step+.008;z1=-12.5+(i+1)*step-.008
        middle=(-.14 if i%3==0 else .18 if i%3==1 else .06)
        for x0,x1 in [(-7.35,middle-.008),(middle+.008,7.35)]:
            b.box('wood',x0,x1,-.20,.02,z0,z1)
        if lod=='near':
            for x in [-6.3,6.3]:
                b.box('iron',x-.025,x+.025,.021,.027,(z0+z1)*.5-.025,(z0+z1)*.5+.025)
    for x in [-5.9,-2.1,2.1,5.9]:b.box('wood',x-.17,x+.17,-.70,-.20,-12.45,-.05)
    for z in [-11.8,-9.2,-6.6,-4,-1.4]:b.box('wood',-7.4,7.4,-.45,-.22,z-.15,z+.15)
    for side in [-1,1]:
        x=side*7.08
        for z in [-11.9,-9.6,-7.3,-5,-2.7,-.5]:
            b.box('wood',x-.13,x+.13,-.22,1.34,z-.14,z+.14)
            b.box('iron',x-.145,x+.145,.12,.28,z-.15,z+.15)
        for y in [.48,1.15]:b.box('wood',x-.09,x+.09,y-.08,y+.08,-12.25,-.35)
        for i,z in enumerate([-11.7,-9.4,-7.1,-4.8,-2.5]):
            b.beam('wood',(x,.30 if i%2 else 1.13,z),(x,1.13 if i%2 else .30,z+1.95),.095)
        # Outer lifting posts and braced top sheaves. The opening remains clear.
        b.box('wood',x-.23,x+.23,-.25,4.45,-12.3,-11.84)
        b.beam('wood',(x,1.2,-11.8),(x,4.1,-10.5),.20)
        b.box('iron',x-.255,x+.255,3.7,3.95,-12.33,-11.80)
        b.box('iron',x-.255,x+.255,.15,.50,-12.33,-11.80)
        b.box('iron',x-.31,x+.31,-.18,.12,-12.5,-11.70)
        start=np.array([x,4.22,-11.95]);end=np.array([x,.30,-.85]);axis=end-start
        axis/=np.linalg.norm(axis);across=np.array([1.,0.,0.]);vertical=np.cross(axis,across)
        if lod=='near':
            count=54
            for i in range(count):
                t=i/(count-1);center=start+(end-start)*t
                # Restrained sag distinguishes linked iron from a rigid beam.
                center[1]-=.18*math.sin(t*math.pi)
                ring(b,center,axis,across if i%2 else vertical,.15,.078)
        else:b.beam('iron',tuple(start),tuple(end),.072)
        for z in [-11.7,-.8]:
            b.box('iron',x-.25,x+.25,.025,.065,z-.28,z+.28)
    return b

def cook(lod,path):
    builder=geometry(lod)
    doc=dict(asset=dict(version='2.0',generator='Valwood lowered drawbridge cook1'),
        buffers=[{}],bufferViews=[],accessors=[],images=[],textures=[],
        samplers=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)],
        materials=[],meshes=[dict(name='Lowered timber drawbridge',primitives=[])],
        nodes=[dict(mesh=0,name='Lowered timber drawbridge')],scenes=[dict(nodes=[0])],scene=0)
    binary=bytearray()
    def view(payload,target=None):
        binary.extend(b'\0'*(-len(binary)%4));d=dict(buffer=0,byteOffset=len(binary),byteLength=len(payload));binary.extend(payload)
        if target:d['target']=target
        doc['bufferViews'].append(d);return len(doc['bufferViews'])-1
    def accessor(array,kind,position=False):
        a=np.asarray(array,dtype='<u4'if kind=='SCALAR'else'<f4')
        item=dict(bufferView=view(a.tobytes(),34963 if kind=='SCALAR'else 34962),componentType=5125 if kind=='SCALAR'else 5126,count=len(a),type=kind)
        if position:item.update(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())
        doc['accessors'].append(item);return len(doc['accessors'])-1
    for channel in ['BC','N','ARM']:
        p=SOURCE/('T_Tileable_Wood_Trims_01_'+channel+'.png')
        image=len(doc['images']);doc['images'].append(dict(name=p.stem,mimeType='image/png',bufferView=view(p.read_bytes())))
        doc['textures'].append(dict(source=image,sampler=0))
    doc['materials']=[dict(name='Resident purchased structural wood',doubleSided=False,
        pbrMetallicRoughness=dict(baseColorTexture=dict(index=0),metallicRoughnessTexture=dict(index=2),roughnessFactor=1,metallicFactor=1),
        normalTexture=dict(index=1),occlusionTexture=dict(index=2)),
        dict(name='Forged drawbridge iron',doubleSided=False,pbrMetallicRoughness=dict(baseColorFactor=[.11,.12,.13,1],roughnessFactor=.53,metallicFactor=.84))]
    for material,(name,(verts,faces)) in enumerate(builder.geometry.items()):
        positions=[];normals=[];uv=[];indices=[]
        for face in faces:
            points=np.asarray([verts[i]for i in face]);normal=np.cross(points[1]-points[0],points[2]-points[0]);normal/=max(np.linalg.norm(normal),1e-12)
            # Grain follows the longest real beam/board axis of each face.
            axes=np.argsort(np.ptp(points,axis=0))[::-1];along,across=int(axes[0]),int(axes[1]);first=len(positions)
            for p in points:
                positions.append(p);normals.append(normal);uv.append([p[along]/2.,.025+p[across]/.8])
            for i in range(1,len(face)-1):indices.extend([first,first+i,first+i+1])
        doc['meshes'][0]['primitives'].append(dict(attributes=dict(POSITION=accessor(positions,'VEC3',True),NORMAL=accessor(normals,'VEC3'),TEXCOORD_0=accessor(uv,'VEC2')),
            indices=accessor(indices,'SCALAR'),material=0 if name=='wood'else 1))
    doc['buffers'][0]['byteLength']=len(binary);path.write_bytes(encoded_glb(doc,binary))

def validate():
    records=[];near_images=None
    donor,blob=glb(MODELS/'village_stall_red.glb')
    resident={hashlib.sha256(blob[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]).hexdigest()
        for image in donor.get('images',[])for v in [donor['bufferViews'][image['bufferView']]]}
    for lod,suffix in [('near',''),('mid','_mid')]:
        path=MODELS/(STEM+suffix+'.glb');doc,binary=glb(path);low,high,verts,tris=glb_bounds(path,True)
        assert all(math.isfinite(v)for p in verts for v in p)
        assert abs(low[0]+7.4)<.15 and abs(high[0]-7.4)<.15 and abs(low[2]+12.5)<1e-6
        assert high[2]<=.00001
        deck_vertices=[p for p in verts if abs(p[1]-.72)<1e-5]
        assert len(deck_vertices)>150,'Normalized timber deck surface is missing'
        assert len(doc['materials'])==2 and len(doc['images'])==3
        images={hashlib.sha256(binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]).hexdigest()
            for image in doc['images']for v in [doc['bufferViews'][image['bufferView']]]}
        assert images<=resident,'Bridge introduced nonresident texture pixels'
        if near_images is None:near_images=images
        else:assert images==near_images
        # No underside/rail/cable may occupy the ordinary walking head volume.
        central=0
        for tri in tris:
            if min(p[0]for p in tri)<6.5 and max(p[0]for p in tri)>-6.5 and all(.60<p[1]<2.5 for p in tri):
                assert max(p[1]for p in tri)<=.7271,('Geometry blocks deck walking volume',tri)
                central+=1
        assert central>20
        records.append(dict(lod=lod,path=str(path.relative_to(ROOT)),sha256=sha(path),bytes=path.stat().st_size,
            triangles=len(tris),vertices=len(verts),bounds=[low,high],materials=2,images_shared_with_resident_assets=3))
    assert records[0]['triangles']<15000 and records[1]['triangles']<3000
    assert all((box[1]<-6.5 or box[0]>6.5)for box in COLLIDERS)
    normalized_colliders=[box[:4]+[box[4]+.7,box[5]+.7]for box in COLLIDERS]
    return dict(schema_version=1,assets=records,alias=ALIAS,collision_boxes=normalized_colliders,
        new_gpu_image_keys=0,deck_height_m=24.02,bed_separate_from_walk_floor=True,
        drawbridge_state='lowered, static demonstration; lift interaction not implemented',
        source_materials=[dict(path=str((SOURCE/('T_Tileable_Wood_Trims_01_'+c+'.png')).relative_to(ROOT)),
            sha256=sha(SOURCE/('T_Tileable_Wood_Trims_01_'+c+'.png')))for c in ['BC','N','ARM']],
        licensing='Authored geometry; user-purchased Medieval Village wood textures remain proprietary.')

def main():
    p=argparse.ArgumentParser();p.add_argument('--cook',action='store_true');p.add_argument('--validate',action='store_true')
    p.add_argument('--refresh-provenance',action='store_true',help='Refresh placement/provenance metadata without rewriting mesh bytes')
    a=p.parse_args();assert a.cook or a.validate or a.refresh_provenance
    if a.cook:
        for lod,suffix in [('near',''),('mid','_mid')]:cook(lod,MODELS/(STEM+suffix+'.glb'))
        text='/* Generated by tools/build_castle_drawbridge.py. Source root on dry approach. */\n'
        text+='static constexpr float castle_drawbridge_collision_boxes[][6]={\n'
        for raw in COLLIDERS:
            box=raw[:4]+[raw[4]+.7,raw[5]+.7]
            text+='    {'+','.join(format(x,'.9g')+'f'if'.'in format(x,'.9g')else format(x,'.9g')+'.0f'for x in box)+'},\n'
        text+='};\n';(ROOT/'engine/world_castle_drawbridge_colliders.inc').write_text(text)
    result=validate()
    if a.cook or a.refresh_provenance:PROVENANCE.parent.mkdir(parents=True,exist_ok=True);PROVENANCE.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()

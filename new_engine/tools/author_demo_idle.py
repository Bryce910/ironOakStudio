#!/usr/bin/env python3
"""Offline Blender retarget of an owner-supplied idle; original GLB bytes retained.

Run with Blender --disable-autoexec --factory-startup --python-exit-code 1.
The new clip is appended to an exact copy of the original mesh/rig/material data.
No source model, source motion or existing clip is overwritten.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import sys
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT/'projects/valwood_world/models/animations/white_knight_brown_armor.glb'
SOURCE = Path('/home/computer/Downloads/breathing_idle.fbx')
OUT = ROOT/'docs/demo-polish/2026-10-29/motion-stage'
NAMES = {'Hips':'Hips', 'Spine02':'Spine', 'Spine01':'Spine1', 'Spine':'Spine2',
         'neck':'Neck', 'Head':'Head'}
for side in ('Left','Right'):
    for part in ('Shoulder','Arm','ForeArm','Hand','UpLeg','Leg','Foot','ToeBase'):
        NAMES[side+part] = side+part
CHILD = {'Hips':'Spine02', 'Spine02':'Spine01', 'Spine01':'Spine', 'Spine':'neck', 'neck':'Head'}
for side in ('Left','Right'):
    for a,b in [('Shoulder','Arm'),('Arm','ForeArm'),('ForeArm','Hand'),
                ('UpLeg','Leg'),('Leg','Foot'),('Foot','ToeBase')]:
        CHILD[side+a] = side+b


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def basis(arm, names):
    p = lambda name: arm.matrix_world @ arm.data.bones[names[name]].head_local
    up = (p('Head')-p('Hips')).normalized()
    side = p('RightArm')-p('LeftArm')
    side = (side-up*side.dot(up)).normalized()
    return Matrix((side,up,side.cross(up))).transposed()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source_sha, base_sha = sha(SOURCE), sha(BASE)
    raw = BASE.read_bytes()
    n,kind = struct.unpack_from('<II',raw,12)
    assert kind == 0x4e4f534a
    doc = json.loads(raw[20:20+n])
    length,kind = struct.unpack_from('<II',raw,20+n)
    assert kind == 0x004e4942
    original_binary = raw[28+n:28+n+length]
    binary = bytearray(original_binary)
    before = {key:json.dumps(doc.get(key),sort_keys=True) for key in ('nodes','skins','meshes','materials','textures','images','animations')}
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.gltf(filepath=str(BASE))
    target = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    target.animation_data.action = None
    for track in target.animation_data.nla_tracks:
        track.mute = True
    for b in target.pose.bones:
        b.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    target_rest = {b.name:target.matrix_world @ b.matrix_local for b in target.data.bones}
    existing = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(SOURCE),use_anim=True,ignore_leaf_bones=False)
    source = next(o for o in set(bpy.data.objects)-existing if o.type == 'ARMATURE')
    mapping = {key:'mixamorig:'+value for key,value in NAMES.items()}
    assert all(name in source.data.bones for name in mapping.values())
    source_rest = {b.name:source.matrix_world @ b.matrix_local for b in source.data.bones}
    target_names = {name:name for name in NAMES}
    conversion = basis(target,target_names) @ basis(source,mapping).transposed()
    cq = conversion.to_quaternion()
    offsets = {}
    for name, donor in mapping.items():
        oriented = cq @ source_rest[donor].to_quaternion()
        child = CHILD.get(name)
        swing = Matrix.Identity(3).to_quaternion()
        if child:
            a = conversion @ (source_rest[mapping[child]].translation-source_rest[donor].translation)
            b = target_rest[child].translation-target_rest[name].translation
            swing = a.normalized().rotation_difference(b.normalized())
        offsets[name] = (swing @ oriented).inverted() @ target_rest[name].to_quaternion()
    order = sorted(target.pose.bones,key=lambda b:len(b.parent_recursive))
    axis = Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)))
    parents = {child:i for i,node in enumerate(doc['nodes']) for child in node.get('children',[])}
    globals_ = {}
    def node_global(i):
        if i in globals_:
            return globals_[i]
        node = doc['nodes'][i]
        if 'matrix' in node:
            values = node['matrix']
            local = Matrix([values[j::4] for j in range(4)])
        else:
            from mathutils import Quaternion
            q = node.get('rotation',[0,0,0,1])
            local = Matrix.LocRotScale(Vector(node.get('translation',[0,0,0])),
                Quaternion((q[3],q[0],q[1],q[2])),Vector(node.get('scale',[1,1,1])))
        globals_[i] = node_global(parents[i]) @ local if i in parents else local
        return globals_[i]
    index = {node.get('name'):i for i,node in enumerate(doc['nodes'])}
    joints = [doc['nodes'][i]['name'] for i in doc['skins'][0]['joints']]
    rest_error = max(((axis @ target_rest[name]).translation-node_global(index[name]).translation).length for name in joints)
    assert rest_error < 1e-4, ('GLTF/Blender coordinate mismatch',rest_error)
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    source_initial = (source.matrix_world @ source.pose.bones[mapping['Hips']].matrix).translation
    leg = lambda rest,hip,foot:(rest[hip].translation-rest[foot].translation).length
    ratio = leg(target_rest,'LeftUpLeg','LeftFoot')/leg(source_rest,mapping['LeftUpLeg'],mapping['LeftFoot'])
    rotations = {name:[] for name in joints}
    root_positions = []
    duration, count = 8.,241
    minimum_upright, max_root_delta = 1.,0.
    for sample in range(count):
        # Retain captured breathing/weight shifts in a slow closed out-and-back
        # loop. Cosine sampling has zero speed at both joins; no raw endpoint snap.
        u = .5-.5*math.cos(math.tau*sample/(count-1))
        frame = 1.+120.*u
        bpy.context.scene.frame_set(math.floor(frame),subframe=frame%1)
        for b in target.pose.bones:
            b.matrix_basis = Matrix.Identity(4)
        bpy.context.view_layer.update()
        inverse = target.matrix_world.to_quaternion().inverted()
        for b in order:
            donor = mapping.get(b.name)
            if donor is None:
                continue
            q = inverse @ cq @ (source.matrix_world @ source.pose.bones[donor].matrix).to_quaternion() @ offsets[b.name]
            position = b.head.copy()
            if b.name == 'Hips':
                delta = (source.matrix_world @ source.pose.bones[donor].matrix).translation-source_initial
                position = target.matrix_world.inverted() @ (target_rest['Hips'].translation+Vector((0,0,max(-.06,min(.06,delta.z*ratio)))))
                max_root_delta = max(max_root_delta,abs(delta.z*ratio))
            b.matrix = Matrix.LocRotScale(position,q,Vector((1,1,1)))
            bpy.context.view_layer.update()
        up = target.matrix_world @ target.pose.bones['Head'].head-target.matrix_world @ target.pose.bones['Hips'].head
        minimum_upright = min(minimum_upright,up.normalized().z)
        posed = {}
        for name in joints:
            delta = axis @ (target.matrix_world @ target.pose.bones[name].matrix) @ target_rest[name].inverted() @ axis.inverted()
            posed[index[name]] = delta @ node_global(index[name])
        for name in joints:
            i = index[name]
            parent = parents.get(i)
            local = (posed.get(parent,node_global(parent)).inverted() @ posed[i]) if parent is not None else posed[i]
            translation,q,scale = local.decompose()
            value = [q.x,q.y,q.z,q.w]
            if rotations[name] and sum(a*b for a,b in zip(value,rotations[name][-1])) < 0:
                value = [-v for v in value]
            rotations[name].append(value)
            if name == 'Hips':
                root_positions.append(list(translation))
    assert minimum_upright > .94, minimum_upright
    def accessor(values, width, kind):
        while len(binary)%4:
            binary.append(0)
        data = struct.pack('<'+'f'*(len(values)*width),*(v for row in values for v in row))
        view = len(doc['bufferViews'])
        doc['bufferViews'].append(dict(buffer=0,byteOffset=len(binary),byteLength=len(data)))
        binary.extend(data)
        out = len(doc['accessors'])
        row = dict(bufferView=view,componentType=5126,count=len(values),type=kind)
        if width == 1:
            row.update(min=[min(v[0] for v in values)],max=[max(v[0] for v in values)])
        doc['accessors'].append(row)
        return out
    times = accessor([[duration*i/(count-1)] for i in range(count)],1,'SCALAR')
    clip = dict(name='Idle',samplers=[],channels=[])
    for name, values in rotations.items():
        sampler = len(clip['samplers'])
        clip['samplers'].append(dict(input=times,output=accessor(values,4,'VEC4'),interpolation='LINEAR'))
        clip['channels'].append(dict(sampler=sampler,target=dict(node=index[name],path='rotation')))
    clip['samplers'].append(dict(input=times,output=accessor(root_positions,3,'VEC3'),interpolation='LINEAR'))
    clip['channels'].append(dict(sampler=len(clip['samplers'])-1,target=dict(node=index['Hips'],path='translation')))
    doc['animations'].append(clip)
    assert bytes(binary[:len(original_binary)]) == original_binary
    for key in ('nodes','skins','meshes','materials','textures','images'):
        assert json.dumps(doc.get(key),sort_keys=True) == before[key]
    assert json.dumps(doc['animations'][:-1],sort_keys=True) == before['animations']
    doc['buffers'][0]['byteLength'] = len(binary)
    encoded = json.dumps(doc,separators=(',',':')).encode()
    encoded += b' '*(-len(encoded)%4)
    binary.extend(b'\0'*(-len(binary)%4))
    body = struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary
    output = OUT/'white_knight_brown_armor_demo.glb'
    output.write_bytes(struct.pack('<III',0x46546c67,2,len(body)+12)+body)
    report = dict(source=SOURCE.name,source_sha256=source_sha,base_sha256=base_sha,
        output_sha256=sha(output),clips=[a['name'] for a in doc['animations']],mapping=mapping,
        samples=count,duration_seconds=duration,minimum_upright_dot=minimum_upright,
        coordinate_rest_error_m=rest_error,maximum_source_hip_vertical_motion_m=max_root_delta,
        mesh_skin_material_images_and_original_clips_unchanged=True,original_binary_prefix_exact=True,
        new_texture_bytes=0,original_sources_unchanged=sha(BASE)==base_sha and sha(SOURCE)==source_sha,
        loop='Captured first4seconds, cosine out-and-back8second loop; exact endpoint pose; no invented breathing offsets')
    (OUT/'idle-provenance.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()

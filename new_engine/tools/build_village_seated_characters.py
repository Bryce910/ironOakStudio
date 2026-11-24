#!/usr/bin/env python3
"""Bake four genuine CC0 Sitting1 keys; no runtime rig or new animation claim.

OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup \
  --python-exit-code 1 --python tools/build_village_seated_characters.py

The original CDmir/TinyWorlds source, textures and action remain unchanged.
A fixed pelvis-centred origin and a fixed scale match a .495m bench seat.
This does not rotate standing geometry or hide legs beneath the terrain.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
SOURCE = ROOT / 'assets/characters/incoming/cdmir_kelgar/bartender-IK.blend'
OUTPUT = ROOT / 'projects/valwood_world/models'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fixed_topology(directory,records):
    """Canonical vertex/triangle permutations, preserving every posed corner.

    Unique offline UV1 IDs identify source face corners. glTF exporter may
    reorder its deduplicated vertices as normals change; sorting by those IDs
    restores exact correspondence without moving or re-triangulating a face.
    """
    import numpy as np
    from audit_village_pose_compatibility import glb,values
    reference=None
    for record in records:
        path=directory/record['file'];raw=path.read_bytes();data,binary=glb(path)
        primitive=data['meshes'][0]['primitives'][0];attrs=primitive['attributes']
        corner=values(data,binary,attrs['TEXCOORD_1'])[:,0]
        assert len(corner)==len(np.unique(corner))==3884*3
        order=np.argsort(corner);inverse=np.empty_like(order);inverse[order]=np.arange(len(order))
        changed=bytearray(raw)
        def write(index,array):
            a=data['accessors'][index];view=data['bufferViews'][a['bufferView']]
            assert not view.get('byteStride')and not a.get('sparse')
            offset=28+struct.unpack_from('<I',raw,12)[0]+view.get('byteOffset',0)+a.get('byteOffset',0)
            encoded=array.tobytes();assert len(encoded)==values(data,binary,index).nbytes
            changed[offset:offset+len(encoded)]=encoded
        for index in attrs.values():write(index,values(data,binary,index)[order])
        old=values(data,binary,primitive['indices']);tri=inverse[old].reshape(-1,3)
        # Cyclic rotations keep the exact oriented triangle; lexicographic
        # sorting changes only submission order, never winding or diagonals.
        tri=np.array([np.roll(t,-int(np.argmin(t)))for t in tri],dtype=old.dtype)
        tri=tri[np.lexsort((tri[:,2],tri[:,1],tri[:,0]))]
        write(primitive['indices'],tri.reshape(old.shape))
        uv=values(data,binary,attrs['TEXCOORD_0'])[order]
        if reference is None:reference=(tri,uv)
        else:assert np.array_equal(tri,reference[0])and np.array_equal(uv,reference[1])
        path.write_bytes(changed);record['sha256']=sha(path)
        record['topology_proof']={'reference':records[0]['file'],'indices_exact':True,'uv0_exact':True,
            'method':'Fixed source quad triangulation before armature; unique UV1 source-corner IDs; vertex/triangle permutations only, preserving every posed position, normal, UV0, material and oriented triangle.'}


def cook():
    import bpy
    import numpy as np
    from mathutils import Vector
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    scene = bpy.context.scene
    arm = next(o for o in scene.objects if o.type == 'ARMATURE')
    original = bpy.data.objects['Bartender']
    bpy.context.view_layer.objects.active=original;original.select_set(True)
    triangulate=original.modifiers.new('Fixed source topology before posing','TRIANGULATE')
    triangulate.quad_method='FIXED'
    while original.modifiers.find(triangulate.name)>0:
        bpy.ops.object.modifier_move_up(modifier=triangulate.name)
    bpy.ops.object.modifier_apply(modifier=triangulate.name)
    action = bpy.data.actions['Sitting1']
    arm.animation_data.action = action
    for track in arm.animation_data.nla_tracks:
        track.mute = True
    scene.frame_set(0)
    bpy.context.view_layer.update()
    hip = arm.matrix_world @ arm.pose.bones['Hips'].head
    graph = bpy.context.evaluated_depsgraph_get()
    initial = [original.matrix_world @ v.co for v in original.evaluated_get(graph).data.vertices]
    frames = [0.,48.5,97.,145.5]
    ground = min(p.z for p in initial)
    for value in frames:
        scene.frame_set(int(value),subframe=value-int(value));bpy.context.view_layer.update()
        graph=bpy.context.evaluated_depsgraph_get()
        ground=min(ground,min((original.matrix_world@v.co).z for v in original.evaluated_get(graph).data.vertices))
    scene.frame_set(0);bpy.context.view_layer.update()
    # The genuine source coat hangs beneath the thigh. Fit the real lowest
    # posterior seat-contact surface, not a bone midpoint that would bury cloth.
    contact = min(p.z for p in initial if abs(p.x-hip.x) < 1.4 and
                  abs(p.y-hip.y) < 1.1 and p.z > 2)
    # Asset root follows the .015m visible floor, so posterior contact is
    # .480m above the root and exactly .495m above the building origin.
    seat_above_floor=.495-.015
    scale = seat_above_floor / (contact-ground)
    origin = Vector((hip.x, hip.y, ground))
    # The legacy Body material uses the second, case-sensitive UVmap layer.
    # The active UVMap is correct for Head/Hair but maps97% of body corners
    # onto blue texture padding. Resolve each material's original mapping
    # explicitly before merging the three source images into one atlas.
    source_uv_layers={'Body':'UVmap','Head':'UVMap','Hair':'UVMap'}
    original_uv={name:[tuple(v.uv)for v in original.data.uv_layers[layer].data]
                 for name,layer in source_uv_layers.items()}
    original_material = [p.material_index for p in original.data.polygons]
    names = [m.name for m in original.data.materials]
    # Three512px source tiles with extruded gutters. Bound total new decoded
    # memory to16MiB including mips while retaining actual diffuse/normal/AO.
    layers = {k: np.ones((1024, 1024, 4), dtype=np.float32) for k in ['base','normal','orm']}
    layers['normal'][:,:,:] = (.5,.5,1,1)
    layers['orm'][:,:,:] = (1,.76,0,1)
    specs = {'Body': ('body-skin.png','body-norm.png','body'),
             'Head': ('head-skin.png','head-norm.png','Head'),
             'Hair': ('hair-skin',None,None)}
    rects = {}
    for name, (x,y) in zip(['Body','Head','Hair'], [(0,0),(512,0),(0,512)]):
        rects[name] = ((x+2)/1024,(y+2)/1024,508/1024,508/1024)
        for channel,image_name in zip(['base','normal','orm'],specs[name]):
            if image_name is None:
                continue
            image = bpy.data.images[image_name].copy()
            if channel != 'base': image.colorspace_settings.name = 'Non-Color'
            image.scale(508,508)
            data = np.empty(508*508*4,dtype=np.float32)
            image.pixels.foreach_get(data)
            data = data.reshape((508,508,4))
            if channel == 'orm':
                # Old source has genuine AO and normal, no roughness/metal map.
                ao = data[:,:,:3].mean(axis=2)
                data[:,:,:] = (1,.76,0,1)
                data[:,:,0] = ao
            else:
                data[:,:,3] = 1 # Source has modeled hair, not alpha cards.
            layers[channel][y:y+512,x:x+512] = np.pad(data,((2,2),(2,2),(0,0)),mode='edge')
            bpy.data.images.remove(image)
    material = bpy.data.materials.new('Seated guest original diffuse normal AO')
    material.use_nodes = True
    nodes=material.node_tree.nodes;links=material.node_tree.links
    # Never let Blender infer coordinates from its transient active-render UV.
    # UV1 is an offline source-corner ID, not a material texture mapping.
    texture_uv=nodes.new('ShaderNodeUVMap');texture_uv.uv_map='UVMap'
    p=nodes.get('Principled BSDF');p.inputs['Metallic'].default_value=0
    p.inputs['Roughness'].default_value=.76
    images={}
    for channel,data in layers.items():
        image=bpy.data.images.new('Seated_'+channel,1024,1024,alpha=True)
        image.colorspace_settings.name='sRGB' if channel=='base' else 'Non-Color'
        image.pixels.foreach_set(data.ravel());image.file_format='PNG';image.pack();images[channel]=image
        node=nodes.new('ShaderNodeTexImage');node.image=image;node.extension='REPEAT'
        links.new(texture_uv.outputs['UV'],node.inputs['Vector'])
        if channel=='base':links.new(node.outputs['Color'],p.inputs['Base Color'])
        elif channel=='normal':
            normal=nodes.new('ShaderNodeNormalMap');normal.uv_map='UVMap';links.new(node.outputs['Color'],normal.inputs['Color']);links.new(normal.outputs['Normal'],p.inputs['Normal'])
        else:
            split=nodes.new('ShaderNodeSeparateColor');links.new(node.outputs['Color'],split.inputs['Color']);links.new(split.outputs['Green'],p.inputs['Roughness']);links.new(split.outputs['Blue'],p.inputs['Metallic'])
            group=bpy.data.node_groups.new('glTF Material Output','ShaderNodeTree')
            group.interface.new_socket(name='Occlusion',in_out='INPUT',socket_type='NodeSocketFloat')
            out=nodes.new('ShaderNodeGroup');out.node_tree=group;links.new(split.outputs['Red'],out.inputs['Occlusion'])
    material.use_backface_culling=False
    records=[]
    with tempfile.TemporaryDirectory(prefix='valwood-seated-') as temp:
        temp=Path(temp)
        for frame,value in enumerate(frames):
            scene.frame_set(int(value),subframe=value-int(value));bpy.context.view_layer.update()
            graph=bpy.context.evaluated_depsgraph_get()
            mesh=bpy.data.meshes.new_from_object(original.evaluated_get(graph),depsgraph=graph)
            mesh.transform(original.matrix_world)
            for vertex in mesh.vertices:vertex.co=(vertex.co-origin)*scale
            for polygon,mi in zip(mesh.polygons,original_material):
                x,y,w,h=rects[names[mi]]
                for i in polygon.loop_indices:
                    u,v=original_uv[names[mi]][i];mesh.uv_layers.active.data[i].uv=(x+u*w,y+v*h)
                polygon.material_index=0
            # Preserve original UV0 and identify each offline face corner in
            # unused UV1. Otherwise glTF's normal-dependent deduplication can
            # merge two source corners in one key but keep them in another.
            primary=mesh.uv_layers.active
            for layer in list(mesh.uv_layers):
                if layer!=primary:mesh.uv_layers.remove(layer)
            primary.name='UVMap';primary.active_render=True
            ids=mesh.uv_layers.new(name='PoseCornerID')
            for i,corner in enumerate(ids.data):corner.uv=(i/len(ids.data),.5)
            mesh.uv_layers.active_index=0
            ids.active_render=False;primary.active_render=True
            mesh.materials.clear();mesh.materials.append(material)
            for attribute in list(mesh.color_attributes):mesh.color_attributes.remove(attribute)
            obj=bpy.data.objects.new('Body_seated_source_Sitting1',mesh);bpy.context.collection.objects.link(obj)
            bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
            filename=f'village_character_seated_idle_{frame:02}.glb'
            path=temp/filename
            bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,
                export_animations=False,export_skins=False,export_image_format='AUTO',export_cameras=False,export_lights=False)
            points=[v.co for v in mesh.vertices]
            bounds=[[min(p[i]for p in points),max(p[i]for p in points)]for i in range(3)]
            assert -.005<bounds[2][0]<.005 and 1.5<bounds[2][1]<1.8,bounds
            bones={n:[float(v)for v in ((arm.matrix_world@arm.pose.bones[n].head-origin)*scale)]for n in ['Hips','UpLeg_L','Leg_L','Foot_L','UpLeg_R','Leg_R','Foot_R']}
            records.append({'file':filename,'source_frame':value,'bytes':path.stat().st_size,'sha256':sha(path),
                'bounds_blender_xyz':bounds,'source_bone_positions_blender_xyz':bones,'seat_contact_y':seat_above_floor,
                'source_action':'Sitting1','source_loop_seconds':194/24,'triangles':sum(len(p.vertices)-2 for p in mesh.polygons)})
            bpy.data.objects.remove(obj,do_unlink=True);bpy.data.meshes.remove(mesh)
        fixed_topology(temp,records)
        manifest={'schema_version':1,'source':str(SOURCE.relative_to(ROOT)),'source_sha256':sha(SOURCE),
            'source_provenance':'assets/characters/incoming/cdmir_kelgar/provenance.json',
            'creator':'CDmir (Čestmír Dammer), TinyWorlds','license':'CC0-1.0',
            'source_page':'https://opengameart.org/content/bartender','method':'Genuine source Sitting1; four static keys, original skeletal articulation. No new animation or sleep clip claimed.',
            'normalization':{'scale':scale,'source_origin_xyz':list(origin),'seat_contact_m':seat_above_floor,'bench_contact_m':.495,'floor_offset_m':.015,'forward':'glTF +Z'},
            'new_rgba8_mips_upper_bound_bytes':3*sum(max(1,1024>>i)**2*4 for i in range(11)),
            'texture_resolution':1024,'texture_channels':['source diffuse','source normal','source AO + constant roughness.76/metal0'],
            'source_uv_layers':source_uv_layers,
            'assets':records}
        for record in records:
            target=OUTPUT/record['file'];pending=target.with_suffix('.pending');pending.write_bytes((temp/record['file']).read_bytes());os.replace(pending,target)
        (OUTPUT/'village_seated_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('SEATED COOK COMPLETE',json.dumps(manifest,ensure_ascii=False),flush=True)


if __name__=='__main__':
    cook()

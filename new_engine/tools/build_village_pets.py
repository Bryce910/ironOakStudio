#!/usr/bin/env python3
"""Offline cook of the user's Labrador and the CC BY 4.0 Bicolor Cat.

Run system Python --prepare-only, then Blender -b -t2 --disable-autoexec
--python-exit-code 1 --python tools/build_village_pets.py. No runtime rigging.
The cat source has a standing head/breathing clip; its walk is an explicitly
authored offline four-paw IK adaptation. Dog walking/sitting uses supplied clips.
"""
from pathlib import Path
import argparse, hashlib, json, math, shutil, struct, sys, tempfile, zipfile
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets/purchased/pets'
SOURCE=ASSETS/'source'
TEXTURES=ASSETS/'textures'
OUTPUT=ROOT/'projects/valwood_world/models'
ARCHIVE=Path('/home/computer/Downloads/uploads_files_6381725_50++Animated+Animals.zip')
CAT_URL='https://raw.githubusercontent.com/code4fukui/vr-cats/main/bicolor_cat.glb'
CAT_PAGE='https://sketchfab.com/3d-models/bicolor-cat-e623a618ca344a8393d7ba4d63ec23cf'
CAT_LICENSE='https://creativecommons.org/licenses/by/4.0/'
PROPRIETARY='User-provided purchased proprietary asset; no redistribution license asserted by this conversion.'
MODES=[('walk',8),('idle',4),('rest',4)]

def sha(path):
    digest=hashlib.sha256()
    with Path(path).open('rb')as stream:
        for block in iter(lambda:stream.read(1048576),b''):digest.update(block)
    return digest.hexdigest()

def glb(path):
    raw=Path(path).read_bytes();assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw))
    size=struct.unpack_from('<I',raw,12)[0]
    return json.loads(raw[20:20+size]),raw[28+size:]

def prepare():
    from PIL import Image
    import io
    SOURCE.mkdir(parents=True,exist_ok=True);TEXTURES.mkdir(parents=True,exist_ok=True)
    entries=[]
    with zipfile.ZipFile(ARCHIVE)as archive:
        for item in archive.infolist():
            if '/1.0/Labrador Dog/'not in item.filename or not item.filename.endswith(('.blend','.png')):continue
            path=SOURCE/Path(item.filename).name
            if not path.exists()or path.stat().st_size!=item.file_size:path.write_bytes(archive.read(item))
            entries.append({'archive_entry':item.filename,'path':str(path.relative_to(ROOT)),
                            'bytes':item.file_size,'crc32':f'{item.CRC:08x}','sha256':sha(path)})
    cat=SOURCE/'bicolor_cat.glb';assert cat.exists(),'Download the publicly distributed CAT_URL first; no login/paywall bypass.'
    data,binary=glb(cat);assert data['asset']['extras']['license'].startswith('CC-BY-4.0')
    for name,index,size in [('cat_base',0,1024),('cat_normal',2,512)]:
        image=data['images'][index];view=data['bufferViews'][image['bufferView']];start=view.get('byteOffset',0)
        im=Image.open(io.BytesIO(binary[start:start+view['byteLength']])).convert('RGBA'if name.endswith('base')else'RGB')
        im.thumbnail((size,size),Image.Resampling.LANCZOS);im.save(TEXTURES/(name+'.png'))
    dog=Image.open(SOURCE/'T_labrador.png').convert('RGBA');dog.thumbnail((1024,1024),Image.Resampling.LANCZOS);dog.save(TEXTURES/'dog_base.png')
    textures={}
    for path in sorted(TEXTURES.glob('*.png')):
        textures[path.stem]={'path':str(path.relative_to(ROOT)),'sha256':sha(path),'dimensions':list(Image.open(path).size)}
    provenance={'schema_version':1,'dog':{'source':'50+ Animated Animals, Labrador Dog, purchased archive supplied by user',
        'archive_external_original':str(ARCHIVE),'archive_bytes':ARCHIVE.stat().st_size,'license':PROPRIETARY,'selected_source_entries':entries},
        'cat':{'title':'Bicolor Cat','creator':'kenchoo; original Fripouille mesh by guillaume bolis',
        'source_page':CAT_PAGE,'redistribution_source':CAT_URL,'license':'CC BY 4.0','license_url':CAT_LICENSE,
        'path':str(cat.relative_to(ROOT)),'sha256':sha(cat),'embedded_source_metadata':data['asset']['extras'],
        'original_mesh_source':'https://skfb.ly/6TKqH'},'textures':textures,
        'conversion':'Source color/UV retained; bounded offline evaluated poses, matte nonmetallic fur, cat source normal map. Cat source standing clip plus authored offline four-paw IK walk. Dog source walking/sitting clips. No fur shell/particle simulation.',
        'estimated_texture_rgba_mips_mib':sum(math.prod(t['dimensions'])*4*4/3 for t in textures.values())/1048576}
    (ASSETS/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n')
    (ASSETS/'LICENSE_SOURCE.txt').write_text('Bicolor Cat by kenchoo, based on "3d modelling my cat: Fripouille" by guillaume bolis.\n'+CAT_PAGE+'\nOriginal: https://skfb.ly/6TKqH\nCreative Commons Attribution 4.0: '+CAT_LICENSE+'\nChanges: bounded offline pose conversion, matte PBR fur, scale/ground alignment and authored four-paw IK walk; source head/breathing idle retained.\n\nLabrador: '+PROPRIETARY+'\nOriginal archive: '+str(ARCHIVE)+'\nChanges: source-clip baking, geometry reduction, 1K source-color texture and grounded scale.\n')
    print(json.dumps(provenance,indent=2));return provenance

def make_material(bpy,species):
    mat=bpy.data.materials.new('VillagePet_'+species);mat.use_nodes=True
    nodes,links=mat.node_tree.nodes,mat.node_tree.links;p=nodes.get('Principled BSDF')
    p.inputs['Metallic'].default_value=0;p.inputs['Roughness'].default_value=.83
    tex=nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(TEXTURES/(species+'_base.png')),check_existing=True)
    links.new(tex.outputs['Color'],p.inputs['Base Color'])
    if species=='cat':
        normal=nodes.new('ShaderNodeTexImage');normal.image=bpy.data.images.load(str(TEXTURES/'cat_normal.png'),check_existing=True);normal.image.colorspace_settings.name='Non-Color'
        convert=nodes.new('ShaderNodeNormalMap');convert.inputs['Strength'].default_value=.55
        links.new(normal.outputs['Color'],convert.inputs['Color']);links.new(convert.outputs['Normal'],p.inputs['Normal'])
    mat.use_backface_culling=False;return mat

def inspect(directory,records):
    textures={};geometry=0;topology={}
    for r in records:
        path=directory/r['file'];data,binary=glb(path)
        assert not data.get('skins')and not data.get('animations')and not data.get('extensionsRequired')
        assert len(data['meshes'])==1
        primitives=data['meshes'][0]['primitives'];assert len(primitives)==1
        primitive=primitives[0];assert primitive.get('mode',4)==4 and 'COLOR_0'not in primitive['attributes']
        assert set(primitive['attributes'])>= {'POSITION','NORMAL','TEXCOORD_0'}
        triangles=data['accessors'][primitive['indices']]['count']//3;assert 1000<=triangles<=9000
        image_views=set();images=[]
        for mat in data['materials']:
            pbr=mat['pbrMetallicRoughness'];assert pbr.get('metallicFactor',1)==0
            assert .75<=pbr.get('roughnessFactor',1)<=.9
            for field,srgb in [(pbr.get('baseColorTexture'),True),(mat.get('normalTexture'),False)]:
                if not field:continue
                assert field.get('texCoord',0)==0 and not field.get('extensions')
                tex=data['textures'][field['index']];im=data['images'][tex['source']];bv=data['bufferViews'][im['bufferView']];image_views.add(im['bufferView'])
                start=bv.get('byteOffset',0);encoded=binary[start:start+bv['byteLength']]
                assert encoded[:8]==b'\x89PNG\r\n\x1a\n';w,h=struct.unpack_from('>II',encoded,16)
                assert max(w,h)<=1024;key=(hashlib.sha256(encoded).hexdigest(),srgb)
                textures[key]=(w,h,len(encoded));images.append(key)
        # Exact static topology and UV bytes must match all adjacent poses.
        accessors=[primitive['indices'],primitive['attributes']['TEXCOORD_0']]
        signature=[]
        for index in accessors:
            a=data['accessors'][index];v=data['bufferViews'][a['bufferView']];start=v.get('byteOffset',0)+a.get('byteOffset',0)
            component_bytes={5121:1,5123:2,5125:4,5126:4}[a['componentType']]
            element_bytes=component_bytes*{'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
            stride=v.get('byteStride',element_bytes)
            packed=b''.join(binary[start+i*stride:start+i*stride+element_bytes]for i in range(a['count']))
            signature.append((a['componentType'],a['count'],a['type'],hashlib.sha256(packed).hexdigest()))
        if r['species']in topology:assert topology[r['species']]==(signature,images),(r['file'],'Changing topology, UV or images')
        else:topology[r['species']]=(signature,images)
        geometry+=sum(v['byteLength']for i,v in enumerate(data['bufferViews'])if i not in image_views)
        r.update(sha256=sha(path),bytes=path.stat().st_size,triangles=triangles,primitive_count=1)
    budget={'pose_count':len(records),'unique_texture_policies':len(textures),
            'estimated_texture_rgba_mips_mib':sum(w*h*4*4/3 for w,h,_ in textures.values())/1048576,
            'geometry_bytes':geometry,'max_pose_triangles':max(r['triangles']for r in records)}
    assert budget['pose_count']==32 and budget['estimated_texture_rgba_mips_mib']<20 and budget['geometry_bytes']<30000000
    return budget

def export_static(path,mesh,species):
    """Keep fixed source vertex/UV identities across poses.

    Generic exporters deduplicate by each pose's changing corner normal, which
    changes a few cat vertices at nearly coplanar fur seams. Build one stable
    smooth-fur vertex per source vertex/UV instead; no animated topology merges.
    """
    from mathutils import Vector
    mesh.calc_loop_triangles();uv=mesh.uv_layers.active.data
    vertices=[];normals=[];uvs=[];indices=[];keys={}
    for tri in mesh.loop_triangles:
        for loop_index in tri.loops:
            loop=mesh.loops[loop_index];coord=uv[loop_index].uv
            key=(loop.vertex_index,struct.pack('<ff',*coord))
            if key not in keys:
                keys[key]=len(vertices);v=mesh.vertices[loop.vertex_index].co
                vertices.append((v.x,v.z,-v.y));normals.append(Vector((0,0,0)));uvs.append((coord.x,1-coord.y))
            index=keys[key];n=mesh.corner_normals[loop_index].vector
            normals[index]+=Vector((n.x,n.z,-n.y));indices.append(index)
    normals=[tuple(n.normalized())for n in normals]
    binary=bytearray();views=[];accessors=[]
    def view(raw,target=None):
        while len(binary)%4:binary.append(0)
        index=len(views);v={'buffer':0,'byteOffset':len(binary),'byteLength':len(raw)}
        if target:v['target']=target
        views.append(v);binary.extend(raw);return index
    def attribute(values,components):
        raw=b''.join(struct.pack('<'+'f'*components,*value)for value in values)
        index=len(accessors);accessors.append({'bufferView':view(raw,34962),'componentType':5126,
            'count':len(values),'type':'VEC'+str(components),
            'min':[min(v[i]for v in values)for i in range(components)],
            'max':[max(v[i]for v in values)for i in range(components)]});return index
    attributes={'POSITION':attribute(vertices,3),'NORMAL':attribute(normals,3),'TEXCOORD_0':attribute(uvs,2)}
    index_accessor=len(accessors);accessors.append({'bufferView':view(struct.pack('<'+'I'*len(indices),*indices),34963),
        'componentType':5125,'count':len(indices),'type':'SCALAR'})
    image_paths=[TEXTURES/(species+'_base.png')]+([TEXTURES/'cat_normal.png']if species=='cat'else[])
    images=[{'bufferView':view(p.read_bytes()),'mimeType':'image/png'}for p in image_paths]
    material={'name':'VillagePet_'+species,'doubleSided':True,
        'pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicFactor':0,'roughnessFactor':.83}}
    if species=='cat':material['normalTexture']={'index':1,'scale':.55}
    data={'asset':{'version':'2.0','generator':'Valwood bounded offline pet cook'},'scene':0,'scenes':[{'nodes':[0]}],
        'nodes':[{'mesh':0}],'meshes':[{'primitives':[{'attributes':attributes,'indices':index_accessor,'material':0,'mode':4}]}],
        'materials':[material],'images':images,'textures':[{'source':i,'sampler':0}for i in range(len(images))],
        'samplers':[{'magFilter':9729,'minFilter':9987,'wrapS':10497,'wrapT':10497}],
        'bufferViews':views,'accessors':accessors,'buffers':[{'byteLength':len(binary)}]}
    encoded=json.dumps(data,separators=(',',':')).encode()
    encoded+=b' '*((-len(encoded))%4);binary.extend(b'\0'*((-len(binary))%4))
    total=12+8+len(encoded)+8+len(binary)
    path.write_bytes(struct.pack('<III',0x46546c67,2,total)+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+
        struct.pack('<II',len(binary),0x004e4942)+binary)

def cook():
    import bpy
    from mathutils import Vector,Matrix,Quaternion
    provenance=json.loads((ASSETS/'PROVENANCE.json').read_text());records=[]
    OUTPUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='valwood-pets-')as temporary:
        stage=Path(temporary)
        for species in ['cat','dog']:
            bpy.ops.wm.read_factory_settings(use_empty=True)
            if species=='cat':
                bpy.ops.import_scene.gltf(filepath=str(SOURCE/'bicolor_cat.glb'))
                originals=[o for o in bpy.context.scene.objects if o.type=='MESH'and o.name!='Icosphere']
            else:
                bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Labrador.blend'))
                originals=[o for o in bpy.context.scene.objects if o.type=='MESH']
            arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
            scene=bpy.context.scene;scene.frame_set(1);bpy.context.view_layer.update()
            material=make_material(bpy,species)
            source_triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons)for o in originals)
            for obj in originals:
                # Shape animation is evaluated before clearing; retain source Basis.
                if obj.data.shape_keys:obj.shape_key_clear()
                for a in list(obj.data.color_attributes):obj.data.color_attributes.remove(a)
                # Cat's normal UV2 and base UV0 are byte-identical in source.
                if len(obj.data.uv_layers)>1:
                    uv=obj.data.uv_layers[0]
                    for other in list(obj.data.uv_layers)[1:]:
                        assert all((a.uv-b.uv).length<1e-6 for a,b in zip(uv.data,other.data))
                    while len(obj.data.uv_layers)>1:obj.data.uv_layers.remove(obj.data.uv_layers[-1])
                obj.data.materials.clear();obj.data.materials.append(material)
                for polygon in obj.data.polygons:polygon.material_index=0
                if species=='dog':
                    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
                    modifier=obj.modifiers.new('Offline pet geometry budget','DECIMATE');modifier.ratio=7600/source_triangles;modifier.use_collapse_triangulate=True
                    while obj.modifiers.find(modifier.name)>0:bpy.ops.object.modifier_move_up(modifier=modifier.name)
                    bpy.ops.object.modifier_apply(modifier=modifier.name)
            if arm.animation_data:
                for track in arm.animation_data.nla_tracks:track.mute=True
            cat_action=bpy.data.actions.get('Animation')if species=='cat'else None
            arm.animation_data_clear();arm.animation_data_create()
            bones=arm.pose.bones
            if species=='cat':
                # The supplied cat clip is standing head/breathing motion.
                arm.animation_data.action=cat_action;arm.animation_data.action_slot=cat_action.slots[0];scene.frame_set(1);bpy.context.view_layer.update()
            else:
                action=bpy.data.actions['labrador_walk_fwd_01'];arm.animation_data.action=action;arm.animation_data.action_slot=action.slots[0];scene.frame_set(1);bpy.context.view_layer.update()
            neutral={b.name:b.matrix_basis.copy()for b in bones}
            original_world=arm.matrix_world.copy()
            graph=bpy.context.evaluated_depsgraph_get();reference=[o.matrix_world@v.co for o in originals for v in o.evaluated_get(graph).data.vertices]
            scale=1. if species=='cat'else .9
            origin_x=0.;origin_y=0.
            targets=[];constraints=[]
            if species=='cat':
                arm.animation_data.action=None
                for side,phase in [('l',0),('r',math.pi)]:
                    for leg,offset in [('FrontLeg',0),('HindLeg',math.pi*.5)]:
                        name=f'Wolf_{side}_{leg}_AnkleSHJnt_'+({'l_FrontLeg':2,'r_FrontLeg':8,'l_HindLeg':24,'r_HindLeg':30}[side+'_'+leg]).__str__()
                        knee=f'Wolf_{side}_{leg}_'+('KneeSHJnt_'if leg=='FrontLeg'else'Knee2SHJnt_')+str({'l_FrontLeg':3,'r_FrontLeg':9,'l_HindLeg':25,'r_HindLeg':31}[side+'_'+leg])
                        target=bpy.data.objects.new('Offline_'+side+leg,None);scene.collection.objects.link(target)
                        target.location=arm.matrix_world@bones[name].head
                        constraint=bones[knee].constraints.new('IK');constraint.target=target;constraint.chain_count=2 if leg=='FrontLeg'else 3;constraint.iterations=80;constraint.use_stretch=False
                        targets.append((target,target.location.copy(),phase+offset));constraints.append(constraint)
            for mode,count in MODES:
                for frame in range(count):
                    fraction=frame/count;phase=2*math.pi*fraction
                    for c in constraints:c.mute=True
                    if species=='dog':
                        name='labrador_walk_fwd_01'if mode=='walk'else'labrador_idle_sniff_pose_01'if mode=='idle'else'labrador_idle_sit_pose_01'
                        action=bpy.data.actions[name];arm.animation_data.action=action;arm.animation_data.action_slot=action.slots[0]
                        moment=1+fraction*28 if mode=='walk'else 1
                        scene.frame_set(int(moment),subframe=moment%1)
                        bpy.context.view_layer.update()
                        if mode!='walk':
                            for name in ['Neck03','Tail02']:
                                bone=bones[name];bone.rotation_mode='QUATERNION';bone.rotation_quaternion=bone.rotation_quaternion@Quaternion((0,0,1),.025*math.sin(phase))
                    else:
                        if mode=='walk':
                            arm.animation_data.action=None
                            for b in bones:b.matrix_basis=neutral[b.name].copy()
                            for target,reference,offset in targets:
                                cycle=phase+offset;target.location=reference+Vector((0,-.10*math.cos(cycle),.033*max(0.,-math.sin(cycle))))
                            for c in constraints:c.mute=False
                        else:
                            arm.animation_data.action=cat_action;arm.animation_data.action_slot=cat_action.slots[0]
                            moment=1+(fraction*208 if mode=='idle'else (fraction*208+45)%208)
                            scene.frame_set(int(moment),subframe=moment%1)
                    bpy.context.view_layer.update();graph=bpy.context.evaluated_depsgraph_get();baked=[]
                    for original in originals:
                        mesh=bpy.data.meshes.new_from_object(original.evaluated_get(graph),depsgraph=graph);mesh.transform(original.matrix_world)
                        obj=bpy.data.objects.new('Pet_'+species,mesh);scene.collection.objects.link(obj);baked.append(obj)
                    low=min(o.data.vertices[i].co.z for o in baked for p in o.data.polygons for i in p.vertices)
                    for obj in baked:
                        for v in obj.data.vertices:v.co=Vector(((v.co.x-origin_x)*scale,(v.co.y-origin_y)*scale,(v.co.z-low)*scale))
                        obj.data.update()
                    bpy.ops.object.select_all(action='DESELECT')
                    for obj in baked:obj.select_set(True)
                    bpy.context.view_layer.objects.active=baked[0]
                    if len(baked)>1:bpy.ops.object.join()
                    body=baked[0];filename=f'village_pet_{species}_{mode}_{frame:02}.glb'
                    used={index for p in body.data.polygons for index in p.vertices}
                    bounds=[[min(body.data.vertices[index].co[i]for index in used),max(body.data.vertices[index].co[i]for index in used)]for i in range(3)]
                    assert -.001<=bounds[2][0]<=.001 and bounds[2][1]<1
                    export_static(stage/filename,body.data,species)
                    records.append({'file':filename,'species':species,'mode':mode,'frame':frame,'bounds_xyz_blender':bounds,
                        'source_triangles':source_triangles,'fixed_scale':scale,'method':'Source walk/sitting clip'if species=='dog'else'Authored offline four-paw IK walk'if mode=='walk'else'Source standing head/breathing clip'})
                    print('PET POSE',species,mode,frame,bounds,flush=True)
                    mesh=body.data;bpy.data.objects.remove(body,do_unlink=True)
                    if not mesh.users:bpy.data.meshes.remove(mesh)
        budget=inspect(stage,records)
        manifest={'schema_version':1,'pet_count':12,'cats':6,'dogs':6,'pose_count':32,'blend_count':38,
            'provenance_file':str((ASSETS/'PROVENANCE.json').relative_to(ROOT)),
            'provenance_sha256':sha(ASSETS/'PROVENANCE.json'),'budget':budget,'poses':records}
        for record in records:shutil.copy2(stage/record['file'],OUTPUT/record['file'])
        (OUTPUT/'village_pets_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        print('PET BUDGET',json.dumps(budget),flush=True)
        return manifest

def test():
    manifest=json.loads((OUTPUT/'village_pets_manifest.json').read_text())
    records=manifest['poses']
    for r in records:assert sha(OUTPUT/r['file'])==r['sha256'],r['file']
    budget=inspect(OUTPUT,records)
    assert manifest['budget']==budget
    assert manifest['provenance_sha256']==sha(ASSETS/'PROVENANCE.json')
    print('PASS pet assets:32 compatible bounded poses; real purchased Labrador and CC BY4.0 source cat; source-image policies shared, textured PBR, ground-aligned bounds;',budget)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--test',action='store_true')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:]if'--'in sys.argv else sys.argv[1:]if'bpy'not in sys.modules else[])
    if args.prepare_only:prepare()
    elif args.test:test()
    else:cook()

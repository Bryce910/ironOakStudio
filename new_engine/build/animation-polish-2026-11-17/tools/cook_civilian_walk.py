#!/usr/bin/env python3
"""Cook licensed Sintel women into the existing16 bounded village pose aliases.

  blender -b -t 2 --factory-startup --disable-autoexec --python-exit-code 1 \
    --python tools/build_sintel_characters.py -- --output /tmp/valwood-sintel-stage

Downloaded Blender/Python scripts are never executed. Source originals stay
unchanged. Only this offline tool evaluates the supplied skeleton. Runtime
receives ordinary five-material static GLBs with shared encoded images.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
import subprocess
import sys

ROOT=Path('/home/computer/Desktop/final_engine/new_engine')
SOURCE=ROOT/'assets/characters/incoming/newcandidate/sintelgame'
ORIGINAL=SOURCE/'sintel_rig_2.blend'
SOURCE_SHA='d98ec188ed191b803fa774392f90bb7e7b4c419aca6970ffe626b5b95c22f2d1'
PREPARED=SOURCE/'prepared'
ROLES={'traderwoman':[('idle',4)],'travelerwoman':[('walk',8),('idle',4)]}
PAGE='https://github.com/jonburesh/sintelgame'
ART_LICENSE='https://durian.blender.org/sharing/'
MATERIALS=['sintel','sintel_metal','hair','eyes','eyelash']

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def prepare():
    """System-Python/Pillow preparation, including secondary-AO-UV remapping."""
    import numpy as np
    from PIL import Image
    assert sha(ORIGINAL)==SOURCE_SHA
    PREPARED.mkdir(exist_ok=True)
    contract=json.loads((SOURCE/'source-uv-contract.json').read_text())
    assert contract['source_sha256']==SOURCE_SHA
    source=SOURCE/'textures'
    def read(name,mode='RGBA'):return Image.open(source/name).convert(mode)
    def save(image,name):
        image.save(PREPARED/name,compress_level=6)
        return {'file':name,'dimensions':list(image.size),'sha256':sha(PREPARED/name)}
    records=[]
    # Exact original source albedo pixels: no lighting/tint/ambient baked in.
    records.append(save(read('sintel_diff.png'),'body_base.png'))
    normal=np.asarray(read('sintel_nrm.png','RGB')).copy()
    normal[np.all(normal==0,axis=2)]=(128,128,255)
    # This writer flips V and relies on the engine's generated UV tangent
    # basis, whose bitangent therefore flips. Match source world normals.
    normal[:,:,1]=255-normal[:,:,1]
    # Keep minified vector length for the renderer's existing specular AA.
    records.append(save(Image.fromarray(normal).resize((1024,1024),Image.Resampling.BOX),'body_normal.png'))
    size=512;ao=np.ones((size,size),dtype=np.float32)
    image=np.asarray(read('sintel_uv_ao.jpg','L'),dtype=np.float32)/255
    height,width=image.shape
    for triangle in contract['triangles']:
        if triangle['material'] not in ('sintel','sintel_metal'):continue
        uv=np.array(triangle['uv'],dtype=np.float64);target=uv*np.array([size,-size])+np.array([-.5,size-.5])
        lo=np.maximum(np.floor(target.min(axis=0)).astype(int),0);hi=np.minimum(np.ceil(target.max(axis=0)).astype(int),size-1)
        if np.any(lo>hi):continue
        a,b,c=target;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-10:continue
        y,x=np.mgrid[lo[1]:hi[1]+1,lo[0]:hi[0]+1]
        wa=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den
        wb=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den;wc=1-wa-wb
        inside=(wa>=-1e-7)&(wb>=-1e-7)&(wc>=-1e-7)
        secondary=np.array(triangle['ao'],dtype=np.float64)
        u=wa*secondary[0,0]+wb*secondary[1,0]+wc*secondary[2,0]
        v=wa*secondary[0,1]+wb*secondary[1,1]+wc*secondary[2,1]
        px=np.clip(u*width-.5,0,width-1);py=np.clip((1-v)*height-.5,0,height-1)
        ix=px.astype(int);iy=py.astype(int);tx=px-ix;ty=py-iy
        value=(image[iy,ix]*(1-tx)*(1-ty)+image[iy,np.minimum(ix+1,width-1)]*tx*(1-ty)+
            image[np.minimum(iy+1,height-1),ix]*(1-tx)*ty+image[np.minimum(iy+1,height-1),np.minimum(ix+1,width-1)]*tx*ty)
        region=ao[lo[1]:hi[1]+1,lo[0]:hi[0]+1];region[inside]=value[inside]
    # Legacy specular luminance is not authored roughness or metallic data.
    # Use a conservative documented approximation, never its opaque alpha.
    spec=np.asarray(read('sintel_spec.png','L').resize((size,size),Image.Resampling.BOX),dtype=np.float32)/255
    rough=np.clip(.76-.30*spec,.50,.76)
    arm=np.stack([np.uint8(np.clip(ao*255,0,255)),np.uint8(rough*255),np.full((size,size),255,np.uint8)],axis=2)
    records.append(save(Image.fromarray(arm),'body_arm.png'))
    tile=448;gutter=4;hair=Image.new('RGBA',(tile*3,tile),(0,0,0,0))
    for i in range(3):
        value=read(f'sintel_hair{i+1}.png').resize((tile-2*gutter,tile-2*gutter),Image.Resampling.LANCZOS)
        padded=np.pad(np.asarray(value),((gutter,gutter),(gutter,gutter),(0,0)),mode='edge')
        hair.paste(Image.fromarray(padded),(i*tile,0))
    records.append(save(hair,'hair_base.png'))
    side=128;eye_base=read('sintel_eyeball_diff.png').resize((side,side),Image.Resampling.LANCZOS)
    normal=np.asarray(read('eye_nrm2.png','RGB')).copy();normal[:,:,1]=255-normal[:,:,1]
    eye_normal=Image.fromarray(normal).resize((side,side),Image.Resampling.BOX)
    lash_source=read('eyelash.png');alpha=np.asarray(lash_source)[:,:,3]
    assert max(alpha[0].max(),alpha[-1].max(),alpha[:,0].max(),alpha[:,-1].max())<.4*255
    lash=lash_source.resize((side,side),Image.Resampling.LANCZOS)
    assert np.asarray(lash)[:,:,:3].max()==0 # No pupil/color can enter any lash mip.
    records.extend([save(eye_base,'eye_base.png'),save(eye_normal,'eye_normal.png'),save(lash,'lash_base.png')])
    budget=sum(math.prod(r['dimensions'])*4*4/3 for r in records)/1048576
    assert budget<=32,budget
    result={'source_sha256':SOURCE_SHA,'textures':records,'estimated_rgba8_mips_mib':budget,
        'normal_convention':'Source Blender tangent normal RGB; green inverted once because direct-writer V-flip also flips generated bitangent. Black unused background replaced by neutral before area reduction; no renormalization erases filtering variance.',
        'ao':'Actual secondary AO UV layer raster-remapped into primary UVTex at512; albedo unchanged.',
        'roughness':'Conservative .76-.30*legacy specular luminance, bounded.50..76; source alpha is not roughness.',
        'atlas':'Source alpha retained; three448hair tiles with four-pixel extruded gutters. Separate128eye and lash images prevent pupil bleed at every mip; source UV remapped consistently.'}
    (PREPARED/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print('SINTEL TEXTURES',json.dumps(result),flush=True);return result

def write_glb(path,name,vertices,indices,images):
    """Static vertex correspondence is chosen once from rest corner keys."""
    import numpy as np
    binary=bytearray();views=[];accessors=[]
    def view(data):
        while len(binary)%4:binary.append(0)
        offset=len(binary);binary.extend(data);views.append({'buffer':0,'byteOffset':offset,'byteLength':len(data)});return len(views)-1
    def accessor(data,kind,component,bounds=False):
        array=np.asarray(data,dtype='<f4' if component==5126 else '<u4');row={'bufferView':view(array.tobytes()),'componentType':component,'count':len(array),'type':kind}
        if bounds:row.update(min=array.min(axis=0).tolist(),max=array.max(axis=0).tolist())
        accessors.append(row);return len(accessors)-1
    primitives=[]
    for material,(points,normals,uv) in enumerate(vertices):
        primitives.append({'attributes':{'POSITION':accessor(points,'VEC3',5126,True),'NORMAL':accessor(normals,'VEC3',5126),'TEXCOORD_0':accessor(uv,'VEC2',5126)},'indices':accessor(indices[material],'SCALAR',5125),'material':material,'mode':4})
    image_rows=[{'bufferView':view(data),'mimeType':'image/png','name':filename}for filename,data in images]
    def material(name,base,normal,arm,metal=0,mask=False):
        row={'name':name,'pbrMetallicRoughness':{'baseColorTexture':{'index':base},'metallicFactor':metal,'roughnessFactor':1.,'baseColorFactor':[1,1,1,1]},'doubleSided':True,'alphaMode':'MASK' if mask else 'OPAQUE'}
        if normal is not None:row['normalTexture']={'index':normal,'scale':1.}
        if arm is not None:row['pbrMetallicRoughness']['metallicRoughnessTexture']={'index':arm};row['occlusionTexture']={'index':arm}
        if arm is None:row['pbrMetallicRoughness']['roughnessFactor']=.75
        if mask:row['alphaCutoff']=.4
        return row
    document={'asset':{'version':'2.0','generator':'Valwood source-faithful Sintel offline pose cooker1'},'buffers':[{'byteLength':len(binary)}],'bufferViews':views,'accessors':accessors,
        'images':image_rows,'samplers':[{'magFilter':9729,'minFilter':9987,'wrapS':33071,'wrapT':33071}],
        'textures':[{'source':i,'sampler':0}for i in range(len(images))],
        'materials':[material('Sintel cloth skin',0,1,2),material('Sintel metal fittings',0,1,2,.7),material('Sintel source alpha hair',3,None,None,mask=True),material('Sintel opaque eyes',4,5,None),material('Sintel source black alpha lashes',6,None,None,mask=True)],
        'meshes':[{'name':name,'primitives':primitives}],'nodes':[{'name':name,'mesh':0}],'scenes':[{'nodes':[0]}],'scene':0}
    document['materials'][3]['pbrMetallicRoughness']['roughnessFactor']=.25
    encoded=json.dumps(document,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4);binary+=b'\0'*((-len(binary))%4)
    raw=struct.pack('<III',0x46546c67,2,28+len(encoded)+len(binary))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary
    path.write_bytes(raw)

def cook(output,selected=None):
    import bpy,bmesh
    from mathutils import Matrix,Vector
    selected=set(ROLES if selected is None else selected);assert selected<=set(ROLES)
    assert sha(ORIGINAL)==SOURCE_SHA
    assert not bpy.context.preferences.filepaths.use_scripts_auto_execute,'Launch Blender with --disable-autoexec'
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(ORIGINAL),load_ui=False,use_scripts=False)
    body=bpy.data.objects['sintel'];rig=bpy.data.objects['sintel_rig'];scene=bpy.context.scene
    neutral={b.name:b.matrix_basis.copy()for b in rig.pose.bones}
    # Lock source quad/ngon connectivity before any skeletal deformation.
    mesh=body.data;bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='FIXED',ngon_method='EAR_CLIP');bm.to_mesh(mesh);bm.free();mesh.update();mesh.calc_normals_split()
    source_uv=[tuple(v.uv)for v in mesh.uv_layers['UVTex'].data];ao_uv=[tuple(v.uv)for v in mesh.uv_layers['AO'].data]
    triangles=[]
    for polygon in mesh.polygons:
        assert len(polygon.vertices)==3
        triangles.append({'material':mesh.materials[polygon.material_index].name,'uv':[source_uv[i]for i in polygon.loop_indices],'ao':[ao_uv[i]for i in polygon.loop_indices]})
    assert (PREPARED/'manifest.json').is_file()
    prepared=json.loads((PREPARED/'manifest.json').read_text());images=[(r['file'],(PREPARED/r['file']).read_bytes())for r in prepared['textures']]
    groups=[[]for _ in range(5)];keys=[{}for _ in range(5)];indices=[[]for _ in range(5)];uvs=[[]for _ in range(5)]
    for polygon in mesh.polygons:
        source_material=mesh.materials[polygon.material_index].name
        material=0 if source_material=='sintel' else 1 if source_material=='sintel_metal' else 2 if source_material.startswith('sintel_hair') else 4 if source_material=='eyelash' else 3
        for loop in polygon.loop_indices:
            u,v=source_uv[loop]
            if material==2:
                tile=int(source_material[-1])-1;u=(tile*448+4+u*440)/1344;v=(4+v*440)/448
            elif material==4:
                # Source lashes extend3.56% beyond their image. Their entire
                # border is below source coverage cutoff; clamp those outliers
                # into that transparent border, never into the eye atlas tile.
                u=max(0.,min(1.,u));v=max(0.,min(1.,v))
            uv=(u,1-v)
            # Rest corner normal identifies authored smooth/flat seams without
            # letting pose-dependent floating-point coincidences change keys.
            normal=tuple(round(c,6)for c in mesh.corner_normals[loop].vector)
            key=(mesh.loops[loop].vertex_index,uv,normal)
            if key not in keys[material]:
                keys[material][key]=len(groups[material]);groups[material].append(loop);uvs[material].append(uv)
            indices[material].append(keys[material][key])
    def evaluate(action,frame):
        rig.animation_data.action=None
        for bone in rig.pose.bones:bone.matrix_basis=neutral[bone.name].copy()
        rig.animation_data.action=bpy.data.actions[action];scene.frame_set(int(frame),subframe=frame-int(frame));bpy.context.view_layer.update()
    raw_evaluate=evaluate
    raw_evaluate('sintel_idle',1)
    idle_basis={bone.name:bone.matrix_basis.copy() for bone in rig.pose.bones}
    def evaluate(action,frame):
        raw_evaluate(action,frame)
        if action!='sintel_walk':return
        walking={bone.name:bone.matrix_basis.copy() for bone in rig.pose.bones}
        rig.animation_data.action=None
        # Shorten this long-stride source walk without slowing it into a jog.
        # Preserve nearly all of its authored counter-swing in the shoulders.
        for bone in rig.pose.bones:
            a,aq,asc=idle_basis[bone.name].decompose();b,bq,bsc=walking[bone.name].decompose()
            weight=.92 if any(x in bone.name.lower() for x in ('arm','elbow','hand','shoulder')) else .70
            bone.matrix_basis=Matrix.LocRotScale(a.lerp(b,weight),aq.slerp(bq,weight),asc.lerp(bsc,weight))
        bpy.context.view_layer.update()
    evaluate('sintel_idle',1)
    reference=rig.matrix_world@rig.pose.bones['DEF-hips'].head
    used_vertices={loop.vertex_index for loop in mesh.loops}
    graph=bpy.context.evaluated_depsgraph_get();ev=body.evaluated_get(graph);evaluated=ev.to_mesh();evaluated.calc_normals_split();points=[body.matrix_world@v.co for v in evaluated.vertices]
    fixed_scale=1.82/(max(points[i].z for i in used_vertices)-min(points[i].z for i in used_vertices));ev.to_mesh_clear()
    foot_indices={s:[]for s in ['L','R']};group_names={g.index:g.name for g in body.vertex_groups}
    for vertex in mesh.vertices:
        if vertex.index not in used_vertices:continue
        for side in foot_indices:
            if sum(g.weight for g in vertex.groups if group_names[g.group] in ['DEF-foot.'+side,'DEF-toe.'+side])>.4:foot_indices[side].append(vertex.index)
    assert all(foot_indices.values())
    samples={s:[]for s in ['L','R']}
    for k in range(257):
        evaluate('sintel_walk',1+29*k/256)
        for side in samples:samples[side].append((rig.matrix_world@rig.pose.bones['DEF-foot.'+side].head).y)
    strides={s:2*(max(a)-min(a))*fixed_scale for s,a in samples.items()};stride=sum(strides.values())/2
    records=[]
    for role in ROLES:
        if role not in selected:continue
        for mode,count in ROLES[role]:
            for frame in range(count):
                fraction=frame/count
                source_frame=1+29*fraction if mode=='walk' else 1+39*(.5-.5*math.cos(2*math.pi*fraction))
                evaluate('sintel_walk' if mode=='walk' else 'sintel_idle',source_frame)
                graph=bpy.context.evaluated_depsgraph_get();ev=body.evaluated_get(graph);evaluated=ev.to_mesh();evaluated.calc_normals_split();points=[body.matrix_world@v.co for v in evaluated.vertices]
                feet={s:min(points[i].z for i in ids)for s,ids in foot_indices.items()};ground=min(feet.values());origin=Vector((reference.x,reference.y,ground))
                normal_matrix=body.matrix_world.to_3x3().inverted().transposed();vertices=[]
                for material,loops in enumerate(groups):
                    positions=[];normals=[]
                    for loop in loops:
                        p=(points[evaluated.loops[loop].vertex_index]-origin)*fixed_scale
                        n=(normal_matrix@evaluated.corner_normals[loop].vector).normalized()
                        # Source faces+Y; rotate180Z, then Blender Z-up->glTF Y-up.
                        positions.append((-p.x,p.z,p.y));normals.append((-n.x,n.z,n.y))
                    vertices.append((positions,normals,uvs[material]))
                filename=f'village_character_{role}_{mode}_{frame:02}.glb';write_glb(output/filename,f'Body_{role}_{mode}_{frame:02}',vertices,indices,images)
                low=(min(points[i].z for i in used_vertices)-ground)*fixed_scale;high=(max(points[i].z for i in used_vertices)-ground)*fixed_scale
                assert low>=-.025 and abs(min(feet.values())-ground)<1e-6
                record={'file':filename,'role':role,'mode':mode,'frame':frame,'triangles':sum(len(i)//3 for i in indices),'body_triangles':sum(len(i)//3 for i in indices),'body_meshes':1,'source_body_meshes':1,'body_height_m':high-low,'body_min_y_m':low,'body_max_y_m':high,'min_foot_m':0,'foot_heights_m':{s:(v-ground)*fixed_scale for s,v in feet.items()},'reference_body_height_m':1.82,'fixed_reference_scale':fixed_scale,'horizontal_origin':'fixed source pelvis reference; source +Y rotated into glTF+Z','source':str(ORIGINAL.relative_to(ROOT)),'source_sha256':SOURCE_SHA,'source_page':PAGE,'creator':'Blender Foundation and Sintel The Game contributors','license':'CC-BY-3.0 art; MIT game project modifications','method':'Owned source walk:70% leg/pelvis excursion,92% upper-body; remeasured stride' if mode=='walk' else 'Supplied idle frames1..40, smoothly retimed out-and-back into a closed cycle','source_frame':source_frame,'walk_stride_m':stride,'stride_per_foot_m':strides,'gear_meshes':0,'bytes':(output/filename).stat().st_size,'sha256':sha(output/filename),'uv_sha256':hashlib.sha256(json.dumps(uvs,separators=(',',':')).encode()).hexdigest(),'source_vertex_colors':'No runtime vertex colors; exact source albedo and remapped UV materials'}
                records.append(record);ev.to_mesh_clear();print('SINTEL POSE',role,mode,frame,record['triangles'],'height',high-low,'stride',stride,flush=True)
    result={'schema_version':1,'sources':{'project':PAGE,'commit':'292e28020b2a9d5cf9fed47746e8b8d4036a4a4c','source_sha256':SOURCE_SHA,'art_license':ART_LICENSE},'budget':prepared,'walk_stride_m':stride,'assets':records}
    (output/'sintel_character_manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    return records

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--output',type=Path,default=Path('/tmp/valwood-sintel-stage'));parser.add_argument('--roles',default=','.join(ROLES))
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else sys.argv[1:])
    if args.prepare_only:prepare()
    else:cook(args.output,args.roles.split(','))

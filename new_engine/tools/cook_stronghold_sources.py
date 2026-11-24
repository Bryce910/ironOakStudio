#!/usr/bin/env python3
"""Blender-only cook of the user's local dwarf and dwarven architecture sources.

Original downloads are never changed. Output uses original UVs, actual supplied
animation clips and bounded PBR textures prepared by build_dwarf_stronghold.py.
"""
from pathlib import Path
import sys, math, json, copy, gc, tempfile, shutil
from zipfile import ZipFile
import bpy
import numpy as np
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_cinematic_diversity import Writer
DATA=ROOT/'assets/purchased/dwarven_stronghold'
OUT=ROOT/'projects/valwood_stronghold/models/stronghold'

def material_set(names):
    materials=[];images=[];textures=[]
    for name in names:
        spec=json.loads((DATA/'textures'/f'{name}.json').read_text())
        m=dict(name=name,pbrMetallicRoughness=dict(baseColorFactor=[1,1,1,1],roughnessFactor=1,metallicFactor=1 if 'arm'in spec else 0))
        for role in ['base','normal','arm']:
            if role not in spec:continue
            idx=len(textures);images.append(dict(uri=str(DATA/'textures'/spec[role]),mimeType='image/png'))
            textures.append(dict(source=len(images)-1,sampler=0));view=dict(index=idx)
            if role=='base':m['pbrMetallicRoughness']['baseColorTexture']=view
            elif role=='normal':m['normalTexture']=view
            else:m['pbrMetallicRoughness']['metallicRoughnessTexture']=view;m['occlusionTexture']=view
        if 'Hair' in name or 'Beard' in name:m.update(alphaMode='MASK',alphaCutoff=.35,doubleSided=True)
        materials.append(m)
    return materials,textures,images

def bake(objects,path,materials,material_for,origin,scale=1):
    graph=bpy.context.evaluated_depsgraph_get();groups={}
    for obj in objects:
        evaluated=obj.evaluated_get(graph);mesh=evaluated.to_mesh();mesh.calc_loop_triangles()
        matrix=obj.matrix_world;normal_matrix=matrix.to_3x3().inverted().transposed()
        positions=np.empty((len(mesh.vertices),3),dtype=np.float32);mesh.vertices.foreach_get('co',positions.ravel())
        corners=np.empty(len(mesh.loops),dtype=np.int32);mesh.loops.foreach_get('vertex_index',corners)
        normals=np.empty((len(mesh.loops),3),dtype=np.float32)
        # Triangulation alone does not compute split normals on Blender 4.0.
        # Request them explicitly there; newer releases expose the computed
        # corner normals directly. Keep smooth faces and authored hard edges.
        if hasattr(mesh,'calc_normals_split'):mesh.calc_normals_split()
        if hasattr(mesh,'corner_normals')and len(mesh.corner_normals)==len(mesh.loops):mesh.corner_normals.foreach_get('vector',normals.ravel())
        else:mesh.loops.foreach_get('normal',normals.ravel())
        uv=np.zeros((len(mesh.loops),2),dtype=np.float32)
        if mesh.uv_layers.active:mesh.uv_layers.active.data.foreach_get('uv',uv.ravel())
        uv[:,1]=1.-uv[:,1]
        triangles=np.empty((len(mesh.loop_triangles),3),dtype=np.int32);mesh.loop_triangles.foreach_get('loops',triangles.ravel())
        slots=np.empty(len(triangles),dtype=np.int32);mesh.loop_triangles.foreach_get('material_index',slots)
        used=np.unique(triangles)
        invalid=used[(~np.isfinite(normals[used]).all(axis=1))|(np.linalg.norm(normals[used],axis=1)<1e-8)]
        if len(invalid):
            # A few source animation corners collapse during a bend. Retain
            # their topology for pose interpolation, borrowing the closest
            # valid evaluated surface normal only for those invisible corners.
            # A missing normal stream still fails rather than becoming clay.
            valid=used[np.isfinite(normals[used]).all(axis=1)&(np.linalg.norm(normals[used],axis=1)>.5)]
            if not len(valid)or len(invalid)>max(32,len(used)*.01):raise RuntimeError(f'Missing split normal stream in {obj.name}: {path.name}')
            points=positions[corners[valid]]
            for loop in invalid:
                delta=points-positions[corners[loop]];closest=valid[np.argmin(np.einsum('ij,ij->i',delta,delta))];normals[loop]=normals[closest]
            print('COLLAPSED CORNER NORMALS',obj.name,path.name,len(invalid),flush=True)
        # Preserve winding when a source object uses a mirrored transform.
        if matrix.to_3x3().determinant()<0:triangles=triangles[:,[0,2,1]]
        transform=np.array(matrix);positions=(positions@transform[:3,:3].T+transform[:3,3]-np.array(origin))*scale
        normals=normals@np.array(normal_matrix).T
        length=np.linalg.norm(normals,axis=1)
        if not np.isfinite(normals[used]).all()or np.any(length[used]<1e-8):raise RuntimeError(f'Invalid split normals in {obj.name}: {path.name}')
        normals/=np.maximum(length[:,None],1e-12)
        positions=positions[:,[0,2,1]]*np.array([1,1,-1]);normals=normals[:,[0,2,1]]*np.array([1,1,-1])
        for source_slot in np.unique(slots):
            slot=min(int(source_slot),max(0,len(obj.data.materials)-1));mat=material_for(obj,obj.data.materials[slot]if obj.data.materials else None)
            loops=triangles[slots==source_slot].ravel()
            groups.setdefault(mat,[]).append((positions[corners[loops]],normals[loops],uv[loops]))
        evaluated.to_mesh_clear()
    w=Writer(*copy.deepcopy(materials))
    w.g['asset']['extras']={'stronghold_uv_origin':'top-left','stronghold_split_normals':2}
    count=0
    for mat,pieces in groups.items():
        p,n,uv=[np.concatenate([piece[i]for piece in pieces])for i in range(3)];idx=np.arange(len(p),dtype=np.uint32).reshape(-1,3)
        w.mesh(path.stem+str(mat),p,n,uv,idx,mat);count+=len(idx)
    temp=path.with_suffix('.cooking');w.save(temp,embedded=True);temp.replace(path)
    return count

def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True);gc.collect()

def architecture():
    source=DATA/'source/architecture/Models'
    for stem,label in [('StoneArchDEP_ST','source_arch'),('DungeonStructureHeadDEP_ST','source_head'),('PillarADEP_ST','source_pillar'),('DungeonGateDEP_ST','source_gate')]:
        clear();bpy.ops.import_scene.fbx(filepath=str(source/(stem+'.fbx')))
        objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
        points=[o.matrix_world@v.co for o in objects for v in o.data.vertices]
        lo=Vector([min(p[i]for p in points)for i in range(3)]);hi=Vector([max(p[i]for p in points)for i in range(3)])
        origin=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
        # Raw architecture faces +X. Rotate to face +Z before baking.
        from mathutils import Matrix
        rotation=Matrix.Rotation(-math.pi/2,4,'Z')
        for o in objects:o.matrix_world=rotation@o.matrix_world
        origin=rotation@origin
        mats=material_set([stem])
        tris=bake(objects,OUT/(label+'.glb'),mats,lambda o,m:0,origin)
        print('COOK ARCH',label,'source bounds',list(lo),list(hi),'triangles',tris,flush=True)

def dwarf(family):
    clear()
    if family=='King':bpy.ops.import_scene.gltf(filepath='/home/computer/Downloads/Dwarf_king.glb')
    else:
        # Extract a single working FBX and immediately remove it after import.
        with tempfile.TemporaryDirectory(prefix='grandstone-fbx-')as folder:
            p=Path(folder)/f'Dwarf_{family}.fbx'
            entry=f'Dwarf_{family}.fbx'if family!='Orme'else'Dwaf_Orme.fbx'
            with ZipFile('/home/computer/Downloads/uploads_files_2784428_fbx_dwarfs_reexport.zip')as z,z.open(entry)as src,p.open('wb')as dst:shutil.copyfileobj(src,dst)
            bpy.ops.import_scene.fbx(filepath=str(p),use_image_search=False)
    rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE'];assert rigs,family
    rig=rigs[0];rig.animation_data_create()
    for track in rig.animation_data.nla_tracks:track.mute=True
    detached={'King':{'Axe_Mesh'},'Broddi':{'Spear_Mesh'},'Orme':{'Arrows_Mesh','Weapon_Hand'}}
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.lower().startswith('icosphere') and o.name not in detached.get(family,set())]
    assert meshes,family
    print('SOURCE',family,[(o.name,len(o.data.vertices),[m.name for m in o.data.materials])for o in meshes],flush=True)
    def triangles(o):o.data.calc_loop_triangles();return len(o.data.loop_triangles)
    original_triangles=sum(triangles(o)for o in meshes)
    if original_triangles>90000:
        candidates=sorted([o for o in meshes if not any(s in o.name.lower()for s in ['head','beard','hair','jaw','eyes','mustache','eyebrow'])],key=triangles,reverse=True)
        total=original_triangles
        for obj in candidates:
            if total<=86000:break
            count=triangles(obj);reduction=min(count*.65,total-85000)
            if reduction<100:continue
            bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
            modifier=obj.modifiers.new('Bounded crowd LOD','DECIMATE');modifier.ratio=max(.35,1-reduction/count)
            while list(obj.modifiers).index(modifier)>0:bpy.ops.object.modifier_move_up(modifier=modifier.name)
            bpy.ops.object.modifier_apply(modifier=modifier.name)
            total=sum(triangles(o)for o in meshes)
        assert total<=100000,(family,total)
        (DATA/f'{family.lower()}_optimization.json').write_text(json.dumps(dict(source_triangles=original_triangles,crowd_triangles=total,face_and_hair_geometry_unchanged=True,method='Offline decimation of dense body/equipment geometry before animation sampling'),indent=2)+'\n')
        print('CROWD LOD',family,original_triangles,'->',total,'triangles; face/hair retained',flush=True)
    names=[f'Dwarf_{family}_{part}'for part in ['Body','Head','Beard','Details','Forge','Weapon']if(DATA/'textures'/f'Dwarf_{family}_{part}.json').exists()]+['Axe']
    mats=material_set(names)
    def mat_for(o,m):
        n=(m.name if m else o.name).lower()
        base=(m.name if m else o.name).split('.')[0]
        if base in names:return names.index(base)
        if 'axe' in n or 'weapon' in n or 'crossbow' in n:return names.index('Axe')
        if 'hair' in n or 'beard' in n:return names.index(f'Dwarf_{family}_Beard')
        if 'head' in n:return names.index(f'Dwarf_{family}_Head')
        return names.index(f'Dwarf_{family}_Body')
    actions=list(bpy.data.actions)
    def action(mode):
        matches=[a for a in actions if a.name.lower().split('_dwarf')[0].split('|')[-1] == mode.lower()]
        if not matches:matches=[a for a in actions if mode.lower() in a.name.lower()and 'back'not in a.name.lower()and 'left'not in a.name.lower()and 'right'not in a.name.lower()and 'sit'not in a.name.lower()and 'other'not in a.name.lower()]
        assert matches,(family,mode,[a.name for a in actions]);return min(matches,key=lambda a:len(a.name))
    rig.animation_data.action=action('Idle');bpy.context.scene.frame_set(int(rig.animation_data.action.frame_range[0])+2);bpy.context.view_layer.update()
    graph=bpy.context.evaluated_depsgraph_get()
    def head_material(o):return any('_head'in m.name.lower()for m in o.data.materials if m)
    # Bedvar's face is named "subtool1". Its assigned head material identifies
    # it reliably; shoe geometry alone must never become the stature reference.
    body_reference=[o for o in meshes if head_material(o)or any(part in o.name.lower()for part in ['head','shoes','helmet','crown'])]
    assert any(head_material(o)for o in body_reference),(family,'Missing head reference')
    points=[o.matrix_world@v.co for o in body_reference for v in o.evaluated_get(graph).data.vertices]
    low=min(p.z for p in points);high=max(p.z for p in points)
    # Use the same 1.48m reference stature for each original dwarf family.
    scale=1.48/(high-low)
    pelvis=next((b for b in rig.pose.bones if b.name.lower().endswith('pelvis')),rig.pose.bones[0])
    records=[]
    for mode,count in [('idle',4),('walk',8)]:
        clip=action(mode);rig.animation_data.action=clip;start,end=clip.frame_range
        for i in range(count):
            frame=start+(end-start)*i/count;bpy.context.scene.frame_set(int(frame),subframe=frame-int(frame));bpy.context.view_layer.update()
            graph=bpy.context.evaluated_depsgraph_get();sole=[]
            for o in meshes:
                if any(n in o.name.lower()for n in ['shoe','boot','foot']):sole.extend((o.matrix_world@v.co).z for v in o.evaluated_get(graph).data.vertices)
            if not sole:sole=[(o.matrix_world@v.co).z for o in meshes for v in o.evaluated_get(graph).data.vertices]
            root=rig.matrix_world@pelvis.head;origin=Vector((root.x,root.y,min(sole)))
            dest=OUT/f'dwarf_{family.lower()}_{mode}_{i:02d}.glb'
            tris=bake(meshes,dest,mats,mat_for,origin,scale)
            records.append(dict(file=dest.name,source_action=clip.name,source_frame=frame,triangles=tris))
        print('COOK DWARF',family,mode,count,'scale',scale,flush=True)
    (DATA/f'{family.lower()}_poses.json').write_text(json.dumps(records,indent=2)+'\n')

if __name__=='__main__':
    OUT.mkdir(parents=True,exist_ok=True)
    args=sys.argv[sys.argv.index('--')+1:]if '--'in sys.argv else[]
    if not args or 'architecture'in args:architecture()
    for family in args:
        if family!='architecture':dwarf(family)

"""Replace coarse mature conifer shoot sheets with small source-aware shoots.
Blender --background --threads 2 --python tools/refine_mature_canopies.py
Uses existing verified source clouds/wood/photographs; Cycles CPU, two threads.
The previous six mature-tree GLBs and manifest are copied once to the historical
authored-landscape baseline before writing runtime files. Seldonne is untouched.
"""
from pathlib import Path
import hashlib,json,math,shutil,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_cinematic_assets as base

BACKUP=base.ROOT/'docs/performance/authored-landscape/before/models'

def backup():
    BACKUP.mkdir(parents=True,exist_ok=True)
    names=['cinematic_'+species+'_'+lod+'.glb' for species in ['pine','fir'] for lod in ['near','mid','far']]
    names+=['cinematic_manifest.json']
    records=[]
    for name in names:
        source=base.OUT/name;dest=BACKUP/name
        if not dest.exists():shutil.copy2(source,dest)
        records.append({'file':name,'bytes':dest.stat().st_size,'sha256':base.sha(dest)})
    record=BACKUP/'baseline_hashes.json'
    if not record.exists():record.write_text(json.dumps({'historical_test_data':True,'purpose':'Original large-shoot conifer asset baseline, before finer shoots',
        'license':'CC0-1.0','files':records},indent=2))
    old_tool=BACKUP/'build_cinematic_assets_before.py'
    if not old_tool.exists():shutil.copy2(Path(base.__file__),old_tool)

def cook():
    import bpy,numpy as np
    from mathutils import Vector,Quaternion
    from bake_gltf_uv import bake_uv_transforms
    import bmesh
    backup();manifest_path=base.OUT/'cinematic_manifest.json';manifest=json.loads(manifest_path.read_text())
    def tris(o):return sum(len(p.vertices)-2 for p in o.data.polygons)
    def clear():bpy.ops.wm.read_factory_settings(use_empty=True)
    def points():return [o.matrix_world@v.co for o in bpy.context.scene.objects if o.type=='MESH' for v in o.data.vertices]
    def bounds():
        pts=points();return Vector([min(p[i] for p in pts) for i in range(3)]),Vector([max(p[i] for p in pts) for i in range(3)])
    def reduce(o,budget,keep=None):
        bpy.context.view_layer.objects.active=o;o.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.remove_doubles(threshold=.00005);bpy.ops.object.mode_set(mode='OBJECT')
        if keep:
            bm=bmesh.new();bm.from_mesh(o.data);seen=set();groups=[]
            for start in bm.verts:
                if start in seen:continue
                todo=[start];seen.add(start);group=[]
                while todo:
                    v=todo.pop();group.append(v)
                    for edge in v.link_edges:
                        other=edge.other_vert(v)
                        if other not in seen:seen.add(other);todo.append(other)
                extent=Vector([max(v.co[i] for v in group)-min(v.co[i] for v in group) for i in range(3)]).length
                groups.append((extent,group))
            groups.sort(key=lambda g:g[0],reverse=True);remove=[v for _,group in groups[keep:] for v in group]
            if remove:bmesh.ops.delete(bm,geom=remove,context='VERTS')
            bm.to_mesh(o.data);bm.free();o.data.update()
        if tris(o)>budget:
            m=o.modifiers.new('Fine-canopy woody LOD','DECIMATE');m.ratio=budget/tris(o);bpy.ops.object.modifier_apply(modifier=m.name)
        for p in o.data.polygons:p.use_smooth=True
        o.select_set(False)
    def clusters(cloud,target):
        # Unchanged immutable source positions determine the complete silhouette.
        # Sampling is deterministic and density is uniform in physical space.
        p=cloud[::3];lo,hi=.04,1.5
        for _ in range(12):
            step=(lo+hi)*.5;keys=np.floor(p/step).astype(np.int32)
            _,inv,n=np.unique(keys,axis=0,return_inverse=True,return_counts=True)
            if len(n)>target:lo=step
            else:hi=step
        keys=np.floor(p/hi).astype(np.int32);_,inv,n=np.unique(keys,axis=0,return_inverse=True,return_counts=True)
        return np.column_stack([np.bincount(inv,weights=p[:,i])/n for i in range(3)])
    def shoot_material(folder):
        m=bpy.data.materials.new('Fine photographed conifer shoots');m.use_nodes=True;m.blend_method='CLIP';m.alpha_threshold=.38
        shader=m.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=.88
        for name,kind in [('shoot_base.png','base'),('shoot_normal.png','normal'),('shoot_arm.png','arm')]:
            tex=m.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(folder/name))
            if kind!='base':tex.image.colorspace_settings.name='Non-Color'
            if kind=='base':
                m.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color']);m.node_tree.links.new(tex.outputs['Alpha'],shader.inputs['Alpha'])
            elif kind=='normal':
                normal=m.node_tree.nodes.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=.65
                m.node_tree.links.new(tex.outputs['Color'],normal.inputs['Color']);m.node_tree.links.new(normal.outputs['Normal'],shader.inputs['Normal'])
            else:
                sep=m.node_tree.nodes.new('ShaderNodeSeparateColor');m.node_tree.links.new(tex.outputs['Color'],sep.inputs['Color']);m.node_tree.links.new(sep.outputs['Green'],shader.inputs['Roughness'])
        return m
    def add_shoots(cloud,folder,height,lod,species):
        centers=clusters(cloud,4000 if lod=='near' else 1200)
        natural_height=float(cloud[:,1].max()+.10);scale=height/natural_height
        # Shoots are physically small: ~.6m pine / .5m fir at near distance.
        # The existing physical photograph ratio is preserved, avoiding stretched
        # needle images and the former metre-scale repeated sheets.
        world_length=(.62 if species=='pine' else .50)*(1.0 if lod=='near' else 1.55)
        image=bpy.data.images.load(str(folder/'shoot_base.png'));aspect=image.size[0]/image.size[1]
        length=world_length/scale;vertices=[];faces=[];uv=[];normals=[];rng=np.random.default_rng(5081 if species=='pine' else 9127)
        low=float(cloud[:,1].min());span=float(cloud[:,1].max()-low)
        for p in centers:
            c=Vector((float(p[0]),float(-p[2]),float(p[1])));radial=Vector((c.x,c.y,0));r=radial.length
            if r>.001:radial/=r
            else:radial=Vector((1,0,0))
            t=(float(p[1])-low)/max(span,.01)
            direction=(radial*(.95-.40*t)+Vector((0,0,.08+.75*t))).normalized()
            across=direction.cross(Vector((0,0,1))).normalized();across.rotate(Quaternion(direction,float(rng.uniform(0,math.pi))))
            normal=across.cross(direction).normalized()
            shoot_len=length*float(rng.uniform(.82,1.18));width=shoot_len*aspect
            bend=(lod=='near' and rng.random()<.30);rows=[0.,.5,1.] if bend else [0.,1.]
            start=len(vertices)
            for v in rows:
                curve=normal*(math.sin(v*math.pi)*shoot_len*.055) if bend else Vector((0,0,0))
                center=c+direction*((v-.5)*shoot_len)+curve
                for side in [-.5,.5]:
                    vertices.append(center+across*(side*width));uv.append((side+.5,v));normals.append(normal)
            for row in range(len(rows)-1):
                i=start+row*2;faces.extend([(i,i+1,i+3),(i,i+3,i+2)])
        mesh=bpy.data.meshes.new('Small source-aware curved shoots');mesh.from_pydata(vertices,[],faces);mesh.update()
        layer=mesh.uv_layers.new(name='UVMap')
        for poly in mesh.polygons:
            for li in poly.loop_indices:layer.data[li].uv=uv[mesh.loops[li].vertex_index]
        mesh.normals_split_custom_set_from_vertices(normals);mesh.use_auto_smooth=True
        obj=bpy.data.objects.new('Fine source-aware canopy',mesh);bpy.context.collection.objects.link(obj);obj.data.materials.append(shoot_material(folder))
        return len(centers)
    def export(name,source,extra):
        for m in bpy.data.materials:
            m.use_backface_culling=False
            if m.node_tree:
                for n in m.node_tree.nodes:
                    if n.type=='TEX_IMAGE' and n.image:n.image.pack()
        path=base.OUT/(name+'.glb');bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',export_yup=True,export_animations=False,export_cameras=False,export_lights=False,export_image_format='AUTO');bake_uv_transforms(path)
        lo,hi=bounds();entry={'asset':name,'source':source,'license':'CC0-1.0','triangles':sum(tris(o) for o in bpy.context.scene.objects if o.type=='MESH'),
                            'bounds_xyz_z_up':[[lo[i],hi[i]] for i in range(3)],'bytes':path.stat().st_size,'sha256':base.sha(path),**extra}
        manifest[:]=[r for r in manifest if r['asset']!=name];manifest.append(entry);manifest_path.write_text(json.dumps(manifest,indent=2));print('REFINED',name,entry['triangles'],flush=True)
        return entry
    def render(species,height):
        scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=32;scene.cycles.use_denoising=False
        scene.cycles.max_bounces=3;scene.cycles.transparent_max_bounces=16;scene.render.threads_mode='FIXED';scene.render.threads=2
        scene.render.resolution_x=1024;scene.render.resolution_y=1536;scene.render.resolution_percentage=100
        scene.render.film_transparent=True;scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
        world=bpy.data.worlds.new('Neutral sky');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.72,.80,1);world.node_tree.nodes['Background'].inputs[1].default_value=.7;scene.world=world
        sun=bpy.data.lights.new('Soft sun','SUN');sun.energy=1.8;sun.angle=.3;ob=bpy.data.objects.new('Soft sun',sun);bpy.context.collection.objects.link(ob);ob.rotation_euler=(.6,-.35,-.5)
        camera=bpy.data.cameras.new('Source silhouette');ob=bpy.data.objects.new('Source silhouette',camera);bpy.context.collection.objects.link(ob)
        ob.location=(0,-45,height*.5);ob.rotation_euler=(Vector((0,0,height*.5))-ob.location).to_track_quat('-Z','Y').to_euler();camera.type='ORTHO';camera.ortho_scale=height*1.06;scene.camera=ob
        scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast';scene.render.filepath=str(base.SOURCE/('cinematic_'+species+'_impostor.png'))
        print('CPU BAKE',species,'two threads',flush=True);bpy.ops.render.render(write_still=True)
    for source,_,prefix,height in base.TREES:
        species='pine' if source.startswith('pine') else 'fir';folder=base.SOURCE/source;cloud=np.fromfile(folder/'canopy_positions.f32',dtype='<f4').reshape(-1,3)
        for lod in ['near','mid']:
            clear();bpy.ops.import_scene.gltf(filepath=str(folder/'selected_wood.gltf'))
            obj=next(o for o in bpy.context.scene.objects if o.type=='MESH');bpy.context.view_layer.objects.active=obj;obj.select_set(True)
            bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.separate(type='MATERIAL');bpy.ops.object.mode_set(mode='OBJECT');bpy.ops.object.select_all(action='DESELECT')
            for o in [o for o in bpy.context.scene.objects if o.type=='MESH']:
                mat=o.data.materials[o.data.polygons[0].material_index].name
                isbranch='bark' in mat;istrunk='trunk' in mat
                budget=(5000 if isbranch else (2100 if istrunk else 300)) if lod=='near' else (600 if isbranch else (650 if istrunk else 100))
                reduce(o,budget,45 if isbranch and lod=='mid' else None)
            count=add_shoots(cloud,folder,height,lod,species);bpy.context.view_layer.update();lo,hi=bounds();scale=height/(hi.z-lo.z)
            for o in bpy.context.scene.objects:
                if o.type=='MESH':o.location.z-=lo.z;o.location*=scale;o.scale*=scale
            bpy.context.view_layer.update();entry=export(prefix+'_'+lod,source,{'lod':lod,'canopy_method':'small source-aware individually rolled shoots, curved subset near',
                'shoot_clusters':count,'target_shoot_length_m':(.62 if species=='pine' else .50)*(1. if lod=='near' else 1.55)})
            assert entry['triangles'] <= (20000 if lod=='near' else 4000),entry
            if lod=='near':render(species,height)
        clear();width=height*1.06*2/3;card_height=height*1.06;bottom=(height-card_height)*.5
        vertices=[(-width*.5,0,bottom),(width*.5,0,bottom),(width*.5,0,bottom+card_height),(-width*.5,0,bottom+card_height)]
        mesh=bpy.data.meshes.new('Upright fine canopy silhouette');mesh.from_pydata(vertices,[],[(0,1,2),(0,2,3)]);mesh.update();obj=bpy.data.objects.new(prefix+'_far',mesh);bpy.context.collection.objects.link(obj)
        layer=mesh.uv_layers.new(name='UVMap');tex=[(0,0),(1,0),(1,1),(0,1)]
        for poly in mesh.polygons:
            for li in poly.loop_indices:layer.data[li].uv=tex[mesh.loops[li].vertex_index]
        m=bpy.data.materials.new('Fine canopy camera-facing silhouette');m.use_nodes=True;m.blend_method='CLIP';m.alpha_threshold=.4
        shader=m.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=.95
        n=m.node_tree.nodes.new('ShaderNodeTexImage');n.image=bpy.data.images.load(str(base.SOURCE/('cinematic_'+species+'_impostor.png')));m.node_tree.links.new(n.outputs['Color'],shader.inputs['Base Color']);m.node_tree.links.new(n.outputs['Alpha'],shader.inputs['Alpha']);obj.data.materials.append(m)
        export(prefix+'_far',source,{'lod':'far','height_m':height,'root_padding_m':-bottom,'canopy_method':'single upright CPU-baked card from finer near shoots'})
        del cloud
    base.provenance()

if __name__=='__main__':
    if '--backup' in sys.argv:backup()
    else:cook()

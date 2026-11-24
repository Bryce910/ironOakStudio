"""Photographed CC0 3D grass tufts, without rendering or generated imagery.
python tools/build_cinematic_grass.py --download
blender --factory-startup --background --python tools/build_cinematic_grass.py
python tools/build_cinematic_assets.py --validate
"""
from pathlib import Path
import json,sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_cinematic_assets as base

# These are actual individual source clumps. No spherical demo or scattered
# presentation mesh is used. Selected by original source object name.
TUFTS=[('grass_medium_01','grass_medium_01_small_a_LOD0',.26,420),
       ('grass_medium_01','grass_medium_01_large_b_LOD0',.32,500),
       ('grass_medium_02','grass_medium_02_c',.35,450)]

def download():
    from PIL import Image
    for asset in sorted({a for a,_,_,_ in TUFTS}):
        folder=base.SOURCE/asset;folder.mkdir(exist_ok=True)
        base.fetch('https://api.polyhaven.com/files/'+asset,folder/'metadata.json')
        meta=json.loads((folder/'metadata.json').read_text());record=meta['gltf']['2k']['gltf']
        base.fetch(record['url'],folder/(asset+'.gltf'),record)
        tasks=[(r['url'],folder/name,r) for name,r in record['include'].items()]
        alpha=meta['Alpha']['2k']['png'];tasks.append((alpha['url'],folder/'source_alpha.png',alpha))
        with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(lambda args:base.fetch(*args),tasks))
        image=Image.open(folder/('textures/'+asset+'_diff_2k.jpg')).convert('RGBA')
        image.putalpha(Image.open(folder/'source_alpha.png').convert('L'))
        image.save(folder/'textures/grass_base_rgba.png',optimize=True)
        gltf=json.loads((folder/(asset+'.gltf')).read_text())
        for image in gltf['images']:
            if '_diff_' in image.get('uri',''):
                image['uri']='textures/grass_base_rgba.png';image['mimeType']='image/png'
        for material in gltf['materials']:
            material['alphaMode']='MASK';material['alphaCutoff']=.35;material['doubleSided']=True
        (folder/'grass_alpha.gltf').write_text(json.dumps(gltf,indent=2))
        print('Prepared',asset,flush=True)
    base.provenance()

def cook():
    import bpy
    from mathutils import Vector
    from bake_gltf_uv import bake_uv_transforms
    manifest_path=base.OUT/'cinematic_manifest.json'
    manifest=json.loads(manifest_path.read_text()) if manifest_path.exists() else []
    for index,(asset,source_name,height,budget) in enumerate(TUFTS):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=str(base.SOURCE/asset/'grass_alpha.gltf'))
        objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
        chosen=next(o for o in objects if o.name==source_name)
        for o in objects:
            if o!=chosen:bpy.data.objects.remove(o,do_unlink=True)
        bpy.context.view_layer.objects.active=chosen;chosen.select_set(True)
        # glTF UV/normal seam splits must not impose a floor on simplification.
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.remove_doubles(threshold=.000001);bpy.ops.object.mode_set(mode='OBJECT')
        source_triangles=sum(len(p.vertices)-2 for p in chosen.data.polygons)
        if source_triangles>budget:
            modifier=chosen.modifiers.new('Bounded photographed tuft','DECIMATE');modifier.ratio=budget/source_triangles
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        count=sum(len(p.vertices)-2 for p in chosen.data.polygons)
        assert count<=500,(source_name,count)
        points=[chosen.matrix_world@Vector(c) for c in chosen.bound_box]
        low=Vector([min(p[i] for p in points) for i in range(3)])
        high=Vector([max(p[i] for p in points) for i in range(3)])
        scale=height/(high.z-low.z)
        chosen.location-=Vector(((low.x+high.x)*.5,(low.y+high.y)*.5,low.z))
        chosen.location*=scale;chosen.scale*=scale
        for mat in chosen.data.materials:
            mat.use_backface_culling=False;mat.blend_method='CLIP';mat.alpha_threshold=.35
            for node in mat.node_tree.nodes:
                if node.type=='TEX_IMAGE' and node.image:node.image.pack()
        name='cinematic_grass_'+str(index);chosen.name=name
        path=base.OUT/(name+'.glb')
        bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',export_yup=True,export_animations=False,
                                  export_cameras=False,export_lights=False,export_image_format='AUTO')
        bake_uv_transforms(path)
        points=[chosen.matrix_world@Vector(c) for c in chosen.bound_box]
        bounds=[[min(p[i] for p in points),max(p[i] for p in points)] for i in range(3)]
        entry={'asset':name,'source':asset,'source_object':source_name,'source_triangles':source_triangles,
               'triangles':count,'height_m':height,'bounds_xyz_z_up':bounds,'bytes':path.stat().st_size,
               'sha256':base.sha(path),'license':'CC0-1.0','method':'individual source 3D tuft with photographed alpha/PBR'}
        manifest[:]=[r for r in manifest if r['asset']!=name];manifest.append(entry)
        manifest_path.write_text(json.dumps(manifest,indent=2));print('COOKED',name,count,bounds,flush=True)
    base.provenance()

if __name__=='__main__':
    if '--download' in sys.argv:download()
    else:cook()

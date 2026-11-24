"""Cook CC0 Poly Haven glTF sources into bounded runtime assets.
Run Blender with --factory-startup --background --python tools/build_forest_assets.py.
See assets/polyhaven/models/manifest.json for source and hash records.
"""
import bpy, math, json, sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
SOURCES=ROOT/'assets/polyhaven/models'
OUT=ROOT/'projects/seldonne';cooked=[]
sys.path.insert(0,str(ROOT/'tools'))
from bake_gltf_uv import bake_uv_transforms
def clear():bpy.ops.wm.read_factory_settings(use_empty=True)
def export_one(source,index,name,height,triangle_budget):
 clear();bpy.ops.import_scene.gltf(filepath=str(SOURCES/source/(source+'.gltf')))
 objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
 objects.sort(key=lambda o:o.name)
 chosen=objects[index]
 for o in objects:
  if o!=chosen:bpy.data.objects.remove(o,do_unlink=True)
 bpy.context.view_layer.objects.active=chosen;chosen.select_set(True)
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 tris=sum(len(p.vertices)-2 for p in chosen.data.polygons)
 if tris>triangle_budget:
  mod=chosen.modifiers.new('Runtime geometric LOD','DECIMATE');mod.ratio=triangle_budget/tris
  bpy.ops.object.modifier_apply(modifier=mod.name)
 # Material remains physically based, with source normal and roughness maps.
 for mat in chosen.data.materials:
  if not mat:continue
  mat.use_backface_culling=False
  for node in mat.node_tree.nodes:
   if node.type=='TEX_IMAGE' and node.image:node.image.pack()
 pts=[chosen.matrix_world@Vector(c) for c in chosen.bound_box]
 lo=Vector([min(p[i] for p in pts) for i in range(3)]);hi=Vector([max(p[i] for p in pts) for i in range(3)])
 factor=height/(hi.z-lo.z)
 root=bpy.data.objects.new(name,None);bpy.context.collection.objects.link(root)
 chosen.parent=root;root.scale=(factor,)*3;root.location=(-factor*(lo.x+hi.x)/2,-factor*(lo.y+hi.y)/2,-factor*lo.z)
 bpy.ops.export_scene.gltf(filepath=str(OUT/'models'/(name+'.glb')),export_format='GLB',export_yup=True,export_animations=False)
 bake_uv_transforms(OUT/'models'/(name+'.glb'))
 cooked.append({'asset':name,'source':source,'source_triangles':tris,'runtime_triangles':sum(len(p.vertices)-2 for p in chosen.data.polygons),'height_m':height,'license':'CC0'})
 return chosen
fir=export_one('fir_sapling_medium',0,'silver_fir',15,65000)
# Render a silhouette-matched distant card from the cooked geometry.
scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE';scene.eevee.taa_render_samples=64;scene.eevee.use_gtao=True
scene.render.resolution_x=1024;scene.render.resolution_y=1024;scene.render.resolution_percentage=100;scene.render.film_transparent=True
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.world=bpy.data.worlds.new('Sky fill');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.30,.35,.40,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.8
sun_data=bpy.data.lights.new('Sun','SUN');sun_data.energy=2;sun_data.angle=.09;sun=bpy.data.objects.new('Sun',sun_data);bpy.context.collection.objects.link(sun);sun.rotation_euler=(.6,-.3,-.5)
# An area fill keeps back-facing branches legible in the distant LOD.
fill_data=bpy.data.lights.new('Sky','AREA');fill_data.energy=1200;fill_data.size=16;fill=bpy.data.objects.new('Sky',fill_data);bpy.context.collection.objects.link(fill);fill.location=(-8,-12,14);fill.rotation_euler=(Vector((0,0,8))-fill.location).to_track_quat('-Z','Y').to_euler()
cam_data=bpy.data.cameras.new('Impostor');cam=bpy.data.objects.new('Impostor',cam_data);bpy.context.collection.objects.link(cam)
cam.location=(20,-40,8);cam.rotation_euler=(Vector((0,0,7.5))-cam.location).to_track_quat('-Z','Y').to_euler();cam_data.type='ORTHO';cam_data.ortho_scale=16.0;scene.camera=cam
scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast';scene.render.filepath=str(OUT/'fir_billboard.png');bpy.ops.render.render(write_still=True)
for i in range(3):export_one('fern_02',i,'woodland_fern_'+str(i),.65+(.12*i),4500)
for i in range(3):export_one('rock_moss_set_02',i,'mossy_boulder_'+str(i),1.1+(.4*i),7500)
(OUT/'cooked_forest_manifest.json').write_text(json.dumps(cooked,indent=2))
print('Cooked forest assets:',cooked)

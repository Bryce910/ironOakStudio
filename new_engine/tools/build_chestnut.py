"""Blender asset pipeline: normalize the supplied chestnut and embed PBR maps.
Run: blender -b --factory-startup --python tools/build_chestnut.py
"""
import bpy
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'projects/seldonne'
SOURCE=ROOT/'models/RealisticLowPoly_Chesnut'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(SOURCE/'Chestnut_Good.fbx'))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
for mat in bpy.data.materials:
    leaf='leaf' in mat.name.lower()
    mat.use_nodes=True; nodes=mat.node_tree.nodes; nodes.clear(); links=mat.node_tree.links
    bsdf=nodes.new('ShaderNodeBsdfPrincipled'); out=nodes.new('ShaderNodeOutputMaterial'); links.new(bsdf.outputs['BSDF'],out.inputs['Surface'])
    bsdf.inputs['Roughness'].default_value=.82
    color=nodes.new('ShaderNodeTexImage'); normal=nodes.new('ShaderNodeTexImage'); normal_map=nodes.new('ShaderNodeNormalMap')
    color.image=bpy.data.images.load(str(SOURCE/('TexturesCom_Branches0018_1_alphamasked_S.png' if leaf else 'tileable_tree_bark_texture_by_ftourini-d3l69hz.jpg')))
    normal.image=bpy.data.images.load(str(SOURCE/('TexturesCom_Branches0018_1_alphamasked_Snor.png' if leaf else 'tileable_tree_bark_texture_by_ftourini-d3l69hznor.jpg')))
    normal.image.colorspace_settings.name='Non-Color'; normal_map.inputs['Strength'].default_value=.55
    links.new(color.outputs['Color'],bsdf.inputs['Base Color']); links.new(normal.outputs['Color'],normal_map.inputs['Color']); links.new(normal_map.outputs['Normal'],bsdf.inputs['Normal'])
    if leaf:
        links.new(color.outputs['Alpha'],bsdf.inputs['Alpha']); mat.blend_method='CLIP'; mat.alpha_threshold=.45; mat.shadow_method='CLIP'
    mat.use_backface_culling=False
pts=[o.matrix_world@Vector(c) for o in meshes for c in o.bound_box]
lo=Vector([min(p[i] for p in pts) for i in range(3)]); hi=Vector([max(p[i] for p in pts) for i in range(3)])
root=bpy.data.objects.new('Valwood chestnut root',None);bpy.context.collection.objects.link(root)
for o in list(bpy.context.scene.objects):
    if o!=root and o.parent is None: o.parent=root
scale=15/(hi.z-lo.z);root.scale=(scale,scale,scale);root.location=(-scale*(lo.x+hi.x)/2,-scale*(lo.y+hi.y)/2,-scale*lo.z)
(OUT/'models').mkdir(parents=True,exist_ok=True)
bpy.ops.export_scene.gltf(filepath=str(OUT/'models/chestnut.glb'),export_format='GLB',export_yup=True,export_animations=False)
# Render a matching distant impostor, with a transparent background.
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=16;scene.cycles.use_denoising=False
scene.render.resolution_x=1024;scene.render.resolution_y=1024;scene.render.resolution_percentage=100;scene.render.film_transparent=True
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.world=bpy.data.worlds.new('Overcast ambient');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.30,.35,.40,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.6
sun_data=bpy.data.lights.new('Sun','SUN');sun_data.energy=2;sun_data.angle=.15
sun=bpy.data.objects.new('Sun',sun_data);bpy.context.collection.objects.link(sun);sun.rotation_euler=(.5,-.4,-.6)
camera_data=bpy.data.cameras.new('Impostor');camera=bpy.data.objects.new('Impostor',camera_data);bpy.context.collection.objects.link(camera)
camera.location=(22,-40,10);camera.rotation_euler=(Vector((0,0,7.4))-camera.location).to_track_quat('-Z','Y').to_euler();camera_data.type='ORTHO';camera_data.ortho_scale=16.2;scene.camera=camera
scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast';scene.view_settings.exposure=0;scene.view_settings.gamma=1
scene.render.filepath=str(OUT/'chestnut_billboard.png');bpy.ops.render.render(write_still=True)
print('Valwood chestnut and matching impostor ready')

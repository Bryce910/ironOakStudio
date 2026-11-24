"""Bake actual owned tree silhouettes, CPU only. No live project writes."""
import bpy, math, json, sys, os
from pathlib import Path
from mathutils import Vector
N=Path(__file__).resolve().parents[1];P=N/'world-project';O=N/'review/art/cards';O.mkdir(parents=True,exist_ok=True)
models=[('oak02','camp_variants/camp_variant_oak02_near.glb'),('oak04','camp_variants/camp_variant_oak04_near.glb'),('beech01','camp_variants/camp_variant_beech01_near.glb'),('beech02','camp_variants/camp_variant_beech02_near.glb')]
records=[]
for name,rel in models:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.gltf(filepath=str(P/'models'/rel))
 # Capture source diffuse colour and opacity without baking a second copy of
 # the directional key; native far cards receive live weather/sky relighting.
 for mat in bpy.data.materials:
  if not mat.use_nodes:continue
  nodes=mat.node_tree.nodes;links=mat.node_tree.links;bs=next((n for n in nodes if n.type=='BSDF_PRINCIPLED'),None);out=next((n for n in nodes if n.type=='OUTPUT_MATERIAL'),None)
  if bs is None or out is None:continue
  em=nodes.new('ShaderNodeEmission');em.inputs['Strength'].default_value=1
  if bs.inputs['Base Color'].links:links.new(bs.inputs['Base Color'].links[0].from_socket,em.inputs['Color'])
  else:em.inputs['Color'].default_value=bs.inputs['Base Color'].default_value
  tr=nodes.new('ShaderNodeBsdfTransparent');mix=nodes.new('ShaderNodeMixShader');links.new(tr.outputs[0],mix.inputs[1]);links.new(em.outputs[0],mix.inputs[2])
  if bs.inputs['Alpha'].links:links.new(bs.inputs['Alpha'].links[0].from_socket,mix.inputs[0])
  else:mix.inputs[0].default_value=bs.inputs['Alpha'].default_value
  links.new(mix.outputs[0],out.inputs['Surface'])
 obs=[o for o in bpy.context.scene.objects if o.type=='MESH'];pts=[o.matrix_world@Vector(v.co)for o in obs for v in o.data.vertices]
 lo=Vector([min(p[i]for p in pts)for i in range(3)]);hi=Vector([max(p[i]for p in pts)for i in range(3)])
 # Blender is Z-up. Gltf source stays untouched; camera uses true world bounds.
 center=(lo+hi)*.5; height=hi.z-lo.z;width=max(hi.x-lo.x,hi.y-lo.y)
 scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=12;scene.cycles.use_denoising=False
 scene.render.resolution_x=384;scene.render.resolution_y=512;scene.render.resolution_percentage=100;scene.render.film_transparent=True;scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
 scene.world=bpy.data.worlds.new('Neutral woodland fill');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.34,.39,.43,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.75
 ld=bpy.data.lights.new('Soft tree bake key','SUN');ld.energy=1.2;ld.angle=.5;light=bpy.data.objects.new('Soft tree bake key',ld);scene.collection.objects.link(light);light.rotation_euler=(.6,-.3,-.7)
 cd=bpy.data.cameras.new('Tree card orthographic');cam=bpy.data.objects.new('Tree card orthographic',cd);scene.collection.objects.link(cam);cd.type='ORTHO';cd.ortho_scale=max(height,width*512/384)*1.06;scene.camera=cam
 scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast';scene.view_settings.exposure=0
 for j in range(2):
  angle=j*math.pi/2;cam.location=center+Vector((math.sin(angle)*60,-math.cos(angle)*60,0));cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(O/f'{name}-{j}.png');bpy.ops.render.render(write_still=True)
 records.append(dict(name=name,source=str(P/'models'/rel),minimum=list(lo),maximum=list(hi),ortho_height=cd.ortho_scale,ortho_width=cd.ortho_scale*384/512,views=2))
(O/'BAKE.json').write_text(json.dumps(records,indent=2)+'\n')

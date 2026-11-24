import bpy,sys,json,math
from pathlib import Path
from mathutils import Vector
BASE=Path(__file__).resolve().parents[1];OUT=BASE/'review/story-hero';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(BASE/'assets/story_hero/civilian-preview-animated.glb'))
rig=[o for o in bpy.data.objects if o.type=='ARMATURE'][0]
actions=[a.name for a in bpy.data.actions]
idle=next((a for a in bpy.data.actions if a.name=='Equipment_Relaxed_Armature' or a.name.startswith('Equipment_Relaxed.')),None)
if idle:
 if not rig.animation_data:rig.animation_data_create()
 for track in list(rig.animation_data.nla_tracks):rig.animation_data.nla_tracks.remove(track)
 rig.animation_data.action=idle
bpy.context.scene.frame_set(15)
# Neutral equipment inspection, same camera and exposure across all views.
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=False;scene.render.threads_mode='FIXED';scene.render.threads=4
scene.render.resolution_x=640;scene.render.resolution_y=800;scene.render.resolution_percentage=100
scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast';scene.view_settings.exposure=0;scene.view_settings.gamma=1
scene.world=bpy.data.worlds.new('Neutral world');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.12,.13,.15,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.5
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.005));floor=bpy.context.object
mat=bpy.data.materials.new('Neutral floor');mat.diffuse_color=(.13,.14,.16,1);floor.data.materials.append(mat)
def area(name,pos,power,size):
 data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;o=bpy.data.objects.new(name,data);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
area('Key',(3,-4,5),420,4);area('Fill',(-3,-2,3),210,4);area('Rim',(0,3,4),330,3)
camdata=bpy.data.cameras.new('Camera');cam=bpy.data.objects.new('Camera',camdata);scene.collection.objects.link(cam);scene.camera=cam;camdata.type='ORTHO';camdata.ortho_scale=2.08
for name,loc in [('front',(0,-4,1.0)),('side',(4,0,1.0)),('back',(0,4,1.0))]:
 cam.location=loc;cam.rotation_euler=(Vector((0,0,.92))-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
(OUT/'BLENDER_DIAGNOSTIC.json').write_text(json.dumps({'source':'candidateassets/story_hero/civilian-preview-animated.glb','actions':actions,'selected_idle':idle.name if idle else None,'frame':15,'renderer':'BlenderCyclesCPU24samples','purpose':'Neutralassetdiagnosticnotshippinggamecapture'},indent=2))

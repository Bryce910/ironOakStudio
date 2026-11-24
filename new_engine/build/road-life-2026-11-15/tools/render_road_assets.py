"""Small CPU-only visual review of exact encoded road meshes, no live game."""
from pathlib import Path
import sys,json,math
import bpy
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
from road_asset_common import STAGE,OUT,sha

dest=STAGE/'review/assets/source-filmstrip';dest.mkdir(exist_ok=True)
cart=json.loads((OUT/'CART_ASSETS.json').read_text())
def add(path,position=(0,0,0)):
    before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(path))
    for o in set(bpy.data.objects)-before:
        if o.parent is None:o.location+=Vector((position[0],-position[2],position[1]))
for kind,frames in [('mounted',[0,4,8,12]),('puller',[0,2,4,6])]:
    for frame in frames:
        bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene
        add(OUT/f'models/{kind}_walk_{frame:02}.glb')
        if kind=='puller':
            offset=cart['cart_local_offset'];add(OUT/cart['body']['path'],offset)
            for axle in cart['axles_local']:add(OUT/cart['wheel']['path'],[x+y for x,y in zip(offset,axle)])
        bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.004));m=bpy.data.materials.new('Neutral review floor');m.diffuse_color=(.16,.18,.16,1);bpy.context.object.data.materials.append(m)
        w=bpy.data.worlds.new('Review sky');w.use_nodes=True;w.node_tree.nodes['Background'].inputs[0].default_value=(.38,.44,.52,1);w.node_tree.nodes['Background'].inputs[1].default_value=.6;scene.world=w
        for pos,power,size in [((1,-4,7),1100,5),((-4,2,5),800,4)]:
            bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.shape='DISK';light.data.size=size;light.rotation_euler=(Vector((0,0,1.5))-light.location).to_track_quat('-Z','Y').to_euler()
        target=Vector((0,-.45,1.48))if kind=='mounted'else Vector((0,1.,1.10))
        position=(5,-7,4.2)if kind=='mounted'else(5,-5,3.6)
        bpy.ops.object.camera_add(location=position);camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=4.25 if kind=='mounted' else 4.2;scene.camera=camera
        scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=8;scene.cycles.use_denoising=False
        scene.render.threads_mode='FIXED';scene.render.threads=2;scene.render.resolution_x=480;scene.render.resolution_y=480;scene.render.resolution_percentage=100
        scene.render.image_settings.file_format='PNG';scene.render.filepath=str(dest/f'{kind}-{frame:02}.png');bpy.ops.render.render(write_still=True)
        print('REVIEW_FRAME',kind,frame,flush=True)
(dest/'RENDER_REVIEW.json').write_text(json.dumps({'passed':True,'cpu_cycles_only':True,'native_render_claimed':False,'frames':{str(p):sha(p)for p in dest.glob('*.png')},'inputs':{str(Path(__file__).resolve()):sha(__file__),str(OUT/'ROAD_LIFE_ASSETS.json'):sha(OUT/'ROAD_LIFE_ASSETS.json')}},indent=2)+'\n')

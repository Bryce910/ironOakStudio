import bpy,sys,pathlib,json,hashlib,math
from mathutils import Vector
HERE=pathlib.Path(__file__).resolve().parent;STAGE=HERE/"arena-project"
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['knight09'];ids=[f'knight{i:02}' for i in range(1,19)] if args==['all'] else args
rows=[]
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def aim(obj,at):obj.rotation_euler=(Vector(at)-obj.location).to_track_quat('-Z','Y').to_euler()
for cid in ids:
 bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE';scene.eevee.taa_render_samples=128;scene.eevee.use_gtao=True;scene.eevee.gtao_distance=.12;scene.eevee.gtao_factor=1.0;scene.eevee.use_soft_shadows=True;scene.cycles.device='CPU';scene.cycles.samples=32;scene.cycles.use_denoising=False;scene.render.threads_mode='FIXED';scene.render.threads=2;scene.render.resolution_x=512;scene.render.resolution_y=640;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
 scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.055,.075,.105,1);scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.5
 source=STAGE/'knights_roster'/cid/'body.glb';bpy.ops.import_scene.gltf(filepath=str(source));bpy.context.view_layer.update();deps=bpy.context.evaluated_depsgraph_get();pts=[]
 for o in scene.objects:
  if o.type=='MESH':
   ev=o.evaluated_get(deps);pts.extend(ev.matrix_world@Vector(v) for v in ev.bound_box)
 low=[min(v[i] for v in pts) for i in range(3)];high=[max(v[i] for v in pts) for i in range(3)];assert high[2]-low[2]>.5,(cid,low,high)
 # A roughness floor matches the in-game actor finish without replacing maps.
 for mat in bpy.data.materials:
  if not mat.use_nodes:continue
  for node in list(mat.node_tree.nodes):
   if node.type!='BSDF_PRINCIPLED':continue
   socket=node.inputs['Roughness']
   if socket.is_linked:
    roughness_source=socket.links[0].from_socket;limit=mat.node_tree.nodes.new('ShaderNodeMath');limit.operation='MAXIMUM';limit.inputs[1].default_value=.30;mat.node_tree.links.new(roughness_source,limit.inputs[0]);mat.node_tree.links.new(limit.outputs[0],socket)
   else:socket.default_value=max(socket.default_value,.30)
 # Actual upperbody framing includes the original helmet/feather while maintaining4:5aspect.
 top=high[2];bottom=max(low[2],top-1.45);height=(top-bottom)*1.12;at=(0,0,(top+bottom)/2)
 cam=bpy.data.cameras.new('Portrait');co=bpy.data.objects.new('Portrait',cam);scene.collection.objects.link(co);co.location=(.65,-4.,at[2]+.04);aim(co,at);cam.type='ORTHO';cam.ortho_scale=height;cam.lens=70;scene.camera=co
 for name,loc,power,size,color in [('Key',(-3,-4,4),650,4,(1,.94,.85)),('Fill',(3,-2,2.4),300,4,(.80,.88,1)),('Rim',(1,2,3.4),500,3,(.85,.92,1))]:
  data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;data.color=color;ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=loc;aim(ob,at)
 scene.view_settings.view_transform='AgX';scene.render.filepath=str(STAGE/'knights_roster/portraits'/f'{cid}.png');pathlib.Path(scene.render.filepath).parent.mkdir(parents=True,exist_ok=True);bpy.ops.render.render(write_still=True)
 row={'id':cid,'source':str(source),'source_sha256':sha(source),'portrait':scene.render.filepath,'portrait_sha256':sha(scene.render.filepath),'world_bounds_min':low,'world_bounds_max':high,'camera_height':height,'actual_original_asset_render':True};rows.append(row);print('PORTRAIT_COMPLETE',cid,row['portrait_sha256'],flush=True)
 # Writeeachreceiptatomicallyforboundedresume.
 out=STAGE/'knights_roster/portraits'/f'{cid}-render.json';out.write_text(json.dumps({'schema':'valwood.original-knight.portrait.v1','passed':True,'producer':str(__file__),'producer_sha256':sha(__file__),'render':row},indent=2)+'\n')

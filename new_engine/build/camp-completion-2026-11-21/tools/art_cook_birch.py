"""Owned birch near/MID meshes and actual far silhouettes. CPU only."""
import bpy,json,math,sys
from pathlib import Path
from mathutils import Vector
N=Path(__file__).resolve().parents[1];R=N/'review/art/birch';A=N/'world-project/models/art_birch';A.mkdir(exist_ok=True);records=[]
for name,target_height in [('Birch1',10.),('Birch3',16.)]:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(R/'source'/(name+'.fbx')))
 obs=[o for o in bpy.context.scene.objects if o.type=='MESH'];pts=[o.matrix_world@Vector(v.co)for o in obs for v in o.data.vertices];minz=min(p.z for p in pts);maxz=max(p.z for p in pts);factor=target_height/(maxz-minz)
 for o in obs:
  bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
  for v in o.data.vertices:v.co*=factor;v.co.z-=minz*factor
 # Original source ground plane z0 is retained in metadata after minY normalization.
 soil=-minz*factor
 for mat in bpy.data.materials:
  mat.use_nodes=True;nodes=mat.node_tree.nodes;nodes.clear();out=nodes.new('ShaderNodeOutputMaterial');bs=nodes.new('ShaderNodeBsdfPrincipled');bs.inputs['Metallic'].default_value=0;bs.inputs['Roughness'].default_value=.9;bs.inputs['Specular IOR Level'].default_value=.32;mat.node_tree.links.new(bs.outputs[0],out.inputs['Surface']);branch='Branch'in mat.name
  image=nodes.new('ShaderNodeTexImage');image.image=bpy.data.images.load(str(R/'maps'/('branch_color.png'if branch else'bark_color.png')),check_existing=True);image.image.colorspace_settings.name='sRGB';mat.node_tree.links.new(image.outputs['Color'],bs.inputs['Base Color'])
  if branch:mat.node_tree.links.new(image.outputs['Alpha'],bs.inputs['Alpha']);mat.blend_method='CLIP';mat.alpha_threshold=.5;mat.use_backface_culling=False
  else:mat.use_backface_culling=True;mat.blend_method='OPAQUE'
  normal=nodes.new('ShaderNodeTexImage');normal.image=bpy.data.images.load(str(R/'maps'/('branch_normal_gl.png'if branch else'bark_normal_gl.png')),check_existing=True);normal.image.colorspace_settings.name='Non-Color';nm=nodes.new('ShaderNodeNormalMap');nm.inputs['Strength'].default_value=.8;mat.node_tree.links.new(normal.outputs['Color'],nm.inputs['Color']);mat.node_tree.links.new(nm.outputs[0],bs.inputs['Normal'])
  mat.name='Birch photographed branch cutouts'if branch else'Birch photographed white bark'
 near=A/(name.lower()+'_near.glb');bpy.ops.export_scene.gltf(filepath=str(near),export_format='GLB',export_yup=True,export_animations=False,export_tangents=True)
 neartri=sum(len(p.vertices)-2 for o in obs for p in o.data.polygons)
 # Capture actual near silhouette using source diffuse and opacity, avoiding
 # baked directional lighting so native far cards respond to live sky/weather.
 materials=[]
 for mat in bpy.data.materials:
  nodes=mat.node_tree.nodes;bs=next(n for n in nodes if n.type=='BSDF_PRINCIPLED');out=next(n for n in nodes if n.type=='OUTPUT_MATERIAL');links=mat.node_tree.links;old=out.inputs['Surface'].links[0].from_socket;materials.append((mat,old));em=nodes.new('ShaderNodeEmission');links.new(bs.inputs['Base Color'].links[0].from_socket,em.inputs['Color']);tr=nodes.new('ShaderNodeBsdfTransparent');mix=nodes.new('ShaderNodeMixShader');links.new(tr.outputs[0],mix.inputs[1]);links.new(em.outputs[0],mix.inputs[2]);alpha=bs.inputs['Alpha'];links.new(alpha.links[0].from_socket,mix.inputs[0])if alpha.links else setattr(mix.inputs[0],'default_value',alpha.default_value);links.new(mix.outputs[0],out.inputs['Surface'])
 scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16;scene.cycles.use_denoising=False;scene.render.resolution_x=384;scene.render.resolution_y=512;scene.render.resolution_percentage=100;scene.render.film_transparent=True;scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA';scene.view_settings.view_transform='Standard';scene.view_settings.look='None'
 pts=[o.matrix_world@Vector(v.co)for o in obs for v in o.data.vertices];lo=Vector([min(p[i]for p in pts)for i in range(3)]);hi=Vector([max(p[i]for p in pts)for i in range(3)]);center=(lo+hi)*.5;width=max(hi.x-lo.x,hi.y-lo.y);height=hi.z-lo.z;cd=bpy.data.cameras.new('Source birch card');cam=bpy.data.objects.new('Source birch card',cd);scene.collection.objects.link(cam);cam.location=center+Vector((0,-60,0));cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();cd.type='ORTHO';cd.ortho_scale=max(height,width*512/384)*1.05;scene.camera=cam;scene.render.filepath=str(R/(name.lower()+'_card.png'));bpy.ops.render.render(write_still=True)
 for mat,old in materials:
  out=next(n for n in mat.node_tree.nodes if n.type=='OUTPUT_MATERIAL');mat.node_tree.links.new(old,out.inputs['Surface'])
 for o in obs:
  bpy.context.view_layer.objects.active=o;modifier=o.modifiers.new('Measured MID geometric reduction','DECIMATE');modifier.ratio=.55;modifier.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=modifier.name)
 mid=A/(name.lower()+'_mid.glb');bpy.ops.export_scene.gltf(filepath=str(mid),export_format='GLB',export_yup=True,export_animations=False,export_tangents=True)
 records.append({'name':name.lower(),'source_name':name,'source_height_m':maxz-minz,'authored_full_height_m':target_height,'source_scale':factor,'soil_plane_glb_y':soil,'minimum':list(lo),'maximum':list(hi),'ortho_height':cd.ortho_scale,'ortho_width':cd.ortho_scale*384/512,'near_triangles':neartri,'mid_triangles':sum(len(p.vertices)-2 for o in obs for p in o.data.polygons)})
(R/'COOK.json').write_text(json.dumps(records,indent=2)+'\n')

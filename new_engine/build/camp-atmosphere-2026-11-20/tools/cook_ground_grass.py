"""Cook three actual photographed Poly Haven tufts into private near/MID pairs."""
from pathlib import Path
import sys,json,hashlib,math
C=Path(__file__).resolve().parents[1]; E=C.parents[1]
S=E/'assets/polyhaven/cinematic/grass_medium_01';O=C/'world-project/models/camp_atmosphere';R=C/'review/grass'
sys.path.insert(0,str(E/'tools'))
SPECS=[('fine','grass_medium_01_small_a_LOD0',420,140),('arching','grass_medium_01_large_b_LOD0',480,160),('wild','grass_medium_01_mid_b_LOD0',420,140)]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare():
 import numpy as np
 from PIL import Image
 from scipy.ndimage import distance_transform_edt
 out=R/'source';out.mkdir(exist_ok=True)
 d=json.loads((S/'grass_alpha.gltf').read_text());(out/'grass_medium_01.bin').symlink_to((S/'grass_medium_01.bin').resolve())
 for im in d['images']:
  source=S/im['uri'];image=Image.open(source)
  if 'rgba' in im['uri']:
   a=np.asarray(image.convert('RGBA'),np.float32)/255;raw_alpha=np.asarray(Image.open(S/'source_alpha.png'));assert raw_alpha.shape==a.shape[:2] and raw_alpha.max()>255;alpha=raw_alpha.astype(np.float32)/65535.;rgb=a[:,:,:3];rgb=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)
   def resize(x):return np.asarray(Image.fromarray(x.astype('float32'),'F').resize((1024,1024),Image.Resampling.LANCZOS))
   al=np.clip(resize(alpha),0,1);rgb=np.stack([resize(rgb[:,:,i]*alpha) for i in range(3)],2)/np.maximum(al[:,:,None],1e-6);rgb=np.clip(rgb,0,1)
   lo,hi=.5,2
   for _ in range(22):
    f=(lo+hi)*.5
    if np.mean(al*f>=.35)<np.mean(alpha>=.35):lo=f
    else:hi=f
   al=np.clip(al*((lo+hi)*.5),0,1);valid=al>.02;idx=distance_transform_edt(~valid,return_distances=False,return_indices=True);rgb[~valid]=rgb[idx[0][~valid],idx[1][~valid]];rgb=np.where(rgb<=.0031308,rgb*12.92,1.055*rgb**(1/2.4)-.055)
   image=Image.fromarray(np.uint8(np.clip(np.rint(np.dstack([rgb,al])*255),0,255)),'RGBA')
  elif '_nor_' in im['uri']:
   a=np.asarray(image.convert('RGB').resize((1024,1024),Image.Resampling.LANCZOS),np.float32)/127.5-1;a/=np.maximum(np.linalg.norm(a,axis=2,keepdims=True),1e-6);image=Image.fromarray(np.uint8(np.clip((a+1)*127.5,0,255)),'RGB')
  else:image=image.convert('RGB').resize((1024,1024),Image.Resampling.LANCZOS)
  name=Path(im['uri']).stem+'.png';image.save(out/name);im['uri']=name;im['mimeType']='image/png'
 (out/'grass.gltf').write_text(json.dumps(d))
 (R/'SOURCE.json').write_text(json.dumps({'license':'CC0-1.0','source':'https://polyhaven.com/a/grass_medium_01','source_pins':{str(p):sha(p)for p in sorted(S.rglob('*'))if p.is_file()},'variants':SPECS,'maps':'Matching source color, OpenGL normal and ARM; source UVs preserved;1024²; alpha coverage kept at cutoff.35'},indent=2)+'\n')
def cook():
 import bpy
 from mathutils import Vector
 from bake_gltf_uv import bake_uv_transforms
 records=[]
 for name,objname,near,mid in SPECS:
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(R/'source/grass.gltf'))
  chosen=next(o for o in bpy.context.scene.objects if o.type=='MESH' and o.name==objname)
  for o in list(bpy.context.scene.objects):
   if o!=chosen:bpy.data.objects.remove(o,do_unlink=True)
  bpy.context.view_layer.objects.active=chosen;chosen.select_set(True)
  bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.remove_doubles(threshold=.000001);bpy.ops.object.mode_set(mode='OBJECT')
  points=[chosen.matrix_world@Vector(v.co) for v in chosen.data.vertices];low=Vector([min(p[i]for p in points)for i in range(3)]);high=Vector([max(p[i]for p in points)for i in range(3)]);scale=.8/(high.z-low.z)
  chosen.location-=Vector(((low.x+high.x)*.5,(low.y+high.y)*.5,low.z));chosen.location*=scale;chosen.scale*=scale
  bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
  points=[chosen.data.vertices[i].co.copy() for i in {j for p in chosen.data.polygons for j in p.vertices}];targetlo=Vector([min(p[i]for p in points)for i in range(3)]);targethi=Vector([max(p[i]for p in points)for i in range(3)])
  for mat in chosen.data.materials:
   mat.use_backface_culling=False;mat.blend_method='CLIP';mat.alpha_threshold=.35
   for node in mat.node_tree.nodes:
    if node.type=='TEX_IMAGE' and node.image:node.image.pack()
  for lod,budget in [('',near),('_mid',mid)]:
   count=sum(len(p.vertices)-2 for p in chosen.data.polygons)
   if count>budget:
    m=chosen.modifiers.new('Source tuft bounded LOD','DECIMATE');m.ratio=(budget-2)/count;bpy.ops.object.modifier_apply(modifier=m.name)
   count=sum(len(p.vertices)-2 for p in chosen.data.polygons);assert count<=budget
   points=[chosen.data.vertices[i].co.copy() for i in {j for p in chosen.data.polygons for j in p.vertices}];lo=Vector([min(p[i]for p in points)for i in range(3)]);hi=Vector([max(p[i]for p in points)for i in range(3)])
   for v in chosen.data.vertices:
    for axis in range(3):v.co[axis]=targetlo[axis]+(v.co[axis]-lo[axis])*(targethi[axis]-targetlo[axis])/(hi[axis]-lo[axis])
   p=O/('grass_'+name+lod+'.glb');bpy.ops.export_scene.gltf(filepath=str(p),export_format='GLB',export_yup=True,export_animations=False,export_cameras=False,export_lights=False,export_image_format='AUTO');bake_uv_transforms(p)
   records.append({'name':name,'source_object':objname,'LOD':lod or 'near','triangles':count,'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size})
 (R/'COOK.json').write_text(json.dumps(records,indent=2)+'\n')
if __name__=='__main__':
 if '--prepare'in sys.argv:prepare()
 else:cook()

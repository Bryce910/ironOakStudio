from pathlib import Path
import bpy,bmesh,numpy as np,json
P=Path(__file__).resolve().parent;bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(P/'intake_roster/ascii-converted/Orc_04_Weapon_Mesh.FBX'),use_anim=False,use_image_search=False)
for o in bpy.data.objects:
 if o.type!='MESH':continue
 bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='BEAUTY',ngon_method='BEAUTY');bm.to_mesh(o.data);bm.free();o.data.update();o.data.calc_normals_split();v=np.asarray([list(o.data.vertices[l.vertex_index].co)for l in o.data.loops]);n=np.asarray([list(x.vector)for x in o.data.corner_normals]);u=np.asarray([list(x.uv)for x in o.data.uv_layers.active.data]);print(o.name,'points',len(v),flush=True)
 for precision in [9,8,7,6,5,4]:print(precision,len(np.unique(np.concatenate([np.round(v,precision),np.round(n,precision),np.round(u,precision)],axis=1),axis=0)),flush=True)
 np.savez_compressed(P/(o.name+'-corner-probe.npz'),v=v,n=n,u=u)

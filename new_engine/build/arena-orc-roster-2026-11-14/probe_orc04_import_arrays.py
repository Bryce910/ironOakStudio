from pathlib import Path
import bpy,numpy as np
P=Path(__file__).resolve().parent;bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(P/'intake_roster/ascii-converted/Orc_04_Weapon_Mesh.FBX'),use_anim=False,use_image_search=False)
for o in bpy.data.objects:
 if o.type!='MESH':continue
 o.data.calc_normals_split();np.savez_compressed(P/(o.name+'-import-probe.npz'),vertices=np.asarray([list(o.matrix_world@v.co)for v in o.data.vertices]),loop=np.asarray([l.vertex_index for l in o.data.loops]),normals=np.asarray([list(n.vector)for n in o.data.corner_normals]),uv=np.asarray([list(u.uv)for u in o.data.uv_layers.active.data]))

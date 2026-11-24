"""Append exact owned skin weights to the completed Orc anatomical exchange.

The body bake is left unchanged. UVs, indices, source seam IDs, bone ordering
and vertex counts must match before hierarchical wound masks may be built.
"""
from pathlib import Path
import hashlib,json
import bpy,bmesh,numpy as np

STAGE=Path(__file__).resolve().parent
SOURCE=STAGE/'orc-intake/powerorc/Base mesh/Base mesh.fbx'
EXCHANGE=STAGE/'arena_orcs/body-semantic.npz'
NAMES=['Power_Orc3_1','Arm_Armors','Belt','Helm','Left_Shoulder_Armor','Legs_Armor','Weapon']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.preferences.filepaths.use_scripts_auto_execute=False
bpy.ops.import_scene.fbx(filepath=str(SOURCE),use_anim=False,use_image_search=False)
arm=next(o for o in bpy.data.objects if o.type=='ARMATURE')
names=list(arm.data.bones.keys());indices={n:i for i,n in enumerate(names)}
before=sha(EXCHANGE)
with np.load(EXCHANGE,allow_pickle=False)as old:result={k:old[k]for k in old.files}
assert list(result['bone_names'])==names
records=[]
for number,name in enumerate(NAMES):
    ob=bpy.data.objects[name]
    bm=bmesh.new();bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='BEAUTY',ngon_method='BEAUTY')
    bm.to_mesh(ob.data);bm.free();ob.data.update()
    mesh=ob.data;mesh.calc_loop_triangles();mesh.calc_normals_split()
    norm=[tuple(n.vector)for n in mesh.corner_normals]
    uv=[tuple(x.uv)for x in mesh.uv_layers.active.data]
    mapping={};raw=[];representatives=[];remap=[]
    for i,loop in enumerate(mesh.loops):
        key=(loop.vertex_index,*np.round(uv[i],8),*np.round(norm[i],6))
        if key not in mapping:mapping[key]=len(raw);raw.append(loop.vertex_index);representatives.append(i)
        remap.append(mapping[key])
    triangles=np.asarray(remap,dtype=np.int64)[np.asarray([t.loops[:]for t in mesh.loop_triangles])]
    texcoords=np.asarray(uv,dtype=np.float32)[representatives];texcoords[:,1]=1.-texcoords[:,1]
    assert np.array_equal(result[f'uv_{number}'],texcoords)
    assert np.array_equal(result[f'indices_{number}'],triangles)
    assert len(result[f'rest_{number}'])==len(raw)
    weights=np.zeros((len(mesh.vertices),len(names)),dtype=np.float32)
    for vertex in mesh.vertices:
        for group in vertex.groups:
            bone=ob.vertex_groups[group.group].name
            if bone in indices:weights[vertex.index,indices[bone]]+=group.weight
    weights=weights[raw]
    assert np.isfinite(weights).all()and np.all(weights>=0)and np.all(weights<=1)
    sums=weights.sum(axis=1)
    assert np.all(sums>.5) and np.all(sums<1.5),(name,float(sums.min()),float(sums.max()))
    result[f'weights_raw_{number}']=weights.copy()
    # Blender's Armature modifier normalizes participating bone weights. The
    # authored FBX has rounding/truncation (one body sum is0.9987085), so expose
    # both unchanged source numbers and the effective normalized skin weights.
    weights/=sums[:,None]
    result[f'weights_{number}']=weights
    records.append({'primitive':number,'source_object':name,'vertices':len(raw),'bones':len(names),'uv_bit_exact':True,'indices_bit_exact':True,'source_weight_sum_range':[float(sums.min()),float(sums.max())],'effective_weights':'Source bone ratios normalized as in the Armature modifier; unchanged encoded values preserved in weights_raw_N','weights_sha256':hashlib.sha256(weights.tobytes()).hexdigest()})
temporary=EXCHANGE.with_suffix('.weights-pending.npz');np.savez_compressed(temporary,**result);temporary.replace(EXCHANGE)
receipt={'schema':'valwood.power-orc.source-skin-weights.v1','passed':True,'source':str(SOURCE),'source_sha256':sha(SOURCE),'before_exchange_sha256':before,'exchange':str(EXCHANGE),'exchange_sha256':sha(EXCHANGE),'primitives':records,'body_geometry_modified':False,'generator':{str(Path(__file__).resolve()):sha(__file__)}}
(STAGE/'arena_orcs/SEMANTIC_WEIGHTS_EXTENSION.json').write_text(json.dumps(receipt,indent=2)+'\n')
manifest=STAGE/'arena_orcs/body/combat_motion_manifest.json';data=json.loads(manifest.read_text())
data['semantic_exchange_sha256']=sha(EXCHANGE);data['generator'].update(receipt['generator']);data['skin_weight_extension']=str(STAGE/'arena_orcs/SEMANTIC_WEIGHTS_EXTENSION.json')
manifest.write_text(json.dumps(data,indent=2)+'\n')
print('POWER_ORC_SKIN_WEIGHTS_VERIFIED',len(records),sha(EXCHANGE),flush=True)

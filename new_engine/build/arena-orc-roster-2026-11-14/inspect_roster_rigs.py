from pathlib import Path
import bpy,json,numpy as np,hashlib,traceback
STAGE=Path(__file__).resolve().parent
SELECTION=STAGE.parent/'arena-orcs-textured-2026-11-14/intake/MESH_CANDIDATE_SELECTION-EXTRACTED.json'
rows=[]
for guid,files in json.loads(SELECTION.read_text())['files'].items():
 p=Path(files['asset']['path']);row={'guid':guid,'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
 try:
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.preferences.filepaths.use_scripts_auto_execute=False
  bpy.ops.import_scene.fbx(filepath=str(p),use_anim=False,use_image_search=False)
  row['armatures']=[];row['meshes']=[]
  for o in bpy.data.objects:
   if o.type=='ARMATURE':row['armatures'].append({'name':o.name,'matrix':list(map(list,o.matrix_world)),'bones':[{'name':b.name,'parent':b.parent.name if b.parent else None,'matrix':list(map(list,o.matrix_world@b.matrix_local))}for b in o.data.bones]})
   if o.type=='MESH':
    o.data.calc_loop_triangles();pts=np.asarray([list(o.matrix_world@v.co)for v in o.data.vertices]);weights=[]
    for v in o.data.vertices:
     weights.append(sum(g.weight for g in v.groups))
    row['meshes'].append({'name':o.name,'vertices':len(o.data.vertices),'triangles':len(o.data.loop_triangles),'bounds':[pts.min(0).tolist(),pts.max(0).tolist()],'matrix':list(map(list,o.matrix_world)),'groups':[g.name for g in o.vertex_groups],'used_materials':sorted(set(f.material_index for f in o.data.polygons)),'materials':[m.name if m else None for m in o.data.materials],'weight_sum_bounds':[min(weights),max(weights)]})
  row['passed']=True
 except Exception as e:row['passed']=False;row['error']=str(e)
 rows.append(row);(STAGE/'ROSTER_SOURCE_RIG_INSPECTION.json').write_text(json.dumps({'rows':rows},indent=2)+'\n');print('ROSTER_RIG',p.name,[(a['name'],len(a['bones']))for a in row.get('armatures',[])],[(m['name'],m['vertices'])for m in row.get('meshes',[])],row.get('error',''),flush=True)

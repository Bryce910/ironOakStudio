from pathlib import Path
import bpy,json,numpy as np
P=Path(__file__).resolve().parent;inventory=json.loads((P/'intake_roster/ORC_SOURCE_ROSTER.json').read_text());out=[]
for r in inventory['characters']:
 if r['id']=='gnur':continue
 weapons=r['weapons'];selected=next((w for w in weapons if w['guid']=='72eeb949d59e9414ea39e4fc9d28a5c7'),None)if r['id']=='orc06'else next((w for w in weapons if 'Weapon_2' in w['path']),None)if r['id']=='orc03'else weapons[0]
 assert selected
 path=Path(selected['path'])
 if r['id']=='orc04':path=P/'intake_roster/ascii-converted/Orc_04_Weapon_Mesh.FBX'
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(path),use_anim=False,use_image_search=False)
 row={'id':r['id'],'source_weapon':selected['path'],'import_weapon':str(path),'weapon_guid':selected['guid'],'meshes':[]}
 for o in sorted([o for o in bpy.data.objects if o.type=='MESH'],key=lambda o:o.name):
  points=np.asarray([list(o.matrix_world@v.co)for v in o.data.vertices]);low,high=points.min(0),points.max(0);span=high-low
  # Genuine source geometry endpoints, explicit raw vertex IDs. The supported
  # part spans the visible upper weapon, excluding the lower handle/pommel.
  section=np.where(points[:,2]>low[2]+span[2]*.40)[0];ids=[int(section[np.argmin(points[section,2])]),int(section[np.argmax(points[section,2])])]
  row['meshes'].append({'name':o.name,'points':points.tolist(),'bounds':[low.tolist(),high.tolist()],'candidate_edge_vertex_ids':ids,'candidate_edge_length':float(np.linalg.norm(points[ids[1]]-points[ids[0]]))})
 out.append(row);(P/'ROSTER_WEAPON_MEASUREMENTS.json').write_text(json.dumps({'passed':True,'characters':out},indent=2)+'\n');print('WEAPON',r['id'],[(m['name'],m['candidate_edge_length'])for m in row['meshes']],flush=True)

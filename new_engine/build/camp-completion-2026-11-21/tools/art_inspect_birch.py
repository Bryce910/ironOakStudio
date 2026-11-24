import bpy,json
from pathlib import Path
from mathutils import Vector
N=Path(__file__).resolve().parents[1];R=N/'review/art/birch';out=[]
for name in ['Birch1','Birch3']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(R/'source'/(name+'.fbx')))
 rec={'name':name,'objects':[],'materials':[]}
 for o in bpy.context.scene.objects:
  if o.type!='MESH':continue
  p=[o.matrix_world@Vector(v.co)for v in o.data.vertices];rec['objects'].append({'name':o.name,'vertices':len(p),'triangles':sum(len(f.vertices)-2 for f in o.data.polygons),'minimum':[min(v[i]for v in p)for i in range(3)],'maximum':[max(v[i]for v in p)for i in range(3)],'materials':[m.name if m else None for m in o.data.materials]})
 for m in bpy.data.materials:
  rec['materials'].append({'name':m.name,'diffuse_color':list(m.diffuse_color),'nodes':[{'name':x.name,'type':x.type,'image':x.image.name if x.type=='TEX_IMAGE'and x.image else None}for x in m.node_tree.nodes]if m.use_nodes else[]})
 out.append(rec)
(R/'SOURCE_INSPECTION.json').write_text(json.dumps(out,indent=2)+'\n')

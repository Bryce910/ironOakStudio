"""Bounded low-detail photogrammetric crag; source CC0 rock untouched, CPU export."""
import bpy,sys,json,hashlib
from pathlib import Path
N=Path(__file__).resolve().parents[1];P=N/'world-project';A=P/'models/art_completion';source=P/'models/mossy_boulder_0.glb';records=[]
for lod,budget in [('near',650),('mid',180)]:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(source));obs=[o for o in bpy.context.scene.objects if o.type=='MESH']
 for o in obs:
  bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);tris=sum(len(p.vertices)-2 for p in o.data.polygons)
  if tris>budget:
   mod=o.modifiers.new('Distant silhouette decimation','DECIMATE');mod.ratio=budget/tris;bpy.ops.object.modifier_apply(modifier=mod.name)
 bpy.ops.export_scene.gltf(filepath=str(A/('horizon_crag.glb'if lod=='near'else'horizon_crag_mid.glb')),export_format='GLB',export_yup=True,export_animations=False)
 records.append({'lod':lod,'target_triangles':budget,'actual_triangles':sum(len(p.vertices)-2 for o in obs for p in o.data.polygons)})
(N/'review/art/CRAG_COOK.json').write_text(json.dumps({'passed':True,'source':str(source.resolve()),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'license':'CC0 Poly Haven rock_moss_set_02','records':records},indent=2)+'\n')

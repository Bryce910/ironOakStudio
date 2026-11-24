"""Restore original ASCII FBX skin-bind frames lost by Assimp's FBX export.
This changes only private imported armature rest matrices; original skin weights,
vertices, UVs and source parent hierarchy are retained. Offsets are read from
independently loaded original source FBX, not inferred from the rendered mesh.
"""
from pathlib import Path
import json,numpy as np,bpy
from mathutils import Matrix

def repair(arm,source_dump,converted_dump,receipt):
 a=json.loads(Path(source_dump).read_text());b=json.loads(Path(converted_dump).read_text())
 def nodes(n,parent=Matrix.Identity(4)):
  m=parent@Matrix(np.asarray(n['transformation']).reshape(4,4));out={n['name']:m}
  for c in n.get('children',[]):out.update(nodes(c,m))
  return out
 na=nodes(a['rootnode']);nb=nodes(b['rootnode']);source_body=next(m for m in a['meshes']if m['name']=='Orc_04_Body_Mesh')
 offsets={v['name']:Matrix(np.asarray(v['offsetmatrix']).reshape(4,4))for v in source_body['bones']}
 scale=Matrix.Diagonal((.01,.01,.01,1.));old={n:arm.matrix_world@bone.matrix_local for n,bone in arm.data.bones.items()};desired={};checks=[]
 for name,offset in offsets.items():
  assert name in old and name in nb
  original_bind=na[source_body['name']]@offset.inverted()
  correction=(scale@nb[name]).inverted()@old[name]
  desired[name]=scale@original_bind@correction
  checks.append({'bone':name,'source_bind_head_m':list((scale@original_bind).translation),'old_imported_head_m':list(old[name].translation),'corrected_head_m':list(desired[name].translation),'bind_frame_change_m':(desired[name].translation-old[name].translation).length})
 bpy.context.view_layer.objects.active=arm;arm.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
 inv=arm.matrix_world.inverted()
 for bone in arm.data.edit_bones:
  if bone.name in desired:bone.matrix=inv@desired[bone.name]
 bpy.ops.object.mode_set(mode='OBJECT')
 for bone in arm.pose.bones:bone.matrix_basis=Matrix.Identity(4)
 bpy.context.view_layer.update()
 errors=[]
 for n,wanted in desired.items():
  actual=arm.matrix_world@arm.data.bones[n].matrix_local;errors.append(max(abs(wanted[i][j]-actual[i][j])for i in range(4)for j in range(4)))
 assert max(errors)<.00001,max(errors)
 result={'schema':'valwood.orc04.original-skin-bind-restoration.v1','passed':True,'method':'Per source bone: original mesh bind-world times inverse original bone offset; retain verified imported bone-axis convention, restore original source bind position/rotation without source geometry or skin weight changes.','maximum_corrected_bind_matrix_error':max(errors),'source_dump':str(source_dump),'converted_dump':str(converted_dump),'bones':checks}
 Path(receipt).write_text(json.dumps(result,indent=2)+'\n');return result

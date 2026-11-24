"""Blender: replace the old six-box throne with bevels, mouldings and inlay.

The original outer bounds are retained for the existing static collider.
"""
from pathlib import Path
import math
import bpy
from mathutils import Matrix,Vector

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'projects/valwood_stronghold/models/stronghold/throne.glb'
TARGET=SOURCE.with_name('nature_throne.glb')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(SOURCE))
materials={m.name:m for m in bpy.data.materials}
stone=materials['Pale carved stone'];dark=materials['Dark granite recess'];bronze=materials['Dwarven aged bronze']
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)

def block(name,center,size,material=stone,bevel=.035):
 bpy.ops.mesh.primitive_cube_add(size=1,location=center);o=bpy.context.object;o.name=name;o.dimensions=size
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(material)
 if bevel:
  mod=o.modifiers.new('Carved edge bevel','BEVEL');mod.width=bevel;mod.segments=3
  bpy.ops.object.modifier_apply(modifier=mod.name)
  mod=o.modifiers.new('Weighted stone face normals','WEIGHTED_NORMAL');mod.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=mod.name)
 # UV density follows physical metres; each face keeps its local projection.
 for polygon in o.data.polygons:
  axis=max(range(3),key=lambda i:abs(polygon.normal[i]));axes=[i for i in range(3)if i!=axis]
  for loop in polygon.loop_indices:
   v=o.data.vertices[o.data.loops[loop].vertex_index].co;o.data.uv_layers.active.data[loop].uv=[v[axes[0]]*.75,v[axes[1]]*.75]
 return o

block('Broad lowest ceremonial step',(0,.1,0),(4,.2,3.6),bevel=.025)
block('Second moulded step',(0,.31,-.12),(3.35,.22,2.95),bevel=.035)
block('Upper stone dais',(0,.53,-.3),(2.55,.22,2.25),bevel=.035)
block('Dais bronze rim',(0,.66,-.30),(2.48,.035,2.18),bronze,.009)
block('Throne seat support',(0,.83,-.35),(1.65,.32,1.3),dark,.06)
block('Throne seat',(0,1.055,-.35),(1.85,.15,1.45),bevel=.06)
block('Inset seat surface',(0,1.143,-.25),(1.42,.027,.97),dark,.012)
block('Carved back stone',(0,2.14,-.94),(1.72,2.17,.32),bevel=.09)
block('Recessed back panel',(0,2.12,-.759),(1.25,1.73,.045),dark,.035)
for side in [-1,1]:
 for z in [-.90,.19]:
  block('Armrest stone support',(side*.91,1.02,z),(.22,.77,.27),bevel=.045)
 block('Long carved armrest',(side*.91,1.45,-.34),(.38,.18,1.61),bevel=.07)
 block('Bronze armrest facing',(side*.91,1.552,-.31),(.26,.035,1.38),bronze,.014)
 block('Back frame upright',(side*.83,2.15,-.73),(.23,2.3,.27),bevel=.045)
 block('Bronze back border',(side*.66,2.12,-.718),(.035,1.84,.035),bronze,.007)
 block('Crown carved shoulder',(side*.55,3.29,-.94),(.68,.20,.45),bevel=.055)
for y in [1.18,3.06]:block('Bronze panel horizontal frame',(0,y,-.718),(1.35,.035,.035),bronze,.006)

def relief_line(a,b,width=.045):
 center=(Vector(a)+Vector(b))/2;direction=Vector(b)-Vector(a)
 o=block('Dwarven bronze relief',center,(width,direction.length,.032),bronze,.006);o.rotation_euler=direction.to_track_quat('Y','Z').to_euler()
for rx,ry in [(.43,.66),(.25,.39)]:
 points=[(0,2.13+ry,-.704),(rx,2.13,-.704),(0,2.13-ry,-.704),(-rx,2.13,-.704)]
 for a,b in zip(points,points[1:]+points[:1]):relief_line(a,b)
relief_line((0,1.74,-.68),(0,2.52,-.68),.055)
block('Crown centre stone',(0,3.435,-.94),(.55,.33,.45),bevel=.07)
block('Crown bronze crest',(0,3.49,-.695),(.16,.18,.03),bronze,.02)

bpy.ops.object.select_all(action='SELECT')
conversion=Matrix.Rotation(math.pi/2,4,'X')
for o in bpy.context.selected_objects:o.matrix_world=conversion@o.matrix_world
for material in [stone,dark,bronze]:
 bpy.ops.object.select_all(action='DESELECT')
 objects=[o for o in bpy.data.objects if o.type=='MESH'and material in list(o.data.materials)]
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join()
bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.gltf(filepath=str(TARGET),export_format='GLB',use_selection=True,export_yup=True,export_apply=True,export_normals=True)
print('Detailed Grandstone throne exported',TARGET)

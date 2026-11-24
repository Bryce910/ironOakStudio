"""Blender-only offline cliff LOD; original downloads are never modified."""
import bpy,sys
args=sys.argv[sys.argv.index('--')+1:]
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=args[0])
for obj in bpy.context.scene.objects:
    if obj.type!='MESH':continue
    bpy.context.view_layer.objects.active=obj
    modifier=obj.modifiers.new('Bounded cliff LOD','DECIMATE')
    modifier.ratio=.32;modifier.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
bpy.ops.export_scene.gltf(filepath=args[1],export_format='GLB',export_materials='NONE',
    export_yup=True,export_normals=True,export_texcoords=True,export_apply=True)

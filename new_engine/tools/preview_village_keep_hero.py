#!/usr/bin/env python3
"""Offline CPU-only geometry/material preview; not an in-engine screenshot."""
from pathlib import Path
import math
import sys
import bpy
from mathutils import Vector

args=sys.argv[sys.argv.index('--')+1:]
asset,output,view=args
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(Path(asset).resolve()))
scene=bpy.context.scene
scene.render.engine='CYCLES'
scene.cycles.device='CPU'
scene.cycles.samples=12
scene.cycles.use_denoising=False
scene.render.threads_mode='FIXED';scene.render.threads=2
scene.render.resolution_x=1100;scene.render.resolution_y=1000
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.filepath=str(Path(output).resolve())
scene.world=bpy.data.worlds.new('OvercastWarmPreview')
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.52,.61,.72,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.4
bpy.ops.mesh.primitive_plane_add(size=250,location=(0,0,-.015))
ground=bpy.context.object
mat=bpy.data.materials.new('PreviewGround');mat.diffuse_color=(.22,.23,.20,1)
ground.data.materials.append(mat)
bpy.ops.object.light_add(type='SUN',location=(32,45,62))
sun=bpy.context.object;sun.data.energy=2.5;sun.data.angle=.075
sun.data.color=(1,.87,.73)
sun.rotation_euler=(Vector((0,0,10))-sun.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(48,70,32) if view=='quarter' else (0,85,21))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,16))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='PERSP';camera.data.lens=47
scene.camera=camera
scene.view_settings.view_transform='AgX'
scene.render.film_transparent=False
bpy.ops.render.render(write_still=True)

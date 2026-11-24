"""Blender-only LOD worker. UVs and normals live on corners; skin on vertices.

The armature is deliberately never imported/exported through Blender, so the
runtime's inverse binds, node hierarchy and palette remain byte-for-byte exact.
"""
from pathlib import Path
import json, sys
import bpy
import numpy as np

folder = Path(sys.argv[sys.argv.index('--') + 1])
rows = json.loads((folder / 'jobs.json').read_text())
results = {}
for row in rows:
    source = np.load(folder / row['input'])
    p, n, uv, j, w, triangles = (source[k] for k in ['POSITION', 'NORMAL', 'TEXCOORD_0', 'JOINTS_0', 'WEIGHTS_0', 'triangles'])
    # Position/skin-identical corners can share a topological vertex while their
    # distinct UV/normal loops remain separate across seams and sharp edges.
    signature = np.c_[np.round(p, 6), j, np.round(w, 6)]
    _, first, inverse = np.unique(signature, axis=0, return_index=True, return_inverse=True)
    mesh = bpy.data.meshes.new(row['key'])
    mesh.from_pydata(p[first].tolist(), [], inverse[triangles].tolist())
    mesh.update()
    # Imported meshes otherwise default to flat polygon shading. Once collapse
    # interpolates/removes custom loop normals that would turn every triangle
    # into a visible facet and unnecessarily split nearly every GPU vertex.
    mesh.polygons.foreach_set('use_smooth', [True]*len(mesh.polygons))
    obj = bpy.data.objects.new(row['key'], mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    layer = mesh.uv_layers.new(name='UVMap')
    loop_source = triangles.reshape(-1)
    layer.data.foreach_set('uv', uv[loop_source].astype(np.float32).reshape(-1))
    mesh.use_auto_smooth = True
    mesh.normals_split_custom_set(n[loop_source].tolist())
    groups = [obj.vertex_groups.new(name=str(k)) for k in range(95)]
    for vertex, original in enumerate(first):
        for joint, weight in zip(j[original], w[original]):
            if weight > 1e-8:
                groups[int(joint)].add([vertex], float(weight), 'REPLACE')
    modifier = obj.modifiers.new(name='Source topology collapse', type='DECIMATE')
    modifier.decimate_type = 'COLLAPSE'
    modifier.ratio = row['ratio']
    modifier.use_collapse_triangulate = True
    # Collapse treats texture-space loops as corner data and interpolates them;
    # all primitives remain separate so material boundaries cannot cross.
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    mesh = obj.data
    mesh.polygons.foreach_set('use_smooth', [True]*len(mesh.polygons))
    mesh.calc_loop_triangles()
    mesh.calc_normals_split()
    vertex_joints, vertex_weights = [], []
    for vertex in mesh.vertices:
        influences = sorted([(g.group, g.weight) for g in vertex.groups if g.weight > 1e-8], key=lambda x: -x[1])[:4]
        total = sum(x[1] for x in influences)
        if not total:
            raise RuntimeError('Lost all influences on a vertex')
        vertex_joints.append([x[0] for x in influences] + [0] * (4-len(influences)))
        vertex_weights.append([x[1]/total for x in influences] + [0] * (4-len(influences)))
    positions = np.array([v.co[:] for v in mesh.vertices], dtype=np.float32)
    loop_vertices = np.array([loop.vertex_index for tri in mesh.loop_triangles for loop in [mesh.loops[k] for k in tri.loops]], dtype=np.int32)
    loop_ids = np.array([k for tri in mesh.loop_triangles for k in tri.loops], dtype=np.int32)
    normals = np.array([mesh.loops[k].normal[:] for k in loop_ids], dtype=np.float32)
    texcoords = np.array([mesh.uv_layers.active.data[k].uv[:] for k in loop_ids], dtype=np.float32)
    positions = positions[loop_vertices]
    # At a few coincident opposing cloth/armor surfaces, a smooth vertex
    # average cancels to zero. Such corners retain a valid triangle surface;
    # use that surface's geometric unit normal rather than exporting a zero.
    normal_length=np.linalg.norm(normals,axis=1)
    invalid=normal_length<1e-6
    if invalid.any():
        face=positions.reshape(-1,3,3)
        face_normal=np.cross(face[:,1]-face[:,0],face[:,2]-face[:,0])
        face_length=np.linalg.norm(face_normal,axis=1)
        expanded=np.repeat(face_normal/np.maximum(face_length[:,None],1e-20),3,axis=0)
        normals[invalid]=expanded[invalid]
        normal_length=np.linalg.norm(normals,axis=1)
    if not np.isfinite(normals).all() or np.min(normal_length)<1e-6:
        raise RuntimeError('No valid smooth or geometric surface normal')
    normals/=normal_length[:,None]
    joints = np.array(vertex_joints, dtype=np.uint16)[loop_vertices]
    weights = np.array(vertex_weights, dtype=np.float32)[loop_vertices]
    # Recover indexed GPU vertices with only genuine UV/normal/skin seams split.
    signature = np.c_[np.round(positions, 6), np.round(normals, 5), np.round(texcoords, 6), joints, np.round(weights, 6)]
    _, first, inverse = np.unique(signature, axis=0, return_index=True, return_inverse=True)
    output = dict(POSITION=positions[first], NORMAL=normals[first], TEXCOORD_0=texcoords[first], JOINTS_0=joints[first], WEIGHTS_0=weights[first], triangles=inverse.reshape(-1, 3).astype(np.uint32))
    np.savez_compressed(folder / row['output'], **output)
    results[row['key']] = {'vertices': len(first), 'triangles': len(inverse)//3, 'input_triangles': len(triangles), 'requested_ratio': row['ratio'], 'opposed_surface_normal_fallback_corners':int(invalid.sum())}
    print('LOD', row['key'], results[row['key']], flush=True)
    bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.meshes.remove(mesh)
(folder / 'lod-results.json').write_text(json.dumps(results, indent=2)+'\n')

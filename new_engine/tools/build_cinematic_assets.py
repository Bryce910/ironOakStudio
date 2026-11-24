"""Prepare source-faithful, bounded Poly Haven forest assets for Valwood.

Preparation (Pillow + NumPy): python tools/build_cinematic_assets.py --download
Cook: blender --factory-startup --background --python tools/build_cinematic_assets.py
The original Seldonne assets are never written. Public API metadata and the source
geometry are retained; only the chosen source variants are range-downloaded.
"""
from pathlib import Path
import copy, hashlib, json, math, struct, sys, urllib.request, time
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/polyhaven/cinematic'
OUT = ROOT / 'projects/valwood_world/models'
TREES = [('pine_tree_01', 1, 'cinematic_pine', 25.0),
         ('fir_tree_01', 2, 'cinematic_fir', 22.0)]
USER_AGENT = 'Valwood-local-asset-cook/1.0 (CC0 game assets)'

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def fetch(url, path, expected=None, byte_range=None):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    size = byte_range[1] - byte_range[0] + 1 if byte_range else (expected or {}).get('size')
    if path.exists() and (size is None or path.stat().st_size == size):
        if not expected or not expected.get('md5') or hashlib.md5(path.read_bytes()).hexdigest() == expected['md5']:
            return
    headers = {'User-Agent': USER_AGENT}
    if byte_range:
        headers['Range'] = 'bytes=%d-%d' % byte_range
    for attempt in range(4):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=180) as response:
                if byte_range and response.status != 206:
                    raise RuntimeError('Source server did not honor bounded Range request')
                data = response.read()
            if size is not None and len(data) != size:
                raise RuntimeError('Truncated source: ' + url)
            if expected and expected.get('md5') and hashlib.md5(data).hexdigest() != expected['md5']:
                raise RuntimeError('Source MD5 mismatch: ' + url)
            temp = path.with_suffix(path.suffix + '.download'); temp.write_bytes(data); temp.replace(path)
            return
        except Exception:
            if attempt == 3: raise
            time.sleep(1 + attempt)

def texture_infos(material):
    pbr = material.get('pbrMetallicRoughness', {})
    return [x for x in [pbr.get('baseColorTexture'), pbr.get('metallicRoughnessTexture'),
                         material.get('normalTexture'), material.get('occlusionTexture')] if x]

def prepare_tree(asset, mesh_index):
    import numpy as np
    folder = SOURCE / asset
    metadata = json.loads((folder / 'metadata.json').read_text())
    record = metadata['gltf']['2k']['gltf']
    source = json.loads((folder / (asset + '.gltf')).read_text())
    binary = record['include'][asset + '.bin']
    primitives = source['meshes'][mesh_index]['primitives']
    leaf = next(p for p in primitives if p['material'] == 2)
    woody = [p for p in primitives if p['material'] != 2]
    # A source canopy point cloud captures its real branch distribution, rather
    # than synthesizing a cone or destroying individual needles with decimation.
    position = source['accessors'][leaf['attributes']['POSITION']]
    view = source['bufferViews'][position['bufferView']]
    start = view.get('byteOffset', 0) + position.get('byteOffset', 0)
    count = position['count']; assert position['componentType'] == 5126 and view.get('byteStride', 12) == 12
    fetch(binary['url'], folder / 'canopy_positions.f32', byte_range=(start, start + count * 12 - 1))
    compact = {'asset': source['asset'], 'scenes': [{'nodes': [0]}], 'scene': 0,
               'nodes': [{'mesh': 0, 'name': asset + '_selected_woody_structure'}],
               'meshes': [{'primitives': copy.deepcopy(woody)}], 'accessors': [],
               'bufferViews': [], 'buffers': [{'uri': 'selected_wood.bin', 'byteLength': 0}],
               'materials': [], 'textures': [], 'images': [], 'samplers': source.get('samplers', [])}
    for k in ['extensionsUsed', 'extensionsRequired']:
        if k in source: compact[k] = source[k]
    accessors = sorted({a for p in woody for a in [p['indices'], *p['attributes'].values()]})
    views = sorted({source['accessors'][a]['bufferView'] for a in accessors})
    amap = {a: i for i, a in enumerate(accessors)}; vmap = {v: i for i, v in enumerate(views)}
    tasks = []
    for v in views:
        old = source['bufferViews'][v]; begin = old.get('byteOffset', 0)
        tasks.append((binary['url'], folder / ('view_%d.bin' % v), None, (begin, begin + old['byteLength'] - 1)))
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda args: fetch(*args), tasks))
    data = bytearray()
    for v in views:
        old = copy.deepcopy(source['bufferViews'][v]); old['byteOffset'] = len(data)
        data.extend((folder / ('view_%d.bin' % v)).read_bytes()); data.extend(b'\0' * (-len(data) % 4))
        compact['bufferViews'].append(old)
    (folder / 'selected_wood.bin').write_bytes(data); compact['buffers'][0]['byteLength'] = len(data)
    for a in accessors:
        ac = copy.deepcopy(source['accessors'][a]); ac['bufferView'] = vmap[ac['bufferView']]
        compact['accessors'].append(ac)
    mats = sorted({p['material'] for p in woody}); mmap = {m: i for i, m in enumerate(mats)}
    tex = sorted({t['index'] for m in mats for t in texture_infos(source['materials'][m])})
    tmap = {t: i for i, t in enumerate(tex)}
    imgs = sorted({source['textures'][t]['source'] for t in tex}); imap = {im: i for i, im in enumerate(imgs)}
    for m in mats:
        mat = copy.deepcopy(source['materials'][m])
        for t in texture_infos(mat): t['index'] = tmap[t['index']]
        compact['materials'].append(mat)
    for t in tex:
        texture = copy.deepcopy(source['textures'][t]); texture['source'] = imap[texture['source']]
        compact['textures'].append(texture)
    tasks = []
    for im in imgs:
        image = source['images'][im]; compact['images'].append(image)
        rec = record['include'][image['uri']]; tasks.append((rec['url'], folder / image['uri'], rec))
    for p in compact['meshes'][0]['primitives']:
        p['indices'] = amap[p['indices']]; p['attributes'] = {k: amap[v] for k, v in p['attributes'].items()}
        p['material'] = mmap[p['material']]
    for kind in ['twig_diff', 'twig_alpha', 'twig_nor_gl', 'twig_arm']:
        # ARM is listed as separately packed AO/roughness/metallic only on some
        # API revisions; find the actual glTF include for the packed texture.
        if kind == 'twig_arm':
            name = next(n for n in record['include'] if '_twig_arm_' in n); rec = record['include'][name]
        elif kind == 'twig_alpha': rec = metadata[kind]['2k']['png']
        else: rec = metadata[kind]['2k']['jpg']
        tasks.append((rec['url'], folder / ('source_' + kind + ('.png' if kind == 'twig_alpha' else '.jpg')), rec))
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda args: fetch(*args), tasks))
    (folder / 'selected_wood.gltf').write_text(json.dumps(compact, indent=2))
    (folder / 'selection.json').write_text(json.dumps({'source_mesh': mesh_index, 'source_canopy_vertices': count,
        'canopy_position_range': [start, start + count * 12 - 1], 'source_binary_url': binary['url'],
        'original_binary_size': binary['size'], 'canopy_sha256': sha(folder / 'canopy_positions.f32'),
        'selected_wood_sha256': sha(folder / 'selected_wood.bin')}, indent=2))
    prepare_shoot(folder, asset)
    print('Prepared', asset, flush=True)

def prepare_shoot(folder, asset):
    """Isolate a photographed shoot; preserve its real alpha/normal/ARM detail."""
    from PIL import Image, ImageFilter, ImageDraw
    import numpy as np
    # Coordinates in the source's 1024px atlas; rescaled for the 2K input.
    rect = (15, 30, 240, 460) if asset.startswith('pine') else (310, 410, 665, 805)
    alpha = Image.open(folder / 'source_twig_alpha.png').convert('L')
    rect = tuple(round(x * alpha.width / 1024) for x in rect)
    mask = alpha.crop(rect)
    # The atlas also contains opaque bleed pixels outside each UV island. Keep
    # only the shoot's connected alpha component, never those corner fragments.
    if asset.startswith('pine'):
        component = mask.point(lambda x: 255 if x >= 128 else 0)
        seed = (136, 350)
        seed = tuple(round(seed[i] * alpha.width / 1024) - rect[i] for i in range(2))
        ImageDraw.floodfill(component, seed, 128)
        component = component.point(lambda x: 255 if x == 128 else 0).filter(ImageFilter.MaxFilter(3))
        mask = Image.fromarray(np.where(np.asarray(component) > 0, np.asarray(mask), 0).astype('uint8'))
    rgba = Image.open(folder / 'source_twig_diff.jpg').convert('RGB').crop(rect).convert('RGBA')
    rgba.putalpha(mask)
    rgba.save(folder / 'shoot_base.png', optimize=True)
    for kind, output in [('twig_nor_gl', 'shoot_normal.png'), ('twig_arm', 'shoot_arm.png')]:
        Image.open(folder / ('source_' + kind + '.jpg')).crop(rect).save(folder / output, optimize=True)

def download():
    SOURCE.mkdir(parents=True, exist_ok=True)
    for asset, _, _, _ in TREES:
        folder = SOURCE / asset; folder.mkdir(exist_ok=True)
        fetch('https://api.polyhaven.com/files/' + asset, folder / 'metadata.json')
        m = json.loads((folder / 'metadata.json').read_text()); rec = m['gltf']['2k']['gltf']
        fetch(rec['url'], folder / (asset + '.gltf'), rec)
    for asset, index, _, _ in TREES: prepare_tree(asset, index)
    asset = 'coastal_cliff_01'; folder = SOURCE / asset
    folder.mkdir(exist_ok=True); fetch('https://api.polyhaven.com/files/' + asset, folder / 'metadata.json')
    meta = json.loads((folder / 'metadata.json').read_text()); rec = meta['gltf']['2k']['gltf']
    fetch(rec['url'], folder / (asset + '.gltf'), rec)
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda item: fetch(item[1]['url'], folder / item[0], item[1]), rec['include'].items()))
    provenance()

def provenance():
    records = []
    for folder in sorted(p for p in SOURCE.iterdir() if p.is_dir()):
        for path in sorted(p for p in folder.rglob('*') if p.is_file() and not p.name.startswith('view_')):
            records.append({'path': str(path.relative_to(SOURCE)), 'bytes': path.stat().st_size,
                            'sha256': sha(path), 'source': 'https://polyhaven.com/a/' + folder.name, 'license': 'CC0-1.0'})
    derived_sources = [('cinematic_pine_young','pine_sapling_small'),('cinematic_broadleaf','tree_small_02'),
                       ('cinematic_pine','pine_tree_01'),('cinematic_fir','fir_tree_01')]
    for path in sorted(p for p in SOURCE.glob('cinematic*') if p.is_file()):
        source = next((asset for prefix,asset in derived_sources if path.name.startswith(prefix)), None)
        if source:
            records.append({'path':path.name,'bytes':path.stat().st_size,'sha256':sha(path),
                            'source':'https://polyhaven.com/a/'+source,'license':'CC0-1.0','derived':True})
    (SOURCE / 'provenance.json').write_text(json.dumps({'asset_license': 'https://polyhaven.com/license',
        'api_terms': 'https://github.com/Poly-Haven/Public-API', 'api_credit': 'Powered by Poly Haven',
        'files': records}, indent=2))

def cook():
    import bpy
    import numpy as np
    from mathutils import Vector
    sys.path.insert(0, str(ROOT / 'tools'))
    from bake_gltf_uv import bake_uv_transforms
    OUT.mkdir(parents=True, exist_ok=True)
    manifest_path = OUT / 'cinematic_manifest.json'
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else []
    only_mid = '--only-mid' in sys.argv

    def clear(): bpy.ops.wm.read_factory_settings(use_empty=True)
    def triangles(o): return sum(len(p.vertices) - 2 for p in o.data.polygons)
    def decimate(o, budget):
        bpy.context.view_layer.objects.active = o
        if triangles(o) > budget:
            mod = o.modifiers.new('Bounded runtime geometry', 'DECIMATE'); mod.ratio = budget / triangles(o)
            bpy.ops.object.modifier_apply(modifier=mod.name)
    def pack():
        for mat in bpy.data.materials:
            mat.use_backface_culling = False
            if mat.node_tree:
                for n in mat.node_tree.nodes:
                    if n.type == 'TEX_IMAGE' and n.image: n.image.pack()
    def export(name, source, extra=None):
        pack(); path = OUT / (name + '.glb')
        bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', export_yup=True,
            export_animations=False, export_cameras=False, export_lights=False, export_image_format='AUTO')
        bake_uv_transforms(path)
        objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
        bounds = [[min(p[i] for p in pts), max(p[i] for p in pts)] for i in range(3)]
        manifest[:] = [r for r in manifest if r['asset'] != name]
        manifest.append({'asset': name, 'source': source, 'license': 'CC0-1.0',
            'triangles': sum(triangles(o) for o in objs), 'bounds_xyz_z_up': bounds,
            'bytes': path.stat().st_size, 'sha256': sha(path), **(extra or {})})
        (OUT / 'cinematic_manifest.json').write_text(json.dumps(manifest, indent=2))
        print('COOKED', name, manifest[-1]['triangles'], flush=True)
    def material(folder, billboard=False):
        mat = bpy.data.materials.new('Scanned needle canopy' if not billboard else 'Canopy silhouette impostor')
        mat.use_nodes = True; mat.blend_method = 'CLIP'; mat.alpha_threshold = .35
        nodes = mat.node_tree.nodes; links = mat.node_tree.links; shader = nodes.get('Principled BSDF')
        shader.inputs['Roughness'].default_value = .88
        for filename, target, space in [('shoot_base.png', 'Base Color', 'sRGB'),
                                         ('shoot_normal.png', 'Normal', 'Non-Color'),
                                         ('shoot_arm.png', 'Roughness', 'Non-Color')]:
            if billboard and target != 'Base Color': continue
            n = nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(str(folder / filename))
            n.image.colorspace_settings.name = space
            if target == 'Normal':
                nm = nodes.new('ShaderNodeNormalMap'); links.new(n.outputs['Color'], nm.inputs['Color'])
                links.new(nm.outputs['Normal'], shader.inputs['Normal'])
            elif target == 'Roughness':
                sep = nodes.new('ShaderNodeSeparateColor'); links.new(n.outputs['Color'], sep.inputs['Color'])
                links.new(sep.outputs['Green'], shader.inputs['Roughness'])
            else:
                links.new(n.outputs['Color'], shader.inputs['Base Color']); links.new(n.outputs['Alpha'], shader.inputs['Alpha'])
        return mat
    def centers(points, target):
        # Source vertex density varies with needle shape. Cluster at a world
        # grid to preserve small branch groups without oversampling dense needles.
        points = points[::3]
        lo, hi = .08, 2.0
        for _ in range(13):
            cell = (lo + hi) * .5; keys = np.floor(points / cell).astype(np.int32)
            _, inverse, counts = np.unique(keys, axis=0, return_inverse=True, return_counts=True)
            if len(counts) > target: lo = cell
            else: hi = cell
        cell = hi; keys = np.floor(points / cell).astype(np.int32)
        _, inv, counts = np.unique(keys, axis=0, return_inverse=True, return_counts=True)
        result = np.column_stack([np.bincount(inv, weights=points[:, i]) / counts for i in range(3)])
        return result, cell
    def keep_major_branches(obj, limit=45):
        # Each source branch is a disconnected tube. A collapse modifier cannot
        # remove whole tubes, so retaining every twig imposes a several-thousand
        # triangle floor. Keep the longest real branches at mid distance.
        import bmesh
        bm=bmesh.new();bm.from_mesh(obj.data);seen=set();groups=[]
        for start in bm.verts:
            if start in seen:continue
            todo=[start];seen.add(start);group=[]
            while todo:
                v=todo.pop();group.append(v)
                for edge in v.link_edges:
                    other=edge.other_vert(v)
                    if other not in seen:seen.add(other);todo.append(other)
            extent=Vector([max(v.co[i] for v in group)-min(v.co[i] for v in group) for i in range(3)]).length
            groups.append((extent,group))
        groups.sort(key=lambda x:x[0],reverse=True)
        remove=[v for _,group in groups[limit:] for v in group]
        if remove:bmesh.ops.delete(bm,geom=remove,context='VERTS')
        bm.to_mesh(obj.data);bm.free();obj.data.update()
    def add_canopy(points, count, mat, seed):
        c, cell = centers(points, count); rng = np.random.default_rng(seed)
        verts, faces, uvs = [], [], []
        for p in c:
            radial = Vector((float(p[0]), float(-p[2]), 0)); radial.normalize()
            yaw = rng.uniform(-math.pi, math.pi)
            across = Vector((math.cos(yaw), math.sin(yaw), rng.uniform(-.25, .25))).normalized()
            # Shoots follow the source's branch cloud; random roll breaks sheets.
            up = (radial * .55 + Vector((0, 0, rng.uniform(.20, .90)))).normalized()
            center = Vector((float(p[0]), float(-p[2]), float(p[1])))
            length = cell * rng.uniform(2.6, 3.5); width = length * .65
            for roll in [0, math.pi * .5]:
                axis = across if not roll else up.cross(across).normalized()
                idx = len(verts)
                for x, y in [(-.5, -.5), (.5, -.5), (.5, .5), (-.5, .5)]:
                    verts.append(center + axis * (x * width) + up * (y * length))
                faces.append((idx, idx+1, idx+2, idx+3)); uvs.extend([(0,0),(1,0),(1,1),(0,1)])
        mesh = bpy.data.meshes.new('Source-clustered needle cards'); mesh.from_pydata(verts, [], faces); mesh.update()
        obj = bpy.data.objects.new('Dense source canopy', mesh); bpy.context.collection.objects.link(obj)
        uv = mesh.uv_layers.new(name='UVMap')
        for poly in mesh.polygons:
            for li in poly.loop_indices: uv.data[li].uv = uvs[mesh.loops[li].vertex_index]
        obj.data.materials.append(mat)
        # Smooth foliage normals avoid obvious individually lit planes.
        normals = []
        for v in mesh.vertices:
            n = Vector((v.co.x, v.co.y, .8)); n.normalize(); normals.append(n)
        mesh.normals_split_custom_set_from_vertices(normals); mesh.use_auto_smooth = True
        return obj
    def normalize(height, center=False):
        objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
        low = Vector([min(p[i] for p in pts) for i in range(3)])
        high = Vector([max(p[i] for p in pts) for i in range(3)])
        scale = height / (high.z - low.z)
        # Tree anchor follows its real trunk axis, not asymmetrical canopy bounds.
        for o in objs:
            if center:
                o.location.x -= (low.x + high.x) * .5
                o.location.y -= (low.y + high.y) * .5
            o.location.z -= low.z
            o.location *= scale; o.scale *= scale
        return high - low
    def render_impostor(name, height):
        scene = bpy.context.scene; scene.render.engine = 'BLENDER_EEVEE'; scene.eevee.taa_render_samples = 64
        scene.eevee.use_gtao = True; scene.eevee.gtao_distance = 3
        scene.render.resolution_x = 1024; scene.render.resolution_y = 1536; scene.render.resolution_percentage = 100
        scene.render.film_transparent = True; scene.render.image_settings.file_format = 'PNG'; scene.render.image_settings.color_mode = 'RGBA'
        scene.world = bpy.data.worlds.new('Soft sky'); scene.world.use_nodes = True
        scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.56,.64,.73,1)
        scene.world.node_tree.nodes['Background'].inputs[1].default_value = .8
        sun = bpy.data.lights.new('Soft sun','SUN'); sun.energy = 2.0; sun.angle = .30
        obj = bpy.data.objects.new('Soft sun',sun); bpy.context.collection.objects.link(obj); obj.rotation_euler = (.65,-.35,-.4)
        fill = bpy.data.lights.new('Sky fill', 'AREA'); fill.energy = 1800; fill.size = 25
        obj = bpy.data.objects.new('Sky fill',fill); bpy.context.collection.objects.link(obj); obj.location = (-10,-15,20)
        obj.rotation_euler = (Vector((0,0,height*.55))-obj.location).to_track_quat('-Z','Y').to_euler()
        camera = bpy.data.cameras.new('Impostor'); obj = bpy.data.objects.new('Impostor',camera); bpy.context.collection.objects.link(obj)
        obj.location = (0,-45,height*.5); obj.rotation_euler = (Vector((0,0,height*.5))-obj.location).to_track_quat('-Z','Y').to_euler()
        camera.type = 'ORTHO'; camera.ortho_scale = height * 1.06; scene.camera = obj
        scene.view_settings.view_transform = 'Standard'; scene.view_settings.look = 'Medium High Contrast'
        scene.render.filepath = str(SOURCE / (name + '_impostor.png')); bpy.ops.render.render(write_still=True)
        return scene.render.filepath, height * 1.06 * 1024 / 1536, height * 1.06

    for asset, index, name, height in TREES:
        # The production recipe runs refine_mature_canopies.py afterwards.
        # Skip the historical coarse canopy and its Eevee render in that path.
        if '--skip-tree-cook' in sys.argv: continue
        folder = SOURCE / asset; points = np.fromfile(folder / 'canopy_positions.f32', dtype='<f4').reshape(-1,3)
        for lod, wood_budget, clusters in [('near', 9500, 1500), ('mid', 1350, 480)]:
            if only_mid and lod != 'mid':continue
            clear(); bpy.ops.import_scene.gltf(filepath=str(folder / 'selected_wood.gltf'))
            wood = [o for o in bpy.context.scene.objects if o.type == 'MESH']
            if lod == 'mid':
                # glTF splits vertices at UV/normal seams. Weld geometric
                # duplicates first, then assign an explicit trunk budget so
                # numerous tiny branch pieces cannot consume all reduction.
                bpy.ops.object.select_all(action='DESELECT')
                for o in wood:
                    o.select_set(True); bpy.context.view_layer.objects.active = o
                    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
                    bpy.ops.mesh.remove_doubles(threshold=.00005)
                    bpy.ops.mesh.separate(type='MATERIAL'); bpy.ops.object.mode_set(mode='OBJECT')
                    o.select_set(False)
                wood = [o for o in bpy.context.scene.objects if o.type == 'MESH']
            for o in wood:
                if lod == 'mid':
                    matname = o.data.materials[o.data.polygons[0].material_index].name
                    budget = 650 if 'trunk' in matname else (100 if 'dead' in matname else 600)
                    if 'bark' in matname:keep_major_branches(o)
                else: budget = wood_budget
                decimate(o, budget)
                for p in o.data.polygons: p.use_smooth = True
            add_canopy(points, clusters, material(folder), 831 if asset.startswith('pine') else 1741)
            normalize(height); export(name + '_' + lod, asset, {'lod': lod, 'canopy_method': 'source-point-clustered photographed needle cards'})
            if lod == 'near':
                image, width, card_height = render_impostor(name, height)
        if only_mid:continue
        clear()
        # One upright card. The runtime rotates this plane cylindrically toward
        # the camera, preventing crossed photographic silhouettes from doubling.
        verts, faces, uvs = [], [], []
        for i in range(1):
            theta = i * math.pi / 3; side = Vector((math.cos(theta),math.sin(theta),0)) * width * .5
            at = len(verts); base = (height-card_height)*.5
            verts.extend([-side+Vector((0,0,base)),side+Vector((0,0,base)),side+Vector((0,0,base+card_height)),-side+Vector((0,0,base+card_height))])
            faces.append((at,at+1,at+2,at+3)); uvs.extend([(0,0),(1,0),(1,1),(0,1)])
        mesh = bpy.data.meshes.new('Upright canopy billboard'); mesh.from_pydata(verts,[],faces); mesh.update()
        obj = bpy.data.objects.new(name+'_far',mesh); bpy.context.collection.objects.link(obj)
        uv=mesh.uv_layers.new(name='UVMap')
        for p in mesh.polygons:
            for li in p.loop_indices:uv.data[li].uv=uvs[mesh.loops[li].vertex_index]
        mat=bpy.data.materials.new('Scanned canopy distant silhouette');mat.use_nodes=True;mat.blend_method='CLIP';mat.alpha_threshold=.25
        shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=.95
        n=mat.node_tree.nodes.new('ShaderNodeTexImage');n.image=bpy.data.images.load(image)
        mat.node_tree.links.new(n.outputs['Color'],shader.inputs['Base Color']);mat.node_tree.links.new(n.outputs['Alpha'],shader.inputs['Alpha'])
        obj.data.materials.append(mat)
        export(name+'_far',asset,{'lod':'far','canopy_method':'single upright camera-facing card baked from near asset','height_m':height})
        del points

    if only_mid:
        provenance()
        return

    # One large shoreline formation, preserving its scanned dimensions and PBR.
    clear(); bpy.ops.import_scene.gltf(filepath=str(SOURCE/'coastal_cliff_01/coastal_cliff_01.gltf'))
    for o in [o for o in bpy.context.scene.objects if o.type=='MESH']:decimate(o,18000)
    export('cinematic_cliff_01','coastal_cliff_01')
    # Existing source scans provide variation without modifying Seldonne's files.
    for index in range(3):
        clear();bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/polyhaven/models/rock_moss_set_02/rock_moss_set_02.gltf'))
        objects=sorted([o for o in bpy.context.scene.objects if o.type=='MESH'],key=lambda o:o.name)
        chosen=objects[index]
        for o in objects:
            if o!=chosen:bpy.data.objects.remove(o,do_unlink=True)
        decimate(chosen,6000);normalize(1.8+index*.7,center=True)
        export('cinematic_rock_'+str(index),'rock_moss_set_02')
    provenance()

def validate():
    """CPU-only checks matching the custom engine's static GLB contract."""
    import io
    import numpy as np
    from PIL import Image
    image_sets = {}
    manifests = {r['asset']: r for r in json.loads((OUT / 'cinematic_manifest.json').read_text())}
    for name, entry in manifests.items():
        path = OUT / (name + '.glb'); raw = path.read_bytes()
        assert struct.unpack_from('<III', raw) == (0x46546c67, 2, len(raw)), name
        length, kind = struct.unpack_from('<II', raw, 12); assert kind == 0x4e4f534a
        gltf = json.loads(raw[20:20+length]); begin = 20 + length
        blen, kind = struct.unpack_from('<II', raw, begin); assert kind == 0x004e4942
        binary = memoryview(raw)[begin+8:begin+8+blen]
        assert len(gltf['meshes']) <= 128 and len(raw) < 128 * 1024 * 1024
        assert not gltf.get('animations') and not gltf.get('skins')
        tris = 0
        def accessor(index):
            a=gltf['accessors'][index];v=gltf['bufferViews'][a['bufferView']]
            dtype={5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']]
            components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
            stride=v.get('byteStride',np.dtype(dtype).itemsize*components)
            offset=v.get('byteOffset',0)+a.get('byteOffset',0)
            return np.ndarray((a['count'],components),dtype=dtype,buffer=binary,offset=offset,strides=(stride,np.dtype(dtype).itemsize))
        for mesh in gltf['meshes']:
            for primitive in mesh['primitives']:
                assert primitive.get('mode',4)==4
                attrs=primitive['attributes']; assert {'POSITION','NORMAL','TEXCOORD_0'} <= attrs.keys()
                pos=accessor(attrs['POSITION']);norm=accessor(attrs['NORMAL']);uv=accessor(attrs['TEXCOORD_0'])
                assert len(pos)==len(norm)==len(uv) and np.isfinite(pos).all() and np.isfinite(norm).all() and np.isfinite(uv).all()
                assert (np.linalg.norm(norm,axis=1) > .8).all(), name+' invalid normals'
                indices=accessor(primitive['indices']); assert len(indices)%3==0 and indices.max()<len(pos)
                tris+=len(indices)//3
        assert tris==entry['triangles'] and sha(path)==entry['sha256']
        if name.endswith('_near'):
            assert tris <= (20000 if name.startswith(('cinematic_broadleaf_','cinematic_pine_young_')) else 18000)
        if name.endswith('_mid'): assert tris <= 4000, (name,tris)
        if name.startswith('cinematic_grass_'):
            assert 150 <= tris <= 500
            assert .25 <= entry['height_m'] <= .45
            assert all(mat.get('alphaMode')=='MASK' for mat in gltf['materials'])
        if name.endswith('_far'):
            assert tris == 2
            assert np.max(np.abs(pos[:,2])) < 1e-6, name+' must be a local XY plane'
        hashes=set()
        for image in gltf['images']:
            assert 'uri' not in image and 'bufferView' in image
            view=gltf['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0)
            encoded=binary[offset:offset+view['byteLength']].tobytes()
            with Image.open(io.BytesIO(encoded)) as decoded:
                assert max(decoded.size)<=2048, (name,decoded.size)
            hashes.add(hashlib.sha256(encoded).hexdigest())
        image_sets[name]=hashes
        for mat in gltf['materials']:
            for info in texture_infos(mat):
                assert not info.get('extensions',{}).get('KHR_texture_transform')
                assert info.get('texCoord',0)==0
        print('VALID',name,tris,'triangles',len(hashes),'embedded images',flush=True)
    for _,_,name,_ in TREES:
        assert image_sets[name+'_near']==image_sets[name+'_mid'], name+' LOD textures must share GPU cache entries'
    for name in ['cinematic_broadleaf','cinematic_pine_young']:
        if name+'_near' in image_sets:
            assert image_sets[name+'_near']==image_sets[name+'_mid'], name+' LOD textures must share GPU cache entries'
    if 'cinematic_deadwood' in image_sets:
        assert image_sets['cinematic_deadwood']==image_sets['cinematic_deadwood_mid']
    print('All cinematic GLBs satisfy runtime limits; near/mid embedded texture bytes are shared.')

def billboard_far():
    """CPU-only conversion of existing crossed-card bakes; image bytes unchanged."""
    import numpy as np
    manifest_path=OUT/'cinematic_manifest.json'
    manifest=json.loads(manifest_path.read_text())
    for _,_,prefix,_ in TREES:
        path=OUT/(prefix+'_far.glb');raw=path.read_bytes()
        length,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a
        gltf=json.loads(raw[20:20+length]);begin=20+length
        blen,kind=struct.unpack_from('<II',raw,begin);assert kind==0x004e4942
        binary=raw[begin+8:begin+8+blen]
        prim=gltf['meshes'][0]['primitives'][0]
        wanted={index:4 for index in prim['attributes'].values()}
        wanted[prim['indices']]=6
        sizes={};image_hashes=[]
        for image in gltf['images']:
            view=gltf['bufferViews'][image['bufferView']]
            image_hashes.append(hashlib.sha256(binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]).hexdigest())
        for index,count in wanted.items():
            a=gltf['accessors'][index];assert a.get('byteOffset',0)==0
            view=gltf['bufferViews'][a['bufferView']];assert 'byteStride' not in view
            components={'VEC3':3,'VEC2':2,'VEC4':4,'SCALAR':1}[a['type']]
            component_size={5126:4,5123:2,5125:4}[a['componentType']]
            sizes[a['bufferView']]=count*components*component_size;a['count']=count
            if index==prim['attributes']['POSITION']:
                p=np.frombuffer(binary,dtype='<f4',count=count*3,offset=view.get('byteOffset',0)).reshape(-1,3)
                assert np.max(np.abs(p[:,2]))<1e-6
                a['min']=p.min(axis=0).tolist();a['max']=p.max(axis=0).tolist()
                bounds=[[float(p[:,0].min()),float(p[:,0].max())],[0.,0.],[float(p[:,1].min()),float(p[:,1].max())]]
        compact=bytearray()
        for index,view in enumerate(gltf['bufferViews']):
            start=view.get('byteOffset',0);n=sizes.get(index,view['byteLength'])
            view['byteOffset']=len(compact);view['byteLength']=n
            compact.extend(binary[start:start+n]);compact.extend(b'\0'*(-len(compact)%4))
        gltf['buffers'][0]['byteLength']=len(compact)
        gltf['meshes'][0]['name']='Upright canopy billboard'
        encoded=json.dumps(gltf,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
        output=struct.pack('<III',0x46546c67,2,12+8+len(encoded)+8+len(compact))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(compact),0x004e4942)+compact
        path.write_bytes(output)
        for image,expected in zip(gltf['images'],image_hashes):
            view=gltf['bufferViews'][image['bufferView']]
            assert hashlib.sha256(compact[view['byteOffset']:view['byteOffset']+view['byteLength']]).hexdigest()==expected
        entry=next(r for r in manifest if r['asset']==prefix+'_far')
        entry.update(triangles=2,bounds_xyz_z_up=bounds,bytes=path.stat().st_size,sha256=sha(path),
                     canopy_method='single upright camera-facing card baked from near asset')
        print('CONVERTED',path.name,'2 triangles; embedded image SHA256 unchanged',flush=True)
    manifest_path.write_text(json.dumps(manifest,indent=2))

if __name__ == '__main__':
    if '--download' in sys.argv: download()
    elif '--validate' in sys.argv: validate()
    elif '--billboard-far' in sys.argv: billboard_far()
    else: cook()

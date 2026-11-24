#!/usr/bin/env python3
"""A bounded, free-asset art pass for the isolated modern opening.

Call ``upgrade_home_art(g)`` after home/story/gaming dressing and before
``g.finish()``. It replaces existing table/bag materials and the cup picture,
then adds two individually selected stationery props and bag stitching. It
does not touch colliders, runtime code, shared sources, or a live project.
All imported images are embedded at 1K or less; source assets stay intact.
"""
from __future__ import annotations

import hashlib
import io
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
STAGED = ROOT / 'assets/polyhaven/opening_upgrade/2026-11-03'
FABRIC = ROOT / 'assets/polyhaven/milestone/fabric_pattern_07/textures'


def _sha(raw):
    return hashlib.sha256(raw).hexdigest()


def _png(image):
    output = io.BytesIO()
    image.save(output, format='PNG', optimize=True)
    return output.getvalue()


def _image(g, texture):
    item = g.doc['images'][g.doc['textures'][texture]['source']]
    view = g.doc['bufferViews'][item['bufferView']]
    start = view.get('byteOffset', 0)
    return bytes(g.binary[start:start + view['byteLength']])


def _texture(g, raw, size, records, name, sources, operation):
    index = g.texture(raw, size=size)
    embedded = _image(g, index)
    image = Image.open(io.BytesIO(embedded))
    records.append(dict(name=name, texture_index=index,
                        width=image.width, height=image.height,
                        sha256=_sha(embedded), embedded_bytes=len(embedded),
                        operation=operation,
                        source_inputs=[dict(path=str(p.relative_to(ROOT)),
                                            sha256=_sha(p.read_bytes()))
                                       for p in sources]))
    return index


def _material(g, name, base, normal, arm, records, sources, size=1024,
              rgb=(1., 1., 1.), normal_scale=.4, metallic=0.):
    material = g.material(name, rgb, 1.)
    doc = g.doc['materials'][material]
    pbr = doc['pbrMetallicRoughness']
    pbr['metallicFactor'] = metallic
    for channel, raw in [('base', base), ('normal', normal), ('arm', arm)]:
        texture = _texture(g, raw, size, records, name + ' / ' + channel,
                           sources, 'Embedded PNG; downsampled to at most %d px' % size)
        if channel == 'base':
            pbr['baseColorTexture'] = dict(index=texture)
        elif channel == 'normal':
            doc['normalTexture'] = dict(index=texture, scale=normal_scale)
        else:
            pbr['metallicRoughnessTexture'] = dict(index=texture)
            doc['occlusionTexture'] = dict(index=texture)
    return material


def _named_material(g, name):
    matches = [i for i, material in enumerate(g.doc['materials'])
               if material.get('name') == name]
    if len(matches) != 1:
        raise ValueError('Expected one home material: ' + name)
    return matches[0]


def _take_part(g, material, center):
    """Remove exactly one original generated prop, never a nearby duplicate."""
    matches = []
    for index, part in enumerate(g.geometry[material]):
        position = part[0]
        midpoint = (position.min(0) + position.max(0)) * .5
        if np.linalg.norm(midpoint - center) < 1e-5:
            matches.append(index)
    if len(matches) != 1:
        raise ValueError('Expected one existing prop at %s; found %d' % (center, len(matches)))
    return g.geometry[material].pop(matches[0])


def _resurface_table(g, records):
    material = _named_material(g, 'coffee_table worn material')
    if len(g.geometry[material]) != 1:
        raise ValueError('Expected the single accepted coffee table')
    position, normal, uv, indices = g.geometry[material].pop()
    del g.geometry[material]
    before = position.min(0).tolist(), position.max(0).tolist()
    source = STAGED / 'wood_table_worn/source'
    paths = [source / ('wood_table_worn_' + suffix + '_2k.jpg')
             for suffix in ['diff', 'nor_gl', 'arm']]
    # Dry scratched furniture rather than a lacquered surface. Keep genuine
    # scan variation, but prevent its shallow smooth patches from looking wet.
    arm = np.array(Image.open(paths[2]).convert('RGB'))
    arm[:, :, 1] = np.maximum(arm[:, :, 1], 202)
    arm[:, :, 2] = 0
    wood = _material(g, 'Art | dry scanned worn wooden coffee table',
                     paths[0].read_bytes(), paths[1].read_bytes(),
                     _png(Image.fromarray(arm)), records, paths, normal_scale=.28)
    # Each original triangle is retained exactly. Independent planar UV seams
    # prevent the old source atlas from chopping up this tiled wood scan.
    tris = indices.reshape(-1, 3)
    points = position[tris].reshape(-1, 3)
    normals = normal[tris].reshape(-1, 3)
    mapped = []
    for triangle in position[tris]:
        face = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
        axis = int(np.argmax(abs(face)))
        if axis == 1:
            # Grain runs along the long X axis of the table.
            mapped.extend(np.stack(((triangle[:, 2] - .90844277) / .90,
                                    (triangle[:, 0] + 3.075) / 1.55), axis=1))
        elif axis == 0:
            mapped.extend(np.stack((triangle[:, 2] / .90, triangle[:, 1] / 1.55), axis=1))
        else:
            mapped.extend(np.stack((triangle[:, 0] / 1.55, triangle[:, 1] / .90), axis=1))
    g.mesh(points, normals, mapped, np.arange(len(points)), wood)
    if not np.array_equal(points.reshape(-1, 3, 3), position[tris]):
        raise AssertionError('Coffee table positions changed')
    return dict(geometry_positions_unchanged=True,
                minimum=before[0], maximum=before[1],
                accepted_top_y=float(position[:, 1].max()),
                triangles=len(tris), replacement='Existing table material and UVs only',
                unused_scanned_table='Small Wooden Table 01 was intentionally not placed: its proportions do not fit the locked coffee-table props.')


def _ordinary_coffee(g, records):
    index = _named_material(g, 'coffee_cup worn material')
    material = g.doc['materials'][index]
    old = material['pbrMetallicRoughness']['baseColorTexture']['index']
    raw = _image(g, old)
    image = Image.open(io.BytesIO(raw)).convert('RGB')
    pixels = np.asarray(image).astype(float)
    height, width = pixels.shape[:2]
    yy, xx = np.indices((height, width), dtype=float)
    # These are the coffee island's actual source-atlas coordinates, not a
    # screen-space decal. The white ceramic rim and exterior remain intact.
    radius = np.sqrt(((xx / width - 299. / 2048.) / (279. / 2048.)) ** 2 +
                     ((yy / height - 883. / 2048.) / (278. / 2048.)) ** 2)
    blend = np.clip((1.003 - radius) * 160., 0., 1.)
    meniscus = np.clip((radius - .91) / .09, 0., 1.)
    variation = .7 * np.sin(xx * .058) * np.sin(yy * .041)
    coffee = np.array([36., 24., 16.])[None, None, :] + variation[:, :, None]
    coffee = coffee + meniscus[:, :, None] * np.array([18., 11., 5.])
    pixels = pixels * (1. - blend[:, :, None]) + coffee * blend[:, :, None]
    derived = _png(Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)))
    source = ROOT / 'models/apartment/coffee_cup.glb'
    texture = _texture(g, derived, 512, records, 'Art | ordinary cold coffee', [source],
                       'Replace only coffee UV island with dark coffee and a restrained meniscus; preserve ceramic and original geometry')
    material['name'] = 'Art | ordinary cold coffee in existing ceramic cup'
    material['pbrMetallicRoughness']['baseColorTexture']['index'] = texture
    material['pbrMetallicRoughness']['roughnessFactor'] = .84
    return dict(original_geometry_unchanged=True, latte_art_removed=True,
                source_texture_sha256=_sha(raw), texture_index=texture)


def _bag(g, records):
    from opening_home_set import _rounded_box, _tube
    sage = _named_material(g, 'Set | faded sage woven cloth')
    dark = _named_material(g, 'Set | matte charcoal hardware')
    body = _take_part(g, sage, [1.35, .84, 4.55])
    # Exactly remove the two old rectangular uprights and their rigid bridge.
    for center in [[1.23, 1.06, 4.55], [1.47, 1.06, 4.55], [1.35, 1.14, 4.55]]:
        _take_part(g, dark, center)
    paths = [FABRIC / ('fabric_pattern_07_' + suffix + '_1k.jpg')
             for suffix in ['col_1', 'nor_gl', 'rough']]
    color = Image.open(paths[0]).convert('RGB').resize((512, 512), Image.Resampling.LANCZOS)
    luma = np.array(color.convert('L')).astype(float)
    # Remove the red check design while keeping its photographed thread weave.
    low = np.array(color.convert('L').filter(ImageFilter.GaussianBlur(7))).astype(float)
    weave = np.clip(luma - low, -34., 34.)
    base = np.clip(np.array([78., 80., 67.])[None, None, :] + weave[:, :, None] * .60, 0, 255).astype(np.uint8)
    rough = Image.open(paths[2]).convert('L').resize((512, 512), Image.Resampling.LANCZOS)
    arm = np.zeros((512, 512, 3), dtype=np.uint8)
    arm[:, :, 0] = 255
    arm[:, :, 1] = np.maximum(np.asarray(rough), 226)
    canvas = _material(g, 'Art | weathered neutral woven work-bag canvas',
                       _png(Image.fromarray(base)), paths[1].read_bytes(),
                       _png(Image.fromarray(arm)), records, paths,
                       size=512, normal_scale=.24)
    # The original rounded body silhouette, floor clearance and badge anchor
    # are retained. Added details fit entirely within the accepted bag bounds.
    g.mesh(*body, canvas)
    seam = g.material('Art | faded canvas seam and webbing', [.135, .145, .115], .99)
    thread = g.material('Art | muted bag seam thread', [.29, .29, .235], .99)
    metal = g.material('Art | worn bag zipper hardware', [.19, .18, .15], .67)
    angle = math.radians(-8)
    c, s = math.cos(angle), math.sin(angle)
    def point(x, y, z):
        return np.array([1.35 + x * c + z * s, .84 + y, 4.55 - x * s + z * c])
    # A shallow stitched pocket leaves the existing worn work badge visible.
    _rounded_box(g, point(0, -.075, -.078), [.285, .145, .007], canvas,
                 .018, yaw=-8, subdivisions=3)
    pocket = [(-.125, -.007), (-.136, -.018), (-.136, -.127),
              (-.125, -.139), (.125, -.139), (.136, -.127),
              (.136, -.018), (.125, -.007)]
    _tube(g, [point(x, y, -.082) for x, y in pocket], .0008, thread, sides=5)
    # The zip lives on the original top surface, with a modest pull at one end.
    _tube(g, [point(x, .174, -.010) for x in np.linspace(-.14, .14, 12)],
          .0016, seam, sides=6)
    _rounded_box(g, point(.117, .176, -.010), [.017, .004, .009], metal,
                 .001, yaw=-8, subdivisions=1)
    # Soft double-layer webbing forms a continuous arch instead of three
    # disconnected cubes. Its upper silhouette stays below the old 1.1575 m.
    rail = []
    for t in np.linspace(0., math.pi, 21):
        rail.append(point(-.119 * math.cos(t), .17 + .139 * math.sin(t), 0.))
    for a, b in zip(rail, rail[1:]):
        direction = b - a
        length = float(np.linalg.norm(direction))
        side = np.array([-s, 0., -c]) * .010
        face = np.cross(direction, side)
        face /= np.linalg.norm(face)
        face *= .0035
        position = np.array([a-side-face, a+side-face, b-side-face, b+side-face,
                             a-side+face, a+side+face, b-side+face, b+side+face])
        faces = [(0, 2, 3, 1), (4, 5, 7, 6), (0, 4, 6, 2),
                 (1, 3, 7, 5), (0, 1, 5, 4), (2, 6, 7, 3)]
        for corners in faces:
            pos = position[list(corners)]
            normal = np.cross(pos[1]-pos[0], pos[2]-pos[0])
            normal /= np.linalg.norm(normal)
            g.mesh(pos, [normal] * 4, [[0,0], [0,1], [1,1], [1,0]],
                   [0,1,2,0,2,3], seam)
    return dict(original_body_geometry_unchanged=True, original_handle_parts_replaced=3,
                badge_preserved=True, texture_resolution=512,
                additions=['Shallow sewn front pocket', 'Restrained seam stitching',
                           'Top zipper and pull', 'Soft webbing carry handle'],
                source_url='https://polyhaven.com/a/fabric_pattern_07')


def _stationery(g, records):
    # Import locally to avoid a cycle when the builder imports this helper.
    from build_opening_prologue import read_glb, array
    path = STAGED / 'stationery_supplies/stationery_supplies.glb'
    doc, binary = read_glb(path)
    source = doc['materials'][0]
    def raw(texture):
        image = doc['images'][doc['textures'][texture]['source']]
        view = doc['bufferViews'][image['bufferView']]
        start = view.get('byteOffset', 0)
        return binary[start:start+view['byteLength']]
    pbr = source['pbrMetallicRoughness']
    mat = _material(g, 'Art | scanned everyday pen and short pencil',
                    raw(pbr['baseColorTexture']['index']),
                    raw(source['normalTexture']['index']),
                    raw(pbr['metallicRoughnessTexture']['index']),
                    records, [path], normal_scale=.4, metallic=1.)
    # The old generated desk clutter was centered in front of the real
    # scanned tabletop. Remove those six unsupported cosmetic pieces instead
    # of leaving two conflicting stationery sets or pretending they touch.
    cream = _named_material(g, 'Set | offwhite paper and ceramics')
    blue = _named_material(g, 'Set | faded blue bookcloth')
    oak = _named_material(g, 'Set | worn pale oak edges')
    _take_part(g, cream, [-3.84, .964, -3.65])
    _take_part(g, blue, [-3.23, 1.02, -3.78])
    for i in range(4):
        _take_part(g, oak, [-3.25 + i*.012, 1.12, -3.78])
    desk_parts = [part for material, parts in g.geometry.items()
                  if g.doc['materials'][material].get('name', '').startswith('full_computer_desk ')
                  for part in parts]
    desk_triangles = np.concatenate([part[0][part[3].reshape(-1, 3)] for part in desk_parts])
    def support_at(x, z):
        # Vertical ray intersections against actual desk triangles, not its
        # AABB (which also contains the empty chair and knee-space region).
        plane = desk_triangles[:, :, [0, 2]]
        a = plane[:, 0]
        v0, v1, v2 = plane[:, 1]-a, plane[:, 2]-a, np.array([x, z])-a
        determinant = v0[:, 0]*v1[:, 1] - v0[:, 1]*v1[:, 0]
        valid = abs(determinant) > 1e-12
        u = np.zeros(len(determinant)); v = np.zeros(len(determinant))
        u[valid] = (v2[:, 0]*v1[:, 1]-v2[:, 1]*v1[:, 0])[valid]/determinant[valid]
        v[valid] = (v0[:, 0]*v2[:, 1]-v0[:, 1]*v2[:, 0])[valid]/determinant[valid]
        inside = valid & (u >= -1e-6) & (v >= -1e-6) & (u+v <= 1.000001)
        y = desk_triangles[:, 0, 1] + u*(desk_triangles[:, 1, 1]-desk_triangles[:, 0, 1]) + v*(desk_triangles[:, 2, 1]-desk_triangles[:, 0, 1])
        hits = y[inside & (y < 1.)]
        if len(hits) == 0:
            raise ValueError('New desk pencil has no real tabletop support')
        return float(hits.max())
    placed = []
    for name, center, yaw in [
            ('stationery_supplies_pen_red', [-2.355, .8116, 1.055], 75.),
            ('stationery_supplies_pencil_used', [-4.20, 0., -4.12], -18.)]:
        nodes = [node for node in doc['nodes'] if node.get('name') == name]
        if len(nodes) != 1:
            raise ValueError('Expected one selected stationery node: ' + name)
        node = nodes[0]
        if any(key in node for key in ['rotation', 'scale', 'matrix', 'children']):
            raise ValueError('Selected stationery needs unsupported transform')
        primitives = doc['meshes'][node['mesh']]['primitives']
        # Bake this object's original node translation before recentering it
        # on its new tabletop anchor. Never import the source display layout.
        translation = np.asarray(node.get('translation', [0., 0., 0.]))
        points = np.concatenate([array(doc, binary, p['attributes']['POSITION']) + translation
                                 for p in primitives])
        lo, hi = points.min(0), points.max(0)
        shift = np.array([-(lo[0]+hi[0])*.5, -lo[1], -(lo[2]+hi[2])*.5])
        angle = math.radians(yaw)
        c, s = math.cos(angle), math.sin(angle)
        rot = np.array([[c,0,s], [0,1,0], [-s,0,c]])
        contact = None
        if name.endswith('pencil_used'):
            footprint = (points+shift) @ rot.T + center
            minimum, maximum = footprint.min(0), footprint.max(0)
            samples = [(center[0], center[2])]+[(x, z)
                      for x in [minimum[0], maximum[0]]
                      for z in [minimum[2], maximum[2]]]
            support = [support_at(x, z) for x, z in samples]
            if max(support)-min(support) > .003:
                raise ValueError('Desk pencil anchor crosses existing clutter or an uneven edge')
            center[1] = max(support)+.0008
            contact = dict(real_desk_triangle_samples=len(samples),
                           minimum_support_y=min(support), maximum_support_y=max(support),
                           minimum_vertical_clearance=.0008)
        output = []
        triangles = 0
        for primitive in primitives:
            position = (array(doc, binary, primitive['attributes']['POSITION']) + translation + shift) @ rot.T + center
            normal = array(doc, binary, primitive['attributes']['NORMAL']) @ rot.T
            uv = array(doc, binary, primitive['attributes']['TEXCOORD_0'])
            indices = array(doc, binary, primitive['indices']).ravel()
            g.mesh(position, normal, uv, indices, mat)
            output.append(position)
            triangles += len(indices) // 3
        output = np.concatenate(output)
        placed.append(dict(node=name, minimum=output.min(0).tolist(),
                           maximum=output.max(0).tolist(), triangles=triangles,
                           original_node_translation_baked=True, desk_contact=contact))
    return dict(source=str(path.relative_to(ROOT)), sha256=_sha(path.read_bytes()),
                selected=placed, source_display_layout_not_imported=True,
                unsupported_old_desk_props_removed=6,
                source_url='https://polyhaven.com/a/stationery_supplies')


def upgrade_home_art(g):
    """Upgrade the complete home accumulator in place and return provenance.

    Hook after ``dress_opening_story(g)`` and before ``g.finish()``. No collider
    parameter is accepted, so the accepted blockers and route cannot change.
    This optional pass is intentionally separate from the accepted old build.
    """
    records = []
    triangles_before = sum(len(part[3])//3 for parts in g.geometry.values() for part in parts)
    table = _resurface_table(g, records)
    coffee = _ordinary_coffee(g, records)
    bag = _bag(g, records)
    stationery = _stationery(g, records)
    # Reduce the bright toy-like accents already authored in this room. This
    # changes no source models, gamepad shape, TV picture or readable print.
    muted = {
        'Set | faded blue bookcloth': [.16, .20, .20],
        'Gaming | blue game case': [.055, .105, .13],
        'Gaming | purple game case': [.135, .095, .11],
        'Gaming | cream game sleeve': [.46, .43, .34],
        'Gaming | muted teal controls': [.10, .21, .18],
    }
    for name, color in muted.items():
        material = g.doc['materials'][_named_material(g, name)]
        material['pbrMetallicRoughness']['baseColorFactor'] = [*color, 1.]
    triangles_after = sum(len(part[3])//3 for parts in g.geometry.values() for part in parts)
    return dict(version=1, cost_usd=0, source_license='CC0-1.0',
                license_url='https://polyhaven.com/license',
                table=table, coffee=coffee, work_bag=bag, stationery=stationery,
                muted_existing_materials=list(muted), derived_embedded_images=records,
                triangles_before=triangles_before, triangles_after=triangles_after,
                collision_bodies_added=0, existing_story_anchors_unchanged=True,
                source_assets_unchanged=True, character_or_hand_animation_added=False,
                policy='Optional isolated art pass; dry worn furniture, selected desk belongings, neutral canvas and ordinary coffee. No duplicated table, full stationery display, extra floor obstacles, source edits or character claims.')

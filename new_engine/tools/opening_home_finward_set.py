#!/usr/bin/env python3
"""Fit purchased Finward furniture into the private opening accumulator.

Call after the existing home, bathroom, story and optional art pass, before
``g.finish()``. Only exact old material names and contained old fixture parts
are removed. The TV, console, bill/coffee anchors, room shell, vanity, shower
and existing room-light setup are retained. Source assets are never changed.

Explicit target dimensions and independently authored box proxies are shared
by both source LOD tiers, so changing the rendered tier cannot change physics.
The caller owns project writes, native visual review and route acceptance.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

from opening_home_finward import import_finward


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'assets/purchased/finward_house_props'
FLOOR = .18


def _bounds(part):
    position = np.asarray(part[0])
    return position.min(0), position.max(0)


def _remove(g, names, *, low=None, high=None, center=None, tolerance=1e-6):
    """Remove whole authored parts only; never cut an unrelated triangle mesh."""
    names = set(names)
    removed = []
    for material, parts in list(g.geometry.items()):
        name = g.doc['materials'][material].get('name')
        if name not in names:
            continue
        keep = []
        for part in parts:
            minimum, maximum = _bounds(part)
            selected = True
            if low is not None:
                selected &= bool(np.all(minimum >= np.asarray(low) - tolerance))
            if high is not None:
                selected &= bool(np.all(maximum <= np.asarray(high) + tolerance))
            if center is not None:
                selected &= bool(np.allclose((minimum + maximum) * .5, center,
                                              atol=tolerance, rtol=0))
            if selected:
                removed.append(dict(material=name, triangles=len(part[3]) // 3,
                                    minimum=minimum.tolist(), maximum=maximum.tolist()))
            else:
                keep.append(part)
        if keep:
            g.geometry[material] = keep
        else:
            del g.geometry[material]
    return removed


def _fixed_proxy(name, x, z, width, height, depth, *, yaw=0., y=FLOOR,
                 inset=.02):
    """Deterministic local AABB, independent of the source LOD's mesh bounds."""
    if yaw not in (0., 90., 180., 270.):
        raise ValueError('Opening proxies require quarter-turn furniture placement')
    sx, sz = (depth, width) if yaw in (90., 270.) else (width, depth)
    return (name, [x, y + height * .5, z],
            [max(.04, sx - 2 * inset), height, max(.04, sz - 2 * inset)])


def upgrade_finward_set(g, dressing, collision_boxes, *, tier='near'):
    """Replace selected furnishings and update the supplied static proxy list.

    ``tier`` selects the cooked source's ``_near`` or ``_mid`` GLB. Both use
    identical local metre dimensions, placement and physics. This function
    returns composition provenance and never writes a file or launches Unity.
    """
    if tier not in ('near', 'mid'):
        raise ValueError('Choose Finward near or mid furniture tier')
    cooked = DATA / 'cooked-v2'
    catalog_path = cooked / 'catalog.json'
    catalog = {item['name']: item for item in json.loads(catalog_path.read_text())}
    required = ('couch', 'bed', 'toilet', 'sidetable_a', 'lamp_table_a', 'kitchen_counter_sink',
                'kitchen_counter_a', 'stove', 'kitchen_cabinet_b', 'fridge')
    sources = {}
    for name in required:
        item = catalog[name]
        model = next(model for model in item['models']
                     if model['file'] == name + '_' + tier + '.glb')
        path = (cooked / model['file']).resolve(strict=True)
        if not path.is_relative_to(cooked.resolve()):
            raise ValueError('Cooked furniture path escaped its asset directory')
        if hashlib.sha256(path.read_bytes()).hexdigest() != model['sha256']:
            raise ValueError('Cooked furniture changed after source review: ' + name)
        sources[name] = path

    before_triangles = sum(len(part[3]) // 3 for parts in g.geometry.values() for part in parts)
    before_boxes = len(collision_boxes)
    removed = {}
    for name in ('couch', 'nice_bed'):
        parts = _remove(g, [name + ' worn material'])
        if not parts:
            raise ValueError('Expected old opening furniture: ' + name)
        removed[name] = parts

    # The replacement bed includes authored bedding. Remove precisely the two
    # generated blanket pieces, without disturbing the entry bag or curtains.
    removed['old_bed_blanket'] = []
    for center in ([3.85, .69, -2.99], [3.85, .57, -2.76]):
        parts = _remove(g, ['Set | faded sage woven cloth'], center=center)
        if len(parts) != 1:
            raise ValueError('Expected one existing generated bed blanket part')
        removed['old_bed_blanket'].extend(parts)

    # The old toilet was cleaned into the same porcelain material as the sink.
    # Isolate only whole parts inside its already audited fixture bounds.
    old_toilet = dressing['bathroom']['furnishings'][0]
    if Path(old_toilet['source']).stem != 'clean_toilet':
        raise ValueError('Unexpected old bathroom fixture order')
    removed['old_toilet'] = _remove(
        g, ['Bathroom | clean neutral porcelain fixtures'],
        low=old_toilet['minimum'], high=old_toilet['maximum'], tolerance=2e-5)
    if sum(part['triangles'] for part in removed['old_toilet']) != old_toilet['private_material_cleanup']['retained_triangles']:
        raise ValueError('Could not isolate the complete old toilet from shared porcelain')
    # Its separate two-button flush plate is not part of the source mesh.
    removed['old_flush_button'] = _remove(
        g, ['Bathroom | brushed nickel shower hardware', 'Bathroom | muted dark hardware'],
        low=[5.25, 1.02, -.015], high=[5.33, 1.045, .055])
    if len(removed['old_flush_button']) != 2:
        raise ValueError('Expected exactly the old toilet flush button and split')

    # Keep the splash tiles and light bar. Shared wood/hardware materials also
    # occur elsewhere; only complete parts within this original kitchen zone
    # may be removed. This replaces the old base, sink/tap, hob/oven and uppers.
    kitchen_materials = [
        'Set | soft greige painted wood', 'Set | matte sage cabinet enamel',
        'Set | matte charcoal hardware', 'Set | honed warm grey worktop',
        'Set | brushed sink and tap', 'Set | shaded sink basin',
        'Set | unlit oven and hob glass', 'Set | unlit hob ring',
    ]
    removed['old_kitchen'] = _remove(
        g, kitchen_materials, low=[-1.31, .18, -4.89], high=[.87, 2.45, -4.10])
    if len(removed['old_kitchen']) < 30:
        raise ValueError('Expected the complete bounded original kitchen set')

    old_proxy_names = {'couch furniture', 'nice_bed furniture',
                       'Bathroom clean_toilet', 'Kitchen base and counter'}
    existing = [name for name, _, _ in collision_boxes if name in old_proxy_names]
    if sorted(existing) != sorted(old_proxy_names):
        raise ValueError('Opening replacement expected one proxy for each old furnishing')
    collision_boxes[:] = [box for box in collision_boxes if box[0] not in old_proxy_names]

    placed = []
    added_proxies = []

    def place(name, label, x, z, width, height, depth, *, yaw=0., y=FLOOR,
              proxy_height=None, proxy=True):
        result = import_finward(g, sources[name], x, z, y=y, width=width,
                                height=height, depth=depth, yaw=yaw,
                                # MID begins 75m away and remains pinned during
                                # transport. 128px is ample at that coverage;
                                # keep the close-up 1K maps and loading headroom.
                                texture_size=1024 if tier == 'near' else 128, label=label)
        result.update(asset=name, label=label, rendered_tier=tier,
                      source_prefab=catalog[name]['source_prefab'],
                      target_local_dimensions=[width, height, depth])
        placed.append(result)
        if proxy:
            body = _fixed_proxy('Finward ' + label, x, z, width,
                                height if proxy_height is None else proxy_height,
                                depth, yaw=yaw, y=y)
            collision_boxes.append(body)
            added_proxies.append(dict(name=body[0], center=body[1], size=body[2]))
        return result

    # Cooked orientation was measured from source high backrests and doors:
    # couch fronts -Z; headboard +Z; kitchen fronts -Z; toilet fronts -Z.
    place('couch', 'living-room couch', -2.75, 3.85, 2.55, .98, 1.20)
    # Keep .43m from the switch approach to the front of the bed. The full
    # production capsule route needs more clearance than the visual silhouette.
    place('bed', 'bedroom bed', 3.85, -3.75, 1.80, 1.03, 2.14, yaw=180.)
    # Half-scale source dimensions retain the actual shallow cabinet ratio.
    # Rotating it west makes its shallow depth fit along the east bedroom wall.
    place('sidetable_a', 'bedside table', 5.03, -3.97,
          .529875, .41792, .19903, yaw=90.)
    lamp = place('lamp_table_a', 'bedside lamp', 5.03, -3.97,
                 .219, .38, .219, y=FLOOR + .41792, proxy=False)
    lamp_emission = []
    for index in lamp['material_indices'].values():
        material = g.doc['materials'][index]
        lamp_emission.append(dict(material=material.get('name'),
                                  source_emissive_factor=material.get('emissiveFactor'),
                                  source_emissive_texture=material.get('emissiveTexture')))
        material.pop('emissiveTexture', None)
        material['emissiveFactor'] = [0., 0., 0.]
    toilet = place('toilet', 'bathroom toilet', 5.04, .02, .47, .84, .65, yaw=90.)

    # Three 70 cm modules retain the original counter zone and 1.10 m worktop.
    # The sink's full height includes its tap; only its cabinet is a blocker.
    place('kitchen_counter_sink', 'kitchen sink cabinet', -.92, -4.52,
          .70, 1.133, .68, yaw=180., proxy_height=.92)
    place('kitchen_counter_a', 'kitchen counter cabinet', -.22, -4.52,
          .70, .92, .68, yaw=180.)
    place('stove', 'kitchen stove', .48, -4.52, .70, .94, .68, yaw=180.)
    for number, x in enumerate((-.92, -.22, .48), 1):
        place('kitchen_cabinet_b', 'kitchen upper cabinet %d' % number,
              x, -4.68, .70, .79, .36, yaw=180., y=1.65)
    # At the left end of the row, the refrigerator leaves standing access to
    # the stove and a wide gap to the desk; no loose appliance blocks the aisle.
    place('fridge', 'kitchen refrigerator', -1.70, -4.52,
          .72, 1.88, .62, yaw=180.)

    # Report the actual replacement width while preserving the original sink,
    # shower, doorway and route records for subsequent native verification.
    dressing['bathroom']['aisle'] = dict(
        west_x=3.8105, east_x=float(toilet['minimum'][0]),
        clear_width=float(toilet['minimum'][0]) - 3.8105)
    after_triangles = sum(len(part[3]) // 3 for parts in g.geometry.values() for part in parts)
    return dict(
        version=1, product_id=265523,
        product_url='https://assetstore.unity.com/packages/3d/props/interior/house-props-furniture-vol-2-265523',
        license='userpurchasedUnityAssetStoreStandardEULA', rendered_tier=tier,
        cooked_catalog=str(catalog_path),
        cooked_catalog_sha256=hashlib.sha256(catalog_path.read_bytes()).hexdigest(),
        removed_parts=removed, placements=placed,
        removed_collision_names=sorted(old_proxy_names), added_collision_boxes=added_proxies,
        collision_bodies_before=before_boxes, collision_bodies_after=len(collision_boxes),
        collision_policy='Fixed authored metre dimensions and quarter-turn AABBs shared by near/mid tiers; .02m horizontal inset; sink blocker excludes tap height',
        triangles_before=before_triangles, triangles_after=after_triangles,
        existing_story_anchors_unchanged=True, source_assets_unchanged=True,
        retained=['TV and console', 'Coffee table and bills', 'Bathroom vanity and shower',
                  'Kitchen backsplash and under-cabinet light bar', 'Existing room-light switches and fixtures'],
        lamp_policy='Bedside lamp fitted below the existing switched practical point; imported emission suppressed to avoid glowing when the bedroom switch is off',
        lamp_emission_suppressed=lamp_emission,
        limitations=['Native framing, material appearance and walking/collision acceptance remain caller verification',
                     'One half-scale shallow source cabinet turned toward the bed; no second table',
                     'Fixed kitchen and furniture proxies approximate solid volumes; fine taps and handles do not add snagging bodies'])

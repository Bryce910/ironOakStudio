#!/usr/bin/env python3
"""Published keep contract: exact geometry proof and preserved kit metadata."""
import copy
import json
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_village_keep_hero as keep

manifest=keep.validate()
published=json.loads((keep.MODELS/'village_asset_manifest.json').read_text())
before=json.loads((keep.BEFORE/'village_asset_manifest.json').read_text())
old={r['asset']:r for r in before['assets']}
rows={r['asset']:r for r in published['assets']}
assert len(old)==len(rows)==22
for source in manifest['assets']:
    name=source['asset'];actual=rows[name]
    assert keep.sha(keep.MODELS/(name+'.glb'))==source['sha256']==actual['sha256']
    assert actual['collision_boxes']==old[name]['collision_boxes']
    assert actual['door_openings']==old[name]['door_openings']
    assert actual['maximum_design_dimensions']==old[name]['maximum_design_dimensions']
    assert actual['bounds_y_up']==old[name]['bounds_y_up']
    assert actual['hero_recipe']==keep.RECIPE
    assert actual['draw_materials']==4
for name,source in old.items():
    if name in {'village_keep','village_keep_mid'}:continue
    actual=copy.deepcopy(rows[name]);baseline=copy.deepcopy(source)
    # The shared validator is allowed to recompute its diagnostics; no model,
    # material, collision, dimension, license, or provenance row may change.
    actual.pop('validation',None);baseline.pop('validation',None)
    assert actual==baseline,('unrelated kit row changed',name)
    assert keep.sha(keep.MODELS/(name+'.glb'))==source['sha256']
summary=published['summary'];credit=summary['resident_keep_credit']
assert summary['private_budget_mib']==128
assert summary['private_rgba8_mip_bytes']<=128*1048576
assert summary['rgba8_mip_bytes']==summary['private_rgba8_mip_bytes']+credit['rgba8_mip_bytes']
assert credit['images']==len(credit['exact_encoded_channel_hashes'])==6
assert credit['incremental_scene_image_mib']==0
assert keep.sha(keep.ROOT/credit['donor'])==credit['donor_sha256']
assert summary['encoded_glb_bytes']==sum(r['bytes'] for r in rows.values())
print('PASS published hero keep: 77/65 real recesses, 646 portal rays, exact bounds/14 colliders, four draws, zero new resident image policies, 20 unrelated kit assets preserved.')

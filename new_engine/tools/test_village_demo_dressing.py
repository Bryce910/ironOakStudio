#!/usr/bin/env python3
"""Focused actual-model regressions for the complete demo's set dressing."""
import copy,json,math,tempfile
from build_castle_village import Author,PATH,Atlas,validate as validate_full_scene
from build_village_demo_dressing import apply,validate

author=Author(json.loads(PATH.read_text()));author.compose()
metadata=apply(author);proof=validate(author)
assert proof['instances']==479 and proof['pockets']==12 and proof['keep_hall_instances']==40
assert proof['new_model_families']==proof['new_texture_policies']==0
assert proof['real_mesh_surface_rays']==490 and proof['keep_collider_boxes_checked']==14
before=copy.deepcopy(author.records);assert apply(author)==metadata and author.records==before
records={r['label']:r for r in author.records}
def rejected(change,expected):
    original=copy.deepcopy(author.records)
    try:
        change();validate(author)
    except AssertionError as error:assert expected in str(error),(expected,error)
    else:raise AssertionError('Invalid demo accepted: '+expected)
    finally:
        for target,source in zip(author.records,original):target.clear();target.update(source)

cargo=next(records[x]for x in metadata['labels']if records[x]['alias']=='purchased_crate')
table=records['demo_workshop_table_0']
rejected(lambda:cargo.update(x=table['x'],z=table['z']),'volume intersects')
lamp=next(records[x]for x in metadata['interior_labels']if records[x]['alias']=='purchased_prop_lamp')
rejected(lambda:lamp.update(x=0),'protected street/crowd/hall aisle')
rejected(lambda:lamp.update(x=11.3,z=-151.5),'volume intersects')
rejected(lambda:cargo.update(x=-36,z=100),'protected street/crowd/hall aisle')
food=records['demo_hall_table_west_goods_0']
rejected(lambda:food.update(elevation=food['elevation']+.1),'supported contact changed')
flag=records['demo_hall_wall_flag_west']
rejected(lambda:flag.update(elevation=2),'cloth lowered')
# Model/field labels and ordering are strict, while actual12-significant-digit
# instance serialization must not be mistaken for a placement edit.
for record in author.records:
    for name in ['x','z','yaw','scale','elevation','burial']:
        record[name]+=2e-9
validate(author)
for target,source in zip(author.records,before):target.clear();target.update(source)
with tempfile.TemporaryDirectory(prefix='valwood-demo-dressing-full-')as directory:
    complete=validate_full_scene(author,Atlas(directory),True)
assert complete['records']==len(author.records)and complete['demo_dressing']['instances']==479
print('PASS demo dressing:479 authored instances/12 exterior stories/40 hall objects; exact old pockets/final103,490 real support rays,14 keep colliders and6m hall aisle; zero new image families; collision/route/contact/lowcloth negatives and numeric roundtrip controls')
print('PASS complete Author/Atlas scene: actual purchased structure envelopes/15cm gaps,all houses/cargo/roads/terrain routes,products and old/new dressing validated together')

#!/usr/bin/env python3
"""Focused deterministic district geometry/connectivity negative controls."""
import copy
import json
import math
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_castle_village as base
import build_village_demo_district as district

layout=json.loads(base.PATH.read_text());layout.pop('demo_dressing',None)
author=base.Author(layout);author.compose();metadata=district.apply(author)
proof=district.validate(author)
records_before=copy.deepcopy(author.records);layout_before=copy.deepcopy(author.layout)
assert district.apply(author)==metadata and author.records==records_before and author.layout==layout_before,'apply is not idempotent'

def clone():
    result=copy.copy(author);result.records=copy.deepcopy(author.records);result.layout=copy.deepcopy(author.layout)
    return result

def rejected(candidate,expected):
    try:district.validate(candidate)
    except AssertionError as error:
        assert expected in str(error),(expected,str(error))
        return
    raise AssertionError('Negative control passed: '+expected)

candidate=clone()
for r in candidate.records:
    r['x']=(float(format(.487+r['x']/base.WIDTH,'.12g'))-.487)*base.WIDTH
    r['z']=(float(format(.320+r['z']/base.DEPTH,'.12g'))-.320)*base.DEPTH
    r['yaw']=math.radians(float(format(math.degrees(r['yaw']),'.12g')))
    for key in ['scale','burial','elevation']:r[key]=float(format(r[key],'.12g'))
district.validate(candidate)

candidate=clone();r=next(r for r in candidate.records if r['label']==metadata['added_labels'][0]);r.update(x=0,z=180)
rejected(candidate,'House blocks a paved road capsule')
candidate=clone();r=next(r for r in candidate.records if r['label']==metadata['added_labels'][0]);b=next(r for r in candidate.records if r['label']==metadata['added_labels'][1]);r.update(x=b['x'],z=b['z'])
rejected(candidate,'Whole-house/eave overlap')
candidate=clone();next(r for r in candidate.records if r['label']=='home_2_0')['x']+=.01
rejected(candidate,'Existing lane changed')
candidate=clone();candidate.layout['roads']=[r for r in candidate.layout['roads'] if r['id']!='demo_lane_north_link']
rejected(candidate,'Missing/moved lane connector')
candidate=clone();next(r for r in candidate.records if r['label']=='west_smithy')['x']+=.01
rejected(candidate,'Existing workshop/inn changed')
print(json.dumps(proof,indent=2))
print('PASS district: deterministic/idempotent 42-frontage composition, actual12-significant-digit instance roundtrip, exact eaves/roads/bays, old route/NPC corridors, five negative controls including0.01m retained moves.')

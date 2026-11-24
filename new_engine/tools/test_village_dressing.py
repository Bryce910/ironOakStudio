#!/usr/bin/env python3
"""Real-mesh negative checks for the authored street's physical constraints."""
import json,math
from types import SimpleNamespace
from build_castle_village import Author,PATH,validate_dressing,validate_authored_lanes

author=Author(json.loads(PATH.read_text()));author.compose()
result=validate_dressing(author)
assert result['dressing_pockets']==6 and result['dressing_objects']==183
assert result['overhead_rows']==3 and result['overhead_flags']==18
assert result['overhead_minimum_clearance_m']>4.8
market=[r for r in author.records if r['label'].startswith('final_')]
assert len(market)==103 and all(math.hypot(r['x']-17,r['z']-32)<35 for r in market)

def rejected(action,expected):
    try:action()
    except AssertionError as error:
        assert expected in str(error),(expected,error)
    else:raise AssertionError('Invalid authored geometry accepted: '+expected)

by_label={r['label']:r for r in author.records}
cargo=by_label['dress_cooper_sack_0'];old=cargo.copy()
cargo.update(x=by_label['dress_cooper_cask_0']['x'],z=by_label['dress_cooper_cask_0']['z'])
rejected(lambda:validate_dressing(author),'Intersecting street cargo volumes')
cargo.update(old)
flag=by_label['dress_bunting_entry_flag_0'];old=flag['elevation'];flag['elevation']=2
rejected(lambda:validate_dressing(author),'Overhead object intrudes on walking volume')
flag['elevation']=old
cargo=by_label['dress_cooper_crate_0'];old=cargo.copy();cargo['x']=61.75
flat=SimpleNamespace(sample=lambda x,z:SimpleNamespace(height=24))
rejected(lambda:validate_authored_lanes(author,flat),'Authored lane blocked')
cargo.update(old)
rejected(lambda:author.surface('starting_table',3,3,.73,.76),'No real support surface')
validate_dressing(author);validate_authored_lanes(author,flat)
print('Street dressing: real cargo intersection, low cloth, blocked walking line and missing tabletop support rejected; restored scene PASS')

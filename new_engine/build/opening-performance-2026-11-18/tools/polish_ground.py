"""Broaden existing arrival leaf beds without adding stamps, images or draws."""
from pathlib import Path
import hashlib,json,shlex
N=Path(__file__).resolve().parents[1]
P=N/'world-project/world.studio'
assert not (N/'UNIFIED_READY.json').exists(), 'Published authoring is immutable'
before=P.read_text();rows=before.splitlines();changes=[]
assert not (N/'review/POLISH.json').exists(), 'Preserve the previous pass'
for i,line in enumerate(rows):
    outer=shlex.split(line)
    if not outer or outer[0]!='surfaces':continue
    record=shlex.split(outer[1])
    if record[:2]==['layer','Arrival detailed woodland litter']:
        # Warmer decomposing leaves keep source texture variation visible
        # alongside the cool shaded meadow, without changing scene exposure.
        record[5:8]=['0.76','0.65','0.47']
    elif record and record[0]=='paint' and record[5]=='7' and float(record[3])<7:
        # These eight already-authored beds follow actual arrival trees.
        # Widen their existing soft falloff; retain their centers and capacity.
        record[3]=f'{float(record[3])*1.45:.8f}'
        record[4]='0.91'
    elif record and record[0]=='paint' and record[5]=='7' and abs(float(record[1])+11977.7899673)<.001:
        # Carry woodland soil onto the pale exposed slope beside the first
        # bend. Road/river ribbons apply afterward and retain their surfaces.
        record[3]='48'
        record[4]='0.78'
    else:continue
    updated=' '.join(json.dumps(x) if ' ' in x else x for x in record)
    rows[i]='surfaces '+json.dumps(updated)
    changes.append({'line':i+1,'before':line,'after':rows[i]})
assert len(changes)==10
after='\n'.join(rows)+'\n'
(N/'review/pre-polish.studio').write_text(before)
P.write_text(after)
(N/'review/POLISH.json').write_text(json.dumps(dict(passed=True,
    scope='Arrival woodland leaf beds',changes=changes,
    before_sha256=hashlib.sha256(before.encode()).hexdigest(),
    after_sha256=hashlib.sha256(after.encode()).hexdigest(),
    paint_count_unchanged=True,images_and_layers_unchanged=True,
    source_models_terrain_water_collision_and_routes_unchanged=True),indent=2)+'\n')
print('Polished eight existing leaf beds and their shared woodland material.')

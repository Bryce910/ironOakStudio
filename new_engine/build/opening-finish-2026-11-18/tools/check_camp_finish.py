#!/usr/bin/env python3
"""Audit exact camp deltas and exercise the production C++ source parser."""
from pathlib import Path
import collections, hashlib, json, math, subprocess

N=Path(__file__).resolve().parents[1];P=N/'world-project';O=N/'review/scenery'
R=json.loads((O/'CANDIDATE.json').read_text())
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
checks={}
checks['candidate_sha256_match']=all(sha(P/f['relative_path'])==f['after_sha256'] for f in R['files'])
for f in R['files'][:2]:
    old=(O/(Path(f['relative_path']).name+'.before')).read_text().splitlines();new=(P/f['relative_path']).read_text().splitlines()
    expected=sorted(c['line'] for c in R['instance_changes'] if c['file']==f['relative_path'])
    assert len(old)==len(new) and [i+1 for i,(a,b) in enumerate(zip(old,new)) if a!=b]==expected
    assert all((a.split()[0] if a.split() else '')==(b.split()[0] if b.split() else '') for a,b in zip(old,new))
checks['exact_source_rows_aliases_and_order']=True
studio=(P/'world.studio').read_text().splitlines();before=(O/'world.studio.before').read_text().splitlines()
assert len(studio)==len(before)
assert [i+1 for i,(a,b) in enumerate(zip(before,studio)) if a!=b]==[c['line'] for c in R['studio_changes']]
checks['exact_six_studio_records']=True
checks['no_durable_source_model_bindings']=not any(s.startswith(('override ','group ')) for s in studio) and not any(
    s.startswith('entities ') and json.loads(s[9:]).split()[3:4]==['2'] for s in studio if not s.startswith('entities "VALWOOD'))
checks['paint_stroke_budget_preserved']=sum(s.startswith('surfaces "paint ') for s in studio)==sum(s.startswith('surfaces "paint ') for s in before)==256
checks['height_unchanged']=sha(P/'ground/showcase.height')==R['source_height_sha256']
checks['wind_response_metadata_preserved']=all(c['before'].split()[14:]==c['after'].split()[14:] for c in R['instance_changes'])
checks['production_numeric_ranges']=all(.15<=float(c['after'].split()[4])<=4 and all(.1<=float(c['after'].split()[k])<=8 for k in (10,11,12)) for c in R['instance_changes'])
checks['fire_npc_entry_clearance']=all(math.hypot((float(c['after'].split()[1])-.5)*100800+11988,
    (float(c['after'].split()[2])-.5)*(100800*2818/5009)-5172)>6.4 for c in R['instance_changes'] if c['kind']=='understory')
fixes=json.loads((O/'REJECTED_SOURCE_SCALES.json').read_text())['corrections']
checks['compensated_geometry_exact']=all(max(abs(float(c['invalid'].split()[4])*float(c['invalid'].split()[k])-
    float(c['valid'].split()[4])*float(c['valid'].split()[k])) for k in (10,11,12))<1e-12 and
    c['invalid'].split()[13]==c['valid'].split()[13] for c in fixes)
binary=O/'check-source-parser';raw=O/'PARSER_RAW.json'
positive=subprocess.run([str(binary),str(P),str(raw)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(O/'parser-positive.log').write_text(positive.stdout)
checks['actual_production_parser_bounds']=positive.returncode==0 and json.loads(raw.read_text())['passed']

# A real invalid-input control ensures the checker catches the exact defect
# missed in the first CPU audit rather than merely counting source rows.
fixture=O/'invalid-scale-fixture';fixture.mkdir(exist_ok=True);(fixture/'scenery').mkdir(exist_ok=True)
for name in ('world.studio','ground','models'):
    target=fixture/name
    if not target.exists():target.symlink_to(P/name)
for name in ('bep_environment_instances.txt','demo_polish_instances.txt'):
    text=(P/'scenery'/name).read_text()
    if name=='bep_environment_instances.txt':
        values=text.splitlines();c=fixes[0];assert c['file']=='scenery/'+name;values[c['line']-1]=c['invalid'];text='\n'.join(values)+'\n'
    (fixture/'scenery'/name).write_text(text)
negative=subprocess.run([str(binary),str(fixture),str(O/'invalid-unexpected.json')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(O/'parser-negative.log').write_text(negative.stdout)
checks['actual_parser_rejects_invalid_scale']=negative.returncode!=0 and 'line57' in negative.stdout.replace(' ','') and 'out-of-range' in negative.stdout
assert all(checks.values()),checks
values=[c['actual_basal_embedding_preserved_m'] for c in R['instance_changes']]
report=dict(passed=True,gpu_used=False,checks=checks,instance_counts=dict(collections.Counter(c['kind'] for c in R['instance_changes'])),
    basal_embedding_m_range=[min(values),max(values)],production_parser=json.loads(raw.read_text()),
    parser_binary_sha256=sha(binary),parser_source_sha256=sha(O/'check_source_parser.cpp'),
    production_source_sha256=sha(N/'source-v1/engine/world_scenery.cpp'),
    initial_cpu_review_superseded=True,notes=['Initial CPU review missed uniform-scale minimum; corrected geometry-equivalent axis serialization and exercised actual parser with positive/negative controls.',
    '36 affected source keys change with intended pose. No durable source binding references them; other row keys/order retained.',
    'Actual visible basal support excludes synthetic datums. Native visual/performance and walking review remains parent-owned.'])
(O/'CPU_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

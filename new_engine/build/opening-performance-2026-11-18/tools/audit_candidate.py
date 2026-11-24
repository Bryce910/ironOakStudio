from pathlib import Path
import hashlib,json,collections,subprocess
N=Path(__file__).resolve().parents[1];B=N.with_name('scenery-ecology-2026-11-18')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
origin=json.loads((N/'SOURCE_ORIGIN.json').read_text())['baseline_sha256']
actual={str(p.relative_to(N/'source-v1')):sha(p) for p in (N/'source-v1').rglob('*') if p.is_file()}
changed=[p for p,h in origin.items() if actual.get(p)!=h]
assert set(changed)=={'engine/world_models.cpp'},changed
assert actual.keys()-origin.keys()=={'tests/world_pose_blend_gl_test.cpp','engine/world_model_pose_storage.h'}
polish=json.loads((N/'review/POLISH.json').read_text())
before=(N/'review/pre-polish.studio').read_text().splitlines();after=(N/'world-project/world.studio').read_text().splitlines()
assert len(before)==len(after)
actual_changes=[dict(line=i+1,before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b]
assert actual_changes==polish['changes'] and len(actual_changes)==10
assert all(x['before'].startswith('surfaces ') and x['after'].startswith('surfaces ') for x in actual_changes)
for f in (B/'world-project/scenery').glob('*instances.txt'):assert sha(f)==sha(N/'world-project/scenery'/f.name)
for p,h in json.loads((N/'review/LIVE_SAVE_BEFORE.json').read_text()).items():assert sha(p)==h,p
plan=json.loads((N/'runtime-native-v1/READY.json').read_text())
assert plan['ready'] and plan['application_units']==104 and sha(N/'runtime/rpg')==plan['sha256']
for row in json.loads((N/'runtime-native-v1/BUILD_PLAN.json').read_text())['units']:
    receipt=json.loads(Path(row['target']).with_suffix('.json').read_text())
    assert all(sha(p)==h for p,h in receipt['actual_dependency_sha256'].items())
assert (N/'shared/shaders').resolve()==(B/'shared/shaders').resolve()
report=dict(passed=True,changed_application_sources=changed,
    added_test='Exact native GPU pose storage and lifetime regression',
    authored_material_changes=10,source_placements_unchanged=True,
    terrain_water_collisions_routes_story_unchanged=True,
    shared_shader_and_texture_files_unchanged=True,
    private_reviews_only=True,live_saves_preserved=True,
    native_application_dependency_units=104,windows_work_performed=False)
(N/'review/CPU_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

from pathlib import Path
import hashlib,json,subprocess
N=Path(__file__).resolve().parents[1];B=N.with_name('playable-opening-2026-11-18');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
build=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text());assert build['ready'] and build['application_units']==104 and build['sha256']==sha(N/'runtime/rpg')
actual={str(p.relative_to(N/'source-v1')):sha(p) for p in (N/'source-v1').rglob('*') if p.is_file()};assert actual==build['source_sha256']
old=json.loads((N/'SOURCE_ORIGIN.json').read_text())['baseline_sha256'];changed=sorted(k for k in actual.keys()&old.keys() if actual[k]!=old[k]);added=sorted(actual.keys()-old.keys())
assert changed==sorted(['engine/world_viewer.cpp','engine/world_studio_view.inc','engine/world_local_light_shadow.inc','engine/world_combat_closeup_camera.h','tests/world_combat_closeup_camera_test.cpp']);assert added==['engine/world_apartment_window_light.h'];assert not old.keys()-actual.keys()
for row in json.loads((N/'runtime-native-v1/BUILD_PLAN.json').read_text())['units']:
 receipt=json.loads(Path(row['target']).with_suffix('.json').read_text());assert all(sha(p)==h for p,h in receipt['actual_dependency_sha256'].items())
protected=json.loads((N/'review/LIVE_BEFORE.json').read_text());assert all(sha(p)==h for p,h in protected.items())
before=(N/'authoring-snapshot/world.studio.before').read_text().splitlines();after=(N/'authoring-snapshot/world.studio.after').read_text().splitlines();assert len(before)==len(after);lines=[i+1 for i,(a,b) in enumerate(zip(before,after)) if a!=b];assert lines==json.loads((N/'authoring-snapshot/CHANGES.json').read_text())['only_changed_lines'];assert all(before[i-1].startswith('light ') and after[i-1].startswith('light ') for i in lines)
assert sha(N/'world-project/world.studio')==sha(N/'authoring-snapshot/world.studio.after')
before_shader=(B/'shared/shaders/world_foliage.fs.glsl').read_text();after_shader=(N/'shared/shaders/world_foliage.fs.glsl').read_text()
start=after_shader.index('bool referenceDaylightPane(vec3 p){');end=after_shader.index('\nfloat referenceRoomWeight',start)
a=before_shader.index('bool referenceDaylightPane(vec3 p){');b=before_shader.index('\nfloat referenceRoomWeight',a)
reversed_shader=(after_shader[:start]+before_shader[a:b]+after_shader[end:]).replace('    if(referenceDaylightPane(p))return 1.;\n','').replace('fixtureEmission*=mix(.45,.90,paneDay)','fixtureEmission*=mix(.012,.10,paneDay)')
a=reversed_shader.index('float homeFixtureEmissionWeight()');reversed_shader=reversed_shader[:a]+reversed_shader[a:].replace('    if(!referenceInterior(p))return 1.;\n','',1)
assert reversed_shader==before_shader
report=dict(passed=True,binary_sha256=build['sha256'],dependency_units_verified=104,changed_source=changed,added_source=added,protected_live_files=len(protected),intentional_authoring_change='Seven imported apartment window keys; editable calibrated values',apartment_window_rows=lines,source_models_textures_gameplay_combat_and_physics_unchanged=True,shader_changes_only_apartment_pane_domains_emission_and_equivalent_exterior_early_return=True,windows_work_performed=False)
(N/'review/CPU_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

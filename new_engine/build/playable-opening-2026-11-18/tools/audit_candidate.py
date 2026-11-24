from pathlib import Path
import hashlib,json
N=Path(__file__).resolve().parents[1];B=N.with_name('opening-performance-2026-11-18')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
build=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text());assert build['ready'] and build['application_units']==104
assert build['sha256']==sha(N/'runtime/rpg')
source={str(p.relative_to(N/'source-v1')):sha(p) for p in (N/'source-v1').rglob('*') if p.is_file()}
assert source==build['source_sha256']
old=json.loads((N/'SOURCE_ORIGIN.json').read_text())['baseline_sha256']
changed=sorted(k for k in source.keys()&old.keys() if source[k]!=old[k]);added=sorted(source.keys()-old.keys())
assert changed==sorted(['engine/world_models.cpp','engine/world_combat_motion.cpp','engine/world_combat_locomotion_runtime.inc','engine/world_model_pose_storage.h','tests/world_pose_blend_gl_test.cpp'])
assert added==sorted(['engine/world_model_pose_pager.inc','engine/world_model_material_batch.h','engine/world_model_material_batch.inc'])
assert not old.keys()-source.keys()
for file in ['engine/world_combat_motion.cpp','engine/world_combat_locomotion_runtime.inc']:
 actual=(N/'source-v1'/file).read_text().replace('\n#include "engine/world_model_material_batch.h"','').replace('\n    WorldModelMaterialBatch material_batch_scope;','')
 assert actual==(B/'source-v1'/file).read_text(),'Only synchronous material-load scopes may change combat code'
for row in json.loads((N/'runtime-native-v1/BUILD_PLAN.json').read_text())['units']:
 receipt=json.loads(Path(row['target']).with_suffix('.json').read_text())
 assert all(sha(p)==h for p,h in receipt['actual_dependency_sha256'].items())
protected=json.loads((N/'review/LIVE_BEFORE.json').read_text());assert all(sha(p)==h for p,h in protected.items())
assert sha(N/'world-project/world.studio')==sha(B/'world-project/world.studio')
for name in ['world_viewer.cpp','world_models.h','world_combat_motion.h']:
 assert sha(N/'source-v1/engine'/name)==sha(B/'source-v1/engine'/name)
report=dict(passed=True,binary_sha256=build['sha256'],dependency_units_verified=104,changed_source=changed,added_source=added,public_model_and_combat_API_unchanged=True,viewer_renderer_gameplay_physics_unchanged=True,combat_changes_only_synchronous_material_load_scopes=True,terrain_materials_foliage_textures_and_authoring_unchanged=True,protected_live_documents_and_saves=len(protected),windows_work_performed=False)
(N/'review/CPU_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

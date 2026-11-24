"""Verify the focused opening against its immutable, validated hero baseline."""
from pathlib import Path
import hashlib,json
N=Path(__file__).resolve().parents[1];B=N.with_name('opening-focus-2026-11-18')
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def inv(root):return {str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file()}
def need(x,m):
 if not x:raise RuntimeError(m)
def main():
 expected=json.loads((N/'review/REVIEWED_INPUTS.json').read_text());old=inv(B/'source-v1');now=inv(N/'source-v1')
 need(not old.keys()-now.keys(),'Source files removed')
 delta={k:v for k,v in now.items() if old.get(k)!=v};need(delta==expected['source_changes'],'Unreviewed source change')
 build=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text());need(build['ready'] and build['application_units']==104,'Incomplete native closure')
 need(build['sha256']==sha(N/'runtime/rpg') and build['source_sha256']==now,'Binary/source mismatch')
 for row in json.loads((N/'runtime-native-v1/BUILD_PLAN.json').read_text())['units']:
  r=json.loads(Path(row['target']).with_suffix('.json').read_text());need(r['command']==row['command'],'Object command changed')
  for p,h in r['actual_dependency_sha256'].items():need(sha(p)==h,'Compiled dependency changed '+p)
 for p,h in expected['candidate_files'].items():need(sha(N/p)==h,'Candidate file changed '+p)
 for name in ('HUMAN_WALK_CPU_REVIEW.json','attack-arc-final/EXECUTION.json'):
  r=json.loads((N/'review'/name).read_text());need(r['passed'],'CPU feature check failed '+name)
  inputs=r.get('source_and_asset_sha256',{}) if name=='HUMAN_WALK_CPU_REVIEW.json' else dict(r['source_sha256'],**r['asset_sha256'])
  need(bool(inputs),'Missing CPU feature inputs '+name)
  for p,h in inputs.items():need(sha(Path(p))==h,'CPU feature input changed '+p)
 motion=inv(N/'motion');need(motion==expected.get('motion_files',{}),'Unreviewed motion asset change')
 patch=json.loads((N/'review/scenery/ROW_PATCH.json').read_text())
 scenery=json.loads((N/'review/scenery/CPU_REVIEW.json').read_text());need(scenery['passed'],'Scenery CPU review failed')
 need(scenery['patch_sha256']==sha(N/'review/scenery/ROW_PATCH.json'),'Scenery patch review changed')
 for f in patch['files']:need(sha(N/'world-project'/f['path'])==f['after_sha256'],'Candidate scenery changed '+f['path'])
 protected=json.loads((N/'review/LIVE_BEFORE.json').read_text())
 for p,h in protected.items():need(sha(p)==h,'Live authoring/progress changed '+p)
 for name in ('CAMERA_CPU.json','EXPLORATION_CAMERA_CPU.json'):
  r=json.loads((N/'review'/name).read_text());need(r['passed'],'CPU camera check failed '+name)
  if name=='CAMERA_CPU.json':
   for test in r['tests'].values():
    for p,h in test['dependency_sha256'].items():need(sha(N/'source-v1'/p)==h,'Camera dependency changed '+p)
  else:
   for p,h in r['source_sha256'].items():need(sha(N/'source-v1'/p)==h,'Framing dependency changed '+p)
 need((N/'assets').resolve()==(B/'assets').resolve(),'Unexpected character assets')
 need((N/'shared/shaders').resolve()==(B/'shared/shaders').resolve(),'Unexpected shader changes')
 return dict(passed=True,binary_sha256=build['sha256'],application_units=104,source_changes=sorted(delta),protected_live_files=len(protected),live_authoring_and_progress_unchanged_before_promotion=True,reviewed_candidate_files=len(expected['candidate_files']),reviewed_motion_files=len(motion),reviewed_scenery_files=len(patch['files']),inherited_character_assets_unchanged=True,shader_changes=False,native_only=True,Windows_work_performed=False)
if __name__=='__main__':
 try:r=main()
 except Exception as e:r=dict(passed=False,error=str(e))
 (N/'review/CPU_REVIEW.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(0 if r['passed'] else 1)

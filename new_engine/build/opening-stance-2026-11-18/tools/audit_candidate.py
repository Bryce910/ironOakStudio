"""Verify source closure, full-body motion evidence, and untouched live data."""
from pathlib import Path
import hashlib,json
N=Path(__file__).resolve().parents[1];B=N.with_name('opening-polish-2026-11-18')
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def ignored_cache(path):
 p=Path(path)
 return '__pycache__' in p.parts and p.suffix in ('.pyc','.pyo')
def source_map(values):return {p:h for p,h in values.items() if not ignored_cache(p)}
def inv(root):return {str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file() and not ignored_cache(p.relative_to(root))}
def need(x,m):
 if not x:raise RuntimeError(m)
def feature(name):
 r=json.loads((N/'review'/name).read_text());need(r['passed'],'Feature check failed '+name)
 inputs=r.get('source_and_asset_sha256',{}) or dict(r.get('source_sha256',{}),**r.get('asset_sha256',{}))
 need(bool(inputs),'Missing feature inputs '+name)
 for p,h in inputs.items():need(sha(p)==h,'Feature input changed '+p)
def main():
 expected=json.loads((N/'review/REVIEWED_INPUTS.json').read_text());old=inv(B/'source-v1');now=inv(N/'source-v1')
 need(not old.keys()-now.keys(),'Source files removed')
 delta={k:v for k,v in now.items() if old.get(k)!=v};need(delta==expected['source_changes'],'Unreviewed source change')
 build=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text());need(build['ready'] and build['application_units']==104,'Incomplete native closure')
 need(build['sha256']==sha(N/'runtime/rpg') and source_map(build['source_sha256'])==now,'Binary/source mismatch')
 for row in json.loads((N/'runtime-native-v1/BUILD_PLAN.json').read_text())['units']:
  r=json.loads(Path(row['target']).with_suffix('.json').read_text());need(r['command']==row['command'],'Object command changed')
  for p,h in r['actual_dependency_sha256'].items():need(sha(p)==h,'Compiled dependency changed '+p)
 for p,h in expected['candidate_files'].items():need(sha(N/p)==h,'Candidate tool changed '+p)
 feature('STANCE_CPU_REVIEW.json');feature('attack-arc-final/EXECUTION.json')
 motion=inv(N/'motion');need(motion==expected['motion_files'],'Unreviewed motion files')
 protected=json.loads((N/'review/LIVE_BEFORE.json').read_text())
 for p,h in protected.items():need(sha(p)==h,'Live authoring/progress changed '+p)
 for name in ('CAMERA_CPU.json','EXPLORATION_CAMERA_CPU.json'):
  r=json.loads((N/'review'/name).read_text());need(r['passed'],'Inherited camera check failed')
  if name=='CAMERA_CPU.json':
   for test in r['tests'].values():
    for p,h in test['dependency_sha256'].items():need(sha(N/'source-v1'/p)==h,'Camera dependency changed '+p)
  else:
   for p,h in r['source_sha256'].items():need(sha(N/'source-v1'/p)==h,'Camera dependency changed '+p)
 for name in ('world.studio','scenery/bep_environment_instances.txt','scenery/demo_polish_instances.txt'):
  need(sha(N/'world-project'/name)==sha(B/'world-project'/name),'Unreviewed scene edit '+name)
 need((N/'assets').resolve()==(B/'assets').resolve(),'Inherited body assets changed')
 need((N/'shared/shaders').resolve()==(B/'shared/shaders').resolve(),'Inherited shaders changed')
 return dict(passed=True,binary_sha256=build['sha256'],application_units=104,source_changes=sorted(delta),ignored_build_cache_files=sorted(p for p in build['source_sha256'] if ignored_cache(p)),protected_live_files=len(protected),world_and_progress_unchanged=True,reviewed_candidate_files=len(expected['candidate_files']),reviewed_motion_files=len(motion),inherited_body_assets_unchanged=True,shader_changes=False,native_only=True,Windows_work_performed=False)
if __name__=='__main__':
 try:r=main()
 except Exception as e:r=dict(passed=False,error=str(e))
 (N/'review/CPU_REVIEW.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(0 if r['passed'] else 1)

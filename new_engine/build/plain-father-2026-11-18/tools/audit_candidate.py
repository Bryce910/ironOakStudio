"""Validate the reviewed Linux hero update without changing the working world."""
from pathlib import Path
import hashlib,json,os
N=Path(__file__).resolve().parents[1];B=N.with_name('opening-finish-2026-11-18')
def sha(p):
 with Path(p).open('rb') as h:return hashlib.file_digest(h,'sha256').hexdigest()
def inv(root):return {str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file()}
def need(x,m):
 if not x:raise RuntimeError(m)
def main():
 expected=json.loads((N/'review/REVIEWED_INPUTS.json').read_text())
 before=inv(B/'source-v1');after=inv(N/'source-v1')
 need(not before.keys()-after.keys(),'Source files removed')
 delta={k:v for k,v in after.items() if before.get(k)!=v}
 need(delta==expected['source_changes'],'Source differs from reviewed changes')
 build=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text())
 need(build['ready'] and build['application_units']==104,'Incomplete native closure')
 need(build['sha256']==sha(N/'runtime/rpg'),'Binary changed')
 need(build['source_sha256']==after,'Build source inventory differs')
 plan=json.loads((N/'runtime-native-v1/BUILD_PLAN.json').read_text())
 for row in plan['units']:
  receipt=json.loads(Path(row['target']).with_suffix('.json').read_text())
  need(receipt['command']==row['command'],'Object command changed')
  for path,digest in receipt['actual_dependency_sha256'].items():need(sha(path)==digest,'Compiled input changed '+path)
 for path,digest in expected['candidate_files'].items():need(sha(N/path)==digest,'Candidate asset/tool changed '+path)
 protected=json.loads((N/'review/LIVE_BEFORE.json').read_text())
 for path,digest in protected.items():need(sha(path)==digest,'Working authoring/progress changed '+path)
 for path in expected['cpu_reviews']:
  need(json.loads((N/path).read_text())['passed'],'CPU check failed '+path)
 compose=json.loads((N/'review/hero-compose/EXECUTION.json').read_text())
 for path,digest in compose['asset_sha256'].items():need(sha(path)==digest,'Composition evidence stale '+path)
 need((N/'shared/shaders').resolve()==(B/'shared/shaders').resolve(),'Unexpected shader change')
 return dict(passed=True,binary_sha256=build['sha256'],application_units=104,source_changes=sorted(delta),protected_live_files=len(protected),working_authoring_and_progress_unchanged=True,reviewed_candidate_files=len(expected['candidate_files']),native_only=True,Windows_work_performed=False)
if __name__=='__main__':
 try:r=main()
 except Exception as e:r=dict(passed=False,error=str(e))
 (N/'review/CPU_REVIEW.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(0 if r['passed'] else 1)

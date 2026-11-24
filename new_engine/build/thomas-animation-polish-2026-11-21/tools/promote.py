"""Activate the reviewed application without moving or replacing user saves."""
from pathlib import Path
import datetime,json,shutil,sys
N=Path(__file__).resolve().parents[1];ROOT=N.parents[2];P=N.with_name('thomas-hair-coverage-2026-11-21')
sys.dont_write_bytecode=True;sys.path.insert(0,str(N))
from check_unified import check,need,read,sha
ready=check();review=read(N/'review/FINAL_REVIEW.json')
need(review.get('passed') is True and review.get('visual_review_performed') is True and review['ready_sha256']==sha(N/'UNIFIED_READY.json'),'Animation review is incomplete or stale')
for p,h in review['evidence_sha256'].items():need(sha(p)==h,'Animation evidence changed: '+p)
manifest=ROOT/'CURRENT_VALWOOD.json';prior=read(manifest)
need(prior['runtime_generation']==str(P),'Active generation changed during animation review')
rollback=N/'rollback/CURRENT_VALWOOD.json';need(not rollback.exists(),'Animation update already published')
rollback.parent.mkdir(exist_ok=True);shutil.copy2(manifest,rollback)
updated=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),guide=str(N/'GUIDE.txt'),binary_sha256=ready['binary_sha256'])
t=manifest.with_suffix('.json.tmp');t.write_text(json.dumps(updated,indent=2)+'\n');t.replace(manifest)
(N/'PUBLICATION.json').write_text(json.dumps(dict(published=True,canonical_state_files_modified=False,rollback_manifest=str(rollback),ready_sha256=sha(N/'UNIFIED_READY.json'),final_review_sha256=sha(N/'review/FINAL_REVIEW.json'),published_utc=datetime.datetime.now(datetime.timezone.utc).isoformat()),indent=2)+'\n')
print('Published Thomas animation update')

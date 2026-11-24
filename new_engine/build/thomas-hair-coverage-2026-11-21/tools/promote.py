from pathlib import Path
import datetime,json,shutil,sys
N=Path(__file__).resolve().parents[1];ROOT=N.parents[2];sys.dont_write_bytecode=True;sys.path.insert(0,str(N))
from check_unified import check,need,read,sha
ready=check();review=read(N/'review/FINAL_REVIEW.json');need(review.get('passed') is True and review.get('visual_review_performed') is True and review['ready_sha256']==sha(N/'UNIFIED_READY.json'),'Hair review is not bound to the frozen patch')
for p,h in review['evidence_sha256'].items():need(sha(p)==h,'Hair review evidence changed: '+p)
manifest=ROOT/'CURRENT_VALWOOD.json';prior=read(manifest);need(prior['runtime_generation']==str(N.with_name('thomas-character-finish-2026-11-21')),'Active generation changed')
rollback=N/'rollback/CURRENT_VALWOOD.json';need(not rollback.exists(),'Hair patch already published');rollback.parent.mkdir(exist_ok=True);shutil.copy2(manifest,rollback)
updated=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),shared_shaders=str(N/'shared/shaders'),guide=str(N/'GUIDE.txt'),binary_sha256=ready['binary_sha256'])
t=manifest.with_suffix('.json.tmp');t.write_text(json.dumps(updated,indent=2)+'\n');t.replace(manifest)
(N/'PUBLICATION.json').write_text(json.dumps(dict(published=True,binary_reused=True,canonical_state_files_modified=False,rollback_manifest=str(rollback),ready_sha256=sha(N/'UNIFIED_READY.json'),final_review_sha256=sha(N/'review/FINAL_REVIEW.json'),published_utc=datetime.datetime.now(datetime.timezone.utc).isoformat()),indent=2)+'\n');print('Published Thomas hair coverage fix')

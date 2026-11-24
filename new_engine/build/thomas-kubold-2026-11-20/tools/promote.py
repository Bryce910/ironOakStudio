"""Publish the reviewed Thomas generation through the stable manifest."""
from pathlib import Path
import datetime
import json
import shutil
import sys

N = Path(__file__).resolve().parents[1]
E = N.parents[1]
ROOT = E.parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(N))
from check_unified import check, need, read, sha

ready = check()
review_path = N/'review/FINAL_REVIEW.json'
review = read(review_path)
need(review.get('passed') is True and review.get('visual_review_performed') is True and
     review.get('ready_sha256') == sha(N/'UNIFIED_READY.json'),
     'Final review must approve the frozen Thomas character and its captured motion')
evidence = review.get('evidence_sha256', {})
need(bool(evidence), 'Final review requires explicit CPU/GPU evidence')
for path, expected in evidence.items():
    need(Path(path).is_file() and sha(path) == expected, 'Final review evidence changed: '+path)
manifest = ROOT/'CURRENT_VALWOOD.json'
prior = read(manifest)
need(prior['runtime_generation'] == str(N.with_name('camp-atmosphere-2026-11-20')),
     'Active runtime changed during character integration; review the new baseline')
rollback = N/'rollback'
rollback.mkdir(exist_ok=True)
backup = rollback/'CURRENT_VALWOOD.json'
need(not backup.exists(), 'This generation has already been published')
shutil.copy2(manifest, backup)
updated = dict(prior, runtime_generation=str(N), binary=str(N/'runtime/rpg'),
               source=str(N/'source-v1'), shared_shaders=str(N/'shared/shaders'),
               character_textures=str(N/'shared/character-textures'), guide=str(N/'GUIDE.txt'),
               binary_sha256=ready['binary_sha256'])
temporary = manifest.with_suffix('.json.tmp')
temporary.write_text(json.dumps(updated, indent=2)+'\n')
temporary.replace(manifest)
(N/'PUBLICATION.json').write_text(json.dumps(dict(
    schema='valwood.thomas-kubold.publication.v1', published=True,
    published_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    manifest=str(manifest), rollback=str(backup), ready_sha256=sha(N/'UNIFIED_READY.json'),
    final_review_sha256=sha(review_path), binary_reused=True, source_reused=True), indent=2)+'\n')
print('Published Thomas/Kubold:', N)

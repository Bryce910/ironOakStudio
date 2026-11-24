#!/usr/bin/env python3
"""Retain the source-scene flags required by the accepted opening, only in NEW."""
from pathlib import Path
import hashlib,json,shutil
N=Path(__file__).resolve().parent;R=N/'review';P=N/'story-project';B=R/'story-data-before-repair'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
rp=R/'STORY_PROJECT_REPAIR_REVIEW.json';r=json.loads(rp.read_text());r['inputs'][str(Path(__file__).resolve())]=sha(__file__)
for name in ['city_showcase.json','publisher_showcase.json']:
 source=B/name;target=P/name
 assert source.is_file() and not target.exists()
 shutil.copy2(source,target);r['outputs'][str(target)]=sha(target)
 r['changes'].append({'path':str(target),'after_sha256':sha(target),'action':'restored_before_native_retry','reason':'Known accepted opening retains both flags; publisher flag activates source-authored arrival camp flame at -11988/5172. Generic castle town_enabled is independent of first-journey Seldonne NPCs.'})
r['showcase_city_enabled']=r['showcase_publisher_enabled']=True
r['mode_policy']='Preserve known accepted story source-scene flags and authored look. No runtime attempt occurred while those flags were absent.'
r['showcase_evidence']='/home/computer/Desktop/final_engine/new_engine/docs/combat-reaction-polish/2026-11-10/qa-first-v2/project'
rp.write_text(json.dumps(r,indent=2,sort_keys=True)+'\n')
print('PASS restored source-scene mode sentinels; original bytes retained')

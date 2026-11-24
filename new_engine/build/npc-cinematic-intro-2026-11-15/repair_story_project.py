#!/usr/bin/env python3
"""Restore the runtime apartment representation inside the independent story copy only."""
from pathlib import Path
import hashlib,json,shutil
N=Path(__file__).resolve().parent
P=N/'story-project'
CANON=N.parents[1]/'projects/valwood_design'
RECOVERY=N.parents[0]/'apartment-selectable-2026-11-11/staging-v1/before-install'
R=N/'review'; BACK=R/'story-data-before-repair'

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def must(v,m):
    if not v:raise RuntimeError(m)
def apartment(row,doc):
    s=row.split()
    if not s:return False
    return (s[0]=='asset' and s[2].startswith('opening/')) if doc else s[0].startswith(('nm_opening_home','nm_opening_reference_'))

def main():
    must(not BACK.exists(),'Repair is one-shot; existing receipt/backups must be preserved')
    BACK.mkdir()
    canonical={};inputs={str(Path(__file__).resolve()):sha(__file__)};outputs={};parity={};changes=[]
    for rel,oldname,doc in [('world.studio','world.studio',True),('scenery/authored_instances.txt','authored_instances.txt',False)]:
        target=P/rel; original=RECOVERY/oldname
        must(target.is_file() and not target.is_symlink() and original.is_file(),'Missing regular recovery data')
        current=target.read_text().splitlines();recovered=original.read_text().splitlines()
        kept_current=[s for s in current if not apartment(s,doc)];kept_recovery=[s for s in recovered if not apartment(s,doc)]
        must(kept_current==kept_recovery,'Recovery changes a non-apartment source row: '+rel)
        parity[rel]={'non_apartment_rows':len(kept_current),'ordered_rows_byte_equal':True,
                     'old_apartment_rows':sum(apartment(s,doc) for s in current),'runtime_apartment_rows':sum(apartment(s,doc) for s in recovered)}
        dst=BACK/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,dst)
        inputs[str(original)]=sha(original);inputs[str(dst)]=sha(dst)
        canonical[str(CANON/rel)]=sha(CANON/rel)
        changes.append({'path':str(target),'before_sha256':sha(target),'after_sha256':sha(original),'recovered_from':str(original)})
        shutil.copy2(original,target);outputs[str(target)]=sha(target)
    for name in ['city_showcase.json','publisher_showcase.json']:
        target=P/name;must(target.is_file() and not target.is_symlink(),'Missing copied showcase flag')
        canonical[str(CANON/name)]=sha(CANON/name)
        shutil.copy2(target,BACK/name);inputs[str(BACK/name)]=sha(BACK/name)
        changes.append({'path':str(target),'before_sha256':sha(target),'after':'absent','reason':'Editor/showcase opt-in conflicts with ordinary story/town runtime'})
        target.unlink()
    for p,d in canonical.items():must(sha(p)==d,'Canonical user metadata changed during repair')
    must(sum(s.startswith('asset ') for s in (P/'world.studio').read_text().splitlines())<=128,'Recovery still exceeds runtime asset budget')
    receipt={'schema':'valwood.first-contact.story-project-repair.v1','passed':True,'repair_applied':True,
             'reason':'The copied editor document had642 aliases, exceeding the accepted runtime128 limit; its all-or-nothing parse left lights/aliases unavailable. Runtime apartment aggregate metadata contains all source furniture without editor-only individual aliases.',
             'non_apartment_parity':parity,'changes':changes,'canonical_metadata_untouched':canonical,
             'source_geometry_modified':False,'engine_budgets_modified':False,'practical_rows_preserved':True,
             'showcase_city_enabled':False,'showcase_publisher_enabled':False,'inputs':inputs,'outputs':outputs,
             'acceptance':'Data repair only. Full production CPU scene/light/catalog preflight is separate; native/camera acceptance remains pending.'}
    (R/'STORY_PROJECT_REPAIR_REVIEW.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    print('PASS isolated metadata repair with exact non-apartment row parity')
if __name__=='__main__':main()

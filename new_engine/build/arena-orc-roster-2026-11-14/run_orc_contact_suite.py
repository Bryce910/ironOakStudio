#!/usr/bin/env python3
"""Real four-loadout suite after explicit body/semantic/anatomy coherence checks."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;C=N/'roster-contact-review';R=N/'arena_orcs_roster';P=N/'roster-review'
IDS={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'}
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,d,h=None):
 p=Path(p).resolve();v=sha(p);require(h is None or h==v,'Stale bank/proof dependency: '+str(p));d[str(p)]=v;return v
def current(doc):
 try:
  for field in ('before_sha256','after_sha256','inputs','outputs_sha256'):
   for p,h in doc.get(field,{}).items():require(sha(p)==h,'Stale case')
  return True
 except (OSError,RuntimeError):return False

def main():
 parser=argparse.ArgumentParser();parser.add_argument('character',choices=sorted(IDS));args=parser.parse_args();identity=args.character;bank=R/identity;inputs={};pin(Path(__file__),inputs)
 body=bank/'body/combat_motion_manifest.json';ready=bank/'anatomy/ANATOMY_READY.json';exchange=bank/'body-semantic.npz';pin(body,inputs);pin(ready,inputs);pin(exchange,inputs);motion=json.loads(body.read_text());anatomy=json.loads(ready.read_text());require(anatomy.get('passed')is True,'Anatomy cook has not passed')
 expected={}
 for field in ('inputs','dependencies_sha256','outputs_sha256'):
  for p,h in anatomy.get(field,{}).items():pin(p,inputs,h);expected[str(Path(p).resolve())]=h
 require(sha(body)in expected.values()and sha(exchange)in expected.values(),'Anatomy receipt does not pin this exact current pose manifest+semantic exchange')
 require(motion['semantic_exchange_sha256']==sha(exchange),'Body semantic exchange does not match baked body')
 pin(bank/'anatomy/anatomy.bin',inputs);pin(bank/'body/combat_weapon_sockets.bin',inputs);pin(bank/'body/combat_weapon_pivots.bin',inputs);pin(bank/'body/combat_presentation.cfg',inputs)
 actual_metadata=sha(bank/'anatomy/anatomy.bin');require(actual_metadata in expected.values(),'Anatomy producer does not pin exact current runtime metadata')
 folder=P/'characters'/identity;folder.mkdir(parents=True,exist_ok=True);coherent=folder/'BANK_COHERENCE_REVIEW.json'
 coherent.write_text(json.dumps(dict(schema='valwood.orc-roster.body-anatomy-coherence.v1',passed=True,character_id=identity,inputs=inputs,exact_body_manifest_semantic_exchange_and_metadata=True),indent=2,sort_keys=True)+'\n')
 cases=[];outputs={}
 for case in ('sword','shield','axe','staff'):
  path=C/identity/case/'ORC_CONTACT_REVIEW.json';old=json.loads(path.read_text())if path.is_file()else{}
  if not(old.get('passed')and current(old)):
   subprocess.run([sys.executable,str(C/'run_roster_contacts.py'),'--reuse-build','--character',identity,'--case',case],check=True)
  doc=json.loads(path.read_text());require(doc['passed']and current(doc),'Real contact case failed: '+identity+'/'+case);pin(path,inputs)
  for p,h in doc['before_sha256'].items():pin(p,inputs,h)
  for p,h in doc['outputs_sha256'].items():pin(p,outputs,h)
  cases.append(dict(case=case,review=str(path),sha256=sha(path)))
 reach=C/identity/'OUTGOING_REACH_REVIEW.json';old=json.loads(reach.read_text())if reach.is_file()else{}
 if not(old.get('passed')and current(old)):
  # Source fixture already compiled for the immutable Gnur control. Require
  # that exact executable/source closure before reuse, rather than trusting a
  # bare --reuse-build which would not itself pin compilation provenance.
  control=C/'gnur/OUTGOING_REACH_REVIEW.json';control_doc=json.loads(control.read_text());require(control_doc.get('passed')and current(control_doc),'Outgoing fixture compilation control is stale')
  subprocess.run([sys.executable,str(C/'run_roster_outgoing_reach.py'),identity,'--reuse-build'],check=True)
 doc=json.loads(reach.read_text());require(doc['passed']and current(doc),'Actual source outgoing reach failed');pin(reach,inputs)
 for p,h in doc['inputs'].items():pin(p,inputs,h)
 pin(coherent,inputs);require(inputs=={p:sha(p)for p in inputs},'Bank changed during actual contact suite')
 target=folder/'CHARACTER_BATTLE_CONTACT_REVIEW.json';target.write_text(json.dumps(dict(schema='valwood.orc-roster.coherent-character-contact-suite.v1',passed=True,character_id=identity,inputs=inputs,outputs_sha256=outputs,cases=cases,outgoing_review=str(reach),four_loadouts_normal_and_combo=True,all_five_directions_body_and_actual_steel=True,source_geometry_and_anatomy_coherent=True,whole_roster_acceptance=False),indent=2,sort_keys=True)+'\n');print('PASS coherent actual four-loadout/source-weapon suite',identity)
if __name__=='__main__':main()

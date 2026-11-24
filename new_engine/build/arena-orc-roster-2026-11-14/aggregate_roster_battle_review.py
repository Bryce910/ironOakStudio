#!/usr/bin/env python3
"""Strict all-character real four-hero-loadout contact and source-sever aggregate."""
from pathlib import Path
import hashlib,json,re,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;P=N/'roster-review';C=N/'roster-contact-review';R=N/'arena_orcs_roster';IDS={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'}
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,d,h=None):
 p=Path(p).resolve();v=sha(p);require(h is None or h==v,'Changed dependency: '+str(p));d[str(p)]=v;return v
def main():
 inputs={};outputs={};pin(Path(__file__),inputs);roster_path=R/'ORC_ROSTER.json';pin(roster_path,inputs);roster=json.loads(roster_path.read_text());entries=roster['entries'];require(len(entries)==14 and {r['id']for r in entries}==IDS,'Runtime roster differs')
 characters=[]
 for entry in entries:
  identity=entry['id'];manifest=R/entry['asset_manifest'];pin(manifest,inputs,entry['asset_manifest_sha256']);asset=json.loads(manifest.read_text());rigid_count=len(asset['rigid_weapon_primitives']);cases=[]
  suite_path=P/'characters'/identity/'CHARACTER_BATTLE_CONTACT_REVIEW.json';pin(suite_path,inputs);suite=json.loads(suite_path.read_text());require(suite['passed']and suite['character_id']==identity and suite['source_geometry_and_anatomy_coherent'],'Exact current body/semantic/anatomy contact-suite coherence proof required')
  for p,h in suite['inputs'].items():pin(p,inputs,h)
  for p,h in suite['outputs_sha256'].items():pin(p,outputs,h)
  callbacks=samples=queries=0;maximum=0.
  for case in ('sword','shield','axe','staff'):
   path=C/identity/case/'ORC_CONTACT_REVIEW.json';pin(path,inputs);doc=json.loads(path.read_text());require(doc['passed']and doc['character_id']==identity and doc['case']==case and doc['actual_purchased_orc_anatomy']and doc['actual_indexed_weapon_rigid_geometry'],'Fresh genuine four-loadout contacts missing: '+identity+'/'+case)
   require(doc['before_sha256']==doc['after_sha256'],'Contact source/bank changed during run')
   for p,h in doc['before_sha256'].items():pin(p,inputs,h)
   for p,h in doc['outputs_sha256'].items():pin(p,outputs,h)
   require(set(doc['reports'])=={'normal','combo'},'Both normal and chained swings required')
   for mode,raw in doc['reports'].items():
    require(raw['passed']and raw['returncode']==0 and raw['real_training_body_callbacks']==5 and raw['pure_pre_render_queries']and raw['all_directions_have_body_and_matching_steel_contacts']and raw['guard_endpoint_samples']==60 and raw['guard_endpoint_max_error_m']<1e-6 and raw['grid_checks']==210,'Actual direction/contact/purity proof failed')
    directions=raw['directions'];require(len(directions)==5 and {d['direction']for d in directions}==set(range(5))and all(d['body_callback_at_125cm']and d['matching_steel_distances_cm']for d in directions),'All5directions need actual body callbacks and physical weapon interceptions')
    stderr=path.parent/(mode+'.stderr.log');pin(stderr,outputs);text=stderr.read_text();certs=re.findall(r'Actual Orc axe: (\d+) source mesh/pair certifications, maximum endpoint error([0-9.eE+\-]+)m',text);require(certs and any(int(n)==217*rigid_count and float(error)<=.002 for n,error in certs),'Every actual rigid primitive must certify all217pairs within2mm')
    for suffix in ('.raw.json','.json'):pin(path.parent/(mode+suffix),outputs)
    callbacks+=raw['real_training_body_callbacks'];samples+=raw['guard_endpoint_samples'];queries+=raw['grid_checks'];maximum=max(maximum,raw['guard_endpoint_max_error_m'])
   cases.append(dict(case=case,review=str(path),sha256=sha(path),passed=True))
  outgoing=C/identity/'OUTGOING_REACH_REVIEW.json';pin(outgoing,inputs);out=json.loads(outgoing.read_text());require(out['passed']and out['character_id']==identity and out['actual_indexed_weapon_geometry']and out['actual_human_capsules']and out['all_five_directions_reach_actual_human_body']and len(out['directions'])==5 and all(d['hits']for d in out['directions']),'Actual selected source weapon must reach Human anatomy in all5directions')
  for p,h in out['inputs'].items():pin(p,inputs,h)
  for name in ('OPPONENT_BANK_CPU_REVIEW.json','SOURCE_PROFILE_SEVER_REVIEW.json'):
   path=P/'characters'/identity/name;pin(path,inputs);doc=json.loads(path.read_text());require(doc['passed']and doc['character_id']==identity,'Fresh source/routing/sever CPU proof missing')
   for p,h in doc['inputs'].items():pin(p,inputs,h)
   for p,h in doc['outputs_sha256'].items():pin(p,outputs,h)
  require(callbacks==40 and samples==480 and queries==1680,'Full normal/combo four-loadout physical coverage differs')
  characters.append(dict(id=identity,cases=cases,ordinary_body_callbacks=callbacks,physical_guard_endpoint_samples=samples,physical_contact_grid_queries=queries,guard_endpoint_max_error_m=maximum,actual_rigid_primitives=rigid_count,certified_pairs_per_primitive=217,actual_outgoing_directions=5,eight_source_sever_regions=True,right_hand_survives_with_left_handover=True,both_hands_disable_combat=True))
 report=dict(schema='valwood.orc-roster.battle-review.v1',passed=True,expected_ids=sorted(IDS),reviewed_ids=sorted(r['id']for r in characters),inputs=inputs,outputs_sha256=outputs,characters=characters,total_ordinary_body_callbacks=14*40,total_physical_grid_queries=14*1680,all_four_human_loadouts_normal_and_combo=True,all_five_body_and_physical_weapon_contacts=True,all_actual_source_outgoing_weapons=True,all_217_pairs_per_true_rigid_primitive_certified_within_2mm=True,all_actual_eight_region_sever_rules_and_profiles=True,
  controller_feel_accepted=False,hardware_performance_accepted=False,
  limits=['Production CPU geometry, real Human hero rigs and each actual owned source weapon/anatomy are used. Steel acceptance requires genuine indexed geometry interceptions, not simulated random misses or larger fake colliders.',
  'Centered1.25m ordinary body callbacks and0.8–1.8m physical steel grids verify deterministic reach. Native visuals/resource checks and interactive controller feel remain separate. One character is selected per bout; no simultaneous mixed-character streaming.'])
 target=P/'ROSTER_BATTLE_REVIEW.json';target.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print('PASS actual14character physical battle/sever aggregate:',target)
if __name__=='__main__':main()

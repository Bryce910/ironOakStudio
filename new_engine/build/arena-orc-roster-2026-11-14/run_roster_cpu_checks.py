#!/usr/bin/env python3
"""Fresh actual per-character CPU contracts; never touches live saves or GPU."""
from pathlib import Path
import argparse,hashlib,json,shlex,struct,subprocess,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;P=N/'roster-review';S=N/'source-v1';R=N/'arena_orcs_roster'
IDS=('orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag')
CXX='/usr/bin/x86_64-linux-gnu-g++-13';FLAGS=['-std=c++17','-O2','-ffunction-sections','-fdata-sections','-I'+str(N),'-I'+str(S)]
UNITS={'metadata':S/'tests/world_combat_opponent_metadata_test.cpp','routing':P/'roster_contact_isolation.cpp','profile':P/'roster_profile.cpp','sever':P/'roster_source_sever.cpp'}
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,d,h=None):
 p=Path(p).resolve();v=sha(p);require(h is None or v==h,'Stale input: '+str(p));d[str(p)]=v;return v
def compile_all():
 paths={Path(__file__).resolve(),*UNITS.values(),S/'engine/world_training_combat.cpp'}
 for unit in [*UNITS.values(),S/'engine/world_training_combat.cpp']:
  dep=subprocess.check_output([CXX,*FLAGS,'-MM','-MT','input',str(unit)],text=True).replace('\\\n',' ')
  paths.update(Path(x).resolve()for x in shlex.split(dep.split(':',1)[1]))
 inputs={str(p.resolve()):sha(p)for p in sorted(paths)};commands=[];outputs={}
 for name,unit in UNITS.items():
  exe=P/('roster-'+name+'-test');cmd=[CXX,*FLAGS,str(unit)]
  if name in {'profile','sever'}:cmd.append(str(S/'engine/world_training_combat.cpp'))
  cmd+=['-Wl,--gc-sections','-o',str(exe)];commands.append(cmd)
  log=P/('roster-'+name+'-compile.log')
  with log.open('w')as f:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
  outputs[str(exe)]=sha(exe);outputs[str(log)]=sha(log)
 require(inputs=={p:sha(p)for p in inputs},'Production changed during CPU compile')
 doc=dict(schema='valwood.orc-roster.cpu-fixtures-build.v1',passed=True,inputs=inputs,outputs_sha256=outputs,commands=commands)
 target=P/'ROSTER_CPU_FIXTURE_BUILD.json';target.write_text(json.dumps(doc,indent=2,sort_keys=True)+'\n');return doc

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--character',choices=IDS);parser.add_argument('--build-only',action='store_true');args=parser.parse_args();P.mkdir(exist_ok=True)
 build_path=P/'ROSTER_CPU_FIXTURE_BUILD.json'
 if args.build_only or not build_path.is_file():build=compile_all()
 else:
  build=json.loads(build_path.read_text())
  for key in ('inputs','outputs_sha256'):
   for p,h in build[key].items():require(sha(p)==h,'CPU fixture build is stale: '+p)
 if args.build_only:print('PASS fresh roster CPU fixtures built against production source');return
 metadata_log=P/'roster-metadata-result.log';metadata_log.write_text(subprocess.check_output([str(P/'roster-metadata-test')],text=True))
 for identity in ([args.character]if args.character else IDS):
  bank=R/identity;manifest=bank/'ORC_ASSETS.json';doc=json.loads(manifest.read_text());inputs=dict(build['inputs']);pin(build_path,inputs);inputs.update(build['outputs_sha256']);pin(Path(__file__),inputs);pin(manifest,inputs)
  for path in (bank/'anatomy/anatomy.bin',bank/'body/combat_presentation.cfg',bank/'anatomy/ANATOMY_READY.json',N/'arena-project/models/longsword_mocap/combat_weapon_sockets.bin',N/'arena-project/models/longsword_mocap/combat_presentation.cfg'):pin(path,inputs)
  for key in ('source_to_cooked_uv_receipt','actual_source_to_cooked_uv','retarget_and_layout_receipt'):
   reference=doc.get(key)
   if not reference:continue
   pin(reference['path'],inputs,reference['sha256']);proof=json.loads(Path(reference['path']).read_text());require(proof.get('passed')is True,'Source proof did not pass')
   for closure in ('inputs','dependencies_sha256','outputs_sha256'):
    for p,h in proof.get(closure,{}).items():pin(p,inputs,h)
  magic,poses,meshes,parts=struct.unpack('<8sIII',(bank/'anatomy/anatomy.bin').read_bytes()[:20]);require(magic==b'VWANA002'and poses==172 and parts==8 and meshes>1,'Actual source metadata header invalid')
  weapons=doc['rigid_weapon_primitives'];require(len(weapons)>0 and len(set(weapons))==len(weapons)and all(isinstance(i,int)and 0<=i<meshes for i in weapons),'Invalid actual rigid primitive set')
  tokens=(bank/'body/combat_presentation.cfg').read_text().split();profile={k:float(tokens[tokens.index(k)+1])for k in ('walk_stride_m','body_radius_m','body_height_m')}
  folder=P/'characters'/identity;folder.mkdir(parents=True,exist_ok=True);outputs={str(metadata_log):sha(metadata_log)};commands=[];results={}
  for name,tail in [('routing',[str(N/'arena-project/models'),str(bank),str(meshes)]),('profile',[str(profile['body_radius_m']),str(profile['body_height_m']),str(profile['body_height_m']-.07)]),('sever',[str(bank/'anatomy/anatomy.bin'),str(meshes),','.join(map(str,weapons))])]:
   command=[str(P/('roster-'+name+'-test')),*tail];commands.append(command);raw=subprocess.check_output(command,text=True);log=folder/(name+'.raw.json');log.write_text(raw);outputs[str(log)]=sha(log);result=json.loads(raw);require(result['passed']is True,'Actual source CPU fixture failed');results[name]=result
  routing=results['routing'];sever=results['sever'];projectile=results['profile']
  require(routing['fallback_parities']==15 and routing['actual_control_hits']>0 and routing['wrong_or_missing_attacker_denials']==30 and routing['captured_edge_parities']==15 and routing['fresh_run_state_cleared']and routing['human_player_preserved']and routing['telemetry_preserved']and routing['measured_stride_sampling']and routing['invalid_profiles_rejected']==8 and routing['anatomy_meshes']==meshes,'Routing/reset contract failed')
  require(sever['actual_orc_source_regions']==8 and sever['ordinary_training_sever_callbacks']==11 and sever['right_hand_survival_and_left_handover']and sever['both_hands_disable_combat']and sever['thrust_lethal_without_sever'],'Sever rules contract failed')
  require(projectile['orc_hit']and projectile['invalid_profiles_rejected']==7 and projectile['eight_actor_pair_clearance_passed']and projectile['player_sweep_preserves_measured_margin']and abs(projectile['measured_movement_radius_with_margin_m']-profile['body_radius_m']-.04)<1e-8,'Measured body/projectile/movement-margin contract failed')
  require(inputs=={p:sha(p)for p in inputs},'Actual bank changed during CPU tests')
  common=dict(passed=True,character_id=identity,source_model=doc['source_model'],inputs=inputs,outputs_sha256=outputs,commands=commands,limits=['CPU probe/routing/profile validation uses actual new source anatomy. Genuine hero swing/steel reach and native GPU sever rendering are separate checks. No controller-feel or hardware-performance acceptance.'])
  first=dict(common,schema='valwood.orc-roster.character-bank.cpu.v1',separate_bank_contact=routing,metadata_malformed_payloads_rejected=13,production_motion_compiled=True)
  second=dict(common,schema='valwood.orc-roster.character-source-profile-sever.v1',actual_source_sever=sever,projectile_profile=projectile,measured_profile=profile)
  for filename,report in [('OPPONENT_BANK_CPU_REVIEW.json',first),('SOURCE_PROFILE_SEVER_REVIEW.json',second)]:
   (folder/filename).write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
  print('PASS fresh actual CPU routing/profile/8part sever',identity,flush=True)
if __name__=='__main__':main()

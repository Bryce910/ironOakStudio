#!/usr/bin/env python3
"""Validate the native rebound generation and inherited shared resources."""
from pathlib import Path
import argparse,hashlib,json,sys
N=Path(__file__).resolve().parent;E=N.parents[1];BASE=N.with_name('third-person-flow-2026-11-17');READY=N/'UNIFIED_READY.json'
NATIVE=('ordinary-duel','equipment')
REPORTS=('QUICK_STEP_REVIEW.json','NATIVE_DODGE_REVIEW.json','VISUAL_REVIEW.json','FINAL_REVIEW.json')
def sha(p):
 with Path(p).open('rb') as h:return hashlib.file_digest(h,'sha256').hexdigest()
def stamp(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def need(ok,message):
 if not ok:raise RuntimeError(message)
def resources():
 from launch import configuration,AREAS
 c={m:configuration(m) for m in ('demo','editor','arena','joust','world')}
 need(len({x[0][0] for x in c.values()})==1,'Modes use different executables')
 need(c['demo'][2]==c['editor'][2]==c['world'][2],'Working world split')
 for p in [x[2] for x in c.values()]+[p for p,_ in AREAS.values()]:
  need((p/'world.studio').is_file() and (p/'project.valwood').is_file(),'Missing authored scene '+str(p))
  need((p/'shaders').resolve()==(N/'shared/shaders').resolve(),'Shader bank differs')
  need((p/'character-textures').resolve()==(N/'shared/character-textures').resolve(),'Character textures differ')
  need((p/'saves').is_dir() and not (p/'saves').is_symlink(),'Invalid progress directory')
 return c
def verify_pins(d):
 for name,digest in d['pins'].items():
  s=stamp(name)
  if s[2]<1024*1024 or s!=d.get('stats',{}).get(name):need(sha(name)==digest and stamp(name)==s,'Pinned dependency changed '+name)
def verify_chain(base):
 seen=set()
 while (base/'UNIFIED_READY.json').is_file():
  need(base not in seen,'Inherited generation cycle');seen.add(base)
  d=json.loads((base/'UNIFIED_READY.json').read_text());need(d['ready'],'Inherited generation not ready')
  # The original unified gate pinned the mutable current-runtime pointer.
  # Later published generations intentionally supersede that pointer; this
  # gate checks its current value separately, while retaining every asset pin.
  if base.name=='unified-runtime-2026-11-16':d=dict(d,pins={p:h for p,h in d['pins'].items() if p!=str(E.parent/'CURRENT_VALWOOD.json')})
  verify_pins(d)
  prior=d.get('baseline')
  if not prior:break
  path=Path(prior);base=path if path.is_absolute() else base.with_name(prior)
def publish():
 need(not READY.exists(),'Published generations are immutable');resources()
 inherited=json.loads((BASE/'UNIFIED_READY.json').read_text());need(inherited['ready'],'Baseline not ready');verify_chain(BASE)
 build=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text());binary=sha(N/'runtime/rpg')
 need(build['ready'] and build['application_units']==104 and build['sha256']==binary,'Incomplete native build')
 for name,digest in build['source_sha256'].items():need(sha(N/'source-v1'/name)==digest,'Source differs from compiled closure '+name)
 for name in NATIVE:
  d=json.loads((N/'review'/name/'EXECUTION.json').read_text());need(d['passed'] is True and d['binary_sha256']==binary,'Native review failed '+name)
 for name in REPORTS:need(json.loads((N/'review'/name).read_text())['passed'] is True,'Review failed '+name)
 files=[N/'runtime/rpg',N/'runtime/ORIGIN_BUILD.json',N/'launch.py',Path(__file__),N/'GUIDE.txt',N/'run_native_review.py',N/'build_native_candidate.py',N/'SOURCE_ORIGIN.json',BASE/'UNIFIED_READY.json',E/'run_current_valwood.py']
 files += [N/'source-v1'/name for name in build['source_sha256']]
 files += [N/'review'/name for name in REPORTS]+[N/'review'/name/'EXECUTION.json' for name in NATIVE]
 files += [p for p in (N/'tools').rglob('*') if p.is_file() and '__pycache__' not in str(p)]
 files += [N/'review'/name for name in ('ordinary-duel/DUEL_NATIVE_RAW.json','ordinary-duel/DUEL_FRAMES.jsonl','ordinary-duel/continuous.mp4','third-person-dodge-preview.mp4','third-person-dodge-review.png','REEL.json')]
 pins={str(p):sha(p) for p in files}
 READY.write_text(json.dumps(dict(ready=True,one_binary=binary,pins=pins,stats={str(p):stamp(p) for p in files},baseline=str(BASE),baseline_ready_sha256=sha(BASE/'UNIFIED_READY.json'),native_only=True,windows_work_performed=False,baseline_tests_inherited_not_claimed_rerun=True,mutable_authoring_and_saves_not_frozen=True),indent=2)+'\n')
def check():
 d=json.loads(READY.read_text());need(d['ready'],'Runtime not ready');resources();verify_pins(d)
 need(sha(BASE/'UNIFIED_READY.json')==d['baseline_ready_sha256'],'Baseline gate changed');verify_chain(BASE)
 m=json.loads((E.parent/'CURRENT_VALWOOD.json').read_text());need(Path(m['runtime_generation'])==N and m['binary_sha256']==d['one_binary'],'Current runtime manifest differs')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--publish',action='store_true');p.add_argument('--check-only',action='store_true');a=p.parse_args()
 try:
  publish() if a.publish else check();print('Third-person movement generation verified: shared Linux runtime, shaders and character bank.')
 except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)

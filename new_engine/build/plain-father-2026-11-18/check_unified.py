from pathlib import Path
import argparse,hashlib,json,sys,os
N=Path(__file__).resolve().parent;E=N.parents[1];BASE=N.with_name('opening-finish-2026-11-18');READY=N/'UNIFIED_READY.json'
# Required gates cover the requested ground opening, outfit/equipment lifecycle,
# view toggles and existing arena. Optional water probes remain explicitly
# recorded as failed/limited in FINAL_REVIEW, without becoming a pass.
NATIVE=('opening-third-person','journey','arena-regression')
def sha(p):
 with Path(p).open('rb') as h:return hashlib.file_digest(h,'sha256').hexdigest()
def stamp(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def need(ok,msg):
 if not ok:raise RuntimeError(msg)
def verify_pins(d):
 for path,digest in d['pins'].items():
  s=stamp(path)
  if s[2]<1024*1024 or s!=d.get('stats',{}).get(path):need(sha(path)==digest and stamp(path)==s,'Pinned dependency changed '+path)
def verify_chain(base):
 seen=set()
 while (base/'UNIFIED_READY.json').is_file():
  need(base not in seen,'Baseline cycle');seen.add(base);d=json.loads((base/'UNIFIED_READY.json').read_text());need(d['ready'],'Baseline not ready')
  if base.name=='unified-runtime-2026-11-16':d=dict(d,pins={p:h for p,h in d['pins'].items() if p!=str(E.parent/'CURRENT_VALWOOD.json')})
  verify_pins(d)
  if not d.get('baseline'):break
  path=Path(d['baseline']);base=path if path.is_absolute() else base.with_name(str(path))
def resources():
 from launch import configuration,AREAS
 modes=[configuration(m) for m in ('demo','editor','arena','joust','world')]
 need(len({x[0][0] for x in modes})==1,'Modes use different executables')
 for p in [x[2] for x in modes]+[p for p,_ in AREAS.values()]:
  need((p/'world.studio').is_file() and (p/'project.valwood').is_file(),'Missing scene '+str(p))
  need((p/'shaders').resolve()==(N/'shared/shaders').resolve(),'Shader mismatch')
  need((p/'character-textures').resolve()==(N/'shared/character-textures').resolve(),'Character bank mismatch')
  need((p/'saves').is_dir() and not (p/'saves').is_symlink(),'Invalid save directory')
def publish():
 need(not READY.exists(),'Published generations are immutable');resources();verify_chain(BASE)
 b=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text());binary=sha(N/'runtime/rpg');need(b['ready'] and b['application_units']==104 and b['sha256']==binary,'Build incomplete')
 for name,digest in b['source_sha256'].items():need(sha(N/'source-v1'/name)==digest,'Source changed '+name)
 for name in NATIVE:
  r=json.loads((N/'review'/name/'EXECUTION.json').read_text());need(r['passed'] is True and r['binary_sha256']==binary,'Review failed '+name)
 for name in ('CAMERA_CPU.json','CPU_REVIEW.json','FINAL_REVIEW.json'):
  need(json.loads((N/'review'/name).read_text())['passed'] is True,'Review failed '+name)
 files=[N/'runtime/rpg',N/'runtime/ORIGIN_BUILD.json',N/'launch.py',Path(__file__),N/'GUIDE.txt',BASE/'UNIFIED_READY.json',N/'build_native_candidate.py',N/'seed_native_cache.py',N/'run_native_review.py',N/'BASELINE.json']
 files += [N/'source-v1'/f for f in b['source_sha256']]
 for folder in ('tools','assets/story_hero','review'):
  files += [p for p in (N/folder).rglob('*') if p.is_file() and '__pycache__' not in str(p) and p.suffix not in ('.o','.log')]
 pins={str(p):sha(p) for p in files};temporary=READY.with_suffix('.json.tmp');temporary.write_text(json.dumps(dict(ready=True,one_binary=binary,pins=pins,stats={str(p):stamp(p) for p in files},baseline=str(BASE),baseline_ready_sha256=sha(BASE/'UNIFIED_READY.json'),native_only=True,Windows_work_performed=False,mutable_authoring_and_saves_not_frozen=True),indent=2)+'\n');os.replace(temporary,READY)

def check():
 d=json.loads(READY.read_text());need(d['ready'],'Not ready');resources();verify_pins(d);verify_chain(BASE)
 m=json.loads((E.parent/'CURRENT_VALWOOD.json').read_text());need(Path(m['runtime_generation'])==N and m['binary_sha256']==d['one_binary'],'Current pointer differs')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--publish',action='store_true');p.add_argument('--check-only',action='store_true');a=p.parse_args()
 try:publish() if a.publish else check();print('Plain father / third-person generation verified.')
 except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)

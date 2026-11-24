#!/usr/bin/env python3
"""Prepare an isolated, small combat arena with shared read-only asset banks.

The source project and its saves are read only. Each output is a new local
playtest copy, with its own shaders, authoring metadata, collision and saves.
Run --bind-runtime only after the owner selects the tested arena runtime.
"""
import argparse,hashlib,json,math,os,shlex,shutil,struct,tempfile
from pathlib import Path
import prepare_playtest_slices as copies
ROOT=Path(__file__).resolve().parents[1]
CENTER=(-1800.,-10500.)
FLOOR=64.
HALF=12.

def sha(p):return copies.digest(p)
def canonical(v):return (json.dumps(v,sort_keys=True,indent=2)+'\n').encode()
def write(p,raw):
 p.parent.mkdir(parents=True,exist_ok=True)
 with p.open('xb') as out:out.write(raw)
def snapshot_saves(source):
 p=source/'saves'
 return {str(q.relative_to(p)):sha(q) for q in sorted(p.rglob('*')) if q.is_file()} if p.is_dir() else {}
def contract():
 x,z=CENTER
 return dict(schema='valwood.combat-arena',version=1,center=[x,FLOOR,z],floor_y=FLOOR,half_extent=HALF,
  bounds=[x-HALF,z-HALF,x+HALF,z+HALF],entry_path=[[x,FLOOR,z-20],[x,FLOOR,z-14],[x,FLOOR,z-8]],
  player_spawn=[x,FLOOR+1.75,z-5,math.pi,-.04],enemy_spawn=[x,FLOOR,z+3],
  queue_positions=[[x-7+2*i,FLOOR,z+9] for i in range(8)],opponents=8,sequential=True,
  weapons={'1':'sword','2':'bow'},opening_disabled=True,private_saves=True)
def flat_heightfield():
 # Authored closed support region; all reachable floor/entry samples are exact.
 side=129;x,z=CENTER
 return struct.pack('<8sIIdddd',b'VWHGT1\0\0',side,side,x-64,z-64,1.,1.)+struct.pack('<f',FLOOR)*(side*side)
def physics():
 x,z=CENTER
 boxes=[('Arena level floor',x,FLOOR-.5,z,12,.5,12),('Arena entry',x,FLOOR-.5,z-16,2,.5,4),
  ('West fence',x-12,FLOOR+.65,z,.18,.65,12),('East fence',x+12,FLOOR+.65,z,.18,.65,12),
  ('North fence',x,FLOOR+.65,z+12,12,.65,.18),('South west fence',x-7,FLOOR+.65,z-12,5,.65,.18),
  ('South east fence',x+7,FLOOR+.65,z-12,5,.65,.18)]
 rows=['VALWOOD_PHYSICS 1','gravity 0 -9.81 0']+[f'layer {i} 255' for i in range(8)]
 for i,(name,px,py,pz,hx,hy,hz) in enumerate(boxes,1):
  rows.append(f'body {i} "{name}" 0 0 0 0 {px:.12g} {py:.12g} {pz:.12g} 0 0 0 1 {hx:.12g} {hy:.12g} {hz:.12g} .75 0 1000 0 1 0 0 0 0 0 0 1')
 return ('\n'.join(rows)+'\n').encode()
def route():
 c=contract();s=c['player_spawn'];x,z=CENTER
 rows=['VALWOOD_PLAYTEST 1','name "Eight challengers: combat practice"','spawn '+' '.join(f'{v:.12g}' for v in s),'count 3']
 for label,p in zip(['Entry','Challenge circle','Eight challengers'],[[x,FLOOR,z-14],[x,FLOOR,z-4],[x,FLOOR,z+3]]):rows.append(f'path "{label}" '+' '.join(f'{v:.12g}' for v in p)+' 2 checkpoint')
 return ('\n'.join(rows)+'\n').encode()
def safe_output(engine,source,output,banks):
 base=(engine/'build/combat-arena').resolve();resolved=output.resolve()
 if resolved==base or base not in resolved.parents:raise ValueError('Choose a fresh output beneath build/combat-arena')
 if resolved==source or source in resolved.parents or resolved in source.parents:raise ValueError('Output overlaps source project')
 for bank in banks.values():
  if resolved==bank or bank in resolved.parents or resolved in bank.parents:raise ValueError('Output overlaps shared asset bank')
 # Refuse symlink aliases even when their resolved destination would be allowed.
 cursor=output
 while cursor!=engine and cursor!=cursor.parent:
  if cursor.is_symlink():raise ValueError('Output enters a symlink')
  cursor=cursor.parent
 if os.path.lexists(output):raise ValueError('Existing output rejected; use a new candidate')
def launcher(engine,output):
 tool=engine/'tools/prepare_combat_arena.py';project=output/'project'
 return f'''#!/usr/bin/env bash
set -euo pipefail
arena_root={shlex.quote(str(output))}
arena_engine={shlex.quote(str(engine))}
for arena_arg in "$@"; do
 case "$arena_arg" in --project|--project=*|--editor|--scenic|--scenic=*|--no-hud) printf '%s\\n' 'Arena launcher preserves its isolated project and gameplay controls.' >&2; exit 2;; esac
done
arena_binary="$(python3 {shlex.quote(str(tool))} --validate-launch {shlex.quote(str(output))})"
exec 8>"$arena_engine/projects/seldonne/.valwood-instance.lock"
flock -n 8 || {{ printf '%s\\n' 'Valwood is already running. Close that session before opening the arena.' >&2; exit 1; }}
exec 9>"$arena_root/.instance.lock"
flock -n 9 || {{ printf '%s\\n' 'This combat arena is already open.' >&2; exit 1; }}
export VALWOOD_PRESENTATION=1
export VALWOOD_PLAYTEST_SLICE=city
export VALWOOD_COMBAT_ARENA=1
unset VALWOOD_OPENING_PROLOGUE VALWOOD_SHOWCASE
cd -- "$arena_engine"
exec "$arena_binary" --project {shlex.quote(str(project))} --play --scenic city-walk --width 1600 --height 900 "$@"
'''.encode()
def prepare(engine=ROOT,source_project=None,output=None):
 engine=Path(engine).resolve(strict=True);source=Path(source_project or engine/'build/playtest-slices/city/project').resolve(strict=True)
 raw_output=Path(output or engine/'build/combat-arena/candidate-v1').absolute();output=raw_output.resolve()
 files,banks=copies.inventory(source);safe_output(engine,source,raw_output,banks)
 required=['city.scene','city.sky','city_showcase.json','publisher_showcase.json']
 if any(n not in files for n in required):raise ValueError('Arena requires the accepted city source project')
 guard={n:sha(p) for n,p in files.items()};save_guard=snapshot_saves(source)
 if (source/'playtest.route').is_file():guard['playtest.route']=sha(source/'playtest.route')
 output.parent.mkdir(parents=True,exist_ok=True);stage=Path(tempfile.mkdtemp(prefix='.arena-',dir=output.parent))
 try:
  project=stage/'project';project.mkdir()
  overrides={'project.valwood','city.physics','playtest.route'}
  for n,p in files.items():
   if n not in overrides:copies.checked_copy(p,project/n,guard[n])
  for name,p in banks.items():
   if name!='ground':(project/name).symlink_to(p,target_is_directory=True)
  ground=project/'ground';ground.mkdir()
  for p in sorted(banks['ground'].iterdir()):
   if p.name=='showcase.height':continue
   (ground/p.name).symlink_to(p.resolve(strict=True),target_is_directory=p.is_dir())
  (ground/'source-city.height').symlink_to((banks['ground']/'showcase.height').resolve(strict=True))
  write(ground/'showcase.height',flat_heightfield())
  write(project/'city.physics',physics());write(project/'playtest.route',route())
  x,z=CENTER;write(project/'project.valwood',f'name=Valwood | Combat Arena\nterrain=valwood_atlas\nspawn={x} {z-5} -.04\ncamera={math.pi} -.04 7\nday_fraction=.60\n'.encode())
  write(project/'combat_arena.json',canonical(contract()));write(project/'arena.config',b'VALWOOD_COMBAT_ARENA 1\n');(project/'saves').mkdir()
  for p in [project/'graphics.settings',project/'world.studio']:
   assert p.is_file() and not p.is_symlink() and p.stat().st_nlink==1
  after_files,after_banks=copies.inventory(source)
  after={n:sha(p) for n,p in after_files.items()}
  if (source/'playtest.route').is_file():after['playtest.route']=sha(source/'playtest.route')
  if after!=guard or banks!=after_banks or snapshot_saves(source)!=save_guard:raise ValueError('Source changed during preparation')
  metadata=dict(schema='valwood.isolated-combat-arena',version=1,source_project=str(source),source_guard=guard,source_save_sha256=save_guard,
   shared_banks={n:str(p) for n,p in banks.items()},project=str(output/'project'),arena=contract(),runtime=None,
   generated_sha256={n:sha(project/n) for n in ['project.valwood','city.physics','playtest.route','combat_arena.json','arena.config','ground/showcase.height']},
   source_unchanged=True,source_saves_copied=False,shared_bank_bytes_copied=0,private_mutable_files=True,source_editor_allowed=False)
  write(stage/'local-origin.json',canonical(metadata));write(stage/'Launch_Combat_Arena.sh',launcher(engine,output));(stage/'Launch_Combat_Arena.sh').chmod(0o755)
  if os.path.lexists(output):raise ValueError('Output appeared during preparation')
  stage.rename(output)
  return metadata
 finally:
  if stage.exists():shutil.rmtree(stage)
def bow_asset_record(bank):
 bank=Path(bank).resolve(strict=True);manifest=bank/'BOW_ASSETS.json'
 if manifest.is_symlink() or not manifest.is_file():raise ValueError('Bow asset manifest must be ordinary')
 d=json.loads(manifest.read_text())
 if d.get('schema')!='valwood.arena-bow-assets' or not d.get('passed') or len(d['assets'])!=6:raise ValueError('Invalid bow asset bank')
 files={}
 for entry in d['assets']:
  name=entry['file']
  if Path(name).name!=name:raise ValueError('Unsafe bow asset name')
  q=bank/name
  if q.is_symlink() or not q.is_file() or sha(q)!=entry['sha256']:raise ValueError('Bow asset changed')
  files[name]=entry['sha256']
 files['BOW_ASSETS.json']=sha(manifest);files['arena_bow_sockets.inc']=sha(bank/'arena_bow_sockets.inc')
 return dict(bank=str(bank),files_sha256=files)
def bind_assets(output,bank):
 output=Path(output).absolute();origin=output/'local-origin.json';project=output/'project'
 if output.is_symlink() or origin.is_symlink() or project.is_symlink():raise ValueError('Private arena paths required')
 data=json.loads(origin.read_text())
 if data.get('schema')!='valwood.isolated-combat-arena' or data.get('runtime') or data.get('arena_assets'):raise ValueError('Arena is already bound or invalid')
 record=bow_asset_record(bank);link=project/'arena_assets'
 if os.path.lexists(link):raise ValueError('Arena assets path already exists')
 link.symlink_to(record['bank'],target_is_directory=True)
 try:
  data['arena_assets']=record;temporary=output/'local-origin.assets.tmp';write(temporary,canonical(data));os.replace(temporary,origin)
 except BaseException:link.unlink();raise
 return record

def blood_asset_record(bank):
 bank=Path(bank).resolve(strict=True);manifest=bank/'BLOOD_ASSETS.json'
 if manifest.is_symlink() or not manifest.is_file() or manifest.stat().st_size>1024*1024:raise ValueError('Blood asset manifest must be ordinary and bounded')
 d=json.loads(manifest.read_text())
 assets=d.get('assets')
 if d.get('schema')!='valwood.volumetric-blood-assets' or d.get('version')!=1 or d.get('ready') is not True or not isinstance(assets,list) or not 1<=len(assets)<=32:raise ValueError('Invalid purchased blood bank')
 files={};total=0
 for entry in assets:
  name=entry['file']
  if not isinstance(name,str) or Path(name).name!=name or name in {'.','..','BLOOD_ASSETS.json'} or '\\' in name or name.casefold() in {n.casefold() for n in files}:raise ValueError('Unsafe blood asset name')
  q=bank/name
  if q.is_symlink() or not q.is_file() or not 0<q.stat().st_size<=256*1024*1024:raise ValueError('Blood asset must be ordinary and bounded')
  total+=q.stat().st_size
  if total>256*1024*1024 or sha(q)!=entry['sha256']:raise ValueError('Purchased blood asset changed')
  files[name]=entry['sha256']
 if {p.name for p in bank.iterdir()}!=set(files)|{'BLOOD_ASSETS.json'}:raise ValueError('Unlisted purchased blood bank files')
 files['BLOOD_ASSETS.json']=sha(manifest)
 return dict(bank=str(bank),files_sha256=files)

def bind_blood(output,bank):
 output=Path(output).absolute();origin=output/'local-origin.json';project=output/'project'
 if output.is_symlink() or origin.is_symlink() or project.is_symlink():raise ValueError('Private arena paths required')
 data=json.loads(origin.read_text())
 if data.get('schema')!='valwood.isolated-combat-arena' or data.get('runtime') or data.get('blood_assets'):raise ValueError('Arena blood bank already bound or runtime selected')
 record=blood_asset_record(bank);link=project/'blood_assets'
 if os.path.lexists(link):raise ValueError('Arena blood path already exists')
 link.symlink_to(record['bank'],target_is_directory=True)
 try:
  data['blood_assets']=record;temporary=output/'local-origin.blood.tmp';write(temporary,canonical(data));os.replace(temporary,origin)
 except BaseException:link.unlink();raise
 return record

def bind_runtime(output,binary):
 output=Path(output).absolute();binary=Path(binary).resolve(strict=True)
 if output.is_symlink() or not output.is_dir():raise ValueError('Expected ordinary arena output')
 origin=output/'local-origin.json'
 if origin.is_symlink() or not origin.is_file():raise ValueError('Expected private origin')
 data=json.loads(origin.read_text())
 if data.get('schema')!='valwood.isolated-combat-arena' or data.get('runtime') is not None:raise ValueError('Origin already bound or invalid')
 if not binary.is_file() or not os.access(binary,os.X_OK):raise ValueError('Runtime is not executable')
 data['runtime']={'binary':str(binary),'sha256':sha(binary)}
 temporary=output/'local-origin.bind.tmp';write(temporary,canonical(data));os.replace(temporary,origin)
 return data['runtime']
def validate_launch(output):
 output=Path(output).absolute();origin=output/'local-origin.json';project=output/'project'
 if output.is_symlink() or origin.is_symlink() or project.is_symlink():raise ValueError('Arena paths must be ordinary private paths')
 d=json.loads(origin.read_text())
 if d.get('schema')!='valwood.isolated-combat-arena' or d.get('project')!=str(project) or not d.get('runtime'):raise ValueError('Tested arena runtime is not bound')
 for n,h in d['generated_sha256'].items():
  p=project/n
  if p.is_symlink() or not p.is_file() or sha(p)!=h:raise ValueError('Arena runtime data changed: '+n)
 for n,p in d['shared_banks'].items():
  if n=='ground':continue
  if not (project/n).is_symlink() or str((project/n).resolve(strict=True))!=p:raise ValueError('Shared bank changed')
 if (project/'saves').is_symlink() or not (project/'saves').is_dir():raise ValueError('Private saves required')
 if d.get('arena_assets'):
  record=d['arena_assets'];link=project/'arena_assets'
  if not link.is_symlink() or str(link.resolve(strict=True))!=record['bank'] or bow_asset_record(record['bank'])!=record:raise ValueError('Arena bow assets changed')
 if d.get('character_assets'):
  from character_polish_assets import character_asset_record
  record=d['character_assets'];link=project/'models'/'longsword_mocap'
  if str(link.resolve(strict=True))!=record['bank'] or character_asset_record(link)!=record:raise ValueError('Polished character assets changed')
 if d.get('blood_assets'):
  record=d['blood_assets'];link=project/'blood_assets'
  if not link.is_symlink() or str(link.resolve(strict=True))!=record['bank'] or blood_asset_record(record['bank'])!=record:raise ValueError('Arena purchased blood assets changed')
 binary=Path(d['runtime']['binary'])
 if not binary.is_file() or not os.access(binary,os.X_OK) or sha(binary)!=d['runtime']['sha256']:raise ValueError('Arena runtime changed')
 return str(binary)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--engine-root',type=Path,default=ROOT);p.add_argument('--source-project',type=Path);p.add_argument('--output',type=Path);p.add_argument('--bind-runtime',type=Path);p.add_argument('--bind-assets',type=Path);p.add_argument('--bind-blood',type=Path);p.add_argument('--asset-bank',type=Path);p.add_argument('--binary',type=Path);p.add_argument('--validate-launch',type=Path)
 a=p.parse_args()
 try:
  if a.validate_launch:print(validate_launch(a.validate_launch));return
  if a.bind_blood:
   if a.asset_bank is None:raise ValueError('--asset-bank required')
   result=bind_blood(a.bind_blood,a.asset_bank)
  elif a.bind_assets:
   if a.asset_bank is None:raise ValueError('--asset-bank required')
   result=bind_assets(a.bind_assets,a.asset_bank)
  elif a.bind_runtime:
   if a.binary is None:raise ValueError('--binary required')
   result=bind_runtime(a.bind_runtime,a.binary)
  else:result=prepare(a.engine_root,a.source_project,a.output)
 except (OSError,ValueError,KeyError) as error:p.exit(1,f'Arena preparation rejected: {error}\n')
 print(json.dumps(result,indent=2))
if __name__=='__main__':main()

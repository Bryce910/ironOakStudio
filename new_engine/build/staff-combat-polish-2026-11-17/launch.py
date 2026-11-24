#!/usr/bin/env python3
"""One runtime and resource configuration for Valwood's main modes."""
from pathlib import Path
import fcntl,json,os,shutil,sys
N=Path(__file__).resolve().parent;E=N.parents[1]
MODES=('demo','editor','arena','joust','world')
AREAS={'city':(E/'projects/valwood_city_current','city-walk'),'grandstone':(E/'projects/valwood_grandstone_current','stronghold-town'),'showcase':(E/'projects/valwood_showcase_current','castle-finished-arcade'),'atlas':(E/'projects/valwood_atlas_current','rainforest')}

def configuration(mode,extra=()):
 if mode not in MODES:raise ValueError('Choose demo, editor, arena, joust or world')
 world=E/'projects/valwood_current'
 extra=list(extra);area=None
 if '--area' in extra:
  i=extra.index('--area')
  if mode not in ('world','editor') or i+1>=len(extra) or extra[i+1] not in AREAS:raise ValueError('Choose a known area in World or Editor mode: '+', '.join(AREAS))
  area=extra[i+1];del extra[i:i+2]
 project=AREAS[area][0] if area else world if mode in ('demo','editor','world') else N/(mode+'-project')
 view=AREAS[area][1] if area else 'city-meadow' if mode=='world' else 'seldonne-arrival'
 env={k:v for k,v in os.environ.items() if not k.startswith('VALWOOD_') and k!='LD_PRELOAD'}
 env['VALWOOD_RUNTIME_GENERATION']='staff-combat-polish-2026-11-17'
 env['VALWOOD_CIVILIAN_MODELS']=str(N/'assets/walkers')
 args=['--project',str(project),'--play']
 if mode=='editor':args+=['--editor','--scenic',view]
 elif mode in ('arena','joust'):args+=['--scenic','city-walk']
 elif mode=='world':args+=['--scenic',view]
 if mode=='demo':env.update(VALWOOD_PRESENTATION='1',VALWOOD_OPENING_PROLOGUE='1')
 elif mode=='world':
  env['VALWOOD_WORLD_EXPLORATION']='1'
  if area in ('city','grandstone','showcase'):env['VALWOOD_PRESENTATION']='1'
  if area=='showcase' and '--scenic' in extra and extra[extra.index('--scenic')+1]=='rainforest-milestone':env['VALWOOD_SHOWCASE']='jungle'
 elif mode in ('arena','joust'):
  env.update(VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city')
  env['VALWOOD_COMBAT_ARENA' if mode=='arena' else 'VALWOOD_JOUST_ARENA']='1'
 if mode!='joust':
  eq=E/'build/cinematic-studio-2026-11-15/equipment-assets'
  env.update(VALWOOD_MODULAR_KNIGHT=json.loads((eq/'HERO_ASSET.json').read_text())['path'],VALWOOD_MODULAR_KNIGHT_SHADER=str(N/'shared/shaders/world_skeletal.vs.glsl'),VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(N/'shared/shaders/rig_first_person.fs.glsl'),VALWOOD_SKELETAL_LOADOUTS=str(eq/'loadouts.manifest'))
  if mode=='arena':env['VALWOOD_MODULAR_KNIGHT_THIRD_PERSON']='1'
 extra=list(extra)
 if '--cinematics' in extra:
  if mode!='editor':raise ValueError('Cinematics is an editor workspace')
  extra.remove('--cinematics');env['VALWOOD_CINEMATIC_EDITOR']='1'
 if any(x=='--project' or x.startswith('--project=') for x in extra):raise ValueError('Use the shared working world through the normal launcher')
 args+=['--width','1600','--height','900',*extra]
 return [str(N/'runtime/rpg'),*args],env,project

def sync_settings(project):
 # Only one engine session is allowed. Carry the last F10 save between modes.
 paths=[N/'shared/graphics.settings',E/'projects/valwood_current/graphics.settings',N/'arena-project/graphics.settings',N/'joust-project/graphics.settings',*[p/'graphics.settings' for p,_ in AREAS.values()]]
 existing=[p for p in paths if p.is_file()]
 newest=max(existing,key=lambda p:p.stat().st_mtime_ns)
 master=paths[0]
 if newest!=master:shutil.copy2(newest,master)
 if project/'graphics.settings'!=master:shutil.copy2(master,project/'graphics.settings')

def main():
 args=sys.argv[1:];mode=args.pop(0) if args and not args[0].startswith('--') else 'demo'
 if mode not in MODES:raise ValueError('Usage: Launch_Valwood.sh [demo|editor|arena|joust|world]')
 from check_unified import check
 check()
 if '--check-only' in args:print('Ready:',mode,'uses the shared Valwood runtime and shaders');return
 if '--print-command' in args:
  args.remove('--print-command');command,env,project=configuration(mode,args);print(json.dumps(dict(command=command,environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},project=str(project)),indent=2));return
 lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
 try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 except BlockingIOError:raise ValueError('Valwood is already running. Save and close it before switching modes.')
 os.set_inheritable(lock,True)
 command,env,project=configuration(mode,args);sync_settings(project)
 print('[Unified] mode='+mode+' runtime='+str(N/'runtime/rpg')+' world='+str(project),flush=True)
 os.chdir(E);os.execve(command[0],command,env)
if __name__=='__main__':
 try:main()
 except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)

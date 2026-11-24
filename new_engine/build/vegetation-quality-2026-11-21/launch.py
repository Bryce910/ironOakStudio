#!/usr/bin/env python3
"""Launch quality-scaled vegetation with the accepted character and saved world."""
from pathlib import Path
import fcntl,importlib.util,json,os,sys
N=Path(__file__).resolve().parent
B=N.with_name('thomas-animation-polish-2026-11-21')
E=N.parents[1]
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('animation_baseline_launcher',B/'launch.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
MODES=base.MODES
AREAS=base.AREAS
def configuration(mode,extra=(),*,private=False):
    command,env,project=base.configuration(mode,extra,private=private)
    command[0]=str(N/'runtime/rpg')
    env['VALWOOD_RUNTIME_GENERATION']=N.name
    env['VALWOOD_SHADER_BANK_ROOT']=str(N/'shared/shaders')
    if mode!='joust':
        env['VALWOOD_MODULAR_KNIGHT_SHADER']=str(N/'shared/shaders/world_skeletal.vs.glsl')
        env['VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER']=str(N/'shared/shaders/rig_first_person.fs.glsl')
    if mode in ('demo','world','editor') and '--area' not in extra:
        env['VALWOOD_VEGETATION_DRESSING']=str(N/'vegetation/creek_dressing_instances.txt')
    return command,env,project
def private_configuration(mode,extra=()):return configuration(mode,extra,private=True)
def main():
    args=sys.argv[1:];mode=args.pop(0) if args and not args[0].startswith('--') else 'demo'
    if mode not in MODES:raise ValueError('Usage: Launch_Valwood.sh [demo|editor|arena|joust|world]')
    from check_unified import check
    check()
    if '--check-only' in args:print('Ready:',mode,'uses quality-scaled vegetation');return
    if '--print-command' in args:
        args.remove('--print-command');command,env,project=configuration(mode,args)
        print(json.dumps(dict(command=command,environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},project=str(project)),indent=2));return
    lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
    try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:raise ValueError('Valwood is already running. Save and close it before switching modes.')
    os.set_inheritable(lock,True)
    command,env,project=configuration(mode,args);base.base.sync_settings(project)
    print('[Unified] mode='+mode+' runtime='+str(N/'runtime/rpg'),flush=True)
    os.chdir(E);os.execve(command[0],command,env)
if __name__=='__main__':
    try:main()
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)

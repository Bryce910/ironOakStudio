from pathlib import Path
import sys,os
n=Path(__file__).resolve().parents[1];sys.path.insert(0,str(n));from launch import configuration
command,env,origin=configuration('arena');env.pop('VALWOOD_COMBAT_ARENA',None);env.update(VALWOOD_WORLD_EXPLORATION='1',SDL_AUDIODRIVER='dummy',LD_PRELOAD=str(n/'review/native_api_trace.so'))
command[command.index('--project')+1]='/tmp/valwood-animation-polish-walking-first-20261117';command[command.index('--width')+1]='1280';command[command.index('--height')+1]='720'
os.chdir(n.parents[1]);os.execvpe('gdb',['gdb','--quiet','--args',*command],env)

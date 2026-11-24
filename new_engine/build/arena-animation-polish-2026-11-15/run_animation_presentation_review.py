#!/usr/bin/env python3
"""CPU checks for shared bow presentation and collision-swept melee framing."""
import hashlib,json,shlex,subprocess
from pathlib import Path
N=Path(__file__).resolve().parent;S=N/'source-v1';O=N/'animation-review'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 O.mkdir(exist_ok=True);binary=O/'animation-presentation-test';dep=binary.with_suffix('.d');test=S/'tests/world_motion_presentation_test.cpp'
 command=['/usr/bin/g++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-MMD','-MF',str(dep),'-I'+str(S),str(test),'-o',str(binary)]
 subprocess.run(command,check=True);result=subprocess.run([str(binary)],capture_output=True,text=True,check=True)
 log=O/'animation-presentation.log';log.write_text(result.stdout+result.stderr)
 inputs={str(Path(__file__).resolve()):sha(__file__)}
 for p in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):inputs[str(Path(p).resolve())]=sha(p)
 old=N.parent/'arena-blade-clash-2026-11-14/source-v1/engine/world_arena_combat.h';core=S/'engine/world_arena_combat.h'
 assert sha(old)==sha(core),'Bow gameplay core changed';inputs[str(old)]=sha(old);inputs[str(core)]=sha(core)
 receipt={'schema':'valwood.animation.presentation-cpu.v1','passed':True,'inputs':inputs,'outputs':{str(p):sha(p) for p in [binary,dep,log]},'command':command,'results':result.stdout.strip(),'bow_gameplay_unchanged':True,'bow_return_seconds':.24,'tested_rates_hz':[30,60,120],'camera_collision_capsule_radius_m':.22,'camera_maximum_lateral_m':1.04,'coverage':['Rigid blade interpolation through180degrees retains lengths and endpoints','Camera floor/capsule obstruction retracts immediately, bounded80mm sweeps','Camera contraction and tight-space offset remain bounded','Bow release returns smoothly to exact neutral; pause holds pose','Fresh draw during return, hidden reset, source discontinuity reset'],'not_certified':['Hardware performance','Hands-on controller feel','New authored bow release or weapon draw/stow animation']}
 (O/'ANIMATION_PRESENTATION_CPU_REVIEW.json').write_text(json.dumps(receipt,indent=2)+'\n');print(result.stdout.strip())
if __name__=='__main__':main()

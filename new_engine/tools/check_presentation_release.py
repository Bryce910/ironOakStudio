#!/usr/bin/env python3
"""Sequential native release checks; no concurrent GPU/build/hash workloads."""
import hashlib,json,os,subprocess,sys
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import native_environment
from build_world_package import verify_package
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/vertical-slice/2026-10-30/release'
PACKAGE=ROOT/'builds/relocated/Valwood Presentation 2026-10-30'
def run(name,cmd,env=None,timeout=900):
    print('Checking '+name,flush=True)
    return subprocess.run(cmd,cwd='/tmp',env=env,timeout=timeout).returncode
def main():
    OUT.mkdir(parents=True,exist_ok=False);os.environ['PYTHONDONTWRITEBYTECODE']='1'
    checks={}
    checks['journey']=run('complete packaged journey',[sys.executable,str(ROOT/'tools/check_presentation.py'),'--package',str(PACKAGE),'--output',str(OUT/'journey'),'--view','complete'],timeout=450)
    checks['routes']=run('four fixed-quality walking routes',[sys.executable,str(ROOT/'tools/check_presentation_routes.py'),'--package',str(PACKAGE),'--output',str(OUT/'routes'),'--camera','both'],timeout=1400)
    for label,scene,preview in [('title','castle-finished-arcade',True),('tavern','castle-tavern-interior',False),('forest','rainforest-pool',False)]:
        folder=OUT/label;folder.mkdir();env,_=native_environment();env['PYTHONDONTWRITEBYTECODE']='1';env['VALWOOD_CAPTURE_VIEW']='first'if not preview else'third'
        if preview:env.pop('SDL_AUDIODRIVER',None)
        env['LD_LIBRARY_PATH']=str(PACKAGE/'lib')+os.pathsep+env.get('LD_LIBRARY_PATH','')
        capture=folder/'view.bmp';profile=folder/'profile.json'
        cmd=[str(PACKAGE/'launch.sh')]if preview else[str(PACKAGE/'bin/rpg'),'--project',str(PACKAGE/'project'),'--play']
        cmd+=['--scenic',scene,'--no-hud','--width','1600','--height','900','--frames','180','--capture',str(capture),'--profile',str(profile)]
        print('Capturing '+label,flush=True)
        with(folder/'native.log').open('w')as log:code=subprocess.run(cmd,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
        world=json.loads(Path(str(profile)+'.world.json').read_text())if Path(str(profile)+'.world.json').exists()else{}
        passed=code==0 and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and capture.exists()
        if preview:passed=passed and world.get('playable_preview',{}).get('enabled')is True
        checks[label]=0 if passed else 1
        if capture.exists():Image.open(capture).convert('RGB').save(folder/'view.png')
        (folder/'results.json').write_text(json.dumps(dict(passed=passed,exit=code,gl_error=world.get('gl_error'),references=world.get('remaining_texture_references'),preview=world.get('playable_preview'),audio=world.get('demo_audio'),purpose='Untimed native visual/audio startup review'),indent=2)+'\n')
    with(OUT/'preflight.log').open('w')as log:checks['preflight']=subprocess.run([str(PACKAGE/'check-system.sh')],cwd='/tmp',env=os.environ,stdout=log,stderr=subprocess.STDOUT).returncode
    print('Verifying package hashes after native checks',flush=True)
    verification=verify_package(PACKAGE);(OUT/'package-verify.json').write_text(json.dumps(verification,indent=2)+'\n');checks['package']=0
    freeze=json.loads((OUT.parent/'frozen-source.json').read_text());changed=[n for n,h in freeze.items()if hashlib.sha256((ROOT/n).read_bytes()).hexdigest()!=h]
    checks['frozen_source']=0 if not changed else 1
    result=dict(passed=all(v==0 for v in checks.values()),checks=checks,changed_runtime_inputs=changed,package=verification,binary_sha256=hashlib.sha256((PACKAGE/'bin/rpg').read_bytes()).hexdigest())
    (OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
    return 0 if result['passed']else 1
if __name__=='__main__':raise SystemExit(main())

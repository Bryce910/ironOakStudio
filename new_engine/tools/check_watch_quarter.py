#!/usr/bin/env python3
"""Native walking, service/controller and saved-inventory rehearsal; no user save writes."""
import argparse,hashlib,json,subprocess,tempfile
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import native_environment
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()if p.exists()else None
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 project=ROOT/'projects/valwood_visual';protected=[project/'world.studio',project/'graphics.settings',project/'saves/presentation.progress'];before={str(p):digest(p)for p in protected}
 with tempfile.TemporaryDirectory(prefix='valwood-town-qa-')as temp:
  env,_=native_environment();env.update(DISPLAY=':0',XAUTHORITY='/home/computer/.Xauthority',VALWOOD_PRESENTATION='1',VALWOOD_QA_TOWN='1',VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'journey.progress'),SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
  profile=out/'profile.json';capture=out/'quarter.bmp'
  command=[str(ROOT/'rpg'),'--project',str(project),'--play','--scenic','castle-quarter','--no-hud','--width','1600','--height','900','--render-scale','.7','--auto-quality','off','--frames','6500','--warmup','120','--profile',str(profile),'--capture',str(capture)]
  with(out/'native.log').open('w')as log:
   try:code=subprocess.run(command,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
   except subprocess.TimeoutExpired:code=-1
  log=(out/'native.log').read_text();metrics=json.loads(profile.read_text())if profile.exists()else{};wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text())if wp.exists()else{}
  passed=code==0 and'[Town QA] PASS'in log and'AMD Radeon RX 5300'in metrics.get('renderer','')and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and before=={str(p):digest(p)for p in protected}
  result=dict(passed=passed,exit=code,binary_sha256=digest(ROOT/'rpg'),renderer=metrics.get('renderer'),gl_error=world.get('gl_error'),remaining_texture_references=world.get('remaining_texture_references'),project_state_unchanged=before=={str(p):digest(p)for p in protected},checks=[l for l in log.splitlines()if'[Town QA]'in l],isolated_progress_bytes=(Path(temp)/'journey.progress').stat().st_size if(Path(temp)/'journey.progress').exists()else 0)
  if capture.exists():Image.open(capture).convert('RGB').save(out/'quarter.png');capture.unlink()
  (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
 return 0 if passed else 1
if __name__=='__main__':raise SystemExit(main())

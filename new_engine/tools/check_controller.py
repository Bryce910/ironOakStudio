#!/usr/bin/env python3
"""Controller regression: native preview driven by an in-process SDL virtual pad.

No operating-system input injection. Save state is isolated and the project's
authoring/graphics files are fingerprinted before and after the run.
"""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import native_environment

ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--project',type=Path,default=ROOT/'projects/valwood_visual')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    project=args.project.resolve();protected=[project/'world.studio',project/'graphics.settings',project/'saves/presentation.progress',project/'saves/demo.progress']
    before={str(p):digest(p) for p in protected}
    env,_=native_environment();env.update(VALWOOD_PRESENTATION='1',VALWOOD_QA_CONTROLLER='1',VALWOOD_QA_FRAME_TIMES_ONLY='1',SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
    binary=ROOT/'rpg';binary_hash=digest(binary);profile=out/'profile.json';capture=out/'controller.bmp'
    with tempfile.TemporaryDirectory(prefix='valwood-controller-qa-') as temp:
        env['VALWOOD_QA_PROGRESS_FILE']=str(Path(temp)/'journey.progress')
        command=[str(binary),'--project',str(project),'--play','--scenic','castle-milestone','--no-hud','--width','1280','--height','720','--render-scale','0.7','--frames','5000','--warmup','120','--profile',str(profile),'--capture',str(capture)]
        with (out/'native.log').open('w') as log:
            try:code=subprocess.run(command,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
            except subprocess.TimeoutExpired:code=-1
        wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text()) if wp.exists() else {}
        metrics=json.loads(profile.read_text()) if profile.exists() else {};renderer=metrics.get('renderer','')
        native=bool(renderer) and not any(x in renderer.lower() for x in ['llvmpipe','softpipe','software'])
        after={str(p):digest(p) for p in protected};unchanged=before==after
        result=dict(passed=code==0 and native and unchanged and world.get('controller',{}).get('qa_passed') is True and world.get('gl_error')==0 and world.get('remaining_texture_references')==0,
            exit=code,binary_sha256=binary_hash,renderer=renderer,native=native,controller=world.get('controller'),gl_error=world.get('gl_error'),remaining_texture_references=world.get('remaining_texture_references'),project_state_unchanged=unchanged,
            isolated_progress_bytes=(Path(temp)/'journey.progress').stat().st_size if (Path(temp)/'journey.progress').exists() else 0,
            device='SDL virtual Xbox-compatible controller; physical hardware and wireless transport are not validated by this run')
        if capture.exists():Image.open(capture).convert('RGB').save(out/'controller.png')
        (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
    return 0 if result['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

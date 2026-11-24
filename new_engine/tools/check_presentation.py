#!/usr/bin/env python3
"""Native preview workflow/captures on isolated save/authoring state."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import native_environment
from demo_qa_state import protected_state
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--view',choices=['complete','title','conversation','trial','crossing','parry','impact'],default='complete')
    p.add_argument('--package',type=Path)
    p.add_argument('--project',type=Path,help='Alternate source project; mutually exclusive with package')
    p.add_argument('--render-scale',type=float,choices=[.7,.8,.85,.9,1.])
    p.add_argument('--camera',choices=['first','third'],default='third')
    p.add_argument('--require-polish',action='store_true')
    p.add_argument('--retry',action='store_true',help='First lose through ordinary incoming attacks, then retry with Enter')
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    before=protected_state();rows=[]
    if args.project and args.package:p.error('--project and --package are mutually exclusive')
    package=args.package.resolve()if args.package else None
    binary=package/'bin/rpg'if package else ROOT/'rpg';project=package/'project'if package else (args.project.resolve()if args.project else ROOT/'projects/valwood_presentation')
    sha=hashlib.sha256(binary.read_bytes()).hexdigest()
    env,_=native_environment();env.update(VALWOOD_PRESENTATION='1',VALWOOD_QA_PRESENTATION='1',VALWOOD_QA_PRESENTATION_VIEW=args.view,VALWOOD_QA_FRAME_TIMES_ONLY='1',VALWOOD_CAPTURE_VIEW=args.camera)
    if args.retry:env['VALWOOD_QA_PRESENTATION_RETRY']='1'
    with tempfile.TemporaryDirectory(prefix='valwood-presentation-qa-')as temp:
        env['VALWOOD_QA_PROGRESS_FILE']=str(Path(temp)/'preview.progress')
        profile=out/'profile.json';capture=out/'preview.bmp'
        command=[str(binary),'--project',str(project),'--play','--scenic','castle-finished-arcade','--no-hud','--frames','18000','--width','1600','--height','900','--warmup','120','--auto-quality','off','--profile',str(profile),'--capture',str(capture)]
        if args.render_scale:command+=['--render-scale',str(args.render_scale)]
        if package:env['LD_LIBRARY_PATH']=str(package/'lib')+os.pathsep+env.get('LD_LIBRARY_PATH','')
        with(out/'native.log').open('w')as log:
            try:code=subprocess.run(command,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=420).returncode
            except subprocess.TimeoutExpired:code=-1
        wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text())if wp.exists()else{};metrics=json.loads(profile.read_text())if profile.exists()else{}
        if capture.exists():Image.open(capture).convert('RGB').save(out/'preview.png')
        renderer=metrics.get('renderer','');native=not any(s in renderer.lower()for s in ['llvmpipe','softpipe','software'])and bool(renderer)
        result=dict(passed=code==0 and native and world.get('playable_preview',{}).get('qa_passed')is True and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and protected_state()==before,
            exit=code,binary_sha256=sha,renderer=renderer,native=native,view=args.view,preview=world.get('playable_preview'),gl_error=world.get('gl_error'),references=world.get('remaining_texture_references'),streaming=world.get('asset_streaming'),source_authoring_unchanged=before==protected_state(),progress_bytes=(Path(temp)/'preview.progress').stat().st_size if(Path(temp)/'preview.progress').exists()else 0,
            note='Actual directional-combat input and quest/memory/save host checks. This mixed UI/loading journey is not a steady walking benchmark.')
        fx=world.get('combat_vfx',{});result['combat_vfx']=fx;result['recorded_dialogue']=world.get('recorded_dialogue');result['combat_timing_windows']=world.get('combat_timing_windows')
        if args.retry:
            result['actual_defeat_and_retry']='Actual retry countdown reached' in (out/'native.log').read_text()
            result['passed']=result['passed']and result['actual_defeat_and_retry']
        if args.require_polish:
            fields=['draws','trail_segments']+(['hits','parries']if args.view=='complete'else['parries']if args.view=='parry'else['hits']if args.view=='impact'else[])
            result['polish_checks']=all(fx.get(k,0)>0 for k in fields) and fx.get('failures')==0
            result['passed']=result['passed']and result['polish_checks']
        (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
    return 0 if result['passed']else 1
if __name__=='__main__':raise SystemExit(main())

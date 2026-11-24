#!/usr/bin/env python3
"""Native display/quality apply, isolated preferences, no OS input injection."""
import argparse, hashlib, json, re, subprocess, tempfile
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import native_environment

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    project=ROOT/'projects/valwood_visual'
    protected=[project/'graphics.settings',project/'world.studio',project/'saves/presentation.progress']
    before={str(p):sha(p) for p in protected}
    with tempfile.TemporaryDirectory(prefix='valwood-display-qa-') as temp:
        env,_=native_environment();env.update(VALWOOD_PRESENTATION='1',VALWOOD_QA_DISPLAY_CYCLE='1',
            VALWOOD_QA_GRAPHICS_FILE=str(Path(temp)/'graphics.settings'),
            VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'journey.progress'))
        profile=out/'display.json';capture=out/'display.bmp'
        cmd=[str(ROOT/'rpg'),'--project',str(project),'--play','--scenic','castle-milestone',
            '--no-hud','--graphics-menu','--frames','730','--warmup','120','--width','1600','--height','900',
            '--auto-quality','off','--profile',str(profile),'--capture',str(capture)]
        with (out/'native.log').open('w') as log:
            try:code=subprocess.run(cmd,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
            except subprocess.TimeoutExpired:code=-1
        path=Path(str(profile)+'.world.json');world=json.loads(path.read_text()) if path.exists() else {}
        text=(out/'native.log').read_text(errors='replace');graphics=world.get('graphics',{})
        modes=re.findall(r'\[Graphics\] Display (Windowed|Borderless) (\d+)x(\d+) VSync (\d+)',text)
        saved=Path(temp)/'graphics.settings'
        preferences=saved.read_text() if saved.exists() else ''
        (out/'saved.settings').write_text(preferences)
        result=dict(exit=code,binary_sha256=sha(ROOT/'rpg'),graphics=graphics,display_transitions=modes,
            gl_error=world.get('gl_error'),remaining_texture_references=world.get('remaining_texture_references'),
            protected_unchanged=before=={str(p):sha(p) for p in protected})
        result['passed']=code==0 and result['protected_unchanged'] and len(modes)==3 and modes[0]==('Windowed','1280','720','0') and modes[1][0]=='Borderless' and modes[1][3]=='1' and modes[2]==('Windowed','1600','900','1') and graphics.get('applied_changes')==3 and graphics.get('apply_failures')==0 and graphics.get('render_width')==1600 and graphics.get('render_height')==900 and result['gl_error']==0 and result['remaining_texture_references']==0 and 'version=2\n' in preferences and 'window_width=1600\n' in preferences and 'vsync=1\n' in preferences
        if capture.exists():Image.open(capture).convert('RGB').save(out/'display.png');capture.unlink()
        (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
    return 0 if result['passed'] else 1

if __name__=='__main__':raise SystemExit(main())

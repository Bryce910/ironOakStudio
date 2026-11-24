#!/usr/bin/env python3
"""Native, untimed views of the focused art milestone, isolated from user saves."""
import argparse, hashlib, json, subprocess, tempfile
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import native_environment
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--view',choices=['court','jungle','hero','all'],default='all');a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);project=ROOT/'projects/valwood_visual';rows=[]
    digest=hashlib.sha256((ROOT/'rpg').read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='valwood-milestone-views-')as temp:
        for view in ['court','jungle','hero']if a.view=='all'else[a.view]:
            env,_=native_environment();env.update(DISPLAY=':0',XAUTHORITY='/home/computer/.Xauthority',VALWOOD_PRESENTATION='1',VALWOOD_QA_MILESTONE_VIEW=view,VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'journey.progress'))
            profile=out/(view+'.json');capture=out/(view+'.bmp');scene={'court':'castle-milestone','jungle':'rainforest-milestone','hero':'castle-hero-portrait'}[view]
            cmd=[str(ROOT/'rpg'),'--project',str(project),'--play','--scenic',scene,'--no-hud','--frames','240','--width','1600','--height','900','--warmup','60','--auto-quality','off','--render-scale','1','--profile',str(profile),'--capture',str(capture)]
            with(out/(view+'.log')).open('w')as log:
                try:code=subprocess.run(cmd,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
                except subprocess.TimeoutExpired:code=-1
            if capture.exists():Image.open(capture).convert('RGB').save(out/(view+'.png'));capture.unlink()
            wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text())if wp.exists()else{};metrics=json.loads(profile.read_text())if profile.exists()else{}
            row=dict(view=view,exit_code=code,renderer=metrics.get('renderer'),review=world.get('milestone_review'),gl_error=world.get('gl_error'),remaining_texture_references=world.get('remaining_texture_references'),streaming=world.get('asset_streaming'))
            row['passed']=code==0 and bool(row['renderer'])and'AMD Radeon RX 5300'in row['renderer']and row['gl_error']==0 and row['remaining_texture_references']==0 and row['streaming']['failed']==0 and row['review']['view']=={'court':1,'jungle':2,'hero':3}[view]and(view!='hero'or(row['review']['portrait_frames']>200 and row['review']['player_body']))
            rows.append(row);(out/'results.json').write_text(json.dumps(dict(binary_sha256=digest,views=rows,passed=all(r['passed']for r in rows)),indent=2)+'\n');print(json.dumps(row),flush=True)
    return 0 if all(r['passed']for r in rows)else 1
if __name__=='__main__':raise SystemExit(main())

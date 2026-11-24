#!/usr/bin/env python3
"""Untimed native captures of the five maintained demo review locations."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image
from benchmark_graphics_settings import ROOT, LAUNCHER, native_environment
from demo_qa_state import protected_state


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--scenes',nargs='+',default=['castle-finished-arcade','castle-tavern-interior','rainforest-purchased-understory','rainforest-pool','rainforest-camp']);args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    before=protected_state();binary=hashlib.sha256((ROOT/'rpg').read_bytes()).hexdigest();rows=[]
    for scene in args.scenes:
        env,_=native_environment();env['VALWOOD_CAPTURE_VIEW']='first'
        capture=out/(scene+'.bmp');profile=out/(scene+'.json')
        with (out/(scene+'.log')).open('w') as log:
            try:code=subprocess.run([str(LAUNCHER),'--scenic',scene,'--no-hud','--frames','180','--width','1600','--height','900','--auto-quality','off','--capture',str(capture),'--profile',str(profile)],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
            except subprocess.TimeoutExpired:code=-1
        wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text()) if wp.exists() else {}
        cp=Path(str(capture)+'.json');pixels=json.loads(cp.read_text()) if cp.exists() else {}
        graphics=world.get('graphics',{});stream=world.get('asset_streaming',{})
        quality=all(graphics.get(k)==v for k,v in dict(display_width=1600,display_height=900,render_width=1120,render_height=630,automatic=False,reflections=True,contact_shadows=True,volumetric_fog=True,volumetric_clouds=True).items())
        passed=code==0 and quality and world.get('gl_error')==0 and world.get('failed_tiles')==0 and world.get('remaining_texture_references')==0 and pixels.get('capture_ok') is True and pixels.get('offscreen') is False and 'Radeon RX 5300' in pixels.get('renderer','') and stream.get('failed')==0 and stream.get('critical_pending')==0
        if capture.exists():
            with Image.open(capture) as im:im.convert('RGB').save(out/(scene+'.png'))
        row=dict(scene=scene,passed=passed,exit=code,quality_unchanged=quality,gl_error=world.get('gl_error'),references=world.get('remaining_texture_references'),lighting=world.get('lighting'),streaming=stream)
        rows.append(row);print(json.dumps({k:row[k] for k in ('scene','passed','exit','gl_error')}),flush=True)
    report=dict(purpose='Untimed visual captures, not frame-rate acceptance',passed=all(r['passed'] for r in rows) and protected_state()==before,binary_sha256=binary,source_unchanged=protected_state()==before,rows=rows)
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');raise SystemExit(0 if report['passed'] else 1)


if __name__=='__main__':main()

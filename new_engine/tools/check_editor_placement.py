#!/usr/bin/env python3
"""Exercise real asset-card drags and automatic rendering on the native GPU."""
import argparse
import csv
import hashlib
import json
import math
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image
from benchmark_graphics_settings import ROOT, PROJECT, LAUNCHER, native_environment
from demo_qa_state import protected_state


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    before=protected_state()
    binary=hashlib.sha256((ROOT/'rpg').read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='valwood-editor-placement-') as folder:
        document=Path(folder)/'world.studio'
        shutil.copy2(PROJECT/'world.studio',document)
        env,_=native_environment()
        env.update(VALWOOD_QA_STUDIO_DOCUMENT=str(document),VALWOOD_QA_EDITOR_PLACEMENT='1',VALWOOD_QA_FRAME_TIMES_ONLY='1')
        profile=out/'profile.json';capture=out/'workspace.bmp'
        # Retain early drag timings as well as the separately filtered steady
        # interval. A profiler warmup would otherwise discard the gestures.
        command=[str(LAUNCHER),'--editor','--scenic','castle-finished-arcade','--frames','1400','--warmup','0','--width','1600','--height','900','--auto-quality','off','--profile',str(profile),'--capture',str(capture)]
        with (out/'native.log').open('w') as log:
            code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
        world_path=Path(str(profile)+'.world.json')
        world=json.loads(world_path.read_text())if world_path.exists()else{}
        metrics=json.loads(profile.read_text())if profile.exists()else{}
        csv_path=Path(str(profile)+'.csv')
        rows=list(csv.DictReader(csv_path.open()))if csv_path.exists()else[]
        values=[float(r['frame_ms'])for r in rows if int(r['frame'])>=800]
        slow=sorted(values,reverse=True)[:max(1,math.ceil(len(values)*.01))]
        passed=code==0 and '[Editor placement QA] PASS 'in(out/'native.log').read_text() and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and before==protected_state()
        result=dict(passed=passed,exit=code,binary_sha256=binary,renderer=metrics.get('renderer'),gl_error=world.get('gl_error'),remaining_texture_references=world.get('remaining_texture_references'),production_files_unchanged=before==protected_state(),steady_samples=len(values),steady_average_fps=1000*len(values)/sum(values)if values else None,steady_one_percent_low_fps=1000*len(slow)/sum(slow)if slow else None,steady_frames_below45=sum(v>1000/45 for v in values),note='Steady frames after 800 exclude explicit object preparation/loading; full frame history is retained in profile CSV.')
        result['drag_runs']=[]
        for cycle,start,end in re.findall(r'\[Editor drag QA\] measured cycle (\d+) frames (\d+) (\d+)',(out/'native.log').read_text()):
            frames=[float(r['frame_ms'])for r in rows if int(start)+4<=int(r['frame'])<=int(end)-3]
            worst=sorted(frames,reverse=True)[:max(1,math.ceil(len(frames)*.01))]
            result['drag_runs'].append(dict(cycle=int(cycle),samples=len(frames),average_fps=1000*len(frames)/sum(frames)if frames else None,one_percent_low_fps=1000*len(worst)/sum(worst)if worst else None,worst_ms=max(frames)if frames else None,frames_below45=sum(ms>1000/45 for ms in frames)))
        if document.exists():shutil.copy2(document,out/'placed-example.world.studio')
        if capture.exists():Image.open(capture).save(out/'workspace.png')
        (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2))
        return 0 if passed else 1


if __name__=='__main__':raise SystemExit(main())

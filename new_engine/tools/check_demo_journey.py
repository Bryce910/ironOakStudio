#!/usr/bin/env python3
"""Untimed actual-input demo journey and save/resume on private state."""
import argparse
import hashlib
import json
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
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    before=protected_state();binary=hashlib.sha256((ROOT/'rpg').read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='valwood-journey-native-') as tmp:
        document=Path(tmp)/'world.studio';shutil.copy2(PROJECT/'world.studio',document)
        save=Path(tmp)/'demo.progress'
        env,_=native_environment();env.update(VALWOOD_CAPTURE_VIEW='third',VALWOOD_QA_STUDIO_DOCUMENT=str(document),VALWOOD_QA_LIVE_WORLD='1',VALWOOD_QA_DEMO_JOURNEY='1',VALWOOD_QA_PROGRESS_FILE=str(save))
        profile=out/'workflow.json';capture=out/'journey.bmp'
        command=[str(LAUNCHER),'--scenic','castle-courtyard','--frames','1600','--width','1600','--height','900','--auto-quality','off','--profile',str(profile),'--capture',str(capture)]
        with (out/'workflow.log').open('w') as log:
            try:code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
            except subprocess.TimeoutExpired:code=-1
        worldfile=Path(str(profile)+'.world.json');world=json.loads(worldfile.read_text()) if worldfile.exists() else {}
        passed=code==0 and '[Journey QA] PASS:' in (out/'workflow.log').read_text() and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and protected_state()==before
        if capture.exists():
            with Image.open(capture) as im:im.convert('RGB').save(out/'journey.png')
        report=dict(passed=passed,exit=code,binary_sha256=binary,source_unchanged=protected_state()==before,gl_error=world.get('gl_error'),texture_refs=world.get('remaining_texture_references'),live=world.get('live_world'),saved_bytes=save.stat().st_size if save.exists() else 0,note='Input/host integration test. The result injection tests host lifecycle; CPU combat tests cover actual combat rules. This is not a player skill or frame-rate benchmark.')
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
    raise SystemExit(0 if passed else 1)


if __name__=='__main__':main()

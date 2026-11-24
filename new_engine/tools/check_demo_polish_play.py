#!/usr/bin/env python3
"""Bounded native interaction/rollback correctness on a private world copy."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from benchmark_graphics_settings import ROOT, PROJECT, LAUNCHER, native_environment
from demo_qa_state import protected_state


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--camera',choices=['first','third'],default='third')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    protected=protected_state()
    binary=sha(ROOT/'rpg')
    with tempfile.TemporaryDirectory(prefix='valwood-demo-play-') as tmp:
        document=Path(tmp)/'world.studio';shutil.copy2(PROJECT/'world.studio',document)
        env,_=native_environment();env.update(VALWOOD_CAPTURE_VIEW=args.camera,
            VALWOOD_QA_STUDIO_DOCUMENT=str(document),VALWOOD_QA_LIVE_WORLD='1')
        profile=out/'workflow.json'
        command=[str(LAUNCHER),'--scenic','castle-courtyard','--no-hud','--frames','900',
            '--width','1600','--height','900','--auto-quality','off','--profile',str(profile),
            '--capture',str(out/'workflow.bmp')]
        with (out/'workflow.log').open('w') as log:
            try:
                result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
                code=result.returncode
            except subprocess.TimeoutExpired:
                code=-1
        wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text()) if wp.exists() else {}
        marker='[Live QA] PASS:' in (out/'workflow.log').read_text()
        unchanged=protected_state()==protected
        report=dict(purpose='Correctness only; teleports, training and loading are not walking FPS',
            camera=args.camera,exit=code,live_workflow_pass_marker=marker,
            gl_error=world.get('gl_error'),remaining_refs=world.get('remaining_texture_references'),
            live=world.get('live_world'),streaming=world.get('asset_streaming'),
            protected_unchanged=unchanged,binary_sha256=binary,binary_unchanged=sha(ROOT/'rpg')==binary)
        report['passed']=code==0 and marker and report['gl_error']==0 and report['remaining_refs']==0 and unchanged and report['binary_unchanged']
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__=='__main__':
    main()

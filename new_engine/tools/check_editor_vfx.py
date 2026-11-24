#!/usr/bin/env python3
"""Verify the VFX workspace with native rendering, real UI input and undo/redo."""
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
    before=protected_state()
    def recoveries():
        return {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(PROJECT.glob('world.studio.recovery*')) if p.is_file()}
    recovery_before=recoveries()
    with tempfile.TemporaryDirectory(prefix='valwood-vfx-') as folder:
        document=Path(folder)/'world.studio';shutil.copy2(PROJECT/'world.studio',document)
        env,_=native_environment();env.update(VALWOOD_QA_STUDIO_DOCUMENT=str(document),VALWOOD_QA_EDITOR_VFX='1')
        profile=out/'profile.json';capture=out/'vfx-editor.bmp'
        command=[str(LAUNCHER),'--editor','--scenic','castle-finished-arcade','--frames','1150','--warmup','0','--width','1600','--height','900','--auto-quality','off','--profile',str(profile),'--capture',str(capture)]
        with (out/'native.log').open('w') as log:
            code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
        sidecar=Path(str(profile)+'.world.json');world=json.loads(sidecar.read_text()) if sidecar.exists() else {}
        metrics=json.loads(profile.read_text()) if profile.exists() else {};renderer=metrics.get('renderer','')
        native=bool(renderer) and not any(x in renderer.lower() for x in ('llvmpipe','softpipe','swrast'))
        result=dict(passed=code==0 and native and '[VFX QA] PASS ' in (out/'native.log').read_text() and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and before==protected_state() and recovery_before==recoveries(),exit=code,renderer=renderer,native_gpu=native,gl_error=world.get('gl_error'),remaining_texture_references=world.get('remaining_texture_references'),production_files_unchanged=before==protected_state(),user_recovery_files_unchanged=recovery_before==recoveries(),binary_sha256=hashlib.sha256((ROOT/'rpg').read_bytes()).hexdigest(),note='Functional native integration; loading/Apply frames included. No performance claim.')
        if document.exists():shutil.copy2(document,out/'effects-example.world.studio')
        if capture.exists():Image.open(capture).save(out/'vfx-editor.png')
        (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
        return 0 if result['passed'] else 1


if __name__=='__main__':raise SystemExit(main())

#!/usr/bin/env python3
"""Native editor TRS + whole-project save/restore, using a disposable project."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image
from benchmark_graphics_settings import ROOT, PROJECT, native_environment
from demo_qa_state import protected_state

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--debug',action='store_true');args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    before=protected_state();binary=hashlib.sha256((ROOT/'rpg').read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='valwood-editor-completion-') as temp:
        project=Path(temp)
        for source in PROJECT.iterdir():
            if source.is_dir():
                if source.name=='ai':shutil.copytree(source,project/source.name)
                elif source.name!='saves':(project/source.name).symlink_to(source,target_is_directory=True)
            elif source.is_file() and source.suffix not in ('.lock','.tmp') and '.recovery' not in source.name:
                shutil.copy2(source,project/source.name)
        for name in ('world.vwproject','world.vwproject.previous','world.vwproject.journal'):(project/name).unlink(missing_ok=True)
        environment,_=native_environment();environment.update(VALWOOD_QA_EDITOR_COMPLETION='1',VALWOOD_QA_STUDIO_DOCUMENT=str(project/'world.studio'))
        profile=out/'workflow.json';capture=out/'workspace.bmp'
        command=[str(ROOT/'rpg'),'--project',str(project),'--editor','--scenic','castle-finished-arcade','--frames','1600','--width','1600','--height','900','--auto-quality','off','--profile',str(profile),'--capture',str(capture)]
        if args.debug:command=['gdb','-batch','-ex','run','-ex','thread apply all bt 10','--args']+command
        with (out/'native.log').open('w') as log:code=subprocess.run(command,cwd=ROOT,env=environment,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
        world_path=Path(str(profile)+'.world.json');world=json.loads(world_path.read_text())if world_path.exists()else{};metrics=json.loads(profile.read_text())if profile.exists()else{}
        passed=code==0 and '[Editor completion QA] PASS' in (out/'native.log').read_text() and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and before==protected_state()
        for name in ('world.vwproject','world.vwproject.previous'):
            if(project/name).exists():shutil.copy2(project/name,out/name)
        if capture.exists():Image.open(capture).save(out/'workspace.png')
        result=dict(passed=passed,exit=code,binary_sha256=binary,renderer=metrics.get('renderer'),gl_error=world.get('gl_error'),texture_refs=world.get('remaining_texture_references'),production_files_unchanged=before==protected_state(),note='Untimed editor workflow with explicit Apply/save/restore. Aggregate FPS is not a steady performance measurement.')
        (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));return 0 if passed else 1
if __name__=='__main__':raise SystemExit(main())

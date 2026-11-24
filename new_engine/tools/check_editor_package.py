#!/usr/bin/env python3
"""Relocated native editor/play smoke tests. Does not certify another machine."""
import argparse,hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import ROOT,native_environment
from demo_qa_state import protected_state
from build_world_package import verify_package

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--package',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();package=args.package.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    identity=verify_package(package);before=protected_state();binary=hashlib.sha256((package/'bin/rpg').read_bytes()).hexdigest();rows=[]
    with tempfile.TemporaryDirectory(prefix='valwood-package-editor-')as temp:
        document=Path(temp)/'world.studio';shutil.copy2(package/'project/world.studio',document)
        for name,scene,frames,view,editor in [('editor','castle-finished-arcade',1400,'third',True),('jungle','rainforest-purchased-understory',360,'first',False)]:
            env,_=native_environment();env.update(VALWOOD_CAPTURE_VIEW=view,PYTHONDONTWRITEBYTECODE='1')
            if editor:env.update(VALWOOD_QA_STUDIO_DOCUMENT=str(document),VALWOOD_QA_DESIGNER_WORKSPACE='1',VALWOOD_QA_EDITOR_OPTIMIZATION='1')
            profile=out/(name+'.json');capture=out/(name+'.bmp');command=[str(package/'launch.sh'),'--editor'if editor else'--play','--scenic',scene,'--frames',str(frames),'--width','1600','--height','900','--auto-quality','off','--no-hud','--profile',str(profile),'--capture',str(capture)]
            with(out/(name+'.log')).open('w')as log:code=subprocess.run(command,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
            wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text())if wp.exists()else{};metrics=json.loads(profile.read_text())if profile.exists()else{};stream=world.get('asset_streaming',{});text=(out/(name+'.log')).read_text()
            passed=code==0 and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and stream.get('failed')==0 and stream.get('critical_pending')==0 and not any(word in metrics.get('renderer','').lower()for word in ('llvmpipe','software','softpipe'))
            if editor:passed=passed and '[Designer QA] PASS 67'in text and '[Editor optimization QA] PASS 137'in text
            if capture.exists():Image.open(capture).save(out/(name+'.png'))
            row=dict(name=name,passed=passed,exit=code,gl_error=world.get('gl_error'),texture_refs=world.get('remaining_texture_references'),streaming=stream,renderer=metrics.get('renderer'));rows.append(row)
    after=verify_package(package);result=dict(passed=all(r['passed']for r in rows)and before==protected_state()and identity==after,source_files_unchanged=before==protected_state(),package_verified_unchanged=identity==after,package=identity,binary_sha256=binary,runs=rows,scope='Relocated folder, cwd /tmp, native host. Source workspace remains mounted; this is not a clean-machine or cross-platform certification.')
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));return 0 if result['passed']else 1
if __name__=='__main__':raise SystemExit(main())

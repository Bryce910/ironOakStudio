#!/usr/bin/env python3
"""Untimed relocated native startup checks; never compile or cook the package."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image
from benchmark_graphics_settings import native_environment
from demo_qa_state import protected_state


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_preferences(project):
    names={'world.studio','graphics.settings','ai/npc_appearances.vwlooks'}
    ai=project/'ai'
    if ai.exists():
        names.update(str(p.relative_to(project)) for p in ai.rglob('*')
                     if p.is_file() and p.suffix in ('.vwlooks','.vwmemory','.vwminds'))
    return {name:sha(project/name) if (project/name).exists() else None for name in sorted(names)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();package=args.package.resolve(strict=True)
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    source_before=protected_state();package_before=package_preferences(package/'project')
    manifest=json.loads((package/'package-manifest.json').read_text())
    binary=sha(package/'bin/rpg');results=[]
    for label,scene,view in [('castle','castle-finished-arcade','third'),
                             ('jungle','rainforest-purchased-understory','first')]:
        env,_=native_environment();env['VALWOOD_CAPTURE_VIEW']=view
        profile=out/(label+'.json');capture=out/(label+'.bmp')
        command=[str(package/'launch.sh'),'--scenic',scene,'--no-hud','--frames','180',
                 '--width','1600','--height','900','--auto-quality','off',
                 '--profile',str(profile),'--capture',str(capture)]
        with (out/(label+'.log')).open('w') as log:
            try:
                process=subprocess.run(command,cwd='/tmp',env=env,stdout=log,
                                       stderr=subprocess.STDOUT,timeout=240)
                code=process.returncode
            except subprocess.TimeoutExpired:
                code=-1
        wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text()) if wp.exists() else {}
        cp=Path(str(capture)+'.json');pixels=json.loads(cp.read_text()) if cp.exists() else {}
        stream=world.get('asset_streaming') or {}
        passed=code==0 and world.get('gl_error')==0 and world.get('remaining_texture_references')==0
        passed=passed and world.get('remaining_texture_entries')==0 and world.get('failed_tiles')==0
        passed=passed and stream.get('failed')==0 and stream.get('critical_pending')==0
        passed=passed and pixels.get('capture_ok') is True and pixels.get('offscreen') is False
        passed=passed and 'Radeon RX 5300' in pixels.get('renderer','')
        camera_confirmed=f'Initial camera {view} person' in (out/(label+'.log')).read_text()
        passed=passed and camera_confirmed
        if capture.exists():
            with Image.open(capture) as img:img.convert('RGB').save(out/(label+'.png'))
        row=dict(scene=scene,view=view,camera_confirmed=camera_confirmed,exit=code,passed=bool(passed),renderer=pixels.get('renderer'),
                 gl_error=world.get('gl_error'),references=world.get('remaining_texture_references'),
                 streaming=stream,launch_cwd='/tmp',command=command)
        results.append(row);print(json.dumps({k:row[k] for k in ('scene','passed','gl_error','references')}),flush=True)
    report=dict(purpose='Untimed relocated native startup; not walking FPS',package=str(package),
        content_id=manifest['content_id'],binary_sha256=binary,
        binary_unchanged=sha(package/'bin/rpg')==binary,
        source_preferences_unchanged=protected_state()==source_before,
        package_preferences_unchanged=package_preferences(package/'project')==package_before,runs=results)
    report['passed']=all(r['passed'] for r in results) and report['binary_unchanged'] and report['source_preferences_unchanged'] and report['package_preferences_unchanged']
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    raise SystemExit(0 if report['passed'] else 1)


if __name__=='__main__':
    main()

#!/usr/bin/env python3
"""Render exact validated Windows seed files in disposable ordinary copies."""
from pathlib import Path
import argparse, fcntl, hashlib, json, os, pickle, subprocess, sys, tempfile
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from benchmark_graphics_settings import native_environment
from build_windows_artist_bundle import plan_package, copy_verified
from PIL import Image, ImageStat

VIEWS = dict(city='city-walk', jungle='rainforest-milestone',
             grandstone='stronghold-town', opening='city-meadow')

def sha(path):
    with Path(path).open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()

def win(path): return 'Z:'+str(Path(path).resolve()).replace('/', '\\')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--area', choices=VIEWS)
    parser.add_argument('--data-plan', type=Path, default=ROOT/'docs/world-designer/2026-11-05/DATA_PLAN.json')
    args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    doc=ROOT/'docs/world-designer/2026-11-05'
    data_path=args.data_plan.resolve(strict=True)
    data=json.loads(data_path.read_text())
    cache=Path(data['private_cache'])
    assert sha(cache)==data['private_cache_sha256']
    with cache.open('rb') as stream: projects=pickle.load(stream)
    binary=args.binary.resolve(strict=True);before=sha(binary)
    report=dict(binary=str(binary),binary_sha256=before,data_plan=str(data_path),data_plan_sha256=sha(data_path),cases=[],
      scope='Exact packaged Windows project/editor opening and render smoke on hardware GPU under Wine; not physical Windows or sustained performance certification.')
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        for label,validated in projects:
            if args.area and label!=args.area: continue
            print('Materializing '+label,flush=True)
            with tempfile.TemporaryDirectory(prefix='valwood-designer-seed-'+label+'-',dir='/tmp') as temporary:
                project=Path(temporary)/'project';project.mkdir()
                entries,_,_=plan_package(validated)
                copied=0
                for entry in entries:
                    if not entry.name.startswith('project/'):continue
                    target=project/entry.name.removeprefix('project/');target.parent.mkdir(parents=True,exist_ok=True)
                    with target.open('xb') as stream:copy_verified(entry,stream)
                    assert not target.is_symlink()
                    copied+=1
                authored={p.name:sha(p) for p in project.iterdir() if p.is_file()}
                env,_=native_environment()
                env={k:v for k,v in env.items() if not k.startswith('VALWOOD_')}
                env.update(WINEPREFIX=str(ROOT/'build/windows/wine-prefix'),WINEDEBUG='-all',
                    WINEDLLOVERRIDES='mscoree,mshtml=',SDL_AUDIODRIVER='dummy',vblank_mode='0')
                profile=out/(label+'.json');capture=out/(label+'.bmp')
                command=['/usr/lib/wine/wine64',win(binary),'--project',win(project),
                    '--editor','--scenic',VIEWS[label],'--frames','360','--warmup','120',
                    '--width','1600','--height','900','--auto-quality','off',
                    '--profile',win(profile),'--capture',win(capture)]
                (out/(label+'-command.json')).write_text(json.dumps(command,indent=2)+'\n')
                print('Opening '+label,flush=True)
                with (out/(label+'.log')).open('w') as log:
                    code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
                metrics=json.loads(profile.read_text()) if profile.is_file() else {}
                sidecar=Path(str(profile)+'.world.json')
                world=json.loads(sidecar.read_text()) if sidecar.is_file() else {}
                renderer=metrics.get('renderer','')
                image_valid=False
                if capture.is_file():
                    img=Image.open(capture).convert('RGB');img.save(capture.with_suffix('.png'))
                    image_valid=max(ImageStat.Stat(img).stddev)>10
                streaming=world.get('asset_streaming',{})
                checks=dict(exit0=code==0,hardware=bool(renderer) and not any(x in renderer.lower() for x in ('llvmpipe','softpipe','swrast')),
                    gl0=world.get('gl_error')==0,texture_refs0=world.get('remaining_texture_references')==0,
                    asset_failures0=streaming.get('failed')==0,critical_pending0=streaming.get('critical_pending')==0,
                    editor_open=world.get('designer_viewport',{}).get('open') is True,
                    story_disabled=world.get('opening_prologue',{}).get('enabled') is False,
                    nonblank_capture=image_valid,binary_unchanged=sha(binary)==before)
                case=dict(area=label,passed=all(checks.values()),checks=checks,renderer=renderer,
                    copied_project_files=copied,ordinary_disposable_files=True,
                    input_authoring_sha256=authored,asset_streaming=streaming,
                    studio=world.get('studio'),view=VIEWS[label])
                report['cases'].append(case);report['passed']=all(c['passed'] for c in report['cases'])
                (out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
                print(label+(': PASS' if case['passed'] else ': FAIL'),flush=True)
    return 0 if report.get('passed') else 1

if __name__=='__main__':raise SystemExit(main())

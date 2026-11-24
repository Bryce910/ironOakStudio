#!/usr/bin/env python3
"""Render each current orientation project in a disposable project directory."""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image
from benchmark_graphics_settings import ROOT, native_environment

PROJECTS = (
    ('city', 'valwood_city_showcase', 'city-walk'),
    ('jungle', 'valwood_visual', 'rainforest-milestone'),
    ('grandstone', 'valwood_showcase', 'stronghold-town'),
)


def windows_path(path):
    return 'Z:'+str(path.resolve()).replace('/', '\\')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--wine', action='store_true')
    parser.add_argument('--area', choices=[p[0] for p in PROJECTS])
    parser.add_argument('--frames', type=int, default=360)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    binary = ROOT/('build/windows/editor/bin/rpg.exe' if args.wine else 'rpg')
    report = dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  scope='Editor opening/render/resource smoke; not a walking or performance certification.',
                  platform='Windows executable under Wine' if args.wine else 'Linux native',
                  cases=[])
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        for label, name, view in PROJECTS:
            if args.area and args.area != label:
                continue
            print('Opening '+label, flush=True)
            source = ROOT/'projects'/name
            with tempfile.TemporaryDirectory(prefix='valwood-orientation-'+label+'-') as temporary:
                project = Path(temporary)/'project'
                project.mkdir()
                for entry in source.iterdir():
                    if entry.name.startswith('.') or entry.name in ('saves', 'qa', 'captures', 'backups'):
                        continue
                    if '.recovery' in entry.name or entry.name.endswith(('.journal', '.previous')):
                        continue
                    if entry.is_dir():
                        (project/entry.name).symlink_to(entry.resolve(), target_is_directory=True)
                    elif entry.is_file():
                        shutil.copy2(entry, project/entry.name)
                environment, _ = native_environment()
                profile = out/(label+'.json')
                capture = out/(label+'.bmp')
                arg_path = windows_path if args.wine else str
                command = [arg_path(binary), '--project', arg_path(project), '--editor',
                           '--scenic', view, '--frames', str(args.frames), '--warmup', '120',
                           '--width', '1600', '--height', '900', '--auto-quality', 'off',
                           '--profile', arg_path(profile), '--capture', arg_path(capture)]
                if args.wine:
                    environment.update(WINEPREFIX=str(ROOT/'build/windows/render-prefix'),
                                       WINEDEBUG='-all', WINEDLLOVERRIDES='mscoree,mshtml=')
                    command = ['/usr/lib/wine/wine64']+command
                with (out/(label+'.log')).open('w') as log:
                    try:
                        code = subprocess.run(command, cwd=ROOT, env=environment,
                                              stdout=log, stderr=subprocess.STDOUT, timeout=300).returncode
                    except subprocess.TimeoutExpired:
                        code = -1
                metrics = json.loads(profile.read_text()) if profile.is_file() else {}
                sidecar = Path(str(profile)+'.world.json')
                world = json.loads(sidecar.read_text()) if sidecar.is_file() else {}
                renderer = metrics.get('renderer', '')
                passed = (code == 0 and capture.is_file() and bool(renderer)
                          and not any(x in renderer.lower() for x in ('llvmpipe', 'softpipe', 'swrast'))
                          and world.get('gl_error') == 0
                          and world.get('remaining_texture_references') == 0)
                if capture.is_file():
                    Image.open(capture).save(capture.with_suffix('.png'))
                report['cases'].append(dict(area=label, passed=passed, exit=code,
                                           renderer=renderer, gl_error=world.get('gl_error'),
                                           texture_refs=world.get('remaining_texture_references'),
                                           project=name, view=view))
                report['passed'] = all(c['passed'] for c in report['cases'])
                (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
                print(label+(': PASS' if passed else ': FAIL — inspect '+str(out/(label+'.log'))), flush=True)
    return 0 if report.get('passed') else 1


if __name__ == '__main__':
    raise SystemExit(main())

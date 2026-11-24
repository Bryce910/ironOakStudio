#!/usr/bin/env python3
"""Native, temporary-document surface editor workflow; never an FPS benchmark.

Run after compiling. Exercises real Apply, rollback, undo and save/reload.
Keeps failed attempts; requires a fresh output directory and the launcher lock.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image
from benchmark_graphics_settings import ROOT, PROJECT, LAUNCHER, native_environment


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=600)
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('Use a fresh output directory; previous failures are preserved.')
    out.mkdir(parents=True, exist_ok=True)
    if not (ROOT/'rpg').exists():
        parser.error('Build rpg first.')
    protected = {name: digest(PROJECT/name) for name in ('world.studio', 'graphics.settings')}
    environment, removed = native_environment()
    with tempfile.TemporaryDirectory(prefix='valwood-surface-qa-') as temporary:
        document = Path(temporary)/'workflow.world.studio'
        shutil.copy2(PROJECT/'world.studio', document)
        environment.update(VALWOOD_CAPTURE_VIEW='first', VALWOOD_QA_STUDIO_DOCUMENT=str(document),
                           VALWOOD_QA_SURFACE_EDITORS='1')
        profile, capture = out/'workflow.json', out/'workflow.bmp'
        command = [str(LAUNCHER), '--scenic', 'forest', '--editor', '--no-hud',
                   '--frames', '900', '--width', '1600', '--height', '900',
                   '--auto-quality', 'off', '--capture', str(capture), '--profile', str(profile)]
        timeout = False
        with (out/'workflow.log').open('w') as log:
            try:
                run = subprocess.run(command, cwd=ROOT, env=environment, stdout=log,
                                     stderr=subprocess.STDOUT, timeout=args.timeout)
                exit_code = run.returncode
            except subprocess.TimeoutExpired:
                timeout, exit_code = True, -1
        worldfile = Path(str(profile)+'.world.json')
        world = json.loads(worldfile.read_text()) if worldfile.exists() else {}
        log = (out/'workflow.log').read_text(errors='replace')
        if capture.exists():
            Image.open(capture).save(out/'workflow.png')
        if document.exists():
            shutil.copy2(document, out/'editable-example.world.studio')
        result = dict(exit=exit_code, timeout=timeout, command=command,
                      purpose='Native editor correctness; Apply/import/compilation stalls are included, no FPS claim.',
                      binary_sha256=digest(ROOT/'rpg'), removed_environment=removed,
                      pass_marker='[Surface QA] PASS:' in log,
                      surface_editors=world.get('surface_editors'),
                      gl_error=world.get('gl_error'),
                      remaining_texture_references=world.get('remaining_texture_references'),
                      protected_unchanged=all(digest(PROJECT/name) == value for name, value in protected.items()))
        result['passed'] = (exit_code == 0 and not timeout and result['pass_marker']
                            and result['gl_error'] == 0 and result['remaining_texture_references'] == 0
                            and result['protected_unchanged']
                            and (result['surface_editors'] or {}).get('phase') == 7)
        (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps({key: value for key, value in result.items() if key != 'command'}), flush=True)
        raise SystemExit(0 if result['passed'] else 1)


if __name__ == '__main__':
    main()

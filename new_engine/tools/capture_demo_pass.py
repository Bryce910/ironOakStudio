#!/usr/bin/env python3
"""Sequential, untimed native demo captures with protected player settings."""
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


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--scene', action='append')
    p.add_argument('--view', choices=['first', 'third'], default='first')
    p.add_argument('--document', type=Path, help='Optional staged world for an explicit art comparison.')
    args = p.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    protected = protected_state()
    binary = sha(ROOT/'rpg')
    results = []
    scenes = args.scene or ['castle-finished-arcade', 'castle-tavern-interior',
                           'rainforest-purchased-understory', 'rainforest-pool', 'rainforest-camp']
    with tempfile.TemporaryDirectory(prefix='valwood-demo-review-') as temp:
        document = Path(temp)/'world.studio'
        source_document=args.document.resolve() if args.document else PROJECT/'world.studio'
        shutil.copy2(source_document, document)
        for scene in scenes:
            env, _ = native_environment()
            env.update(VALWOOD_CAPTURE_VIEW=args.view, VALWOOD_QA_STUDIO_DOCUMENT=str(document))
            capture, profile = out/(scene+'.bmp'), out/(scene+'.json')
            command = [str(LAUNCHER), '--scenic', scene, '--no-hud', '--frames', '180',
                       '--width', '1600', '--height', '900', '--auto-quality', 'off',
                       '--capture', str(capture), '--profile', str(profile)]
            with (out/(scene+'.log')).open('w') as log:
                try:
                    run = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                         stderr=subprocess.STDOUT, timeout=240)
                    code = run.returncode
                except subprocess.TimeoutExpired:
                    code = -1
            wp = Path(str(profile)+'.world.json')
            world = json.loads(wp.read_text()) if wp.exists() else {}
            if capture.exists():
                Image.open(capture).convert('RGB').save(out/(scene+'.png'))
            row = dict(scene=scene, exit=code, gl_error=world.get('gl_error'),
                       references=world.get('remaining_texture_references'),
                       graphics=world.get('graphics'), lighting=world.get('lighting'),
                       streaming=world.get('asset_streaming'))
            row['passed'] = code == 0 and row['gl_error'] == 0 and row['references'] == 0 and capture.exists()
            results.append(row)
            (out/'results.json').write_text(json.dumps(dict(purpose='Untimed visual QA, not FPS proof',
                binary_sha256=binary, source_document_sha256=sha(source_document), protected_before=protected, results=results), indent=2)+'\n')
            print(json.dumps({k:v for k,v in row.items() if k not in ['graphics','lighting','streaming']}), flush=True)
            if sha(ROOT/'rpg') != binary:
                raise RuntimeError('Executable changed during captures')
    after = protected_state()
    unchanged = after == protected
    (out/'protected-after.json').write_text(json.dumps(dict(unchanged=unchanged,
        sha256=after), indent=2)+'\n')
    raise SystemExit(0 if unchanged and all(r['passed'] for r in results) else 1)


if __name__ == '__main__':
    main()

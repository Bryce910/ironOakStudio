#!/usr/bin/env python3
"""Untimed native look comparison using private documents and saved quality."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

import numpy as np
from PIL import Image
from benchmark_graphics_settings import ROOT, PROJECT, LAUNCHER, native_environment


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--stage', type=Path, default=ROOT/'docs/studio/surface-finish-2026-10-29/look-staged')
    parser.add_argument('--jungle', action='store_true', help='Only after the HDR biome-gain change is compiled.')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('Use a fresh output directory.')
    out.mkdir(parents=True, exist_ok=True)
    protected = {name: sha(PROJECT/name) for name in ('world.studio', 'graphics.settings')}
    binary = sha(ROOT/'rpg')
    cases = [('arcade-original', 'castle-finished-arcade', PROJECT/'world.studio'),
             ('arcade-readable-analytic', 'castle-finished-arcade', args.stage/'cloudy-readable-analytic.world.studio'),
             ('arcade-readable-hdr', 'castle-finished-arcade', args.stage/'cloudy-readable-hdr.world.studio'),
             ('tavern-readable-hdr', 'castle-tavern-interior', args.stage/'cloudy-readable-hdr.world.studio')]
    if args.jungle:
        cases = [('jungle-original', 'rainforest-pool', PROJECT/'world.studio'),
                 ('jungle-readable-hdr', 'rainforest-pool', args.stage/'cloudy-readable-hdr.world.studio')]
    results = []
    with tempfile.TemporaryDirectory(prefix='valwood-look-compare-') as temporary:
        for name, scene, source in cases:
            document = Path(temporary)/(name+'.world.studio')
            shutil.copy2(source, document)
            env, _ = native_environment()
            env.update(VALWOOD_CAPTURE_VIEW='first', VALWOOD_QA_STUDIO_DOCUMENT=str(document))
            capture, profile = out/(name+'.bmp'), out/(name+'.json')
            command = [str(LAUNCHER), '--scenic', scene, '--no-hud', '--frames', '180',
                       '--width', '1600', '--height', '900', '--auto-quality', 'off',
                       '--capture', str(capture), '--profile', str(profile)]
            with (out/(name+'.log')).open('w') as log:
                try:
                    run = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                         stderr=subprocess.STDOUT, timeout=240)
                    exit_code = run.returncode
                except subprocess.TimeoutExpired:
                    exit_code = -1
            worldfile = Path(str(profile)+'.world.json')
            world = json.loads(worldfile.read_text()) if worldfile.exists() else {}
            item = dict(name=name, scene=scene, source_document_sha256=sha(source), exit=exit_code,
                        gl_error=world.get('gl_error'), remaining_refs=world.get('remaining_texture_references'),
                        lighting=world.get('lighting'), environment=world.get('environment'),
                        graphics=world.get('graphics'))
            if capture.exists():
                image = Image.open(capture).convert('RGB')
                image.save(out/(name+'.png'))
                pixels = np.asarray(image, dtype=np.float32)/255
                # Stable central street crop; this is a readability measurement,
                # not a quality score or a substitute for visual review.
                linear = np.where(pixels <= .04045, pixels/12.92, ((pixels+.055)/1.055)**2.4)
                luminance = linear@np.array([.2126,.7152,.0722])
                crop = luminance[440:880, 440:1080]
                item['central_crop_linear_luminance'] = dict(mean=float(crop.mean()),
                                                             median=float(np.median(crop)),
                                                             p10=float(np.percentile(crop, 10)))
            item['passed'] = exit_code == 0 and item['gl_error'] == 0 and item['remaining_refs'] == 0 and capture.exists()
            results.append(item)
            (out/'results.json').write_text(json.dumps(dict(purpose='Untimed visual comparison, no FPS claim.',
                                                           binary_sha256=binary, cases=results), indent=2)+'\n')
            print(json.dumps({k:v for k,v in item.items() if k not in ('lighting','environment','graphics')}), flush=True)
            if sha(ROOT/'rpg') != binary:
                raise RuntimeError('Executable changed during comparison; retain this run and use a fresh output directory.')
    unchanged = all(sha(PROJECT/name) == value for name, value in protected.items())
    (out/'protected-state.json').write_text(json.dumps(dict(unchanged=unchanged, sha256=protected), indent=2)+'\n')
    raise SystemExit(0 if unchanged and all(item['passed'] for item in results) else 1)


if __name__ == '__main__':
    main()

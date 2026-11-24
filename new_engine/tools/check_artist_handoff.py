#!/usr/bin/env python3
"""Native packaged artist edit/save/return/review/reopen; isolated workspaces only."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from PIL import Image
from benchmark_graphics_settings import native_environment
from build_world_package import verify_package
from demo_qa_state import protected_state
from world_artist_handoff import create_workspace, export_return, review_return


def run_visible(command, cwd, env, log):
    with log.open('w') as stream:
        process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT)
        # Some X11/compositor combinations block presentation while fully
        # occluded. Raise only this fixture's window, once, before measuring.
        if shutil.which('wmctrl'):
            for _ in range(30):
                if process.poll() is not None:
                    break
                lines = subprocess.run(['wmctrl', '-lp'], text=True, capture_output=True).stdout.splitlines()
                windows = [line.split()[0] for line in lines if len(line.split()) > 2 and line.split()[2] == str(process.pid)]
                if windows:
                    subprocess.run(['wmctrl', '-ir', windows[0], '-b', 'add,above'], capture_output=True)
                    subprocess.run(['wmctrl', '-ia', windows[0]], capture_output=True)
                    break
                time.sleep(.5)
        try:
            return process.wait(timeout=300)
        except subprocess.TimeoutExpired:
            process.kill();process.wait()
            return -1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--offscreen', action='store_true', help='Functional GPU validation without X11 presentation; no desktop FPS claim')
    args = parser.parse_args()
    seed, out = args.package.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    original = verify_package(seed)
    before = protected_state()
    runs = []
    with tempfile.TemporaryDirectory(prefix='valwood-editor-completion-artist-') as temp:
        root = Path(temp); work = root / 'work'; review = root / 'review'
        create_workspace(seed, work)
        for label, folder, qa, frames in [('edit-save', work, True, 950), ('review-reopen', review, False, 1500)]:
            env, _ = native_environment()
            env.update(PYTHONDONTWRITEBYTECODE='1', VALWOOD_QA_FRAME_TIMES_ONLY='1')
            if qa:
                env.update(VALWOOD_QA_EDITOR_COMPLETION='1', VALWOOD_QA_STUDIO_DOCUMENT=str(folder/'project/world.studio'))
            profile, capture = out/(label+'.json'), out/(label+'.bmp')
            command = [str(folder/'launch.sh'), '--editor', '--scenic', 'castle-finished-arcade',
                       '--frames', str(frames), '--warmup', '120', '--width', '1600', '--height', '900',
                       '--auto-quality', 'off', '--profile', str(profile), '--capture', str(capture)]
            if args.offscreen:
                command.append('--headless')
            code = run_visible(command, '/tmp', env, out/(label+'.log'))
            wp = Path(str(profile)+'.world.json')
            world = json.loads(wp.read_text()) if wp.exists() else {}
            metrics = json.loads(profile.read_text()) if profile.exists() else {}
            native = not any(s in str(metrics.get('renderer', '')).lower() for s in ('llvmpipe','software','softpipe')) and bool(metrics.get('renderer'))
            passed = code == 0 and native and world.get('gl_error') == 0 and world.get('remaining_texture_references') == 0
            if qa:
                passed = passed and '[Editor completion QA] PASS 20' in (out/(label+'.log')).read_text()
            if capture.exists():
                Image.open(capture).save(out/(label+'.png'))
            row = dict(name=label, passed=passed, exit=code, renderer=metrics.get('renderer'),
                       gl_error=world.get('gl_error'), texture_refs=world.get('remaining_texture_references'))
            csv_path = Path(str(profile)+'.csv')
            if not qa and not args.offscreen and csv_path.exists():
                samples = [float(r['frame_ms']) for r in csv.DictReader(csv_path.open()) if int(r['frame']) >= 500]
                slow = sorted(samples, reverse=True)[:max(1, math.ceil(len(samples)*.01))]
                row['steady'] = dict(samples=len(samples), average_fps=1000*len(samples)/sum(samples),
                    one_percent_low_fps=1000*len(slow)/sum(slow), worst_ms=max(samples),
                    frames_below45=sum(v > 1000/45 for v in samples),
                    scope='Editor at saved Custom settings, scale 0.7; frames 500 onward. Complete raw profile retained.')
            runs.append(row)
            if not passed:
                break
            if qa:
                exported = export_return(work, out/'return-packet')
                staged = review_return(out/'return-packet', seed, review)
                (out/'exchange.json').write_text(json.dumps(dict(export=exported, review=staged), indent=2)+'\n')
                validate = subprocess.run(['python3', str(review/'bin/tools/build_world_package.py'),
                    '--engine-root', str(review/'bin'), '--project', str(review/'project'), '--validate-only'],
                    cwd='/tmp', capture_output=True, text=True)
                (out/'review-validation.log').write_text(validate.stdout+validate.stderr)
                if not staged['ok'] or validate.returncode:
                    runs.append(dict(name='staged-validation', passed=False));break
    result = dict(passed=len(runs)==2 and all(r['passed'] for r in runs), runs=runs,
        source_files_unchanged=before==protected_state(), seed_unchanged=verify_package(seed)==original,
        binary_sha256=hashlib.sha256((seed/'bin/rpg').read_bytes()).hexdigest(),
        offscreen=args.offscreen, desktop_presentation_verified=not args.offscreen and all(r['passed'] for r in runs),
        scope='Relocated Linux package, cwd /tmp, native host; disposable complete workspace and review copies. Not a clean-machine/cross-platform or full walking performance certification.')
    result['passed'] = result['passed'] and result['source_files_unchanged'] and result['seed_unchanged']
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

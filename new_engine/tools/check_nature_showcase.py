#!/usr/bin/env python3
"""Capture and measure the actual native publisher scene on the local GPU."""
from pathlib import Path
import argparse, fcntl, hashlib, json, os, subprocess, tempfile
from PIL import Image
from benchmark_graphics_settings import native_environment, MemoryMonitor

ROOT = Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=ROOT / 'projects/valwood_showcase')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scene', default='stronghold-town')
    parser.add_argument('--frames', type=int, default=240)
    parser.add_argument('--warmup', type=int, default=120)
    parser.add_argument('--scale', type=float, default=1.)
    args = parser.parse_args()
    project, out = args.project.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    lock = (ROOT / 'projects/seldonne/.valwood-instance.lock').open('a')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise RuntimeError('The game is already running; an exclusive session is needed for measurement')
    protected = [project / 'graphics.settings', project / 'world.studio']
    for name in ['valwood_showcase', 'valwood_stronghold', 'valwood_visual', 'valwood_world']:
        protected += [p for p in (ROOT / 'projects' / name / 'saves').rglob('*') if p.is_file()]
    before = {str(p): digest(p) for p in protected if p.is_file()}
    shaders = {str(p): digest(p) for p in (project / 'shaders').glob('*.glsl')}
    for name in ['profile.json', 'profile.json.world.json', 'capture.bmp', 'capture.png', 'result.json']:
        (out / name).unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='valwood-publisher-native-') as temp:
        env, _ = native_environment()
        env.update(DISPLAY=':0', XAUTHORITY='/home/computer/.Xauthority',
                   VALWOOD_PRESENTATION='1', VALWOOD_QA_PROGRESS_FILE=str(Path(temp) / 'journey.progress'),
                   VALWOOD_QA_FRAME_TIMES_ONLY='1', VALWOOD_CAPTURE_VIEW='first')
        command = [str(ROOT / 'rpg_grandstone'), '--project', str(project), '--play',
                   '--scenic', args.scene, '--no-hud', '--frames', str(args.frames),
                   '--width', '1600', '--height', '900', '--warmup', str(args.warmup),
                   '--auto-quality', 'off', '--render-scale', str(args.scale),
                   '--profile', str(out / 'profile.json'), '--capture', str(out / 'capture.bmp')]
        monitor = MemoryMonitor()
        monitor.start()
        try:
            with (out / 'run.log').open('w') as log:
                result = subprocess.run(command, cwd='/tmp', env=env, stdout=log,
                                        stderr=subprocess.STDOUT, timeout=1200)
        finally:
            monitor.stop()
    if result.returncode != 0:
        raise RuntimeError(f'Native scene failed ({result.returncode}); see {out / "run.log"}')
    perf = json.loads((out / 'profile.json').read_text())
    world = json.loads((out / 'profile.json.world.json').read_text())
    assert perf['frames'] > 0 and (out / 'capture.bmp').is_file()
    assert 'AMD' in perf['renderer'] and 'llvmpipe' not in perf['renderer'].lower(), perf['renderer']
    for key in ['gl_error', 'failed_tiles', 'remaining_texture_references', 'weather_failures']:
        assert world[key] == 0, (key, world[key])
    assert world['source_terrain_materials']['enabled'], world['source_terrain_materials']
    assert world['stronghold']['failures'] == 0
    assert world['powder_snow']['failures'] == 0
    assert world['asset_streaming']['failed'] == 0, world['asset_streaming']
    after = {str(p): digest(p) for p in protected if p.is_file()}
    assert before == after, 'A saved scene, preference, or save changed during capture'
    assert shaders == {str(p): digest(p) for p in (project / 'shaders').glob('*.glsl')}, 'Shaders changed during capture'
    Image.open(out / 'capture.bmp').convert('RGB').save(out / 'capture.png')
    (out / 'capture.bmp').unlink()
    report = dict(passed=True, scene=args.scene, render_scale=args.scale,
                  renderer=perf['renderer'], mean_fps=perf['mean_fps'],
                  one_percent_low_fps=perf['one_percent_low_fps'],
                  frames_below45=perf['frames_over_45fps_budget'], max_ms=perf['max_ms'],
                  source_terrain_materials=world['source_terrain_materials'],
                  saved_project_state_unchanged=True, memory=monitor.summary(),
                  screenshot=str(out / 'capture.png'), binary_sha256=digest(ROOT / 'rpg_grandstone'))
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()

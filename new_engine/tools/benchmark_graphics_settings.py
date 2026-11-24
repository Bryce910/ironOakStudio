#!/usr/bin/env python3
"""Sequential native graphics-preset/DRS profiles or untimed full-size captures.

Uses the existing launcher lock. Never builds, cooks, edits graphics.settings,
or treats a missed FPS gate as a reason to discard later scene measurements.
Run this tool on the native host; software/offscreen output fails validation.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import threading
import time

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'projects/valwood_world'
LAUNCHER = ROOT.parent / 'Launch_Valwood_World.sh'
DEFAULT_SCENES = ['castle-hero-mid', 'castle-market', 'wildlife-farm', 'wildlife-rainforest']
PRESETS = {
    'performance': {'name': 'Performance', 'shadow': 2048, 'reflection': .25, 'contact': False, 'ceiling': .85},
    'balanced': {'name': 'Balanced', 'shadow': 2048, 'reflection': .5, 'contact': True, 'ceiling': 1.},
    'cinematic': {'name': 'Cinematic', 'shadow': 4096, 'reflection': 1., 'contact': True, 'ceiling': 1.},
}
WIDTH, HEIGHT, WARMUP = 1600, 900, 120


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def input_paths():
    paths = {ROOT / 'rpg', ROOT / 'Makefile', LAUNCHER, PROJECT / 'project.valwood', Path(__file__).resolve()}
    for directory in [ROOT / 'engine', ROOT / 'shaders']:
        paths.update(p for p in directory.rglob('*') if p.is_file() and p.suffix in ('.c', '.cpp', '.h', '.inc', '.glsl'))
    for suffix in ('*.c', '*.cpp', '*.h', '*.inc'):
        paths.update(ROOT.glob(suffix))
    for directory in [PROJECT / 'models', PROJECT / 'scenery', PROJECT / 'ground']:
        paths.update(p for p in directory.rglob('*') if p.is_file())
    # graphics.settings is deliberately not a rendered-input fingerprint: it is
    # read normally, then the explicit preset/auto/target CLI overrides apply.
    return sorted(paths)


def label(path):
    return str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)


def metadata():
    result = {}
    for path in input_paths():
        stat = path.stat()
        result[label(path)] = [stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns]
    return result


def fingerprints():
    return {label(path): digest(path) for path in input_paths()}


def preference_state():
    path = PROJECT / 'graphics.settings'
    if not path.exists():
        return {'exists': False}
    return {'exists': True, 'bytes': path.stat().st_size, 'sha256': digest(path)}


def native_environment():
    environment = os.environ.copy()
    removed = []
    exact = {'VALWOOD_SHADOW_RESOLUTION', 'LIBGL_ALWAYS_SOFTWARE', 'GALLIUM_DRIVER',
             'MESA_LOADER_DRIVER_OVERRIDE', 'LIBGL_DRIVERS_PATH', 'SDL_VIDEODRIVER',
             'EGL_PLATFORM', 'VALWOOD_TEST_NATIVE_GL', 'LIBGL_ALWAYS_INDIRECT',
             'MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE'}
    for key in list(environment):
        if key in exact or key.startswith(('VALWOOD_QA_', 'VALWOOD_DISABLE_')) or key.endswith('_SEQUENCE'):
            removed.append(key)
            del environment[key]
    environment.update(SDL_AUDIODRIVER='dummy', VALWOOD_VILLAGE_LIGHTING='afternoon')
    return environment, sorted(removed)


class MemoryMonitor:
    """Read public aggregate device counters only; no process/private data."""
    def __init__(self):
        self.stop_event = threading.Event()
        self.samples = []
        self.devices = sorted(Path('/sys/class/drm').glob('card[0-9]*/device'))
        self.thread = threading.Thread(target=self.run, daemon=True)

    def run(self):
        while not self.stop_event.is_set():
            row = {'monotonic_seconds': time.monotonic(), 'devices': {}}
            for device in self.devices:
                values = {}
                for key in ('mem_info_vram_used', 'mem_info_vram_total', 'mem_info_gtt_used'):
                    try:
                        values[key] = int((device / key).read_text())
                    except (OSError, ValueError):
                        pass
                if values:
                    row['devices'][device.parent.name] = values
            self.samples.append(row)
            self.stop_event.wait(1.)

    def start(self):
        self.thread.start()

    def stop(self):
        self.stop_event.set()
        self.thread.join()

    def summary(self):
        used = [sum(v.get('mem_info_vram_used', 0) for v in row['devices'].values()) for row in self.samples if row['devices']]
        return {'available': bool(used), 'samples': len(self.samples),
                'peak_aggregate_vram_used_bytes': max(used, default=None)}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def finite_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def validate(stem, args, frames):
    capture_path = stem.with_suffix('.bmp')
    capture = json.loads(Path(str(capture_path) + '.json').read_text())
    renderer = str(capture.get('renderer', ''))
    require(('AMD' in renderer or 'Radeon' in renderer) and not any(x in renderer.lower() for x in ('llvmpipe', 'softpipe', 'swrast', 'software')),
            f'Expected native AMD renderer, received {renderer!r}')
    require(capture.get('capture_ok') is True and capture.get('offscreen') is False, 'Native capture failed or was offscreen')
    require((capture.get('width'), capture.get('height'), capture.get('frames')) == (WIDTH, HEIGHT, frames), 'Capture dimensions/frame count mismatch')
    with Image.open(capture_path) as image:
        require(image.size == (WIDTH, HEIGHT), f'BMP is not full drawable size: {image.size}')
        image.save(stem.with_suffix('.png'))
    report_path = Path(str(capture_path if args.captures else stem.with_suffix('.json')) + '.world.json')
    world = json.loads(report_path.read_text())
    for field in ('gl_error', 'failed_tiles', 'remaining_texture_entries', 'remaining_texture_references'):
        require(world.get(field) == 0, f'{field}: {world.get(field)!r}')
    require(world.get('travel_loading_frames') == 0, 'A stationary scene held loading images during this run')
    graphics = world['graphics']
    expected = PRESETS[args.preset]
    require(graphics.get('preset') == ('Custom' if args.scale is not None else expected['name']), 'Actual preset differs from requested preset')
    automatic = args.auto_quality == 'on'
    require(graphics.get('automatic') is automatic and graphics.get('target_fps') == 60, 'Quality mode/60FPS target override did not apply')
    require(graphics.get('apply_failures') == 0, 'A graphics target/settings allocation failed')
    require((graphics.get('display_width'), graphics.get('display_height')) == (WIDTH, HEIGHT), 'Display dimensions changed')
    rw, rh = graphics['render_width'], graphics['render_height']
    require(isinstance(rw, int) and isinstance(rh, int) and 1 <= rw <= WIDTH and 1 <= rh <= HEIGHT, 'Invalid internal dimensions')
    scale = graphics['effective_scale']
    require(finite_number(scale) and abs(rw / WIDTH - scale) <= .0011 and abs(rh / HEIGHT - scale) <= .0016, 'Nonuniform or misreported render scale')
    ceiling = args.scale if args.scale is not None else expected['ceiling']
    minimum, maximum = graphics['minimum_observed_scale'], graphics['maximum_observed_scale']
    require(all(finite_number(x) for x in (minimum, maximum)), 'Nonfinite observed scales')
    require(.70 - .001 <= minimum <= maximum <= ceiling + .001, 'Adaptive scale exceeded selected bounds')
    require(minimum - .002 <= scale <= maximum + .002, 'Final effective scale is outside observed range')
    for field in ('scale_changes', 'accepted_gpu_samples', 'excluded_gpu_samples', 'stale_gpu_samples'):
        require(isinstance(graphics.get(field), int) and graphics[field] >= 0, f'Invalid DRS counter {field}')
    if automatic and not args.menu and frames > 180:
        require(graphics['accepted_gpu_samples'] > 0, 'No fresh eligible GPU samples reached the controller')
    if not automatic:
        require(graphics['scale_changes'] == 0, 'Fixed quality unexpectedly resized its render targets')
        require(abs(minimum - ceiling) <= .00051 and abs(maximum - ceiling) <= .00051 and abs(scale - ceiling) <= .0011,
                'Fixed quality did not retain the requested/default scale')
    require(world.get('shadow_map_side') == expected['shadow'] and world.get('soft_shadow_taps') == 9, 'Actual shadow detail differs from preset')
    require(graphics.get('reflections') is True and graphics.get('reflection_scale') == expected['reflection'], 'Reflection preset did not apply')
    require(graphics.get('contact_shadows') is expected['contact'] and world.get('contact_lighting') is expected['contact'], 'Contact-lighting preset did not apply')
    require(world.get('occlusion_frames') == (frames if expected['contact'] else 0), 'Contact-lighting resolve count differs from selected effect')
    require(graphics.get('volumetric_fog') is True and graphics.get('volumetric_clouds') is True and world.get('volumetric_clouds') is True, 'Cloud/mist preset did not apply')
    for field in ('occlusion_failures', 'weather_failures'):
        require(world.get(field) == 0, f'{field}: {world.get(field)!r}')
    wildlife = world.get('wildlife', {})
    if wildlife:
        require(wildlife.get('load_failures') == 0 and wildlife.get('unsafe_roots') == 0, 'Wildlife load/support regression')
    row = {'renderer': renderer, 'graphics': graphics, 'correctness_pass': True,
           'world_report': str(report_path), 'capture': str(capture_path), 'native_dimensions': [WIDTH, HEIGHT],
           'travel_loading_frames': world['travel_loading_frames']}
    if not args.captures:
        timing = json.loads(stem.with_suffix('.json').read_text())
        require(timing.get('samples') == frames - WARMUP and timing.get('dropped_samples') == 0, 'Missing measured frame samples')
        for field in ('mean_fps', 'one_percent_low_fps', 'p99_ms', 'max_ms', 'mean_render_total_gpu_ms'):
            require(finite_number(timing.get(field)) and timing[field] > 0, f'Invalid timing field {field}')
        require(finite_number(timing.get('within_45fps_budget_percent')) and 0 <= timing['within_45fps_budget_percent'] <= 100, 'Invalid frame-budget percentage')
        row['timing'] = timing
        row.update({field: timing[field] for field in ('mean_fps', 'one_percent_low_fps', 'p99_ms', 'max_ms',
                    'within_45fps_budget_percent', 'mean_render_total_gpu_ms')})
        row['strict_45fps_pass'] = timing['mean_fps'] >= 45 and timing['one_percent_low_fps'] >= 45
    else:
        row['timing'] = None
        row['strict_45fps_pass'] = None
    return row


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument('--preset', choices=PRESETS, default='balanced')
    result.add_argument('--auto-quality', choices=('on', 'off'), default='on', help='Adaptive image quality (default:on); off validates a fixed image scale')
    result.add_argument('--scenes', nargs='+', choices=DEFAULT_SCENES, default=DEFAULT_SCENES)
    result.add_argument('--frames', type=int, help='Total frames; defaults to1800 (profiles),300 (captures)')
    result.add_argument('--output', type=Path, default=ROOT / 'docs/performance/graphics-settings/native')
    result.add_argument('--captures', action='store_true', help='Untimed native captures, no profile or FPS claim')
    result.add_argument('--menu', action='store_true', help='Show the ordinary graphics menu via --graphics-menu')
    result.add_argument('--scale', type=float, help='Optional explicit .70..1 ceiling; runtime reports Custom preset')
    return result


def main(argv=None):
    cli = parser()
    args = cli.parse_args(argv)
    frames = args.frames if args.frames is not None else (300 if args.captures else 1800)
    if frames <= (0 if args.captures else WARMUP):
        cli.error('frames must be positive and profiles must exceed the120-frame warmup')
    if args.scale is not None and (not math.isfinite(args.scale) or not .70 <= args.scale <= 1.):
        cli.error('scale must be finite and between .70 and1')
    if len(args.scenes) != len(set(args.scenes)):
        cli.error('scenes must be unique')
    output = args.output.expanduser().resolve()
    for directory in (ROOT / 'engine', ROOT / 'shaders', PROJECT / 'models', PROJECT / 'scenery', PROJECT / 'ground'):
        if output == directory or output.is_relative_to(directory):
            cli.error('output must not be inside a frozen runtime-input directory')
    if not (ROOT / 'rpg').is_file() or not os.access(ROOT / 'rpg', os.X_OK) or not (PROJECT / 'project.valwood').is_file() or not LAUNCHER.is_file():
        cli.error('Prepare the executable/project first; this tool never invokes builds or cooks')
    suffix = 'view' if args.captures else 'final'
    reserved = [output / name for name in ('source-freeze.json', 'results.json', 'gpu-memory.json')]
    for scene in args.scenes:
        reserved.extend(output / (scene + '-' + suffix + ext) for ext in ('.log', '.bmp', '.png', '.json', '.bmp.json', '.json.csv', '.json.world.json', '.bmp.world.json'))
    if any(path.exists() for path in reserved):
        cli.error('output contains evidence from an earlier run; choose a fresh directory')
    output.mkdir(parents=True, exist_ok=True)
    environment, removed = native_environment()
    before_metadata, frozen = metadata(), fingerprints()
    require(metadata() == before_metadata, 'Inputs changed while preparing their fingerprints')
    preferences = preference_state()
    manifest = {'scope': 'Executable, launcher, engine sources, shaders, model/scenery/ground inputs; graphics.settings separately checked for preservation.',
                'sha256': frozen, 'preferences_before': preferences, 'removed_environment_keys': removed,
                'command_overrides': {'preset': args.preset, 'auto_quality': args.auto_quality, 'target_fps': 60, 'scale': args.scale}}
    (output / 'source-freeze.json').write_text(json.dumps(manifest, indent=2) + '\n')
    rows, fatal_errors = [], []
    monitor = MemoryMonitor()
    monitor.start()
    try:
        for scene in args.scenes:
            stem = output / (scene + '-' + suffix)
            command = [str(LAUNCHER), '--scenic', scene, '--width', str(WIDTH), '--height', str(HEIGHT),
                       '--frames', str(frames), '--no-hud', '--graphics-preset', args.preset,
                       '--auto-quality', args.auto_quality, '--target-fps', '60', '--capture', str(stem.with_suffix('.bmp'))]
            if args.scale is not None:
                command += ['--render-scale', format(args.scale, '.9g')]
            if args.menu:
                command += ['--graphics-menu']
            if not args.captures:
                command += ['--benchmark', '--view', 'sweep', '--warmup', str(WARMUP), '--profile', str(stem.with_suffix('.json'))]
            row = {'scene': scene, 'command': command, 'frames': frames, 'capture_only': args.captures, 'menu_visible': args.menu,
                   'auto_quality': args.auto_quality}
            print(f'Native graphics {args.preset}: {scene} ({suffix})', flush=True)
            try:
                with stem.with_suffix('.log').open('w') as log:
                    process = subprocess.run(command, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, check=False)
                row['returncode'] = process.returncode
                require(process.returncode == 0, f'Native launch exited{process.returncode}; see {stem.with_suffix(".log")}')
                row.update(validate(stem, args, frames))
            except (OSError, ValueError, KeyError, TypeError) as error:
                row.update(correctness_pass=False, error=str(error))
                # Keep valid timing evidence even if a separate renderer-state
                # invariant failed; do not erase a slow or defective scene.
                if not args.captures and stem.with_suffix('.json').is_file():
                    try:
                        timing = json.loads(stem.with_suffix('.json').read_text())
                        row['timing'] = timing
                        for field in ('mean_fps', 'one_percent_low_fps', 'p99_ms', 'max_ms', 'within_45fps_budget_percent', 'mean_render_total_gpu_ms'):
                            if field in timing:
                                row[field] = timing[field]
                        mean, low = timing.get('mean_fps'), timing.get('one_percent_low_fps')
                        row['strict_45fps_pass'] = finite_number(mean) and finite_number(low) and mean >= 45 and low >= 45
                    except (OSError, ValueError, TypeError) as profile_error:
                        row['profile_error'] = str(profile_error)
            rows.append(row)
            print(json.dumps({key: value for key, value in row.items() if key not in ('timing', 'command')}), flush=True)
            # Metadata checks are cheap between native runs. Full streaming
            # hashes happen before/after the batch, never during GPU rendering.
            if metadata() != before_metadata or preference_state() != preferences:
                fatal_errors.append('Runtime inputs or user preferences changed during the batch; remaining runs cancelled')
                break
    finally:
        monitor.stop()
        (output / 'gpu-memory.json').write_text(json.dumps(monitor.samples, indent=2) + '\n')
        try:
            require(fingerprints() == frozen, 'Frozen runtime input hashes changed')
            require(preference_state() == preferences, 'User graphics preferences changed; this tool did not restore/overwrite them')
        except (OSError, ValueError) as error:
            fatal_errors.append(str(error))
        timed = [row for row in rows if row.get('timing')]
        passed = [row['scene'] for row in timed if row['strict_45fps_pass']]
        failed = [row['scene'] for row in timed if not row['strict_45fps_pass']]
        summary = {'preset': args.preset, 'requested_scale': args.scale, 'target_fps': 60, 'auto_quality': args.auto_quality,
                   'capture_only': args.captures, 'menu_visible': args.menu, 'frames': frames,
                   'warmup_frames': 0 if args.captures else WARMUP, 'runs': rows,
                   'correctness_pass': len(rows) == len(args.scenes) and all(row.get('correctness_pass') for row in rows) and not fatal_errors,
                   'fatal_errors': fatal_errors, 'gpu_memory': monitor.summary(),
                   'preferences_preserved': preference_state() == preferences,
                   'strict_45fps_gate': None if args.captures else {'definition': 'mean_fps>=45 AND one_percent_low_fps>=45, separately from correctness',
                       'all_pass': len(timed) == len(args.scenes) and not failed, 'passed_scenes': passed, 'failed_scenes': failed},
                   'note': 'FPS gates never terminate later scenes. Captures do not measure FPS. Ordinary scale-transition frame costs remain in profiles; controller-ineligible frames are not deleted.'}
        (output / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
    return 0 if summary['correctness_pass'] else 1


if __name__ == '__main__':
    sys.exit(main())

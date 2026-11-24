#!/usr/bin/env python3
"""Read-only, input-free benchmark of an already running private candidate.

Run after placing the private game at a safe normal-play viewpoint, without a
video recorder. This collector never launches, focuses or controls the game and
never edits source, settings, saves or world data. Only new private evidence is
written. quality_dt is the flushed real frame interval used for timing; dt is
retained as simulation timing evidence, not substituted silently.
"""
from pathlib import Path
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import math
import os
import statistics
import sys
import time

N = Path(__file__).resolve().parents[1]
REVIEW_ROOT = (N / 'review').resolve()


class BenchmarkError(RuntimeError):
    pass


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def process_identity(pid, project):
    try:
        os.kill(pid, 0)
        proc = Path('/proc') / str(pid)
        command = [part.decode(errors='replace') for part in (proc / 'cmdline').read_bytes().split(b'\0') if part]
        status = (proc / 'stat').read_text().rsplit(') ', 1)[1].split()
        start_ticks = int(status[19])  # original proc stat field22
    except (OSError, ValueError, IndexError) as exc:
        raise BenchmarkError('Private game PID is no longer live/readable.') from exc
    if not command or '--project' not in command:
        raise BenchmarkError('PID does not identify a project-based game.')
    try:
        actual_project = Path(command[command.index('--project') + 1]).resolve()
        executable = Path(command[0]).resolve()
    except (IndexError, OSError) as exc:
        raise BenchmarkError('Invalid private game command line.') from exc
    if actual_project != project or not executable.is_relative_to(N.resolve()):
        raise BenchmarkError('PID does not run this candidate/private project.')
    return {'pid': pid, 'start_ticks': start_ticks, 'command': command}


def drm_devices():
    devices = []
    seen = set()
    for used in sorted(Path('/sys/class/drm').glob('card*/device/mem_info_vram_used')):
        device = used.parent.resolve()
        if device in seen:
            continue
        seen.add(device)
        try:
            total = int((device / 'mem_info_vram_total').read_text())
            current = int(used.read_text())
            vendor = (device / 'vendor').read_text().strip()
        except (OSError, ValueError):
            continue
        if total > 0 and current >= 0:
            devices.append({'card': used.parents[1].name, 'device': str(device),
                            'vendor': vendor, 'total_bytes': total,
                            'used_path': str(used)})
    if not devices:
        raise BenchmarkError('No readable DRM VRAM used/total counters are available.')
    return devices


def sample_vram(devices, elapsed):
    samples = []
    for device in devices:
        try:
            used = int(Path(device['used_path']).read_text())
        except (OSError, ValueError) as exc:
            raise BenchmarkError('DRM VRAM counter became unreadable.') from exc
        samples.append({'observed_seconds': round(elapsed, 6), 'utc': utc_now(),
                        'card': device['card'], 'used_bytes': used,
                        'total_bytes': device['total_bytes']})
    return samples


def percentile(values, q):
    ordered = sorted(values)
    position = (len(ordered) - 1) * q
    low = math.floor(position)
    high = math.ceil(position)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def collect(args):
    review = Path(args.review).resolve()
    if not review.is_relative_to(REVIEW_ROOT) or review == REVIEW_ROOT:
        raise BenchmarkError('Review must be a private child of this candidate review directory.')
    project = (review / 'project').resolve()
    marker = project / 'PRIVATE_CAPTURE_PROJECT.json'
    try:
        private = json.loads(marker.read_text()).get('private') is True
        pid = int((review / 'pid').read_text().strip())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        raise BenchmarkError('Readable private project marker and PID file are required.') from exc
    if not private or not project.is_relative_to(review):
        raise BenchmarkError('A real local private project is required; symlinked published projects are forbidden.')
    identity = process_identity(pid, project)
    trace = review / 'battlefield.jsonl'
    if not trace.is_file() or trace.is_symlink():
        raise BenchmarkError('A local flushed battlefield.jsonl observer is required.')
    settings = project / 'graphics.settings'
    try:
        settings_before = settings.read_bytes()
    except OSError as exc:
        raise BenchmarkError('Private graphics.settings is required.') from exc
    devices = drm_devices()
    if Path(args.output).name != args.output or args.output in ('', '.', '..'):
        raise BenchmarkError('--output must be one new directory name below the private review.')
    output = review / args.output
    if output.exists() or output.is_symlink():
        raise BenchmarkError('Output already exists; preserve evidence with a new --output name.')
    output.mkdir()
    (output / 'graphics.settings').write_bytes(settings_before)
    report = {'passed': False, 'error': None, 'collector_read_only': True,
              'sent_inputs': False, 'launched_game': False, 'private_project': str(project),
              'process': identity, 'trace': str(trace), 'requested_seconds': args.seconds,
              'expected_stage': args.expect_stage, 'minimum_frames': args.minimum_frames,
              'timing_field': 'quality_dt', 'percentile_method': 'linear interpolation, sorted frame intervals',
              'graphics_settings_sha256': digest(settings_before), 'drm_devices': devices}
    accepted = []
    vram = []
    excluded = Counter()
    observed_stages = Counter()
    durations = []
    simulation_durations = []
    render_sizes = Counter()
    prior_frame = None
    last_vram = -1.
    partial = b''
    error = None
    start = time.monotonic()
    report['start_utc'] = utc_now()
    try:
        with trace.open('rb') as stream, (output / 'frames.jsonl').open('w') as frames, (output / 'vram.jsonl').open('w') as memory:
            stat = os.fstat(stream.fileno())
            initial_offset = stream.seek(0, os.SEEK_END)
            report['trace_start_offset'] = initial_offset
            skip_partial = False
            if initial_offset:
                stream.seek(initial_offset - 1)
                skip_partial = stream.read(1) != b'\n'
                stream.seek(initial_offset)
            deadline = start + args.seconds
            while time.monotonic() < deadline:
                elapsed = time.monotonic() - start
                if process_identity(pid, project) != identity:
                    raise BenchmarkError('Private process identity changed during collection.')
                current = trace.stat()
                if (current.st_dev, current.st_ino) != (stat.st_dev, stat.st_ino) or current.st_size < stream.tell():
                    raise BenchmarkError('Observer was replaced or truncated during collection.')
                if elapsed - last_vram >= .25:
                    for row in sample_vram(devices, elapsed):
                        vram.append(row)
                        memory.write(json.dumps(row) + '\n')
                    memory.flush()
                    last_vram = elapsed
                chunk = stream.read()
                # No read occurs after the interval deadline; all accepted rows
                # were first observed within this collector's interval.
                observed = time.monotonic() - start
                if observed > args.seconds:
                    excluded['post_deadline_read'] += int(bool(chunk))
                    break
                if chunk:
                    pieces = (partial + chunk).split(b'\n')
                    partial = pieces.pop()
                    for raw in pieces:
                        if skip_partial:
                            skip_partial = False
                            excluded['initial_partial_row'] += 1
                            continue
                        if not raw:
                            continue
                        try:
                            row = json.loads(raw)
                        except (ValueError, UnicodeDecodeError):
                            excluded['malformed_row'] += 1
                            continue
                        stage = row.get('stage', 'missing')
                        observed_stages[stage] += 1
                        if row.get('paused') is not False:
                            excluded['paused'] += 1
                            continue
                        if row.get('loading') is not False:
                            excluded['loading'] += 1
                            continue
                        if row.get('screen') != 1 or stage in ('inactive', 'missing'):
                            excluded['title_or_nonplay'] += 1
                            continue
                        if args.expect_stage and stage != args.expect_stage:
                            excluded['unexpected_stage'] += 1
                            continue
                        frame = row.get('frame')
                        interval = row.get('quality_dt')
                        size = row.get('render_size')
                        if not isinstance(frame, int) or isinstance(frame, bool) or not isinstance(interval, (int, float)) or not math.isfinite(interval) or interval <= 0 or interval > 10:
                            excluded['missing_or_invalid_flushed_timing'] += 1
                            continue
                        if not isinstance(size, list) or len(size) != 2 or any(not isinstance(x, int) or x <= 0 for x in size):
                            excluded['missing_or_invalid_render_size'] += 1
                            continue
                        if prior_frame is not None and frame <= prior_frame:
                            raise BenchmarkError('Observer frame identity repeated/reset during collection.')
                        prior_frame = frame
                        row = dict(row, _observed_seconds=round(observed, 6))
                        accepted.append(row)
                        durations.append(float(interval))
                        if isinstance(row.get('dt'), (int, float)) and math.isfinite(row['dt']):
                            simulation_durations.append(float(row['dt']))
                        render_sizes[f'{size[0]}x{size[1]}'] += 1
                        frames.write(json.dumps(row) + '\n')
                    frames.flush()
                time.sleep(min(.05, max(0., deadline - time.monotonic())))
            report['trace_end_offset'] = stream.tell()
            for row in sample_vram(devices, time.monotonic() - start):
                vram.append(row)
                memory.write(json.dumps(row) + '\n')
        if process_identity(pid, project) != identity:
            raise BenchmarkError('Private process identity changed at collection end.')
        settings_after = settings.read_bytes()
        report['graphics_settings_end_sha256'] = digest(settings_after)
        if settings_after != settings_before:
            (output / 'graphics-end.settings').write_bytes(settings_after)
            raise BenchmarkError('Graphics settings changed during collection; mixed profiles are not accepted.')
        if len(accepted) < args.minimum_frames:
            raise BenchmarkError(f'Insufficient normal-play frames: {len(accepted)} < {args.minimum_frames}.')
        coverage = accepted[-1]['_observed_seconds'] - accepted[0]['_observed_seconds']
        if coverage < args.seconds * .8:
            raise BenchmarkError(f'Insufficient observed interval coverage: {coverage:.3f}s.')
        if sum(durations) < args.seconds * .65:
            raise BenchmarkError('Timing sum is too short for the requested interval; observer may be sparse or clamped.')
        if excluded['unexpected_stage']:
            raise BenchmarkError('Expected safe gameplay stage changed during the benchmark.')
        report['passed'] = True
    except (BenchmarkError, OSError) as exc:
        error = str(exc)
    finally:
        report['end_utc'] = utc_now()
        report['actual_seconds'] = round(time.monotonic() - start, 6)
        report['error'] = error
        report['frames'] = len(accepted)
        report['excluded_rows'] = dict(excluded)
        report['observed_stages'] = dict(observed_stages)
        report['render_sizes'] = dict(render_sizes)
        if durations:
            milliseconds = [dt * 1000 for dt in durations]
            report['timing'] = {'sum_frame_seconds': sum(durations),
                                'mean_fps': len(durations) / sum(durations),
                                'mean_ms': statistics.mean(milliseconds),
                                'p50_ms': percentile(milliseconds, .50),
                                'p95_ms': percentile(milliseconds, .95),
                                'p99_ms': percentile(milliseconds, .99),
                                'maximum_ms': max(milliseconds),
                                'frames_below_30fps': sum(dt > 1 / 30 for dt in durations),
                                'frames_below_45fps': sum(dt > 1 / 45 for dt in durations),
                                'sum_simulation_dt_seconds': sum(simulation_durations),
                                'first_frame': accepted[0]['frame'], 'last_frame': accepted[-1]['frame'],
                                'observed_coverage_seconds': accepted[-1]['_observed_seconds'] - accepted[0]['_observed_seconds']}
        report['vram'] = {}
        for device in devices:
            values = [row['used_bytes'] for row in vram if row['card'] == device['card']]
            if values:
                report['vram'][device['card']] = {'samples': len(values), 'total_bytes': device['total_bytes'],
                                                'start_used_bytes': values[0], 'end_used_bytes': values[-1],
                                                'maximum_used_bytes': max(values), 'mean_used_bytes': statistics.mean(values)}
        (output / 'benchmark.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({'passed': report['passed'], 'report': str(output / 'benchmark.json'),
                          'frames': len(accepted), 'error': error}, indent=2), flush=True)
    return 0 if report['passed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--review', default=str(N / 'review/live-battlefield'))
    parser.add_argument('--output', required=True, help='New directory name within the private review.')
    parser.add_argument('--seconds', type=float, default=30.)
    parser.add_argument('--minimum-frames', type=int, default=120)
    parser.add_argument('--expect-stage', default='', help='Recommended: flee_to_ambush for a stationary safe-view comparison.')
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or not 5 <= args.seconds <= 60 or not 20 <= args.minimum_frames <= 10000:
        parser.error('Use5–60 seconds and20–10000 minimum frames.')
    try:
        return collect(args)
    except (BenchmarkError, OSError) as exc:
        print(json.dumps({'passed': False, 'error': str(exc)}), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())

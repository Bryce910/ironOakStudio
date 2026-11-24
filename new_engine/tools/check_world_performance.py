#!/usr/bin/env python3
"""Audit complete frame records and export counters, never invented GPU spans."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import tempfile


def finite(value, name, positive=False):
    value = float(value)
    if not math.isfinite(value) or value < 0 or (positive and value == 0):
        raise ValueError(f'Invalid {name}')
    return value


def integer(value, name):
    result = int(value)
    if str(result) != str(value) or result < 0:
        raise ValueError(f'Invalid {name}')
    return result


def audit(path, minimum=45., allowed_misses=0, minimum_samples=120):
    if not math.isfinite(minimum) or minimum <= 0 or allowed_misses < 0 or minimum_samples < 1:
        raise ValueError('Invalid performance budget')
    path = Path(path)
    meta = json.loads(path.read_text())
    count = integer(meta['frames'], 'frame count')
    warmup = integer(meta['warmup_frames'], 'warmup')
    if count > 1_000_000 or warmup >= count:
        raise ValueError('Frame count exceeds bounded audit or warmup consumes run')
    if integer(meta['dropped_samples'], 'dropped samples') != 0:
        raise ValueError('Recorder dropped samples; run cannot pass')
    rows = []
    with Path(str(path) + '.csv').open(newline='') as stream:
        for expected, row in enumerate(csv.DictReader(stream), 1):
            if expected > count or integer(row['frame'], 'frame') != expected:
                raise ValueError('Missing, duplicate, reordered or excess CSV frame')
            # engine_perf_frame has no preceding interval on its very first
            # warmup row. No zero interval may enter measured statistics.
            row['frame_ms'] = finite(row['frame_ms'], 'frame interval', expected > warmup or expected != 1)
            for name in ('raw_update_ms', 'raw_render_cpu_ms', 'raw_editor_ms', 'present_ms',
                         'shadow_gpu_ms', 'render_total_gpu_ms', 'forest_gpu_ms', 'reflection_gpu_ms'):
                if name in row:
                    row[name] = finite(row[name], name)
            row['frame'] = expected
            rows.append(row)
    if len(rows) != count or integer(meta['samples'], 'samples') != count - warmup:
        raise ValueError('CSV/metadata sample counts disagree')
    measured = rows[warmup:]
    if len(measured) < minimum_samples:
        raise ValueError('Too few measured samples')
    values = sorted(row['frame_ms'] for row in measured)
    slow_count = math.ceil(len(values) * .01)
    average = 1000 * len(values) / sum(values)
    low = 1000 * slow_count / sum(values[-slow_count:])
    budget = 1000 / minimum
    misses = [row for row in measured if row['frame_ms'] > budget]
    streak = longest = 0
    for row in measured:
        streak = streak + 1 if row['frame_ms'] > budget else 0
        longest = max(streak, longest)
    renderer = str(meta.get('renderer', ''))
    native = bool(renderer) and not any(token in renderer.lower() for token in
                                      ('llvmpipe', 'softpipe', 'swrast', 'software'))
    result = dict(profile=str(path.resolve()), renderer=renderer, native_renderer=native,
                  recorded_frames=count, warmup_frames=warmup, samples=len(measured),
                  mean_fps=average, one_percent_low_fps=low, worst_frame_ms=values[-1],
                  frame_budget_ms=budget, frames_over_budget=len(misses),
                  longest_over_budget_streak=longest, allowed_budget_misses=allowed_misses,
                  every_frame_within_budget=not misses,
                  statistical_target_pass=average >= minimum and low >= minimum,
                  worst_frames=sorted(measured, key=lambda row: row['frame_ms'], reverse=True)[:10])
    sidecar = Path(str(path) + '.world.json')
    errors = []
    if sidecar.exists():
        world = json.loads(sidecar.read_text())
        for key in ('gl_error', 'failed_tiles', 'remaining_texture_references'):
            if key in world and world[key] != 0:
                errors.append(f'{key}={world[key]}')
        result['graphics'] = world.get('graphics')
    else:
        result['world_correctness_note'] = 'No world sidecar; frame audit alone does not validate rendering.'
    result['correctness_errors'] = errors
    result['pass'] = native and not errors and result['statistical_target_pass'] and len(misses) <= allowed_misses
    return result, rows, meta


def trace_events(rows, meta):
    # The recorder supplies per-frame counters, not exact CPU job start/end
    # stamps. GPU stages are delayed/smoothed. Do not mislabel them as spans.
    events = [dict(name='process_name', ph='M', pid=1, tid=1,
                   args=dict(name='Valwood frame counters (not execution spans)'))]
    now = 0.
    gpu = bool(meta.get('gpu_stage_timing_available', False))
    for row in rows:
        now += row['frame_ms'] * 1000
        args = {key: row[key] for key in ('frame_ms', 'raw_update_ms', 'raw_render_cpu_ms',
                                         'raw_editor_ms', 'present_ms') if key in row}
        events.append(dict(name='Frame / CPU / preceding swap (ms)', ph='C', pid=1, tid=1, ts=now, args=args))
        if gpu:
            args = {key: row[key] for key in ('render_total_gpu_ms', 'forest_gpu_ms',
                                             'shadow_gpu_ms', 'reflection_gpu_ms') if key in row}
            events.append(dict(name='Delayed smoothed GPU estimates (ms)', ph='C', pid=1, tid=2, ts=now, args=args))
    return dict(traceEvents=events, displayTimeUnit='ms',
                otherData=dict(note='Counter timeline only; GPU stages asynchronous, present_ms is preceding swap. Warmup retained.'))


def save_new(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='.valwood-report-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as stream:
            json.dump(value, stream, indent=2, allow_nan=False)
            stream.write('\n'); stream.flush(); os.fsync(stream.fileno())
        # Link publication fails if destination exists; no silent overwrite.
        os.link(temporary, path)
    finally:
        os.unlink(temporary)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', nargs='+', type=Path)
    parser.add_argument('--minimum-fps', type=float, default=45.)
    parser.add_argument('--allowed-budget-misses', type=int, default=0,
                        help='Default 0 enforces every retained frame; any relaxation is explicit in output')
    parser.add_argument('--minimum-samples', type=int, default=120)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--trace', type=Path, help='One input only; Chrome Trace compatible counter export')
    args = parser.parse_args(argv)
    if not math.isfinite(args.minimum_fps) or args.minimum_fps <= 0 or args.allowed_budget_misses < 0 or args.minimum_samples < 1:
        parser.error('Invalid performance budget')
    if args.trace and len(args.profiles) != 1:
        parser.error('--trace requires one profile')
    try:
        reports = []
        for path in args.profiles:
            report, rows, meta = audit(path, args.minimum_fps, args.allowed_budget_misses, args.minimum_samples)
            reports.append(report)
            if args.trace:
                save_new(args.trace, trace_events(rows, meta))
        result = dict(schema_version=1, pass_all=all(report['pass'] for report in reports), profiles=reports)
        if args.output:
            save_new(args.output, result)
        print(json.dumps(result, allow_nan=False))
        return 0 if result['pass_all'] else 1
    except (OSError, ValueError, KeyError, TypeError, csv.Error, json.JSONDecodeError) as error:
        print(json.dumps(dict(error=str(error))))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())

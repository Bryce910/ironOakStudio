#!/usr/bin/env python3
"""Analyze actual post-presentation phase intervals without hiding hitches.

Baseline CSV has no equivalent phase labels and is mixed-run context only.
This tool neither launches the game nor changes scene/runtime data.
"""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import io
import json
import math
from pathlib import Path

N = Path(__file__).resolve().parents[1]
FLAGS = ("normal_playable", "controls", "loading", "modal", "cinematic",
         "editor", "inventory", "paused")
NUMBERS = ("interval_ms", "work_ms", "capture_hooks_ms", "x", "z", "present_ms")
THRESHOLDS = (16.666667, 22.222222, 33., 50., 100., 250., 1000.)


def digest(path):
    with Path(path).open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def distribution(values):
    data = sorted(values)
    if not data:
        return dict(samples=0, duration_seconds=0., mean_ms=None, mean_fps=None,
                    p50_ms=None, p95_ms=None, p99_ms=None, max_ms=None,
                    threshold_counts={str(t): 0 for t in THRESHOLDS})
    def percentile(q):
        return data[max(0, math.ceil(len(data)*q)-1)]
    total = sum(data)
    return dict(samples=len(data), duration_seconds=total/1000,
                mean_ms=total/len(data), mean_fps=1000*len(data)/total,
                p50_ms=percentile(.5), p95_ms=percentile(.95),
                p99_ms=percentile(.99), max_ms=data[-1],
                threshold_counts={str(t): sum(v > t for v in data) for t in THRESHOLDS})


def parse_phases(text):
    rows, issues, flag_disagreements = [], [], []
    previous = dict(controls=False, loading=True, modal=True)
    previous_frame = None
    for line_number, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
            if not isinstance(row.get("frame"), int) or isinstance(row["frame"], bool):
                raise ValueError("frame is not an integer")
            for key in FLAGS:
                if not isinstance(row.get(key), bool):
                    raise ValueError(key+" is not boolean")
            for key in NUMBERS:
                if isinstance(row.get(key), bool) or not isinstance(row.get(key), (int, float)) or not math.isfinite(row[key]):
                    raise ValueError(key+" is not finite numeric")
            if row["interval_ms"] <= 0 or min(row["work_ms"], row["capture_hooks_ms"], row["present_ms"]) < 0:
                raise ValueError("nonpositive interval or negative stage duration")
            if previous_frame is not None and row["frame"] != previous_frame+1:
                issues.append(dict(line=line_number, reason="nonconsecutive frame ID", frame=row["frame"], previous=previous_frame))
            expected = (row["controls"] and previous["controls"] and
                        not row["loading"] and not previous["loading"] and
                        not row["modal"] and not previous["modal"] and
                        row["capture_hooks_ms"] < 1.)
            if row["normal_playable"] != expected:
                flag_disagreements.append(dict(frame=row["frame"], native=row["normal_playable"], independently_recomputed=expected))
            row = dict(row, previous_controls=previous["controls"],
                       previous_loading=previous["loading"], previous_modal=previous["modal"])
            rows.append(row)
            previous, previous_frame = row, row["frame"]
        except (ValueError, TypeError, KeyError) as error:
            issues.append(dict(line=line_number, reason=str(error)))
    return rows, issues, flag_disagreements


def route_points(path):
    points = []
    for line in path.read_text().splitlines():
        q = line.split()
        if q and q[0] == "point":
            points.append((float(q[1]), float(q[3])))
    if len(points) != 6:
        raise ValueError("Expected actual six-point traffic route")
    return points


def route_location(x, z, points):
    station = 0.
    best = (float("inf"), 0.)
    for a, b in zip(points[:-1], points[1:]):
        dx, dz = b[0]-a[0], b[1]-a[1]
        length = math.hypot(dx, dz)
        if length <= 0:
            raise ValueError("Degenerate route segment")
        t = max(0., min(1., ((x-a[0])*dx+(z-a[1])*dz)/(length*length)))
        distance = math.hypot(x-a[0]-t*dx, z-a[1]-t*dz)
        if distance < best[0]:
            best = distance, station+t*length
        station += length
    return dict(distance_m=best[0], station_m=best[1])


def analyze_rows(rows, points):
    groups = collections.defaultdict(list)
    partition = collections.defaultdict(list)
    bins = collections.defaultdict(list)
    xlo, xhi = min(p[0] for p in points)-100, max(p[0] for p in points)+100
    zlo, zhi = min(p[1] for p in points)-100, max(p[1] for p in points)+100
    for row in rows:
        groups["all_actual_intervals"].append(row)
        capture = row["capture_hooks_ms"] >= 1.
        loading = row["loading"] or row["previous_loading"]
        modal = row["modal"] or row["previous_modal"]
        if capture:
            groups["capture_hooks_ge_1ms"].append(row)
        if loading:
            groups["loading_or_loading_transition"].append(row)
        if modal:
            groups["modal_or_modal_transition"].append(row)
        for key in ("cinematic", "editor", "inventory", "paused"):
            if row[key]:
                groups[key+"_current_flag"].append(row)
        if row["normal_playable"]:
            groups["normal_playable"].append(row)
            if xlo <= row["x"] <= xhi and zlo <= row["z"] <= zhi:
                groups["outdoor_route_region_normal_playable"].append(row)
            location = route_location(row["x"], row["z"], points)
            if location["distance_m"] <= 40:
                groups["route_corridor_40m_normal_playable"].append(row)
                bins[int(location["station_m"]//50)*50].append(row)
                if math.hypot(row["x"]-points[0][0], row["z"]-points[0][1]) <= 30:
                    groups["camp_30m_normal_playable"].append(row)
                elif math.hypot(row["x"]-points[-1][0], row["z"]-points[-1][1]) <= 45:
                    groups["town_45m_normal_playable"].append(row)
                else:
                    groups["road_between_camp_and_town_normal_playable"].append(row)
        # Explicit priority makes the partition additive. Nonexclusive flags
        # above retain overlaps and must not be summed.
        category = ("capture_hooks_ge_1ms" if capture else
                    "loading_or_loading_transition" if loading else
                    "modal_or_modal_transition" if modal else
                    "normal_playable" if row["normal_playable"] else
                    "controls_unavailable_or_transition")
        partition[category].append(row)
    summarize = lambda source: {key: distribution([row["interval_ms"] for row in value]) for key, value in source.items()}
    for key in ("normal_playable", "outdoor_route_region_normal_playable", "route_corridor_40m_normal_playable",
                "road_between_camp_and_town_normal_playable", "loading_or_loading_transition", "modal_or_modal_transition", "capture_hooks_ge_1ms"):
        groups.setdefault(key, [])
    for key in ("capture_hooks_ge_1ms","loading_or_loading_transition","modal_or_modal_transition","normal_playable","controls_unavailable_or_transition"):
        partition.setdefault(key,[])
    worst = []
    for row in sorted(rows, key=lambda r: r["interval_ms"], reverse=True)[:25]:
        worst.append(dict(row, route_location=route_location(row["x"], row["z"], points)))
    return dict(nonexclusive_groups=summarize(groups), additive_partition=summarize(partition),
                partition_sample_count=sum(len(value) for value in partition.values()),
                spatial_50m_bins=[dict(station_begin_m=key, station_end_m=key+50,
                    limited_sample=len(value)<120, **distribution([row["interval_ms"] for row in value])) for key, value in sorted(bins.items())],
                outdoor_xz_bounds=[xlo,zlo,xhi,zhi], worst_25_intervals=worst)


def mixed_csv(directory):
    path = directory/"performance.json.csv"
    if not path.exists():
        return dict(available=False)
    values, invalid = [], 0
    for row in csv.DictReader(io.StringIO(path.read_text())):
        try:
            value = float(row["frame_ms"])
            if not math.isfinite(value) or value <= 0:
                invalid += 1
            else:
                values.append(value)
        except (ValueError, KeyError):
            invalid += 1
    return dict(available=True, path=str(path), sha256=digest(path), invalid_or_nonpositive_intervals=invalid,
                all_positive_intervals=distribution(values), phase_split_available=False,
                scope="Original profiler boundaries; mixed loading/menu/cinematic/QA work. No phase-based exclusions.")


def self_test():
    def row(frame, **change):
        result=dict(frame=frame,interval_ms=20.,work_ms=19.,capture_hooks_ms=.1,
                    normal_playable=frame>0,controls=True,loading=False,modal=False,
                    cinematic=False,editor=False,inventory=False,paused=False,x=0.,z=5.,present_ms=1.)
        result.update(change)
        return result
    inputs=[row(0),row(1),row(2,interval_ms=1200.),row(3,interval_ms=2000.,normal_playable=False,capture_hooks_ms=1900.),
            row(4,normal_playable=False,loading=True),row(5,normal_playable=False),row(6)]
    rows,issues,disagree=parse_phases("\n".join(map(json.dumps,inputs)))
    assert not issues and not disagree
    result=analyze_rows(rows,[(0.,0.),(0.,10.),(0.,20.),(0.,30.),(0.,40.),(0.,50.)])
    assert result['partition_sample_count']==len(rows)
    assert result['nonexclusive_groups']['normal_playable']['max_ms']==1200.
    assert result['nonexclusive_groups']['all_actual_intervals']['max_ms']==2000.
    assert result['nonexclusive_groups']['capture_hooks_ge_1ms']['samples']==1
    assert result['nonexclusive_groups']['loading_or_loading_transition']['samples']==3  # initial+entry+exit
    assert distribution([1.,2.,3.,1000.])['p95_ms']==1000.
    assert route_location(3,5,[(0,0),(0,10)])==dict(distance_m=3.,station_m=5.)
    assert parse_phases(json.dumps(row(0,normal_playable=True)))[2]
    assert parse_phases(json.dumps(row(0,interval_ms=float('nan'))))[1]
    assert parse_phases(json.dumps(row(0))+"\n"+json.dumps(row(0)))[1]
    return 10


def build(directory, baseline):
    inputs={}
    phase=directory/"performance.json.phases.jsonl"
    execution=directory/"EXECUTION.json"
    source=N/"source-v1/engine/world_frame_phase_profile.inc"
    route=N/"world-project/opening/road-traffic.settings"
    for p in (source,route,Path(__file__).resolve(),N/"run_native_review.py"):
        inputs[str(p)]=digest(p)
    report=dict(schema="valwood.route-phase-analysis.v1",candidate=str(N),directory=str(directory),
                passed=False,available=phase.exists(),self_tests=self_test(),
                baseline_mixed_context=mixed_csv(baseline),candidate_mixed_context=mixed_csv(directory),
                measured_performance_gain_claim_available=False,performance_budget_passed=None,
                comparison_limits=["Baseline has no phase logger and cannot establish a matched normal-playable comparison.",
                    "Baseline compilation overlapped part of the early run; do not attribute mixed-run differences to this change.",
                    "External compositor screenshots remain in timings; their manifests do not provide exact synchronized exclusion intervals."],
                definitions=dict(percentile="Nearest rank ceil(q*N)-1; all finite positive intervals retained, including hitches.",
                    normal_playable="Native conservative current+previous controls, loading/modal states, capture_hooks_ms<1; independently recomputed.",
                    capture_hooks="Measured QA-hook/engine_launch_frame block >=1ms, not an explicit screenshot-count flag or attribution of the entire interval.",
                    partition_priority=["capture_hooks_ge_1ms","loading_or_loading_transition","modal_or_modal_transition","normal_playable","controls_unavailable_or_transition"],
                    outdoor="XZ route rectangle plus100m; no height/water/combat-state discrimination.",
                    corridor="Actual authored six-point traffic polyline, <=40m distance; 50m station bins include return travel.",
                    boundary="After QA hooks, presentation and optional pacing; logger stdio overhead itself is outside the next measured interval."),
                inputs_sha256=inputs,external_capture_manifests=[])
    for d in (baseline,directory):
        for name in ('performance.json.csv','performance.json','EXECUTION.json'):
            p=d/name
            if p.exists():inputs[str(p)]=digest(p)
        for p in sorted(d.glob("*EXTERNAL*.json")):
            report['external_capture_manifests'].append(dict(path=str(p),sha256=digest(p),data=json.loads(p.read_text())))
            inputs[str(p)]=digest(p)
    if not phase.exists():
        report['status']='pending_phase_log';return report
    raw=phase.read_bytes();inputs[str(phase)]=hashlib.sha256(raw).hexdigest()
    rows,issues,disagree=parse_phases(raw.decode())
    report.update(analyze_rows(rows,route_points(route)))
    report.update(parse_issues=issues,normal_flag_disagreements=disagree,valid_intervals=len(rows))
    native=json.loads(execution.read_text()) if execution.exists() else {}
    if execution.exists():inputs[str(execution)]=digest(execution)
    report['native_run_completed_and_passed']=native.get('passed') is True and native.get('exit_code')==0 and not native.get('timed_out',False)
    report['native_binary_sha256']=native.get('binary_sha256')
    runtime=N/'runtime/rpg'
    report['native_matches_current_candidate_binary']=runtime.is_file() and native.get('binary_sha256')==digest(runtime)
    if runtime.is_file():inputs[str(runtime)]=digest(runtime)
    report['phase_log_stable_during_analysis']=digest(phase)==inputs[str(phase)]
    report['passed']=bool(rows) and not issues and not disagree and report['native_run_completed_and_passed'] and report['native_matches_current_candidate_binary'] and report['phase_log_stable_during_analysis']
    report['status']='complete' if report['passed'] else 'incomplete_or_invalid'
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',nargs='?',type=Path,default=N/'review/journey-final')
    parser.add_argument('--baseline',type=Path,default=N/'review/baseline-journey')
    parser.add_argument('--output',type=Path,default=N/'review/route-budget/ROUTE_PHASE_REVIEW.json')
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:print('Phase analysis self-tests passed:',self_test());return
    if (N/'UNIFIED_READY.json').exists():raise ValueError('Published generation is immutable')
    if not args.output.resolve().is_relative_to((N/'review').resolve()):raise ValueError('Report must remain inside candidate review')
    report=build(args.directory.resolve(),args.baseline.resolve())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({key:report.get(key) for key in ['passed','status','valid_intervals','native_run_completed_and_passed','measured_performance_gain_claim_available']},indent=2))
    raise SystemExit(0 if report['passed'] else 2)


if __name__=='__main__':main()

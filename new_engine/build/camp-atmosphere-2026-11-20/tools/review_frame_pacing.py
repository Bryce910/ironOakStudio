#!/usr/bin/env python3
"""Read frame timings without inventing gameplay/loading classifications.

Import API: build_report(directory, profile=None, world=None) -> dict.
CLI: review_frame_pacing.py REVIEW_DIR [--profile PROFILE_OR_CSV]
     [--world WORLD_JSON] [--output REPORT_JSON]

No timing rows are excluded because of checkpoint captures. The following
profiler row is listed only as a possible association: runtime capture happens
AFTER engine_perf_frame(), while its next invocation measures the intervening
interval. Independent scene/profiler counters do not establish an exact join.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
from typing import Any

SCHEMA = "valwood.frame-pacing-evidence.v1"
UNAVAILABLE_LABELS = [
    "per_frame_player_controls_allowed",
    "per_frame_destination_loading",
    "per_frame_modal_state",
    "per_frame_cinematic_state",
    "per_frame_capture_work",
    "complete_capture_frame_ids",
]


def _sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _read_object(path: Path) -> tuple[dict[str, Any] | None, str | None]:
    if not path.is_file():
        return None, None
    try:
        value = json.loads(path.read_text())
        return (value, None) if isinstance(value, dict) else (None, "JSON root is not an object")
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return None, str(error)


def _stats(values: list[float]) -> dict[str, Any]:
    if not values:
        return {"samples": 0, "mean_ms": None, "mean_fps": None,
                "median_ms": None, "p95_ms": None, "p99_ms": None,
                "max_ms": None, "one_percent_low_fps": None}
    ordered = sorted(values)
    count = len(values)
    mean = math.fsum(values) / count
    low_count = math.ceil(count * .01)
    slow_mean = math.fsum(ordered[-low_count:]) / low_count

    def percentile(fraction: float) -> float:
        return ordered[max(0, math.ceil(count * fraction) - 1)]

    over45 = sum(value > 1000 / 45 for value in values)
    return {
        "samples": count,
        "measured_interval_seconds": math.fsum(values) / 1000,
        "mean_ms": mean,
        "mean_fps": 1000 / mean,
        "median_ms": percentile(.5),
        "p95_ms": percentile(.95),
        "p99_ms": percentile(.99),
        "max_ms": ordered[-1],
        "one_percent_low_fps": 1000 / slow_mean,
        "one_percent_slowest_sample_count": low_count,
        "frames_over_45fps_budget": over45,
        "frames_over_30fps_budget": sum(value > 1000 / 30 for value in values),
        "within_45fps_budget_percent": 100 * (count - over45) / count,
    }


def build_report(directory: str | Path, profile: str | Path | None = None,
                 world: str | Path | None = None) -> dict[str, Any]:
    """Return overall timings and sparse capture correlations; never filter them.

    Relative explicit profile/world paths are resolved against REVIEW_DIR.
    Passing a CSV as profile also looks for the JSON path with `.csv` removed.
    Missing optional JSON is reported as unavailable, without fabricating data.
    """
    directory = Path(directory).resolve()

    def supplied(value: str | Path | None, default: Path) -> Path:
        path = Path(value) if value is not None else default
        return (directory / path).resolve() if not path.is_absolute() else path.resolve()

    profile_path = supplied(profile, directory / "performance.json")
    if profile_path.suffix.lower() == ".csv":
        csv_path = profile_path
        profile_path = Path(str(csv_path)[:-4])
    else:
        csv_path = Path(str(profile_path) + ".csv")
    world_path = supplied(world, Path(str(profile_path) + ".world.json"))
    profile_data, profile_error = _read_object(profile_path)
    world_data, world_error = _read_object(world_path)
    inputs: dict[str, str] = {}
    warnings: list[str] = []
    for path in (profile_path, world_path):
        if path.is_file():
            inputs[str(path)] = _sha(path)
    if profile_error:
        warnings.append(f"Profile JSON could not be used: {profile_error}")
    if world_error:
        warnings.append(f"World JSON could not be used: {world_error}")
    rows: list[dict[str, float | int]] = []
    invalid_rows: list[dict[str, Any]] = []
    columns: list[str] = []
    csv_error: str | None = None
    if csv_path.is_file():
        inputs[str(csv_path)] = _sha(csv_path)
        try:
            with csv_path.open(newline="") as stream:
                reader = csv.DictReader(stream)
                columns = list(reader.fieldnames or [])
                if not {"frame", "frame_ms"}.issubset(columns):
                    raise ValueError("CSV requires frame and frame_ms columns")
                for line, row in enumerate(reader, 2):
                    try:
                        frame = int(row["frame"])
                        interval = float(row["frame_ms"])
                        if frame < 1 or not math.isfinite(interval) or interval < 0:
                            raise ValueError("invalid frame ID or non-finite/negative interval")
                        parsed: dict[str, float | int] = {"frame": frame, "frame_ms": interval}
                        for key in columns:
                            if key not in parsed:
                                try:
                                    value = float(row[key])
                                    if math.isfinite(value):
                                        parsed[key] = value
                                except (TypeError, ValueError):
                                    pass
                        rows.append(parsed)
                    except (TypeError, ValueError, KeyError) as error:
                        invalid_rows.append({"csv_line": line, "reason": str(error)})
        except (OSError, UnicodeError, ValueError, csv.Error) as error:
            csv_error = str(error)
    else:
        csv_error = "Timing CSV is missing"
    by_frame: dict[int, list[dict[str, float | int]]] = {}
    for row in rows:
        by_frame.setdefault(int(row["frame"]), []).append(row)
    duplicate_ids = sorted(frame for frame, matches in by_frame.items() if len(matches) > 1)
    if duplicate_ids:
        warnings.append("Duplicate profiler frame IDs make capture correlations ambiguous.")
    warmup = (profile_data or {}).get("warmup_frames")
    warmup_available = isinstance(warmup, int) and not isinstance(warmup, bool) and warmup >= 0
    all_positive = [float(row["frame_ms"]) for row in rows if row["frame_ms"] > 0]
    post_warmup = [float(row["frame_ms"]) for row in rows
                   if row["frame_ms"] > 0 and int(row["frame"]) > warmup] if warmup_available else None

    captures: list[dict[str, Any]] = []
    sidecar_stems: set[str] = set()
    skipped = {profile_path, world_path}
    for path in sorted(directory.glob("*.json")):
        if path.resolve() in skipped:
            continue
        data, _ = _read_object(path)
        if not data:
            continue
        frame, name = data.get("frame"), data.get("name")
        if not (isinstance(frame, int) and not isinstance(frame, bool) and frame >= 1
                and isinstance(name, str) and name):
            continue
        inputs[str(path)] = _sha(path)
        sidecar_stems.add(path.stem)
        candidate = frame + 1
        matches = by_frame.get(candidate, [])
        snapshot = {key: data[key] for key in ("journey_stage", "combat_state", "third_person",
                                               "grounded", "inventory_open") if key in data}
        captures.append({
            "name": name, "sidecar": str(path), "capture_scene_frame": frame,
            "candidate_following_profiler_frame": candidate,
            "profiler_rows_at_candidate_frame": len(matches),
            "following_interval_ms": matches[0]["frame_ms"] if len(matches) == 1 else None,
            "checkpoint_state_snapshot": snapshot,
            "association_only": True,
        })
    # BMP is the native capture format. Converted PNGs are not counted again.
    unlabelled_images = sorted(str(path) for path in directory.glob("*.bmp")
                              if path.stem not in sidecar_stems)
    loading: dict[str, Any] = {"available": world_data is not None,
                               "frame_ranges_available": False,
                               "counts_are_not_additive": True}
    if world_data is not None:
        for key in ("travel_loading_frames", "longest_travel_loading_streak"):
            if key in world_data:
                loading[key] = world_data[key]
        readiness = world_data.get("destination_readiness")
        if isinstance(readiness, dict):
            loading["destination_readiness"] = readiness
        loading["note"] = "Aggregate counts lack profiler frame IDs; readiness reasons can overlap."

    report: dict[str, Any] = {
        "schema": SCHEMA,
        "directory": str(directory),
        "available": bool(rows),
        "playable_loading_split_available": False,
        "normal_playable_frame_pacing_claim_available": False,
        "unavailable_labels": UNAVAILABLE_LABELS.copy(),
        "inputs": inputs,
        "csv": {"path": str(csv_path), "columns": columns, "rows": len(rows),
                "zero_intervals_not_used_for_statistics": sum(row["frame_ms"] == 0 for row in rows),
                "invalid_rows": invalid_rows, "duplicate_frame_ids": duplicate_ids,
                "error": csv_error},
        "overall_csv_intervals": {
            "scope": "All valid positive CSV intervals, including loading, menus, cinematics and QA work when present.",
            "percentile_method": "nearest-rank, matching the native profiler",
            "one_percent_low_method": "1000 / mean of the slowest ceil(1% of samples) frame intervals",
            "all_positive_intervals": _stats(all_positive),
            "declared_warmup_frames": warmup if warmup_available else None,
            "after_declared_warmup": _stats(post_warmup) if post_warmup is not None else None,
            "capture_or_state_based_exclusions_applied": False,
        },
        "checkpoint_capture_correlations": {
            "known_checkpoint_sidecars": len(captures),
            "records": captures,
            "native_bmp_files_without_frame_sidecars": unlabelled_images,
            "alignment_verified": False,
            "alignment_caveat": "Scene frame IDs and profiler frame IDs are independent counters. Their alignment is not explicitly recorded; frame F+1 is only a candidate association.",
            "timing_boundary_caveat": "engine_perf_frame runs before checkpoint readback/file writes. Its next timing interval can include that capture work, so excluding only CSV frame F would be wrong.",
            "causal_attribution_available": False,
            "adjusted_statistics_produced": False,
            "note": "Sparse correlations do not identify every capture, loading interval or modal state, and do not measure capture cost in isolation.",
        },
        "aggregate_loading": loading,
        "limitations": [
            "No normal-playable-only timing distribution can be derived from these sparse checkpoint snapshots.",
            "Intervals are never discarded because they are slow; ordinary gameplay hitches remain in overall results.",
            "GPU stage snapshots are delayed estimates and cannot classify individual frames; present_ms refers to the preceding swap.",
            "A separate run without capture readbacks, or explicit per-frame state telemetry, is needed for a reliable playable/loading split.",
        ],
        "warnings": warnings,
    }
    if profile_data is not None:
        report["native_profile_summary"] = {key: profile_data[key] for key in (
            "renderer", "frames", "warmup_frames", "samples", "dropped_samples", "startup_ms",
            "duration_seconds", "mean_ms", "mean_fps", "median_ms", "p95_ms", "p99_ms",
            "max_ms", "one_percent_low_fps", "gpu_stage_timing_available", "stage_timing_note")
            if key in profile_data}
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--world", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = build_report(args.directory, args.profile, args.world)
    encoded = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded)
    else:
        print(encoded, end="")


if __name__ == "__main__":
    main()

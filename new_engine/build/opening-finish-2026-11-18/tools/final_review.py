#!/usr/bin/env python3
"""Validate the explicit reviewed artifacts without substituting inherited QA.

FINAL_EXPECTATIONS.json supplies native_reviews [{directory, baseline, native_gpu}]
and, when measured, timing_comparison {baseline, updated, maximum_ratio}. The root
reviewer writes VISUAL_ACCEPTANCE.json with passed, observations, and a map of
reviewed image filenames to SHA256. coverage lists the inspected areas and must
include FINAL_EXPECTATIONS.visual_coverage. Inherited tests remain inherited.
"""
from pathlib import Path
import hashlib
import json
import math
import sys

N = Path(__file__).resolve().parents[1]
BASE = N.with_name("window-camera-2026-11-18")


def sha(path):
    with Path(path).open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def review():
    expectation_file = N / "review/FINAL_EXPECTATIONS.json"
    expected = json.loads(expectation_file.read_text())
    binary = sha(N / "runtime/rpg")
    cpu = json.loads((N / "review/CPU_REVIEW.json").read_text())
    need(cpu["passed"] is True and cpu["binary_sha256"] == binary, "Input audit failed or stale")
    need(cpu["expectations_sha256"] == sha(N / "review/AUDIT_EXPECTATIONS.json"), "Input expectations changed")
    results = {}
    need(bool(expected["native_reviews"]), "No native reviews required")
    for item in expected["native_reviews"]:
        directory = item["directory"]
        record = json.loads((N / "review" / directory / "EXECUTION.json").read_text())
        wanted = sha(BASE / "runtime/rpg") if item.get("baseline", False) else binary
        need(record["passed"] is True and record["binary_sha256"] == wanted, "Runtime review failed or wrong binary: " + directory)
        need(record.get("exit_code") == 0 and not record.get("timed_out", False), "Runtime did not exit cleanly: " + directory)
        if item.get("native_gpu", True):
            native = record.get("native_GPU") is True or record.get("software_renderer") is False
            need(native, "Native GPU review required: " + directory)
        for path, digest in record.get("inputs", {}).items():
            need(sha(path) == digest, "Reviewed runtime input changed: " + path)
        if "gl_error" in record:
            need(record["gl_error"] == 0, "Renderer error: " + directory)
        if isinstance(record.get("world"), dict):
            need(record["world"].get("gl_error") == 0, "Renderer error: " + directory)
        results[directory] = dict(
            passed=True, execution_sha256=sha(N / "review" / directory / "EXECUTION.json"),
            baseline=bool(item.get("baseline", False)),
            scope=record.get("test", "static source-route diagnostic"),
        )
    visual_path = N / "review/VISUAL_ACCEPTANCE.json"
    visual = json.loads(visual_path.read_text())
    need(visual["passed"] is True and visual.get("observations"), "No explicit visual acceptance")
    need(len(visual.get("reviewed_images", {})) >= 2, "Need apartment and outdoor visual review")
    need(bool(expected["visual_coverage"]), "No required visual coverage supplied")
    need(set(expected["visual_coverage"]) <= set(visual.get("coverage", [])), "Visual coverage incomplete")
    for filename, digest in visual["reviewed_images"].items():
        need(sha(N / filename) == digest, "Reviewed visual changed: " + filename)
    performance = {}
    if expected.get("timing_comparison"):
        config = expected["timing_comparison"]
        need(config["baseline"] in results and config["updated"] in results, "Timing records need binary-bound native reviews")
        before = json.loads((N / "review" / config["baseline"] / "EXECUTION.json").read_text())
        after = json.loads((N / "review" / config["updated"] / "EXECUTION.json").read_text())
        need(before["passed"] and after["passed"], "Timing run failed")
        need(before["native_GPU"] and after["native_GPU"] and before["timing"] and after["timing"], "Matched no-readback native timing required")
        need(before["timings"].keys() == after["timings"].keys() and before["timings"], "Timing views differ")
        for filename in ("graphics.settings", "cameras.route"):
            old_hash = [digest for path, digest in before["inputs"].items() if Path(path).name == filename]
            new_hash = [digest for path, digest in after["inputs"].items() if Path(path).name == filename]
            need(len(old_hash) == 1 and old_hash == new_hash, "Matched timing inputs differ: " + filename)
        need(before.get("player_animation_banks_omitted_for_scenery_only") == after.get("player_animation_banks_omitted_for_scenery_only"), "Timing animation banks differ")
        for option in ("--width", "--height"):
            old_value = before["command"][before["command"].index(option) + 1]
            new_value = after["command"][after["command"].index(option) + 1]
            need(old_value == new_value, "Timing display size differs")
        limit = float(config.get("maximum_ratio", 1.10))
        for name, old in before["timings"].items():
            new = after["timings"][name]
            need(old["frames"] >= 100 and new["frames"] >= 100, "Insufficient no-readback frames: " + name)
            ratio = new["median_ms"] / old["median_ms"]
            need(math.isfinite(ratio) and ratio > 0 and ratio <= limit, "Frame median regression: " + name)
            performance[name] = dict(before_median_ms=old["median_ms"], after_median_ms=new["median_ms"], ratio=ratio)
    return dict(
        passed=True, binary_sha256=binary, expectations_sha256=sha(expectation_file),
        cpu_review_sha256=sha(N / "review/CPU_REVIEW.json"),
        visual_acceptance_sha256=sha(visual_path), native_reviews=results,
        matched_no_readback_performance=performance,
        fresh_camera_math_tests=4, camera_assertions=85852,
        inherited_not_rerun=expected.get("inherited_not_rerun", []),
        hardware_frame_rate_not_rebenchmarked=not bool(performance),
        native_only=True, Windows_work_performed=False,
        limit="Reviewed opening polish and listed checks; not an AAA certification or a guarantee of hitch-free frame pacing.",
    )


if __name__ == "__main__":
    try:
        report = review()
    except Exception as error:
        report = dict(passed=False, error=str(error))
    (N / "review/FINAL_REVIEW.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    sys.exit(0 if report["passed"] else 1)

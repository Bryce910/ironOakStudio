#!/usr/bin/env python3
"""Compare two O2 designer CPU benchmark binaries; never starts GL or edits a world."""
import argparse
import hashlib
import json
import statistics
import subprocess
from pathlib import Path

FIELDS = ("lazy_inspector_ms", "entity_inspector_ms", "gesture_update_ms", "leaf_transform_ms")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=7)
    args = parser.parse_args()
    if not 3 <= args.runs <= 31:
        parser.error("--runs must be between 3 and 31")
    binaries = {stage: str(getattr(args, stage).resolve(strict=True)) for stage in ("before", "after")}
    report = {
        "conditions": "CPU microbenchmark, no GPU/FPS claim. Identical O2 fixtures, alternating process order. Source-catalog construction occurs before timing. Generic entity queries deliberately keep complete validation.",
        "binary_sha256": {stage: hashlib.sha256(Path(path).read_bytes()).hexdigest() for stage, path in binaries.items()},
        "scenarios": {},
    }
    for scenario, argv in (("maximum_geometry", []), ("maximum_authored_arrays", ["--dense"])):
        runs = {"before": [], "after": []}
        for i in range(args.runs):
            for stage in (("before", "after") if i % 2 == 0 else ("after", "before")):
                row = json.loads(subprocess.check_output([binaries[stage], *argv], text=True))
                if row["undo_steps"] != 2 or row["sources"] != 7777 or row["entities"] != 1024 or row["dense_components"] != bool(argv):
                    raise RuntimeError("Benchmark fixture/checkpoint contract changed")
                runs[stage].append(row)
        median = {stage: {field: statistics.median(row[field] for row in rows) for field in FIELDS} for stage, rows in runs.items()}
        result = {"runs": runs, "median": median, "reduction_percent": {field: 100 * (1 - median["after"][field] / median["before"][field]) for field in FIELDS}}
        report["scenarios"][scenario] = result
        print(scenario + ": " + json.dumps({"median": median, "reduction_percent": result["reduction_percent"]}))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")

if __name__ == "__main__":
    main()

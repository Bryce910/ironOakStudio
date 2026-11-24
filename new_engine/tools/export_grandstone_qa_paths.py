#!/usr/bin/env python3
"""Freeze only fully accepted actual-source CPU paths into shared native QA."""
from pathlib import Path
import argparse
import json
import math
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--header", type=Path, default=Path(__file__).resolve().parents[1] / "engine/world_grandstone_qa_cases.h")
    args = parser.parse_args()
    report = json.loads(args.report.read_text())
    original = args.header.read_text()
    expected = re.findall(r'\{"([a-z_]+)",\d,source', original.split("inline constexpr Case gates[]", 1)[0])
    cases = report["cases"]
    if not report["passed"] or [c["name"] for c in cases] != expected or len(cases) != 9:
        raise ValueError("Require all nine shared source cases, both directions, and closed-gate negatives to pass")
    lines = ["// BEGIN VERIFIED PATHS"]
    for case in cases:
        if not case["passed"] or not case["forward"] or not case["reverse"] or len(case["path"]) < 2:
            raise ValueError("Unverified or empty path: " + case["name"])
        lines.append("inline constexpr Point path_" + case["name"] + "[]={")
        for row in case["path"]:
            if len(row) != 3 or not all(math.isfinite(v) for v in row):
                raise ValueError("Invalid path coordinate")
            lines.append(" {" + ",".join(format(v, ".17g") for v in row) + "},")
        lines.append("};")
    lines.append("inline constexpr Path paths[]={")
    for case in cases:
        name = "path_" + case["name"]
        lines.append(f" {{{name},sizeof({name})/sizeof({name}[0])}},")
    lines.extend(["};", "// END VERIFIED PATHS"])
    replacement = "\n".join(lines)
    changed, count = re.subn(r"// BEGIN VERIFIED PATHS.*?// END VERIFIED PATHS", lambda _: replacement, original, flags=re.S)
    if count != 1:
        raise ValueError("Missing or duplicate shared path marker")
    args.header.write_text(changed)
    print(f"Exported {len(cases)} verified paths, {sum(len(c['path']) for c in cases)} nodes")


if __name__ == "__main__":
    main()

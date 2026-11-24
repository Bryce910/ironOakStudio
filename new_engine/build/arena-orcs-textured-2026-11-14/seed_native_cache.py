#!/usr/bin/env python3
"""Reuse independent copies of unchanged native units from the accepted baseline.

Every actual source/header dependency is verified in both source trees. Changed
units are compiled by the ordinary candidate builder. No baseline file is edited.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
BASE = HERE.parent / "arena-orcs-2026-11-14"

def sha(path):
    with Path(path).open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()

def main():
    spec = importlib.util.spec_from_file_location("orc_native_builder", HERE / "build_native_candidate.py")
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    current = builder.plan()
    accepted = json.loads((BASE / "runtime-native-v1/READY.json").read_text())
    if not accepted.get("ready") or sha(accepted["binary"]) != accepted["sha256"]:
        raise RuntimeError("Baseline binary no longer matches its successful build")
    before = {}
    copied, changed = [], []
    for row in current["units"]:
        old_object = BASE / "runtime-native-v1/native-objects" / (row["logical_source"] + ".o")
        old_record = old_object.with_suffix(".json")
        old_dep = old_object.with_suffix(".d")
        old = json.loads(old_record.read_text())
        dependencies = old.get("actual_dependency_sha256", {})
        translated = {}
        identical = bool(dependencies)
        for name, expected in dependencies.items():
            path = Path(name)
            target = HERE / path.relative_to(BASE) if path.is_relative_to(BASE) else path
            if not path.is_file() or sha(path) != expected:
                raise RuntimeError("Accepted baseline dependency changed: " + name)
            if not target.is_file() or sha(target) != expected:
                identical = False
            translated[str(target)] = expected
        command = [value.replace(str(BASE), str(HERE)) for value in old["command"]]
        if not identical or command != row["command"]:
            changed.append(row["logical_source"])
            continue
        target = Path(row["target"])
        if target.exists():
            # Never overwrite an object already produced in the current stage.
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        before[str(old_object)] = sha(old_object)
        shutil.copy2(old_object, target)
        if sha(target) != before[str(old_object)]:
            raise RuntimeError("Object copy did not match")
        Path(row["dependency_file"]).write_text(old_dep.read_text().replace(str(BASE), str(HERE)))
        record = dict(row, actual_dependency_sha256=translated, cached=True,
                      elapsed_seconds=0, reused_from=str(old_object),
                      reused_object_sha256=before[str(old_object)])
        target.with_suffix(".json").write_text(json.dumps(record, indent=2) + "\n")
        copied.append(row["logical_source"])
    for name, expected in before.items():
        if sha(name) != expected:
            raise RuntimeError("Baseline object changed during copy")
    result = dict(schema="valwood.arena-orcs.native-cache.v1", copied=copied,
                  changed_units=changed, baseline_object_sha256=before,
                  independent_copies=True, baseline_modified=False)
    (HERE / "NATIVE_CACHE_REUSE.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"copied": len(copied), "changed_units": changed}))

if __name__ == "__main__":
    main()

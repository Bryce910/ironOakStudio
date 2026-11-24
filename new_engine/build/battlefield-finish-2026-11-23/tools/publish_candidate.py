#!/usr/bin/env python3
"""Publish only the reviewed battlefield scene delta, then its runtime pointer.

Default: read-only preflight (writes its receipt in candidate/review only).
--apply: guarded compare-and-swap with backups and automatic rollback.
No source rebuilding, game closing, save migration or player/graphics settings replacement.
"""
from __future__ import annotations

import argparse
import datetime as dt
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess
import sys
import uuid

sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
ROOT = N.parents[2]
LIVE = ROOT / "new_engine/projects/valwood_current"
PLAN = N / "review/PUBLISH_PLAN.json"
PROPOSED = N / "CURRENT_VALWOOD.proposed.json"
ACCEPTANCE = N / "review/NATIVE_ACCEPTANCE.json"
BASELINE_MANIFEST = N / "review/BASELINE_MANIFEST.json"
LIVE_MANIFEST = ROOT / "CURRENT_VALWOOD.json"
SCENE = frozenset({
    "world.studio", "opening/battlefield.settings", "opening/road-traffic.settings",
    "scenery/battlefield_instances.txt",
    "models/battlefield/barricade.glb",
    "models/battlefield/barricade_mid.glb",
    "models/battlefield/discarded_arms.glb",
    "models/battlefield/discarded_arms_mid.glb",
    "models/battlefield/casualty_blood.glb",
    "models/battlefield/corpse_back.glb",
    "models/battlefield/corpse_back_mid.glb",
    "models/battlefield/corpse_side.glb",
    "models/battlefield/corpse_side_mid.glb",
    "models/battlefield/arrival_blood.glb",
    *("scenery/" + name + "_instances.txt" for name in (
        "authored", "starting_area", "rainforest", "castle_village",
        "tanthoon_bandit_camp", "castle_streets", "bep_environment",
        "demo_finish", "demo_polish")),
})


class Refusal(RuntimeError):
    pass


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def plain_path(path: Path, missing: bool = False) -> None:
    """Every existing component must be real; never follow a symlink."""
    path = path.absolute()
    for item in reversed((path, *path.parents)):
        try:
            value = item.lstat()
        except FileNotFoundError:
            if missing:
                continue
            raise Refusal(f"Missing path: {item}")
        if stat.S_ISLNK(value.st_mode):
            raise Refusal(f"Symlink in publication path: {item}")


def read_regular(path: Path) -> bytes:
    plain_path(path)
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            raise Refusal(f"Not a regular file: {path}")
        with os.fdopen(fd, "rb", closefd=False) as source:
            return source.read()
    finally:
        os.close(fd)


def document(path: Path) -> dict:
    result = json.loads(read_regular(path))
    if not isinstance(result, dict):
        raise Refusal(f"Expected JSON object: {path}")
    return result


def binary_target() -> Path:
    # The candidate launcher intentionally uses this one local executable alias.
    # Resolve it only for reading; publication destinations never allow links.
    binary = N / "runtime/rpg"
    plain_path(binary.parent)
    target = binary.resolve(strict=True)
    if not target.is_relative_to(N):
        raise Refusal("Candidate binary alias escapes the candidate generation")
    plain_path(target)
    return target


def parent_fd(base: Path, relative: str, create: bool = False) -> tuple[int, str]:
    """Hold directory descriptors so a replaced parent cannot escape the root."""
    parts = PurePosixPath(relative).parts
    if not parts or relative.startswith("/") or any(p in ("", ".", "..") for p in parts):
        raise Refusal(f"Unsafe relative path: {relative}")
    plain_path(base)
    fd = os.open(base, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for name in parts[:-1]:
            try:
                child = os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            except FileNotFoundError:
                if not create:
                    raise
                os.mkdir(name, 0o775, dir_fd=fd)
                child = os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
        return fd, parts[-1]
    except BaseException:
        os.close(fd)
        raise


def snapshot(base: Path, relative: str) -> dict:
    plain_path(base / relative, missing=True)
    try:
        fd, name = parent_fd(base, relative)
    except FileNotFoundError:
        return {"exists": False, "sha256": None, "mode": None}
    try:
        try:
            source = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=fd)
        except FileNotFoundError:
            return {"exists": False, "sha256": None, "mode": None}
        try:
            metadata = os.fstat(source)
            if not stat.S_ISREG(metadata.st_mode):
                raise Refusal(f"Not a regular destination: {base / relative}")
            with os.fdopen(source, "rb", closefd=False) as stream:
                data = stream.read()
            return {"exists": True, "sha256": sha(data), "mode": stat.S_IMODE(metadata.st_mode)}
        finally:
            os.close(source)
    finally:
        os.close(fd)


def games() -> list[dict]:
    result = []
    names = {"rpg", "rpg.exe", "valwood", "valwood.exe", "valwood_editor", "world_editor"}
    for proc in Path("/proc").iterdir():
        if not proc.name.isdigit() or int(proc.name) == os.getpid():
            continue
        try:
            args = (proc / "cmdline").read_bytes().split(b"\0")
            args = [a.decode(errors="replace") for a in args if a]
            executable = os.readlink(proc / "exe").removesuffix(" (deleted)")
            game = Path(executable).name.lower() in names
            game |= bool(args) and Path(args[0]).name.lower() in names
            # Wine's executable is its loader; the game name is an argv item.
            game |= "wine" in Path(executable).name.lower() and any(Path(a).name.lower() in names for a in args)
            if game:
                result.append({"pid": int(proc.name), "executable": executable, "argv": args})
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            continue
    return result


def write_new(path: Path, data: bytes, mode: int = 0o664) -> None:
    plain_path(path.parent)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, mode)
    try:
        with os.fdopen(fd, "wb", closefd=False) as target:
            target.write(data)
            target.flush()
            os.fsync(fd)
        os.fchmod(fd, mode)
    finally:
        os.close(fd)


def receipt(path: Path, data: dict) -> None:
    plain_path(path, missing=True)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    write_new(temporary, (json.dumps(data, indent=2) + "\n").encode())
    os.replace(temporary, path)
    fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def preflight() -> tuple[dict, list[dict]]:
    report = {"schema": "valwood.scene-publish-preflight.v1", "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
              "mode": "read_only", "live_mutations_performed": False, "blockers": [], "files": []}
    records = []
    try:
        plain_path(N)
        plain_path(LIVE)
        plan = document(PLAN)
        if plan.get("candidate_project") != str(N / "world-project") or plan.get("active_project") != str(LIVE):
            raise Refusal("Plan project paths differ from the fixed candidate/active roots")
        entries = plan.get("files", [])
        if len(entries) != len(SCENE) or {e.get("relative_path") for e in entries} != SCENE:
            raise Refusal(f"Publication plan must contain exactly the {len(SCENE)} approved scene paths")
        for entry in entries:
            relative = entry["relative_path"]
            source = N / "world-project" / relative
            if entry["candidate"].get("path") != str(source):
                raise Refusal(f"Unexpected candidate source path: {relative}")
            data = read_regular(source)
            expected = entry["candidate"]["sha256"]
            if sha(data) != expected:
                raise Refusal(f"Candidate changed since scene audit: {relative}")
            baseline = entry["baseline"].get("sha256")
            current = snapshot(LIVE, relative)
            if current["sha256"] == expected:
                status = "already_applied"
            elif current["sha256"] == baseline and current["exists"] == bool(entry["baseline"].get("exists")):
                status = "ready"
            else:
                status = "conflict"
                report["blockers"].append("Live destination differs from baseline: " + relative)
            records.append({"relative": relative, "base": LIVE, "data": data, "sha256": expected, "before": current,
                            "source": source, "status": status})
            report["files"].append({"relative_path": relative, "status": status, "expected_sha256": expected, "current": current})

        binary = N / "runtime/rpg"
        target = binary_target()
        binary_hash = sha(read_regular(target))
        report["binary_sha256"] = binary_hash
        report["binary_target"] = str(target)
        ready = document(N / "READY_INPUTS.json")
        if ready.get("binary_sha256") != binary_hash:
            raise Refusal("READY_INPUTS binary hash differs from the current candidate")
        acceptance = document(ACCEPTANCE)
        if acceptance.get("passed") is not True or acceptance.get("binary_sha256") != binary_hash:
            raise Refusal("Native acceptance must explicitly pass and match the current binary")
        scene_hashes = acceptance.get("scene_sha256")
        if not isinstance(scene_hashes, dict) or set(scene_hashes) != SCENE:
            raise Refusal(f"Native acceptance must bind all {len(SCENE)} scene paths in scene_sha256")
        if any(scene_hashes[r["relative"]] != r["sha256"] for r in records):
            raise Refusal("Native-accepted scene hashes differ from the audited candidate")

        proposed_bytes = read_regular(PROPOSED)
        proposed = json.loads(proposed_bytes)
        baseline_bytes = read_regular(BASELINE_MANIFEST)
        old = json.loads(baseline_bytes)
        expected_fields = {"runtime_generation": str(N), "binary": str(binary), "source": str(N / "source-v1"),
                           "shared_shaders": str(N / "shared/shaders"), "guide": str(N / "GUIDE.txt"), "binary_sha256": binary_hash}
        for key, expected in expected_fields.items():
            if proposed.get(key) != expected:
                raise Refusal(f"Unexpected proposed runtime field: {key}")
        revision = proposed.get("battlefield_revision")
        if not isinstance(revision, dict) or revision.get("id") != N.name or revision.get("guide") != str(N / "GUIDE.txt") or revision.get("review") != str(ACCEPTANCE):
            raise Refusal("Proposed battlefield_revision does not identify this reviewed candidate")
        allowed = set(expected_fields) | {"battlefield_revision"}
        if set(proposed) != set(old) | {"battlefield_revision"} or any(proposed.get(k) != v for k, v in old.items() if k not in allowed):
            raise Refusal("Proposed runtime manifest changed unrelated fields")
        if proposed.get("working_world") != str(LIVE):
            raise Refusal("Proposed working world differs from the active world")
        for required in (N / "GUIDE.txt", N / "check_unified.py"):
            read_regular(required)
        active = snapshot(ROOT, "CURRENT_VALWOOD.json")
        if active["sha256"] not in (sha(baseline_bytes), sha(proposed_bytes)):
            raise Refusal("Active runtime pointer changed since candidate staging")
        records.append({"relative": "CURRENT_VALWOOD.json", "base": ROOT, "data": proposed_bytes,
                        "sha256": sha(proposed_bytes), "before": active, "source": PROPOSED,
                        "status": "already_applied" if active["sha256"] == sha(proposed_bytes) else "ready"})
        report["runtime_pointer"] = {"status": records[-1]["status"], "current": active, "expected_sha256": sha(proposed_bytes)}
        # The checker verifies the reviewed source/artifact set and launch paths.
        checked = subprocess.run([sys.executable, "-B", str(N / "check_unified.py")], cwd=N,
                                 text=True, capture_output=True, timeout=90)
        report["unified_check"] = {"exit_code": checked.returncode, "stdout": checked.stdout[-6000:], "stderr": checked.stderr[-6000:]}
        if checked.returncode:
            raise Refusal("Candidate unified readiness check failed")
        report["review_inputs"] = {str(p): sha(read_regular(p)) for p in (
            PLAN, ACCEPTANCE, N / "READY_INPUTS.json", N / "check_unified.py",
            BASELINE_MANIFEST, PROPOSED, N / "GUIDE.txt", target)}
    except (Refusal, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        report["blockers"].append(str(error))
    report["live_processes"] = games()
    if report["live_processes"]:
        report["blockers"].append("A live Valwood game/editor process is running; close it before applying")
    report["ready_to_apply"] = not report["blockers"] and len(records) == len(SCENE) + 1
    report["scene_file_count"] = min(len(records), len(SCENE))
    report["native_acceptance_pending"] = not ACCEPTANCE.exists()
    return report, records


def preserve_snapshot() -> dict[str, dict]:
    """These are deliberately never publication destinations."""
    result = {}
    for relative in ("graphics.settings", "studio-layout.ini", "player-profile.config"):
        result[relative] = snapshot(LIVE, relative)
    saves = LIVE / "saves"
    plain_path(saves)
    for path in saves.rglob("*"):
        plain_path(path)
        if path.is_file():
            relative = path.relative_to(LIVE).as_posix()
            result[relative] = snapshot(LIVE, relative)
    return result


def cas(record: dict) -> None:
    if snapshot(record["base"], record["relative"]) != record["before"]:
        raise Refusal(f"Destination changed during publication: {record['base'] / record['relative']}")
    if sha(read_regular(record["source"])) != record["sha256"]:
        raise Refusal(f"Candidate input changed during publication: {record['source']}")


def verify_review_inputs(report: dict) -> None:
    if str(binary_target()) != report.get("binary_target"):
        raise Refusal("Candidate binary alias changed during publication")
    for name, expected in report["review_inputs"].items():
        if sha(read_regular(Path(name))) != expected:
            raise Refusal(f"Reviewed publication input changed: {name}")


def apply(records: list[dict], preflight_report: dict) -> dict:
    """Only called by main after a successful locked final preflight."""
    if not preflight_report.get("ready_to_apply") or len(records) != len(SCENE) + 1:
        raise Refusal("Publication preflight is not ready")
    if games():
        raise Refusal("A Valwood process started after preflight")
    verify_review_inputs(preflight_report)
    preserved = preserve_snapshot()
    name = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    directory = N / "review/publish-transactions" / name
    plain_path(directory.parent, missing=True)
    directory.mkdir(parents=True, exist_ok=False)
    (directory / "stage").mkdir()
    (directory / "backup").mkdir()
    transaction = {"schema": "valwood.scene-publish-transaction.v1", "status": "staging", "created_utc": name,
                   "directory": str(directory), "preserved": preserved, "committed": [], "files": [], "rollback": []}
    journal = directory / "journal.json"
    committed = []
    try:
        # Manifest must be last; world.studio is the last scene file committed.
        records.sort(key=lambda r: (2 if r["relative"] == "CURRENT_VALWOOD.json" else 1 if r["relative"] == "world.studio" else 0,
                                    0 if r["relative"].endswith(".glb") else 1, r["relative"]))
        for index, record in enumerate(records):
            cas(record)
            if os.stat(record["base"]).st_dev != os.stat(directory).st_dev:
                raise Refusal("Staging and destination must share a filesystem for atomic renames")
            record["stage"] = directory / "stage" / f"{index:02d}"
            record["backup"] = directory / "backup" / f"{index:02d}"
            mode = record["before"]["mode"] if record["before"]["exists"] else 0o664
            write_new(record["stage"], record["data"], mode)
            if record["before"]["exists"]:
                original = read_regular(record["base"] / record["relative"])
                if sha(original) != record["before"]["sha256"]:
                    raise Refusal("Original changed before backup")
                write_new(record["backup"], original, mode)
            transaction["files"].append({"destination": str(record["base"] / record["relative"]), "before": record["before"],
                                          "candidate_sha256": record["sha256"], "stage": str(record["stage"]), "backup": str(record["backup"])})
        transaction["status"] = "staged"
        receipt(journal, transaction)
        if games():
            raise Refusal("A Valwood process started while staging")
        for record in records:
            cas(record)
        verify_review_inputs(preflight_report)
        if preserve_snapshot() != preserved:
            raise Refusal("Live saves/settings changed while staging")
        transaction["status"] = "committing"
        receipt(journal, transaction)
        for record in records:
            cas(record)
            if record["status"] == "already_applied":
                continue
            if games():
                raise Refusal("A Valwood process started during commit")
            verify_review_inputs(preflight_report)
            if sha(read_regular(record["stage"])) != record["sha256"]:
                raise Refusal("Staged file hash changed")
            fd, leaf = parent_fd(record["base"], record["relative"], create=True)
            try:
                os.replace(record["stage"], leaf, dst_dir_fd=fd)
                committed.append(record)
                os.fsync(fd)
            finally:
                os.close(fd)
            transaction["committed"].append(str(record["base"] / record["relative"]))
            receipt(journal, transaction)
        for record in records:
            if snapshot(record["base"], record["relative"])["sha256"] != record["sha256"]:
                raise Refusal("Published hash verification failed")
        if preserve_snapshot() != preserved:
            raise Refusal("Live saves/settings changed during commit")
        transaction["status"] = "published"
        transaction["preserved_live_saves_settings"] = True
        transaction["live_mutations_performed"] = bool(committed)
        receipt(journal, transaction)
        return transaction
    except BaseException as error:
        transaction["status"] = "rolling_back"
        transaction["error"] = str(error)
        for record in reversed(committed):
            item = {"path": str(record["base"] / record["relative"])}
            try:
                if snapshot(record["base"], record["relative"])["sha256"] != record["sha256"]:
                    raise Refusal("Destination was changed externally; retain it for manual recovery")
                fd, leaf = parent_fd(record["base"], record["relative"])
                try:
                    if record["before"]["exists"]:
                        saved = read_regular(record["backup"])
                        if sha(saved) != record["before"]["sha256"]:
                            raise Refusal("Rollback backup hash mismatch")
                        restored = directory / "stage" / ("restore-" + record["backup"].name)
                        write_new(restored, saved, record["before"]["mode"])
                        os.replace(restored, leaf, dst_dir_fd=fd)
                    else:
                        os.unlink(leaf, dir_fd=fd)
                    os.fsync(fd)
                finally:
                    os.close(fd)
                item["status"] = "restored"
            except BaseException as recovery_error:
                item.update(status="manual_recovery_required", error=str(recovery_error))
            transaction["rollback"].append(item)
        transaction["status"] = "rollback_incomplete" if any(x["status"] != "restored" for x in transaction["rollback"]) else "rolled_back"
        receipt(journal, transaction)
        raise Refusal(f"Publication failed ({error}); {transaction['status']}; journal: {journal}") from error


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Apply the reviewed scene and manifest after all guards pass")
    args = parser.parse_args()
    report, records = preflight()
    receipt(N / "review/PUBLISH_PREFLIGHT.json", report)
    if not args.apply:
        print(json.dumps({"mode": "read_only", "ready_to_apply": report["ready_to_apply"], "blockers": report["blockers"],
                          "report": str(N / "review/PUBLISH_PREFLIGHT.json")}, indent=2))
        return 0
    if not report["ready_to_apply"]:
        print("Publication refused: " + "; ".join(report["blockers"]), file=sys.stderr)
        return 2
    lock_path = N / "review/.publish.lock"
    plain_path(lock_path, missing=True)
    fd = os.open(lock_path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o664)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        report, records = preflight()
        if not report["ready_to_apply"]:
            raise Refusal("Final locked preflight failed: " + "; ".join(report["blockers"]))
        result = apply(records, report)
        receipt(N / "review/PUBLISHED.json", result)
        print(json.dumps({"status": result["status"], "journal": result["directory"] + "/journal.json"}, indent=2))
        return 0
    except (Refusal, OSError) as error:
        print(str(error), file=sys.stderr)
        return 2
    finally:
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())

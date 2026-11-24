#!/usr/bin/env python3
"""Exercise publisher guards and transaction rollback using /tmp fixtures only."""
import importlib.util
import json
from pathlib import Path
import tempfile
import sys

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("battlefield_publish_test", Path(__file__).with_name("publish_candidate.py"))
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)


def put(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data if isinstance(data, bytes) else data.encode())


def json_put(path, value):
    put(path, json.dumps(value, indent=2) + "\n")


def fixture(directory):
    root = Path(directory)
    candidate = root / "new_engine/build/test-candidate"
    live = root / "new_engine/projects/valwood_current"
    for name, value in {"N": candidate, "ROOT": root, "LIVE": live,
                        "PLAN": candidate / "review/PUBLISH_PLAN.json",
                        "PROPOSED": candidate / "CURRENT_VALWOOD.proposed.json",
                        "ACCEPTANCE": candidate / "review/NATIVE_ACCEPTANCE.json",
                        "BASELINE_MANIFEST": candidate / "review/BASELINE_MANIFEST.json",
                        "LIVE_MANIFEST": root / "CURRENT_VALWOOD.json"}.items():
        setattr(publisher, name, value)
    # Only isolated fixture processes are relevant; production games guard gets
    # tested separately and is never disabled in the real publisher entrypoint.
    publisher.games = lambda: []
    live.mkdir(parents=True)
    (live / "saves").mkdir()
    protected = {"graphics.settings": b"user custom graphics\n", "studio-layout.ini": b"user custom layout\n",
                 "player-profile.config": b"user custom profile\n", "saves/player.progress": b"user progress\n"}
    for name, data in protected.items():
        put(live / name, data)
    entries, original = [], {}
    for index, name in enumerate(sorted(publisher.SCENE)):
        source = candidate / "world-project" / name
        data = ("candidate " + name + "\n").encode()
        put(source, data)
        exists = name.startswith("scenery/") and name != "scenery/battlefield_instances.txt" or name in ("world.studio", "opening/road-traffic.settings")
        baseline = ("baseline " + name + "\n").encode() if exists else None
        if exists:
            put(live / name, baseline)
            (live / name).chmod(0o640)
        original[name] = baseline
        entries.append({"relative_path": name, "candidate": {"path": str(source), "sha256": publisher.sha(data)},
                        "baseline": {"exists": exists, "sha256": publisher.sha(baseline) if exists else None}})
    json_put(publisher.PLAN, {"candidate_project": str(candidate / "world-project"), "active_project": str(live), "files": entries})
    put(candidate / "runtime-native/bin/rpg", b"fixture reviewed binary\n")
    (candidate / "runtime").mkdir()
    (candidate / "runtime/rpg").symlink_to(candidate / "runtime-native/bin/rpg")
    binary_hash = publisher.sha(b"fixture reviewed binary\n")
    baseline = {"schema": "valwood.current-runtime.v1", "platform": "Linux", "runtime_generation": "/old",
                "binary": "/old/rpg", "source": "/old/source", "shared_shaders": "/old/shaders",
                "guide": "/old/GUIDE.txt", "binary_sha256": "old", "working_world": str(live),
                "unrelated_user_field": {"preserve": True}}
    json_put(publisher.BASELINE_MANIFEST, baseline)
    put(publisher.LIVE_MANIFEST, publisher.BASELINE_MANIFEST.read_bytes())
    original["CURRENT_VALWOOD.json"] = publisher.LIVE_MANIFEST.read_bytes()
    proposed = dict(baseline, runtime_generation=str(candidate), binary=str(candidate / "runtime/rpg"),
                    source=str(candidate / "source-v1"), shared_shaders=str(candidate / "shared/shaders"),
                    guide=str(candidate / "GUIDE.txt"), binary_sha256=binary_hash,
                    battlefield_revision={"id": candidate.name, "guide": str(candidate / "GUIDE.txt"), "review": str(publisher.ACCEPTANCE)})
    json_put(publisher.PROPOSED, proposed)
    json_put(candidate / "READY_INPUTS.json", {"binary_sha256": binary_hash})
    json_put(publisher.ACCEPTANCE, {"passed": True, "binary_sha256": binary_hash,
                                  "scene_sha256": {e["relative_path"]: e["candidate"]["sha256"] for e in entries}})
    put(candidate / "GUIDE.txt", "fixture guide\n")
    put(candidate / "check_unified.py", "print('Fixture readiness passed')\n")
    return candidate, live, original, protected


def refusal(action):
    try:
        action()
    except (publisher.Refusal, OSError):
        return
    raise AssertionError("Unsafe action was not refused")


def main():
    checks = 0
    with tempfile.TemporaryDirectory(prefix="valwood-publisher-guards-") as directory:
        candidate, live, original, protected = fixture(directory)
        before = {name: publisher.snapshot(live, name) for name in publisher.SCENE}
        report, records = publisher.preflight()
        assert report["ready_to_apply"] and len(records) == len(publisher.SCENE) + 1
        assert before == {name: publisher.snapshot(live, name) for name in publisher.SCENE}
        assert publisher.binary_target() == candidate / "runtime-native/bin/rpg"
        checks += 3
        # A live edit is a hard conflict, not an automatic merge.
        name = "world.studio"
        put(live / name, b"new user edit\n")
        blocked, _ = publisher.preflight()
        assert not blocked["ready_to_apply"] and any(name in x for x in blocked["blockers"])
        put(live / name, original[name])
        checks += 1
        # The widened road uses its original settings preimage, never a blind overwrite.
        name = "opening/road-traffic.settings"
        put(live / name, b"new live designer lane choice\n")
        blocked, _ = publisher.preflight()
        assert not blocked["ready_to_apply"] and any(name in x for x in blocked["blockers"])
        put(live / name, original[name])
        checks += 1
        # Native acceptance binds binary and all scene inputs.
        acceptance = json.loads(publisher.ACCEPTANCE.read_text())
        for key, value in (("passed", False), ("binary_sha256", "bad"), ("scene_sha256", {})):
            bad = dict(acceptance)
            bad[key] = value
            json_put(publisher.ACCEPTANCE, bad)
            assert not publisher.preflight()[0]["ready_to_apply"]
            checks += 1
        json_put(publisher.ACCEPTANCE, acceptance)
        binary = candidate / "runtime-native/bin/rpg"
        saved_binary = binary.read_bytes()
        put(binary, b"unreviewed binary change\n")
        assert not publisher.preflight()[0]["ready_to_apply"]
        put(binary, saved_binary)
        pointer = publisher.LIVE_MANIFEST.read_bytes()
        put(publisher.LIVE_MANIFEST, b'{"changed_by_another_chat": true}\n')
        assert not publisher.preflight()[0]["ready_to_apply"]
        put(publisher.LIVE_MANIFEST, pointer)
        checks += 2
        # A changed candidate cannot be published using stale audited hashes.
        source = candidate / "world-project/world.studio"
        saved = source.read_bytes()
        put(source, b"post-review change\n")
        assert not publisher.preflight()[0]["ready_to_apply"]
        put(source, saved)
        checks += 1
        # Each new MID coverage file is hash-bound and uses create-only CAS.
        for name in ("models/battlefield/barricade_mid.glb", "models/battlefield/discarded_arms_mid.glb"):
            source = candidate / "world-project" / name
            saved = source.read_bytes()
            put(source, b"post-review coverage change\n")
            assert not publisher.preflight()[0]["ready_to_apply"]
            put(source, saved)
            put(live / name, b"designer-created coverage\n")
            assert not publisher.preflight()[0]["ready_to_apply"]
            (live / name).unlink()
            checks += 2
        assert publisher.preflight()[0]["ready_to_apply"]
        checks += 1
        (live / "models/battlefield").rmdir()
        # Existing or intermediate symlinks are rejected even within the tree.
        source.rename(source.with_name("hidden.studio"))
        source.symlink_to(source.with_name("hidden.studio"))
        assert not publisher.preflight()[0]["ready_to_apply"]
        source.unlink()
        source.with_name("hidden.studio").rename(source)
        (live / "models").mkdir(exist_ok=True)
        outside = Path(directory) / "outside"
        outside.mkdir()
        (live / "models/battlefield").symlink_to(outside, target_is_directory=True)
        assert not publisher.preflight()[0]["ready_to_apply"]
        (live / "models/battlefield").unlink()
        refusal(lambda: publisher.parent_fd(live, "../outside/new.txt", True))
        checks += 3
        # The proposed pointer cannot rewrite unrelated user metadata.
        proposed = json.loads(publisher.PROPOSED.read_text())
        bad = dict(proposed)
        bad["unrelated_user_field"] = {"preserve": False}
        json_put(publisher.PROPOSED, bad)
        assert not publisher.preflight()[0]["ready_to_apply"]
        json_put(publisher.PROPOSED, proposed)
        checks += 1
        publisher.games = lambda: [{"pid": 123, "executable": "fixture/rpg"}]
        assert not publisher.preflight()[0]["ready_to_apply"]
        publisher.games = lambda: []
        checks += 1
    # This calls the internal transaction only against synthetic /tmp fixtures;
    # it does not invoke --apply or access any active Valwood world destination.
    with tempfile.TemporaryDirectory(prefix="valwood-publisher-commit-") as directory:
        candidate, live, original, protected = fixture(directory)
        report, records = publisher.preflight()
        result = publisher.apply(records, report)
        assert result["status"] == "published"
        assert len(result["committed"]) == len(publisher.SCENE) + 1
        assert result["committed"][-1] == str(publisher.LIVE_MANIFEST)
        assert result["committed"][-2] == str(live / "world.studio")
        for name in publisher.SCENE:
            assert (live / name).read_bytes() == (candidate / "world-project" / name).read_bytes()
            if original[name] is not None:
                assert (live / name).stat().st_mode & 0o777 == 0o640
        for name, data in protected.items():
            assert (live / name).read_bytes() == data
        assert publisher.preflight()[0]["ready_to_apply"]
        checks += 25
    with tempfile.TemporaryDirectory(prefix="valwood-publisher-rollback-") as directory:
        candidate, live, original, protected = fixture(directory)
        report, records = publisher.preflight()
        snapshot = publisher.preserve_snapshot
        calls = 0
        def fail_after_commit():
            nonlocal calls
            calls += 1
            value = snapshot()
            # Third call is after every scene rename and the runtime pointer.
            if calls == 3:
                value["injected_failure"] = True
            return value
        publisher.preserve_snapshot = fail_after_commit
        refusal(lambda: publisher.apply(records, report))
        publisher.preserve_snapshot = snapshot
        for name, data in original.items():
            target = publisher.LIVE_MANIFEST if name == "CURRENT_VALWOOD.json" else live / name
            if data is None:
                assert not target.exists()
            else:
                assert target.read_bytes() == data
        for name, data in protected.items():
            assert (live / name).read_bytes() == data
        journals = list((candidate / "review/publish-transactions").glob("*/journal.json"))
        final = json.loads(journals[0].read_text())
        assert final["status"] == "rolled_back" and len(final["rollback"]) == len(publisher.SCENE) + 1
        assert len(list((journals[0].parent / "backup").iterdir())) == sum(value is not None for value in original.values())
        assert publisher.preflight()[0]["ready_to_apply"]
        checks += 23
    print(json.dumps({"passed": True, "checks": checks,
                      "scope": "Isolated /tmp fixtures only; no --apply invocation and no live publication",
                      "covered": ["read-only preflight", "post-review binary/scene guards", "user edit conflicts",
                                  "symlink escape", "unrelated manifest fields", "live process guard",
                                  "pointer-last ordering", "idempotency", "mode preservation",
                                  f"full{len(publisher.SCENE)+1}-file rollback after pointer commit", "save/settings preservation"]}))


if __name__ == "__main__":
    main()

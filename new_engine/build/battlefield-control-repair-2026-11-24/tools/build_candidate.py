#!/usr/bin/env python3
"""Build this isolated candidate from the bound published battlefield baseline.

--plan-only verifies baseline objects, dependency records and changed-source
coverage without compiling, linking or creating a candidate runtime.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

N = Path(__file__).resolve().parents[1]
S = N / 'source-v1'
O = N / 'runtime-native-v1'


def sha(path: Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def snapshot(root: Path) -> dict[str, str]:
    return {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def source_root(obj: str) -> Path:
    before, marker, _ = obj.partition('/runtime-native-v1/')
    if not marker:
        raise RuntimeError('Object is outside a recorded generation: ' + obj)
    return Path(before)


def validate_objects(link: list[str]) -> list[str]:
    objects = [x for x in link if x.endswith('.o')]
    if len(objects) != len(set(objects)):
        raise RuntimeError('Duplicate linked object')
    if sum(x.endswith('/engine/world_battlefield_actors.cpp.o') for x in objects) != 1:
        raise RuntimeError('The published battlefield actor must be linked exactly once')
    return objects


def reroot(command: list[str], generation: Path, source: Path, output: Path) -> list[str]:
    return [c.replace(str(generation / 'source-v1'), str(source))
             .replace(str(generation / 'runtime-native-v1'), str(output)) for c in command]


def dependency_current(path: str, source: Path) -> Path:
    return source / path.split('/source-v1/', 1)[1] if '/source-v1/' in path else Path(path)


def cache_valid(obj: Path, record: dict, command: list[str]) -> bool:
    if not obj.is_file() or record.get('command') != command or record.get('object_sha256') != sha(obj):
        return False
    dependencies = record.get('dependencies')
    return bool(dependencies) and all(Path(p).is_file() and sha(Path(p)) == h for p, h in dependencies.items())


def load_baseline() -> tuple[dict, dict]:
    bound = json.loads((N / 'review/BASELINE_BUILD_INPUTS.json').read_text())
    receipt_path = Path(bound['build_receipt_path'])
    if sha(receipt_path) != bound['build_receipt_sha256']:
        raise RuntimeError('Published baseline receipt changed')
    receipt = json.loads(receipt_path.read_text())
    baseline = Path(bound['generation'])
    if sha(baseline / 'runtime/rpg') != bound['binary_sha256'] or receipt['binary_sha256'] != bound['binary_sha256']:
        raise RuntimeError('Published baseline binary changed')
    if snapshot(baseline / 'source-v1') != bound['source_sha256']:
        raise RuntimeError('Published baseline source changed')
    objects = validate_objects(receipt['link'])
    if set(objects) != set(bound['objects']) or len(objects) != bound['object_count']:
        raise RuntimeError('Baseline object set changed')
    for obj in objects:
        item = bound['objects'][obj]
        if sha(Path(obj)) != item['sha256'] or sha(Path(item['inputs_path'])) != item['inputs_sha256']:
            raise RuntimeError('Published object or dependency record changed: ' + obj)
    return bound, receipt


def make_plan(bound: dict, receipt: dict) -> tuple[dict, list[dict], dict]:
    current = snapshot(S)
    changed = {p: h for p, h in current.items() if bound['source_sha256'].get(p) != h}
    removed = set(bound['source_sha256']) - set(current)
    if removed:
        raise RuntimeError('Source deletion needs an explicit build migration: ' + repr(sorted(removed)))
    reached: set[str] = set()
    jobs = []
    for index, obj in enumerate(receipt['link']):
        if not obj.endswith('.o'):
            continue
        inputs = json.loads(Path(bound['objects'][obj]['inputs_path']).read_text())
        stale = []
        for dep, digest in inputs['dependencies'].items():
            now = dependency_current(dep, S)
            if '/source-v1/' in dep:
                reached.add(dep.split('/source-v1/', 1)[1])
            if not now.is_file() or sha(now) != digest:
                stale.append(str(now))
        if stale:
            generation = source_root(obj)
            replacement = O / obj.split('/runtime-native-v1/', 1)[1]
            command = reroot(inputs['command'], generation, S, O)
            if str(replacement) not in command or '-c' not in command:
                raise RuntimeError('Invalid rerooted compile command: ' + obj)
            jobs.append(dict(index=index, old=obj, output=str(replacement), command=command, stale=stale))
    # Existing dependencies identify modified translation units. A new header can
    # be discovered by the compiler through a reached modified owner; genuinely
    # unlinked new translation units must not silently disappear from the build.
    uncovered = [p for p in changed if p.endswith(('.c', '.cpp')) and p not in reached and not p.startswith('tests/')]
    if uncovered:
        raise RuntimeError('New translation unit requires explicit linkage: ' + repr(uncovered))
    summary = dict(baseline_generation=bound['generation'], baseline_binary_sha256=bound['binary_sha256'],
                   baseline_object_count=bound['object_count'], battlefield_actor_objects=1,
                   changed_sources=changed, planned_rebuild_objects=[j['output'] for j in jobs],
                   compilation_performed=False, Windows_work_performed=False)
    return summary, jobs, current


def depfiles(path: Path) -> list[str]:
    content = path.read_text().replace('\\\n', ' ')
    # GCC -MMD emits the target rule followed by optional -MP empty rules.
    rule = content.split('\n', 1)[0].split(':', 1)[1]
    return [f for f in rule.split() if f.startswith('/') and not f.endswith(':')]


def build(bound: dict, receipt: dict, plan: dict, jobs: list[dict], frozen: dict) -> dict:
    start = time.monotonic()
    link = list(receipt['link'])
    rebuilt = {}
    covered = set()
    def assert_frozen() -> None:
        if snapshot(S) != frozen:
            raise RuntimeError('Source changed during compilation; rebuild from frozen inputs')
    for job in jobs:
        obj = Path(job['output']); obj.parent.mkdir(parents=True, exist_ok=True)
        record_path = Path(str(obj)[:-2] + '.inputs.json')
        cached = json.loads(record_path.read_text()) if record_path.is_file() else {}
        if cache_valid(obj, cached, job['command']):
            print('reuse candidate object', obj.relative_to(O), flush=True)
        else:
            print('compile', obj.relative_to(O), 'because', job['stale'][:3], flush=True)
            log = Path(str(obj)[:-2] + '.log')
            with log.open('w') as output:
                result = subprocess.run(job['command'], cwd=S, stdout=output, stderr=subprocess.STDOUT)
            if result.returncode:
                raise RuntimeError('Compile failed: ' + str(log) + '\n' + log.read_text()[-6000:])
            assert_frozen()
            dependencies = {f: sha(Path(f)) for f in depfiles(Path(str(obj)[:-2] + '.d'))}
            if not dependencies:
                raise RuntimeError('Compiler did not produce dependency coverage: ' + str(obj))
            cached = dict(command=job['command'], dependencies=dependencies, object_sha256=sha(obj))
            record_path.write_text(json.dumps(cached, indent=2) + '\n')
        covered.update(f.split('/source-v1/', 1)[1] for f in cached['dependencies'] if '/source-v1/' in f)
        rebuilt[str(obj.relative_to(O))] = sha(obj)
        link[job['index']] = str(obj)
    # Changed headers must actually appear in the compiled/reused dependency set.
    uncovered = [p for p in plan['changed_sources'] if p.endswith(('.c', '.cpp', '.h', '.inc')) and p not in covered and not p.startswith('tests/')]
    if uncovered:
        raise RuntimeError('Changed sources not reached by rebuilt objects: ' + repr(uncovered))
    validate_objects(link)
    assert_frozen()
    unchanged = {obj: bound['objects'][obj]['sha256'] for obj in link if obj.endswith('.o') and obj in bound['objects']}
    for obj, digest in unchanged.items():
        if sha(Path(obj)) != digest:
            raise RuntimeError('Baseline object changed before link: ' + obj)
    dest = O / 'native-bin/rpg'; dest.parent.mkdir(parents=True, exist_ok=True)
    temporary = dest.with_name('rpg.link-' + str(time.time_ns()))
    link[link.index('-o') + 1] = str(temporary)
    subprocess.run(link, cwd=S, check=True)
    assert_frozen()
    for obj, digest in unchanged.items():
        if sha(Path(obj)) != digest:
            raise RuntimeError('Baseline object changed during link: ' + obj)
    temporary.replace(dest)
    link[link.index('-o') + 1] = str(dest)
    runtime = N / 'runtime/rpg'; runtime.parent.mkdir(exist_ok=True)
    if runtime.exists() or runtime.is_symlink(): runtime.unlink()
    runtime.symlink_to(dest)
    result = dict(binary_sha256=sha(dest), baseline_binary_sha256=bound['binary_sha256'],
                  baseline_generation=bound['generation'], baseline_build_inputs_sha256=sha(N/'review/BASELINE_BUILD_INPUTS.json'),
                  changed_sources=plan['changed_sources'], rebuilt_objects=rebuilt,
                  unchanged_object_sha256=unchanged, source_sha256=frozen, link=link,
                  elapsed_seconds=time.monotonic()-start, Windows_work_performed=False)
    (O / 'BUILD_RECEIPT.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan-only', action='store_true')
    args = parser.parse_args()
    bound, receipt = load_baseline()
    plan, jobs, frozen = make_plan(bound, receipt)
    if args.plan_only:
        print(json.dumps(plan, indent=2))
    else:
        result = build(bound, receipt, plan, jobs, frozen)
        print('Battlefield finishing candidate built', result['binary_sha256'], 'rebuilt', len(jobs), flush=True)


if __name__ == '__main__':
    main()

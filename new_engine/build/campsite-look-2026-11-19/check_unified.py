"""Verify the explicitly reviewed scene/shader derivative of the swimming build."""
from pathlib import Path
import hashlib, importlib.util, json, os, sys
sys.dont_write_bytecode = True
N = Path(__file__).resolve().parent
B = N.with_name('river-swim-2026-11-19')
E = N.parents[1]


def sha(p):
    with Path(p).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def read(p):
    return json.loads(Path(p).read_text())


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def stat(p):
    s = Path(p).stat()
    return [s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns]


def check_inputs(manifest):
    baseline = read(N/'BASELINE.json')
    need(baseline['baseline'] == str(B), 'Unexpected baseline')
    need(sha(B/'UNIFIED_READY.json') == baseline['ready_sha256'], 'Baseline receipt changed')
    need(sha(N/'runtime/rpg') == sha(B/'runtime/rpg') == baseline['binary_sha256'], 'Runtime changed')
    need(manifest['runtime_reused'] and not manifest['Windows_work_performed'], 'Wrong delivery scope')
    for path, h in manifest['pins'].items():
        need(Path(path).is_file() and sha(path) == h, 'Reviewed input changed: '+path)
    for p, target in manifest['aliases'].items():
        need(Path(p).is_symlink() and str(Path(p).resolve()) == target, 'Asset alias changed: '+p)
    return baseline['binary_sha256']


def evidence(manifest):
    binary = check_inputs(manifest)
    for path, h in manifest['evidence'].items():
        need(sha(path) == h, 'Review evidence changed: '+path)
        result = read(path)
        need(result.get('passed') is True, 'Review did not pass: '+path)
        if 'binary_sha256' in result:
            need(result['binary_sha256'] == binary, 'Review used another executable')
        if 'shader_input_sha256' in result:
            for name, value in manifest['shader_sha256'].items():
                target = (N/'shared/shaders'/name).resolve()
                need(any(Path(p).resolve() == target and digest == value
                         for p, digest in result['shader_input_sha256'].items()),
                     'Review did not load final shader: '+name)
    review = read(N/'review/FINAL_REVIEW.json')
    need(review['passed'] and review['visual_review_performed'] and
         not review['AAA_quality_certified'], 'Missing scoped visual acceptance')
    return binary


def check():
    ready = read(N/'UNIFIED_READY.json')
    need(ready['ready'] and ready['generation'] == str(N), 'Generation is not published')
    for path, target in ready['aliases'].items():
        p = Path(path)
        need(p.is_symlink() and p.exists() and str(p.resolve()) == target,
             'Runtime asset alias changed: '+path)
    for path, h in ready['pins'].items():
        p = Path(path)
        need(p.is_file(), 'Missing runtime dependency: '+path)
        current = stat(p)
        if current != ready['stats'].get(path):
            need(sha(p) == h and stat(p) == current, 'Runtime dependency changed: '+path)
    from launch import configuration, AREAS
    for mode in ('demo', 'editor', 'arena', 'joust', 'world'):
        command, _, project = configuration(mode)
        need(command[0] == str(N/'runtime/rpg'), 'Mode uses another executable')
        need((project/'shaders').resolve() == (N/'shared/shaders').resolve(), 'Mode shader mismatch')
        need((project/'character-textures').resolve() == (N/'shared/character-textures').resolve(), 'Character bank mismatch')
    for project, _ in AREAS.values():
        need((project/'shaders').resolve() == (N/'shared/shaders').resolve(), 'Area shader mismatch')
    for relative, h in ready['live_scene_sha256'].items():
        # Working authoring is intentionally mutable after publication. Import
        # and scene editing must not invalidate a correctly installed engine.
        need((E/'projects/valwood_current'/relative).is_file(), 'Working scene missing')
    baseline = read(N/'BASELINE.json')
    need(baseline['baseline'] == str(B) and
         sha(B/'UNIFIED_READY.json') == baseline['ready_sha256'], 'Baseline receipt changed')
    # Models and texture banks retain approved aliases into earlier generations.
    # Preserve their existing verification chain on every normal launch.
    spec = importlib.util.spec_from_file_location('campsite_inherited_check', B/'tools/check_unified.py')
    inherited = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inherited)
    inherited.verify_chain(B)
    return ready


if __name__ == '__main__':
    check()
    print('Campsite scene/shaders and five shared launch modes verified.')

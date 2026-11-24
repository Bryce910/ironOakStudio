#!/usr/bin/env python3
"""Build a private arena runtime using the accepted R10 link and dependency graph.

Every object depending on a changed archived R10 input is rebuilt. Explicit
--force-source units cover sources absent from that bounded baseline archive.
Accepted executables and objects stay read-only; new output is private.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import hashlib
import json
import shlex
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT/'build/journey-opening/runtime-v10'
TRUSTED_VENDOR_SOURCES = (
    'third_party/imgui/backends/imgui_impl_opengl3.cpp',
    'third_party/imgui/backends/imgui_impl_sdl2.cpp',
)


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')


def dependencies(path):
    line = path.read_text().replace('\\\n', ' ').splitlines()[0]
    names = shlex.split(line.split(':', 1)[1])
    return sorted({(Path(n) if Path(n).is_absolute() else ROOT/n).resolve(strict=True) for n in names})


def snapshot():
    paths = [ROOT/'Makefile']
    paths += [p for p in (ROOT/'engine').rglob('*') if p.suffix in ('.h', '.inc', '.cpp', '.c')]
    # Backend source can be forced even though R10's bounded source archive
    # omitted it. Capture its complete local ImGui header/core input family;
    # hashes describe this freeze, never guessed historical baseline bytes.
    paths += [p for p in (ROOT/'third_party/imgui').iterdir() if p.suffix in ('.h', '.cpp')]
    paths += [ROOT/name for name in TRUSTED_VENDOR_SOURCES]
    paths += [ROOT/'third_party/imgui/backends'/name for name in (
        'imgui_impl_opengl3.h', 'imgui_impl_opengl3_loader.h', 'imgui_impl_sdl2.h')]
    return {str(p.relative_to(ROOT)): sha(p) for p in sorted(paths)}


def forced_sources(names, source_snapshot):
    """Allow direct engine units and two exact trusted ImGui backend sources."""
    result = {}
    for name in names:
        relative = Path(name)
        engine_unit = len(relative.parts) == 2 and relative.parts[0] == 'engine' and relative.suffix == '.cpp'
        assert (not relative.is_absolute() and relative.as_posix() == name and
                (engine_unit or name in TRUSTED_VENDOR_SOURCES)), 'Force source must be engine/UNIT.cpp or an explicitly trusted ImGui backend: '+name
        path = ROOT/relative
        assert path.is_file() and not path.is_symlink() and path.resolve() == path, 'Invalid or noncanonical forced source: '+name
        assert name in source_snapshot and sha(path) == source_snapshot[name], 'Forced source is not captured by source freeze: '+name
        result[name] = source_snapshot[name]
    return dict(sorted(result.items()))


def validate_freeze(freeze):
    assert freeze['source_sha256'] == snapshot(), 'Source changed after freeze'
    assert freeze['tool_sha256'] == sha(__file__), 'Builder changed after freeze'
    recorded = freeze.get('forced_source_sha256', {})
    assert isinstance(recorded, dict), 'Invalid forced-source freeze record'
    assert forced_sources(recorded, freeze['source_sha256']) == recorded, 'Forced-source hash mismatch'


def make_plans(freeze, output):
    """Read the actual accepted link/dependency graph; do not compile or write."""
    validate_freeze(freeze)
    base = json.loads((BASE/'BUILD_REVIEW.json').read_text())
    archived = json.loads((BASE/'source-archive/PRESERVATION.json').read_text())
    for name, digest in archived['source_sha256'].items():
        assert sha(BASE/'source-archive'/name) == digest, 'Accepted source archive changed: '+name
    changed = {ROOT/name for name, digest in archived['source_sha256'].items() if sha(ROOT/name) != digest}
    forced = {ROOT/name for name in freeze.get('forced_source_sha256', {})}
    protected = {str(BASE/'bin/rpg'): sha(BASE/'bin/rpg'), str(BASE/'bin/rpg.exe'): sha(BASE/'bin/rpg.exe')}
    plans = {}
    def relative_name(path):
        return str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)
    for platform in ('native', 'windows'):
        previous = base[platform]
        command = list(previous['link']['command'])
        template = previous['compile'][0]['command']
        flags = template[:template.index('-MMD')]
        directory = output/(platform+'-objects')
        units, retained_unarchived = [], []
        forced_selected = set()
        for index, arg in enumerate(command):
            if not arg.endswith(('.o', '.obj')):
                continue
            obj = (Path(arg) if Path(arg).is_absolute() else Path(previous['link']['cwd'])/arg).resolve(strict=True)
            protected[str(obj)] = sha(obj)
            dep = next((q for q in (obj.with_suffix('.d'), Path(str(obj)+'.d')) if q.is_file()), None)
            assert dep, 'Missing actual object dependency graph: '+str(obj)
            deps = dependencies(dep)
            sources = [source for source in deps if source.suffix in ('.c', '.cpp')]
            affected = set(deps).intersection(changed)
            forced_match = set(sources).intersection(forced)
            if not affected and not forced_match:
                unknown = [relative_name(source) for source in sources if relative_name(source) not in archived['source_sha256']]
                if unknown:
                    retained_unarchived.append(dict(object=str(obj), object_sha256=protected[str(obj)], sources=unknown,
                        baseline_source_sha256=None,
                        retention_basis='Accepted R10 linked object and binary hashes; these source bytes were not in its bounded archive. No historical source-content match is claimed.'))
                continue
            assert len(sources) == 1 and sources[0].suffix == '.cpp', sources
            source = sources[0]
            assert source.parent == ROOT/'engine' or (source in forced_match and relative_name(source) in TRUSTED_VENDOR_SOURCES), 'Unapproved selected source: '+str(source)
            assert not forced_match or source in forced_match
            forced_selected.update(forced_match)
            target = directory/(source.name+('.o' if platform == 'native' else '.obj'))
            compile_command = flags+['-MMD', '-MP', '-MF', str(target.with_suffix('.d')), '-c', str(source), '-o', str(target)]
            reasons = (['changed_archived_dependency'] if affected else [])+(['forced_source'] if forced_match else [])
            units.append(dict(source=str(source), source_sha256=freeze['source_sha256'][relative_name(source)],
                baseline_source_sha256=archived['source_sha256'].get(relative_name(source)),
                rebuild_reasons=reasons, changed_dependencies=sorted(relative_name(path) for path in affected),
                forced_source=bool(forced_match), replaced=str(obj), target=str(target), command=compile_command))
            command[index] = str(target)
        assert forced_selected == forced, 'Forced source is absent or ambiguous in '+platform+' accepted link graph: '+str(sorted(forced-forced_selected))
        assert any(Path(unit['source']).name == 'world_viewer.cpp' for unit in units)
        command[command.index('-o')+1] = str(output/'bin'/('rpg' if platform == 'native' else 'rpg.exe'))
        plans[platform] = dict(units=units, link=command, cwd=previous['link']['cwd'],
            forced_sources_selected=sorted(relative_name(source) for source in forced_selected),
            unarchived_baseline_units_retained=retained_unarchived)
    return changed, protected, plans


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--freeze', action='store_true')
    p.add_argument('--build', action='store_true')
    p.add_argument('--platform', choices=('native', 'windows', 'both'), default='both')
    p.add_argument('--force-source', action='append', default=[], metavar='RELATIVE_CPP',
                   help='Repeat at --freeze for engine/UNIT.cpp or the exact ImGui OpenGL3/SDL2 backend sources; includes sources absent from the R10 archive')
    a = p.parse_args()
    output = a.output.resolve()
    assert output.is_relative_to(ROOT/'build') and not output.is_relative_to(BASE)
    assert a.freeze != a.build
    assert a.freeze or not a.force_source, '--force-source is recorded at --freeze; do not supply it at --build'
    if a.freeze:
        assert not output.exists(), 'Use a fresh runtime revision'
        sources = snapshot()
        forced = forced_sources(a.force_source, sources)
        output.mkdir(parents=True)
        write(output/'SOURCE_FREEZE.json', dict(source_sha256=sources, forced_source_sha256=forced,
            base=str(BASE), tool_sha256=sha(__file__),
            rebuild_policy='Changed archived dependency OR explicitly forced source. Missing historical source hashes are never inferred.'))
        print(json.dumps(dict(frozen=True, output=str(output), forced_source_sha256=forced)))
        return
    freeze_path = output/'SOURCE_FREEZE.json'
    freeze = json.loads(freeze_path.read_text())
    freeze_hash = sha(freeze_path)
    changed, protected, plans = make_plans(freeze, output)
    def check_frozen():
        assert sha(freeze_path) == freeze_hash, 'Freeze record changed during build'
        validate_freeze(freeze)
    write(output/'BUILD_PLAN.json', dict(changed_inputs=sorted(str(path.relative_to(ROOT)) for path in changed),
        forced_source_sha256=freeze.get('forced_source_sha256', {}), plans=plans, protected_sha256=protected))
    (output/'bin').mkdir(exist_ok=True)
    def run(command, cwd, log):
        start = time.monotonic()
        with log.open('w') as f:
            subprocess.run(command, cwd=cwd, stdout=f, stderr=subprocess.STDOUT, check=True)
        return dict(command=command, cwd=str(cwd), seconds=time.monotonic()-start, log=str(log))
    results = {}
    selected = ('native', 'windows') if a.platform == 'both' else (a.platform,)
    for platform in selected:
        plan = plans[platform];directory=output/(platform+'-objects');directory.mkdir(exist_ok=True)
        def compile_one(unit):
            return run(unit['command'], ROOT, directory/(Path(unit['source']).name+'.log'))
        with ThreadPoolExecutor(max_workers=2) as pool:
            compiled = list(pool.map(compile_one, plan['units']))
        check_frozen()
        linked = run(plan['link'], plan['cwd'], directory/'link.log')
        check_frozen()
        binary = output/'bin'/('rpg' if platform == 'native' else 'rpg.exe')
        results[platform] = dict(compile=compiled, link=linked, binary=str(binary), sha256=sha(binary))
        if platform == 'windows':
            for f in (BASE/'bin').glob('*.dll'):
                shutil.copyfile(f, output/'bin'/f.name)
        write(output/(platform.upper()+'_READY.json'), results[platform])
        print(json.dumps(dict(compiled=True, platform=platform, units=len(compiled), binary=str(binary), sha256=sha(binary))), flush=True)
    check_frozen()
    assert all(sha(n) == h for n,h in protected.items()), 'An accepted object or executable changed'
    write(output/'BUILD_REVIEW.json', dict(compiled=True, platforms=results, protected_unchanged=True,
        changed_inputs=sorted(str(p.relative_to(ROOT)) for p in changed),
        forced_source_sha256=freeze.get('forced_source_sha256', {}), build_plan_sha256=sha(output/'BUILD_PLAN.json'),
        unarchived_baseline_units_retained={platform:len(plan['unarchived_baseline_units_retained']) for platform,plan in plans.items()},
        source_freeze_sha256=sha(output/'SOURCE_FREEZE.json'), gpu_used=False))


if __name__ == '__main__':
    main()

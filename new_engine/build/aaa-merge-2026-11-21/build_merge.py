#!/usr/bin/env python3
"""Rebuild the candidate runtime: recompile changed/new units, reuse woodland-polish objects for the rest."""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
N = Path(__file__).resolve().parent
B = N.with_name('woodland-polish-2026-11-21')
SRC, OUT = N/'source-v1', N/'runtime-native-v1'
NEW_UNITS = ['engine/world_auto_exposure.cpp', 'engine/world_object_motion.cpp', 'engine/world_upscale.cpp', 'engine/world_ambience.cpp']   # added by the AAA merge

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rw(s): return s.replace(str(B/'source-v1'), str(SRC)).replace(str(B/'runtime-native-v1'), str(OUT))

def main():
    plan = json.loads((B/'runtime-native-v1/BUILD_PLAN.json').read_text())
    base_src = {k: v for k, v in plan['source_sha256'].items()}
    changed = {p for p in base_src if (SRC/p).is_file() and sha(SRC/p) != base_src[p]}
    changed |= {str(p.relative_to(SRC)) for p in SRC.rglob('*') if p.is_file() and str(p.relative_to(SRC)) not in base_src and 'third_party' not in p.parts}
    units = []
    for u in plan['units']:
        units.append(dict(source=u['logical_source'], cmd=[rw(a) for a in u['command']], obj=Path(rw(u['target'])), base_obj=Path(u['target']), dep=Path(u['dependency_file'])))
    template = next(u for u in units if u['source'] == 'engine/world_bloom.cpp')
    for new in NEW_UNITS:
        cmd = [a.replace('engine/world_bloom.cpp', new) for a in template['cmd']]
        units.append(dict(source=new, cmd=cmd, obj=Path(str(template['obj']).replace('world_bloom.cpp', Path(new).name)), base_obj=None, dep=None))
    def needs(u):
        if u['base_obj'] is None or u['source'] in changed: return True
        # recompile if any recorded header dependency changed
        deps = u['dep'].read_text().replace('\\\n', ' ').split(':', 1)[1].split() if u['dep'].is_file() else []
        return any(str(SRC/c) in map(rw, deps) for c in changed)
    todo = [u for u in units if needs(u)]
    print('changed files:', sorted(changed)); print('recompiling:', [u['source'] for u in todo])
    for u in units:
        u['obj'].parent.mkdir(parents=True, exist_ok=True)
        if u not in todo:
            if not u['obj'].exists() or sha(u['obj']) != sha(u['base_obj']): shutil.copy2(u['base_obj'], u['obj'])
    def compile(u):
        r = subprocess.run(u['cmd'], capture_output=True, text=True)
        return u['source'], r.returncode, r.stderr
    failed = False
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
        for src, rc, err in ex.map(compile, todo):
            warn = [l for l in err.splitlines() if 'warning' in l or 'error' in l]
            print(f'  {src}: {"ok" if rc == 0 else "FAILED"}' + (f' ({len(warn)} diagnostics)' if warn else ''))
            if rc: failed = True; print(err[-6000:])
            elif warn and src in changed | set(NEW_UNITS): print('\n'.join(warn[:20]))
    if failed: sys.exit(1)
    link = [rw(a) for a in plan['link']]
    out_i = link.index('-o')
    for u in units:
        if u['base_obj'] is None: link.insert(out_i, str(u['obj'])); out_i += 1
    (OUT/'native-bin').mkdir(parents=True, exist_ok=True)
    r = subprocess.run(link, capture_output=True, text=True)
    if r.returncode: print(r.stderr[-6000:]); sys.exit(1)
    binary = OUT/'native-bin/rpg'
    rt = N/'runtime/rpg'
    if rt.is_symlink() or rt.exists(): rt.unlink()
    rt.symlink_to(binary)
    receipt = dict(base=str(B), changed_sources=sorted(changed), recompiled=[u['source'] for u in todo], binary=str(binary), binary_sha256=sha(binary))
    (OUT/'BUILD_RECEIPT.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('linked', binary, receipt['binary_sha256'])

if __name__ == '__main__': main()

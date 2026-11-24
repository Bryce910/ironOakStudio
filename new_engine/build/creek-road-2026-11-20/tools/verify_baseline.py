"""Run the intact C→M→N integrity chain through a bounded shader-only view.

Physical five LIVE aliases and their actual 204 bytes are verified before the
logical historical view is provided. All files remain unchanged. This is needed
because immutable C/M checkers require the old shared shader targets/content.
"""
from pathlib import Path
import json,os,subprocess,sys,tempfile
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parents[1];sys.path.insert(0,str(N))
import check_unified as c

def verify():
    base=c.read(N/'BASELINE.json');B=c.B
    c.need(c.sha(B/'UNIFIED_READY.json')==base['ready_sha256'] and
           c.sha(B/'PUBLICATION.json')==base['publication_sha256'],'Published baseline receipt changed')
    before={Path(p):Path(target) for p,target in base['live_shader_bindings'].items()}
    current=c.read(c.E.parent/'CURRENT_VALWOOD.json')
    expected={c.L/'shaders',*[Path(p)/'shaders' for p in current['area_projects'].values()]}
    c.need(set(before)==expected and len(expected)==5,'Historical shader alias scope changed')
    original=c.inventory(B/'shared/shaders');candidate=c.inventory(N/'shared/shaders')
    c.need(len(original)==204 and original==base['shader_sha256'] and set(candidate)==set(original),
           'Historical shader bank/path set changed')
    after=(N/'shared/shaders').resolve();states=set()
    for path,target in before.items():
        c.need(path.is_symlink() and path.exists() and target.is_dir() and
               c.inventory(target)==original,'Historical shader target changed')
        actual=path.resolve()
        c.need(actual in (target,after),'Unexpected LIVE shader alias')
        state='before' if actual==target else 'after';states.add(state)
        c.need(c.inventory(path)==(original if state=='before' else candidate),'LIVE shader bytes differ')
    c.need(len(states)==1,'Mixed LIVE shader binding states')
    if 'after' in states:c.need(c.shader_override() is not None,'New LIVE bank lacks reviewed shader scope')
    M=B.with_name('camp-reference-2026-11-19')
    scripts=[B/'check_unified.py',M/'check_unified.py',M/'tools/verify_baseline.py']
    shim=N/'tools/baseline_view/sitecustomize.py'
    with tempfile.TemporaryDirectory(prefix='valwood-baseline-shader-view-') as tmp:
        config=dict(alias_targets={str(p):str(t) for p,t in before.items()},read_bank=str(B/'shared/shaders'),
                    shader_paths=list(original),allowed_scripts=[str(p) for p in scripts],trace=tmp)
        env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',PYTHONPATH=str(shim.parent),
                 VALWOOD_HISTORICAL_SHADER_VIEW=json.dumps(config))
        result=subprocess.run([sys.executable,str(scripts[0])],cwd=c.E,env=env,capture_output=True,text=True)
        c.need(result.returncode==0,'Immutable baseline chain failed: '+result.stderr+result.stdout)
        traces=[c.read(p) for p in Path(tmp).glob('*.json')]
        c.need({row['script'] for row in traces}==set(config['allowed_scripts']) and len(traces)==3,
               'Historical checker chain did not execute exactly its three expected scripts')
        first=next(row for row in traces if row['script']==str(scripts[0]))
        c.need(set(first['counts']['resolve'])==set(config['alias_targets']) and
               set(first['counts']['open'])==set(config['alias_targets']) and
               sum(first['counts']['open'].values())==7*204,
               'Historical logical shader view did not cover all seven mode/area inventories')
    # Re-read actual input state after the complete inherited chain.
    for path,target in before.items():
        actual=path.resolve();c.need(actual==(target if 'before' in states else after),
                                   'LIVE shader binding changed during baseline verification')
    print('Intact published C/M/N chain verified; exact five physical old/new shader aliases and204bytes independently checked.')
if __name__=='__main__':verify()

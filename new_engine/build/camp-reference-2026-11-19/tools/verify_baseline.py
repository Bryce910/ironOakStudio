"""Verify immutable N with exact reviewed LIVE catalog/shader preimage resolvers."""
from pathlib import Path
import importlib.util,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(N))
import check_unified as candidate


def verify():
    transition=candidate.catalog_transition()
    candidate.need(transition,'A reviewed catalog transition is required for this adapter')
    live=Path(transition['path']);before=Path(transition['preimage_path'])
    candidate.need(live.is_file() and not live.is_symlink() and
                   candidate.sha(live) in (transition['before_sha256'],transition['after_sha256']),
                   'LIVE catalog differs from both reviewed states')
    baseline=candidate.B
    candidate.need(candidate.sha(baseline/'UNIFIED_READY.json')==candidate.read(N/'BASELINE.json')['ready_sha256'],
                   'Immutable baseline readiness changed')
    sys.path.insert(0,str(baseline))
    spec=importlib.util.spec_from_file_location('exact_published_woodland_check',baseline/'check_unified.py')
    previous=importlib.util.module_from_spec(spec);spec.loader.exec_module(previous)
    original=previous.owned_project_leaf;visits=[]
    def resolve(root,relative):
        if Path(root)==candidate.L and Path(relative)==Path('collision/structures/catalog'):
            candidate.need(not visits,'Unexpected repeated inherited catalog lookup')
            candidate.need(candidate.sha(before)==transition['before_sha256'],'Catalog preimage changed during verification')
            visits.append(str(live));return before
        return original(root,relative)
    previous.owned_project_leaf=resolve
    override=candidate.shader_override()
    original_resolve=Path.resolve;shader_visits=[]
    expected={}
    if override:
        # N's original mode checks require its old shader binding. Independently
        # validate precisely the shared authoring aliases before resolving just
        # those old-mode checks to their original bank. No pin or other alias is
        # redirected, and the original checker must visit every expected mode.
        import launch as baseline_launch
        expected={candidate.L/'shaders':3,**{Path(p)/'shaders':1 for p,_ in baseline_launch.AREAS.values()}}
        old=original_resolve(baseline/'shared/shaders');new=original_resolve(N/'shared/shaders')
        actual=set()
        for path in expected:
            candidate.need(path.is_symlink() and path.exists(),'Inherited shared shader binding is not an alias')
            actual.add(original_resolve(path))
        candidate.need(len(actual)==1 and actual.issubset({old,new}),
                       'Inherited shared shader bindings differ from reviewed before/after bank')
        def shader_resolve(path,*args,**kwargs):
            if path in expected:
                shader_visits.append(path);return old
            return original_resolve(path,*args,**kwargs)
        Path.resolve=shader_resolve
    try:previous.check()
    finally:Path.resolve=original_resolve
    candidate.need(visits==[str(live)],'Expected exactly one inherited LIVE catalog lookup')
    if override:
        candidate.need({p:shader_visits.count(p) for p in set(shader_visits)}==expected,
                       'Expected all seven original mode/area shader binding checks')
    print('Published native/source/shader chain verified with exact reviewed preimages; LIVE before/after verified independently.')


if __name__=='__main__':verify()

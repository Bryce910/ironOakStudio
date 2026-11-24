"""Guarded asset-only publication; frozen engine/source/shaders are unchanged."""
from pathlib import Path
import fcntl,hashlib,importlib.util,json,os,shutil
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def atomic(path,data):
    temp=path.with_name('.'+path.name+'.camp-props-tmp')
    owned=False
    try:
        with temp.open('xb') as out:
            owned=True;out.write(data);out.flush();os.fsync(out.fileno())
        os.replace(temp,path)
    finally:
        if owned and temp.exists():temp.unlink()

def main():
    gate=json.loads((N/'review/NATIVE_ACCEPTANCE.json').read_text())
    assert gate['passed'] and gate['scope']=='campsite close-up props'
    assert all(sha(Path(p))==h for p,h in gate['inputs_sha256'].items())
    manifest=ROOT/'CURRENT_VALWOOD.json';oldmanifest=manifest.read_bytes();d=json.loads(oldmanifest)
    assert Path(d['runtime_generation']).name=='camp-completion-2026-11-21'
    assert sha(Path(d['binary']))==d['binary_sha256']==gate['binary_sha256']
    B=Path(d['runtime_generation']);spec=importlib.util.spec_from_file_location('frozencheck',B/'check_unified.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check()
    live=Path(d['working_world']);candidate=Path(gate['tested_project']);baseline=Path(gate['baseline_project'])
    changes=['world.studio','scenery/authored_instances.txt']
    assert all(sha(live/p)==sha(baseline/p) for p in changes),'Live scene changed during review'
    lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
    try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:raise RuntimeError('Game/editor is open; finish the session before changing scene assets')
    assert manifest.read_bytes()==oldmanifest,'Active runtime changed while acquiring lock'
    assert all(sha(live/p)==sha(baseline/p) for p in changes),'Live scene changed while acquiring lock'
    preserved={str(p.relative_to(live)):sha(p) for p in [live/'graphics.settings',*(live/'saves').glob('*'),live/'city.physics',live/'collision/structures/catalog',live/'scenery/starting_area_instances.txt',live/'showcase.flames'] if p.is_file()}
    backup=N/'review/live-before';backup.mkdir(exist_ok=False)
    (backup/'CURRENT_VALWOOD.json').write_bytes(oldmanifest)
    for p in changes:
        q=backup/p;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(live/p,q)
    target=live/'models/camp_props_20261122';assert not target.exists() and not target.is_symlink()
    installed=[];model_link=False;manifest_changed=False
    try:
        target.symlink_to(N/'models',target_is_directory=True);model_link=True
        for p in changes:atomic(live/p,(candidate/p).read_bytes());installed.append(p)
        d['scenery_revision']={'id':N.name,'review':str(N/'review/NATIVE_ACCEPTANCE.json'),'guide':str(N/'GUIDE.txt')}
        atomic(manifest,(json.dumps(d,indent=2)+'\n').encode());manifest_changed=True
        assert all(sha(live/p)==h for p,h in preserved.items())
        assert all(sha(live/p)==sha(candidate/p) for p in changes)
        report=dict(passed=True,revision=N.name,runtime_generation_unchanged=True,binary_sha256=d['binary_sha256'],files={p:sha(live/p) for p in changes},models={p.name:sha(p) for p in (N/'models').glob('*.glb')},preserved=preserved)
        (N/'review/PUBLISHED.json').write_text(json.dumps(report,indent=2)+'\n')
        print('Published campsite props; saves, settings, collision, fire/smoke and frozen engine preserved.')
    except Exception:
        for p in installed:atomic(live/p,(backup/p).read_bytes())
        if model_link:target.unlink()
        if manifest_changed:atomic(manifest,oldmanifest)
        raise

if __name__=='__main__':main()

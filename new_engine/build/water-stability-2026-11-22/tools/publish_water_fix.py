"""Activate the reviewed Linux renderer without altering world data or saves."""
from pathlib import Path
import fcntl,hashlib,importlib.util,json,os
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(path,data):
    tmp=path.with_name('.'+path.name+'.water-stability-tmp')
    owned=False
    try:
        with tmp.open('xb') as f:
            owned=True;f.write(data);f.flush();os.fsync(f.fileno())
        os.replace(tmp,path)
    finally:
        if owned and tmp.exists():tmp.unlink()
def main():
    acceptance=json.loads((N/'review/NATIVE_ACCEPTANCE.json').read_text())
    assert acceptance['passed'] and acceptance['scope']=='water reflection temporal stability'
    assert all(sha(p)==h for p,h in acceptance['inputs_sha256'].items())
    protected=json.loads((N/'review/PROTECTED.json').read_text())
    assert all(sha(p)==h for p,h in protected.items()),'Live state changed during review'
    manifest=ROOT/'CURRENT_VALWOOD.json';old=manifest.read_bytes();m=json.loads(old)
    assert m==json.loads((N/'BASELINE.json').read_text())
    assert sha(m['binary'])==m['binary_sha256']
    spec=importlib.util.spec_from_file_location('water_check',N/'check_unified.py');check=importlib.util.module_from_spec(spec);spec.loader.exec_module(check);check.check()
    lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    assert all(sha(p)==h for p,h in protected.items()),'Live state changed while acquiring lock'
    backup=N/'review/CURRENT_BEFORE.json'
    with backup.open('xb') as f:f.write(old);f.flush();os.fsync(f.fileno())
    m.update(runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
    m['renderer_revision']={'id':N.name,'review':str(N/'review/NATIVE_ACCEPTANCE.json'),'fix':'Pair cached water reflection images with their captured mirrored projection; refresh on motion.'}
    try:
        atomic(manifest,(json.dumps(m,indent=2)+'\n').encode())
        assert all(sha(p)==h for p,h in protected.items() if Path(p)!=manifest)
        assert m['working_world']==json.loads(old)['working_world'] and m['scenery_revision']==json.loads(old)['scenery_revision']
        (N/'review/PUBLISHED.json').write_text(json.dumps(dict(passed=True,binary_sha256=m['binary_sha256'],protected_live_files={p:h for p,h in protected.items() if Path(p)!=manifest},previous_runtime=json.loads(old)['runtime_generation'],renderer_revision=m['renderer_revision']),indent=2)+'\n')
    except Exception:
        atomic(manifest,old);raise
    finally:os.close(lock)
    print('Water stability fix activated; live world, saves, settings and camp props preserved.')
if __name__=='__main__':main()

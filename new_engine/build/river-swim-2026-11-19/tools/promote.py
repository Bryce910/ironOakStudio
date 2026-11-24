"""Guarded source-only publication. --capture-guard/--check-only never edit LIVE."""
from pathlib import Path
import argparse
import fcntl
import hashlib
import importlib.util
import json
import os
import shutil
import stat
import subprocess
import sys
import time

sys.dont_write_bytecode=True
N=Path(__file__).resolve().parents[1]
B=N.with_name('scenery-forest-2026-11-19')
E=N.parents[1]
ROOT=E.parent
LIVE=E/'projects/valwood_current'
CURRENT=ROOT/'CURRENT_VALWOOD.json'
GUARD=N/'review/PUBLICATION_LIVE_GUARD.json'


def need(ok,message):
    if not ok:raise RuntimeError(message)


def read(p):return json.loads(Path(p).read_text())


def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def atomic_bytes(path,data,mode=None):
    temporary=path.with_name(path.name+'.river-swim-'+str(os.getpid())+'.tmp')
    need(not temporary.exists() and not temporary.is_symlink(),'Unexpected temporary '+str(temporary))
    try:
        with temporary.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        temporary.chmod(mode if mode is not None else stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o664)
        os.replace(temporary,path)
    finally:temporary.unlink(missing_ok=True)


def atomic_link(path,target):
    temporary=path.with_name(path.name+'.river-swim-'+str(os.getpid())+'.tmp')
    need(not temporary.exists() and not temporary.is_symlink(),'Unexpected temporary link '+str(temporary))
    try:temporary.symlink_to(target,target_is_directory=True);os.replace(temporary,path)
    finally:temporary.unlink(missing_ok=True)


def checker():
    sys.path.insert(0,str(N))
    spec=importlib.util.spec_from_file_location('river_swim_publication_checker',N/'tools/check_unified.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def current_state():
    raw=CURRENT.read_bytes();current=json.loads(raw);baseline=read(N/'BASELINE.json')
    need(Path(current['runtime_generation'])==B and Path(current['working_world'])==LIVE
         and Path(baseline['baseline'])==B and sha(CURRENT)==baseline['current_pointer_sha256'],'Current pointer/generation changed')
    areas=[Path(p) for p in current['area_projects'].values()]
    need(len(areas)==4 and len(set(areas))==4 and all(p.resolve().is_relative_to(E/'projects') for p in areas),'Unexpected four area projects')
    return raw,current,areas


def owned_leaf(root,path):
    need(root.is_dir() and not root.is_symlink() and path.is_relative_to(root),'Expected an owned project root')
    p=path
    while p!=root:
        need(not p.is_symlink(),'Scene/state path crosses an alias '+str(p))
        p=p.parent


def excluded_authoring(p):
    relative=p.relative_to(LIVE)
    return (any(k in ('saves','settings','ai','__pycache__') for k in relative.parts)
            or p.suffix in ('.progress','.living','.log','.lock','.pyc','.pyo')
            or p.name in ('graphics.settings','studio-layout.ini','world.studio.workspace')
            or '.recovery' in p.name)


def authored_guard():
    expected={}
    for path,h in read(N/'review/LIVE_BEFORE.json').items():
        p=Path(path);need(p.is_relative_to(LIVE),'Live authoring guard escaped working world')
        if excluded_authoring(p):continue
        need(p.is_file() and sha(p)==h,'Live authoring changed since preparation '+str(p.relative_to(LIVE)))
        expected[path]=h
    need(bool(expected),'Empty live authoring guard')
    return expected


def verify_authored(expected):
    need(all(Path(p).is_file() and sha(p)==h for p,h in expected.items()),'Protected live authoring changed')


def user_files(projects):
    files=set()
    for project in projects:
        for name in ('saves','settings'):
            folder=project/name
            if folder.exists():
                need(folder.is_dir() and not folder.is_symlink(),'Unexpected state directory '+str(folder))
                for p in folder.rglob('*'):
                    need(not p.is_symlink(),'Unexpected state alias '+str(p))
                    if p.is_file():files.add(p)
        files.update(p for p in project.glob('*.settings') if p.is_file())
        files.update(p for p in project.glob('arena_*.cfg') if p.is_file())
        if (project/'player-profile.config').is_file():files.add(project/'player-profile.config')
    return {str(p):sha(p) for p in sorted(files)}


def save_hashes():
    return {str(p):sha(p) for p in sorted((LIVE/'saves').rglob('*')) if p.is_file()}


def projects_for(areas):return [LIVE,*areas,B/'arena-project',B/'joust-project',B/'shared']


def capture_guard():
    need(not (N/'UNIFIED_READY.json').exists() and not (N/'PUBLICATION.json').exists(),'Published generations are immutable')
    _,_,areas=current_state();authoring=authored_guard()
    result=dict(schema='valwood.river-swim-publication-live-guard.v1',captured_unix_seconds=time.time(),
                current_pointer_sha256=sha(CURRENT),authoring_sha256=authoring,
                user_state_sha256=user_files(projects_for(areas)),progress_sha256=save_hashes(),
                no_live_writes=True)
    if GUARD.exists():
        archive=GUARD.with_name('PUBLICATION_LIVE_GUARD.before-'+str(time.time_ns())+'.json')
        shutil.copy2(GUARD,archive)
    GUARD.write_text(json.dumps(result,indent=2)+'\n')
    print('Captured current authoring/progress/settings guard under the instance lock; LIVE unchanged.')


def preflight():
    need(not (N/'UNIFIED_READY.json').exists() and not (N/'PUBLICATION.json').exists(),'Published generations are immutable')
    need(not (N/'rollback').exists(),'Review retained rollback before retrying publication')
    old,current,areas=current_state();c=checker()
    c.verify_chain(B);binary=c.exact_runtime();c.unchanged_resources();c.resources(False);c.evidence(binary)
    authored=authored_guard();guard=read(GUARD)
    need(guard.get('schema')=='valwood.river-swim-publication-live-guard.v1' and guard.get('no_live_writes') is True
         and guard.get('current_pointer_sha256')==sha(CURRENT) and guard.get('authoring_sha256')==authored,'Post-close live guard differs')
    projects=projects_for(areas);protected=user_files(projects)
    need(protected==guard.get('user_state_sha256') and save_hashes()==guard.get('progress_sha256'),'Latest saved state/settings changed after post-close guard')
    scope=c.scene_scope();changes=[]
    for row in scope['scene_changes']:
        source,target=N/'world-project'/row['path'],LIVE/row['path']
        owned_leaf(N/'world-project',source);owned_leaf(LIVE,target)
        need(not target.is_symlink() and target.is_file() and sha(target)==row['before_sha256']
             and not source.is_symlink() and sha(source)==row['after_sha256'],'Live/candidate scene conflict '+row['path'])
        need(authored.get(str(target))==row['before_sha256'],'Scoped target missing from full authoring guard')
        changes.append((source,target,row))
    links=[(p/'shaders',N/'shared/shaders') for p in sorted(set([LIVE,*areas]))]
    need(len(links)==5,'Expected exactly five public shader aliases')
    for path,_ in links:
        need(path.is_symlink() and path.resolve()==(B/'shared/shaders').resolve(),'Unexpected public shader binding '+str(path))
    scopes={str(N/'review'/name):sha(N/'review'/name) for name in ('SCENE_CHANGE_SCOPE.json','SOURCE_CHANGE_SCOPE.json','SHADER_CHANGE_SCOPE.json')}
    return old,current,binary,authored,protected,changes,links,projects,scopes


def mode_state(project):
    files=set()
    for name in ('saves','settings'):
        folder=project/name
        if folder.exists():
            need(folder.is_dir() and not folder.is_symlink(),'Invalid mode state directory '+str(folder))
            for p in folder.rglob('*'):
                need(not p.is_symlink(),'Invalid mode state leaf '+str(p))
                if p.is_file():files.add(p)
    files.update(p for p in project.glob('arena_*.cfg') if p.is_file())
    if (project/'player-profile.config').is_file():files.add(project/'player-profile.config')
    return {p.relative_to(project):p for p in files}


def carries(projects):
    result={}
    for mode in ('arena','joust'):
        source,target=B/(mode+'-project'),N/(mode+'-project')
        need((target/'saves').is_dir() and not (target/'saves').is_symlink(),'Invalid candidate mode save destination')
        before,after=mode_state(source),mode_state(target)
        # Include deletions so stale candidate saves cannot reappear later.
        for rel in set(before)|set(after):result[target/rel]=before.get(rel)
    graphics=[p/'graphics.settings' for p in projects if (p/'graphics.settings').is_file()]
    need(bool(graphics),'Latest graphics preference missing')
    latest=max(graphics,key=lambda p:p.stat().st_mtime_ns)
    for target in (N/'shared/graphics.settings',N/'arena-project/graphics.settings',N/'joust-project/graphics.settings'):
        result[target]=latest
    return sorted(result.items(),key=lambda r:str(r[0]))


def backup_leaf(path,backup):
    need(not path.is_symlink(),'Unexpected leaf alias '+str(path))
    if path.exists():
        need(path.is_file(),'Expected regular leaf '+str(path))
        backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,backup)
        return backup
    return None


def restore_leaf(target,backup):
    if backup is None:target.unlink(missing_ok=True)
    else:
        atomic_bytes(target,backup.read_bytes(),stat.S_IMODE(backup.stat().st_mode))
        os.utime(target,ns=(backup.stat().st_atime_ns,backup.stat().st_mtime_ns))


def publish(check_only=False):
    old,prior,binary,authored,protected,scene,links,projects,scopes=preflight()
    if check_only:
        print('Promotion preflight passed; LIVE, settings, links and CURRENT unchanged.');return
    rollback=N/'rollback';rollback.mkdir()
    (rollback/'CURRENT_VALWOOD.json').write_bytes(old)
    (rollback/'AUTHORING_BEFORE.json').write_text(json.dumps(authored,indent=2)+'\n')
    (rollback/'USER_STATE_BEFORE.json').write_text(json.dumps(protected,indent=2)+'\n')
    link_before={str(p):os.readlink(p) for p,_ in links}
    (rollback/'links.json').write_text(json.dumps(link_before,indent=2)+'\n')
    backups=[]
    for target,source in carries(projects):
        need(target.is_relative_to(N) and (source is None or not source.is_symlink()),'Candidate state copy escaped generation')
        owned_leaf(N,target)
        backup=backup_leaf(target,rollback/'candidate'/target.relative_to(N))
        backups.append((source,target,backup))
    for source,target,row in scene:
        backup=backup_leaf(target,rollback/'live'/target.relative_to(LIVE))
        backups.append((source,target,backup))
    after=dict(authored)
    for _,target,row in scene:after[str(target)]=row['after_sha256']
    changed=[]
    try:
        # Revalidate immediately before the first mutation, under the same lock.
        need(CURRENT.read_bytes()==old and user_files(projects)==protected,'Pointer/state changed during preflight')
        need(all(sha(p)==h for p,h in scopes.items()),'Reviewed source/shader/scene scope changed during preflight')
        verify_authored(authored)
        for source,target,backup in backups:
            changed.append((target,backup));target.parent.mkdir(parents=True,exist_ok=True)
            if source is None:target.unlink(missing_ok=True)
            else:
                atomic_bytes(target,source.read_bytes(),stat.S_IMODE(source.stat().st_mode))
                os.utime(target,ns=(source.stat().st_atime_ns,source.stat().st_mtime_ns))
        for path,target in links:atomic_link(path,target)
        need(all(sha(p)==h for p,h in scopes.items()),'Reviewed source/shader/scene scope changed during publication')
        need(user_files(projects)==protected,'Live/B saved state changed during scene publication');verify_authored(after)
        subprocess.run([sys.executable,str(N/'check_unified.py'),'--publish'],check=True)
        next_=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),
                   shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),
                   guide=str(N/'GUIDE.txt'),binary_sha256=binary)
        atomic_bytes(CURRENT,(json.dumps(next_,indent=2)+'\n').encode())
        for mode in ('demo','editor','arena','joust','world'):
            subprocess.run([str(ROOT/'Launch_Valwood.sh'),mode,'--check-only'],cwd=ROOT,check=True)
        need(user_files(projects)==protected,'Publication changed saved state/preferences');verify_authored(after)
    except BaseException as original:
        errors=[]
        def restore(action):
            try:action()
            except BaseException as error:errors.append(str(error))
        restore(lambda:atomic_bytes(CURRENT,old))
        for path,target in link_before.items():restore(lambda p=Path(path),t=target:atomic_link(p,t))
        for target,backup in reversed(changed):restore(lambda p=target,b=backup:restore_leaf(p,b))
        restore(lambda:(N/'UNIFIED_READY.json').unlink(missing_ok=True))
        restore(lambda:verify_authored(authored))
        restore(lambda:need(user_files(projects)==protected,'Saved state differs after rollback'))
        (rollback/'ROLLBACK.json').write_text(json.dumps(dict(restored=not errors,original_error=str(original),rollback_errors=errors),indent=2)+'\n')
        if errors:raise RuntimeError('Publication failed; rollback needs review: '+'; '.join(errors)) from original
        raise
    result=dict(schema='valwood.river-swim-scene-publication.v1',passed=True,generation=str(N),binary_sha256=binary,
                scene_only=False,runtime_reused=False,application_source_changed=True,shader_bytes_changed=True,
                reviewed_shader_changes=checker().shader_scope()['shader_changes'],
                all_five_launchers_checked=True,live_save_and_settings_hashes_preserved=True,
                protected_authoring_files=len(authored),changed_project_files=[r['path'] for _,_,r in scene],
                reviewed_scene_after_sha256={r['path']:r['after_sha256'] for _,_,r in scene},
                latest_mode_state_and_graphics_carried=True,native_only=True,Windows_work_performed=False,
                rollback=str(rollback))
    (N/'PUBLICATION.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--check-only',action='store_true');ap.add_argument('--capture-guard',action='store_true');a=ap.parse_args()
    need(not(a.capture_guard and a.check_only),'Choose capture or preflight')
    lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
    try:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise RuntimeError('Valwood is running; close it before capturing/publishing the source-only update')
        capture_guard() if a.capture_guard else publish(a.check_only)
    finally:os.close(lock)


if __name__=='__main__':main()

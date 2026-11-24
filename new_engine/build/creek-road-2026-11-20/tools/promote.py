"""Publish the reviewed native campsite pass while preserving user state."""
from pathlib import Path
import argparse, fcntl, hashlib, importlib.util, json, os, shutil, sys, time
sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
E = N.parents[1]
R = E.parent
B = N.with_name('camp-atmosphere-2026-11-20')
L = E/'projects/valwood_current'
sys.path.insert(0, str(N))
from check_unified import (sha, read, need, stat, evidence, allowed_scene_path,
                           owned_project_leaf, verify_reuse,shader_delivery)


def _temporary(path):
    temporary = Path(path).with_name(Path(path).name+'.scene-art.tmp')
    # A dangling alias is occupied too. Never remove somebody else's leftover.
    need(not temporary.exists() and not temporary.is_symlink(), 'Unexpected temporary file')
    return temporary


def write(p, value):
    p = Path(p)
    temporary = _temporary(p)
    created = False
    try:
        with temporary.open('x') as output:
            created = True
            output.write(json.dumps(value, indent=2)+'\n')
        os.replace(temporary, p)
    finally:
        if created:
            temporary.unlink(missing_ok=True)


def atomic_copy(source, target):
    target = Path(target)
    temp = _temporary(target)
    created = False
    try:
        # Reserve the name exclusively before copy2 overwrites our own file.
        with temp.open('xb'):
            created = True
        shutil.copy2(source, temp)
        os.replace(temp, target)
    finally:
        if created:
            temp.unlink(missing_ok=True)


def atomic_link(path, target):
    path = Path(path)
    temp = _temporary(path)
    created = False
    try:
        temp.symlink_to(target, target_is_directory=True)
        created = True
        os.replace(temp, path)
    finally:
        if created:
            temp.unlink(missing_ok=True)


def mutable_state_files(project):
    project = Path(project)
    need(project.is_dir() and not project.is_symlink(), 'User project unexpectedly aliased')
    files = set()
    for folder in ('saves', 'settings'):
        root = project/folder
        need(not root.is_symlink(), 'User state directory unexpectedly aliased')
        if not root.exists():
            continue
        need(root.is_dir(), 'User state directory is not a directory')
        for p in root.rglob('*'):
            need(not p.is_symlink(), 'User state unexpectedly aliased')
            need(p.is_file() or p.is_dir(), 'Unsupported user state entry')
            if p.is_file():
                files.add(p)
    for pattern in ('*.settings', 'arena_*.cfg', 'player-profile.config'):
        for p in project.glob(pattern):
            need(p.is_file() and not p.is_symlink(), 'User state unexpectedly aliased')
            files.add(p)
    return sorted(files)


def relative_state(project, *, graphics=True):
    project = Path(project)
    return {str(p.relative_to(project)): sha(p) for p in mutable_state_files(project)
            if graphics or p.relative_to(project) != Path('graphics.settings')}


def state_files(projects):
    return {str(p): sha(p) for project in projects for p in mutable_state_files(project)}


def copy_state_exact(source, target):
    source, target = Path(source), Path(target)
    expected = relative_state(source)
    existing = mutable_state_files(target)  # Reject aliases before any deletion.
    for folder in ('saves', 'settings'):
        if (target/folder).exists():
            shutil.rmtree(target/folder)
        if (source/folder).exists():
            shutil.copytree(source/folder, target/folder)
    for p in existing:
        if len(p.relative_to(target).parts) == 1:
            p.unlink()
    for p in mutable_state_files(source):
        if len(p.relative_to(source).parts) == 1:
            shutil.copy2(p, target/p.name)
    need(relative_state(source) == expected, 'Accepted state changed while copying')
    need(relative_state(target) == expected, 'New mode did not receive exact accepted state')


def live_leaf(relative, *, absent=False):
    relative=Path(relative)
    need(not relative.is_absolute() and '..' not in relative.parts,'Escaping LIVE target')
    p=L/relative
    need(not L.is_symlink() and all(not q.is_symlink() for q in p.parents if q.is_relative_to(L)),
         'LIVE target parent unexpectedly aliased')
    if absent: need(not p.exists() and not p.is_symlink(),'New model target already exists')
    else: owned_project_leaf(L,relative)
    return p


def restore_publication(changes, new_links, rollback, ready_written, created_dirs, shader_links=()):
    errors=[]
    operations=[(str(target),lambda target=target:atomic_copy(
        rollback/'world-project'/target.relative_to(L),target)) for _,target in changes]
    def remove_own_link(path,target):
        if not path.exists() and not path.is_symlink(): return
        need(path.is_symlink() and str(path.resolve())==str(Path(target).resolve()),
             'Refusing to remove unexpected rollback model target')
        path.unlink()
    operations += [(str(path),lambda path=path,target=target:remove_own_link(path,target))
                   for path,target in new_links]
    operations += [(str(path),lambda path=path:path.rmdir()) for path in reversed(created_dirs)]
    def restore_shader(path,original,target):
        need(path.is_symlink() and path.resolve()==Path(target).resolve(),
             'Refusing to overwrite unexpected rollback shader binding')
        atomic_link(path,original)
    operations += [(str(path),lambda path=path,original=original,target=target:restore_shader(path,original,target))
                   for path,original,target in shader_links]
    operations.append((str(R/'CURRENT_VALWOOD.json'),lambda:atomic_copy(
        rollback/'CURRENT_VALWOOD.json',R/'CURRENT_VALWOOD.json')))
    if ready_written:
        operations.append((str(N/'UNIFIED_READY.json'),lambda:(N/'UNIFIED_READY.json').unlink(missing_ok=True)))
    for label,operation in operations:
        try: operation()
        except BaseException as error: errors.append(label+': '+type(error).__name__+': '+str(error))
    return errors


def preflight():
    need(not (N/'PUBLICATION.json').exists() and not (N/'UNIFIED_READY.json').exists(),
         'Published generations are immutable')
    current=read(R/'CURRENT_VALWOOD.json');baseline=read(N/'BASELINE.json')
    need(current.get('runtime_generation')==str(B) and sha(R/'CURRENT_VALWOOD.json')==baseline['current_pointer_sha256'],
         'Current generation changed since staging')
    verify_reuse()
    manifest=read(N/'review/CHANGESET.json');binary=evidence(manifest)
    # Resource expansion exists only in the private candidate. LIVE retains
    # its original owned parent directories and receives new aliases only.
    for name,overlay in manifest.get('resource_overlays',{}).items():
        need(name in ('surfaces','environments') and (L/name).is_dir() and not (L/name).is_symlink(),
             'LIVE resource overlay parent must remain owned')
        need(str((B/'world-project'/name).resolve())==overlay['original_alias']==str((L/name).resolve()),
             'Inherited resource overlay alias changed')
        for relative,row in overlay['original_leaves'].items():
            original=L/relative
            need(Path(relative).parts[0]==name and original.is_file() and sha(original)==row['sha256'],
                 'LIVE original resource changed: '+relative)
    for name,h in read(N/'review/LIVE_BEFORE.json').items():
        if name=='ground/showcase.height':
            p=L/name;record=baseline['live_height_alias']
            need(p.is_symlink() and p.is_file() and str(p)==record['path'] and
                 str(p.resolve())==record['target'] and sha(p)==record['sha256'],
                 'Unchanged LIVE height alias changed')
        else:p=owned_project_leaf(L,name)
        need(sha(p)==h,'LIVE authoring changed: '+name)
    changes=[];addition_paths={row['path'] for row in manifest['model_additions']}
    for relative,row in manifest['scene_delta'].items():
        if relative in addition_paths: continue
        need(allowed_scene_path(relative),'Unreviewed scene target')
        source=owned_project_leaf(N/'world-project',relative);target=live_leaf(relative)
        need(sha(source)==row['after']['sha256'] and sha(target)==row['before']['sha256'],'Scene conflict')
        changes.append((source,target))
    need(changes,'No reviewed scene changes')
    additions=[]
    for row in manifest['model_additions']:
        target=live_leaf(row['path'],absent=True);source=owned_project_leaf(N/'world-project',row['path'])
        need(sha(source)==row['sha256'],'New model source changed')
        additions.append((target,source))
    areas=[Path(p) for p in current['area_projects'].values()]
    shader_links=[]
    for project in [L,*areas]:
        need((project/'shaders').is_symlink() and str((project/'shaders').resolve())==baseline['live_shader_bindings'][str(project/'shaders')],
             'Existing shared shader binding changed')
        if manifest.get('shader_bank_reused') is False:
            shader_links.append((project/'shaders',os.readlink(project/'shaders'),N/'shared/shaders'))
    return current,manifest,binary,areas,changes,additions,shader_links


def create_parents(path,created):
    missing=[];parent=path.parent
    while not parent.exists():
        need(parent.is_relative_to(L) and not parent.is_symlink(),'Invalid new model parent')
        missing.append(parent);parent=parent.parent
    need(parent.is_dir() and not parent.is_symlink(),'New model parent must be owned')
    for parent in reversed(missing):
        parent.mkdir();created.append(parent)


def publish(current,manifest,binary,areas,changes,additions,shader_links=()):
    projects=[L,*areas,B/'arena-project',B/'joust-project',B/'shared']
    before_state=state_files(projects)
    rollback=N/'rollback';need(not rollback.exists(),'Retain/review existing rollback before retry')
    rollback.mkdir();shutil.copy2(R/'CURRENT_VALWOOD.json',rollback/'CURRENT_VALWOOD.json')
    for _,target in changes:
        dest=rollback/'world-project'/target.relative_to(L);dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(target,dest)
    # All mutable mode state is mirrored only into the new candidate. Nothing
    # is copied back over accepted saves, settings, authoring or workspaces.
    for mode in ('arena','joust'): copy_state_exact(B/(mode+'-project'),N/(mode+'-project'))
    settings=[p/'graphics.settings' for p in projects if (p/'graphics.settings').is_file()]
    need(settings,'No accepted graphics settings');newest=max(settings,key=lambda p:p.stat().st_mtime_ns)
    for project in (N/'shared',N/'arena-project',N/'joust-project'):
        shutil.copy2(newest,project/'graphics.settings')
    for mode in ('arena','joust'):
        need(relative_state(N/(mode+'-project'),graphics=False)==relative_state(B/(mode+'-project'),graphics=False),
             'Carried mode state differs')
        need(sha(N/(mode+'-project')/'graphics.settings')==sha(newest),'Carried latest graphics differ')
    ready_written=False;created=[];installed=[];installed_shaders=[]
    try:
        for source,target in changes: atomic_copy(source,target)
        for target,source in additions:
            live_leaf(target.relative_to(L),absent=True);create_parents(target,created)
            atomic_link(target,source);installed.append((target,source))
        for path,original,target in shader_links:
            need(path.is_symlink() and os.readlink(path)==original,'Shader binding changed during publication')
            atomic_link(path,target);installed_shaders.append((path,original,target))
        pins=dict(manifest['pins'])
        for p in (N/'review/CHANGESET.json',N/'review/FINAL_REVIEW.json',N/'check_unified.py',N/'launch.py'):
            pins[str(p)]=sha(p)
        ready=dict(schema='valwood.scene-art.ready.v1',ready=True,generation=str(N),baseline=str(B),
            runtime_reused=False,source_reused=False,shader_bank_reused=manifest.get('shader_bank_reused',True),Windows_work_performed=False,
            binary_sha256=binary,pins=pins,stats={p:stat(p) for p in pins},aliases=manifest['aliases'],
            resource_overlays=manifest.get('resource_overlays',{}),
            shader_changes=manifest.get('shader_changes',[]),shader_override=manifest.get('shader_override'),
            live_shader_bindings={str(path):str(target.resolve()) for path,_,target in shader_links},
            live_scene_sha256={str(t.relative_to(L)):sha(s) for s,t in changes},
            live_model_additions={str(t):str(s.resolve()) for t,s in additions})
        write(N/'UNIFIED_READY.json',ready);ready_written=True
        write(R/'CURRENT_VALWOOD.json',dict(current,runtime_generation=str(N),binary=str(N/'runtime/rpg'),
            source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),
            character_textures=str(B/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=binary))
        from check_unified import check
        check()
        need(state_files(projects)==before_state,'Accepted saves/settings changed during publication')
        write(N/'PUBLICATION.json',dict(passed=True,generation=str(N),baseline=str(B),runtime_reused=False,
            source_reused=False,shader_bank_reused=manifest.get('shader_bank_reused',True),Windows_work_performed=False,binary_sha256=binary,
            scene_delta=manifest['scene_delta'],model_additions=manifest['model_additions'],
            resource_overlays=manifest.get('resource_overlays',{}),
            shader_changes=manifest.get('shader_changes',[]),shader_override=manifest.get('shader_override'),
            live_save_and_settings_hashes_preserved=True,all_five_launchers_checked=True,
            published_unix=time.time(),rollback=str(rollback)))
    except BaseException as original:
        errors=restore_publication(changes,installed,rollback,ready_written,created,installed_shaders)
        if errors: raise RuntimeError('Publication failed; rollback could not restore: '+'; '.join(errors)) from original
        raise


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--apply',action='store_true');a=ap.parse_args()
    lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
    try:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        values=preflight()
        if not a.apply:
            print('Native campsite preflight passed; changes',len(values[4]),'new model aliases',len(values[5]));return
        publish(*values)
    finally: os.close(lock)


if __name__=='__main__': main()



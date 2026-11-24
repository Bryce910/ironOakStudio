"""Validate and copy actual native saves from a cleanly closed private review.

No process control or input APIs. The caller creates a fresh project using the
current candidate world; only regular native saves and graphics.settings from
the validated archived session are eligible to replace its initial defaults.
"""
from datetime import datetime,timezone
from pathlib import Path
import hashlib
import json
import shutil
import stat

class ResumeError(ValueError):pass

def validate_profile_prefix(prefix,review_directory):
    prefix=Path(prefix)
    if not prefix.is_absolute():raise ResumeError('Private profile prefix must be absolute.')
    prefix=prefix.resolve();review=Path(review_directory).resolve()
    if prefix==review or not prefix.is_relative_to(review):
        raise ResumeError('Private profile prefix must be below the new review directory.')
    return prefix

def sha256(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()

def regular(path,maximum=8*1024*1024):
    path=Path(path)
    try:info=path.lstat()
    except FileNotFoundError as exc:raise ResumeError('Required private resume file is missing: '+str(path)) from exc
    if not stat.S_ISREG(info.st_mode):raise ResumeError('Private resume files must be regular, without symlinks: '+str(path))
    if info.st_size>maximum:raise ResumeError('Private resume file exceeds its bounded size: '+str(path))
    return path

def json_record(path):return json.loads(regular(path).read_text())

def validate_resume_source(source,review_root,current_binary,destination=None,render_build_receipt=None):
    root=Path(review_root).resolve(strict=True);source=Path(source).resolve(strict=True)
    if source==root or not source.is_relative_to(root):raise ResumeError('Resume source must be an archived session below this candidate review directory.')
    if destination is not None and source==Path(destination).resolve():raise ResumeError('Archive the prior review before reusing its live session path.')
    project=source/'project'
    if project.is_symlink() or not project.is_dir():raise ResumeError('Private resume project must be a real directory.')
    marker=json_record(project/'PRIVATE_CAPTURE_PROJECT.json')
    if marker.get('private') is not True:raise ResumeError('Private resume marker is required.')
    preservation=json_record(source/'state-preservation.json')
    if type(preservation.get('exit_code')) is not int or preservation['exit_code']!=0 or preservation.get('live_state_unchanged') is not True:
        raise ResumeError('Resume requires a clean exit0 with verified unchanged published state.')
    launch=json_record(source/'launch.json');binary_hash=sha256(current_binary)
    compatibility=None
    if launch.get('binary_sha256')!=binary_hash:
        if render_build_receipt is None:raise ResumeError('Archived review was not run with the current candidate binary.')
        # Explicit cross-build save/load review is permitted only when the
        # complete compiled-source receipts prove a renderer-only correction.
        # Gameplay, save formats, controls and every other engine file must
        # match. The bytes copied below still come from actual native saves.
        old_path=source/'BUILD_RECEIPT.json';new_path=Path(render_build_receipt)
        old=json_record(old_path);new=json_record(new_path)
        if old.get('binary_sha256')!=launch.get('binary_sha256') or new.get('binary_sha256')!=binary_hash:
            raise ResumeError('Render-compatible resume build receipts do not match the actual binaries.')
        a=old.get('source_sha256');b=new.get('source_sha256')
        if not isinstance(a,dict) or not isinstance(b,dict) or not a or not b:
            raise ResumeError('Complete compiled source receipts are required for compatible resume.')
        changed=sorted(k for k in set(a)|set(b) if a.get(k)!=b.get(k))
        engine_changes=[k for k in changed if not k.startswith('tests/')]
        if engine_changes not in (['engine/world_model_cpu.cpp'],
                                 ['engine/world_model_cpu.cpp','engine/world_scenery_streaming.inc']):
            raise ResumeError('Compatible resume allows only the reviewed model texture preparation correction.')
        compatibility={'source_binary_sha256':launch['binary_sha256'],'engine_changes':engine_changes,
                       'old_build_receipt_sha256':sha256(old_path),'new_build_receipt_sha256':sha256(new_path),
                       'gameplay_and_save_sources_identical':True}
    saves=project/'saves'
    if saves.is_symlink() or not saves.is_dir():raise ResumeError('Native save directory must be a real private directory.')
    entries=sorted(saves.iterdir())
    if not entries or len(entries)>64:raise ResumeError('Private resume needs one to64 regular native save files.')
    files=[]
    for path in entries:
        regular(path);files.append({'relative':str(path.relative_to(project)),'sha256':sha256(path),'bytes':path.stat().st_size})
    for name,header in [('opening.progress','VALWOOD_OPENING 1'),('player.progress','VALWOOD_PLAYER_PROFILE 1')]:
        path=regular(saves/name)
        lines=path.read_text().splitlines()
        if not lines or lines[0]!=header:raise ResumeError('Required actual native save header is missing: '+name)
    # All root settings are required to be real files, even though the only
    # carried setting is the graphics choice made through the ordinary UI.
    for path in project.glob('*.settings'):regular(path)
    graphics=regular(project/'graphics.settings')
    files.append({'relative':'graphics.settings','sha256':sha256(graphics),'bytes':graphics.stat().st_size})
    return {'kind':'actual_native_private_resume','source_session':str(source),'source_project':str(project),
            'source_launch_sha256':sha256(source/'launch.json'),'source_preservation_sha256':sha256(source/'state-preservation.json'),
            'source_marker_sha256':sha256(project/'PRIVATE_CAPTURE_PROJECT.json'),'binary_sha256':binary_hash,
            'source_exit_code':0,'source_published_state_unchanged':True,'validated_utc':datetime.now(timezone.utc).isoformat(),
            'files':files,'seeded_checkpoint_written':False,'normal_continue_required':True,
            'render_build_compatibility':compatibility}

def copy_resume_files(provenance,destination):
    project=Path(provenance['source_project']);destination=Path(destination)
    for entry in provenance['files']:
        relative=Path(entry['relative'])
        if relative.is_absolute() or '..' in relative.parts:raise ResumeError('Unsafe resume relative path.')
        source=regular(project/relative)
        if sha256(source)!=entry['sha256']:raise ResumeError('Native resume file changed after validation: '+str(relative))
        target=destination/relative
        if target.exists() or target.is_symlink():regular(target)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
        if sha256(target)!=entry['sha256'] or sha256(source)!=entry['sha256']:
            raise ResumeError('Native resume copy did not preserve the original bytes: '+str(relative))

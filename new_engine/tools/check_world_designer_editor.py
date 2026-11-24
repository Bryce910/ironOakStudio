#!/usr/bin/env python3
"""Exercise the delivered Windows editor on disposable authoring files.

Wine's Z: current drive resolves the deliberately literal /tmp/ QA document.
That preserves the engine's existing private-document guard without changing
the shipped executable. The actual PASS marker is mandatory.
"""
from pathlib import Path
import argparse
import fcntl
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from benchmark_graphics_settings import native_environment

CASES = {
    'clipboard': ('VALWOOD_QA_EDITOR_COPY_PASTE', '[Editor clipboard QA] PASS ', 1100),
    'snap': ('VALWOOD_QA_EDITOR_TERRAIN_SNAP', '[Terrain snap QA] PASS ', 1050),
    'vfx': ('VALWOOD_QA_EDITOR_VFX', '[VFX QA] PASS ', 1150),
    'placement': ('VALWOOD_QA_EDITOR_PLACEMENT', '[Editor placement QA] PASS ', 1500),
}


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def win(path):
    return 'Z:'+str(Path(path).resolve()).replace('/', '\\')


def authoring_snapshot(project):
    return {p.name: sha(p) for p in sorted(project.iterdir())
            if p.is_file() and not p.name.endswith('.lock')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--project', type=Path, default=ROOT/'projects/valwood_world')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--action', choices=CASES, required=True)
    args = parser.parse_args()
    binary, source = args.binary.resolve(strict=True), args.project.resolve(strict=True)
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    binary_before, source_before = sha(binary), authoring_snapshot(source)
    flag, marker, frames = CASES[args.action]
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with tempfile.TemporaryDirectory(prefix='valwood-world-designer-'+args.action+'-', dir='/tmp') as folder:
            project = Path(folder)
            for entry in source.iterdir():
                if entry.name.startswith('.') or entry.name in {'saves','captures','logs','backups','qa'} or any(s in entry.name for s in ('.recovery','.journal','.previous')):
                    continue
                if entry.is_dir():
                    (project/entry.name).symlink_to(entry.resolve(), target_is_directory=True)
                elif entry.is_file():
                    shutil.copy2(entry, project/entry.name)
            document = project/'world.studio'; original = document.read_bytes()
            env, removed = native_environment()
            for key in list(env):
                if key.startswith('VALWOOD_'):
                    env.pop(key)
            env.update(WINEPREFIX=str(ROOT/'build/windows/wine-prefix'), WINEDEBUG='-all',
                       WINEDLLOVERRIDES='mscoree,mshtml=', SDL_AUDIODRIVER='dummy',
                       VALWOOD_QA_STUDIO_DOCUMENT=str(document), vblank_mode='0')
            env[flag] = '1'
            profile, capture = output/'profile.json', output/'editor.bmp'
            command = ['/usr/lib/wine/wine64', win(binary), '--project', win(project),
                '--editor', '--scenic','castle-finished-arcade','--frames',str(frames),
                '--warmup','0','--width','1280','--height','720','--auto-quality','off',
                '--render-scale','0.8','--profile',win(profile),'--capture',win(capture)]
            (output/'command.json').write_text(json.dumps(dict(command=command,
                binary_sha256=binary_before, private_document_literal=str(document),
                wine_current_drive='Z: from project/working directory',
                environment={k:v for k,v in env.items() if k.startswith(('VALWOOD_','WINE','SDL_')) or k=='vblank_mode'},
                removed_environment=removed),indent=2)+'\n')
            with (output/'windows.log').open('w') as log:
                code = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                    stderr=subprocess.STDOUT, timeout=300).returncode
            text = (output/'windows.log').read_text(errors='replace')
            world_path = Path(str(profile)+'.world.json')
            world = json.loads(world_path.read_text()) if world_path.exists() else {}
            metrics = json.loads(profile.read_text()) if profile.exists() else {}
            renderer = metrics.get('renderer','')
            hardware = bool(renderer) and not any(x in renderer.lower() for x in ('llvmpipe','softpipe','swrast'))
            saved = document.read_bytes()
            (output/'before.world.studio').write_bytes(original)
            (output/'saved.world.studio').write_bytes(saved)
            reopen = None
            if marker in text and code == 0:
                reopen_env = {key:value for key,value in env.items() if key != flag}
                reopen_profile, reopen_capture = output/'reopen.json', output/'reopen.bmp'
                reopen_command = list(command)
                reopen_command[reopen_command.index('--frames')+1] = '180'
                reopen_command[reopen_command.index('--profile')+1] = win(reopen_profile)
                reopen_command[reopen_command.index('--capture')+1] = win(reopen_capture)
                with (output/'reopen.log').open('w') as log:
                    reopen_code = subprocess.run(reopen_command, cwd=ROOT, env=reopen_env,
                        stdout=log, stderr=subprocess.STDOUT, timeout=300).returncode
                reopen_world_path = Path(str(reopen_profile)+'.world.json')
                reopen_world = json.loads(reopen_world_path.read_text()) if reopen_world_path.exists() else {}
                reopen_text = (output/'reopen.log').read_text(errors='replace')
                reopen_checks = dict(exit0=reopen_code==0,
                    same_saved_document=document.read_bytes()==saved,
                    same_rendered_instances=reopen_world.get('authored_instances')==world.get('authored_instances'),
                    same_added_models=reopen_world.get('studio',{}).get('added_models')==world.get('studio',{}).get('added_models'),
                    no_rejected_scene=reopen_world.get('studio',{}).get('rejected')==0,
                    gl0=reopen_world.get('gl_error')==0,
                    texture_refs0=reopen_world.get('remaining_texture_references')==0,
                    no_saved_scene_rejection=not any(s in reopen_text for s in ('[Studio] Saved scene rejected','Saved asset definitions could not load')),
                    actual_capture=reopen_capture.is_file())
                if args.action == 'vfx':
                    reopen_checks['all_eight_saved_effects_loaded'] = reopen_world.get('runtime_editors',{}).get('particles') == 8
                reopen=dict(passed=all(reopen_checks.values()),checks=reopen_checks,
                    studio=reopen_world.get('studio'))
                if reopen_capture.is_file():
                    from PIL import Image
                    Image.open(reopen_capture).save(output/'reopen.png')
            checks = dict(exit0=code==0, explicit_actual_qa_pass=marker in text,
                no_qa_failure=not any('[%s' % p in text for p in ('Editor clipboard QA] FAIL','Terrain snap QA] FAIL','VFX QA] FAIL','Editor placement QA] FAIL')),
                hardware_renderer=hardware, gl0=world.get('gl_error')==0,
                no_texture_references=world.get('remaining_texture_references')==0,
                saved_document_changed=saved!=original,
                source_unchanged=authoring_snapshot(source)==source_before,
                binary_unchanged=sha(binary)==binary_before, actual_capture=capture.is_file(),
                saved_scene_reopens=reopen is not None and reopen['passed'])
            result=dict(passed=all(checks.values()),checks=checks,exit=code,
                binary=str(binary),binary_sha256=binary_before,renderer=renderer,
                actual_pass_marker=marker,studio=world.get('studio'),reopen=reopen,
                before_sha256=hashlib.sha256(original).hexdigest(), saved_sha256=hashlib.sha256(saved).hexdigest(),
                source_project=str(source),source_authoring_sha256=source_before,
                scope='Actual Windows executable/editor SDL workflow under Wine on hardware GPU; not a performance or physical Windows certification.')
            if capture.is_file():
                from PIL import Image
                Image.open(capture).save(output/'editor.png')
            (output/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
            print(json.dumps(result,indent=2))
            return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

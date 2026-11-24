#!/usr/bin/env python3
"""Portable World Designer actions. Uses independent seeds and working copies.

The shipped copy lives beside Install_Artist_Bundle.py. All content mutations
are confined to a NEW sibling *_Work directory, never the extracted bundle.
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
from datetime import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid

sys.dont_write_bytecode = True
PROJECTS = {
    'city': ('Castle City & Countryside', 'city-walk'),
    'jungle': ('Expanded Jungle', 'rainforest-milestone'),
    'grandstone': ('Grandstone Stronghold', 'stronghold-town'),
    'opening': ('Apartment & Valois Arrival', 'city-meadow'),
}
MARKER = 'world-designer-workspace.json'
SCHEMA = 'valwood.world-designer.workspace.v1'


def canonical(value):
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True, allow_nan=False)+'\n').encode()


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def ordinary(path):
    path = Path(path).absolute()
    for part in [*reversed(path.parents), path]:
        info = part.lstat()
        if part.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Links and junctions are not supported: '+str(part))
    return path


def atomic(path, payload):
    path = Path(path)
    if path.exists(): ordinary(path)
    fd, name = tempfile.mkstemp(prefix='.designer-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(payload); stream.flush(); os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        Path(name).unlink(missing_ok=True)


def clean_environment(environment=None):
    return {key: value for key, value in (environment or os.environ).items()
            if not key.upper().startswith('VALWOOD_')}


def editor_command(workspace, label):
    if label not in PROJECTS: raise ValueError('Unknown world project')
    return [str(workspace/'bin/rpg.exe'), '--project', str(workspace/'project'),
            '--editor', '--scenic', PROJECTS[label][1], '--width', '1600', '--height', '900']


@contextmanager
def exclusive(path):
    """Cross-process, automatically released lock, including during editor use."""
    if Path(path).exists(): ordinary(path)
    with open(path, 'a+b') as stream:
        if stream.tell() == 0: stream.write(b'0'); stream.flush()
        stream.seek(0)
        try:
            if os.name == 'nt':
                import msvcrt
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise ValueError('The designer is already copying, exporting, or running the editor. Close the editor before another world action.') from error
        try: yield
        finally:
            if os.name == 'nt':
                stream.seek(0); msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)


def process_identity(pid):
    """A PID plus creation time and full image path prevents PID-reuse mistakes."""
    if os.name != 'nt': return None
    import ctypes
    from ctypes import wintypes
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    api.OpenProcess.restype = wintypes.HANDLE
    api.CloseHandle.argtypes = (wintypes.HANDLE,)
    api.GetProcessTimes.argtypes = (wintypes.HANDLE, *([ctypes.POINTER(wintypes.FILETIME)]*4))
    api.QueryFullProcessImageNameW.argtypes = (wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD))
    api.GetExitCodeProcess.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
    handle = api.OpenProcess(0x1000, False, pid)
    if not handle:
        if ctypes.get_last_error() == 5: raise ValueError('Cannot verify the existing editor process; close it before continuing.')
        return None
    try:
        code = wintypes.DWORD()
        if not api.GetExitCodeProcess(handle, ctypes.byref(code)): raise OSError(ctypes.get_last_error(), 'Cannot inspect editor')
        if code.value != 259: return None
        times = [wintypes.FILETIME() for _ in range(4)]
        length = wintypes.DWORD(32768); name = ctypes.create_unicode_buffer(length.value)
        if not api.GetProcessTimes(handle, *[ctypes.byref(value) for value in times]): raise OSError(ctypes.get_last_error(), 'Cannot inspect editor time')
        if not api.QueryFullProcessImageNameW(handle, 0, name, ctypes.byref(length)): raise OSError(ctypes.get_last_error(), 'Cannot inspect editor path')
        return dict(pid=pid, created=(times[0].dwHighDateTime << 32)|times[0].dwLowDateTime,
                    executable=os.path.normcase(os.path.abspath(name.value)))
    finally: api.CloseHandle(handle)


class Designer:
    def __init__(self, bundle, status=None):
        self.bundle = ordinary(Path(bundle).absolute())
        self.base = self.bundle.parent/(self.bundle.name+'_Work')
        if self.base.exists(): ordinary(self.base)
        else: self.base.mkdir()
        for name in ('Seeds', 'Workspaces', 'Returns', 'Logs'):
            path = self.base/name
            if path.exists(): ordinary(path)
            else: path.mkdir()
        self.status_file = Path(status).absolute() if status else None
        if self.status_file and self.status_file.parent != self.base/'Logs':
            raise ValueError('Status file must be in the designer Logs directory')
        self.log = self.base/'Logs'/('designer-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+str(os.getpid())+'.log')
        module_path = ordinary(self.bundle/'Install_Artist_Bundle.py')
        spec = importlib.util.spec_from_file_location('_valwood_designer_installer', module_path)
        self.installer = importlib.util.module_from_spec(spec); spec.loader.exec_module(self.installer)
        self.status('Checking the world bundle…')
        _, self.index, self.contents = self.installer.load_index(self.bundle)
        self.projects = {row['label']: row for row in self.index['projects']}

    def status(self, message):
        with self.log.open('a', encoding='utf-8') as stream:
            stream.write(datetime.now().isoformat(timespec='seconds')+' '+message+'\n')
        print(message, flush=True)
        if self.status_file: atomic(self.status_file, (message+'\n').encode('utf-8'))

    def project(self, label):
        if label not in PROJECTS or label not in self.projects: raise ValueError('This bundle does not contain the selected world project.')
        return self.projects[label]

    def manifest(self, label):
        row = self.project(label)
        raw = self.installer.read_checked(self.bundle, self.contents[row['manifest_sha256']], limit=self.installer.MAX_METADATA)
        doc = json.loads(raw)
        if doc['content_id'] != row['package_content_id'] or doc['project_id'] != row['project_id']:
            raise ValueError('Selected project identity does not match its package')
        if hashlib.sha256(canonical(doc['files'])).hexdigest() != doc['content_id']:
            raise ValueError('Selected project manifest is corrupt')
        return doc, raw

    def helpers(self, seed, manifest):
        """Check executable helper bytes before importing from the pristine seed."""
        entries = {row['path']: row for row in manifest['files']}
        for name in ('tools/world_artist_handoff.py', 'tools/build_world_package.py',
                     'tools/world_asset_database.py', 'tools/world_package_cache_selection.py', 'tools/windows_file_io.py'):
            row = entries.get(name)
            if not row: raise ValueError('Seed is missing a required artist helper: '+name)
            path = ordinary(seed/name)
            if path.stat().st_size != row['bytes'] or digest(path) != row['sha256']:
                raise ValueError('Seed helper changed: '+name)
        sys.path.insert(0, str(seed/'tools'))
        import world_artist_handoff
        return world_artist_handoff

    def seed(self, label):
        manifest, raw = self.manifest(label)
        seed = self.base/'Seeds'/label
        if seed.exists():
            ordinary(seed)
            if ordinary(seed/'package-manifest.json').read_bytes() != raw:
                raise ValueError('Existing seed belongs to another bundle revision. Keep it and extract this bundle into a new folder.')
            helper = self.helpers(seed, manifest)
            self.status('Verifying the original '+PROJECTS[label][0]+' seed…')
            helper.verify_package(seed)
        else:
            self.status('Installing '+PROJECTS[label][0]+' ('+format(manifest['total_bytes']/1e9, '.1f')+' GB). Please keep this window open…')
            self.installer.install(self.bundle, label, seed)
            helper = self.helpers(seed, manifest)
        return seed, manifest, helper

    def state(self):
        path = self.base/'workspace-pointers.json'
        if not path.exists(): return {}
        ordinary(path)
        value = json.loads(path.read_text(encoding='utf-8'))
        if not isinstance(value, dict) or any(label not in PROJECTS or not isinstance(name, str) for label, name in value.items()):
            raise ValueError('Designer workspace selection is invalid. Existing work has been preserved.')
        return value

    def workspace_path(self, label):
        self.project(label)
        name = self.state().get(label, label)
        self.installer.safe_name(name)
        if '/' in name or not (name == label or name.startswith(label+'-')):
            raise ValueError('Workspace selection does not belong to the selected project')
        return self.base/'Workspaces'/name

    def select(self, label, workspace):
        state = self.state(); state[label] = workspace.name
        atomic(self.base/'workspace-pointers.json', canonical(state))

    def verify_workspace(self, workspace, label, manifest, helper):
        ordinary(workspace)
        marker = json.loads(ordinary(workspace/MARKER).read_text(encoding='utf-8'))
        expected = dict(schema=SCHEMA, label=label, project_id=manifest['project_id'],
                        package_content_id=manifest['content_id'])
        if marker != expected: raise ValueError('This working copy belongs to another project or revision. It has not been changed.')
        if json.loads(ordinary(workspace/'package-manifest.json').read_text()) != manifest:
            raise ValueError('Working copy manifest changed. Existing work has been preserved.')
        baseline = helper.baseline_document(workspace)
        expected_files = {row['path'][8:]: {key: row[key] for key in ('sha256','bytes')}
                          for row in manifest['files'] if row['path'].startswith('project/') and helper.authoring_path(row['path'][8:])}
        if baseline['package_content_id'] != manifest['content_id'] or baseline['files'] != expected_files:
            raise ValueError('Working copy baseline changed. It will not be reset or overwritten.')
        # Personal scene edits are allowed; executable helpers/runtime stay pinned.
        for row in manifest['files']:
            if row['path'].startswith(('bin/', 'tools/')):
                path = ordinary(workspace/row['path'])
                if path.stat().st_size != row['bytes'] or digest(path) != row['sha256']:
                    raise ValueError('Working copy runtime/helper changed: '+row['path'])
        return workspace

    def new_workspace(self, label, seed, manifest, helper, fresh=False):
        workspace = self.workspace_path(label)
        if workspace.exists() and not fresh:
            return self.verify_workspace(workspace, label, manifest, helper)
        if fresh:
            workspace = self.base/'Workspaces'/(label+'-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
        required = manifest['total_bytes'] + 128*1024*1024
        if shutil.disk_usage(workspace.parent).free < required:
            raise ValueError('Not enough free disk space for an independent working copy ('+format(required/1e9, '.1f')+' GB required).')
        self.status('Creating an independent '+PROJECTS[label][0]+' working copy. Please keep this window open…')
        helper.create_workspace(seed, workspace)
        with (workspace/MARKER).open('xb') as stream:
            stream.write(canonical(dict(schema=SCHEMA, label=label, project_id=manifest['project_id'], package_content_id=manifest['content_id'])))
        self.verify_workspace(workspace, label, manifest, helper)
        self.select(label, workspace)
        return workspace

    def guard_editor(self):
        record = self.base/'editor-session.json'
        if not record.exists(): return
        ordinary(record); data = json.loads(record.read_text())
        actual = process_identity(data['pid'])
        if actual and actual == data:
            raise ValueError('The editor is still open. Save and close it before opening another project, making a copy, or exporting changes.')

    def open_editor(self, workspace, label):
        logdir = workspace/'logs'
        if logdir.exists(): ordinary(logdir)
        else: logdir.mkdir()
        log = logdir/'editor-latest.log'
        if log.exists(): ordinary(log)
        command = editor_command(workspace, label)
        if len(str(workspace/'project').encode('utf-8')) >= 1024:
            raise ValueError('The working-copy path is too long. Extract the bundle into a shorter writable folder.')
        self.status('Opening '+PROJECTS[label][0]+'. First load may take time…')
        with log.open('w', encoding='utf-8') as stream:
            stream.write('[World Designer] '+json.dumps(command, ensure_ascii=False)+'\n'); stream.flush()
            child = subprocess.Popen(command, cwd=workspace, env=clean_environment(), stdin=subprocess.DEVNULL,
                                     stdout=stream, stderr=subprocess.STDOUT, shell=False, close_fds=True)
            identity = process_identity(child.pid)
            if identity: atomic(self.base/'editor-session.json', canonical(identity))
            self.status('Editor open — save and close its window to return here. Log: '+str(log))
            code = child.wait()
        if identity:
            record = self.base/'editor-session.json'
            if record.exists() and json.loads(record.read_text()) == identity: record.unlink()
        if code: raise ValueError('The editor closed with code '+str(code)+'. Please send logs/editor-latest.log to the owner.')
        self.status('Editor closed. Saved work is ready to reopen or return.')
        return dict(ok=True, workspace=str(workspace), editor_exit=code)

    def run(self, action, label):
        self.project(label)
        if action == 'guide':
            target = ordinary(self.bundle/'WORLD_DESIGNER_GUIDE.md')
            if os.name == 'nt':
                try: os.startfile(target)
                except OSError:
                    # Markdown has no default association on many clean PCs.
                    subprocess.Popen(['notepad.exe', str(target)], shell=False, close_fds=True)
            self.status('Opened the World Designer guide.')
            return dict(ok=True, guide=str(target))
        if action == 'folder':
            target = self.workspace_path(label)
            if not target.exists(): target = self.base/'Workspaces'
            ordinary(target)
            if os.name == 'nt': os.startfile(target)
            self.status('Opened '+str(target))
            return dict(ok=True, folder=str(target))
        with exclusive(self.base/'.designer.lock'):
            self.guard_editor()
            seed, manifest, helper = self.seed(label)
            if action == 'export':
                workspace = self.workspace_path(label)
                if not workspace.exists(): raise ValueError('Open this project and save your work before returning changes.')
                self.verify_workspace(workspace, label, manifest, helper)
                target = self.base/'Returns'/(label+'-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
                self.status('Checking saved changes for the owner…')
                result = helper.export_return(workspace, target)
                if os.name == 'nt': os.startfile(target)
                self.status('Return ready: '+str(target))
                return result
            workspace = self.new_workspace(label, seed, manifest, helper, fresh=action == 'new')
            if action == 'new':
                if os.name == 'nt': os.startfile(workspace)
                self.status('New workcopy selected: '+str(workspace))
                return dict(ok=True, workspace=str(workspace), new=True)
            if action == 'open': return self.open_editor(workspace, label)
            raise ValueError('Unknown designer action')


def main(argv=None):
    # Redirected Windows Python defaults to the system ANSI code page. Status
    # can contain any Unicode work-folder name; keep both captured streams UTF-8.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8', errors='backslashreplace')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle-root', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--action', choices=('open','new','export','folder','guide'), required=True)
    parser.add_argument('--project', choices=tuple(PROJECTS), required=True)
    parser.add_argument('--status-file', type=Path)
    args = parser.parse_args(argv)
    designer = None
    try:
        designer = Designer(args.bundle_root, args.status_file)
        result = designer.run(args.action, args.project)
        print(json.dumps(result, ensure_ascii=True), flush=True)
        return 0
    except (OSError, ValueError, KeyError, TypeError, ImportError) as error:
        message = 'Action stopped: '+str(error)
        if designer: designer.status(message)
        else:
            print(message, flush=True)
            if args.status_file:
                expected = args.bundle_root.absolute().parent/(args.bundle_root.name+'_Work')/'Logs'
                if args.status_file.absolute().parent == expected and expected.is_dir():
                    atomic(args.status_file, (message+'\n').encode('utf-8'))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Stream one or more validated Windows artist seeds into a deduplicated ZIP.

No project/asset staging copy is made. Each SHA256 payload occurs once, except
the normal embedded-Python bootstrap filenames needed before installation.
The installer creates independent ordinary files in a NEW selected seed.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
import hashlib
import io
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import zipfile

sys.dont_write_bytecode = True
import build_windows_package as windows
from build_world_package import _native_hash, _publish_directory
from world_asset_database import VERSION, ValidationError, canonical_json, open_regular_nofollow, sha256_bytes
from install_windows_artist_bundle import SCHEMA, safe_label, safe_names

WRAPPER = b'import runpy,sys\nfrom pathlib import Path\nsys.dont_write_bytecode=True\nroot=Path(__file__).resolve().parents[2]\nsys.path.insert(0,str(root/"tools"))\nargs=sys.argv[1:]\nif "--engine-root" in args:\n i=args.index("--engine-root")\n if i+1<len(args) and Path(args[i+1]).resolve()==root/"bin":args[i+1]=str(root)\nsys.argv=[str(root/"tools/build_windows_package.py")]+args\nrunpy.run_path(sys.argv[0],run_name="__main__")\n'
README = b'Valwood custom engine - Windows x64 artist seed\r\n\r\nRead ARTIST_START_HERE.md. Run Create_Artist_Workspace.bat and work in the NEW copy.\r\nLaunch_Editor.bat starts Studio; Return_Changes.bat exports saved changes.\r\nPython and runtime DLLs are bundled; no administrator install is needed.\r\nA GPU vendor OpenGL 3.3 driver is required. Native Windows GPU/input/performance\r\nacceptance must be completed on the artist PC; it is not certified by this ZIP.\r\nOriginal archives, private saves and unsaved drafts are excluded.\r\nAsset license rights remain with the owner and permitted collaborators.\r\n'


@dataclass
class Entry:
    name: str
    digest: str
    size: int
    source: Path | None = None
    expected_source_hash: str | None = None
    identity: tuple | None = None
    lexical: Path | None = None
    payload: bytes | None = None

    def record(self):
        return dict(path=self.name, bytes=self.size, sha256=self.digest, mode='0644')


def generated(name, payload):
    return Entry(name, sha256_bytes(payload), len(payload), payload=payload)


def source_entry(name, path, *, source_hash=None, identity=None, lexical=None, payload=None):
    path = Path(path).absolute()
    info = path.stat()
    digest = source_hash or _native_hash(path)
    return Entry(name, sha256_bytes(payload) if payload is not None else digest,
                 len(payload) if payload is not None else info.st_size, source=path,
                 expected_source_hash=digest, identity=identity or (info.st_dev, info.st_ino),
                 lexical=lexical, payload=payload)


def plan_package(validated, mode='development', *, world_designer=False):
    """Same validated inputs/batches/metadata contract as the ordinary builder."""
    inventory, database, runtime_files, extras, native = validated
    entries = []
    for row in database['files']:
        item = inventory.sources[row['path']]
        entry = source_entry('project/'+row['path'], item.resolved, source_hash=item.source_hash,
                             identity=item.identity, lexical=item.lexical, payload=item.content)
        if entry.digest != row['sha256'] or entry.size != row['bytes']:
            raise ValidationError('Inventory changed: '+row['path'])
        entries.append(entry)
    for name, source in runtime_files+extras:
        payload = None
        if source.suffix.casefold() in ('.md', '.txt'):
            text = source.read_text(encoding='utf-8').replace(str(inventory.engine), 'engine-source')
            payload = re.sub(r'(?:/home/|/Users/|/tmp/)[^\s"<>]+', 'source-file', text).encode()
        entries.append(source_entry(name, source, payload=payload))
    for action, name in [('launch','Launch_Editor.bat'), ('create','Create_Artist_Workspace.bat'),
                         ('export','Return_Changes.bat'), ('verify','Verify_Seed.bat'),
                         ('memory','Launch_Memory_Editor.bat')]:
        entries.append(generated(name, windows.batch(action)))
    readme = README
    if world_designer:
        readme = (b'Valwood World Designer - Windows x64 reference/work package\r\n\r\n'
            b'Use Valwood World Designer.exe in the extracted delivery folder.\r\n'
            b'Select the area and choose Open Editor. It creates or reuses an\r\n'
            b'independent working copy beside the delivery, under <bundle>_Work.\r\n'
            b'Keep the original Seeds package unchanged. Work in Workspaces.\r\n'
            b'Guide opens WORLD_DESIGNER_GUIDE.md with the learning exercise.\r\n'
            b'Save and close the editor, then choose Return Saved Changes.\r\n\r\n'
            b'The included .bat actions are advanced manual fallbacks.\r\n'
            b'No Unity or separate Python installation is required.\r\n'
            b'A GPU vendor OpenGL 3.3 driver is required; complete the receiving-PC checks.\r\n')
    entries += [generated('bin/tools/build_world_package.py', WRAPPER), generated('README.txt', readme),
                generated('asset-database.json', canonical_json(database)),
                generated('native-dependencies.json', canonical_json(native))]
    provenance = dict(schema_version=VERSION, project_id=database['project_id'], records=database['provenance'],
        credit_files=sorted(e.name for e in entries if 'CREDITS' in e.name or e.name.startswith('licenses/')))
    entries.append(generated('asset-provenance.json', canonical_json(provenance)))
    recipe = dict(builder='build_windows_package.py', target='windows-x64', mode=mode,
        project_id=database['project_id'], builder_sha256=_native_hash(Path(windows.__file__).resolve()),
        executable_sha256=_native_hash(dict(runtime_files)['bin/rpg.exe']),
        transport_builder='build_windows_artist_bundle.py',
        transport_builder_sha256=_native_hash(Path(__file__).resolve()),
        pruning=database.get('managed_revision_selection') or 'Full validated inventory; no implicit pruning')
    entries.append(generated('build-recipe.json', canonical_json(recipe)))
    entries.sort(key=lambda e:e.name)
    windows.windows_names([e.name for e in entries])
    safe_names([e.name for e in entries]+['package-manifest.json'])
    records = [e.record() for e in entries]
    manifest = dict(schema='valwood.windows-package', schema_version=VERSION, project_id=database['project_id'],
        scope=database['scope'], content_id=sha256_bytes(canonical_json(records)), files=records,
        total_bytes=sum(e.size for e in entries))
    return entries, generated('package-manifest.json', canonical_json(manifest)), manifest


def copy_verified(entry, output=None):
    """Hash every source stream, including sanitized sources and duplicate files."""
    if entry.source is not None:
        if entry.lexical is not None and entry.lexical.resolve(strict=True) != entry.source:
            raise ValidationError('Source link changed: '+entry.name)
        with open_regular_nofollow(entry.source) as stream:
            before = os.fstat(stream.fileno())
            if (before.st_dev, before.st_ino) != entry.identity:
                raise ValidationError('Source replaced: '+entry.name)
            digest = hashlib.sha256()
            while block := stream.read(1024*1024):
                digest.update(block)
                if entry.payload is None and output is not None:
                    output.write(block)
            after = os.fstat(stream.fileno())
        if (digest.hexdigest() != entry.expected_source_hash or
            (before.st_size, before.st_mtime_ns, before.st_ctime_ns) !=
            (after.st_size, after.st_mtime_ns, after.st_ctime_ns)):
            raise ValidationError('Source changed during bundle build: '+entry.name)
    if entry.payload is not None:
        if sha256_bytes(entry.payload) != entry.digest or len(entry.payload) != entry.size:
            raise ValidationError('Generated payload changed: '+entry.name)
        if output is not None:
            output.write(entry.payload)


def zip_info(name):
    item = zipfile.ZipInfo(name, date_time=(1980,1,1,0,0,0))
    item.compress_type = zipfile.ZIP_DEFLATED
    item._compresslevel = 3
    item.create_system = 3
    item.external_attr = 0o100644 << 16
    return item


def install_batch(label):
    return ('@echo off\r\nsetlocal\r\nset "PYTHONDONTWRITEBYTECODE=1"\r\n'
        '"%~dp0bin\\python\\python.exe" -B "%~dp0Install_Artist_Bundle.py" --bundle-root "%~dp0." --project '+label+' %*\r\n'
        'set "result=%errorlevel%"\r\nif not "%result%"=="0" echo Installation failed. Existing folders were not replaced.\r\n'
        'pause\r\nexit /b %result%\r\n').encode()


def override_docs(validated, docs_root):
    """Explicit new handoff guides; historical/default packaging stays intact."""
    if docs_root is None:
        return validated
    docs_root = Path(docs_root).absolute()
    if docs_root.is_symlink() or not docs_root.is_dir():
        raise ValidationError('Docs root must be an ordinary directory')
    inventory, database, runtime_files, extras, native = validated
    replacements = {}
    for name in ('ARTIST_START_HERE.md', 'EDITOR_GUIDE.md', 'FIRST_ASSIGNMENT.md',
                 'TESTING_STATUS.md', 'ACCEPTANCE_CHECKLIST.csv', 'WINDOWS_BUILD.md',
                 'WORLD_DESIGNER_GUIDE.md'):
        path = docs_root/name
        if path.is_symlink() or not path.is_file():
            raise ValidationError('Missing ordinary handoff guide: '+name)
        replacements[name] = path
    legacy_briefs = {'HANDOFF_PLAN.md', 'DEMO_GUIDE.md', 'PRESENTATION_GUIDE.md',
                     'CHARACTER_ASSETS.md', 'voice-recording-script.csv'}
    extras = [(name,path) for name,path in extras if name not in replacements and name not in legacy_briefs]
    extras += list(replacements.items())
    return inventory, database, runtime_files, extras, native


def build_bundle(projects, output, *, mode='development', extra_bootstrap=(), docs_root=None):
    """`projects` is a list of (safe label, existing Windows validated tuple)."""
    if not projects:
        raise ValidationError('At least one named project is required')
    labels = [safe_label(label) for label, _ in projects]
    if len(labels) != len(set(label.casefold() for label in labels)):
        raise ValidationError('Duplicate project label')
    output = Path(output).absolute()
    output = output.parent.resolve(strict=True)/output.name
    windows.windows_names([output.name])
    safe_names([output.name])
    if output.suffix.casefold() != '.zip' or output.exists() or output.is_symlink():
        raise ValidationError('Output must be a NEW .zip file')
    for _, validated in projects:
        if output.is_relative_to(validated[0].project):
            raise ValidationError('Output must be outside every source project')
    bootstrap_extras = []
    for name, path in extra_bootstrap:
        # These caller-selected launcher/guide files live at the bundle root.
        # They cannot replace a blob, installer, Python runtime or index.
        if '/' in name or '\\' in name or Path(name).suffix.casefold() not in ('.exe','.py','.md','.txt','.csv'):
            raise ValidationError('Bootstrap extras must be named root launcher/guide files')
        safe_names([name])
        path = Path(path).absolute()
        if path.is_symlink() or not path.is_file():
            raise ValidationError('Bootstrap source must be an ordinary file: '+name)
        bootstrap_extras.append(source_entry(name,path))
    planned = []
    python_files = {}
    world_designer = any(entry.name == 'Valwood World Designer.exe' for entry in bootstrap_extras)
    for label, validated in projects:
        entries, manifest_entry, manifest = plan_package(override_docs(validated,docs_root), mode,world_designer=world_designer)
        planned.append((label, entries, manifest_entry, manifest))
        for entry in entries:
            if entry.name.startswith('bin/python/'):
                previous = python_files.setdefault(entry.name, entry)
                if (previous.digest, previous.size) != (entry.digest, entry.size):
                    raise ValidationError('Projects require different Python bootstrap files')
    if 'bin/python/python.exe' not in python_files:
        raise ValidationError('Missing Windows Python bootstrap')
    installer = generated('Install_Artist_Bundle.py', Path(__file__).with_name('install_windows_artist_bundle.py').read_bytes())
    instructions = ('Valwood Windows artist content bundle\r\n\r\nExtract the ENTIRE ZIP, keeping its folders. Python is included.\r\n'
        'Tomorrow is system orientation and review, not an art assignment.\r\n'
        'Install ONE project at a time to look around and learn the editor.\r\n'
        'Double-click Install_LABEL.bat for the project you want. It creates a NEW\r\n'
        'sibling Valwood_LABEL_Seed directory; --output "NEW PATH" is optional.\r\n'
        'The seed contains independent ordinary files (no links). Allow its full\r\n'
        'size PLUS another working copy before Create_Artist_Workspace.bat.\r\n'
        'Then use Create_Artist_Workspace.bat, Launch_Editor.bat and Return_Changes.bat\r\n'
        'inside that selected seed/working copy, following ARTIST_START_HERE.md.\r\n'
        'Native Windows GPU/editor/performance acceptance remains pending.\r\n\r\n'+
        '\r\n'.join(label+': '+str(manifest['total_bytes'])+' bytes per materialized seed'
                      for label, _, _, manifest in planned)+'\r\n').encode()
    if world_designer:
        instructions = ('Valwood World Designer - Windows x64\r\n\r\n'
            'Extract the ENTIRE ZIP to a writable local folder, keeping every folder.\r\n'
            'Run Valwood World Designer.exe. Select an area and click Open Editor.\r\n'
            'On first use, it automatically verifies the seed and creates an independent\r\n'
            'working copy beside the delivery in <bundle folder>_Work. Later opens\r\n'
            'reuse that workcopy. New Workcopy preserves previous work.\r\n\r\n'
            'Guide opens WORLD_DESIGNER_GUIDE.md. Begin with the world tour and\r\n'
            'small-prop placement, copy/paste, terrain snap and save/reopen exercise.\r\n'
            'After saving and closing the editor, choose Return Saved Changes.\r\n'
            'This assignment is world/environment design.\r\n\r\n'
            'Allow space for this extracted delivery PLUS the selected reference seed\r\n'
            'and its independent working copy. The first-open check reports low space.\r\n'
            'Python and runtime libraries are bundled. Unity is not needed.\r\n'
            'Complete the receiving Windows PC checks described in TESTING_STATUS.md.\r\n\r\n'+
            '\r\n'.join(label+': '+str(manifest['total_bytes'])+' bytes per seed, plus the same space for one workcopy'
                for label, _, _, manifest in planned)+'\r\n').encode()
    bootstrap = [*python_files.values(), installer, generated('READ_ME_FIRST.txt', instructions), *bootstrap_extras]
    bootstrap += [generated('Install_'+label+'.bat', install_batch(label)) for label in labels]
    safe_names([e.name for e in bootstrap]+['bundle-index.json'])
    canonical_members = {}
    for entry in python_files.values():
        canonical_members.setdefault(entry.digest, entry.name)
    fd, temporary_name = tempfile.mkstemp(prefix='.valwood-artist-bundle-', suffix='.zip', dir=output.parent)
    os.close(fd)
    temporary = Path(temporary_name)
    try:
        contents = {}
        project_rows = []
        with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=3, allowZip64=True) as archive:
            for entry in bootstrap:
                with archive.open(zip_info(entry.name), 'w', force_zip64=True) as stream:
                    copy_verified(entry, stream)
                if entry.name.startswith('bin/python/'):
                    contents.setdefault(entry.digest, dict(sha256=entry.digest, bytes=entry.size,
                                                          member=canonical_members[entry.digest]))
            for label, entries, manifest_entry, manifest in planned:
                for entry in [*entries, manifest_entry]:
                    if entry.digest in contents:
                        if entry.size != contents[entry.digest]['bytes']:
                            raise ValidationError('Inconsistent SHA256 length')
                        copy_verified(entry)
                    else:
                        member = 'blobs/'+entry.digest[:2]+'/'+entry.digest
                        with archive.open(zip_info(member), 'w', force_zip64=True) as stream:
                            copy_verified(entry, stream)
                        contents[entry.digest] = dict(sha256=entry.digest, bytes=entry.size, member=member)
                project_rows.append(dict(label=label, project_id=manifest['project_id'],
                    manifest_sha256=manifest_entry.digest, package_content_id=manifest['content_id'],
                    files=len(entries), materialized_bytes=manifest['total_bytes']))
                print('Bundled '+label+': '+str(len(entries))+' paths; '+str(len(contents))+' unique payloads', file=sys.stderr, flush=True)
            index = dict(schema=SCHEMA, schema_version=VERSION, projects=project_rows,
                contents=sorted(contents.values(), key=lambda row:row['sha256']),
                bootstrap_files=[dict(member=e.name, bytes=e.size, sha256=e.digest) for e in bootstrap],
                materialization='Independent ordinary files; no symlinks or hardlinks',
                native_windows_acceptance='pending')
            index['content_id'] = sha256_bytes(canonical_json(index))
            archive.writestr(zip_info('bundle-index.json'), canonical_json(index))
        with temporary.open('rb') as stream:
            os.fsync(stream.fileno())
        # Publication is no-replace and does not use hardlinks. The existing
        # helper also safely publishes ordinary files with renameat2/MoveFile.
        _publish_directory(temporary, output)
        return dict(ok=True, output=str(output), content_id=index['content_id'], zip_bytes=output.stat().st_size,
            unique_payloads=len(contents), unique_payload_bytes=sum(r['bytes'] for r in contents.values()),
            projects=project_rows, native_windows_acceptance='pending')
    finally:
        temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', action='append', required=True, metavar='LABEL=PATH')
    parser.add_argument('--engine-root', type=Path, required=True)
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--active-imports-only', action='store_true', help='Opt in to the existing dependency-closure managed-revision filter')
    parser.add_argument('--docs-root', type=Path, help='Explicit refreshed handoff guides; defaults stay unchanged')
    parser.add_argument('--bootstrap', action='append', default=[], metavar='NAME=PATH', help='Additional reviewed root launcher/guide file')
    parser.add_argument('--mode', choices=('development','production'), default='development')
    args = parser.parse_args(argv)
    try:
        projects = []
        labels = set()
        for specification in args.project:
            label, separator, name = specification.partition('=')
            safe_label(label)
            if not separator or not name or label.casefold() in labels:
                raise ValidationError('Use unique --project LABEL=PATH arguments')
            labels.add(label.casefold())
            options = argparse.Namespace(project=Path(name), engine_root=args.engine_root,
                runtime=args.runtime, executable=None, previous_database=None, project_id=None,
                active_imports_only=args.active_imports_only)
            print('Validating '+label+' without staging project assets', file=sys.stderr, flush=True)
            projects.append((label, windows.validate(options)))
        extras = []
        for specification in args.bootstrap:
            name, separator, path = specification.partition('=')
            if not separator or not name or not path:
                raise ValidationError('Use --bootstrap NAME=PATH')
            extras.append((name,Path(path)))
        print(json.dumps(build_bundle(projects, args.output, mode=args.mode,
            extra_bootstrap=extras,docs_root=args.docs_root), sort_keys=True));return 0
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        print(json.dumps(dict(ok=False, error=str(error)), sort_keys=True));return 1


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Build a small, separate playtest overlay for a verified extracted orientation bundle.

Reads only bounded metadata from the base ZIP. Unchanged blobs are referenced,
not extracted or copied. New seeds materialize independent ordinary files.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import uuid
import zipfile

sys.dont_write_bytecode = True
from build_windows_artist_bundle import generated, source_entry, copy_verified, zip_info
from build_windows_package import pe_imports
from build_world_package import _native_hash, _publish_directory
from install_windows_artist_bundle import (SCHEMA as BASE_SCHEMA, VERSION, MAX_METADATA,
    canonical, checked_record, safe_name, safe_names, safe_label, regular)
from install_windows_playtest_bundle import SCHEMA
from verify_windows_artist_bundle import checked_json, require, verify_database, verify_native_dependencies


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def install_batch(label):
    return ('@echo off\r\nsetlocal\r\nset "PYTHONDONTWRITEBYTECODE=1"\r\n'
        '"%~dp0bin\\python\\python.exe" -B "%~dp0Install_Playtest_Bundle.py" --bundle-root "%~dp0." --project '+label+' %*\r\n'
        'set "result=%errorlevel%"\r\nif not "%result%"=="0" echo Playtest installation failed. Existing folders were not replaced.\r\n'
        'pause\r\nexit /b %result%\r\n').encode()


class BaseMetadata:
    def __init__(self, archive, verification):
        self.archive = archive; self.memo = {}
        safe_names(info.filename for info in archive.infolist())
        self.raw = self.read('bundle-index.json')
        self.index = checked_json(self.raw, 'original bundle index')
        require(self.index.get('schema') == BASE_SCHEMA and self.index.get('schema_version') == VERSION,
                'Unsupported original orientation bundle')
        body = {key:value for key,value in self.index.items() if key != 'content_id'}
        require(digest(canonical(body)) == self.index.get('content_id'), 'Original bundle content ID mismatch')
        require(verification.get('ok') is True and verification.get('verified') is True
                and verification.get('bundle_content_id') == self.index['content_id'], 'Base verification report/index mismatch')
        self.contents = {row['sha256']:checked_record(row) for row in self.index['contents']}
        require(len(self.contents) == len(self.index['contents']), 'Duplicate base content identity')
        expected = {row['member'] for row in self.index['contents']+self.index['bootstrap_files']} | {'bundle-index.json'}
        require(set(archive.namelist()) == expected, 'Unexpected/missing original ZIP members')

    def read(self, member, row=None):
        safe_name(member)
        info = self.archive.getinfo(member)
        require(not info.is_dir() and not info.flag_bits & 1 and info.file_size <= MAX_METADATA,
                'Invalid/oversized base metadata: '+member)
        if row: require(info.file_size == row['bytes'], 'Base metadata length mismatch')
        raw = self.archive.read(member)
        if row: require(digest(raw) == row['sha256'], 'Base metadata SHA mismatch')
        return raw

    def object(self, sha):
        if sha not in self.memo:
            row = self.contents[sha]
            self.memo[sha] = checked_json(self.read(row['member'],row), row['member'])
        return copy.deepcopy(self.memo[sha])


def revise_database(database, entries, patched, dependency_overrides, base_package_id):
    require(database.get('schema') == 'valwood.asset-database' and database.get('schema_version') == VERSION,
            'Invalid source project database')
    files = {row['path']:copy.deepcopy(row) for row in database['files']}
    assets = {row['id']:copy.deepcopy(row) for row in database['assets']}
    by_key = {row['logical_key']:row for row in assets.values()}
    namespace = uuid.UUID(database['project_id'])
    project_entries = {path[8:]:row for path,row in entries.items() if path.startswith('project/')}
    for path,entry in project_entries.items():
        if path not in files:
            require(path in dependency_overrides, 'New project file needs explicit dependency entry: '+path)
            managed = re.fullmatch(r'models/imports/([A-Za-z0-9_-]+)/[0-9a-f]{24}/asset(?:_near|_mid|_far)?\.glb',path)
            key = 'import:'+managed[1] if managed else 'path:'+path
            if key not in by_key:
                asset = dict(id=str(uuid.uuid5(namespace,key)),logical_key=key,artifacts=[],dependencies=[],provenance_records=[],
                    import_recipe=dict(pipeline='playtest-explicit-patch-v1',tool_version=VERSION,source_preserved=True,settings={}))
                by_key[key] = asset; assets[asset['id']] = asset
            files[path] = dict(path=path,asset_id=by_key[key]['id'],artifact_id=str(uuid.uuid5(namespace,'artifact:'+path)),
                              dependency_paths=[],dependencies=[])
        if path in patched:
            files[path].update(sha256=entry['sha256'],source_sha256=entry['sha256'],bytes=entry['bytes'],
                source_bytes=entry['bytes'],source_identity='project:'+path,metadata_sanitized=False,symlink_materialized=False)
    require(set(files) == set(project_entries), 'Original database/project closure differs from patched seed')
    for owner,refs in dependency_overrides.items():
        safe_name(owner); require(owner in files and isinstance(refs,list), 'Unknown project dependency owner: '+owner)
        require(len(refs) == len(set(refs)), 'Duplicate project dependency')
        files[owner]['dependency_paths'] = sorted(safe_name(ref) for ref in refs)
    for path,row in files.items():
        require(all(ref in files for ref in row['dependency_paths']), 'Missing project dependency: '+path)
        row['dependencies'] = sorted({files[ref]['asset_id'] for ref in row['dependency_paths']})
    for asset in assets.values():
        rows = [row for row in files.values() if row['asset_id'] == asset['id']]
        asset['artifacts'] = sorted(row['artifact_id'] for row in rows)
        asset['dependencies'] = sorted({dep for row in rows for dep in row['dependencies']} - {asset['id']})
        asset['version'] = digest(canonical(sorted(row['sha256'] for row in rows)))
    database.update(files=sorted(files.values(),key=lambda row:row['path']),
                    assets=sorted(assets.values(),key=lambda row:row['logical_key']))
    database['summary'].update(files=len(files),logical_assets=len(assets),
        source_bytes=sum(row.get('source_bytes',row['bytes']) for row in files.values()),
        packaged_bytes=sum(row['bytes'] for row in files.values()))
    database['playtest_overlay'] = dict(base_package_content_id=base_package_id,
        patched_project_files=sorted(patched),explicit_dependency_owners=sorted(dependency_overrides))
    return database


def build_overlay(base_path, patches, output, verification):
    base_path = regular(base_path); output = Path(output).absolute()
    output = output.parent.resolve(strict=True)/safe_name(output.name)
    sidecar = Path(str(output)+'.sha256')
    require(output.suffix.casefold() == '.zip' and not output.exists() and not output.is_symlink()
            and not sidecar.exists(), 'Output and SHA sidecar must be NEW .zip paths')
    require(patches.get('schema') == 'valwood.windows-playtest-patches' and patches.get('schema_version') == VERSION,
            'Invalid playtest patch map schema')
    projects = patches['projects']; labels = [safe_label(project['label']) for project in projects]
    require(0 < len(labels) <= 16 and len(labels) == len(set(label.casefold() for label in labels)), 'Missing/duplicate playtest label')
    require(base_path.stat().st_size == verification.get('zip_bytes'), 'Base archive size differs from verification report')
    require(re.fullmatch('[0-9a-f]{64}',verification.get('zip_sha256','')), 'Missing verified base ZIP SHA256')
    new_entries = {}; output_contents = {}; project_rows = []; details = []; pe_cache = {}; source_entries = []; reachable = set()
    def include(entry, base):
        known = base.contents.get(entry.digest)
        if known:
            require(known['bytes'] == entry.size, 'Inconsistent content identity length')
            output_contents[entry.digest] = known
        else:
            row = dict(sha256=entry.digest,bytes=entry.size,member='blobs/'+entry.digest[:2]+'/'+entry.digest)
            output_contents[entry.digest] = row
        new_entries.setdefault(entry.digest,entry)
        if entry.source is not None: source_entries.append(entry)
        return entry.record()
    with base_path.open('rb') as stream:
        before = os.fstat(stream.fileno())
        with zipfile.ZipFile(stream) as archive:
            base = BaseMetadata(archive,verification)
            require(not patches.get('base_content_id') or patches['base_content_id'] == base.index['content_id'], 'Patch map base identity mismatch')
            base_projects = {row['label']:row for row in base.index['projects']}
            for specification in projects:
                label = specification['label']; base_label = safe_label(specification['base_label'])
                require(base_label in base_projects,'Unknown original project label: '+base_label)
                original_project = base_projects[base_label]
                manifest = base.object(original_project['manifest_sha256'])
                require(manifest.get('schema') == 'valwood.windows-package' and manifest.get('schema_version') == VERSION,
                        'Invalid original Windows package')
                require(digest(canonical(manifest['files'])) == manifest.get('content_id') == original_project['package_content_id'],
                        'Original package content ID mismatch')
                safe_names([row['path'] for row in manifest['files']]+['package-manifest.json'])
                entries = {row['path']:copy.deepcopy(checked_record(row)) for row in manifest['files']}
                for row in entries.values():
                    require(base.contents[row['sha256']]['bytes'] == row['bytes'], 'Original package byte closure mismatch')
                database = base.object(entries['asset-database.json']['sha256'])
                verify_database(database,manifest,entries)
                native = base.object(entries['native-dependencies.json']['sha256'])
                native_rows = {row['path']:row for row in native['binaries']}
                patch_map = specification['patches']; require(isinstance(patch_map,dict) and patch_map,'Empty playtest patch map')
                safe_names(patch_map)
                patched_project = set(); patch_hashes = {}
                for name,source in patch_map.items():
                    safe_name(name)
                    require(name not in ('package-manifest.json','asset-database.json','build-recipe.json','native-dependencies.json')
                            and not name.startswith('bin/python/'), 'Reserved package patch path: '+name)
                    source = regular(source); require(source.is_file(),'Patch is not an ordinary file: '+name)
                    entry = source_entry(name,source)
                    record = include(entry,base)
                    if name in entries: record['mode'] = entries[name]['mode']
                    entries[name] = record; patch_hashes[name] = entry.digest
                    if name.startswith('project/'): patched_project.add(name[8:])
                    if name.startswith('bin/') and Path(name).suffix.casefold() in ('.exe','.dll','.pyd'):
                        if entry.digest not in pe_cache:
                            try: pe_cache[entry.digest] = pe_imports(source)
                            except struct.error as error: raise ValueError('Truncated patched Windows binary: '+name) from error
                        native_rows[name] = dict(path=name,imports=pe_cache[entry.digest])
                require({'bin/rpg.exe','Launch_Playtest.bat','PLAYTEST_GUIDE.md','project/playtest.route'} <= entries.keys(),
                        'Playtest seed requires runtime, guide, launch batch and project route')
                database = revise_database(database,entries,patched_project,
                    specification.get('project_dependencies',{}),manifest['content_id'])
                entries['asset-database.json'] = include(generated('asset-database.json',canonical(database)),base)
                native['binaries'] = sorted(native_rows.values(),key=lambda row:row['path'])
                verify_native_dependencies(native,entries)
                entries['native-dependencies.json'] = include(generated('native-dependencies.json',canonical(native)),base)
                recipe = base.object(entries['build-recipe.json']['sha256'])
                recipe.update(executable_sha256=entries['bin/rpg.exe']['sha256'],transport_builder='build_windows_playtest_overlay.py',
                    transport_builder_sha256=_native_hash(Path(__file__).resolve()),base_bundle_content_id=base.index['content_id'],
                    base_package_content_id=manifest['content_id'],explicit_patch_hashes=patch_hashes)
                entries['build-recipe.json'] = include(generated('build-recipe.json',canonical(recipe)),base)
                files = sorted(entries.values(),key=lambda row:row['path'])
                safe_names([row['path'] for row in files]+['package-manifest.json'])
                package = dict(schema='valwood.windows-package',schema_version=VERSION,project_id=manifest['project_id'],
                    scope=manifest.get('scope','explicit-playtest-overlay'),files=files,
                    content_id=digest(canonical(files)),total_bytes=sum(row['bytes'] for row in files),
                    playtest=dict(base_package_content_id=manifest['content_id'],patch_paths=sorted(patch_map)))
                check = verify_database(database,package,entries)
                package_entry = generated('package-manifest.json',canonical(package)); include(package_entry,base)
                reachable.update(row['sha256'] for row in files); reachable.add(package_entry.digest)
                for row in files:
                    if row['sha256'] not in output_contents: output_contents[row['sha256']] = base.contents[row['sha256']]
                project_rows.append(dict(label=label,base_label=base_label,project_id=package['project_id'],
                    manifest_sha256=package_entry.digest,package_content_id=package['content_id'],
                    files=len(files),materialized_bytes=package['total_bytes']))
                details.append(dict(label=label,patch_paths=sorted(patch_map),project_database=check))
            output_contents = {sha:row for sha,row in output_contents.items() if sha in reachable}
            # Only newly named files are added beside the original bootstrap.
            bootstrap_entries = [source_entry('Install_Playtest_Bundle.py',Path(__file__).with_name('install_windows_playtest_bundle.py')),
                generated('READ_PLAYTEST_FIRST.txt',(
                    'Valwood playable area review add-on\r\n\r\n'
                    'Extract this ENTIRE add-on ZIP into the existing extracted orientation bundle folder.\r\n'
                    'Keep bundle-index.json, the original installer, bin/python and all blobs.\r\n'
                    'Run Install_Playtest_LABEL.bat to create a NEW independent playtest seed.\r\n'
                    'Then open PLAYTEST_GUIDE.md and run Launch_Playtest.bat in that new seed.\r\n'
                    'Existing artist work and orientation seeds are never replaced.\r\n'
                    'Each installed seed still needs its full materialized disk size.\r\n'
                    'Install ONE area at a time if disk space is limited.\r\n'
                    'Native Windows GPU/input/performance acceptance remains pending.\r\n\r\n'+
                    '\r\n'.join('Install_Playtest_'+row['label']+'.bat: '+str(row['materialized_bytes'])+
                                 ' bytes for the new seed' for row in project_rows)+'\r\n').encode())]
            bootstrap_entries += [generated('Install_Playtest_'+label+'.bat',install_batch(label)) for label in labels]
            bootstrap = [dict(member=entry.name,bytes=entry.size,sha256=entry.digest) for entry in bootstrap_entries]
            bootstrap += [copy.deepcopy(row) for row in base.index['bootstrap_files'] if row['member'].startswith('bin/python/')]
            safe_names([row['member'] for row in bootstrap]+['playtest-index.json'])
            old_names = set(archive.namelist())
            require(not any(entry.name in old_names for entry in bootstrap_entries)
                    and 'playtest-index.json' not in old_names,'Overlay filenames collide with original bundle')
            index = dict(schema=SCHEMA,schema_version=VERSION,projects=project_rows,
                contents=sorted(output_contents.values(),key=lambda row:row['sha256']),bootstrap_files=bootstrap,
                base_bundle=dict(content_id=base.index['content_id'],index_sha256=digest(base.raw),
                    zip_sha256=verification['zip_sha256'],zip_bytes=verification['zip_bytes']),
                materialization='NEW independent ordinary-file playtest seeds; original orientation files preserved',
                native_windows_acceptance='pending')
            index['content_id'] = digest(canonical(index))
            added = [entry for sha,entry in new_entries.items() if sha not in base.contents and sha in output_contents]
            members = [entry.name for entry in bootstrap_entries]+[output_contents[entry.digest]['member'] for entry in added]+['playtest-index.json']
            safe_names(members)
            fd,temp_name = tempfile.mkstemp(prefix='.valwood-playtest-overlay-',suffix='.zip',dir=output.parent)
            os.close(fd); temporary = Path(temp_name)
            try:
                with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=3,allowZip64=True) as target:
                    for entry in bootstrap_entries:
                        with target.open(zip_info(entry.name),'w',force_zip64=True) as destination: copy_verified(entry,destination)
                    for entry in added:
                        with target.open(zip_info(output_contents[entry.digest]['member']),'w',force_zip64=True) as destination:
                            copy_verified(entry,destination)
                    target.writestr(zip_info('playtest-index.json'),canonical(index))
                # Check every explicit source even if its bytes were deduplicated
                # against another patch or an original bundle payload.
                for entry in source_entries: copy_verified(entry)
                require((before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns)
                        == (lambda s:(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns))(os.fstat(stream.fileno())),
                        'Original ZIP changed during metadata read')
                _publish_directory(temporary,output)
            finally:
                temporary.unlink(missing_ok=True)
    zip_sha = _native_hash(output)
    with sidecar.open('x',encoding='ascii') as target: target.write(zip_sha+'  '+output.name+'\n')
    return dict(ok=True,output=str(output),zip_bytes=output.stat().st_size,zip_sha256=zip_sha,
        content_id=index['content_id'],base_content_id=base.index['content_id'],new_payloads=len(added),
        new_payload_bytes=sum(entry.size for entry in added),reused_base_payloads=len(output_contents)-len(added),
        projects=project_rows,checks=details,base_payloads_extracted=0,native_windows_acceptance='pending')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base',type=Path,required=True)
    parser.add_argument('--base-verification',type=Path,required=True,help='Successful full base-ZIP verification report')
    parser.add_argument('--patch-map',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args(argv)
    try:
        result = build_overlay(args.base,checked_json(args.patch_map.read_bytes(),'patch map'),args.output,
                               checked_json(args.base_verification.read_bytes(),'base verification'))
        print(json.dumps(result,indent=2)); return 0
    except (OSError,ValueError,KeyError,TypeError,zipfile.BadZipFile) as error:
        print(json.dumps(dict(ok=False,error=str(error)),indent=2)); return 1


if __name__ == '__main__':
    raise SystemExit(main())

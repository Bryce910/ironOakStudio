#!/usr/bin/env python3
"""Verify a published artist ZIP by streaming every member once, without extraction."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import time
import zipfile
import zlib

sys.dont_write_bytecode = True
from install_windows_artist_bundle import (SCHEMA, VERSION, MAX_METADATA, canonical,
                                           checked_record, regular, safe_label, safe_name, safe_names)
from world_asset_database import open_regular_nofollow
from build_windows_package import SYSTEM_DLLS


def require(condition, message):
    if not condition:
        raise ValueError(message)


def stamp(info):
    return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns


def checked_json(raw, label):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate JSON key in '+label)
            result[key] = value
        return result
    value = json.loads(raw, object_pairs_hook=unique,
                       parse_constant=lambda value: (_ for _ in ()).throw(ValueError('Nonfinite JSON in '+label)))
    require(isinstance(value, dict), 'Invalid metadata object: '+label)
    return value


def verify_database(database, manifest, entries):
    require(database.get('schema') == 'valwood.asset-database' and database.get('schema_version') == VERSION,
            'Invalid project asset database')
    require(database.get('project_id') == manifest['project_id'], 'Database/project identity mismatch')
    files = database['files']; require(isinstance(files, list), 'Invalid database files')
    safe_names(row['path'] for row in files)
    by_path = {row['path']: checked_record(row) for row in files}
    require({'project/'+path for path in by_path} == {path for path in entries if path.startswith('project/')},
            'Project/database file closure mismatch')
    assets = database['assets']; require(isinstance(assets, list), 'Invalid database assets')
    asset_ids = {row['id'] for row in assets}
    require(len(asset_ids) == len(assets), 'Duplicate logical asset identity')
    artifact_ids = {row['artifact_id'] for row in files}
    require(len(artifact_ids) == len(files), 'Duplicate artifact identity')
    for path, row in by_path.items():
        entry = entries['project/'+path]
        require((entry['sha256'], entry['bytes']) == (row['sha256'], row['bytes']),
                'Project/database bytes mismatch: '+path)
        refs = row['dependency_paths']
        require(isinstance(refs, list) and len(refs) == len(set(refs)), 'Invalid dependency paths: '+path)
        require(all(safe_name(ref) in by_path for ref in refs), 'Missing project dependency: '+path)
        require(row['asset_id'] in asset_ids, 'Missing logical asset: '+path)
        require(set(row['dependencies']) == {by_path[ref]['asset_id'] for ref in refs},
                'Dependency asset identity mismatch: '+path)
    for asset in assets:
        require(set(asset['dependencies']).issubset(asset_ids), 'Missing logical asset dependency')
        require(set(asset['artifacts']) == {row['artifact_id'] for row in files if row['asset_id'] == asset['id']},
                'Logical asset/artifact closure mismatch')
    summary = database['summary']
    require(summary['files'] == len(files) and summary['logical_assets'] == len(assets)
            and summary['packaged_bytes'] == sum(row['bytes'] for row in files), 'Database byte/count total mismatch')
    return dict(files=len(files), dependencies=sum(len(row['dependency_paths']) for row in files),
                packaged_bytes=summary['packaged_bytes'])


def verify_native_dependencies(report, entries):
    require(report.get('format') == 'PE32+' and report.get('architecture') == 'AMD64', 'Invalid Windows dependency report')
    binaries = report['binaries']; safe_names(row['path'] for row in binaries)
    names = {path.casefold(): path for path in entries}
    for row in binaries:
        path = safe_name(row['path']); require(path in entries, 'Missing reported Windows binary: '+path)
        require(isinstance(row['imports'], list), 'Invalid binary imports')
        for dll in row['imports']:
            require(isinstance(dll, str) and re.fullmatch(r'[A-Za-z0-9_.+-]+', dll), 'Invalid DLL dependency')
            stem = dll.casefold().removesuffix('.dll')
            if stem in SYSTEM_DLLS or stem.startswith(('api-ms-win-', 'ext-ms-win-')):
                continue
            parents = {PurePosixPath(path).parent.as_posix(), 'bin', 'bin/python'}
            require(any((parent+'/'+dll).casefold() in names for parent in parents),
                    'Unbundled reported DLL '+dll+' required by '+path)


def verify_bundle(path, sha256_file=None, *, progress=False):
    started = time.monotonic(); path = regular(path)
    sidecar = regular(sha256_file or Path(str(path)+'.sha256'))
    require(sidecar.stat().st_size <= 4096, 'Oversized ZIP SHA256 sidecar')
    match = re.fullmatch(r'([0-9a-fA-F]{64})(?: [ *]([^\r\n]+))?\r?\n?', sidecar.read_text(encoding='ascii'))
    require(match is not None, 'Invalid ZIP SHA256 sidecar')
    require(not match[2] or Path(match[2]).name == path.name, 'ZIP sidecar filename mismatch')
    expected_zip_sha = match[1].lower()
    with open_regular_nofollow(path) as raw:
        before = os.fstat(raw.fileno())
        with zipfile.ZipFile(raw) as archive:
            infos = archive.infolist(); names = [info.filename for info in infos]
            safe_names(names)
            info_by_name = {info.filename: info for info in infos}
            require('bundle-index.json' in info_by_name, 'Missing bundle index')
            for info in infos:
                require(info.orig_filename == info.filename and '\0' not in info.orig_filename,
                        'NUL/normalized ZIP member name')
                require(not info.is_dir() and not (info.flag_bits & 1), 'Directory/encrypted ZIP member')
                require(info.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED), 'Unsupported ZIP compression')
                require(stat.S_IFMT(info.external_attr >> 16) in (0, stat.S_IFREG), 'Nonregular ZIP member')
            processed = set(); payload_bytes = 0

            def read_member(member, record=None, capture=False):
                nonlocal payload_bytes
                require(member not in processed, 'Payload requested twice: '+member)
                require(member in info_by_name, 'Missing ZIP member: '+member)
                info = info_by_name[member]
                if record is not None:
                    checked_record(record)
                    require(info.file_size == record['bytes'], 'ZIP member length mismatch: '+member)
                require(not capture or info.file_size <= MAX_METADATA, 'Oversized metadata: '+member)
                digest = hashlib.sha256(); crc = 0; total = 0; chunks = [] if capture else None
                with archive.open(info) as stream:
                    while block := stream.read(1024*1024):
                        total += len(block); require(total <= info.file_size, 'ZIP inflated length mismatch: '+member)
                        digest.update(block); crc = zlib.crc32(block, crc)
                        if capture: chunks.append(block)
                require(total == info.file_size and crc == info.CRC, 'ZIP length/CRC mismatch: '+member)
                if record is not None:
                    require(digest.hexdigest() == record['sha256'], 'Payload SHA256 mismatch: '+member)
                processed.add(member); payload_bytes += total
                if progress and len(processed) % 250 == 0:
                    print(f'Verified {len(processed)}/{len(infos)} members; {payload_bytes} bytes', file=sys.stderr, flush=True)
                return b''.join(chunks) if capture else None

            index = checked_json(read_member('bundle-index.json', capture=True), 'bundle index')
            require(index.get('schema') == SCHEMA and index.get('schema_version') == VERSION, 'Unsupported artist bundle')
            require(index.get('content_id') == hashlib.sha256(canonical({k:v for k,v in index.items() if k != 'content_id'})).hexdigest(),
                    'Bundle content ID mismatch')
            contents = {}; expected = {}
            def register(row):
                checked_record(row); member = safe_name(row['member'])
                previous = expected.setdefault(member, row)
                require((previous['sha256'], previous['bytes']) == (row['sha256'], row['bytes']),
                        'Conflicting ZIP member identity: '+member)
            for row in index['contents']:
                register(row)
                require(row['sha256'] not in contents, 'Duplicate content SHA256')
                contents[row['sha256']] = row
            safe_names(row['member'] for row in contents.values())
            bootstrap = index['bootstrap_files']; safe_names(row['member'] for row in bootstrap)
            for row in bootstrap: register(row)
            require('bundle-index.json' not in expected, 'Bundle index cannot be a content/bootstrap member')
            require(set(info_by_name) == set(expected) | {'bundle-index.json'}, 'Unexpected or missing ZIP members')
            projects = index['projects']; labels = [safe_label(row['label']) for row in projects]
            require(labels and len(labels) == len(set(label.casefold() for label in labels)), 'Missing/duplicate project label')
            bootstrap_names = {row['member'] for row in bootstrap}
            require({'Install_Artist_Bundle.py', 'READ_ME_FIRST.txt', 'bin/python/python.exe'} <= bootstrap_names,
                    'Missing installer/Python bootstrap')
            require(all('Install_'+label+'.bat' in bootstrap_names for label in labels), 'Missing project install batch')
            require(any(re.fullmatch(r'bin/python/python\d+\.zip', name) for name in bootstrap_names)
                    and any(re.fullmatch(r'bin/python/python\d+\.dll', name) for name in bootstrap_names)
                    and any(re.fullmatch(r'bin/python/python\d+\._pth', name) for name in bootstrap_names),
                    'Incomplete embedded Python runtime')
            manifests = []; reachable = set()
            for project in projects:
                digest = project['manifest_sha256']; row = contents[digest]
                if row['member'] in processed:
                    # Different labels may intentionally share an identical package.
                    manifest = next(manifest for _,manifest,_,_ in manifests if manifest['_digest'] == digest)
                else:
                    manifest = checked_json(read_member(row['member'], row, True), 'package manifest')
                    require(manifest.get('schema') == 'valwood.windows-package' and manifest.get('schema_version') == VERSION,
                            'Invalid Windows package manifest')
                    files = manifest['files']; require(isinstance(files, list) and len(files) <= 1000000, 'Invalid package file list')
                    require(manifest.get('content_id') == hashlib.sha256(canonical(files)).hexdigest(), 'Package content ID mismatch')
                    safe_names([entry['path'] for entry in files]+['package-manifest.json'])
                    for entry in files:
                        checked_record(entry)
                        require(entry.get('mode') in ('0644', '0755'), 'Invalid package file mode')
                        content = contents[entry['sha256']]
                        require(content['bytes'] == entry['bytes'], 'Package/content length mismatch')
                    manifest['_digest'] = digest
                files = manifest['files']; entries = {entry['path']:entry for entry in files}
                total = sum(entry['bytes'] for entry in files)
                require(type(manifest.get('total_bytes')) is int and manifest['total_bytes'] == total
                        and project['materialized_bytes'] == total and project['files'] == len(files), 'Project/package byte/count mismatch')
                require(project['package_content_id'] == manifest['content_id']
                        and project['project_id'] == manifest['project_id'], 'Project/package identity mismatch')
                require({'bin/rpg.exe','bin/valwood_project_validate.exe','bin/python/python.exe',
                         'Launch_Editor.bat','Create_Artist_Workspace.bat','Return_Changes.bat',
                         'asset-database.json','native-dependencies.json','project/project.valwood'} <= entries.keys(),
                        'Missing required Windows seed files')
                for entry in files:
                    if entry['path'].startswith('bin/python/'):
                        boot = expected.get(entry['path'])
                        require(boot and (boot['sha256'],boot['bytes']) == (entry['sha256'],entry['bytes']), 'Python bootstrap/package mismatch')
                reachable.update(entry['sha256'] for entry in files); reachable.add(digest)
                manifests.append((project, manifest, entries, {}))
            require(reachable == set(contents), 'Unreachable indexed content payload')
            metadata = {}
            for _, _, entries, _ in manifests:
                for name in ('asset-database.json','native-dependencies.json'):
                    row = contents[entries[name]['sha256']]
                    if row['sha256'] not in metadata:
                        metadata[row['sha256']] = checked_json(read_member(row['member'], row, True), name)
            reports = []
            for project, manifest, entries, _ in manifests:
                database = metadata[entries['asset-database.json']['sha256']]
                report = verify_database(database, manifest, entries)
                verify_native_dependencies(metadata[entries['native-dependencies.json']['sha256']], entries)
                reports.append(dict(label=project['label'], files=len(entries), materialized_bytes=manifest['total_bytes'],
                                    package_content_id=manifest['content_id'], project=report))
            for member, row in expected.items():
                if member not in processed: read_member(member, row)
            require(processed == set(info_by_name), 'Unverified ZIP member')
        raw.seek(0); digest = hashlib.file_digest(raw, 'sha256').hexdigest()
        require(digest == expected_zip_sha, 'ZIP SHA256 sidecar mismatch')
        require(stamp(before) == stamp(os.fstat(raw.fileno())), 'ZIP changed during verification')
    return dict(ok=True, verified=True, archive=str(path), zip_bytes=before.st_size, zip_sha256=digest,
                bundle_content_id=index['content_id'], members=len(processed), streamed_member_bytes=payload_bytes,
                unique_content_payloads=len(contents), projects=reports, extracted_files=0,
                each_member_streamed_once=True, elapsed_seconds=round(time.monotonic()-started,2),
                native_windows_acceptance='pending')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--sha256-file', type=Path)
    parser.add_argument('--output', type=Path, help='NEW JSON result path; existing files are never replaced')
    args = parser.parse_args(argv)
    try:
        result = verify_bundle(args.archive, args.sha256_file, progress=True)
        if args.output:
            with args.output.open('x', encoding='utf-8') as stream: json.dump(result, stream, indent=2); stream.write('\n')
        print(json.dumps(result, indent=2)); return 0
    except (OSError, ValueError, KeyError, TypeError, StopIteration, zipfile.BadZipFile, NotImplementedError) as error:
        print(json.dumps(dict(ok=False, error=str(error)), indent=2)); return 1


if __name__ == '__main__':
    raise SystemExit(main())

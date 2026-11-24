#!/usr/bin/env python3
"""Materialize one new artist seed from an extracted Valwood content bundle.

Standard library only. No game, Windows executable, symlink or hardlink is
launched/created. Existing package verification/publication helpers run only
after all selected helper bytes have passed their recorded SHA256 checks.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import sys
import tempfile

sys.dont_write_bytecode = True
SCHEMA = 'valwood.windows-artist-bundle'
VERSION = 1
MAX_METADATA = 256 * 1024 * 1024


def canonical(value):
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True, allow_nan=False)+'\n').encode()


def safe_name(name):
    if not isinstance(name, str) or not name or name.startswith('/'):
        raise ValueError('Invalid bundle filename')
    for part in name.split('/'):
        if (part in ('', '.', '..') or len(part) > 255 or
            re.search(r'[<>:"\\|?*\x00-\x1f\x7f]', part) or part.endswith(('.', ' ')) or
            re.fullmatch(r'(CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[1-9¹²³]|LPT[1-9¹²³])(?:\..*)?', part, re.I)):
            raise ValueError('Unsafe Windows filename: '+name)
    return name


def safe_label(label):
    if not isinstance(label, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,47}', label):
        raise ValueError('Project labels use 1–48 ASCII letters, digits, underscores or hyphens')
    safe_name(label)
    return label


def safe_names(names):
    seen = set()
    for name in names:
        key = safe_name(name).casefold()
        if key in seen:
            raise ValueError('Windows filename collision: '+name)
        seen.add(key)
    for key in seen:
        parts = key.split('/')
        if any('/'.join(parts[:i]) in seen for i in range(1, len(parts))):
            raise ValueError('File/directory collision: '+key)


def regular(path):
    """Reject symlinks/junctions in every existing input/output component."""
    path = Path(path).absolute()
    for part in [*reversed(path.parents), path]:
        info = part.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Bundle paths must not contain symlinks/junctions: '+str(part))
    return path


def checked_record(row):
    if not isinstance(row, dict):
        raise ValueError('Invalid content record')
    if not re.fullmatch('[0-9a-f]{64}', row.get('sha256', '')):
        raise ValueError('Invalid content SHA256')
    if type(row.get('bytes')) is not int or not 0 <= row['bytes'] <= 1024**4:
        raise ValueError('Invalid content length')
    return row


def read_checked(root, row, destination=None, limit=None, hash_only=False):
    checked_record(row)
    member = safe_name(row['member'])
    source = regular(root/member)
    if not source.is_file() or (limit is not None and row['bytes'] > limit):
        raise ValueError('Invalid/oversized bundle content: '+member)
    digest = hashlib.sha256()
    total = 0
    chunks = [] if destination is None and not hash_only else None
    with source.open('rb') as stream:
        before = os.fstat(stream.fileno())
        while block := stream.read(1024*1024):
            total += len(block)
            if total > row['bytes']:
                raise ValueError('Content length mismatch: '+member)
            digest.update(block)
            if chunks is not None:
                chunks.append(block)
            elif destination is not None:
                destination.write(block)
        after = os.fstat(stream.fileno())
    if (digest.hexdigest() != row['sha256'] or total != row['bytes'] or
        (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) !=
        (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)):
        raise ValueError('Content hash/length changed: '+member)
    return b''.join(chunks) if chunks is not None else None


def load_index(root):
    root = regular(root)
    path = regular(root/'bundle-index.json')
    if path.stat().st_size > MAX_METADATA:
        raise ValueError('Oversized bundle index')
    index = json.loads(path.read_bytes())
    if index.get('schema') != SCHEMA or index.get('schema_version') != VERSION:
        raise ValueError('Unsupported artist bundle')
    body = {k:v for k,v in index.items() if k != 'content_id'}
    if index.get('content_id') != hashlib.sha256(canonical(body)).hexdigest():
        raise ValueError('Bundle content ID mismatch')
    contents = {}
    for row in index['contents']:
        checked_record(row)
        if row['sha256'] in contents:
            raise ValueError('Duplicate content SHA256')
        safe_name(row['member'])
        contents[row['sha256']] = row
    safe_names(row['member'] for row in contents.values())
    labels = [safe_label(row['label']) for row in index['projects']]
    if len(labels) != len(set(label.casefold() for label in labels)):
        raise ValueError('Duplicate project label')
    if not labels:
        raise ValueError('Bundle contains no projects')
    safe_names(row['member'] for row in index['bootstrap_files'])
    for row in index['bootstrap_files']:
        # Hash bootstrap too, although installation never executes its EXEs.
        read_checked(root, row, limit=MAX_METADATA, hash_only=True)
    return root, index, contents


def install(bundle, label, output):
    root, index, contents = load_index(bundle)
    safe_label(label)
    matches = [p for p in index['projects'] if p['label'] == label]
    if len(matches) != 1:
        raise ValueError('Unknown project label: '+label)
    project = matches[0]
    manifest_row = contents[project['manifest_sha256']]
    manifest_bytes = read_checked(root, manifest_row, limit=MAX_METADATA)
    manifest = json.loads(manifest_bytes)
    if manifest.get('schema') != 'valwood.windows-package' or manifest.get('schema_version') != VERSION:
        raise ValueError('Invalid Windows package manifest')
    files = manifest['files']
    if len(files) > 1000000 or manifest.get('content_id') != hashlib.sha256(canonical(files)).hexdigest():
        raise ValueError('Package manifest content ID mismatch')
    if manifest['content_id'] != project['package_content_id']:
        raise ValueError('Project/package identity mismatch')
    safe_names([entry['path'] for entry in files]+['package-manifest.json'])
    for entry in files:
        checked_record(entry)
        if entry['sha256'] not in contents or contents[entry['sha256']]['bytes'] != entry['bytes']:
            raise ValueError('Missing/inconsistent file content: '+entry['path'])
        if entry.get('mode') not in ('0644', '0755'):
            raise ValueError('Invalid file mode')
    total = sum(entry['bytes'] for entry in files)
    if type(manifest.get('total_bytes')) is not int or manifest['total_bytes'] != total:
        raise ValueError('Package byte total mismatch')
    destination = Path(output).absolute()
    safe_name(destination.name)
    regular(destination.parent)
    if destination.exists() or destination.is_symlink():
        raise ValueError('Destination already exists; choose a NEW folder')
    if destination.is_relative_to(root):
        raise ValueError('Destination must be outside the extracted bundle')
    parents = {str(parent) for entry in files for parent in Path(entry['path']).parents if str(parent) != '.'}
    disk_required = total+len(manifest_bytes)+4096*(len(files)+len(parents))+64*1024*1024
    if shutil.disk_usage(destination.parent).free < disk_required:
        raise ValueError('Not enough free disk space for independent ordinary files: '+str(disk_required)+' bytes required including allocation margin')
    stage = Path(tempfile.mkdtemp(prefix='.valwood-artist-install-', dir=destination.parent))
    try:
        for entry in files:
            path = stage/entry['path']
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open('xb') as stream:
                read_checked(root, contents[entry['sha256']], stream)
                stream.flush()
                os.fsync(stream.fileno())
            os.chmod(path, int(entry['mode'], 8))
            os.utime(path, (0, 0))
        with (stage/'package-manifest.json').open('xb') as stream:
            stream.write(manifest_bytes)
            stream.flush()
            os.fsync(stream.fileno())
        # Reuse the existing exact package/file verifier and no-replace
        # publication. All imported helper bytes were hashed above; these
        # helpers, like this installer, use only Python's standard library.
        sys.path.insert(0, str(stage/'tools'))
        try:
            from build_world_package import verify_package, _publish_directory
            verified = verify_package(stage)
            _publish_directory(stage, destination)
        finally:
            sys.path.pop(0)
        return dict(ok=True, project=label, output=str(destination), files=len(files),
                    materialized_bytes=total, disk_preflight_bytes=disk_required, content_id=verified['content_id'],
                    independent_files=True, native_windows_acceptance='pending')
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle-root', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--project')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--list', action='store_true')
    args = parser.parse_args(argv)
    try:
        if args.list:
            _, index, _ = load_index(args.bundle_root)
            result = dict(ok=True, projects=[dict(label=p['label'], materialized_bytes=p['materialized_bytes'])
                                            for p in index['projects']])
        else:
            if not args.project:
                raise ValueError('--project is required (use --list to see labels)')
            output = args.output or args.bundle_root.absolute().parent/('Valwood_'+safe_label(args.project)+'_Seed')
            result = install(args.bundle_root, args.project, output)
        print(json.dumps(result, indent=2));return 0
    except (OSError, ValueError, KeyError, TypeError, ImportError) as error:
        print('ERROR:', error);return 1


if __name__ == '__main__':
    raise SystemExit(main())

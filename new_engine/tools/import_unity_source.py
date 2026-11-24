#!/usr/bin/env python3
"""Inventory a purchased .unitypackage and extract selected source data.

Preserves Unity GUIDs, source paths, and importer metadata. Unity scripts and
plugins are excluded; source geometry/material data is staged outside Unity.
"""
import argparse
from collections import Counter
from contextlib import contextmanager
import gzip
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile


DATA_SUFFIXES = {'.fbx', '.obj', '.png', '.jpg', '.jpeg', '.tga', '.tif', '.tiff',
                 '.exr', '.hdr', '.mat', '.prefab', '.shader', '.shadergraph',
                 '.shadersubgraph', '.cginc', '.hlsl', '.txt', '.pdf'}


@contextmanager
def open_package(package):
    # Unity appends Asset Store metadata in gzip's optional extra header.
    # GzipFile handles that header; tarfile's r|gz mini-reader does not.
    with gzip.open(package, 'rb') as decoded:
        with tarfile.open(fileobj=decoded, mode='r|') as archive:
            yield archive
        # Validate the gzip CRC/trailer even if tar ended before the padding.
        while decoded.read(4 * 1024**2):
            pass


def source_path(value):
    path = PurePosixPath(value)
    if path.is_absolute() or '\\' in value or '..' in path.parts or not path.parts or path.parts[0] != 'Assets':
        raise ValueError(f'Unsafe Unity source path: {value!r}')
    return path


def inventory(package):
    by_guid = {}
    with open_package(package) as archive:
        for index, member in enumerate(archive):
            if index > 100000:
                raise ValueError('Package exceeds the entry limit')
            parts = PurePosixPath(member.name).parts
            if len(parts) != 2 or not re.fullmatch(r'[0-9a-fA-F]{32}', parts[0]):
                continue
            if not member.isfile():
                continue
            item = by_guid.setdefault(parts[0], {'guid': parts[0]})
            if parts[1] == 'pathname':
                if member.size > 16384:
                    raise ValueError('Oversized Unity pathname')
                raw = archive.extractfile(member).read().decode('utf-8-sig').rstrip('\x00\r\n')
                # Recent Unity packages append a metadata line after the pathname.
                lines = raw.splitlines()
                item['path'] = str(source_path(lines[0] if lines else ''))
                if len(lines) > 1:
                    item['pathname_metadata'] = lines[1:]
            elif parts[1] in ('asset', 'asset.meta'):
                item[parts[1]] = {'member': member.name, 'bytes': member.size}
    entries = sorted((v for v in by_guid.values() if 'path' in v and 'asset' in v), key=lambda v: v['path'])
    paths = [v['path'] for v in entries]
    if len(set(paths)) != len(paths):
        raise ValueError('Duplicate source path in Unity package')
    total = sum(v['asset']['bytes'] for v in entries)
    if total > 32 * 1024**3:
        raise ValueError('Package source data exceeds 32 GiB')
    suffixes = Counter(PurePosixPath(v['path']).suffix.lower() for v in entries)
    digest = hashlib.sha256()
    with package.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024**2), b''):
            digest.update(block)
    return {'package': str(package.resolve()), 'package_bytes': package.stat().st_size,
            'sha256': digest.hexdigest(), 'asset_count': len(entries), 'source_bytes': total,
            'extensions': dict(sorted(suffixes.items())), 'entries': entries}


def extract(package, report, destination, prefixes):
    destination = destination.resolve()
    selected = {}
    for item in report['entries']:
        path = source_path(item['path'])
        if path.suffix.lower() not in DATA_SUFFIXES or not any(item['path'].startswith(p) for p in prefixes):
            continue
        for field, suffix in [('asset', ''), ('asset.meta', '.meta')]:
            if field in item:
                target = destination / (str(path) + suffix)
                if not target.resolve().is_relative_to(destination):
                    raise ValueError('Source destination escapes the staging directory')
                selected[item[field]['member']] = (target, item[field]['bytes'])
    extracted = []
    with open_package(package) as archive:
        for member in archive:
            choice = selected.get(member.name)
            if choice is None:
                continue
            target, size = choice
            if not member.isfile() or member.size != size:
                raise ValueError('Source archive member changed')
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                raise FileExistsError(f'Refusing to replace an existing source file: {target}')
            temporary = target.with_name(target.name + '.extracting')
            try:
                with archive.extractfile(member) as source, temporary.open('xb') as output:
                    shutil.copyfileobj(source, output, 1024**2)
                if temporary.stat().st_size != size:
                    raise ValueError('Truncated source member')
                temporary.replace(target)
            finally:
                temporary.unlink(missing_ok=True)
            extracted.append(str(target.relative_to(destination)))
    if len(extracted) != len(selected):
        raise ValueError('Some selected source files were missing from the archive')
    return extracted


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--extract-to', type=Path)
    parser.add_argument('--prefix', action='append', default=[])
    parser.add_argument('--reuse-inventory', action='store_true', help='Reuse an inventory of this unchanged archive')
    args = parser.parse_args()
    if args.extract_to and not args.prefix:
        parser.error('Extraction requires an explicit --prefix')
    if args.reuse_inventory:
        report = json.loads(args.inventory.read_text())
        if report['package'] != str(args.package.resolve()) or report['package_bytes'] != args.package.stat().st_size:
            raise ValueError('Inventory does not match the source archive')
        digest = hashlib.sha256()
        with args.package.open('rb') as stream:
            for block in iter(lambda: stream.read(4 * 1024**2), b''):
                digest.update(block)
        if digest.hexdigest() != report['sha256']:
            raise ValueError('Source archive changed since inventory')
    else:
        report = inventory(args.package)
    if args.extract_to:
        report['extracted'] = extract(args.package, report, args.extract_to, args.prefix)
    args.inventory.parent.mkdir(parents=True, exist_ok=True)
    args.inventory.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('entries', 'extracted')}, indent=2))
    if 'extracted' in report:
        print('Extracted source files:', len(report['extracted']))


if __name__ == '__main__':
    main()

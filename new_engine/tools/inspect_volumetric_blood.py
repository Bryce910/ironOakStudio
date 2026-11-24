#!/usr/bin/env python3
"""Inspect purchased Unity blood source offline; never import or execute it.

Extraction requires explicit Assets/... prefixes and a nonexistent bank. The
bank retains source paths, GUID metadata and hashes, including binary baked
animation data. Scripts and plugins remain inventory-only.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat

from import_unity_source import DATA_SUFFIXES, inventory, open_package


MIB = 1024 * 1024
MAX_MEMBERS = 100000
SOURCE_SUFFIXES = DATA_SUFFIXES | {'.asset', '.bytes', '.anim', '.mesh', '.dds',
    '.ktx', '.ktx2', '.json', '.glsl', '.compute'}
SHADER_SUFFIXES = {'.shader', '.shadergraph', '.shadersubgraph', '.cginc',
    '.hlsl', '.glsl', '.compute'}
SCRIPT_SUFFIXES = {'.cs', '.js', '.boo', '.py', '.sh', '.bat', '.cmd', '.ps1',
    '.dll', '.so', '.dylib', '.exe', '.bundle', '.asmdef', '.asmref'}


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(MIB), b''):
            digest.update(block)
    return digest.hexdigest()


def strict_source_path(value):
    parts = value.split('/')
    if (not value or parts[0] != 'Assets' or len(parts) < 2 or
            any(p in ('', '.', '..') or len(p) > 255 for p in parts) or
            any(ord(c) < 32 or c in '\\:' for c in value)):
        raise ValueError(f'Unsafe Unity source path: {value!r}')
    return PurePosixPath(value)


def category(path):
    value = PurePosixPath(path)
    if value.suffix.lower() in SCRIPT_SUFFIXES or any(p.lower() == 'plugins' for p in value.parts):
        return 'excluded-script-or-plugin'
    if value.suffix.lower() in SHADER_SUFFIXES:
        return 'shader-source-text'
    return 'source-data' if value.suffix.lower() in SOURCE_SUFFIXES else 'inventory-only'


def inspect(package, max_file_bytes=256*MIB, max_total_bytes=2048*MIB):
    package = Path(package).absolute()
    if package.is_symlink() or not package.is_file():
        raise ValueError('Package must be a regular file, not a symlink')
    if max_file_bytes <= 0 or max_total_bytes <= 0:
        raise ValueError('Source limits must be positive')
    initial_hash = sha256_file(package)
    members, sources, paths, total = {}, {}, set(), 0
    with open_package(package) as archive:
        for index, member in enumerate(archive):
            if index >= MAX_MEMBERS:
                raise ValueError('Package exceeds the member limit')
            name = member.name
            parts = name.split('/')
            store_icon = name == '.icon.png'
            if not store_icon and (len(parts) not in (1, 2) or not re.fullmatch('[0-9a-fA-F]{32}', parts[0]) or
                    (len(parts) == 2 and (parts[1] in ('', '.', '..') or '\\' in parts[1]))):
                raise ValueError(f'Unsafe archive member: {name!r}')
            key = name.lower()
            if key in members:
                raise ValueError(f'Duplicate archive member: {name!r}')
            if member.isdir():
                if len(parts) != 1 or member.size:
                    raise ValueError(f'Invalid archive directory: {name!r}')
                members[key] = {'directory': True}
                continue
            if not member.isfile() or (not store_icon and len(parts) != 2) or member.linkname:
                raise ValueError(f'Nonregular archive member: {name!r}')
            if member.size < 0 or member.size > max_file_bytes:
                raise ValueError(f'Archive member exceeds the per-file limit: {name!r}')
            total += member.size
            if total > max_total_bytes:
                raise ValueError('Package exceeds the total unpacked source limit')
            field = parts[1] if len(parts) == 2 else ''
            if field == 'pathname' and member.size > 16384:
                raise ValueError('Oversized Unity pathname')
            digest, count, pathname = hashlib.sha256(), 0, bytearray()
            with archive.extractfile(member) as stream:
                for block in iter(lambda: stream.read(MIB), b''):
                    count += len(block)
                    digest.update(block)
                    if field == 'pathname':
                        pathname.extend(block)
            if count != member.size:
                raise ValueError(f'Truncated archive member: {name!r}')
            members[key] = {'member': name, 'bytes': count, 'sha256': digest.hexdigest()}
            if field == 'pathname':
                raw = pathname.decode('utf-8-sig').rstrip('\x00\r\n')
                lines = raw.splitlines()
                path = str(strict_source_path(lines[0] if lines else ''))
                if path.casefold() in paths:
                    raise ValueError(f'Duplicate source path: {path!r}')
                paths.add(path.casefold())
                sources[parts[0].lower()] = path
    # Reuse the generic format parser only after stricter bounded validation.
    report = inventory(package)
    if report['sha256'] != initial_hash:
        raise ValueError('Package changed during inspection')
    for item in report['entries']:
        if sources.get(item['guid'].lower()) != item['path']:
            raise ValueError('GUID/path inventory mismatch')
        item['category'] = category(item['path'])
        for field in ('asset', 'asset.meta'):
            if field in item:
                verified = members[item[field]['member'].lower()]
                item[field]['sha256'] = verified['sha256']
        item['source_sha256'] = item['asset']['sha256']
    report['unpacked_bytes_including_metadata'] = total
    report['limits'] = {'max_file_bytes': max_file_bytes, 'max_total_bytes': max_total_bytes,
                        'max_members': MAX_MEMBERS}
    report['categories'] = dict(Counter(item['category'] for item in report['entries']))
    report['execution'] = 'none; archive data and shader text only'
    return report


def no_symlink_ancestors(path):
    for part in (path, *path.parents):
        if part.is_symlink():
            raise ValueError(f'Symlink destination component: {part}')
        if part.exists() and not part.is_dir():
            raise ValueError(f'Nondirectory destination component: {part}')


def extract_sources(package, report, destination, prefixes):
    package, destination = Path(package).absolute(), Path(destination).absolute()
    if destination.exists() or destination.is_symlink():
        raise FileExistsError(f'Refusing existing bank: {destination}')
    no_symlink_ancestors(destination.parent)
    if not prefixes:
        raise ValueError('Extraction requires explicit --prefix Assets/... selections')
    prefixes = [str(strict_source_path(value.rstrip('/'))) for value in prefixes]
    if sha256_file(package) != report['sha256'] or str(package.resolve()) != report['package']:
        raise ValueError('Package does not match inspected source')
    selected, target_paths = {}, set()
    for item in report['entries']:
        path = item['path']
        if item['category'] not in ('source-data', 'shader-source-text') or not any(
                path == prefix or path.startswith(prefix + '/') for prefix in prefixes):
            continue
        for field, suffix in (('asset', ''), ('asset.meta', '.meta')):
            if field not in item:
                continue
            target = path + suffix
            if target.casefold() in target_paths:
                raise ValueError(f'Colliding extraction target: {target}')
            target_paths.add(target.casefold())
            selected[item[field]['member']] = dict(item[field], path=target, guid=item['guid'],
                                                   category=item['category'])
    if not selected:
        raise ValueError('No eligible data matched the explicit prefixes')
    destination.parent.mkdir(parents=True, exist_ok=True)
    no_symlink_ancestors(destination.parent)
    destination.mkdir(mode=0o700)
    extracted, seen = [], set()
    try:
        with open_package(package) as archive:
            for member in archive:
                choice = selected.get(member.name)
                if choice is None:
                    continue
                if member.name in seen or not member.isfile() or member.linkname or member.size != choice['bytes']:
                    raise ValueError('Selected archive member changed')
                seen.add(member.name)
                target = destination / choice['path']
                target.parent.mkdir(parents=True, exist_ok=True)
                no_symlink_ancestors(target.parent)
                flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, 'O_NOFOLLOW', 0)
                digest, size = hashlib.sha256(), 0
                with os.fdopen(os.open(target, flags, 0o600), 'wb') as output:
                    if not stat.S_ISREG(os.fstat(output.fileno()).st_mode):
                        raise ValueError('Extraction target is not a regular file')
                    with archive.extractfile(member) as source:
                        for block in iter(lambda: source.read(MIB), b''):
                            size += len(block)
                            if size > choice['bytes']:
                                raise ValueError('Selected source size changed')
                            digest.update(block)
                            output.write(block)
                if size != choice['bytes'] or digest.hexdigest() != choice['sha256']:
                    raise ValueError('Selected source hash changed')
                extracted.append({k: choice[k] for k in ('path', 'guid', 'bytes', 'sha256', 'category')})
        if len(seen) != len(selected) or sha256_file(package) != report['sha256']:
            raise ValueError('Package changed or selected source is missing')
        manifest = {'package': report['package'], 'package_sha256': report['sha256'],
                    'prefixes': prefixes, 'extracted': sorted(extracted, key=lambda item: item['path']),
                    'extracted_bytes': sum(item['bytes'] for item in extracted), 'executed_files': 0}
        with (destination/'extraction-manifest.json').open('x') as stream:
            json.dump(manifest, stream, indent=2)
            stream.write('\n')
        return manifest
    except BaseException:
        shutil.rmtree(destination)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--extract-to', type=Path)
    parser.add_argument('--prefix', action='append', default=[])
    parser.add_argument('--max-file-mib', type=int, default=256)
    parser.add_argument('--max-total-mib', type=int, default=2048)
    args = parser.parse_args()
    if args.extract_to and not args.prefix:
        parser.error('Extraction requires explicit --prefix Assets/... selections')
    if args.report.exists() or args.report.is_symlink():
        raise FileExistsError(f'Refusing existing report: {args.report}')
    report = inspect(args.package, args.max_file_mib*MIB, args.max_total_mib*MIB)
    if args.extract_to:
        report['extraction'] = extract_sources(args.package, report, args.extract_to, args.prefix)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open('x') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps({key: value for key, value in report.items() if key not in ('entries', 'extraction')}, indent=2))


if __name__ == '__main__':
    main()

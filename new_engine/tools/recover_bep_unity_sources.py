#!/usr/bin/env python3
"""Selectively inventory purchased BEP Unity packages without importing scripts.

Only small Unity metadata and serialized YAML are retained. Models, images,
audio, previews and binary TerrainData remain in the original archives unless
explicitly requested by GUID. No Unity editor or source code is executed.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import re
import tarfile
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.local-tools/bep-source'
GUID = re.compile(r'^[0-9a-f]{32}$')


def write_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + '.tmp')
    temp.write_text(json.dumps(obj, indent=2) + '\n')
    temp.replace(path)


def scan(package, output):
    started = time.monotonic()
    records = {}
    kept = 0
    last = started
    with tarfile.open(package, 'r|gz') as archive:
        for member in archive:
            parts = PurePosixPath(member.name).parts
            if len(parts) != 2 or not GUID.fullmatch(parts[0]) or not member.isfile():
                continue
            guid, kind = parts
            row = records.setdefault(guid, {'guid': guid})
            if kind == 'pathname':
                path = archive.extractfile(member).read(4096).decode('utf-8').rstrip('\0\r\n')
                pure = PurePosixPath(path)
                if pure.is_absolute() or '..' in pure.parts:
                    raise ValueError('Unsafe source pathname: ' + path)
                row['path'] = path
            elif kind == 'asset.meta':
                if member.size > 2 * 1024 * 1024:
                    raise ValueError('Unexpected metadata size')
                data = archive.extractfile(member).read()
                dest = output / 'guid' / guid / 'asset.meta'
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(data)
                row['meta'] = str(dest.relative_to(OUT))
                row['meta_sha256'] = hashlib.sha256(data).hexdigest()
                kept += len(data)
            elif kind == 'asset':
                row.update(bytes=member.size, tar_offset=member.offset_data)
                f = archive.extractfile(member)
                head = f.read(min(64, member.size))
                row['header_hex'] = head[:16].hex()
                if head.startswith(b'%YAML'):
                    if member.size > 256 * 1024 * 1024:
                        raise ValueError('Oversize serialized YAML')
                    data = head + f.read()
                    dest = output / 'guid' / guid / 'asset'
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(data)
                    row['yaml'] = str(dest.relative_to(OUT))
                    row['sha256'] = hashlib.sha256(data).hexdigest()
                    kept += len(data)
                    if kept > 768 * 1024 * 1024:
                        raise ValueError('Selective source budget exceeded')
            if time.monotonic() - last > 20:
                print(f'{package.name}: {len(records)} GUIDs, {kept / 1048576:.1f} MiB preserved', flush=True)
                last = time.monotonic()
    rows = sorted(records.values(), key=lambda r: r.get('path', r['guid']))
    counts = {}
    for row in rows:
        extension = PurePosixPath(row.get('path', '')).suffix.lower() or '<folder>'
        counts[extension] = counts.get(extension, 0) + 1
    report = dict(archive=str(package), archive_bytes=package.stat().st_size,
                  duration_seconds=time.monotonic() - started, preserved_bytes=kept,
                  counts=counts, assets=rows)
    write_json(output / 'inventory.json', report)
    print(f'READY {output / "inventory.json"}: {len(rows)} GUIDs, {kept / 1048576:.1f} MiB', flush=True)


def extract_guid(package, output, wanted):
    found = set()
    with tarfile.open(package, 'r|gz') as archive:
        for member in archive:
            parts = PurePosixPath(member.name).parts
            if len(parts) != 2 or parts[0] not in wanted or parts[1] != 'asset':
                continue
            if member.size > 128 * 1024 * 1024:
                raise ValueError('Explicit extraction exceeds per-asset 128 MiB bound')
            dest = output / 'guid' / parts[0] / 'asset'
            dest.parent.mkdir(parents=True, exist_ok=True)
            with dest.open('wb') as f:
                source = archive.extractfile(member)
                while block := source.read(1024 * 1024):
                    f.write(block)
            found.add(parts[0])
            if len(found) % 100 == 0 or found == wanted:
                print(f'EXTRACTED {len(found)}/{len(wanted)} serialized sources from {package.name}', flush=True)
            if found == wanted:
                break
    if found != wanted:
        raise ValueError('Missing GUIDs: ' + str(wanted - found))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('packages', nargs='+', type=Path)
    parser.add_argument('--guid', action='append', default=[])
    parser.add_argument('--recover-serialized', action='store_true',
                        help='Recover scenes, prefabs, materials, layers and small binary assets from the saved inventory')
    args = parser.parse_args()
    for package in args.packages:
        output = OUT / package.stem
        if args.recover_serialized:
            inventory = json.loads((output / 'inventory.json').read_text())
            extensions = {'.unity', '.prefab', '.mat', '.terrainlayer', '.asset'}
            wanted = {r['guid'] for r in inventory['assets']
                      if PurePosixPath(r.get('path', '')).suffix.lower() in extensions
                      and r.get('bytes', 0) <= 128 * 1024 * 1024}
            print(f'Recovering {len(wanted)} bounded serialized files from {package.name}', flush=True)
            extract_guid(package, output, wanted)
        elif args.guid:
            assert all(GUID.fullmatch(x) for x in args.guid)
            extract_guid(package, output, set(args.guid))
        else:
            scan(package, output)


if __name__ == '__main__':
    main()

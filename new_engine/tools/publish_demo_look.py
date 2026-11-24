#!/usr/bin/env python3
"""Publish a reviewed look with an exact backup and protected-row proof."""
import argparse
import hashlib
import json
from pathlib import Path
import os

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT/'projects/valwood_world'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def protected(data):
    return b''.join(line for line in data.splitlines(keepends=True)
                    if not line.startswith((b'look ', b'environment ')))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-world-sha256', required=True)
    args = parser.parse_args()
    target = PROJECT/'world.studio'
    original, proposed = target.read_bytes(), args.staged.read_bytes()
    if sha(original) != args.expected_world_sha256:
        parser.error('World changed since review; merge and review before publication.')
    if protected(original) != protected(proposed):
        parser.error('Only look/environment rows may change.')
    if sum(line.startswith(b'look ') for line in proposed.splitlines()) != 1:
        parser.error('Expected exactly one look row.')
    if sum(line.startswith(b'environment ') for line in proposed.splitlines()) != 1:
        parser.error('Expected exactly one HDR environment row.')
    args.output.mkdir(parents=True, exist_ok=False)
    backup = args.output/'world.before-lighting.studio'
    with backup.open('xb') as handle:
        handle.write(original)
    settings_before = sha((PROJECT/'graphics.settings').read_bytes())
    temporary = target.with_name('world.studio.look-publish.tmp')
    with temporary.open('xb') as handle:
        handle.write(proposed)
        handle.flush()
        os.fsync(handle.fileno())
    if target.read_bytes() != original:
        temporary.unlink()
        raise RuntimeError('Concurrent world edit; original preserved, publication cancelled.')
    os.replace(temporary, target)
    report = dict(published=True, original_world_sha256=sha(original),
                  published_world_sha256=sha(proposed),
                  protected_rows_byte_identical=protected(original) == protected(target.read_bytes()),
                  protected_rows_sha256=sha(protected(original)),
                  graphics_settings_unchanged=settings_before == sha((PROJECT/'graphics.settings').read_bytes()),
                  backup=str(backup.resolve()), staged=str(args.staged.resolve()),
                  changed_rows=[line.decode() for line in proposed.splitlines()
                                if line.startswith((b'look ', b'environment '))],
                  scope='Lighting and HDR only; no geometry, placement, material, decal, rain, wind, fog or sharpness changes.')
    (args.output/'publication.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

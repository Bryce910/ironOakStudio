#!/usr/bin/env python3
"""Private artist workspaces and conflict-checked returns for Valwood Linux Studio.

No command writes into the owner's project. A return is a file-level review
packet, not a scene/entity merge. Requires the packaged standard-library helpers.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import sys

# A delivered seed is hash-verified and must remain byte-for-byte pristine.
# Loading its sibling helpers must not add unmanifested __pycache__ files.
sys.dont_write_bytecode = True

from build_world_package import verify_package, _publish_directory
from world_asset_database import (ROOT_FILES, RUNTIME_DIRS, SUFFIXES,
    PRIVATE_PARTS, PRIVATE_NAMES, ValidationError, canonical_json, relative_name,
    open_regular_nofollow)

SCHEMA = "valwood.artist-workspace.v1"
RETURN_SCHEMA = "valwood.artist-return.v1"
EXCLUDED_ROOT = {"graphics.settings", "README.txt"}


def editor_binary(workspace):
    return workspace / 'bin' / ('rpg.exe' if (workspace/'bin/rpg.exe').is_file() else 'rpg')


def copy_runtime_file(source, destination):
    if os.name != 'nt':
        return shutil.copy2(source, destination)
    # Use the same checked ordinary-file reader as verification. Streaming
    # also avoids CopyFile2 requirements in compatibility environments.
    with open_regular_nofollow(Path(source).absolute()) as stream, open(destination, 'wb') as output:
        shutil.copyfileobj(stream, output, 1024 * 1024)
        output.flush()
        os.fsync(output.fileno())
    shutil.copystat(source, destination, follow_symlinks=False)
    return destination


def authoring_path(name):
    try:
        name = relative_name(name)
    except ValidationError:
        return False
    parts = Path(name).parts
    if any(p.lower() in PRIVATE_PARTS or p.lower() in PRIVATE_NAMES or p.startswith('.') for p in parts):
        return False
    if len(parts) == 1:
        return name in ROOT_FILES and name not in EXCLUDED_ROOT
    return parts[0] in RUNTIME_DIRS and Path(name).suffix.lower() in SUFFIXES


def fingerprint(path):
    with open_regular_nofollow(path) as stream:
        before = os.fstat(stream.fileno())
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
        after = os.fstat(stream.fileno())
    if (before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (after.st_size, after.st_mtime_ns, after.st_ctime_ns):
        raise ValidationError("File changed during read: " + path.name)
    return {"sha256": digest, "bytes": after.st_size}


def project_files(project):
    if project.is_symlink() or not project.is_dir():
        raise ValidationError("Expected an ordinary project directory")
    result = {}
    for path in sorted(project.rglob('*')):
        name = path.relative_to(project).as_posix()
        # Reject symlinks even in excluded folders; never follow outside assets.
        if path.is_symlink():
            raise ValidationError("Materialize linked assets before handing off: " + name)
        if path.is_file() and authoring_path(name):
            result[name] = fingerprint(path)
    return result


def new_destination(path, forbidden):
    path = path.absolute()
    parent = path.parent.resolve(strict=True)
    path = parent / path.name
    if path.exists() or path.is_symlink():
        raise ValidationError("Output must be a new directory")
    if any(path.is_relative_to(root) or root.is_relative_to(path) for root in forbidden):
        raise ValidationError("Output must be separate from the input workspace")
    return path


def publish_copy(source, destination, callback):
    stage = Path(tempfile.mkdtemp(prefix='.valwood-artist-', dir=destination.parent))
    try:
        shutil.copytree(source, stage, dirs_exist_ok=True, copy_function=copy_runtime_file)
        callback(stage)
        _publish_directory(stage, destination)
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def create_workspace(package, output):
    package = package.resolve(strict=True)
    identity = verify_package(package)
    output = new_destination(output, [package])
    manifest = json.loads((package / 'package-manifest.json').read_text())
    baseline = {r['path'][8:]: {k: r[k] for k in ('sha256', 'bytes')}
                for r in manifest['files'] if r['path'].startswith('project/') and authoring_path(r['path'][8:])}
    metadata = dict(schema=SCHEMA, package_content_id=identity['content_id'], files=baseline,
                    binary_sha256=fingerprint(editor_binary(package))['sha256'])
    def finish(stage):
        if project_files(stage / 'project') != baseline:
            raise ValidationError("Copied project differs from the verified seed")
        verify_package(stage)
        (stage / 'artist-baseline.json').write_bytes(canonical_json(metadata))
    publish_copy(package, output, finish)
    return dict(ok=True, workspace=str(output), baseline_files=len(baseline), seed_content_id=identity['content_id'])


def baseline_document(workspace):
    path = workspace / 'artist-baseline.json'
    if path.is_symlink():
        raise ValidationError("Workspace baseline must be an ordinary file")
    doc = json.loads(path.read_text())
    if doc.get('schema') != SCHEMA or not isinstance(doc.get('files'), dict):
        raise ValidationError("Create an artist workspace first")
    for name, value in doc['files'].items():
        if not authoring_path(name) or not isinstance(value, dict) or not valid_hash(value.get('sha256')):
            raise ValidationError("Invalid baseline record")
    if fingerprint(editor_binary(workspace))['sha256'] != doc['binary_sha256']:
        raise ValidationError("Editor binary changed; use one editor revision per assignment")
    return doc


def valid_hash(value):
    return isinstance(value, str) and len(value) == 64 and all(c in '0123456789abcdef' for c in value)


def export_return(workspace, output):
    workspace = workspace.resolve(strict=True)
    baseline = baseline_document(workspace)
    current = project_files(workspace / 'project')
    output = new_destination(output, [workspace])
    rows = []
    for name in sorted(baseline['files'].keys() | current.keys()):
        before, after = baseline['files'].get(name), current.get(name)
        if before != after:
            rows.append(dict(path=name, before=before, after=after))
    if not rows:
        raise ValidationError("No saved authoring changes; Apply and Save in the editor first")
    stage = Path(tempfile.mkdtemp(prefix='.valwood-return-', dir=output.parent))
    try:
        for row in rows:
            if row['after'] is None:
                continue
            target = stage / 'files' / row['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            copy_runtime_file(workspace / 'project' / row['path'], target)
            if fingerprint(target) != row['after']:
                raise ValidationError("File changed during export: " + row['path'])
        if project_files(workspace / 'project') != current:
            raise ValidationError("Project changed during export; close the editor and retry")
        doc = dict(schema=RETURN_SCHEMA, seed_content_id=baseline['package_content_id'],
                   binary_sha256=baseline['binary_sha256'], changes=rows,
                   scope="Saved allowlisted project files only. No automatic entity merge, player saves, preferences or unsaved recovery drafts.")
        (stage / 'changes.json').write_bytes(canonical_json(doc))
        _publish_directory(stage, output)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    return dict(ok=True, output=str(output), changed_files=len(rows), deletions=sum(r['after'] is None for r in rows))


def read_return(packet):
    doc = json.loads((packet / 'changes.json').read_text())
    if doc.get('schema') != RETURN_SCHEMA or not isinstance(doc.get('changes'), list) or not doc['changes']:
        raise ValidationError("Invalid or empty return packet")
    seen, expected = set(), {'changes.json'}
    for row in doc['changes']:
        name = row['path']
        if not authoring_path(name) or name in seen:
            raise ValidationError("Unsafe or duplicate return path")
        seen.add(name)
        for state in ('before', 'after'):
            value = row[state]
            if value is not None and (not isinstance(value, dict) or not valid_hash(value.get('sha256')) or type(value.get('bytes')) is not int or value['bytes'] < 0):
                raise ValidationError("Invalid return fingerprint")
        if row['before'] == row['after']:
            raise ValidationError("Redundant return record")
        if row['after'] is not None:
            expected.add('files/' + name)
    actual = set()
    for path in packet.rglob('*'):
        if path.is_symlink():
            raise ValidationError("Return packet contains a symlink")
        if path.is_file():
            actual.add(path.relative_to(packet).as_posix())
    if actual != expected:
        raise ValidationError("Unexpected or missing return files")
    for row in doc['changes']:
        if row['after'] is not None and fingerprint(packet / 'files' / row['path']) != row['after']:
            raise ValidationError("Return file integrity failed: " + row['path'])
    return doc


def review_return(packet, workspace, output=None):
    packet, workspace = packet.resolve(strict=True), workspace.resolve(strict=True)
    doc = read_return(packet)
    manifest = json.loads((workspace / 'package-manifest.json').read_text())
    if doc['seed_content_id'] != manifest.get('content_id'):
        raise ValidationError("Return belongs to a different seed package")
    if fingerprint(editor_binary(workspace))['sha256'] != doc['binary_sha256']:
        raise ValidationError("Return uses a different editor binary")
    current = project_files(workspace / 'project')
    conflicts = [row['path'] for row in doc['changes'] if current.get(row['path']) != row['before']]
    if conflicts:
        return dict(ok=False, conflicts=conflicts, staged=False, note="Owner changed the same files. Resolve in a separate copy; no automatic merge.")
    if output is not None:
        output = new_destination(output, [workspace, packet])
        def finish(stage):
            if project_files(stage / 'project') != current:
                raise ValidationError("Owner project changed during staging")
            for row in doc['changes']:
                target = stage / 'project' / row['path']
                if row['after'] is None:
                    target.unlink()
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    copy_runtime_file(packet / 'files' / row['path'], target)
                    if fingerprint(target) != row['after']:
                        raise ValidationError("Return changed during staging")
            (stage / 'ARTIST_RETURN_REVIEW.json').write_bytes(canonical_json(doc))
            # Rebase subsequent artist deltas on this reviewed candidate.
            (stage / 'artist-baseline.json').write_bytes(canonical_json(dict(
                schema=SCHEMA, package_content_id=doc['seed_content_id'],
                binary_sha256=doc['binary_sha256'], files=project_files(stage / 'project'))))
        publish_copy(workspace, output, finish)
    return dict(ok=True, conflicts=[], staged=output is not None,
                output=str(output) if output else None, changed_files=len(doc['changes']),
                note="File-level review only. Validate, launch and benchmark the staged copy before approval. Seed manifest is historical after editing.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    create = sub.add_parser('create', help='Verify seed and create a separate full working copy')
    create.add_argument('--package', type=Path, required=True)
    create.add_argument('--output', type=Path, required=True)
    export = sub.add_parser('export', help='Export saved project changes for owner review')
    export.add_argument('--workspace', type=Path, required=True)
    export.add_argument('--output', type=Path, required=True)
    review = sub.add_parser('review', help='Check conflicts; optionally stage a NEW complete review copy')
    review.add_argument('--packet', type=Path, required=True)
    review.add_argument('--workspace', type=Path, required=True)
    review.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'create':
            result = create_workspace(args.package, args.output)
        elif args.command == 'export':
            result = export_return(args.workspace, args.output)
        else:
            result = review_return(args.packet, args.workspace, args.output)
    except (OSError, ValueError, KeyError, TypeError) as error:
        result = dict(ok=False, error=str(error))
    print(json.dumps(result, indent=2))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

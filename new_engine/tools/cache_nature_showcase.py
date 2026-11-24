#!/usr/bin/env python3
"""Prepare measured full-resolution GPU texture blocks for the showcase.

Only accepts offline caches certified against decoded original pixels. The
native loader retains the original image whenever a cache is absent or fails.
"""
from pathlib import Path
import argparse, json, subprocess
from benchmark_graphics_settings import native_environment

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=ROOT / 'projects/valwood_showcase')
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--extra-catalog', type=Path)
    parser.add_argument('--linear-only', action='store_true')
    args = parser.parse_args()
    project = args.project.resolve()
    cache = project / 'texture_blocks'
    if cache.is_symlink():
        previous = cache.resolve()
        cache.unlink()
        cache.mkdir()
        for source in previous.iterdir():
            if source.is_file():
                (cache / source.name).symlink_to(source)
    cache.mkdir(exist_ok=True)
    # Imported revisions use their adjacent cache directory. All immutable
    # revisions share one source-keyed cache without duplicate GPU data.
    for model in (project / 'models/imports').glob('*/*/asset.glb'):
        link = model.parent / 'texture_blocks'
        if not link.exists():
            link.symlink_to(cache, target_is_directory=True)
    catalogs = [args.catalog] + ([args.extra_catalog] if args.extra_catalog else [])
    files = sorted({Path(p) for catalog in catalogs for r in json.loads(catalog.read_text()) for p in r['files']})
    env, _ = native_environment()
    env.update(DISPLAY=':0', XAUTHORITY='/home/computer/.Xauthority', VALWOOD_TEST_NATIVE_GL='1')
    for linear in ([True] if args.linear_only else [True, False]):
        command = ['/tmp/valwood-cook-texture-blocks', '--output', str(cache)]
        if linear:
            command.append('--linear')
        for file in files:
            command += ['--model', str(file)]
        subprocess.run(command, env=env, cwd=ROOT, check=True)

if __name__ == '__main__':
    main()

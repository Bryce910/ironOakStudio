#!/usr/bin/env python3
"""Create small local playtest copies with private mutable state and shared banks.

These Linux copies intentionally use asset-bank symlinks and require the guarded
playtest runtime. They are not portable artist packages or editable workspaces.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PROJECTS = {'city':'valwood_city_showcase', 'jungle':'valwood_visual', 'grandstone':'valwood_showcase'}
BANKS = frozenset({'models','scenery','ground','particles','audio','fonts','environments',
                  'assets','texture_blocks','imports','textures','surfaces','prefabs','cinematics'})
COPIED_TREES = ('ai','shaders')
PRIVATE = frozenset({'saves','qa','captures','backups','downloads','credentials','secrets',
                     'passwords','tokens','__pycache__','studio-layout.ini'})
ROOT_SUFFIXES = frozenset({'.valwood','.studio','.json','.settings','.txt','.md','.entities',
                           '.physics','.gameplay','.living','.regions','.components','.scene',
                           '.sky','.views','.flames','.residents'})
MANIFEST = 'local-origin.json'


def digest(path):
    value=hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda:source.read(1024*1024),b''): value.update(block)
    return value.hexdigest()


def excluded(name):
    low=name.casefold()
    return (name.startswith('.') or low in PRIVATE or Path(low).stem in PRIVATE or '.recovery' in low or '.previous' in low
            or '.backup' in low or low.endswith(('.workspace','.lock','.bak','.tmp','~')))


def ordinary_tree(path, relative=Path(), ancestors=()):
    """Resolve source links but never create links for mutable copied trees."""
    resolved=path.resolve(strict=True)
    if resolved in ancestors: raise ValueError('Cyclic source authoring tree')
    if not resolved.is_dir(): raise ValueError('Expected source authoring directory')
    for child in sorted(resolved.iterdir()):
        if excluded(child.name): continue
        item=child.resolve(strict=True); name=relative/child.name
        if item.is_dir(): yield from ordinary_tree(item,name,ancestors+(resolved,))
        elif item.is_file(): yield name,item
        else: raise ValueError('Nonregular source authoring entry')


def inventory(source):
    files={}; banks={}
    for entry in sorted(source.iterdir()):
        if excluded(entry.name): continue
        resolved=entry.resolve(strict=True)
        if entry.name in COPIED_TREES:
            for name,target in ordinary_tree(resolved): files[(Path(entry.name)/name).as_posix()]=target
        elif entry.name in BANKS:
            if not resolved.is_dir(): raise ValueError('Runtime bank is not a directory')
            banks[entry.name]=resolved
        elif resolved.is_file() and entry.suffix.casefold() in ROOT_SUFFIXES: files[entry.name]=resolved
    if not {'project.valwood','world.studio','graphics.settings'} <= files.keys():
        raise ValueError('Incomplete source project')
    if sum(p.stat().st_size for p in files.values())>32*1024*1024:
        raise ValueError('Mutable project copy unexpectedly exceeds 32MiB')
    return files,banks


def runtime_inputs(engine,area,runtime_directory=None):
    folder=(Path(runtime_directory) if runtime_directory is not None else
            engine/'docs/playtest-slices/2026-11-02'/area/'runtime')
    mapping={'playtest.route':folder/'playtest.route'}
    if area=='grandstone':
        mapping['playtest-hall.route']=folder/'playtest-hall.route'
        mapping.update({f'playtest/{sector}.{suffix}':folder/f'{sector}.{suffix}'
                        for sector in ('approach','hall') for suffix in ('physics','settings')})
    for target,path in mapping.items():
        if not path.is_file(): raise ValueError(f'Runtime data not ready for {area}: {path}')
        if path.is_symlink(): raise ValueError('Generated runtime input must be an ordinary file')
    return mapping


def canonical(value): return (json.dumps(value,sort_keys=True,indent=2)+'\n').encode()


def checked_copy(source,target,expected):
    target.parent.mkdir(parents=True,exist_ok=True)
    with source.open('rb') as src, target.open('xb') as dst:
        value=hashlib.sha256()
        for block in iter(lambda:src.read(1024*1024),b''): value.update(block); dst.write(block)
    if value.hexdigest()!=expected: raise ValueError('Source changed while copying')
    shutil.copymode(source,target)


def local_file(project,name):
    path=project
    for part in Path(name).parts:
        if part in ('.','..') or part.startswith('.'): raise ValueError('Unsafe generated filename')
        path=path/part
        if path.is_symlink(): raise ValueError('Generated output enters a symlink')
    return path


def prepare_area(engine,output,area,refresh=False,runtime_directory=None):
    source=(engine/'projects'/PROJECTS[area]).resolve(strict=True)
    files,banks=inventory(source); guard={name:digest(path) for name,path in files.items()}
    runtime=runtime_inputs(engine,area,runtime_directory); generated={name:digest(path) for name,path in runtime.items()}
    destination=output/area
    if refresh:
        if destination.is_symlink() or not destination.is_dir(): raise ValueError('Missing ordinary playtest area')
        project=destination/'project'; origin=destination/MANIFEST
        if project.is_symlink() or not project.is_dir() or origin.is_symlink(): raise ValueError('Unsafe playtest copy')
        metadata=json.loads(origin.read_text())
        if (metadata.get('schema')!='valwood.local-playtest-copy' or metadata.get('area')!=area
                or metadata.get('source_project')!=str(source) or metadata.get('source_guard')!=guard
                or metadata.get('shared_banks')!={name:str(path) for name,path in banks.items()}):
            raise ValueError('Playtest source revision changed; choose a new output root')
        for name,path in banks.items():
            link=project/name
            if not link.is_symlink() or link.resolve(strict=True)!=path: raise ValueError('Shared bank changed')
        for name in runtime:
            target=local_file(project,name)
            if target.exists() and (not target.is_file() or digest(target)!=metadata['generated'].get(name)):
                raise ValueError('Generated runtime file was independently modified')
    else:
        if os.path.lexists(destination): raise ValueError(f'Existing output rejected: {destination}')
        metadata={'schema':'valwood.local-playtest-copy','version':1,'area':area,
                  'source_project':str(source),'source_guard':guard,
                  'shared_banks':{name:str(path) for name,path in banks.items()},
                  'policy':'private root/AI/shaders/saves; immutable shared banks; guarded playtest runtime only'}
    output.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.'+area+'-',dir=output))
    try:
        staged=stage/'project';staged.mkdir()
        if not refresh:
            for name,path in files.items(): checked_copy(path,staged/name,guard[name])
            for name,path in banks.items(): (staged/name).symlink_to(path,target_is_directory=True)
            (staged/'saves').mkdir()
        for name,path in runtime.items(): checked_copy(path,staged/name,generated[name])
        after_files,after_banks=inventory(source)
        if ({name:digest(path) for name,path in after_files.items()}!=guard or after_banks!=banks):
            raise ValueError('Protected source changed during preparation')
        metadata['generated']=generated
        (stage/MANIFEST).write_bytes(canonical(metadata))
        if not refresh:
            if os.path.lexists(destination): raise ValueError('Output appeared during preparation')
            stage.rename(destination)
        else:
            # Stage all bytes before replacing only our generated files. Roll
            # back that narrow transaction if any filesystem operation fails.
            originals={name:(local_file(project,name).read_bytes() if local_file(project,name).exists() else None)
                       for name in runtime}
            old_manifest=origin.read_bytes(); installed=[]
            try:
                for name in runtime:
                    target=local_file(project,name);target.parent.mkdir(parents=True,exist_ok=True)
                    os.replace(staged/name,target);installed.append(name)
                os.replace(stage/MANIFEST,origin)
            except BaseException:
                for name in reversed(installed):
                    target=local_file(project,name);raw=originals[name]
                    if raw is None: target.unlink()
                    else:
                        undo=stage/('rollback-'+str(len(name)));undo.write_bytes(raw);os.replace(undo,target)
                recovery=stage/'manifest-rollback';recovery.write_bytes(old_manifest);os.replace(recovery,origin)
                raise
        return {'area':area,'project':str(destination/'project'),'refreshed_runtime_only':refresh,
                'copied_bytes':sum(p.stat().st_size for p in files.values()) if not refresh else 0,
                'source_unchanged':True,'generated':generated,'manifest':str(destination/MANIFEST)}
    finally:
        if stage.exists(): shutil.rmtree(stage)


def prepare(engine,areas,output=None,binary=None,refresh=False,runtime_directory=None):
    engine=Path(engine).resolve(strict=True);output=Path(output or engine/'build/playtest-slices').resolve()
    projects=(engine/'projects').resolve()
    if output==projects or projects in output.parents: raise ValueError('Output cannot enter source projects')
    areas=list(dict.fromkeys(areas))
    if not areas or any(a not in PROJECTS for a in areas): raise ValueError('Choose a known playtest area')
    if runtime_directory is not None and len(areas)!=1:
        raise ValueError('A runtime directory override requires a single playtest area')
    # Preflight every requested area so unavailable Grandstone data does not
    # leave an unexpected partial collection behind.
    for area in areas:
        _,banks=inventory((projects/PROJECTS[area]).resolve(strict=True));runtime_inputs(engine,area,runtime_directory)
        if any(output==bank or bank in output.parents for bank in banks.values()):
            raise ValueError('Output cannot enter a shared source bank')
        if not refresh and os.path.lexists(output/area): raise ValueError('Existing output rejected')
    if binary is not None:
        binary=Path(binary).resolve(strict=True);target=output/'bin/rpg';expected=digest(binary)
        if target.is_symlink(): raise ValueError('Shared runtime cannot be a symlink')
        if target.exists() and digest(target)!=expected: raise ValueError('Existing runtime differs; owner must install it explicitly')
        if not target.exists():
            target.parent.mkdir(parents=True,exist_ok=True)
            try: checked_copy(binary,target,expected)
            except BaseException:
                if target.exists(): target.unlink()
                raise
    return [prepare_area(engine,output,area,refresh,runtime_directory) for area in areas]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine-root',type=Path,default=ROOT)
    parser.add_argument('--output-root',type=Path)
    parser.add_argument('--area',action='append',required=True,choices=sorted(PROJECTS))
    parser.add_argument('--binary',type=Path,help='Copy owner-selected final runtime to shared bin/rpg; never overwrite a different one')
    parser.add_argument('--refresh-runtime-data',action='store_true',help='Refresh only generated route/sector files, preserving saves/settings/AI')
    parser.add_argument('--runtime-directory',type=Path,help='Use staged route/sector files for one selected area; defaults remain unchanged')
    args=parser.parse_args()
    try: result=prepare(args.engine_root,args.area,args.output_root,args.binary,args.refresh_runtime_data,args.runtime_directory)
    except (OSError,ValueError,KeyError) as error: parser.exit(1,f'Playtest preparation failed: {error}\n')
    print(json.dumps({'passed':True,'areas':result},indent=2))


if __name__=='__main__':main()

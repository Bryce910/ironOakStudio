"""Run with bundled Windows Python. Exercises the real handoff on a tiny private seed."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from world_artist_handoff import create_workspace,export_return,review_return
from build_world_package import verify_package
from world_asset_database import canonical_json,sha256_bytes,VERSION,ValidationError,write_new_json

with tempfile.TemporaryDirectory(prefix='valwood-windows-handoff-') as tmp:
    root=Path(tmp)/'Artist work é spaces';root.mkdir()
    seed=root/'Seed';(seed/'project').mkdir(parents=True);(seed/'bin').mkdir()
    (seed/'project/project.valwood').write_text('name=Private Windows Fixture\nterrain=valwood_atlas\n')
    studio=seed/'project/world.studio'
    studio.write_text('VALWOOD_STUDIO 4\nlook .28 1 1 1 17 0 .2 0 1 .6\n')
    shutil.copyfile(Path(__file__).resolve().parents[1]/'build/windows/editor/bin/rpg.exe',seed/'bin/rpg.exe')
    entries=[]
    for p in sorted(seed.rglob('*')):
        if p.is_file(): entries.append(dict(path=p.relative_to(seed).as_posix(),sha256=hashlib.file_digest(p.open('rb'),'sha256').hexdigest(),bytes=p.stat().st_size,mode='0644'))
    manifest=dict(schema='valwood.windows-package',schema_version=VERSION,content_id=sha256_bytes(canonical_json(entries)),files=entries)
    (seed/'package-manifest.json').write_bytes(canonical_json(manifest))
    before=verify_package(seed)
    write_new_json(root/'metadata.json',dict(ok=True),forbidden_roots=[seed])
    assert json.loads((root/'metadata.json').read_text())['ok']
    created=create_workspace(seed,root/'Work')
    changed=root/'Work/project/world.studio'
    changed.write_text(studio.read_text()+'# Windows artist saved change\n')
    returned=export_return(root/'Work',root/'Return')
    assert returned['changed_files']==1
    reviewed=review_return(root/'Return',seed,root/'Review')
    assert reviewed['ok'] and (root/'Review/project/world.studio').read_bytes()==changed.read_bytes()
    assert verify_package(seed)==before
    try:create_workspace(seed,root/'Work')
    except ValidationError:pass
    else:raise AssertionError('Existing workspace overwritten')
    (root/'Review/project/world.studio').write_text('# independently edited\n')
    assert review_return(root/'Return',root/'Review')['conflicts']==['world.studio']
    print(json.dumps(dict(ok=True,create=created,export=returned,review=reviewed,
        seed_unchanged=True,existing_output_rejected=True,conflict_rejected=True)))

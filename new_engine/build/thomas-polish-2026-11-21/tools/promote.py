"""Publish the reviewed character quality generation through the stable manifest."""
from pathlib import Path
import datetime
import json
import shutil
import sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent
sys.dont_write_bytecode=True;sys.path.insert(0,str(N))
from check_unified import check,need,read,sha

def state_files(project):
    files=[]
    for name in ('saves','settings'):
        folder=project/name
        need(not folder.is_symlink(),'State folder unexpectedly aliased: '+str(folder))
        if folder.exists():
            for p in folder.rglob('*'):
                need(not p.is_symlink(),'State entry unexpectedly aliased: '+str(p))
                if p.is_file():files.append(p)
    for pattern in ('*.settings','arena_*.cfg','player-profile.config'):
        files+=list(project.glob(pattern))
    for p in files:need(p.is_file() and not p.is_symlink(),'State file unexpectedly aliased: '+str(p))
    return sorted(set(files))

def carry_state():
    previous=N.with_name('thomas-quality-2026-11-20');transfers={}
    for mode in ('arena','joust'):
        source=previous/(mode+'-project');target=N/(mode+'-project')
        before={str(p.relative_to(source)):sha(p) for p in state_files(source)}
        for name in ('saves','settings'):
            destination=target/name
            need(not destination.is_symlink(),'Candidate state folder unexpectedly aliased')
            if destination.exists():shutil.rmtree(destination)
            if (source/name).exists():shutil.copytree(source/name,destination)
        for p in state_files(target):
            if len(p.relative_to(target).parts)==1:p.unlink()
        for p in state_files(source):
            if len(p.relative_to(source).parts)==1:shutil.copy2(p,target/p.name)
        after={str(p.relative_to(source)):sha(p) for p in state_files(source)}
        copied={str(p.relative_to(target)):sha(p) for p in state_files(target)}
        need(before==after==copied,'Mode state changed while carrying it into the new generation')
        transfers[mode]=before
    paths=[previous/'shared/graphics.settings',previous/'arena-project/graphics.settings',previous/'joust-project/graphics.settings',E/'projects/valwood_current/graphics.settings']
    paths += [p/'graphics.settings' for p in (E/'projects').glob('valwood_*_current')]
    paths=[p for p in paths if p.is_file()]
    newest=max(paths,key=lambda p:p.stat().st_mtime_ns);digest=sha(newest)
    shutil.copy2(newest,N/'shared/graphics.settings')
    need(sha(newest)==digest==sha(N/'shared/graphics.settings'),'Newest graphics setting changed during publication')
    return dict(mode_state_sha256=transfers,newest_graphics_source=str(newest),graphics_sha256=digest,canonical_user_files_modified=False)

ready=check();review_path=N/'review/FINAL_REVIEW.json';review=read(review_path)
need(review.get('passed') is True and review.get('visual_review_performed') is True and review.get('ready_sha256')==sha(N/'UNIFIED_READY.json'),'Final review must approve frozen model tiers and captured third-person motion')
evidence=review.get('evidence_sha256',{});need(bool(evidence),'Final review requires explicit CPU/GPU evidence')
for p,h in evidence.items():need(Path(p).is_file() and sha(p)==h,'Final review evidence changed: '+p)
manifest=ROOT/'CURRENT_VALWOOD.json';prior=read(manifest)
need(prior['runtime_generation']==str(N.with_name('thomas-quality-2026-11-20')),'Active runtime changed during quality integration; review baseline')
rollback=N/'rollback';rollback.mkdir(exist_ok=True);backup=rollback/'CURRENT_VALWOOD.json';need(not backup.exists(),'Generation has already been published')
state_transfer=carry_state()
shutil.copy2(manifest,backup)
updated=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=ready['binary_sha256'])
t=manifest.with_suffix('.json.tmp');t.write_text(json.dumps(updated,indent=2)+'\n');t.replace(manifest)
(N/'PUBLICATION.json').write_text(json.dumps(dict(schema='valwood.thomas-polish.publication.v1',published=True,published_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),manifest=str(manifest),rollback=str(backup),ready_sha256=sha(N/'UNIFIED_READY.json'),final_review_sha256=sha(review_path),binary_reused=False,source_reused=False,state_transfer=state_transfer),indent=2)+'\n')
print('Published Thomas character quality:',N)

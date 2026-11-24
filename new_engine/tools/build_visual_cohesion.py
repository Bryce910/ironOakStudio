#!/usr/bin/env python3
"""Stage the reviewed city/meadow look in a private project revision."""
from pathlib import Path
import argparse
import json
import math
import os
import shutil
import numpy as np
from PIL import Image

from check_opening_prologue import isolate_project, mutable_snapshot, sha
from visual_ground_finish import patch_shader as ground_patch_shader
from visual_town_entry_blend import patch_shader as entry_patch_shader
from visual_town_entry_blend import patch_vertex_shader

ROOT=Path(__file__).resolve().parents[1]
DOC=ROOT/'docs/visual-cohesion/2026-11-04'
DEST=ROOT/'build/opening-finward-reference/v14'


def patch_shader(text):
    return entry_patch_shader(ground_patch_shader(text))


def write_json(p,d):
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(json.dumps(d,indent=2,allow_nan=False)+'\n')


def private_path(project,relative):
    """Detach only parents needed by an override, retaining read-only banks."""
    relative=Path(relative)
    assert not relative.is_absolute() and '..' not in relative.parts
    current=project
    for part in relative.parts[:-1]:
        current=current/part
        if current.is_symlink():
            source=current.resolve(strict=True)
            assert source.is_dir()
            current.unlink();current.mkdir()
            for item in source.iterdir():
                (current/item.name).symlink_to(item.resolve(strict=True),target_is_directory=item.is_dir())
        else:current.mkdir(exist_ok=True)
    target=project/relative
    if target.is_symlink():target.unlink()
    return target


def lighting(project):
    studio=project/'world.studio';rows=studio.read_text().splitlines()
    old=next(r for r in rows if r.startswith('look '))
    # sharpness, sun, sky, exposure, source hour/bearing, cloud, rain, wind, fog
    look='look 0 1.25 1.15 1.05 16.2675544121 -84.2842953297 .18 0 .3 .05'
    assert rows.count(old)==1;rows[rows.index(old)]=look;studio.write_text('\n'.join(rows)+'\n')
    (project/'city.scene').write_text('VALWOOD_CITY_LOOK 1\nsun .98 .90 .78\nsky .40 .43 .47\nground .16 .15 .13\nfog .42 .44 .46\nzenith .12 .22 .36\nhaze .0006\n')
    (project/'city.sky').write_text('VALWOOD_CITY_SKY 1\npanorama "environments/publisher-visible-sky.hdr"\nmaterial 1.0 1.05 -0.1687459945678711\ncube 512\n')
    source=(project/'ground/village_grade.png').resolve(strict=True)
    grade=np.asarray(Image.open(source).convert('RGB'),dtype=np.float64)
    assert grade.shape==(16,256,3)
    # The runtime applies its source grade at 70%. Bake a calibrated weak
    # transform into a private LUT: effective artistic grade becomes 18%.
    identity=np.zeros_like(grade)
    for blue in range(16):
        for green in range(16):
            for red in range(16):identity[green,blue*16+red]=[red*17,green*17,blue*17]
    target=private_path(project,'ground/village_grade.png')
    Image.fromarray(np.rint(identity+(grade-identity)*(.18/.70)).clip(0,255).astype(np.uint8)).save(target)
    return dict(studio_before=old,studio_after=look,source_grade_sha256=sha(source),
        grade_sha256=sha(target),effective_source_grade=.18,
        sky_side=512,sky_contrast=1.05,sky_brightness=1.,
        sky_extra_gpu_bytes=6*(512*512-256*256)*6,
        source_hdr_unchanged=True)


def replacements():
    ground=json.loads((DOC/'ground/candidate-v1/GROUND_FINISH_REVIEW.json').read_text())
    grass=json.loads((DOC/'grass/CANDIDATE_REVIEW.json').read_text())
    rows=list(ground['replacements'])
    for asset in grass['assets']:
        stem=asset['studio_row'].split()[2]
        for model in asset['candidates']:
            names=[stem+'.glb',stem+'_near.glb'] if model['lod']=='near' else [stem+'_mid.glb']
            for name in names:rows.append(dict(relative_path='models/'+name,candidate=model['path'],sha256=model['sha256']))
    entry=json.loads((DOC/'ground/entrance-v2/ENTRANCE_REVIEW.json').read_text())
    merged={r['relative_path']:r for r in rows}
    merged.update({r['relative_path']:r for r in entry['replacements']})
    soil=json.loads((DOC/'ground/dry-soil-v1/SOIL_REVIEW.json').read_text())
    merged.update({r['relative_path']:r for r in soil['replacements']})
    seal=json.loads((DOC/'ground/seal-v1/SEAL_REVIEW.json').read_text())
    merged.update({r['relative_path']:r for r in seal['replacements']})
    return list(merged.values())


def placement_patch(text, name):
    rows=json.loads((DOC/'grass/CONTACT_PLACEMENT_PATCH.json').read_text())['row_edits']
    for row in rows:
        if row['project_relative_file']!=name:continue
        if row['proposed_line'] in text:continue
        assert text.splitlines().count(row['original_line'])==1, name
        text=text.replace(row['original_line'],row['proposed_line'],1)
    return text


def placements(project):
    rows=json.loads((DOC/'grass/CONTACT_PLACEMENT_PATCH.json').read_text())['row_edits']
    changed=[]
    for name in sorted({row['project_relative_file'] for row in rows}):
        source=project/name;before=sha(source);data=placement_patch(source.read_text(),name)
        target=private_path(project,name);target.write_text(data)
        changed.append(dict(relative_path=name,before_sha256=before,sha256=sha(target)))
    return changed


def apply(project):
    result=dict(lighting=lighting(project),models=[],placements=placements(project))
    for row in replacements():
        source=Path(row['candidate']);assert sha(source)==row['sha256']
        target=private_path(project,row['relative_path']);shutil.copyfile(source,target)
        result['models'].append(dict(row,installed_sha256=sha(target)))
    shader=project/'shaders/world_foliage.fs.glsl'
    text=shader.read_text();before=sha(shader);shader.write_text(patch_shader(text))
    result['shader']=dict(before_sha256=before,sha256=sha(shader),scope='Only tagged countryside ground blend branches; previous wind, city materials and apartment policy retained.')
    vertex=project/'shaders/world_foliage.vs.glsl';vertex.write_text(patch_vertex_shader(vertex.read_text()))
    return result


def stage():
    source=ROOT/'build/opening-finward-reference/v13/project'
    if DEST.exists():raise ValueError('Use a new revision directory')
    before=mutable_snapshot(source)
    project=DEST/'project';isolate_project(source,project)
    changes=apply(project)
    reference=project/'opening/reference.json';meta=json.loads(reference.read_text())
    meta['shader']['before_visual_cohesion_sha256']=meta['shader']['sha256']
    meta['shader']['sha256']=sha(project/'shaders/world_foliage.fs.glsl')
    meta['shader']['ground_blend']='Mode2 explicit7.875 tag only; continuous reviewed dirt/grass weights.'
    write_json(reference,meta)
    write_json(project/'opening/visual-cohesion.json',dict(version=1,review=str(DOC),changes=changes,
        geometry_physics_and_story_unchanged=True,source_archives_unchanged=True))
    origin=json.loads((source.parent/'opening-origin.json').read_text())
    generated={p.relative_to(project).as_posix():sha(p) for p in project.rglob('*') if p.is_file() and not p.is_symlink()}
    write_json(DEST/'opening-origin.json',dict(passed=True,project=str(project),source_project=str(source),generated=generated,
        source_assets_unchanged=True,reference_scene_copied=True,cook_review=origin['cook_review'],scope='Coherent city/meadow lighting, source-ground material transitions and grass compositions; runtime/story/physics preserved.'))
    authored=lambda s:{k:v for k,v in s.items() if k!='graphics.settings' and not k.startswith('saves/')}
    assert authored(before)==authored(mutable_snapshot(source))
    write_json(DOC/'STAGED.json',dict(passed=True,project=str(project),source_unchanged=True,changes=changes,runtime_rebuilt=False))
    # The same scene with no opening marker supports the existing direct-view
    # QA route, while the final playable project remains the guarded prologue.
    qa=DOC/'after-project';isolate_project(ROOT/'projects/valwood_city_showcase',qa)
    qachanges=apply(qa);write_json(DOC/'QA_STAGE.json',dict(passed=True,project=str(qa),changes=qachanges))
    return project


def refresh():
    """Bind final reviewed bytes after bounded lighting/contact refinements."""
    for project,report in [(DEST/'project','STAGED.json'),(DOC/'after-project','QA_STAGE.json')]:
        data=json.loads((DOC/report).read_text());changes=data['changes']
        changes['lighting']['studio_after']=next(r for r in (project/'world.studio').read_text().splitlines() if r.startswith('look '))
        changes['models']=[dict(r,installed_sha256=sha(project/r['relative_path'])) for r in replacements()]
        assert all(r['sha256']==r['installed_sha256'] for r in changes['models'])
        changes['placements']=placements(project)
        changes['shader']['sha256']=sha(project/'shaders/world_foliage.fs.glsl')
        changes['shader']['scope']='Tagged ground shoulders and bounded town entrance material blend; previous wind and apartment policy retained.'
        changes['source_paint_decoder']=dict(vertex_shader_sha256=sha(project/'shaders/world_foliage.vs.glsl'),review=str(DOC/'ground/source-paint-decoder/REVIEW.json'))
        changes['dry_soil_review']=str(DOC/'ground/dry-soil-v1/SOIL_REVIEW.json')
        changes['subsurface_seal_review']=str(DOC/'ground/seal-v1/SEAL_REVIEW.json')
        changes['town_entrance_review']=str(DOC/'ground/entrance-v2/ENTRANCE_REVIEW.json')
        write_json(DOC/report,data)
        if project==DEST/'project':
            reference=project/'opening/reference.json';meta=json.loads(reference.read_text())
            meta['shader']['sha256']=sha(project/'shaders/world_foliage.fs.glsl');write_json(reference,meta)
            write_json(project/'opening/visual-cohesion.json',dict(version=1,review=str(DOC),changes=changes,
                original_ground_surfaces_physics_and_story_unchanged=True,subsurface_seal_added=True,
                grass_contact_patches_refit=4,source_archives_unchanged=True))
            origin=DEST/'opening-origin.json';meta=json.loads(origin.read_text())
            meta['generated']={p.relative_to(project).as_posix():sha(p) for p in project.rglob('*') if p.is_file() and not p.is_symlink()}
            write_json(origin,meta)
    return True


def publish():
    for proof in ['after-sealed/result.json','native-v14-final/results.json','wine-v14-recovery-v2/results.json','review/SOURCE_BANK_PRESERVATION_PROOF.json']:
        accepted=json.loads((DOC/proof).read_text());assert accepted['passed'],proof
    projects=[ROOT/'projects/valwood_city_showcase',ROOT/'build/playtest-slices/city/project']
    changed=[]
    relative=['world.studio','city.scene','city.sky','ground/village_grade.png','shaders/world_foliage.fs.glsl','shaders/world_foliage.vs.glsl']
    relative += [r['relative_path'] for r in replacements()]
    relative += sorted({r['project_relative_file'] for r in json.loads((DOC/'grass/CONTACT_PLACEMENT_PATCH.json').read_text())['row_edits']})
    assert len(relative)==len(set(relative))
    # Validate every destination and prepare its exact bytes before any write.
    pending=[]
    for project in projects:
        # These current scene copies use identical source assets and lighting.
        # All writes replace their destination inode, retaining prior snapshots.
        for name in relative:
            src=DEST/'project'/name
            dst=project/name;backup=DOC/'before'/dst.relative_to(ROOT)
            assert dst.is_file(),dst
            assert not backup.exists(),backup
            if name=='world.studio':
                # Preserve any per-copy registered assets and edit only look.
                text=dst.read_text();old=next(r for r in text.splitlines() if r.startswith('look '));new=next(r for r in src.read_text().splitlines() if r.startswith('look '));data=text.replace(old,new,1).encode()
            elif name=='shaders/world_foliage.fs.glsl':data=patch_shader(dst.read_text()).encode()
            elif name=='shaders/world_foliage.vs.glsl':data=patch_vertex_shader(dst.read_text()).encode()
            elif name.startswith('scenery/'):data=placement_patch(dst.read_text(),name).encode()
            else:data=src
            pending.append((dst,backup,data,sha(dst)))
    for dst,backup,data,before in pending:
        assert sha(dst)==before, dst
        backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(dst,backup)
        tmp=dst.with_suffix(dst.suffix+'.cohesion-new')
        if isinstance(data,Path):shutil.copyfile(data,tmp)
        else:tmp.write_bytes(data)
        os.replace(tmp,dst)
        changed.append(dict(path=str(dst),before_sha256=sha(backup),sha256=sha(dst),backup=str(backup)))
    write_json(DOC/'PUBLICATION.json',dict(passed=True,files=changed,source_archives_unchanged=True,earlier_opening_projects_and_windows_archives_preserved=True))
    return len(changed)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=['stage','refresh','publish'])
    args=parser.parse_args();print(dict(stage=stage,refresh=refresh,publish=publish)[args.action]())

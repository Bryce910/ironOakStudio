#!/usr/bin/env python3
"""Reproduce the accepted arrival recipe in a fresh private project.

No default invocation writes files. Use ``verify``/``inputs`` for read-only
acceptance; ``stage`` requires an explicit destination that does not exist.
The historical v16 directory is never updated in place by this tool.
"""
from pathlib import Path
import argparse
from collections import Counter
import json
import math
import shlex
import shutil
import numpy as np
from check_opening_prologue import isolate_project, mutable_snapshot, sha
from build_visual_cohesion import private_path, write_json
from build_city_countryside import readfield, sample

ROOT=Path(__file__).resolve().parents[1]
DOC=ROOT/'docs/arrival-redesign/2026-11-04'
SOURCE=ROOT/'build/opening-finward-reference/v15/project'
HISTORICAL_PROJECT=ROOT/'build/opening-finward-reference/v16/project'
TERRAIN_REVIEW=DOC/'terrain/candidate-v7/TERRAIN_REVIEW.json'
ART_REVIEW=DOC/'art-v3/ART_CANDIDATE.json'
NEAR_INSTANCES=DOC/'art/surrounding_instances.txt'  # Exact90rows installed in v16.
MID_INSTANCES=DOC/'terrain/midground-v7/instances.txt'  # Replaces old14rows entirely.
SHADER_RECIPE=DOC/'terrain/ring-grass-shader-patch.json'
SHADER_RECIPE_SHA256='7a7df0ddfc8a2007afd860a7281506b588dc0bd0ee3c484eb141218952eb783c'
LOOK='look 0 1.35 1.05 1.08 15.2 -84.2842953297 .12 0 .3 .05'
CITY_LOOK=('VALWOOD_CITY_LOOK 1\nsun .98 .90 .78\nsky .32 .36 .42\n'
           'ground .105 .095 .078\nfog .42 .44 .46\nzenith .12 .22 .36\nhaze .0006\n')
FIRE_HEIGHT,FIRE_WIDTH,FIRE_INTENSITY,FIRE_SEED='.62','.40','.38','379'
FIRE_LIGHT_POWER='1.1'
NORTH_STOOL_CENTER=[-1487.37664868499,19.177702258446782,-10229.260611064201]


def require(condition,message):
    if not condition:raise ValueError(message)


def rows(text):
    return [r.strip()for r in text.splitlines()if r.strip()and not r.lstrip().startswith('#')]


def load_json(path):return json.loads(Path(path).read_text())


def apply_shader_recipe(text,recipe):
    """Apply each exact replacement once; an already-applied recipe is a no-op."""
    for patch in recipe['replacements']:
        needle,replacement=patch['needle'],patch['replacement']
        count=patch['expected_count']
        require(count==1,'Arrival ring recipe must specify exactly one replacement')
        old,new=text.count(needle),text.count(replacement)
        if old==count and new==0:text=text.replace(needle,replacement,count)
        else:require(old==0 and new==count,'Mixed, absent, or duplicated ring shader recipe')
    require(text.count('ARRIVAL_RING_GRASS_V1')==1,'Ring shader must occur exactly once')
    return text


def selections(source=SOURCE,terrain_review=TERRAIN_REVIEW,art_review=ART_REVIEW,
               near_instances=NEAR_INSTANCES,mid_instances=MID_INSTANCES,
               shader_recipe=SHADER_RECIPE):
    paths=dict(source=Path(source).resolve(),terrain_review=Path(terrain_review).resolve(),
               art_review=Path(art_review).resolve(),near_instances=Path(near_instances).resolve(),
               mid_instances=Path(mid_instances).resolve(),shader_recipe=Path(shader_recipe).resolve())
    source=paths['source'];terrain=load_json(paths['terrain_review']);art=load_json(paths['art_review'])
    require(terrain.get('passed')and art.get('models'),'Missing accepted terrain/art inputs')
    acceptance=load_json(paths['art_review'].parent/'ACCEPTANCE.json')
    require(acceptance.get('passed')and acceptance['manifest_sha256']==sha(paths['art_review']),
            'Art acceptance does not match the selected art manifest')
    require(sha(paths['shader_recipe'])==SHADER_RECIPE_SHA256,'Ring shader recipe changed; review explicitly')
    recipe=load_json(paths['shader_recipe'])
    require(recipe['required_relative_path']=='shaders/world_foliage.fs.glsl','Unexpected shader recipe target')
    near_text=paths['near_instances'].read_text();mid_text=paths['mid_instances'].read_text()
    near,mid=rows(near_text),rows(mid_text)
    require(len(near)==90 and len(mid)==124,'Arrival requires exactly90near and124midground rows')
    require(len(set(near+mid))==214,'Duplicate arrival vegetation rows')
    mid_review_path=paths['mid_instances'].parent/'MIDGROUND_REVIEW.json';mid_review=load_json(mid_review_path)
    require(mid_review.get('passed')and mid_review['new_instances']==124,'Midground review not accepted')
    require(mid_review['terrain_manifest_sha256']==sha(paths['terrain_review']),
            'Midground root contacts were measured against a different terrain revision')
    require([r['row']for r in mid_review['records']]==mid,'Midground file differs from measured124sites')
    replacements=list(terrain['replacements'])
    for model in art['models']:
        stem='arrival_roast'if'roast'in Path(model['path']).name else'arrival_camp'
        names=[stem+'.glb',stem+'_near.glb']if model['lod']=='near'else[stem+'_mid.glb']
        replacements.extend(dict(relative_path='models/opening/'+name,candidate=model['path'],sha256=model['sha256'])for name in names)
    require(len({r['relative_path']for r in replacements})==len(replacements),'Duplicate replacement destinations')
    for row in replacements:
        relative=Path(row['relative_path'])
        require(not relative.is_absolute()and'..'not in relative.parts,'Unsafe replacement destination')
        require(sha(Path(row['candidate']))==row['sha256'],'Candidate hash mismatch: '+row['candidate'])
    north=next(b for b in art['physics']if b['id']=='north_seat')
    require(np.allclose(north['center_world'],NORTH_STOOL_CENTER,rtol=0,atol=1e-10),'Unexpected north stool socket')
    paths.update(midground_review=mid_review_path)
    return dict(paths=paths,terrain=terrain,art=art,recipe=recipe,replacements=replacements,
                near_text=near_text,mid_text=mid_text,near=near,mid=mid,mid_review=mid_review)


def build_plan(**kwargs):
    """Read and validate source closure, then construct changed text in memory."""
    p=selections(**kwargs);source=p['paths']['source'];art=p['art'];terrain=p['terrain']
    fire=art['fire']['world_position'];contract=terrain['contract']
    require(contract['alias']=='bep_opening_arrival_border','Legacy nm_ border alias is obsolete')
    studio=(source/'world.studio').read_text().splitlines();changed_lights=0;looks=0
    for i,row in enumerate(studio):
        f=row.split()
        if not f:continue
        if f[0]=='look':studio[i]=LOOK;looks+=1
        if f[0]=='light'and abs(float(f[1])-fire[0])<.01 and abs(float(f[3])-fire[2])<.01:
            f[5]=FIRE_LIGHT_POWER;studio[i]=' '.join(f);changed_lights+=1
    require(looks==1 and changed_lights==1,'Expected one world look and one cooking-fire light')
    require(not any(r.startswith('asset '+contract['alias']+' ')for r in studio),'Source is already a redesigned project')
    studio.append(contract['studio_row'])
    require(sum(r.startswith('asset ')for r in studio)<=128,'Native scenery asset budget exceeded')
    flames=(source/'showcase.flames').read_text().splitlines();changed_flames=0
    for i,row in enumerate(flames):
        f=row.split()
        if len(f)==7 and abs(float(f[0])-fire[0])<.01 and abs(float(f[2])-fire[2])<.01:
            f[3:7]=[FIRE_HEIGHT,FIRE_WIDTH,FIRE_INTENSITY,FIRE_SEED];flames[i]=' '.join(f);changed_flames+=1
    require(changed_flames==1,'Expected one existing cooking-fire emitter')
    authored=(source/'scenery/authored_instances.txt').read_text().splitlines();changed_roasts=0
    require(not any(row in authored for row in p['near']+p['mid']),'Source already contains arrival dressing rows')
    for i,row in enumerate(authored):
        f=row.split()
        if f and f[0]=='nm_opening_arrival_roast':
            f[-1]=format(art['roast']['instance_bottom_world_y'],'.17g');authored[i]=' '.join(f);changed_roasts+=1
    require(changed_roasts==1,'Expected one cooking chicken instance')
    authored.extend([contract['instance_row'],*p['near_text'].splitlines(),*p['mid_text'].splitlines()])
    counts=Counter(rows('\n'.join(authored)))
    require(all(counts[r]==1 for r in p['near']+p['mid']+[contract['instance_row']]),'Arrival rows must occur exactly once')
    physics=(source/'city.physics').read_text().splitlines();changed_proxies=0
    for i,line in enumerate(physics):
        f=shlex.split(line)
        if len(f)>16 and f[:2]==['body','205']:
            require(f[2]=='Arrival camp north_seat','Body205 is not the known rear stool')
            f[7:10]=[format(v,'.17g')for v in NORTH_STOOL_CENTER]
            f[14:17]=[format(v,'.17g')for v in [.25,.23,.25]]
            physics[i]='body 205 "Arrival camp north_seat" '+' '.join(f[3:]);changed_proxies+=1
    require(changed_proxies==1,'Expected exactly one north stool physics proxy')
    shader_relative=p['recipe']['required_relative_path']
    shader=apply_shader_recipe((source/shader_relative).read_text(),p['recipe'])
    require(apply_shader_recipe(shader,p['recipe'])==shader,'Ring recipe is not idempotent')
    p['texts']={'world.studio':'\n'.join(studio)+'\n','city.scene':CITY_LOOK,
                'showcase.flames':'\n'.join(flames)+'\n','scenery/authored_instances.txt':'\n'.join(authored)+'\n',
                'city.physics':'\n'.join(physics)+'\n',shader_relative:shader}
    return p


def summary(p):
    return dict(passed=True,source_project=str(p['paths']['source']),
                terrain_review=str(p['paths']['terrain_review']),terrain_review_sha256=sha(p['paths']['terrain_review']),
                art_review=str(p['paths']['art_review']),art_review_sha256=sha(p['paths']['art_review']),
                near_instances=90,midground_instances=124,replacements=len(p['replacements']),
                ring_shader_occurrences=1,fire=dict(height=.62,width=.4,intensity=.38,seed=379,light_power=1.1),
                world_hour=15.2,sun_azimuth=-84.2842953297,north_stool_center=NORTH_STOOL_CENTER)


def stage(dest,**kwargs):
    """Create a fresh isolated project; never refine an existing revision."""
    p=build_plan(**kwargs);source=p['paths']['source'];dest=Path(dest).resolve()
    require(not dest.exists()and not dest.is_symlink(),'Destination must not exist; in-place refinement is unsupported')
    require(dest/'project'!=HISTORICAL_PROJECT,'Historical v16 is immutable to this staging tool')
    before=mutable_snapshot(source);project=dest/'project';isolate_project(source,project)
    for row in p['replacements']:shutil.copyfile(row['candidate'],private_path(project,row['relative_path']))
    for relative,text in p['texts'].items():private_path(project,relative).write_text(text)
    art=p['art'];terrain_path=p['paths']['terrain_review'];art_path=p['paths']['art_review']
    meta_path=project/'opening/arrival-campsite.json';meta=load_json(meta_path)
    models={r['relative_path']:r for r in meta['models']}
    models.update({r['relative_path']:r for r in p['replacements']if r['relative_path'].startswith('models/')})
    meta.update(models=list(models.values()),source_art=str(art_path),source_art_sha256=sha(art_path),
                soil_review=str(terrain_path),terrain_redesign_sha256=sha(terrain_path),surroundings=art['surroundings'])
    for body in meta['physics_bodies']:
        if body['id']==205:body.update(center=NORTH_STOOL_CENTER,half_extents=[.25,.23,.25])
    write_json(meta_path,meta)
    changes=dict(scope='Private reproducible arrival redesign; final GPU acceptance recorded separately.',models=p['replacements'],
                 terrain_review=str(terrain_path),terrain_review_sha256=sha(terrain_path),art_review=str(art_path),art_review_sha256=sha(art_path),
                 near_instances=str(p['paths']['near_instances']),near_instance_count=90,
                 midground_review=str(p['paths']['midground_review']),midground_instances=str(p['paths']['mid_instances']),midground_instance_count=124,
                 shader_recipe=str(p['paths']['shader_recipe']),shader_recipe_sha256=sha(p['paths']['shader_recipe']),
                 shader_sha256=sha(project/p['recipe']['required_relative_path']),look='15.2h, restrained hemisphere fill and directional warm sun',
                 cooking_fire=summary(p)['fire'],north_stool_center=NORTH_STOOL_CENTER,
                 source_project_unchanged=before==mutable_snapshot(source),visual_acceptance_pending=True)
    require(changes['source_project_unchanged'],'Source project changed while staging')
    write_json(project/'opening/arrival-redesign.json',changes)
    refresh(project,source)
    require(verify(project,plan=p)['passed'],'Fresh project did not reproduce selected inputs')
    return project


def verify(project,plan=None,**kwargs):
    """Read-only semantic/byte closure check against the selected recipe."""
    p=plan or build_plan(**kwargs);project=Path(project).resolve();checks={}
    for row in p['replacements']:checks[row['relative_path']]=sha(project/row['relative_path'])==row['sha256']
    for relative,expected in p['texts'].items():
        actual=(project/relative).read_text()
        if relative=='scenery/authored_instances.txt':checks[relative]=Counter(rows(actual))==Counter(rows(expected))
        elif relative=='city.physics':checks[relative]=[shlex.split(r)for r in rows(actual)]==[shlex.split(r)for r in rows(expected)]
        else:checks[relative]=actual==expected
    require(all(checks.values()),'Recipe mismatch: '+', '.join(k for k,v in checks.items()if not v))
    return dict(**summary(p),project=str(project),read_only=True,checks=checks)


def refresh(project,source=SOURCE):
    """Explicit origin refresh for a freshly staged project only."""
    project=Path(project).resolve();source=Path(source).resolve()
    require(project!=HISTORICAL_PROJECT,'Historical v16 origin must not be refreshed by this tool')
    old=load_json(source.parent/'opening-origin.json')
    generated={p.relative_to(project).as_posix():sha(p)for p in project.rglob('*')if p.is_file()and not p.is_symlink()}
    write_json(project.parent/'opening-origin.json',dict(passed=True,project=str(project),source_project=str(source),generated=generated,
        cook_review=old['cook_review'],source_assets_unchanged=True,scope='Private arrival redesign. Integrity only; GPU visual acceptance is separate.'))


def route(project,output):
    """Write explicit visual-review stations using the selected project's field."""
    grid,origin,step=readfield(Path(project)/'ground/countryside.height');cx,cz=-1486.37664868499,-10224.9106110642;stations=[]
    for i in range(8):
        x,z=cx,cz+5.2;y=float(sample(grid,np.array([[x,z]]),origin,step)[0])+1.75
        stations.append((f'camp-{i*45:03d}',[x,y,z,math.radians(i*45),-.025,math.radians(60)]))
    for name,dx,dz,yaw in [('west-edge',-20,0,-math.pi/2),('east-approach',9,-4,-1.15),('ground-detail',0,5.2,0)]:
        x,z=cx+dx,cz+dz;y=float(sample(grid,np.array([[x,z]]),origin,step)[0])+1.75
        stations.append((name,[x,y,z,yaw,-.38 if name=='ground-detail'else-.025,math.radians(60)]))
    output=Path(output);require(not output.exists(),'Do not overwrite existing review route')
    output.write_text('\n'.join(['VALWOOD_SOURCE_REVIEW 1 60']+[name+' 0 60 1 '+' '.join(format(v,'.17g')for v in values)for name,values in stations])+'\n')
    return output


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    for name in ['inputs','verify','stage']:
        q=sub.add_parser(name);q.add_argument('--source',type=Path,default=SOURCE);q.add_argument('--terrain-review',type=Path,default=TERRAIN_REVIEW)
        q.add_argument('--art-review',type=Path,default=ART_REVIEW);q.add_argument('--near-instances',type=Path,default=NEAR_INSTANCES)
        q.add_argument('--mid-instances',type=Path,default=MID_INSTANCES);q.add_argument('--shader-recipe',type=Path,default=SHADER_RECIPE)
        if name=='stage':q.add_argument('--dest',type=Path,required=True)
        if name=='verify':q.add_argument('--project',type=Path,required=True)
    q=sub.add_parser('route');q.add_argument('--project',type=Path,required=True);q.add_argument('--output',type=Path,required=True)
    a=vars(parser.parse_args());command=a.pop('command')
    if command=='route':print(route(**a))
    elif command=='stage':print(stage(**a))
    elif command=='verify':print(json.dumps(verify(**a),indent=2))
    else:print(json.dumps(summary(build_plan(**a)),indent=2))

if __name__=='__main__':main()

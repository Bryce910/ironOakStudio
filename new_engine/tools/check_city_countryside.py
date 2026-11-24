#!/usr/bin/env python3
"""Repeatable CPU audit of the installed city-to-countryside walking loop.

Writes only the requested report folder. Uses the game's actual Jolt/terrain
query helper, plus conservative native tree/rock collision-footprint checks.
No game launch, GPU work, project edits, source edits or performance claims.
"""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import os
import re
import subprocess
import struct

import numpy as np

ROOT=Path(__file__).resolve().parents[1]


def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb')as f:
        while block:=f.read(4*1024*1024):h.update(block)
    return h.hexdigest()


def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':'))


def field_samples(path):
    """Read exact source bytes without the cooker's geometry dependencies."""
    data=Path(path).read_bytes()
    if len(data)<48:raise ValueError('Truncated protected height field')
    header=struct.unpack_from('<8sIIdddd',data)
    magic,columns,rows,x,z,dx,dz=header
    if (magic!=b'VWHGT1\0\0'or not 2<=columns<=2049 or not 2<=rows<=2049
            or len(data)!=48+columns*rows*4 or not np.isfinite([x,z,dx,dz]).all()or dx<=0 or dz<=0):
        raise ValueError('Invalid protected height field')
    values=np.frombuffer(data,dtype='<f4',offset=48)
    if not np.isfinite(values).all():raise ValueError('Non-finite protected height field')
    return header,values


def ground_support_revision(catalog,project,expected_original,expected_coverage):
    """A revision can change only samples outside immutable source coverage."""
    revision=catalog.get('ground_support_revision',{})
    if not isinstance(revision,dict)or not revision:raise ValueError('Missing ground support revision')
    if revision.get('original_field_sha256')!=expected_original:raise ValueError('Ground revision original hash differs from source evidence')
    if revision.get('coverage_sha256')!=expected_coverage:raise ValueError('Ground revision coverage differs from source evidence')
    audit_path=Path(revision['audit_path'])
    if not audit_path.is_absolute()or digest(audit_path)!=revision.get('audit_sha256'):raise ValueError('Ground revision audit hash mismatch')
    audit=json.loads(audit_path.read_text())
    original_path=Path(audit.get('input_primary')or audit.get('source_height',''))
    repaired_path=Path(revision['repaired_field'])
    if not original_path.is_absolute()or not repaired_path.is_absolute():raise ValueError('Ground revision requires immutable absolute input/output paths')
    if digest(original_path)!=expected_original:raise ValueError('Ground revision original bytes differ from source evidence')
    audit_original=audit.get('input_primary_sha256')or audit.get('source_height_sha256')
    if audit_original!=expected_original:raise ValueError('Ground audit original hash differs from source evidence')
    repaired_hash=revision['repaired_field_sha256']
    if digest(repaired_path)!=repaired_hash or digest(project/'ground/showcase.height')!=repaired_hash:
        raise ValueError('Staged/installed repaired primary bytes differ')
    if audit.get('output_height_sha256')!=repaired_hash:raise ValueError('Ground audit output hash differs from revision')
    if audit.get('coverage_sha256')!=expected_coverage or digest(project/'ground/city.coverage')!=expected_coverage:
        raise ValueError('Original source coverage changed')
    original_header,original=field_samples(original_path);new_header,repaired=field_samples(project/'ground/showcase.height')
    mask_header,mask=field_samples(project/'ground/city.coverage')
    if original_header!=new_header or original_header!=mask_header:raise ValueError('Ground revision grid differs from original/coverage')
    if np.any((mask<0)|(mask>1)):raise ValueError('Invalid original source coverage values')
    covered=mask>.5;before=original[covered].tobytes();after=repaired[covered].tobytes()
    if before!=after:raise ValueError('Ground revision changed covered source samples')
    certificate=hashlib.sha256(before).hexdigest()
    if (revision.get('source_covered_samples_sha256')!=certificate
            or audit.get('source_covered_samples_sha256')!=certificate
            or audit.get('output_covered_samples_sha256')!=certificate
            or audit.get('covered_samples_bitwise_exact')is not True):
        raise ValueError('Ground revision covered-sample certificate differs')
    return dict(kind='Generated support in source-ground holes only',audit_path=str(audit_path),audit_sha256=digest(audit_path),
                original_field_sha256=expected_original,repaired_field_sha256=repaired_hash,coverage_sha256=expected_coverage,
                covered_nodes=int(covered.sum()),covered_samples_bitwise_exact=True,covered_samples_sha256=certificate)


def read_entry(path):
    lines=Path(path).read_text().splitlines()
    if not lines or lines[0]!='VALWOOD_CITY_WALK 1':raise ValueError('Invalid validated city exit route')
    result=[]
    for line in lines[1:]:
        fields=line.split('#',1)[0].split()
        if fields:result.append([float(fields[1]),float(fields[2])])
    result=np.asarray(result,float)
    if result.ndim!=2 or result.shape[1]!=2 or not np.isfinite(result).all():raise ValueError('Invalid city exit positions')
    return result


def protected_city(catalog,project,meta,baseline_path,source_evidence):
    baseline=json.loads(baseline_path.read_text());assets={a['name']:a for a in catalog['assets']}
    errors=[];allowed_assets=[]
    revision=catalog.get('authored_support_revision',{})
    for old in baseline['assets']:
        current=assets.get(old['name'])
        if current==old:continue
        # This one explicitly authored backdrop hill is outside the recovered
        # publisher geometry. Permit its reviewed support repair only when the
        # previous asset and both replacement model digests are bound exactly.
        permitted=False
        if old['name']=='SM_City_Castle_Backdrop_Plinth'and current and revision.get('asset')==old['name']:
            allowed_fields={'files','pivot_offset_native','bounds','stats','provenance'}
            # The reviewed source-material repair adds provenance alongside its
            # already hash-pinned model revision. Permit only the exact same
            # metadata declared by that revision, never unrelated asset fields.
            material_revision=current.get('material_revision')
            if isinstance(material_revision,dict)and material_revision==revision.get('material_revision'):
                allowed_fields.add('material_revision')
            unchanged={k:v for k,v in old.items()if k not in allowed_fields}=={k:v for k,v in current.items()if k not in allowed_fields}
            previous_hash=hashlib.sha256(canonical(old).encode()).hexdigest()
            permitted=(unchanged and revision.get('previous_asset_sha256')==previous_hash
                       and revision.get('previous_model_hashes')==[s['sha256']for s in old['stats']]
                       and revision.get('new_model_hashes')==[s['sha256']for s in current['stats']]
                       and len(current['files'])==len(old['files'])==2
                       and current.get('collision')is None and current.get('background_only')is True
                       and revision.get('collision')is False and revision.get('walkable')is False)
            if permitted:allowed_assets.append(dict(asset=old['name'],kind='Previously authored rock support revision; publisher castle unchanged',previous_asset_sha256=previous_hash,new_model_sha256=revision['new_model_hashes']))
        if not permitted:errors.append('Changed source asset definition: '+old['name'])
    expected=collections.Counter(canonical(p)for p in baseline['placements'])
    actual=collections.Counter(canonical(p)for p in catalog['placements'])
    missing=expected-actual;allowed=[]
    opening=catalog.get('opening',{})
    if missing and opening:
        # Historical metadata called this source_component104; it is the
        # catalog row index. The recovered UE component ID is1605.
        row_index=int(opening['source_component'])
        original=baseline['placements'][row_index]
        delta=np.asarray(opening['native_translation_delta'],float)
        replacement=json.loads(canonical(original))
        matrix=np.asarray(original['matrix_native'],float).reshape(4,4).T.copy()
        matrix[:3,3]+=delta;replacement['matrix_native']=matrix.T.flatten().tolist()
        original_key,replacement_key=canonical(original),canonical(replacement)
        if (opening.get('asset')==original['asset'] and delta.shape==(3,)and np.isfinite(delta).all()
                and missing[original_key]==1 and actual[replacement_key]==1):
            allowed.append(dict(index=row_index,source_component=original.get('source_component'),asset=original['asset'],translation_delta=delta.tolist()))
            missing[original_key]-=1
    repairs=catalog.get('placement_repairs',[])
    if not isinstance(repairs,list)or len(repairs)>18:
        errors.append('Source placement repair list must contain at most 18 reviewed house moves');repairs=[]
    reviewed_indices=set()
    for repair in repairs:
        try:
            index=repair['original_index']
            if type(index)is not int or not 0<=index<len(baseline['placements'])or index in reviewed_indices:
                raise ValueError('Invalid/duplicate original placement index')
            reviewed_indices.add(index);original=baseline['placements'][index];replacement=repair['replacement_record']
            if (repair['original_record']!=original or repair.get('source_component')!=original.get('source_component')
                    or repair.get('asset')!=original['asset']or not original['asset'].startswith('SM_House_')):
                raise ValueError('Repair does not identify the original source house')
            old_hash=hashlib.sha256(canonical(original).encode()).hexdigest();new_hash=hashlib.sha256(canonical(replacement).encode()).hexdigest()
            if repair.get('original_record_sha256')!=old_hash or repair.get('replacement_record_sha256')!=new_hash:
                raise ValueError('Original/replacement placement record hash mismatch')
            if ({k:v for k,v in original.items()if k!='matrix_native'}
                    !={k:v for k,v in replacement.items()if k!='matrix_native'}):
                raise ValueError('Repair changed source placement fields other than its transform')
            before=np.asarray(original['matrix_native'],float).reshape(4,4).T
            after=np.asarray(replacement['matrix_native'],float).reshape(4,4).T
            if (not np.isfinite(after).all()or not np.array_equal(before[:3,:3],after[:3,:3])
                    or not np.array_equal(before[3],after[3])):
                raise ValueError('Repair changed source rotation/scale or affine transform')
            original_key,replacement_key=canonical(original),canonical(replacement)
            if original_key==replacement_key or missing[original_key]!=1 or actual[replacement_key]!=1:
                raise ValueError('Reviewed placement replacement is missing, duplicated, or not a replacement')
            missing[original_key]-=1
            allowed.append(dict(index=index,source_component=original.get('source_component'),asset=original['asset'],
                kind='Explicit reviewed source house translation',translation_delta=(after[:3,3]-before[:3,3]).tolist(),
                original_record_sha256=old_hash,replacement_record_sha256=new_hash))
        except (KeyError,TypeError,ValueError)as error:errors.append('Invalid reviewed source placement repair: '+str(error))
    missing=+missing
    if missing:errors.append(f'{sum(missing.values())} unauthorized source placement changes/removals')
    for key in ['ground_asset','cameras','source_scene_sha256','coordinate_system']:
        if catalog.get(key)!=baseline.get(key):errors.append('Changed source '+key)
    source_hashes={}
    if source_evidence.exists():
        evidence=json.loads(source_evidence.read_text()).get('source_priority_files_sha256',{})
        for name in ['ground/showcase.height','ground/city.coverage']:
            current=digest(project/name);expected_hash=evidence.get(name)
            source_hashes[name]=dict(actual=current,expected=expected_hash,passed=bool(expected_hash and current==expected_hash))
            if name=='ground/showcase.height'and catalog.get('ground_support_revision'):
                try:
                    authorized=ground_support_revision(catalog,project,expected_hash,evidence.get('ground/city.coverage'))
                    source_hashes[name].update(passed=True,authorized_revision=authorized)
                except (KeyError,TypeError,ValueError,OSError)as error:
                    source_hashes[name].update(passed=False,revision_error=str(error))
            if not source_hashes[name]['passed']:errors.append('Changed protected source field: '+name)
    else:errors.append('Missing original city field hash evidence: '+str(source_evidence))
    installed_hash=meta.get('catalog_sha256')
    snapshot=project/meta.get('installed_catalog_snapshot','qa/installed-countryside-catalog.json')
    if not snapshot.exists()or json.loads(snapshot.read_text())!=catalog:errors.append('Installed catalog snapshot differs from requested catalog')
    aliases=meta.get('asset_aliases',{});stems={}
    for line in (project/'world.studio').read_text().splitlines():
        fields=line.split()
        if fields and fields[0]=='asset':stems[fields[1]]=fields[2]
    # Hash the actual installed original geometry roles. Shared immutable
    # inodes are read once; no decoder/texture/GPU allocations are involved.
    checked=0;hash_cache={}
    for asset in baseline['assets']:
        alias=aliases.get(asset['name']);stem=stems.get(alias)
        if not stem:errors.append('Missing installed source alias: '+asset['name']);continue
        if any(r['asset']==asset['name']for r in allowed_assets):asset=assets[asset['name']]
        paths=[project/'models'/(stem+'.glb'),project/'models'/(stem+'_mid.glb')]
        for role,(source,stats)in enumerate(zip(asset['files'],asset.get('stats',[]))):
            expected_hash=stats.get('sha256')
            if not expected_hash:expected_hash=digest(Path(source))
            path=paths[min(role,1)]
            if not path.exists():errors.append('Missing installed source model: '+str(path));continue
            stat=path.stat();key=(stat.st_dev,stat.st_ino,stat.st_size,stat.st_mtime_ns)
            if key not in hash_cache:hash_cache[key]=digest(path)
            if hash_cache[key]!=expected_hash:errors.append('Installed source bytes differ: '+str(path))
            checked+=1
    return dict(passed=not errors,errors=errors,baseline=str(baseline_path),baseline_sha256=digest(baseline_path),
                original_assets=len(baseline['assets']),original_placements=len(baseline['placements']),
                protected_publisher_and_other_assets=len(baseline['assets'])-len(allowed_assets),authorized_authored_asset_revisions=allowed_assets,
                unchanged_source_placements=len(baseline['placements'])-len(allowed),authorized_relocations=allowed,
                installed_source_model_roles_checked=checked,source_fields=source_hashes,installed_catalog_sha256=installed_hash)


def clearance(catalog,road):
    assets={a['name']:a for a in catalog['assets']}
    native_source=(ROOT/'engine/world_scenery.cpp').read_text()
    trunk_radii={}
    for line in native_source.splitlines():
        match=re.search(r'\{"([^"]+)".*Collision::Trunk,\s*([\d.]+)f',line)
        if match:trunk_radii[match[1]]=max(.3,float(match[2]))
    start,end=road[:-1],road[1:];segment=end-start;length2=(segment*segment).sum(1)
    minimum=float('inf');nearest=None;intersections=[];counts=collections.Counter()
    for index,placement in enumerate(catalog['placements']):
        asset=assets[placement['asset']];native=asset.get('native',{});alias=native.get('alias','')
        kind=native.get('collision',0)
        if not alias.startswith('bep_nature_')or kind not in [1,2,3]:continue
        bounds=[asset['bounds']]+[r['bounds']for r in asset.get('stats',[])if'bounds'in r]
        lo=np.min([b[0]for b in bounds],0);hi=np.max([b[1]for b in bounds],0)
        matrix=np.asarray(placement['matrix_native'],float).reshape(4,4).T.copy()
        matrix[:3,3]+=matrix[:3,:3]@np.asarray(asset['pivot_offset_native'])
        if kind==3:
            radius=trunk_radii.get(alias,.3);lo=np.array([-radius,0,-radius]);hi=np.array([radius,hi[1],radius])
        elif kind==2:
            radius=max(abs(lo[0]),abs(hi[0]),abs(lo[2]),abs(hi[2]));lo[[0,2]]=-radius;hi[[0,2]]=radius
        corners=np.array([[x,y,z,1]for x in [lo[0],hi[0]]for y in [lo[1],hi[1]]for z in [lo[2],hi[2]]])@matrix.T
        center=(corners[:,[0,2]].min(0)+corners[:,[0,2]].max(0))*.5
        # A circle containing every projected collider corner is conservative
        # for rotated boxes and tilted/scaled native trunk colliders.
        radius=float(np.linalg.norm(corners[:,[0,2]]-center,axis=1).max())
        t=np.clip(((center-start)*segment).sum(1)/np.maximum(length2,1e-30),0,1)
        distance=np.linalg.norm(center-(start+segment*t[:,None]),axis=1)
        segment_index=int(distance.argmin());gap=float(distance[segment_index]-radius-.37-.05)
        row=dict(index=index,asset=asset['name'],alias=alias,collision=kind,segment=segment_index,
                 conservative_enclosing_radius_m=radius,clearance_after_capsule_and_guard_m=gap)
        if gap<minimum:minimum=gap;nearest=row
        if gap<0:intersections.append(row)
        counts['trunks'if kind==3 else'rocks']+=1
    return dict(passed=not intersections,method='Exact 2D point-to-route-segment distance against circles enclosing native trunk/rock collider corners',
                player_capsule_radius_m=.37,extra_guard_m=.05,counts=dict(counts),route_segments=len(start),
                minimum_conservative_clearance_m=minimum if nearest else None,nearest=nearest,possible_intersections=intersections)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog',type=Path,required=True)
    parser.add_argument('--project',type=Path,default=ROOT/'projects/valwood_city_showcase')
    parser.add_argument('--review',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--checker',type=Path,default=Path('/tmp/valwood-city-countryside-collision-check'))
    parser.add_argument('--entry-route',type=Path,default=ROOT/'docs/village/2026-11-02/countryside-route/reviewed-opening.walk')
    parser.add_argument('--source-evidence',type=Path,default=ROOT/'docs/village/2026-11-02/countryside-route/runtime-support.json')
    args=parser.parse_args();project=args.project.resolve();out=args.output.resolve();catalog_path=args.catalog.resolve()
    if out==project or project in out.parents:raise ValueError('Report output must be outside the protected project')
    if not args.checker.is_file()or not os.access(args.checker,os.X_OK):raise ValueError('Native CPU collision checker is missing/not executable')
    out.mkdir(parents=True,exist_ok=True);catalog=json.loads(catalog_path.read_text());meta=json.loads((project/'city_showcase.json').read_text())
    review=(args.review or catalog_path.parent/'landscape-review.npz').resolve();arrays=np.load(review)
    road=np.asarray(arrays['road_native'],float)
    if road.ndim!=2 or road.shape[1]!=2 or len(road)<2 or len(road)>8192 or not np.isfinite(road).all():raise ValueError('Invalid bounded landscape route')
    origin=np.asarray(meta['origin'],float)[[0,2]];entry=read_entry(args.entry_route)
    selected=road[::12]
    if not np.array_equal(selected[-1],road[-1]):selected=np.concatenate((selected,road[-1:]))
    world=np.concatenate((entry,selected+origin));path=out/'countryside-loop.walk'
    path.write_text('VALWOOD_CITY_WALK 1\n'+''.join(f'p{i} {x:.14g} {z:.14g}\n'for i,(x,z)in enumerate(world)))
    primary=project/'ground/showcase.height';country=project/'ground/countryside.height';physics=project/'city.physics'
    command=[str(args.checker.resolve()),str(physics),str(primary),str(path),str(country)]
    result=subprocess.run(command,capture_output=True,text=True,timeout=120)
    (out/'native-collision.log').write_text(result.stdout+result.stderr)
    try:native=json.loads(result.stdout)
    except json.JSONDecodeError:native=dict(error='Checker returned no JSON',stdout=result.stdout)
    baseline=Path(catalog['opening']['unmodified_original_catalog']).resolve()
    protected=protected_city(catalog,project,meta,baseline,args.source_evidence)
    installed=digest(catalog_path)==meta.get('catalog_sha256')
    candidate_country=Path(catalog['countryside']['ground_files'][0]);field_matches=digest(candidate_country)==digest(country)
    road_check=clearance(catalog,world-origin)
    passed=result.returncode==0 and protected['passed']and road_check['passed']and installed and field_matches
    report=dict(kind='CPU-only installed city/countryside collision and source-preservation audit',passed=bool(passed),
                catalog=str(catalog_path),catalog_sha256=digest(catalog_path),project=str(project),
                landscape_review=str(review),landscape_review_sha256=digest(review),catalog_installed=installed,
                installed_country_matches_candidate=field_matches,
                route=dict(file=str(path),sha256=digest(path),points=len(world),distance_m=float(np.linalg.norm(np.diff(world,axis=0),axis=1).sum()),
                           source_points=len(road),sampling='Original validated3-point city exit + landscape road[::12] + exact final endpoint'),
                native=dict(passed=result.returncode==0,returncode=result.returncode,result=native,diagnostic=result.stderr,command=command),
                fields={str(p.relative_to(project)):digest(p)for p in [primary,country,physics]},
                protected_source=protected,new_scenery_clearance=road_check,
                scope='Full reviewed~398m loop on installed primary-priority/fallback fields and actual Jolt source UCX. Tree/rock footprint guard is conservative; not a gameplay, GPU, FPS or whole-world-navigation test.')
    (out/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps(dict(passed=bool(passed),report=str(out/'report.json'),native=native,
                         minimum_scenery_clearance_m=road_check['minimum_conservative_clearance_m'],
                         protection_errors=protected['errors'],catalog_installed=installed,country_field_matches=field_matches),indent=2))
    raise SystemExit(0 if passed else 1)


if __name__=='__main__':main()

#!/usr/bin/env python3
"""Verify an opening transport, its independent seed closure and exact patches.

Streams every added member, checks complete package/database/native metadata,
and proves unchanged original records remain bound to the verified Orientation
transport. Does not extract the base or launch Windows, Wine or the game.
"""
from pathlib import Path
import argparse
import hashlib
import json
import stat
import zipfile

from build_windows_opening_overlay import SCHEMA
from build_windows_playtest_overlay import BaseMetadata
from build_world_package import _native_hash
from install_windows_artist_bundle import canonical,checked_record,safe_names,regular,MAX_METADATA
from verify_windows_artist_bundle import require,verify_database,verify_native_dependencies


def verify(base_path,overlay_path,patches,verification):
    base_path=regular(base_path);overlay_path=regular(overlay_path)
    sidecar=Path(str(overlay_path)+'.sha256').read_text().split()
    require(sidecar==[_native_hash(overlay_path),overlay_path.name],'Opening SHA sidecar mismatch')
    with zipfile.ZipFile(base_path)as old,zipfile.ZipFile(overlay_path)as added:
        base=BaseMetadata(old,verification);names=added.namelist();safe_names(names)
        require(not set(names)&set(old.namelist()),'Opening overwrites an original transport member')
        payloads={}
        for info in added.infolist():
            require(not info.is_dir()and not info.flag_bits&1 and stat.S_IFMT(info.external_attr>>16)in(0,stat.S_IFREG),'Nonregular/encrypted opening member')
            require(info.file_size<=MAX_METADATA,'Oversized opening member')
            raw=added.read(info);require(len(raw)==info.file_size,'Opening member length mismatch');payloads[info.filename]=raw
        index=json.loads(payloads['opening-index.json'])
        require(index['schema']==SCHEMA and index['schema_version']==1,'Wrong opening schema')
        require(index['content_id']==hashlib.sha256(canonical({k:v for k,v in index.items()if k!='content_id'})).hexdigest(),'Opening content ID mismatch')
        binding=index['base_bundle']
        require(binding['content_id']==base.index['content_id']and binding['index_sha256']==hashlib.sha256(base.raw).hexdigest()and binding['zip_sha256']==verification['zip_sha256']and binding['zip_bytes']==base_path.stat().st_size,'Opening/base identity mismatch')
        contents={row['sha256']:checked_record(row)for row in index['contents']};require(len(contents)==len(index['contents']),'Duplicate content SHA')
        old_records={r['member']:r for r in base.index['contents']+base.index['bootstrap_files']};expected={}
        for row in index['contents']+index['bootstrap_files']:
            checked_record(row)
            if row['member']in old_records:
                previous=old_records[row['member']];require((row['bytes'],row['sha256'])==(previous['bytes'],previous['sha256']),'Original content identity changed')
            else:
                same=expected.setdefault(row['member'],row);require((same['bytes'],same['sha256'])==(row['bytes'],row['sha256']),'Conflicting added content')
        require(set(names)==set(expected)|{'opening-index.json'},'Missing/unexpected opening members')
        for name,row in expected.items():require(len(payloads[name])==row['bytes']and hashlib.sha256(payloads[name]).hexdigest()==row['sha256'],'Opening payload hash mismatch: '+name)
        def obj(sha):
            row=contents[sha]
            return json.loads(payloads[row['member']])if row['member']in payloads else base.object(sha)
        require([r['label']for r in index['projects']]==['opening']and len(patches['projects'])==1,'Opening-only project required')
        row=index['projects'][0];spec=patches['projects'][0];package=obj(row['manifest_sha256']);files={r['path']:r for r in package['files']};safe_names(files)
        require(package['content_id']==row['package_content_id']==hashlib.sha256(canonical(package['files'])).hexdigest(),'Opening package content ID mismatch')
        require(package['total_bytes']==row['materialized_bytes']==sum(r['bytes']for r in files.values())and len(files)==row['files'],'Opening package byte/count mismatch')
        for r in files.values():require(r['sha256']in contents and contents[r['sha256']]['bytes']==r['bytes']and r['mode']in('0644','0755'),'Missing/invalid opening package file')
        original=base.object(next(r for r in base.index['projects']if r['label']==spec['base_label'])['manifest_sha256']);before={r['path']:r for r in original['files']}
        require(set(files)==set(before)|set(spec['patches']),'Opening removed/added an unexpected seed path')
        changed=set(spec['patches'])|{'asset-database.json','native-dependencies.json','build-recipe.json'}
        unchanged=0
        for name,r in before.items():
            if name not in changed:require(files[name]==r,'Original seed changed: '+name);unchanged+=1
        for name,path in spec['patches'].items():require(files[name]['sha256']==_native_hash(regular(path))and files[name]['bytes']==Path(path).stat().st_size,'Explicit opening patch differs: '+name)
        database=verify_database(obj(files['asset-database.json']['sha256']),package,files)
        verify_native_dependencies(obj(files['native-dependencies.json']['sha256']),files)
        reachable={r['sha256']for r in files.values()}|{row['manifest_sha256']};require(reachable==set(contents),'Unreachable/missing indexed content')
        require(not any(n.startswith('project/saves/')for n in files),'Opening includes saved player state')
        required={'opening-index.json','Install_Opening_Bundle.py','Install_Opening.bat','READ_OPENING_FIRST.txt'};require(required<=set(names),'Opening installer/bootstrap missing')
        script=payloads['Install_Opening_Bundle.py'];compile(script,'Install_Opening_Bundle.py','exec')
        require(b'opening-index.json'in script and b'_Opening_Seed'in script and b"(stage/'project'/'saves').mkdir"in script,'Opening installer naming/private save hook missing')
        require(b'Install_Opening_Bundle.py'in payloads['Install_Opening.bat']and b'--project opening'in payloads['Install_Opening.bat'],'Opening batch invokes wrong installer/project')
        for n in ['Launch_Opening.bat','OPENING_GUIDE.md','project/opening.config','project/opening/home.settings','project/opening/home.physics','project/models/opening/home.glb','project/models/opening/waystone.glb']:require(n in files,'Required opening seed file missing: '+n)
        new_text_files=0
        for name,path in spec['patches'].items():
            if name.startswith('project/')and Path(name).suffix in {'.json','.config','.settings','.studio','.valwood','.txt','.physics','.views'}:
                raw=Path(path).read_bytes();require(b'/home/'not in raw and b'/tmp/'not in raw,'Linux absolute path in opening project patch: '+name);new_text_files+=1
    return dict(ok=True,verified=True,overlay=str(overlay_path),zip_bytes=overlay_path.stat().st_size,zip_sha256=sidecar[0],
                content_id=index['content_id'],base_content_id=base.index['content_id'],overlay_members=len(names),
                every_added_member_hashed=True,base_payloads_extracted=0,original_transport_members_overwritten=0,
                project=dict(label='opening',files=len(files),materialized_bytes=package['total_bytes'],unchanged_original_records=unchanged,database=database,new_project_text_files_without_linux_absolute_paths=new_text_files,private_saved_state_files=0),
                runtime_sha256=files['bin/rpg.exe']['sha256'],native_windows_acceptance='pending',
                scope='CPU transport integrity, complete database/native dependency closure, source patch identity and portable relative runtime layout; no Windows/Wine/GPU launch or full seed installation.')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--overlay',type=Path,required=True);p.add_argument('--patch-map',type=Path,required=True);p.add_argument('--base-verification',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    try:result=verify(a.base,a.overlay,json.loads(a.patch_map.read_bytes()),json.loads(a.base_verification.read_bytes()))
    except (OSError,ValueError,KeyError,TypeError,zipfile.BadZipFile)as e:p.exit(1,f'Opening verification failed: {e}\n')
    with a.output.open('x')as f:json.dump(result,f,indent=2);f.write('\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()

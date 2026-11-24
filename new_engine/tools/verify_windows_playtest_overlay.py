#!/usr/bin/env python3
"""Verify the small add-on and its closure against base metadata, without extracting assets."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys
import zipfile
import zlib

sys.dont_write_bytecode = True
from build_windows_playtest_overlay import BaseMetadata
from build_world_package import _native_hash
from install_windows_artist_bundle import canonical, checked_record, safe_names, regular, MAX_METADATA
from install_windows_playtest_bundle import SCHEMA
from verify_windows_artist_bundle import require, checked_json, verify_database, verify_native_dependencies, stamp


def runtime_route(label, case, specification):
    """Bind the tested section to its packaged route, including the first hall."""
    section = case['playtest_slice']['section']
    require(type(section) is int and section in ((0, 1) if label == 'grandstone' else (0,)),
            'Invalid tested playtest section: '+label)
    member = 'project/playtest-hall.route' if section else 'project/playtest.route'
    require(member in specification['patches'], 'Missing tested section route patch: '+member)
    route = Path(specification['patches'][member])
    count = [int(line.split()[1]) for line in route.read_text().splitlines() if line.startswith('count ')]
    require(count == [case['playtest_slice']['nodes']],
            'Route node count differs from actual runtime profile')
    return section, member, count[0]


def verify_overlay(base_path, overlay_path, patches, base_verification, opening=None, instructions=None):
    base_path, overlay_path = regular(base_path), regular(overlay_path)
    sidecar = Path(str(overlay_path)+'.sha256').read_text().split()
    require(len(sidecar)==2 and re.fullmatch('[0-9a-f]{64}',sidecar[0]) and sidecar[1]==overlay_path.name,'Invalid overlay SHA sidecar')
    zip_sha = _native_hash(overlay_path); require(zip_sha==sidecar[0],'Overlay ZIP SHA mismatch')
    specs = {row['label']:row for row in patches['projects']}; reports=[]; source_hashes={}
    if opening:
        require(opening.get('passed') is True and opening.get('source_state_unchanged') is True
                and opening.get('binary_unchanged') is True,'Runtime acceptance/source guards did not pass')
    with base_path.open('rb') as base_stream, overlay_path.open('rb') as overlay_stream:
        base_before, overlay_before = os.fstat(base_stream.fileno()),os.fstat(overlay_stream.fileno())
        require(base_before.st_size == base_verification['zip_bytes'],'Base ZIP size differs from its verification')
        with zipfile.ZipFile(base_stream) as base_zip, zipfile.ZipFile(overlay_stream) as archive:
            base=BaseMetadata(base_zip,base_verification); infos={row.filename:row for row in archive.infolist()}
            safe_names(row.filename for row in archive.infolist())
            require(not set(infos)&set(base_zip.namelist()),'Overlay overwrites an original transport member')
            for info in infos.values():
                require(not info.is_dir() and not info.flag_bits&1 and info.orig_filename==info.filename
                        and stat.S_IFMT(info.external_attr>>16) in (0,stat.S_IFREG),'Nonregular/unsafe overlay member')
            processed=set(); metadata={}; uncompressed=0
            def read(member,row=None,capture=False):
                nonlocal uncompressed
                require(member in infos and member not in processed,'Missing/repeated overlay member: '+member)
                info=infos[member]
                if row:
                    checked_record(row);require(info.file_size==row['bytes'],'Overlay member length mismatch')
                require(not capture or info.file_size<=MAX_METADATA,'Oversized overlay metadata')
                total=0;crc=0;h=hashlib.sha256();chunks=[]
                with archive.open(info) as stream:
                    while block:=stream.read(1024*1024):
                        total+=len(block);require(total<=info.file_size,'Inflated overlay length mismatch')
                        crc=zlib.crc32(block,crc);h.update(block)
                        if capture:chunks.append(block)
                require(total==info.file_size and crc==info.CRC,'Overlay member CRC/length mismatch')
                if row:require(h.hexdigest()==row['sha256'],'Overlay payload SHA mismatch')
                processed.add(member);uncompressed+=total
                if capture:metadata[member]=b''.join(chunks)
                return metadata.get(member)
            index=checked_json(read('playtest-index.json',capture=True),'playtest index')
            require(index.get('schema')==SCHEMA and index.get('schema_version')==1,'Invalid playtest overlay schema')
            require(index['content_id']==hashlib.sha256(canonical({k:v for k,v in index.items()if k!='content_id'})).hexdigest(),
                    'Playtest content ID mismatch')
            binding=index['base_bundle']
            require(binding['content_id']==base.index['content_id'] and binding['index_sha256']==hashlib.sha256(base.raw).hexdigest()
                    and binding['zip_sha256']==base_verification['zip_sha256'] and binding['zip_bytes']==base_before.st_size,
                    'Original bundle anchor mismatch')
            contents={row['sha256']:checked_record(row) for row in index['contents']}
            require(len(contents)==len(index['contents']),'Duplicate playtest content identity')
            expected={}
            for row in index['contents']+index['bootstrap_files']:
                if row['member'] in base_zip.namelist():
                    matches=[old for old in base.index['contents']+base.index['bootstrap_files']if old['member']==row['member']]
                    require(matches and all((old['sha256'],old['bytes'])==(row['sha256'],row['bytes'])for old in matches),
                            'Changed original transport reference')
                else:
                    old=expected.setdefault(row['member'],row)
                    require((old['sha256'],old['bytes'])==(row['sha256'],row['bytes']),'Conflicting overlay member identity')
            safe_names(expected)
            require(set(infos)==set(expected)|{'playtest-index.json'},'Unexpected/missing overlay ZIP members')
            def object_(sha):
                row=contents[sha]
                if row['member'] in infos:
                    if row['member'] not in metadata:read(row['member'],row,True)
                    return checked_json(metadata[row['member']],row['member'])
                return base.object(sha)
            labels=[row['label']for row in index['projects']]
            require(len(labels)==len(set(labels)) and set(labels)==set(specs),'Patch-map/project label mismatch')
            base_projects={row['label']:row for row in base.index['projects']}
            reachable=set()
            for project in index['projects']:
                label=project['label'];spec=specs[label];package=object_(project['manifest_sha256'])
                require(package['schema']=='valwood.windows-package' and package['schema_version']==1,'Invalid playtest package manifest')
                require(package['project_id']==project['project_id'],'Index/package project identity mismatch')
                files=package['files'];safe_names([row['path']for row in files]+['package-manifest.json'])
                entries={row['path']:checked_record(row)for row in files}
                require(hashlib.sha256(canonical(files)).hexdigest()==package['content_id']==project['package_content_id'],
                        'Playtest package content ID mismatch')
                require(package['total_bytes']==project['materialized_bytes']==sum(row['bytes']for row in files)
                        and project['files']==len(files),'Playtest package byte/count mismatch')
                for row in files:require(contents[row['sha256']]['bytes']==row['bytes'] and row['mode']in ('0644','0755'),'Invalid package content/length/mode')
                original=base.object(base_projects[spec['base_label']]['manifest_sha256']);old={row['path']:row for row in original['files']}
                require(set(entries)==set(old)|set(spec['patches']),'Unexpected/removal of original seed paths')
                generated={'asset-database.json','native-dependencies.json','build-recipe.json'}
                unchanged=0
                for name,row in old.items():
                    if name not in set(spec['patches'])|generated:
                        require(entries[name]==row,'Changed unpatched original seed file: '+name);unchanged+=1
                for name,path in spec['patches'].items():
                    path=Path(path);key=str(path.resolve())
                    if key not in source_hashes:source_hashes[key]=(_native_hash(path),path.stat().st_size)
                    require((entries[name]['sha256'],entries[name]['bytes'])==source_hashes[key],'Patch source/transport mismatch: '+name)
                database=object_(entries['asset-database.json']['sha256'])
                dbcheck=verify_database(database,package,entries)
                verify_native_dependencies(object_(entries['native-dependencies.json']['sha256']),entries)
                profile=None
                if opening:
                    matches=[row for row in opening['cases']if row['area']==label]
                    require(len(matches)==1 and matches[0]['passed'] is True,'Missing successful Windows runtime case')
                    require(entries['bin/rpg.exe']['sha256']==opening['binary_sha256'],'Runtime differs from tested Windows executable')
                    section,route_member,route_nodes=runtime_route(label,matches[0],spec)
                    command_path=Path(opening['_path']).parent/(label+'-command.json')
                    command=json.loads(command_path.read_text())['command'];project_path=command[command.index('--project')+1]
                    if project_path.startswith('Z:'):project_path=project_path[2:].replace('\\','/')
                    for name in spec['patches']:
                        if name.startswith('project/'):
                            live=Path(project_path)/name[8:]
                            require(_native_hash(live)==entries[name]['sha256'],'Patch differs from tested project data: '+name)
                    profile=dict(runtime_sha256=opening['binary_sha256'],route_nodes=route_nodes,
                                 section=section,route_member=route_member,acceptance_mode=opening.get('mode','opening'),tested_project=project_path,
                                 all_patch_bytes_match_current_profile_project=True,platform=opening['platform'],
                                 data_binding_scope='Historical runtime acceptance profiles record node counts, not per-file hashes; exact patch hashes are compared with the current private project at the recorded command path.')
                reports.append(dict(label=label,files=len(files),materialized_bytes=package['total_bytes'],
                    unchanged_original_records=unchanged,explicit_patch_paths=sorted(spec['patches']),database=dbcheck,opening_profile=profile))
                reachable.update(row['sha256']for row in files);reachable.add(project['manifest_sha256'])
            require(reachable==set(contents),'Unreachable/missing indexed content')
            for member,row in expected.items():
                if member not in processed:read(member,row,True if member.startswith('Install_Playtest')or member=='READ_PLAYTEST_FIRST.txt' else False)
            require(processed==set(infos),'Unverified overlay ZIP member')
            required={'playtest-index.json','Install_Playtest_Bundle.py'}|{'Install_Playtest_'+label+'.bat'for label in specs}
            require(required<=set(infos),'Missing playtest installer/index/batch')
            for label in specs:
                raw=metadata['Install_Playtest_'+label+'.bat']
                require(b'Install_Playtest_Bundle.py' in raw and ('--project '+label).encode() in raw,'Install batch points at wrong installer/project')
            require(b'_Playtest_Seed' in metadata['Install_Playtest_Bundle.py'],'Wrong default seed naming')
            if instructions:
                guide=Path(instructions).read_text()
                require(all(name in guide for name in required),'Root playtest guide filenames differ from actual add-on')
            require(stamp(base_before)==stamp(os.fstat(base_stream.fileno())) and stamp(overlay_before)==stamp(os.fstat(overlay_stream.fileno())),
                    'Base or add-on archive changed during verification')
    return dict(ok=True,verified=True,overlay=str(overlay_path),zip_bytes=overlay_before.st_size,zip_sha256=zip_sha,
        verifier_sha256=_native_hash(Path(__file__).resolve()),
        content_id=index['content_id'],base_content_id=base.index['content_id'],overlay_members=len(processed),
        each_overlay_member_streamed_once=True,streamed_overlay_bytes=uncompressed,base_payloads_extracted=0,
        base_rehashed=False,original_transport_members_overwritten=0,projects=reports,
        scope='Small add-on integrity, complete manifest/database closure, unchanged base records and current tested runtime/data; no full base extraction/rehash or native game launch.')


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base',type=Path,required=True);parser.add_argument('--overlay',type=Path,required=True)
    parser.add_argument('--patch-map',type=Path,required=True);parser.add_argument('--base-verification',type=Path,required=True)
    parser.add_argument('--opening-results',type=Path,help='Accepted Windows opening or combat run results')
    parser.add_argument('--instructions',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(argv)
    try:
        opening=checked_json(args.opening_results.read_bytes(),'opening results')if args.opening_results else None
        if opening:opening['_path']=str(args.opening_results.resolve())
        report=verify_overlay(args.base,args.overlay,checked_json(args.patch_map.read_bytes(),'patch map'),
            checked_json(args.base_verification.read_bytes(),'base verification'),opening,args.instructions)
        with args.output.open('x')as stream:json.dump(report,stream,indent=2);stream.write('\n')
        print(json.dumps(report,indent=2));return 0
    except (OSError,ValueError,KeyError,TypeError,zipfile.BadZipFile)as error:
        print(json.dumps(dict(ok=False,error=str(error)),indent=2));return 1


if __name__=='__main__':raise SystemExit(main())

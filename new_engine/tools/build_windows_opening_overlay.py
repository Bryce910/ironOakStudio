#!/usr/bin/env python3
"""Build a separately named opening add-on for the original Orientation ZIP.

The existing verified playtest builder supplies the complete independent city
seed, content deduplication, native dependency checks and asset database. This
wrapper gives the opening its own installer/index names so it can coexist with
the older transport add-ons without replacing their bootstrap files.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import os
import shutil
import tempfile
import zipfile

from build_windows_playtest_overlay import build_overlay, zip_info
from build_world_package import _native_hash, _publish_directory
from install_windows_artist_bundle import canonical, regular, safe_names

SCHEMA='valwood.windows-opening-bundle'


def installer_bytes(raw):
    text=raw.decode('utf-8')
    if text.count("SCHEMA = 'valwood.windows-playtest-bundle'")!=1:raise ValueError('Unexpected source installer schema')
    text=text.replace("SCHEMA = 'valwood.windows-playtest-bundle'",f"SCHEMA = '{SCHEMA}'")
    text=text.replace('playtest-index.json','opening-index.json').replace('_Playtest_Seed','_Opening_Seed')
    anchor="        with (stage/'package-manifest.json').open('xb') as stream:"
    if text.count(anchor)!=1:raise ValueError('Unexpected source installer save-directory hook')
    text=text.replace(anchor,"        (stage/'project'/'saves').mkdir(parents=True,exist_ok=True)\n"+anchor)
    return text.encode('utf-8')


def install_batch():
    return ('@echo off\r\nsetlocal\r\nset "PYTHONDONTWRITEBYTECODE=1"\r\n'
        '"%~dp0bin\\python\\python.exe" -B "%~dp0Install_Opening_Bundle.py" --bundle-root "%~dp0." --project opening %*\r\n'
        'set "result=%errorlevel%"\r\nif not "%result%"=="0" echo Opening installation failed. Existing folders were not replaced.\r\n'
        'pause\r\nexit /b %result%\r\n').encode()


def build(base,patches,verification,readme,output):
    output=Path(output).absolute();sidecar=Path(str(output)+'.sha256')
    if output.exists()or sidecar.exists()or output.is_symlink():raise ValueError('Choose NEW output and SHA paths')
    if output.suffix.lower()!='.zip':raise ValueError('Output must be a ZIP')
    regular(output.parent)
    if [p['label']for p in patches['projects']]!=['opening']:raise ValueError('Opening-only patch map required')
    stage=Path(tempfile.mkdtemp(prefix='.valwood-opening-transport-',dir=output.parent))
    try:
        intermediate=stage/'inner-playtest.zip'
        inner=build_overlay(base,patches,intermediate,verification)
        with zipfile.ZipFile(intermediate)as source:
            index=json.loads(source.read('playtest-index.json'));index['schema']=SCHEMA
            replacements={
                'Install_Playtest_Bundle.py':('Install_Opening_Bundle.py',installer_bytes(source.read('Install_Playtest_Bundle.py'))),
                'Install_Playtest_opening.bat':('Install_Opening.bat',install_batch()),
                'READ_PLAYTEST_FIRST.txt':('READ_OPENING_FIRST.txt',Path(readme).read_bytes()),
            }
            bootstrap=[]
            for row in index['bootstrap_files']:
                if row['member']in replacements:
                    name,raw=replacements[row['member']]
                    bootstrap.append(dict(member=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
                else:bootstrap.append(copy.deepcopy(row))
            index['bootstrap_files']=bootstrap
            index['materialization']='NEW independent ordinary-file opening seed; private empty saves; original Orientation and prior add-ons preserved'
            index['opening_transport_builder']=dict(name=Path(__file__).name,sha256=_native_hash(Path(__file__)))
            index.pop('content_id');index['content_id']=hashlib.sha256(canonical(index)).hexdigest()
            names=[replacements.get(n,(n,None))[0]for n in source.namelist()if n!='playtest-index.json']+['opening-index.json']
            safe_names(names)
            pending=stage/'opening.zip'
            with zipfile.ZipFile(pending,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=3,allowZip64=True)as target:
                for old in source.namelist():
                    if old=='playtest-index.json':continue
                    if old in replacements:
                        name,raw=replacements[old];target.writestr(zip_info(name),raw)
                    else:
                        with source.open(old)as src,target.open(zip_info(old),'w',force_zip64=True)as dst:shutil.copyfileobj(src,dst,1024*1024)
                target.writestr(zip_info('opening-index.json'),canonical(index))
        _publish_directory(pending,output)
        digest=_native_hash(output)
        with sidecar.open('x',encoding='ascii')as f:f.write(digest+'  '+output.name+'\n')
        result=dict(ok=True,output=str(output),zip_bytes=output.stat().st_size,zip_sha256=digest,
                    content_id=index['content_id'],base_content_id=index['base_bundle']['content_id'],
                    projects=index['projects'],new_payloads=inner['new_payloads'],new_payload_bytes=inner['new_payload_bytes'],
                    reused_base_payloads=inner['reused_base_payloads'],base_payloads_extracted=0,
                    installer='Install_Opening_Bundle.py',index='opening-index.json',native_windows_acceptance='pending')
        return result
    finally:shutil.rmtree(stage)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--base-verification',type=Path,required=True);p.add_argument('--patch-map',type=Path,required=True);p.add_argument('--readme',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    try:result=build(a.base,json.loads(a.patch_map.read_bytes()),json.loads(a.base_verification.read_bytes()),a.readme,a.output)
    except (OSError,ValueError,KeyError,TypeError,zipfile.BadZipFile)as e:p.exit(1,f'Opening transport failed: {e}\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()

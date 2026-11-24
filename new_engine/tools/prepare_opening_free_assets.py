#!/usr/bin/env python3
"""Stage three selected CC0 assets for the apartment art pass.

Fetches only official Poly Haven asset metadata and the selected source files.
Never edits a project, runtime model, collider, shader or player save.
"""
from pathlib import Path, PurePosixPath
import argparse
import copy
import datetime
import hashlib
import json
import shutil
import struct
import tempfile
import urllib.parse
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
USER_AGENT='ValwoodManualAssetPrep/1.0'


def get(url,limit=16*1024*1024):
    parts=urllib.parse.urlsplit(url)
    if parts.scheme!='https' or parts.hostname not in {'api.polyhaven.com','dl.polyhaven.org'}:
        raise ValueError('Download must use an official asset host')
    request=urllib.request.Request(url,headers={'User-Agent':USER_AGENT})
    with urllib.request.urlopen(request,timeout=45) as response:
        final=urllib.parse.urlsplit(response.url)
        if final.scheme!='https' or final.hostname not in {'api.polyhaven.com','dl.polyhaven.org'}:
            raise ValueError('Unexpected download redirect')
        data=response.read(limit+1)
    if len(data)>limit:raise ValueError('Selected source file exceeds download budget')
    return data


def checked_source(root,name,descriptor):
    path=PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or '\\' in name:
        raise ValueError('Unsafe relative asset path')
    data=get(descriptor['url'])
    if len(data)!=descriptor['size'] or hashlib.md5(data).hexdigest()!=descriptor['md5']:
        raise ValueError('Source metadata hash/size mismatch: '+name)
    destination=root.joinpath(*path.parts);destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(data)
    return dict(file=name,source_url=descriptor['url'],bytes=len(data),
                source_md5=descriptor['md5'],sha256=hashlib.sha256(data).hexdigest())


def local_data(root,uri):
    if not isinstance(uri,str) or ':' in uri or '\\' in uri:
        raise ValueError('Only local asset dependencies are accepted')
    relative=PurePosixPath(urllib.parse.unquote(uri))
    if relative.is_absolute() or '..' in relative.parts:raise ValueError('Unsafe asset dependency')
    return root.joinpath(*relative.parts).read_bytes()


def pack_gltf(path,output):
    doc=json.loads(path.read_text());doc=copy.deepcopy(doc);binary=bytearray();offsets=[]
    def append(data):
        binary.extend(b'\0'*(-len(binary)%4));offset=len(binary);binary.extend(data);return offset
    for buffer in doc['buffers']:
        data=local_data(path.parent,buffer['uri'])
        if len(data)!=buffer['byteLength']:raise ValueError('Model buffer length mismatch')
        offsets.append(append(data))
    for view in doc.get('bufferViews',[]):
        view['byteOffset']=offsets[view['buffer']]+view.get('byteOffset',0);view['buffer']=0
    for image in doc.get('images',[]):
        if 'uri' not in image:continue
        uri=image.pop('uri');data=local_data(path.parent,uri);offset=append(data)
        doc.setdefault('bufferViews',[]).append(dict(buffer=0,byteOffset=offset,byteLength=len(data)))
        image['bufferView']=len(doc['bufferViews'])-1
        image['mimeType']='image/png' if data[:8]==b'\x89PNG\r\n\x1a\n' else 'image/jpeg'
    doc['buffers']=[dict(byteLength=len(binary))]
    metadata=json.dumps(doc,separators=(',',':')).encode();metadata+=b' '*(-len(metadata)%4)
    binary.extend(b'\0'*(-len(binary)%4))
    raw=struct.pack('<III',0x46546c67,2,28+len(metadata)+len(binary))+struct.pack('<II',len(metadata),0x4e4f534a)+metadata+struct.pack('<II',len(binary),0x004e4942)+binary
    output.write_bytes(raw)
    return dict(file=output.name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),
                conversion='Embedded original model buffers and images; geometry, node transforms and materials unchanged')


def prepare(output):
    output=Path(output).resolve()
    if output.exists():raise ValueError('Use a new staging directory')
    output.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.opening-free-',dir=output.parent))
    try:
        assets=[]
        for asset,resolution in [('small_wooden_table_01','2k'),('stationery_supplies','1k'),('wood_table_worn','2k')]:
            folder=stage/asset;folder.mkdir();source=folder/'source';source.mkdir()
            metadata=json.loads(get('https://api.polyhaven.com/files/'+asset))
            info=json.loads(get('https://api.polyhaven.com/info/'+asset))
            (folder/'download-metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
            (folder/'asset-info.json').write_text(json.dumps(info,indent=2)+'\n')
            records=[];packed=None
            if asset!='wood_table_worn':
                descriptor=metadata['gltf'][resolution]['gltf'];name=Path(urllib.parse.urlsplit(descriptor['url']).path).name
                records.append(checked_source(source,name,descriptor))
                for relative,entry in descriptor['include'].items():records.append(checked_source(source,relative,entry))
                packed=pack_gltf(source/name,folder/(asset+'.glb'))
            else:
                for key in ['Diffuse','nor_gl','arm']:
                    descriptor=metadata[key][resolution]['jpg'];name=Path(urllib.parse.urlsplit(descriptor['url']).path).name
                    records.append(checked_source(source,name,descriptor))
            provenance=dict(asset_id=asset,name=info.get('name',asset),source_page='https://polyhaven.com/a/'+asset,
                license='CC0-1.0',license_url='https://polyhaven.com/license',resolution=resolution,
                authors=info.get('authors'),source_files=records,packed_model=packed,
                status='Downloaded and verified; staged for art integration, not placed in the running game')
            (folder/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n');assets.append(provenance)
        manifest=dict(schema='valwood.opening-free-asset-staging',version=1,prepared_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            passed=True,cost_usd=0,source='Poly Haven',credit='Powered by Poly Haven',
            api_terms='https://github.com/Poly-Haven/Public-API/blob/master/ToS.md',
            license_url='https://polyhaven.com/license',assets=assets,
            selection='Two modest game props and one worn surface; 1K/2K files selected, no high-resolution archive or duplicate existing models downloaded',
            game_projects_changed=False)
        (stage/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        stage.rename(output)
        return dict(passed=True,output=str(output),assets=len(assets),cost_usd=0,
                    downloaded_bytes=sum(f['bytes'] for a in assets for f in a['source_files']))
    finally:
        if stage.exists():shutil.rmtree(stage)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'assets/polyhaven/opening_upgrade/2026-11-03')
    print(json.dumps(prepare(parser.parse_args().output),indent=2))

#!/usr/bin/env python3
"""Repair only the generated zero-area datum's material; all BIN bytes frozen."""
from pathlib import Path
import argparse,copy,hashlib,json,struct
ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/medieval_village/publisher-source/catalog.json')
    ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/medieval_village/publisher-source-datum')
    a=ap.parse_args();original=json.loads(a.catalog.read_text());result=copy.deepcopy(original);repairs=[];(a.output/'models').mkdir(parents=True,exist_ok=True)
    for asset in result['assets']:
        for lod,filename in enumerate(asset['files']):
            path=Path(filename);blob=path.read_bytes();jlen=struct.unpack_from('<I',blob,12)[0];g=json.loads(blob[20:20+jlen]);binary_chunk=blob[20+jlen:]
            primitive=g['meshes'][0]['primitives'][-1];mat=g['materials'][primitive['material']]
            if not mat.get('extensions',{}).get('VALWOOD_materials_facade'):continue
            pos=g['accessors'][primitive['attributes']['POSITION']]
            if pos['count']!=3 or pos.get('min')!=[0.,0.,0.]or pos.get('max')!=[0.,0.,0.]:raise ValueError(('Expected exact zero-area source datum',path))
            primitive['material']=len(g['materials']);g['materials'].append(dict(name='Source_Pivot_Datum',pbrMetallicRoughness=dict(baseColorFactor=[0,0,0,1],metallicFactor=0,roughnessFactor=1)))
            encoded=json.dumps(g,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
            out=struct.pack('<III',0x46546c67,2,20+len(encoded)+len(binary_chunk))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+binary_chunk
            target=a.output/'models'/path.name;target.write_bytes(out)
            if target.read_bytes()[20+len(encoded):]!=binary_chunk:raise ValueError('Source binary payload changed')
            asset['files'][lod]=str(target.resolve());asset['stats'][lod]['bytes']=len(out);asset['stats'][lod]['sha256']=hashlib.sha256(out).hexdigest()
            repairs.append(dict(asset=asset['name'],lod=lod,source=str(path),output=str(target.resolve()),original_sha256=hashlib.sha256(blob).hexdigest(),repaired_sha256=hashlib.sha256(out).hexdigest(),
                binary_chunk_sha256=hashlib.sha256(binary_chunk).hexdigest(),binary_unchanged=True,real_primitives_unchanged=True,images_unchanged=True))
    result['generated_datum_repairs']=repairs;result['previous_catalog']=str(a.catalog.resolve());result['previous_catalog_sha256']=hashlib.sha256(a.catalog.read_bytes()).hexdigest()
    (a.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print('READY',a.output/'catalog.json','repaired',len(repairs),'GLBs; all source BIN/image/real primitive bytes unchanged')
if __name__=='__main__':main()

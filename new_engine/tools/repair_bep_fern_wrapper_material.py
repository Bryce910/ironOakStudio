#!/usr/bin/env python3
"""Stage only the verified Fern_01 MID wrapper material correction.

Existing catalogs/GLBs/projects are read-only. The new full vegetation catalog
reuses every non-fern record byte-for-byte semantically. Geometry streams and
all embedded texture payloads are checked against the original fern files.
"""
from pathlib import Path
import copy,json,struct,hashlib
import numpy as np
from cook_bep_reforged_vegetation import Cook,ROOT,sha

BASE=ROOT/'assets/purchased/bep_reforged/countryside-vegetation'
OUT=ROOT/'assets/purchased/bep_reforged/countryside-fern-binding'

def read(path):
    b=Path(path).read_bytes();n=struct.unpack_from('<I',b,12)[0];return json.loads(b[20:20+n]),b[28+n:]
def accessor(g,b,i):
    a=g['accessors'][i];v=g['bufferViews'][a['bufferView']];n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];size=4*n*a['count'];start=v.get('byteOffset',0)+a.get('byteOffset',0);return b[start:start+size]
def verify(before,after):
    old,ob=read(before);new,nb=read(after);assert len(old['meshes'][0]['primitives'])==len(new['meshes'][0]['primitives'])
    for p,q in zip(old['meshes'][0]['primitives'],new['meshes'][0]['primitives']):
        assert set(p['attributes'])==set(q['attributes'])
        for key in p['attributes']:assert accessor(old,ob,p['attributes'][key])==accessor(new,nb,q['attributes'][key]),key
        assert accessor(old,ob,p['indices'])==accessor(new,nb,q['indices'])
    def images(g,b):
        result=[]
        for image in g['images']:
            v=g['bufferViews'][image['bufferView']];start=v.get('byteOffset',0);result.append(hashlib.sha256(b[start:start+v['byteLength']]).hexdigest())
        return sorted(result)
    assert images(old,ob)==images(new,nb),'Texture payload changed'
    return dict(attributes_and_indices_bitwise_identical=True,embedded_image_payloads_identical=True)

def main():
    c=Cook(OUT);original=json.loads((BASE/'catalog.json').read_text());catalog=copy.deepcopy(original);asset=next(a for a in catalog['assets']if a['native']['alias']=='bep_nature_fern');old_files=asset['files'][:]
    miny=asset['pivot_offset_native'][1];files=[];stats=[];checks=[]
    for i,guid in enumerate(asset['source_guids']):
        v,parts=c.geometry(guid);path=OUT/'models'/('bep_nature_fern_'+('near'if i==0 else'mid')+'.glb');temp=path.with_suffix('.glb.tmp')
        info=c.emit(temp,v,parts,miny);temp.replace(path);stats.append(info);files.append(str(path.resolve()));checks.append(verify(old_files[i],path))
        g,b=read(path);material=g['materials'][0]
        assert material['name']=='19d1542897ce4ab488b51ff52e215ced:6880142849598374712'
        assert abs(material['alphaCutoff']-.3630000054836273)<1e-12
        assert np.allclose(material['pbrMetallicRoughness']['baseColorFactor'],[.7615291921606779]*3+[1],atol=0,rtol=0)
    asset['files']=files;asset['stats']=stats;asset['material_binding']='Actual Fern_01 LODGroup wrapper: all tiers use HIGH leaf material (same cutoff/tint).'
    before={a['name']:a for a in original['assets']if a['native']['alias']!='bep_nature_fern'};after={a['name']:a for a in catalog['assets']if a['native']['alias']!='bep_nature_fern'};assert before==after
    catalog['revision']='Fern MID material corrected from exact source wrapper; other assets unchanged'
    (OUT/'catalog.json').write_text(json.dumps(catalog,indent=2,allow_nan=False)+'\n')
    (OUT/'materials.json').write_text(json.dumps(c.rows,indent=2)+'\n')
    audit=dict(source_catalog=str((BASE/'catalog.json').resolve()),source_catalog_sha256=sha(BASE/'catalog.json'),geometry=c.geometry_audit,materials=c.texture_audit,geometry_image_checks=checks,
        non_fern_asset_records_unchanged=True,additional_texture_GPU_bytes=0,change='MID cutoff .5→.363; linear material tint1→.7615291922, exactly matching HIGH material bound by the original wrapper. No geometry, source AO, texture image, shader or project changes.')
    (OUT/'source-audit.json').write_text(json.dumps(audit,indent=2,allow_nan=False)+'\n')
    print('FERN READY',OUT/'catalog.json',sha(OUT/'catalog.json'),flush=True)
    for file in files:print(file,sha(file),flush=True)

if __name__=='__main__':main()

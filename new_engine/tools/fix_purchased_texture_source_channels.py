#!/usr/bin/env python3
"""Publish canonical TextureSource ground and image-only near facade masks."""
import copy,hashlib,json,os,shutil
from pathlib import Path
import numpy as np
from PIL import Image
from build_purchased_village import ROOT,SOURCE,OUT,glb,encode_glb,inspect_glb,prepare,validate_from_rows,sha
from build_purchased_village_facades import HOUSES,compact

def main():
    audit=json.loads((SOURCE/'unreal-material-source-audit.json').read_text())
    assert len(audit['recovered_textures'])==9
    for row in audit['recovered_textures']:
        assert row['storage_format']=='TSF_BGRA8'
        assert sha(SOURCE/row['output'])==row['canonical_png_sha256']==row['png_sha256']
        assert sha(SOURCE/row['storage_png'])==row['storage_png_sha256']
        raw=np.asarray(Image.open(SOURCE/row['storage_png']).convert('RGBA'))
        canonical=np.asarray(Image.open(SOURCE/row['output']).convert('RGBA'))
        assert np.array_equal(canonical,raw[:,:,[2,1,0,3]])
    provenance_path=SOURCE/'PROVENANCE.json';previous=json.loads(provenance_path.read_text())
    prepare()
    provenance=json.loads(provenance_path.read_text())
    if 'current_near_house_representation' in previous:provenance['current_near_house_representation']=previous['current_near_house_representation']
    provenance['texture_source_channel_normalization']='TSF_BGRA8 internal PNG storage normalized exactly once to canonical RGBA; original Downloads unchanged'
    provenance_path.write_text(json.dumps(provenance,indent=2)+'\n')
    backup=SOURCE/'source-facades'/'before-channel-correction';backup.mkdir(parents=True,exist_ok=True)
    manifest_path=OUT/'purchased_village_manifest.json';manifest=json.loads(manifest_path.read_text())
    if not (backup/manifest_path.name).exists():shutil.copy2(manifest_path,backup/manifest_path.name)
    proof=[]
    for family in HOUSES:
        name='purchased_village_'+family+'_near';path=OUT/(name+'.glb')
        if not (backup/path.name).exists():shutil.copy2(path,backup/path.name)
        doc,binary=glb(path);old_doc=copy.deepcopy(doc);old_binary=binary;binary=bytearray(binary);changes=[]
        for image in doc['images']:
            if image.get('name') not in {'T_RGB_Mask','T_Tileable_Wall_Plaster_01_RGB_Mask'}:continue
            source=SOURCE/'prepared'/(image['name']+'.png');encoded=source.read_bytes()
            view=doc['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0)
            before=bytes(binary[offset:offset+view['byteLength']]);before_hash=hashlib.sha256(before).hexdigest()
            import io
            old=np.asarray(Image.open(io.BytesIO(before)).convert('RGBA'));new=np.asarray(Image.open(io.BytesIO(encoded)).convert('RGBA'))
            assert np.array_equal(new,old[:,:,[2,1,0,3]]) or np.array_equal(new,old)
            binary.extend(b'\0'*((-len(binary))%4));view['byteOffset']=len(binary);view['byteLength']=len(encoded);binary.extend(encoded)
            changes.append(dict(image=image['name'],before_sha256=before_hash,after_sha256=sha(source),dimensions=list(Image.open(source).size)))
        assert changes,(family,'Source mask image absent')
        doc,binary=compact(doc,binary)
        for old_mesh,new_mesh in zip(old_doc['meshes'],doc['meshes']):
            for old_p,new_p in zip(old_mesh['primitives'],new_mesh['primitives']):
                assert old_p['material']==new_p['material'] and old_p['attributes'].keys()==new_p['attributes'].keys()
                for old_i,new_i in [(old_p['indices'],new_p['indices']),*[(old_p['attributes'][k],new_p['attributes'][k]) for k in old_p['attributes']]]:
                    old_a,new_a=old_doc['accessors'][old_i],doc['accessors'][new_i]
                    old_v,new_v=old_doc['bufferViews'][old_a['bufferView']],doc['bufferViews'][new_a['bufferView']]
                    oo,no=old_v.get('byteOffset',0),new_v.get('byteOffset',0)
                    assert old_binary[oo:oo+old_v['byteLength']]==binary[no:no+new_v['byteLength']]
        assert doc['materials']==old_doc['materials'] and doc['textures']==old_doc['textures']
        temp=backup/(path.name+'.tmp');temp.write_bytes(encode_glb(doc,binary));entry=inspect_glb(temp)
        row=next(r for r in manifest['assets'] if r['asset']==name)
        assert entry['triangles']==row['triangles'] and entry['bounds_y_up']==row['bounds_y_up'] and entry['primitive_count']==row['primitive_count']
        row.update(entry);row['source_facade']['texture_source_mask_channels']='Canonical RGBA from TSF_BGRA8 storage; red/blue corrected, green unchanged'
        os.replace(temp,path)
        proof.append(dict(asset=name,changes=changes,all_vertex_index_and_paint_bytes_exact=True,material_and_sampler_parameters_exact=True))
    before=json.loads((backup/manifest_path.name).read_text());old_rows={r['asset']:r for r in before['assets']}
    for row in manifest['assets']:
        if row['lod']=='mid':assert row==old_rows[row['asset']] and sha(OUT/(row['asset']+'.glb'))==row['sha256']
    manifest['gpu_rgba8_mip_bytes']=validate_from_rows(manifest);manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
    ground_path=ROOT/'projects/valwood_world/ground/village_material_provenance.json';ground=json.loads(ground_path.read_text())
    if not (backup/ground_path.name).exists():shutil.copy2(ground_path,backup/ground_path.name)
    for row in ground['maps']:
        source=ROOT/row['source'];target=ROOT/row['runtime']
        record=next(r for r in audit['recovered_textures'] if r['output']==str(source.relative_to(SOURCE)))
        if not (backup/target.name).exists():shutil.copy2(target,backup/target.name)
        temp=target.with_suffix('.tmp.png');shutil.copyfile(source,temp);os.replace(temp,target)
        assert sha(source)==sha(target)==record['canonical_png_sha256']
        row.update(sha256=sha(target),canonical_png_sha256=record['canonical_png_sha256'],storage_format=record['storage_format'],storage_png=str((SOURCE/record['storage_png']).relative_to(ROOT)),storage_png_sha256=record['storage_png_sha256'],channel_normalization=record['channel_normalization'])
    ground['packing']='Canonical AO/R, roughness/G, metallic/B; ground master metallic is constant0'
    ground['texture_source_storage']='TSF_BGRA8 internal PNG normalized to canonical RGBA exactly once'
    ground_path.write_text(json.dumps(ground,indent=2)+'\n')
    report=dict(schema=1,raw_and_canonical_pngs_verified=9,original_downloads_modified=False,all_mid_files_and_materials_exact=True,architecture_gpu_mib=manifest['gpu_rgba8_mip_bytes']/1024/1024,near_image_only_changes=proof,ground_runtime_bytes_match_canonical_source=True,
        remaining_limit='Historical mid baked plaster normal/ARM blend used the earlier swapped recovered mask; mid geometry/images intentionally unchanged. Source-UV near plaster now samples correct canonical blue mask.')
    (SOURCE/'source-facades/channel-correction-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PUBLISHED canonical ground3maps + source near mask image corrections;9raw/canonical PNGs verified; all geometry/mids exact;',report['architecture_gpu_mib'],'MiB',flush=True)

if __name__=='__main__':main()

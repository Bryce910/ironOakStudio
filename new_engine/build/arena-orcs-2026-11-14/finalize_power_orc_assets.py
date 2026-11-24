#!/usr/bin/env python3
"""Pin the actual owned PowerOrc14 source and generated opponent dependencies.

This producer receipt is deliberately separate from native/contact acceptance.
The original-map flag remains false until genuine purchased maps are present.
"""
from pathlib import Path
import hashlib,json,datetime

STAGE=Path(__file__).resolve().parent
ROOT=STAGE/'arena_orcs'
SOURCE=STAGE/'orc-intake/powerorc'

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    extraction=json.loads((STAGE/'orc-intake/SOURCE_EXTRACTIONS.json').read_text())
    archives={str(Path(path).resolve()):sha(path)for path in extraction['archive_sha256']}
    assert archives==extraction['archive_sha256'],'Original archives changed since extraction'
    body=ROOT/'body';anatomy=ROOT/'anatomy'
    bodies=[body/f'combat_body_{i:03}.glb'for i in range(172)]
    caps=[anatomy/'caps'/f'cap_{i:03}.glb'for i in range(172)]
    assert all(p.is_file()for p in bodies+caps),'Complete genuine body/cap bank is required'
    assert json.loads((ROOT/'BODY_LAYOUT_REVIEW.json').read_text())['passed']
    assert json.loads((ROOT/'SEMANTIC_WEIGHTS_EXTENSION.json').read_text())['passed']
    anatomy_proof=json.loads((anatomy/'ANATOMY_READY.json').read_text())
    assert anatomy_proof.get('ready') is True,'Anatomy producer has not completed'
    material_tokens=(ROOT/'ORC_MATERIALS.cfg').read_text().split()
    assert material_tokens==['VALWOOD_ORC_MATERIALS','1','original_textures','0','end'], 'Original textures require a separate verified material update'
    files={str(p.resolve()):sha(p)for p in sorted(SOURCE.rglob('*.fbx'))}
    assert len(files)==31 and any('Attack' in p for p in files)
    generators=[STAGE/n for n in ['cook_power_orc_body.py','power_orc_guard_fit.py','augment_power_orc_weights.py','orc_anatomy_cook.py','finalize_power_orc_assets.py']]
    generators+=[STAGE.parents[1]/'tools/import_village_shield_knight.py']
    manifest=ROOT/'ORC_ASSETS.json'
    outputs={str(p.resolve()):sha(p)for p in sorted(ROOT.rglob('*'))if p.is_file()and p!=manifest and not p.name.endswith('.tmp')}
    receipt={
        'schema':'valwood.power-orc.arena-assets.v1',
        'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'source_name':'PowerOrc14','source_model':'PowerOrc14',
        'source_product':'https://www.cgtrader.com/3d-models/character/fantasy-character/powerorc14',
        'source_product_id':'5973135',
        'source_archives':archives,'source_files':files,
        'source_extraction_receipt':{'path':str(STAGE/'orc-intake/SOURCE_EXTRACTIONS.json'),'sha256':sha(STAGE/'orc-intake/SOURCE_EXTRACTIONS.json')},
        'generator':{str(p.resolve()):sha(p)for p in generators},
        'body_directory':'body','anatomy_directory':'anatomy','body_pose_count':172,'cap_pose_count':172,
        'actual_geometry_sha256':{str(p.relative_to(ROOT)):sha(p)for p in bodies+caps},
        'outputs':outputs,
        'materials_provisional':True,'original_textures_present':False,'visual_ready':False,
        'producer_complete':True,'accepted':False,
        'status':'genuine_source_geometry_ready_for_independent_native_contact_review; original_purchased_maps_pending',
        'material_note':'Actual purchased FBX geometry and skeletal animation. Original source FBXs contain no embedded images. Current authored skin/armor/axe factors are provisional and cannot pass the original-texture visual gate.',
        'source_rights_note':'User-owned commercial PowerOrc14 source. Preserve the purchased license and keep the source archives separate from game delivery. This receipt grants no redistribution rights.',
        'acceptance_limits':['Independent file validation and native model/contact/animation checks are separate requirements.','No final visual quality or physical-controller feel claim.'],
    }
    manifest.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    audit_path=STAGE/'ORC_SOURCE_AUDIT.json';audit=json.loads(audit_path.read_text())
    audit['next_required']='Complete independent geometry/native contact acceptance and obtain/map original purchased textures; source discovery is complete.'
    audit['public_name_verification']={'exact_asset_title_found':True,'actual_product':'PowerOrc14','publisher':'A1981 Studio','url':receipt['source_product'],'product_id':'5973135','note':'The user’s Orc Power Pack refers to the actual owned PowerOrc14 source; it is not a Unity catalog product.'}
    audit['source_body_bank_manifest']={'path':str(manifest),'sha256':sha(manifest)}
    audit_path.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({'producer_complete':True,'accepted':False,'visual_ready':False,'outputs':len(outputs),'manifest':str(manifest),'sha256':sha(manifest)}))

if __name__=='__main__':main()

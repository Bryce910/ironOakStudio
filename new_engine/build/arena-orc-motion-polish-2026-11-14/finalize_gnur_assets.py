#!/usr/bin/env python3
"""Pin the genuine new Gnur source, maps, posed body and source anatomy.

Producer completion does not grant visual/native acceptance. This never changes
the body, textures, mapping receipts, anatomy, settings, or the accepted baseline.
"""
from pathlib import Path
import json,hashlib,datetime,struct

STAGE=Path(__file__).resolve().parent
ROOT=STAGE/'arena_orcs';INTAKE=STAGE/'intake'

def sha(path):
    with Path(path).open('rb')as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def checked(values):
    result={}
    for name,expected in values.items():
        path=Path(name).resolve();actual=sha(path)
        assert actual==expected,('Source/dependency changed',str(path))
        result[str(path)]=actual
    return result
def receipt(path):
    path=Path(path).resolve();data=json.loads(path.read_text())
    assert data.get('passed')is True,('Required producer proof incomplete',str(path))
    return data,{'path':str(path),'sha256':sha(path)}

def main():
    body=ROOT/'body';anatomy=ROOT/'anatomy'
    bodies=[body/f'combat_body_{i:03}.glb'for i in range(172)]
    caps=[anatomy/'caps'/f'cap_{i:03}.glb'for i in range(172)]
    assert all(p.is_file()for p in bodies+caps),'Complete new Gnur172 body/cap bank is required'
    assert len(list(body.glob('combat_body_*.glb')))==172 and len(list((anatomy/'caps').glob('cap_*.glb')))==172
    body_manifest=json.loads((body/'combat_motion_manifest.json').read_text())
    assert body_manifest['pose_origin_policy']=='preserve','Polished Run source must retain its real airborne origin'
    material=json.loads((STAGE/'ORC_MATERIAL_MAPPING.json').read_text())
    assert material['source_model']=='Unity Orc Gnur'and material['original_textures_present']is True and material['materials_provisional']is False
    assert material['primitive_materials']==[0,0,0,0,0,0,0,1]
    assert [m['name']for m in material['gltf']['materials']]==['Owned Unity Orc Gnur body','Owned Unity Orc Gnur weapon']
    assert len(material['converted_textures'])==3
    sources=checked(material['source_files']);checked(material['converted_textures'])
    sources[str((STAGE/'ORC_MATERIAL_MAPPING.json').resolve())]=sha(STAGE/'ORC_MATERIAL_MAPPING.json')
    mapping=json.loads((ROOT/'ORC_TEXTURE_MAPPING.json').read_text())
    assert mapping['original_textures_present']is True and mapping['geometry_edited']is False
    checked(mapping['inputs'])
    uv,uv_ref=receipt(INTAKE/'GNUR_SOURCE_TO_COOKED_UV.json')
    assert uv['primitive_count']==8 and len(uv['primitives'])==8
    sources.update(checked(uv['inputs']))
    retarget,retarget_ref=receipt(INTAKE/'GNUR_RETARGET_AND_LAYOUT_REVIEW.json')
    assert retarget['actual_target_bones']==61 and retarget['explicit_virtual_held_frames']==['Weapon1']
    assert retarget['body_poses']==172 and retarget['source_mesh_primitives']==8
    assert retarget['immutable_uv_index_tangent_sign_material_layout']is True
    sources.update(checked(retarget['inputs']))
    source_motion,motion_ref=receipt(INTAKE/'ORC_SOURCE_ANIMATION_REVIEW.json')
    assert source_motion['fbx_count']==92
    sources.update(checked(source_motion['inputs']))
    checked(source_motion['generator'])
    attack_motion,attack_ref=receipt(INTAKE/'GNUR_ATTACK_SOURCE_REVIEW.json')
    assert len(attack_motion['clips'])==8
    sources.update(checked(attack_motion['inputs']))
    motion_curve,curve_ref=receipt(ROOT/'MOTION_CURVE_REVIEW.json')
    assert motion_curve['continuous_source_sampling']is True
    assert motion_curve['idle_source_fraction']==1.
    assert motion_curve['idle_both_endpoints_shared_neutral']is True
    assert motion_curve['authored_locomotion']=='Run'and motion_curve['locomotion_source_phase_offset']==.125
    assert motion_curve['pose_origin_policy']=='preserve'
    comparison,comparison_ref=receipt(ROOT/'MOTION_SOURCE_COMPARISON.json')
    sources.update(checked(comparison['inputs']))
    assert comparison['source_guard_20_glbs_byte_identical']and comparison['source_guard_joints_and_sockets_identical']
    locomotion,locomotion_ref=receipt(ROOT/'WALK_SOURCE_MEASUREMENT.json')
    assert locomotion['authored_locomotion']=='Run'and locomotion['source_phase_offset']==.125
    for original in [locomotion['source'],locomotion['root_motion_source']]:
        sources.update(checked({original['source']:original['source_sha256']}))
    stage_origin_path=STAGE/'MOTION_STAGE_ORIGIN.json'
    stage_origin=json.loads(stage_origin_path.read_text())
    baseline_ready=Path(stage_origin['baseline'])/'GNUR_ARENA_READY.json'
    assert sha(baseline_ready)==stage_origin['baseline_ready_sha256']
    assert stage_origin['independent_source']and stage_origin['independent_bank']and stage_origin['source_assets_read_only']
    sources[str(stage_origin_path.resolve())]=sha(stage_origin_path)
    sources[str(baseline_ready.resolve())]=sha(baseline_ready)
    source_material,material_ref=receipt(INTAKE/'GNUR_SOURCE_MATERIAL_MAPPING.json')
    sources.update(checked(source_material['inputs']))
    normal,normal_ref=receipt(INTAKE/'GNUR_NORMAL_BASIS.json');sources.update(checked(normal['inputs']))
    assert normal['required_normal_green_policy']=='flip'and normal['vertical_bitmap_flip']is False
    anim,anim_ref=receipt(INTAKE/'ANIMATIONS_EXTRACTION.json')
    assert anim['fbx_count']==anim['identical_unity_asset_count']==92
    for entry in anim['files']:
        assert entry['identical_unity_package_assets']
        sources.update(checked({entry['path']:entry['sha256']}))
    package=json.loads((INTAKE/'UNITY_PACKAGE_EXTRACTION.json').read_text())
    assert package['source_unchanged']and package['archive_paths_safe']
    archives={package['outer_archive']['path']:package['outer_archive']['sha256'],package['inner_package']['path']:package['inner_package']['sha256'],anim['archive']['path']:anim['archive']['sha256']}
    archives=checked(archives)
    anatomy_proof,anatomy_ref=receipt(anatomy/'ANATOMY_READY.json')
    assert anatomy_proof['ready']and anatomy_proof['poses']==172
    assert anatomy_proof['source_model']=='Gnur'and anatomy_proof['jaw_in_head']and anatomy_proof['armor_attachments_follow_actual_parent']
    assert anatomy_proof['pose_origin_policy']==body_manifest['pose_origin_policy']
    checked(anatomy_proof['dependencies_sha256']);checked(anatomy_proof['outputs_sha256'])
    header=(anatomy/'anatomy.bin').read_bytes()[:20]
    assert header[:8]==b'VWANA002'and struct.unpack('<III',header[8:])==(172,8,8)
    cfg=(body/'combat_presentation.cfg').read_text().split();profile=anatomy_proof['collision_profile']
    assert cfg[:2]==['VALWOOD_COMBAT_PRESENTATION','1']and cfg[-1]=='end'
    for field in ['body_radius_m','body_height_m']:
        assert cfg.count(field)==1 and abs(float(cfg[cfg.index(field)+1])-profile[field])<1e-6
    assert float(cfg[cfg.index('walk_stride_m')+1])>0
    assert abs(float(cfg[cfg.index('walk_stride_m')+1])-locomotion['walk_stride_m'])<1e-8
    # Setting status1 is an earlier explicit material-production step, never
    # inferred from a picture or changed as a side effect of final acceptance.
    assert (ROOT/'ORC_MATERIALS.cfg').read_text().split()==['VALWOOD_ORC_MATERIALS','1','original_textures','1','end']
    generator_names=['cook_gnur_body.py','gnur_body_functions.py','gnur_guard_fit.py','gnur_anatomy_cook.py','convert_owned_orc_materials.py','finalize_gnur_assets.py','intake/inventory_unity_package.py','intake/extract_selected_guids.py','intake/map_gnur_sources.py','intake/probe_gnur_normal_basis.py','intake/review_owned_orc_clips.py','intake/classify_gnur_attacks.py']
    generators={str((STAGE/name).resolve()):sha(STAGE/name)for name in generator_names}
    generators[str((STAGE.parents[1]/'tools/import_village_shield_knight.py').resolve())]=sha(STAGE.parents[1]/'tools/import_village_shield_knight.py')
    # Keep all original intake receipts available to independently trace the
    # exact source payloads, GUID assignments, normal basis and matching clips.
    intake_proofs={str(p.resolve()):sha(p)for p in [INTAKE/'UNITY_PACKAGE_EXTRACTION.json',INTAKE/'UNITY_GUID_INVENTORY.json',INTAKE/'MESH_CANDIDATE_SELECTION-EXTRACTED.json',INTAKE/'GNUR_MATERIAL_SELECTION-EXTRACTED.json',INTAKE/'ANIMATIONS_EXTRACTION.json',INTAKE/'GNUR_SOURCE_MATERIAL_MAPPING.json',INTAKE/'GNUR_SOURCE_TO_COOKED_UV.json',INTAKE/'GNUR_NORMAL_BASIS.json']}
    sources.update(intake_proofs)
    sources[str((INTAKE/'ORC_SOURCE_ANIMATION_REVIEW.json').resolve())]=sha(INTAKE/'ORC_SOURCE_ANIMATION_REVIEW.json')
    sources[str((INTAKE/'GNUR_ATTACK_SOURCE_REVIEW.json').resolve())]=sha(INTAKE/'GNUR_ATTACK_SOURCE_REVIEW.json')
    manifest=ROOT/'ORC_ASSETS.json'
    outputs={str(p.resolve()):sha(p)for p in sorted(ROOT.rglob('*'))if p.is_file()and p!=manifest and not p.name.endswith('.tmp')}
    result={'schema':'valwood.owned-orc.arena-assets.v2','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_name':'Gnur','source_collection':'Characters Orcs (Pack)','source_model':'Gnur','source_archives':archives,'source_files':sources,'generator':generators,'outputs':outputs,'body_directory':'body','anatomy_directory':'anatomy','body_pose_count':172,'cap_pose_count':172,'rigid_weapon_primitives':[7],'rigid_weapon_material_name':'Owned Unity Orc Gnur weapon','source_bones':61,'explicit_virtual_weapon_frames':1,'actual_geometry_sha256':{str(p.relative_to(ROOT)):sha(p)for p in bodies+caps},'actual_source_to_cooked_uv':uv_ref,'source_to_cooked_uv_receipt':uv_ref,'retarget_and_layout_receipt':retarget_ref,'original_material_receipt':material_ref,'material_mapping_receipt':{'path':str(STAGE/'ORC_MATERIAL_MAPPING.json'),'sha256':sha(STAGE/'ORC_MATERIAL_MAPPING.json')},'normal_basis_receipt':normal_ref,'matching_animation_receipt':anim_ref,'anatomy_receipt':anatomy_ref,'measured_collision_profile':profile,'materials_provisional':False,'original_textures_present':True,'producer_complete':True,'visual_ready':False,'accepted':False,'status':'genuine_matching_Gnur_geometry_original_materials_matching_source_motion_and_anatomy_produced; independent_native_contact_visual_acceptance_separate','source_rights_note':'User-provided owned commercial Characters Orcs (Pack) sources. Preserve the purchased license; no source redistribution rights are granted by this receipt.','limitations':['New genuine Gnur model integration; PowerOrc14 atlas mismatch was recorded and never used to assign these maps.','Producer completion is separate from independent file validation, native loading, physical contacts, rendered appearance and controller review.']}
    result.update(motion_polish_origin=stage_origin,source_animation_metadata_receipt=motion_ref,original_attack_joint_review_receipt=attack_ref,motion_curve_receipt=curve_ref,motion_source_comparison_receipt=comparison_ref,locomotion_source_receipt=locomotion_ref,authored_locomotion='Run',source_locomotion_phase_offset=.125,pose_origin_policy=body_manifest['pose_origin_policy'])
    manifest.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps({'producer_complete':True,'visual_ready':False,'original_textures_present':True,'source_model':'Gnur','outputs':len(outputs),'manifest':str(manifest),'sha256':sha(manifest)}))

if __name__=='__main__':main()

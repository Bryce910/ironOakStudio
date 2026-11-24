#!/usr/bin/env python3
"""Independent original-source/bind and tightly scoped flesh-capsule witness."""
from pathlib import Path
import hashlib,json,struct,sys
import numpy as np
sys.dont_write_bytecode=True
from orc_child_contract import Glb

N=Path(__file__).resolve().parent
B=N/'arena_orcs_roster/orc04'
P=N/'roster-review'

def require(ok,message):
    if not ok:raise RuntimeError(message)

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def pin(path,inputs,expected=None):
    path=Path(path).resolve();actual=sha(path)
    require(expected is None or actual==expected,'Changed source/certificate: '+str(path))
    inputs[str(path)]=actual
    return path

def proof(path,inputs):
    path=pin(path,inputs);doc=json.loads(path.read_text())
    require(doc.get('passed') is True,'Producer proof failed: '+str(path))
    for field in ('inputs','outputs_sha256','dependencies_sha256'):
        for name,expected in doc.get(field,{}).items():pin(name,inputs,expected)
    return doc

def main():
    inputs={};pin(__file__,inputs);pin(N/'orc_child_contract.py',inputs)
    provenance=proof(N/'intake_roster/orc04/ORIGINAL_NORMAL_ARRAYS_PROVENANCE.json',inputs)
    bind=proof(N/'intake_roster/orc04/ORIGINAL_SKIN_BIND_RESTORATION.json',inputs)
    grip=proof(N/'intake_roster/orc04/PREFAB_GRIP_REVIEW.json',inputs)
    manifest=json.loads(pin(B/'ORC_ASSETS.json',inputs).read_text())
    motion=json.loads(pin(B/'body/combat_motion_manifest.json',inputs).read_text())
    require(motion.get('original_source_normal_skinning') is True and motion.get('normal_repair_vertices')==0,'Original normals were replaced/repaired')
    source_rows=[];source_triangles=0;source_normal_vertices=0;source_body_bones=set()
    for suffix in (0,1):
        dump=pin(N/f'orc04-{suffix}-source.assjson',inputs);data=json.loads(dump.read_text())
        for mesh in data['meshes']:
            if suffix==0:source_body_bones.update(bone['name'] for bone in mesh.get('bones',[]))
            vertex=np.asarray(mesh['vertices'],dtype=np.float64).reshape(-1,3)
            normal=np.asarray(mesh['normals'],dtype=np.float64).reshape(-1,3)
            require(vertex.shape==normal.shape and np.isfinite(vertex).all() and np.isfinite(normal).all(),'Original full-precision source arrays incomplete')
            lengths=np.linalg.norm(normal,axis=1)
            require(np.min(lengths)>.5 and np.max(np.abs(lengths-1))<.01,'Original source has invalid full-precision normals')
            triangles=sum(len(face)-2 for face in mesh['faces'])
            require(triangles>0 and all(len(face)>=3 for face in mesh['faces']),'Invalid original polygons')
            source_rows.append(dict(name=mesh['name'],original_vertices=len(vertex),original_polygon_count=len(mesh['faces']),triangulated_original_triangles=triangles,normal_length_min=float(lengths.min()),normal_length_max=float(lengths.max())))
            source_triangles+=triangles;source_normal_vertices+=len(normal)
    require(len(source_rows)==13 and source_triangles==103181,'Original Orc04 source inventory changed')
    body=Glb(B/'body/combat_body_000.glb',inputs)
    cooked_triangles=sum(body.accessor(primitive['indices']).size//3 for primitive in body.primitives)
    require(len(body.primitives)==13 and cooked_triangles==source_triangles,'Lossless weld changed original source triangles')
    require(int(motion['body_primitive_count'])==11 and motion['weapon_primitives']==[11,12],'Original body/weapon primitive separation changed')
    require(bind['maximum_corrected_bind_matrix_error']<1e-6 and {row['bone'] for row in bind['bones']}==source_body_bones,'Original skin-bind restoration is not precise/complete for its actual source bones')
    bind_errors=[]
    for row in bind['bones']:
        error=float(np.linalg.norm(np.asarray(row['source_bind_head_m'])-np.asarray(row['corrected_head_m'])))
        require(error<1e-5,'Restored skin bind differs from original source')
        bind_errors.append(error)
    require(max(grip['unchanged_bind_frame_max_errors'])<1e-5,'Original prefab hand/grip transform was not preserved')
    policy=proof(B/'anatomy/CONTROLLER_PROFILE_POLICY.json',inputs)
    require(policy['schema']=='valwood.owned-orc.flesh-torso-and-controller-profile.v2' and policy['flesh_torso_capsule_only'] and policy['original_geometry_and_sever_masks_unchanged'],'Unexpected coarse/precise collision edit policy')
    prior=pin(N/'anatomy-history/orc04/before-flesh-torso-capsule/anatomy/anatomy.bin',inputs)
    current=pin(B/'anatomy/anatomy.bin',inputs)
    before=prior.read_bytes();after=current.read_bytes();require(len(before)==len(after),'Anatomy layout changed')
    patched=bytearray(before);changes=policy['precise_anatomy_changes'];radii=[]
    require(changes['passed'] and changes['changed_torso_radii']==172 and len(changes['per_pose'])==172,'Incomplete precise torso change certificate')
    for row in changes['per_pose']:
        offset=row['byte_offset'];old=struct.unpack_from('<f',before,offset)[0];new=struct.unpack_from('<f',after,offset)[0]
        require(abs(old-row['original_radius_m'])<1e-7 and abs(new-row['flesh_radius_m'])<1e-7,'Torso float differs from measured certificate')
        require(.25<new<.30 and old>new,'Flesh torso fit is not anatomically bounded')
        patched[offset:offset+4]=after[offset:offset+4];radii.append(new)
    require(bytes(patched)==after,'Anatomy changed outside the172specified torso radius floats')
    cap_pins=policy['cut_cap_outputs_unchanged'];require(len(cap_pins)==172,'Incomplete cut-cap invariance proof')
    for name,expected in cap_pins.items():pin(name,inputs,expected)
    require(manifest['source_character_id']=='orc04','Wrong owned character manifest')
    target=P/'ORC04_SOURCE_FIDELITY_REVIEW.json'
    report=dict(schema='valwood.orc-roster.orc04-independent-source-fidelity.v1',passed=True,character_id='orc04',inputs=inputs,
        original_source_meshes=source_rows,original_source_normal_vertices=source_normal_vertices,original_triangle_count=source_triangles,cooked_triangle_count=cooked_triangles,
        original_normal_skinning_declared=True,normal_repair_vertices=0,original_bind_bones=len(bind_errors),maximum_restored_bind_head_error_m=max(bind_errors),maximum_original_prefab_grip_error_m=max(grip['unchanged_bind_frame_max_errors']),
        allowed_changed_torso_radius_floats=172,all_other_precise_anatomy_bytes_identical=True,all172_cut_cap_glbs_identical=True,measured_flesh_torso_radius_range_m=[min(radii),max(radii)],
        limits=['Original full-precision source arrays, triangle preservation and bind/grip certificates are independently checked. The producer source-normal skin transform is pinned and declared; this witness does not reimplement the entire source animation retargeter. Actual native material/render acceptance is separate.'])
    target.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print('PASS Orc04 source fidelity and exact172-float torso correction:',target)

if __name__=='__main__':main()

#!/usr/bin/env python3
"""Apply only the purchased HDRP eye smoothness scalar to owned candidate GLBs.

This is not a full port of Unity HDRP's EyeSubTarget. Geometry, textures and all
other materials remain exactly as the published baseline. The generic runtime
BRDF's established actor roughness floor still bounds the resolved lobe.
"""
from pathlib import Path
import copy, hashlib, json, re, sys

GEN=Path(__file__).resolve().parents[1]
ENGINE=GEN.parents[1]
BASELINE=ENGINE/'build/thomas-quality-2026-11-20'
ROOT=ENGINE/'assets/characters/incoming/thomas_kubold/thomas/Assets/David Grette/Adventure_Pack/Thomas'
REVIEW=GEN/'review/asset-audit'
sys.path.insert(0,str(ENGINE/'tools'))
import finalize_modular_knight_glb as glb
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def graph_objects(path):
    text=path.read_text();decoder=json.JSONDecoder();objects=[];at=0
    while at<len(text):
        if text[at].isspace():at+=1;continue
        obj,at=decoder.raw_decode(text,at);objects.append(obj)
    return objects
def main():
    shader=REVIEW/'source-shaders/Eyes_HDPR.shadergraph'
    material=ROOT/'Materials/HDPR_Materials/Eyes/M_Thomas_Eyes.mat'
    objects=graph_objects(shader);by_id={x['m_ObjectId']:x for x in objects}
    block=next(x for x in objects if x.get('m_SerializedDescriptor')=='SurfaceDescription.Smoothness')
    edges=[e for e in objects[0]['m_Edges'] if e['m_InputSlot']['m_Node']['m_Id']==block['m_ObjectId'] and e['m_InputSlot']['m_SlotId']==0]
    assert len(edges)==1
    node=by_id[edges[0]['m_OutputSlot']['m_Node']['m_Id']]
    assert node['m_Type']=='UnityEditor.ShaderGraph.PropertyNode' and edges[0]['m_OutputSlot']['m_SlotId']==0
    prop=by_id[node['m_Property']['m_Id']]
    reference=prop['m_DefaultReferenceName']
    assert reference=='Vector1_912eda3114e749d5aa844619dcfe113c'
    text=material.read_text()
    assert 'guid: 05ed59fee9139674dbebd22a211e7bfe' in text
    smoothness=float(re.search(r'^    - '+re.escape(reference)+r': ([\d.]+)$',text,re.M)[1])
    assert smoothness==1.3
    roughness=1-min(1,max(0,smoothness))
    assert roughness==0
    proof={
        'schema':'valwood.thomas.hdrp-eye-revision.v1','passed':True,
        'baseline':str(BASELINE),'material':str(material),'material_sha256':sha(material),
        'graph':str(shader),'graph_sha256':sha(shader),'graph_guid':'05ed59fee9139674dbebd22a211e7bfe',
        'source_target':'UnityEditor.Rendering.HighDefinition.ShaderGraph.EyeSubTarget',
        'surface_block_id':block['m_ObjectId'],'direct_edge':edges[0],
        'property_node_id':node['m_ObjectId'],'property_reference':reference,
        'property_display_name':prop['m_Name'],'authored_smoothness':smoothness,
        'clamped_smoothness':1,'derived_roughness_factor':roughness,
        'policy':'Only Thomas_EyeBall roughnessFactor changes from Built-in fallback 0.5 to purchased HDRP graph smoothness scalar (1-clamp(1.3,0,1)=0). This ports the authored surface scalar, not the complete HDRP EyeSubTarget BRDF. No coat or geometry is invented.',
        'runtime_bound':'Existing actor roughness floor 0.06 remains; source-dependent specular antialiasing remains active.',
        'scope':{},
    }
    report_path=GEN/'assets/character-quality/QUALITY_BUILD.json'
    report=json.loads(report_path.read_text())
    validation_path=GEN/'assets/character-quality/QUALITY_VALIDATION.json'
    validation=json.loads(validation_path.read_text())
    for tier in ['Low','Medium','High','Ultra']:
        for outfit in ['civilian','armored']:
            relative=Path('assets/character-quality')/tier/'story_hero'/outfit/'body.glb'
            before=BASELINE/relative;after=GEN/relative
            assert after.is_file() and not after.is_symlink()
            sd,sb,_=glb.load(before);d,b,_=glb.load(after)
            # The operation is safely repeatable, but cannot conceal any other
            # candidate geometry, texture or material modifications.
            clean=copy.deepcopy(d)
            eyes=[m for m in clean['materials'] if m.get('name')=='Thomas_EyeBall'];assert len(eyes)==1
            eyes[0]['pbrMetallicRoughness']['roughnessFactor']=.5
            assert clean==sd and b==sb,(tier,outfit,'Unexpected candidate delta')
            baseline_eye=next(m for m in sd['materials'] if m.get('name')=='Thomas_EyeBall')
            assert baseline_eye['pbrMetallicRoughness']['roughnessFactor']==.5
            next(m for m in d['materials'] if m.get('name')=='Thomas_EyeBall')['pbrMetallicRoughness']['roughnessFactor']=roughness
            before_source_sha=sha(before)
            after.write_bytes(glb.encode(d,b))
            ad,ab,_=glb.load(after);restore=copy.deepcopy(ad)
            next(m for m in restore['materials'] if m.get('name')=='Thomas_EyeBall')['pbrMetallicRoughness']['roughnessFactor']=.5
            assert restore==sd and ab==sb
            assert sha(before)==before_source_sha
            key=f'{tier}/{outfit}'
            proof['scope'][key]={
                'file':str(after),'baseline_sha256':before_source_sha,'candidate_sha256':sha(after),
                'baseline_roughness_factor':.5,'candidate_roughness_factor':roughness,
                'binary_payload_sha256':hashlib.sha256(ab).hexdigest(),
                'binary_payload_identical':True,'all_other_document_fields_identical':True,
                'baseline_file_bytes':before.stat().st_size,'candidate_file_bytes':after.stat().st_size,
            }
            report['tiers'][tier][outfit].update(sha256=sha(after),file_bytes=after.stat().st_size)
            validation['bodies'][key].update(sha256=sha(after),file_bytes=after.stat().st_size)
    proof_path=REVIEW/'EYE_HDRP_REVISION.json'
    proof_path.write_text(json.dumps(proof,indent=2)+'\n')
    exception={'material':'Thomas_EyeBall','field':'pbrMetallicRoughness.roughnessFactor','before':.5,'after':roughness,'reason':proof['policy'],'proof':str(proof_path),'proof_sha256':sha(proof_path)}
    report['material_revision_policy']=[exception]
    validation['material_preservation_exceptions']=[exception]
    validation['revision_validation']={'schema':proof['schema'],'passed':True,'bodies':8,'all_binary_payloads_identical':True,'all_other_document_fields_identical':True,'published_baseline_unchanged':True,'proof':str(proof_path),'proof_sha256':sha(proof_path)}
    validation['assertions']=[line.replace('All original material factors, alpha cutoffs, double-sided flags and sampler factors are retained.','All material factors, alpha cutoffs, double-sided flags and sampler factors are retained except the explicit source-authored HDRP Thomas_EyeBall roughness correction in material_preservation_exceptions.') for line in validation['assertions']]
    report_path.write_text(json.dumps(report,indent=2)+'\n')
    validation_path.write_text(json.dumps(validation,indent=2)+'\n')
    print(json.dumps(proof,indent=2))
if __name__=='__main__':main()

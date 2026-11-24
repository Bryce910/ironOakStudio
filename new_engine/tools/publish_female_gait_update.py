#!/usr/bin/env python3
"""Validate/publish the staged female-only leg-excursion update."""
from pathlib import Path
import hashlib,json,os,shutil
import numpy as np
from audit_village_pose_compatibility import ROOT,MODELS,pose

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    stage=ROOT/'assets/characters/female-gait-stage';backup=ROOT/'assets/characters/female-gait-before'
    old=json.loads((MODELS/'village_character_manifest.json').read_text());new=json.loads((stage/'village_character_manifest.json').read_text())
    original={r['file']:r for r in old['assets']};records=[];prepared=[]
    for row in new['assets']:
        filename=row['file'];prior=original[filename]
        if row['role']!='travelerwoman':
            assert row==prior and sha(MODELS/filename)==prior['sha256'];continue
        before=pose(MODELS/filename);after=pose(stage/filename)
        assert len(before['records'])==len(after['records'])
        assert all(a['signature']==b['signature'] for a,b in zip(before['records'],after['records']))
        assert before['raw_min_y']==after['raw_min_y']==0 and 1.68<=after['height']<=1.92
        assert row['triangles']==prior['triangles'] and row['primitive_count']==prior['primitive_count']
        assert row['fixed_reference_scale']==prior['fixed_reference_scale'] and row['uv_sha256']==prior['uv_sha256']
        delta=max(float(np.max(np.abs(a['world']-b['world']))) for a,b in zip(before['records'],after['records']))
        if row['mode']=='idle':
            assert delta==0 and all(np.array_equal(a['normal'],b['normal']) for a,b in zip(before['records'],after['records']))
        row['gait_leg_blend']=.54 if row['mode']=='walk' else None
        row['gait_upper_body_blend']=.34 if row['mode']=='walk' else None
        row['estimated_source_two_step_stride_m']=1.1464667996125442 if row['mode']=='walk' else None
        records.append(dict(file=filename,before_sha256=sha(MODELS/filename),after_sha256=sha(stage/filename),all_material_texture_index_uv_and_vertex_counts_exact=True,weighted_sole_ground_m=0.,body_height_m=after['height'],position_maximum_change_m=delta,mode=row['mode']))
        prepared.append(filename)
    assert len(prepared)==12 and new['budget']['unique_texture_policies']==old['budget']['unique_texture_policies']
    assert new['budget']['estimated_rgba_mips_mib']==old['budget']['estimated_rgba_mips_mib']
    # All staged per-pose correspondences and image identities pass before
    # the first live mutation. Source downloads and other80poses stay exact.
    backup.mkdir(parents=True,exist_ok=True)
    for filename in prepared:
        if not (backup/filename).exists():shutil.copy2(MODELS/filename,backup/filename)
    if not (backup/'village_character_manifest.json').exists():shutil.copy2(MODELS/'village_character_manifest.json',backup/'village_character_manifest.json')
    for filename in prepared:
        target=MODELS/filename
        if sha(target)==sha(stage/filename):continue
        temporary=target.with_suffix('.glb.pending');shutil.copyfile(stage/filename,temporary);os.replace(temporary,target)
    new['total_bytes']=sum(r['bytes'] for r in new['assets']);pending=MODELS/'village_character_manifest.json.pending';pending.write_text(json.dumps(new,indent=2)+'\n');os.replace(pending,MODELS/'village_character_manifest.json')
    report=dict(schema=1,published=True,appearance='travelerwoman',source_leg_blend_before=.42,source_leg_blend_after=.54,source_upper_body_blend=.34,estimated_two_step_stride_m=1.1464667996125442,pose_count=12,source_files_modified=False,other_appearances_modified=False,texture_images_or_memory_changed=False,records=records)
    (ROOT/'assets/characters/female-gait-publication.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PUBLISHED female-only12poses; strict count/index/UV/material/image preservation; soles0; fixedscale unchanged; stride1.1464668m; otherappearances unchanged',flush=True)

if __name__=='__main__':main()

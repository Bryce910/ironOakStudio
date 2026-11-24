#!/usr/bin/env python3
"""Audit near-only facade repacks before publication; mids must stay exact."""
import json
import math
import numpy as np
from build_purchased_village import SOURCE,OUT,inspect_glb,validate,sha
from validate_purchased_village_fidelity import triangles,sample


def main():
    stage=SOURCE/'cooked';manifest_path=stage/'purchased_village_manifest.json'
    baseline=SOURCE/'facade-repack-before' if (SOURCE/'facade-repack-before'/'purchased_village_manifest.json').exists() else OUT
    manifest=json.loads(manifest_path.read_text());previous=json.loads((baseline/'purchased_village_manifest.json').read_text())
    old_rows={r['asset']:r for r in previous['assets']};gpu=validate(manifest_path,stage)
    bary=np.asarray([[1/3,1/3,1/3],[.6,.2,.2],[.2,.6,.2],[.2,.2,.6]])
    results=[]
    for family in ['house_a','house_b','house_c','house_d','house_e','house_f']:
        name='purchased_village_'+family+'_near';path=stage/(name+'.glb');assert path.exists()
        print('Facade check',family,flush=True)
        row=next(r for r in manifest['assets']if r['asset']==name);old=old_rows[name]
        actual=inspect_glb(path)
        assert actual['triangles']==old['triangles'] and actual['primitive_count']==old['primitive_count']
        assert actual['bounds_y_up']==old['bounds_y_up']
        assert sha(SOURCE/'source_meshes'/row['source'])==row['source_sha256']
        mid_name='purchased_village_'+family+'_mid'
        mid_row=next(r for r in manifest['assets']if r['asset']==mid_name)
        assert mid_row==old_rows[mid_name] and not (stage/(mid_name+'.glb')).exists()
        assert sha(OUT/(mid_name+'.glb'))==mid_row['sha256']
        old_doc,old_triangles=triangles(baseline/path.name);new_doc,new_triangles=triangles(path)
        old_index={(t['material'],t['key']):t for t in old_triangles}
        assert {(t['material'],t['key']) for t in new_triangles}==set(old_index)
        old_tiles={m['name']:m for m in old_doc['materials']if m['name'].startswith('Purchased_Tile_')}
        new_tiles={m['name']:m for m in new_doc['materials']if m['name'].startswith('Purchased_Tile_')}
        assert old_tiles.keys()==new_tiles.keys()
        old_sum=np.zeros(3);new_sum=np.zeros(3);palette_area=0.;area=0.;old_texels=0.;new_texels=0.;max_tile_uv_delta=0.
        for t in new_triangles:
            old_t=old_index[t['material'],t['key']]
            order=[int(np.argmin(np.sum((old_t['xyz']-p)**2,axis=1))) for p in t['xyz']]
            assert len(set(order))==3
            old_uv=old_t['uv'][order]
            if t['material'].startswith('Purchased_Tile_'):
                max_tile_uv_delta=max(max_tile_uv_delta,float(np.max(np.abs(t['uv']-old_uv))))
                assert t['sha']==old_t['sha'];continue
            assert np.min(t['uv'])>=-1e-5 and np.max(t['uv'])<=1.00001
            area+=t['area'];old_texels+=old_t['density']**2*t['area'];new_texels+=t['density']**2*t['area']
            before=sample(old_t['pixels'],bary@old_uv);after=sample(t['pixels'],bary@t['uv'])
            common=(before[:,3]>=.4)&(after[:,3]>=.4)
            old_sum+=np.sum(before[:,:3]*common[:,None],axis=0)*t['area']/4
            new_sum+=np.sum(after[:,:3]*common[:,None],axis=0)*t['area']/4
            palette_area+=float(np.mean(common))*t['area']
        assert max_tile_uv_delta==0.,(family,'tile UV modified',max_tile_uv_delta)
        old_density=math.sqrt(old_texels/area);new_density=math.sqrt(new_texels/area)
        assert new_density>=old_density*1.2,(family,'facade density improvement too small',old_density,new_density)
        before=old_sum/palette_area;after=new_sum/palette_area;delta=float(np.max(np.abs(before-after)))
        assert delta<.05,(family,'near/mid broad linear palette mismatch',delta,before.tolist(),after.tolist())
        result=dict(family=family,triangles=actual['triangles'],primitives=actual['primitive_count'],
            bounds_and_near_geometry_unchanged=True,mid_file_and_material_bytes_unchanged=True,
            source_fbx_hash_unchanged=True,tile_uv_maximum_delta=max_tile_uv_delta,tile_image_bytes_unchanged=True,
            facade_area_m2=area,before_texels_m=old_density,after_texels_m=new_density,density_ratio=new_density/old_density,
            before_linear_rgb=before.tolist(),after_linear_rgb=after.tolist(),maximum_broad_linear_rgb_delta=delta,
            chart_audit=row['facade_repack'])
        results.append(result)
        print(family,'density',round(old_density,2),'->',round(new_density,2),'exterior',round(row['facade_repack']['achieved_exterior_texels_m'],2),'palette delta',round(delta,5),flush=True)
    report=dict(schema=1,stage_validated=True,architecture_gpu_mib=gpu/1024/1024,near_models_updated=6,
                source_originals_modified=False,mid_files_modified=False,records=results)
    (SOURCE/'facade-repack-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('VALID facade-only repack: six near models, unchanged mids/roof/stone/geometry;',round(gpu/1024/1024,2),'MiB architecture',flush=True)


if __name__=='__main__':main()

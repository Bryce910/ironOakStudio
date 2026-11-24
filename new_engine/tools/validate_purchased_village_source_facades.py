#!/usr/bin/env python3
"""Independently check source-facade geometry, paint, pixels and LOD palette."""
import hashlib,json,math
import numpy as np
from build_purchased_village import SOURCE,OUT,glb,sha,inspect_glb,validate
from build_purchased_village_facades import HOUSES,WOOD,PLASTER,EXT,STAGE,DEST,BACKUP,accessor,source_triangles
from validate_purchased_village_fidelity import triangles,sample,image_data,key

def uv_scale(uv,s):return np.stack([uv[:,0]*s,1-(1-uv[:,1])*s],axis=1)
def exact_key(points):return tuple(sorted(tuple(float(v) for v in p) for p in points))

def validate_stage():
    manifest=json.loads((STAGE/'purchased_village_manifest.json').read_text())
    baseline=BACKUP if (BACKUP/'purchased_village_manifest.json').exists() else OUT
    old=json.loads((baseline/'purchased_village_manifest.json').read_text());old_rows={r['asset']:r for r in old['assets']}
    gpu=validate(STAGE/'purchased_village_manifest.json',STAGE);reports=[]
    bary=np.asarray([[1/3,1/3,1/3],[.6,.2,.2],[.2,.6,.2],[.2,.2,.6]])
    for family in HOUSES:
        name='purchased_village_'+family+'_near';path=STAGE/(name+'.glb');doc,binary=glb(path)
        before_doc,before_tri=triangles(baseline/path.name);before={exact_key(t['xyz']):t for t in before_tri}
        source=source_triangles(family);actual=inspect_glb(path);previous=old_rows[name]
        assert actual['triangles']==previous['triangles'] and actual['bounds_y_up']==previous['bounds_y_up']
        new_tri=[];stats={};seen=set()
        for primitive in doc['meshes'][0]['primitives']:
            attrs=primitive['attributes'];xyz=accessor(doc,binary,attrs['POSITION'],3);uv=accessor(doc,binary,attrs['TEXCOORD_0'],2)
            tangent=accessor(doc,binary,attrs['TANGENT'],4);normals=accessor(doc,binary,attrs['NORMAL'],3)
            material=doc['materials'][primitive['material']];params=material.get('extensions',{}).get(EXT,{}).get('parameters')
            pbr=material['pbrMetallicRoughness'];pixels,base_encoded=image_data(doc,binary,pbr['baseColorTexture']['index'])
            base_sha=hashlib.sha256(base_encoded).hexdigest()
            paint=accessor(doc,binary,attrs['_VALWOOD_PAINT'],1).ravel() if params else None
            mask_pixels=alternate_pixels=None
            if params:
                assert params[0]==1 and params[1] in {1,2};wood=params[1]==1;stem='T_Tileable_Wood_Trims_01' if wood else 'T_Tileable_Wall_Plaster_01'
                expected=[(pbr['baseColorTexture']['index'],stem,'BC'),(material['normalTexture']['index'],stem,'N'),(pbr['metallicRoughnessTexture']['index'],stem,'ARM')]
                expected+= [(params[2],'T_Tileable_Wood_Trims_01_01','BC'),(params[3],'T_RGB','Mask')] if wood else [(params[2],'T_Tileable_Wall_Plaster_02','N'),(params[3],'T_Tileable_Wall_Plaster_02','ARM'),(params[4],'T_Tileable_Wall_Plaster_01_RGB','Mask')]
                for index,stem,kind in expected:
                    _,encoded=image_data(doc,binary,index);assert hashlib.sha256(encoded).hexdigest()==sha(SOURCE/'prepared'/(stem+'_'+kind+'.png'))
                assert material['alphaMode']=='OPAQUE' and np.min(paint)>=0 and np.max(paint)<=1
                if wood:
                    mask_pixels,_=image_data(doc,binary,params[3]);alternate_pixels,_=image_data(doc,binary,params[2])
            for tri in accessor(doc,binary,primitive['indices'],1).reshape(-1,3):
                points=xyz[tri];k=exact_key(points);assert k in before;seen.add(k);old_t=before[k]
                order=[int(np.argmin(np.sum((old_t['xyz']-p)**2,axis=1))) for p in points]
                assert len(set(order))==3 and np.array_equal(old_t['xyz'][order],points),(family,material['name'],old_t['xyz'][order].tolist(),points.tolist())
                area=float(np.linalg.norm(np.cross(points[1]-points[0],points[2]-points[0]))*.5)
                if material['name'].startswith('Purchased_Tile_'):
                    assert material['name']==old_t['material'] and np.array_equal(old_t['uv'][order],uv[tri])
                    # Roof/stone source maps remain byte-identical.
                    assert base_sha==old_t['sha'];continue
                if not params:continue
                src=min(source[key(points)],key=lambda r:(float(np.sum(np.min(np.sum((r['xyz'][:,None,:]-points[None,:,:])**2,axis=2),axis=0))),float(np.sum(np.abs(np.mean(r['normal'],axis=0)-np.mean(normals[tri],axis=0))))))
                so=[int(np.argmin(np.sum((src['xyz']-p)**2,axis=1))) for p in points]
                assert np.array_equal(uv[tri],src['uv'][so]) and np.array_equal(tangent[tri],src['tangent'][so]) and np.array_equal(paint[tri],src['paint'][so])
                assert src['material']==(WOOD if params[1]==1 else PLASTER)
                sample_uv=bary@uv[tri];value=sample(pixels,uv_scale(sample_uv,params[5]))
                if params[1]==1:
                    weight=(bary@paint[tri])*sample(mask_pixels,uv_scale(sample_uv,params[7]))[:,1]
                    alt=sample(alternate_pixels,uv_scale(sample_uv,params[6]));value[:,:3]=value[:,:3]*(1-weight[:,None])+alt[:,:3]*weight[:,None]
                prior=sample(old_t['pixels'],bary@old_t['uv'][order])
                # Opaque source material ignores utility alpha; original bake
                # covered its facade region. Compare matched broad radiance.
                u=uv[tri]*params[5];du,dv=u[1]-u[0],u[2]-u[0];texel_area=abs(float(du[0]*dv[1]-du[1]*dv[0]))*.5*pixels.shape[0]*pixels.shape[1]
                g=stats.setdefault(material['name'],dict(area=0.,texels=0.,old_texels=0.,old=np.zeros(3),new=np.zeros(3),triangles=0))
                g['area']+=area;g['texels']+=texel_area;g['old_texels']+=old_t['density']**2*area
                g['old']+=np.mean(prior[:,:3],axis=0)*area;g['new']+=np.mean(value[:,:3],axis=0)*area;g['triangles']+=1
        assert seen==set(before)
        mid='purchased_village_'+family+'_mid';mid_row=next(r for r in manifest['assets'] if r['asset']==mid)
        assert mid_row==old_rows[mid] and sha(OUT/(mid+'.glb'))==mid_row['sha256'] and not (STAGE/(mid+'.glb')).exists()
        surfaces=[]
        for label,g in stats.items():
            delta=float(np.max(np.abs((g['old']-g['new'])/g['area'])));density=math.sqrt(g['texels']/g['area']);old_density=math.sqrt(g['old_texels']/g['area'])
            assert delta<.055,(family,label,'broad source palette mismatch',delta)
            assert density>old_density*1.2,(family,label,'insufficient detail improvement',density,old_density)
            surfaces.append(dict(material=label,triangles=g['triangles'],area_m2=g['area'],before_texels_m=old_density,source_texels_m=density,density_ratio=density/old_density,maximum_broad_linear_rgb_delta=delta))
        reports.append(dict(family=family,geometry_positions_and_bounds_exact=True,mid_file_exact=True,roof_stone_uv_and_base_image_exact=True,source_uv_tangent_paint_exact=True,source_pbr_image_bytes_exact=True,primitives=actual['primitive_count'],surfaces=surfaces))
        print('VALID_SOURCE_FACADE',family,[(r['material'],round(r['density_ratio'],1),round(r['maximum_broad_linear_rgb_delta'],5)) for r in surfaces],flush=True)
    report=dict(schema=1,architecture_rgba8_mip_mib=gpu/1024/1024,source_downloads_modified=False,records=reports)
    (DEST/'validation.json').write_text(json.dumps(report,indent=2)+'\n');return report

if __name__=='__main__':validate_stage()

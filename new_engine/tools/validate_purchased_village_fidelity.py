#!/usr/bin/env python3
"""Compare staged source-UV architecture against the previous live atlases.

Checks identical geometry/bounds, source imagebytes, bounded draw/texture cost,
and matched triangle texel density. Also compares integrated linear albedo of
the near repeating surface with its corrected, shared mid atlas; differences
are reported independently from lighting/exposure.
"""
from pathlib import Path
import hashlib
import io
import json
import math
import sys
import numpy as np
from PIL import Image
from build_purchased_village import ROOT,SOURCE,OUT,glb,inspect_glb,sha,validate


def array(doc,binary,index,width):
    a=doc['accessors'][index]; view=doc['bufferViews'][a['bufferView']]
    dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]
    stride=np.dtype(dtype).itemsize
    return np.ndarray((a['count'],width),dtype=dtype,buffer=binary,
                      offset=view.get('byteOffset',0)+a.get('byteOffset',0),
                      strides=(view.get('byteStride',stride*width),stride))


def image_data(doc,binary,index):
    image=doc['images'][doc['textures'][index]['source']]
    view=doc['bufferViews'][image['bufferView']]; offset=view.get('byteOffset',0)
    data=binary[offset:offset+view['byteLength']]
    pixels=np.asarray(Image.open(io.BytesIO(data)).convert('RGBA'),dtype=np.float32)/255.
    return pixels,data


def key(points): return tuple(sorted(tuple(int(round(float(v)*100000)) for v in p) for p in points))


def triangles(path):
    doc,binary=glb(path); rows=[]
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            positions=array(doc,binary,primitive['attributes']['POSITION'],3)
            uv=array(doc,binary,primitive['attributes']['TEXCOORD_0'],2)
            indices=array(doc,binary,primitive['indices'],1).reshape(-1,3)
            material=doc['materials'][primitive['material']]
            index=material['pbrMetallicRoughness']['baseColorTexture']['index']
            pixels,encoded=image_data(doc,binary,index)
            image_sha=hashlib.sha256(encoded).hexdigest()
            texture=doc['textures'][index];sampler=doc['samplers'][texture['sampler']]
            assert sampler.get('wrapS',10497)==sampler.get('wrapT',10497)==10497
            for tri in indices:
                xyz=positions[tri]; texture_uv=uv[tri]
                area=float(np.linalg.norm(np.cross(xyz[1]-xyz[0],xyz[2]-xyz[0]))*.5)
                u,v=texture_uv[1]-texture_uv[0],texture_uv[2]-texture_uv[0]
                uv_area=abs(float(u[0]*v[1]-u[1]*v[0]))*.5
                density=math.sqrt(uv_area*pixels.shape[0]*pixels.shape[1]/area) if area>1e-12 else 0
                rows.append(dict(key=key(xyz),xyz=xyz,uv=texture_uv,area=area,density=density,
                                 material=material['name'],pixels=pixels,masked=material.get('alphaMode')=='MASK',cutoff=material.get('alphaCutoff',.5),sha=image_sha))
    return doc,rows


def sample(pixels,uv):
    h,w=pixels.shape[:2]; uv=np.mod(uv,1.)
    # Bilinear sampling in PNG/glTF top-left UV convention.
    p=uv*np.array([w,h])-.5;lo=np.floor(p).astype(int);frac=p-lo
    x,y=lo[:,0]%w,lo[:,1]%h;xx,yy=(x+1)%w,(y+1)%h
    rgb=(pixels[y,x]*(1-frac[:,0,None])*(1-frac[:,1,None])+
         pixels[y,xx]*frac[:,0,None]*(1-frac[:,1,None])+
         pixels[yy,x]*(1-frac[:,0,None])*frac[:,1,None]+
         pixels[yy,xx]*frac[:,0,None]*frac[:,1,None])
    rgb[:,:3]=np.where(rgb[:,:3]<=.04045,rgb[:,:3]/12.92,((rgb[:,:3]+.055)/1.055)**2.4)
    return rgb


def main():
    stage=SOURCE/'cooked'; manifest_path=stage/'purchased_village_manifest.json'
    manifest=json.loads(manifest_path.read_text());gpu=validate(manifest_path,stage)
    baseline=SOURCE/'material-fidelity-before' if (SOURCE/'material-fidelity-before'/'purchased_village_manifest.json').exists() else OUT
    previous=json.loads((baseline/'purchased_village_manifest.json').read_text());previous={r['asset']:r for r in previous['assets']}
    records=[]; bary=np.asarray([[1/3,1/3,1/3],[.6,.2,.2],[.2,.6,.2],[.2,.2,.6]])
    for row in manifest['assets']:
        path=stage/(row['asset']+'.glb')
        if not path.exists(): continue
        print('Checking',row['asset'],flush=True)
        old=previous[row['asset']]; new=inspect_glb(path)
        assert new['triangles']<=row['budget']
        if row['lod']=='near':assert new['triangles']==old['triangles'],(row['asset'],'near triangle count changed')
        else:assert abs(new['triangles']-old['triangles'])<=8,(row['asset'],'unexpected mid decimation change')
        assert max(abs(new['bounds_y_up'][i][j]-old['bounds_y_up'][i][j]) for i in range(3) for j in range(2))<.0001
        assert sha(SOURCE/'source_meshes'/row['source'])==row['source_sha256']
        result=dict(asset=row['asset'],triangles=new['triangles'],primitives=new['primitive_count'],
                    triangle_delta_from_previous=new['triangles']-old['triangles'],
                    identical_bounds=True,source_fbx_hash_unchanged=True)
        if row['lod']=='mid':
            assert new['primitive_count']==1
        else:
            if row['family'].startswith('house_'):assert new['primitive_count']>=3,'Source tile materials were collapsed into the building atlas'
            doc,old_triangles=triangles(baseline/path.name)
            old_index={r['key']:r for r in old_triangles}
            doc,new_triangles=triangles(path);groups={}
            for material in doc['materials']:
                if not material['name'].startswith('Purchased_Tile_'):continue
                stem=material['name'].removeprefix('Purchased_Tile_').removesuffix('_Mask')
                assert material.get('alphaMode')==('MASK' if material['name'].endswith('_Mask') else 'OPAQUE')
                if material['name'].endswith('_Mask'):assert abs(material['alphaCutoff']-.3333)<1e-6
                pbr=material['pbrMetallicRoughness'];_,binary=glb(path)
                for kind,index in [('BC',pbr['baseColorTexture']['index']),('N',material['normalTexture']['index']),('ARM',pbr['metallicRoughnessTexture']['index'])]:
                    _,data=image_data(doc,binary,index)
                    assert hashlib.sha256(data).hexdigest()==sha(SOURCE/'prepared'/(stem+'_'+kind+'.png'))
            # A single corrected atlas remains embedded for non-tile surfaces
            # and is identical to the mid LOD. Reuse its new pixels at the
            # previous (unchanged) near atlas UVs for the LOD palette check.
            # A near-only facade repack no longer contains roof/stone charts.
            # Compare source tiles against the unchanged complete MID atlas.
            mid_path=stage/('purchased_village_'+row['family']+'_mid.glb')
            if not mid_path.exists():mid_path=OUT/mid_path.name
            mid_doc,mid_binary=glb(mid_path)
            mid_texture=mid_doc['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index']
            atlas,_=image_data(mid_doc,mid_binary,mid_texture)
            assert set(t['key'] for t in new_triangles)==set(old_index)
            for t in new_triangles:
                assert t['key'] in old_index
                if not t['material'].startswith('Purchased_Tile_'): continue
                old_t=old_index[t['key']]
                # Match triangle corner ordering by exact source positions.
                order=[int(np.argmin(np.sum((old_t['xyz']-p)**2,axis=1))) for p in t['xyz']]
                assert len(set(order))==3
                atlas_uv=old_t['uv'][order]
                near_samples=sample(t['pixels'],bary@t['uv']);mid_samples=sample(atlas,bary@atlas_uv)
                common=(near_samples[:,3]>=t['cutoff'] if t['masked'] else np.ones(4,dtype=bool))&(mid_samples[:,3]>=.4)
                near_color=np.sum(near_samples[:,:3]*common[:,None],axis=0)/4.
                mid_color=np.sum(mid_samples[:,:3]*common[:,None],axis=0)/4.
                g=groups.setdefault(t['material'],dict(area=0.,new_texel_area=0.,old_texel_area=0.,
                    near_color=np.zeros(3),mid_color=np.zeros(3),palette_area=0.,triangles=0))
                g['area']+=t['area'];g['new_texel_area']+=t['density']**2*t['area'];g['old_texel_area']+=old_t['density']**2*t['area']
                g['near_color']+=near_color*t['area'];g['mid_color']+=mid_color*t['area'];g['triangles']+=1
                g['palette_area']+=float(np.mean(common))*t['area']
            result['tile_surfaces']=[]
            for name,g in groups.items():
                area=g['area'];assert g['palette_area']>0
                near=g['near_color']/g['palette_area'];mid=g['mid_color']/g['palette_area']
                density=math.sqrt(g['new_texel_area']/area);before=math.sqrt(g['old_texel_area']/area)
                delta=float(np.max(np.abs(near-mid)))
                assert delta<.065,(row['asset'],name,'near/mid broad linear albedo changed',delta)
                assert density>before*1.5,(row['asset'],name,'insufficient tile density improvement')
                result['tile_surfaces'].append(dict(material=name,triangles=g['triangles'],surface_area_m2=area,
                    old_atlas_texels_per_m=before,near_source_texels_per_m=density,density_ratio=density/before,
                    near_linear_rgb=near.tolist(),mid_atlas_linear_rgb=mid.tolist(),maximum_broad_linear_rgb_delta=delta))
        records.append(result)
    report=dict(schema=1,stage_validated=True,source_originals_modified=False,architecture_gpu_mib=gpu/1024/1024,
                model_count=len(records),records=records,
                palette_check='Four interior samples per matched original triangle, world-area weighted linear RGB; no lighting/exposure')
    (SOURCE/'material-fidelity-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('VALID source-UV fidelity:',len(records),'LODs;',round(gpu/1024/1024,2),'MiB unique architecture images',flush=True)
    for r in records:
        for t in r.get('tile_surfaces',[]):
            print(r['asset'],t['material'],round(t['density_ratio'],2),'x source texel density; broad linear RGB delta',round(t['maximum_broad_linear_rgb_delta'],4),flush=True)


if __name__=='__main__': main()

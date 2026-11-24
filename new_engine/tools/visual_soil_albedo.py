#!/usr/bin/env python3
"""Normalize the shared purchased wet-dark soil to dry, readable brown dirt.

Append a single consistently graded image to countryside and town-rim models.
Only albedo bindings change. Keep source images/geometry bytes as an exact BIN
prefix; no material strength, lighting, shader, placement or collision changes.
"""
from pathlib import Path
import argparse,copy,io,json
import numpy as np
from PIL import Image
from visual_ground_finish import ROOT,GLB,append_texture,write_glb,sha


def linear(a):return np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)
def srgb(a):return np.where(a<=.0031308,a*12.92,1.055*np.maximum(a,0)**(1/2.4)-.055)


def prepare(output):
    source=ROOT/'assets/purchased/bep_reforged/countryside-materials/textures/earthpath_base.png'
    original=np.asarray(Image.open(source).convert('RGB'),np.float64)/255.;a=linear(original)
    target=np.array([.10,.075,.05]);ceiling=.6;gains=[];graded=np.empty_like(a)
    # Bounded Reinhard shoulder preserves source variation and cannot make
    # isolated pebbles full-white when the very dark source is brought up.
    for c in range(3):
        low,high=0.,128.
        for _ in range(36):
            gain=(low+high)*.5;x=a[:,:,c]*gain;result=x/(1+x/ceiling)
            if result.mean()<target[c]:low=gain
            else:high=gain
        gains.append((low+high)*.5);x=a[:,:,c]*gains[-1];graded[:,:,c]=x/(1+x/ceiling)
    encoded=np.uint8(np.clip(np.rint(srgb(graded)*255),0,255));buf=io.BytesIO();Image.fromarray(encoded).save(buf,format='PNG',compress_level=3);raw=buf.getvalue()
    dest=output/'dry_shared_soil.png';dest.write_bytes(raw);actual=linear(encoded/255.)
    audit=dict(source=str(source),source_sha256=sha(source),output=str(dest),sha256=sha(raw),source_srgb_mean=original.mean((0,1)).tolist(),
        source_linear_mean=a.mean((0,1)).tolist(),target_linear_mean=target.tolist(),actual_linear_mean=actual.mean((0,1)).tolist(),
        source_linear_luminance=float(a.mean((0,1))@np.array([.2126,.7152,.0722])),actual_linear_luminance=float(actual.mean((0,1))@np.array([.2126,.7152,.0722])),
        source_linear_std=a.std((0,1)).tolist(),actual_linear_std=actual.std((0,1)).tolist(),actual_srgb_mean=(encoded/255.).mean((0,1)).tolist(),
        per_channel_linear_gains=gains,linear_soft_shoulder_ceiling=ceiling,actual_linear_max=actual.max((0,1)).tolist(),source_size=[original.shape[1],original.shape[0]],
        sRGB_decode_exactly_once_at_runtime=True,material_tint_unchanged=True,normal_strength_unchanged=True)
    return raw,audit


def build(project,output):
    project=Path(project).resolve();output=Path(output).resolve()
    if output.exists():raise ValueError('Use a fresh immutable output')
    output.mkdir(parents=True);raw,image_audit=prepare(output);models=[];replacements=[]
    for line in(project/'world.studio').read_text().splitlines():
        s=line.split()
        if len(s)<4 or s[0]!='asset':continue
        alias,prefix=s[1:3];town=alias=='nm_city_11_sm_ground_mesh'
        if not(alias.startswith('bep_country_terrain_')or town):continue
        for suffix,label in [('.glb','near'),('_mid.glb','mid')]:
            relative=Path('models')/(prefix+suffix);source=project/relative;old=GLB(source);doc=copy.deepcopy(old.doc)
            binary,tex=append_texture(doc,old.binary,raw);bindings=[]
            for i,mat in enumerate(doc['materials']):
                if town:
                    if mat['name']!='MI_POM_Ground':continue
                    bindings.append(dict(material=i,old_texture=mat['pbrMetallicRoughness']['baseColorTexture']['index'],new_texture=tex,slot='base_color'))
                    mat['pbrMetallicRoughness']['baseColorTexture']['index']=tex
                else:
                    params=mat.get('extensions',{}).get('VALWOOD_materials_facade',{}).get('parameters')
                    if not params or params[0]!=1 or params[1]!=2 or params[7]!=7.875:continue
                    bindings.append(dict(material=i,old_texture=params[4],new_texture=tex,slot='facade_soil_color_aux2'))
                    params[4]=tex
            if not bindings:raise ValueError(('Expected shared soil layer',source))
            dest=output/'models'/alias/('asset_'+label+'.glb');write_glb(dest,doc,binary);new=GLB(dest)
            exact=dict(original_binary_prefix_exact=new.binary[:len(old.binary)]==old.binary,meshes_attributes_indices_exact=new.doc['meshes']==old.doc['meshes'],
                original_accessor_views_exact=new.doc['accessors']==old.doc['accessors']and new.doc['bufferViews'][:len(old.doc['bufferViews'])]==old.doc['bufferViews'],
                nodes_and_scenes_exact=all(old.doc[k]==new.doc[k]for k in ['nodes','scenes']))
            if not all(exact.values()):raise ValueError('Source mesh changed')
            models.append(dict(alias=alias,lod=label,source=str(source),source_sha256=sha(source),output=str(dest),sha256=sha(dest),bindings=bindings,checks=exact))
            replacements.append(dict(relative_path=relative.as_posix(),candidate=str(dest),sha256=sha(dest)))
            if label=='near':replacements.append(dict(relative_path=(Path('models')/(prefix+'_near.glb')).as_posix(),candidate=str(dest),sha256=sha(dest)))
    if len(models)!=34 or len(replacements)!=51:raise ValueError('Expected all shared countryside+town soil bindings')
    report=dict(schema=1,status='SHARED_DRY_SOIL_ALBEDO_CANDIDATE',source_project=str(project),image=image_audit,models=models,replacements=replacements,
        new_shader_patch_required=False,uses_existing_ground_and_entry_blend_shaders=True,original_source_images_and_geometry_preserved=True,material_tints_normal_scales_roughness_metallic_unchanged=True,
        source_project_modified=False,physics_and_heightfields_modified=False,shared_image_identical_across_all_34_models=True,texture_GPU_size_unchanged=True,
        visual_GPU_review_pending=True)
    (output/'SOIL_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');return report


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--project',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args();r=build(args.project,args.output)
    print(json.dumps(dict(status=r['status'],models=len(r['models']),image=r['image']),indent=2))


if __name__=='__main__':main()

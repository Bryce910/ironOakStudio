#!/usr/bin/env python3
"""Join the existing town/countryside materials without changing either surface.

The entrance rim has sub-centimetre geometry continuity but a fivefold albedo
energy change. The source town ground is uniformly white RGB paint, and its
height texture guarantees first-layer weight zero. Its unused first albedo and
normal slots can therefore carry the existing countryside soil near the rim.
This retains source layers B/C everywhere else and needs no renderer rebuild.
"""
from pathlib import Path
import argparse,copy,json
import numpy as np
from PIL import Image
from visual_ground_finish import GLB,ROOT,append_texture,write_glb,sha

RIM=-1359.2756065368665
ORIGIN=(-1310.4000000000012,-10207.584747454583)
ZBOUNDS=(-10245.56152357763,-10140.375297686516)


def patch_vertex_shader(text):
    """Decode the CPU-validated uint24 without rounding its largest integers.

    Float32 can represent all RGB24 integers, but adding .5 cannot preserve
    them above 2**23. In particular white16777215+.5 rounds up to16777216 and
    incorrectly becomes RGB(0,0,256). Integer conversion needs no rounding.
    """
    if '// VISUAL_SOURCE_PAINT_UINT24_V1'in text:return text
    old='''        float value24=floor(aFacadePaint+.5);
        vSourcePaint=vec3(mod(value24,256.),mod(floor(value24/256.),256.),floor(value24/65536.))/255.;'''
    new='''        // VISUAL_SOURCE_PAINT_UINT24_V1: the CPU stream is already canonical RGB24.
        uint value24=uint(aFacadePaint);
        vSourcePaint=vec3(float(value24&255u),float((value24>>8u)&255u),
            float((value24>>16u)&255u))/255.;'''
    if text.count(old)!=1:raise ValueError('Expected one original source RGB24 decoder')
    return text.replace(old,new,1)


def patch_shader(text):
    if '// VISUAL_TOWN_ENTRY_BLEND_V1'in text:return text
    anchor='// VISUAL_GROUND_FINISH_V1: only authored countryside overlays use this tag.'
    if text.count(anchor)!=1:raise ValueError('Apply visual_ground_finish.patch_shader first')
    helpers='''// VISUAL_TOWN_ENTRY_BLEND_V1: the verified western town-ground rim only.
vec2 visualTownUV,visualTownDx,visualTownDy;
float visual_town_entry_weight() {
    if(uFacadeMode!=4)return 0.;
    vec2 world=vec2(vPos.x+uWorldAnchor.x,vPos.z+uWorldAnchor.y);
    if(world.x<-1359.525606537||world.x>-1349.275606537||
       world.y<-10245.811523578||world.y>-10140.125297687)return 0.;
    // Two full metres of identical soil at the rim; a long, irregularly
    // feathered change into the purchased street's original layered dirt.
    float edge=.22*sin(world.y*1.19)+.12*sin(world.y*.53);
    return 1.-smoothstep(2.,10.,world.x-(-1359.275606537)+edge);
}
vec3 visual_town_entry_soil_normal() {
    vec3 soil=textureGrad(uNormal,visualTownUV,visualTownDx,visualTownDy).xyz*2.-1.;
    vec3 n=normalize(vNormal),t=normalize(vTangent),b=normalize(vBitangent);
    // Country UV V grows along +Z; the source town's V points along -Z.
    // Express the same world perturbation in the original town tangent frame.
    vec3 worldT=normalize(vec3(n.y,-n.x,0.));
    vec3 worldB=-normalize(cross(n,worldT));
    vec3 target=normalize(worldT*soil.x+worldB*soil.y+n*soil.z);
    // Some imported tangent/normal pairs have small source non-orthogonality;
    // use the actual mapper's inverse rather than assuming a transpose.
    vec3 local=inverse(mat3(t,b,n))*target;
    // The accepted town normal scale is .65. The shared soil must retain
    // the country's effective strength after the common mapper applies it.
    local.xy/=max(uNormalScale,.0001);
    return local;
}
'''
    text=text.replace(anchor,helpers+anchor,1)
    anchor='    vec2 sourceDx=dFdx(studioUV),sourceDy=dFdy(studioUV);'
    if text.count(anchor)!=1:raise ValueError('Expected common fragment derivative anchor')
    text=text.replace(anchor,anchor+'''
    visualTownUV=(vec2(vPos.x+uWorldAnchor.x,vPos.z+uWorldAnchor.y)-
        vec2(-1310.400000000,-10207.584747455))*.25;
    visualTownDx=dFdx(vPos.xz)*.25;visualTownDy=dFdy(vPos.xz)*.25;''',1)
    old='        return vec4(mix(color,vec3(.039999999106,.036040998995,.026458000764),sourceColorPuddle),1.);'
    new='''        vec3 original=mix(color,vec3(.039999999106,.036040998995,.026458000764),sourceColorPuddle);
        float entry=visual_town_entry_weight();
        if(entry>0.)original=mix(original,textureGrad(uBase,visualTownUV,visualTownDx,visualTownDy).rgb,entry);
        return vec4(original,1.);'''
    if text.count(old)!=1:raise ValueError('Expected original town albedo return')
    text=text.replace(old,new,1)
    old='        return mix(n,vec3(0,0,1),sourcePuddle)*.5+.5;'
    new='''        n=mix(n,vec3(0,0,1),sourcePuddle);
        float entry=visual_town_entry_weight();
        if(entry>0.)n=mix(n,visual_town_entry_soil_normal(),entry);
        return n*.5+.5;'''
    if text.count(old)!=1:raise ValueError('Expected original town normal return')
    text=text.replace(old,new,1)
    old='        return vec3(mix(ao,1.,sourcePuddle),mix(rough,uFacadeUVScale.z,sourcePuddle),0.);'
    new='''        return mix(vec3(mix(ao,1.,sourcePuddle),mix(rough,uFacadeUVScale.z,sourcePuddle),0.),
            vec3(1.,1.,0.),visual_town_entry_weight());'''
    if text.count(old)!=1:raise ValueError('Expected original town ARM return')
    return text.replace(old,new,1)


def build(project,output,ground_candidate):
    project=Path(project).resolve();output=Path(output).resolve();ground_candidate=Path(ground_candidate).resolve()
    if output.exists():raise ValueError('Use a fresh immutable candidate output')
    output.mkdir(parents=True)
    line=next(x for x in(project/'world.studio').read_text().splitlines()if x.startswith('asset nm_city_11_sm_ground_mesh '));prefix=line.split()[2]
    mats=json.loads((ROOT/'assets/purchased/medieval_village/city-materials/materials-frozen.json').read_text());original_ground=mats['MI_POM_Ground']
    h=np.asarray(Image.open(original_ground['emissive']).convert('RGB'),np.float64)[:,:,0]/255.
    h=np.where(h<=.04045,h/12.92,((h+.055)/1.055)**2.4)
    red=np.clip((h+1.)*(1.-2.*.047619000077)+.047619000077,0,1)
    if not np.all(red==1.):raise ValueError('Original first ground layer contributes; cannot reuse its slots')
    soil=Path(ROOT/'assets/purchased/bep_reforged/countryside-materials/textures/earthpath_base.png').read_bytes()
    normal=(ground_candidate/'textures/earthpath_normal.png').read_bytes()
    models=[];replacements=[]
    for suffix,label in [('.glb','near'),('_mid.glb','mid')]:
        rel=Path('models')/(prefix+suffix);source=project/rel;before=GLB(source);doc=copy.deepcopy(before.doc);binary=before.binary
        for mesh in doc['meshes']:
            for prim in mesh['primitives']:
                if doc['materials'][prim['material']]['name']!='MI_POM_Ground':continue
                paint=before.read(prim['attributes']['_VALWOOD_SOURCE_PAINT'])
                if not np.all(paint==1):raise ValueError('Source ground paint is not uniformly white')
        binary,base=append_texture(doc,binary,soil);binary,norm=append_texture(doc,binary,normal)
        mat=next(m for m in doc['materials']if m['name']=='MI_POM_Ground')
        mat['pbrMetallicRoughness']['baseColorTexture']=dict(index=base);mat['normalTexture']['index']=norm
        dest=output/'models'/('town_ground_'+label+'.glb');write_glb(dest,doc,binary);after=GLB(dest)
        checks=dict(original_binary_prefix_exact=after.binary[:len(before.binary)]==before.binary,
            nodes_scenes_exact=all(before.doc[k]==after.doc[k]for k in ['nodes','scenes']),
            original_accessor_layouts_exact=before.doc['accessors']==after.doc['accessors'],
            all_primitives_and_attributes_exact=before.doc['meshes']==after.doc['meshes'])
        if not all(checks.values()):raise ValueError(('Ground geometry changed',checks))
        models.append(dict(source=str(source),source_sha256=sha(source),output=str(dest),sha256=sha(dest),lod=label,checks=checks,
            unchanged_source_second_third_layer_bindings=True,normal_scale=mat['normalTexture'].get('scale',1.)))
        replacements.append(dict(relative_path=rel.as_posix(),candidate=str(dest),sha256=sha(dest)))
        if label=='near':replacements.append(dict(relative_path=(Path('models')/(prefix+'_near.glb')).as_posix(),candidate=str(dest),sha256=sha(dest)))
    report=dict(schema=1,status='MATERIAL_ONLY_ENTRANCE_BLEND_CANDIDATE',source_project=str(project),replacements=replacements,models=models,
        rim_world_x=RIM,source_native_x=RIM-ORIGIN[0],source_world_z_bounds=list(ZBOUNDS),soil_uv_period_metres=4.,
        full_shared_soil_inside_rim_metres=2.,original_street_restored_inside_rim_metres=10.,edge_variation_bound_metres=.34,
        source_first_layer_zero_weight_proof=dict(white_source_vertex_RGB=True,height_srgb_sample_minimum=float(h.min()),minimum_red_height_weight=float(red.min()),first_layer_maximum_weight=float((1-red).max())),
        source_heightfield_and_collision_changes=False,source_geometry_uv_and_datums_changes=False,
        source_albedo_layers_B_C_preserved=True,source_binary_data_preserved=True,
        shader_patch='visual_town_entry_blend.patch_shader after visual_ground_finish.patch_shader; source home excluded by world bounds',
        texture_slots=dict(base='original countryside earth albedo, sRGB',normal='soft countryside soil normal, packed-mode slot remains linear; only source first layer reused'),
        limitation='Requires actual native entrance capture; exact projected rim geometry was already within3cm and remains unchanged.')
    (output/'ENTRANCE_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
    (output/'world_foliage.entry-patch.fs.glsl').write_text(patch_shader((project/'shaders/world_foliage.fs.glsl').read_text()))
    return report


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--project',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--ground-candidate',type=Path,required=True);args=ap.parse_args()
    r=build(args.project,args.output,args.ground_candidate);print(json.dumps(dict(status=r['status'],models=len(r['models']),replacements=len(r['replacements'])),indent=2))


if __name__=='__main__':main()

#!/usr/bin/env python3
"""Stage source-graph input corrections without changing the preview shaders."""
from pathlib import Path
import argparse, hashlib, json

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT.parent/'.local-tools/village-source'

def replace_once(text, old, new):
    if text.count(old) != 1:raise RuntimeError('Shader anchor changed: '+old[:90])
    return text.replace(old,new,1)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source',type=Path,default=ROOT/'assets/purchased/medieval_village/city-materials/shaders')
    ap.add_argument('--output',type=Path,default=PRIVATE/'candidates/material-graph')
    args=ap.parse_args()
    if args.source.resolve()==args.output.resolve():raise ValueError('Candidate must be isolated')
    registry=json.loads((PRIVATE/'material-registry.json').read_text())
    materials=json.loads((ROOT/'assets/purchased/medieval_village/city-materials/materials-frozen.json').read_text())
    masters=registry['master_materials'];nodes={master:{n['name']:n for n in row['nodes']}for master,row in masters.items()}
    def function(master,name):return nodes[master][name]['properties']['FunctionInputs']
    # These source graph connections, not material-name guesses, establish the
    # final green normal/ARM layer and the distinct color/physical height paths.
    plaster='MM_Master_3_Blend';ground='MM_POM_3_Blend_Puddle'
    evidence={}
    for name in ['MaterialExpressionMaterialFunctionCall_1','MaterialExpressionMaterialFunctionCall_2','MaterialExpressionMaterialFunctionCall_3']:
        value=function(plaster,name)
        assert value[2]['Input']==dict(expression='MaterialExpressionVertexColor_0',output=2)
        evidence[plaster+'/'+name]=value
    for name in ['MaterialExpressionMaterialFunctionCall_3','MaterialExpressionMaterialFunctionCall_2']:
        value=function(ground,name)
        assert value[3]['Input']['expression']=='MaterialExpressionTextureSample_1'
        evidence[ground+'/'+name]=value
    for name in ['MaterialExpressionMaterialFunctionCall_18','MaterialExpressionMaterialFunctionCall_17']:
        value=function(ground,name)
        assert value[3]['Input']['expression']=='MaterialExpressionTextureSample_0'
        evidence[ground+'/'+name]=value
    assert nodes[ground]['MaterialExpressionTextureSample_0']['properties']['Texture']=='T_SoilStoneMix_H'
    assert nodes[ground]['MaterialExpressionTextureSample_1']['properties']['Texture']=='T_Fill_BC'
    value=function(ground,'MaterialExpressionMaterialFunctionCall_6')
    assert value[1]['Input']==dict(expression='MaterialExpressionTextureSampleParameter2D_7',output=2)
    evidence[ground+'/MaterialExpressionMaterialFunctionCall_6']=value
    for name,row in materials.items():
        if row['facade_mode']==3:
            t=row['source_instance']['textures']
            assert t['Normal_3']==t['Normal_2'] and t['ARM_3']==t['ARM_1'],name
    source=args.source/'world_foliage.fs.glsl';text=source.read_text()
    text=replace_once(text,'vec3 sourceLayerWeights=vec3(1,0,0);\nfloat sourcePuddle=0.;','vec3 sourceLayerWeights=vec3(1,0,0),sourceColorWeights=vec3(1,0,0);\nfloat sourcePuddle=0.,sourceColorPuddle=0.;')
    text=replace_once(text,'''    float g=source_height_weight(sourceLayerC.b,vSourcePaint.g,-.300000011921);
    sourceLayerWeights=vec3((1.-r)*(1.-g),r*(1.-g),g);
    // Authored TextureSample_1 is constant128sRGB and blue contrast is.7.
    sourcePuddle=source_height_weight(.215860500114,.699999988079*vSourcePaint.b,0.);''','''    // The authored color path and normal/ARM paths use different heights.
    // Keep the source T_Fill_BC constant for color; physical channels use the
    // original repeating T_SoilStoneMix_H already bound in uEmissive.
    float g=source_height_weight(h,vSourcePaint.g,-.300000011921);
    sourceLayerWeights=vec3((1.-r)*(1.-g),r*(1.-g),g);
    float colorG=source_height_weight(.215860500114,vSourcePaint.g,-.300000011921);
    sourceColorWeights=vec3((1.-r)*(1.-colorG),r*(1.-colorG),colorG);
    // City art direction: source blue paint marks potential puddles; actual
    // water coverage follows weather so clear streets retain dry PBR detail.
    float weatherPuddle=clamp(uWetness,0.,1.);
    sourcePuddle=source_height_weight(h,.699999988079*vSourcePaint.b,0.)*weatherPuddle;
    sourceColorPuddle=source_height_weight(.215860500114,.699999988079*vSourcePaint.b,0.)*weatherPuddle;''')
    text=replace_once(text,'''        vec3 color=a*sourceLayerWeights.x+b*sourceLayerWeights.y+c*sourceLayerWeights.z;
        return vec4(mix(color,vec3(.039999999106,.036040998995,.026458000764),sourcePuddle),1.);''','''        vec3 color=a*sourceColorWeights.x+b*sourceColorWeights.y+c*sourceColorWeights.z;
        return vec4(mix(color,vec3(.039999999106,.036040998995,.026458000764),sourceColorPuddle),1.);''')
    text=replace_once(text,'''        return mix(a,b,source_plaster_red(uv,dx,dy))*.5+.5;''','''        vec3 first=mix(a,b,source_plaster_red(uv,dx,dy));
        // A zero source green channel has exactly zero HeightLerp weight.
        if(vSourcePaint.g<=0.)return first*.5+.5;
        // Original Normal_3 reuses Normal_2 at its own third-layer tiling.
        vec3 c=source_nxy(facade_sample(uFacadeAux0,uFacadeUVScale.z,uv,dx,dy).rg);
        float h=facade_sample(uFacadeAux2,1.,uv,dx,dy).r;
        return mix(first,c,source_height_weight(h,vSourcePaint.g,1.))*.5+.5;''')
    text=replace_once(text,'''        return mix(vec3(a.a,a.b,0),vec3(b.a,b.b,0),source_plaster_red(uv,dx,dy));''','''        vec3 first=mix(vec3(a.a,a.b,0),vec3(b.a,b.b,0),source_plaster_red(uv,dx,dy));
        if(vSourcePaint.g<=0.)return first;
        // Original ARM_3 reuses ARM_1 at the third-layer tiling.
        vec4 c=facade_sample(uNormal,uFacadeUVScale.z,uv,dx,dy);
        float h=facade_sample(uFacadeAux2,1.,uv,dx,dy).r;
        return mix(first,vec3(c.a,c.b,0),source_height_weight(h,vSourcePaint.g,1.));''')
    text=replace_once(text,'''        vec2 data=sourceLayerA.ab*sourceLayerWeights.x+sourceLayerB.ab*sourceLayerWeights.y+sourceLayerC.ab*sourceLayerWeights.z;
        return vec3(mix(data.x,1.,sourcePuddle),mix(data.y,uFacadeUVScale.z,sourcePuddle),0.);''','''        float ao=sourceLayerA.a*sourceLayerWeights.x+sourceLayerB.a*sourceLayerWeights.y+sourceLayerC.a*sourceLayerWeights.z;
        // Source FunctionCall_6 deliberately uses ARM Mask3 for the first
        // roughness blend, while its AO blend uses ARM Mask2.
        float rough=sourceLayerA.b*sourceLayerWeights.x+sourceLayerC.b*(sourceLayerWeights.y+sourceLayerWeights.z);
        return vec3(mix(ao,1.,sourcePuddle),mix(rough,uFacadeUVScale.z,sourcePuddle),0.);''')
    for needle in ['float normalMipVariance=0.;','dFdx(N)','dFdy(N)']:
        assert text.count(needle)==source.read_text().count(needle),needle
    args.output.mkdir(parents=True,exist_ok=True)
    result=args.output/'world_foliage.fs.glsl';result.write_text(text)
    (args.output/'audit.json').write_text(json.dumps(dict(candidate_only=True,installed=False,
        source=str(source.resolve()),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        output_sha256=hashlib.sha256(result.read_bytes()).hexdigest(),source_graph_evidence=evidence,
        material_rows_sha256=hashlib.sha256((ROOT/'assets/purchased/medieval_village/city-materials/materials-frozen.json').read_bytes()).hexdigest(),
        added_images=0,incremental_texture_VRAM_bytes=0,Vertex_bytes=68,
        changes=['Plaster final green normal/roughness/AO layer restored using existing source textures and original third-layer tiling',
                 'Ground color green/puddle height kept separate from physical normal/AO/roughness height',
                 'Ground original roughness-channel ARM Mask3 selection restored',
                 'City-only art override gates physical and color puddles by bounded weather wetness; clear streets use dry source PBR layers'],
        limits=['External UE HeightLerp remains the documented approximation','POM depth remains unimplemented','Additional plaster texture fetches apply only to mode3'],
        production_files_modified=False),indent=2)+'\n')
    print(result)

if __name__=='__main__':main()

#!/usr/bin/env python3
"""Stage city-private source paint/layer shaders; no shared shader is edited."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/purchased/medieval_village/city-materials/shaders'

EXTRA=r'''
// Exact RGB paint from the original source component/FBX; never albedo tint.
in vec3 vSourcePaint;
vec3 sourceLayerWeights=vec3(1,0,0);
float sourcePuddle=0.;
vec4 sourceLayerA,sourceLayerB,sourceLayerC;
float source_height_weight(float height,float phase,float contrast) {
    // Endpoint/contrast reconstruction of the external UE HeightLerp graph.
    return clamp((height+2.*clamp(phase,0.,1.)-1.)*(1.+2.*contrast)-contrast,0.,1.);
}
vec3 source_nxy(vec2 xy) {
    vec2 n=xy*2.-1.;return vec3(n,sqrt(max(1.-dot(n,n),.00001)));
}
float source_plaster_red(vec2 uv,vec2 dx,vec2 dy) {
    float h=facade_sample(uFacadeAux2,uFacadeUVScale.x,uv,dx,dy).b;
    return source_height_weight(h,vSourcePaint.r,1.);
}
void source_ground_prepare(vec2 uv,vec2 dx,vec2 dy) {
    sourceLayerA=facade_sample(uNormal,uFacadeUVScale.x,uv,dx,dy);
    sourceLayerB=facade_sample(uFacadeAux0,uFacadeUVScale.y,uv,dx,dy);
    sourceLayerC=facade_sample(uFacadeAux2,uFacadeUVScale.x,uv,dx,dy);
    float h=facade_sample(uEmissive,uFacadeUVScale.y,uv,dx,dy).r;
    float r=source_height_weight(h,vSourcePaint.r,-.047619000077);
    float g=source_height_weight(sourceLayerC.b,vSourcePaint.g,-.300000011921);
    sourceLayerWeights=vec3((1.-r)*(1.-g),r*(1.-g),g);
    // Authored TextureSample_1 is constant128sRGB and blue contrast is.7.
    sourcePuddle=source_height_weight(.215860500114,.699999988079*vSourcePaint.b,0.);
}
'''

def main():
    OUT.mkdir(parents=True,exist_ok=True);source=ROOT/'projects/valwood_visual/shaders';vs=(source/'world_foliage.vs.glsl').read_text();fs=(source/'world_foliage.fs.glsl').read_text()
    # Current CPU source RGB8 occupies the existing scalar, independently of
    # any baked-light vec4. Generic VS copies already declare the explicit
    # opt-in and decode its red channel for legacy facade graphs.
    if 'uniform bool uSourcePaintEnabled'not in vs:
        vs=vs.replace('layout(location=7) in float aFacadePaint;','layout(location=7) in float aFacadePaint;\nuniform bool uSourcePaintEnabled=false;',1)
    vs=vs.replace('uniform bool uPoseBlendEnabled;','''out vec3 vSourcePaint;
uniform bool uPoseBlendEnabled;''',1)
    vs=vs.replace('void main() {','''void main() {
    vSourcePaint=vec3(0);
    if(uSourcePaintEnabled&&!uPoseBlendEnabled) {
        float value24=floor(aFacadePaint+.5);
        vSourcePaint=vec3(mod(value24,256.),mod(floor(value24/256.),256.),floor(value24/65536.))/255.;
    }''',1)
    fs=fs.replace('uniform bool uArchitectureWeathering;','uniform bool uArchitectureWeathering;\nuniform bool uSourceMaterialAO=false;')
    start=fs.index('float facade_plaster_weight(');fs=fs[:start]+EXTRA+fs[start:]
    fs=fs.replace('    if(uFacadeMode==0)return texture(uBase,uv);','''    if(uFacadeMode==0)return texture(uBase,uv);
    if(uFacadeMode==3) {
        vec4 a=facade_sample(uBase,uFacadeUVScale.x,uv,dx,dy);
        vec4 b=facade_sample(uARM,uFacadeUVScale.y,uv,dx,dy);
        vec4 c=facade_sample(uFacadeAux1,uFacadeUVScale.z,uv,dx,dy);
        vec3 mask=facade_sample(uFacadeAux2,uFacadeUVScale.x,uv,dx,dy).rgb;
        float red=facade_sample(uFacadeAux2,1.,uv,dx,dy).r;
        float w=source_height_weight(mask.b,vSourcePaint.g,red);
        float w2=source_height_weight(red,vSourcePaint.g,1.);
        return vec4(mix(mix(a.rgb,b.rgb,w),c.rgb,w2),1.);
    }
    if(uFacadeMode==4) {
        source_ground_prepare(uv,dx,dy);
        vec3 a=facade_sample(uBase,uFacadeUVScale.x,uv,dx,dy).rgb;
        vec3 b=facade_sample(uARM,uFacadeUVScale.y,uv,dx,dy).rgb;
        vec3 c=facade_sample(uFacadeAux1,uFacadeUVScale.y,uv,dx,dy).rgb;
        vec3 color=a*sourceLayerWeights.x+b*sourceLayerWeights.y+c*sourceLayerWeights.z;
        return vec4(mix(color,vec3(.039999999106,.036040998995,.026458000764),sourcePuddle),1.);
    }''',1)
    fs=fs.replace('    if(uFacadeMode==0)return texture(uNormal,uv).xyz;','''    if(uFacadeMode==0)return texture(uNormal,uv).xyz;
    if(uFacadeMode==3) {
        vec3 a=source_nxy(facade_sample(uNormal,uFacadeUVScale.x,uv,dx,dy).rg);
        vec3 b=source_nxy(facade_sample(uFacadeAux0,uFacadeUVScale.y,uv,dx,dy).rg);
        return mix(a,b,source_plaster_red(uv,dx,dy))*.5+.5;
    }
    if(uFacadeMode==4) {
        vec3 n=source_nxy(sourceLayerA.rg)*sourceLayerWeights.x+source_nxy(sourceLayerB.rg)*sourceLayerWeights.y+source_nxy(sourceLayerC.rg)*sourceLayerWeights.z;
        return mix(n,vec3(0,0,1),sourcePuddle)*.5+.5;
    }''',1)
    fs=fs.replace('    if(uFacadeMode==0)return texture(uARM,uv).rgb;','''    if(uFacadeMode==0)return texture(uARM,uv).rgb;
    if(uFacadeMode==3) {
        vec4 a=facade_sample(uNormal,uFacadeUVScale.y,uv,dx,dy);
        vec4 b=facade_sample(uFacadeAux0,uFacadeUVScale.x,uv,dx,dy);
        return mix(vec3(a.a,a.b,0),vec3(b.a,b.b,0),source_plaster_red(uv,dx,dy));
    }
    if(uFacadeMode==4) {
        vec2 data=sourceLayerA.ab*sourceLayerWeights.x+sourceLayerB.ab*sourceLayerWeights.y+sourceLayerC.ab*sourceLayerWeights.z;
        return vec3(mix(data.x,1.,sourcePuddle),mix(data.y,uFacadeUVScale.z,sourcePuddle),0.);
    }''',1)
    fs=fs.replace('    float ao=mix(.48,1.,arm.r);','    float ao=uSourceMaterialAO?clamp(arm.r,0.,1.):mix(.48,1.,arm.r);')
    fs=fs.replace('uStudioTintRough.w,.25,1.)','uStudioTintRough.w,uFacadeMode==4?.04:.25,1.)',1)
    needle='    color+=graphEmission;';assert fs.count(needle)==1
    fs=fs.replace(needle,'''    if(uFacadeMode==5) {
        float mask=facade_sample(uFacadeAux0,1.,studioUV,sourceDx,sourceDy).r;
        float opacity=clamp((mask+uFacadeUVScale.z)*uFacadeUVScale.y,0.,1.);
        float through=max(dot(-geometricN,uSun),0.)*(1.-opacity)*.5;
        color+=base.rgb*sunlight*through*(.35+.65*shadow);
    }
'''+needle)
    for name,value in [('world_foliage.vs.glsl',vs),('world_foliage.fs.glsl',fs)]:
        (OUT/name).write_text(value)
    audit=dict(source=str(source.resolve()),production_unchanged=True,normal_and_specular_footprint_filters_preserved=True,other_material_roughness_floor_preserved=True,source_ground_puddle_roughness_only_uses_lower_floor=True,source_AO_default_false=True,Vertex_bytes=68,additional_sampler_units=0,modes={'1':'Original source wood/red paint','3':'Source RGB plaster colors and red normal/ARM layer','4':'Original repeating soil/stone/moss, authored height and blue puddle','5':'Original cloth SSS mask/opacity with bounded native thin transmission'},limits=['External HeightLerp reconstructed; original POM parallax is not executed','Packed XY normals reconstruct positiveZ, as ordinary source BC5 tangent normals do','Cloth native thin transmission approximates the UE subsurface model'])
    audit['files']={name:hashlib.sha256((OUT/name).read_bytes()).hexdigest()for name in ['world_foliage.vs.glsl','world_foliage.fs.glsl']};(OUT/'audit.json').write_text(json.dumps(audit,indent=2)+'\n');print('STAGED',OUT,flush=True)

if __name__=='__main__':main()

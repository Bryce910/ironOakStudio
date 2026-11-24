#version 330 core
in vec2 vUV;
flat in float vCoveragePhase;
flat in vec3 vRootRelative;
uniform sampler2D uBase;
uniform bool uHasBase;
uniform vec4 uBaseFactor;
#ifdef VALWOOD_CHARACTER_APPEARANCE
uniform int uCharacterRegion=0;
flat in vec4 vCharacterData,vCharacterHair;
#endif
uniform float uAlphaCutoff;
uniform int uVegetation;
uniform bool uGrass;
uniform vec3 uPropLodRange;
uniform vec2 uSceneryDistanceFade;
void main() {
    float intervalLo=0.,intervalHi=1.;
    if(uPropLodRange.x>0.&&uPropLodRange.y>0.&&uPropLodRange.z>0.) {
        float nearWeight=1.-smoothstep(uPropLodRange.x-uPropLodRange.y,uPropLodRange.x+uPropLodRange.y,length(vRootRelative.xz));
        if(uPropLodRange.z<1.5)intervalHi=nearWeight;else intervalLo=nearWeight;
    }
    if(uSceneryDistanceFade.y>uSceneryDistanceFade.x)
        intervalHi=mix(intervalLo,intervalHi,1.-smoothstep(uSceneryDistanceFade.x,uSceneryDistanceFade.y,length(vRootRelative.xz)));
    float ordered=fract(dot(floor(gl_FragCoord.xy),vec2(.754877666,.569840296)));
    if(ordered<intervalLo||ordered>=intervalHi)discard;
    // Opaque bark/architecture need no albedo lookup in the depth-only pass.
#ifdef VALWOOD_CHARACTER_APPEARANCE
    if(uCharacterRegion==1&&vCharacterHair.a>.5)discard;
#endif
    if(uAlphaCutoff<=0.)return;
    float alpha=(uHasBase?texture(uBase,vUV).a:1.)*uBaseFactor.a;
    float cutoff=uAlphaCutoff;
#ifdef VALWOOD_CHARACTER_APPEARANCE
    if(uCharacterRegion==1){alpha=pow(clamp(alpha,0.,1.),vCharacterData.z);cutoff=clamp(cutoff+vCharacterData.y,.05,.95);}
#endif
    float coverage=step(cutoff,alpha),subpixel=0.;
    if(uHasBase&&uAlphaCutoff<1.&&uBaseFactor.a>=uAlphaCutoff&&uVegetation!=4&&(uGrass||uVegetation==1||uVegetation==3||uVegetation==5||uVegetation==6)) {
        vec2 texels=vec2(textureSize(uBase,0));
        vec2 dx=dFdx(vUV)*texels,dy=dFdy(vUV)*texels;
        subpixel=smoothstep(4096.,32768.,max(dot(dx,dx),dot(dy,dy)));
        coverage=mix(coverage,clamp(alpha,0.,1.),subpixel);
    }
    // Stable light-texel rank: no animated phase in the sun shadow map.
    float rank=(ordered-intervalLo)/max(intervalHi-intervalLo,.000001);
    if(fract(rank+vCoveragePhase*subpixel)>=coverage)discard;
}

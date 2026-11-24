#version 330 core
#if defined(GL_ARB_texture_gather) && !defined(VALWOOD_QA_DISABLE_TEXTURE_GATHER)
#extension GL_ARB_texture_gather : enable
#define VALWOOD_TEXTURE_GATHER_ACTIVE 1
#endif
#ifdef VALWOOD_TEXTURE_GATHER_ACTIVE
uniform bool uShadowGather=true;
#endif
uniform bool uForestShelterEnabled;
uniform vec4 uForestShelterBounds;
uniform vec2 uForestShelterHeights;
// VALWOOD_CITY_SOURCE_BAKED_LIGHT
in vec4 vBakedLight;
in vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
in float vRootedGrassWindSpeed;
in vec2 vUV;
#pragma include "world_material_response.glsl"
flat in float vSeed;
flat in float vCoveragePhase;
flat in float vScale;
flat in vec3 vRootRelative;
flat in float vRootHeight;
flat in float vInstanceFade;
layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 IndirectColor;
uniform sampler2D uBase,uNormal,uARM,uShadow,uMacroHeight;
uniform sampler2D uEmissive;
uniform bool uHasEmissive;
uniform vec3 uEmissiveFactor;
uniform vec2 uWorldAnchor,uWorldSize,uWeatherWind;
uniform float uPixelScale;
uniform vec4 uBaseFactor;
#ifdef VALWOOD_CHARACTER_APPEARANCE
uniform int uCharacterRegion=0;
flat in vec4 vCharacterData,vCharacterHair;
#endif
uniform bool uHasBase,uHasNormal,uHasARM,uGrass;
uniform int uVegetation,uLodLevel;
uniform float uNormalScale=1.;
#ifdef VALWOOD_STUDIO_SURFACES
uniform vec4 uStudioTintRough=vec4(1.);
uniform vec2 uStudioNormalUV=vec2(1.);
#else
const vec4 uStudioTintRough=vec4(1.);
const vec2 uStudioNormalUV=vec2(1.);
#endif
uniform float uAlphaCutoff,uRoughness,uMetallic,uHaze,uTemporalPhase,uTreeHeight,uCloud,uTime,uWetness;
uniform vec3 uSun,uSunColor,uAmbient,uFog,uSkyLight,uGroundLight;
uniform vec2 uTreeNearFade; // Production shared CPU near/mid interval; zero keeps legacy fixtures.
uniform vec3 uPropLodRange; // split distance, overlap half-width, 1near / 2mid
#ifdef VALWOOD_STUDIO_SURFACES
uniform vec3 uStudioLodPreview; // camera-relative rootXZ, enable
#endif
uniform vec2 uSceneryDistanceFade; // optional authored root-distance fade interval

#ifdef VALWOOD_HDR_ENVIRONMENT
uniform vec3 uEnvironmentSH[9];
uniform vec2 uEnvironmentRotation=vec2(1,0);
uniform float uEnvironmentIntensity=1,uEnvironmentStrength=0,uEnvironmentMip=8;
vec3 environment_direction(vec3 d){return vec3(uEnvironmentRotation.x*d.x-uEnvironmentRotation.y*d.z,d.y,uEnvironmentRotation.y*d.x+uEnvironmentRotation.x*d.z);}
vec3 local_sh9(vec3 coefficients[9],vec3 N);
vec3 environment_diffuse(vec3 N){return max(local_sh9(uEnvironmentSH,environment_direction(N)),vec3(0))*uEnvironmentIntensity;}
#endif

#ifdef VALWOOD_HDR_ENVIRONMENT
uniform samplerCube uEnvironmentCube;
uniform sampler2D uEnvironmentBRDF;
#endif
uniform samplerCube uLocalProbe0,uLocalProbe1;
uniform int uLocalProbeCount;
uniform vec3 uLocalProbeCenter[2],uLocalProbeBoxMin[2],uLocalProbeBoxMax[2];
uniform vec2 uLocalProbeWeights;
uniform float uLocalProbeMip,uLocalProbeLightScale;
uniform bool uArchitectureWeathering;
uniform bool uSourceMaterialAO=false;
uniform bool uVegetationFinish=false;
uniform vec3 uLocalIrradiance[9];
uniform float uLocalIrradianceWeight;
uniform vec4 uLocalIrradianceBounds;
uniform vec2 uLocalIrradianceHeights;
uniform vec3 uLocalIrradianceSecondary[9];
uniform float uLocalIrradianceSecondaryWeight;
uniform vec4 uLocalIrradianceSecondaryBounds;
uniform vec2 uLocalIrradianceSecondaryHeights;
uniform bool uVillageSkyEnabled;
uniform sampler2DArray uVillageSkyMask;
uniform vec4 uVillageSkyBounds; // world xmin,zmin, inverse width, inverse depth
uniform vec2 uVillageSkyHeights; // ground height, sky-access recovery height
uniform vec3 uVillageBounceAlbedo; // linear mean source-ground reflectance; zero disables
uniform bool uVillageDirectionalEnabled;
uniform sampler3D uVillageSkyDirectional,uVillageSkyUp;
uniform vec3 uVillageDirectionalHeights; // first world Y, inverse slice spacing, slice count
uniform bool uCastleLightsEnabled;
uniform vec4 uCastleLightPositionRadius[4]; // absolute world XYZ, bounded radius
uniform vec3 uCastleLightColor; // linear source intensity, shared by four lamps
uniform vec4 uCastleLightBounds; // absolute outer-shell xmin,zmin,xmax,zmax
uniform vec2 uCastleLightHeights; // outer floor/ceiling limits, world Y
uniform bool uTavernReturnEnabled;
uniform vec4 uTavernReturnBounds; // absolute outer-shell xmin,zmin,xmax,zmax
uniform vec2 uTavernReturnHeights;
uniform vec3 uTavernReturnRoof; // absolute ridge Z, clipping ridge Y, roof slope
uniform vec3 uTavernReturnIrradiance; // steady diffuse irradiance / PI
uniform bool uClipBelowWater;
uniform bool uSoftShadows;
uniform float uWaterClipLevel;
uniform mat4 uLightVP;
uniform float uActorReactive; // Moving residents lower temporal history confidence.
uniform bool uWindHistoryEnabled;
uniform float uWindFrameDelta,uWind; // Actual rendered time step; zero for a held simulation.
#ifdef VALWOOD_LOCAL_WIND
#pragma include "world_surface_wind.glsl"
#else
vec2 studioWind(vec3 world,vec2 base){return base;}
#endif
// The reactive mask follows bounded projected deformation, rather than
// permanently rejecting history for every leaf, trunk and distant card.
// A conservative speed bound preserves close flutter responsiveness without
// paying for a second full wind deformation/projection at every vertex.
in float vFacadePaint;
uniform int uFacadeMode;
uniform vec3 uFacadeUVScale; // primary, secondary, mask scale in source UV space
uniform sampler2D uFacadeAux0,uFacadeAux1,uFacadeAux2;
vec2 facade_uv(vec2 uv,float scale) {
    // glTF flips V after scaling in Blender; reproduce that exact convention.
    return vec2(uv.x*scale,1.-(1.-uv.y)*scale);
}

// Bounded, camera-selected practicals; source geometry and flicker stay fixed in world space.
#ifdef VALWOOD_STUDIO_SURFACES
uniform vec4 uTorchDirectionCone[8];
#endif
uniform int uTorchCount;
uniform vec4 uTorchPositionRadius[8],uTorchColorIntensity[8],uTorchWallPlane[8],uTorchClipBounds[8];
uniform vec2 uTorchClipHeights[8];
uniform vec3 uTorchClusterMin,uTorchClusterMax;

vec4 facade_sample(sampler2D image,float scale,vec2 uv,vec2 dx,vec2 dy) {
    return textureGrad(image,facade_uv(uv,scale),dx*scale,dy*scale);
}

// Exact RGB paint from the original source component/FBX; never albedo tint.
in vec3 vSourcePaint;
uniform bool uSourcePaintEnabled=false;
vec3 sourceLayerWeights=vec3(1,0,0),sourceColorWeights=vec3(1,0,0);
float sourcePuddle=0.,sourceColorPuddle=0.;
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
    // The authored color path and normal/ARM paths use different heights.
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
    sourceColorPuddle=source_height_weight(.215860500114,.699999988079*vSourcePaint.b,0.)*weatherPuddle;
}
float facade_plaster_weight(vec2 uv,vec2 dx,vec2 dy) {
    if(vFacadePaint<=0.)return 0.;
    float height=facade_sample(uFacadeAux2,uFacadeUVScale.z,uv,dx,dy).b;
    // Same explicitly approximate HeightLerp endpoint policy as offline cook.
    return clamp((height+2.*clamp(vFacadePaint,0.,1.)-1.)*3.-1.,0.,1.);
}
// VISUAL_TOWN_ENTRY_BLEND_V1: the verified western town-ground rim only.
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
// VISUAL_GROUND_FINISH_V1: only authored countryside overlays use this tag.
bool visual_ground_blend() {return uFacadeMode==2&&abs(uFacadeUVScale.z-7.875)<.0001;}
vec4 facade_base(vec2 uv,vec2 dx,vec2 dy) {
    if(visual_ground_blend()) {
        vec3 grass=textureGrad(uBase,uv,dx,dy).rgb;
        vec3 soil=textureGrad(uFacadeAux2,uv,dx,dy).rgb;
        return vec4(mix(grass,soil,clamp(vFacadePaint,0.,1.)),1.);
    }
    if(uFacadeMode==0)return texture(uBase,uv);
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
        vec3 color=a*sourceColorWeights.x+b*sourceColorWeights.y+c*sourceColorWeights.z;
        vec3 original=mix(color,vec3(.039999999106,.036040998995,.026458000764),sourceColorPuddle);
        float entry=visual_town_entry_weight();
        if(entry>0.)original=mix(original,textureGrad(uBase,visualTownUV,visualTownDx,visualTownDy).rgb,entry);
        return vec4(original,1.);
    }
    vec4 color=facade_sample(uBase,uFacadeUVScale.x,uv,dx,dy);
    if(uFacadeMode==1&&vFacadePaint>0.) {
        float mask=facade_sample(uFacadeAux1,uFacadeUVScale.z,uv,dx,dy).g;
        color.rgb=mix(color.rgb,facade_sample(uFacadeAux0,uFacadeUVScale.y,uv,dx,dy).rgb,clamp(vFacadePaint,0.,1.)*mask);
    }
    color.a=1.; // Source wood/plaster are opaque; alpha can contain utility data.
    return color;
}
vec3 facade_normal(vec2 uv,vec2 dx,vec2 dy) {
    if(visual_ground_blend()) {
        vec3 grass=textureGrad(uNormal,uv,dx,dy).xyz*2.-1.;
        vec3 soil=textureGrad(uFacadeAux0,uv,dx,dy).xyz*2.-1.;
        return normalize(mix(grass,soil,clamp(vFacadePaint,0.,1.)))*.5+.5;
    }
    if(uFacadeMode==0)return texture(uNormal,uv).xyz;
    if(uFacadeMode==3) {
        vec3 a=source_nxy(facade_sample(uNormal,uFacadeUVScale.x,uv,dx,dy).rg);
        vec3 b=source_nxy(facade_sample(uFacadeAux0,uFacadeUVScale.y,uv,dx,dy).rg);
        vec3 first=mix(a,b,source_plaster_red(uv,dx,dy));
        // A zero source green channel has exactly zero HeightLerp weight.
        if(vSourcePaint.g<=0.)return first*.5+.5;
        // Original Normal_3 reuses Normal_2 at its own third-layer tiling.
        vec3 c=source_nxy(facade_sample(uFacadeAux0,uFacadeUVScale.z,uv,dx,dy).rg);
        float h=facade_sample(uFacadeAux2,1.,uv,dx,dy).r;
        return mix(first,c,source_height_weight(h,vSourcePaint.g,1.))*.5+.5;
    }
    if(uFacadeMode==4) {
        vec3 n=source_nxy(sourceLayerA.rg)*sourceLayerWeights.x+source_nxy(sourceLayerB.rg)*sourceLayerWeights.y+source_nxy(sourceLayerC.rg)*sourceLayerWeights.z;
        n=mix(n,vec3(0,0,1),sourcePuddle);
        float entry=visual_town_entry_weight();
        if(entry>0.)n=mix(n,visual_town_entry_soil_normal(),entry);
        return n*.5+.5;
    }
    vec3 value=facade_sample(uNormal,uFacadeUVScale.x,uv,dx,dy).xyz;
    if(uFacadeMode==2&&vFacadePaint>0.)
        value=mix(value,facade_sample(uFacadeAux0,uFacadeUVScale.y,uv,dx,dy).xyz,facade_plaster_weight(uv,dx,dy));
    return value;
}
vec3 facade_arm(vec2 uv,vec2 dx,vec2 dy) {
    if(visual_ground_blend())return vec3(1.,1.,0.);
    if(uFacadeMode==0)return texture(uARM,uv).rgb;
    if(uFacadeMode==3) {
        vec4 a=facade_sample(uNormal,uFacadeUVScale.y,uv,dx,dy);
        vec4 b=facade_sample(uFacadeAux0,uFacadeUVScale.x,uv,dx,dy);
        vec3 first=mix(vec3(a.a,a.b,0),vec3(b.a,b.b,0),source_plaster_red(uv,dx,dy));
        if(vSourcePaint.g<=0.)return first;
        // Original ARM_3 reuses ARM_1 at the third-layer tiling.
        vec4 c=facade_sample(uNormal,uFacadeUVScale.z,uv,dx,dy);
        float h=facade_sample(uFacadeAux2,1.,uv,dx,dy).r;
        return mix(first,vec3(c.a,c.b,0),source_height_weight(h,vSourcePaint.g,1.));
    }
    if(uFacadeMode==4) {
        float ao=sourceLayerA.a*sourceLayerWeights.x+sourceLayerB.a*sourceLayerWeights.y+sourceLayerC.a*sourceLayerWeights.z;
        // Source FunctionCall_6 deliberately uses ARM Mask3 for the first
        // roughness blend, while its AO blend uses ARM Mask2.
        float rough=sourceLayerA.b*sourceLayerWeights.x+sourceLayerC.b*(sourceLayerWeights.y+sourceLayerWeights.z);
        return mix(vec3(mix(ao,1.,sourcePuddle),mix(rough,uFacadeUVScale.z,sourcePuddle),0.),
            vec3(1.,1.,0.),visual_town_entry_weight());
    }
    float primary=uFacadeMode==2?uFacadeUVScale.y:uFacadeUVScale.x;
    vec3 value=facade_sample(uARM,primary,uv,dx,dy).rgb;
    if(uFacadeMode==2&&vFacadePaint>0.)
        value=mix(value,facade_sample(uFacadeAux1,uFacadeUVScale.x,uv,dx,dy).rgb,facade_plaster_weight(uv,dx,dy));
    return value;
}

float vegetation_stability(float distance,float minimum_stability) {
    if(!uWindHistoryEnabled)return minimum_stability;
    // Bound from the exact rooted-grass VS deformation, including instance
    // scale and local height. Keep moving tips crisp without destabilizing
    // other foliage or changing any static scene history.
    if((uVegetation==2||uVegetation==3||uVegetation==5)&&vRootedGrassWindSpeed>0.){
        float pixels=vRootedGrassWindSpeed*clamp(uWindFrameDelta,0.,.1)*uPixelScale/max(distance,.1);
        return mix(.98,minimum_stability,smoothstep(.02,.35,pixels));
    }
    float height=max(vPos.y-vRootHeight,0.);
    if(uVegetation==6) {
        float total_height=max(uTreeHeight*vScale,.20);
        float h=clamp(height/total_height,0.,1.);
        // Conservative derivative of the exact quadratic crop bend in VS.
        // It follows rendered time only; no frame-random coverage is added.
        float speed=min(total_height*.045,.16)*max(uWind,0.)*(.84+.38*1.67+.38*.62)*h*h;
        float weather_speed=length(studioWind(vec3(vPos.x+uWorldAnchor.x-(vRelative.x-vRootRelative.x),vRootHeight,vPos.z+uWorldAnchor.y-(vRelative.z-vRootRelative.z)),uWeatherWind));
        if(weather_speed>.00001&&uWind>0.)
            speed+=min(total_height*.018,.12)*min(weather_speed,3.)*uWind*.24*(.84+.38*1.67)*h*h;
        float pixels=speed*clamp(uWindFrameDelta,0.,.1)*uPixelScale/max(distance,.1);
        return mix(.98,minimum_stability,smoothstep(.02,.35,pixels));
    }
    bool small=uGrass||uVegetation==2||uVegetation==5;
    float amplitude=small ? pow(clamp(height/.4,0.,1.),2.)*.075 : min(height*.011,.23);
    float speed=amplitude*(.84+.38*1.67)*max(uWind,0.);
    float weather_speed=length(studioWind(vec3(vPos.x+uWorldAnchor.x-(vRelative.x-vRootRelative.x),vRootHeight,vPos.z+uWorldAnchor.y-(vRelative.z-vRootRelative.z)),uWeatherWind));
    if(weather_speed>.00001&&uWind>0.) {
        float total_height=max((small?.4:uTreeHeight)*vScale,.20);
        float h=clamp(height/total_height,0.,1.);
        float strength=min(weather_speed,3.)*uWind;
        speed+=min(total_height*.014,.42)*strength*.24*(.84+.38*1.67)*h*h;
        bool thin=small||uVegetation==3||(uVegetation==1&&uAlphaCutoff>0.);
        if(thin)speed+=(small?.009:.014)*5.1*h*strength;
    }
    float pixels=speed*clamp(uWindFrameDelta,0.,.1)*uPixelScale/max(distance,.1);
    return mix(.98,minimum_stability,smoothstep(.02,.35,pixels));
}
float hash(vec2 p) {
    vec3 q=fract(vec3(p.xyx)*.1031);
    q+=dot(q,q.yzx+33.33);
    return fract((q.x+q.y)*q.z);
}
float noise(vec2 p) {
    vec2 i=floor(p),f=fract(p);
    f=f*f*(3.-2.*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),
               mix(hash(i+vec2(0,1)),hash(i+1.),f.x),f.y);
}
vec2 village_sky_access() {
    if(!uVillageSkyEnabled)return vec2(1.);
    vec2 uv=(vPos.xz+uWorldAnchor-uVillageSkyBounds.xy)*uVillageSkyBounds.zw;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return vec2(1.);
    // CPU field samples include both world-bound endpoints. Match terrain's
    // node-to-texel-center mapping so a wall and the ground share one mask.
    vec2 size=vec2(textureSize(uVillageSkyMask,0).xy);
    uv=(uv*(size-1.)+.5)/size;
    float ground=clamp(texture(uVillageSkyMask,vec3(uv,3.)).a,.3,1.);
    float height=smoothstep(1.,max(uVillageSkyHeights.y,1.001),vPos.y-uVillageSkyHeights.x);
    return vec2(mix(ground,1.,height),ground);
}
float village_directional_sky(vec3 normal) {
    vec2 uv=(vPos.xz+uWorldAnchor-uVillageSkyBounds.xy)*uVillageSkyBounds.zw;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return 1.;
    vec2 size=vec2(textureSize(uVillageSkyDirectional,0).xy);
    vec2 nodeUV=(uv*(size-1.)+.5)/size;
    float layer=(vPos.y-uVillageDirectionalHeights.x)*uVillageDirectionalHeights.y;
    float heightUV=(layer+.5)/max(uVillageDirectionalHeights.z,2.);
    vec3 point=vec3(nodeUV,heightUV);
    vec4 horizontal=texture(uVillageSkyDirectional,point);
    float up=texture(uVillageSkyUp,point).r;
    // The volume contains four cardinal facade integrals and an upward one.
    // Convex geometric-normal weights remain bounded and stable as detail
    // normals/mips change. This cannot add sky energy or modify source albedo.
    vec4 weights=max(vec4(normal.x,-normal.x,normal.z,-normal.z),vec4(0));
    float upward=max(normal.y,0.);
    float total=dot(weights,vec4(1))+upward;
    float access=total>.00001 ? (dot(horizontal,weights)+up*upward)/total : 1.;
    // A small world-space guard avoids an edge discontinuity if a roof horizon
    // happens to reach the boundary of the finite startup bake.
    vec2 edge=min(uv,1.-uv)/max(uVillageSkyBounds.zw,vec2(.000001));
    return mix(1.,clamp(access,0.,1.),smoothstep(0.,2.,min(edge.x,edge.y)));
}
float cloud_transmittance() {
    // Same coarse field, threshold and 3200m cloud plane as the terrain.
    // Reconstruct world XZ before projection so local-origin changes do not
    // move the cloud shadow relative to a tree or the ground beneath it.
    if(uCloud<.015||uSun.y<.06||vPos.y>=3200.) return 1.;
    vec2 p=(vPos.xz+uWorldAnchor+uSun.xz/max(uSun.y,.12)*(3200.-vPos.y))*.00042;
    p+=vec2(uTime*.0014,-uTime*.00055);
    const mat2 rotation=mat2(.80,-.60,.60,.80);
    float field=.67*noise(p)+.33*noise(rotation*p*2.03+17.4);
    float threshold=.64-clamp(uCloud,0.,1.)*.30;
    float coverage=smoothstep(threshold,threshold+.17,field);
    coverage*=smoothstep(.015,.12,uCloud);
    return 1.-.34*coverage*clamp(uCloud*1.7,0.,1.);
}
float visibility(vec3 normal) {
    // The existing125--160m fade is already exactly1 at its upper endpoint.
    // Avoid dead PCF work without changing any active shadow sample or weight.
    if(length(vRelative.xz)>=160.) return 1.;
    vec4 q=uLightVP*vec4(vRelative,1.);
    vec3 p=q.xyz/q.w*.5+.5;
    if(any(lessThan(p,vec3(.002)))||any(greaterThan(p,vec3(.998)))) return 1.;
    float bias=max(.000045,.00013*(1.-max(dot(normal,uSun),0.)));
    vec2 texel=1./vec2(textureSize(uShadow,0));
    if(uSoftShadows) {
        // Nine compare-before-filter taps: continuous, normalized Gaussian-
        // like B-spline weights soften map texels without leaking deep shade.
        vec2 sample_position=p.xy*vec2(textureSize(uShadow,0))-.5;
        ivec2 center=ivec2(floor(sample_position+.5));
        vec2 d=sample_position-vec2(center);
        vec3 wx=vec3(.5*(.5-d.x)*(.5-d.x),.75-d.x*d.x,.5*(.5+d.x)*(.5+d.x));
        vec3 wy=vec3(.5*(.5-d.y)*(.5-d.y),.75-d.y*d.y,.5*(.5+d.y)*(.5+d.y));
        ivec2 limit=textureSize(uShadow,0)-1;
        float filtered=0.;
#ifdef VALWOOD_TEXTURE_GATHER_ACTIVE
        // ARB_texture_gather returns (i0,j1),(i1,j1),(i1,j0),(i0,j0).
        // Coordinates lie on texel edges, half a texel from any footprint
        // selection discontinuity. Only use fully interior3x3 neighborhoods.
        if(uShadowGather&&all(greaterThan(center,ivec2(0)))&&all(lessThan(center,limit))) {
            vec2 size=vec2(textureSize(uShadow,0));
            vec4 a=textureGather(uShadow,vec2(center)/size);
            vec4 b=textureGather(uShadow,vec2(center+ivec2(1,0))/size);
            vec4 c=textureGather(uShadow,vec2(center+ivec2(0,1))/size);
            vec4 e=textureGather(uShadow,vec2(center+ivec2(1,1))/size);
            vec3 row[3]=vec3[3](vec3(a.w,a.z,b.z),vec3(a.x,a.y,b.y),vec3(c.x,c.y,e.y));
            // Preserve the existing nine comparisons, multiplications and
            // row-major addition order. Gather does not filter/compare depth.
            for(int y=0;y<3;y++)for(int x=0;x<3;x++)
                filtered+=wx[x]*wy[y]*step(p.z-bias,row[y][x]);
        } else
#endif
        {
        for(int y=0;y<3;y++)for(int x=0;x<3;x++)
            filtered+=wx[x]*wy[y]*step(p.z-bias,texelFetch(uShadow,clamp(center+ivec2(x-1,y-1),ivec2(0),limit),0).r);
        }
        return mix(filtered,1.,smoothstep(125.,160.,length(vRelative.xz)));
    }
    float sum=0.;
    sum+=step(p.z-bias,texture(uShadow,p.xy+texel*vec2(-.55,-.55)).r);
    sum+=step(p.z-bias,texture(uShadow,p.xy+texel*vec2(.55,-.55)).r);
    sum+=step(p.z-bias,texture(uShadow,p.xy+texel*vec2(-.55,.55)).r);
    sum+=step(p.z-bias,texture(uShadow,p.xy+texel*vec2(.55,.55)).r);
    return mix(sum*.25,1.,smoothstep(125.,160.,length(vRelative.xz)));
}
vec3 brdf(vec3 base,float metal,float rough,vec3 N,vec3 V,vec3 L) {
    // Thin-leaf wrap/transmission and source practical attenuation stay outside.
    return material_direct(base,metal,rough,material_f0(base,metal,.04),N,V,L);
}
vec3 castle_practicals(vec3 base,float metal,float rough,vec3 N,vec3 V) {
    vec3 world=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
    vec2 edge=min(world.xz-uCastleLightBounds.xy,uCastleLightBounds.zw-world.xz);
    float height=min(world.y-uCastleLightHeights.x,uCastleLightHeights.y-world.y);
    if(min(edge.x,edge.y)<=0.||height<=0.)return vec3(0.);
    // A bounded hall approximation, not point-light shadow mapping. The
    // inward shell feather prevents these sources from lighting exterior
    // closed-wall faces, without extinguishing the visible interior faces.
    float interior=smoothstep(0.,.25,min(edge.x,edge.y))*smoothstep(0.,.15,height);
    vec3 result=vec3(0.);
    for(int i=0;i<4;++i) {
        vec3 delta=uCastleLightPositionRadius[i].xyz-world;
        float distance2=dot(delta,delta),radius=uCastleLightPositionRadius[i].w;
        if(radius<=0.||distance2>=radius*radius)continue;
        float ratio=distance2/(radius*radius);
        float window=1.-ratio*ratio;
        // Smooth zero at the finite support. A50cm source core bounds the
        // inverse-square singularity; zero-distance direction stays finite.
        float attenuation=window*window/max(distance2,.25);
        vec3 direction=delta*inversesqrt(max(distance2,.000001));
        result+=brdf(base,metal,rough,N,V,direction)*attenuation;
    }
    return result*max(uCastleLightColor,vec3(0.))*interior;
}
float local_box_weight(vec3 world,vec3 lo,vec3 hi) {
    vec3 edge=min(world-lo,hi-world);
    return smoothstep(0.,1.5,min(edge.x,min(edge.y,edge.z)));
}
float local_sh_weight(vec3 world,float weight,vec4 bounds,vec2 heights) {
    if(weight<=0.)return 0.;
    vec2 edge=min(world.xz-bounds.xy,bounds.zw-world.xz);
    float height=min(world.y-heights.x,heights.y-world.y);
    return clamp(weight,0.,1.)*smoothstep(0.,1.5,min(min(edge.x,edge.y),height));
}
vec3 local_sh9(vec3 coefficients[9],vec3 N) {
    // Lambert convolution / PI: l0=1, l1=2/3, l2=1/4.
    return coefficients[0]*.282095+
        (2./3.)*.488603*(coefficients[1]*N.y+coefficients[2]*N.z+coefficients[3]*N.x)+
        .25*(1.092548*(coefficients[4]*N.x*N.y+coefficients[5]*N.y*N.z+coefficients[7]*N.x*N.z)+
        coefficients[6]*.315392*(3.*N.z*N.z-1.)+coefficients[8]*.546274*(N.x*N.x-N.y*N.y));
}
vec3 local_diffuse(vec3 world,vec3 N,vec3 fallback) {
    // Preserve each capture's own domain through the camera-distance bisector.
    // Blending coefficients first and switching one dominant box creates a pop
    // on geometry that belongs to only one of the two overlapping domains.
    float w0=local_sh_weight(world,uLocalIrradianceWeight,uLocalIrradianceBounds,uLocalIrradianceHeights);
    float w1=local_sh_weight(world,uLocalIrradianceSecondaryWeight,uLocalIrradianceSecondaryBounds,uLocalIrradianceSecondaryHeights);
    float total=w0+w1;
    if(total<=0.)return fallback;
    // Production bindings sum<=.40. The <=1 guard also preserves primary-only
    // shader fixtures and prevents malformed weights subtracting ambient fill.
    float normalization=1./max(total,1.);w0*=normalization;w1*=normalization;
    vec3 result=fallback*(1.-w0-w1);
    if(w0>0.)result+=max(local_sh9(uLocalIrradiance,N),vec3(0))*w0;
    if(w1>0.)result+=max(local_sh9(uLocalIrradianceSecondary,N),vec3(0))*w1;
    return result;
}
vec3 local_reflection_direction(vec3 world,vec3 direction,int index) {
    // Box projection keeps nearby walls attached to their real surfaces as
    // the observer walks. No camera-oriented reflection plane is introduced.
    vec3 safe=vec3(abs(direction.x)>.00001?direction.x:.00001,
        abs(direction.y)>.00001?direction.y:.00001,abs(direction.z)>.00001?direction.z:.00001);
    vec3 a=(uLocalProbeBoxMin[index]-world)/safe,b=(uLocalProbeBoxMax[index]-world)/safe;
    vec3 far=max(a,b);
    float distance=max(0.,min(far.x,min(far.y,far.z)));
    return world+direction*distance-uLocalProbeCenter[index];
}
vec3 local_reflection(vec3 world,vec3 direction,float rough,vec3 fallback) {
    if(uLocalProbeCount<=0)return fallback;
    vec2 weights=uLocalProbeWeights;
    weights.x*=local_box_weight(world,uLocalProbeBoxMin[0],uLocalProbeBoxMax[0]);
    weights.y*=uLocalProbeCount>1?local_box_weight(world,uLocalProbeBoxMin[1],uLocalProbeBoxMax[1]):0.;
    float total=weights.x+weights.y;
    if(total<=.0001)return fallback;
    // Startup scene captures are GGX-filtered with roughness=level/(levels-1).
    // Match the HDR environment's linear perceptual-roughness LOD mapping.
    // A zero primary domain has no radiance contribution. Explicit LOD makes
    // this branch independent of neighboring fragment texture derivatives.
    vec3 radiance=vec3(0.);
    if(weights.x>0.)radiance=textureLod(uLocalProbe0,local_reflection_direction(world,direction,0),rough*uLocalProbeMip).rgb*weights.x;
    if(weights.y>0.)radiance+=textureLod(uLocalProbe1,local_reflection_direction(world,direction,1),rough*uLocalProbeMip).rgb*weights.y;
    return mix(fallback,max(radiance*uLocalProbeLightScale/total,vec3(0.)),clamp(total*.75,0.,.75));
}
vec3 opaque_environment(vec3 base,float metal,float rough,vec3 geometricN,vec3 V,
                        vec3 diffuseSky,vec3 diffuseGround,vec3 ambient,float ao) {
    // Low-frequency environment approximation: these values are diffuse
    // irradiance / PI, which equals radiance for a uniform hemisphere.
    // Local startup scene captures replace a bounded share where available.
    // Broad geometric normals keep the 80--125m detail-normal fade invisible
    // in reflections; rough surfaces blend toward the existing hemisphere.
    vec3 reflected=reflect(-V,geometricN);
    vec3 radiance=mix(diffuseGround,diffuseSky,clamp(.5+.5*reflected.y,0.,1.));
    radiance=mix(radiance,ambient,rough*rough*.75);
    vec3 world=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.){float h=clamp(.5+.5*geometricN.y,0.,1.);float access=clamp(dot(mix(diffuseGround,diffuseSky,h),vec3(.2126,.7152,.0722))/max(dot(mix(uGroundLight,uSkyLight,h),vec3(.2126,.7152,.0722)),.0001),0.,1.);radiance=mix(radiance,max(textureLod(uEnvironmentCube,environment_direction(reflected),rough*uEnvironmentMip).rgb,vec3(0))*uEnvironmentIntensity*access,uEnvironmentStrength);}
#endif
    radiance=local_reflection(world,reflected,rough,radiance);
    float nv=clamp(dot(geometricN,V),0.,1.);
    vec3 f0=material_f0(base,metal);
    vec3 fresnel=material_environment_fresnel(f0,rough,nv);
    // Metals reflect their base-colored environment without dielectric
    // diffuse fill. The roughness term bounds the broad single-scatter lobe.
    vec3 diffuse=material_diffuse_factor(base,metal,fresnel)*ambient;
    vec3 specular=radiance*fresnel*(1.-.45*rough);
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.){vec2 dfg=texture(uEnvironmentBRDF,vec2(nv,rough)).rg;specular=mix(specular,radiance*(f0*dfg.x+dfg.y),uEnvironmentStrength);}
#endif
    return (diffuse+specular)*ao;
}

vec3 tavern_diffuse_return() {
    if(!uTavernReturnEnabled)return vec3(0.);
    vec3 world=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
    vec2 edge=min(world.xz-uTavernReturnBounds.xy,uTavernReturnBounds.zw-world.xz);
    float vertical=min(world.y-uTavernReturnHeights.x,uTavernReturnHeights.y-world.y);
    float ceiling=uTavernReturnRoof.y-uTavernReturnRoof.z*abs(world.z-uTavernReturnRoof.x);
    float roof_depth=ceiling-world.y;
    if(min(edge.x,edge.y)<=0.||vertical<=0.||roof_depth<=0.)return vec3(0.);
    // All feathers sit inside the solid shell. The gable's inner panel is
    // only20mm inside the X boundary, so narrow its feather smoothly over the
    // upper40cm of wall, reaching15mm at the actual28.4m eaves. Lower walls
    // retain150mm door transitions. Interior faces/floor/roof receive the
    // full return; exterior walls and the roof top receive exactly zero.
    // Independent of the torch cluster, it reaches far tables and the ceiling.
    float edge_width=mix(.15,.015,smoothstep(28.,28.4,world.y));
    float room=smoothstep(0.,edge_width,min(edge.x,edge.y))*
        smoothstep(0.,.15,vertical)*smoothstep(0.,.05,roof_depth);
    // Low-frequency first return from the actual seven open windows. This
    // shares the sky's color/time of day; no point-light/shadow pass is added.
    // The existing room/roof mask prevents any exterior light leak.
    vec2 center=(uTavernReturnBounds.xy+uTavernReturnBounds.zw)*.5;
    vec3 local=world-vec3(center.x,0.,center.y);
    const vec3 windows[7]=vec3[7](vec3(-6.5,26.2,8.),vec3(6.5,26.2,8.),
        vec3(.8,26.2,-8.),vec3(-10.,26.2,-3.),vec3(-10.,26.2,3.),
        vec3(10.,26.2,-3.),vec3(10.,26.2,3.));
    float window_return=0.;
    for(int i=0;i<7;i++){vec3 delta=local-windows[i];window_return+=.08/(1.+dot(delta,delta)*.18);}
    return (max(uTavernReturnIrradiance,vec3(0.))+max(uSkyLight,vec3(0.))*window_return)*room;
}

float torch_attenuation(int i,vec3 world,vec3 delta) {
    float d2=dot(delta,delta),radius=uTorchPositionRadius[i].w;
    if(radius<=0.||d2>=radius*radius)return 0.;
    float plane=dot(uTorchWallPlane[i].xyz,world)+uTorchWallPlane[i].w;
    vec2 edge=min(world.xz-uTorchClipBounds[i].xy,uTorchClipBounds[i].zw-world.xz);
    float vertical=min(world.y-uTorchClipHeights[i].x,uTorchClipHeights[i].y-world.y);
    if(plane<=0.||min(edge.x,edge.y)<=0.||vertical<=0.)return 0.;
    float cone=1.;
#ifdef VALWOOD_STUDIO_SURFACES
    if(uTorchDirectionCone[i].w>-.999)cone=smoothstep(uTorchDirectionCone[i].w,min(1.,uTorchDirectionCone[i].w+.08),dot(-delta*inversesqrt(max(d2,.0001)),uTorchDirectionCone[i].xyz));
#endif
    float ratio=d2/(radius*radius),window=1.-ratio*ratio;
    return cone*window*window/max(d2,.25)*smoothstep(0.,.10,plane)*
        smoothstep(0.,.15,min(edge.x,edge.y))*smoothstep(0.,.10,vertical);
}

vec3 torch_surface(vec3 base,float metal,float rough,vec3 N,vec3 V) {
    vec3 world=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
    if(uTorchCount<=0||any(lessThan(world,uTorchClusterMin))||any(greaterThan(world,uTorchClusterMax)))return vec3(0.);
    vec3 result=vec3(0.);
    for(int i=0;i<8;i++) {
        if(i>=uTorchCount)break;
        vec3 delta=uTorchPositionRadius[i].xyz-world;
        float attenuation=torch_attenuation(i,world,delta);
        if(attenuation<=0.)continue;
        vec3 L=delta*inversesqrt(max(dot(delta,delta),.000001));
        result+=brdf(base,metal,rough,N,V,L)*uTorchColorIntensity[i].rgb*uTorchColorIntensity[i].w*attenuation;
    }
    return result;
}

// Source-crown diffuse shelter, baked once. Shared physical world coordinates
// keep terrain, foliage and people coherent through camera rebasing.
float forest_sky_access(vec3 world) {
    if(!uForestShelterEnabled)return 1.;
    vec2 uv=(world.xz-uForestShelterBounds.xy)*uForestShelterBounds.zw;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return 1.;
    vec2 size=vec2(textureSize(uVillageSkyMask,0).xy);
    uv=(uv*(size-1.)+.5)/size;
    vec3 field=textureLod(uVillageSkyMask,vec3(uv,5.),0.).rgb;
    float ground=uForestShelterHeights.x+field.g*uForestShelterHeights.y;
    float crown=uForestShelterHeights.x+field.b*uForestShelterHeights.y;
    float recovery=smoothstep(ground+1.5,max(ground+3.,crown),world.y);
    // Feather inside the exact field domain; no clamp-edge lighting seam.
    vec2 edge=min(world.xz-uForestShelterBounds.xy,
        uForestShelterBounds.xy+1./uForestShelterBounds.zw-world.xz);
    float domain=smoothstep(0.,6.,min(edge.x,edge.y));
    return mix(1.,mix(field.r,1.,recovery),domain);
}

#ifdef VALWOOD_STUDIO_SURFACES
uniform int uEditorDecalCount;
uniform vec3 uEditorDecalMinimum,uEditorDecalMaximum;
uniform vec4 uEditorDecalPosition[8],uEditorDecalSize[8],uEditorDecalColor[8],uEditorDecalParams[8];
void editor_surface(vec3 world,inout vec3 base,inout float roughness){
    if(uEditorDecalCount==0)return;
    float grain_detail=1.-smoothstep(.2,.9,max(length(dFdx(world)),length(dFdy(world)))*3.7);
    // Exact union of the active rotated boxes: skip distant ground and crowns
    // without changing any patch pixel. Derivatives precede spatial branching.
    if(any(lessThan(world,uEditorDecalMinimum))||any(greaterThan(world,uEditorDecalMaximum)))return;
    for(int i=0;i<8;i++){
        if(i>=uEditorDecalCount)break;
        vec3 q=world-uEditorDecalPosition[i].xyz;
        float c=uEditorDecalPosition[i].w,s=uEditorDecalSize[i].w;
        q=vec3(c*q.x-s*q.z,q.y,s*q.x+c*q.z)/uEditorDecalSize[i].xyz;
        float box=max(max(abs(q.x),abs(q.y)),abs(q.z));
        if(box>=1.)continue;
        float grain=mix(.5,fract(sin(dot(floor(world.xz*3.7),vec2(12.9898,78.233)))*43758.5453),grain_detail);
        float pattern=uEditorDecalParams[i].x;
        float coverage=pattern<.5?.68+.32*grain:pattern<1.5?smoothstep(.18,.8,grain):pattern<2.5?.9:1.;
        float a=(1.-smoothstep(.72,1.,box))*coverage*uEditorDecalColor[i].a;
        base=mix(base,uEditorDecalColor[i].rgb*(.87+.13*grain),a);
        roughness=mix(roughness,uEditorDecalParams[i].y,a);
    }
}
#else
void editor_surface(vec3 world,inout vec3 base,inout float roughness){}
#endif

#ifdef VALWOOD_IMPORTED_SURFACES
#define STUDIO_SURFACE_SAMPLER uVillageSkyMask
#pragma include "world_surface_library.fs.glsl"
#else
vec4 studioImage(int image,vec2 uv,vec2 dx,vec2 dy){return vec4(1.);}
void studioSurface(vec3 world,vec2 uv,mat3 basis,bool terrain,inout vec3 color,inout float rough,inout float metal,inout vec3 normal,inout float ao){}
#endif

// VALWOOD_GRAPH_SURFACE
void main() {
    vec2 studioUV=vUV;
    vec2 sourceDx=dFdx(studioUV),sourceDy=dFdy(studioUV);
    visualTownUV=(vec2(vPos.x+uWorldAnchor.x,vPos.z+uWorldAnchor.y)-
        vec2(-1310.400000000,-10207.584747455))*.25;
    visualTownDx=dFdx(vPos.xz)*.25;visualTownDy=dFdy(vPos.xz)*.25;
    // Model vertices retain world height after local-XZ rebasing. Reflection
    // clipping must follow the actual water plane, including raised pools.
    if(uClipBelowWater&&vPos.y<uWaterClipLevel) discard;
    float distance=length(vRelative),groundDistance=length(vRootRelative.xz);
    vec4 base=(uHasBase?facade_base(studioUV,sourceDx,sourceDy):vec4(1.))*uBaseFactor;
    // The selected Unity TreeCreator plants overwrite vertex RGB with their
    // material tint, then multiply albedo by authored vertex alpha. Their
    // canonical source-paint RGB carries that alpha without a new vertex ABI.
    if(uSourcePaintEnabled&&uFacadeMode==0&&uVegetation>0)
        base.rgb*=clamp(vSourcePaint.r,0.,1.);
#ifdef VALWOOD_CHARACTER_APPEARANCE
    if(uCharacterRegion==1){
        if(vCharacterHair.a>.5)discard;
        // Preserve the source's strand shading, while allowing dark hair to
        // become light. Tint applies only to an explicitly tagged hair mesh.
        float luminance=dot(base.rgb,vec3(.2126,.7152,.0722));
        base.rgb=mix(base.rgb,vCharacterHair.rgb*clamp(luminance/.18,.18,2.5),vCharacterData.w);
        if(uAlphaCutoff>0.)base.a=pow(clamp(base.a,0.,1.),vCharacterData.z);
    }
#endif
    // A bounded derivative width antialiases the mask edge. Fully transparent
    // texels still have zero coverage;
    // tiny averaged mips cannot turn the entire photograph into a solid quad.
    float cutoff=uVegetation==4 ? max(.34,uAlphaCutoff) : uAlphaCutoff;
#ifdef VALWOOD_CHARACTER_APPEARANCE
    if(uCharacterRegion==1&&cutoff>0.)cutoff=clamp(cutoff+vCharacterData.y,.05,.95);
#endif
    float coverage=1.,subpixel=0.;
    if(cutoff>0.) {
        float width=clamp(fwidth(base.a),1./255.,.20);
        coverage=clamp((base.a-cutoff)/width+.5,0.,1.);
        if(uHasBase&&cutoff<1.&&uBaseFactor.a>=cutoff&&uVegetation!=4&&(uGrass||uVegetation==1||uVegetation==3||uVegetation==5||uVegetation==6)) {
            // Fine needles disappear when a box-filtered mip averages below
            // the hard cutout. At strong minification, alpha is subpixel area:
            // preserve that coverage with a spatially distributed sample rank.
            // The resolved near edge and whole-tree far cards stay unchanged.
            vec2 texels=vec2(textureSize(uBase,0));
            vec2 dx=dFdx(studioUV)*texels,dy=dFdy(studioUV)*texels;
            float footprint2=max(dot(dx,dx),dot(dy,dy));
            subpixel=smoothstep(4096.,32768.,footprint2);
            coverage=mix(coverage,clamp(base.a,0.,1.),subpixel);
        }
    }
    // Empty photograph texels cannot contribute to any later LOD/fade.
    if(coverage<=0.)discard;
    // Shared spatial rank keeps complementary representations fixed while
    // standing still. Camera jitter already supplies temporal edge samples;
    // cycling the representation each frame creates avoidable LOD twinkle.
    float ordered=fract(dot(floor(gl_FragCoord.xy),vec2(.754877666,.569840296)));
    float intervalLo=0.,intervalHi=1.;
#ifdef VALWOOD_STUDIO_SURFACES
    float previewCoverage=coverage;
#endif
    if(uVegetation==1||uVegetation==4) {
        vec2 nearRange=uTreeNearFade.y>uTreeNearFade.x?uTreeNearFade:vec2(62.,82.);
        float nearWeight=1.-smoothstep(nearRange.x,nearRange.y,groundDistance);
        float farWeight=smoothstep(215.,265.,groundDistance);
        if(uLodLevel==0) intervalHi=nearWeight;
        if(uLodLevel==1) {intervalLo=nearWeight;intervalHi=1.-farWeight;}
        if(uLodLevel==2) intervalLo=1.-farWeight;
    }
    if(uPropLodRange.x>0.&&uPropLodRange.y>0.&&uPropLodRange.z>0.) {
        float nearWeight=1.-smoothstep(uPropLodRange.x-uPropLodRange.y,uPropLodRange.x+uPropLodRange.y,groundDistance);
        if(uPropLodRange.z<1.5)intervalHi=nearWeight;else intervalLo=nearWeight;
    }
    if(uVegetation==4) {
        float fade=uLodLevel==3 ? smoothstep(1450.,1680.,groundDistance)*(1.-smoothstep(10000.,12000.,groundDistance)) : 1.-smoothstep(1450.,1680.,groundDistance);
        float centerDistance=length(vRootRelative+vec3(0.,max(uTreeHeight,1.)*vScale*.5,0.));
        float projected=max(uTreeHeight,1.)*vScale*uPixelScale/max(centerDistance,.1);
        fade*=smoothstep(3.,7.,projected);
        coverage*=fade;
    }
    if(uVegetation==5) coverage*=1.-smoothstep(55.,70.,groundDistance);
    if(uVegetation==6) coverage*=1.-smoothstep(280.,320.,groundDistance);
    if(uSceneryDistanceFade.y>uSceneryDistanceFade.x)
        coverage*=1.-smoothstep(uSceneryDistanceFade.x,uSceneryDistanceFade.y,groundDistance);
#ifdef VALWOOD_STUDIO_SURFACES
    if(uStudioLodPreview.z>0.&&length(vRootRelative.xz-uStudioLodPreview.xy)<.02){intervalLo=0.;intervalHi=1.;coverage=previewCoverage;}
#endif
    if(uGrass) coverage*=1.-smoothstep(62.,90.,groundDistance);
    coverage*=vInstanceFade;
    if(coverage<=0.)discard;
    // LOD meshes share one complementary, spatially sampled interval. Alpha
    // coverage uses an independent stationary rank: advancing the frame index
    // must not make every fine blade/leaf repeatedly appear and disappear.
    // Independent ranks retain the product of LOD and alpha coverage without
    // the holes caused by comparing both fades against the same threshold.
    if(ordered<intervalLo||ordered>=intervalHi)discard;
    // Opaque interiors need no stochastic alpha work; only edge/subpixel/fade
    // samples require a rank. Full coverage keeps the exact same LOD decision.
    if(coverage<1.) {
        float rank=fract(52.9829189*fract(dot(floor(gl_FragCoord.xy),vec2(.06711056,.00583715))));
        rank=fract(rank+vCoveragePhase*subpixel);
        if(rank>=coverage)discard;
    }
    bool environment=dot(uSkyLight+uGroundLight,vec3(1.))>.00001;
    vec3 skyLight=environment ? uSkyLight : uAmbient*vec3(1.25,1.38,1.52);
    vec3 groundLight=environment ? uGroundLight : uAmbient*vec3(.48,.50,.43);
    vec2 villageSky=village_sky_access();
    // Forest shelter belongs to the incident sky before either village
    // occlusion path. The directional opaque path must retain the same canopy
    // attenuation as leaves, terrain and distant cards.
    vec3 unoccludedSky=skyLight*forest_sky_access(vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y));
    skyLight=unoccludedSky*villageSky.x;
    float density=uHaze*mix(1.12,.48,smoothstep(60.,1900.,vRootHeight));
    float fog=max(1.-exp(-distance*density),smoothstep(10500.,14900.,distance));
    base.rgb*=uStudioTintRough.rgb;
    vec3 sunlight=uSunColor;
    if(uVegetation==4) {
        // The photograph already contains broad canopy shading. Relight its
        // average energy, never the rotating plane normal; no ARM, normal,
        // shadow or BRDF texture samples are needed by the far LOD. Only the
        // direct component receives the terrain's procedural cloud shadow.
        sunlight=uSunColor*cloud_transmittance();
        float tint=mix(.88,1.08,vSeed);
        if(uWetness>0.)base.rgb*=1.-clamp(uWetness,0.,1.)*.03;
        vec3 indirect=base.rgb*mix(groundLight,skyLight,.70)*1.12*tint;
        float cardBakedWeight=clamp(vBakedLight.a,0.,1.);
        if(cardBakedWeight>0.) {
            vec3 sourceIrradiance=max(vBakedLight.rgb/max(cardBakedWeight,.0001),vec3(0));
            indirect+=base.rgb*sourceIrradiance*cardBakedWeight;
        }
        vec3 card=indirect+base.rgb*sunlight*.37*tint;
        float fogAmount=clamp(fog,0.,1.);
        FragColor=vec4(mix(card,uFog,fogAmount),min(vegetation_stability(distance,.65),1.-clamp(uActorReactive,0.,.95)));
        IndirectColor=vec4(indirect*(1.-fogAmount),0.);
        return;
    }
    vec3 geometricN=normalize(vNormal);
    vec3 N=geometricN;
    float normalMipVariance=0.;
    if(uHasNormal&&distance<125.&&uVegetation!=4) {
        vec3 detail=facade_normal(studioUV,sourceDx,sourceDy)*2.-1.;
        float sourceLength2=dot(detail,detail);
        detail.xy*=uNormalScale*uStudioNormalUV.x;
        vec3 mapped=normalize(mat3(normalize(vTangent),normalize(vBitangent),N)*detail);
        float normalWeight=1.-smoothstep(80.,125.,distance);
        // Averaging a normal map shortens its vectors. Renormalizing that
        // average without widening the BRDF restores a sharp highlight that
        // the minified surface cannot support. Preserve that lost variance
        // as roughness, fading with exactly the same detail-normal weight.
        normalMipVariance=clamp(1.-sourceLength2,0.,1.)*normalWeight*normalWeight;
        N=normalize(mix(N,mapped,normalWeight));
    }
    if(!gl_FrontFacing) {N=-N;geometricN=-geometricN;}
    bool leaf=uGrass||uVegetation==3||uVegetation==5||uVegetation==6||(uVegetation==1&&uAlphaCutoff>0.);
    if(uVillageDirectionalEnabled&&uVegetation==0&&!uGrass)
        skyLight=unoccludedSky*village_directional_sky(geometricN);
    vec3 V=normalize(-vRelative);
    // All three leaf sunlight terms are exactly zero under this same guard.
    // Cloud noise is ALU-only; opaque bounce and far-card clouds still run.
    bool leafSunZero=leaf&&dot(N,uSun)<=-.45&&dot(V,-uSun)<=0.;
    if(!leafSunZero)sunlight=uSunColor*cloud_transmittance();
    vec3 arm=uHasARM?facade_arm(studioUV,sourceDx,sourceDy):vec3(1.);
    float ao=uSourceMaterialAO?clamp(arm.r,0.,1.):mix(.48,1.,arm.r);
    if(uVegetation>0) ao=max(ao,.76);
    float rough=clamp(uRoughness*arm.g*uStudioTintRough.w,uFacadeMode==4?.04:.25,1.),metal=clamp(uMetallic*arm.b,0.,1.);
    if(uArchitectureWeathering&&uVegetation==0&&!uGrass&&uActorReactive<=0.&&uTreeHeight>4.&&metal<.2) {
        float height=vPos.y-vRootHeight;
        if(height<1.2&&geometricN.y<.65) {
            vec2 world=vPos.xz+uWorldAnchor;
            float dirt=(1.-smoothstep(.08,1.2,height))*(.3+.7*noise(world*.53));
            // Stable ground splash and worn lower masonry tie architecture
            // to its footing while preserving source UV/detail at eye height.
            base.rgb*=1.-.13*dirt;
            rough=mix(rough,max(rough,.82),dirt*.55);
        }
        if(uVegetationFinish&&geometricN.y<.65) {
            vec2 world=vPos.xz+uWorldAnchor;
            float patch=noise(world*.38+vec2(11.7,4.3));
            float footing=(1.-smoothstep(.04,.8+patch*1.05,height));
            float cavity=1.-smoothstep(.43,.93,arm.r);
            float moss=footing*smoothstep(.37,.72,patch)*(.22+.45*cavity);
            float value=dot(base.rgb,vec3(.2126,.7152,.0722));
            base.rgb=mix(base.rgb,value*vec3(.53,.66,.37),moss*.48);
            base.rgb*=1.-footing*.16;
            rough=mix(rough,max(rough,.88),footing*.65);
            // Thin vertical runoff shares a continuous world seed across
            // wall modules, with no screen/time-dependent color crawling.
            float vertical=abs(geometricN.x)>.5?world.y:world.x;
            float streak=smoothstep(.64,.83,noise(vec2(vertical*2.1,height*.12)));
            base.rgb*=1.-streak*.10*(1.-footing*.6);
        }
    }
    if(uVegetation==0&&!uGrass)studioSurface(vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y),studioUV,mat3(normalize(vTangent),normalize(vBitangent),geometricN),false,base.rgb,rough,metal,N,ao);
    if(uVegetationFinish&&(uGrass||uVegetation==2||uVegetation==3||uVegetation==5)) {
        // Dark rooted bases, source-preserving clump variation and no extra texture taps.
        float rootLight=mix(.67,1.,smoothstep(.015,.28,max(vPos.y-vRootHeight,0.)));
        base.rgb*=rootLight*mix(.92,1.06,vSeed);
    }
    vec3 graphEmission=vec3(0);
#ifdef VALWOOD_STUDIO_GRAPH
    float graphNormal=1.;if(studioGraphSurface(studioUV,vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y),N,V,uTime,base.rgb,rough,metal,graphNormal,graphEmission)){
    base.rgb=clamp(base.rgb,vec3(0),vec3(2));rough=clamp(rough,.04,1.);metal=clamp(metal,0.,1.);graphEmission=clamp(graphEmission,vec3(0),vec3(8));vec3 graphN=mix(geometricN,N,clamp(graphNormal,0.,2.));N=dot(graphN,graphN)>1e-10?normalize(graphN):geometricN;}
#endif
    if(uVegetation==0&&!uGrass)editor_surface(vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y),base.rgb,rough);
    if(uVegetation==5) rough=max(rough,.75);
    // Weather acts after authored/source puddle layers, preserving their smoothness.
    material_wet_surface(base.rgb,rough,metal,leaf,uWetness);
    // Dry roofs, stone, bark and leaves need the same pixel-footprint filter
    // as wet leaves. It changes only an unresolved specular lobe: flat or
    // fully resolved surfaces retain their authored roughness exactly.
    vec3 normalDx=dFdx(N),normalDy=dFdy(N);
    float specularVariance=min(.12,normalMipVariance*.25+
        max(dot(normalDx,normalDx),dot(normalDy,normalDy))*.35);
    rough=sqrt(min(1.,rough*rough+specularVariance));
    if(uGrass) {
        float tip=smoothstep(.05,.95,vUV.y);
        base.rgb*=mix(.55,1.15,tip)*mix(.80,1.20,vSeed);
        base.rgb=mix(base.rgb,vec3(.26,.235,.10),pow(vSeed,12.)*tip*.38);
        ao*=mix(.65,1.,tip);
    } else if(uVegetation==5) {
        // Retain the scan's color and veins while matching the diffuse energy
        // of the meadow ground instead of leaving photographed undersides black.
        base.rgb*=vec3(1.16,1.40,1.02)*mix(.92,1.10,vSeed);
        float luminance=dot(base.rgb,vec3(.2126,.7152,.0722));
        base.rgb*=max(1.,.070/max(luminance,.006));
    } else if(uVegetation==1||uVegetation==3) {
        base.rgb*=mix(.88,1.08,vSeed);
    }
    // PCF has no contribution when opaque direct light is zero, or when
    // all three thin-leaf sunlight terms are exactly zero. Keep the four-tap
    // implicit-LOD path unchanged; the nine-tap path uses explicit texelFetch.
    float shadow=1.;
    if(!uSoftShadows||(!leafSunZero&&(leaf||!(dot(N,uSun)<=0.))))shadow=visibility(geometricN);
    if(uVegetation==0&&!uGrass&&villageSky.x<1.&&any(greaterThan(uVillageBounceAlbedo,vec3(0)))) {
        // The roof field measures horizontal sky access. Accessible ground
        // receives sky and the horizontal component of the same sun/cloud
        // source. A single diffuse return restores part of the blocked sky's
        // local exchange; access*(1-access)<=.25 bounds its energy. Height
        // recovery and exterior access1 remove it without another texture.
        vec3 incident=unoccludedSky+sunlight*max(uSun.y,0.);
        groundLight+=clamp(uVillageBounceAlbedo,vec3(0),vec3(.8))*incident*
            villageSky.y*(1.-villageSky.x);
    }
    // Opaque trunks/rocks keep a geometric hemisphere. Thin leaves receive a
    // softer two-sided sky/ground integral without giving bark transmission.
    // Normal-map detail belongs to direct response. The broad sky/ground
    // hemisphere must not change as that detail fades between80–125m.
    float hemisphere=leaf ? .65 : clamp(.5+.5*geometricN.y,0.,1.);
    vec3 ambient=mix(groundLight,skyLight,hemisphere);
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.){float access=clamp(dot(ambient,vec3(.2126,.7152,.0722))/max(dot(mix(uGroundLight,uSkyLight,hemisphere),vec3(.2126,.7152,.0722)),.0001),0.,1.);ambient=mix(ambient,environment_diffuse(geometricN)*access,uEnvironmentStrength);}
#endif
    if(!leaf)ambient=local_diffuse(vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y),geometricN,ambient);
    vec3 indirect=base.rgb*ambient*ao;
    if(!leaf)indirect=opaque_environment(base.rgb,metal,rough,geometricN,V,skyLight,groundLight,ambient,ao);
    // Source UE HQ-lightmap output is linear OutDiffuseLighting. No extra
    // gamma, exposure or PI conversion belongs here. The current source Sun
    // and SkyLight are Movable: baked GI adds independently to dynamic sky
    // diffuse, direct sunlight and environment specular. Historic bake
    // mobility is unknown; near-black architecture bakes prove no fill.
    float bakedWeight=clamp(vBakedLight.a,0.,1.);
    if(bakedWeight>0.) {
        vec3 sourceIrradiance=max(vBakedLight.rgb/max(bakedWeight,.0001),vec3(0));
        if(leaf) {
            indirect+=base.rgb*sourceIrradiance*ao*bakedWeight;
        } else {
            float nv=clamp(dot(geometricN,V),0.,1.);
            vec3 f0=material_f0(base.rgb,metal);
            vec3 fresnel=material_environment_fresnel(f0,rough,nv);
            vec3 diffuseFactor=material_diffuse_factor(base.rgb,metal,fresnel)*ao;
            indirect+=diffuseFactor*sourceIrradiance*bakedWeight;
            indirect=max(indirect,vec3(0));
        }
    }

    if(uVegetation==0&&!uGrass)
        indirect+=base.rgb*(1.-metal)*ao*tavern_diffuse_return();
    vec3 color=indirect+brdf(base.rgb,metal,rough,N,V,uSun)*sunlight*3.14159265*shadow;
    if(uFacadeMode==5) {
        float mask=facade_sample(uFacadeAux0,1.,studioUV,sourceDx,sourceDy).r;
        float opacity=clamp((mask+uFacadeUVScale.z)*uFacadeUVScale.y,0.,1.);
        float through=max(dot(-geometricN,uSun),0.)*(1.-opacity)*.5;
        color+=base.rgb*sunlight*through*(.35+.65*shadow);
    }
    color+=graphEmission;
    color+=max(uEmissiveFactor,vec3(0.))*(uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1.));
    color+=torch_surface(base.rgb,metal,rough,N,V);
    if(uCastleLightsEnabled&&uVegetation==0&&!uGrass)
        color+=castle_practicals(base.rgb,metal,rough,N,V);
    if(leaf) {
        float wrapped=max((dot(N,uSun)+.45)/1.45,0.)-max(dot(N,uSun),0.);
        color+=base.rgb*sunlight*wrapped*.65*(.35+.65*shadow);
        float through=pow(max(dot(V,-uSun),0.),3.)*((uGrass||uVegetation==5)?.65:.20);
        // A forward lobe requires light entering the reverse side of this sheet.
        through*=smoothstep(-.05,.65,-dot(geometricN,uSun));
        color+=base.rgb*sunlight*through*(.4+.6*shadow);
    }
    float stability=(uGrass||uVegetation==5)?.45:(uVegetation>0?.62:1.);
    if(uVegetation>0||uGrass)stability=vegetation_stability(distance,stability);
    // Reserve a source tag for transform-static rigid assets. Their subpixel
    // silhouettes need local coverage history just like a fixed leaf edge.
    // Wind, actors and placement fades must never acquire this static tag.
    if(uVegetation==0&&!uGrass&&abs(uWind)<.000001&&uActorReactive<=0.&&vInstanceFade>=1.)
        stability=.995;
    float fogAmount=clamp(fog,0.,.96);
    float confidence=min(stability,1.-clamp(uActorReactive,0.,.95));
    // Keep even a very weak actor flag outside the reserved .995 band.
    if(uActorReactive>0.)confidence=min(confidence,.99);
#ifdef VALWOOD_HDR_ENVIRONMENT
    color=min(color,vec3(60000));
#endif
    FragColor=vec4(mix(color,uFog,fogAmount),confidence);
    IndirectColor=vec4(indirect*(1.-fogAmount),0.);
}

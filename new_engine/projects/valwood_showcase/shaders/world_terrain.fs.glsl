#version 330 core
#if defined(GL_ARB_texture_gather) && !defined(VALWOOD_QA_DISABLE_TEXTURE_GATHER)
#extension GL_ARB_texture_gather : enable
#define VALWOOD_TEXTURE_GATHER_ACTIVE 1
#endif
#ifdef VALWOOD_TEXTURE_GATHER_ACTIVE
uniform bool uShadowGather=true;
#endif
uniform bool uForestShelterEnabled,uForestGroundReady;
uniform vec4 uForestShelterBounds;
uniform vec2 uForestShelterHeights;

in vec3 vPos, vNormal, vRelative;
layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 IndirectColor;
uniform sampler2D uGrass, uRock, uSand, uDirt;
uniform sampler2D uGrassNormal, uRockNormal, uGrassARM, uRockARM;
uniform sampler2D uClimate, uShadow, uMacroHeight;
uniform bool uForestLitter=false;
uniform sampler2DArray uLitterPBR; // exact normal layer0 / AO-roughness-metal layer1
uniform sampler2D uVillageGroundMask;
uniform vec4 uVillageGroundBounds;
uniform bool uVillageGroundReady;
uniform bool uVillageStochasticPaving;
uniform sampler2D uStartingMask;
uniform vec4 uStartingBounds;
uniform bool uHasStarting;
uniform vec4 uVillageBounds;
uniform int uFarmCount;
uniform vec2 uFarmOriginXZ;
uniform vec4 uFarmBounds[4];
uniform vec2 uFarmRows[4]; // spacing, village-local X phase

uniform bool uHasVillage;
uniform vec4 uBanditBounds;
uniform bool uHasBandit;
uniform sampler2DArray uLandscape;
uniform sampler2DArray uVillagePBR;
uniform bool uVillagePBRReady,uVillageSkyEnabled;
uniform vec3 uVillageBounceAlbedo; // same measured linear ground reflectance as architecture
uniform vec4 uDistricts[3],uTrailBounds;
uniform vec3 uTrail[9];
// Global feature enable, not a camera biome weight. The ellipse keeps terrain
// materials stable while walking and while viewing the forest from outside.
uniform float uRainforest;
uniform vec4 uRainforestBounds; // centerXZ, inverse radiiXZ
uniform vec2 uRainforestPath[6];
// Conservative freshwater cap; the actual streamed terrain height owns its
// shoreline. Zero inverse radii preserve old projects and shader fixtures.
uniform vec4 uFreshwaterPool; // centerXZ, inverse radiiXZ
uniform float uFreshwaterLevel;
uniform vec3 uSun, uSunColor, uAmbient, uFog;
// Diffuse irradiance / PI, distinct from the sky's visible radiance.
// Zero additions preserve the legacy fixture/old-project lighting contract.
uniform vec3 uSkyLight, uGroundLight;
#ifdef VALWOOD_HDR_ENVIRONMENT
uniform vec3 uEnvironmentSH[9];
uniform vec2 uEnvironmentRotation=vec2(1,0);
uniform float uEnvironmentIntensity=1,uEnvironmentStrength=0,uEnvironmentMip=8;
vec3 environment_direction(vec3 d){return vec3(uEnvironmentRotation.x*d.x-uEnvironmentRotation.y*d.z,d.y,uEnvironmentRotation.y*d.x+uEnvironmentRotation.x*d.z);}
vec3 local_sh9(vec3 coefficients[9],vec3 N);
vec3 environment_diffuse(vec3 N){return max(local_sh9(uEnvironmentSH,environment_direction(N)),vec3(0))*uEnvironmentIntensity;}
#endif

uniform vec3 uLocalIrradiance[9]; // scene-captured radiance SH, world-space basis
uniform float uLocalIrradianceWeight;
uniform vec4 uLocalIrradianceBounds; // xmin,zmin,xmax,zmax
uniform vec2 uLocalIrradianceHeights;
uniform vec3 uLocalIrradianceSecondary[9];
uniform float uLocalIrradianceSecondaryWeight;
uniform vec4 uLocalIrradianceSecondaryBounds;
uniform vec2 uLocalIrradianceSecondaryHeights; // minY,maxY
uniform vec2 uWorldSize;
uniform vec3 uEye;
uniform mat4 uLightVP;
uniform float uHaze, uCloud, uTime, uWetness,uWaterClipLevel;
uniform bool uClipBelowWater;
uniform bool uSoftShadows;

// Bounded, camera-selected practicals; source geometry and flicker stay fixed in world space.
#ifdef VALWOOD_STUDIO_SURFACES
uniform vec4 uTorchDirectionCone[8];
#endif
uniform int uTorchCount;
uniform vec4 uTorchPositionRadius[8],uTorchColorIntensity[8],uTorchWallPlane[8],uTorchClipBounds[8];
uniform vec2 uTorchClipHeights[8];
uniform vec3 uTorchClusterMin,uTorchClusterMax;

const float PI = 3.14159265;
#pragma include "world_material_response.glsl"
vec2 atlas_uv(vec2 uv) {
    vec2 size=vec2(textureSize(uClimate,0));
    return (uv*(size-1.)+.5)/size;
}

float hash(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * .1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x),
               mix(hash(i + vec2(0, 1)), hash(i + 1.0), f.x), f.y);
}
float cloud_transmittance() {
    if (uCloud < .015 || uSun.y < .06 || vPos.y >= 3200.0) return 1.0;
    vec2 p = (vPos.xz + uSun.xz / max(uSun.y, .12) * (3200.0 - vPos.y)) * .00042;
    p += vec2(uTime * .0014, -uTime * .00055);
    const mat2 rotation = mat2(.80, -.60, .60, .80);
    float field = .67 * noise(p) + .33 * noise(rotation * p * 2.03 + 17.4);
    float threshold = .64 - clamp(uCloud, 0.0, 1.0) * .30;
    float coverage = smoothstep(threshold, threshold + .17, field);
    coverage *= smoothstep(.015, .12, uCloud);
    return 1.0 - .34 * coverage * clamp(uCloud * 1.7, 0.0, 1.0);
}
float rainforest_weight(vec2 p) {
    if(uRainforest<=0.0||any(lessThanEqual(uRainforestBounds.zw,vec2(0))))return 0.0;
    vec2 q=(p-uRainforestBounds.xy)*uRainforestBounds.zw;
    return clamp(uRainforest,0.0,1.0)*(1.0-smoothstep(.36,1.0,dot(q,q)));
}
float rainforest_path(vec2 p) {
    if(uRainforest<=0.0||any(lessThanEqual(uRainforestBounds.zw,vec2(0))))return 0.0;
    // The six CPU-authored offsets span X[-15,12], Z[-110,95]m. Only
    // fragments in that 2.8m-expanded AABB evaluate the five path segments.
    vec2 local=p-uRainforestBounds.xy;
    if(any(lessThan(local,vec2(-18,-113)))||any(greaterThan(local,vec2(15,98))))return 0.0;
    float nearest=10000.0;
    for(int i=1;i<6;++i) {
        vec2 a=uRainforestPath[i-1],ab=uRainforestPath[i]-a;
        float t=clamp(dot(p-a,ab)/max(dot(ab,ab),.001),0.0,1.0);
        nearest=min(nearest,length(p-a-ab*t));
    }
    return clamp(uRainforest,0.0,1.0)*(1.0-smoothstep(1.6,2.8,nearest));
}
vec4 authored_material(vec2 p) {
    vec4 weights=vec4(0);
    for(int i=0;i<3;i++) {
        vec2 uv=(p-uDistricts[i].xy)*uDistricts[i].zw;
        if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))) {
            weights=texture(uLandscape,vec3((uv*512.+.5)/513.,float(i)));
            break;
        }
    }
    if(all(greaterThanEqual(p,uTrailBounds.xy))&&all(lessThanEqual(p,uTrailBounds.zw))) {
        float edge=10000.;
        for(int i=1;i<9;i++) {
            vec2 a=uTrail[i-1].xy,b=uTrail[i].xy,ab=b-a;
            float t=clamp(dot(p-a,ab)/max(dot(ab,ab),.001),0.,1.);
            float width=mix(uTrail[i-1].z,uTrail[i].z,t);
            edge=min(edge,length(p-a-ab*t)-width);
        }
        weights.r=1.-smoothstep(0.,.65,edge);
    }
    weights.r=max(weights.r,rainforest_path(p));
    return weights;
}
float village_paving=0.,village_sky=1.,village_ground_sky=1.;
vec3 starting_material(vec2 p) {
    vec3 result=vec3(0);
    if(uHasStarting) {
        vec2 uv=(p-uStartingBounds.xy)*uStartingBounds.zw;
        if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1))))
            result=texture(uStartingMask,(uv*512.+.5)/513.).rgb;
    }
    if(uHasVillage) {
        vec2 uv=(p-uVillageBounds.xy)*uVillageBounds.zw;
        if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))) {
            vec4 village=texture(uLandscape,vec3((uv*512.+.5)/513.,3.));
            result=max(result,village.rgb);
            if(uVillagePBRReady)village_paving=village.g;
            if(uVillageSkyEnabled) {
                village_ground_sky=clamp(village.a,.3,1.);
                village_sky=mix(village_ground_sky,1.,smoothstep(1.,18.,vPos.y-24.));
            }
        }
    }
    if(uHasBandit) {
        vec2 uv=(p-uBanditBounds.xy)*uBanditBounds.zw;
        if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1))))
            result=max(result,texture(uLandscape,vec3((uv*512.+.5)/513.,4.)).rgb);
    }
    return result;
}
vec3 village_linear(vec3 s) {
    return mix(s/12.92,pow((s+.055)/1.055,vec3(2.4)),step(vec3(.04045),s));
}
float village_history_region(vec3 p) {
    if(!uHasVillage||!uVillagePBRReady||!uVillageGroundReady||
       any(lessThanEqual(uVillageGroundBounds.zw,vec2(0))))return 0.;
    vec2 uv=(p.xz-uVillageGroundBounds.xy)*uVillageGroundBounds.zw;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return 0.;
    vec2 edge=min(uv,1.-uv)/uVillageGroundBounds.zw;
    return smoothstep(0.,2.,min(edge.x,edge.y))*
        (1.-smoothstep(.12,.45,abs(p.y-24.)));
}
vec4 village_ground_history(vec3 p,float region,vec2 world_dx,vec2 world_dy) {
    if(region<=0.)return vec4(0);
    vec2 uv=(p.xz-uVillageGroundBounds.xy)*uVillageGroundBounds.zw;
    vec2 size=vec2(textureSize(uVillageGroundMask,0));
    // The cached field is linear material history, never painted illumination.
    // Row0 is minimum worldZ; source map UV orientation is independent.
    vec2 scale=uVillageGroundBounds.zw*(size-1.)/size;
    return textureGrad(uVillageGroundMask,(uv*(size-1.)+.5)/size,
        world_dx*scale,world_dy*scale)*region;
}
// Two ordinary filtered samples break repetition without unstable UV derivatives.
vec3 meadow_color(vec2 uv, float pattern) {
    return mix(texture(uGrass, uv).rgb, texture(uGrass, uv + vec2(17.13, 9.71)).rgb,
               smoothstep(.25, .75, pattern));
}
vec3 rock_color(vec3 p, vec3 weights, vec3 signs) {
    vec3 x = texture(uRock, vec2(-p.z * signs.x, p.y)).rgb;
    vec3 y = texture(uRock, vec2(p.x, -p.z * signs.y)).rgb;
    vec3 z = texture(uRock, vec2(p.x * signs.z, p.y)).rgb;
    return x * weights.x + y * weights.y + z * weights.z;
}
vec3 rock_normal(vec3 p, vec3 weights, vec3 signs) {
    vec3 x = texture(uRockNormal, vec2(-p.z * signs.x, p.y)).xyz * 2.0 - 1.0;
    vec3 y = texture(uRockNormal, vec2(p.x, -p.z * signs.y)).xyz * 2.0 - 1.0;
    vec3 z = texture(uRockNormal, vec2(p.x * signs.z, p.y)).xyz * 2.0 - 1.0;
    return normalize(vec3(x.z * signs.x, x.y, -x.x * signs.x) * weights.x +
                     vec3(y.x, y.z * signs.y, -y.y * signs.y) * weights.y +
                     vec3(z.x * signs.z, z.y, z.z * signs.z) * weights.z);
}
vec2 rock_material(vec3 p, vec3 weights, vec3 signs) {
    // Roughness/AO use the dominant projection; albedo and near normals blend.
    vec2 uv = vec2(p.x, -p.z * signs.y);
    if (weights.x > weights.y && weights.x > weights.z) uv = vec2(-p.z * signs.x, p.y);
    else if (weights.z > weights.y) uv = vec2(p.x * signs.z, p.y);
    return texture(uRockARM, uv).rg;
}
vec3 brdf(vec3 base,float metal,float rough,vec3 N,vec3 V,vec3 L) {
    return material_direct(base,metal,rough,material_f0(base,metal),N,V,L);
}
float visibility(vec3 N) {
    // The existing125--160m fade is already exactly1 at its upper endpoint.
    // Avoid dead PCF work without changing any active shadow sample or weight.
    if(length(vRelative.xz)>=160.) return 1.;
    vec4 projected = uLightVP * vec4(vRelative, 1);
    vec3 p = projected.xyz / projected.w * .5 + .5;
    if (any(lessThan(p, vec3(.002))) || any(greaterThan(p, vec3(.998)))) return 1.0;
    vec2 texel = 1.0 / vec2(textureSize(uShadow, 0));
    float bias = .00009 + .00018 * (1.0 - max(dot(N, uSun), 0.0));
    if(uSoftShadows) {
        // Compare the nine stored depths before filtering. The separable
        // quadratic B-spline is a compact Gaussian approximation whose
        // subtexel weights meet continuously at every texel boundary.
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
    float result = 0.0;
    for (int i = 0; i < 4; ++i) {
        vec2 offset = vec2((i & 1) == 0 ? -.65 : .65, (i & 2) == 0 ? -.65 : .65);
        result += step(p.z - bias, texture(uShadow, p.xy + offset * texel).r);
    }
    return mix(result * .25, 1.0, smoothstep(125.0, 160.0, length(vRelative.xz)));
}
float macro_visibility() {
    if(vPos.y<120.||uSun.y<.06) return 1.;
    vec2 direction=uSun.xz/max(length(uSun.xz),.001);
    float rise=uSun.y/max(length(uSun.xz),.001);
    const float ranges[5]=float[5](180.,460.,1050.,2300.,4300.);
    float visible=1.;
    for(int i=0;i<5;i++) {
        float d=ranges[i];
        float height=texture(uMacroHeight,atlas_uv((vPos.xz+direction*d)/uWorldSize+.5)).r;
        visible=min(visible,1.-smoothstep(vPos.y+rise*d-12.,vPos.y+rise*d+25.,height)*.82);
    }
    return visible;
}
float sky_visibility(vec3 geometric_normal) {
    // Four bounded horizon probes describe the broad surrounding terrain.
    // This is a coarse sky-access estimate, not screen-space AO or GI. Bias
    // absorbs height-map interpolation and local detail omitted from the map.
    const vec2 directions[4] = vec2[4](vec2(1, 0), vec2(0, 1), vec2(-1, 0), vec2(0, -1));
    float blocked = 0.0;
    for (int i = 0; i < 4; ++i) {
        vec2 uv = atlas_uv((vPos.xz + directions[i] * 550.0) / uWorldSize + .5);
        // An unbroken inclined plane already receives the tilted hemisphere.
        // Only terrain rising above that local tangent is an extra blocker.
        float tangent_rise = -dot(geometric_normal.xz, directions[i]) /
            max(geometric_normal.y, .20) * 550.0;
        float horizon = max(texture(uMacroHeight, uv).r - vPos.y - tangent_rise - 14.0, 0.0) / 550.0;
        // sin(elevation)^2 is the obstructed cosine-weighted irradiance of a
        // horizontal horizon band. No arbitrary distance-darkening term.
        blocked += horizon * horizon / (1.0 + horizon * horizon);
    }
    return max(1.0 - blocked * .25, .38);
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
    vec3 world=vPos;
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

// The same authored bounds suppress wild grass on the CPU. Explicit footprint
// filtering removes unresolved furrow stripes and bump normals in the distance.
vec3 village_farm_surface(vec2 world,float footprint) {
    if(uFarmCount<=0)return vec3(0.);
    vec2 p=world-uFarmOriginXZ;
    vec3 result=vec3(0.);
    for(int i=0;i<4;i++) {
        if(i>=uFarmCount)break;
        vec2 edge=min(p-uFarmBounds[i].xy,uFarmBounds[i].zw-p);
        float weight=smoothstep(-.6,.2,min(edge.x,edge.y));
        if(weight<=0.)continue;
        float spacing=max(.1,uFarmRows[i].x);
        float phase=(p.x-uFarmRows[i].y)*6.28318530718/spacing;
        float filtered=1.-smoothstep(.20,.65,footprint/spacing);
        float ridge=cos(phase)*filtered;
        float slope=-sin(phase)*(.012*6.28318530718/spacing)*filtered;
        result=vec3(weight,ridge,slope);
    }
    return result;
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

// Source-crown diffuse shelter, baked once. Shared physical world coordinates
// keep terrain, foliage and people coherent through camera rebasing.
float forest_outcrop=0.;
float forest_sky_access(vec3 world) {
    if(!uForestShelterEnabled)return 1.;
    vec2 uv=(world.xz-uForestShelterBounds.xy)*uForestShelterBounds.zw;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return 1.;
    vec2 size=vec2(textureSize(uLandscape,0).xy);
    uv=(uv*(size-1.)+.5)/size;
    vec4 field=textureLod(uLandscape,vec3(uv,5.),0.);
    float ground=uForestShelterHeights.x+field.g*uForestShelterHeights.y;
    float crown=uForestShelterHeights.x+field.b*uForestShelterHeights.y;
    float recovery=smoothstep(ground+1.5,max(ground+3.,crown),world.y);
    // Feather inside the exact field domain; no clamp-edge lighting seam.
    vec2 edge=min(world.xz-uForestShelterBounds.xy,
        uForestShelterBounds.xy+1./uForestShelterBounds.zw-world.xz);
    float domain=smoothstep(0.,6.,min(edge.x,edge.y));
    forest_outcrop=uForestGroundReady?field.a*domain:0.;
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
#define STUDIO_SURFACE_SAMPLER uLandscape
#pragma include "world_surface_library.fs.glsl"
#else
vec4 studioImage(int image,vec2 uv,vec2 dx,vec2 dy){return vec4(1.);}
void studioSurface(vec3 world,vec2 uv,mat3 basis,bool terrain,inout vec3 color,inout float rough,inout float metal,inout vec3 normal,inout float ao){}
#endif

// VALWOOD_GRAPH_SURFACE

// Private source-showcase terrain: exact eight painted channels, source metre
// tiling and PBR maps. The old atlas material samplers become inactive.
uniform bool uShowcaseReady=false;
uniform sampler2DArray uShowcaseAlbedo,uShowcaseSurface,uShowcaseWeights;
uniform sampler2D uShowcaseHoles;
uniform bool uShowcaseHolesReady=false;
uniform vec4 uShowcaseBounds,uShowcaseUV[8];
uniform float uShowcaseNormalScale[8];
#ifdef VALWOOD_HDR_ENVIRONMENT
uniform samplerCube uEnvironmentCube;
#endif
uniform sampler2D uSnowImprints;
uniform bool uSnowImprintsReady=false;
uniform vec4 uSnowImprintBounds;
uniform mat4 uVP;
float showcase_imprint_reactive=0.;

void showcase_imprint(vec3 geometry,inout vec3 base,inout float ao,inout vec3 N) {
    if(!uSnowImprintsReady||geometry.y<.55)return;
    vec2 uv=((vPos.xz-uSnowImprintBounds.zw)-uSnowImprintBounds.xy)/32.;
    if(any(lessThan(uv,vec2(.015)))||any(greaterThan(uv,vec2(.985))))return;
    vec2 ray=vRelative.y<-.18?vRelative.xz/vRelative.y:vec2(0);
    ray=clamp(ray,vec2(-8),vec2(8));
    float h=texture(uSnowImprints,uv).r;vec2 offset=ray*h;
    for(int i=0;i<2;i++){h=texture(uSnowImprints,uv+offset/32.).r;offset=ray*h;}
    vec2 q=uv+offset/32.,step_uv=vec2(1./1024.,0);
    float hx=texture(uSnowImprints,q+step_uv).r-texture(uSnowImprints,q-step_uv).r;
    float hz=texture(uSnowImprints,q+step_uv.yx).r-texture(uSnowImprints,q-step_uv.yx).r;
    N=normalize(N+vec3(-hx,0,-hz)/.0625);
    float depression=clamp(-h/.038,0.,1.);base*=1.-depression*.09;ao*=1.-depression*.20;
    if(abs(h)>.0001){
        vec4 clip=uVP*vec4(vRelative+vec3(offset.x,h,offset.y),1);
        gl_FragDepth=clamp(clip.z/clip.w*.5+.5,0.,1.);showcase_imprint_reactive=clamp(abs(h)*32.,0.,1.);
    }
}

vec3 showcase_brdf(vec3 base,float rough,float metal,vec3 N,vec3 V,vec3 L) {
    return material_direct(base,metal,rough,material_f0(base,metal),N,V,L);
}

vec3 showcase_practicals(vec3 base,float rough,float metal,vec3 N,vec3 V) {
    if(uTorchCount<=0||any(lessThan(vPos,uTorchClusterMin))||any(greaterThan(vPos,uTorchClusterMax)))return vec3(0);
    vec3 result=vec3(0);
    for(int i=0;i<8;i++){
        if(i>=uTorchCount)break;vec3 delta=uTorchPositionRadius[i].xyz-vPos;
        float attenuation=torch_attenuation(i,vPos,delta);if(attenuation<=0.)continue;
        vec3 L=delta*inversesqrt(max(dot(delta,delta),.000001));
        result+=showcase_brdf(base,rough,metal,N,V,L)*uTorchColorIntensity[i].rgb*uTorchColorIntensity[i].w*attenuation;
    }
    return result;
}

void main() {
    gl_FragDepth=gl_FragCoord.z;
    vec2 p=vPos.xz-uShowcaseBounds.xy,dx=dFdx(p),dy=dFdy(p);
    vec2 alpha_uv=p*uShowcaseBounds.zw;
    bool inside=uShowcaseReady&&all(greaterThanEqual(alpha_uv,vec2(0)))&&all(lessThanEqual(alpha_uv,vec2(1)));
    if(inside&&uShowcaseHolesReady){
        ivec2 size=textureSize(uShowcaseHoles,0);
        ivec2 cell=clamp(ivec2(floor(alpha_uv*vec2(size))),ivec2(0),size-1);
        if(texelFetch(uShowcaseHoles,cell,0).r<.5)discard;
    }
    vec2 alpha_size=vec2(textureSize(uShowcaseWeights,0).xy);
    vec2 alpha_sample=(clamp(alpha_uv,0.,1.)*(alpha_size-1.)+.5)/max(alpha_size,vec2(1));
    vec4 low=inside?texture(uShowcaseWeights,vec3(alpha_sample,0)):vec4(0);
    vec4 high=inside?texture(uShowcaseWeights,vec3(alpha_sample,1)):vec4(0);
    float total=dot(low+high,vec4(1));
    vec3 geometry=normalize(vNormal),N=geometry;
    vec3 base=vec3(.24,.27,.29),detail_normal=vec3(0),arm=vec3(0);
    float rough=.86,metal=0.,ao=1.,snow=0.;
    if(total>.0001){
        low/=total;high/=total;snow=high.w;base=vec3(0);float used=0.;
        for(int i=0;i<8;i++){
            float weight=i<4?low[i]:high[i-4];if(weight<.001)continue;
            vec4 transform=uShowcaseUV[i];vec2 uv=p*transform.xy+transform.zw;
            vec2 uv_dx=dx*transform.xy,uv_dy=dy*transform.xy;
            base+=textureGrad(uShowcaseAlbedo,vec3(uv,float(i)),uv_dx,uv_dy).rgb*weight;
            vec3 mapped=textureGrad(uShowcaseSurface,vec3(uv,float(i*2)),uv_dx,uv_dy).xyz*2.-1.;
            mapped.xy*=uShowcaseNormalScale[i];detail_normal+=mapped*weight;
            arm+=textureGrad(uShowcaseSurface,vec3(uv,float(i*2+1)),uv_dx,uv_dy).rgb*weight;used+=weight;
        }
        base/=max(used,.0001);arm/=max(used,.0001);
        // Source texture V follows Unity+Z, therefore engine-Z. Keep its raw
        // tangent green with T=engine+X and B=engine-Z on a flat surface.
        vec3 tangent=vec3(geometry.y,-geometry.x,0);
        tangent=dot(tangent,tangent)>.000001?normalize(tangent):vec3(1,0,0);
        vec3 bitangent=normalize(cross(geometry,tangent));
        N=normalize(mat3(tangent,bitangent,geometry)*normalize(detail_normal));
        ao=clamp(arm.r,0.,1.);rough=clamp(arm.g,.06,1.);metal=clamp(arm.b,0.,1.);
    }
    if(uClipBelowWater&&vPos.y<(uWaterClipLevel==0.0?.2:uWaterClipLevel))discard;
    if(snow>.5)showcase_imprint(geometry,base,ao,N);
    editor_surface(vPos,base,rough);
    vec3 V=normalize(-vRelative);
    material_wet_surface(base,rough,metal,false,uWetness);
    vec3 normalDx=dFdx(N),normalDy=dFdy(N);
    rough=sqrt(min(1.,rough*rough+min(.12,max(dot(normalDx,normalDx),dot(normalDy,normalDy))*.35)));
    float hemisphere=clamp(.5+.5*geometry.y,0.,1.);
    float access=sky_visibility(geometry);
    vec3 sky_light=uSkyLight,ground_light=uGroundLight;
    vec3 ambient=mix(ground_light,sky_light*access,hemisphere);
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.)ambient=mix(ambient,environment_diffuse(geometry)*mix(1.,access,hemisphere),uEnvironmentStrength);
#endif
    ambient=local_diffuse(vPos,geometry,ambient);
    vec3 reflection=reflect(-V,geometry);
    vec3 radiance=mix(ground_light,sky_light,clamp(.5+.5*reflection.y,0.,1.));
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.)radiance=mix(radiance,max(textureLod(uEnvironmentCube,environment_direction(reflection),rough*uEnvironmentMip).rgb,vec3(0))*uEnvironmentIntensity*access,uEnvironmentStrength);
#endif
    vec3 f0=material_f0(base,metal);
    vec3 indirect=material_indirect(base,metal,rough,f0,geometry,V,ambient,radiance,ao);
    float direct_visibility=min(visibility(geometry),macro_visibility());
    vec3 color=indirect+showcase_brdf(base,rough,metal,N,V,uSun)*uSunColor*PI*direct_visibility*cloud_transmittance();
    color+=showcase_practicals(base,rough,metal,N,V);
    float distance_to_eye=length(vRelative),density=uHaze*mix(1.12,.48,smoothstep(60.,1900.,vPos.y));
    float fog=max(1.-exp(-distance_to_eye*density),smoothstep(10500.,14900.,distance_to_eye));
    float scatter=pow(max(dot(-V,uSun),0.),8.);vec3 aerial=uFog+uSunColor*scatter*.035;
    float fog_amount=clamp(fog,0.,1.);
    FragColor=vec4(mix(color,aerial,fog_amount),1.-showcase_imprint_reactive*.75);
    IndirectColor=vec4(indirect*(1.-fog_amount),0);
}

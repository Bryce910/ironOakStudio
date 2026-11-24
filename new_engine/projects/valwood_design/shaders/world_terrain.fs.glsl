#version 330 core
#if defined(GL_ARB_texture_gather) && !defined(VALWOOD_QA_DISABLE_TEXTURE_GATHER)
#extension GL_ARB_texture_gather : enable
#define VALWOOD_TEXTURE_GATHER_ACTIVE 1
#endif
#ifdef VALWOOD_TEXTURE_GATHER_ACTIVE
uniform bool uShadowGather=true;
#endif
// SOURCE_CITY_GROUND_MASK: exact projected source mesh coverage.
uniform sampler2DArray uCityGroundCoverage;
uniform vec4 uCityGroundBounds;
uniform bool uCityGroundMaskReady=false;
// Optional surrounding mesh uses layer1. Layer0 and its source-grid mapping
// remain exact; the two coverage rasters can have different world bounds.
uniform vec4 uCountrysideGroundBounds;
uniform bool uCountrysideGroundMaskReady=false;
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
#pragma include "world_local_light_shadow.glsl"
#pragma include "world_storm_lighting.glsl"
#pragma include "world_authored_weather.glsl"
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
    bool authored=uAuthoredWeatherCount>0;
    float cloud=authored?authoredWeather(vPos.xz,vec3(0.0,uAuthoredWeatherGlobal.y,0.0)).y:uCloud;
    float cloud_plane=authored?max(650.0,vPos.y+100.0):3200.0;
    if (cloud < .015 || uSun.y < .06 || vPos.y >= (authored?1500.0:3200.0)) return 1.0;
    vec2 p = (vPos.xz + uSun.xz / max(uSun.y, .12) * (cloud_plane - vPos.y)) * .00042;
    p += vec2(uTime * .0014, -uTime * .00055);
    const mat2 rotation = mat2(.80, -.60, .60, .80);
    float field = .67 * noise(p) + .33 * noise(rotation * p * 2.03 + 17.4);
    float threshold = .64 - clamp(cloud, 0.0, 1.0) * .30;
    float coverage = smoothstep(threshold, threshold + .17, field);
    coverage *= smoothstep(.015, .12, cloud);
    float shadow=1.0-.34*coverage*clamp(cloud*1.7,0.0,1.0);
    return authored?shadow*(1.0-.75*smoothstep(.65,.95,cloud)):shadow;
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
        attenuation*=local_light_shadow(world,N,uTorchPositionRadius[i].xyz);
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

void main() {
    vec2 localWetHistory=uWetStateEnabled?authoredWetness(vPos.xz,0.):vec2(authoredWeatherRain(vPos.xz,uWetness),0.);
    float localWeatherWetness=localWetHistory.x;
    // Derivatives precede discard and all spatial/material branches. Both
    // paving samples and the history field retain a defined mip footprint.
    vec2 paving_uv=vec2(vPos.x,-vPos.z)*.25;
    vec2 paving_dx=dFdx(paving_uv),paving_dy=dFdy(paving_uv);
    vec2 farm_dx=dFdx(vPos.xz),farm_dy=dFdy(vPos.xz);
    vec2 bank_dx=farm_dx*vec2(.224,-.224),bank_dy=farm_dy*vec2(.224,-.224);
    vec3 farm=village_farm_surface(vPos.xz,length(vec2(farm_dx.x,farm_dy.x)));
    // Zero retains the legacy ocean reflection's 20cm guard. An explicitly
    // raised/lowered pool clips at its real plane so its banks stay visible.
    if (uClipBelowWater && vPos.y < (uWaterClipLevel==0.0?.2:uWaterClipLevel)) discard;
    vec2 city_uv=(vPos.xz-uCityGroundBounds.xy)*uCityGroundBounds.zw;
    if(uCityGroundMaskReady && all(greaterThanEqual(city_uv,vec2(0))) && all(lessThanEqual(city_uv,vec2(1))) && texture(uCityGroundCoverage,vec3((city_uv*(vec2(textureSize(uCityGroundCoverage,0).xy)-1.)+.5)/vec2(textureSize(uCityGroundCoverage,0).xy),0)).r>.5)discard;
    vec2 countryside_uv=(vPos.xz-uCountrysideGroundBounds.xy)*uCountrysideGroundBounds.zw;
    if(uCountrysideGroundMaskReady && all(greaterThanEqual(countryside_uv,vec2(0))) && all(lessThanEqual(countryside_uv,vec2(1))) && texture(uCityGroundCoverage,vec3((countryside_uv*(vec2(textureSize(uCityGroundCoverage,0).xy)-1.)+.5)/vec2(textureSize(uCityGroundCoverage,0).xy),1)).r>.5)discard;
    float distance_to_eye = length(vRelative);
    vec3 geometric_normal = normalize(vNormal), N = geometric_normal;
    float forest_access=forest_sky_access(vPos);
    vec4 climate = texture(uClimate, atlas_uv(vPos.xz / uWorldSize + .5));
    vec4 authored=authored_material(vPos.xz);
    vec3 starting=starting_material(vPos.xz);
    float village_region=village_history_region(vPos);
    vec4 village_history=village_ground_history(vPos,village_region,
        paving_dx*vec2(4.,-4.),paving_dy*vec2(4.,-4.));
    authored.r=max(authored.r,starting.r);
    float broad = noise(vPos.xz * .0027);
    float patch = noise(vPos.xz * .017 + vec2(51.7, 19.4));
    // Metre-scale vegetation/litter transitions sit between the scanned
    // centimetre structure and the broad biome. Fade the sub-metre field
    // before it becomes an unstable pattern in distant terrain pixels.
    float material_detail = 1.0 - smoothstep(110.0, 420.0, distance_to_eye);
    // These ALU-only fields have exactly zero weight at the existing fade
    // endpoints. Keep every positive-weight mix expression unchanged.
    float meso = patch;
    if(material_detail>0.)meso=mix(patch,noise(vPos.xz*.23+vec2(8.1,31.7)),material_detail);
    float micro_detail=1.0-smoothstep(45.0,155.0,distance_to_eye);
    float micro=.5;
    if(micro_detail>0.)micro=mix(.5,noise(vPos.xz*1.17+vec2(71.4,9.2)),micro_detail);
    float floor_pattern = clamp(meso * .74 + micro * .26, 0.0, 1.0);
    float rainforest=rainforest_weight(vPos.xz)*smoothstep(.60,.96,climate.a);
    float woodland = max(starting.b, smoothstep(.15, .53, climate.r));
    if(rainforest>0.0)woodland=max(woodland,rainforest);
    float slope = 1.0 - clamp(N.y, 0.0, 1.0);
    // A low inland valley is still meadow. Only the macro shoreline band can
    // turn the low-elevation material into sand/gravel.
    float coast = 1.0 - smoothstep(.60, .96, climate.a);
    // Height follows the new dry bank geometry, while existing world-space
    // patches break up the strandline. Avoid a wide, constant brown contour.
    float strand_variation = (patch - .5) * 1.15 + (broad - .5) * .75;
    float beach = coast * (1.0 - smoothstep(1.0, 5.8, vPos.y - strand_variation));
    beach *= mix(.70, 1.0, smoothstep(.18, .72, patch + broad * .18));
    float rocky = smoothstep(.16 + (patch - .5) * .12, .51, slope);
    rocky = max(rocky, smoothstep(850.0, 1750.0, vPos.y) * smoothstep(.30, .68, patch) * .65);
    rocky=max(rocky,max(authored.g*.92,forest_outcrop*.88));
    float snow_line = 1720.0 + (broad - .5) * 330.0;
    float snow = smoothstep(snow_line, snow_line + 520.0, vPos.y);
    snow *= smoothstep(.30, .76, N.y + (patch - .5) * .23);
    vec2 ground_uv = vec2(vPos.x, -vPos.z) * .32;
    vec2 litter_uv = ground_uv * 1.18;
    vec3 weights = pow(abs(N), vec3(6));
    weights /= max(weights.x + weights.y + weights.z, .0001);
    vec3 signs = mix(vec3(-1), vec3(1), step(vec3(0), N));
    vec3 rock_position = vPos * .115;
    vec3 base = vec3(.79, .83, .86);
    float roughness = .79, ao = 1.0;
    float detail = 1.0 - smoothstep(90.0, 245.0, distance_to_eye);
    if (snow < .985) {
        vec3 meadow = meadow_color(ground_uv, mix(broad, floor_pattern, .76 * material_detail));
        vec3 organic_scan=meadow;
        float luminance = dot(meadow, vec3(.2126, .7152, .0722));
        // Retain photographic structure, with moss/grass rather than yellow hues.
        vec3 green = luminance * mix(vec3(.66,.94,.39),vec3(.41,.79,.33),climate.g);
        // Keep the scan's fallen leaves and grass-color variation; converting
        // every photographed texel to one green made the forest read as lawn.
        meadow = mix(meadow, green, mix(.72, .43, woodland)) * mix(.76, 1.03, floor_pattern);
        // Growth and drying happen in connected metre-wide patches, rather
        // than in independent noisy texels. Reuse the filtered-scale material
        // fields so this variation stays attached to the ground while walking.
        float surface_state=clamp(meso*.62+patch*.38,0.0,1.0);
        meadow*=mix(vec3(.87,.98,.84),vec3(1.08,1.035,.90),surface_state);
        // Keep photographed fallen leaves beneath the rainforest canopy.
        // Preserve broad color structure instead of turning the scan into lawn.
        meadow=mix(meadow,organic_scan*mix(.80,.98,surface_state),rainforest*.80);
        meadow=mix(meadow,meadow*vec3(.88,1.12,.82),authored.b*.55);
        vec3 litter_scan = texture(uDirt, litter_uv).rgb;
        float mineral = dot(litter_scan, vec3(.2126, .7152, .0722));
        // Fine earth keeps a warm tint while the photographed stones retain
        // their neutral mineral colors instead of becoming uniformly brown.
        vec3 soil = litter_scan * mix(vec3(.66, .54, .42), vec3(.93, .92, .86),
                                     smoothstep(.10, .35, mineral));
        float bare_soil = clamp(climate.r * .25 + smoothstep(.54, .82, floor_pattern) * .32 +
                                (1.0 - climate.g) * .12, 0.0, .65);
        float forest_floor=smoothstep(.28,.65,climate.r)*(1.-smoothstep(.30,.65,slope));
        if(rainforest>0.0)forest_floor=max(forest_floor,rainforest*(1.-smoothstep(.30,.65,slope)));
        float litter=max(bare_soil,forest_floor*mix(.34,.79,floor_pattern))*(1.-authored.b*.86);
        litter=max(litter,starting.b*mix(.35,.75,floor_pattern));
        float rainforest_ground=rainforest*(1.-beach)*(1.-rocky)*(1.-snow)
            *(1.-authored.r)*(1.-starting.g)*(1.-smoothstep(.24,.60,slope));
        if(rainforest_ground>0.0)litter=max(litter,rainforest_ground*mix(.22,.54,floor_pattern));
        // Break only the blended shoulder. The fully worn path remains earth,
        // and zero path coverage cannot grow a new dirty strip in the meadow.
        float shoulder=4.0*authored.r*(1.0-authored.r);
        float leaf_height=smoothstep(.20,.50,dot(organic_scan,vec3(.2126,.7152,.0722)));
        // Scanned leaf/stone height interrupts the soft earth edge. Reuse the
        // existing photographic sample; derivatives/mips filter distant detail.
        float shoulder_height=(meso-.5)*.36+(leaf_height-.5)*rainforest*.22*material_detail;
        float path_earth=clamp(authored.r+shoulder*shoulder_height,0.0,1.0);
        litter=max(litter,path_earth);
        // Leaf edges survive beside compacted soil, with filtered patch edges.
        float leaf_shoulder=rainforest*shoulder*(.25+.75*leaf_height)*material_detail;
        litter=clamp(litter-leaf_shoulder*.23,0.,1.);
        // Compacted centres stay mineral; leaf shoulders retain their captured
        // hues instead of turning into one uniform brown strip.
        vec3 trail_soil=mix(soil,soil*mix(vec3(.86,.91,.87),vec3(1.03,.99,.91),surface_state),rainforest*path_earth);
        base = mix(meadow, trail_soil, litter);
        float wet_runoff=rainforest*smoothstep(.52,.80,meso)*(1.-smoothstep(.02,.18,slope))*path_earth;
        base=mix(base,soil*vec3(.67,.69,.66),wet_runoff*.55);
        vec3 moss=luminance*vec3(.32,.52,.19)*mix(.78,1.03,micro);
        float moss_patches=max(authored.a*.65,forest_floor*climate.g*.38*
                              smoothstep(.38,.72,1.0-floor_pattern));
        if(rainforest_ground>0.0)moss_patches=max(moss_patches,
            rainforest_ground*smoothstep(.28,.70,1.-floor_pattern)*.46);
        base=mix(base,moss,moss_patches*(1.-authored.r)*(1.-starting.g));
        // The compact waystation mask retains metre-wide paths that the broad
        // district texture cannot resolve. Reuse the photographic gravel scan.
        float compacted_surface=0.0;
        if(starting.g>.005) {
            // The CPU mask owns paving coverage so grass ecology agrees with
            // shading. Reuse this same scan/UV for gravel, ARM and normals.
            // Cart/foot-worn earth and exposed aggregate share the scan's
            // stones, but have different broad colour and roughness. At the
            // former single uniform tint the entire market read as one tile.
            // The state field has no time/screen seed or hard cell boundaries.
            float aggregate=smoothstep(.24,.76,surface_state);
            vec3 gravel=mix(litter_scan,mineral*vec3(.97,1.0,.97),.48);
            vec3 packed_earth=mineral*vec3(.67,.56,.44)+litter_scan*.14;
            gravel=mix(packed_earth,gravel,mix(.10,.95,aggregate));
            gravel*=mix(.83,1.08,patch)*mix(.95,1.035,micro);
            base=mix(base,gravel,starting.g);
            compacted_surface=starting.g*(1.0-aggregate);
        }
        vec2 material = texture(uGrassARM, ground_uv).rg;
        vec2 litter_material=texture(uLitterPBR,vec3(litter_uv,1.)).rg;
        float mineral_weight=max(litter,starting.g);
        material=mix(material,litter_material,mineral_weight);
        float forest_litter_weight=0.;
        if(uForestLitter&&rainforest_ground>.001) {
            vec4 photographedMaterial=texture(uLitterPBR,vec3(litter_uv,4.));
            vec3 photographed=texture(uLitterPBR,vec3(litter_uv,2.)).rgb;
            photographed=mix(photographed/12.92,pow((photographed+.055)/1.055,vec3(2.4)),step(vec3(.04045),photographed));
            // Source displacement height breaks the leaf/soil edge; the worn
            // centre remains mineral and existing landscape geometry is retained.
            forest_litter_weight=rainforest_ground*(1.-smoothstep(.16,.88,path_earth))*smoothstep(.24,.58,photographedMaterial.a*.58+floor_pattern*.42)*.96;
            base=mix(base,photographed*mix(.84,1.04,surface_state),forest_litter_weight);
            material=mix(material,photographedMaterial.rg,forest_litter_weight);
        }

        ao = mix(1.0, material.r, .48);
        roughness = clamp(material.g, .58, .97);
        roughness=clamp(roughness+(surface_state-.5)*.075,.58,.97);
        roughness=mix(roughness,max(.50,roughness*.75),wet_runoff);
        // Compacted dry dust has broad highlights and a less craggy normal
        // than loose aggregate. Both stay rough; these are not wet puddles.
        roughness=mix(roughness,max(roughness,.91),compacted_surface*.65);
        if(rainforest_ground>0.0) {
            // Damp organic ground keeps the scan's variation and a broad,
            // rough response. This adds neither a global green tint nor gloss.
            float damp=rainforest_ground*mix(.55,1.0,floor_pattern);
            base*=1.-damp*.10;
            roughness=mix(roughness,max(.68,roughness*.92),damp);
        }
        if (beach > .01) {
            vec3 sand = texture(uSand, ground_uv * .7).rgb;
            float sand_value = dot(sand, vec3(.2126, .7152, .0722));
            // This scan contains pebbles and fine gravel. Keep that structure
            // and neutral stone color instead of recoloring the bank yellow.
            sand = mix(sand, sand_value * vec3(.97, .99, 1.01), .58) * .90;
            base = mix(base, sand, beach * (1.0 - rocky));
            float stones = smoothstep(.10, .38, sand_value);
            roughness = mix(roughness, mix(.90, .74, stones), beach);
        }
        if (rocky > .01) {
            float macro_lod=smoothstep(100.,650.,distance_to_eye);
            vec3 macro_position=vPos*.0065;
            macro_position.xz+=vec2(broad-.5,noise(vPos.xz*.0011+19.4)-.5)*1.7;
            vec3 rock=vec3(0);
            if(macro_lod<.995)rock=rock_color(rock_position,weights,signs);
            if(macro_lod>.005) {
                vec3 large_rock=rock_color(macro_position,weights,signs);
                float value=dot(large_rock,vec3(.2126,.7152,.0722));
                large_rock=mix(large_rock,value*vec3(.89,.94,.98),.42);
                rock=mix(rock,large_rock,macro_lod);
            }
            float strata = noise(vPos.xz * .0047 + vec2(vPos.y*.013, vPos.y*.007));
            rock *= mix(vec3(.72, .75, .75), vec3(.91, .86, .78), strata * .60);
            base = mix(base, rock, rocky);
            vec2 material = rock_material(rock_position, weights, signs);
            material=mix(material,vec2(.92,.84),macro_lod);
            roughness = mix(roughness, clamp(material.g, .48, .94), rocky);
            ao = mix(ao, mix(1.0, material.r, .58), rocky);
        }
        float rock_moss=forest_outcrop*rainforest*max(geometric_normal.y,0.)*
            (.12+.36*smoothstep(.38,.72,1.-floor_pattern));
        base=mix(base,moss,rock_moss);roughness=mix(roughness,max(roughness,.86),rock_moss);
        vec3 litter_mapped=vec3(0,0,1);
        if (detail > .005) {
            vec3 normal_detail = texture(uGrassNormal, ground_uv).xyz * 2.0 - 1.0;
            litter_mapped=texture(uLitterPBR,vec3(litter_uv,0.)).xyz*2.-1.;
            normal_detail=mix(normal_detail,litter_mapped,mineral_weight);
            if(forest_litter_weight>0.)normal_detail=mix(normal_detail,texture(uLitterPBR,vec3(litter_uv,3.)).xyz*2.-1.,forest_litter_weight);
            vec3 ground_normal = normalize(N + vec3(normal_detail.x, 0, -normal_detail.y) *
                mix(.32,.19,compacted_surface));
            if (rocky > .05) {
                vec3 tri_normal = rock_normal(rock_position, weights, signs);
                ground_normal = normalize(mix(ground_normal, normalize(N + tri_normal * .45), rocky));
            }
            N = normalize(mix(N, ground_normal, detail * (1.0 - snow * .8)));
        }
        if(village_paving>.001) {
            // Original tiled stone/soil PBR from the purchased set, four metres
            // per repeat. Retain soft authored road shoulders and mip filtering.
            vec2 uv=paving_uv;
            vec3 paving=village_linear(textureGrad(uVillagePBR,vec3(uv,0.),paving_dx,paving_dy).rgb);
            vec3 paving_arm=textureGrad(uVillagePBR,vec3(uv,2.),paving_dx,paving_dy).rgb;
            vec2 paving_offset=vec2(.371,.619);
            float paving_mix=0.;
            if(uVillageStochasticPaving&&village_region>0.) {
                // Offset, never rotate, all three source maps together. A
                // metre-scale world field breaks the four-metre repeat while
                // keeping UV derivatives and the normal's tangent frame intact.
                vec3 alternate_arm=textureGrad(uVillagePBR,vec3(uv+paving_offset,2.),paving_dx,paving_dy).rgb;
                float field=.65*noise(vPos.xz*.11+vec2(7.1,41.7))+.35*patch;
                paving_mix=smoothstep(.28,.72,field+.22*(alternate_arm.r-paving_arm.r))*village_region;
                vec3 alternate=village_linear(textureGrad(uVillagePBR,vec3(uv+paving_offset,0.),paving_dx,paving_dy).rgb);
                paving=mix(paving,alternate,paving_mix);
                float rough_squared=mix(paving_arm.g*paving_arm.g,alternate_arm.g*alternate_arm.g,paving_mix);
                paving_arm=mix(paving_arm,alternate_arm,paving_mix);
                paving_arm.g=sqrt(max(rough_squared,0.));
            }
            // Actual foundation/cargo sections deposit earth into the stone's
            // recesses; worn footways retain exposed, slightly burnished stone.
            float recess=mix(.34,1.,1.-clamp(paving_arm.r,0.,1.));
            float deposit=clamp((village_history.r*.83+village_history.a*.72)*recess,0.,.82);
            deposit*=1.-village_history.g*.45;
            // Selected-project finish: broad wear stays in world space. Soil
            // gathers in captured stone recesses rather than staining every
            // cobble equally; the existing walked lanes remain readable.
            float stone_age=0.;
            if(uForestLitter) {
                stone_age=smoothstep(.24,.78,noise(vPos.xz*.19+vec2(27.3,5.1)));
                float joint=1.-smoothstep(.38,.86,paving_arm.r);
                deposit=clamp(deposit+joint*(.13+.21*stone_age)*(1.-village_history.g*.55),0.,.82);
                paving*=mix(vec3(.75,.78,.72),vec3(.97,.94,.86),stone_age);
            }
            float burnish=village_history.g*(1.-deposit*.65);
            float damp=village_history.b*mix(.30,.80,clamp(localWeatherWetness,0.,1.));
            vec3 deposit_base=soil*vec3(.92,.93,.94);
            paving=mix(paving,deposit_base,deposit);
            paving*=1.+burnish*.045-damp*.18;
            float paving_roughness=mix(clamp(paving_arm.g,.38,.98),clamp(litter_material.g,.72,.98),deposit);
            paving_roughness=mix(paving_roughness,max(.60,paving_roughness-.065),burnish);
            paving_roughness=mix(paving_roughness,.43,damp*.72);
            if(uForestLitter)paving_roughness=clamp(paving_roughness+(stone_age-.5)*.08,.42,.98);
            base=mix(base,paving,village_paving);
            roughness=mix(roughness,paving_roughness,village_paving);
            ao=mix(ao,mix(1.,mix(paving_arm.r,litter_material.r,deposit),.65),village_paving);
            if(detail>.005) {
                vec3 mapped=textureGrad(uVillagePBR,vec3(uv,1.),paving_dx,paving_dy).xyz*2.-1.;
                if(uVillageStochasticPaving&&village_region>0.)
                    mapped=mix(mapped,textureGrad(uVillagePBR,vec3(uv+paving_offset,1.),paving_dx,paving_dy).xyz*2.-1.,paving_mix);
                float variance=max(1.-dot(mapped,mapped),0.);
                roughness=mix(roughness,sqrt(min(1.,roughness*roughness+variance*.35)),village_paving);
                mapped=normalize(mix(mapped,litter_mapped,deposit));
                vec3 paving_normal=normalize(geometric_normal+vec3(mapped.x,0.,-mapped.y)*.7*(1.-burnish*.26)*(1.-damp*.16));
                N=normalize(mix(N,paving_normal,village_paving*detail));
            }
        }
        // Sediment and wall-foot earth bridge the paving/grass shoulder using
        // the same soil maps. No displaced height or ecology/collision change.
        float verge=clamp(village_history.a*.58+village_history.r*.38,0.,.72)*(1.-village_paving);
        if(verge>.001) {
            base=mix(base,soil,verge);roughness=mix(roughness,clamp(litter_material.g,.72,.98),verge);
            ao=mix(ao,mix(1.,litter_material.r,.48),verge);
            if(detail>.005)N=normalize(mix(N,normalize(geometric_normal+vec3(litter_mapped.x,0.,-litter_mapped.y)*.25),verge*detail));
        }
        // Sediment uses the same photographed fine aggregate as the coast,
        // with explicit gradients calculated before any material branch. Wet
        // contact follows the real bed height, never an independent oval mask.
        // This makes the transmitted bottom meet the dry bank continuously.
        float freshwater=0.,freshwater_level=uFreshwaterLevel;
        if(all(greaterThan(uFreshwaterPool.zw,vec2(0)))) {
            vec2 pool_local=(vPos.xz-uFreshwaterPool.xy)*uFreshwaterPool.zw;
            if(all(lessThan(abs(pool_local),vec2(1.5))))
                freshwater=1.-smoothstep(1.02,1.48,length(pool_local));
        }
        if(uHasVillage&&all(greaterThan(uVillageBounds.zw,vec2(0)))) {
            vec2 town_center=uVillageBounds.xy+.5/uVillageBounds.zw;
            vec2 town_local=abs(vPos.xz-town_center);
            // The complete town interior stays dry. The creek bed is the
            // only lowered surface in this narrow exterior terrace reserve.
            float exterior=step(205.,max(town_local.x,town_local.y))*
                (1.-step(225.,town_local.x))*(1.-step(223.,town_local.y));
            if(exterior>0.) { freshwater=exterior;freshwater_level=22.9; }
        }
        float bank_height=vPos.y-freshwater_level;
        float bank= freshwater*(1.-smoothstep(.28,1.05,bank_height));
        if(bank>.001) {
            vec3 gravel=textureGrad(uSand,ground_uv*.7,bank_dx,bank_dy).rgb;
            float grain=dot(gravel,vec3(.2126,.7152,.0722));
            gravel=mix(gravel,grain*vec3(.94,.97,.93),.55)*.80;
            float silt=smoothstep(.27,.73,surface_state)*.70+.15;
            vec3 sediment=mix(gravel,soil*vec3(.88,.91,.82),silt);
            float wet_edge=1.-smoothstep(-.02,.46+(meso-.5)*.24,bank_height);
            sediment*=mix(1.,.73,wet_edge);
            base=mix(base,sediment,bank*(1.-village_paving));
            float bank_rough=mix(clamp(litter_material.g,.72,.96),.42,wet_edge*.80);
            roughness=mix(roughness,bank_rough,bank);
            ao=mix(ao,mix(1.,litter_material.r,.38),bank);
            if(detail>.005) {
                vec3 bed_normal=normalize(geometric_normal+
                    vec3(litter_mapped.x,0.,-litter_mapped.y)*mix(.22,.12,wet_edge));
                N=normalize(mix(N,bed_normal,bank*detail));
            }
        }
        float damp_reach = .55 + patch * 1.15 + broad * .45;
        float wet = coast * (1.0 - smoothstep(.04, damp_reach, vPos.y)) * (1.0 - snow);
        base *= mix(1.0, .66, wet);
        roughness = mix(roughness, .36, wet * .70);
    }
    vec3 snow_color = vec3(.79, .83, .86) * mix(.91, 1.02, patch);
    base = mix(base, snow_color, snow);
    roughness = mix(roughness, .77, snow);
    ao = mix(ao, 1.0, snow);
    base *= mix(.92, 1.04, broad);
    // Beyond individual needles, woodland reads as a continuous canopy. Match
    // the scanned conifer palette so distant silhouette cards merge into it.
    float canopy=smoothstep(.13,.51,climate.r)*smoothstep(220.,900.,distance_to_eye);
    canopy*=smoothstep(.46,.83,geometric_normal.y)*(1.-snow);
    canopy*=(1.-smoothstep(.18,.75,authored.g))*(1.-authored.b*.94);
    vec3 canopy_base=mix(vec3(.027,.047,.020),vec3(.061,.082,.030),patch*.65+broad*.35);
    base=mix(base,canopy_base,canopy*.94);
    roughness=mix(roughness,.93,canopy);
    float farm_weight=farm.x*(1.-smoothstep(.1,.7,abs(vPos.y-24.)));
    if(farm_weight>0.) {
        vec2 uv=vec2(vPos.x,-vPos.z)*.23;
        vec3 scan=textureGrad(uDirt,uv,farm_dx*vec2(.23,-.23),farm_dy*vec2(.23,-.23)).rgb;
        float grain=dot(scan,vec3(.2126,.7152,.0722));
        vec3 earth=(scan*vec3(.40,.29,.19)+grain*vec3(.18,.105,.055))*(.92+.13*farm.y);
        earth*=mix(.86,1.10,meso);
        base=mix(base,earth,farm_weight);
        roughness=mix(roughness,.90,farm_weight);
        ao=mix(ao,.94+.04*farm.y,farm_weight);
        N=normalize(mix(N,normalize(geometric_normal+vec3(-farm.z,0.,0.)),farm_weight));
    }
    float surfaceMetal=0.;studioSurface(vPos,vPos.xz,mat3(vec3(1,0,0),vec3(0,0,1),geometric_normal),true,base,roughness,surfaceMetal,N,ao);
    editor_surface(vPos,base,roughness);
    vec3 V = normalize(-vRelative);
    vec3 graphEmission=vec3(0);
#ifdef VALWOOD_STUDIO_GRAPH
    float graphNormal=1.;if(studioGraphSurface(vPos.xz,vPos,N,V,uTime,base,roughness,surfaceMetal,graphNormal,graphEmission)){base=clamp(base,vec3(0),vec3(2));roughness=clamp(roughness,.04,1.);graphEmission=clamp(graphEmission,vec3(0),vec3(8));surfaceMetal=clamp(surfaceMetal,0.,1.);vec3 graphN=mix(geometric_normal,N,clamp(graphNormal,0.,2.));N=dot(graphN,graphN)>1e-10?normalize(graphN):geometric_normal;}
#endif
    // One weather layer for earth, paving and graph materials, after authored layers.
    surfaceMetal=clamp(surfaceMetal,0.,1.);
    material_wet_surface(base,roughness,surfaceMetal,false,localWeatherWetness);
    if(uWetStateEnabled)material_wet_film(base,roughness,surfaceMetal,false,localWetHistory.y,N,vPos.xz);
    if(uWetStateEnabled)material_rain_runoff(base,roughness,authoredWeatherRain(vPos.xz,uWetness)*village_sky*forest_access,localWetHistory.y,N,vPos,uTime);
    vec3 normalDx=dFdx(N),normalDy=dFdy(N);
    roughness=sqrt(min(1.,roughness*roughness+min(.12,max(dot(normalDx,normalDx),dot(normalDy,normalDy))*.35)));
    bool legacy_ambient = max(max(uSkyLight.r, uSkyLight.g), uSkyLight.b) <= 0.0;
    vec3 ground_light = legacy_ambient ? uAmbient * vec3(.48, .50, .43) : uGroundLight;
    vec3 sky_light = legacy_ambient ? uAmbient * vec3(1.25, 1.38, 1.52) : uSkyLight;
    if(village_sky<1.&&any(greaterThan(uVillageBounceAlbedo,vec3(0)))) {
        // Match the architecture's single local ground return. The existing
        // hemisphere/contact terms apply it to geometry, never direct light.
        vec3 incident=sky_light+uSunColor*cloud_transmittance()*max(uSun.y,0.);
        ground_light+=clamp(uVillageBounceAlbedo,vec3(0),vec3(.8))*incident*
            village_ground_sky*(1.-village_sky);
    }
    float sky_access = sky_visibility(geometric_normal)*village_sky*forest_access;
    // The macro hemisphere follows the surface, not the normal-map grain.
    float hemisphere = material_hemisphere(geometric_normal,false);
    vec3 indirect = mix(ground_light, sky_light * sky_access, hemisphere);
    // A small, material-colored return from obstructing ground keeps sheltered
    // slopes from acquiring uniformly black creases. The supplied ground
    // irradiance bounds its energy; this does not trace secondary light paths.
    indirect += ground_light * base * (1.0 - sky_access) * .28;
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.)indirect=mix(indirect,environment_diffuse(geometric_normal)*mix(1.,sky_access,hemisphere),uEnvironmentStrength);
#endif
    indirect=local_diffuse(vPos,geometric_normal,indirect);
    // Same low-frequency reflected hemisphere and opaque response as models.
    // The 16-sampler terrain path adds no cube, lookup texture or render pass.
    vec3 reflected=reflect(-V,geometric_normal);
    vec3 radiance=mix(ground_light,sky_light*sky_access,clamp(.5+.5*reflected.y,0.,1.));
    radiance=mix(radiance,indirect,roughness*roughness*.75);
    vec3 indirect_color=material_indirect(base,surfaceMetal,roughness,material_f0(base,surfaceMetal),geometric_normal,V,indirect,radiance,ao);
    vec3 color = indirect_color;
    float direct_visibility = min(visibility(geometric_normal), macro_visibility());
    color += brdf(base, surfaceMetal, roughness, N, V, uSun) * uSunColor * PI * direct_visibility * cloud_transmittance();
    color += torch_surface(base,surfaceMetal,roughness,N,V);
    color += stormFlashLight(vRelative,N,V,base,surfaceMetal,roughness,sky_access);
    float density = uHaze * mix(1.12, .48, smoothstep(60.0, 1900.0, vPos.y));
    float fog = max(1.0 - exp(-distance_to_eye * density), smoothstep(10500.0, 14900.0, distance_to_eye));
    float forward_scatter = pow(max(dot(-V, uSun), 0.0), 8.0);
    vec3 aerial_color = uFog + uSunColor * forward_scatter * .035;
    float fog_amount = clamp(fog, 0.0, 1.0);
#ifdef VALWOOD_HDR_ENVIRONMENT
    color=min(color,vec3(60000));
#endif
    color+=graphEmission;
    FragColor = vec4(mix(color, aerial_color, fog_amount), 1);
    // The contact pass attenuates indirect material response without darkening
    // direct sunlight or aerial scattering again. Single-target fixtures ignore this.
    IndirectColor = vec4(indirect_color * (1.0 - fog_amount), 0);
}

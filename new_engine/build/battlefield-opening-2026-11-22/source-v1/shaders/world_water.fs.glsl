#version 330 core
#pragma include "world_authored_weather.glsl"
#pragma include "world_storm_lighting.glsl"

in vec3 vRelative, vPos;
in vec4 vRiverFlow; // world direction XZ, distance to channel bank, downhill grade
#ifdef VALWOOD_STUDIO_WATER
uniform vec3 uStudioWaterTint,uStudioWaterAbsorption;
uniform vec4 uStudioWaterParams; // roughness, waves, flow, foam
#endif
layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 IndirectColor;
uniform vec3 uSun, uSunColor, uAmbient, uFog, uSkyLight, uGroundLight, uSkyZenith;
uniform vec3 uForward,uRight,uUp;
uniform float uEyeHeight,uNear,uFar,uCloud;
uniform float uTanHalfFov=.5994296;
uniform vec2 uResolution,uJitterNDC;
uniform sampler2D uOpaqueColor,uOpaqueDepth;
uniform bool uHasOpaque;
uniform sampler2D uShadow;
uniform mat4 uLightVP;
uniform float uTime, uHaze,uWaterLevel,uRainIntensity;
uniform vec2 uOrigin, uWorldSize,uWeatherWind;
uniform bool uLocalPool,uCastleCreek,uRiverSurface,uAuthoredPond;
uniform vec4 uPoolBounds;
// The same bounded polyline excavates the CPU terrain. A rectangle alone
// must never put water through a courtyard or a temporary terrain gap.
uniform vec2 uCastleCreekOrigin;
uniform vec3 uCastleCreekPoints[32];
uniform int uCastleCreekPointCount;
uniform sampler2D uClimate;
uniform sampler2D uReflection;
#ifdef VALWOOD_HDR_ENVIRONMENT
uniform samplerCube uEnvironmentCube;
uniform vec2 uEnvironmentRotation=vec2(1,0);
uniform float uEnvironmentIntensity=1,uEnvironmentStrength=0,uEnvironmentMip=8;
#endif
uniform mat4 uReflectionVP;
uniform vec3 uReflectionShift;
uniform bool uHasReflection;
// Reflection work scales independently of water optics. Defaults preserve the
// accepted High path; all runtime budgets stay within fixed driver loop bounds.
uniform int uSSRSteps=24,uSSRRefineSteps=6;
uniform bool uFilteredReflections=true;
// Low/Medium/High/Ultra. Independent of whether the user enables reflection
// rendering: shore contact remains present even on the lowest tier.
uniform int uSurfaceDetail=2;
// The one shared mirror represents a nearby horizontal river plane. Sloped
// fragments smoothly fall back instead of sampling an unrelated mirror plane.
uniform float uRiverReflectionLevel;
uniform bool uSoftShadows;

// Bounded, camera-selected practicals; source geometry and flicker stay fixed in world space.
#ifdef VALWOOD_STUDIO_SURFACES
uniform vec4 uTorchDirectionCone[8];
#endif
uniform int uTorchCount;
uniform vec4 uTorchPositionRadius[8],uTorchColorIntensity[8],uTorchWallPlane[8],uTorchClipBounds[8];
uniform vec2 uTorchClipHeights[8];
uniform vec3 uTorchClusterMin,uTorchClusterMax;

#ifdef VALWOOD_LOCAL_WIND
#pragma include "world_surface_wind.glsl"
#else
vec2 studioWind(vec3 world,vec2 base){return base;}
#endif
const float PI = 3.14159265;
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
float linear_depth(float d) {
    return 2.*uNear*uFar/max(uFar+uNear-(d*2.-1.)*(uFar-uNear),.0001);
}
vec3 view_ray(vec2 uv) {
    vec2 q=uv*2.-1.-uJitterNDC;
    return uForward+q.x*(uResolution.x/uResolution.y)*uTanHalfFov*uRight+q.y*uTanHalfFov*uUp;
}
// Read-only opaque buffers contain the visible banks and canopy at their
// actual elevations. Use them only where the single local river mirror is
// geometrically invalid. Projection is the inverse of view_ray(), including
// the current temporal jitter; linear depth is forward distance, not ray length.
bool river_screen_position(vec3 p,out vec2 uv,out float z) {
    z=dot(p,uForward);
    if(z<=uNear+.015)return false;
    vec2 view=vec2(dot(p,uRight),dot(p,uUp));
    uv=(view/(z*uTanHalfFov*vec2(uResolution.x/uResolution.y,1.))+uJitterNDC)*.5+.5;
    vec2 border=1.5/uResolution;
    return all(greaterThan(uv,border))&&all(lessThan(uv,1.-border));
}
// Approximate the finite GGX reflection footprint in the opaque snapshot.
// A short ray cone grows with hit travel; nearby water does not receive a fixed
// giant screen blur. Reject foreground and submerged samples, while allowing
// sky behind the hit to soften a canopy silhouette. Every accepted tap is
// renormalized: roughness redistributes reflected energy instead of darkening.
vec3 river_screen_filter(vec2 uv,float hit_depth,float travel,vec3 surface_normal,float roughness,out float geometry_support) {
    geometry_support=1.;
    ivec2 size=textureSize(uOpaqueDepth,0),limit=size-1;
    ivec2 center=clamp(ivec2(uv*vec2(size)),ivec2(0),limit);
    vec3 sum=max(texelFetch(uOpaqueColor,center,0).rgb,vec3(0))*.4;
    float weight=.4;
    // A sloping Medium creek cannot use its single horizontal mirror. Its
    // SSR hit needs a one-pixel coverage footprint too: one nearest opaque
    // texel otherwise exposes stationary leaf/LOD masks as dark stipple.
    // Keep physical roughness and the existing depth/clearance tests intact.
    bool medium_coverage_aa=!uFilteredReflections&&uSurfaceDetail>=1;
    if(!uFilteredReflections&&!medium_coverage_aa)return sum/weight;
    float cone=roughness*roughness*travel/max(hit_depth,.2);
    float radius=clamp(cone*float(size.y)/max(2.*uTanHalfFov,.1)*.55,.65,5.);
    if(medium_coverage_aa)radius=max(radius,1.25);
    if(radius<.85)return sum/weight; // subpixel cone needs no extra depth/color taps
    vec2 offsets[4]=vec2[4](vec2(1,0),vec2(-1,0),vec2(0,1),vec2(0,-1));
    float supported_neighbors=0.;
    for(int i=0;i<4;i++) {
        vec2 sample_uv=uv+offsets[i]*radius/vec2(size);
        if(any(lessThan(sample_uv,vec2(0)))||any(greaterThanEqual(sample_uv,vec2(1))))continue;
        ivec2 pixel=clamp(ivec2(sample_uv*vec2(size)),ivec2(0),limit);
        float raw=texelFetch(uOpaqueDepth,pixel,0).r;
        float depth=linear_depth(raw);
        vec2 pixel_uv=(vec2(pixel)+.5)/vec2(size);
        bool valid=raw>=.99999;
        if(!valid) {
            vec3 actual=view_ray(pixel_uv)*depth;
            valid=depth>=hit_depth-max(.12,hit_depth*.006)&&
                dot(actual-vRelative,surface_normal)>.035;
            if(medium_coverage_aa&&valid) {
                // A lone opaque leaf/LOD texel is not a coherent SSR surface.
                // Reuse these exact depth reads to test nearby geometry, not
                // sky or an unrelated bank behind the hit. Color still uses
                // its original normalized filter; only hit confidence fades.
                float tolerance=max(.12,hit_depth*.006);
                float same_surface=1.-smoothstep(tolerance*.5,tolerance,abs(depth-hit_depth));
                float above=smoothstep(.035,.10,dot(actual-vRelative,surface_normal));
                supported_neighbors+=same_surface*above;
            }
        }
        if(valid) {
            sum+=max(texelFetch(uOpaqueColor,pixel,0).rgb,vec3(0))*.15;
            weight+=.15;
        }
    }
    if(medium_coverage_aa)geometry_support=smoothstep(.25,.75,supported_neighbors*.25);
    return sum/weight;
}
vec4 river_screen_reflection(vec3 direction,vec3 surface_normal,float eye_distance,float roughness) {
    int steps=clamp(uSSRSteps,0,32),refine_steps=clamp(uSSRRefineSteps,0,8);
    if(steps==0)return vec4(0.); // Low keeps all other water shading, with no SSR reads
    float above=dot(direction,surface_normal);
    float range_confidence=(1.-smoothstep(80.,140.,eye_distance))*smoothstep(.025,.12,above);
    if(range_confidence<=0.)return vec4(0.);
    vec3 origin=vRelative+surface_normal*.045;
    float previous=0.,previous_gap=0.;
    // A bounded non-stochastic march has no screen-space noise or frame-varying
    // hash. Quadratic spacing resolves nearby roots without tracing the entire
    // forest. Low/Medium/High/Ultra request 0/12/24/32 samples, with 0/4/6/8
    // refinement samples. Quadratic spacing spans the same 64 m at every tier;
    // a filtered hit adds at most four bounded filter-depth checks.
    for(int i=1;i<=32;i++) {
        if(i>steps)break;
        float phase=steps==24?float(i)/24.:float(i)/float(steps);
        float travel=.08+63.92*phase*phase;
        vec3 p=origin+direction*travel;
        vec2 uv;float z;
        if(!river_screen_position(p,uv,z))break;
        float raw=texture(uOpaqueDepth,uv).r;
        float scene=linear_depth(raw);
        if(raw<.99999&&z>scene) {
            float lo=previous,hi=travel;
            float lo_gap=previous>0.?previous_gap:dot(origin,uForward)-scene;
            float hi_gap=z-scene;
            // Refine the first front-to-back depth crossing. A discontinuity at
            // a silhouette is accepted only if the final actual depth agrees.
            for(int j=0;j<8;j++) {
                if(j>=refine_steps)break;
                float mid=(lo+hi)*.5;
                vec2 q;float qz;
                if(!river_screen_position(origin+direction*mid,q,qz))return vec4(0.);
                float depth=texture(uOpaqueDepth,q).r;
                if(depth<.99999&&qz>linear_depth(depth)) {
                    hi=mid;
                    if(steps<24)hi_gap=qz-linear_depth(depth);
                } else {
                    lo=mid;
                    if(steps<24)lo_gap=qz-linear_depth(depth);
                }
            }
            float hit_travel=hi;
            if(steps<24&&lo_gap<=0.&&hi_gap>=0.&&hi_gap-lo_gap>.00001) {
                // Four Medium bisections can leave a wider bracket than the
                // hit thickness. Interpolate its measured signed residuals,
                // then keep the same independent depth/clearance validation.
                // This costs no extra texture read; High/Ultra retain hi.
                float fraction=clamp(-lo_gap/(hi_gap-lo_gap),0.,1.);
                hit_travel=min(hi,mix(lo,hi,fraction)+.001);
            }
            vec3 hit=origin+direction*hit_travel;
            if(!river_screen_position(hit,uv,z))return vec4(0.);
            raw=texture(uOpaqueDepth,uv).r;scene=linear_depth(raw);
            float gap=z-scene;
            float thickness=clamp(.10+scene*.0015,.10,.28);
            if(raw>=.99999||gap<0.||gap>thickness)return vec4(0.);
            // Reject riverbed/self hits, screen-edge guesses and distant
            // unsupported intersections. No fabricated tree tint replaces sky.
            vec3 actual=view_ray(uv)*scene;
            float clearance=dot(actual-vRelative,surface_normal);
            float edge=min(min(uv.x,uv.y),min(1.-uv.x,1.-uv.y));
            float confidence=range_confidence*smoothstep(.015,.085,edge)*
                (1.-smoothstep(44.,64.,hit_travel))*smoothstep(.035,.18,clearance);
            float geometry_support;
            vec3 sample_color=river_screen_filter(uv,scene,hit_travel,surface_normal,roughness,geometry_support);
            return vec4(sample_color,confidence*geometry_support);
        }
        previous=travel;previous_gap=z-scene;
    }
    return vec4(0.);
}
float cloud_visibility(vec2 world) {
    if(uCloud<.015||uSun.y<.06||uWaterLevel>=3200.)return 1.;
    vec2 p=(world+uSun.xz*(3200.-uWaterLevel)/max(uSun.y,.12))*.00042+vec2(uTime*.0014,-uTime*.00055);
    const mat2 rotation=mat2(.80,-.60,.60,.80);
    float field=.67*noise(p)+.33*noise(rotation*p*2.03+17.4);
    float density=smoothstep(.64-uCloud*.30,.81-uCloud*.30,field);
    density*=smoothstep(.015,.12,uCloud);
    return 1.-.34*density*clamp(uCloud*1.7,0.,1.);
}
float glint_visibility() {
    float distance=length(vRelative.xz);
    if(distance>=160.)return 1.;
    vec4 q=uLightVP*vec4(vRelative,1.);
    vec3 p=q.xyz/q.w*.5+.5;
    if(any(lessThan(p,vec3(.002)))||any(greaterThan(p,vec3(.998))))return 1.;
    vec2 texel=1./vec2(textureSize(uShadow,0));
    if(uSoftShadows) {
        vec2 sample_position=p.xy*vec2(textureSize(uShadow,0))-.5;
        ivec2 center=ivec2(floor(sample_position+.5));
        vec2 d=sample_position-vec2(center);
        vec3 wx=vec3(.5*(.5-d.x)*(.5-d.x),.75-d.x*d.x,.5*(.5+d.x)*(.5+d.x));
        vec3 wy=vec3(.5*(.5-d.y)*(.5-d.y),.75-d.y*d.y,.5*(.5+d.y)*(.5+d.y));
        ivec2 limit=textureSize(uShadow,0)-1;
        float filtered=0.;
        for(int y=0;y<3;y++)for(int x=0;x<3;x++)
            filtered+=wx[x]*wy[y]*step(p.z-.00017,texelFetch(uShadow,clamp(center+ivec2(x-1,y-1),ivec2(0),limit),0).r);
        return mix(filtered,1.,smoothstep(125.,160.,distance));
    }
    float visible=0.;
    for(int i=0;i<4;i++) {
        vec2 d=vec2((i&1)==0?-.65:.65,(i&2)==0?-.65:.65)*texel;
        visible+=step(p.z-.00017,texture(uShadow,p.xy+d).r);
    }
    return mix(visible*.25,1.,smoothstep(125.,160.,distance));
}
float point_reflection(vec3 N, vec3 V, vec3 L, float roughness) {
    vec3 sum = V + L;
    vec3 H = sum * inversesqrt(max(dot(sum, sum), .0001));
    float nl = max(dot(N, L), 0.0), nv = max(dot(N, V), .001);
    float nh = max(dot(N, H), 0.0), vh = max(dot(V, H), 0.0);
    float a = roughness * roughness, a2 = a * a;
    float denominator = nh * nh * (a2 - 1.0) + 1.0;
    float D = a2 / max(PI * denominator * denominator, .00000001);
    float k = roughness * roughness * .5;
    float G = nl / (nl * (1.0 - k) + k) * nv / (nv * (1.0 - k) + k);
    float F = .0204 + .9796 * pow(1.0 - vh, 5.0);
    return min(18.0, D * G * F / max(4.0 * nv, .001));
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

float sun_reflection(vec3 N,vec3 V,float roughness) {return point_reflection(N,V,uSun,roughness);}
vec3 torch_water(vec3 world,vec3 N,vec3 V,float roughness) {
    if(uTorchCount<=0||any(lessThan(world,uTorchClusterMin))||any(greaterThan(world,uTorchClusterMax)))return vec3(0.);
    vec3 result=vec3(0.);
    for(int i=0;i<8;i++) {
        if(i>=uTorchCount)break;
        vec3 delta=uTorchPositionRadius[i].xyz-world;
        float attenuation=torch_attenuation(i,world,delta);
        if(attenuation<=0.)continue;
        vec3 L=delta*inversesqrt(max(dot(delta,delta),.000001));
        result+=uTorchColorIntensity[i].rgb*uTorchColorIntensity[i].w*attenuation*point_reflection(N,V,L,roughness);
    }
    return result;
}

vec2 rain_slope(vec2 world,float rain) {
    // Each impact remains inside its world cell: it has no cell-boundary
    // seams, neighboring-cell loop, texture, or camera-relative phase.
    vec2 grid=world*1.8,cell=floor(grid);
    float seed=hash(cell);
    vec2 center=.5+(vec2(hash(cell+17.1),hash(cell+43.7))-.5)*.22;
    vec2 delta=fract(grid)-center;
    float radius=length(delta),age=fract(uTime*(1.05+seed*.55)+seed*19.7);
    float ring=radius-age*.28;
    float phase=ring*72.;
    float filtered=1.-smoothstep(.5,2.0,fwidth(phase));
    float envelope=exp(-abs(ring)*32.)*(1.-smoothstep(.27,.33,radius));
    envelope*=smoothstep(0.,.06,age)*(1.-age)*(1.-age);
    return delta/max(radius,.001)*cos(phase)*envelope*filtered*rain*.022;
}
#ifdef VALWOOD_IMPORTED_SURFACES
uniform sampler2DArray uSurfaceTextures;
#define STUDIO_SURFACE_SAMPLER uSurfaceTextures
#pragma include "world_surface_library.fs.glsl"
#else
vec4 studioImage(int image,vec2 uv,vec2 dx,vec2 dy){return vec4(1.);}
void studioSurface(vec3 world,vec2 uv,mat3 basis,bool terrain,inout vec3 color,inout float rough,inout float metal,inout vec3 normal,inout float ao){}
#endif

// VALWOOD_GRAPH_SURFACE

void main() {
    IndirectColor=vec4(0);
    vec2 world = vPos.xz + uOrigin;
    vec2 channel_flow=vRiverFlow.xy;
    float channel_bank=vRiverFlow.z,channel_grade=vRiverFlow.w;
    if(uCastleCreek) {
        vec2 p=world-uCastleCreekOrigin;
        float nearest=2.;
        for(int i=1;i<uCastleCreekPointCount;i++) {
            vec3 a=uCastleCreekPoints[i-1],b=uCastleCreekPoints[i];
            vec2 delta=b.xy-a.xy;
            float t=clamp(dot(p-a.xy,delta)/max(dot(delta,delta),.000001),0.,1.);
            vec2 offset=p-a.xy-delta*t;
            float width=mix(a.z,b.z,t);
            float score=dot(offset,offset)/(width*width);
            if(score<nearest){nearest=score;channel_flow=delta/max(length(delta),.000001);channel_bank=width-length(offset);}
        }
        if(nearest>=1.)discard;
    }
    if(uLocalPool&&!uCastleCreek&&!uRiverSurface) {
        if(any(lessThanEqual(uPoolBounds.zw,vec2(0))))discard;
        vec2 local=(world-uPoolBounds.xy)*uPoolBounds.zw;
        if(dot(local,local)>1.)discard;
    }
    float actual_level=uRiverSurface?vPos.y:uWaterLevel;
    bool underwater_side=uEyeHeight<actual_level-.025;
    if(uRiverSurface) {
        // A distant uphill fragment can be above the camera while the camera
        // remains above its actual sloping plane. Classify the geometric side
        // in metres, not the fragment's absolute altitude. This normal is used
        // only for sidedness; the smooth flow normal still shades the water.
        vec3 plane=cross(dFdx(vRelative),dFdy(vRelative));
        float plane_length2=dot(plane,plane);
        plane=plane_length2>1.e-16?plane*inversesqrt(plane_length2):vec3(0,1,0);
        if(plane.y<0.)plane=-plane;
        underwater_side=dot(plane,-vRelative)<-.025;
    }
    if(underwater_side) {
        // Water-to-air view: a bright Snell window overhead, internal
        // reflection at grazing angles, and gentle surface motion below.
        vec3 toEye=normalize(-vRelative);
        float cosine=abs(toEye.y);
        float window=smoothstep(.64,.74,cosine);
        float ripple=sin(world.x*2.2+uTime*1.7)*sin(world.y*1.9-uTime*1.3);
        vec3 ceiling=mix(vec3(.022,.095,.082),mix(uSkyZenith,uFog,.35)*.72+uAmbient*.18,window);
        FragColor=vec4(max(ceiling*(.97+.03*ripple),vec3(0)),1.);
        return;
    }
    vec2 weatherWind=studioWind(vec3(world.x,vPos.y,world.y),uWeatherWind);
    bool flowing=uRiverSurface||uCastleCreek;
#ifdef VALWOOD_STUDIO_WATER
    vec3 water_tint=uStudioWaterTint,water_absorption=uStudioWaterAbsorption;
    // Only the former untouched creation preset is corrected. Artist-authored
    // colors, absorption and any other changed style parameter retain control.
    bool legacy_pond=uAuthoredPond&&uLocalPool&&!flowing&&
        all(lessThan(abs(water_tint-vec3(.65,.90,1.10)),vec3(.00001)))&&
        all(lessThan(abs(water_absorption-vec3(3.,1.,.35)),vec3(.00001)))&&
        all(lessThan(abs(uStudioWaterParams-vec4(1.2,.7,1.,.45)),vec4(.00001)));
    if(legacy_pond){water_tint=vec3(.92,.98,.94);water_absorption=vec3(1.8,2.,2.2);}
#endif
    vec2 channel_slope=channel_flow*channel_grade;
    float flow_coherence=clamp(length(channel_flow),0.,1.);
    if(flowing)channel_flow=dot(channel_flow,channel_flow)>.000001?normalize(channel_flow):vec2(1,0);
    float flow_speed=clamp(.32+sqrt(max(channel_grade,0.))*3.2,.32,1.9);
    // Opposing interpolated directions meet as calm water, rather than an
    // arbitrary X-facing ripple seam at the zero vector.
    if(flowing)flow_speed*=smoothstep(.04,.35,flow_coherence);
    // Two bounded advection phases avoid world-position dot varying-flow
    // derivatives and time-growing texture shear at bends/confluences.
    float flow_phase=fract(uTime*.35),flow_phase_b=fract(flow_phase+.5);
    float flow_blend=1.-abs(flow_phase*2.-1.);
    vec2 transported=world-channel_flow*(flow_phase*flow_speed/.35);
    vec2 transported_b=world-channel_flow*(flow_phase_b*flow_speed/.35);
    float distance_to_eye = length(vRelative);
    float rain=clamp(authoredWeatherRain(world,uRainIntensity),0.,1.),wind=min(length(weatherWind),3.);
    vec2 warp = vec2(noise(world * .037 + uTime * .003),
                     noise(world * .029 + vec2(31.7, 19.2) - uTime * .002)) * 2.0 - 1.0;
    float a = dot(world, vec2(.80, .60)) * .95 + warp.x * 3.3 + uTime * .72;
    float b = dot(world, vec2(-.35, .94)) * 1.73 + warp.y * 2.9 - uTime * .91;
    float c = dot(world, vec2(.95, -.31)) * 4.17 + (warp.x + warp.y) * 2.2 + uTime * 1.27;
    vec2 direction_a=vec2(.80,.60),direction_b=vec2(-.35,.94),direction_c=vec2(.95,-.31);
    if(wind>.00001) {
        vec2 direction=weatherWind/max(length(weatherWind),.00001);
        direction_a=direction;direction_b=vec2(-direction.y,direction.x);
        direction_c=direction*.92+direction_b*.39;
        vec2 advected=world-weatherWind*uTime*.22;
        a=dot(advected,direction_a)*.95+warp.x*3.3+uTime*.72;
        b=dot(advected,direction_b)*1.73+warp.y*2.9-uTime*.91;
        c=dot(advected,direction_c)*4.17+(warp.x+warp.y)*2.2+uTime*1.27;
    }
    if(flowing) {
        // Fixed world wave axes keep phase coherent across the shared mesh.
        // Their sampling positions advect with the interpolated downhill flow.
        direction_a=vec2(.80,.60);direction_b=vec2(-.35,.94);
        direction_c=vec2(.95,-.31);
        a=dot(transported,direction_a)*2.1+noise(transported*.17)*1.1;
        b=dot(transported,direction_b)*3.7+noise(transported*.31+19.2)*.7;
        c=dot(transported,direction_c)*6.8;
    }
    // Grazing-distance phase frequencies can exceed a pixel even for gentle
    // waves. Filter each component before it becomes repeating scanlines.
    vec3 wave_filter = 1.0 - smoothstep(vec3(.35), vec3(2.2),
                                      vec3(fwidth(a), fwidth(b), fwidth(c)));
    vec2 slope = direction_a * cos(a) * .034 * wave_filter.x +
                 direction_b * cos(b) * .023 * wave_filter.y +
                 direction_c * sin(c) * .009 * wave_filter.z;
    if(flowing) {
        float a2=dot(transported_b,direction_a)*2.1+noise(transported_b*.17)*1.1;
        float b2=dot(transported_b,direction_b)*3.7+noise(transported_b*.31+19.2)*.7;
        float c2=dot(transported_b,direction_c)*6.8;
        vec3 filter_b=1.-smoothstep(vec3(.35),vec3(2.2),vec3(fwidth(a2),fwidth(b2),fwidth(c2)));
        vec2 slope_b=direction_a*cos(a2)*.034*filter_b.x+
            direction_b*cos(b2)*.023*filter_b.y+direction_c*sin(c2)*.009*filter_b.z;
        slope=mix(slope_b,slope,flow_blend);
    }
    if(wind>.00001)slope*=1.+wind*.22;
    // A sheltered forest pool has a shorter wind fetch than open water.
    // Keep ripples and canopy reflections legible without giving the small
    // pond ocean-sized distortion. Rain's independent rings remain intact.
    if(uLocalPool&&!flowing)slope*=.34;
    if(flowing)slope*=.72+clamp(channel_grade*8.,0.,1.2);
    if(flowing&&uSurfaceDetail>=1) {
        // A narrow wave train reflects from the real channel banks. Its metre
        // distance field comes from the same river mesh that cuts the ground;
        // no screen-space depth gradient becomes a moving normal seam.
        float bank=max(channel_bank,0.);
        float bank_phase=bank*14.-uTime*2.6;
        float bank_filter=1.-smoothstep(.35,2.2,fwidth(bank_phase));
        float amplitude=(uSurfaceDetail>=2?.010:.006)*exp(-bank*2.8)*bank_filter;
        slope+=vec2(-channel_flow.y,channel_flow.x)*cos(bank_phase)*amplitude;
        if(uSurfaceDetail>=3)slope+=channel_flow*sin(bank_phase*.63+uTime*.75)*amplitude*.32;
    }
    if(rain>0.)slope+=rain_slope(world,rain);
    float footprint = max(length(dFdx(world)), length(dFdy(world)));
#ifdef VALWOOD_STUDIO_WATER
    slope*=uStudioWaterParams.y;
#endif
    // The shared CPU flow/grade field is smooth across triangle diagonals.
    // A raw per-triangle derivative normal reveals every 2m grid cell as a
    // shiny facet; downhill grade supplies the continuous physical slope.
    vec3 geometric=flowing?normalize(vec3(channel_slope.x,1.,channel_slope.y)):vec3(0,1,0);
    vec3 N = normalize(geometric+vec3(-slope.x,0,-slope.y));
    vec3 V = normalize(-vRelative);
    float roughness=clamp(.12+footprint*.007,.12,.26);
    if(wind>.00001||rain>0.)roughness=clamp(roughness+wind*.025+rain*.045,.12,.34);
#ifdef VALWOOD_STUDIO_WATER
    roughness=clamp(roughness*uStudioWaterParams.x,.06,.85);
#endif
    float fresnel = .0204 + .9796 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 reflection = reflect(-V, N);
    float daylight = clamp(dot(uSunColor, vec3(.2126, .7152, .0722)), .05, 1.0);
    vec3 zenith=dot(uSkyZenith,uSkyZenith)>.000001?uSkyZenith:vec3(.035,.105,.225)*mix(.30,1.,daylight);
    vec3 sky = mix(uFog, zenith,
                   pow(max(reflection.y, 0.0), .42));
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.){vec3 d=vec3(uEnvironmentRotation.x*reflection.x-uEnvironmentRotation.y*reflection.z,reflection.y,uEnvironmentRotation.y*reflection.x+uEnvironmentRotation.x*reflection.z);sky=mix(sky,max(textureLod(uEnvironmentCube,d,.12*uEnvironmentMip).rgb,vec3(0))*uEnvironmentIntensity,uEnvironmentStrength);}
#endif
    // Regional overcast fallback matches the planar sky's mirrored observer,
    // including its cloud-slab altitude fade. Keep the clear-weather expression
    // unchanged; reflection quality settings must not reveal a sunny blue gap.
    float overcast = smoothstep(.65, .95, clamp(uCloud, 0.0, 1.0))
        * (1.0 - smoothstep(2700.0, 4100.0, 2.0*uWaterLevel-uEyeHeight));
    if (overcast > 0.0) {
        const vec3 luminance = vec3(.2126, .7152, .0722);
        vec3 gray_zenith = max(dot(zenith, luminance), 0.0) * vec3(.94, 1.0, .98);
        vec3 gray_horizon = max(dot(uFog, luminance), 0.0) * vec3(.94, 1.0, .98);
        vec3 sheet = mix(gray_horizon, gray_zenith, pow(max(reflection.y, 0.0), .42));
        sky += uSunColor * pow(max(dot(reflection, uSun), 0.0), 28.0) * .065 * (1.0-overcast);
        sky = mix(sky, sheet, overcast);
    } else {
        sky += uSunColor * pow(max(dot(reflection, uSun), 0.0), 28.0) * .065;
    }
    // End regional overcast fallback.
    vec3 reflected_scene = sky;
    float planar_coverage=0.;
    if (uHasReflection) {
        // Medium rivers use a very small mirror with stationary foliage
        // coverage samples. Resolve their rough planar footprint coherently;
        // enlarging that unfiltered mask makes leaves read as black stipple.
        // Low retains one sample; SSR depth/filter budgets stay independent.
        bool filter_planar=uFilteredReflections||(uRiverSurface&&uSurfaceDetail>=1);
        vec4 projected = uReflectionVP * vec4(vRelative + uReflectionShift, 1);
        if (projected.w > .001) {
            vec2 uv = projected.xy / projected.w * .5 + .5;
            // Confidence follows the ACTUAL distorted/filter footprint in
            // mirror texels. A fixed fraction of screen width faded valid
            // reflected banks into a pale vertical band at either side.
            // Preserve projection/frustum rejection and never invent border
            // geometry by treating the subsequent safety clamp as coverage.
            if (all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))) {
                vec2 distortion = (uReflectionVP * vec4(N.x, 0, N.z, 0)).xy * .085;
                distortion = clamp(distortion, vec2(-.009), vec2(.009));
                vec2 texel = 1.0 / vec2(textureSize(uReflection, 0));
                vec2 sample_center=uv+distortion;
                float filter_radius=0.;
                float radius=clamp(.8+footprint*.055,.8,5.);
                if(filter_planar) {
                    if(uRiverSurface||wind>.00001||rain>0.) {
                        radius=clamp(.8+footprint*.055+rain*.85+wind*.30,.8,5.);
                        if(uRiverSurface) {
                            float cone=roughness*roughness*float(textureSize(uReflection,0).y)/max(2.*uTanHalfFov,.1);
                            radius=max(radius,clamp(cone*.55,1.,8.));
                        }
                    }
                    filter_radius=radius;
                }
                // One texel protects bilinear interpolation; a 1.5-texel ramp
                // avoids a hard confidence seam as the valid footprint exits.
                vec2 available=(min(sample_center,1.-sample_center)/texel)-vec2(filter_radius+1.);
                float coverage=smoothstep(0.,1.5,min(available.x,available.y));
                // An inward wave offset can fit its texture footprint even
                // while the unshifted projection exits the frustum. Keep that
                // rejection continuous in mirror texels, never screen percent.
                vec2 projection_edge=min(uv,1.-uv)/texel;
                coverage*=smoothstep(0.,1.5,min(projection_edge.x,projection_edge.y));
                if(uRiverSurface) {
                    coverage*=1.-smoothstep(.10,.40,abs(actual_level-uRiverReflectionLevel));
                    coverage*=1.-smoothstep(.02,.06,channel_grade);
                }
                if(coverage>0.) {
                vec2 sample_uv = clamp(sample_center, texel, 1.0 - texel);
                vec2 blur=texel*radius;
                vec3 filtered=texture(uReflection,sample_uv).rgb*.5;
                if(!filter_planar) {
                    filtered*=2.; // normalized single planar sample; optics stay unchanged
                } else if(uRiverSurface||wind>.00001||rain>0.) {
                    filtered*=.8;
                    filtered+=texture(uReflection,clamp(sample_uv+vec2(blur.x,0),texel,1.-texel)).rgb*.15;
                    filtered+=texture(uReflection,clamp(sample_uv-vec2(blur.x,0),texel,1.-texel)).rgb*.15;
                    filtered+=texture(uReflection,clamp(sample_uv+vec2(0,blur.y),texel,1.-texel)).rgb*.15;
                    filtered+=texture(uReflection,clamp(sample_uv-vec2(0,blur.y),texel,1.-texel)).rgb*.15;
                } else {
                    filtered+=texture(uReflection,clamp(sample_uv+blur,texel,1.-texel)).rgb*.25;
                    filtered+=texture(uReflection,clamp(sample_uv-blur,texel,1.-texel)).rgb*.25;
                }
                planar_coverage=coverage;
                reflected_scene = mix(sky,max(filtered,vec3(0)),coverage);
                }
            }
        }
    }
    if(uRiverSurface&&uHasOpaque&&uHasReflection&&planar_coverage<.98) {
        vec4 screen_scene=river_screen_reflection(reflection,geometric,distance_to_eye,roughness);
        // Planar coverage has already been applied once above. Replace only
        // its uncovered sky contribution, preserving p*planar exactly. Mixing
        // the whole result again gave planar p*(1-s*(1-p)) and retained a pale
        // sky band even where a fully confident screen-space hit existed.
        reflected_scene+=(screen_scene.rgb-sky)*screen_scene.a*(1.-planar_coverage);
    }
    float near_shore=1.;
    if(!uLocalPool) {
        vec2 atlas_size=vec2(textureSize(uClimate,0));
        vec2 atlas_uv=((world/uWorldSize+.5)*(atlas_size-1.)+.5)/atlas_size;
        near_shore=smoothstep(.12,.53,texture(uClimate,atlas_uv).a);
    }
    vec3 sky_light=dot(uSkyLight,uSkyLight)>.000001?uSkyLight:uAmbient;
    vec3 body=mix(vec3(.010,.045,.052),vec3(.028,.088,.076),near_shore);
    if(uLocalPool)body=uCastleCreek?vec3(.024,.041,.029):vec3(.018,.030,.020);
    body*=.35+clamp(dot(sky_light,vec3(.7)),.03,.8);
    float foam=0.,shore_coverage=1.;vec3 shore_bottom=body;
    if(uHasOpaque) {
        float surface_level=uRiverSurface?vPos.y:uWaterLevel;
        vec2 texel=1./uResolution;
        vec2 uv=clamp(gl_FragCoord.xy/uResolution,texel*.5,1.-texel*.5);
        float water_z=linear_depth(gl_FragCoord.z);
        float raw=texture(uOpaqueDepth,uv).r;
        float scene_z=linear_depth(raw);
        vec3 ray=view_ray(uv);
        float bottom_y=uEyeHeight+ray.y*scene_z;
        float bottom_below=surface_level-bottom_y;
        if(uLocalPool&&raw<1.&&scene_z<water_z-.001)discard;
        // Use geometric depth at the undistorted pixel for stable shore contact.
        // Refraction can only select geometry actually below the water plane.
        if(raw<1.&&scene_z>=water_z&&bottom_y<surface_level+.12) {
            float column=clamp((scene_z-water_z)*length(ray),0.,200.);
            vec2 bend=clamp(slope*vec2(1.,-.65)*.028,vec2(-.0025),vec2(.0025));
            bend*=smoothstep(.05,1.8,bottom_below);
            vec2 bent_uv=clamp(uv+bend,texel*.5,1.-texel*.5);
            // Validate every texel in the bilinear color footprint. Testing
            // only the center depth lets above-water banks leak into refraction.
            vec2 corner=(floor(bent_uv*uResolution-.5)+.5)*texel;
            bool safe_bend=true;
            for(int iy=0;iy<2;iy++)for(int ix=0;ix<2;ix++) {
                vec2 sample_uv=clamp(corner+vec2(ix,iy)*texel,texel*.5,1.-texel*.5);
                float bent_raw=texture(uOpaqueDepth,sample_uv).r;
                float bent_z=linear_depth(bent_raw);
                float bent_y=uEyeHeight+view_ray(sample_uv).y*bent_z;
                float surface_slack=uLocalPool?.005:.05;
                safe_bend=safe_bend&&bent_raw<1.&&bent_z>=water_z&&bent_y<surface_level+surface_slack;
            }
            if(safe_bend)uv=bent_uv;
            vec3 bottom=max(texture(uOpaqueColor,uv).rgb,vec3(0));
            // Water is composited from a read-only opaque snapshot, not alpha
            // blended. Fade its optical response in metres at bank contact;
            // keep the alpha channel reserved for temporal reactivity.
            shore_bottom=max(texture(uOpaqueColor,clamp(gl_FragCoord.xy/uResolution,texel*.5,1.-texel*.5)).rgb,vec3(0));
            shore_coverage=smoothstep(.008,.16,max(bottom_below,0.));
            vec3 extinction=uLocalPool?(uCastleCreek?vec3(.17,.14,.20):vec3(.15,.19,.27)):
                vec3(.28,.115,.070);
            // Authored freshwater carries suspended material; its measured
            // metre-scale optical depth should not expose a sharp grassy bowl.
            // The built-in forest pool, ocean and flowing channels retain
            // their own absorption response and all authored style controls.
            if(uAuthoredPond&&!flowing)extinction=vec3(.7,.6,.5);
#ifdef VALWOOD_STUDIO_WATER
            extinction*=water_absorption;
#endif
            vec3 transmission=exp(-extinction*column);
            body=bottom*transmission+body*(1.-transmission);
            float contact=1.-smoothstep(.025,flowing?.14:.10,bottom_below);
            // Sparse lace at actual contact, never a continuous white border.
            // Blend the same bounded phases as the ripples so confluences do
            // not grow stretched triangular foam streaks over elapsed time.
            float patch_a=noise(transported*.63),patch_b=noise(transported_b*.63);
            if(uSurfaceDetail>=1){patch_a=patch_a*.65+noise(transported*2.9+17.3)*.35;patch_b=patch_b*.65+noise(transported_b*2.9+17.3)*.35;}
            float patches=flowing?mix(patch_b,patch_a,flow_blend):noise(world*.72)*.65+noise(world*3.1)*.35;
            float lace=smoothstep(.64,.80,patches);
            float bank_contact=flowing?1.-smoothstep(.04,.30,max(channel_bank,0.)):contact;
            foam=contact*lace*.22;
            if(flowing) {
                foam=max(foam,bank_contact*lace*.19);
                float riffle=smoothstep(.035,.22,channel_grade)*smoothstep(.69,.84,patches);
                foam+=riffle*.14*smoothstep(.12,.50,bottom_below);
                if(uSurfaceDetail>=2) {
                    // Submerged rock faces and shallow banks interrupt the
                    // opaque depth beneath this exact surface sample. Sparse
                    // moving crests collect there instead of outlining every
                    // rock with a static, equally bright white border.
                    float shallow=1.-smoothstep(.10,.45,bottom_below);
                    float crest_phase=max(channel_bank,0.)*18.-uTime*3.+patches*4.;
                    float crest_filter=1.-smoothstep(.5,2.5,fwidth(crest_phase));
                    float crest=pow(max(sin(crest_phase),0.),7.)*crest_filter;
                    foam+=shallow*crest*lace*.075*smoothstep(.1,.8,flow_speed);
                }
            }
            foam*=1.-smoothstep(70.,260.,distance_to_eye);
            if(uLocalPool&&!flowing)foam*=.08;
            foam*=smoothstep(.008,.065,max(bottom_below,0.));
            foam=min(foam,.28);
        }
    }
    float cloud_shadow=cloud_visibility(world);
#ifdef VALWOOD_STUDIO_WATER
    body*=water_tint;foam=clamp(foam*uStudioWaterParams.w,0.,.28);
#endif
    // Local fresh water retains a restrained broad sky response even when
    // viewed down into its basin. It must read as a level reflective surface.
    float surface_reflection=uLocalPool?max(fresnel,flowing?.035:.065):fresnel;
    vec3 color=mix(body,reflected_scene,surface_reflection);
    vec3 graphEmission=vec3(0);
#ifdef VALWOOD_STUDIO_GRAPH
    float graphNormal=1.,graphMetal=0.;if(studioGraphSurface(world,vec3(world.x,vPos.y,world.y),N,V,uTime,color,roughness,graphMetal,graphNormal,graphEmission)){color=clamp(color,vec3(0),vec3(4));roughness=clamp(roughness,.04,1.);graphEmission=clamp(graphEmission,vec3(0),vec3(8));vec3 candidate=mix(vec3(0,1,0),N,clamp(graphNormal,0.,2.));N=dot(candidate,candidate)>1e-8?normalize(candidate):vec3(0,1,0);}
#endif
    float glint=sun_reflection(N,V,roughness);
    float direct_visibility=dot(uSunColor,vec3(.2126,.7152,.0722))*glint>.001?glint_visibility():1.;
    color+=uSunColor*glint*cloud_shadow*direct_visibility;
    color+=torch_water(vec3(world.x,vPos.y,world.y),N,V,roughness);
    color+=stormFlashLight(vRelative,N,V,vec3(.025,.04,.045),0.,roughness,1.);
    vec3 foam_light=sky_light*.75+uSunColor*.28*cloud_shadow;
    color=mix(color,foam_light,foam);
    float fog = max(1.0 - exp(-distance_to_eye * uHaze),
                    smoothstep(10500.0, 14900.0, distance_to_eye));
#ifdef VALWOOD_HDR_ENVIRONMENT
    color=min(color,vec3(60000));
#endif
    color+=graphEmission;
    color=mix(shore_bottom,mix(color,uFog,clamp(fog,0.,1.)),shore_coverage);
    FragColor = vec4(color, .28-rain*.06);
}

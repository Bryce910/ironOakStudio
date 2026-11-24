#version 330 core

in vec3 vRelative, vPos;
#ifdef VALWOOD_STUDIO_WATER
uniform vec3 uStudioWaterTint,uStudioWaterAbsorption;
uniform vec4 uStudioWaterParams; // roughness, waves, flow, foam
#endif
layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 IndirectColor;
uniform vec3 uSun, uSunColor, uAmbient, uFog, uSkyLight, uGroundLight, uSkyZenith;
uniform vec3 uForward,uRight,uUp;
uniform float uEyeHeight,uNear,uFar,uCloud;
uniform vec2 uResolution,uJitterNDC;
uniform sampler2D uOpaqueColor,uOpaqueDepth;
uniform bool uHasOpaque;
uniform sampler2D uShadow;
uniform mat4 uLightVP;
uniform float uTime, uHaze,uWaterLevel,uRainIntensity;
uniform vec2 uOrigin, uWorldSize,uWeatherWind;
uniform bool uLocalPool,uCastleCreek,uRiverSurface;
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
    return uForward+q.x*(uResolution.x/uResolution.y)*.5994296*uRight+q.y*.5994296*uUp;
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
    if(uCastleCreek) {
        vec2 p=world-uCastleCreekOrigin;
        float nearest=2.;
        for(int i=1;i<uCastleCreekPointCount;i++) {
            vec3 a=uCastleCreekPoints[i-1],b=uCastleCreekPoints[i];
            vec2 delta=b.xy-a.xy;
            float t=clamp(dot(p-a.xy,delta)/max(dot(delta,delta),.000001),0.,1.);
            vec2 offset=p-a.xy-delta*t;
            float width=mix(a.z,b.z,t);
            nearest=min(nearest,dot(offset,offset)/(width*width));
        }
        if(nearest>=1.)discard;
    }
    if(uLocalPool&&!uCastleCreek&&!uRiverSurface) {
        if(any(lessThanEqual(uPoolBounds.zw,vec2(0))))discard;
        vec2 local=(world-uPoolBounds.xy)*uPoolBounds.zw;
        if(dot(local,local)>1.)discard;
    }
    vec2 weatherWind=studioWind(vec3(world.x,vPos.y,world.y),uWeatherWind);
    float distance_to_eye = length(vRelative);
    float rain=clamp(uRainIntensity,0.,1.),wind=min(length(weatherWind),3.);
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
    // Grazing-distance phase frequencies can exceed a pixel even for gentle
    // waves. Filter each component before it becomes repeating scanlines.
    vec3 wave_filter = 1.0 - smoothstep(vec3(.35), vec3(2.2),
                                      vec3(fwidth(a), fwidth(b), fwidth(c)));
    vec2 slope = direction_a * cos(a) * .034 * wave_filter.x +
                 direction_b * cos(b) * .023 * wave_filter.y +
                 direction_c * sin(c) * .009 * wave_filter.z;
    if(wind>.00001)slope*=1.+wind*.22;
    // A sheltered forest pool has a shorter wind fetch than open water.
    // Keep ripples and canopy reflections legible without giving the small
    // pond ocean-sized distortion. Rain's independent rings remain intact.
    if(uLocalPool&&!uCastleCreek)slope*=.62;
    if(rain>0.)slope+=rain_slope(world,rain);
    float footprint = max(length(dFdx(world)), length(dFdy(world)));
#ifdef VALWOOD_STUDIO_WATER
    slope*=uStudioWaterParams.y;
#endif
    vec3 N = normalize(vec3(-slope.x, 1, -slope.y));
    vec3 V = normalize(-vRelative);
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
    if (uHasReflection) {
        vec4 projected = uReflectionVP * vec4(vRelative + uReflectionShift, 1);
        if (projected.w > .001) {
            vec2 uv = projected.xy / projected.w * .5 + .5;
            float edge = min(min(uv.x, uv.y), min(1.0 - uv.x, 1.0 - uv.y));
            float coverage = smoothstep(.002, .028, edge);
            if (coverage > 0.0) {
                // Project the world-space wave orientation into the same
                // mirrored camera used to render the reflection texture.
                vec2 distortion = (uReflectionVP * vec4(N.x, 0, N.z, 0)).xy * .085;
                distortion = clamp(distortion, vec2(-.009), vec2(.009));
                vec2 texel = 1.0 / vec2(textureSize(uReflection, 0));
                vec2 sample_uv = clamp(uv + distortion, texel, 1.0 - texel);
                vec2 blur=texel*clamp(.8+footprint*.055,.8,5.);
                vec3 filtered=texture(uReflection,sample_uv).rgb*.5;
                if(wind>.00001||rain>0.) {
                    blur=texel*clamp(.8+footprint*.055+rain*.85+wind*.30,.8,5.);
                    filtered*=.8;
                    filtered+=texture(uReflection,clamp(sample_uv+vec2(blur.x,0),texel,1.-texel)).rgb*.15;
                    filtered+=texture(uReflection,clamp(sample_uv-vec2(blur.x,0),texel,1.-texel)).rgb*.15;
                    filtered+=texture(uReflection,clamp(sample_uv+vec2(0,blur.y),texel,1.-texel)).rgb*.15;
                    filtered+=texture(uReflection,clamp(sample_uv-vec2(0,blur.y),texel,1.-texel)).rgb*.15;
                } else {
                    filtered+=texture(uReflection,clamp(sample_uv+blur,texel,1.-texel)).rgb*.25;
                    filtered+=texture(uReflection,clamp(sample_uv-blur,texel,1.-texel)).rgb*.25;
                }
                reflected_scene = mix(sky,max(filtered,vec3(0)),coverage);
            }
        }
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
    float foam=0.;
    if(uHasOpaque) {
        vec2 texel=1./uResolution;
        vec2 uv=clamp(gl_FragCoord.xy/uResolution,texel*.5,1.-texel*.5);
        float water_z=linear_depth(gl_FragCoord.z);
        float raw=texture(uOpaqueDepth,uv).r;
        float scene_z=linear_depth(raw);
        vec3 ray=view_ray(uv);
        float bottom_y=uEyeHeight+ray.y*scene_z;
        float bottom_below=uWaterLevel-bottom_y;
        if(uLocalPool&&raw<1.&&scene_z<water_z-.001)discard;
        // Use geometric depth at the undistorted pixel for stable shore contact.
        // Refraction can only select geometry actually below the water plane.
        if(raw<1.&&scene_z>=water_z&&bottom_y<uWaterLevel+.12) {
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
                safe_bend=safe_bend&&bent_raw<1.&&bent_z>=water_z&&bent_y<uWaterLevel+surface_slack;
            }
            if(safe_bend)uv=bent_uv;
            vec3 bottom=max(texture(uOpaqueColor,uv).rgb,vec3(0));
            vec3 extinction=uLocalPool?(uCastleCreek?vec3(.17,.14,.20):vec3(.15,.19,.27)):
                vec3(.28,.115,.070);
#ifdef VALWOOD_STUDIO_WATER
            extinction*=uStudioWaterAbsorption;
#endif
            vec3 transmission=exp(-extinction*column);
            body=bottom*transmission+body*(1.-transmission);
            float contact=1.-smoothstep(.06,.72,bottom_below);
            float broken=smoothstep(.34,.67,noise(world*.49+warp*.35));
            float wash=.55+.45*sin(dot(world,vec2(.13,.09))-uTime*.87+warp.x*1.4);
            foam=contact*broken*smoothstep(.30,.75,wash)*.55;
            foam*=1.-smoothstep(70.,260.,distance_to_eye);
            if(uLocalPool)foam*=.25;
        }
    }
    float cloud_shadow=cloud_visibility(world);
#ifdef VALWOOD_STUDIO_WATER
    body*=uStudioWaterTint;foam*=uStudioWaterParams.w;
#endif
    vec3 color=mix(body,reflected_scene,fresnel);
    float roughness=clamp(.12+footprint*.007,.12,.26);
    if(wind>.00001||rain>0.)roughness=clamp(roughness+wind*.025+rain*.045,.12,.34);
#ifdef VALWOOD_STUDIO_WATER
    roughness=clamp(roughness*uStudioWaterParams.x,.06,.85);
#endif
    vec3 graphEmission=vec3(0);
#ifdef VALWOOD_STUDIO_GRAPH
    float graphNormal=1.,graphMetal=0.;if(studioGraphSurface(world,vec3(world.x,vPos.y,world.y),N,V,uTime,color,roughness,graphMetal,graphNormal,graphEmission)){color=clamp(color,vec3(0),vec3(4));roughness=clamp(roughness,.04,1.);graphEmission=clamp(graphEmission,vec3(0),vec3(8));vec3 candidate=mix(vec3(0,1,0),N,clamp(graphNormal,0.,2.));N=dot(candidate,candidate)>1e-8?normalize(candidate):vec3(0,1,0);}
#endif
    float glint=sun_reflection(N,V,roughness);
    float direct_visibility=dot(uSunColor,vec3(.2126,.7152,.0722))*glint>.001?glint_visibility():1.;
    color+=uSunColor*glint*cloud_shadow*direct_visibility;
    color+=torch_water(vec3(world.x,uWaterLevel,world.y),N,V,roughness);
    vec3 foam_light=sky_light*.75+uSunColor*.28*cloud_shadow;
    color=mix(color,foam_light,foam);
    float fog = max(1.0 - exp(-distance_to_eye * uHaze),
                    smoothstep(10500.0, 14900.0, distance_to_eye));
#ifdef VALWOOD_HDR_ENVIRONMENT
    color=min(color,vec3(60000));
#endif
    color+=graphEmission;
    FragColor = vec4(mix(color, uFog, clamp(fog, 0.0, 1.0)), .28-rain*.06);
}

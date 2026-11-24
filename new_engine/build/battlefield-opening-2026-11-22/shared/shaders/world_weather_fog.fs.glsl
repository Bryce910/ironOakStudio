#version 330 core
#pragma include "world_authored_weather.glsl"
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uDepth,uShadow,uMacroHeight,uTerrain;
uniform vec2 uResolution,uNearFar,uJitterNDC,uWorldSize,uWind;
uniform vec3 uRight,uUp,uForward,uCamera,uSun,uSunColor,uFogColor,uSkyLight,uGroundLight;
uniform mat4 uLightVP;
uniform vec4 uLocalBounds;
uniform float uTan,uTime,uFogStrength,uRain;
uniform float uShaftStrength=1.;
uniform int uFogSteps=8;
uniform bool uHasTerrain,uHasMacro,uHasShadow,uFogActive;
float linear_depth(float d) {
    return uNearFar.x*uNearFar.y/max(uNearFar.y-d*(uNearFar.y-uNearFar.x),.00001);
}
float hash(vec2 p) {return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p) {
    vec2 cell=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(hash(cell),hash(cell+vec2(1,0)),f.x),
               mix(hash(cell+vec2(0,1)),hash(cell+1.0),f.x),f.y);
}
float ground_height(vec2 world) {
    vec2 local=(world-uLocalBounds.xy)*uLocalBounds.zw;
    if(uHasTerrain&&all(greaterThanEqual(local,vec2(0)))&&all(lessThanEqual(local,vec2(1))))
        return max(0.0,texture(uTerrain,(local*64.0+.5)/65.0).r);
    if(!uHasMacro)return 0.0;
    vec2 size=vec2(textureSize(uMacroHeight,0));
    vec2 uv=clamp(world/uWorldSize+.5,0.0,1.0);
    return max(0.0,texture(uMacroHeight,(uv*(size-1.0)+.5)/size).r);
}
float sun_visibility(vec3 relative) {
    if(!uHasShadow)return 1.0;
    vec4 p=uLightVP*vec4(relative,1);
    if(p.w<=0.0)return 1.0;
    vec3 uv=p.xyz/p.w*.5+.5;
    if(any(lessThan(uv,vec3(0)))||any(greaterThan(uv,vec3(1))))return 1.0;
    return uv.z-.0015<=texture(uShadow,uv.xy).r?1.0:.12;
}
void main() {
    float depth=texture(uDepth,vUV).r;
    if(depth>=.999999||!uFogActive){FragColor=vec4(0,0,0,1);return;}
    vec2 ndc=vUV*2.0-1.0-uJitterNDC;
    vec3 unnormalized=uForward+uRight*(ndc.x*uResolution.x/uResolution.y*uTan)+uUp*(ndc.y*uTan);
    float ray_length=length(unnormalized);
    vec3 ray=unnormalized/max(ray_length,.0001);
    float distance=min(linear_depth(depth)*ray_length,1200.0);
    int steps=clamp(uFogSteps,4,12);
    float step_length=distance/float(steps);
    float offset=.35+.30*hash(gl_FragCoord.xy);
    vec3 scattered=vec3(0);
    float transmittance=1.0;
    float sun_length=max(length(uSun),.001);
    float forward_scatter=pow(max(dot(ray,uSun/sun_length),0.0),8.0);
    vec3 ambient=uFogColor*.48+mix(uGroundLight,uSkyLight,.8)*.85;
    for(int i=0;i<12;i++) {
        if(i>=steps)break;
        vec3 relative=ray*((float(i)+offset)*step_length);
        vec3 world=uCamera+relative;
        float clearance=world.y-ground_height(world.xz);
        float layer=exp(-max(clearance,0.0)/16.0)*smoothstep(-1.5,.6,clearance);
        float variation=.74+.26*noise(world.xz*.024-uWind*uTime*.010);
        // A local ground layer supplements the engine's distant aerial haze.
        // No opaque gray slab is placed over clear sky pixels.
        vec3 weather=authoredWeather(world.xz,vec3(uRain,0.0,uFogStrength));
        float density=weather.z*(.00120*layer*variation+.000012)*(1.0+weather.x*.12);
        float segment=1.0-exp(-density*step_length);
        float direct=sun_visibility(relative);
        // A shadowed ground layer carries the same canopy occlusion as the
        // scene. Additional forward scattering reveals openings in the
        // canopy; it never brightens the fully shadowed ambient contribution.
        float shaft=clamp(uShaftStrength,0.,2.)*.22*forward_scatter;
        vec3 source=ambient+uSunColor*(.075+.28*forward_scatter+shaft)*direct;
        scattered+=transmittance*segment*source;
        transmittance*=1.0-segment;
    }
    FragColor=vec4(max(scattered,vec3(0)),clamp(transmittance,0.0,1.0));
}

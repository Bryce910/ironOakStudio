#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uColor;
uniform vec2 uSourceSize;
uniform float uExposure;

vec3 finiteRadiance(vec3 v){
    return vec3(isnan(v.r)||isinf(v.r)?0.:clamp(v.r,0.,64.),
                isnan(v.g)||isinf(v.g)?0.:clamp(v.g,0.,64.),
                isnan(v.b)||isinf(v.b)?0.:clamp(v.b,0.,64.));
}
vec3 highlight(vec2 offset){
    vec2 uv=clamp(vUV+offset/uSourceSize,vec2(.5)/uSourceSize,1.-vec2(.5)/uSourceSize);
    vec3 exposed=finiteRadiance(finiteRadiance(texture(uColor,uv).rgb)*uExposure);
    float brightness=max(exposed.r,max(exposed.g,exposed.b));
    const float threshold=1.25,knee=.5;
    float soft=clamp(brightness-threshold+knee,0.,2.*knee);
    soft=soft*soft/(4.*knee);
    float contribution=max(brightness-threshold,soft)/max(brightness,.00001);
    return exposed*(contribution/uExposure);
}
void main(){
    // Normalized 13-tap downsample. Threshold each source sample so small HDR
    // lamps remain represented instead of falling below threshold after averaging.
    vec3 color=highlight(vec2(0))*.125;
    color+=(highlight(vec2(-2,2))+highlight(vec2(2,2))+
            highlight(vec2(-2,-2))+highlight(vec2(2,-2)))*.03125;
    color+=(highlight(vec2(0,2))+highlight(vec2(-2,0))+
            highlight(vec2(2,0))+highlight(vec2(0,-2)))*.0625;
    color+=(highlight(vec2(-1,1))+highlight(vec2(1,1))+
            highlight(vec2(-1,-1))+highlight(vec2(1,-1)))*.125;
    FragColor=vec4(color,0.);
}

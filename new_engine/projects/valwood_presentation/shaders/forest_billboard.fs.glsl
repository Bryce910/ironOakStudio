#version 330 core
in vec2 vUV;
in float vFade;
in float vSnowTint;
flat in float vSnowTex;
uniform sampler2D uTex;
uniform sampler2D uTexSnow;
uniform bool uHasTex;
uniform bool uHasTexSnow;
uniform vec3 uSunColor,uAmbient;
out vec4 FragColor;
void main(){
    vec4 c;
    if(uHasTex){
        if(uHasTexSnow&&vSnowTex>0.5) c=texture(uTexSnow,vUV);
        else c=texture(uTex,vUV);
        if(c.a<0.15)discard;
        c.rgb=mix(c.rgb,vec3(0.88,0.93,1.0),vSnowTint*0.72);
    } else {
        vec2 uv=vUV*2.0-1.0;
        float trunk=clamp(1.0-abs(uv.x)*8.0,0.0,1.0)*clamp(uv.y+1.0,0.0,1.0)*clamp(-uv.y+0.1,0.0,1.0);
        float crown=clamp(1.0-length(uv+vec2(0,0.2))*1.4,0.0,1.0);
        if(trunk+crown<0.05)discard;
        vec3 base_c=mix(vec3(0.22,0.14,0.06),vec3(0.12,0.38,0.08),crown);
        base_c=mix(base_c,vec3(0.88,0.93,1.0),vSnowTint*crown);
        c=vec4(base_c,1.0);
    }
    vec3 lit=c.rgb*(uAmbient+uSunColor*0.6);
    FragColor=vec4(lit,c.a*vFade);
}

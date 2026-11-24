// Shared world-space field: blade reflectance and terrain cover use the same
// low-frequency pattern, including when geometry fades into the distance.
float grassRandom(ivec2 p,uint salt){
    uint h=uint(p.x)*0x1f123bb5u ^ uint(p.y)*0x5f356495u ^ salt*0x6c8e9cf5u;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;
    return float(h&0xffffffu)/16777216.0;
}
float grassNoise(vec2 p){
    ivec2 i=ivec2(floor(p));vec2 f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(grassRandom(i,977u),grassRandom(i+ivec2(1,0),977u),f.x),
               mix(grassRandom(i+ivec2(0,1),977u),grassRandom(i+ivec2(1,1),977u),f.x),f.y);
}
float grassCover(vec2 p){return clamp((grassNoise(p*.19)*.67+grassNoise(p*.87+vec2(8.3,-6.1))*.33-.18)/.65,0.0,1.0);}
vec3 grassReflectance(vec2 p,float tip,float variation,float style){
    float patch=grassNoise(p*.105+vec2(3.7,1.4));
    vec3 root=vec3(.16,.24,.055),leaf=vec3(.25,.36,.085);
    // Straw and olive leaves occur in patches, with restrained variation per blade.
    float dry=smoothstep(.60,.84,patch)*.60;
    if(style>=1.5&&style<3.5)dry=max(dry,.85);
    root=mix(root,vec3(.19,.14,.055),dry);
    leaf=mix(leaf,vec3(.29,.23,.085),dry);
    if(style>=3.5){root=vec3(.012,.034,.033);leaf=vec3(.045,.095,.082);}
    return mix(root,leaf,smoothstep(.02,.82,tip))*(.87+patch*.20+variation*.09);
}
vec3 grassTint(float t){
    if(t<.5)return vec3(1);if(t<1.5)return vec3(.86,1.03,.82);if(t<2.5)return vec3(1.15,1.02,.78);
    if(t<3.5)return vec3(.84,.96,1.02);if(t<4.5)return vec3(.75,.65,.52);return vec3(.68,.83,.66);
}

#version 330 core
in vec3 vDir;
uniform sampler2D uSkyTexDay,uSkyTexNight;
uniform bool uHasDayTex,uHasNightTex;
uniform float uDayBlend,uSunElev,uOvercast;
uniform vec3 uSunDir;
out vec4 F;
float hash21(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash21(i),hash21(i+vec2(1,0)),f.x),mix(hash21(i+vec2(0,1)),hash21(i+1.0),f.x),f.y);}
float fbm(vec2 p){float v=0.0,a=.5;for(int i=0;i<5;i++){v+=a*noise(p);p=mat2(.8,-.6,.6,.8)*p*2.03+vec2(19.2,7.1);a*=.5;}return v;}
vec3 daylight(vec3 d){
    float h=max(d.y,0.0),sun=pow(max(dot(d,uSunDir),0.0),12.0);
    vec3 horizon=mix(vec3(.50,.62,.73),vec3(.86,.68,.43),sun*.45);
    vec3 sky=mix(horizon,vec3(.09,.23,.43),pow(h,.48));
    vec2 cloudUV=d.xz/(h+.24)*1.8+vec2(4.2,8.1);
    float field=fbm(cloudUV),fine=fbm(cloudUV*2.4);
    float clouds=smoothstep(.48,.70,field+fine*.12)*smoothstep(.01,.20,h);
    vec3 cloud=mix(vec3(.39,.46,.52),vec3(.98,.91,.78),smoothstep(.45,.68,field));
    sky=mix(sky,cloud,clouds*.82);
    float disk=smoothstep(.9996,.9999,dot(d,uSunDir));
    sky+=vec3(1.0,.78,.47)*(sun*.22+disk*3.0);
    return mix(sky,vec3(.48,.53,.57),uOvercast*.72);
}
void main(){
    vec3 d=normalize(vDir);
    vec2 uv=vec2(.5+atan(d.z,d.x)/6.2831853,.5-asin(clamp(d.y,-1.0,1.0))/3.14159265);
    vec3 day=uHasDayTex?pow(texture(uSkyTexDay,uv).rgb,vec3(2.2)):daylight(d);
    vec3 night=uHasNightTex?pow(texture(uSkyTexNight,uv).rgb,vec3(2.2))*.16:vec3(.005,.009,.025);
    F=vec4(mix(night,day,uDayBlend),1.0);
}

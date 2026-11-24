#version 330 core
// Historical visual reference, frozen before the 2026-10-23 optimization pass.
// Test data only: do not use for runtime rendering or update to mirror production.
in vec3 vNormal,vPos,vColor,vTangent,vBitangent;
uniform float uTime,uSnowAccum,uWetness,uFogDensity,uHeightFogDensity,uHeightFogBase;
uniform vec3 uCamPos,uFogColor,uCampfirePos;
uniform vec2 uWorldMin,uWorldSize;
uniform sampler2D uTexSand,uTexGrass,uTexDirt,uTexRock,uTexSnow,uGrassCoverage;
uniform sampler2D uWearMask,uFarmMask;
uniform sampler2D uNrmSand,uNrmGrass,uNrmDirt,uNrmRock,uNrmSnow;
uniform sampler2D uRghSand,uRghGrass,uRghDirt,uRghRock,uRghSnow;
uniform bool uHasNrm,uHasRgh;
uniform vec3 uSunDir,uSunColor,uAmbient;
uniform float uSunElev;
uniform bool uToonShading;
uniform float uToonBands;
uniform int uNumLights;
uniform vec3 uLightPos[32],uLightColor[32];
uniform float uLightRadius[32];
uniform sampler2D uShadowMap;
uniform sampler2D uShadowMapWide;
uniform mat4 uLightVP;
uniform mat4 uLightVPWide;

uniform vec3 uLightDir[32];
uniform vec4 uLightParams[32];

// All color inputs and output are linear; presentation performs the only tone map.
const float PI=3.14159265359;
vec3 pbrBRDF(vec3 base,float metal,float rough,vec3 N,vec3 V,vec3 L){
    vec3 H=normalize(V+L);float nl=max(dot(N,L),0.0),nv=max(dot(N,V),0.001),nh=max(dot(N,H),0.0);
    float a=rough*rough,a2=a*a,d=nh*nh*(a2-1.0)+1.0;
    float D=a2/max(PI*d*d,0.0001),k=(rough+1.0)*(rough+1.0)/8.0;
    float G=nl/(nl*(1.0-k)+k)*nv/(nv*(1.0-k)+k);
    vec3 F0=mix(vec3(.04),base,metal),F=F0+(1.0-F0)*pow(1.0-max(dot(H,V),0.0),5.0);
    return ((1.0-F)*(1.0-metal)*base/PI+D*G*F/max(4.0*nl*nv,.001))*nl;
}
float pbrShadowMap(sampler2D map,vec3 p,float bias){
    vec2 texel=1.0/vec2(textureSize(map,0));float s=0.0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)
        s+=step(p.z-bias,texture(map,p.xy+vec2(x,y)*texel*1.35).r);
    return s/9.0;
}
float pbrVisibility(vec3 pos,vec3 normal){
    vec4 q=uLightVP*vec4(pos+normal*.035,1);vec3 p=q.xyz/q.w*.5+.5;
    float edge=min(min(p.x,p.y),min(1.0-p.x,1.0-p.y));
    bool valid=p.z>0.0&&p.z<1.0&&edge>.005;
    float near=valid?pbrShadowMap(uShadowMap,p,.00014):1.0;
    // Most visible fragments are entirely within the near cascade.
    if(valid&&edge>=.075)return near;
    vec4 qw=uLightVPWide*vec4(pos+normal*.09,1);vec3 pw=qw.xyz/qw.w*.5+.5;
    float wide=1.0;
    if(pw.z>0.0&&pw.z<1.0&&all(greaterThan(pw.xy,vec2(.005)))&&all(lessThan(pw.xy,vec2(.995))))wide=pbrShadowMap(uShadowMapWide,pw,.00025);
    return valid?mix(wide,near,smoothstep(.01,.075,edge)):wide;
}
vec3 pbrLighting(vec3 base,float metal,float rough,float ao,vec3 pos,vec3 N,vec3 V,float foliage){
    vec3 L=normalize(-uSunDir);float vis=pbrVisibility(pos,N);
    vec3 sky=mix(uAmbient*vec3(.55,.48,.37),uAmbient*1.65,clamp(N.y*.5+.5,0.0,1.0));
    vec3 col=base*sky*ao*(1.0-metal);
    // Broad sky response, an approximation until reflection probes are baked.
    float fresnel=pow(1.0-max(dot(N,V),0.0),5.0);
    col+=mix(vec3(.04),base,metal)*uAmbient*(.35+.65*fresnel)*(1.0-rough*.6)*ao;
    col+=pbrBRDF(base,metal,rough,N,V,L)*uSunColor*PI*vis;
    // Thin leaf/grass transmission lets low sunlight pass through the canopy.
    col+=base*uSunColor*foliage*(.10+.30*pow(max(dot(V,-L),0.0),3.0))*vis;
    for(int i=0;i<uNumLights;i++){
        vec3 delta=uLightPos[i]-pos;float dist=length(delta);
        if(dist>=uLightRadius[i])continue;
        float att=pow(1.0-dist/max(uLightRadius[i],.001),2.0);
        vec3 direction=delta/max(dist,.001);
        if(uLightParams[i].x>.5)att*=smoothstep(uLightParams[i].z,uLightParams[i].y,dot(normalize(uLightDir[i]),-direction));
        if(att<=0.0)continue;
        col+=pbrBRDF(base,metal,rough,N,V,direction)*uLightColor[i]*PI*att;
    }
    if(uToonShading){float diff=max(dot(N,L),0.0)*vis;diff=ceil(diff*max(uToonBands,1.0))/max(uToonBands,1.0);col=base*(sky+uSunColor*diff);}
    return col;
}

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

out vec4 FragColor;
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);vec2 k=vec2(127.1,311.7);return mix(mix(fract(sin(dot(i,k))*43758.5453),fract(sin(dot(i+vec2(1,0),k))*43758.5453),f.x),mix(fract(sin(dot(i+vec2(0,1),k))*43758.5453),fract(sin(dot(i+1.0,k))*43758.5453),f.x),f.y);}
// The same coordinates and blend are used for all three material channels.
void layer(sampler2D albedo,sampler2D normalMap,sampler2D arm,float scale,float w,inout vec3 base,inout vec3 normal,inout vec2 surface){
    if(w<.005)return;
    vec2 uv=vPos.xz*scale;float blend=smoothstep(.15,.85,noise(vPos.xz*.075));
    vec2 uv2=uv+vec2(.371,.643);
    vec3 a=mix(texture(albedo,uv).rgb,texture(albedo,uv2).rgb,blend);
    vec3 n=vec3(0,0,1);if(uHasNrm)n=normalize(mix(texture(normalMap,uv).xyz,texture(normalMap,uv2).xyz,blend)*2.0-1.0);
    vec2 rg=vec2(1,.9);if(uHasRgh)rg=mix(texture(arm,uv).rg,texture(arm,uv2).rg,blend);
    base+=a*w;normal+=n*w;surface+=rg*w;
}
void rockLayer(vec3 geo,float w,inout vec3 base,inout vec2 surface,out vec3 worldNormal){
    worldNormal=geo;if(w<.005)return;
    vec3 weights=pow(abs(geo),vec3(4));weights/=max(dot(weights,vec3(1)),.001);
    vec2 x=vPos.zy/1.8,y=vPos.xz/1.8,z=vPos.xy/1.8;
    base+=(texture(uTexRock,x).rgb*weights.x+texture(uTexRock,y).rgb*weights.y+texture(uTexRock,z).rgb*weights.z)*w;
    vec2 arm=vec2(1,.9);
    if(uHasRgh)arm=texture(uRghRock,x).rg*weights.x+texture(uRghRock,y).rg*weights.y+texture(uRghRock,z).rg*weights.z;
    surface+=arm*w;
    if(uHasNrm){
        vec3 nx=texture(uNrmRock,x).xyz*2.0-1.0,ny=texture(uNrmRock,y).xyz*2.0-1.0,nz=texture(uNrmRock,z).xyz*2.0-1.0;
        worldNormal=normalize(vec3(nx.z*sign(geo.x),nx.y,nx.x)*weights.x+vec3(ny.x,ny.z,ny.y)*weights.y+vec3(nz.x,nz.y,nz.z*sign(geo.z))*weights.z);
    }
}
void main(){
    vec3 geo=normalize(vNormal);float slope=1.0-max(geo.y,0.0);
    float macro=noise(vPos.xz*.025),patch=noise(vPos.xz*.13);
    vec2 uv=clamp((vPos.xz-uWorldMin)/uWorldSize,vec2(0),vec2(1));
    vec2 paint=texture(uWearMask,uv).rg;float wear=smoothstep(.10,.85,paint.r);
    float style=floor(paint.g*255.0/48.0+.5),farm=smoothstep(.18,.72,texture(uFarmMask,uv).r);
    float rock=smoothstep(.20,.52,slope);rock=max(rock,smoothstep(19.0,47.0,vPos.y)*(.42+.40*macro));
    float sand=(1.0-smoothstep(.3,2.6,vPos.y))*(1.0-rock);
    float earth=(.08+.38*smoothstep(.42,.76,macro))*(.7+.3*patch);
    earth=max(earth,wear*.96);earth=max(earth,farm*.95);
    float camp=1.0-smoothstep(2.2,6.5,length(vPos.xz-uCampfirePos.xz));earth=max(earth,camp);
    float snow=smoothstep(mix(52.0,13.0,uSnowAccum),mix(75.0,34.0,uSnowAccum),vPos.y)*(1.0-smoothstep(.25,.7,slope));
    float wg=(1.0-earth)*(1.0-rock)*(1.0-sand)*(1.0-snow),wd=earth*(1.0-rock)*(1.0-sand)*(1.0-snow),wr=rock*(1.0-snow),ws=sand*(1.0-snow);
    vec3 base=vec3(0),nmap=vec3(0);vec2 surf=vec2(0);
    layer(uTexGrass,uNrmGrass,uRghGrass,1.0/2.0,wg,base,nmap,surf);
    layer(uTexDirt,uNrmDirt,uRghDirt,1.0/3.2,wd,base,nmap,surf);
    vec3 rockNormal;rockLayer(geo,wr,base,surf,rockNormal);
    nmap+=vec3(0,0,1)*wr;
    layer(uTexSand,uNrmSand,uRghSand,.31,ws,base,nmap,surf);
    layer(uTexSnow,uNrmSnow,uRghSnow,.15,snow,base,nmap,surf);
    // Match ground color to the same world-space ecology as the living blades.
    // Texel-center mapping exactly matches the CPU's bilinear authoring mask.
    vec3 coverage=texture(uGrassCoverage,(uv*511.0+.5)/512.0).rgb;
    float cover=smoothstep(.035,.65,coverage.r)*(.75+.25*grassCover(vPos.xz));
    float underlay=cover*(1.0-rock)*(1.0-sand)*(1.0-snow)*(1.0-wear)*(1.0-farm)*(1.0-camp);
    vec3 meadow=grassReflectance(vPos.xz,.58,0.0,floor(coverage.g*255.0+.5))*grassTint(floor(coverage.b*255.0+.5));
    float groundDetail=clamp(dot(base,vec3(.2126,.7152,.0722))/.075,.50,1.65);
    base=mix(base,meadow*.48*groundDetail,underlay*.92);
    base*=mix(vec3(.87,.92,.82),vec3(1.07,1.02,.91),macro);
    if(style>.5&&style<1.5)base=mix(base,base*vec3(.58,.88,.58),wear*.6);
    else if(style>=1.5&&style<2.5)base=mix(base,base*vec3(1.10,.97,.70),wear*.5);
    else if(style>=2.5&&style<3.5)base=mix(base,base*vec3(.58,.78,.45),wear*.6);
    else if(style>=3.5)base=mix(base,base*vec3(.82,.88,.91),wear*.3);
    base=mix(base,base*.24,camp*.70);
    float detail=1.0-smoothstep(80.0,240.0,length(uCamPos-vPos));
    nmap=normalize(mix(vec3(0,0,1),normalize(nmap),detail*.78));
    // Terrain UVs are world X/Z: tangent +X and bitangent +Z, including slopes.
    vec3 T=normalize(vec3(1,0,0)-geo*geo.x),B=normalize(cross(T,geo));
    vec3 N=normalize(mix(mat3(T,B,geo)*nmap,rockNormal,wr));
    float wet=uWetness*(1.0-snow)*(1.0-rock*.5);base*=1.0-wet*.32;
    float rough=clamp(mix(surf.g,.22,wet),.12,1.0),ao=mix(1.0,surf.r,.65);
    vec3 col=pbrLighting(base,0.0,rough,ao,vPos,N,normalize(uCamPos-vPos),0.0);
    float fogDist=length(vPos-uCamPos);
    float lowFog=1.0-exp(-uHeightFogDensity*max(0.0,uHeightFogBase-vPos.y));
    float fog=1.0-exp(-uFogDensity*fogDist*(1.0+lowFog*3.0));
    FragColor=vec4(mix(col,uFogColor,clamp(fog,0.0,.88)),1.0);
}

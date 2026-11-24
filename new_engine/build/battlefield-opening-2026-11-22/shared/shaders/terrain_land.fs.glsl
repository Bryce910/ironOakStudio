#version 330 core
in vec3 vNormal,vPos,vColor,vTangent,vBitangent;
uniform float uTime,uSnowAccum,uWetness,uFogDensity,uHeightFogDensity,uHeightFogBase;
uniform vec3 uCamPos,uFogColor,uCampfirePos;
uniform vec2 uWorldMin,uWorldSize;
uniform sampler2D uTexSand,uTexGrass,uTexDirt,uTexRock,uTexSnow,uGrassCoverage;
uniform sampler2D uWearMask,uFarmMask;
uniform sampler2D uNrmSand,uNrmGrass,uNrmDirt,uNrmRock,uNrmSnow;
uniform sampler2D uRghSand,uRghGrass,uRghDirt,uRghRock,uRghSnow;
uniform bool uHasNrm,uHasRgh;
#pragma include "common/lighting_uniforms.glsl"
#pragma include "common/pbr.glsl"
#pragma include "common/grass_ecology.glsl"
out vec4 FragColor;
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);vec2 k=vec2(127.1,311.7);return mix(mix(fract(sin(dot(i,k))*43758.5453),fract(sin(dot(i+vec2(1,0),k))*43758.5453),f.x),mix(fract(sin(dot(i+vec2(0,1),k))*43758.5453),fract(sin(dot(i+1.0,k))*43758.5453),f.x),f.y);}
// The same coordinates and blend are used for all three material channels.
void layer(sampler2D albedo,sampler2D normalMap,sampler2D arm,float scale,float w,float blend,bool normalDetail,inout vec3 base,inout vec3 normal,inout vec2 surface){
    vec2 uv=vPos.xz*scale;
    // Capture derivatives before either varying branch, so normal-map mip
    // selection remains defined across material and far-detail boundaries.
    vec2 dx=dFdx(uv),dy=dFdy(uv);
    if(w<.005)return;
    vec2 uv2=uv+vec2(.371,.643);
    vec3 a=mix(texture(albedo,uv).rgb,texture(albedo,uv2).rgb,blend);
    vec3 n=vec3(0,0,1);
    if(uHasNrm&&normalDetail)n=normalize(mix(textureGrad(normalMap,uv,dx,dy).xyz,textureGrad(normalMap,uv2,dx,dy).xyz,blend)*2.0-1.0);
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
    float materialBlend=smoothstep(.15,.85,noise(vPos.xz*.075));
    float fogDist=length(vPos-uCamPos);
    float detail=1.0-smoothstep(80.0,240.0,fogDist);
    bool normalDetail=detail>0.0;
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
    layer(uTexGrass,uNrmGrass,uRghGrass,1.0/2.0,wg,materialBlend,normalDetail,base,nmap,surf);
    layer(uTexDirt,uNrmDirt,uRghDirt,1.0/3.2,wd,materialBlend,normalDetail,base,nmap,surf);
    vec3 rockNormal;rockLayer(geo,wr,base,surf,rockNormal);
    nmap+=vec3(0,0,1)*wr;
    layer(uTexSand,uNrmSand,uRghSand,.31,ws,materialBlend,normalDetail,base,nmap,surf);
    layer(uTexSnow,uNrmSnow,uRghSnow,.15,snow,materialBlend,normalDetail,base,nmap,surf);
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
    nmap=normalize(mix(vec3(0,0,1),normalize(nmap),detail*.78));
    // Terrain UVs are world X/Z: tangent +X and bitangent +Z, including slopes.
    vec3 T=normalize(vec3(1,0,0)-geo*geo.x),B=normalize(cross(T,geo));
    vec3 N=normalize(mix(mat3(T,B,geo)*nmap,rockNormal,wr));
    float wet=uWetness*(1.0-snow)*(1.0-rock*.5);base*=1.0-wet*.32;
    float rough=clamp(mix(surf.g,.22,wet),.12,1.0),ao=mix(1.0,surf.r,.65);
    vec3 col=pbrLighting(base,0.0,rough,ao,vPos,N,normalize(uCamPos-vPos),0.0);
    float lowFog=1.0-exp(-uHeightFogDensity*max(0.0,uHeightFogBase-vPos.y));
    float fog=1.0-exp(-uFogDensity*fogDist*(1.0+lowFog*3.0));
    FragColor=vec4(mix(col,uFogColor,clamp(fog,0.0,.88)),1.0);
}

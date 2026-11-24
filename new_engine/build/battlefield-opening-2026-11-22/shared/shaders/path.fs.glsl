#version 330 core
in vec3 vPos;
in vec2 vUV;
in float vMask;
#pragma include "common/lighting_uniforms.glsl"
uniform vec3 uCamPos,uFogColor;
uniform float uFogDensity,uHeightFogDensity,uHeightFogBase;
uniform float uPathWidthTiles;
uniform int uPathStyle;
uniform vec3 uPathTint;
uniform sampler2D uPathTex,uPathNrm,uPathRgh;
uniform bool uHasPathTex,uHasPathNrm,uHasPathRgh;
out vec4 FragColor;
void main(){
    vec2 uv = vec2(vUV.x*uPathWidthTiles,vUV.y);
    float shoulderMask = clamp(vMask,0.0,1.0);
    float edge = abs(vUV.x*2.0-1.0);
    float edgeNoise = fract(sin(dot(floor(vec2(vUV.y*2.7,vUV.x*13.0)),vec2(53.1,217.7)))*43758.5);
    float worldNoiseA = fract(sin(dot(floor(vPos.xz*0.85),vec2(91.7,187.3)))*43758.5);
    float worldNoiseB = fract(sin(dot(floor(vPos.xz*1.65+vec2(7.0,19.0)),vec2(39.1,127.9)))*24631.7);
    float irregular = (worldNoiseA*0.65 + worldNoiseB*0.35)*2.0 - 1.0;
    float edgeWobble = mix(-0.12,0.12,edgeNoise) + irregular*0.10;
    float edgeFeather = (uPathStyle<=1)?0.26:0.12;
    float edgeBlend = smoothstep(0.55+edgeWobble*0.30,1.00+edgeFeather+edgeWobble*0.18,edge);
    float edgeWear = 1.0-smoothstep(0.52,0.98,edge);
    vec3 base;
    vec3 bleed=vec3(0.34,0.29,0.20);
    vec3 n=normalize(vec3(0.0,1.0,0.0));
    float rough=0.88;
    float pathAlpha = 1.0;
    if(uPathStyle==0){
        float grain=fract(sin(dot(floor(uv*3.0),vec2(91.7,227.1)))*43758.5);
        float rut=smoothstep(0.08,0.55,1.0-abs(fract(uv.x*0.35)-0.5)*2.0);
        float fade = smoothstep(0.70+irregular*0.16,1.16+irregular*0.12,edge);
        base=mix(vec3(0.32,0.24,0.15),vec3(0.43,0.31,0.18),grain*0.55+0.25);
        base*=mix(0.86,1.06,rut*0.20);
        base*=uPathTint;
        bleed=mix(vec3(0.25,0.30,0.18),vec3(0.38,0.33,0.22),grain*0.45);
        bleed*=mix(vec3(1.0),uPathTint,0.65);
        rough=0.97;
        pathAlpha=(1.0-fade*0.92)*shoulderMask;
    }else if(uPathStyle==1){
        float grain=fract(sin(dot(floor(uv*4.0),vec2(73.1,181.7)))*43758.5);
        float center=1.0-clamp(abs(fract(uv.x*0.28)-0.5)*2.0,0.0,1.0);
        float wear=pow(center,1.8);
        float fade = smoothstep(0.62+irregular*0.18,1.14+irregular*0.15,edge);
        base=mix(vec3(0.22,0.19,0.14),vec3(0.54,0.44,0.30),wear);
        base*=mix(0.92,1.08,grain*0.18);
        base*=mix(vec3(1.0),uPathTint,0.85);
        bleed=mix(vec3(0.26,0.34,0.19),vec3(0.47,0.39,0.27),wear*0.7+grain*0.15);
        bleed*=mix(vec3(1.0),uPathTint,0.45);
        rough=mix(0.98,0.84,wear);
        pathAlpha=(1.0-fade*0.95)*shoulderMask;
    }else if(uPathStyle==2){
        vec2 buv=uv/1.35;
        float row=floor(buv.y);
        buv.x+=mod(row,2.0)*0.5;
        vec2 brick=fract(buv);
        float mortar=0.08;
        float mask=step(mortar,brick.x)*(1.0-step(1.0-mortar,brick.x))*step(mortar,brick.y)*(1.0-step(1.0-mortar,brick.y));
        float grain=fract(sin(dot(floor(buv),vec2(39.3,211.7)))*43758.5);
        vec3 brickCol=mix(vec3(0.42,0.17,0.11),vec3(0.63,0.28,0.18),grain);
        base=mix(vec3(0.23,0.21,0.18),brickCol,mask);
        bleed=mix(vec3(0.29,0.26,0.19),vec3(0.40,0.25,0.18),grain*0.45);
        rough=mix(0.95,0.72,mask);
    }else if(uHasPathTex){
        base=texture(uPathTex,uv).rgb;
        bleed=mix(vec3(0.28,0.24,0.18),vec3(0.38,0.33,0.24),edgeNoise*0.5);
    }else{
        float row = floor(uv.y);
        uv.x += mod(row, 2.0)*0.5;
        vec2 sid = floor(uv);
        float v=fract(sin(dot(sid,vec2(127.1,311.7)))*43758.5);
        vec2 t=fract(uv);
        float m=0.10;
        float s=step(m,t.x)*(1.0-step(1.0-m,t.x))
               *step(m,t.y)*(1.0-step(1.0-m,t.y));
        vec2 e=smoothstep(0.0,m*1.5,t)*smoothstep(1.0,1.0-m*1.5,t);
        float edg=e.x*e.y;
        vec3 stone=vec3(0.46+v*0.14,0.44+v*0.11,0.41+v*0.10)*mix(0.75,1.0,edg);
        vec3 mortar=vec3(0.24,0.23,0.21);
        base=mix(mortar,stone,s);
        bleed=mix(vec3(0.30,0.27,0.21),vec3(0.41,0.36,0.28),v*0.45);
    }
    base=mix(bleed,base,1.0-edgeBlend*0.82);
    base*=mix(0.92,1.0,edgeWear);
    rough=mix(min(1.0,rough+0.10),rough,1.0-edgeBlend*0.65);
    pathAlpha=clamp(pathAlpha,0.0,1.0);
    if(pathAlpha<0.03) discard;
    if(uHasPathNrm){
        vec3 tn=texture(uPathNrm,vUV).xyz*2.0-1.0;
        n=normalize(vec3(tn.x*0.65*(1.0-edgeBlend*0.5),max(tn.z,0.35),tn.y*0.65*(1.0-edgeBlend*0.5)));
    }
    if(uHasPathRgh){ rough=clamp(texture(uPathRgh,vUV).g,0.04,1.0); }
    vec3 shadow_n=n;
#pragma include "common/lighting_calc.glsl"
    vec3 view=normalize(uCamPos-vPos);
    vec3 refl=reflect(-normalize(uSunDir),n);
    col+=vec3(0.22)*pow(max(dot(refl,view),0.0),mix(40.0,8.0,rough))*clamp(1.0-rough,0.0,1.0)*0.45;
    float fogDist2=length(vPos-uCamPos);
    float distFog=1.0-exp(-uFogDensity*fogDist2);
    float hfog=1.0-exp(-uHeightFogDensity*max(0.0,uHeightFogBase-vPos.y));
    col=mix(col,uFogColor,clamp(distFog*(1.0+hfog*4.0),0.0,0.96));
    FragColor=vec4(col,pathAlpha);
}

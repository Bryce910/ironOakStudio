#version 330 core
in vec3 vNormal,vFragPos,vTangent,vBitangent;
in vec2 vUV;
uniform sampler2D uAlbedo,uNormalTex,uMRTex,uEmissiveTex;
uniform bool uHasTexture,uHasNormalTex,uHasMRTex,uHasEmissiveTex;
uniform bool uTintEnabled;
uniform int uTintRegion;
uniform vec3 uTintColors[5];
uniform bool uDebugAlbedo;
uniform vec4 uBaseColor;
uniform vec3 uEmissiveFactor,uCamPos;
uniform float uMetallic,uRoughness,uAlphaCutoff,uFlash;
#pragma include "common/lighting_uniforms.glsl"
out vec4 FragColor;

float D_GGX(float NdotH,float rough){
    float a=rough*rough;float a2=a*a;
    float d=NdotH*NdotH*(a2-1.0)+1.0;
    return a2/(3.14159265*d*d+1e-7);}
float G_SchlickGGX(float x,float k){return x/(x*(1.0-k)+k);}
float G_Smith(float NdotV,float NdotL,float rough){
    float r=rough+1.0;float k=r*r/8.0;
    return G_SchlickGGX(max(NdotV,0.0001),k)*G_SchlickGGX(max(NdotL,0.0001),k);}
vec3 F_Schlick(float cosTheta,vec3 F0){return F0+(1.0-F0)*pow(clamp(1.0-cosTheta,0.0,1.0),5.0);}

void main(){
    // Geometric normal for shadow bias; shading normal (N) gets normal-mapped below
    vec3 gN=normalize(vNormal);
    vec3 N=gN;
    if(uHasNormalTex){vec3 tn=texture(uNormalTex,vUV).xyz*2.0-1.0;N=normalize(mat3(normalize(vTangent),normalize(vBitangent),gN)*tn);}
    vec4 texSample=uHasTexture?texture(uAlbedo,vUV):vec4(1.0);
    vec4 baseSample=uHasTexture?vec4(texSample.rgb,texSample.a*uBaseColor.a):texSample*uBaseColor;
    if(baseSample.a<uAlphaCutoff)discard;
    if(uDebugAlbedo){FragColor=vec4(baseSample.rgb,baseSample.a);return;}
    vec3 albedo=baseSample.rgb;
    if(uTintEnabled&&uTintRegion>0&&uTintRegion<=4){
        vec3 tint=uTintColors[uTintRegion-1];
        float lum=dot(albedo,vec3(0.299,0.587,0.114));
        albedo=mix(albedo,mix(albedo*(0.55+tint*0.85),tint*(0.35+lum*0.95),0.45),0.75);
    }
    float rough=uRoughness,metal=uMetallic;
    if(uHasMRTex){vec3 mr=texture(uMRTex,vUV).rgb;rough=clamp(rough*mr.g,0.04,1.0);metal=clamp(metal*mr.b,0.0,1.0);}
    vec3 emissive=uEmissiveFactor*(uHasEmissiveTex?texture(uEmissiveTex,vUV).rgb:vec3(1.0));
    vec3 F0=mix(vec3(0.04),albedo,metal);
    vec3 V=normalize(uCamPos-vFragPos);
    float NdotV=max(dot(N,V),0.0001);

    // Shadow — PCSS from cascade shadow maps, using geometric normal for slope bias
    float sunFade=clamp(uSunElev*10.0,0.0,1.0);
    float shadow=0.0;
    float gNdotL=dot(gN,-uSunDir);
    if(gNdotL<=0.0){shadow=sunFade;}
    else if(sunFade>0.0){
        float sb=clamp(1.0-gNdotL,0.0,1.0);
        vec4 lsF=uLightVP*vec4(vFragPos+gN*(1.5+sb*3.5),1.0);
        vec3 pC=lsF.xyz/lsF.w*0.5+0.5;
        vec2 pd[12]=vec2[12](vec2(-0.326,-0.406),vec2(-0.840,-0.074),vec2(-0.696,0.457),vec2(-0.203,0.621),vec2(0.962,-0.195),vec2(0.473,-0.480),vec2(0.519,0.767),vec2(0.185,-0.893),vec2(0.507,0.064),vec2(0.896,0.412),vec2(-0.322,-0.933),vec2(-0.792,-0.598));
        if(pC.z<=1.0&&pC.x>0.02&&pC.x<0.98&&pC.y>0.02&&pC.y<0.98){
            vec2 ts=1.0/vec2(textureSize(uShadowMap,0));float bias=0.0015+sb*0.0045;
            float bD=0.0,bC=0.0;
            for(int i=0;i<5;i++){float d=texture(uShadowMap,pC.xy+pd[i]*ts*5.0).r;if(pC.z-bias>d){bD+=d;bC+=1.0;}}
            float pen=(bC>0.0)?clamp((pC.z-bD/bC)/(bD/bC)*3.5,0.5,9.0):2.5;
            float pcf=0.0;for(int i=0;i<12;i++){float sd=texture(uShadowMap,pC.xy+pd[i]*ts*pen).r;pcf+=(pC.z-bias>sd)?1.0:0.0;}
            shadow=clamp(pcf/12.0,0.0,0.65)*sunFade;
        }else{
            vec4 lsW=uLightVPWide*vec4(vFragPos+gN*(3.0+sb*6.0),1.0);
            vec3 pW=lsW.xyz/lsW.w*0.5+0.5;
            if(pW.z<=1.0&&pW.x>0.0&&pW.x<1.0&&pW.y>0.0&&pW.y<1.0){
                vec2 tw=1.0/vec2(textureSize(uShadowMapWide,0));float pcfw=0.0;float bW=0.003+sb*0.006;
                for(int i=0;i<12;i++){float sd=texture(uShadowMapWide,pW.xy+pd[i]*tw*4.0).r;pcfw+=(pW.z-bW>sd)?1.0:0.0;}
                shadow=clamp(pcfw/12.0,0.0,0.55)*sunFade*0.7;
            }
        }
    }

    // Sun directional light
    vec3 L=normalize(-uSunDir);
    vec3 H=normalize(V+L);
    float NdotL=max(dot(N,L),0.0);
    float NdotH=max(dot(N,H),0.0);
    float HdotV=max(dot(H,V),0.0);
    vec3 col=vec3(0.0);
    if(!uToonShading){
        // PBR path: GGX specular + Lambertian diffuse
        vec3 F=F_Schlick(HdotV,F0);
        vec3 kD=(vec3(1.0)-F)*(1.0-metal);
        float D=D_GGX(NdotH,rough);float G=G_Smith(NdotV,NdotL,rough);
        vec3 spec=D*G*F/(4.0*NdotV*NdotL+0.001);
        col+=(kD*albedo/3.14159265+spec)*uSunColor*NdotL*(1.0-shadow);
        // Hemisphere ambient
        float hemi=N.y*0.5+0.5;
        vec3 skyAmb=mix(uAmbient*0.7,uAmbient*1.3+vec3(0.04,0.05,0.08),hemi);
        vec3 Fa=F_Schlick(NdotV,F0);
        col+=(vec3(1.0)-Fa)*(1.0-metal)*albedo*skyAmb+Fa*skyAmb*(1.0-rough*0.5);
        // Point lights — full GGX per light
        for(int i=0;i<uNumLights;i++){
            vec3 lv=uLightPos[i]-vFragPos;float dist=length(lv);
            float att=1.0-clamp(dist/uLightRadius[i],0.0,1.0);att*=att;
            vec3 Ll=normalize(lv);vec3 Hl=normalize(V+Ll);
            float NdotLl=max(dot(N,Ll),0.0);float NdotHl=max(dot(N,Hl),0.0);float HdotVl=max(dot(Hl,V),0.0);
            vec3 Fl=F_Schlick(HdotVl,F0);
            float Dl=D_GGX(NdotHl,rough);float Gl=G_Smith(NdotV,NdotLl,rough);
            col+=((vec3(1.0)-Fl)*(1.0-metal)*albedo/3.14159265+Dl*Gl*Fl/(4.0*NdotV*NdotLl+0.001))*uLightColor[i]*NdotLl*att;
        }
    }else{
        // Toon path: quantized bands, ink rim, shadow-aware
        float rawDiff=NdotL*(1.0-shadow);
        float toonDiff=(rawDiff>0.0)?ceil(rawDiff*uToonBands)/uToonBands:0.0;
        float toonSpec=step(0.52,pow(NdotH,mix(96.0,8.0,rough)))*(1.0-shadow);
        float rim=pow(clamp(1.0-NdotV,0.0,1.0),1.8);
        vec3 diffC=mix(albedo,vec3(0.0),metal);vec3 specC=mix(vec3(0.04),albedo,metal);
        col=diffC*(uAmbient*0.75+uSunColor*(0.12+toonDiff*0.88))+specC*uSunColor*toonSpec*0.22+albedo*rim*0.08;
        float ink=smoothstep(0.46,0.88,rim)*(1.0-smoothstep(0.0,0.55,rawDiff));
        col=mix(col,vec3(0.05,0.06,0.07),ink*0.82);
        col=mix(vec3(dot(col,vec3(0.299,0.587,0.114))),col,1.22);
        for(int i=0;i<uNumLights;i++){
            vec3 lv=uLightPos[i]-vFragPos;float d=length(lv);
            float att=1.0-clamp(d/uLightRadius[i],0.0,1.0);att*=att;
            vec3 Ll=normalize(lv);
            float ldiff=max(dot(N,Ll),0.0);
            if(ldiff>0.0)ldiff=ceil(ldiff*uToonBands)/uToonBands;
            vec3 Hl=normalize(V+Ll);
            float lspec=step(0.58,pow(max(dot(N,Hl),0.0),mix(96.0,8.0,rough)));
            col+=diffC*uLightColor[i]*ldiff*att+specC*uLightColor[i]*lspec*att*(1.0-rough*0.65);
        }
    }
    col+=emissive;
    col=mix(col,vec3(1.0,0.1,0.1),uFlash);
    FragColor=vec4(col,baseSample.a);
}

#version 330 core
in vec3 vNormal,vPos;in vec2 vUV;
uniform sampler2D uAlbedo,uNormal,uMR,uEmissive;
uniform bool uHasTexture,uHasNormal,uHasMR,uHasEmissive;
uniform vec4 uBaseColor;
uniform vec3 uEmissiveFactor,uCamPos,uFogColor;
uniform float uAlphaCutoff,uMetallic,uRoughness,uFogDensity;
#pragma include "common/lighting_uniforms.glsl"
#pragma include "common/pbr.glsl"
out vec4 FragColor;
void main(){
    vec4 sampleColor=(uHasTexture?texture(uAlbedo,vUV):vec4(1))*uBaseColor;
    if(sampleColor.a<uAlphaCutoff)discard;
    vec3 N=normalize(gl_FrontFacing?vNormal:-vNormal);
    if(uHasNormal){
        vec3 px=dFdx(vPos),py=dFdy(vPos);vec2 tx=dFdx(vUV),ty=dFdy(vUV);
        float det=tx.x*ty.y-tx.y*ty.x;
        if(abs(det)>1e-8){vec3 T=normalize((px*ty.y-py*tx.y)/det);T=normalize(T-N*dot(N,T));vec3 B=normalize(cross(N,T))*sign(det);
        N=normalize(mat3(T,B,N)*(texture(uNormal,vUV).xyz*2.0-1.0));}
    }
    vec3 mr=uHasMR?texture(uMR,vUV).rgb:vec3(1);
    float rough=clamp(uRoughness*mr.g,.08,1.0),metal=clamp(uMetallic*mr.b,0,1);
    vec3 V=normalize(uCamPos-vPos);
    vec3 col=pbrLighting(sampleColor.rgb,metal,rough,1.0,vPos,N,V,uAlphaCutoff>0.0?1.0:0.0);
    col+=uEmissiveFactor*(uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1));
    float fog=1.0-exp(-uFogDensity*length(uCamPos-vPos));
    col=mix(col,uFogColor,clamp(fog,0.0,.88));FragColor=vec4(col,1);
}

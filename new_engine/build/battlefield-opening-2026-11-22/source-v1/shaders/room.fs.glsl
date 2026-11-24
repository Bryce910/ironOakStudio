#version 330 core
in vec3 vColor;
in vec3 vNorm;
in vec3 vPos;
#pragma include "common/lighting_uniforms.glsl"
uniform vec3 uRoomAmbient;
uniform vec3 uRoomCenter;
uniform sampler2D uTex;
uniform bool uHasTex;
uniform float uUvScale;
uniform int uSurfaceKind;
out vec4 F;
void main(){
    vec3 local=vPos-uRoomCenter;
    vec2 uv=local.xz;
    if(uSurfaceKind==0){
        uv=(abs(vNorm.x)>0.5)?vec2(local.z,local.y):vec2(local.x,local.y);
    }else if(uSurfaceKind==2){
        uv=vec2(local.x,-local.z);
    }
    uv*=uUvScale;
    vec3 n=normalize(vNorm);
    float d=max(dot(n,normalize(-uSunDir)),0.0);
    vec3 base=uHasTex?texture(uTex,uv).rgb:vColor;
    vec3 col=base*(uAmbient+uRoomAmbient+uSunColor*d*0.22);
    for(int i=0;i<uNumLights;i++){
        vec3 lv=uLightPos[i]-vPos;float dist=length(lv);
        float att=1.0-clamp(dist/uLightRadius[i],0.0,1.0);att*=att;
        col+=base*uLightColor[i]*max(dot(n,normalize(lv)),0.0)*att;
    }
    col=min(col*1.05,vec3(1.0));
    F=vec4(col,1.0);
}

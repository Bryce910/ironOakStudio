#version 330 core
layout(location=0)in vec2 aXY;
layout(location=1)in vec2 aUV;
layout(location=2)in float aHF;
uniform mat4 uProj,uView,uModel;
uniform float uTime,uWidth,uHeight;
out vec2  vUV;
out float vHF;
out vec3  vWorldPos;
void main(){
    float turb=aHF*aHF;
    float tx=sin(aUV.x*6.28+uTime*2.1)*0.04*turb
            +sin(aUV.x*13.7+uTime*3.4)*0.02*turb;
    float tz=cos(aUV.x*9.1 +uTime*1.8)*0.03*turb;
    vec3 local=vec3((aXY.x+tx)*uWidth, aXY.y*uHeight, tz*uWidth*0.15);
    vec4 world=uModel*vec4(local,1.0);
    gl_Position=uProj*uView*world;
    vUV=aUV; vHF=aHF; vWorldPos=world.xyz;
}

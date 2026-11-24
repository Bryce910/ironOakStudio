#version 330 core
layout(location=0)in vec2 aCorner;
layout(location=1)in vec2 aUV;
layout(location=2)in vec3 iPos;
layout(location=3)in float iSize;
layout(location=4)in float iPhase;
layout(location=5)in float iColorVar;
layout(location=6)in float iType;
uniform mat4 uProj,uView;
uniform float uTime;
uniform vec3 uCamPos;
out vec2 vUV;
out float vType;
out float vColorVar;
out float vFade;
out vec3 vWorld;
void main(){
    vec3 right=vec3(uView[0][0],uView[1][0],uView[2][0]);
    vec3 camUp=vec3(uView[0][1],uView[1][1],uView[2][1]);
    float ph=uTime*0.9+iPos.x*0.28+iPos.z*0.22+iPhase;
    float sw=sin(ph)*0.30+sin(ph*2.3+0.8)*0.15;
    vec3 center=vec3(iPos.x+sw*iSize*0.14, iPos.y+iSize*2.2, iPos.z+sw*iSize*0.10);
    vec3 world=center+right*(aCorner.x*iSize)+camUp*(aCorner.y*iSize);
    gl_Position=uProj*uView*vec4(world,1.0);
    vUV=aUV;
    vType=iType;
    vColorVar=iColorVar;
    vFade=1.0-smoothstep(24.0,42.0,length(iPos-uCamPos));
    vWorld=world;
}

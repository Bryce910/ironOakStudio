#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec2 aUV;
uniform mat4 uProj,uView;
uniform vec3 uBirdPos;
uniform float uScale,uBank;
uniform float uFrame;
out vec2 vUV;
void main(){
    vec3 right=vec3(uView[0][0],uView[1][0],uView[2][0]);
    vec3 up   =vec3(uView[0][1],uView[1][1],uView[2][1]);
    float cb=cos(uBank),sb=sin(uBank);
    vec3 r2=right*cb+up*sb;
    vec3 u2=-right*sb+up*cb;
    vec3 wpos=uBirdPos+r2*aPos.x*uScale+u2*aPos.y*uScale;
    gl_Position=uProj*uView*vec4(wpos,1.0);
    vUV=vec2(aUV.x*0.5+uFrame*0.5,aUV.y);
}

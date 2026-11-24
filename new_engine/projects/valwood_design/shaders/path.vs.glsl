#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec2 aUV;
layout(location=2)in float aMask;
uniform mat4 uProj,uView;
out vec3 vPos;
out vec2 vUV;
out float vMask;
void main(){
    gl_Position=uProj*uView*vec4(aPos,1.0);
    vPos=aPos; vUV=aUV; vMask=aMask;
}

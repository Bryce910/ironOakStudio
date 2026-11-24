#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec2 aUV;
uniform mat4 uProj,uView;
uniform vec3 uPos;
uniform float uScale;
out vec2 vUV;
void main(){
    vec3 right=vec3(uView[0][0],uView[1][0],uView[2][0]);
    vec3 up   =vec3(0,1,0);
    vec3 wpos=uPos+right*aPos.x*uScale+up*aPos.y*uScale;
    gl_Position=uProj*uView*vec4(wpos,1.0);
    vUV=aUV;
}

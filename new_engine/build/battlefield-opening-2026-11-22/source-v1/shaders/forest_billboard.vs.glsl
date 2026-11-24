#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec2 aUV;
layout(location=2)in vec3 iTreePos;
layout(location=3)in float iScale;
layout(location=4)in float iSnowTint;
layout(location=5)in float iSnowTex;
uniform mat4 uProj,uView;
uniform vec3 uCamPos;
out vec2 vUV;
out float vFade;
out float vSnowTint;
flat out float vSnowTex;
void main(){
    vec3 right=normalize(vec3(uView[0][0],uView[1][0],uView[2][0]));
    vec3 up=vec3(0,1,0);
    vec3 wpos=iTreePos + right*aPos.x*iScale + up*aPos.y*iScale;
    gl_Position=uProj*uView*vec4(wpos,1.0);
    vUV=aUV;
    float dist=length(iTreePos-uCamPos);
    vFade=clamp((dist-60.0)/30.0,0.0,1.0);
    vSnowTint=iSnowTint;
    vSnowTex=iSnowTex;
}

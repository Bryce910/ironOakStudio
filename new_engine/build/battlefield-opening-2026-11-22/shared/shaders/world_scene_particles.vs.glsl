#version 330 core
layout(location=0) in vec4 aCenterSize;
layout(location=1) in vec4 aMotion;
layout(location=2) in vec4 aStyle;
layout(location=3) in vec4 aTint;
uniform mat4 uVP;
uniform vec3 uRight,uUp;
out vec2 vUV;
out vec3 vRelative;
flat out vec4 vMotion,vStyle,vTint;
void main() {
    const vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    vec2 q=corners[gl_VertexID];
    float c=cos(aMotion.x),s=sin(aMotion.x);
    vec2 shape=q;
    if(aStyle.x>3.5&&aStyle.x<4.5)shape.x*=.35+.65*abs(sin(aMotion.x*2.+aMotion.y*27.));
    vec2 rotated=mat2(c,s,-s,c)*shape;
    vec3 p=aCenterSize.xyz+(uRight*rotated.x+uUp*rotated.y)*aCenterSize.w;
    vUV=q;vRelative=p;vMotion=aMotion;vStyle=aStyle;vTint=aTint;
    gl_Position=uVP*vec4(p,1);
}

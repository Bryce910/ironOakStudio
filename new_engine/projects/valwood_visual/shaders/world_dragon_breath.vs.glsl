#version 330 core
layout(location=0) in vec4 aCenterWidth;
layout(location=1) in vec4 aAxisLength;
layout(location=2) in vec4 aLife;   // age,opacity,variant,kind
layout(location=3) in vec4 aStyle;  // phase,sprite-frame,softness,radiance
uniform mat4 uVP;
uniform vec3 uRight,uUp,uForward;
out vec2 vUV;
out vec3 vRelative;
flat out vec4 vLife,vStyle;
void main() {
    const vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    vec2 q=corners[gl_VertexID];
    vec3 along=aAxisLength.xyz;
    vec3 side=cross(along,uForward);
    // End-on breath still has a finite cross section, rather than collapsing
    // into a degenerate ribbon. This also avoids look-aligned NaNs.
    float projected=dot(side,side);
    side=projected>.0001?side*inversesqrt(max(projected,.0001)):uRight;
    along=projected>.015?along:uUp;
    if(aLife.w>1.5&&aLife.w<2.5) {
        float c=cos(aStyle.x),s=sin(aStyle.x);
        side=uRight*c+uUp*s;along=uUp*c-uRight*s;
    }
    vec3 p=aCenterWidth.xyz+side*q.x*aCenterWidth.w+along*q.y*aAxisLength.w;
    vUV=q;vRelative=p;vLife=aLife;vStyle=aStyle;
    gl_Position=uVP*vec4(p,1);
}

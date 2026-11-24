#version 330 core
layout(location=0) in vec3 aRiverPosition;
uniform bool uRiverSurface;
uniform mat4 uVP;
uniform vec3 uOffset;
uniform float uSize,uWaterLevel;
uniform bool uLocalPool;
uniform vec2 uOrigin;
uniform vec4 uPoolBounds;
out vec3 vRelative,vPos;
void main() {
    vec2 q[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    vec2 xz=q[gl_VertexID]*uSize;
    if(uLocalPool)xz=uPoolBounds.xy-uOrigin+q[gl_VertexID]/max(uPoolBounds.zw,vec2(.000001));
    vec3 p=uRiverSurface?vec3(aRiverPosition.x-uOrigin.x,aRiverPosition.y,aRiverPosition.z-uOrigin.y):vec3(xz.x,uWaterLevel,xz.y);
    vRelative=p+uOffset;vPos=p;gl_Position=uVP*vec4(vRelative,1);
}

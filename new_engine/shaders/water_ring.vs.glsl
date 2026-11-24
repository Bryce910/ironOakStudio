#version 330 core
layout(location=0)in vec2 aUV;
uniform mat4 uProj,uView;
uniform float uTime,uRingR,uRingStr,uWaterLevel;
uniform vec3 uPlayerPos;
out vec3 vPos;
out float vFoam;
void main(){
    vec2 local=(aUV*2.0-1.0)*22.0;
    vec3 p=vec3(uPlayerPos.x+local.x, uWaterLevel, uPlayerPos.z+local.y);
    p.y+=sin(p.x*0.4+uTime*1.2)*0.08+sin(p.z*0.3+uTime*0.9)*0.06;
    float pd=length(local);
    float depr=exp(-pd*pd*0.18)*0.16;
    p.y-=depr;
    float rdist=abs(pd-uRingR);
    float ring=exp(-rdist*rdist*3.5)*uRingStr;
    p.y+=ring;
    vFoam=clamp(ring*2.0+depr*0.8,0.0,1.0);
    vPos=p;
    gl_Position=uProj*uView*vec4(p,1.0);
}

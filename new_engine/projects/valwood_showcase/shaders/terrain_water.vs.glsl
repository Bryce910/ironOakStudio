#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec3 aColor;
uniform mat4 uProj,uView;
uniform float uTime;
uniform float uWaveAmp,uWaveSpeed;
uniform int   uZoneCount;
uniform vec2  uZonePos[8];
uniform float uZoneRadius[8];
uniform float uZoneWaveAmp[8],uZoneWaveSpeed[8];
out vec3  vPos;
out vec2  vOrigXZ;
out float vBaseY,vEdgeAlpha,vFlowCoord,vRiverMask,vWaveHeight;
void main(){
    float wa=uWaveAmp,ws=uWaveSpeed,best=0.0;
    for(int zi=0;zi<uZoneCount;zi++){
        float d=length(aPos.xz-uZonePos[zi]);
        float r=uZoneRadius[zi];
        if(d>=r) continue;
        float t=clamp(1.0-(d/r-0.9)*10.0,0.0,1.0);
        if(t<=best) continue;
        best=t;
        wa=mix(uWaveAmp,uZoneWaveAmp[zi],t);
        ws=mix(uWaveSpeed,uZoneWaveSpeed[zi],t);
    }
    float px=aPos.x*0.053+uTime*0.82*ws;
    float pz=aPos.z*0.047-uTime*0.71*ws;
    float w0=(sin(px)+sin(2.0*px)*0.15)+(cos(pz)+cos(2.0*pz)*0.15);
    float w1=sin((aPos.x+aPos.z)*0.031+uTime*0.53*ws)*0.65;
    float w2=cos(aPos.x*0.088-uTime*1.10*ws)*0.40;
    float w3=sin(aPos.z*0.072+aPos.x*0.021+uTime*0.65*ws)*0.30;
    float rawDisp=(w0+w1)*wa*0.055+(w2+w3)*wa*0.020;
    float shoreDamp=mix(0.30,1.0,clamp(aColor.r*2.0,0.0,1.0));
    float disp=rawDisp*shoreDamp;
    vec3 p=aPos; p.y+=disp;
    gl_Position=uProj*uView*vec4(p,1.0);
    vPos=p; vOrigXZ=aPos.xz; vBaseY=aPos.y;
    vEdgeAlpha=aColor.r; vFlowCoord=aColor.g; vRiverMask=aColor.b;
    vWaveHeight=disp;
}

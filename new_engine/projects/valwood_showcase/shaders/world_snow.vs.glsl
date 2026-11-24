#version 330 core
layout(location=0)in vec4 iPosition;
layout(location=1)in vec4 iDetails;
uniform mat4 uVP;
uniform vec3 uOffset,uRight,uUp;
uniform float uTime;
uniform int uMode;
out vec2 vUV;
out float vAge;
flat out float vSide;
void main(){
    const vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    vec2 q=corners[gl_VertexID];vUV=q;vAge=uTime-iDetails.x;vSide=iDetails.z;
    vec3 p=iPosition.xyz+uOffset;
    if(uMode==0){
        if(vAge<0.||vAge>180.){gl_Position=vec4(2,2,2,1);return;}
        float c=cos(iPosition.w),s=sin(iPosition.w);
        vec3 sole=vec3(c,0,s)*q.x*.145+vec3(s,0,-c)*q.y*.205;
        sole.y=sole.x*iDetails.y+sole.z*iDetails.w;
        p+=sole;
    }else p+=(uRight*q.x+uUp*q.y)*iDetails.y;
    gl_Position=uVP*vec4(p,1);
}

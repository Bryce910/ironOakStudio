#version 330 core
layout(location=0)in vec3 aPos;layout(location=1)in vec3 aNormal;layout(location=2)in vec2 aUV;layout(location=3)in vec4 aTangent;
layout(location=4)in vec4 aInstance;layout(location=5)in float aYaw;
layout(location=8)in vec3 aPosePosition;layout(location=9)in vec3 aPoseNormal;layout(location=10)in vec4 aPoseTangent;layout(location=11)in float aPoseBlend;
uniform bool uPoseBlendEnabled;uniform mat4 uProjection;
out vec3 P,N,T,B;out vec2 UV;
void main(){float w=uPoseBlendEnabled?aPoseBlend:0.;vec3 p=mix(aPos,aPosePosition,w),n=normalize(mix(aNormal,aPoseNormal,w));vec4 tangent=mix(aTangent,aPoseTangent,w);float c=cos(aYaw),s=sin(aYaw);mat3 R=mat3(c,0,-s,0,1,0,s,0,c);P=R*p*aInstance.w+aInstance.xyz;N=R*n;T=normalize(R*tangent.xyz);B=normalize(cross(N,T))*sign(tangent.w);UV=aUV;gl_Position=uProjection*vec4(P,1);}

#version 330 core
layout(location=0)in vec4 a0;layout(location=1)in vec4 a1;layout(location=2)in vec4 a2;layout(location=3)in vec4 a3;layout(location=4)in vec4 aColor;layout(location=5)in vec4 aStyle;
uniform mat4 uVP;
out vec2 vUV;flat out vec4 vColor;flat out float vStyle;out float vDepth;
void main(){const int index[6]=int[6](0,1,2,0,2,3);const vec2 uv[4]=vec2[4](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,1));vec3 p[4]=vec3[4](a0.xyz,a1.xyz,a2.xyz,a3.xyz);int i=index[gl_VertexID];vec4 clip=uVP*vec4(p[i],1);vUV=uv[i];vColor=aColor;vStyle=aStyle.x;vDepth=clip.w;gl_Position=clip;}

#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec2 aUV;
uniform mat4 uProj,uView,uModel;
out vec3 vNormal,vPos;out vec2 vUV;
void main(){vec4 w=uModel*vec4(aPos,1);gl_Position=uProj*uView*w;vPos=w.xyz;vUV=aUV;vNormal=normalize(transpose(inverse(mat3(uModel)))*aNormal);}

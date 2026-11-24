#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uVP;
uniform vec3 uOffset;
uniform vec2 uWorldOrigin;
out vec3 vPos,vNormal,vRelative;
void main(){vPos=vec3(aPos.x+uWorldOrigin.x,aPos.y,aPos.z+uWorldOrigin.y);vNormal=aNormal;vRelative=aPos+uOffset;gl_Position=uVP*vec4(vRelative,1);}

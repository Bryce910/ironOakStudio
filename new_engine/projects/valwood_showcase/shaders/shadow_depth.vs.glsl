#version 330 core
layout(location=0)in vec3 aPos;
uniform mat4 uLightVP,uModel;
void main(){gl_Position=uLightVP*uModel*vec4(aPos,1);}

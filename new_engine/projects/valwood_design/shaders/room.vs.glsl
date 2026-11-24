#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNorm;
layout(location=2)in vec3 aColor;
uniform mat4 uProj,uView;
out vec3 vColor;
out vec3 vNorm;
out vec3 vPos;
void main(){
    gl_Position=uProj*uView*vec4(aPos,1.0);
    vPos=aPos; vNorm=aNorm; vColor=aColor;
}

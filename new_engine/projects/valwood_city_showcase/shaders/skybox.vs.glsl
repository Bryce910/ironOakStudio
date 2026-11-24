#version 330 core
layout(location=0)in vec3 aPos;
uniform mat4 uProj,uView;
out vec3 vDir;
void main(){
    vDir=aPos;
    mat4 v=uView;
    v[3]=vec4(0,0,0,1);
    vec4 p=uProj*v*vec4(aPos,1);
    gl_Position=p.xyww;
}

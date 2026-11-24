#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec3 aColor;
uniform mat4 uProj,uView;
out vec3 vNormal,vPos,vColor;
out vec3 vTangent,vBitangent;
void main(){
    gl_Position=uProj*uView*vec4(aPos,1);
    vNormal=aNormal; vPos=aPos; vColor=aColor;
    vec3 up=abs(aNormal.y)>0.9?vec3(1,0,0):vec3(0,1,0);
    vTangent=normalize(cross(up,aNormal));
    vBitangent=normalize(cross(aNormal,vTangent));
}

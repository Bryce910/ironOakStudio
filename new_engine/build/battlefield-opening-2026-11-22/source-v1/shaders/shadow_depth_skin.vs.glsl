#version 330 core
layout(location=0)in vec3 aPos;
layout(location=3)in vec4 aWeights;
layout(location=4)in ivec4 aBones;
uniform mat4 uLightVP,uModel,uBones[128];
void main(){
    mat4 sk=aWeights.x*uBones[aBones.x]+aWeights.y*uBones[aBones.y]+aWeights.z*uBones[aBones.z]+aWeights.w*uBones[aBones.w];
    if(dot(aWeights,vec4(1))<0.05)sk=mat4(1);
    gl_Position=uLightVP*uModel*sk*vec4(aPos,1);
}

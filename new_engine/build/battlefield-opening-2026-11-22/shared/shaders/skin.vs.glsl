#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec2 aUV;
layout(location=3)in vec4 aWeights;
layout(location=4)in ivec4 aBones;
layout(location=5)in vec3 aTangent;
layout(location=6)in vec3 aBitangent;
uniform mat4 uProj,uView,uModel,uBones[128];
out vec3 vNormal,vFragPos,vTangent,vBitangent;
out vec2 vUV;
void main(){
    mat4 skin=aWeights.x*uBones[aBones.x]+aWeights.y*uBones[aBones.y]+aWeights.z*uBones[aBones.z]+aWeights.w*uBones[aBones.w];
    if(dot(aWeights,vec4(1))<0.05)skin=mat4(1);
    vec3 skinnedNormal=mat3(skin)*aNormal;
    vec3 skinnedTangent=mat3(skin)*aTangent;
    vec3 skinnedBitangent=mat3(skin)*aBitangent;
    vec4 world=uModel*skin*vec4(aPos,1);
    gl_Position=uProj*uView*world;
    vFragPos=world.xyz;
    mat3 nmat=mat3(transpose(inverse(uModel)));
    vNormal=normalize(nmat*skinnedNormal);
    vTangent=normalize(nmat*skinnedTangent);
    vBitangent=normalize(nmat*skinnedBitangent);
    vUV=aUV;
}

#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTangent;
layout(location=4) in vec4 aInstance;
layout(location=5) in float aYaw;
layout(location=6) in uint aPaletteBase;
layout(location=7) in uvec4 aJoints;
layout(location=8) in vec4 aWeights;
uniform samplerBuffer uSkinPalette;
uniform int uJointCount;
uniform mat4 uVP;
uniform vec3 uOffset;
uniform vec2 uWorldAnchor;
uniform float uTime;
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
out float vRootedGrassWindSpeed;
out vec2 vUV;
out float vFacadePaint;
flat out float vSeed,vCoveragePhase,vScale,vRootHeight,vInstanceFade;
flat out vec3 vRootRelative;
// VALWOOD_GRAPH_VERTEX

mat4 jointMatrix(uint joint) {
    int base=int(aPaletteBase+min(joint,uint(max(uJointCount-1,0)))*4u);
    return mat4(texelFetch(uSkinPalette,base),texelFetch(uSkinPalette,base+1),
                texelFetch(uSkinPalette,base+2),texelFetch(uSkinPalette,base+3));
}
void main() {
    vRootedGrassWindSpeed=0.;
    // The committed palette already includes the animated source mesh-node
    // transform. It is shared unchanged by color, reflection and shadow passes.
    mat4 skin=jointMatrix(aJoints.x)*aWeights.x+jointMatrix(aJoints.y)*aWeights.y+
              jointMatrix(aJoints.z)*aWeights.z+jointMatrix(aJoints.w)*aWeights.w;
    vec3 position=(skin*vec4(aPos,1.)).xyz;
    mat3 basis=mat3(skin);
    float skinDeterminant=determinant(basis);
    vec3 normal=abs(skinDeterminant)>1e-10?transpose(inverse(basis))*aNormal:aNormal;
    normal=dot(normal,normal)>1e-12?normalize(normal):vec3(0,1,0);
    vec3 tangent=basis*aTangent.xyz;
    tangent-=normal*dot(tangent,normal);
    vec3 fallback=cross(abs(normal.y)<.9?vec3(0,1,0):vec3(1,0,0),normal);
    tangent=dot(tangent,tangent)>1e-12?normalize(tangent):normalize(fallback);
    float c=cos(aYaw),s=sin(aYaw);
    mat3 yaw=mat3(c,0,-s,0,1,0,s,0,c);
    vPos=yaw*position*aInstance.w+aInstance.xyz;
#ifdef VALWOOD_STUDIO_GRAPH
    // The same bounded graph displacement follows the same frozen skin pose
    // in color, reflection and depth; do not scale/rotate an offset twice.
    vPos+=studioGraphVertex(aUV,vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y),uTime);
#endif
    vRelative=vPos+uOffset;
    vNormal=normalize(yaw*normal);
    vTangent=normalize(yaw*tangent);
    vBitangent=cross(vNormal,vTangent)*aTangent.w*(skinDeterminant<0.?-1.:1.);
    vUV=aUV;
    vFacadePaint=0.;
    vScale=aInstance.w;
    vRootRelative=aInstance.xyz+uOffset;
    vRootHeight=aInstance.y;
    vInstanceFade=1.;
    vSeed=fract(sin(aYaw*37.17+aInstance.w*19.47)*4375.85);
    vec3 q=fract(aPos*.173+vec3(vSeed));
    q+=dot(q,q.yzx+19.19);
    vCoveragePhase=fract((q.x+q.y)*q.z);
    gl_Position=uVP*vec4(vRelative,1.);
}

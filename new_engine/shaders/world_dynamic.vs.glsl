#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTangent;
layout(location=4) in vec4 aInstance;
layout(location=7) in float aFacadePaint;
uniform mat4 uVP;
uniform vec3 uOffset;
uniform vec2 uWorldAnchor;
uniform float uTime;
uniform vec4 uRigidQuaternion=vec4(0,0,0,1);
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
out vec2 vUV;
// Neutral inputs for the shared world material fragment contract.
out vec4 vBakedLight;
flat out vec2 vItemWind;
out vec3 vSourcePaint;
out float vFacadePaint;
flat out float vSeed,vCoveragePhase,vScale,vRootHeight,vInstanceFade;
flat out vec3 vRootRelative;
// VALWOOD_GRAPH_VERTEX
vec3 rotateRigid(vec3 p) {
    // CPU supplies a normalized quaternion from the committed physics pose.
    return p+2.*cross(uRigidQuaternion.xyz,cross(uRigidQuaternion.xyz,p)+uRigidQuaternion.w*p);
}
void main() {
    vBakedLight=vec4(0.);vItemWind=vec2(0.);vSourcePaint=vec3(0.);
    vPos=rotateRigid(aPos)*aInstance.w+aInstance.xyz;
#ifdef VALWOOD_STUDIO_GRAPH
    // Graph offsets are world-space metres, after placement, exactly once.
    vPos+=studioGraphVertex(aUV,vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y),uTime);
#endif
    vRelative=vPos+uOffset;
    vNormal=normalize(rotateRigid(aNormal));
    vTangent=normalize(rotateRigid(aTangent.xyz));
    vBitangent=cross(vNormal,vTangent)*aTangent.w;
    vUV=aUV;vFacadePaint=aFacadePaint;
    vRootRelative=aInstance.xyz+uOffset;vRootHeight=aInstance.y;
    vScale=aInstance.w;vInstanceFade=1.;
    vSeed=fract(sin(aInstance.w*19.47)*4375.85);
    vec3 q=fract(aPos*.173+vec3(vSeed));q+=dot(q,q.yzx+19.19);
    vCoveragePhase=fract((q.x+q.y)*q.z);
    gl_Position=uVP*vec4(vRelative,1.);
}

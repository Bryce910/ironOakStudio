#version 330 core
// Per-object motion for temporal AA: each vertex is skinned with this frame's
// and last frame's joint palettes; the previous position is also moved back
// by last frame's root translation/yaw, then projected with the previous
// frame's matrix. Without a valid previous palette only root motion is used.
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
layout(location=4) in vec4 aInstance;
layout(location=5) in float aYaw;
layout(location=6) in uint aPaletteBase;
layout(location=7) in uvec4 aJoints;
layout(location=8) in vec4 aWeights;
layout(location=9) in uint aMorphVertex;
uniform samplerBuffer uSkinPalette,uPrevSkinPalette;
uniform bool uHasPrevPalette;
uniform int uJointCount;
uniform int uPrevPaletteOffset; // mode1 previous bank relative to aPaletteBase
uniform int uMorphBase,uMorphCount,uMorphMode;
uniform mat4 uVP,uPrevVP;
uniform vec3 uOffset,uCameraDelta,uRootDelta;
uniform float uRootYawDelta;
out vec2 vUV;
out vec4 vPreviousClip,vStaticClip;
mat4 jointMatrix(uint joint) {
    int base=int(aPaletteBase+min(joint,uint(max(uJointCount-1,0)))*4u);
    return mat4(texelFetch(uSkinPalette,base),texelFetch(uSkinPalette,base+1),
                texelFetch(uSkinPalette,base+2),texelFetch(uSkinPalette,base+3));
}
mat4 previousJointMatrix(uint joint) {
    int base=int(aPaletteBase)+uPrevPaletteOffset+int(min(joint,uint(max(uJointCount-1,0)))*4u);
    return mat4(texelFetch(uPrevSkinPalette,base),texelFetch(uPrevSkinPalette,base+1),
                texelFetch(uPrevSkinPalette,base+2),texelFetch(uPrevSkinPalette,base+3));
}
// Facial morph offsets (positions only) precede skinning, matching world_skeletal.vs.
vec3 facialMorph(vec3 position,samplerBuffer palette,int paletteBase){
    if(aMorphVertex==0u||uMorphCount<=0)return position;
    if(uMorphMode==2)return position+texelFetch(palette,paletteBase+uJointCount*4+8+int(aMorphVertex-1u)*3).xyz;
    int weightsBase=paletteBase+uJointCount*4;
    int vertexBase=uMorphBase+int(aMorphVertex-1u)*uMorphCount*3;
    for(int target=0;target<32;++target){if(target>=uMorphCount)break;
        float weight=texelFetch(palette,weightsBase+target/4)[target%4];
        if(weight>0.)position+=texelFetch(palette,vertexBase+target*3).xyz*weight;
    }
    return position;
}
mat3 yawMatrix(float a){float c=cos(a),s=sin(a);return mat3(c,0,-s,0,1,0,s,0,c);}
void main() {
    mat4 skin=jointMatrix(aJoints.x)*aWeights.x+jointMatrix(aJoints.y)*aWeights.y+
              jointMatrix(aJoints.z)*aWeights.z+jointMatrix(aJoints.w)*aWeights.w;
    vec3 local=facialMorph(aPos,uSkinPalette,int(aPaletteBase));
    vec3 position=(skin*vec4(local,1.)).xyz;
    vec3 previousPosition=position;
    if(uHasPrevPalette){
        mat4 previousSkin=previousJointMatrix(aJoints.x)*aWeights.x+previousJointMatrix(aJoints.y)*aWeights.y+
                          previousJointMatrix(aJoints.z)*aWeights.z+previousJointMatrix(aJoints.w)*aWeights.w;
        vec3 previousLocal=facialMorph(aPos,uPrevSkinPalette,int(aPaletteBase)+uPrevPaletteOffset);
        previousPosition=(previousSkin*vec4(previousLocal,1.)).xyz;
    }
    vec3 current=yawMatrix(aYaw)*position*aInstance.w+aInstance.xyz;
    vec3 previous=yawMatrix(aYaw-uRootYawDelta)*previousPosition*aInstance.w+aInstance.xyz-uRootDelta;
    vec3 relative=current+uOffset;
    // Both are expressed relative to the previous camera, so the stored
    // difference is the object's own motion on top of camera reprojection.
    vStaticClip=uPrevVP*vec4(relative+uCameraDelta,1.);
    vPreviousClip=uPrevVP*vec4(previous+uOffset+uCameraDelta,1.);
    vUV=aUV;
    gl_Position=uVP*vec4(relative,1.);
}

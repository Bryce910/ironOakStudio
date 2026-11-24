#version 330 core
layout(location=0)in vec3 aPos;layout(location=1)in vec3 aNormal;layout(location=2)in vec2 aUV;layout(location=3)in vec4 aTangent;
layout(location=4)in vec4 aInstance;layout(location=5)in float aYaw;
layout(location=6)in vec4 aVertexPosition;
layout(location=14)in vec4 aVertexNormal;
layout(location=15)in vec4 aVertexTangent;
layout(location=8)in vec3 aPosePosition;layout(location=9)in vec3 aPoseNormal;layout(location=10)in vec4 aPoseTangent;layout(location=11)in float aPoseBlend;
uniform bool uPoseBlendEnabled;uniform mat4 uProjection;
uniform bool uVertexOverrideEnabled=false;
uniform bool uRigidWeaponPose=false;
uniform vec4 uRigidWeaponRotation=vec4(0.,0.,0.,1.);
uniform vec3 uRigidWeaponPivotA=vec3(0.),uRigidWeaponPivotB=vec3(0.);
out vec3 P,N,T,B;out vec2 UV;
vec3 rotateWeapon(vec3 p,vec4 q){return p+2.*cross(q.xyz,cross(q.xyz,p)+q.w*p);}
vec3 safeNormal(vec3 n,vec3 fallback){return dot(n,n)>1e-12?normalize(n):fallback;}
void main(){
    float w=uPoseBlendEnabled?clamp(aPoseBlend,0.,1.):0.;
    vec3 p=mix(aPos,aPosePosition,w),n=safeNormal(mix(aNormal,aPoseNormal,w),aNormal);
    vec3 tangent=mix(aTangent.xyz,aPoseTangent.xyz,w);
    // Match the world renderer and the contact/trail socket exactly. The
    // former viewmodel shader linearly shortened rotating weapon meshes.
    if(uPoseBlendEnabled&&uRigidWeaponPose){
        vec4 q=normalize(mix(vec4(0.,0.,0.,1.),uRigidWeaponRotation,w));
        p=rotateWeapon(aPos-uRigidWeaponPivotA,q)+mix(uRigidWeaponPivotA,uRigidWeaponPivotB,w);
        n=safeNormal(rotateWeapon(aNormal,q),aNormal);tangent=rotateWeapon(aTangent.xyz,q);
    }
    if(uPoseBlendEnabled&&uVertexOverrideEnabled&&aVertexPosition.w>0.){
        float strength=clamp(aVertexPosition.w,0.,1.);
        p=mix(p,aVertexPosition.xyz,strength);
        n=safeNormal(mix(n,aVertexNormal.xyz,strength),n);
        tangent=mix(tangent,aVertexTangent.xyz,strength);
    }
    tangent-=n*dot(tangent,n);
    vec3 fallback=cross(abs(n.y)<.9?vec3(0,1,0):vec3(1,0,0),n);
    tangent=safeNormal(tangent,normalize(fallback));
    float c=cos(aYaw),s=sin(aYaw);mat3 R=mat3(c,0,-s,0,1,0,s,0,c);
    P=R*p*aInstance.w+aInstance.xyz;N=R*n;T=R*tangent;
    B=normalize(cross(N,T))*aTangent.w;UV=aUV;gl_Position=uProjection*vec4(P,1);
}

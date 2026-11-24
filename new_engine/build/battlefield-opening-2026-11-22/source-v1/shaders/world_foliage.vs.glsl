#version 330 core
invariant gl_Position; // AAA merge: prepass and colour pass must produce identical depth
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTangent;
layout(location=4) in vec4 aInstance;
layout(location=5) in float aYaw;
layout(location=6) in vec4 aFadeWind; // fade, item sway, item flutter; optional player position/weight
uniform bool uItemWindEnabled=false;
layout(location=14) in vec4 aPitchRoll;
layout(location=15) in vec4 aAxisScale;
uniform bool uVertexOverrideEnabled=false;
uniform bool uFullTransform;
layout(location=7) in float aFacadePaint;
layout(location=8) in vec3 aPosePosition;
layout(location=9) in vec3 aPoseNormal;
layout(location=10) in vec4 aPoseTangent;
layout(location=11) in float aPoseBlend;
#ifdef VALWOOD_CHARACTER_APPEARANCE
layout(location=12) in vec4 aCharacterData;
layout(location=13) in vec4 aCharacterHair;
uniform bool uCharacterEnabled=false;
flat out vec4 vCharacterData,vCharacterHair;
#endif
uniform bool uSourcePaintEnabled=false;
uniform bool uRootedGrassWind=false;
#pragma include "world_rooted_grass_wind.glsl"
#pragma include "world_grass_interaction.glsl"
out vec3 vSourcePaint;
// VALWOOD_CITY_SOURCE_BAKED_LIGHT
#ifndef VALWOOD_CHARACTER_APPEARANCE
layout(location=12) in vec4 aBakedLight;
layout(location=13) in vec4 aAnatomyUpper;
#endif
uniform bool uBakedLightEnabled=false;
out vec4 vBakedLight;
out float gl_ClipDistance[8];
uniform bool uAnatomyEnabled=false,uAnatomyMirror=false,uAnatomyCaps=false;
uniform int uAnatomyMask=0,uAnatomyKeep=0;
uniform vec4 uAnatomyInstance=vec4(0.,0.,0.,1.),uAnatomyRotation=vec4(0.,0.,0.,1.);
uniform vec2 uAnatomyYawWeight=vec2(0.);
uniform vec3 uAnatomyPivot=vec3(0.);
uniform float uAnatomyCapSide=1.;
uniform bool uPoseBlendEnabled;
uniform bool uRigidWeaponPose=false;
uniform vec4 uRigidWeaponRotation=vec4(0.,0.,0.,1.);
uniform vec3 uRigidWeaponPivotA=vec3(0.),uRigidWeaponPivotB=vec3(0.);
vec3 rigidWeaponRotate(vec3 p,vec4 q){return p+2.*cross(q.xyz,cross(q.xyz,p)+q.w*p);}
uniform vec4 uGrassInteractor=vec4(0.); // world XZ, feet Y, bounded contact radius
uniform vec4 uGrassInteractionHistory[3]; // world XZ, feet Y, recovering weight
uniform vec4 uGrassInteractionBounds=vec4(0.); // center XZ, squared radius, contact speed
// Only the color program opts in. Shadow fragment coverage has no size fade.
uniform bool uColorCoverageCull=false;
uniform mat4 uVP;
uniform vec3 uOffset;
uniform vec2 uWorldAnchor,uWeatherWind;
uniform float uTime,uWind,uTreeHeight,uAlphaCutoff,uPixelScale;
uniform int uVegetation;
uniform bool uInstanceFade;
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
out float vRootedGrassWindSpeed;
flat out vec2 vItemWind;
out vec2 vUV;
out float vFacadePaint;
flat out float vSeed;
flat out float vCoveragePhase;
flat out float vScale;
flat out vec3 vRootRelative;
flat out float vRootHeight;
flat out float vInstanceFade;
#ifdef VALWOOD_STUDIO_SURFACES
uniform vec2 uStudioNormalUV=vec2(1.);
#endif
#ifdef VALWOOD_LOCAL_WIND
#pragma include "world_surface_wind.glsl"
#else
vec2 studioWind(vec3 world,vec2 base){return base;}
#endif
// VALWOOD_GRAPH_VERTEX
void main() {
    vec4 instance=uAnatomyEnabled?uAnatomyInstance:aInstance;
    float instanceYaw=uAnatomyEnabled?uAnatomyYawWeight.x:aYaw;
    float poseBlend=uAnatomyEnabled?uAnatomyYawWeight.y:aPoseBlend;
    for(int j=0;j<8;++j)gl_ClipDistance[j]=1.;

    vRootedGrassWindSpeed=0.;
    vItemWind=uItemWindEnabled?uWind*clamp(aFadeWind.yz,vec2(0.),vec2(2.)):vec2(uWind);
    float swayWind=vItemWind.x,flutterWind=vItemWind.y;
    // Decode a separate validated GI stream. Premultiplication prevents a
    // fallback vertex from darkening a neighbouring valid sample.
    vBakedLight=vec4(0);
#ifndef VALWOOD_CHARACTER_APPEARANCE
    if(uBakedLightEnabled&&!uPoseBlendEnabled)
        vBakedLight=vec4(aBakedLight.rgb*aBakedLight.a,aBakedLight.a);
#endif
    vSourcePaint=vec3(0);
    // Source RGB8 is decoded per vertex; interpolation operates on RGB.
    if(uSourcePaintEnabled&&!uPoseBlendEnabled) {
        // VISUAL_SOURCE_PAINT_UINT24_V1: the CPU stream is already canonical RGB24.
        uint value24=uint(aFacadePaint);
        vSourcePaint=vec3(float(value24&255u),float((value24>>8u)&255u),
            float((value24>>16u)&255u))/255.;
    }
    // Begin zero-coverage card guard.
    // The fragment shader already discards every card below3 projected pixels.
    // A2.99 guard retains a conservative margin for VS/FS float differences.
    // Only finite, positive-scale inputs may use the early all-vertex clip.
    if(!uFullTransform&&uColorCoverageCull&&uVegetation==4&&instance.w>0.&&uPixelScale>0.&&
       !isnan(uTreeHeight)&&!isinf(uTreeHeight)&&!isinf(uPixelScale)) {
        vec3 rootRelative=instance.xyz+uOffset;
        float height=max(uTreeHeight,1.)*instance.w;
        float centerDistance=length(rootRelative+vec3(0.,height*.5,0.));
        float projected=height*uPixelScale/max(centerDistance,.1);
        if(!any(isnan(rootRelative))&&!any(isinf(rootRelative))&&
           !isnan(height)&&!isinf(height)&&!isnan(centerDistance)&&!isinf(centerDistance)&&
           projected>=0.&&projected<2.99) {
            gl_Position=vec4(2.,2.,2.,1.);
            return;
        }
    }
    // End zero-coverage card guard.
    vec3 posePosition=aPos,poseNormal=aNormal;
    vec4 poseTangent=aTangent;
    if(uPoseBlendEnabled) {
        float weight=clamp(poseBlend,0.,1.);
        if(weight>=1.) {posePosition=aPosePosition;poseNormal=aPoseNormal;poseTangent=aPoseTangent;}
        else if(weight>0.) {
            // Interpolate the same indexed surface once, before all placement,
            // view/reflection/shadow transforms. No alpha crossfade/extra draw.
            posePosition=mix(aPos,aPosePosition,weight);
            vec3 normal=mix(aNormal,aPoseNormal,weight);
            poseNormal=dot(normal,normal)>1e-12?normalize(normal):aNormal;
            vec3 tangent=mix(aTangent.xyz,aPoseTangent.xyz,weight);
            tangent-=poseNormal*dot(tangent,poseNormal);
            vec3 fallback=cross(abs(poseNormal.y)<.9?vec3(0,1,0):vec3(1,0,0),poseNormal);
            poseTangent=vec4(dot(tangent,tangent)>1e-12?normalize(tangent):normalize(fallback),aTangent.w);
        }
    }

    // Certified rigid weapons keep their authored size at every fraction.
    // The same grip path, shortest quaternion arc and socket sampling are used
    // by color, depth, reflection and first-person trails.
    if(uPoseBlendEnabled&&uRigidWeaponPose){
        float w=clamp(poseBlend,0.,1.);
        vec4 q=normalize(mix(vec4(0.,0.,0.,1.),uRigidWeaponRotation,w));
        posePosition=rigidWeaponRotate(aPos-uRigidWeaponPivotA,q)+mix(uRigidWeaponPivotA,uRigidWeaponPivotB,w);
        vec3 n=rigidWeaponRotate(aNormal,q);poseNormal=dot(n,n)>1e-12?normalize(n):aNormal;
        vec3 t=rigidWeaponRotate(aTangent.xyz,q);t-=poseNormal*dot(t,poseNormal);
        vec3 fallback=cross(abs(poseNormal.y)<.9?vec3(0,1,0):vec3(1,0,0),poseNormal);
        poseTangent=vec4(dot(t,t)>1e-12?normalize(t):normalize(fallback),aTangent.w);
    }

    // One continuous indexed surface: only anatomically selected lower-body
    // vertices blend to the collision-speed-matched gait. Hands/weapon/head
    // weights are exactly zero. Main, reflection and depth use this same path.
    if(uVertexOverrideEnabled&&uPoseBlendEnabled&&aFadeWind.w>0.){
        float w=clamp(aFadeWind.w,0.,1.);
        posePosition=mix(posePosition,aFadeWind.xyz,w);
        vec3 n=mix(poseNormal,aPitchRoll.xyz,w);
        poseNormal=dot(n,n)>1e-12?normalize(n):poseNormal;
        vec3 t=mix(poseTangent.xyz,aAxisScale.xyz,w);
        t-=poseNormal*dot(t,poseNormal);
        if(dot(t,t)>1e-12)poseTangent.xyz=normalize(t);
    }


#ifdef VALWOOD_CHARACTER_APPEARANCE
    vCharacterData=uCharacterEnabled?aCharacterData:vec4(1.,0.,1.,0.);
    vCharacterHair=uCharacterEnabled?aCharacterHair:vec4(1.,1.,1.,0.);
    // Inverse-transpose normals and forward tangents for the same rooted XZ
    // body shape, after interpolation and before placement in every pass.
    if(uCharacterEnabled){
        vec3 shape=vec3(aCharacterData.x,1.,aCharacterData.x);
        posePosition*=shape;poseNormal=normalize(poseNormal/shape);
        vec3 tangent=poseTangent.xyz*shape;tangent-=poseNormal*dot(tangent,poseNormal);
        poseTangent.xyz=normalize(tangent);
    }
#endif
    if(uAnatomyEnabled){
        if(!uAnatomyCaps){
#ifdef VALWOOD_CHARACTER_APPEARANCE
            vec4 regions0=aCharacterData,regions1=aCharacterHair;
#else
            vec4 regions0=aBakedLight,regions1=aAnatomyUpper;
#endif
            float regions[8]=float[8](regions0.x,regions0.y,regions0.z,regions0.w,regions1.x,regions1.y,regions1.z,regions1.w);
            if(uAnatomyMirror){float t=regions[2];regions[2]=regions[3];regions[3]=t;t=regions[4];regions[4]=regions[5];regions[5]=t;t=regions[6];regions[6]=regions[7];regions[7]=t;}
            for(int j=0;j<8;++j){if(uAnatomyKeep==j+1)gl_ClipDistance[j]=regions[j]-.5;else if((uAnatomyMask&(1<<j))!=0)gl_ClipDistance[j]=.5-regions[j];}
        }
        if(uAnatomyCaps&&uAnatomyCapSide<0.){poseNormal=-poseNormal;poseTangent.w=-poseTangent.w;}
        if(uAnatomyMirror){posePosition.x=-posePosition.x;poseNormal.x=-poseNormal.x;poseTangent.x=-poseTangent.x;poseTangent.w=-poseTangent.w;}
        posePosition=rigidWeaponRotate(posePosition-uAnatomyPivot,uAnatomyRotation);
        poseNormal=rigidWeaponRotate(poseNormal,uAnatomyRotation);poseTangent.xyz=rigidWeaponRotate(poseTangent.xyz,uAnatomyRotation);
    }
    vFacadePaint=uSourcePaintEnabled?vSourcePaint.r:aFacadePaint;
    vInstanceFade=uInstanceFade?clamp(aFadeWind.x,0.,1.):1.;
    float c=cos(instanceYaw),s=sin(instanceYaw);
    mat3 rotation=mat3(c,0,-s,0,1,0,s,0,c);
    vec2 root=instance.xz+uWorldAnchor;
    // Static meshes and explicitly motion-disabled items never consume the
    // wind field. Skip its sixteen authored-area tests without changing any
    // nonzero sway/flutter or the separate player-contact path below.
    vec2 weatherWind=vec2(0.);
    if(uVegetation>0&&(swayWind!=0.||flutterWind!=0.))
        weatherWind=studioWind(vec3(root.x,instance.y,root.y),uWeatherWind);
    vec3 axisScale=vec3(1);
    if(uFullTransform){float cx=cos(aPitchRoll.x),sx=sin(aPitchRoll.x),cz=cos(aPitchRoll.y),sz=sin(aPitchRoll.y);rotation=rotation*mat3(1,0,0,0,cx,sx,0,-sx,cx)*mat3(cz,sz,0,-sz,cz,0,0,0,1);axisScale=aAxisScale.xyz;}
    vec3 p=rotation*(posePosition*axisScale)*instance.w;
    if(uVegetation==4) {
        vec2 toward=-(instance.xz+uOffset.xz);
        float squared=dot(toward,toward);
        toward=squared>.001 ? toward*inversesqrt(squared) : vec2(0.,1.);
        p=vec3(toward.y*posePosition.x,posePosition.y,-toward.x*posePosition.x)*instance.w;
        if(uFullTransform){float cx=cos(aPitchRoll.x),sx=sin(aPitchRoll.x),cz=cos(aPitchRoll.y),sz=sin(aPitchRoll.y);rotation=mat3(toward.y,0,-toward.x,0,1,0,toward.x,0,toward.y)*mat3(1,0,0,0,cx,sx,0,-sx,cx)*mat3(cz,sz,0,-sz,cz,0,0,0,1);p=rotation*(posePosition*axisScale)*instance.w;}
    }
    vScale=instance.w;
    // LOD selection on the CPU uses the root. Keep the entire crown in that
    // same overlap band, including camera-facing cards and moving branches.
    vRootRelative=instance.xyz+uOffset;
    vRootHeight=instance.y;
    float phase=dot(root,vec2(.071,.047));
    float gust=0.;
    if(swayWind!=0.)gust=sin(uTime*.84+phase)+.38*sin(uTime*1.67+phase*.43);
    float amplitude=(uVegetation==2||uVegetation==5) ? pow(clamp(posePosition.y/.4,0.,1.),2.)*.075 : min(max(p.y,0.)*.011,.23);
    vec2 bend_derivative=vec2(0);
    float weather_speed=length(weatherWind);
    bool rootedGrass=uRootedGrassWind&&(uVegetation==2||uVegetation==3||uVegetation==5);
    vec3 grassUp=normalize(rotation[1]);
    vec3 grassDerivative=vec3(0.);
    RootedGrassWarp grassWind;
    grassWind.displacement=vec3(0.);grassWind.derivative=vec3(0.);grassWind.speed_bound=0.;
    if(rootedGrass) {
        float modelHeight=max(uTreeHeight,.0001);
        float worldHeight=max(modelHeight*instance.w*axisScale.y,.0001);
        RootedGrassWarp warp=rootedGrassWindSplit(uTime,root,weatherWind,modelHeight,worldHeight,posePosition.y,swayWind,flutterWind);
        grassWind=warp;p+=warp.displacement;grassDerivative=warp.derivative;vRootedGrassWindSpeed=warp.speed_bound;
    } else if(uVegetation==6) {
        // Crop clusters retain every rooted stalk. One smooth field gust bends
        // their height quadratically, without translating the soil-level base.
        // At the fixed .60 wind, all supported scales/weather stay within .35m
        // horizontal displacement; existing source spheres have a2m guard.
        float height=max(uTreeHeight*instance.w,.20);
        float h=clamp(max(p.y,0.)/height,0.,1.);
        vec2 bend=vec2(gust,sin(uTime*.62+phase*.73)*.38)*min(height*.045,.16)*swayWind;
        if(weather_speed>.00001&&swayWind>0.) {
            float strength=min(weather_speed,3.)*swayWind;
            bend+=weatherWind/weather_speed*min(height*.018,.12)*strength*(.58+.24*gust);
        }
        p.xz+=bend*h*h;
        if(p.y>0.&&p.y<=height)bend_derivative=bend*(2.*h/height);
    } else if(swayWind!=0.) {
        p.x+=gust*amplitude*swayWind;
        p.z+=sin(uTime*.62+phase*.73)*amplitude*.38*swayWind;
    }
    if(!rootedGrass&&uVegetation!=6&&weather_speed>.00001&&(swayWind>0.0||flutterWind>0.0)&&uVegetation>0) {
        vec2 direction=weatherWind/weather_speed;
        float height=max(((uVegetation==2||uVegetation==5)?.4:uTreeHeight)*instance.w,.20);
        float h=clamp(max(p.y,0.0)/height,0.0,1.0);
        float strength=min(weather_speed,3.0)*swayWind;
        float lean=min(height*.014,.42)*strength*(.58+.24*gust);
        p.xz+=direction*lean*h*h;
        bend_derivative=direction*lean*(2.0*h/height);
        bool thin=uVegetation==2||uVegetation==3||uVegetation==5||(uVegetation==1&&uAlphaCutoff>0.0);
        if(thin) {
            float flutter=sin(uTime*5.1+phase*2.7+dot(posePosition.xz,vec2(3.7,4.1)));
            float flutter_size=(uVegetation==2||uVegetation==5)?.009:.014;
            p.xz+=vec2(-direction.y,direction.x)*flutter*flutter_size*h*(min(weather_speed,3.0)*flutterWind);
        }
    }
    bool contacted=false;vec3 contactDerivative=vec3(0.);
    float contactHeight=max(uTreeHeight*instance.w*axisScale.y,.0001);
    bool itemMotion=!uItemWindEnabled||aFadeWind.y>0.||aFadeWind.z>0.;
    if(uGrassInteractionBounds.z>0.&&grassContactEligible(uVegetation,contactHeight,itemMotion)) {
        // A tuft shares its stable soil root, matching wind and preserving
        // exact height derivatives under nonuniform scale/slope rotation.
        vec2 anchor=root;
        vec2 broad=anchor-uGrassInteractionBounds.xy;
        if(dot(broad,broad)<uGrassInteractionBounds.z) {
            vec2 push=grassContactPush(anchor,instance.y,vec4(uGrassInteractor.xyz,uGrassInteractor.w>0.?1.:0.),.95);
            for(int i=0;i<3;++i)push+=grassContactPush(anchor,instance.y,uGrassInteractionHistory[i],.95);
            RootedGrassWarp contact=grassContactWarp(push,max(uTreeHeight,.0001),contactHeight,posePosition.y,uGrassInteractionBounds.w);
            contacted=dot(contact.displacement,contact.displacement)>0.;
            if(rootedGrass) {
                RootedGrassWarp combined=grassContactCombine(grassWind,contact,max(uTreeHeight,.0001),contactHeight,posePosition.y);
                p+=combined.displacement-grassWind.displacement;
                grassDerivative=combined.derivative;
                vRootedGrassWindSpeed=combined.speed_bound;
            } else {
                p+=contact.displacement;
                contactDerivative=contact.derivative;
                vRootedGrassWindSpeed+=contact.speed_bound;
            }
        }
    }
#ifdef VALWOOD_STUDIO_GRAPH
    p+=studioGraphVertex(aUV,vec3(p.x+instance.x+uWorldAnchor.x,p.y+instance.y,p.z+instance.z+uWorldAnchor.y),uTime);
#endif
    vPos=p+instance.xyz;
    vRelative=vPos+uOffset;
    vNormal=normalize(rotation*(poseNormal/axisScale));
    vTangent=normalize(rotation*(poseTangent.xyz*axisScale));
    if(uFullTransform)vTangent=normalize(vTangent-vNormal*dot(vNormal,vTangent));
    if(rootedGrass) {
        vNormal=normalize(rootedGrassNormal(vNormal,grassDerivative,grassUp));
        vTangent=rootedGrassTangent(vTangent,grassDerivative,grassUp);
        vTangent=normalize(vTangent-vNormal*dot(vTangent,vNormal));
    } else if(contacted) {
        vec3 windDerivative=vec3(bend_derivative.x,0.,bend_derivative.y);
        vNormal=normalize(grassContactNormal(vNormal,windDerivative,contactDerivative,grassUp));
        vTangent=grassContactTangent(vTangent,windDerivative,contactDerivative,grassUp);
        vTangent=normalize(vTangent-vNormal*dot(vTangent,vNormal));
    } else if(uVegetation==6||(weather_speed>.00001&&(swayWind>0.0||flutterWind>0.0)&&uVegetation>0)) {
        // Inverse-transpose of the same height-dependent horizontal bend.
        vec3 bent_normal=vec3(vNormal.x,vNormal.y-dot(bend_derivative,vNormal.xz),vNormal.z);
        vNormal=dot(bent_normal,bent_normal)>1e-10?normalize(bent_normal):vec3(0,1,0);
        vec3 bent_tangent=vec3(vTangent.x+bend_derivative.x*vTangent.y,vTangent.y,vTangent.z+bend_derivative.y*vTangent.y);
        bent_tangent-=vNormal*dot(bent_tangent,vNormal);
        vec3 fallback=cross(abs(vNormal.y)<.9?vec3(0,1,0):vec3(1,0,0),vNormal);
        vTangent=dot(bent_tangent,bent_tangent)>1e-10?normalize(bent_tangent):normalize(fallback);
    }
    vBitangent=cross(vNormal,vTangent)*poseTangent.w;
    vUV=aUV;
#ifdef VALWOOD_STUDIO_SURFACES
    vUV*=max(uStudioNormalUV.y,.001);
#endif
    // World-seeded yaw/scale stay bit-identical when the local origin moves.
    // Hashing reconstructed float positions could change a plant's tint.
    vSeed=fract(sin(instanceYaw*37.17+instance.w*19.47)*4375.85);
    // Flat model-local phase decorrelates overlapping tiny shoots. It does not
    // depend on camera/origin shifts, wind time or instance-list ordering.
    vec3 q=fract(posePosition*.173+vec3(vSeed));
    q+=dot(q,q.yzx+19.19);
    vCoveragePhase=fract((q.x+q.y)*q.z);
    gl_Position=uVP*vec4(vRelative,1.);
}

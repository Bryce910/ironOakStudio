#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTangent;
layout(location=4) in vec4 aInstance;
layout(location=5) in float aYaw;
layout(location=6) in float aFade;
layout(location=14) in vec2 aPitchRoll;
layout(location=15) in vec3 aAxisScale;
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
out vec3 vSourcePaint;
// VALWOOD_CITY_SOURCE_BAKED_LIGHT
#ifndef VALWOOD_CHARACTER_APPEARANCE
layout(location=12) in vec4 aBakedLight;
#endif
uniform bool uBakedLightEnabled=false;
out vec4 vBakedLight;
uniform bool uPoseBlendEnabled;
uniform vec4 uGrassInteractor=vec4(0.); // world XZ, feet Y, bounded contact radius
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
    vRootedGrassWindSpeed=0.;
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
    if(!uFullTransform&&uColorCoverageCull&&uVegetation==4&&aInstance.w>0.&&uPixelScale>0.&&
       !isnan(uTreeHeight)&&!isinf(uTreeHeight)&&!isinf(uPixelScale)) {
        vec3 rootRelative=aInstance.xyz+uOffset;
        float height=max(uTreeHeight,1.)*aInstance.w;
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
        float weight=clamp(aPoseBlend,0.,1.);
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
    vFacadePaint=uSourcePaintEnabled?vSourcePaint.r:aFacadePaint;
    vInstanceFade=uInstanceFade?clamp(aFade,0.,1.):1.;
    float c=cos(aYaw),s=sin(aYaw);
    mat3 rotation=mat3(c,0,-s,0,1,0,s,0,c);
    vec2 root=aInstance.xz+uWorldAnchor;
    vec2 weatherWind=studioWind(vec3(root.x,aInstance.y,root.y),uWeatherWind);
    vec3 axisScale=vec3(1);
    if(uFullTransform){float cx=cos(aPitchRoll.x),sx=sin(aPitchRoll.x),cz=cos(aPitchRoll.y),sz=sin(aPitchRoll.y);rotation=rotation*mat3(1,0,0,0,cx,sx,0,-sx,cx)*mat3(cz,sz,0,-sz,cz,0,0,0,1);axisScale=aAxisScale;}
    vec3 p=rotation*(posePosition*axisScale)*aInstance.w;
    if(uVegetation==4) {
        vec2 toward=-(aInstance.xz+uOffset.xz);
        float squared=dot(toward,toward);
        toward=squared>.001 ? toward*inversesqrt(squared) : vec2(0.,1.);
        p=vec3(toward.y*posePosition.x,posePosition.y,-toward.x*posePosition.x)*aInstance.w;
        if(uFullTransform){float cx=cos(aPitchRoll.x),sx=sin(aPitchRoll.x),cz=cos(aPitchRoll.y),sz=sin(aPitchRoll.y);rotation=mat3(toward.y,0,-toward.x,0,1,0,toward.x,0,toward.y)*mat3(1,0,0,0,cx,sx,0,-sx,cx)*mat3(cz,sz,0,-sz,cz,0,0,0,1);p=rotation*(posePosition*axisScale)*aInstance.w;}
    }
    vScale=aInstance.w;
    // LOD selection on the CPU uses the root. Keep the entire crown in that
    // same overlap band, including camera-facing cards and moving branches.
    vRootRelative=aInstance.xyz+uOffset;
    vRootHeight=aInstance.y;
    float phase=dot(root,vec2(.071,.047));
    float gust=sin(uTime*.84+phase)+.38*sin(uTime*1.67+phase*.43);
    float amplitude=(uVegetation==2||uVegetation==5) ? pow(clamp(posePosition.y/.4,0.,1.),2.)*.075 : min(max(p.y,0.)*.011,.23);
    vec2 bend_derivative=vec2(0);vec2 unbentXZ=p.xz;
    float weather_speed=length(weatherWind);
    bool rootedGrass=uRootedGrassWind&&(uVegetation==2||uVegetation==3||uVegetation==5);
    vec3 grassUp=normalize(rotation[1]);
    vec3 grassDerivative=vec3(0.);
    if(rootedGrass) {
        float modelHeight=max(uTreeHeight,.0001);
        float worldHeight=max(modelHeight*aInstance.w*axisScale.y,.0001);
        RootedGrassWarp warp=rootedGrassWind(uTime,root,weatherWind,modelHeight,worldHeight,posePosition.y,uWind);
        p+=warp.displacement;grassDerivative=warp.derivative;vRootedGrassWindSpeed=warp.speed_bound;
    } else if(uVegetation==6) {
        // Crop clusters retain every rooted stalk. One smooth field gust bends
        // their height quadratically, without translating the soil-level base.
        // At the fixed .60 wind, all supported scales/weather stay within .35m
        // horizontal displacement; existing source spheres have a2m guard.
        float height=max(uTreeHeight*aInstance.w,.20);
        float h=clamp(max(p.y,0.)/height,0.,1.);
        vec2 bend=vec2(gust,sin(uTime*.62+phase*.73)*.38)*min(height*.045,.16)*uWind;
        if(weather_speed>.00001&&uWind>0.) {
            float strength=min(weather_speed,3.)*uWind;
            bend+=weatherWind/weather_speed*min(height*.018,.12)*strength*(.58+.24*gust);
        }
        p.xz+=bend*h*h;
        if(p.y>0.&&p.y<=height)bend_derivative=bend*(2.*h/height);
    } else {
        p.x+=gust*amplitude*uWind;
        p.z+=sin(uTime*.62+phase*.73)*amplitude*.38*uWind;
    }
    if(!rootedGrass&&uVegetation!=6&&weather_speed>.00001&&uWind>0.0&&uVegetation>0) {
        vec2 direction=weatherWind/weather_speed;
        float height=max(((uVegetation==2||uVegetation==5)?.4:uTreeHeight)*aInstance.w,.20);
        float h=clamp(max(p.y,0.0)/height,0.0,1.0);
        float strength=min(weather_speed,3.0)*uWind;
        float lean=min(height*.014,.42)*strength*(.58+.24*gust);
        p.xz+=direction*lean*h*h;
        bend_derivative=direction*lean*(2.0*h/height);
        bool thin=uVegetation==2||uVegetation==3||uVegetation==5||(uVegetation==1&&uAlphaCutoff>0.0);
        if(thin) {
            float flutter=sin(uTime*5.1+phase*2.7+dot(posePosition.xz,vec2(3.7,4.1)));
            float flutter_size=(uVegetation==2||uVegetation==5)?.009:.014;
            p.xz+=vec2(-direction.y,direction.x)*flutter*flutter_size*h*strength;
        }
    }
    bool contacted=false;
    if(uGrassInteractor.w>0.&&(uVegetation==2||uVegetation==5||(uVegetation==3&&uTreeHeight<1.1))) {
        vec2 delta=root+unbentXZ-uGrassInteractor.xy;float distance=length(delta);
        float contact=(1.-smoothstep(.12,uGrassInteractor.w,distance))*(1.-smoothstep(.20,.85,abs(aInstance.y-uGrassInteractor.z)));
        float height=max(.18,min(uTreeHeight*aInstance.w,.65));
        float h=rootedGrass?clamp(posePosition.y/max(uTreeHeight,.0001),0.,1.):clamp(max(p.y,0.)/height,0.,1.);
        vec2 push=(distance>.0001?delta/distance:vec2(1,0))*(.18*contact);
        p.xz+=push*h*h;
        if(rootedGrass) {
            if(posePosition.y>0.&&posePosition.y<=uTreeHeight)
                grassDerivative+=vec3(push.x,0.,push.y)*(2.*h/max(uTreeHeight*aInstance.w*axisScale.y,.0001));
        } else if(p.y>0.&&p.y<height)bend_derivative+=push*(2.*h/height);
        contacted=contact>0.;
    }
#ifdef VALWOOD_STUDIO_GRAPH
    p+=studioGraphVertex(aUV,vec3(p.x+aInstance.x+uWorldAnchor.x,p.y+aInstance.y,p.z+aInstance.z+uWorldAnchor.y),uTime);
#endif
    vPos=p+aInstance.xyz;
    vRelative=vPos+uOffset;
    vNormal=normalize(rotation*(poseNormal/axisScale));
    vTangent=normalize(rotation*(poseTangent.xyz*axisScale));
    if(uFullTransform)vTangent=normalize(vTangent-vNormal*dot(vNormal,vTangent));
    if(rootedGrass) {
        vNormal=normalize(rootedGrassNormal(vNormal,grassDerivative,grassUp));
        vTangent=rootedGrassTangent(vTangent,grassDerivative,grassUp);
        vTangent=normalize(vTangent-vNormal*dot(vTangent,vNormal));
    } else if(contacted||uVegetation==6||(weather_speed>.00001&&uWind>0.0&&uVegetation>0)) {
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
    vSeed=fract(sin(aYaw*37.17+aInstance.w*19.47)*4375.85);
    // Flat model-local phase decorrelates overlapping tiny shoots. It does not
    // depend on camera/origin shifts, wind time or instance-list ordering.
    vec3 q=fract(posePosition*.173+vec3(vSeed));
    q+=dot(q,q.yzx+19.19);
    vCoveragePhase=fract((q.x+q.y)*q.z);
    gl_Position=uVP*vec4(vRelative,1.);
}

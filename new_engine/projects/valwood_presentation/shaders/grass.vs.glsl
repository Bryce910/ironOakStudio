#version 330 core
layout(location=0)in float aT;
layout(location=1)in float aSide;
layout(location=2)in vec3 iPos;
layout(location=3)in float iHeight;
layout(location=4)in float iWidth;
layout(location=5)in float iLean;
layout(location=6)in float iPhase;
layout(location=7)in float iColorVar;
layout(location=8)in float iStyle;
layout(location=9)in float iTint;
uniform mat4 uProj,uView;
uniform vec3 uPlayerPos,uCamPos;
uniform float uTime,uFxTime,uWindStr,uGust,uBladeLean,uWindMul,uPushRadius,uPushStrength,uViewportHeight;
uniform vec2 uWindDir;
uniform int uTrailCount;
uniform vec4 uTrailPosRad[24];
uniform vec2 uTrailData[24];
out float vTip,vColorVar,vStyle,vTint,vSide,vFade,vCoverage;
out vec3 vWorld,vNormal,vAcross;
void main(){
    float t=aT,t2=t*t;
    vec2 direction=vec2(cos(iLean),sin(iLean)),across=vec2(-direction.y,direction.x);
    float wave=sin(dot(iPos.xz,vec2(.13,.09))-uFxTime*1.25);
    float flutter=sin(uTime*2.1+iPhase)*.17;
    float wind=uWindStr*uWindMul*(wave*.13+flutter*.035)+uGust*.07*sin(dot(iPos.xz,uWindDir)*.23-uFxTime*1.6);
    vec2 bend=direction*iHeight*(.40+.75*fract(iPhase*.413))*uBladeLean;
    bend+=uWindDir*(wind*iHeight);
    vec2 delta=iPos.xz-uPlayerPos.xz;float distanceToPlayer=length(delta);
    float press=1.0-smoothstep(.15,max(uPushRadius*.55,.3),distanceToPlayer);
    if(distanceToPlayer>.001)bend+=delta/distanceToPlayer*iHeight*press*uPushStrength*.24;
    if(distanceToPlayer<28.0)for(int i=0;i<uTrailCount;i++){
        vec2 d=iPos.xz-uTrailPosRad[i].xz;float len=length(d);
        float influence=(1.0-smoothstep(0.0,uTrailPosRad[i].w,len))*uTrailData[i].x*uTrailData[i].y;
        if(len>.001)bend+=d/len*iHeight*influence*.8;
    }
    float dist=length(iPos.xz-uCamPos.xz);
    vFade=1.0-smoothstep(44.0,62.0,dist);
    float height=iHeight*(1.0-press*.35);
    vec3 center=iPos+vec3(bend.x*t2,height*(t-.19*t2)-.018,bend.y*t2);
    // Preserve subpixel energy without turning distant blades into fat wedges.
    float depth=max(-(uView*vec4(center,1)).z,.05);
    float pixelWorld=2.0*depth/max(uProj[1][1]*uViewportHeight,1.0);
    float width=max(iWidth,pixelWorld*.72);
    vCoverage=min(iWidth/width,1.0);
    float taper=(.25+.75*smoothstep(0.0,.24,t))*pow(max(1.0-t,0.0),.65);
    vec3 axis=vec3(across.x,0,across.y);
    vec3 tangent=vec3(2.0*bend.x*t,height*(1.0-.38*t),2.0*bend.y*t);
    vec3 world=center+axis*(width*.65*taper*aSide);
    vNormal=normalize(cross(axis,tangent));vAcross=axis;
    vWorld=world;vTip=t;vColorVar=iColorVar;vStyle=iStyle;vTint=iTint;vSide=aSide;
    gl_Position=uProj*uView*vec4(world,1);
}

#version 330 core
in vec3 vNormal,vPosition;
in vec2 vUV;
in float vDepth;
uniform sampler2D uDepth,uMask;
uniform vec2 uResolution,uNearFar;
uniform vec3 uSun,uSunColor,uAmbient;
uniform vec4 uColor;
uniform float uFade;
uniform vec2 uSize,uMaskScale;
uniform float uPoolDiameter,uAge,uSeed;
uniform float uFloorY;
uniform int uMode;
layout(location=0)out vec4 FragColor;
float linearDepth(float d){return uNearFar.x*uNearFar.y/max(uNearFar.y-d*(uNearFar.y-uNearFar.x),.00001);}
void main(){
    float sceneDepth=linearDepth(texture(uDepth,gl_FragCoord.xy/uResolution).r);
    float clearance=sceneDepth-vDepth;
    if(clearance<=0.)discard;
    if(uMode==0&&vPosition.y<=uFloorY+.002)discard;
    // A stain must touch an opaque surface; it cannot float over empty sky or
    // span a gap merely because its floor quad is in front of the far plane.
    if(uMode==1&&clearance>.06+.003*vDepth)discard;
    vec3 n=length(vNormal)>.0001?normalize(vNormal):vec3(0,1,0);
    vec3 viewDir=length(vPosition)>.0001?normalize(-vPosition):vec3(0,0,1);
    float nl=max(0.,dot(n,uSun));
    vec3 illumination=uAmbient+uSunColor*(.12+.88*nl);
    vec3 pigment=vec3(uColor.r*.30,max(uColor.g*.30,uColor.r*.006),max(uColor.b*.30,uColor.r*.010));
    vec3 color=pigment*illumination;
    float alpha=uColor.a*uFade;
    if(uMode==1){
        float growth=.35+.65*smoothstep(0.,.65,uAge);
        vec2 local=(vUV-.5)*uSize;
        vec2 pool=local/(uPoolDiameter*growth*vec2(.5,.43));
        float angle=atan(pool.y,pool.x+.00001),radius=length(pool);
        float rim=1.+.10*sin(angle*3.+uSeed)+.06*sin(angle*7.-uSeed)+.035*sin(angle*11.+uSeed*2.);
        float puddle=1.-smoothstep(rim-.09,rim+.055,radius);
        float grain=fract(sin(dot(floor(local*80.),vec2(12.9898,78.233))+uSeed)*43758.5453);
        puddle*=.88+.12*grain;
        vec2 maskUV=(vUV-.5)*uMaskScale/(.72+.28*smoothstep(0.,.85,uAge))+.5;
        vec4 mask=texture(uMask,maskUV);
        if(any(lessThan(maskUV,vec2(0)))||any(greaterThan(maskUV,vec2(1))))mask=vec4(0);
        alpha*=max(mask.a*.82,puddle*.88);
        // Source mask splashes surround an irregular, growing dark wet center.
        // All highlights remain incident-light driven, including in darkness.
        color*=mix(mask.rgb*.50,vec3(.20,.16,.16),puddle)*(.70+.30*grain);
        vec3 halfDir=uSun+viewDir;
        if(length(halfDir)>.0001)color+=uSunColor*pow(max(0.,dot(n,normalize(halfDir))),160.)*.018*puddle;
    }else{
        vec3 halfDir=uSun+viewDir;
        if(length(halfDir)>.0001)color+=uSunColor*pow(max(0.,dot(n,normalize(halfDir))),96.)*.025*nl;
        alpha*=clamp(clearance/.025,0.,1.);
    }
    alpha=clamp(alpha,0.,.98);
    if(alpha<.003)discard;
    FragColor=vec4(max(color,vec3(0))*alpha,alpha);
}

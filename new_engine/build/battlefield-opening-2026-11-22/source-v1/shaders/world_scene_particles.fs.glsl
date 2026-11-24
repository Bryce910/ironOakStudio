#version 330 core
in vec2 vUV;
in vec3 vRelative;
flat in vec4 vMotion,vStyle,vTint;
layout(location=0) out vec4 FragColor;
uniform sampler2D uDepth,uShadow,uSmokeMask;
uniform bool uUseSmokeMask;
uniform vec2 uResolution,uNearFar;
uniform vec3 uForward,uSun,uSunColor,uSkyLight,uGroundLight,uFog;
uniform mat4 uLightVP;
uniform bool uHasShadow;
uniform float uTimePhase,uHaze;
float linear_depth(float d) {
    return uNearFar.x*uNearFar.y/max(uNearFar.y-d*(uNearFar.y-uNearFar.x),.00001);
}
float lattice(vec2 p) {
    uvec2 q=uvec2(mod(p,16.));
    uint n=q.x*0x1f123bb5u^q.y*0x5f356495u;
    n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;
    return float(n>>8)*(1./16777216.);
}
float noise(vec2 p) {
    vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
    return mix(mix(lattice(i),lattice(i+vec2(1,0)),f.x),mix(lattice(i+vec2(0,1)),lattice(i+vec2(1)),f.x),f.y);
}
float visibility(vec3 p) {
    if(!uHasShadow)return 1.;
    vec4 s=uLightVP*vec4(p,1);
    if(s.w<=0.)return 1.;
    vec3 uv=s.xyz/s.w*.5+.5;
    if(any(lessThan(uv,vec3(0)))||any(greaterThan(uv,vec3(1))))return 1.;
    vec2 texel=1./vec2(textureSize(uShadow,0));
    float lit=0.;
    // Four fixed taps avoid binary shadow popping across a broad smoke wisp.
    for(int y=0;y<2;y++)for(int x=0;x<2;x++)
        lit+=uv.z-.0012<=texture(uShadow,uv.xy+(vec2(x,y)-.5)*texel*1.5).r?1.:.08;
    return lit*.25;
}
void main() {
    float opaque=linear_depth(texture(uDepth,gl_FragCoord.xy/uResolution).r);
    float viewDepth=dot(vRelative,uForward),clearance=opaque-viewDepth;
    if(clearance<=0.||viewDepth<=uNearFar.x||vMotion.z<=0.)discard;
    float distance=length(vRelative),shape=0.;
    bool smoke=vStyle.x<.5;
    bool leaf=vStyle.x>3.5&&vStyle.x<4.5,firefly=vStyle.x>4.5;
    if(smoke) {
        float age=vMotion.y;
        // Noise advection repeats exactly at64seconds; octave frequency fades
        // before it becomes smaller than a pixel, avoiding distant shimmer.
        vec2 p=vUV*2.2+vec2(vMotion.w,uTimePhase*(16.*3./6.28318530718));
        float footprint=max(length(dFdx(p)),length(dFdy(p)));
        float coarse=mix(noise(p),.5,smoothstep(.7,2.,footprint));
        float fine=mix(noise(p*2.+vec2(2.7,-uTimePhase*(16.*2./6.28318530718))),.5,smoothstep(.35,1.,footprint));
        vec2 curl=vUV+vec2(coarse-.5,fine-.5)*.40;
        float radius=length(curl);
        float edge=1.-smoothstep(.48,1.,radius);
        shape=edge*edge*(.42+.58*coarse)*(.74+.26*fine);
        // Individual low-opacity layers reveal drifting internal structure.
        shape*=.75+.25*sin(age*3.14159265);
        if(uUseSmokeMask)shape*=texture(uSmokeMask,vUV*.5+.5+vec2(coarse-.5,fine-.5)*.04).a*1.8;
    } else if(leaf) {
        // Pointed leaf silhouette with a subtle central fold and vein. The
        // card is only a few centimetres wide; it has no luminous halo.
        float width=.48*pow(max(0.,1.-abs(vUV.y)),.65);
        float edge=abs(vUV.x)-width;
        shape=1.-smoothstep(-max(fwidth(edge),.015),max(fwidth(edge),.015),edge);
        shape*=1.-smoothstep(.90,1.,abs(vUV.y));
    } else {
        float r=length(vUV);
        float aa=max(fwidth(r),.08);
        shape=(1.-smoothstep(.35-aa,1.+aa,r));shape*=shape;
    }
    float nearFade=smoothstep(.35,1.35,distance);
    float alpha=shape*vMotion.z*vStyle.z*nearFade*clamp(clearance/(smoke?.65:.08),0.,1.);
    alpha=clamp(alpha,0.,smoke?.34:leaf?.85:firefly?.55:.20);
    if(alpha<.001)discard;
    vec3 ray=normalize(vRelative+vec3(.00001));
    float cosine=clamp(dot(ray,uSun),-1.,1.);
    float forward=pow(max(cosine,0.),smoke?4.:12.);
    float light=visibility(vRelative);
    vec3 ambient=mix(uGroundLight,uSkyLight,smoke?.79:.9);
    vec3 color=ambient*(smoke?.92:1.1)+uSunColor*light*(smoke?(.045+forward*.28):(.055+forward*.85));
    if(smoke)color+=vec3(.12,.046,.013)*vStyle.y*(1.-vMotion.y)*(1.-vMotion.y);
    if(leaf) {
        float fold=.76+.24*smoothstep(.0,.12,abs(vUV.x));
        float vein=1.-smoothstep(.012,.035,abs(vUV.x));
        color=(ambient*.90+uSunColor*light*(.18+forward*.35))*fold*(1.-vein*.12);
    }
    color=color*vTint.rgb+vTint.rgb*vTint.a;
    color*=smoke?(.78+.22*vMotion.y):1.;
    color=mix(max(color,vec3(0)),uFog,1.-exp(-distance*uHaze));
    // Alpha is both RGB coverage and blend input; the renderer's separate
    // alpha blend ZERO/ZERO marks only surviving moving fragments as unstable.
    FragColor=vec4(color*alpha,alpha);
}

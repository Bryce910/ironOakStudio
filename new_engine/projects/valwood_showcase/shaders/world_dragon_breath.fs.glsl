#version 330 core
in vec2 vUV;
in vec3 vRelative;
flat in vec4 vLife,vStyle;
layout(location=0) out vec4 FragColor;
uniform sampler2D uDepth,uSmoke;
uniform sampler2DArray uFire;
uniform bool uUseFire,uUseSmoke;
uniform vec2 uResolution,uNearFar;
uniform vec3 uForward,uFog;
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
void main() {
    float viewDepth=dot(vRelative,uForward);
    float clearance=linear_depth(texture(uDepth,gl_FragCoord.xy/uResolution).r)-viewDepth;
    if(clearance<=0.||viewDepth<=uNearFar.x||vLife.y<=0.)discard;
    float distance=length(vRelative),age=vLife.x,kind=vLife.w;
    float fade=smoothstep(.35,1.5,distance)*(1.-smoothstep(350.,500.,distance));
    fade*=clamp(clearance/max(vStyle.z,.05),0.,1.)*exp(-distance*uHaze);
    vec2 p=vUV*vec2(2.4,3.2)+vec2(vStyle.x,uTimePhase*(16.*7./6.28318530718));
    float footprint=max(length(dFdx(p)),length(dFdy(p)));
    float turbulent=mix(noise(p),.5,smoothstep(.6,1.7,footprint));
    float shape=0.;vec3 color=vec3(0);float alpha=0.;
    bool cold=vLife.z>.5;
    if(kind>1.5&&kind<2.5) {
        vec2 curl=vUV+vec2(turbulent-.5,noise(p*.63)-.5)*.35;
        shape=1.-smoothstep(.35,1.,length(curl));shape*=shape*(.6+.4*turbulent);
        if(uUseSmoke)shape*=texture(uSmoke,vUV*.5+.5).a*1.65;
        alpha=min(shape*vLife.y*fade,.24);
        color=cold?mix(vec3(.30,.39,.48),vec3(.68,.79,.86),age):mix(vec3(.095,.063,.042),vec3(.17,.18,.19),age);
        // Only the young throat-facing wisps receive orange/blue illumination.
        color+=(cold?vec3(.08,.30,.56):vec3(.48,.11,.012))*pow(1.-age,3.);
        color=mix(color,uFog,1.-exp(-distance*uHaze));
    } else if(kind>2.5) {
        float r=length(vUV),aa=max(fwidth(r),.08);
        shape=1.-smoothstep(.22-aa,.95+aa,r);shape*=shape;
        alpha=shape*vLife.y*fade;
        color=cold?vec3(1.8,5.0,8.):mix(vec3(10.,4.6,.65),vec3(3.,.15,.008),age);
    } else if(kind>.5) {
        float r=length(vUV*vec2(1.,.76));
        shape=1.-smoothstep(.12,1.,r);shape*=shape;
        shape*=.7+.3*turbulent;
        alpha=shape*vLife.y*fade;
        color=cold?vec3(1.6,4.4,5.4):vec3(5.2,2.5,.22);
    } else {
        if(uUseFire) {
            vec2 uv=vec2(vUV.x*.5+.5,vUV.y*.5+.5);
            float base=floor(vStyle.y),next=mod(base+1.,72.);
            vec3 a=texture(uFire,vec3(uv,base)).rgb;
            vec3 b=texture(uFire,vec3(uv,next)).rgb;
            vec3 art=mix(a,b,fract(vStyle.y));
            float energy=clamp(max(art.r,max(art.g,art.b)),0.,1.);
            float heat=clamp(art.g/max(art.r,.001),0.,1.);
            if(cold)color=energy*mix(vec3(.06,.55,2.6),vec3(.62,2.3,3.5),heat*heat);
            else color=energy*mix(vec3(2.7,.12,.002),vec3(4.,1.15,.04),heat*heat);
            // A low-energy continuous envelope connects the individual art
            // tongues, while retaining dark turbulent holes and orange edges.
            float body=1.-smoothstep(.24,1.,length(vUV*vec2(1.,.78)));
            body=body*body*(.5+.5*turbulent);
            color+=(cold?vec3(.035,.24,.55):vec3(.95,.042,.001))*body;
            color*=1.-age*.30;
            alpha=vLife.y*fade;
        } else {
            float warp=(turbulent-.5)*.4;
            float r=length(vec2(vUV.x+warp,vUV.y*.88));
            shape=1.-smoothstep(.28,1.,r);shape*=shape*(.65+.35*turbulent);
            alpha=shape*vLife.y*fade;
            float heat=clamp(1.-r-age*.55+turbulent*.20,0.,1.);
            color=cold?mix(vec3(.035,.5,2.3),vec3(.7,2.4,3.6),heat*heat):mix(vec3(2.7,.10,.002),vec3(4.2,1.2,.045),heat*heat);
        }
    }
    if(alpha<.001||max(color.r,max(color.g,color.b))*alpha<.001)discard;
    // Separate alpha blend reduces temporal confidence only on surviving
    // fragments; bright RGB is added in linear HDR before bloom/tonemapping.
    FragColor=vec4(color*alpha*vStyle.w,clamp(alpha*(kind>1.5&&kind<2.5?1.:3.),.0,.97));
}

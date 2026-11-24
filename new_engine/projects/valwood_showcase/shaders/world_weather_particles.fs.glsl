#version 330 core
in vec2 vParticleUV;
in float vViewDepth,vOpacity,vAge;
in vec3 vRelative;
flat in float vVariation;
flat in int vMode;
out vec4 FragColor;
uniform sampler2D uDepth,uShadow;
uniform vec2 uResolution,uNearFar;
uniform vec3 uSkyLight,uGroundLight,uSunColor,uSun;
uniform mat4 uLightVP;
uniform bool uHasShadow;
float linear_depth(float d) {
    return uNearFar.x*uNearFar.y/max(uNearFar.y-d*(uNearFar.y-uNearFar.x),.00001);
}
float sun_visibility(vec3 relative) {
    if(!uHasShadow)return 1.0;
    vec4 p=uLightVP*vec4(relative,1);
    if(p.w<=0.0)return 1.0;
    vec3 uv=p.xyz/p.w*.5+.5;
    if(any(lessThan(uv,vec3(0)))||any(greaterThan(uv,vec3(1))))return 1.0;
    return uv.z-.0015<=texture(uShadow,uv.xy).r?1.0:.10;
}
void main() {
    float distance=linear_depth(texture(uDepth,gl_FragCoord.xy/uResolution).r);
    float clearance=distance-vViewDepth;
    if(vOpacity<=.001||clearance<=0.0)discard;
    float shape;
    if(vMode==0) {
        shape=1.-smoothstep(.25,1.,length(vParticleUV));
    } else if(vMode==1) {
        float r=length(vParticleUV);
        float width=max(fwidth(r)*1.3,.065);
        shape=(1.0-smoothstep(width,width*2.0,abs(r-.77)))*(1.0-smoothstep(.88,1.0,r));
        // A faint inner pressure ring appears only during the initial impact.
        shape+=.25*(1.0-smoothstep(width,width*2.0,abs(r-.39)))*(1.0-smoothstep(.12,.45,vAge));
    } else {
        float x=vParticleUV.x,y=(vParticleUV.y+1.0)*.5;
        float w=max(fwidth(x)*1.3,.055);
        float lobe=.17+.47*pow(abs(x),.60)+.07*sin(x*19.0+vVariation*6.28);
        float rim=1.0-smoothstep(w,w*2.2,abs(y-lobe));
        float curtain=(1.0-smoothstep(lobe-.11,lobe+.04,y))*.12;
        // Analytic ballistic droplets leave the crown; no frame-dependent RNG.
        float dx=.28+vAge*.46;
        float dy=.22+vAge*1.45-vAge*vAge*1.38;
        float droplet=1.0-smoothstep(.04,.04+w,length(vec2((abs(x)-dx)*1.3,y-dy)));
        shape=(rim+curtain+droplet*.75)*(1.0-smoothstep(.76,1.0,abs(x)));
        shape*=1.0-smoothstep(.54,.97,vAge);
    }
    float opacity=shape*vOpacity*clamp(clearance/(vMode==0?.24:.065),0.0,1.0);
    if(opacity<.002)discard;
    vec3 ray=normalize(vRelative+vec3(.00001));
    float forward_scatter=pow(max(dot(ray,normalize(uSun+vec3(.00001))),0.0),6.0);
    float lit=sun_visibility(vRelative);
    vec3 color=vec3(.70,.81,.94)+uSkyLight*.3;
    color+=uSunColor*(.12+forward_scatter*.72)*lit;
    color*=.78+.35*vVariation;
    // Ordinary RGB coverage; separate alpha blending marks every surviving
    // fragment unstable, including the faint edge, so TAA cannot leave trails.
    FragColor=vec4(max(color,vec3(0)),clamp(opacity,0.0,.45));
}

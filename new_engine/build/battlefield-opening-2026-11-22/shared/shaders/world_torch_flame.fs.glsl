#version 330 core
in vec2 vUV;
in vec3 vRelative;
in float vWorldY;
flat in vec4 vShape;
flat in float vFlicker,vAge,vEnergy;
flat in vec3 vRootRelative;
flat in int vLayer;
layout(location=0) out vec4 FragColor;
uniform float uTimePhase,uPixelScale,uHaze,uClipHeight;
uniform bool uClipBelow,uUseSprites;
uniform sampler2DArray uSprites;
float lattice(vec2 p) {
    uvec2 q=uvec2(mod(p,16.));
    uint n=q.x*0x1f123bb5u^q.y*0x5f356495u;
    n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;
    return float(n>>8)*(1./16777216.);
}
float flow_noise(vec2 p) {
    vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
    return mix(mix(lattice(i),lattice(i+vec2(1,0)),f.x),
               mix(lattice(i+vec2(0,1)),lattice(i+vec2(1)),f.x),f.y)*2.-1.;
}
float tongue(float x,float y,float height,float center,float width) {
    float q=y/height;
    float radius=width*pow(max(1.-q,0.),.72)+.012;
    float radial=abs(x-center)/radius;
    float aa=max(fwidth(radial),.055);
    return (1.-smoothstep(1.-aa,1.+aa,radial))*(1.-smoothstep(.90,1.035,q));
}
void main() {
    float distance=length(vRootRelative);
    float projected=vShape.x*uPixelScale/max(distance,.1);
    float visible=smoothstep(.8,2.5,projected)*(1.-smoothstep(230.,300.,distance));
    float opacity=0.;vec3 emission=vec3(0);
    if(vLayer<4) {
        float layerWeight=vLayer==0?.40:(vLayer==1?.34:(vLayer==2?.29:.25));
        if(uUseSprites) {
            // Purchased72-frame sheets, interpolated at23.625fps.21complete
            // loops in64s preserve the analytic effect's seamless time wrap.
            // Array mips cannot bleed into adjacent animation frames.
            float frame=mod(uTimePhase*(72.*21./6.28318530718)+vShape.w*11.4591559+float(vLayer)*17.25,72.);
            float base=vShape.y/vShape.x>.42?72.:0.;
            vec2 uv=vec2(vUV.x*.5+.5,vUV.y);
            vec3 a=texture(uSprites,vec3(uv,base+floor(frame))).rgb;
            vec3 b=texture(uSprites,vec3(uv,base+mod(floor(frame)+1.,72.))).rgb;
            emission=mix(a,b,fract(frame))*8.1;
            opacity=layerWeight*visible*.85;
            if(max(emission.r,max(emission.g,emission.b))*opacity<.002)discard;
        } else {
            float phase=vShape.w+float(vLayer)*2.17;
            float heightScale=1.+.2*(vFlicker-1.);
            float y=vUV.y*1.35/heightScale;
            // Smooth lattice turbulence moves up through a16-cell periodic field.
            // Its integer advection cycles and sine curls meet continuously at
            // the shared64-second phase wrap. Fine detail fades before aliasing.
            vec2 coarse=vec2(vUV.x*2.6+phase,y*4.6-uTimePhase*(16.*14./6.28318530718));
            vec2 fine=vec2(vUV.x*5.1+phase*.37,y*9.2-uTimePhase*(16.*23./6.28318530718));
            float coarseFoot=max(length(dFdx(coarse)),length(dFdy(coarse)));
            float fineFoot=max(length(dFdx(fine)),length(dFdy(fine)));
            float wisp=flow_noise(coarse)*(1.-smoothstep(.6,1.8,coarseFoot))*.72+
                       flow_noise(fine)*(1.-smoothstep(.6,1.8,fineFoot))*.28;
            float curl=(sin(uTimePhase*79.+y*3.7+phase)*.18+
                        sin(uTimePhase*113.-y*5.1+phase*.71)*.12)*y*y;
            float bend=curl+wisp*.11*y;
            float widthWave=1.+wisp*.29;
            float body=tongue(vUV.x,y,.86+.055*sin(uTimePhase*97.+phase),bend,.60*widthWave);
            float left=tongue(vUV.x,y,1.07+.11*sin(uTimePhase*121.+phase),
                bend-.27*(1.-y*.48)+.11*wisp,.235*widthWave);
            float right=tongue(vUV.x,y,1.01+.12*sin(uTimePhase*103.+phase*.71),
                bend+.24*(1.-y*.45)-.08*wisp,.215*widthWave);
            float edge=clamp(max(body,(left+right)*.78),0.,1.);
            float ends=smoothstep(0.,.045,y);
            float heat=clamp((1.-abs(vUV.x-bend)/.68)*.91-y*.42+wisp*.19,0.,1.);
            // Orange transparent fringes surround a compact yellow hot core.
            // Four weights sum1.28, below the previous three-sheet sum1.37.
            emission=mix(vec3(2.65,.17,.006),vec3(8.1,4.65,1.16),heat*heat);
            opacity=edge*ends*(.73+.27*wisp)*layerWeight*visible;
        }
    } else {
        vec2 q=vec2(vUV.x,vUV.y*2.-1.);
        float radial=length(q),aa=max(fwidth(radial),.08);
        float edge=1.-smoothstep(.48-aa,.93+aa,radial);
        float life=smoothstep(0.,.12,vAge)*(1.-smoothstep(.46,1.,vAge));
        float variation=.75+.25*sin(vShape.w+float(vLayer)*2.37);
        opacity=edge*life*.235*variation*visible*vEnergy*(1.-smoothstep(28.,65.,distance));
        emission=mix(vec3(8.,3.8,.48),vec3(2.9,.15,.006),vAge);
    }
    opacity*=exp(-distance*uHaze);
    if(uClipBelow&&vWorldY<uClipHeight)discard;
    if(opacity<.002)discard;
    // Animated emissive pixels cannot borrow static-wall/foliage history.
    // RGB remains smoothly premultiplied; alpha is confidence metadata and
    // every written destination stays outside foliage's[.4,.99) source band.
    FragColor=vec4(emission*opacity*vShape.z*vFlicker,max(.70,clamp(opacity*1.8,0.,.98)));
}

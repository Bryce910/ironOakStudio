#version 330 core
// AAA merge: temporal upscaler (TAAU) at display resolution.
// raw    = this frame's jittered render-resolution color (before the temporal resolve)
// stable = render-resolution temporal resolve (anti-aliased, unjittered)
// depth  = resolved (unjittered) depth at render resolution
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uRaw,uStable,uDepth,uHistory,uObjectMotion;
uniform bool uHasHistory,uHasObjectMotion;
uniform mat4 uReprojection;
uniform vec2 uInSize,uOutSize,uJitterUV,uPrevJitterUV;

vec3 to_ycocg(vec3 c){return vec3(dot(c,vec3(.25,.5,.25)),.5*(c.r-c.b),.5*c.g-.25*(c.r+c.b));}
vec3 from_ycocg(vec3 c){return vec3(c.x+c.y-c.z,c.x+c.z,c.x-c.y-c.z);}
vec3 tonemap(vec3 c){return c/(1.+max(max(c.r,c.g),c.b));}           // reversible weighting space
vec3 untonemap(vec3 c){return c/max(1.-max(max(c.r,c.g),c.b),1e-4);}

// 5-tap Catmull-Rom (bilinear-folded) reconstruction.
vec3 catmull_rom(sampler2D tex,vec2 uv,vec2 size){
    vec2 p=uv*size,c=floor(p-.5)+.5,f=p-c,f2=f*f,f3=f2*f;
    vec2 w0=-.5*f3+f2-.5*f,w1=1.5*f3-2.5*f2+1.,w2=-1.5*f3+2.*f2+.5*f,w3=.5*f3-.5*f2;
    vec2 w12=w1+w2,t0=(c-1.)/size,t3=(c+2.)/size,t12=(c+w2/w12)/size;
    vec3 r=texture(tex,vec2(t12.x,t0.y)).rgb*(w12.x*w0.y)+texture(tex,vec2(t0.x,t12.y)).rgb*(w0.x*w12.y)+
           texture(tex,t12).rgb*(w12.x*w12.y)+texture(tex,vec2(t3.x,t12.y)).rgb*(w3.x*w12.y)+
           texture(tex,vec2(t12.x,t3.y)).rgb*(w12.x*w3.y);
    float n=w12.x*w0.y+w0.x*w12.y+w12.x*w12.y+w3.x*w12.y+w12.x*w3.y;
    return max(r/max(n,1e-4),vec3(0));
}
vec3 clip_box(vec3 c,vec3 lo,vec3 hi){
    vec3 center=.5*(hi+lo),extent=.5*(hi-lo)+1e-4,d=c-center;
    vec3 a=abs(d/extent);float m=max(a.x,max(a.y,a.z));
    return m>1.?center+d/m:c;
}

void main(){
    vec2 uv=vUV;
    vec3 stable=catmull_rom(uStable,uv,uInSize);

    // Neighbourhood statistics of the stable image (render resolution).
    ivec2 isize=ivec2(uInSize);
    ivec2 base=clamp(ivec2(uv*uInSize),ivec2(0),isize-1);
    vec3 m1=vec3(0),m2=vec3(0),lo=vec3(1e9),hi=vec3(-1e9);
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
        vec3 s=to_ycocg(tonemap(max(texelFetch(uStable,clamp(base+ivec2(x,y),ivec2(0),isize-1),0).rgb,vec3(0))));
        m1+=s;m2+=s*s;lo=min(lo,s);hi=max(hi,s);
    }
    m1/=9.;vec3 sigma=sqrt(max(m2/9.-m1*m1,vec3(0)));
    vec3 box_lo=max(lo,m1-1.25*sigma),box_hi=min(hi,m1+1.25*sigma);

    // Raw jittered sample nearest this display pixel, weighted by its distance
    // in DISPLAY pixels from the display pixel centre.
    vec2 raw_pos=(uv+uJitterUV)*uInSize-.5;
    ivec2 ri=clamp(ivec2(floor(raw_pos+.5)),ivec2(0),isize-1);
    vec2 d=(raw_pos-vec2(ri))*(uOutSize/uInSize);
    float w=exp(-2.0*dot(d,d));
    // Reactive actors do not have complete per-NPC motion/depth history.
    // Keep the already antialiased resolve on their silhouettes instead of
    // injecting a nearest-neighbour raw sample into a second temporal history.
    // Dilate by one render texel to cover skin/cloth edges against the scenery.
    float confidence=texelFetch(uRaw,ri,0).a;
    confidence=min(confidence,texelFetch(uRaw,clamp(ri+ivec2(1,0),ivec2(0),isize-1),0).a);
    confidence=min(confidence,texelFetch(uRaw,clamp(ri+ivec2(-1,0),ivec2(0),isize-1),0).a);
    confidence=min(confidence,texelFetch(uRaw,clamp(ri+ivec2(0,1),ivec2(0),isize-1),0).a);
    confidence=min(confidence,texelFetch(uRaw,clamp(ri+ivec2(0,-1),ivec2(0),isize-1),0).a);
    if(confidence<.4){FragColor=vec4(stable,1.);return;}
    vec3 raw=to_ycocg(tonemap(max(texelFetch(uRaw,ri,0).rgb,vec3(0))));
    raw=clip_box(raw,lo,hi);   // stochastic cutout/dither stays inside the local range

    if(!uHasHistory){FragColor=vec4(stable,1.);return;}

    float depth=texture(uDepth,uv).r;
    vec4 clip=vec4((uv+uJitterUV)*2.-1.,depth*2.-1.,1.);
    vec4 prev=uReprojection*clip;
    if(prev.w<=1e-6){FragColor=vec4(stable,1.);return;}
    vec3 prev_ndc=prev.xyz/prev.w;
    if(uHasObjectMotion&&depth<1.){
        vec4 om=texelFetch(uObjectMotion,base,0);
        if(om.a>.5)prev_ndc+=om.xyz;
    }
    vec2 prev_uv=prev_ndc.xy*.5+.5-uPrevJitterUV;
    vec2 half_texel=.5/uOutSize;
    if(any(lessThan(prev_uv,half_texel))||any(greaterThan(prev_uv,1.-half_texel))){FragColor=vec4(stable,1.);return;}

    vec3 history=to_ycocg(tonemap(catmull_rom(uHistory,prev_uv,uOutSize)));
    history=clip_box(history,box_lo,box_hi);

    float speed=length((prev_uv-uv)*uOutSize);
    float alpha=.035+.16*w;                       // ~0.85-0.95 history
    alpha=mix(alpha,.5,smoothstep(8.,40.,speed)); // fast motion: lean on the new frame
    vec3 accumulated=from_ycocg(mix(history,raw,alpha));
    vec3 result=untonemap(clamp(accumulated,0.,.999));
    // Keep a little of the stable reconstruction so edges never drift.
    result=mix(result,stable,.06+.3*smoothstep(16.,64.,speed));
    FragColor=vec4(max(result,vec3(0)),1.);
}

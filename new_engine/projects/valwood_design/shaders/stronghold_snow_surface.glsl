// Private Grandstone terrain finish. All coordinates are district-local to
// preserve fine detail 43km from atlas origin. Derivative-filtered grain avoids
// sparkling pixels; the large banks themselves are authored collision terrain.
uniform sampler2D uSnowImprints;
uniform bool uSnowImprintsReady=false;
uniform vec4 uSnowImprintBounds;
uniform mat4 uVP;
float snow_imprint_reactive=0.;
void stronghold_boot_imprint(vec3 geometry,inout vec3 base,inout float ao,inout vec3 N) {
    if(!uSnowImprintsReady||geometry.y<.55)return;
    vec2 uv=((vPos.xz-uSnowImprintBounds.zw)-uSnowImprintBounds.xy)/32.;
    if(any(lessThan(uv,vec2(.015)))||any(greaterThan(uv,vec2(.985))))return;
    // Follow the camera ray into the shallow height field. Keep the offset
    // bounded at grazing angles and on banks above the camera.
    vec2 ray=vRelative.y<-.18?vRelative.xz/vRelative.y:vec2(0);
    ray=clamp(ray,vec2(-8),vec2(8));
    float h=texture(uSnowImprints,uv).r;
    vec2 offset=ray*h;
    for(int i=0;i<2;i++) {
        h=texture(uSnowImprints,uv+offset/32.).r;
        offset=ray*h;
    }
    vec2 q=uv+offset/32.;
    vec2 step_uv=vec2(1./1024.,0);
    float hx=texture(uSnowImprints,q+step_uv).r-texture(uSnowImprints,q-step_uv).r;
    float hz=texture(uSnowImprints,q+step_uv.yx).r-texture(uSnowImprints,q-step_uv.yx).r;
    vec2 gradient=vec2(hx,hz)/.0625;
    N=normalize(N+vec3(-gradient.x,0.,-gradient.y));
    float depression=clamp(-h/.038,0.,1.);
    base*=1.-depression*.09;
    ao*=1.-depression*.20;
    if(abs(h)>.0001) {
        vec4 clip=uVP*vec4(vRelative+vec3(offset.x,h,offset.y),1);
        gl_FragDepth=clamp(clip.z/clip.w*.5+.5,0.,1.);
        snow_imprint_reactive=clamp(abs(h)*32.,0.,1.);
    }
}
void stronghold_snow_surface(vec3 world,vec3 geometry,float distance_to_eye,
    inout vec3 base,inout float rough,inout float ao,inout vec3 N) {
    vec2 p=world.xz-vec2(-43344.,-23987.82415651827);
    float region=1.-smoothstep(1800.,2800.,length(p));
    float hall=step(abs(p.x),32.5)*step(-121.,p.y)*step(p.y,12.);
    if(region<=0.||hall>.5)return;
    vec2 dx=dFdx(p),dy=dFdy(p);
    float footprint=max(length(dx),length(dy));
    float near_detail=1.-smoothstep(45.,130.,distance_to_eye);
    float macro=noise(p*.049+19.7),meso=noise(p*.21-3.2);
    float drift=max(0.,world.y-1130.80872);
    float path_center=sin(p.y*.043)*.54;
    float path=1.-smoothstep(1.8+meso*.25,2.8+macro*.45,abs(p.x-path_center));
    path*=smoothstep(12.,22.,p.y);
    float blend=smoothstep(.2,.8,noise(p*.073));
    vec2 uv=vec2(p.x,-p.y)*.29;
    vec3 scan=mix(textureGrad(uGrass,uv,dx*vec2(.29,-.29),dy*vec2(.29,-.29)).rgb,
        textureGrad(uGrass,uv+vec2(.371,.637),dx*vec2(.29,-.29),dy*vec2(.29,-.29)).rgb,blend);
    vec3 mapped=mix(textureGrad(uGrassNormal,uv,dx*vec2(.29,-.29),dy*vec2(.29,-.29)).xyz,
        textureGrad(uGrassNormal,uv+vec2(.371,.637),dx*vec2(.29,-.29),dy*vec2(.29,-.29)).xyz,blend)*2.-1.;
    // Powder keeps its measured scan structure with restrained albedo contrast;
    // depressions and granular crust read through normals, not painted shadows.
    float snow_grain=dot(scan,vec3(.2126,.7152,.0722));
    vec3 powder=vec3(.74,.81,.88)*(.94+.07*macro+.055*(snow_grain-.5));
    vec3 compacted=powder*vec3(.79,.84,.89)*(.96+.08*meso);
    vec3 snow_base=mix(powder,compacted,path*.78);
    float wind=noise(vec2(p.x*.15+p.y*.035,p.y*.13-p.x*.04));
    float exposure=smoothstep(.50,.69,wind*.62+meso*.38);
    exposure*=smoothstep(13.,24.,abs(p.x))*(1.-smoothstep(.5,1.8,drift));
    exposure*=smoothstep(14.,22.,p.y);
    // Broken patches of frozen gravel, with fine snow caught in the crevices.
    vec3 gravel=textureGrad(uDirt,vec2(p.x,-p.y)*.38,dx*vec2(.38,-.38),dy*vec2(.38,-.38)).rgb;
    float gravel_luma=dot(gravel,vec3(.2126,.7152,.0722));
    gravel=mix(vec3(gravel_luma)*vec3(.58,.65,.72),gravel*.64,.25);
    float granular=smoothstep(.16,.42,gravel_luma);
    vec3 scoured=mix(gravel,powder,granular*.37);
    base=mix(base,mix(snow_base,scoured,exposure*.88),region);
    rough=mix(rough,mix(mix(.88,.63,path),.94,exposure),region);
    ao=mix(ao,mix(.99,.86,exposure),region);
    // Wind-cut centimetre ridges fade at their actual pixel footprint. The
    // slope is an analytic derivative of height, not a world-up normal replace.
    vec2 wind_axis=normalize(vec2(.94,.342));
    float phase=dot(p,wind_axis)*6.2+(noise(p*.24)-.5)*6.0+sin(dot(p,vec2(-.342,.94))*.53)*1.15;
    float ridge_filter=1.-smoothstep(.035,.21,footprint);
    float ridge_patch=smoothstep(.25,.76,noise(p*.34+11.3));
    float ridge_slope=cos(phase)*.065*ridge_patch*ridge_filter*(1.-path*.85)*(1.-exposure);
    vec2 small_slope=wind_axis*ridge_slope;
    float fine_filter=1.-smoothstep(.004,.03,footprint);
    small_slope+=vec2(sin(p.x*157.+p.y*39.),cos(p.y*173.-p.x*43.))*.045*fine_filter;
    vec3 tangent=normalize(vec3(geometry.y,-geometry.x,0.));
    vec3 bitangent=normalize(cross(tangent,geometry));
    vec2 detail_slope=vec2(mapped.x,-mapped.y)*mix(.16,.36,path)*near_detail;
    detail_slope-=small_slope;
    vec3 normal=normalize(geometry+tangent*detail_slope.x+bitangent*detail_slope.y);
    N=normalize(mix(N,normal,region));
    float basin=length((p-vec2(63.,32.))/vec2(13.,10.));
    float frozen=(1.-smoothstep(.82,1.08,basin))*smoothstep(13.,18.,p.y);
    if(frozen>0.) {
        float cracks=1.;
        // Thin world-fixed fractures, softened to their screen footprint.
        for(int i=0;i<4;i++) {
            float a=float(i)*.79+.21;vec2 axis=vec2(cos(a),sin(a));
            float line=abs(dot(p-vec2(63.,32.),axis)+sin(dot(p,axis.yx)*.7+float(i))*.43+(noise(p*1.7+float(i))-.5)*.19-float(i-2)*1.9);
            cracks=min(cracks,smoothstep(.008,.025+footprint*.5,line));
        }
        vec3 ice_color=mix(vec3(.22,.36,.42),vec3(.09,.19,.25),macro);
        float frost=smoothstep(.53,.76,meso)*.45+smoothstep(.55,1.05,basin)*.32;
        ice_color=mix(ice_color,vec3(.55,.66,.72),frost);
        ice_color=mix(ice_color+vec3(.11,.14,.15),ice_color,cracks);
        base=mix(base,ice_color,frozen);rough=mix(rough,.21,frozen);
        N=normalize(mix(N,geometry+vec3(mapped.x,0.,-mapped.y)*.075,frozen));
    }
    if(frozen<.1)stronghold_boot_imprint(geometry,base,ao,N);
}

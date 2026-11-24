// All color inputs and output are linear; presentation performs the only tone map.
const float PI=3.14159265359;
vec3 pbrBRDF(vec3 base,float metal,float rough,vec3 N,vec3 V,vec3 L){
    vec3 H=normalize(V+L);float nl=max(dot(N,L),0.0),nv=max(dot(N,V),0.001),nh=max(dot(N,H),0.0);
    float a=rough*rough,a2=a*a,d=nh*nh*(a2-1.0)+1.0;
    float D=a2/max(PI*d*d,0.0001),k=(rough+1.0)*(rough+1.0)/8.0;
    float G=nl/(nl*(1.0-k)+k)*nv/(nv*(1.0-k)+k);
    vec3 F0=mix(vec3(.04),base,metal),F=F0+(1.0-F0)*pow(1.0-max(dot(H,V),0.0),5.0);
    return ((1.0-F)*(1.0-metal)*base/PI+D*G*F/max(4.0*nl*nv,.001))*nl;
}
float pbrShadowMap(sampler2D map,vec3 p,float bias){
    vec2 texel=1.0/vec2(textureSize(map,0));float s=0.0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)
        s+=step(p.z-bias,texture(map,p.xy+vec2(x,y)*texel*1.35).r);
    return s/9.0;
}
float pbrVisibility(vec3 pos,vec3 normal){
    vec4 q=uLightVP*vec4(pos+normal*.035,1);vec3 p=q.xyz/q.w*.5+.5;
    float edge=min(min(p.x,p.y),min(1.0-p.x,1.0-p.y));
    bool valid=p.z>0.0&&p.z<1.0&&edge>.005;
    float near=valid?pbrShadowMap(uShadowMap,p,.00014):1.0;
    // Most visible fragments are entirely within the near cascade.
    if(valid&&edge>=.075)return near;
    vec4 qw=uLightVPWide*vec4(pos+normal*.09,1);vec3 pw=qw.xyz/qw.w*.5+.5;
    float wide=1.0;
    if(pw.z>0.0&&pw.z<1.0&&all(greaterThan(pw.xy,vec2(.005)))&&all(lessThan(pw.xy,vec2(.995))))wide=pbrShadowMap(uShadowMapWide,pw,.00025);
    return valid?mix(wide,near,smoothstep(.01,.075,edge)):wide;
}
vec3 pbrLighting(vec3 base,float metal,float rough,float ao,vec3 pos,vec3 N,vec3 V,float foliage){
    vec3 L=normalize(-uSunDir);
    // Visibility only modulates directional lighting. Back-facing opaque
    // surfaces have no such contribution; thin foliage still transmits it.
    float vis=(dot(N,L)>0.0||foliage!=0.0)?pbrVisibility(pos,N):1.0;
    vec3 sky=mix(uAmbient*vec3(.55,.48,.37),uAmbient*1.65,clamp(N.y*.5+.5,0.0,1.0));
    vec3 col=base*sky*ao*(1.0-metal);
    // Broad sky response, an approximation until reflection probes are baked.
    float fresnel=pow(1.0-max(dot(N,V),0.0),5.0);
    col+=mix(vec3(.04),base,metal)*uAmbient*(.35+.65*fresnel)*(1.0-rough*.6)*ao;
    col+=pbrBRDF(base,metal,rough,N,V,L)*uSunColor*PI*vis;
    // Thin leaf/grass transmission lets low sunlight pass through the canopy.
    col+=base*uSunColor*foliage*(.10+.30*pow(max(dot(V,-L),0.0),3.0))*vis;
    for(int i=0;i<uNumLights;i++){
        vec3 delta=uLightPos[i]-pos;float dist=length(delta);
        if(dist>=uLightRadius[i])continue;
        float att=pow(1.0-dist/max(uLightRadius[i],.001),2.0);
        vec3 direction=delta/max(dist,.001);
        if(uLightParams[i].x>.5)att*=smoothstep(uLightParams[i].z,uLightParams[i].y,dot(normalize(uLightDir[i]),-direction));
        if(att<=0.0)continue;
        col+=pbrBRDF(base,metal,rough,N,V,direction)*uLightColor[i]*PI*att;
    }
    if(uToonShading){float diff=max(dot(N,L),0.0)*vis;diff=ceil(diff*max(uToonBands,1.0))/max(uToonBands,1.0);col=base*(sky+uSunColor*diff);}
    return col;
}

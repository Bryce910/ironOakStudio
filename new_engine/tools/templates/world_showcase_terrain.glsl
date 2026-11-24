// Private source-showcase terrain: exact eight painted channels, source metre
// tiling and PBR maps. The old atlas material samplers become inactive.
uniform bool uShowcaseReady=false;
uniform sampler2DArray uShowcaseAlbedo,uShowcaseSurface,uShowcaseWeights;
uniform sampler2D uShowcaseHoles;
uniform bool uShowcaseHolesReady=false;
uniform vec4 uShowcaseBounds,uShowcaseUV[8];
uniform float uShowcaseNormalScale[8];
#ifdef VALWOOD_HDR_ENVIRONMENT
uniform samplerCube uEnvironmentCube;
#endif
uniform sampler2D uSnowImprints;
uniform bool uSnowImprintsReady=false;
uniform vec4 uSnowImprintBounds;
uniform mat4 uVP;
float showcase_imprint_reactive=0.;

void showcase_imprint(vec3 geometry,inout vec3 base,inout float ao,inout vec3 N) {
    if(!uSnowImprintsReady||geometry.y<.55)return;
    vec2 uv=((vPos.xz-uSnowImprintBounds.zw)-uSnowImprintBounds.xy)/32.;
    if(any(lessThan(uv,vec2(.015)))||any(greaterThan(uv,vec2(.985))))return;
    vec2 ray=vRelative.y<-.18?vRelative.xz/vRelative.y:vec2(0);
    ray=clamp(ray,vec2(-8),vec2(8));
    float h=texture(uSnowImprints,uv).r;vec2 offset=ray*h;
    for(int i=0;i<2;i++){h=texture(uSnowImprints,uv+offset/32.).r;offset=ray*h;}
    vec2 q=uv+offset/32.,step_uv=vec2(1./1024.,0);
    float hx=texture(uSnowImprints,q+step_uv).r-texture(uSnowImprints,q-step_uv).r;
    float hz=texture(uSnowImprints,q+step_uv.yx).r-texture(uSnowImprints,q-step_uv.yx).r;
    N=normalize(N+vec3(-hx,0,-hz)/.0625);
    float depression=clamp(-h/.038,0.,1.);base*=1.-depression*.09;ao*=1.-depression*.20;
    if(abs(h)>.0001){
        vec4 clip=uVP*vec4(vRelative+vec3(offset.x,h,offset.y),1);
        gl_FragDepth=clamp(clip.z/clip.w*.5+.5,0.,1.);showcase_imprint_reactive=clamp(abs(h)*32.,0.,1.);
    }
}

vec3 showcase_brdf(vec3 base,float rough,float metal,vec3 N,vec3 V,vec3 L) {
    float nl=max(dot(N,L),0.);if(nl<=0.)return vec3(0);
    vec3 sum=V+L,H=sum*inversesqrt(max(dot(sum,sum),.0001));
    float nv=max(dot(N,V),.001),nh=max(dot(N,H),0.),vh=max(dot(V,H),0.);
    float a=rough*rough,a2=a*a,d=nh*nh*(a2-1.)+1.,k=(rough+1.)*(rough+1.)*.125;
    vec3 f0=mix(vec3(.04),base,metal),F=f0+(1.-f0)*pow(1.-vh,5.);
    float D=a2/max(PI*d*d,.0001),G=nl/(nl*(1.-k)+k)*nv/(nv*(1.-k)+k);
    return ((1.-F)*base*(1.-metal)/PI+D*G*F/max(4.*nl*nv,.001))*nl;
}

vec3 showcase_practicals(vec3 base,float rough,float metal,vec3 N,vec3 V) {
    if(uTorchCount<=0||any(lessThan(vPos,uTorchClusterMin))||any(greaterThan(vPos,uTorchClusterMax)))return vec3(0);
    vec3 result=vec3(0);
    for(int i=0;i<8;i++){
        if(i>=uTorchCount)break;vec3 delta=uTorchPositionRadius[i].xyz-vPos;
        float attenuation=torch_attenuation(i,vPos,delta);if(attenuation<=0.)continue;
        vec3 L=delta*inversesqrt(max(dot(delta,delta),.000001));
        result+=showcase_brdf(base,rough,metal,N,V,L)*uTorchColorIntensity[i].rgb*uTorchColorIntensity[i].w*attenuation;
    }
    return result;
}

void main() {
    gl_FragDepth=gl_FragCoord.z;
    vec2 p=vPos.xz-uShowcaseBounds.xy,dx=dFdx(p),dy=dFdy(p);
    vec2 alpha_uv=p*uShowcaseBounds.zw;
    bool inside=uShowcaseReady&&all(greaterThanEqual(alpha_uv,vec2(0)))&&all(lessThanEqual(alpha_uv,vec2(1)));
    if(inside&&uShowcaseHolesReady){
        ivec2 size=textureSize(uShowcaseHoles,0);
        ivec2 cell=clamp(ivec2(floor(alpha_uv*vec2(size))),ivec2(0),size-1);
        if(texelFetch(uShowcaseHoles,cell,0).r<.5)discard;
    }
    vec2 alpha_size=vec2(textureSize(uShowcaseWeights,0).xy);
    vec2 alpha_sample=(clamp(alpha_uv,0.,1.)*(alpha_size-1.)+.5)/max(alpha_size,vec2(1));
    vec4 low=inside?texture(uShowcaseWeights,vec3(alpha_sample,0)):vec4(0);
    vec4 high=inside?texture(uShowcaseWeights,vec3(alpha_sample,1)):vec4(0);
    float total=dot(low+high,vec4(1));
    vec3 geometry=normalize(vNormal),N=geometry;
    vec3 base=vec3(.24,.27,.29),detail_normal=vec3(0),arm=vec3(0);
    float rough=.86,metal=0.,ao=1.,snow=0.;
    if(total>.0001){
        low/=total;high/=total;snow=high.w;base=vec3(0);float used=0.;
        for(int i=0;i<8;i++){
            float weight=i<4?low[i]:high[i-4];if(weight<.001)continue;
            vec4 transform=uShowcaseUV[i];vec2 uv=p*transform.xy+transform.zw;
            vec2 uv_dx=dx*transform.xy,uv_dy=dy*transform.xy;
            base+=textureGrad(uShowcaseAlbedo,vec3(uv,float(i)),uv_dx,uv_dy).rgb*weight;
            vec3 mapped=textureGrad(uShowcaseSurface,vec3(uv,float(i*2)),uv_dx,uv_dy).xyz*2.-1.;
            mapped.xy*=uShowcaseNormalScale[i];detail_normal+=mapped*weight;
            arm+=textureGrad(uShowcaseSurface,vec3(uv,float(i*2+1)),uv_dx,uv_dy).rgb*weight;used+=weight;
        }
        base/=max(used,.0001);arm/=max(used,.0001);
        // Source texture V follows Unity+Z, therefore engine-Z. Keep its raw
        // tangent green with T=engine+X and B=engine-Z on a flat surface.
        vec3 tangent=vec3(geometry.y,-geometry.x,0);
        tangent=dot(tangent,tangent)>.000001?normalize(tangent):vec3(1,0,0);
        vec3 bitangent=normalize(cross(geometry,tangent));
        N=normalize(mat3(tangent,bitangent,geometry)*normalize(detail_normal));
        ao=clamp(arm.r,0.,1.);rough=clamp(arm.g,.06,1.);metal=clamp(arm.b,0.,1.);
    }
    if(uClipBelowWater&&vPos.y<(uWaterClipLevel==0.0?.2:uWaterClipLevel))discard;
    if(snow>.5)showcase_imprint(geometry,base,ao,N);
    editor_surface(vPos,base,rough);
    vec3 V=normalize(-vRelative);
    float hemisphere=clamp(.5+.5*geometry.y,0.,1.);
    float access=sky_visibility(geometry);
    vec3 sky_light=uSkyLight,ground_light=uGroundLight;
    vec3 ambient=mix(ground_light,sky_light*access,hemisphere);
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.)ambient=mix(ambient,environment_diffuse(geometry)*mix(1.,access,hemisphere),uEnvironmentStrength);
#endif
    ambient=local_diffuse(vPos,geometry,ambient);
    vec3 reflection=reflect(-V,geometry);
    vec3 radiance=mix(ground_light,sky_light,clamp(.5+.5*reflection.y,0.,1.));
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.)radiance=mix(radiance,max(textureLod(uEnvironmentCube,environment_direction(reflection),rough*uEnvironmentMip).rgb,vec3(0))*uEnvironmentIntensity*access,uEnvironmentStrength);
#endif
    vec3 f0=mix(vec3(.04),base,metal);
    vec3 fresnel=f0+(max(vec3(1.-rough),f0)-f0)*pow(1.-clamp(dot(geometry,V),0.,1.),5.);
    vec3 indirect=(base*ambient*(1.-fresnel)*(1.-metal)+radiance*fresnel*(1.-.45*rough))*ao;
    float direct_visibility=min(visibility(geometry),macro_visibility());
    vec3 color=indirect+showcase_brdf(base,rough,metal,N,V,uSun)*uSunColor*PI*direct_visibility*cloud_transmittance();
    color+=showcase_practicals(base,rough,metal,N,V);
    float distance_to_eye=length(vRelative),density=uHaze*mix(1.12,.48,smoothstep(60.,1900.,vPos.y));
    float fog=max(1.-exp(-distance_to_eye*density),smoothstep(10500.,14900.,distance_to_eye));
    float scatter=pow(max(dot(-V,uSun),0.),8.);vec3 aerial=uFog+uSunColor*scatter*.035;
    float fog_amount=clamp(fog,0.,1.);
    FragColor=vec4(mix(color,aerial,fog_amount),1.-showcase_imprint_reactive*.75);
    IndirectColor=vec4(indirect*(1.-fog_amount),0);
}

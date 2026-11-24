// Shared opaque GGX and low-frequency environment response. No texture taps.
#ifndef VALWOOD_MATERIAL_RESPONSE
#define VALWOOD_MATERIAL_RESPONSE
const float MATERIAL_PI=3.141592653589793;
// Thin foliage reflects the viewed face and transmits a bounded share from
// the reverse face. The weights sum to one: a uniform sky gains no energy.
float material_hemisphere(vec3 geometricN,bool thin) {
    float front=clamp(.5+.5*geometricN.y,0.,1.);
    return thin?mix(1.-front,front,.70):front;
}
vec3 material_thin_sh9(vec3 coefficients[9],vec3 N) {
    // Even SH bands are identical on both faces; evaluate them only once.
    vec3 even=coefficients[0]*.282095+
        .25*(1.092548*(coefficients[4]*N.x*N.y+coefficients[5]*N.y*N.z+coefficients[7]*N.x*N.z)+
        coefficients[6]*.315392*(3.*N.z*N.z-1.)+coefficients[8]*.546274*(N.x*N.x-N.y*N.y));
    vec3 odd=(2./3.)*.488603*(coefficients[1]*N.y+coefficients[2]*N.z+coefficients[3]*N.x);
    return .70*max(even+odd,vec3(0))+.30*max(even-odd,vec3(0));
}
float material_root_occlusion(float height) {
    // Root contact attenuates indirect irradiance, not the source albedo or
    // direct sunlight. No screen-dependent noise or extra sampler is needed.
    return mix(.67,1.,smoothstep(.015,.28,max(height,0.)));
}
vec3 material_f0(vec3 base,float metal,float dielectric) {
    return mix(vec3(clamp(dielectric,0.,1.)),clamp(base,vec3(0),vec3(1)),clamp(metal,0.,1.));
}
vec3 material_f0(vec3 base,float metal) {return material_f0(base,metal,.04);}
vec3 material_direct(vec3 base,float metal,float rough,vec3 f0,vec3 N,vec3 V,vec3 L) {
    float nl=max(dot(N,L),0.);
    if(nl<=0.)return vec3(0.);
    rough=clamp(rough,.04,1.);metal=clamp(metal,0.,1.);
    vec3 sum=V+L,H=sum*inversesqrt(max(dot(sum,sum),.0001));
    float nv=max(dot(N,V),.001),nh=clamp(dot(N,H),0.,1.),vh=clamp(dot(V,H),0.,1.);
    float a=rough*rough,a2=a*a;
    // This form avoids cancellation at nh=1. A .0001 denominator floor
    // erases resolved puddle/metal highlights below roughness ~.2.
    float d=nh*nh*a2+(1.-nh*nh);
    float D=a2/max(MATERIAL_PI*d*d,1e-12),k=(rough+1.)*(rough+1.)*.125;
    float G=nl/(nl*(1.-k)+k)*nv/(nv*(1.-k)+k);
    vec3 F=f0+(1.-f0)*pow(1.-vh,5.);
    // Light enters and exits a dielectric diffuse layer. Both interface
    // losses are needed for reciprocal, energy-bounded grazing response.
    vec3 Fv=f0+(1.-f0)*pow(1.-clamp(nv,0.,1.),5.);
    vec3 Fl=f0+(1.-f0)*pow(1.-clamp(nl,0.,1.),5.);
    return ((1.-Fv)*(1.-Fl)*max(base,vec3(0))*(1.-metal)/MATERIAL_PI+D*G*F/max(4.*nl*nv,.001))*nl;
}
vec3 material_environment_fresnel(vec3 f0,float rough,float nv) {
    return f0+(max(vec3(1.-rough),f0)-f0)*pow(1.-clamp(nv,0.,1.),5.);
}
vec3 material_diffuse_factor(vec3 base,float metal,vec3 fresnel) {
    return max(base,vec3(0))*(1.-fresnel)*(1.-clamp(metal,0.,1.));
}
vec3 material_indirect(vec3 base,float metal,float rough,vec3 f0,vec3 N,vec3 V,
                       vec3 ambient,vec3 radiance,float ao) {
    vec3 fresnel=material_environment_fresnel(f0,rough,dot(N,V));
    return (material_diffuse_factor(base,metal,fresnel)*ambient+
        radiance*fresnel*(1.-.45*rough))*clamp(ao,0.,1.);
}
void material_wet_surface(inout vec3 base,inout float rough,float metal,bool leaf,float wet) {
    wet=clamp(wet,0.,1.);metal=clamp(metal,0.,1.);
    base*=1.-wet*(leaf?.03:mix(.08,.02,metal));
    // Damp porous stone stays broad; authored smooth water films stay smooth.
    // A floor must never make a wet surface rougher than its dry source.
    float floorR=min(rough,leaf?.50:mix(.34,.12,metal));
    float target=max(floorR,rough*(leaf?.84:mix(.90,.97,metal)));
    rough=clamp(mix(rough,target,wet),.04,1.);
}
float material_film_noise(vec2 p){vec2 c=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec3 q=fract(vec3(c.xyx)*.1031);q+=dot(q,q.yzx+33.33);float a=fract((q.x+q.y)*q.z);q=fract(vec3((c+vec2(1,0)).xyx)*.1031);q+=dot(q,q.yzx+33.33);float b=fract((q.x+q.y)*q.z);q=fract(vec3((c+vec2(0,1)).xyx)*.1031);q+=dot(q,q.yzx+33.33);float d=fract((q.x+q.y)*q.z);q=fract(vec3((c+vec2(1)).xyx)*.1031);q+=dot(q,q.yzx+33.33);float e=fract((q.x+q.y)*q.z);return mix(mix(a,b,f.x),mix(d,e,f.x),f.y);}
void material_wet_film(inout vec3 base,inout float rough,float metal,bool leaf,float film,vec3 normal,vec2 world){
    // Film gathers in irregular upward-facing patches rather than making an
    // entire street glossy. Porous dampness remains the broad response above.
    if(film<=.0001)return;
    float upward=smoothstep(.55,.96,leaf?abs(normal.y):normal.y);
    float micro=material_film_noise(world*.23)+.24*material_film_noise(world*.91);
    float patches=smoothstep(.52,.96,micro);
    float retention=leaf?.22:mix(.07,1.,upward);
    float water=clamp(film,0.,1.)*retention*patches;
    float target=min(rough,leaf?.30:mix(.13,.08,clamp(metal,0.,1.)));
    rough=mix(rough,target,water*.72);
    base*=1.-water*(leaf?.015:.045)*(1.-clamp(metal,0.,1.));
}
void material_rain_runoff(inout vec3 base,inout float rough,float rain,float film,vec3 N,vec3 P,float time){
    // A thin moving response along projected gravity. This is visual runoff,
    // bounded by current rainfall/retained film; it never changes water levels.
    float slope=1.-abs(N.y),amount=clamp(rain*film,0.,1.);
    if(slope<.035||amount<.002)return;
    vec3 down=vec3(0,-1,0)-N*dot(N,vec3(0,-1,0));
    down*=inversesqrt(max(dot(down,down),.0001));
    vec3 side=cross(N,down);float crossSlope=dot(P,side);
    float strip=1.-smoothstep(.06,.20,abs(sin(crossSlope*14.+material_film_noise(P.xz*.5)*2.)));
    float bead=smoothstep(.05,.25,fract(dot(P,down)*1.7-time*.85+material_film_noise(vec2(crossSlope,0.))));
    float flow=amount*smoothstep(.035,.25,slope)*strip*bead*.3;
    rough=mix(rough,min(rough,.14),flow);base*=1.-flow*.045;
}
#endif

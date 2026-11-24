// Shared opaque GGX and low-frequency environment response. No texture taps.
#ifndef VALWOOD_MATERIAL_RESPONSE
#define VALWOOD_MATERIAL_RESPONSE
const float MATERIAL_PI=3.141592653589793;
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
#endif

#ifndef VALWOOD_STORM_LIGHTING
#define VALWOOD_STORM_LIGHTING
// Camera-relative strike point/radius. Root binds these while each program is
// current; zero color is the exact legacy no-op. Caller supplies sky exposure
// from its existing enclosure/roof policy, and applies its ordinary aerial fog.
uniform vec4 uStormFlashPositionRadius=vec4(0.0);
uniform vec3 uStormFlashColor=vec3(0.0);
vec3 stormFlashLight(vec3 relativePosition,vec3 N,vec3 V,vec3 base,float metal,float roughness,float skyExposure) {
    if(max(max(uStormFlashColor.r,uStormFlashColor.g),uStormFlashColor.b)<=0.0 || uStormFlashPositionRadius.w<=0.0 || skyExposure<=0.0)return vec3(0.0);
    vec3 delta=uStormFlashPositionRadius.xyz-relativePosition;
    float d2=dot(delta,delta),radius=uStormFlashPositionRadius.w;
    if(d2>=radius*radius)return vec3(0.0);
    vec3 L=delta*inversesqrt(max(d2,.0001));
    float nl=max(dot(N,L),0.0),nv=max(dot(N,V),.001);
    if(nl<=0.0)return vec3(0.0);
    vec3 sum=L+V;vec3 H=dot(sum,sum)>.000001?normalize(sum):N;
    float nh=max(dot(N,H),0.0),vh=max(dot(V,H),0.0);
    float rough=clamp(roughness,.045,1.0),a=rough*rough,a2=a*a;
    float denominator=nh*nh*(a2-1.0)+1.0;
    float distribution=a2/(3.14159265359*denominator*denominator+.000001);
    float k=(rough+1.0)*(rough+1.0)*.125;
    float geometry=nl/(nl*(1.0-k)+k)*nv/(nv*(1.0-k)+k);
    vec3 albedo=clamp(base,vec3(0.0),vec3(4.0));float metallic=clamp(metal,0.0,1.0);
    vec3 f0=mix(vec3(.04),albedo,metallic),F=f0+(1.0-f0)*pow(1.0-vh,5.0);
    vec3 specular=distribution*geometry*F/max(4.0*nl*nv,.0001);
    vec3 diffuse=(1.0-F)*(1.0-metallic)*albedo*.31830988618;
    float falloff=1.0-sqrt(max(d2,0.0))/radius;falloff*=falloff;
    return (diffuse+specular)*uStormFlashColor*nl*falloff*clamp(skyExposure,0.0,1.0);
}
#endif

#version 330 core
in vec3 P,N,T,B;in vec2 UV;out vec4 FragColor;
uniform sampler2D uBase,uNormal,uARM;
uniform bool uHasBase,uHasNormal,uHasARM;
uniform vec4 uBaseFactor;
uniform float uRoughness,uMetallic;
uniform vec3 uSun,uSunColor,uSkyLight,uGroundLight,uWorldUp;
const float PI=3.14159265359;
void main(){
 vec3 base=(uHasBase?texture(uBase,UV).rgb:vec3(1))*uBaseFactor.rgb;
 vec3 normal=normalize(N);if(uHasNormal)normal=normalize(mat3(normalize(T),normalize(B),normal)*(texture(uNormal,UV).xyz*2.-1.));
 vec3 arm=uHasARM?texture(uARM,UV).rgb:vec3(1);float rough=clamp(uRoughness*arm.g,.32,1.),metal=clamp(uMetallic*arm.b,0.,1.);
 vec3 dx=dFdx(normal),dy=dFdy(normal);rough=sqrt(min(1.,rough*rough+min(.12,max(dot(dx,dx),dot(dy,dy))*.35)));
 vec3 V=normalize(-P),L=normalize(uSun),H=normalize(V+L);float nl=max(dot(normal,L),0.),nv=max(dot(normal,V),.001),nh=max(dot(normal,H),0.),vh=max(dot(V,H),0.);
 float a=rough*rough,a2=a*a,d=nh*nh*(a2-1.)+1.,D=a2/max(PI*d*d,.0001),k=(rough+1.)*(rough+1.)*.125,G=nl/(nl*(1.-k)+k)*nv/(nv*(1.-k)+k);
 vec3 F0=mix(vec3(.04),base,metal),F=F0+(1.-F0)*pow(1.-vh,5.);
 vec3 direct=((1.-F)*(1.-metal)*base/PI+D*G*F/max(4.*nl*nv,.001))*nl*uSunColor*PI;
 vec3 geometric=normalize(N),worldUp=normalize(uWorldUp);
 vec3 ambient=mix(uGroundLight,uSkyLight,clamp(.5+.5*dot(geometric,worldUp),0.,1.));
 // Same bounded hemisphere environment response as world opaque materials.
 // World-up is transformed into view space, so looking up/down cannot rotate
 // sky illumination with the camera or leave the steel artificially black.
 vec3 reflected=reflect(-V,geometric);
 vec3 radiance=mix(uGroundLight,uSkyLight,clamp(.5+.5*dot(reflected,worldUp),0.,1.));
 radiance=mix(radiance,ambient,rough*rough*.75);
 vec3 fresnel=F0+(max(vec3(1.-rough),F0)-F0)*pow(1.-clamp(dot(geometric,V),0.,1.),5.);
 vec3 indirect=(base*ambient*(1.-fresnel)*(1.-metal)+radiance*fresnel*(1.-.45*rough))*mix(.48,1.,arm.r);
 // Linear HDR, composed after weather and before the same world exposure,
 // tone curve and display encoding. Low history weight prevents arm trails.
 FragColor=vec4(max(vec3(0),direct+indirect),.08);
}

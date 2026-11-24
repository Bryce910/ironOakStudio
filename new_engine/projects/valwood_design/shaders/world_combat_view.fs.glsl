#version 330 core
in vec3 P,N,T,B;in vec2 UV;out vec4 FragColor;
uniform sampler2D uBase,uNormal,uARM;
uniform bool uHasBase,uHasNormal,uHasARM;
uniform vec4 uBaseFactor;
uniform float uRoughness,uMetallic,uNormalScale;
uniform vec3 uSun,uSunColor,uSkyLight,uGroundLight,uWorldUp;
const float PI=3.14159265359;
// The legacy fitted-mail sleeves have a cloth normal and constant color, with
// no color/ARM atlas. Give only that fallback material a restrained link
// pattern. Filter it out before it becomes subpixel; owned mapped materials
// keep their authored images and UVs.
float fittedMailHeight(vec2 q){
 q.x+=.5*mod(floor(q.y),2.);
 vec2 cell=(fract(q)-.5)*vec2(1.12,1.38);
 float ring=(length(cell)-.34)/.105;
 return exp(-ring*ring);
}
void main(){
 vec3 base=(uHasBase?texture(uBase,UV).rgb:vec3(1))*uBaseFactor.rgb;
 vec3 tangentNormal=vec3(0,0,1);
 if(uHasNormal){tangentNormal=texture(uNormal,UV).xyz*2.-1.;tangentNormal.xy*=uNormalScale;}
 vec3 arm=uHasARM?texture(uARM,UV).rgb:vec3(1);float rough=clamp(uRoughness*arm.g,.32,1.),metal=clamp(uMetallic*arm.b,0.,1.);
 if(uHasNormal&&!uHasBase&&!uHasARM&&uMetallic>.5){
  vec2 q=UV*vec2(30.,90.);
  float coverage=1.-smoothstep(.25,.8,max(fwidth(q.x),fwidth(q.y)));
  float h=fittedMailHeight(q),epsilon=.025;
  vec2 slope=vec2(fittedMailHeight(q+vec2(epsilon,0))-fittedMailHeight(q-vec2(epsilon,0)),fittedMailHeight(q+vec2(0,epsilon))-fittedMailHeight(q-vec2(0,epsilon)))/(2.*epsilon);
  tangentNormal.xy-=slope*.12*coverage;
  base=mix(base,vec3(.22,.23,.235),.65)*mix(1.,.72+.36*h,coverage);
  rough=max(rough,.68);arm.r*=mix(1.,.76+.24*h,coverage);
 }
 vec3 normal=normalize(mat3(normalize(T),normalize(B),normalize(N))*normalize(tangentNormal));
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

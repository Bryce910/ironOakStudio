#version 330 core
// Neutral standalone material inspection. This is not Valwood world lighting,
// and does not claim to reproduce a Unity HDRP marketing render.
in vec3 vPos,vNormal,vTangent,vBitangent;
in vec2 vUV;
uniform vec3 uEye;
uniform sampler2D uBase,uNormal,uARM,uEmissive;
uniform bool uHasBase,uHasNormal,uHasARM,uHasEmissive;
uniform vec4 uBaseFactor;
uniform float uRoughness,uMetallic,uNormalScale,uAlphaCutoff;
uniform vec3 uEmissiveFactor;
out vec4 color;
const float PI=3.14159265359;
vec3 light(vec3 n,vec3 v,vec3 l,vec3 radiance,vec3 base,float roughness,float metallic){
    vec3 h=normalize(v+l);float nl=max(dot(n,l),0.),nv=max(dot(n,v),.001),nh=max(dot(n,h),0.);
    float a=roughness*roughness,a2=a*a,d=nh*nh*(a2-1.)+1.;
    float D=a2/max(PI*d*d,.00001),k=(roughness+1.)*(roughness+1.)/8.;
    float G=(nv/(nv*(1.-k)+k))*(nl/(nl*(1.-k)+k));
    vec3 f0=mix(vec3(.04),base,metallic),F=f0+(1.-f0)*pow(1.-max(dot(h,v),0.),5.);
    return ((1.-F)*(1.-metallic)*base/PI+D*G*F/max(4.*nv*nl,.0001))*radiance*nl;
}
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
void main(){
    vec4 base=uBaseFactor*(uHasBase?texture(uBase,vUV):vec4(1.));
    if(base.a<uAlphaCutoff)discard;
    vec3 n=normalize(vNormal);if(!gl_FrontFacing)n=-n;
    if(uHasNormal){vec3 t=texture(uNormal,vUV).xyz*2.-1.;t.xy*=uNormalScale;
        n=normalize(mat3(normalize(vTangent),normalize(vBitangent),n)*normalize(t));}
    vec3 arm=uHasARM?texture(uARM,vUV).rgb:vec3(1.);
    float roughness=clamp(uRoughness*arm.g,.055,1.),metallic=clamp(uMetallic*arm.b,0.,1.);
    vec3 v=normalize(uEye-vPos);
    vec3 result=light(n,v,normalize(vec3(-.55,.8,1.)),vec3(3.7,3.55,3.35),base.rgb,roughness,metallic)
               +light(n,v,normalize(vec3(.8,.3,.25)),vec3(.7,.8,1.),base.rgb,roughness,metallic)
               +light(n,v,normalize(vec3(.25,.55,-1.)),vec3(1.4,1.5,1.7),base.rgb,roughness,metallic);
    // Analytic neutral sky fill, deliberately distinct from an HDRI/IBL test.
    float sky=.5+.5*n.y;vec3 ambient=mix(vec3(.11,.105,.10),vec3(.30,.34,.40),sky);
    vec3 f0=mix(vec3(.04),base.rgb,metallic);
    result+=(base.rgb*(1.-metallic)*ambient+f0*ambient*.65)*arm.r;
    result+=uEmissiveFactor*(uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1.));
    color=vec4(pow(aces(result),vec3(1./2.2)),1.);
}

#version 330 core
in vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
in vec2 vUV;
uniform sampler2D uBase,uNormal,uARM,uEmissive;
uniform bool uHasBase,uHasNormal,uHasARM,uHasEmissive;
uniform vec4 uBaseFactor;
uniform float uRoughness,uMetallic,uNormalScale,uAlphaCutoff;
uniform vec3 uEmissiveFactor;
// Direct light remains in the yaw-aligned viewmodel frame. Scene IBL uses
// the host's actual world basis/origin and existing baked world probes.
uniform vec3 uSunDirectionCamera,uSunColor,uSkyFill,uGroundFill;
uniform vec3 uViewWorldOrigin,uViewWorldRight,uViewWorldForward;
uniform samplerCube uLocalProbe0,uLocalProbe1;
uniform int uLocalProbeCount;
uniform vec3 uLocalProbeCenter[2],uLocalProbeBoxMin[2],uLocalProbeBoxMax[2];
uniform vec2 uLocalProbeWeights;
uniform float uLocalProbeMip,uLocalProbeLightScale;
#ifdef VALWOOD_HDR_ENVIRONMENT
uniform samplerCube uEnvironmentCube;
uniform sampler2D uEnvironmentBRDF;
uniform vec3 uEnvironmentSH[9];
uniform vec2 uEnvironmentRotation=vec2(1,0);
uniform float uEnvironmentIntensity=1,uEnvironmentStrength=0,uEnvironmentMip=8;
uniform bool uEnvironmentSkyOnly=false;
#endif
out vec4 fragColor;
#pragma include "world_material_response.glsl"
vec3 view_world_direction(vec3 d){
 return uViewWorldRight*d.x+vec3(0,1,0)*d.y-uViewWorldForward*d.z;
}
#ifdef VALWOOD_HDR_ENVIRONMENT
vec3 environment_direction(vec3 d){
 return vec3(uEnvironmentRotation.x*d.x-uEnvironmentRotation.y*d.z,d.y,
             uEnvironmentRotation.y*d.x+uEnvironmentRotation.x*d.z);
}
vec3 environment_diffuse(vec3 N){
 // Identical Lambert-convolved SH9 / PI convention to world actors.
 vec3 n=environment_direction(N);
 return max(uEnvironmentSH[0]*.282095+
  (2./3.)*.488603*(uEnvironmentSH[1]*n.y+uEnvironmentSH[2]*n.z+uEnvironmentSH[3]*n.x)+
  .25*(1.092548*(uEnvironmentSH[4]*n.x*n.y+uEnvironmentSH[5]*n.y*n.z+uEnvironmentSH[7]*n.x*n.z)+
       uEnvironmentSH[6]*.315392*(3.*n.z*n.z-1.)+uEnvironmentSH[8]*.546274*(n.x*n.x-n.y*n.y)),vec3(0))*uEnvironmentIntensity;
}
#endif
float local_box_weight(vec3 world,vec3 lo,vec3 hi){
 vec3 edge=min(world-lo,hi-world);
 return smoothstep(0.,1.5,min(edge.x,min(edge.y,edge.z)));
}
vec3 local_reflection_direction(vec3 world,vec3 direction,int index){
 vec3 safe=vec3(abs(direction.x)>.00001?direction.x:.00001,
                abs(direction.y)>.00001?direction.y:.00001,
                abs(direction.z)>.00001?direction.z:.00001);
 vec3 a=(uLocalProbeBoxMin[index]-world)/safe,b=(uLocalProbeBoxMax[index]-world)/safe;
 vec3 far=max(a,b);
 float distance=max(0.,min(far.x,min(far.y,far.z)));
 return world+direction*distance-uLocalProbeCenter[index];
}
vec3 local_reflection(vec3 world,vec3 direction,float rough,vec3 fallback){
 if(uLocalProbeCount<=0)return fallback;
 vec2 weights=uLocalProbeWeights;
 weights.x*=local_box_weight(world,uLocalProbeBoxMin[0],uLocalProbeBoxMax[0]);
 weights.y*=uLocalProbeCount>1?local_box_weight(world,uLocalProbeBoxMin[1],uLocalProbeBoxMax[1]):0.;
 float total=weights.x+weights.y;
 if(total<=.0001)return fallback;
 vec3 radiance=vec3(0);
 if(weights.x>0.)radiance=textureLod(uLocalProbe0,local_reflection_direction(world,direction,0),rough*uLocalProbeMip).rgb*weights.x;
 if(weights.y>0.)radiance+=textureLod(uLocalProbe1,local_reflection_direction(world,direction,1),rough*uLocalProbeMip).rgb*weights.y;
 // Same bounded capture contribution and roughness LOD as world actors.
 return mix(fallback,max(radiance*uLocalProbeLightScale/total,vec3(0)),clamp(total*.75,0.,.75));
}

// Bounded cosmetic layer in the original mesh UVs. No world/screen positions,
// time, extra textures, geometry passes or camera-dependent random seeds.
#ifndef VALWOOD_GENERATED_GRASS_FRAGMENT
uniform vec3 uBloodWear=vec3(0.);
float armorBloodHash(vec2 p){vec3 q=fract(vec3(p.xyx)*.1031);q+=dot(q,q.yzx+33.33);return fract((q.x+q.y)*q.z);}
float armorBloodNoise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(armorBloodHash(i),armorBloodHash(i+vec2(1,0)),f.x),mix(armorBloodHash(i+vec2(0,1)),armorBloodHash(i+vec2(1)),f.x),f.y);}
void armorBloodWear(vec2 uv,inout vec3 albedo,inout float rough,inout float metal){
 if(uBloodWear.x<=0.)return;
 vec2 p=uv*vec2(6.,8.)+vec2(uBloodWear.y*.017,uBloodWear.y*.031);
 vec2 cell=floor(p),q=fract(p)-vec2(.25+.5*armorBloodHash(cell+3.7),.25+.5*armorBloodHash(cell+7.9));
 float random=armorBloodHash(cell),density=.10+.52*clamp(uBloodWear.x,0.,1.);
 float footprint=max(length(dFdx(p)),length(dFdy(p)));
 float aa=max(.012,footprint*.65),radius=.10+.25*armorBloodHash(cell+19.);
 float angle=armorBloodHash(cell+29.)*6.2831853;vec2 direction=vec2(cos(angle),sin(angle));
 vec2 shape=vec2(dot(q,direction),dot(q,vec2(-direction.y,direction.x)));
 float edge=length(shape*vec2(.76,1.25))+(armorBloodNoise(q*15.+cell*3.)-.5)*.16;
 float splash=1.-smoothstep(radius-aa,radius+aa,edge);
 // Short irregular trails descend from the larger splashes, with thin ends.
 float tail=max(0.,1.-abs(q.x+.025*sin(q.y*17.+random*9.))/max(.012,.055*(1.-max(q.y,0.)*1.7)));
 tail*=smoothstep(-.03,.04,q.y)*(1.-smoothstep(.14,.40,q.y));
 float cluster=smoothstep(.32,.67,armorBloodNoise(p*.38+uBloodWear.y*.001));
 float mark=max(splash,tail*.80)*step(random,density)*mix(.12,1.,cluster);
 vec2 fp=p*3.1,fc=floor(fp),fq=fract(fp)-.5;
 float drop=1.-smoothstep(.035-aa*3.1,.075+aa*3.1,length(fq));
 mark=max(mark,drop*step(armorBloodHash(fc+47.1),density*.12)*mix(.2,1.,cluster));
 // Unresolved splashes converge to coverage rather than flickering with LOD.
 mark*=1.-smoothstep(.65,1.5,footprint);
 float opacity=clamp(mark*.94,0.,1.);
 float dried=clamp(.20+.65*armorBloodHash(cell+61.)+(1.-uBloodWear.z)*.15,0.,1.);
 vec3 blood=mix(vec3(.12,.0025,.0015),vec3(.025,.001,.0007),dried);
 albedo=mix(albedo,blood,opacity);
 rough=mix(rough,mix(.34,.76,dried),opacity);
 metal*=1.-opacity;
}
#endif

void main(){
 // Sleeve closures are ordinary primitives with their own matte material.
 // The shared skeletal vertex stage does not supply a sleeve-tag varying;
 // reading an unbound input made their material classification undefined.
 bool sleeveCloth=false;
 // The additive undersleeves use matte charcoal fabric. Their camera-reaching
 // UVs must not stretch the purchased glove's leather/metal texture maps.
 vec4 base=sleeveCloth?vec4(.035,.045,.055,1.):uBaseFactor*(uHasBase?texture(uBase,vUV):vec4(1));
 if(base.a<uAlphaCutoff)discard;
 vec3 geometricN=normalize(vNormal),N=geometricN;
 if(uHasNormal&&!sleeveCloth){
  // Interpolation bends the vertex frame away from orthogonality. Rebuild
  // its tangent plane per pixel without changing the source UV handedness.
  vec3 T=vTangent-N*dot(N,vTangent);
  vec3 fallback=cross(abs(N.y)<.9?vec3(0,1,0):vec3(1,0,0),N);
  T=dot(T,T)>1e-12?normalize(T):normalize(fallback);
  float handedness=dot(cross(N,T),vBitangent)<0.?-1.:1.;
  vec3 B=cross(N,T)*handedness;
  vec3 q=texture(uNormal,vUV).xyz*2.-1.;q.xy*=uNormalScale;
  N=normalize(mat3(T,B,N)*q);
 }
 if(!gl_FrontFacing){N=-N;geometricN=-geometricN;}
 vec3 arm=uHasARM&&!sleeveCloth?texture(uARM,vUV).rgb:vec3(1);
 float rough=sleeveCloth?.95:clamp(uRoughness*arm.g,.06,1.),metal=sleeveCloth?0.:clamp(uMetallic*arm.b,0.,1.);
 // Match the world actor finish; preserve rougher source leather/cloth.
 rough=sqrt(rough*rough+.085*metal*(1.-rough*rough));
 vec3 dx=dFdx(N),dy=dFdy(N);
 rough=sqrt(min(1.,rough*rough+min(.12,max(dot(dx,dx),dot(dy,dy))*.35)));
 if(!sleeveCloth)armorBloodWear(vUV,base.rgb,rough,metal);
 vec3 V=normalize(-vRelative),L=normalize(uSunDirectionCamera);
 float nv=max(dot(N,V),.001);
 vec3 f0=material_f0(base.rgb,metal);
 // Broad hemispheric irradiance supplies bounced diffuse light. Metal has
 // no diffuse lobe: it reads the same scene sky/ground through its F0 and a
 // roughness-filtered environment BRDF instead of being darkened to grey.
 vec3 worldNormal=normalize(view_world_direction(geometricN));
 float hemi=clamp(worldNormal.y*.5+.5,0.,1.);
 vec3 irradiance=mix(uGroundFill,uSkyFill,hemi);
 vec3 R=normalize(view_world_direction(reflect(-V,N)));
 float reflection_hemi=smoothstep(-.12,.28,R.y);
 vec3 environment=mix(uGroundFill,uSkyFill,reflection_hemi);
 environment=mix(environment,(uGroundFill+uSkyFill)*.5,rough*rough);
#ifdef VALWOOD_HDR_ENVIRONMENT
 if(uEnvironmentStrength>0.){
  irradiance=mix(irradiance,environment_diffuse(worldNormal),uEnvironmentStrength);
  vec3 captured=max(textureLod(uEnvironmentCube,environment_direction(R),rough*uEnvironmentMip).rgb,vec3(0))*uEnvironmentIntensity;
  // A pure-sky panorama supplies no real ground below its horizon.
  if(uEnvironmentSkyOnly)captured=mix(uGroundFill,captured,smoothstep(-.12,.28,R.y));
  environment=mix(environment,captured,uEnvironmentStrength);
 }
#endif
 vec3 world=uViewWorldOrigin+view_world_direction(vPos);
 environment=local_reflection(world,R,rough,environment);
 const vec4 c0=vec4(-1.,-.0275,-.572,.022);
 const vec4 c1=vec4(1.,.0425,1.04,-.04);
 vec4 brdf=rough*c0+c1;
 float brdf_a=min(brdf.x*brdf.x,exp2(-9.28*nv))*brdf.x+brdf.y;
 vec2 brdf_ab=vec2(-1.04,1.04)*brdf_a+brdf.zw;
 vec3 environment_spec=environment*(f0*brdf_ab.x+brdf_ab.y);
#ifdef VALWOOD_HDR_ENVIRONMENT
 if(uEnvironmentStrength>0.){
  vec2 dfg=texture(uEnvironmentBRDF,vec2(nv,rough)).rg;
  environment_spec=mix(environment_spec,environment*(f0*dfg.x+dfg.y),uEnvironmentStrength);
 }
#endif
 vec3 fill=(irradiance*base.rgb*(1.-metal)+environment_spec)*arm.r;
 vec3 glow=sleeveCloth?vec3(0.):uEmissiveFactor*(uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1));
 // The host's sun color uses the same irradiance / PI convention as world
 // actors. Share their reciprocal GGX response and precision, applying the
 // same PI conversion once; ambient fill and scene exposure stay unchanged.
 fragColor=vec4(fill+material_direct(base.rgb,metal,rough,f0,N,V,L)*uSunColor*MATERIAL_PI+glow,base.a);
}

#version 330 core
in float vTip,vColorVar,vStyle,vTint,vSide,vFade,vCoverage;
in vec3 vWorld,vNormal,vAcross;
uniform vec3 uSunColor,uAmbient,uSunDir,uCamPos,uBaseCol,uTipCol,uBiomeTint,uFogColor;
uniform float uFogDensity,uHeightFogDensity,uHeightFogBase,uAO,uWetness,uDew,uFxTime,uSunElev;
uniform sampler2D uGrassShadowMap,uGrassShadowMapWide;
uniform mat4 uGrassLightVP,uGrassLightVPWide;
uniform bool uToonShading;
uniform float uToonBands;
uniform int uNumLights;
uniform vec3 uLightPos[32],uLightColor[32],uLightDir[32];
uniform vec4 uLightParams[32];
uniform float uLightRadius[32];
#pragma include "common/grass_ecology.glsl"
out vec4 FragColor;
float shadowSample(sampler2D map,vec3 p,float bias){
    vec2 t=1.0/vec2(textureSize(map,0));
    return (step(p.z-bias,texture(map,p.xy+t*vec2(-.7,-.3)).r)+step(p.z-bias,texture(map,p.xy+t*vec2(.3,-.7)).r)
           +step(p.z-bias,texture(map,p.xy+t*vec2(.7,.3)).r)+step(p.z-bias,texture(map,p.xy+t*vec2(-.3,.7)).r))*.25;
}
float visibility(){
    vec4 q=uGrassLightVP*vec4(vWorld+vec3(0,.035,0),1);vec3 p=q.xyz/q.w*.5+.5;
    if(p.z>0.0&&p.z<1.0&&all(greaterThan(p.xy,vec2(.015)))&&all(lessThan(p.xy,vec2(.985))))return shadowSample(uGrassShadowMap,p,.0002);
    q=uGrassLightVPWide*vec4(vWorld,1);p=q.xyz/q.w*.5+.5;
    if(p.z>0.0&&p.z<1.0&&all(greaterThan(p.xy,vec2(.005)))&&all(lessThan(p.xy,vec2(.995))))return shadowSample(uGrassShadowMapWide,p,.00035);
    return 1.0;
}
void main(){
    float edge=1.0-smoothstep(1.0-max(fwidth(vSide)*.65,.015),1.0,abs(vSide));
    float alpha=edge*vCoverage*vFade;
    if(alpha<.025)discard;
    // A shallow transverse fold gives the two halves of a leaf distinct normals.
    vec3 N=normalize(vNormal+vAcross*vSide*.24);
    if(!gl_FrontFacing)N=-N;
    // Bend leaf normals toward the meadow canopy while retaining the curved
    // surface and transverse fold. Edge-on leaves still receive sky/sun fill.
    N=normalize(N*.65+vec3(0,.65,0));
    vec3 V=normalize(uCamPos-vWorld),L=normalize(-uSunDir);
    vec3 base=grassReflectance(vWorld.xz,vTip,vColorVar,vStyle)*uBiomeTint*grassTint(vTint);
    base*=1.0-uWetness*.18;
    if(uToonShading)base=mix(uBaseCol,uTipCol,smoothstep(.05,.85,vTip));
    float vis=visibility(),nl=clamp((dot(N,L)+.25)/1.25,0.0,1.0);
    float rootOcclusion=mix(.65,1.0,smoothstep(0.0,.65,vTip));
    float hemi=.68+.32*clamp(N.y*.5+.5,0.0,1.0);
    vec3 col=base*uAmbient*1.65*hemi*rootOcclusion;
    col+=base*uSunColor*nl*vis;
    // Thin leaves transmit sunlight; it stays colored and follows the sun.
    float through=max(dot(-N,L),0.0)*(.18+.24*pow(max(dot(V,-L),0.0),3.0));
    col+=base*uSunColor*through*vis*(.45+.55*vTip);
    vec3 H=normalize(V+L);float sheen=pow(max(dot(N,H),0.0),24.0)*(.012+uWetness*.025+uDew*.006);
    col+=uSunColor*sheen*vis;
    for(int i=0;i<uNumLights;i++){
        vec3 d=uLightPos[i]-vWorld;float len=length(d);if(len>=uLightRadius[i])continue;
        vec3 light=d/max(len,.001);float att=pow(1.0-len/uLightRadius[i],2.0);
        if(uLightParams[i].x>.5)att*=smoothstep(uLightParams[i].z,uLightParams[i].y,dot(normalize(uLightDir[i]),-light));
        col+=base*uLightColor[i]*(max(dot(N,light),0.0)+.15*max(dot(-N,light),0.0))*att;
    }
    float distance=length(vWorld-uCamPos),lowFog=1.0-exp(-uHeightFogDensity*max(0.0,uHeightFogBase-vWorld.y));
    float fog=1.0-exp(-uFogDensity*distance*(1.0+lowFog*3.0));
    FragColor=vec4(mix(col,uFogColor,clamp(fog,0.0,.88)),alpha);
}

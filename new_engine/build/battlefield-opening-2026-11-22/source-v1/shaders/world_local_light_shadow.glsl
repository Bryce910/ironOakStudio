// One nearby practical light receives real six-face geometry shadowing.
// Lower capability renderers retain the bounded practical-light fallback.
#ifndef VALWOOD_LOCAL_LIGHT_SHADOW_GLSL
#define VALWOOD_LOCAL_LIGHT_SHADOW_GLSL
#ifdef VALWOOD_LOCAL_LIGHT_SHADOW
uniform samplerCube uLocalLightShadow;
uniform vec4 uLocalLightShadowPositionRadius;
uniform vec2 uLocalLightShadowNearFar;
uniform float uLocalLightShadowEnabled;
uniform float uLocalLightShadowSide;
float local_light_shadow(vec3 world,vec3 N,vec3 lightWorld) {
    if(uLocalLightShadowEnabled<=0.||
       dot(lightWorld-uLocalLightShadowPositionRadius.xyz,lightWorld-uLocalLightShadowPositionRadius.xyz)>.0004)
        return 1.;
    vec3 delta=world-uLocalLightShadowPositionRadius.xyz;
    float distance=length(delta),nearPlane=uLocalLightShadowNearFar.x,farPlane=uLocalLightShadowNearFar.y;
    if(distance<nearPlane*2.||distance>=farPlane)return 1.;
    // Receiver offset is in metres and scales with the cube's texel footprint.
    // Compare the ordinary perspective depth written by the existing alpha-
    // correct depth programs; no radial gl_FragDepth or extra fragment pass.
    delta+=N*(.004+distance/max(uLocalLightShadowSide,1.));
    vec3 direction=normalize(delta);
    vec3 axis=abs(direction.y)<.9?vec3(0,1,0):vec3(1,0,0);
    vec3 tangent=normalize(cross(direction,axis)),bitangent=cross(direction,tangent);
    float visibility=0.;
    for(int i=0;i<4;i++) {
        vec2 corner=vec2((i==0||i==2)?-.65:.65,i<2?-.65:.65);
        vec3 tap=delta+(tangent*corner.x+bitangent*corner.y)*(2.*distance/max(uLocalLightShadowSide,1.));
        float faceDepth=max(max(abs(tap.x),abs(tap.y)),abs(tap.z));
        float expected=farPlane/(farPlane-nearPlane)-farPlane*nearPlane/((farPlane-nearPlane)*max(faceDepth-.006,nearPlane));
        visibility+=expected<=texture(uLocalLightShadow,tap).r?1.:0.;
    }
    return mix(1.,visibility*.25,clamp(uLocalLightShadowEnabled,0.,1.));
}
#else
float local_light_shadow(vec3 world,vec3 N,vec3 lightWorld){return 1.;}
#endif
#endif

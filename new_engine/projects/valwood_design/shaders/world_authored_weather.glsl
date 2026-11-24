// The CPU authoring sampler sorts these records by priority, then persistent
// entity ID. Every receiver uses the same ordered alpha composition in WORLD
// XZ. Zero authored count returns its original pass-specific values exactly.
#ifndef VALWOOD_AUTHORED_WEATHER
#define VALWOOD_AUTHORED_WEATHER
uniform int uAuthoredWeatherCount=0,uAuthoredWeatherAreaCount=0;
uniform vec4 uAuthoredWeatherGlobal=vec4(0.0); // rain, cloud, fog, explicit global
uniform vec4 uAuthoredWeatherBounds[16];     // center XZ, inner / outer radius
uniform vec4 uAuthoredWeatherChannels[16];   // rain, cloud, fog, transient influence
vec3 authoredWeather(vec2 world,vec3 legacy) {
    if(uAuthoredWeatherCount==0)return legacy;
    vec3 value=uAuthoredWeatherGlobal.w>0.5?uAuthoredWeatherGlobal.xyz:legacy;
    for(int i=0;i<16;i++) {
        if(i>=uAuthoredWeatherAreaCount)break;
        if(uAuthoredWeatherChannels[i].w<=0.0)continue;
        vec4 bounds=uAuthoredWeatherBounds[i];
        vec2 delta=world-bounds.xy;
        float distance2=dot(delta,delta);
        if(distance2>=bounds.w*bounds.w)continue;
        float weight=1.0;
        if(distance2>bounds.z*bounds.z) {
            float t=clamp((sqrt(distance2)-bounds.z)/(bounds.w-bounds.z),0.0,1.0);
            weight=1.0-t*t*(3.0-2.0*t);
        }
        weight*=clamp(uAuthoredWeatherChannels[i].w,0.0,1.0);
        value+=(uAuthoredWeatherChannels[i].xyz-value)*weight;
    }
    return value;
}
float authoredWeatherRain(vec2 world,float legacy) {
    return authoredWeather(world,vec3(legacy,0.0,0.0)).x;
}
// One CPU-evolved world-space reservoir packet is shared by every receiver.
// Rain/audio still use authoredWeatherRain; saturation must never drive rain.
uniform bool uWetStateEnabled=false;
uniform int uWetStateCount=0;
uniform vec4 uWetStateGlobal=vec4(0.0);
uniform vec4 uWetStateBounds[32],uWetStateChannels[32];
vec2 authoredWetness(vec2 world,float legacy) {
    if(!uWetStateEnabled)return vec2(legacy,legacy*legacy);
    vec2 value=uWetStateGlobal.xy;
    for(int i=0;i<32;i++) {
        if(i>=uWetStateCount)break;
        vec4 b=uWetStateBounds[i];vec2 delta=world-b.xy;
        float d2=dot(delta,delta);if(d2>=b.w*b.w)continue;
        float weight=1.0;
        if(d2>b.z*b.z){float t=clamp((sqrt(d2)-b.z)/(b.w-b.z),0.,1.);weight=1.-t*t*(3.-2.*t);}
        weight*=clamp(uWetStateChannels[i].w,0.,1.);
        value+=(uWetStateChannels[i].xy-value)*weight;
    }
    return clamp(value,vec2(0),vec2(1));
}
#endif

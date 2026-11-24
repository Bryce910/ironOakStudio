uniform int uSurfaceWindCount;
uniform vec4 uSurfaceWindCenter[16],uSurfaceWindRadius[16],uSurfaceWindDirection[16];
vec2 studioWind(vec3 world,vec2 base){
    for(int i=0;i<16;i++){
        if(i>=uSurfaceWindCount)break;
        vec3 q=(world-uSurfaceWindCenter[i].xyz)/uSurfaceWindRadius[i].xyz;
        float distance2=dot(q,q);if(distance2>=1.)continue;
        float weight=1.-smoothstep(0.,1.,sqrt(distance2));
        base+=uSurfaceWindDirection[i].xy*uSurfaceWindCenter[i].w*weight;
    }
    float speed=length(base);return speed>5.?base*(5./speed):base;
}

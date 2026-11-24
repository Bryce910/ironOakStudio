// Private city grass math. Also compiled directly by the CPU invariant test.
// All displacement is horizontal and rooted in original model-local height,
// independent of the patch's slope rotation or floating world origin.
struct CityGrassWarp {
    vec3 displacement;
    vec3 derivative; // d(horizontal displacement)/d(world distance along root-up)
    float speed_bound; // conservative world metres/second for temporal confidence
};

CityGrassWarp cityGrassWind(float time,vec2 root,vec2 weatherWind,
                           float modelHeight,float worldHeight,
                           float localHeight,float wind) {
    CityGrassWarp result;
    result.displacement=vec3(0.);
    result.derivative=vec3(0.);
    result.speed_bound=0.;
    if(wind<=0.||localHeight<=0.)return result;
    float h=clamp(localHeight/max(modelHeight,.0001),0.,1.);
    float speed=length(weatherWind);
    vec2 direction=speed>.00001?weatherWind/speed:normalize(vec2(.93,.37));
    vec2 across=vec2(-direction.y,direction.x);
    float phase=dot(root,vec2(.071,.047));
    float gust=sin(time*.95+phase)+.28*sin(time*1.91+phase*.71);
    float amplitude=min(worldHeight*.20,.15)*clamp(wind/.45,0.,2.)*(.70+.30*min(speed,3.));
    vec2 bend=amplitude*(direction*(.35+.65*gust)+across*.16*sin(time*2.10+phase*.90));
    // Preserve a non-folding Jacobian even for small/tilted plants in storms;
    // this is also well inside the existing2m visibility guard.
    float limit=min(.18,worldHeight*.22),magnitude=length(bend);
    if(magnitude>limit)bend*=limit/max(magnitude,.000001);
    vec3 horizontal=vec3(bend.x,0.,bend.y);
    result.displacement=horizontal*h*h;
    // Projection onto the length cap is nonexpansive, so the unclamped
    // sum of directional speed bounds also bounds the final displacement.
    result.speed_bound=amplitude*(.65*(.95+.28*1.91)+.16*2.10)*h*h;
    if(localHeight<=modelHeight)
        result.derivative=horizontal*(2.*h/max(worldHeight,.0001));
    return result;
}

vec3 cityGrassNormal(vec3 normal,vec3 derivative,vec3 rootUp) {
    // Exact inverse-transpose of I + derivative outer rootUp.
    return normal-rootUp*(dot(derivative,normal)/max(1.+dot(rootUp,derivative),.2));
}

vec3 cityGrassTangent(vec3 tangent,vec3 derivative,vec3 rootUp) {
    return tangent+derivative*dot(rootUp,tangent);
}

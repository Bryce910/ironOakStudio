// Shared production math; the CPU invariant test compiles this exact GLSL.
bool grassContactEligible(int kind,float worldHeight,bool motionEnabled) {
    return motionEnabled&&(kind==2||kind==5||(kind==3&&worldHeight<=float(1.1)));
}
vec2 grassContactPush(vec2 anchor,float rootY,vec4 contact,float radius) {
    if(contact.w<=0.)return vec2(0.);
    vec2 delta=anchor-vec2(contact.x,contact.y);
    float squared=dot(delta,delta);
    if(squared>=radius*radius)return vec2(0.);
    float distance=sqrt(squared);
    float weight=(1.-smoothstep(.12,radius,distance))*
                 (1.-smoothstep(.20,.85,abs(rootY-contact.z)))*contact.w;
    // Continuous at zero distance, rather than an arbitrary direction which
    // jumps as a moving foot crosses the root. All contacts have finite slope.
    return delta/max(distance,.12)*weight;
}
RootedGrassWarp grassContactWarp(vec2 push,float modelHeight,float worldHeight,
                                float localHeight,float movementSpeed) {
    RootedGrassWarp result;
    result.displacement=vec3(0.);result.derivative=vec3(0.);result.speed_bound=0.;
    if(localHeight<=0.)return result;
    float magnitude=length(push);
    if(magnitude<=0.)return result;
    if(magnitude>1.)push=push/magnitude;
    float h=clamp(localHeight/max(modelHeight,.0001),0.,1.);
    float tip=min(.30,worldHeight*.40);
    vec3 horizontal=vec3(push.x,0.,push.y)*tip;
    result.displacement=horizontal*h*h;
    if(localHeight<=modelHeight)result.derivative=horizontal*(2.*h/max(worldHeight,.0001));
    // Bounds foot translation (radial field slope<=~12/m), all three
    // smooth history decays and per-update sample replacement. Color TAA
    // must not treat physical contact/recovery as static foliage.
    result.speed_bound=tip*(12.*max(movementSpeed,0.)+24.)*h*h;
    return result;
}
vec3 grassContactNormal(vec3 normal,vec3 windDerivative,vec3 contactDerivative,vec3 rootUp) {
    // Inverse transpose of I + windDerivative outer worldY +
    // contactDerivative outer rootUp. Preserve the existing non-rooted wind.
    float a=1.+windDerivative.y,b=dot(windDerivative,rootUp);
    float c=contactDerivative.y,d=1.+dot(contactDerivative,rootUp);
    float determinant=max(a*d-b*c,.1);
    float w=dot(windDerivative,normal),p=dot(contactDerivative,normal);
    return normal-vec3(0.,1.,0.)*((d*w-b*p)/determinant)-rootUp*((a*p-c*w)/determinant);
}
vec3 grassContactTangent(vec3 tangent,vec3 windDerivative,vec3 contactDerivative,vec3 rootUp) {
    return tangent+windDerivative*tangent.y+contactDerivative*dot(rootUp,tangent);
}
RootedGrassWarp grassContactCombine(RootedGrassWarp wind,RootedGrassWarp contact,
                                   float modelHeight,float worldHeight,float localHeight) {
    if(dot(contact.displacement,contact.displacement)<=0.)return wind;
    RootedGrassWarp result;
    result.displacement=wind.displacement+contact.displacement;
    result.derivative=wind.derivative+contact.derivative;
    result.speed_bound=wind.speed_bound+contact.speed_bound;
    float h=clamp(localHeight/max(modelHeight,.0001),0.,1.);
    float limit=min(.30,worldHeight*.40)*h*h;
    float magnitude=length(result.displacement);
    if(magnitude>limit) {
        float ratio=limit/max(magnitude,.000001);
        result.displacement=result.displacement*ratio;
        result.derivative=result.derivative*ratio;
    }
    // Both components share h^2, so the cap ratio is independent of height.
    // |derivative|<=.8 ensures det(I+d outer rootUp)>=.2 even on slopes.
    return result;
}

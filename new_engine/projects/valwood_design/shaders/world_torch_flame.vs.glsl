#version 330 core
// Twelve analytic quads share one32-byte emitter; the divisor repeats it.
layout(location=0) in vec4 aBaseHeight;
layout(location=1) in vec4 aWidthIntensityPhaseFlicker;
uniform mat4 uVP;
uniform vec3 uRight,uForward;
uniform vec2 uWind;
uniform float uTimePhase,uEyeY,uPixelScale;
out vec2 vUV;
out vec3 vRelative;
out float vWorldY;
flat out vec4 vShape; // height,width,intensity,seed phase
flat out float vFlicker,vAge,vEnergy;
flat out vec3 vRootRelative;
flat out int vLayer;
void main() {
    const vec2 corner[6]=vec2[6](vec2(-1,0),vec2(1,0),vec2(1,1),vec2(-1,0),vec2(1,1),vec2(-1,1));
    vec2 q=corner[gl_VertexID];
    int layer=gl_InstanceID%12;
    float height=aBaseHeight.w,width=aWidthIntensityPhaseFlicker.x,seed=aWidthIntensityPhaseFlicker.z;
    vec3 right=vec3(uRight.x,0,uRight.z);
    right=dot(right,right)>.0001?normalize(right):vec3(1,0,0);
    vec3 toward=vec3(uForward.x,0,uForward.z);
    toward=dot(toward,toward)>.0001?normalize(toward):vec3(0,0,-1);
    vec3 p=aBaseHeight.xyz;
    vAge=0.;vEnergy=1.;
    if(layer<4) {
        // Offset sheets retain a hot volume in overhead/reflection views.
        // Wind bends only the tip; the fuel mouth stays on its authored model.
        float offset=(float(layer)-1.5)*.22;
        if(layer==3)offset+=(q.y-.5)*.70;
        p+=right*q.x*width+toward*offset*width+vec3(0,q.y*height*1.35,0);
        p.xz+=uWind*height*q.y*q.y*.24;
    } else {
        float particle=float(layer-4);
        // Integer cycle counts preserve the64-second seamless phase wrap.
        float cycles=19.+float((layer*7)%23);
        float age=fract(uTimePhase*cycles/6.28318530718+seed*.1591549431+particle*.381966);
        vAge=age;
        float angle=seed+particle*2.399963;
        vec2 radial=vec2(cos(angle),sin(angle));
        vec2 swirl=vec2(sin(age*5.+seed+particle),cos(age*4.+seed-particle));
        vec2 drift=(radial*.85+swirl*.30)*width*age+uWind*height*.50*age*age;
        vec3 center=p+vec3(drift.x,height*(.24+age*2.55),drift.y);
        // Screen-facing splinters follow projected velocity rather than all
        // rising as identical round dots. Expansion is capped at3x physical
        // size and compensated in radiance, so distant embers cannot sparkle
        // as full-bright oversized pixels or exceed the CPU effect envelope.
        vec2 swirlDerivative=vec2(cos(age*5.+seed+particle)*5.,-sin(age*4.+seed-particle)*4.);
        vec2 derivative=(radial*.85+swirl*.30+swirlDerivative*.30*age)*width+uWind*height*age;
        vec3 velocity=vec3(derivative.x,height*2.55,derivative.y);
        vec3 up=normalize(cross(uRight,uForward));
        vec2 projected=vec2(dot(velocity,uRight),dot(velocity,up));
        projected=dot(projected,projected)>.000001?normalize(projected):vec2(0,1);
        vec3 along=uRight*projected.x+up*projected.y;
        vec3 across=uRight*projected.y-up*projected.x;
        float physical=min(min(width*.14,.022),height*.075)*(1.-age*.45);
        float depth=max(dot(center,uForward),.1);
        float halfWidth=max(physical,min(depth/uPixelScale*.52,physical*3.));
        float elongation=1.6+1.8*(.5+.5*sin(seed+particle*3.17));
        float halfLength=max(physical*elongation,halfWidth*.8);
        vEnergy=physical*physical*elongation/max(halfWidth*halfLength,.0000001);
        p=center+across*q.x*halfWidth+along*(q.y*2.-1.)*halfLength;
        p.y=max(p.y,aBaseHeight.y+.01*height);
    }
    vUV=q;vLayer=layer;vShape=vec4(height,width,aWidthIntensityPhaseFlicker.y,seed);
    vFlicker=aWidthIntensityPhaseFlicker.w;vRelative=p;vRootRelative=aBaseHeight.xyz;
    vWorldY=p.y+uEyeY;gl_Position=uVP*vec4(p,1);
}

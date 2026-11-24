#version 330 core
// Bloom mip chain upsample: 3x3 tent filter of the next-smaller level,
// additively blended onto the current level (GL_ONE, GL_ONE). Each level
// keeps its own highlight energy, so wide halos accumulate around bright
// sources without spreading a haze over ordinary surfaces.
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uColor;
uniform vec2 uSourceSize;
uniform float uRadius,uWeight;
vec3 tap(vec2 o){return texture(uColor,clamp(vUV+o*uRadius/uSourceSize,vec2(.5)/uSourceSize,1.-vec2(.5)/uSourceSize)).rgb;}
void main(){
    vec3 color=tap(vec2(0))*4.;
    color+=(tap(vec2(-1,0))+tap(vec2(1,0))+tap(vec2(0,-1))+tap(vec2(0,1)))*2.;
    color+=tap(vec2(-1,-1))+tap(vec2(1,-1))+tap(vec2(-1,1))+tap(vec2(1,1));
    FragColor=vec4(color*(uWeight/16.),0.);
}

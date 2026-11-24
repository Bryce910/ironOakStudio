#version 330 core
// Bloom mip chain downsample (Jimenez, "Next Generation Post Processing in
// Call of Duty: Advanced Warfare", 2014). Thirteen bilinear taps cover a
// 6x6 source footprint; overlapping 4-tap groups suppress aliasing/flicker.
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uColor;
uniform vec2 uSourceSize;
vec3 tap(vec2 o){return texture(uColor,clamp(vUV+o/uSourceSize,vec2(.5)/uSourceSize,1.-vec2(.5)/uSourceSize)).rgb;}
void main(){
    vec3 a=tap(vec2(-2, 2)),b=tap(vec2(0, 2)),c=tap(vec2(2, 2));
    vec3 d=tap(vec2(-2, 0)),e=tap(vec2(0, 0)),f=tap(vec2(2, 0));
    vec3 g=tap(vec2(-2,-2)),h=tap(vec2(0,-2)),i=tap(vec2(2,-2));
    vec3 j=tap(vec2(-1, 1)),k=tap(vec2(1, 1)),l=tap(vec2(-1,-1)),m=tap(vec2(1,-1));
    vec3 color=e*.125+(a+c+g+i)*.03125+(b+d+f+h)*.0625+(j+k+l+m)*.125;
    FragColor=vec4(max(color,vec3(0)),0.);
}

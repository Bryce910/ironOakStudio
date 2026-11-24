#version 330 core
in vec2 vUV;
uniform vec3 uColor;
uniform float uFade;
out vec4 FragColor;
void main(){
    vec2 uv=vUV*2.0-1.0;
    if(length(uv)>1.0)discard;
    FragColor=vec4(uColor,uFade);
}

#version 330 core
in vec2 vUV;
uniform sampler2D uColor;
layout(location=0) out vec4 FragColor;
void main(){FragColor=texture(uColor,vUV);}

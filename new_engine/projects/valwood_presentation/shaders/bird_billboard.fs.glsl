#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform bool uHasTex;
uniform float uFade;
out vec4 FragColor;
void main(){
    if(uHasTex){
        vec4 c=texture(uTex,vUV);
        if(c.a<0.2)discard;
        FragColor=vec4(c.rgb,c.a*uFade);
    } else {
        vec2 uv=vUV*2.0-1.0;
        float wing=abs(uv.x)-abs(uv.y)*1.5;
        if(wing>0.0||abs(uv.y)>0.25)discard;
        FragColor=vec4(0.08,0.06,0.06,uFade);
    }
}

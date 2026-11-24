#version 330 core
in vec2  vUV;
in float vHF;
in vec3  vWorldPos;
uniform float uTime;
out vec4 FragColor;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float vnoise(vec2 p){
    vec2 i=floor(p);vec2 f=fract(p);
    vec2 u=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),u.x),
               mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),u.x),u.y);
}
void main(){
    vec2 flowUV=vec2(vUV.x*4.0, vUV.y*6.0 - uTime*3.5);
    float s =vnoise(flowUV);
    float s2=vnoise(flowUV*2.1+vec2(3.7,1.3));
    float streak=mix(s,s2,0.4);
    streak=pow(streak,1.8);
    float ex=vUV.x*2.0-1.0;
    float edge=1.0-ex*ex;
    edge=edge*edge;
    float foam=exp(-vHF*8.0)*0.6;
    vec3 deepCol =vec3(0.55,0.75,0.95);
    vec3 foamCol =vec3(0.90,0.95,1.00);
    vec3 col=mix(deepCol,foamCol,streak*0.5+foam);
    col+=vec3(0.3)*pow(streak,3.0);
    float alpha=(0.35+streak*0.35+foam*0.5)*edge;
    alpha=clamp(alpha,0.0,0.92);
    FragColor=vec4(col,alpha);
}

#version 330 core
in vec2 vUV;
in float vAge;
flat in float vSide;
uniform int uMode;
uniform vec3 uSky,uSun;
out vec4 FragColor;
void main(){
    float alpha;vec3 color;
    vec3 light=uSky*1.55+uSun*.44+vec3(.025);
    if(uMode==0){
        float toe=length((vUV-vec2(.04*vSide,.30))/vec2(.70,.53))-1.;
        float heel=length((vUV-vec2(-.06*vSide,-.53))/vec2(.47,.29))-1.;
        float arch=length((vUV-vec2(-.06*vSide,-.07))/vec2(.49,.46))-1.;
        float d=min(toe,min(heel,arch));
        float rim=exp(-pow((d-.045)*8.,2.));
        float inside=1.-smoothstep(-.13,.035,d);
        float tread=smoothstep(.60,.90,.5+.5*cos(vUV.y*42.))*inside*.10;
        color=light*mix(vec3(.22,.26,.30),vec3(.83,.88,.94),rim*.32);
        color*=1.-tread-.10*smoothstep(-.45,-.02,d);
        alpha=max(inside*.68,rim*.16)*(1.-smoothstep(90.,180.,vAge));
    }else{
        alpha=(1.-smoothstep(.1,1.,length(vUV)))*sin(clamp(vAge/.95,0.,1.)*3.14159)*.30;
        color=light*vec3(.85,.93,1.);
    }
    if(alpha<.005)discard;
    FragColor=vec4(color,alpha);
}

#version 330 core
in vec2 vUV;
in float vType;
in float vColorVar;
in float vFade;
in vec3 vWorld;
uniform vec3 uFogColor;
uniform vec3 uCamPos;
uniform float uFogDensity,uHeightFogDensity,uHeightFogBase;
uniform float uSunY;
out vec4 FragColor;
vec3 petal(float t){ if(t<0.5) return vec3(0.95,0.95,0.95); if(t<1.5) return vec3(0.90,0.24,0.24); if(t<2.5) return vec3(0.95,0.88,0.22); return vec3(0.86,0.96,0.86); }
vec3 center(float t){ if(t<0.5) return vec3(0.95,0.80,0.10); if(t<1.5) return vec3(0.20,0.08,0.05); if(t<2.5) return vec3(0.86,0.64,0.10); return vec3(0.76,0.90,0.24); }
void main(){
    vec2 uvc=(vUV-0.5)*2.0;
    float r=length(uvc);
    if(r>1.0) discard;
    float theta=atan(uvc.y,uvc.x);
    float petalMask=0.0;
    float petalR=0.0;
    if(vType<0.5){ petalR=0.42+cos(theta*8.0)*0.38; petalMask=(r<petalR&&r>0.18)?1.0:0.0; }
    else if(vType<1.5){ petalR=0.50+cos(theta*5.0)*0.32; petalMask=(r<petalR&&r>0.15)?1.0:0.0; }
    else if(vType<2.5){ petalR=0.35+cos(theta*16.0)*0.48; petalMask=(r<petalR&&r>0.10)?1.0:0.0; }
    else { float lobe=abs(cos(theta*2.0)); petalR=lobe*0.72; petalMask=(r<petalR)?1.0:0.0; }
    float stamen=(r<0.20)?1.0:0.0;
    if(petalMask<0.5&&stamen<0.5) discard;
    vec3 p=clamp(petal(vType)+(vColorVar*0.06),0.0,1.0);
    vec3 c=center(vType);
    vec3 col=stamen>0.5?c:p;
    float sunY=clamp(uSunY,0.0,1.0);
    float shade=0.70+sunY*0.30;
    col*=shade*(1.0-r*0.18);
    float fogDist=length(vWorld-uCamPos);
    float distFog=1.0-exp(-uFogDensity*uFogDensity*fogDist*fogDist);
    float hfog=1.0-exp(-uHeightFogDensity*max(0.0,uHeightFogBase-vWorld.y));
    col=mix(col,uFogColor,clamp(distFog*(1.0+hfog*4.0),0.0,0.82));
    FragColor=vec4(col,vFade);
}

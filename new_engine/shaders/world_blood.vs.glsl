#version 330 core
layout(location=0)in vec3 aPosition;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec3 bPosition;
layout(location=3)in vec3 bNormal;
uniform mat4 uVP;
uniform vec3 uOrigin;
uniform vec2 uRotation,uSize;
uniform float uWeight,uScale;
uniform int uMode;
out vec3 vNormal,vPosition;
out vec2 vUV;
out float vDepth;
vec3 rotateY(vec3 p){return vec3(uRotation.x*p.x+uRotation.y*p.z,p.y,-uRotation.y*p.x+uRotation.x*p.z);}
void main(){
    vec3 p,n;
    if(uMode==0){p=mix(aPosition,bPosition,uWeight);n=mix(aNormal,bNormal,uWeight);vUV=vec2(0);}
    else{
        const vec2 corners[6]=vec2[6](vec2(-.5,-.5),vec2(.5,-.5),vec2(.5,.5),vec2(-.5,-.5),vec2(.5,.5),vec2(-.5,.5));
        vec2 q=corners[gl_VertexID];p=vec3(q.x*uSize.x,0,q.y*uSize.y);n=vec3(0,1,0);vUV=q+.5;
    }
    vNormal=rotateY(n);vPosition=uOrigin+rotateY(p*uScale);if(uMode==1)vPosition.y+=.003;
    vec4 clip=uVP*vec4(vPosition,1);vDepth=clip.w;gl_Position=clip;
}

#version 330 core
in vec3 vPos;
in float vFoam;
uniform float uTime;
uniform vec3 uSunColor,uAmbient,uCamPos,uSunDir;
out vec4 FragColor;
void main(){
    vec3 dx=dFdx(vPos), dz=dFdy(vPos);
    vec3 n=normalize(cross(dz,dx));
    if(n.y<0.0)n=-n;
    vec3 base=vec3(0.06,0.22,0.52);
    float diff=max(dot(n,-uSunDir),0.0);
    vec3 col=base*(uAmbient+uSunColor*diff*0.9);
    vec3 view=normalize(uCamPos-vPos);
    vec3 refl=reflect(uSunDir,n);
    col+=vec3(0.9)*pow(max(dot(refl,view),0.0),48.0)*0.6;
    col=mix(col,vec3(0.88,0.93,0.98),vFoam*0.35);
    float edgeDist=length(vPos.xz-uCamPos.xz);
    float edge=clamp(1.0-edgeDist/22.0,0.0,1.0);
    float alpha=clamp(edge*4.0,0.0,1.0)*(0.18+vFoam*0.18);
    FragColor=vec4(col,alpha);
}

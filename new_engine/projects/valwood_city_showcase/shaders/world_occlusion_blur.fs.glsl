#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uField;
uniform vec2 uHalfSize,uDirection;
void main(){
    vec4 center=texture(uField,vUV);vec2 sum=center.rg;float total=1.;
    float threshold=.035+center.b*.006;
    for(int i=1;i<=3;i++)for(int sign_=-1;sign_<=1;sign_+=2){
        vec2 uv=clamp(vUV+uDirection*float(i*sign_)/uHalfSize,vec2(.5)/uHalfSize,1.-vec2(.5)/uHalfSize);
        vec4 value=texture(uField,uv);
        float weight=exp(-float(i*i)*.25)*max(0.,1.-abs(value.b-center.b)/threshold);
        sum+=value.rg*weight;total+=weight;
    }
    FragColor=vec4(sum/total,center.ba);
}

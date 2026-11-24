#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uColor;
uniform vec2 uSourceSize,uDirection;
void main(){
    // Symmetric normalized nine-tap Gaussian: preserves a uniform radiance
    // field and RGB ratios. Edge clamping avoids wraparound light leakage.
    const float weight[5]=float[5](.2270270270,.1945945946,.1216216216,.0540540541,.0162162162);
    vec3 color=texture(uColor,vUV).rgb*weight[0];
    for(int i=1;i<=4;i++){
        vec2 offset=uDirection*float(i)/uSourceSize;
        color+=(texture(uColor,clamp(vUV+offset,vec2(.5)/uSourceSize,1.-vec2(.5)/uSourceSize)).rgb+
                texture(uColor,clamp(vUV-offset,vec2(.5)/uSourceSize,1.-vec2(.5)/uSourceSize)).rgb)*weight[i];
    }
    FragColor=vec4(color,0.);
}

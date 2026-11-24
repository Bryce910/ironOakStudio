#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uColor,uDepth,uFogField;
uniform vec2 uResolution,uHalfSize,uNearFar;
uniform float uFogStrength;
uniform bool uFogActive;
float linear_depth(float d) {
    return uNearFar.x*uNearFar.y/max(uNearFar.y-d*(uNearFar.y-uNearFar.x),.00001);
}
void main() {
    vec4 source=texture(uColor,vUV);
    float depth=texture(uDepth,vUV).r;
    if(!uFogActive||depth>=.999999){FragColor=source;return;}
    float center=linear_depth(depth);
    vec2 p=vUV*uHalfSize-.5,base=floor(p),fraction=fract(p);
    vec4 field=vec4(0);float total=0.0;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++) {
        vec2 offset=vec2(x,y);
        vec2 uv=clamp((base+offset+.5)/uHalfSize,.5/uHalfSize,1.0-.5/uHalfSize);
        vec2 axis=mix(1.0-fraction,fraction,offset);
        float separation=abs(linear_depth(texture(uDepth,uv).r)-center)/max(.25,center*.025);
        float weight=axis.x*axis.y/(1.0+separation*separation);
        field+=texture(uFogField,uv)*weight;total+=weight;
    }
    field/=max(total,.000001);
    FragColor=vec4(source.rgb*clamp(field.a,0.0,1.0)+field.rgb,source.a);
}

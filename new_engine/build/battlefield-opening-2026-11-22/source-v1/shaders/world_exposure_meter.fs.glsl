#version 330 core
// Auto-exposure metering: 64x64 centre-weighted log2 luminance of the
// scene-linear HDR image (before exposure). Four taps per output texel; the
// mip chain reduces r/g to the weighted log-average.
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uColor;
uniform vec2 uSourceSize;
void main(){
    const vec2 cell=vec2(1./64.);
    float sum=0.;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        vec2 uv=vUV+(vec2(x,y)-.5)*cell*.5;
        vec3 c=texture(uColor,clamp(uv,vec2(.5)/uSourceSize,1.-vec2(.5)/uSourceSize)).rgb;
        float lum=dot(c,vec3(.2126,.7152,.0722));
        lum=(isnan(lum)||isinf(lum))?1.:clamp(lum,1./16384.,256.);
        sum+=log2(lum);
    }
    // Centre-weighted: the subject and path ahead matter more than the sky edge.
    vec2 d=(vUV-.5)*vec2(1.,1.25);
    float weight=mix(.30,1.,1.-smoothstep(.15,.62,length(d)));
    FragColor=vec4(sum*.25*weight,weight,0.,1.);
}

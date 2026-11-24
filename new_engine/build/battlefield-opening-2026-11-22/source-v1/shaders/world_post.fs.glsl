#version 330 core

in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene, uDepth;
uniform float uDetailSharpness;
uniform float uExposure, uNear, uFar;
uniform vec2 uResolution;
uniform bool uTemporalActive;
uniform sampler3D uVillageGrade;
uniform float uVillageGradeWeight;
uniform sampler2D uBloom;
uniform float uBloomStrength;
uniform float uUnderwaterBlend,uSubmergeDepth,uWaterTime;
uniform sampler2D uHandsDepth;uniform bool uHandsActive;
// Explicit character portrait only. The ordinary walking resolve is identical.
uniform float uPortraitFocus;
uniform float uCampGrade,uFilmGrain,uFilmTime;
float eye_depth(vec2 uv){float z=texture(uDepth,uv).r;return uNear*uFar/max(uFar-z*(uFar-uNear),.0001);}
vec3 filmic(vec3 x) {
    vec3 fitted=(x*(2.51*x+.03))/(x*(2.43*x+.59)+.14);
    // Keep the existing toe/midtones exactly. The fitted curve crosses1 at
    // high radiance, flattening bright armor into a white plateau. Continue
    // its value and slope at2 with a bounded, monotonic shoulder instead.
    // HDR detail remains finite without lowering the whole scene exposure.
    const float join=2.;
    const float value=10.10/11.04;
    const float slope=(10.07*11.04-10.10*10.31)/(11.04*11.04);
    vec3 shoulder=vec3(1.)-(1.-value)/(1.+max(x-join,vec3(0.))*slope/(1.-value));
    return max(mix(fitted,shoulder,greaterThan(x,vec3(join))),vec3(0.));
}
// Display rendering. 1 = gamut-compressed curve (default), 0 = the previous
// per-channel curve, kept for A/B comparison.
#define VALWOOD_HUE_STABLE_TONEMAP 1
// AgX-style inset/outset around the established curve (Sobotka/Wrensch
// Rec.709 matrices). Both matrices map grey to grey, so neutral exposure,
// contrast and the authored grade response are unchanged. Saturated HDR
// light (fire, sunset, sky) is pulled toward the neutral axis before the
// per-channel shoulder, so it no longer skews toward yellow/cyan, then the
// outset restores chroma that remains in gamut.
const mat3 TONEMAP_INSET=mat3(
    .842479062253094,.0423282422610123,.0423756549057051,
    .0784335999999992,.878468636469772,.0784336,
    .0792237451477643,.0791661274605434,.879142973793104);
const mat3 TONEMAP_OUTSET=mat3(
    1.19687900512017,-.0528968517574562,-.0529716355144438,
    -.0980208811401368,1.15190312990417,-.0980434501171241,
    -.0990297440797205,-.0989611768448433,1.15107367264116);
vec3 display_tonemap(vec3 x) {
#if VALWOOD_HUE_STABLE_TONEMAP
    return clamp(TONEMAP_OUTSET*filmic(max(TONEMAP_INSET*x,vec3(0.))),0.,1.);
#else
    return filmic(x);
#endif
}
// Interleaved gradient noise (Jimenez 2014), decorrelated per channel and
// summed to a triangular distribution of +-1 output code value. Removes the
// 8-bit banding in sky, fog and dim interiors without visible grain.
float tonemap_ign(vec2 p){return fract(52.9829189*fract(dot(p,vec2(.06711056,.00583715))));}
vec3 display_dither(vec2 p) {
    vec3 a=vec3(tonemap_ign(p),tonemap_ign(p+vec2(47.,17.)),tonemap_ign(p+vec2(13.,71.)));
    vec3 b=vec3(tonemap_ign(p+vec2(89.,5.)),tonemap_ign(p+vec2(31.,113.)),tonemap_ign(p+vec2(151.,59.)));
    return (a+b-1.)/255.;
}
vec3 display_srgb(vec3 linear) {
    // The scene and temporal history stay linear HDR. The default framebuffer
    // has sRGB conversion disabled, so encode exactly once, including its
    // linear toe instead of lifting dark stone/wood with an approximate gamma.
    return mix(1.055 * pow(linear, vec3(1.0 / 2.4)) - .055,
               linear * 12.92, lessThanEqual(linear, vec3(.0031308)));
}
// Upscaling (AAA candidate): when the scene renders below display size,
// reconstruct with a 5-tap bilinear-optimised Catmull-Rom (sharper than plain
// bilinear, no ringing worth clamping at <=1.43x) and sharpen in proportion to
// the scale factor. uOutputResolution 0 or equal size keeps the native path.
uniform vec2 uOutputResolution=vec2(0.);
uniform float uTaauSharpen=0.; // AAA merge: restoring contrast after temporal upscaling
vec3 scene_catmull_rom(vec2 uv) {
    vec2 size=uResolution,position=uv*size,center=floor(position-.5)+.5,f=position-center;
    vec2 w0=f*(-.5+f*(1.-.5*f)),w1=1.+f*f*(-2.5+1.5*f),w2=f*(.5+f*(2.-1.5*f)),w3=f*f*(-.5+.5*f);
    vec2 w12=w1+w2,t0=(center-1.)/size,t3=(center+2.)/size,t12=(center+w2/w12)/size;
    vec3 r=texture(uScene,vec2(t12.x,t0.y)).rgb*(w12.x*w0.y)+
           texture(uScene,vec2(t0.x,t12.y)).rgb*(w0.x*w12.y)+
           texture(uScene,t12).rgb*(w12.x*w12.y)+
           texture(uScene,vec2(t3.x,t12.y)).rgb*(w3.x*w12.y)+
           texture(uScene,vec2(t12.x,t3.y)).rgb*(w12.x*w3.y);
    float norm=w12.x*w0.y+w0.x*w12.y+w12.x*w12.y+w3.x*w12.y+w12.x*w3.y;
    return max(r/max(norm,1e-4),vec3(0));
}
void main() {
    // Tiny optical distortion applies to every scene/depth tap together.
    vec2 uv=vUV;
    if(uUnderwaterBlend>0.)uv=clamp(vUV+vec2(sin(vUV.y*19.+uWaterTime*1.4),cos(vUV.x*17.-uWaterTime*1.1))*.00065*uUnderwaterBlend,vec2(.001),vec2(.999));
    float exposure = max(uExposure, 0.0);
    vec2 pixel = 1.0 / uResolution;
    float upscale=uOutputResolution.x>0.?uOutputResolution.x/uResolution.x:1.;
    vec3 center = upscale>1.02?scene_catmull_rom(uv):max(texture(uScene, uv).rgb, vec3(0));
    vec3 north = max(texture(uScene, uv + vec2(0, pixel.y)).rgb, vec3(0));
    vec3 south = max(texture(uScene, uv - vec2(0, pixel.y)).rgb, vec3(0));
    vec3 east = max(texture(uScene, uv + vec2(pixel.x, 0)).rgb, vec3(0));
    vec3 west = max(texture(uScene, uv - vec2(pixel.x, 0)).rgb, vec3(0));
    vec3 luminance = vec3(.2126, .7152, .0722);
    float c = dot(filmic(center * exposure), luminance);
    float n = dot(filmic(north * exposure), luminance);
    float s = dot(filmic(south * exposure), luminance);
    float e = dot(filmic(east * exposure), luminance);
    float w = dot(filmic(west * exposure), luminance);
    float low = min(c, min(min(n, s), min(e, w)));
    float high = max(c, max(max(n, s), max(e, w)));
    float edge = smoothstep(max(.035, high * .10), max(.09, high * .30), high - low);
    // Filter high-contrast edges while preserving photographic surface detail.
    vec3 average=(north+south+east+west)*.25;
    vec3 color = mix(center, average, edge * (uTemporalActive?.045:.27));
    if(uTemporalActive) {
        // Restore minified texture contrast using existing taps. Suppress
        // sharpening on silhouettes/highlights; a relative bound prevents
        // bright halos or negative ringing against dark foliage.
        float upscale_sharpen=clamp((upscale-1.)*1.1,0.,.5)+uTaauSharpen;
        vec3 detail=(center-average)*(clamp(uDetailSharpness,0.,.65)+upscale_sharpen)*(1.-edge*.85);
        vec3 limit=max(center,vec3(.025))*(.10+upscale_sharpen*.12);
        color=max(color+clamp(detail,-limit,limit),vec3(0));
    }
    if(uPortraitFocus>0.) {
        float depth=eye_depth(uv);
        float coc=smoothstep(.32,2.8,abs(depth-uPortraitFocus));
        if(coc>.001) {
            const vec2 disk[12]=vec2[](vec2(.52,.12),vec2(-.42,.38),vec2(.12,-.54),vec2(.36,.72),vec2(-.74,-.12),vec2(-.26,-.81),vec2(.86,-.46),vec2(-.61,.72),vec2(.03,.96),vec2(.73,.62),vec2(-.94,.18),vec2(.44,-.88));
            vec3 sum=color;float weight=1.;
            for(int i=0;i<12;i++) {
                vec2 q=clamp(uv+disk[i]*pixel*(coc*6.),pixel*.5,1.-pixel*.5);
                // Background taps cannot drag a sharp helmet/hand silhouette
                // into the background, even at a near/far depth discontinuity.
                float sampleDepth=eye_depth(q);
                float keep=depth>uPortraitFocus+.5?step(uPortraitFocus+.4,sampleDepth):1.;
                sum+=max(texture(uScene,q).rgb,vec3(0))*keep;weight+=keep;
            }
            color=mix(color,sum/weight,coc);
        }
    }
    // Highlight energy is filtered at quarter resolution in scene-linear HDR.
    // Composite before exposure/tone mapping; surfaces below the soft threshold
    // keep their contrast instead of receiving an all-screen haze.
    vec3 bloom=uBloomStrength>0.?max(texture(uBloom,uv).rgb,vec3(0)):vec3(0);
    if(uUnderwaterBlend>0.) {
        // Metre-scale extinction removes distant above-water silhouettes while
        // keeping nearby fish/riverbed readable. Red light absorbs first.
        float distance=min(eye_depth(uv),240.);
        if(uHandsActive){float d=texture(uHandsDepth,uv).r;if(d<.9999)distance=.025*10./max(10.-d*(10.-.025),.00001);}
        vec3 transmission=exp(-distance*vec3(.22,.085,.060));
        vec3 riverLight=vec3(.035,.16,.145)*exp(-max(uSubmergeDepth,0.)*.09);
        vec3 submerged=color*transmission+riverLight*(1.-transmission);
        color=mix(color,submerged,clamp(uUnderwaterBlend,0.,1.));
        bloom*=mix(vec3(1),transmission,clamp(uUnderwaterBlend,0.,1.));
    }
    color = display_tonemap((color + bloom * uBloomStrength) * exposure);
    float value = dot(color, luminance);
    color = mix(vec3(value), color, .98);
    // Gentle cool forest shadows and warm highlights; history remains HDR
    // and all UI is composed later. The surrounding areas retain their grade.
    vec3 forestTint=mix(vec3(.970,1.005,1.025),vec3(1.015,1.003,.985),smoothstep(.1,.75,value));
    color*=mix(vec3(1.),forestTint,clamp(uCampGrade,0.,1.));
    color = display_srgb(max(color, vec3(0)));
    if(uVillageGradeWeight>0.0) {
        // A16-cube's endpoints lie at texel centres. Grade display values once;
        // keeping this after temporal resolve leaves its HDR history unbiased.
        vec3 graded=texture(uVillageGrade,clamp(color,0.0,1.0)*(15.0/16.0)+vec3(.5/16.0)).rgb;
        color=mix(color,graded,clamp(uVillageGradeWeight,0.0,1.0));
    }
    float vignette = 1.0 - .055 * pow(length((uv - .5) * 1.25), 2.0);
    float grain=(tonemap_ign(gl_FragCoord.xy+floor(uFilmTime*24.)*vec2(17.,31.))-.5)*uFilmGrain/255.;
    FragColor = vec4(clamp(color * vignette + display_dither(gl_FragCoord.xy)+grain, 0., 1.), 1);
}

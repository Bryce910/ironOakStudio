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
vec3 filmic(vec3 x) {
    return clamp((x * (2.51 * x + .03)) / (x * (2.43 * x + .59) + .14), 0.0, 1.0);
}
vec3 display_srgb(vec3 linear) {
    // The scene and temporal history stay linear HDR. The default framebuffer
    // has sRGB conversion disabled, so encode exactly once, including its
    // linear toe instead of lifting dark stone/wood with an approximate gamma.
    return mix(1.055 * pow(linear, vec3(1.0 / 2.4)) - .055,
               linear * 12.92, lessThanEqual(linear, vec3(.0031308)));
}
void main() {
    float exposure = max(uExposure, 0.0);
    vec2 pixel = 1.0 / uResolution;
    vec3 center = max(texture(uScene, vUV).rgb, vec3(0));
    vec3 north = max(texture(uScene, vUV + vec2(0, pixel.y)).rgb, vec3(0));
    vec3 south = max(texture(uScene, vUV - vec2(0, pixel.y)).rgb, vec3(0));
    vec3 east = max(texture(uScene, vUV + vec2(pixel.x, 0)).rgb, vec3(0));
    vec3 west = max(texture(uScene, vUV - vec2(pixel.x, 0)).rgb, vec3(0));
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
        vec3 detail=(center-average)*clamp(uDetailSharpness,0.,.65)*(1.-edge*.85);
        vec3 limit=max(center,vec3(.025))*.10;
        color=max(color+clamp(detail,-limit,limit),vec3(0));
    }
    // Highlight energy is filtered at quarter resolution in scene-linear HDR.
    // Composite before exposure/tone mapping; surfaces below the soft threshold
    // keep their contrast instead of receiving an all-screen haze.
    vec3 bloom=uBloomStrength>0.?max(texture(uBloom,vUV).rgb,vec3(0)):vec3(0);
    color = filmic((color + bloom * uBloomStrength) * exposure);
    float value = dot(color, luminance);
    color = mix(vec3(value), color, .98);
    color = display_srgb(max(color, vec3(0)));
    if(uVillageGradeWeight>0.0) {
        // A16-cube's endpoints lie at texel centres. Grade display values once;
        // keeping this after temporal resolve leaves its HDR history unbiased.
        vec3 graded=texture(uVillageGrade,clamp(color,0.0,1.0)*(15.0/16.0)+vec3(.5/16.0)).rgb;
        color=mix(color,graded,clamp(uVillageGradeWeight,0.0,1.0));
    }
    float vignette = 1.0 - .055 * pow(length((vUV - .5) * 1.25), 2.0);
    FragColor = vec4(color * vignette, 1);
}

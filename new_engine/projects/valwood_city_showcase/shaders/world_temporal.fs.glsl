#version 330 core
in vec2 vUV;
layout(location = 0) out vec4 ResolvedColor;
layout(location = 1) out float ResolvedDepth;
uniform sampler2D uCurrentColor, uCurrentDepth, uHistoryColor, uHistoryDepth;
uniform mat4 uReprojection, uSkyReprojection;
uniform vec2 uResolution, uNearFar, uPreviousNearFar;
uniform vec2 uCurrentJitterUV, uPreviousJitterUV;
uniform bool uHasHistory, uReactiveAlpha;
uniform float uHistoryWeight;

vec3 to_ycocg(vec3 c) {
    return vec3(dot(c, vec3(.25, .5, .25)), .5 * (c.r - c.b), .5 * c.g - .25 * (c.r + c.b));
}
vec3 from_ycocg(vec3 c) {
    return vec3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}
float linear_depth(float depth, vec2 planes) {
    return 2.0 * planes.x * planes.y /
           max(planes.y + planes.x - (depth * 2.0 - 1.0) * (planes.y - planes.x), 1.e-5);
}
// The HDR sample is reconstructed at (pixel center + projection jitter).
// Its depth must describe that same point, rather than the nearest jittered
// sample. On a rasterized triangle device depth is screen-linear. Estimate
// its two slopes from the smaller one-sided depth difference so a silhouette
// never averages an unrelated foreground/background depth into that plane.
// This also works at a cliff next to sky, where the valid surface side wins.
float plane_depth(ivec2 at, ivec2 size, float center) {
    if (center >= 1.0 || all(equal(uCurrentJitterUV, vec2(0)))) return center;
    float left = texelFetch(uCurrentDepth, clamp(at - ivec2(1,0), ivec2(0), size - 1), 0).r;
    float right = texelFetch(uCurrentDepth, clamp(at + ivec2(1,0), ivec2(0), size - 1), 0).r;
    float down = texelFetch(uCurrentDepth, clamp(at - ivec2(0,1), ivec2(0), size - 1), 0).r;
    float up = texelFetch(uCurrentDepth, clamp(at + ivec2(0,1), ivec2(0), size - 1), 0).r;
    float dx0 = center - left, dx1 = right - center;
    float dy0 = center - down, dy1 = up - center;
    float dx = abs(dx0) < abs(dx1) ? dx0 : dx1;
    float dy = abs(dy0) < abs(dy1) ? dy0 : dy1;
    // A one-pixel leaf can have neither neighbor on its surface. In that case
    // avoid manufacturing a depth between layers; the conservative point
    // sample retains the existing disocclusion behavior.
    float center_view = linear_depth(center, uNearFar);
    float threshold = max(.075, center_view * .01);
    // A steep but continuous plane is valid too: its opposite one-sided
    // slopes agree, even when one screen pixel spans more than 1% in range.
    bool x_plane = abs(dx0 - dx1) <= max(2.e-7, max(abs(dx0), abs(dx1)) * .05);
    bool y_plane = abs(dy0 - dy1) <= max(2.e-7, max(abs(dy0), abs(dy1)) * .05);
    bool x_surface = x_plane || min(abs(linear_depth(left, uNearFar) - center_view),
                                    abs(linear_depth(right, uNearFar) - center_view)) <= threshold;
    bool y_surface = y_plane || min(abs(linear_depth(down, uNearFar) - center_view),
                                    abs(linear_depth(up, uNearFar) - center_view)) <= threshold;
    vec2 offset = uCurrentJitterUV * uResolution;
    return clamp(center + (x_surface ? dx * offset.x : 0.0) +
                          (y_surface ? dy * offset.y : 0.0), 0.0, 1.0);
}
// The .995 source tag survives RGBA16F as .995117. Its reserved band is
// separate from foliage<=.98, weak actors<=.99, and ordinary opaque/sky1.
bool rigid_tag(float alpha) { return alpha > .993 && alpha < .997; }
bool coverage_tag(float alpha) { return (alpha >= .4 && alpha < .99) || rigid_tag(alpha); }
void main() {
    ivec2 size = ivec2(uResolution);
    vec2 half_pixel = .5 / uResolution;
    vec2 current_uv = clamp(vUV + uCurrentJitterUV, half_pixel, 1.0 - half_pixel);
    ivec2 at = clamp(ivec2(current_uv * uResolution), ivec2(0), size - 1);
    vec4 current = texture(uCurrentColor, current_uv);
    current.rgb = clamp(current.rgb, vec3(0), vec3(60000));
    float depth = plane_depth(at, size, texelFetch(uCurrentDepth, at, 0).r);
    bool sky = depth >= 1.0;
    float view_depth = sky ? 0.0 : linear_depth(depth, uNearFar);
    // Evaluate derivatives before any per-pixel early return.
    float depth_gradient = abs(dFdx(view_depth)) + abs(dFdy(view_depth));
    ResolvedDepth = depth;
    ResolvedColor = vec4(current.rgb, 1.0);
    if (!uHasHistory) return;

    vec4 clip = vec4(current_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 previous_clip = sky ? uSkyReprojection * clip : uReprojection * clip;
    if (previous_clip.w <= 1.e-6) return;
    vec3 previous_ndc = previous_clip.xyz / previous_clip.w;
    vec2 previous_uv = previous_ndc.xy * .5 + .5 - uPreviousJitterUV;
    if (any(lessThan(previous_uv, half_pixel)) || any(greaterThan(previous_uv, 1.0 - half_pixel))) return;
    if (!sky && (previous_ndc.z <= -1.0 || previous_ndc.z >= 1.0)) return;
    float expected = sky ? 0.0 : linear_depth(previous_ndc.z * .5 + .5, uPreviousNearFar);
    // Relative tolerance accommodates 24-bit far depth quantization. Local
    // derivatives allow slopes, but are capped to reject silhouette jumps.
    float slope = min(depth_gradient, expected * .003);
    float tolerance = max(.075, expected * .0025) + slope;
    // Reconstruct bilinear history from matching-depth taps only. Rejecting
    // the entire footprint whenever one tap touched a grass blade discarded
    // history along almost every thin leaf, even when that tap's weight was
    // zero. Sampling ordinary bilinear color after a nearest-depth check would
    // instead admit foreground halos, so invalid taps must never add color.
    vec2 history_pixel = previous_uv * uResolution - .5;
    ivec2 history_at = ivec2(floor(history_pixel));
    vec2 fraction = fract(history_pixel);
    float valid_weights[4], old_depths[4], bilinear_weights[4];
    bool depth_matches[4];
    float valid_mass = 0.0, original_valid_mass = 0.0;
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
        int k = y * 2 + x;
        float old_depth = texelFetch(uHistoryDepth, clamp(history_at + ivec2(x,y), ivec2(0), size - 1), 0).r;
        bool valid = sky ? old_depth >= 1.0 : old_depth < 1.0 &&
            abs(linear_depth(old_depth, uPreviousNearFar) - expected) <= tolerance;
        float bilinear_weight = (x == 0 ? 1.0 - fraction.x : fraction.x) *
                                (y == 0 ? 1.0 - fraction.y : fraction.y);
        old_depths[k] = old_depth; bilinear_weights[k] = bilinear_weight;
        depth_matches[k] = valid;
        original_valid_mass += valid ? bilinear_weight : 0.0;
    }
    // A jittered alpha-cutout alternates leaf/sky (or leaf/ground) at a single
    // display pixel. Strict center-layer rejection makes that legitimate area
    // coverage flicker forever. Retain a history layer only while the same
    // layer is still represented within one current pixel and at least one
    // side has an explicit vegetation or transform-static rigid source tag.
    // Untagged opaque/terrain disocclusion stays strict. A vanished layer is
    // rejected, and reactive foregrounds cannot admit a rigid background.
    float coverage_stability = 1.0;
    bool reconstructed_coverage = false;
    if (original_valid_mass < .999 && uReactiveAlpha) {
        float support_depths[9], support_alpha[9];
        float point_alpha=texelFetch(uCurrentColor,at,0).a;
        bool current_coverage = !sky && coverage_tag(point_alpha);
        bool reactive_foreground = !sky && point_alpha < .4;
        bool has_coverage = false;
        for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
            int k = (y + 1) * 3 + x + 1;
            ivec2 sample_at = clamp(at + ivec2(x,y), ivec2(0), size - 1);
            support_depths[k] = texelFetch(uCurrentDepth, sample_at, 0).r;
            support_alpha[k] = texelFetch(uCurrentColor, sample_at, 0).a;
            has_coverage = has_coverage || (support_depths[k] < 1.0 && coverage_tag(support_alpha[k]));
        }
        // Restrict the local support to ordinary reprojection movement; fast
        // turns/translations still reject layers and expose the new image.
        if (has_coverage && length((previous_uv-vUV)*uResolution) < 1.5) {
            for (int k = 0; k < 4; ++k) if (!depth_matches[k] && bilinear_weights[k] > 0.0) {
                bool old_sky = old_depths[k] >= 1.0;
                float old_view = old_sky ? 0.0 : linear_depth(old_depths[k], uPreviousNearFar);
                // Local translation/turn correction at the reprojected ray.
                float current_range = sky ? old_view : old_view + view_depth - expected;
                float support_tolerance = max(.075,current_range*.0055);
                // Compare in device depth after converting the two interval
                // endpoints once; no per-tap range divisions in this loop.
                float range_low=max(current_range-support_tolerance,uNearFar.x);
                float range_high=max(current_range+support_tolerance,uNearFar.x);
                float depth_low=uNearFar.y/(uNearFar.y-uNearFar.x)*(1.-uNearFar.x/range_low);
                float depth_high=uNearFar.y/(uNearFar.y-uNearFar.x)*(1.-uNearFar.x/range_high);
                bool supported = false;
                float local_stability = 1.0;
                for (int s = 0; s < 9; ++s) {
                    bool tagged = support_depths[s] < 1.0 && coverage_tag(support_alpha[s]) &&
                        !(reactive_foreground && rigid_tag(support_alpha[s]));
                    bool same_layer = old_sky ? support_depths[s] >= 1.0 : support_depths[s] < 1.0 &&
                        support_depths[s] >= depth_low && support_depths[s] <= depth_high;
                    if (same_layer && (current_coverage || tagged)) {
                        supported = true;
                        local_stability = min(local_stability,tagged ? support_alpha[s] : current.a);
                    }
                }
                if (supported) {
                    depth_matches[k] = true;
                    reconstructed_coverage = true;
                    coverage_stability = min(coverage_stability,local_stability);
                }
            }
        }
    }
    bool full_footprint = true;
    for (int k = 0; k < 4; ++k) {
        valid_weights[k] = depth_matches[k] ? bilinear_weights[k] : 0.0;
        valid_mass += valid_weights[k];
        full_footprint = full_footprint && depth_matches[k];
    }
    if (valid_mass < .001) return;

    // Keep the single filtered color lookup for the common opaque case.
    vec4 history = vec4(0);
    if (full_footprint) history = texture(uHistoryColor, previous_uv);
    else {
        for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
            float tap_weight = valid_weights[y * 2 + x];
            if (tap_weight > 0.0)
                history += texelFetch(uHistoryColor, clamp(history_at + ivec2(x,y), ivec2(0), size - 1), 0) * tap_weight;
        }
        history /= valid_mass;
    }

    vec3 low = vec3(1.e20), high = vec3(-1.e20), mean = vec3(0), second = vec3(0);
    bool neighboring_coverage = false;
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        ivec2 sample_at = clamp(at + ivec2(x,y), ivec2(0), size - 1);
        vec4 sample_color = texelFetch(uCurrentColor, sample_at, 0);
        vec3 sample_ycocg = to_ycocg(clamp(sample_color.rgb, vec3(0), vec3(60000)));
        low = min(low, sample_ycocg); high = max(high, sample_ycocg);
        mean += sample_ycocg; second += sample_ycocg * sample_ycocg;
        // A sky-depth history can already contain the correct leaf/sky
        // average. Keep the same coverage policy on its sky side. Cloud
        // reactive alpha alone must never identify a coverage layer.
        if(uReactiveAlpha && sky && current.a >= .98 && coverage_tag(sample_color.a) &&
           texelFetch(uCurrentDepth, sample_at, 0).r < 1.0) {
            neighboring_coverage = true;
            coverage_stability = min(coverage_stability, sample_color.a);
        }
    }
    mean /= 9.0;
    vec3 deviation = sqrt(max(second / 9.0 - mean * mean, vec3(0)));
    float motion = length((previous_uv - vUV) * uResolution);
    // Thin, depth-supported vegetation is an area-coverage signal. A narrow
    // color box repeatedly clips a converged leaf/sky average back toward the
    // current jitter sample, even when depth reprojection is valid. Admit its
    // full local variation while preserving untagged opaque/actor clipping.
    bool coverage_history = reconstructed_coverage || neighboring_coverage;
    if(!coverage_history && uReactiveAlpha && !sky && current.a >= .4 && current.a < .998) {
        float point_alpha = texelFetch(uCurrentColor, at, 0).a;
        coverage_history = coverage_tag(point_alpha) && (!rigid_tag(point_alpha) || original_valid_mass < .999);
    }
    float clip_gamma = coverage_history && motion < 1.5 ? 2.0 : 1.15;
    low = max(low, mean - deviation * clip_gamma);
    high = min(high, mean + deviation * clip_gamma);
    vec3 old_ycocg = to_ycocg(history.rgb);
    vec3 limited = clamp(old_ycocg, low, high);
    float outside = abs(old_ycocg.x - limited.x) / max(mean.x + .05, .05);
    float stability = uReactiveAlpha ? clamp(current.a, 0.0, 1.0) : 1.0;
    if(coverage_history)stability=min(stability,coverage_stability);
    // Metadata changes no fully supported rigid texture interior: decode its
    // ordinary confidence to exactly1 until an edge needs local coverage.
    if(!coverage_history && !sky && rigid_tag(current.a))stability=1.;
    float maximum_weight = mix(min(.15, uHistoryWeight), uHistoryWeight, stability);
    // Eight jitter phases need more than the ordinary opaque sample's short
    // history to converge at subpixel foliage/rigid edges. Increase only supported,
    // nearly static coverage; wind-reactive leaves and actors keep their
    // existing confidence, and all depth/disocclusion checks above still win.
    if(coverage_history) {
        float static_coverage = smoothstep(.94,.98,stability)*(1.-smoothstep(.35,1.5,motion));
        maximum_weight = mix(maximum_weight,max(maximum_weight,min(.96,uHistoryWeight+.06)),static_coverage);
    }
    if (sky && !coverage_history) maximum_weight = min(maximum_weight, .85);
    float weight = min(maximum_weight, history.a / (history.a + 1.0));
    // A tiny surviving corner is a weak reprojection, not a full history hit.
    weight *= smoothstep(.05, .5, valid_mass);
    weight *= exp2(-motion * .035);
    weight *= 1.0 - .8 * smoothstep(.03, .4, outside);
    ResolvedColor = vec4(max(mix(current.rgb, from_ycocg(limited), weight), vec3(0)), min(history.a + 1.0, 32.0));
}

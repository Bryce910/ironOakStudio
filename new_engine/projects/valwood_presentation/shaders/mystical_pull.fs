#version 330 core

in  vec2  v_uv;
out vec4  frag_color;

uniform float u_intensity;   /* 0..1 — how far into the TRANS_PULL stage  */
uniform float u_time;        /* total_time for animation                  */
uniform float u_fade;        /* 0..1 — full-screen fade (white then black)*/

/* ── Helpers ───────────────────────────────────────────────── */
float smootherstep(float e0, float e1, float x) {
    float t = clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

/* Rotate a 2-D vector by angle (radians) */
vec2 rot2(vec2 v, float a) {
    float c = cos(a), s = sin(a);
    return vec2(c*v.x - s*v.y, s*v.x + c*v.y);
}

/* Soft radial beam — returns brightness of a single beam at angle `bang`  */
float beam(vec2 dir, float bang, float width) {
    float ang = atan(dir.y, dir.x);
    float diff = abs(mod(ang - bang + 3.14159, 6.28318) - 3.14159);
    return smootherstep(width, 0.0, diff);
}

void main() {
    vec2 uv  = v_uv;
    vec2 cen = uv - 0.5;              /* centred coords  -0.5..0.5  */
    float dist = length(cen);
    float ang  = atan(cen.y, cen.x);

    /* ── Vortex pull: UV warps toward centre as intensity rises ── */
    float pull_strength = u_intensity * 0.35;
    vec2 warped_uv = mix(uv, vec2(0.5), pull_strength * (1.0 - dist * 1.5));

    /* ── Rotating golden + purple beams ───────────────────────── */
    float spin_gold   =  u_time * 0.55;          /* slow clockwise      */
    float spin_purple = -u_time * 0.80 + 1.047;  /* faster counter-CW   */

    /* 6 golden beams evenly distributed */
    float gold_beams = 0.0;
    for (int i = 0; i < 6; i++) {
        float b_ang = spin_gold + float(i) * 1.0472;  /* 60° spacing */
        gold_beams += beam(cen, b_ang, 0.18 + u_intensity * 0.12);
    }
    gold_beams = clamp(gold_beams, 0.0, 1.0);

    /* 4 purple beams offset 22.5° */
    float purple_beams = 0.0;
    for (int i = 0; i < 4; i++) {
        float b_ang = spin_purple + float(i) * 1.5708;  /* 90° spacing */
        purple_beams += beam(cen, b_ang, 0.10 + u_intensity * 0.08);
    }
    purple_beams = clamp(purple_beams, 0.0, 1.0);

    /* ── Radial glow — brightest at centre ─────────────────────── */
    float radial_glow = exp(-dist * (4.0 - u_intensity * 2.5));

    /* ── Pulsing vignette at edges ─────────────────────────────── */
    float pulse        = 0.5 + 0.5 * sin(u_time * 4.8);
    float edge_vignette = smoothstep(0.38, 0.55, dist) * (0.4 + 0.3 * pulse);

    /* ── Chromatic aura rings (concentric) ──────────────────────── */
    float ring_r = smoothstep(0.006, 0.0, abs(dist - 0.20 - u_intensity * 0.08));
    float ring_p = smoothstep(0.005, 0.0, abs(dist - 0.32 - u_intensity * 0.12));
    float ring_w = smoothstep(0.010, 0.0, abs(dist - 0.10 * u_intensity));

    /* ── Assemble overlay colour ────────────────────────────────── */
    vec3 gold   = vec3(1.00, 0.78, 0.20);
    vec3 purple = vec3(0.55, 0.10, 0.95);
    vec3 white  = vec3(1.00, 1.00, 1.00);
    vec3 amber  = vec3(1.00, 0.45, 0.05);

    vec3 col = vec3(0.0);
    col += gold   * gold_beams   * (0.6 + 0.4 * radial_glow);
    col += purple * purple_beams * (0.5 + 0.5 * radial_glow);
    col += amber  * ring_r       * 2.0;
    col += purple * ring_p       * 1.5;
    col += white  * ring_w       * 3.0;
    col += white  * radial_glow  * u_intensity * 1.2;

    /* Edge vignette darkens the overlay periphery */
    col *= (1.0 - edge_vignette * 0.8);

    /* Overall alpha: overlay builds up with intensity, additive over scene */
    float overlay_alpha = clamp(u_intensity * 1.2, 0.0, 1.0);

    /* ── Full-screen fade pass (white → black) ──────────────────── */
    /* u_fade 0..0.5 → fade to white; 0.5..1.0 → fade to black      */
    float white_fade  = smoothstep(0.0, 0.5, u_fade);
    float black_fade  = smoothstep(0.5, 1.0, u_fade);
    vec3  fade_col    = mix(vec3(1.0), vec3(0.0), black_fade);
    float fade_alpha  = max(white_fade, black_fade);

    /* ── Combine: mystical overlay then fade on top ─────────────── */
    vec4 out_col = vec4(col, overlay_alpha);

    /* Fade overrides everything at its alpha */
    out_col.rgb = mix(out_col.rgb, fade_col, fade_alpha);
    out_col.a   = max(out_col.a, fade_alpha);

    frag_color = out_col;
}

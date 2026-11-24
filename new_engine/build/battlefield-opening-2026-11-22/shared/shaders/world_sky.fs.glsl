#version 330 core
#pragma include "world_authored_weather.glsl"
#pragma include "world_storm_lighting.glsl"

in vec2 vUV;
out vec4 FragColor;
uniform vec3 uRight, uUp, uForward, uSun, uSunColor, uFog, uAmbient;
uniform bool uSunDisc=true;
uniform float uAerialHorizon=0.;
uniform vec3 uSkyLight, uGroundLight, uSkyZenith, uEye;
uniform float uAspect, uTan, uCloud, uTime;
uniform vec2 uJitterNDC;
uniform sampler3D uCloudNoise;
#ifdef VALWOOD_HDR_ENVIRONMENT
uniform samplerCube uEnvironmentSky;
uniform vec2 uEnvironmentRotation=vec2(1,0);
uniform float uEnvironmentIntensity=1,uEnvironmentStrength=0;
#endif
uniform bool uHasVolume;
// Default preserves standalone compatibility fixtures, whose density path
// can use procedural noise without uploading the runtime's 3D volume.
uniform bool uVolumetricClouds = true;

float hash(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * .1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x),
               mix(hash(i + vec2(0, 1)), hash(i + 1.0), f.x), f.y);
}
// The same broad weather footprint drives terrain and foliage cloud shadows.
float cloud_field(vec2 p) {
    const mat2 rotation = mat2(.80, -.60, .60, .80);
    return .67 * noise(p) + .33 * noise(rotation * p * 2.03 + 17.4);
}
vec3 safe_normalize(vec3 v, vec3 fallback) {
    return dot(v, v) > 1e-12 ? normalize(v) : fallback;
}
float hg(float cosine, float g) {
    return (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * cosine, .02), 1.5);
}
vec3 atmosphere(vec3 ray, vec3 sun, float toward_sun, vec3 zenith, float solar_visibility) {
    float elevation = max(ray.y, 0.0);
    float air_density = exp(-clamp(uEye.y, 0.0, 80000.0) / 8500.0);
    float air_mass = air_density / max(elevation + .025 * exp(-15.0 * elevation), .015);
    float horizon = 1.0 - exp(-air_mass * .065);
    float rayleigh = .75 * (1.0 + toward_sun * toward_sun);
    vec3 sky = mix(zenith * (.88 + .12 * rayleigh), uFog, horizon);
    // A bounded aerosol forward lobe surrounds an angular, antialiased HDR sun.
    float daylight = smoothstep(-.10, .04, sun.y);
    sky += max(uSunColor, vec3(0)) * hg(toward_sun, .76) * .006
        * (1.0 - exp(-air_mass * .09)) * daylight * solar_visibility;
    float disc_width = max(fwidth(toward_sun), .000001);
    float disc = smoothstep(.999989 - disc_width, .999989 + disc_width, toward_sun);
    if(uSunDisc)sky += max(uSunColor, vec3(0)) * disc * 7.0 * daylight * solar_visibility;
    return mix(max(uFog, vec3(0)) * .82, sky, smoothstep(-.11, .015, ray.y));
}
vec3 moving_position(vec3 p) {
    // Exact coarse-field wind units: moving the observer against this wind
    // preserves both the weather field and every 3D density sample.
    return p + vec3(uTime * ( .0014 / .00042), 0, uTime * (-.00055 / .00042));
}
float cloud_bottom(){return uAuthoredWeatherCount>0?420.0:2700.0;}
float cloud_top(){return uAuthoredWeatherCount>0?1500.0:4100.0;}
float density(vec3 world, float weather, float lod) {
    float h = (world.y - cloud_bottom()) / (cloud_top()-cloud_bottom());
    if (h <= 0.0 || h >= 1.0) return 0.0;
    // Local fronts occupy their authored WORLD column. A reference ray can
    // intersect a neighboring area while a later 3D density sample misses it.
    if(uAuthoredWeatherAreaCount>0){
        float local_cloud=authoredWeather(world.xz,vec3(0.0,uAuthoredWeatherGlobal.y,0.0)).y;
        if(local_cloud<.015)return 0.0;
        float threshold=.64-clamp(local_cloud,0.0,1.0)*.30;
        weather=smoothstep(threshold-.06,threshold+.17,cloud_field(world.xz*.00042+vec2(uTime*.0014,-uTime*.00055)));
        weather=max(weather,.55*smoothstep(.65,.95,local_cloud));
    }
    // AAA merge: finer 2D breakup so a large weather blob does not read as
    // one smooth dome with a clean arc edge when seen across the horizon.
    weather*=.45+.55*smoothstep(.25,.75,cloud_field(moving_position(world).xz*.0029+vec2(5.3,1.9)));
    float profile = smoothstep(0.0, .12, h)
        * (1.0 - smoothstep(mix(.35, .72, weather), 1.0, h));
    vec3 p = moving_position(world) / (uAuthoredWeatherCount>0?vec3(2000.0,1500.0,2000.0):vec3(4200.0,3000.0,4200.0));
    vec3 shape;
    if (uHasVolume) {
        shape = textureLod(uCloudNoise, p, lod).rgb;
    } else {
        // Compatibility for standalone shader fixtures. Runtime always binds
        // the immutable 3D volume; this path is not the in-game renderer.
        shape = vec3(noise(p.xz * 8.0 + p.y * 5.0), noise(p.xz * 16.0 - p.y * 9.0), .5);
    }
    float threshold = mix(.66, .25, weather * profile);
    float body = smoothstep(threshold, threshold + .22, shape.r);
    float erosion = (1.0 - shape.g) * .32 + (1.0 - shape.b) * .12;
    // AAA merge: a high-frequency detail octave erodes the soft edge of the
    // body so large masses read as billowed cumulus instead of a smooth dome.
    if (uHasVolume && body > 0.0 && body < 1.0) {
        float detail = textureLod(uCloudNoise, p * 4.7 + vec3(.31, .17, .53), lod + 1.0).r;
        body = max(body - (1.0 - detail) * .45 * (1.0 - body * .6), 0.0);
    }
    return max(body - erosion - (1.0 - profile) * .12, 0.0) * profile;
}
void main() {
    vec2 q = vUV * 2.0 - 1.0 - uJitterNDC;
    vec3 ray = safe_normalize(uForward + q.x * uAspect * uTan * uRight + q.y * uTan * uUp, vec3(0, 1, 0));
    vec3 sun = safe_normalize(uSun, vec3(0, 1, 0));
    float toward_sun = clamp(dot(ray, sun), -1.0, 1.0);
    bool legacy = max(max(uSkyLight.r, uSkyLight.g), uSkyLight.b) <= 0.0;
    vec3 sky_light = max(legacy ? uAmbient * vec3(1.25, 1.38, 1.52) : uSkyLight, vec3(0));
    vec3 ground_light = max(legacy ? uAmbient * vec3(.48, .50, .43) : uGroundLight, vec3(0));
    float daylight = clamp(dot(uSunColor, vec3(.2126, .7152, .0722)), .02, 1.0);
    vec3 zenith = max(max(uSkyZenith.r, uSkyZenith.g), uSkyZenith.b) > 0.0
        ? uSkyZenith : vec3(.035, .105, .225) * mix(.12, 1.0, daylight);
    // Dense weather has a continuous high sheet as well as the broken volume
    // below. Keep this inexpensive sky when cloud raymarching is disabled;
    // turning down quality must not turn overcast jungle weather into blue sky.
    // Above the cloud slab the observer can still see the clear atmosphere.
    float cloud_cover=uCloud;vec2 cloud_world=uEye.xz;
    if(uAuthoredWeatherCount>0){
        float cloud_distance=clamp((650.0-uEye.y)/max(ray.y,.025),0.0,65000.0);
        cloud_world=uEye.xz+ray.xz*cloud_distance;
        cloud_cover=authoredWeather(cloud_world,vec3(0.0,uAuthoredWeatherGlobal.y,0.0)).y;
    }
    float slab_visibility=1.0-smoothstep(cloud_bottom(),cloud_top(),uEye.y);
    float spatial_overcast=smoothstep(.65,.95,clamp(cloud_cover,0.0,1.0))*slab_visibility;
    float overcast=spatial_overcast;
    if(uAuthoredWeatherAreaCount>0){
        // A local weather footprint is a 3D cloud column, not an opaque gray
        // disk at the 650m reference plane. Let the volume carry its mass;
        // retain only a broken, translucent high-sheet contribution above
        // the global baseline. Dry overrides can still remove the baseline.
        float baseline_overcast=smoothstep(.65,.95,clamp(uAuthoredWeatherGlobal.y,0.0,1.0))*slab_visibility;
        if(overcast>baseline_overcast){
            float breakup=.36+.64*cloud_field(cloud_world*.0017+vec2(uTime*.002,-uTime*.0007));
            overcast=baseline_overcast+min(overcast-baseline_overcast,.42*(1.0-baseline_overcast))*breakup;
        }
    }
    float solar_visibility = 1.0 - overcast;
    // Begin full-overcast dead-work guard.
    // Fully overcast weather replaces the clear/HDR background exactly. Keep
    // the volume below unchanged, but avoid shading a background with zero
    // contribution, including its otherwise discarded HDR cube fetch.
    vec3 sky = vec3(0);
    if (overcast < 1.0) {
        sky = atmosphere(ray, sun, toward_sun, max(zenith, vec3(0)), solar_visibility);
#ifdef VALWOOD_HDR_ENVIRONMENT
        if(uEnvironmentStrength>0.){vec3 d=vec3(uEnvironmentRotation.x*ray.x-uEnvironmentRotation.y*ray.z,ray.y,uEnvironmentRotation.y*ray.x+uEnvironmentRotation.x*ray.z);sky=mix(sky,max(textureLod(uEnvironmentSky,d,0.).rgb,vec3(0))*uEnvironmentIntensity,uEnvironmentStrength);}
#endif
        // A photographic panorama has no knowledge of the world's aerial
        // extinction. Match its lowest elevations to the same fog radiance as
        // distant geometry, with a smooth angular transition and no hard rim.
        if(uAerialHorizon>0.)
            sky=mix(sky,max(uFog,vec3(0.)),uAerialHorizon*(1.-smoothstep(.012,.12,ray.y)));
    }
    if (overcast > 0.0) {
        const vec3 luminance = vec3(.2126, .7152, .0722);
        vec3 gray_zenith = max(dot(zenith, luminance), 0.0) * vec3(.94, 1.0, .98);
        vec3 gray_horizon = max(dot(uFog, luminance), 0.0) * vec3(.94, 1.0, .98);
        vec3 sheet = mix(gray_horizon, gray_zenith, pow(max(ray.y, 0.0), .42));
        if(uAuthoredWeatherCount>0)sheet*=.72+.28*cloud_field(cloud_world*.0012+vec2(uTime*.002,-uTime*.0007));
        sky = mix(sky, sheet, overcast);
    }
    // End full-overcast dead-work guard.
    float stability = 1.0;
    float coverage = smoothstep(.015, .12, cloud_cover);
    // Rays outside the slab, an empty weather cell and zero coverage do not
    // pay for any 3D density fetches. The horizon dissolves into aerial haze.
    if (uVolumetricClouds && coverage > 0.0 && abs(ray.y) > .0001) {
        float entry = (cloud_bottom() - uEye.y) / ray.y;
        float exit = (cloud_top() - uEye.y) / ray.y;
        float start = max(0.0, min(entry, exit));
        float finish = min(65000.0, max(entry, exit));
        float horizon_fade = smoothstep(.11, .34, abs(ray.y)); // AAA merge: was .085-.27
        if (finish > start && horizon_fade > 0.0) {
            float reference_distance = clamp(((uAuthoredWeatherCount>0?650.0:3200.0) - uEye.y) / ray.y, start, finish);
            vec2 weather_position = (uEye.xz + ray.xz * reference_distance) * .00042
                + vec2(uTime * .0014, -uTime * .00055);
            float threshold = .64 - clamp(cloud_cover, 0.0, 1.0) * .30;
            float weather = smoothstep(threshold - .06, threshold + .17, cloud_field(weather_position));
            if(uAuthoredWeatherCount>0)weather=max(weather,spatial_overcast*.55);
            // AAA merge: with authored areas the density re-derives weather per
            // sample; gating on this single reference-plane value cut the
            // volume along a smooth isoline (a hard white arc in the sky).
            if (weather > .008 || uAuthoredWeatherAreaCount > 0) {
                float step_length = (finish - start) / 12.0;
                // Stable stratification removes regular marching bands without
                // injecting new time noise into the existing temporal resolve.
                float phase = .40 + .20 * hash(floor(gl_FragCoord.xy));
                float transmittance = 1.0;
                vec3 radiance = vec3(0);
                // Begin zero-solar phase guard.
                // The entire direct term is multiplied by solar_visibility.
                // These HG lobes cannot contribute when it is exactly zero.
                float phase_light = 0.0, sun_up = 0.0;
                if (solar_visibility > (uAuthoredWeatherCount>0?.08:0.0)) {
                    phase_light = .80 * hg(toward_sun, .55) + .20 * hg(toward_sun, -.20);
                    sun_up = smoothstep(-.08, .08, sun.y);
                }
                // End zero-solar phase guard.
                for (int i = 0; i < 12; ++i) {
                    float distance = start + (float(i) + phase) * step_length;
                    vec3 position = uEye + ray * distance;
                    float lod = clamp(log2(max(distance * .00017, 1.0)), 0.0, 2.0);
                    float d = density(position, weather, lod);
                    if (d > .003) {
                        // Two occupied-sample light steps approximate the
                        // optical depth of the sunward cloud, not a flat tint.
                        // Fully overcast weather has no direct solar lobe.
                        // Skip its two density fetches once their continuously
                        // fading contribution reaches exactly zero.
                        float direct = 0.0;
                        if (solar_visibility > (uAuthoredWeatherCount>0?.08:0.0)) {
                            float light_depth = density(position + sun * 260.0, weather, lod) * 260.0
                                + density(position + sun * 760.0, weather, lod + .5) * 520.0;
                            direct = exp(-light_depth * .0042);
                        }
                        float vertical = clamp((position.y-cloud_bottom())/(cloud_top()-cloud_bottom()),0.0,1.0);
                        vec3 ambient = sky_light * mix(.55, 1.02, vertical) + ground_light * .10;
                        vec3 illumination = ambient + max(uSunColor, vec3(0)) * sun_up * solar_visibility
                            * (.10 + direct * (.25 + .24 * phase_light));
                        if(max(max(uStormFlashColor.r,uStormFlashColor.g),uStormFlashColor.b)>0.0){
                            float flash_distance=length(position-uEye-uStormFlashPositionRadius.xyz);
                            float flash_weight=clamp(1.0-flash_distance/max(uStormFlashPositionRadius.w*1.8,1.0),0.0,1.0);
                            illumination+=uStormFlashColor*flash_weight*flash_weight*(.08+.08*vertical);
                        }
                        float opacity = 1.0 - exp(-d * step_length * .0042 * coverage);
                        radiance += transmittance * opacity * illumination;
                        transmittance *= 1.0 - opacity;
                        if (transmittance < .025) break;
                    }
                }
                float aerial = exp(-start / 38000.0) * horizon_fade;
                sky = mix(sky, radiance + transmittance * sky, aerial);
                stability = mix(1.0, .68, (1.0 - transmittance) * aerial);
            }
        }
    }
    if(max(max(uStormFlashColor.r,uStormFlashColor.g),uStormFlashColor.b)>0.0){
        vec3 glow_position=uStormFlashPositionRadius.xyz+vec3(0.0,420.0,0.0);
        float glow_distance=length(glow_position);
        vec3 glow_direction=glow_distance>.001?glow_position/glow_distance:vec3(0,1,0);
        float glow=pow(max(dot(ray,glow_direction),0.0),24.0)*clamp(1.0-glow_distance/max(uStormFlashPositionRadius.w*4.0,1.0),0.0,1.0);
        sky+=uStormFlashColor*glow*.12;
    }
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.)sky=min(sky,vec3(60000));
#endif
    FragColor = vec4(max(sky, vec3(0)), stability);
}

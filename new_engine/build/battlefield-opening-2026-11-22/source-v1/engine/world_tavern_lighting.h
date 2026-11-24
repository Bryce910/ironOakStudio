#ifndef VALWOOD_WORLD_TAVERN_LIGHTING_H
#define VALWOOD_WORLD_TAVERN_LIGHTING_H

#include <array>
#include <cmath>

// Low-frequency, single diffuse-return estimate for the Copper Lantern's four
// actual practicals. Source intensity and mean interior reflectance are scene-
// linear; the result is diffuse irradiance / PI, matching the sky inputs.
// This is a bounded authored room approximation, not traced global lighting.
struct WorldTavernLighting {
    std::array<float,4> bounds = {}; // absolute world xmin,zmin,xmax,zmax
    std::array<float,2> heights = {{23.8f,32.3f}};
    // Absolute ridge Z, clipping ridge Y, and roof fall per metre of |Z-ridge|.
    // The clipping surface is midway between the actual top and underside.
    std::array<float,3> roof = {};
    std::array<float,3> irradiance = {};

    static constexpr float hearth_intensity = 28.f;
    static constexpr float lamp_intensity = 18.f;
    static constexpr float added_lamp_intensity = 36.f;
    // Approximate closed interior area: 20x16m floor, 4.4m walls, pitched
    // ceiling and gables. Ignoring openings treats their energy as absorbed.
    static constexpr float surface_area = 1044.f;
    static constexpr std::array<float,3> mean_reflectance = {{.38f,.32f,.25f}};
    static constexpr std::array<float,3> source_color = {{1.f,.47f,.12f}};
};

inline bool world_tavern_lighting_build(double origin_x,double origin_z,
                                        WorldTavernLighting &out) {
    if(!std::isfinite(origin_x)||!std::isfinite(origin_z)||
       std::abs(origin_x)>100800.||std::abs(origin_z)>100800.)return false;
    WorldTavernLighting result;
    result.bounds={{float(origin_x-155.),float(origin_z-90.),
                    float(origin_x-135.),float(origin_z-74.)}};
    result.roof={{float(origin_z-82.),32.12f,3.8f/8.6f}};
    // Isotropic point intensity I emits 4*PI*I; one Lambertian return from
    // area A has radiance (4*PI*I/A)*reflectance/PI = 4*I*reflectance/A.
    // Use the steady mean, independent of frame time, camera-selected torch
    // slots and flicker, so the whole room cannot pump with an individual flame.
    constexpr float mean_return=4.f*(WorldTavernLighting::hearth_intensity+
        WorldTavernLighting::lamp_intensity+WorldTavernLighting::added_lamp_intensity)/WorldTavernLighting::surface_area;
    for(unsigned i=0;i<3;i++)
        result.irradiance[i]=mean_return*WorldTavernLighting::source_color[i]*
            WorldTavernLighting::mean_reflectance[i];
    out=result;
    return true;
}

#endif

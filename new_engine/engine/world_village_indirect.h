#ifndef VALWOOD_WORLD_VILLAGE_INDIRECT_H
#define VALWOOD_WORLD_VILLAGE_INDIRECT_H

#include "world_village_sky.h"

struct WorldVillageIndirectConfig {
    std::size_t columns=257,rows=257,slices=9;
    float first_height=24.f,height_step=3.f;
};

struct WorldVillageIndirectField {
    WorldVillageIndirectConfig config;
    // Z-height slices, then row-major X/Z nodes. RGBA = +X,-X,+Z,-Z.
    std::vector<std::uint8_t> horizontal_rgba,up_r;
};

/* Startup-only directional diffuse-sky visibility from grounded roof heights.
 * Input nodes span the same world bounds as the output. Output intervals must
 * divide the input intervals exactly (the standard 513 -> 257 reduction).
 * Rays still traverse the original input grid, retaining narrow roof edges.
 *
 * At each height, eight fixed azimuths find the maximum positive roof horizon
 * at distances1,2,3,5,8,13,21,34,55 input cells. Integrating elevation above that
 * horizon gives cos^2(alpha) for an upward normal, and
 * 1-2/pi*(alpha+sin(alpha)*cos(alpha)) for a horizontal normal. Four cardinal
 * facade integrals use positive azimuth-cosine weights. Every result is bounded
 * [0,1]; unlike the legacy scalar field it needs no artificial sky minimum.
 *
 * This is a 2.5D roof-horizon approximation, not full GI: it does not model
 * windows, open ground floors, reflected buildings, overhang thickness, or
 * dynamic occluders. Height slices measure actual clearance rather than a
 * guessed height-recovery factor. The last slice must clear every input roof,
 * making GL_CLAMP_TO_EDGE above the volume physically open. For taller roofs,
 * increase slices (maximum33) or height_step before calling.
 *
 * Upload horizontal_rgba as GL_RGBA8 and up_r as GL_R8 GL_TEXTURE_3D volumes
 * (width=columns,height=rows,depth=slices), linear min/mag, clampS/T/R, no mips.
 * Pixel unpack alignment must be1 for up_r. Shader height binding is
 * (first_height,1/height_step,float(slices)); the existing world bounds apply.
 * Invalid/nonfinite input or allocation failure leaves output unchanged.
 */
inline bool world_village_indirect_build(const float *roof_heights,std::size_t height_count,
                                        const WorldVillageSkyConfig &input,
                                        const WorldVillageIndirectConfig &config,
                                        WorldVillageIndirectField &output) {
    if(!roof_heights||input.columns<2||input.rows<2||input.columns>4097||input.rows>4097||
       config.columns<2||config.rows<2||config.columns>513||config.rows>513||
       config.columns>input.columns||config.rows>input.rows||config.slices<2||config.slices>33||
       (input.columns-1)%(config.columns-1)||(input.rows-1)%(config.rows-1)||
       !std::isfinite(input.cell_width)||!std::isfinite(input.cell_depth)||
       input.cell_width<=0||input.cell_depth<=0||!std::isfinite(config.first_height)||
       !std::isfinite(config.height_step)||config.height_step<=0)return false;
    const std::size_t input_count=input.columns*input.rows;
    if(height_count!=input_count)return false;
    const double top=double(config.first_height)+double(config.height_step)*double(config.slices-1);
    if(!std::isfinite(top))return false;
    for(std::size_t k=0;k<input_count;k++)
        if(!std::isfinite(roof_heights[k])||roof_heights[k]>top)return false;

    constexpr int strides[]={1,2,3,5,8,13,21,34,55};
    constexpr int directions[][2]={{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    struct Sample {int x,z;double inverse_distance;};
    std::array<std::array<Sample,9>,8> rays;
    std::array<std::array<double,4>,8> weights={};
    std::array<double,4> normalization={};
    for(std::size_t ray=0;ray<rays.size();ray++) {
        const double dx=double(directions[ray][0])*input.cell_width,dz=double(directions[ray][1])*input.cell_depth;
        const double azimuth_distance=std::hypot(dx,dz);
        if(!std::isfinite(azimuth_distance)||azimuth_distance<=0)return false;
        weights[ray]={std::max(dx,0.)/azimuth_distance,std::max(-dx,0.)/azimuth_distance,
                      std::max(dz,0.)/azimuth_distance,std::max(-dz,0.)/azimuth_distance};
        for(std::size_t c=0;c<4;c++)normalization[c]+=weights[ray][c];
        for(std::size_t k=0;k<rays[ray].size();k++) {
            const int x=directions[ray][0]*strides[k],z=directions[ray][1]*strides[k];
            const double distance=std::hypot(double(x)*input.cell_width,double(z)*input.cell_depth);
            const double inverse=1./distance;
            if(!std::isfinite(distance)||distance<=0||!std::isfinite(inverse))return false;
            rays[ray][k]={x,z,inverse};
        }
    }
    const std::size_t nodes=config.columns*config.rows*config.slices;
    WorldVillageIndirectField next;next.config=config;
    try {next.horizontal_rgba.resize(nodes*4);next.up_r.resize(nodes);}catch(const std::bad_alloc &){return false;}
    const std::size_t x_step=(input.columns-1)/(config.columns-1),z_step=(input.rows-1)/(config.rows-1);
    const int columns=int(input.columns),rows=int(input.rows);
    constexpr double two_over_pi=.63661977236758134308;
    const auto quantize=[](double value){return std::uint8_t(std::lround(std::clamp(value,0.,1.)*255.));};
    for(std::size_t y=0;y<config.slices;y++) {
        const double reference=double(config.first_height)+double(y)*config.height_step;
        for(std::size_t z=0;z<config.rows;z++)for(std::size_t x=0;x<config.columns;x++) {
            const int ix=int(x*x_step),iz=int(z*z_step);
            double up=0;std::array<double,4> horizontal={};
            for(std::size_t ray=0;ray<rays.size();ray++) {
                double horizon=0;
                for(const auto &sample:rays[ray]) {
                    const int sx=ix+sample.x,sz=iz+sample.z;
                    if(sx<0||sz<0||sx>=columns||sz>=rows)continue;
                    const double roof=roof_heights[std::size_t(sz)*input.columns+std::size_t(sx)];
                    horizon=std::max(horizon,(roof-reference)*sample.inverse_distance);
                }
                // atan(infinity) is defined, but slope/(1+slope^2) can form
                // inf/inf with extreme finite input heights/spacing. Use the
                // stable reciprocal branch without accepting nonfinite data.
                const double reciprocal=1./horizon;
                const double cos_squared=horizon<=1.?1./(1.+horizon*horizon):
                    reciprocal*reciprocal/(1.+reciprocal*reciprocal);
                const double sin_cos=horizon<=1.?horizon/(1.+horizon*horizon):
                    reciprocal/(1.+reciprocal*reciprocal);
                up+=cos_squared;
                const double visible=std::clamp(1.-two_over_pi*(std::atan(horizon)+sin_cos),0.,1.);
                for(std::size_t c=0;c<4;c++)horizontal[c]+=weights[ray][c]*visible;
            }
            const std::size_t node=(y*config.rows+z)*config.columns+x;
            next.up_r[node]=quantize(up/8.);
            for(std::size_t c=0;c<4;c++)next.horizontal_rgba[node*4+c]=quantize(horizontal[c]/normalization[c]);
        }
    }
    output.config=next.config;output.horizontal_rgba.swap(next.horizontal_rgba);output.up_r.swap(next.up_r);
    return true;
}

#endif

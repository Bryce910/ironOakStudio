// Reconstruct only reviewed previous expressions from actual production GLSL.
// Pixel fixtures must never depend on scratch candidates or reimplement PCF.
#ifndef WORLD_SHADER_LOSSLESS_TEST_SOURCES_H
#define WORLD_SHADER_LOSSLESS_TEST_SOURCES_H
#include <cassert>
#include <string>
namespace world_shader_lossless_test {
inline std::string replace_exact(std::string source,const std::string &old,const std::string &replacement){
    const auto at=source.find(old);assert(at!=std::string::npos&&source.find(old,at+1)==std::string::npos);
    source.replace(at,old.size(),replacement);return source;
}
inline std::string without_gather(std::string source){
    source=replace_exact(source,R"VALWOOD(#if defined(GL_ARB_texture_gather) && !defined(VALWOOD_QA_DISABLE_TEXTURE_GATHER)
#extension GL_ARB_texture_gather : enable
#define VALWOOD_TEXTURE_GATHER_ACTIVE 1
#endif
#ifdef VALWOOD_TEXTURE_GATHER_ACTIVE
uniform bool uShadowGather=true;
#endif
)VALWOOD","");
    return replace_exact(source,R"VALWOOD(#ifdef VALWOOD_TEXTURE_GATHER_ACTIVE
        // ARB_texture_gather returns (i0,j1),(i1,j1),(i1,j0),(i0,j0).
        // Coordinates lie on texel edges, half a texel from any footprint
        // selection discontinuity. Only use fully interior3x3 neighborhoods.
        if(uShadowGather&&all(greaterThan(center,ivec2(0)))&&all(lessThan(center,limit))) {
            vec2 size=vec2(textureSize(uShadow,0));
            vec4 a=textureGather(uShadow,vec2(center)/size);
            vec4 b=textureGather(uShadow,vec2(center+ivec2(1,0))/size);
            vec4 c=textureGather(uShadow,vec2(center+ivec2(0,1))/size);
            vec4 e=textureGather(uShadow,vec2(center+ivec2(1,1))/size);
            vec3 row[3]=vec3[3](vec3(a.w,a.z,b.z),vec3(a.x,a.y,b.y),vec3(c.x,c.y,e.y));
            // Preserve the existing nine comparisons, multiplications and
            // row-major addition order. Gather does not filter/compare depth.
            for(int y=0;y<3;y++)for(int x=0;x<3;x++)
                filtered+=wx[x]*wy[y]*step(p.z-bias,row[y][x]);
        } else
#endif
        {
        for(int y=0;y<3;y++)for(int x=0;x<3;x++)
            filtered+=wx[x]*wy[y]*step(p.z-bias,texelFetch(uShadow,clamp(center+ivec2(x-1,y-1),ivec2(0),limit),0).r);
        }
)VALWOOD",R"VALWOOD(        for(int y=0;y<3;y++)for(int x=0;x<3;x++)
            filtered+=wx[x]*wy[y]*step(p.z-bias,texelFetch(uShadow,clamp(center+ivec2(x-1,y-1),ivec2(0),limit),0).r);
)VALWOOD");
}
inline std::string without_leaf_cloud_zero(std::string source){
    source=replace_exact(source,"    vec3 sunlight=uSunColor;\n","    vec3 sunlight=uSunColor*cloud_transmittance();\n");
    source=replace_exact(source,"        sunlight=uSunColor*cloud_transmittance();\n","");
    source=replace_exact(source,
        "    // All three leaf sunlight terms are exactly zero under this same guard.\n"
        "    // Cloud noise is ALU-only; opaque bounce and far-card clouds still run.\n"
        "    bool leafSunZero=leaf&&dot(N,uSun)<=-.45&&dot(V,-uSun)<=0.;\n"
        "    if(!leafSunZero)sunlight=uSunColor*cloud_transmittance();\n","");
    return replace_exact(source,"    float shadow=1.;\n",
        "    bool leafSunZero=leaf&&dot(N,uSun)<=-.45&&dot(V,-uSun)<=0.;\n"
        "    float shadow=1.;\n");
}
inline std::string without_leaf_zero(std::string source){
    return replace_exact(source,R"VALWOOD(    // PCF has no contribution when opaque direct light is zero, or when
    // all three thin-leaf sunlight terms are exactly zero. Keep the four-tap
    // implicit-LOD path unchanged; the nine-tap path uses explicit texelFetch.
    bool leafSunZero=leaf&&dot(N,uSun)<=-.45&&dot(V,-uSun)<=0.;
    float shadow=1.;
    if(!uSoftShadows||(!leafSunZero&&(leaf||!(dot(N,uSun)<=0.))))shadow=visibility(geometricN);
)VALWOOD",R"VALWOOD(    // Opaque direct BRDF is exactly zero on the sun-opposed side. Thin
    // leaves still need PCF for wrapped/transmitted light. Keep implicit-LOD
    // four-tap sampling in its original control flow; nine-tap uses texelFetch.
    float shadow=1.;
    if(!uSoftShadows||leaf||!(dot(N,uSun)<=0.))shadow=visibility(geometricN);
)VALWOOD");
}
}
#endif

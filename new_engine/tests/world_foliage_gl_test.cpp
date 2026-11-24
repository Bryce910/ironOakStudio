/* Actual GLSL regression, no game assets or native benchmark. Run from the
 * engine root (or pass it as argv[1]); default SDL driver is offscreen.
 * Compile: g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. $(pkg-config
 * --cflags sdl2) tests/world_foliage_gl_test.cpp -o /tmp/valwood-foliage-gl
 * $(pkg-config --libs sdl2) -lGL
 * Run: LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-foliage-gl
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include "stb_image.h"
#include "gl_helpers.h"
#include "engine/world_village_indirect.h"
#include "engine/world_castle_lights.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
constexpr int size = 128;
using Pixel = std::array<float, 4>;
const char *fixture_vertex = R"GLSL(#version 330 core
layout(location=0) in vec2 p;
uniform vec3 testRoot,testWorld,testNormal;
uniform float testSpread,testUVScale,testCoveragePhase,testInstanceFade,testRootDrop;
out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;
out vec2 vUV;
flat out float vSeed,vScale,vRootHeight,vCoveragePhase,vInstanceFade;
out float vFacadePaint;
flat out vec3 vRootRelative;
void main() {
    vec3 span=vec3(p*testSpread,0.);
    vRelative=testRoot+span; vPos=testWorld+span;
    vRootRelative=testRoot; vRootHeight=testWorld.y-testRootDrop;
    vNormal=testNormal; vTangent=vec3(1.,0.,0.);
    vBitangent=cross(testNormal,vTangent);
    vUV=(p*.5+.5)*testUVScale; vSeed=.5; vScale=1.;vCoveragePhase=testCoveragePhase;
    vInstanceFade=testInstanceFade;vFacadePaint=0.;
    gl_Position=vec4(p,0.,1.);
})GLSL";
const char *root_fragment = R"GLSL(#version 330 core
flat in vec3 vRootRelative;
flat in float vRootHeight,vSeed,vCoveragePhase;
layout(location=0) out vec4 color;
layout(location=1) out vec4 phase;
void main(){color=vec4(vRootRelative.x,vRootRelative.z,vRootHeight,vSeed);phase=vec4(vCoveragePhase,0,0,1);}
)GLSL";

std::string source(const char *path) {
    std::ifstream file(path);
    assert(file);
    std::ostringstream out; out << file.rdbuf();
    return out.str();
}
GLuint program(const char *vs, const char *fs) {
    GLuint result=GLH_program(vs,fs);
    GLint linked=0;
    glGetProgramiv(result,GL_LINK_STATUS,&linked);
    assert(result&&linked);
    glUseProgram(result);glUniform1i(glGetUniformLocation(result,"uVillageSkyMask"),12);
    glUniform1i(glGetUniformLocation(result,"uLocalProbe0"),4);glUniform1i(glGetUniformLocation(result,"uLocalProbe1"),5);
    glUniform1i(glGetUniformLocation(result,"uLocalProbeCount"),0);glUniform1f(glGetUniformLocation(result,"uLocalIrradianceWeight"),0);
    glUniform1i(glGetUniformLocation(result,"uVillageSkyDirectional"),13);glUniform1i(glGetUniformLocation(result,"uVillageSkyUp"),14);
    return result;
}
void scalar(GLuint p,const char *name,float value) {glUniform1f(glGetUniformLocation(p,name),value);}
void integer(GLuint p,const char *name,int value) {glUniform1i(glGetUniformLocation(p,name),value);}
void vector(GLuint p,const char *name,float x,float y,float z) {glUniform3f(glGetUniformLocation(p,name),x,y,z);}
std::vector<Pixel> draw(GLuint p) {
    glUseProgram(p);
    glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES,0,6);
    std::vector<Pixel> pixels(size*size);
    glReadPixels(0,0,size,size,GL_RGBA,GL_FLOAT,pixels.data());
    for(const auto &pixel:pixels)for(float value:pixel)assert(std::isfinite(value));
    assert(glGetError()==GL_NO_ERROR);
    return pixels;
}
bool covered(const Pixel &p) {return p[3]>0;}
float fraction(const std::vector<Pixel> &pixels) {
    size_t count=0;for(const auto &p:pixels)count+=covered(p);
    return float(count)/pixels.size();
}
Pixel center(const std::vector<Pixel> &pixels) {return pixels[(size/2)*size+size/2];}
float smooth(float lo,float hi,float value) {
    float t=std::clamp((value-lo)/(hi-lo),0.f,1.f);return t*t*(3-2*t);
}
void expect_fraction(const std::vector<Pixel> &pixels,float expected) {
    float actual=fraction(pixels);
    if(std::fabs(actual-expected)>.006f)
        std::fprintf(stderr,"Coverage %.6f, expected %.6f\n",actual,expected);
    assert(std::fabs(actual-expected)<.006f);
}
void defaults(GLuint p) {
    glUseProgram(p);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,identity);
    vector(p,"testRoot",0,0,-300);vector(p,"testWorld",0,0,0);
    vector(p,"testNormal",0,1,0);scalar(p,"testSpread",0);scalar(p,"testUVScale",1);scalar(p,"testCoveragePhase",0);
    scalar(p,"testInstanceFade",1);scalar(p,"testRootDrop",0);
    integer(p,"uHasBase",0);integer(p,"uHasNormal",0);integer(p,"uHasARM",0);
    integer(p,"uGrass",0);integer(p,"uVegetation",1);integer(p,"uLodLevel",-1);
    integer(p,"uBase",0);integer(p,"uNormal",1);
    glUniform4f(glGetUniformLocation(p,"uBaseFactor"),.4f,.5f,.2f,1);
    scalar(p,"uAlphaCutoff",0);scalar(p,"uRoughness",.8f);scalar(p,"uMetallic",0);
    scalar(p,"uTemporalPhase",0);scalar(p,"uHaze",0);
    scalar(p,"uCloud",0);scalar(p,"uTime",0);scalar(p,"uWetness",0);scalar(p,"uActorReactive",0);
    integer(p,"uWindHistoryEnabled",0);scalar(p,"uWindFrameDelta",0);scalar(p,"uWind",0);
    glUniform2f(glGetUniformLocation(p,"uWeatherWind"),0,0);
    glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);
    scalar(p,"uTreeHeight",25);scalar(p,"uPixelScale",10000);
    vector(p,"uSun",0,1,0);vector(p,"uSunColor",0,0,0);
    vector(p,"uAmbient",.3f,.3f,.3f);vector(p,"uFog",.6f,.7f,.8f);
    vector(p,"uSkyLight",.6f,.7f,.8f);vector(p,"uGroundLight",.1f,.15f,.2f);
    vector(p,"uPropLodRange",0,0,0);
    integer(p,"uClipBelowWater",0);scalar(p,"uWaterClipLevel",0);
    integer(p,"uVillageSkyEnabled",0);integer(p,"uVillageDirectionalEnabled",0);vector(p,"uVillageBounceAlbedo",0,0,0);integer(p,"uFacadeMode",0);
    integer(p,"uCastleLightsEnabled",0);
    integer(p,"uLocalProbe0",4);integer(p,"uLocalProbe1",5);integer(p,"uLocalProbeCount",0);scalar(p,"uLocalIrradianceWeight",0);
    integer(p,"uArchitectureWeathering",0);
}
void check_architecture_weathering(GLuint p) {
    defaults(p);integer(p,"uVegetation",0);scalar(p,"uTreeHeight",12);
    vector(p,"testRoot",0,0,-30);vector(p,"testWorld",20,24.25f,40);
    vector(p,"testNormal",0,0,1);scalar(p,"testRootDrop",.25f);
    const auto energy=[](const Pixel &v){return v[0]+v[1]+v[2];};
    const auto clean=draw(p);integer(p,"uArchitectureWeathering",1);const auto weathered=draw(p);
    const auto base=center(clean),low=center(weathered);
    assert(energy(low)<energy(base)*.997f&&energy(low)>energy(base)*.80f);
    for(std::size_t k=0;k<clean.size();++k)assert(clean[k][3]==weathered[k][3]);
    // Ground splash is fixed to the source/world coordinates. Simulation
    // time and the128m-relative camera origin must not animate its pattern.
    scalar(p,"uTime",10000);assert(draw(p)==weathered);scalar(p,"uTime",0);
    vector(p,"testWorld",20-128,24.25f,40-256);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),128,256);
    assert(draw(p)==weathered);
    vector(p,"testWorld",20,24.25f,40);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);
    // A sharp aligned GGX highlight loses far more than the maximum13% base
    // tint. This proves roughening as well as darkening in the real shader.
    vector(p,"uAmbient",0,0,0);vector(p,"uSkyLight",0,0,0);vector(p,"uGroundLight",0,0,0);
    vector(p,"uSun",0,0,1);vector(p,"uSunColor",1,1,1);scalar(p,"uRoughness",.25f);
    integer(p,"uArchitectureWeathering",0);const auto sharp=center(draw(p));
    integer(p,"uArchitectureWeathering",1);const auto rough=center(draw(p));
    assert(energy(rough)>0&&energy(rough)<energy(sharp)*.8f);
    scalar(p,"testRootDrop",1.3f);integer(p,"uArchitectureWeathering",0);const auto upper=draw(p);
    integer(p,"uArchitectureWeathering",1);assert(draw(p)==upper);
    scalar(p,"testRootDrop",.25f);
    // Plant classes, grass, moving actors, small furnishings and metal retain
    // their own source material response rather than receiving masonry dirt.
    for(int kind:{1,3,4,5,6}) {
        integer(p,"uVegetation",kind);integer(p,"uArchitectureWeathering",0);const auto plant=draw(p);
        integer(p,"uArchitectureWeathering",1);assert(draw(p)==plant);
    }
    integer(p,"uVegetation",0);integer(p,"uGrass",1);integer(p,"uArchitectureWeathering",0);const auto grass=draw(p);
    integer(p,"uArchitectureWeathering",1);assert(draw(p)==grass);integer(p,"uGrass",0);
    scalar(p,"uActorReactive",.85f);integer(p,"uArchitectureWeathering",0);const auto actor=draw(p);
    integer(p,"uArchitectureWeathering",1);assert(draw(p)==actor);scalar(p,"uActorReactive",0);
    scalar(p,"uTreeHeight",3);integer(p,"uArchitectureWeathering",0);const auto furnishing=draw(p);
    integer(p,"uArchitectureWeathering",1);assert(draw(p)==furnishing);scalar(p,"uTreeHeight",12);
    scalar(p,"uMetallic",1);integer(p,"uArchitectureWeathering",0);const auto metal=draw(p);
    integer(p,"uArchitectureWeathering",1);assert(draw(p)==metal);
    defaults(p);
    std::printf("PASS architecture weathering: lower-wall energy%.4f, GGX peak%.4f of clean source; upper walls/vegetation/actors/small props/metals exact; fixed world/time/origin and unchanged alpha\n",energy(low)/energy(base),energy(rough)/energy(sharp));
}
void check_lod_partition(GLuint p) {
    defaults(p);
    for(float distance:{62.f,70.f,78.f,82.f,215.f,235.f,250.f,265.f}) {
        vector(p,"testRoot",0,0,-distance);
        // Crown vertices cross the CPU selection boundaries; the root must
        // nevertheless supply one constant LOD weight for this entire plant.
        scalar(p,"testSpread",50);
        std::array<std::vector<Pixel>,3> masks;
        float near=1-smooth(62,82,distance),far=smooth(215,265,distance);
        for(int lod=0;lod<3;lod++) {
            integer(p,"uVegetation",lod==2?4:1);integer(p,"uLodLevel",lod);
            masks[lod]=draw(p);
            expect_fraction(masks[lod],lod==0?near:(lod==1?1-near-far:far));
        }
        for(size_t i=0;i<masks[0].size();i++)
            assert(int(covered(masks[0][i]))+int(covered(masks[1][i]))+int(covered(masks[2][i]))==1);
    }
    std::puts("PASS foliage: root-based near/mid/far partition has no opaque holes or overlap");
}
void check_combined_fades(GLuint p) {
    defaults(p);integer(p,"uVegetation",4);integer(p,"uLodLevel",2);
    vector(p,"testRoot",0,0,-235);
    const float center_distance=std::hypot(235.f,12.5f);
    const float lod=smooth(215,265,235);
    for(float projected:{2.f,3.f,4.f,5.f,6.f,7.f,8.f}) {
        scalar(p,"uPixelScale",projected*center_distance/25);
        expect_fraction(draw(p),lod*smooth(3,7,projected));
    }
    // The five-pixel card must retain half its LOD interval, not disappear
    // because that interval begins above the same random rank's size fade.
    scalar(p,"uPixelScale",5*center_distance/25);
    const auto flat=draw(p);
    scalar(p,"testSpread",200);
    const auto wide=draw(p);
    for(size_t i=0;i<flat.size();i++)assert(covered(flat[i])==covered(wide[i]));
    scalar(p,"testSpread",0);scalar(p,"uPixelScale",10000);
    vector(p,"testRoot",0,0,-1565);
    for(int lod_level:{2,3}) {
        integer(p,"uLodLevel",lod_level);
        expect_fraction(draw(p),.5f);
    }
    std::puts("PASS foliage: projected-size and distance coverage multiply inside the LOD interval");
}
void check_lighting_and_haze(GLuint p) {
    defaults(p);
    const Pixel opaque_up=center(draw(p));
    vector(p,"testNormal",0,-1,0);
    const Pixel opaque_down=center(draw(p));
    scalar(p,"uAlphaCutoff",.4f);
    const Pixel leaf_down=center(draw(p));
    vector(p,"testNormal",0,1,0);
    const Pixel leaf_up=center(draw(p));
    for(int k=0;k<3;k++) {
        assert(opaque_up[k]>leaf_up[k]&&leaf_up[k]>leaf_down[k]&&leaf_down[k]>opaque_down[k]);
        assert(leaf_up[k]/leaf_down[k]<opaque_up[k]/opaque_down[k]);
    }
    vector(p,"uSkyLight",0,0,0);vector(p,"uGroundLight",0,0,0);
    vector(p,"uAmbient",0,0,0);vector(p,"uSunColor",1,1,1);
    vector(p,"testNormal",1,0,0);scalar(p,"uAlphaCutoff",0);
    const Pixel bark_sun=center(draw(p));
    scalar(p,"uAlphaCutoff",.4f);const Pixel leaf_sun=center(draw(p));
    for(int k=0;k<3;k++)assert(bark_sun[k]==0&&leaf_sun[k]>.01f);
    // Zero new uniforms must remain compatible with old lighting callers.
    defaults(p);
    vector(p,"uSkyLight",0,0,0);vector(p,"uGroundLight",0,0,0);
    const Pixel fallback=center(draw(p));
    assert(fallback[0]>0&&fallback[1]>0&&fallback[2]>0);
    defaults(p);scalar(p,"uAlphaCutoff",.4f);
    vector(p,"testWorld",0,1200,0);
    float factors[2];
    for(int kind=0;kind<2;kind++) {
        integer(p,"uVegetation",kind?4:1);scalar(p,"uHaze",0);
        const Pixel clear=center(draw(p));
        scalar(p,"uHaze",.001f);
        const Pixel hazy=center(draw(p));
        factors[kind]=(hazy[0]-clear[0])/(.6f-clear[0]);
        assert(factors[kind]>.1f&&factors[kind]<.4f);
    }
    assert(std::fabs(factors[0]-factors[1])<.0001f);
    std::puts("PASS foliage: two-sided leaf hemisphere, opaque wood, legacy light fallback and equal mesh/card haze");
}
void check_mask_and_normal_fade(GLuint p,GLuint base,GLuint normal) {
    defaults(p);integer(p,"uHasBase",1);scalar(p,"uAlphaCutoff",.4f);
    std::vector<Pixel> gradient(32*32);
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)gradient[y*32+x]={1,1,1,float(x)/31};
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,base);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,32,32,0,GL_RGBA,GL_FLOAT,gradient.data());
    for(int kind:{1,4}) {
        integer(p,"uVegetation",kind);
        float average=0;
        for(int phase=0;phase<8;phase++) {
            scalar(p,"uTemporalPhase",float(phase)/8);
            const auto pixels=draw(p);average+=fraction(pixels)/8;
            for(int y=0;y<size;y++) {
                assert(!covered(pixels[y*size]));
                assert(covered(pixels[y*size+size-1]));
            }
        }
        assert(std::fabs(average-.6f)<.012f);
    }
    // Exact-zero alpha stays empty even when other fades overlap.
    for(auto &pixel:gradient)pixel[3]=0;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,32,32,0,GL_RGBA,GL_FLOAT,gradient.data());
    expect_fraction(draw(p),0);
    defaults(p);integer(p,"uHasNormal",1);scalar(p,"uAlphaCutoff",.4f);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,normal);
    const Pixel tilted={.95f,.1f,.5f,1};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,tilted.data());
    vector(p,"testRoot",0,0,-90.f);
    const Pixel mapped_ambient=center(draw(p));
    integer(p,"uHasNormal",0);const Pixel geometric_ambient=center(draw(p));
    for(int k=0;k<3;k++)assert(std::fabs(mapped_ambient[k]-geometric_ambient[k])<.000001f);
    integer(p,"uHasNormal",1);vector(p,"uSunColor",1,.9f,.7f);
    vector(p,"testRoot",0,0,-124.99f);const Pixel before=center(draw(p));
    vector(p,"testRoot",0,0,-125.01f);const Pixel after=center(draw(p));
    vector(p,"testRoot",0,0,-90.f);const Pixel detail=center(draw(p));
    for(int k=0;k<3;k++)assert(std::fabs(before[k]-after[k])<.0001f);
    assert(std::fabs(detail[0]-after[0])>.01f);
    std::puts("PASS foliage: temporal alpha edges preserve empty/opaque pixels; normal detail fades continuously");
}
std::vector<bool> shadow_mask(GLuint p) {
    glUseProgram(p);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
    glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,6);
    std::vector<float> depth(size*size);glReadPixels(0,0,size,size,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
    std::vector<bool> mask(depth.size());for(std::size_t k=0;k<depth.size();++k){assert(std::isfinite(depth[k]));mask[k]=depth[k]<.9999f;}
    const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDisable(GL_DEPTH_TEST);assert(glGetError()==GL_NO_ERROR);return mask;
}
float shadow_fraction(GLuint p) {const auto mask=shadow_mask(p);return float(std::count(mask.begin(),mask.end(),true))/mask.size();}
void check_prop_lod(GLuint color,GLuint shadow,GLuint base) {
    std::vector<Pixel> alpha(32*32,Pixel{1,1,1,1});for(int y=0;y<32;++y)for(int x=0;x<16;++x)alpha[y*32+x][3]=0;
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,base);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,32,32,0,GL_RGBA,GL_FLOAT,alpha.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    for(bool masked:{false,true})for(float distance:{75.f,76.f,78.f,80.f,82.f,84.f,85.f}) {
        for(GLuint p:{color,shadow}){defaults(p);integer(p,"uVegetation",masked?3:0);integer(p,"uHasBase",masked);scalar(p,"uAlphaCutoff",masked?.4f:0.f);vector(p,"testRoot",0,0,-distance);scalar(p,"testSpread",50);}
        const auto original=draw(color);const auto original_shadow=shadow_mask(shadow);
        std::array<std::vector<Pixel>,2> colors;std::array<std::vector<bool>,2> shadows;
        for(int lod=0;lod<2;++lod) {
            glUseProgram(color);vector(color,"uPropLodRange",80,4,float(lod+1));colors[lod]=draw(color);
            glUseProgram(shadow);vector(shadow,"uPropLodRange",80,4,float(lod+1));shadows[lod]=shadow_mask(shadow);
        }
        for(std::size_t k=0;k<original.size();++k) {
            assert(int(covered(colors[0][k]))+int(covered(colors[1][k]))==int(covered(original[k])));
            assert(int(shadows[0][k])+int(shadows[1][k])==int(original_shadow[k]));
            for(int lod=0;lod<2;++lod)assert(covered(colors[lod][k])==shadows[lod][k]);
        }
        float expected=(masked?.5f:1.f)*(1-smooth(76,84,distance));assert(std::abs(fraction(colors[0])-expected)<.006f);
    }
    defaults(color);defaults(shadow);
    std::puts("PASS props: opaque/masked near+mid4m overlap is complementary in color/shadow, root based, zero legacy range preserves coverage");
}
void check_minified_needles(GLuint color,GLuint shadow,GLuint base) {
    // Independent signal: source needle rows occupy exactly25% of each8x8
    // tile. Box-generated coarse mips therefore have alpha.25, below the.4
    // hard threshold. Correct subpixel coverage must retain the source area.
    constexpr int extent=256;
    std::vector<unsigned char> pixels(extent*extent*4);
    for(int y=0;y<extent;y++)for(int x=0;x<extent;x++) {
        auto *p=pixels.data()+(y*extent+x)*4;p[0]=102;p[1]=128;p[2]=51;p[3]=y%8<2?255:0;
    }
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,base);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,extent,extent,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    for(GLuint p:{color,shadow}) {
        defaults(p);integer(p,"uHasBase",1);scalar(p,"uAlphaCutoff",.4f);scalar(p,"testUVScale",128);
        glUniform4f(glGetUniformLocation(p,"uBaseFactor"),1,1,1,1);vector(p,"testRoot",0,0,-20);
        for(int kind:{1,3,5}) {
            integer(p,"uVegetation",kind);
            if(p==color)expect_fraction(draw(p),.25f);
            else assert(std::fabs(shadow_fraction(p)-.25f)<.006f);
        }
        // A whole photographed card and an alpha-masked solid keep the exact
        // old cutout policy; this is only subpixel3D vegetation coverage.
        for(int kind:{0,4}) {
            integer(p,"uVegetation",kind);
            if(p==color)expect_fraction(draw(p),0);
            else assert(shadow_fraction(p)==0);
        }
    }
    defaults(color);integer(color,"uHasBase",1);scalar(color,"uAlphaCutoff",.4f);
    scalar(color,"testUVScale",128);vector(color,"testRoot",0,0,-20);
    glUniform4f(glGetUniformLocation(color,"uBaseFactor"),1,1,1,1);
    float average=0;
    for(int phase=0;phase<8;phase++){scalar(color,"uTemporalPhase",phase/8.f);average+=fraction(draw(color))/8;}
    assert(std::fabs(average-.25f)<.006f);
    scalar(color,"uTemporalPhase",0);
    std::vector<bool> combined(size*size,false);
    for(int layer=0;layer<4;layer++) {
        scalar(color,"testCoveragePhase",layer*.25f);auto pixels=draw(color);
        for(std::size_t i=0;i<pixels.size();i++)combined[i]=combined[i]||covered(pixels[i]);
    }
    std::size_t union_coverage=0;for(bool pixel:combined)union_coverage+=pixel;
    assert(union_coverage==combined.size());
    std::puts("PASS foliage: minified needle mips preserve25% source opacity in color/shadow; near masks/far cards unchanged, overlapping shoots decorrelated");
}
void check_stationary_coverage(GLuint p,GLuint base) {
    // A stationary half-covered leaf must not twinkle merely because the
    // temporal sample index advances. This signal is independent of the
    // terrain, wind and history resolve: one constant alpha mask, eight phases.
    const Pixel half_mask={1,1,1,.4f};
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,base);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,half_mask.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    for(int kind:{1,3,4,5}) {
        defaults(p);integer(p,"uVegetation",kind);integer(p,"uHasBase",1);
        scalar(p,"uAlphaCutoff",.4f);vector(p,"testRoot",0,0,-20);
        glUniform4f(glGetUniformLocation(p,"uBaseFactor"),1,1,1,1);
        const auto first=draw(p);expect_fraction(first,.5f);
        std::size_t changed=0;
        for(int phase=1;phase<8;phase++) {
            scalar(p,"uTemporalPhase",phase/8.f);const auto next=draw(p);
            expect_fraction(next,.5f);
            for(std::size_t i=0;i<first.size();++i)changed+=covered(first[i])!=covered(next[i]);
        }
        if(changed)std::fprintf(stderr,"Stationary kind%d alpha mask changed %zu pixels over eight temporal phases\n",kind,changed);
        assert(changed==0);
    }
    // Independent alpha and LOD ranks must retain the product of their areas,
    // independently of the temporal sample index supplied by the caller.
    defaults(p);integer(p,"uVegetation",1);integer(p,"uHasBase",1);
    scalar(p,"uAlphaCutoff",.4f);vector(p,"testRoot",0,0,-72);
    glUniform4f(glGetUniformLocation(p,"uBaseFactor"),1,1,1,1);
    for(int phase=0;phase<8;phase++)for(int lod=0;lod<2;lod++) {
        scalar(p,"uTemporalPhase",phase/8.f);integer(p,"uLodLevel",lod);
        expect_fraction(draw(p),.25f);
    }
    std::puts("PASS foliage: stationary fractional masks have zero phase-driven changes; alpha/LOD area remains unbiased across all eight phases");
}
void check_instance_coverage(GLuint p,GLuint base) {
    for(int kind:{0,1,3,4,5}) {
        defaults(p);integer(p,"uVegetation",kind);vector(p,"testRoot",0,0,-20);
        const auto full=draw(p);expect_fraction(full,1);
        for(float fade:{0.f,.25f,.5f,.75f,1.f}) {
            scalar(p,"testInstanceFade",fade);expect_fraction(draw(p),fade);
        }
        assert(draw(p)==full);
    }
    defaults(p);integer(p,"uVegetation",5);integer(p,"uHasBase",1);
    scalar(p,"uAlphaCutoff",.4f);vector(p,"testRoot",0,0,-20);
    scalar(p,"testInstanceFade",.5f);
    glUniform4f(glGetUniformLocation(p,"uBaseFactor"),1,1,1,1);
    const Pixel half_mask={1,1,1,.4f};
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,base);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,half_mask.data());
    // Root opacity must multiply leaf opacity rather than replacing it.
    expect_fraction(draw(p),.25f);
    std::puts("PASS foliage: rooted fades preserve 0/25/50/75/100% area, multiply masked leaf opacity, and restore full RGB exactly");
}
void check_stationary_lod(GLuint p) {
    // No camera, wind, root or opacity changes: merely advancing the temporal
    // index must not swap a stationary tree/prop between representations.
    struct Sample {int kind,lod;float distance,prop;};
    for(const Sample sample:std::array<Sample,8>{{
        {1,0,72,0},{1,1,72,0},{1,1,240,0},{4,2,240,0},
        {4,2,1565,0},{4,3,1565,0},{0,-1,72,1},{0,-1,72,2}}}) {
        defaults(p);integer(p,"uVegetation",sample.kind);vector(p,"testRoot",0,0,-sample.distance);
        if(sample.prop>0)vector(p,"uPropLodRange",72,4,sample.prop);
        else integer(p,"uLodLevel",sample.lod);
        const auto first=draw(p);expect_fraction(first,.5f);
        std::size_t changed=0;
        for(int phase=1;phase<8;phase++) {
            scalar(p,"uTemporalPhase",phase/8.f);const auto next=draw(p);
            expect_fraction(next,.5f);
            for(std::size_t i=0;i<first.size();++i)changed+=covered(first[i])!=covered(next[i]);
        }
        if(changed)std::fprintf(stderr,"Stationary kind%d LOD%d mask changed %zu pixels over eight temporal phases\n",sample.kind,sample.lod,changed);
        assert(changed==0);
    }
    std::puts("PASS foliage: stationary near/mid/far/horizon tree and prop LOD masks have zero phase-driven changes");
}
Pixel indirect_center() {
    Pixel p;glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(size/2,size/2,1,1,GL_RGBA,GL_FLOAT,p.data());
    glReadBuffer(GL_COLOR_ATTACHMENT0);for(float value:p)assert(std::isfinite(value));return p;
}
void check_opaque_environment(GLuint p,GLuint normal) {
    defaults(p);integer(p,"uVegetation",0);scalar(p,"uMetallic",1);scalar(p,"uRoughness",.25f);
    glUniform4f(glGetUniformLocation(p,"uBaseFactor"),.5f,.5f,.5f,1);
    vector(p,"testRoot",0,0,-30);vector(p,"testNormal",0,.70710678f,.70710678f);
    vector(p,"uSkyLight",.03f,.08f,1);vector(p,"uGroundLight",1,.05f,.02f);
    const Pixel sky=center(draw(p));const Pixel sky_indirect=indirect_center();
    vector(p,"testNormal",0,-.70710678f,.70710678f);const Pixel ground=center(draw(p));
    assert(sky[2]>sky[0]*2&&ground[0]>ground[2]*2);
    for(int k=0;k<3;++k)assert(std::fabs(sky[k]-sky_indirect[k])<.000001f);
    const float smooth_contrast=std::fabs(sky[2]-ground[2])+std::fabs(sky[0]-ground[0]);
    scalar(p,"uRoughness",1);const Pixel rough_ground=center(draw(p));
    vector(p,"testNormal",0,.70710678f,.70710678f);const Pixel rough_sky=center(draw(p));
    const float rough_contrast=std::fabs(rough_sky[2]-rough_ground[2])+std::fabs(rough_sky[0]-rough_ground[0]);
    assert(rough_contrast<smooth_contrast*.85f);
    Pixel previous=rough_sky;
    for(int i=1;i<=75;++i) {
        scalar(p,"uRoughness",1-i*.01f);const Pixel next=center(draw(p));
        for(int k=0;k<3;++k)assert(std::fabs(next[k]-previous[k])<.025f);
        previous=next;
    }
    // A metal receives reflected environment energy, not the dielectric's
    // extra base-color diffuse term. This uses actual HDR outputs.
    vector(p,"uSkyLight",1,1,1);vector(p,"uGroundLight",1,1,1);
    glUniform4f(glGetUniformLocation(p,"uBaseFactor"),.25f,.25f,.25f,1);
    scalar(p,"uRoughness",1);const Pixel metal=center(draw(p));
    scalar(p,"uMetallic",0);const Pixel dielectric=center(draw(p));
    for(int k=0;k<3;++k)assert(metal[k]<dielectric[k]*.8f);
    // Neither a dielectric nor a metal invents light in a dark environment.
    vector(p,"uSkyLight",0,0,0);vector(p,"uGroundLight",0,0,0);vector(p,"uAmbient",0,0,0);
    for(float metallic:{0.f,1.f}) {
        scalar(p,"uMetallic",metallic);const Pixel dark=center(draw(p));
        for(int k=0;k<3;++k)assert(dark[k]==0);
    }
    // Broad reflections use geometric normals, so the existing 80--125m
    // normal-map fade cannot create a new reflected-sky distance transition.
    vector(p,"uSkyLight",.2f,.3f,.8f);vector(p,"uGroundLight",.2f,.1f,.04f);
    integer(p,"uHasNormal",1);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,normal);
    const Pixel tilted={.9f,.2f,.8f,1};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,tilted.data());
    vector(p,"testRoot",0,0,-50);const Pixel near=center(draw(p));
    for(float distance:{80.f,110.f,125.f,140.f}) {
        vector(p,"testRoot",0,0,-distance);const Pixel farther=center(draw(p));
        for(int k=0;k<3;++k)assert(std::fabs(near[k]-farther[k])<.000001f);
    }
    // An actual constant depth map must remove only direct sun. Both shadow
    // states keep identical environment radiance in the indirect attachment.
    integer(p,"uHasNormal",0);vector(p,"testRoot",0,0,-30);
    const float centered_shadow[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,centered_shadow);
    GLuint shadow;glGenTextures(1,&shadow);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,shadow);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    integer(p,"uShadow",2);integer(p,"uSoftShadows",0);
    float depth=1;glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&depth);
    const Pixel environment=center(draw(p));
    vector(p,"uSunColor",2,1.8f,1.4f);const Pixel direct=center(draw(p));const Pixel lit_indirect=indirect_center();
    depth=0;glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&depth);
    const Pixel shadowed=center(draw(p));const Pixel shaded_indirect=indirect_center();
    for(int k=0;k<3;++k) {
        assert(direct[k]>environment[k]);assert(std::fabs(shadowed[k]-environment[k])<.000001f);
        assert(std::fabs(lit_indirect[k]-shaded_indirect[k])<.000001f);
        assert(std::fabs(lit_indirect[k]-environment[k])<.000001f);
    }
    glDeleteTextures(1,&shadow);integer(p,"uShadow",0);glActiveTexture(GL_TEXTURE0);
    std::printf("PASS opaque environment: colored metal sky/ground reflection, rough contrast %.4f < %.4f, no metal diffuse, dark environment, stable normal LOD and sun-shadow isolation\n",rough_contrast,smooth_contrast);
}
void check_indirect_radiance(GLuint p) {
    for(int kind:{0,1,3,4,5}) {
        defaults(p);integer(p,"uVegetation",kind);vector(p,"testRoot",0,0,-30);
        auto ambient=center(draw(p));auto indirect=indirect_center();assert(indirect[3]==0);
        for(int k=0;k<3;k++)assert(std::fabs(ambient[k]-indirect[k])<.000001f);
        vector(p,"uSunColor",2,1.8f,1.4f);auto lit=center(draw(p));auto sun_indirect=indirect_center();
        for(int k=0;k<3;k++){assert(lit[k]>ambient[k]);assert(std::fabs(sun_indirect[k]-indirect[k])<.000001f);}
        scalar(p,"uHaze",.01f);draw(p);auto fog_indirect=indirect_center();
        float transmission=std::exp(-30*.01f*1.12f);
        for(int k=0;k<3;k++)assert(std::fabs(fog_indirect[k]-indirect[k]*transmission)<.00001f);
    }
    std::puts("PASS foliage: MRT stores only fog-transmitted indirect radiance; direct sunlight/reactive alpha stay separate");
}
void check_actor_reactivity(GLuint p) {
    defaults(p);integer(p,"uVegetation",0);vector(p,"testRoot",0,0,-10);
    const auto original=draw(p);const Pixel indirect=indirect_center();
    assert(std::fabs(center(original)[3]-.995f)<1e-6f);
    scalar(p,"uActorReactive",.85f);const auto actor=draw(p);
    for(std::size_t i=0;i<actor.size();i++) {
        assert(std::abs(actor[i][3]-.15f)<.000001f);
        for(int k=0;k<3;k++)assert(actor[i][k]==original[i][k]);
    }
    assert(indirect_center()==indirect);
    scalar(p,"uActorReactive",2.f);assert(std::abs(center(draw(p))[3]-.05f)<.000001f);
    // Even a weak actor flag must not collide with the reserved rigid tag.
    for(float reactive:{.0001f,.005f,.01f,.05f}) {
        scalar(p,"uActorReactive",reactive);const auto weak=draw(p);
        assert(center(weak)[3]<=.99f);
        for(std::size_t q=0;q<weak.size();q++)for(int c=0;c<3;c++)assert(weak[q][c]==original[q][c]);
    }
    scalar(p,"uActorReactive",-1.f);assert(draw(p)==original);
    scalar(p,"uActorReactive",0.f);assert(draw(p)==original);
    scalar(p,"uWind",.1f);assert(center(draw(p))[3]==1.f);
    scalar(p,"uWind",0.f);assert(draw(p)==original);
    std::puts("PASS actors: moving-resident temporal mask lowers history confidence only; RGB/indirect unchanged, bounded inputs and exact reset");
}
void check_wind_history(GLuint p) {
    for(int kind:{1,3,4,5}) {
        defaults(p);integer(p,"uVegetation",kind);vector(p,"testRoot",0,0,-6);vector(p,"testWorld",0,20,0);scalar(p,"testRootDrop",20);
        scalar(p,"uAlphaCutoff",.4f);scalar(p,"uWind",1);
        const Pixel legacy=center(draw(p));
        integer(p,"uWindHistoryEnabled",1);scalar(p,"uWindFrameDelta",0);
        const Pixel held=center(draw(p));assert(std::fabs(held[3]-.98f)<.000001f);
        for(int k=0;k<3;k++)assert(held[k]==legacy[k]);
        scalar(p,"uWindFrameDelta",1.f/60.f);
        const Pixel moving=center(draw(p));assert(moving[3]<=legacy[3]+.001f);
        for(int k=0;k<3;k++)assert(moving[k]==legacy[k]);
        scalar(p,"testRootDrop",0);assert(std::fabs(center(draw(p))[3]-.98f)<.000001f);
        scalar(p,"testRootDrop",20);scalar(p,"uWind",0);assert(std::fabs(center(draw(p))[3]-.98f)<.000001f);
        scalar(p,"uWind",1);scalar(p,"uWindFrameDelta",0);scalar(p,"uActorReactive",.85f);
        assert(std::fabs(center(draw(p))[3]-.15f)<.000001f);
        if(kind==1||kind==4) {
            scalar(p,"uActorReactive",0);scalar(p,"uWindFrameDelta",1.f/60.f);vector(p,"testRoot",0,0,-1000);
            assert(center(draw(p))[3]>.95f);
        }
    }
    defaults(p);integer(p,"uVegetation",0);integer(p,"uWindHistoryEnabled",1);scalar(p,"uWindFrameDelta",1.f/60.f);
    assert(std::fabs(center(draw(p))[3]-.995f)<1e-6f);
    defaults(p);
    std::puts("PASS wind history: static/far/root foliage converges; close wind retains reactive confidence, actor mask wins; RGB and opaque walls unchanged");
}
void check_cloud_coherence(GLuint p) {
    defaults(p);scalar(p,"uAlphaCutoff",.4f);scalar(p,"uTime",42);
    vector(p,"uSun",.36f,.8f,-.48f);
    float minimum=1,maximum=0;
    for(int point=0;point<16;point++) {
        vector(p,"testWorld",float(point*1237-9000),120,float(point*811-5300));
        float factors[2];
        for(int kind=0;kind<2;kind++) {
            integer(p,"uVegetation",kind?4:1);
            vector(p,"uSunColor",0,0,0);scalar(p,"uCloud",0);
            const Pixel ambient=center(draw(p));
            scalar(p,"uCloud",1);const Pixel ambient_cloud=center(draw(p));
            for(int k=0;k<3;k++)assert(ambient[k]==ambient_cloud[k]);
            vector(p,"uSunColor",1,.9f,.7f);scalar(p,"uCloud",0);
            const Pixel clear=center(draw(p));
            scalar(p,"uCloud",.01f);const Pixel negligible=center(draw(p));
            for(int k=0;k<3;k++)assert(clear[k]==negligible[k]);
            scalar(p,"uCloud",1);const Pixel cloudy=center(draw(p));
            factors[kind]=(cloudy[0]-ambient[0])/(clear[0]-ambient[0]);
            assert(factors[kind]>=.6599f&&factors[kind]<=1.0001f);
        }
        assert(std::fabs(factors[0]-factors[1])<.0001f);
        minimum=std::min(minimum,factors[0]);maximum=std::max(maximum,factors[0]);
    }
    assert(maximum-minimum>.05f&&minimum<.95f);
    for(int kind:{1,4}) {
        integer(p,"uVegetation",kind);scalar(p,"uCloud",1);
        vector(p,"testWorld",1245.25f,120,-453.75f);
        const Pixel original=center(draw(p));
        vector(p,"testWorld",1117.25f,120,-197.75f);
        glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),128,-256);
        const Pixel shifted=center(draw(p));
        for(int k=0;k<4;k++)assert(original[k]==shifted[k]);
        glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);
        vector(p,"testWorld",1245.25f,3200,-453.75f);
        const Pixel above=center(draw(p));
        scalar(p,"uCloud",0);const Pixel clear=center(draw(p));
        for(int k=0;k<4;k++)assert(above[k]==clear[k]);
    }
    std::printf("PASS foliage: coherent mesh/card cloud direct light %.3f..%.3f, unchanged sky fill, cloud-zero and world-anchor invariance\n",minimum,maximum);
}
void check_real_vertex(GLuint p) {
    glUseProgram(p);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,identity);
    integer(p,"uVegetation",4);scalar(p,"uTime",17);scalar(p,"uWind",0);
    glDisableVertexAttribArray(4);glDisableVertexAttribArray(5);
    glVertexAttrib1f(5,.7f);
    glVertexAttrib4f(4,12,0,26,1);vector(p,"uOffset",-12,0,-26);
    glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);
    const auto before=draw(p);const Pixel phase_before=indirect_center();
    glReadBuffer(GL_COLOR_ATTACHMENT1);std::vector<Pixel> phases(size*size);
    glReadPixels(0,0,size,size,GL_RGBA,GL_FLOAT,phases.data());glReadBuffer(GL_COLOR_ATTACHMENT0);
    float minimum=1,maximum=0;
    for(const auto &pixel:phases){minimum=std::min(minimum,pixel[0]);maximum=std::max(maximum,pixel[0]);}
    assert(minimum>=0&&maximum<1&&maximum-minimum>.01f);
    assert(fraction(before)>.99f); // A camera directly over the root must not collapse its card.
    glVertexAttrib4f(4,-116,0,-230,1);vector(p,"uOffset",116,0,230);
    glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),128,256);
    const auto after=draw(p);const Pixel phase_after=indirect_center();
    for(int k=0;k<4;k++)assert(phase_before[k]==phase_after[k]);
    for(size_t i=0;i<before.size();i++)for(int k=0;k<4;k++)assert(before[i][k]==after[i][k]);
    std::puts("PASS foliage: actual vertex billboard stays finite over its root and preserves origin-shift seed/LOD roots/subpixel phase");
}
void check_wetness(GLuint p) {
    for(int kind:{0,1,3,4,5}) {
        defaults(p);integer(p,"uVegetation",kind);scalar(p,"uAlphaCutoff",kind?.4f:0.f);
        vector(p,"testRoot",0,0,-30);const Pixel dry=center(draw(p));
        scalar(p,"uWetness",1);const Pixel wet=center(draw(p));
        // Wet opaque surfaces may gain a bounded reflected-sky highlight as
        // roughness falls. Thin leaves/cards retain their diffuse contract.
        for(int k=0;k<3;++k)assert(wet[k]>=dry[k]*.88f&&wet[k]<=dry[k]*(kind?1.001f:1.15f));
        assert(wet[3]==dry[3]);
        vector(p,"uSunColor",1,.9f,.7f);
        for(float angle:{0.f,.4f,.9f,1.3f}) {
            vector(p,"testNormal",std::sin(angle),std::cos(angle),0);const Pixel lit=center(draw(p));
            for(int k=0;k<3;++k)assert(lit[k]>=0&&lit[k]<8);
        }
        scalar(p,"uWetness",0);vector(p,"uSunColor",0,0,0);vector(p,"testNormal",0,1,0);
        const Pixel restored=center(draw(p));for(int k=0;k<4;++k)assert(restored[k]==dry[k]);
    }
    std::puts("PASS foliage: wet dielectric response stays finite, retains diffuse light/coverage, zero wetness restores exactly");
}
void check_minified_specular(GLuint p,GLuint normal) {
    auto normal_length=[&](float length) {
        const Pixel texel={.5f,.5f,.5f+.5f*length,1};
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,normal);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,texel.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    };
    float maximum_ratio=0;
    for(int kind:{0,1,3})for(float wet:{0.f,1.f}) {
        defaults(p);integer(p,"uVegetation",kind);integer(p,"uHasNormal",1);
        scalar(p,"uAlphaCutoff",kind?.4f:0.f);scalar(p,"uWetness",wet);
        scalar(p,"uRoughness",.25f);vector(p,"testRoot",0,0,-30);
        vector(p,"testNormal",0,0,1);vector(p,"uSun",0,0,1);
        vector(p,"uSunColor",1,1,1);vector(p,"uAmbient",0,0,0);
        vector(p,"uSkyLight",0,0,0);vector(p,"uGroundLight",0,0,0);
        normal_length(1);const auto resolved=draw(p);const Pixel sharp=center(resolved);
        normal_length(.5f);const auto minified=draw(p);const Pixel filtered=center(minified);
        for(int k=0;k<3;k++) {
            assert(sharp[k]>0&&filtered[k]>0);
            maximum_ratio=std::max(maximum_ratio,filtered[k]/sharp[k]);
            if(!(filtered[k]<sharp[k]*.98f))std::fprintf(stderr,"specular kind%d wet%.1f channel%d sharp%.6f filtered%.6f ratio%.6f\n",kind,wet,k,sharp[k],filtered[k],filtered[k]/sharp[k]);
            assert(filtered[k]<sharp[k]*.98f);
        }
        // Widening the specular lobe never changes geometric coverage or the
        // motion/reactive mask, and a resolved normal returns byte-exactly.
        for(std::size_t i=0;i<resolved.size();i++)assert(resolved[i][3]==minified[i][3]);
        normal_length(1);assert(draw(p)==resolved);
        // Once detail normals have faded out, mip length cannot change the
        // broad geometric shading. No new 125m material discontinuity.
        vector(p,"testRoot",0,0,-125);const auto distant=draw(p);
        normal_length(.5f);assert(draw(p)==distant);
    }
    normal_length(1);glActiveTexture(GL_TEXTURE0);defaults(p);
    std::printf("PASS minified specular: dry/wet opaque roofs and leaves filter unresolved normal highlights (peak ratio <= %.4f), exact resolved/far reset and coverage\n",maximum_ratio);
}
void check_reflection_clip(GLuint p) {
    for(int kind:{0,1,3,4,5}) {
        defaults(p);integer(p,"uVegetation",kind);vector(p,"testRoot",0,0,-30);
        integer(p,"uClipBelowWater",1);scalar(p,"uWaterClipLevel",134);
        vector(p,"testWorld",0,133.99f,0);expect_fraction(draw(p),0);
        vector(p,"testWorld",0,134.01f,0);expect_fraction(draw(p),1);
        // Main rendering ignores the retained raised-plane value.
        integer(p,"uClipBelowWater",0);vector(p,"testWorld",0,133.99f,0);expect_fraction(draw(p),1);
        integer(p,"uClipBelowWater",1);scalar(p,"uWaterClipLevel",0);
        vector(p,"testWorld",0,-.01f,0);expect_fraction(draw(p),0);
        vector(p,"testWorld",0,.01f,0);expect_fraction(draw(p),1);
    }
    defaults(p);
    std::puts("PASS foliage: raised/sea reflection planes clip by exact world height; disabled main pass preserves coverage");
}
void check_village_sky_access(GLuint p) {
    GLuint mask=0;glGenTextures(1,&mask);glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,mask);
    std::array<Pixel,16> texels;for(auto &pixel:texels)pixel={0,0,0,.3f};
    glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,2,2,4,0,GL_RGBA,GL_FLOAT,texels.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    for(int kind:{0,1,3,4}) {
        defaults(p);integer(p,"uVegetation",kind);vector(p,"testRoot",0,0,-30);vector(p,"testWorld",50,24,50);
        glUniform4f(glGetUniformLocation(p,"uVillageSkyBounds"),0,0,.01f,.01f);
        glUniform2f(glGetUniformLocation(p,"uVillageSkyHeights"),24,18);
        const auto disabled=draw(p);const Pixel open=center(disabled);const Pixel open_indirect=indirect_center();
        integer(p,"uVillageSkyEnabled",1);const auto enabled=draw(p);const Pixel sheltered=center(enabled);const Pixel sheltered_indirect=indirect_center();
        for(int k=0;k<3;k++){assert(sheltered[k]<open[k]);assert(std::fabs(sheltered[k]-sheltered_indirect[k])<1e-6f);}
        for(std::size_t k=0;k<enabled.size();k++)assert(enabled[k][3]==disabled[k][3]);
        vector(p,"uSunColor",1,.9f,.8f);const Pixel sheltered_lit=center(draw(p));
        integer(p,"uVillageSkyEnabled",0);const Pixel open_lit=center(draw(p));
        for(int k=0;k<3;k++)assert(std::fabs((sheltered_lit[k]-sheltered[k])-(open_lit[k]-open[k]))<2e-6f);
        vector(p,"uSunColor",0,0,0);integer(p,"uVillageSkyEnabled",1);
        vector(p,"testWorld",50,42,50);const auto recovered=draw(p);integer(p,"uVillageSkyEnabled",0);assert(draw(p)==recovered);
        vector(p,"testWorld",101,24,50);const auto outside=draw(p);integer(p,"uVillageSkyEnabled",1);assert(draw(p)==outside);
        vector(p,"testWorld",-78,24,50);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),128,0);const auto shifted=draw(p);assert(shifted==enabled);
        glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);vector(p,"testWorld",50,24,50);
        vector(p,"uSkyLight",0,0,0);const auto ground=draw(p);integer(p,"uVillageSkyEnabled",0);assert(draw(p)==ground);
        assert(open_indirect[3]==sheltered_indirect[3]);
    }
    // CPU endpoint nodes must map to texture centers, like the terrain pass.
    // With two nodes(.3,1), worldfraction.25 means skyaccess.475, not.3.
    texels[13][3]=texels[15][3]=1;
    glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,mask);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,0,2,2,4,GL_RGBA,GL_FLOAT,texels.data());
    defaults(p);integer(p,"uVegetation",0);vector(p,"testRoot",0,0,-30);vector(p,"testWorld",25,24,50);
    vector(p,"uGroundLight",0,0,0);glUniform4f(glGetUniformLocation(p,"uVillageSkyBounds"),0,0,.01f,.01f);
    glUniform2f(glGetUniformLocation(p,"uVillageSkyHeights"),24,18);
    const Pixel open=center(draw(p));integer(p,"uVillageSkyEnabled",1);const Pixel quarter=center(draw(p));
    for(int k=0;k<3;k++)assert(std::fabs(quarter[k]-open[k]*.475f)<2e-6f);
    glDeleteTextures(1,&mask);glActiveTexture(GL_TEXTURE0);defaults(p);
    std::puts("PASS village sky access: bounded sheltered sky fill only, direct/ground/coverage unchanged,42m height recovery, outside-mask and world-anchor exactness, disabled reset");
}
void check_village_ground_return(GLuint p) {
    GLuint mask;glGenTextures(1,&mask);glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,mask);
    std::array<Pixel,4> field;for(auto &a:field)a={0,0,0,.5f};
    glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,1,1,4,0,GL_RGBA,GL_FLOAT,field.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    const auto albedo=[&](float amount){vector(p,"uVillageBounceAlbedo",.16384777f*amount,.12907667f*amount,.09379962f*amount);};
    defaults(p);integer(p,"uVegetation",0);integer(p,"uVillageSkyEnabled",1);
    vector(p,"testWorld",50,24,50);vector(p,"testRoot",0,0,-30);
    glUniform4f(glGetUniformLocation(p,"uVillageSkyBounds"),0,0,.01f,.01f);
    glUniform2f(glGetUniformLocation(p,"uVillageSkyHeights"),24,18);
    vector(p,"uSunColor",1,.8f,.6f);vector(p,"uSkyLight",.3f,.4f,.5f);vector(p,"uGroundLight",.04f,.04f,.04f);
    for(const auto &normal:std::array<std::array<float,3>,3>{{{0,-1,0},{0,0,1},{0,1,0}}}) {
        vector(p,"testNormal",normal[0],normal[1],normal[2]);albedo(0);
        const auto before=draw(p);const Pixel color0=center(before),indirect0=indirect_center();
        albedo(1);const auto after=draw(p);const Pixel color1=center(after),indirect1=indirect_center();
        for(int k=0;k<3;k++) {
            assert(color1[k]>color0[k]);
            // One diffuse bounce cannot approach the incident source energy.
            assert(indirect1[k]-indirect0[k]<.08f);
            assert(std::fabs((color1[k]-indirect1[k])-(color0[k]-indirect0[k]))<2e-6f);
        }
        for(std::size_t k=0;k<after.size();k++)assert(after[k][3]==before[k][3]);
        albedo(.5f);const Pixel half=center(draw(p));
        for(int k=0;k<3;k++)assert(std::fabs((half[k]-color0[k])*2-(color1[k]-color0[k]))<3e-6f);
        // Both default-disabled and excluded-domain paths are exact, including
        // explicit actors' confidence. Source geometry/normal response stays live.
        scalar(p,"uActorReactive",.85f);albedo(0);const auto actor0=draw(p);albedo(1);const auto actor1=draw(p);
        for(std::size_t k=0;k<actor1.size();k++)assert(actor1[k][3]==actor0[k][3]);
        scalar(p,"uActorReactive",0);
        vector(p,"testWorld",101,24,50);albedo(0);const auto outside=draw(p);albedo(1);assert(draw(p)==outside);
        vector(p,"testWorld",50,42,50);albedo(0);const auto recovered=draw(p);albedo(1);assert(draw(p)==recovered);
        vector(p,"testWorld",50,24,50);
    }
    for(int kind:{1,3,4,5}) {integer(p,"uVegetation",kind);albedo(0);const auto leaves=draw(p);albedo(1);assert(draw(p)==leaves);}
    integer(p,"uVegetation",0);vector(p,"uSunColor",0,0,0);vector(p,"uSkyLight",0,0,0);vector(p,"uGroundLight",0,0,0);vector(p,"uAmbient",0,0,0);
    const Pixel dark=center(draw(p));for(int k=0;k<3;k++)assert(dark[k]==0);
    glDeleteTextures(1,&mask);defaults(p);glActiveTexture(GL_TEXTURE0);
    std::puts("PASS village ground return: bounded linear source reflectance, eave/wall/up-facing response, indirect-only/direct exact, no emission, actor alpha exact, vegetation/exterior/height unchanged");
}
void check_village_directional_sky(GLuint p) {
    WorldVillageSkyConfig grid;grid.columns=grid.rows=129;
    WorldVillageIndirectConfig config;config.columns=config.rows=65;
    std::vector<float> roofs(grid.columns*grid.rows,24.f);
    WorldVillageIndirectField field;
    GLuint volumes[2];glGenTextures(2,volumes);
    const auto upload=[&]() {
        assert(world_village_indirect_build(roofs.data(),roofs.size(),grid,config,field));
        GLint alignment=0;glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        for(int kind=0;kind<2;kind++) {
            glActiveTexture(GL_TEXTURE13+kind);glBindTexture(GL_TEXTURE_3D,volumes[kind]);
            glTexImage3D(GL_TEXTURE_3D,0,kind?GL_R8:GL_RGBA8,GLsizei(config.columns),GLsizei(config.rows),GLsizei(config.slices),0,
                kind?GL_RED:GL_RGBA,GL_UNSIGNED_BYTE,kind?field.up_r.data():field.horizontal_rgba.data());
            glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glActiveTexture(GL_TEXTURE0);assert(glGetError()==GL_NO_ERROR);
    };
    const auto setup=[&]() {
        defaults(p);integer(p,"uVegetation",0);vector(p,"testWorld",64,24,64);vector(p,"testRoot",0,0,-30);
        glUniform4f(glGetUniformLocation(p,"uVillageSkyBounds"),0,0,1.f/128,1.f/128);
        vector(p,"uVillageDirectionalHeights",config.first_height,1.f/config.height_step,float(config.slices));
        vector(p,"uSkyLight",.3f,.4f,.5f);vector(p,"uGroundLight",.04f,.05f,.06f);
    };
    const auto same=[](const Pixel &a,const Pixel &b,float epsilon=0.f) {
        for(int c=0;c<4;c++)assert(std::fabs(a[c]-b[c])<=epsilon);
    };
    const auto energy=[](const Pixel &a){return a[0]+a[1]+a[2];};
    upload();setup();
    for(const auto &normal:std::array<std::array<float,3>,7>{{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{.6f,.8f,0}}}) {
        vector(p,"testNormal",normal[0],normal[1],normal[2]);integer(p,"uVillageDirectionalEnabled",0);
        const auto open=draw(p);integer(p,"uVillageDirectionalEnabled",1);assert(draw(p)==open);
    }
    for(std::size_t z=0;z<grid.rows;z++)roofs[z*grid.columns+69]=34.f;
    upload();setup();vector(p,"testNormal",1,0,0);
    const auto flat=draw(p);const Pixel flat_color=center(flat),flat_indirect=indirect_center();
    integer(p,"uVillageDirectionalEnabled",1);const auto directional=draw(p);
    const Pixel wall=center(directional),wall_indirect=indirect_center();
    assert(energy(wall)<energy(flat_color)*.30f&&energy(wall)>0);
    for(std::size_t k=0;k<flat.size();k++)assert(flat[k][3]==directional[k][3]);
    for(int c=0;c<3;c++)assert(std::fabs((wall[c]-wall_indirect[c])-(flat_color[c]-flat_indirect[c]))<2e-6f);
    vector(p,"testNormal",-1,0,0);same(flat_color,center(draw(p)));
    vector(p,"testNormal",1,0,0);vector(p,"uSun",.6f,.8f,0);vector(p,"uSunColor",1,.9f,.8f);
    const Pixel lit_wall=center(draw(p));integer(p,"uVillageDirectionalEnabled",0);const Pixel lit_flat=center(draw(p));
    for(int c=0;c<3;c++)assert(std::fabs((lit_wall[c]-wall[c])-(lit_flat[c]-flat_color[c]))<2e-6f);
    vector(p,"uSunColor",0,0,0);integer(p,"uVillageDirectionalEnabled",1);
    // Actual GL3D interpolation between measured height slices must agree
    // with the endpoint radiance interpolation, including specular energy.
    vector(p,"testWorld",64,27,64);const Pixel higher=center(draw(p));
    assert(energy(higher)>energy(wall));
    for(float fraction:{.25f,.5f,.75f}) {
        vector(p,"testWorld",64,24+3*fraction,64);const Pixel between=center(draw(p));
        for(int c=0;c<4;c++)assert(std::fabs(between[c]-(wall[c]*(1-fraction)+higher[c]*fraction))<3e-6f);
    }
    vector(p,"testWorld",64,48,64);const auto roofline=draw(p);integer(p,"uVillageDirectionalEnabled",0);assert(draw(p)==roofline);
    integer(p,"uVillageDirectionalEnabled",1);vector(p,"testWorld",64,24,64);
    const auto anchored=draw(p);vector(p,"testWorld",-64,24,64);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),128,0);assert(draw(p)==anchored);
    glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);vector(p,"testWorld",64,24,64);
    scalar(p,"uTime",9999);assert(draw(p)==anchored);
    // Filtered source ARM remains live. AO scales the indirect result by the
    // same authored factor, and metals retain environment-only reflection.
    GLuint arm;glGenTextures(1,&arm);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,arm);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    const auto arm_value=[&](float ao) {const Pixel value={ao,.6f,.8f,1};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,value.data());};
    integer(p,"uHasARM",1);integer(p,"uARM",2);
    for(float metal:{0.f,1.f}) {
        scalar(p,"uMetallic",metal);arm_value(1);const Pixel unoccluded=center(draw(p));
        arm_value(.2f);const Pixel occluded=center(draw(p));
        assert(energy(unoccluded)>0);
        for(int c=0;c<3;c++)assert(std::fabs(occluded[c]-unoccluded[c]*.584f)<3e-6f);
    }
    glDeleteTextures(1,&arm);glActiveTexture(GL_TEXTURE0);integer(p,"uHasARM",0);scalar(p,"uMetallic",0);
    scalar(p,"uActorReactive",.85f);integer(p,"uVillageDirectionalEnabled",0);const auto actor0=draw(p);
    integer(p,"uVillageDirectionalEnabled",1);const auto actor1=draw(p);
    for(std::size_t k=0;k<actor0.size();k++)assert(actor0[k][3]==actor1[k][3]);
    for(const auto &point:std::array<std::array<float,3>,4>{{{-1,24,64},{129,24,64},{64,24,-1},{64,24,129}}}) {
        vector(p,"testWorld",point[0],point[1],point[2]);integer(p,"uVillageDirectionalEnabled",0);const auto exterior=draw(p);
        integer(p,"uVillageDirectionalEnabled",1);assert(draw(p)==exterior);
    }
    vector(p,"testWorld",64,24,64);
    for(int vegetation:{1,3,4,5}) {
        integer(p,"uVegetation",vegetation);integer(p,"uVillageDirectionalEnabled",0);const auto plants=draw(p);
        integer(p,"uVillageDirectionalEnabled",1);assert(draw(p)==plants);
    }
    integer(p,"uVegetation",0);integer(p,"uGrass",1);integer(p,"uVillageDirectionalEnabled",0);const auto grass=draw(p);
    integer(p,"uVillageDirectionalEnabled",1);assert(draw(p)==grass);integer(p,"uGrass",0);
    vector(p,"uSkyLight",0,0,0);integer(p,"uVillageDirectionalEnabled",0);const auto ground=draw(p);
    integer(p,"uVillageDirectionalEnabled",1);assert(draw(p)==ground);
    vector(p,"uGroundLight",0,0,0);vector(p,"uAmbient",0,0,0);assert(energy(center(draw(p)))==0);
    // Place the wall near the bake edge: disabling outside and the2m guard
    // must join continuously, rather than adding a camera-dependent seam.
    std::fill(roofs.begin(),roofs.end(),24.f);
    for(std::size_t z=0;z<grid.rows;z++)roofs[z*grid.columns+1]=34.f;
    upload();setup();vector(p,"testNormal",1,0,0);vector(p,"testWorld",0,24,64);
    const Pixel boundary=center(draw(p));integer(p,"uVillageDirectionalEnabled",1);same(boundary,center(draw(p)));
    float max_step=0;Pixel previous=boundary;
    for(int i=1;i<=80;i++) {
        vector(p,"testWorld",i*.025f,24,64);const Pixel current=center(draw(p));
        max_step=std::max(max_step,std::fabs(energy(current)-energy(previous)));previous=current;
    }
    assert(max_step<energy(boundary)*.025f);
    glDeleteTextures(2,volumes);defaults(p);glActiveTexture(GL_TEXTURE0);
    std::printf("PASS directional village sky: real roof-field facade ratio%.4f; open/exterior exact; direct/ground/ARM/metal/actor-alpha preserved; height trilinear, world anchor/time stable; edge max25mm change%.4f%%\n",energy(wall)/energy(flat_color),100*max_step/energy(boundary));
}
void check_castle_practicals(GLuint p) {
    WorldCastleLights lights;
    assert(world_castle_lights_build(-1310.4,-10207.58474745458,lights));
    const auto valid=lights;
    assert(!world_castle_lights_build(NAN,0,lights)&&lights.position_radius==valid.position_radius&&lights.bounds==valid.bounds);
    assert(!world_castle_lights_build(0,100801,lights)&&lights.bounds==valid.bounds);
    for(const auto &light:lights.position_radius) {
        for(float value:light)assert(std::isfinite(value));
        assert(light[0]>lights.bounds[0]+.25f&&light[0]<lights.bounds[2]-.25f);
        assert(light[2]>lights.bounds[1]+.25f&&light[2]<lights.bounds[3]-.25f);
        assert(light[1]>lights.heights[0]+.15f&&light[1]<lights.heights[1]-.15f&&light[3]==20.f);
    }
    defaults(p);integer(p,"uVegetation",0);vector(p,"testRoot",0,0,-3);vector(p,"testNormal",0,0,1);
    vector(p,"uSkyLight",0,0,0);vector(p,"uGroundLight",0,0,0);vector(p,"uAmbient",0,0,0);
    glUniform4fv(glGetUniformLocation(p,"uCastleLightBounds"),1,lights.bounds.data());
    glUniform2fv(glGetUniformLocation(p,"uCastleLightHeights"),1,lights.heights.data());
    glUniform3fv(glGetUniformLocation(p,"uCastleLightColor"),1,lights.color.data());
    const auto sources=[&](int selected) {
        auto values=lights.position_radius;
        for(int i=0;i<4;i++)if(selected>=0&&i!=selected)values[i][3]=0;
        glUniform4fv(glGetUniformLocation(p,"uCastleLightPositionRadius[0]"),4,values[0].data());
    };
    const auto point=[&](float x,float y,float z) {vector(p,"testWorld",x,y,z);};
    const auto energy=[](const Pixel &value){return value[0]+value[1]+value[2];};
    const auto &first=lights.position_radius[0];sources(0);
    point(first[0],first[1],first[2]-2);
    const auto disabled=draw(p);const Pixel dark_indirect=indirect_center();
    integer(p,"uCastleLightsEnabled",1);const auto lit=draw(p);const Pixel dielectric=center(lit);
    assert(energy(dielectric)>.1f&&indirect_center()==dark_indirect);
    for(std::size_t i=0;i<lit.size();i++)assert(lit[i][3]==disabled[i][3]);
    // N=V=L=(0,0,1) gives an independent closed-form GGX response. This
    // catches wrong units, diffuse-only lamps, source hue/albedo edits and
    // accidental ambient/AO changes while exercising the actual shader.
    const float radius=first[3],ratio=4/(radius*radius),attenuation=(1-ratio*ratio)*(1-ratio*ratio)/4;
    constexpr float pi=3.14159265358979323846f;
    const std::array<float,3> base={{.4f,.5f,.2f}};
    for(int c=0;c<3;c++) {
        const float expected=((1-.035f)*base[c]/pi+.035f/(4*pi*std::pow(.8f,4)))*attenuation*lights.color[c];
        assert(std::fabs(dielectric[c]-expected)<2e-6f);
    }
    scalar(p,"uMetallic",1);scalar(p,"uRoughness",.4f);const Pixel metal=center(draw(p));
    for(int c=0;c<3;c++)assert(std::fabs(metal[c]-base[c]/(4*pi*std::pow(.4f,4))*attenuation*lights.color[c])<1e-5f);
    scalar(p,"uMetallic",0);scalar(p,"uRoughness",.8f);
    vector(p,"testNormal",0,0,-1);assert(energy(center(draw(p)))==0);vector(p,"testNormal",0,0,1);
    scalar(p,"uTime",10000);assert(draw(p)==lit);scalar(p,"uTime",0);
    point(first[0]-128,first[1],first[2]-2-256);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),128,256);
    assert(draw(p)==lit);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);
    // Exact radius rejection and finite source center, including the zero
    // direction vector. All are real HDR readbacks with GL-error checks.
    vector(p,"testNormal",-1,0,0);
    for(float distance:{radius-.5f,radius-.001f,radius,radius+.001f}) {
        // Across the hall width these points remain inside the shell mask,
        // so a zero outside radius tests the range window itself.
        point(first[0]+distance,first[1],first[2]);const Pixel edge=center(draw(p));
        if(distance<radius-.1f)assert(energy(edge)>1e-5f);else assert(energy(edge)<1e-6f);
    }
    vector(p,"testNormal",0,0,1);
    point(first[0],first[1],first[2]);draw(p);
    // No color or coverage changes outside any of the six shell planes.
    for(const auto &position:std::array<std::array<float,3>,6>{{
        {lights.bounds[0],26.7f,first[2]}, {lights.bounds[2],26.7f,first[2]},
        {first[0],26.7f,lights.bounds[1]}, {first[0],26.7f,lights.bounds[3]},
        {first[0],lights.heights[0],first[2]}, {first[0],lights.heights[1],first[2]}}}) {
        point(position[0],position[1],position[2]);integer(p,"uCastleLightsEnabled",0);const auto outside=draw(p);
        integer(p,"uCastleLightsEnabled",1);assert(draw(p)==outside);
    }
    point(first[0],24.015f,first[2]);vector(p,"testNormal",0,1,0);assert(energy(center(draw(p)))>.01f);
    point(first[0],31.f,first[2]);vector(p,"testNormal",0,-1,0);assert(energy(center(draw(p)))>.01f);
    // The wall feather is continuous within solid thickness and leaves the
    // inner wall fully lit. It must not become a spatial hard light seam.
    vector(p,"testNormal",1,0,0);float previous=0,max_step=0,final_energy=0;
    for(int i=0;i<=10;i++) {
        point(lights.bounds[0]+i*.025f,26.7f,first[2]);float current=energy(center(draw(p)));
        assert(current>=previous);max_step=std::max(max_step,current-previous);previous=current;final_energy=current;
    }
    assert(final_energy>.01f&&max_step<final_energy*.20f);
    // Four sources sum direct radiance; no source changes the separate
    // indirect attachment, nor the moving-actor temporal confidence.
    point((lights.bounds[0]+lights.bounds[2])*.5f,25.f,(first[2]+lights.position_radius[2][2])*.5f);
    vector(p,"testNormal",0,.70710678f,.70710678f);scalar(p,"uActorReactive",.85f);
    Pixel sum={};
    for(int i=0;i<4;i++) {sources(i);const Pixel single=center(draw(p));for(int c=0;c<3;c++)sum[c]+=single[c];}
    sources(-1);const auto all=draw(p);const Pixel combined=center(all);
    for(int c=0;c<3;c++)assert(std::fabs(combined[c]-sum[c])<2e-6f);
    assert(energy(combined)>.00001f&&std::fabs(combined[3]-.15f)<1e-6f);
    integer(p,"uCastleLightsEnabled",0);const Pixel actor_off=center(draw(p));
    assert(actor_off[3]==combined[3]&&indirect_center()==dark_indirect);
    for(int vegetation:{1,3,4,5}) {
        integer(p,"uVegetation",vegetation);integer(p,"uCastleLightsEnabled",0);const auto plants=draw(p);
        integer(p,"uCastleLightsEnabled",1);assert(draw(p)==plants);
    }
    integer(p,"uVegetation",0);integer(p,"uGrass",1);integer(p,"uCastleLightsEnabled",0);const auto grass=draw(p);
    integer(p,"uCastleLightsEnabled",1);assert(draw(p)==grass);
    defaults(p);
    std::printf("PASS castle practicals: four bounded%.1fm sources, analytical diffuse/GGX metal, floor/ceiling, finite center, exact exterior/vegetation/disabled/indirect/actor alpha, continuous wall feather, world-anchor/time stability; center radiance%.6f\n",radius,energy(combined));
}
void check_weather_vertex(const std::string &vs,const std::string &shadow_fs) {
    const char *varyings[]={"vRelative","vNormal","vRootRelative","vCoveragePhase","vSeed"};
    GLuint programs[]={program(vs.c_str(),root_fragment),program(vs.c_str(),shadow_fs.c_str())};
    for(GLuint p:programs) {glTransformFeedbackVaryings(p,5,varyings,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);assert(okay);}
    GLuint buffer;glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,11*sizeof(float),nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    for(int attribute:{0,1,2,3,4,5})glDisableVertexAttribArray(attribute);
    glVertexAttrib3f(1,1,0,0);glVertexAttrib2f(2,0,0);glVertexAttrib4f(3,0,1,0,1);glVertexAttrib1f(5,.7f);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(GLuint p:programs) {glUseProgram(p);glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,identity);integer(p,"uVegetation",1);scalar(p,"uWind",1);scalar(p,"uTreeHeight",25);scalar(p,"uAlphaCutoff",.4f);}
    auto capture=[&](GLuint p,float height,float wind_x,float wind_z,bool shifted=false,float time=17) {
        glUseProgram(p);glVertexAttrib3f(0,.4f,height,.1f);glVertexAttrib4f(4,shifted?-116.f:12.f,0,shifted?-230.f:26.f,1);
        vector(p,"uOffset",shifted?116.f:-12.f,0,shifted?230.f:-26.f);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),shifted?128.f:0.f,shifted?256.f:0.f);
        glUniform2f(glGetUniformLocation(p,"uWeatherWind"),wind_x,wind_z);scalar(p,"uTime",time);
        glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
        std::array<float,11> result;glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result.data());for(float v:result)assert(std::isfinite(v));return result;
    };
    const auto dry=capture(programs[0],20,0,0),wind=capture(programs[0],20,1.1f,.35f),shadow=capture(programs[1],20,1.1f,.35f);
    float displacement=std::hypot(dry[0]-wind[0],dry[2]-wind[2]);assert(displacement>.015f&&displacement<1.5f);
    for(int k=0;k<11;++k)assert(std::abs(wind[k]-shadow[k])<1e-6f);
    const auto shifted=capture(programs[0],20,1.1f,.35f,true),later=capture(programs[0],20,1.1f,.35f,false,18);
    for(int k=0;k<11;++k)assert(std::abs(wind[k]-shifted[k])<.00001f);
    for(int k=6;k<11;++k)assert(wind[k]==dry[k]&&wind[k]==later[k]);
    assert(std::hypot(wind[0]-later[0],wind[2]-later[2])>.001f);
    assert(std::abs(std::sqrt(wind[3]*wind[3]+wind[4]*wind[4]+wind[5]*wind[5])-1)<.00001f);
    const auto root_dry=capture(programs[0],0,0,0),root_wind=capture(programs[0],0,1.1f,.35f);
    for(int k=0;k<3;++k)assert(root_dry[k]==root_wind[k]);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);for(GLuint p:programs)glDeleteProgram(p);
    glEnableVertexAttribArray(0);assert(glGetError()==GL_NO_ERROR);
    std::printf("PASS weather vertex: %.4fm bounded crown movement, fixed roots/LOD/alpha seeds, origin/time coherence, exact color/shadow positions\n",displacement);
}
}

int main(int argc,char **argv) {
    assert(argc<=2);if(argc==2)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Foliage shader regression",0,0,size,size,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Foliage shader renderer: %s\n",glGetString(GL_RENDERER));
    const std::string fs=source("shaders/world_foliage.fs.glsl"),vs=source("shaders/world_foliage.vs.glsl");
    const std::string shadow_fs=source("shaders/world_foliage_shadow.fs.glsl");
    const GLuint color=program(fixture_vertex,fs.c_str()),vertex=program(vs.c_str(),root_fragment),shadow=program(fixture_vertex,shadow_fs.c_str());
    GLuint fbo,target,indirect,depth,vao,vbo,base,normal;
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,size,size,0,GL_RGBA,GL_FLOAT,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
    glGenTextures(1,&indirect);glBindTexture(GL_TEXTURE_2D,indirect);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,size,size,0,GL_RGBA,GL_FLOAT,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,indirect,0);
    glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,size,size,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);glReadBuffer(GL_COLOR_ATTACHMENT0);
    glGenTextures(1,&base);glGenTextures(1,&normal);
    for(GLuint texture:{base,normal}) {
        glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    const float quad[]={-1,-1,1,-1,1,1,-1,-1,1,1,-1,1};
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glViewport(0,0,size,size);
    check_lod_partition(color);check_combined_fades(color);check_lighting_and_haze(color);
    check_mask_and_normal_fade(color,base,normal);check_minified_needles(color,shadow,base);check_stationary_coverage(color,base);check_instance_coverage(color,base);check_stationary_lod(color);
    check_opaque_environment(color,normal);check_indirect_radiance(color);check_actor_reactivity(color);check_wind_history(color);check_cloud_coherence(color);check_real_vertex(vertex);check_architecture_weathering(color);
    check_wetness(color);check_minified_specular(color,normal);check_reflection_clip(color);check_village_sky_access(color);check_village_ground_return(color);check_village_directional_sky(color);check_castle_practicals(color);check_weather_vertex(vs,shadow_fs);check_prop_lod(color,shadow,base);
    glDeleteProgram(color);glDeleteProgram(vertex);glDeleteProgram(shadow);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);
    for(GLuint texture:{target,indirect,depth,base,normal})glDeleteTextures(1,&texture);
    glDeleteFramebuffers(1,&fbo);assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::puts("PASS foliage shader regression: coverage, root LOD, masks, hemisphere, haze, detail fade and actual vertex contract");
}

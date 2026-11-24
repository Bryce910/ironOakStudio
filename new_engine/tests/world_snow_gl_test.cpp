#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_snow.h"
#include "engine/world_atlas.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

int main() {
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Snow logic verification",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    assert(world_atlas_init());
    constexpr double x=-43344.,z=(.077-.5)*100800.*2818./5009.;
    std::string error;
    WorldSnow *snow=world_snow_create("projects/valwood_stronghold/shaders","",error);assert(snow&&error.empty());
    double t=0,px=x,pz=z+100;
    auto update=[&](bool grounded) {world_snow_update(snow,t,px,world_atlas_height(px,pz),pz,grounded,x,z);};
    update(true);
    // Stationary, airborne, and teleport movement must never stamp snow.
    for(unsigned i=0;i<10;i++){t+=.1;update(true);}
    assert(world_snow_stats(snow).created==0);
    for(unsigned i=0;i<10;i++){t+=.1;px+=.6;update(false);}
    assert(world_snow_stats(snow).created==0);
    t+=.1;px+=20;update(true);assert(world_snow_stats(snow).created==0);
    // A dry point beyond the portal is also excluded.
    pz=z-30;t+=.1;update(true);
    for(unsigned i=0;i<10;i++){t+=.1;px+=.6;update(true);}
    assert(world_snow_stats(snow).created==0);
    pz=z+100;t+=.1;update(true);
    // Frozen plunge-pool ice must not produce powder or boot impressions.
    px=x+63;pz=z+32;t+=.1;update(true);
    for(unsigned i=0;i<10;i++){t+=.1;px+=.6;update(true);}
    assert(world_snow_stats(snow).created==0);
    px=x+26;pz=z+100;t+=.1;update(true);
    // More steps than capacity must alternate soles and reuse fixed storage.
    for(unsigned i=0;i<1000;i++){t+=.15;px+=(i/100)%2?-.6:.6;update(true);}
    auto stats=world_snow_stats(snow);
    assert(stats.created>=990&&stats.footprints==384);
    assert(std::abs(int(stats.left)-int(stats.right))<=1);
    assert(stats.puffs==stats.created*6&&stats.powder<=128);
    assert(stats.imprint_updates>100&&stats.depressed_texels>100);
    assert(stats.deepest_imprint>.03f&&stats.deepest_imprint<.04f);
    unsigned before=stats.created;
    world_snow_update(snow,std::numeric_limits<double>::quiet_NaN(),px,0,pz,true,x,z);
    assert(world_snow_stats(snow).created==before);
    t+=2;update(true);assert(world_snow_stats(snow).powder==0);
    t+=181;update(true);assert(world_snow_stats(snow).footprints==0);
    assert(world_snow_stats(snow).depressed_texels==0&&world_snow_stats(snow).deepest_imprint==0);
    world_snow_free(snow);assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS: stationary/air/dry/teleport/ice rejection, alternating soles, bounded height field, depression depth, expiry, clean GL teardown\n");
}

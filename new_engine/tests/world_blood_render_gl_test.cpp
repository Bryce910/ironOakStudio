#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_blood_render.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
constexpr unsigned width=640,height=384;
static unsigned red_pixels(const std::vector<float> &pixels){unsigned count=0;for(unsigned i=0;i<pixels.size();i+=4)count+=pixels[i]>.012f&&pixels[i]>pixels[i+1]*1.35f&&pixels[i]>pixels[i+2]*1.35f;return count;}
struct Pigment {double mean=0;float peak=0;unsigned pixels=0;};
static Pigment pigment(const std::vector<float> &pixels){Pigment result;for(unsigned i=0;i<pixels.size();i+=4)if(pixels[i]>.012f&&pixels[i]>pixels[i+1]*1.35f&&pixels[i]>pixels[i+2]*1.35f){result.mean+=pixels[i];result.peak=std::max(result.peak,pixels[i]);++result.pixels;}if(result.pixels)result.mean/=result.pixels;return result;}
static void capture(const char *name,const std::vector<float> &pixels){
    const char *directory=std::getenv("VALWOOD_BLOOD_TEST_CAPTURE_DIR");if(!directory)return;
    std::vector<unsigned char> bytes(pixels.size());
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)for(unsigned c=0;c<4;++c)bytes[(y*width+x)*4+c]=static_cast<unsigned char>(std::clamp(pixels[((height-y-1)*width+x)*4+c],0.f,1.f)*255);
    auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(bytes.data(),width,height,32,width*4,SDL_PIXELFORMAT_RGBA32);
    if(surface){char path[4096];std::snprintf(path,sizeof path,"%s/%s.bmp",directory,name);SDL_SaveBMP(surface,path);SDL_FreeSurface(surface);}
}

int main(int argc,char **argv){
    CHECK(argc==2);CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Purchased blood source check",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);CHECK(window);
    const auto context=SDL_GL_CreateContext(window);CHECK(context);std::printf("Renderer: %s\n",glGetString(GL_RENDERER));
    char error[512]={};auto *renderer=world_blood_create(argv[1],error,sizeof error);
    if(!renderer)std::fprintf(stderr,"Blood create: %s\n",error);
    CHECK(renderer);CHECK(world_blood_prepare(renderer,width,height));
    const auto loaded=world_blood_stats(renderer);CHECK(loaded.loaded_variants==3&&loaded.provenance_loaded);
    CHECK(loaded.source_frames>200&&loaded.triangles>9000&&loaded.geometry_bytes>1000000&&loaded.texture_bytes>0);
    CHECK(loaded.draws==0&&loaded.mesh_draws==0&&loaded.failures==0&&loaded.max_duration>2);
    WorldBloodFrame frame;auto &f=frame.target;f.width=width;f.height=height;f.near_plane=.4f;f.far_plane=100;
    f.vp[0]=.9f;f.vp[5]=1.5f;f.vp[10]=-100.4f/99.6f;f.vp[11]=-1;f.vp[14]=-80.f/99.6f;
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=.7f;frame.sun[0]=-.4f;frame.sun[1]=.8f;frame.sun[2]=.3f;
    std::vector<float> background(width*height*4,.18f),depth(width*height,1);
    for(unsigned i=3;i<background.size();i+=4)background[i]=1;
    GLuint textures[2]={},reader=0;glGenTextures(2,textures);glGenFramebuffers(1,&reader);
    for(unsigned i=0;i<2;++i){glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,i?GL_R32F:GL_RGBA16F,width,height,0,i?GL_RED:GL_RGBA,GL_FLOAT,i?depth.data():background.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    f.color=textures[0];f.depth=textures[1];
    auto clear=[&](){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_FLOAT,background.data());glBindTexture(GL_TEXTURE_2D,textures[1]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RED,GL_FLOAT,depth.data());};
    auto read=[&](){std::vector<float> pixels(background.size());glBindFramebuffer(GL_FRAMEBUFFER,reader);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[0],0);glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,pixels.data());return pixels;};
    world_blood_effects::System effects;CHECK(effects.configure(world_blood_config(renderer)));CHECK(effects.config().persistent_stains);
    CHECK(effects.config().variant_count==loaded.loaded_variants&&effects.config().mesh_duration<1);
    world_blood_effects::Input input;input.contacts[1]={{0,.1,-3.2},{0,0,1},-1,true};input.outgoing_hits=1;
    CHECK(effects.update(input,0));clear();CHECK(world_blood_draw(renderer,effects,frame));
    CHECK(world_blood_stats(renderer).active_meshes==1&&world_blood_stats(renderer).secondary_draws==0);
    CHECK(effects.update(input,.08));clear();CHECK(world_blood_draw(renderer,effects,frame));
    const auto early=read();capture("early-fluid",early);CHECK(red_pixels(early)>10);
    CHECK(world_blood_stats(renderer).active_meshes==2&&world_blood_stats(renderer).secondary_draws>0);
    CHECK(effects.update(input,.10));clear();CHECK(world_blood_draw(renderer,effects,frame));
    GLint initial_private_depth=0;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&initial_private_depth);CHECK(initial_private_depth>0);
    const auto first=read();const unsigned first_red=red_pixels(first);capture("first-fluid",first);
    const auto fluid_color=pigment(first);
    std::printf("first_red=%u early_red=%u frame=%u mean_red=%.6f peak_red=%.6f GL_error=%x\n",first_red,red_pixels(early),world_blood_stats(renderer).last_frame,fluid_color.mean,fluid_color.peak,glGetError());
    CHECK(first_red>40&&fluid_color.mean<.25&&fluid_color.peak<.45);
    CHECK(effects.update(input,.2));clear();CHECK(world_blood_draw(renderer,effects,frame));const auto second=read();
    capture("second-fluid",second);unsigned changed=0;for(unsigned i=0;i<first.size();i+=4)changed+=std::abs(first[i]-second[i])>.002||std::abs(first[i+1]-second[i+1])>.002;
    CHECK(changed>4);CHECK(world_blood_stats(renderer).last_frame>0&&world_blood_stats(renderer).frame_transitions>0);
    for(float value:second)CHECK(std::isfinite(value));

    // Pausing preserves both source frame and exact source pixels.
    input.paused=true;CHECK(effects.update(input,10));const auto before_pause=world_blood_stats(renderer);clear();CHECK(world_blood_draw(renderer,effects,frame));const auto paused=read();
    CHECK(world_blood_stats(renderer).last_frame==before_pause.last_frame&&world_blood_stats(renderer).frame_transitions==before_pause.frame_transitions);
    CHECK(paused==second);input.paused=false;

    // A late-life fluid sheet crosses the real floor. An orthographic witness
    // makes its world-Y floor boundary a precise screen row: no fluid may draw
    // below it even when the caller's opaque depth is entirely clear.
    const auto perspective=frame;WorldBloodFrame orthographic=frame;
    std::fill(std::begin(orthographic.target.vp),std::end(orthographic.target.vp),0.f);
    orthographic.target.vp[0]=orthographic.target.vp[5]=.25f;orthographic.target.vp[10]=-.05f;orthographic.target.vp[15]=1;
    auto ground_input=input;ground_input.outgoing_hits=1;
    world_blood_effects::System grounded,unclipped;CHECK(grounded.configure(world_blood_config(renderer))&&unclipped.configure(world_blood_config(renderer)));
    CHECK(grounded.update(ground_input,0));ground_input.contacts[1].floor_y=-10;CHECK(unclipped.update(ground_input,0));
    for(unsigned n=0;n<3;++n){CHECK(grounded.update(ground_input,.14));CHECK(unclipped.update(ground_input,.14));}
    clear();CHECK(world_blood_draw(renderer,grounded,orthographic));const auto clipped=read();capture("late-ground-clipped",clipped);
    clear();CHECK(world_blood_draw(renderer,unclipped,orthographic));const auto below_ground=read();capture("late-unclipped-witness",below_ground);
    unsigned clipped_below=0,witness_below=0;const unsigned floor_row=height*3/8;
    for(unsigned y=0;y<floor_row;++y)for(unsigned x=0;x<width;++x){const auto at=(y*width+x)*4;clipped_below+=clipped[at]>clipped[at+1]*1.35f&&clipped[at]>.012f;witness_below+=below_ground[at]>below_ground[at+1]*1.35f&&below_ground[at]>.012f;}
    std::printf("ground_clip: below=%u witness_below=%u above=%u\n",clipped_below,witness_below,red_pixels(clipped));
    CHECK(clipped_below==0&&witness_below>5&&red_pixels(clipped)>5);frame=perspective;

    // Entire purchased 3D mesh and floor stain disappear behind opaque depth.
    depth.assign(depth.size(),0);clear();CHECK(world_blood_draw(renderer,effects,frame));const auto hidden=read();
    for(unsigned i=0;i<hidden.size();i+=4)CHECK(std::abs(hidden[i]-.18f)<.0002&&std::abs(hidden[i+1]-.18f)<.0002);

    // Reconstruct the known horizontal floor's depth from its perspective ray.
    // Only its purchased stain remains after the source fluid clip completes.
    for(unsigned y=0;y<height;++y){const double ndc=(2.*(y+.5)/height-1),ray_y=ndc/1.5;
        const double distance=ray_y<0?-1./ray_y:10000;
        const float d=distance>=100?1.f:float((100.-.4*100./distance)/(100.-.4));
        for(unsigned x=0;x<width;++x)depth[y*width+x]=d;
    }
    while(effects.stage().mesh_count)CHECK(effects.update(input,.1));
    CHECK(effects.stats().elapsed<1.01); // completed playback also frees every CPU lobe
    clear();CHECK(world_blood_draw(renderer,effects,frame));const auto stained=read();capture("stain",stained);const unsigned stains=red_pixels(stained);
    CHECK(stains>200&&world_blood_stats(renderer).active_meshes==0&&world_blood_stats(renderer).active_stains==1);
    const auto floor_color=pigment(stained);CHECK(floor_color.mean<.16&&floor_color.peak<.24);
    std::printf("floor_pigment: mean_red=%.6f peak_red=%.6f\n",floor_color.mean,floor_color.peak);
    CHECK(effects.rebase(0,0,0));input.outgoing_hits=0;CHECK(effects.update(input,.1));clear();CHECK(world_blood_draw(renderer,effects,frame));CHECK(red_pixels(read())>2);
    const auto original_stain=effects.stage().stains[0];const auto retained=read();
    for(unsigned frame_number=0;frame_number<128;++frame_number)CHECK(effects.update(input,.25));
    clear();CHECK(world_blood_draw(renderer,effects,frame));const auto persistent=read();capture("persistent-after-32s",persistent);
    CHECK(persistent==retained&&effects.stage().stain_count==1&&effects.stage().mesh_count==0);
    CHECK(effects.stage().stains[0].serial==original_stain.serial&&effects.stage().stains[0].position.x==original_stain.position.x&&effects.stage().stains[0].position.z==original_stain.position.z);
    CHECK(world_blood_stats(renderer).minimum_stain_fade==1&&world_blood_stats(renderer).oldest_stain_age>=30&&world_blood_stats(renderer).persistent_stains_drawn==1);
    effects.reset();clear();CHECK(world_blood_draw(renderer,effects,frame));CHECK(world_blood_stats(renderer).active_meshes==0&&world_blood_stats(renderer).active_stains==0);
    CHECK(world_blood_stats(renderer).minimum_stain_fade==1&&world_blood_stats(renderer).oldest_stain_age==0&&world_blood_stats(renderer).persistent_stains_drawn==0);
    const auto empty=read();CHECK(red_pixels(empty)==0);

    // Wet centers visibly grow after impact and finishing hits produce a larger
    // irregular pool. No source-mesh cloud is involved in these floor-only views.
    world_blood_effects::System growing;CHECK(growing.configure({.01,24}));
    world_blood_effects::Input growth=input;growth.outgoing_hits=1;
    CHECK(growing.update(growth,0)&&growing.update(growth,.06));clear();CHECK(world_blood_draw(renderer,growing,frame));
    const unsigned young_stain=red_pixels(read());
    for(unsigned n=0;n<8;++n)CHECK(growing.update(growth,.1));
    clear();CHECK(world_blood_draw(renderer,growing,frame));
    const auto grown=read();capture("grown-pool",grown);const unsigned grown_stain=red_pixels(grown);
    CHECK(grown_stain>young_stain*1.4&&grown_stain>200);
    world_blood_effects::System finishing;CHECK(finishing.configure({.01,24}));growth.contacts[1].strength=1.5f;growth.contacts[1].finisher=true;
    CHECK(finishing.update(growth,0));for(unsigned n=0;n<9;++n)CHECK(finishing.update(growth,.1));
    clear();CHECK(world_blood_draw(renderer,finishing,frame));const auto finish=read();capture("finisher-pool",finish);
    CHECK(finishing.stats().finisher_events==1&&red_pixels(finish)>grown_stain*2);

    // Fixed saturation uses the same resident source geometry and textures.
    for(unsigned group=1;group<=9;++group){input.outgoing_hits=group*16;CHECK(effects.update(input,0));}
    CHECK(effects.update(input,.2));CHECK(effects.stats().stains_replaced==16);
    for(unsigned n=0;n<20;++n)CHECK(world_blood_draw(renderer,effects,frame));
    const auto saturated=world_blood_stats(renderer);CHECK(saturated.active_meshes<=8&&saturated.active_stains==128&&saturated.peak_meshes<=8);
    CHECK(saturated.persistent_stains_drawn==128&&saturated.minimum_stain_fade==1);
    CHECK(saturated.geometry_bytes==loaded.geometry_bytes&&saturated.texture_bytes==loaded.texture_bytes&&saturated.loaded_variants==loaded.loaded_variants);
    GLint stable_private_depth=0;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&stable_private_depth);CHECK(stable_private_depth==initial_private_depth);
    std::vector<float> after_depth(depth.size());glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[1]);glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,after_depth.data());CHECK(after_depth==depth);
    // Invalid prepare preserves usable old storage. A valid resize is explicit;
    // draw rejects stale dimensions instead of silently allocating mid-frame.
    CHECK(!world_blood_prepare(renderer,0,height));CHECK(glIsRenderbuffer(GLuint(initial_private_depth)));
    CHECK(world_blood_draw(renderer,effects,frame));CHECK(world_blood_prepare(renderer,width/2,height/2));
    CHECK(!world_blood_draw(renderer,effects,frame));CHECK(!glIsRenderbuffer(GLuint(initial_private_depth)));
    CHECK(world_blood_prepare(renderer,width,height));CHECK(world_blood_draw(renderer,effects,frame));
    auto bad=frame;bad.target.depth=bad.target.color;CHECK(!world_blood_draw(renderer,effects,bad));
    bad=frame;bad.sun[0]=std::numeric_limits<float>::quiet_NaN();CHECK(!world_blood_draw(renderer,effects,bad));
    CHECK(world_blood_stats(renderer).failures==4&&glGetError()==GL_NO_ERROR);
    world_blood_free(renderer);CHECK(glIsTexture(textures[0])&&glIsTexture(textures[1]));glDeleteFramebuffers(1,&reader);glDeleteTextures(2,textures);
    CHECK(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS %u native purchased-blood checks: %u source-fluid pixels, %u moving pixels, %u purchased-stain pixels; actual geometry animation, pause, opaque occlusion, wave persistence, reset, fixed resident budgets and caller texture ownership\n",checks,first_red,changed,stains);
}

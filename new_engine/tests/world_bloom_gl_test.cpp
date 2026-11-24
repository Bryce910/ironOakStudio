/* Actual GL3.3 bloom, default software renderer: correctness, not FPS.
 * Link with engine/world_bloom.cpp and -Wl,--wrap=glTexImage2D. The wrapper
 * can leave a newly allocated image undefined, exercising transactional FBO
 * failure without forcing dangerous out-of-memory allocations or GL errors.
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_bloom.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <vector>

namespace {
using Pixel=std::array<float,4>;
bool fail_image=false,record_images=false;
unsigned image_calls=0;
std::vector<GLuint> owned_images,owned_fbos;
}
extern "C" void APIENTRY __real_glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void *);
extern "C" void APIENTRY __wrap_glTexImage2D(GLenum target,GLint level,GLint internal,GLsizei w,GLsizei h,GLint border,GLenum format,GLenum type,const void *data){
    image_calls++;
    if(record_images){GLint texture=0,fbo=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);owned_images.push_back(GLuint(texture));owned_fbos.push_back(GLuint(fbo));}
    if(fail_image){fail_image=false;return;}
    __real_glTexImage2D(target,level,internal,w,h,border,format,type,data);
}
namespace {
struct Input {
    GLuint id=0;int w=128,h=128;std::vector<Pixel> data;
    Input(){glGenTextures(1,&id);}
    void upload(){
        glBindTexture(GL_TEXTURE_2D,id);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,w,h,0,GL_RGBA,GL_FLOAT,data.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    void fill(Pixel color){data.assign(std::size_t(w)*h,color);upload();}
    ~Input(){glDeleteTextures(1,&id);}
};
std::vector<Pixel> read(GLuint id,int w,int h){
    std::vector<Pixel> result(std::size_t(w)*h);glBindTexture(GL_TEXTURE_2D,id);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,result.data());
    for(auto p:result){for(float value:p)assert(std::isfinite(value));for(int c=0;c<3;c++)assert(p[c]>=0&&p[c]<=64.001f);assert(p[3]==0);}
    return result;
}
Pixel sample(const Input &in,float x,float y){
    x=std::clamp(x,0.f,float(in.w-1));y=std::clamp(y,0.f,float(in.h-1));
    int ix=int(std::floor(x)),iy=int(std::floor(y)),jx=std::min(ix+1,in.w-1),jy=std::min(iy+1,in.h-1);
    float fx=x-ix,fy=y-iy;Pixel result={};
    for(int c=0;c<3;c++)result[c]=(in.data[iy*in.w+ix][c]*(1-fx)+in.data[iy*in.w+jx][c]*fx)*(1-fy)+(in.data[jy*in.w+ix][c]*(1-fx)+in.data[jy*in.w+jx][c]*fx)*fy;
    return result;
}
Pixel highlight(Pixel color,float exposure){
    for(int c=0;c<3;c++)color[c]=std::clamp(color[c]*exposure,0.f,64.f);
    float maximum=std::max({color[0],color[1],color[2]}),soft=std::clamp(maximum-.75f,0.f,1.f);soft=soft*soft*.5f;
    float factor=std::max(maximum-1.25f,soft)/std::max(maximum,.00001f)/exposure;
    for(int c=0;c<3;c++)color[c]*=factor;
    color[3]=0;return color;
}
std::array<double,3> extraction_energy(const Input &in,float exposure){
    constexpr int offsets[13][2]={{0,0},{-2,2},{2,2},{-2,-2},{2,-2},{0,2},{-2,0},{2,0},{0,-2},{-1,1},{1,1},{-1,-1},{1,-1}};
    constexpr float weights[13]={.125f,.03125f,.03125f,.03125f,.03125f,.0625f,.0625f,.0625f,.0625f,.125f,.125f,.125f,.125f};
    std::array<double,3> total={};int qw=(in.w+3)/4,qh=(in.h+3)/4;
    for(int y=0;y<qh;y++)for(int x=0;x<qw;x++)for(int tap=0;tap<13;tap++){
        auto value=highlight(sample(in,(x+.5f)*in.w/qw-.5f+offsets[tap][0],(y+.5f)*in.h/qh-.5f+offsets[tap][1]),exposure);
        for(int c=0;c<3;c++)total[c]+=value[c]*weights[tap];
    }
    return total;
}
GLuint resolve(WorldBloom *b,const Input &in,float exposure=1){record_images=true;GLuint result=world_bloom_resolve(b,in.id,in.w,in.h,exposure);record_images=false;assert(result&&result!=in.id);return result;}
}
int main(int argc,char **argv){
    const std::filesystem::path root=argc>1?argv[1]:".";
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("HDR bloom tests",0,0,128,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Bloom renderer: %s\n",glGetString(GL_RENDERER));
    auto create=[&](){return world_bloom_create((root/"shaders/world_post.vs.glsl").c_str(),(root/"shaders/world_bloom_extract.fs.glsl").c_str(),(root/"shaders/world_bloom_blur.fs.glsl").c_str());};
    assert(!world_bloom_create("/missing",nullptr,nullptr));assert(!world_bloom_prepare(nullptr,128,128));assert(!world_bloom_resolve(nullptr,1,128,128,1));world_bloom_free(nullptr);
    WorldBloom *b=create();assert(b&&world_bloom_stats(b).bytes==0);
    {
        Input in;in.fill({.5f,.25f,.125f,.7f});GLuint output=resolve(b,in);auto p=read(output,32,32);
        for(auto pixel:p)assert(pixel[0]==0&&pixel[1]==0&&pixel[2]==0);
        auto stats=world_bloom_stats(b);assert(stats.allocations==1&&stats.resolves==1&&stats.quarter_width==32&&stats.quarter_height==32&&stats.bytes==16384);
        in.fill({4,2,1,1});output=resolve(b,in,1.25f);p=read(output,32,32);
        for(auto pixel:p){assert(std::abs(pixel[0]-3)<.006f);assert(std::abs(pixel[1]-1.5f)<.003f);assert(std::abs(pixel[2]-.75f)<.002f);}
        in.fill({1,1,1,0});output=resolve(b,in,1);p=read(output,32,32);float knee=p[0][0];assert(std::abs(knee-.03125f)<.0001f);
        in.fill({0,0,0,0});for(int y=64;y<68;y++)for(int x=64;x<68;x++)in.data[y*in.w+x]={32,16,8,0};in.upload();
        auto reference=extraction_energy(in,1);output=resolve(b,in);p=read(output,32,32);std::array<double,3> measured={};int positive=0;
        for(auto pixel:p){for(int c=0;c<3;c++)measured[c]+=pixel[c];if(pixel[0]>.00001f)positive++;}
        assert(positive>=81&&positive<=144); // A local highlight spreads across the two nine-tap axes.
        for(int c=0;c<3;c++){assert(std::abs(measured[c]-reference[c])/reference[c]<.005);assert(p[0][c]==0);}
        assert(p[16*32+16][0]>0&&p[16*32+19][0]>0&&p[19*32+16][0]>0);
        in.fill({std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),-5,0});output=resolve(b,in);p=read(output,32,32);for(auto pixel:p)assert(pixel[0]==0&&pixel[1]==0&&pixel[2]==0);
        in.fill({1e10f,1e10f,1e10f,0});output=resolve(b,in);p=read(output,32,32);for(auto pixel:p)assert(pixel[0]>62&&pixel[0]<=64);
        const auto before=world_bloom_stats(b);const unsigned before_images=image_calls;
        for(int i=0;i<5;i++)assert(resolve(b,in)==output);
        auto after=world_bloom_stats(b);assert(after.allocations==before.allocations&&after.resolves==before.resolves+5&&image_calls==before_images);
        assert(!world_bloom_resolve(b,output,in.w,in.h,1));assert(!world_bloom_resolve(b,in.id,in.w,in.h,NAN));assert(!world_bloom_resolve(b,in.id,in.w,in.h,0));assert(!world_bloom_prepare(b,0,in.h));
        auto preserved=world_bloom_stats(b);assert(preserved.width==in.w&&preserved.height==in.h&&preserved.bytes==before.bytes&&glIsTexture(output));
        // Force the next allocation to have no defined color image. The new
        // incomplete FBO must be destroyed while the previous result survives.
        fail_image=true;record_images=true;assert(!world_bloom_prepare(b,73,39));record_images=false;assert(!fail_image);
        after=world_bloom_stats(b);assert(after.allocations==preserved.allocations&&after.width==128&&after.quarter_width==32&&after.bytes==preserved.bytes&&glIsTexture(output));
        assert(resolve(b,in)==output); // The preserved pair remains usable.
        in.w=73;in.h=39;in.fill({4,2,1,0});GLuint old_output=output;output=resolve(b,in);assert(output!=old_output&&!glIsTexture(old_output));
        stats=world_bloom_stats(b);assert(stats.allocations==2&&stats.quarter_width==19&&stats.quarter_height==10&&stats.bytes==3040);p=read(output,19,10);
        for(auto pixel:p)assert(std::abs(pixel[0]-2.75f)<.006f);
        const unsigned reuse_images=image_calls;assert(world_bloom_prepare(b,74,40));assert(image_calls==reuse_images&&world_bloom_stats(b).allocations==2); // Same ceil quarter size.
        for(int i=0;i<8;i++){in.w=i%2?73:128;in.h=i%2?39:128;in.fill({4,2,1,0});output=resolve(b,in);assert(output);}
        auto final_stats=world_bloom_stats(b);world_bloom_free(b);b=nullptr;
        assert(glIsTexture(in.id));for(GLuint id:owned_images)assert(!glIsTexture(id));for(GLuint id:owned_fbos)assert(!glIsFramebuffer(id));
        std::printf("PASS HDR bloom: below-knee black, soft knee %.5f, exposed chromatic HDR, localized spread %d pixels, normalized energy %.5f/%.5f, non-finite/firefly containment, stable allocation reuse, odd ceil sizes, feedback rejection, injected failed-resize preservation, %llu complete allocation pairs released\n",knee,positive,measured[0],reference[0],static_cast<unsigned long long>(final_stats.allocations));
    }
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}

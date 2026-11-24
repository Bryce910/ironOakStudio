// Small real BC7 fixture: native linear sampling, both production loaders,
// full-mip angular validation, role isolation and failed-upload fallback.
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "gl_helpers.h"
#include "engine/world_model_texture_blocks.h"
#include "engine/world_model_cpu.h"
#include "tools/world_texture_blocks_encoder.h"
#include "tests/world_texture_blocks_fixture.h"
#include <iostream>
#include <set>
#include <stdexcept>
#include <unistd.h>
extern "C" void __real_glCompressedTexImage2D(GLenum,GLint,GLenum,GLsizei,GLsizei,GLint,GLsizei,const void *);
static std::set<GLuint> linear_images;
static unsigned srgb_uploads=0,failures=0,checks=0;
static bool fail_next=false;
extern "C" void __wrap_glCompressedTexImage2D(GLenum target,GLint level,GLenum format,GLsizei w,GLsizei h,GLint border,GLsizei bytes,const void *data){
    if(format==GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM)srgb_uploads++;
    if(fail_next&&format==GL_COMPRESSED_RGBA_BPTC_UNORM){fail_next=false;failures++;__real_glCompressedTexImage2D(target,level,0,w,h,border,bytes,data);return;}
    __real_glCompressedTexImage2D(target,level,format,w,h,border,bytes,data);
    if(level==0&&format==GL_COMPRESSED_RGBA_BPTC_UNORM){GLint texture=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);linear_images.insert(GLuint(texture));}
}
static void require(bool okay,const std::string &message){checks++;if(!okay)throw std::runtime_error(message);}
static world_texture_blocks::Image cook(){
    namespace bc=world_texture_blocks;const auto png=texture_blocks_fixture::source();
    const GLuint original=GLH_texture_mem(png.data(),int(png.size()));require(original!=0,"Source decode/upload");
    GLuint encoded=0;glGenTextures(1,&encoded);bc::Image image;image.width=image.height=4;image.format=bc::format_normal_bc7;
    double maximum_angle=0,maximum_rms=0;
    for(unsigned mip=0,side=4;;mip++,side/=2){
        std::vector<unsigned char> before(side*side*4),after(before.size());glBindTexture(GL_TEXTURE_2D,original);glGetTexImage(GL_TEXTURE_2D,GLint(mip),GL_RGBA,GL_UNSIGNED_BYTE,before.data());
        bc::Level level;std::string error;require(world_texture_blocks_encoder::encode(before,side,side,level,error),"BC7 encode: "+error);
        glBindTexture(GL_TEXTURE_2D,encoded);glCompressedTexImage2D(GL_TEXTURE_2D,GLint(mip),GL_COMPRESSED_RGBA_BPTC_UNORM,GLsizei(side),GLsizei(side),0,GLsizei(level.blocks.size()),level.blocks.data());glGetTexImage(GL_TEXTURE_2D,GLint(mip),GL_RGBA,GL_UNSIGNED_BYTE,after.data());require(glGetError()==GL_NO_ERROR,"Actual BC7 decoding");
        double squares=0,angles=0,bias[3]={};unsigned maximum_error=0;
        for(unsigned p=0;p<side*side;p++){
            double dot=0,a=0,b=0;
            for(unsigned c=0;c<3;c++){const int d=int(after[p*4+c])-before[p*4+c];squares+=d*d;bias[c]+=d;maximum_error=std::max(maximum_error,unsigned(std::abs(d)));const double x=before[p*4+c]*2./255-1,y=after[p*4+c]*2./255-1;dot+=x*y;a+=x*x;b+=y*y;}
            require(after[p*4+3]==255&&a>=.04&&b>=.04,"Opaque alpha and valid normal vectors");const double angle=std::acos(std::clamp(dot/std::sqrt(a*b),-1.,1.))*180/3.141592653589793;maximum_angle=std::max(maximum_angle,angle);angles+=angle*angle;
        }
        maximum_rms=std::max(maximum_rms,std::sqrt(angles/(side*side)));level.rmse=float(std::sqrt(squares/(side*side*3)));level.max_error=maximum_error;for(unsigned c=0;c<3;c++)level.bias[c]=float(bias[c]/(side*side));
        require(maximum_angle<=2&&maximum_rms<=.75&&bc::detail::linear_quality(level.rmse,level.bias,maximum_error),"Every full-resolution mip meets normal and data quality");image.levels.push_back(std::move(level));if(side==1)break;
    }
    image.normal_max_degrees=float(maximum_angle);image.normal_rms_degrees=float(maximum_rms);glDeleteTextures(1,&encoded);glDeleteTextures(1,&original);linear_images.clear();return image;
}
static void verify_linear_images(){
    require(linear_images.size()==1&&srgb_uploads==0,"Exactly one role uses linear BC7, no sRGB data upload");
    for(const auto texture:linear_images){glBindTexture(GL_TEXTURE_2D,texture);for(unsigned mip=0,side=4;;mip++,side/=2){GLint format=0,width=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(mip),GL_TEXTURE_INTERNAL_FORMAT,&format);glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(mip),GL_TEXTURE_WIDTH,&width);require(format==GL_COMPRESSED_RGBA_BPTC_UNORM&&width==int(side),"Actual native data texture format and dimensions");if(side==1)break;}}
    require(glGetError()==GL_NO_ERROR,"No queued GL errors");linear_images.clear();
}
int main(){
    const auto directory=std::filesystem::temp_directory_path()/("valwood-linear-gl-"+std::to_string(getpid()));
    SDL_Window *window=nullptr;SDL_GLContext context=nullptr;
    try{
        if(!SDL_getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
        require(SDL_Init(SDL_INIT_VIDEO)==0,"SDL video init");SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        window=SDL_CreateWindow("Linear BC7 fixture",0,0,32,32,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);require(window!=nullptr,"Hidden GL window");context=SDL_GL_CreateContext(window);require(context!=nullptr,"GL context");std::cout<<"Renderer: "<<glGetString(GL_RENDERER)<<'\n';
        require(world_model_texture_blocks::available(false,false,world_texture_blocks::format_normal_bc7),"Native BPTC support");
        const auto path=texture_blocks_fixture::model(directory),cache=texture_blocks_fixture::cache(directory);const auto source=texture_blocks_fixture::source();auto blocks=cook();std::string error;
        for(unsigned format:{world_texture_blocks::format_normal_bc7,world_texture_blocks::format_linear_bc7}){
            blocks.format=format;require(world_texture_blocks::write(cache.c_str(),source.data(),source.size(),blocks,error),"Write real cache: "+error);
            auto *model=world_model_load(path.c_str());require(model!=nullptr,"Immediate production loader");verify_linear_images();require(texture_cache_stats().gpu_bytes==144,"Immediate compressed+original+neutral GPU bytes");world_model_free(model);require(texture_cache_stats().entries==0&&texture_cache_stats().gpu_bytes==0,"Immediate ownership cleanup");
            world_model_cpu::Options options;options.bc7=true;auto cpu=world_model_cpu::prepare(path.c_str(),options,error);require(bool(cpu),"CPU prepare: "+error);auto *stage=world_model_stage(cpu,1,1);require(stage!=nullptr,"Staged production loader init");bool complete=false;for(unsigned step=0;step<100&&!complete;step++){std::size_t consumed=0;require(world_model_stage_step(stage,64,&consumed,&complete)&&consumed<=64,"Bounded staged upload");}require(complete,"Staged completion");model=world_model_stage_take(stage);require(model!=nullptr,"Staged owner commit");world_model_stage_free(stage);verify_linear_images();require(texture_cache_stats().gpu_bytes==144,"Staged compressed+original+neutral GPU bytes");world_model_free(model);require(texture_cache_stats().entries==0&&texture_cache_stats().gpu_bytes==0,"Staged ownership cleanup");
        }
        blocks.format=world_texture_blocks::format_normal_bc7;require(world_texture_blocks::write(cache.c_str(),source.data(),source.size(),blocks,error),"Write failure fixture");const auto before=world_model_texture_blocks::counters().upload_failures;fail_next=true;auto *fallback=world_model_load(path.c_str());require(fallback&&failures==1&&!fail_next,"Real invalid linear upload falls back to original");require(world_model_texture_blocks::counters().upload_failures==before+1&&glGetError()==GL_NO_ERROR,"Failure counted without poisoning original upload");require(texture_cache_stats().gpu_bytes==180,"Fallback retains both full original linear images");world_model_free(fallback);require(texture_cache_stats().entries==0&&texture_cache_stats().gpu_bytes==0,"Failure ownership cleanup");
        texture_cache_shutdown();std::filesystem::remove_all(directory);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"Linear BC7 native PASS: "<<checks<<" checks; real encode/decode, immediate/staged roles, exact fallback, full mips and cleanup\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(directory);if(context)SDL_GL_DeleteContext(context);if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;}
}

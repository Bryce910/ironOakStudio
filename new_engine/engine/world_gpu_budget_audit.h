#ifndef VALWOOD_WORLD_GPU_BUDGET_AUDIT_H
#define VALWOOD_WORLD_GPU_BUDGET_AUDIT_H
/* QA-only post-profile inventory of live named GL stores. Never call in the
 * walking loop. Ordinary execution performs no GL queries when the flag is off.
 * ARB_direct_state_access defines GL_TEXTURE_TARGET and documents that DSA
 * cube-level queries inspect positiveX only; multiply its image bytes by6.
 * https://registry.khronos.org/OpenGL/extensions/ARB/ARB_direct_state_access.txt
 * https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGetTexLevelParameter.xhtml
 * Bytes describe named logical stores, excluding driver-private scratch/BO
 * caches, allocation alignment, default framebuffer images and aliased views.
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <utility>

struct WorldGpuBudgetAuditResult {
    bool requested=false,available=false,saved=false,complete=false;
    GLenum preexisting_error=GL_NO_ERROR,query_error=GL_NO_ERROR;
    unsigned query_errors=0,textures=0,buffers=0,renderbuffers=0,unknown_textures=0;
    std::uint64_t texture_logical_bytes=0,texture_storage_estimate_bytes=0,buffer_bytes=0,renderbuffer_bytes=0;
};
namespace world_gpu_budget_detail {
using GetParameter=void(APIENTRY *)(GLuint,GLenum,GLint *);
using GetLevel=void(APIENTRY *)(GLuint,GLint,GLenum,GLint *);
using GetBuffer=void(APIENTRY *)(GLuint,GLenum,GLint64 *);
constexpr GLuint texture_limit=4096,buffer_limit=32768,renderbuffer_limit=4096;
struct Level {int level=0,w=0,h=0,d=0,format=0,samples=1,bits=0;bool compressed=false;std::uint64_t logical=0,storage=0;};
struct Texture {GLuint id=0;GLint target=0,base=0,max=0,view_layers=0,immutable_levels=0;std::uint64_t logical=0,storage=0;std::vector<Level> levels;};
struct Buffer {GLuint id=0;std::uint64_t bytes=0;};
struct Renderbuffer {GLuint id=0;GLint w=0,h=0,format=0,samples=1;std::uint64_t bytes=0;};
inline void string(FILE *f,const char *text){
    std::fputc('"',f);for(const unsigned char *p=reinterpret_cast<const unsigned char *>(text?text:"");*p;p++){
        if(*p=='"'||*p=='\\'){std::fputc('\\',f);std::fputc(*p,f);}
        else if(*p<32)std::fprintf(f,"\\u%04x",unsigned(*p));else std::fputc(*p,f);
    }std::fputc('"',f);
}
inline bool checked(WorldGpuBudgetAuditResult &r){
    const GLenum error=glGetError();if(error==GL_NO_ERROR)return true;
    if(r.query_error==GL_NO_ERROR)r.query_error=error;
    r.query_errors++;return false;
}
inline unsigned storage_texel_bytes(GLint format,unsigned bits){
    switch(format){
        case GL_DEPTH_COMPONENT24:case GL_DEPTH_COMPONENT32:case GL_DEPTH_COMPONENT32F:case GL_DEPTH24_STENCIL8:return 4;
        case GL_DEPTH32F_STENCIL8:return 8;
        case GL_RGB8:case GL_SRGB8:case GL_RGB8_SNORM:return 4;
        case GL_RGB16:case GL_RGB16F:case GL_RGB16_SNORM:return 8;
        case GL_RGB32F:case GL_RGB32I:case GL_RGB32UI:return 16;
        default:return (bits+7)/8;
    }
}
inline const char *target_name(GLint target){
    switch(target){
        case GL_TEXTURE_1D:return "1D";case GL_TEXTURE_2D:return "2D";case GL_TEXTURE_3D:return "3D";
        case GL_TEXTURE_1D_ARRAY:return "1D_ARRAY";case GL_TEXTURE_2D_ARRAY:return "2D_ARRAY";
        case GL_TEXTURE_CUBE_MAP:return "CUBE";case GL_TEXTURE_CUBE_MAP_ARRAY:return "CUBE_ARRAY";
        case GL_TEXTURE_RECTANGLE:return "RECTANGLE";case GL_TEXTURE_2D_MULTISAMPLE:return "2D_MULTISAMPLE";
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:return "2D_MULTISAMPLE_ARRAY";default:return "UNSUPPORTED";
    }
}
inline bool supported_target(GLint target){return std::strcmp(target_name(target),"UNSUPPORTED")!=0;}
inline void report(FILE *f,const WorldGpuBudgetAuditResult &r,const char *status,const std::vector<Texture> &textures,
                   const std::vector<Buffer> &buffers,const std::vector<Renderbuffer> &renderbuffers,bool renderbuffer_dsa){
    std::fprintf(f,"{\n  \"schema_version\":1,\"phase\":\"after_engine_perf_finish_before_teardown\",\"status\":");string(f,status);
    std::fprintf(f,",\n  \"available\":%s,\"complete\":%s,\"bindings_modified\":false,\"image_readbacks\":0,\n  \"renderer\":",r.available?"true":"false",r.complete?"true":"false");
    string(f,reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    std::fprintf(f,",\n  \"preexisting_gl_error\":%u,\"query_gl_error\":%u,\"query_error_batches\":%u,\n  \"texture_scan_last_id\":%u,\"buffer_scan_last_id\":%u,\"renderbuffer_scan_last_id\":%u,\n  \"texture_limit_hit\":%s,\"buffer_limit_hit\":%s,\"renderbuffer_dsa_available\":%s,\n",
        unsigned(r.preexisting_error),unsigned(r.query_error),r.query_errors,texture_limit,buffer_limit,renderbuffer_limit,
        !textures.empty()&&std::any_of(textures.begin(),textures.end(),[](const Texture &t){return t.id==texture_limit;})?"true":"false",
        !buffers.empty()&&std::any_of(buffers.begin(),buffers.end(),[](const Buffer &b){return b.id==buffer_limit;})?"true":"false",renderbuffer_dsa?"true":"false");
    std::fprintf(f,"  \"live_textures\":%u,\"live_buffers\":%u,\"live_renderbuffers\":%u,\"unknown_textures\":%u,\n  \"texture_logical_bytes\":%llu,\"texture_storage_estimate_bytes\":%llu,\"buffer_bytes\":%llu,\"renderbuffer_bytes\":%llu,\n  \"total_named_storage_estimate_bytes\":%llu,\n",
        r.textures,r.buffers,r.renderbuffers,r.unknown_textures,
        static_cast<unsigned long long>(r.texture_logical_bytes),static_cast<unsigned long long>(r.texture_storage_estimate_bytes),
        static_cast<unsigned long long>(r.buffer_bytes),static_cast<unsigned long long>(r.renderbuffer_bytes),
        static_cast<unsigned long long>(r.texture_storage_estimate_bytes+r.buffer_bytes+r.renderbuffer_bytes));
    std::fputs("  \"notes\":\"Fixed ID scans inventory live named objects, not driver-private shader scratch, retired/upload BO caches, default framebuffer images or allocation padding. Logical channel bytes and conservative packed depth/RGB storage estimates are distinct. Texture views can alias storage; this engine creates none. Cube DSA level query is positiveX only, counted six times.\",\n  \"top_textures\":[",f);
    for(std::size_t index=0;index<std::min<std::size_t>(64,textures.size());index++){
        const auto &t=textures[index];std::fprintf(f,"%s\n    {\"id\":%u,\"target\":",index?",":"",t.id);string(f,target_name(t.target));
        std::fprintf(f,",\"target_enum\":%d,\"base_level\":%d,\"max_level\":%d,\"view_layers\":%d,\"immutable_levels\":%d,\"logical_bytes\":%llu,\"storage_estimate_bytes\":%llu,\"levels\":[",
            t.target,t.base,t.max,t.view_layers,t.immutable_levels,static_cast<unsigned long long>(t.logical),static_cast<unsigned long long>(t.storage));
        for(std::size_t k=0;k<t.levels.size();k++){
            const auto &l=t.levels[k];std::fprintf(f,"%s{\"level\":%d,\"width\":%d,\"height\":%d,\"depth_or_layers\":%d,\"format\":%d,\"samples\":%d,\"component_bits\":%d,\"compressed\":%s,\"logical_bytes\":%llu,\"storage_estimate_bytes\":%llu}",
                k?",":"",l.level,l.w,l.h,l.d,l.format,l.samples,l.bits,l.compressed?"true":"false",static_cast<unsigned long long>(l.logical),static_cast<unsigned long long>(l.storage));
        }std::fputs("]}",f);
    }
    std::fputs("\n  ],\n  \"top_buffers\":[",f);
    for(std::size_t index=0;index<std::min<std::size_t>(64,buffers.size());index++)std::fprintf(f,"%s{\"id\":%u,\"bytes\":%llu}",index?",":"",buffers[index].id,static_cast<unsigned long long>(buffers[index].bytes));
    std::fputs("],\n  \"renderbuffers\":[",f);
    for(std::size_t index=0;index<renderbuffers.size();index++){
        const auto &b=renderbuffers[index];std::fprintf(f,"%s{\"id\":%u,\"width\":%d,\"height\":%d,\"format\":%d,\"samples\":%d,\"storage_estimate_bytes\":%llu}",index?",":"",b.id,b.w,b.h,b.format,b.samples,static_cast<unsigned long long>(b.bytes));
    }std::fputs("]\n}\n",f);
}
}
inline WorldGpuBudgetAuditResult world_gpu_budget_audit(const char *path){
    using namespace world_gpu_budget_detail;WorldGpuBudgetAuditResult result;
    const char *flag=std::getenv("VALWOOD_QA_GPU_BUDGET");if(!flag||std::strcmp(flag,"1"))return result;
    result.requested=true;FILE *file=path&&*path?std::fopen(path,"w"):nullptr;if(!file)return result;
    std::vector<Texture> textures;std::vector<Buffer> buffers;std::vector<Renderbuffer> renderbuffers;
    const char *status="unavailable_dsa";bool renderbuffer_dsa=false;
    // OpenGL cannot reinsert an error once read. Return consumed errors so the
    // caller folds them into its shutdown failure check; never silently hide one.
    result.preexisting_error=glGetError();
    if(result.preexisting_error!=GL_NO_ERROR)status="preexisting_gl_error";
    else {
        GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
        const bool core45=major>4||(major==4&&minor>=5);
        const bool dsa=core45||SDL_GL_ExtensionSupported("GL_ARB_direct_state_access");
        const bool views=major>4||(major==4&&minor>=3)||SDL_GL_ExtensionSupported("GL_ARB_texture_view");
        const auto parameter=reinterpret_cast<GetParameter>(SDL_GL_GetProcAddress("glGetTextureParameteriv"));
        const auto level=reinterpret_cast<GetLevel>(SDL_GL_GetProcAddress("glGetTextureLevelParameteriv"));
        const auto buffer=reinterpret_cast<GetBuffer>(SDL_GL_GetProcAddress("glGetNamedBufferParameteri64v"));
        const auto renderbuffer=reinterpret_cast<GetParameter>(SDL_GL_GetProcAddress("glGetNamedRenderbufferParameteriv"));
        renderbuffer_dsa=renderbuffer!=nullptr;
        if(checked(result)&&dsa&&parameter&&level&&buffer){
            result.available=true;status="complete";
            try {
                textures.reserve(512);buffers.reserve(4096);renderbuffers.reserve(16);
                for(GLuint id=1;id<=texture_limit;id++)if(glIsTexture(id)){
                    result.textures++;Texture t;t.id=id;parameter(id,GL_TEXTURE_TARGET,&t.target);
                    if(!checked(result)||!supported_target(t.target)){result.unknown_textures++;continue;}
                    parameter(id,GL_TEXTURE_BASE_LEVEL,&t.base);parameter(id,GL_TEXTURE_MAX_LEVEL,&t.max);
                    if(views){parameter(id,GL_TEXTURE_VIEW_NUM_LAYERS,&t.view_layers);parameter(id,GL_TEXTURE_IMMUTABLE_LEVELS,&t.immutable_levels);}
                    GLint w=0,h=0,d=0;level(id,0,GL_TEXTURE_WIDTH,&w);level(id,0,GL_TEXTURE_HEIGHT,&h);level(id,0,GL_TEXTURE_DEPTH,&d);
                    if(!checked(result)){result.unknown_textures++;continue;}
                    if(w<=0){textures.push_back(std::move(t));continue;}
                    const bool multisample=t.target==GL_TEXTURE_2D_MULTISAMPLE||t.target==GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
                    int spatial=w;if(t.target!=GL_TEXTURE_1D&&t.target!=GL_TEXTURE_1D_ARRAY)spatial=std::max(spatial,h);
                    if(t.target==GL_TEXTURE_3D)spatial=std::max(spatial,d);
                    int last=0;if(!multisample&&t.target!=GL_TEXTURE_RECTANGLE)for(int n=spatial;n>1;n/=2)last++;
                    for(int mip=0;mip<=last;mip++){
                        Level l;l.level=mip;GLint compressed=0;
                        level(id,mip,GL_TEXTURE_WIDTH,&l.w);level(id,mip,GL_TEXTURE_HEIGHT,&l.h);level(id,mip,GL_TEXTURE_DEPTH,&l.d);
                        level(id,mip,GL_TEXTURE_INTERNAL_FORMAT,&l.format);level(id,mip,GL_TEXTURE_COMPRESSED,&compressed);
                        if(!checked(result)){result.unknown_textures++;break;}
                        if(l.w<=0)continue;
                        l.h=std::max(l.h,1);l.d=std::max(l.d,1);l.compressed=compressed!=0;
                        const unsigned faces=t.target==GL_TEXTURE_CUBE_MAP?6u:1u;
                        if(multisample){level(id,mip,GL_TEXTURE_SAMPLES,&l.samples);l.samples=std::max(l.samples,1);}
                        if(l.compressed){GLint bytes=0;level(id,mip,GL_TEXTURE_COMPRESSED_IMAGE_SIZE,&bytes);if(bytes>0)l.logical=l.storage=std::uint64_t(bytes)*faces;}
                        else {
                            const GLenum sizes[]={GL_TEXTURE_RED_SIZE,GL_TEXTURE_GREEN_SIZE,GL_TEXTURE_BLUE_SIZE,GL_TEXTURE_ALPHA_SIZE,GL_TEXTURE_DEPTH_SIZE,GL_TEXTURE_STENCIL_SIZE};
                            for(GLenum size:sizes){GLint bits=0;level(id,mip,size,&bits);l.bits+=std::max(bits,0);}
                            if(l.format==GL_RGB9_E5){GLint shared=0;level(id,mip,GL_TEXTURE_SHARED_SIZE,&shared);l.bits+=std::max(shared,0);}
                            const std::uint64_t texels=std::uint64_t(l.w)*l.h*l.d*faces*unsigned(l.samples);
                            l.logical=texels*unsigned((l.bits+7)/8);l.storage=texels*storage_texel_bytes(l.format,unsigned(l.bits));
                        }
                        if(!checked(result)){result.unknown_textures++;break;}
                        if(!l.logical)result.unknown_textures++;
                        t.logical+=l.logical;t.storage+=l.storage;t.levels.push_back(l);
                    }
                    result.texture_logical_bytes+=t.logical;result.texture_storage_estimate_bytes+=t.storage;textures.push_back(std::move(t));
                }
                for(GLuint id=1;id<=buffer_limit;id++)if(glIsBuffer(id)){
                    result.buffers++;GLint64 bytes=0;buffer(id,GL_BUFFER_SIZE,&bytes);
                    if(checked(result)&&bytes>=0){buffers.push_back({id,std::uint64_t(bytes)});result.buffer_bytes+=std::uint64_t(bytes);}
                }
                for(GLuint id=1;id<=renderbuffer_limit;id++)if(glIsRenderbuffer(id)){
                    result.renderbuffers++;if(!renderbuffer)continue;Renderbuffer b;b.id=id;GLint bits=0;
                    renderbuffer(id,GL_RENDERBUFFER_WIDTH,&b.w);renderbuffer(id,GL_RENDERBUFFER_HEIGHT,&b.h);
                    renderbuffer(id,GL_RENDERBUFFER_INTERNAL_FORMAT,&b.format);renderbuffer(id,GL_RENDERBUFFER_SAMPLES,&b.samples);b.samples=std::max(b.samples,1);
                    const GLenum sizes[]={GL_RENDERBUFFER_RED_SIZE,GL_RENDERBUFFER_GREEN_SIZE,GL_RENDERBUFFER_BLUE_SIZE,GL_RENDERBUFFER_ALPHA_SIZE,GL_RENDERBUFFER_DEPTH_SIZE,GL_RENDERBUFFER_STENCIL_SIZE};
                    for(GLenum size:sizes){GLint component=0;renderbuffer(id,size,&component);bits+=std::max(component,0);}
                    if(checked(result)){b.bytes=std::uint64_t(std::max(b.w,0))*std::max(b.h,0)*unsigned(b.samples)*storage_texel_bytes(b.format,unsigned(bits));result.renderbuffer_bytes+=b.bytes;renderbuffers.push_back(b);}
                }
                (void)checked(result);
                std::sort(textures.begin(),textures.end(),[](const Texture &a,const Texture &b){return a.storage>b.storage||(a.storage==b.storage&&a.id<b.id);});
                std::sort(buffers.begin(),buffers.end(),[](const Buffer &a,const Buffer &b){return a.bytes>b.bytes||(a.bytes==b.bytes&&a.id<b.id);});
                result.complete=!result.query_errors&&!result.unknown_textures&&(!result.renderbuffers||renderbuffer_dsa);
                if(!result.complete)status="partial_query_failure";
            }catch(...){status="allocation_failure";result.complete=false;}
        }
    }
    report(file,result,status,textures,buffers,renderbuffers,renderbuffer_dsa);
    result.saved=std::fflush(file)==0&&!std::ferror(file);if(std::fclose(file)!=0)result.saved=false;
    return result;
}
#endif

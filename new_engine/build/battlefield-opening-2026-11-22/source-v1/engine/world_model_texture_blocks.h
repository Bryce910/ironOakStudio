#ifndef VALWOOD_WORLD_MODEL_TEXTURE_BLOCKS_H
#define VALWOOD_WORLD_MODEL_TEXTURE_BLOCKS_H

// Loading-only upload of prevalidated offline blocks. No encoder is linked
// into the game, and no disk access/readback occurs during ordinary drawing.
#include "engine/world_models.h"
#include "engine/world_texture_blocks_format.h"
#include "stb_image.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

namespace world_model_texture_blocks {
constexpr unsigned policy_bit=1u<<16;
// Role-specific cache keys prevent a generic linear certificate from ever
// satisfying a normal-map request for the same encoded image.
inline unsigned policy_flags(unsigned format){return policy_bit|(format==world_texture_blocks::format_srgb_bc7?0:format<<17);}
inline unsigned policy_format(unsigned flags){const auto format=(flags>>17)&3u;return format?format:world_texture_blocks::format_srgb_bc7;}
inline WorldModelTextureBlockStats &counters(){static WorldModelTextureBlockStats value={};return value;}
inline std::string &directory(){static thread_local std::string value;return value;}
struct DirectoryScope {
    std::string previous;
    explicit DirectoryScope(const char *model):previous(std::move(directory())){
        auto parent=std::filesystem::path(model).parent_path();
        if(parent.filename()=="models")parent=parent.parent_path();
        directory()=(parent/"texture_blocks").string();
    }
    ~DirectoryScope(){directory()=std::move(previous);}
};
inline bool available(bool srgb,bool opaque_material,unsigned data_format=0){
    if(!(srgb&&opaque_material)&&!(!srgb&&world_texture_blocks::linear_format(data_format)))return false;
    const char *disabled=SDL_getenv("VALWOOD_QA_DISABLE_OPAQUE_BC7");
    if(disabled&&!std::strcmp(disabled,"1"))return false;
    const bool supported=SDL_GL_ExtensionSupported("GL_ARB_texture_compression_bptc");
    counters().enabled=supported;return supported;
}
struct UnpackScope {
    GLint values[7]={};
    const GLenum names[7]={GL_PIXEL_UNPACK_BUFFER_BINDING,GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,
        GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_IMAGES};
    UnpackScope(){
        for(unsigned i=0;i<7;i++)glGetIntegerv(names[i],&values[i]);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        for(unsigned i=2;i<7;i++)glPixelStorei(names[i],0);
    }
    ~UnpackScope(){
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(values[0]));
        for(unsigned i=1;i<7;i++)glPixelStorei(names[i],values[i]);
    }
};
inline GLuint upload(const void *encoded,std::size_t length,unsigned expected_format=world_texture_blocks::format_srgb_bc7,unsigned max_edge=0){
    auto &stats=counters();stats.candidates++;
    if(!world_texture_blocks::supported_format(expected_format)){stats.rejected_cache++;return 0;}
    char leaf[40];std::snprintf(leaf,sizeof(leaf),world_texture_blocks::linear_format(expected_format)?"%016llx.linear.vbc7":"%016llx.vbc7",
        static_cast<unsigned long long>(world_texture_blocks::fingerprint(encoded,length)));
    try {
        const auto path=std::filesystem::path(directory())/leaf;
        std::error_code ec;
        if(!std::filesystem::is_regular_file(path,ec)){stats.cache_misses++;return 0;}
        world_texture_blocks::Image image;std::string error;
        // read validates the full encoded source, every length/quality field,
        // and a checksum covering all metadata/source/block bytes before GL.
        if(!world_texture_blocks::read(path.u8string().c_str(),encoded,length,image,error,expected_format)){
            stats.rejected_cache++;return 0;
        }
        int width=0,height=0,channels=0;
        if(!stbi_info_from_memory(static_cast<const stbi_uc*>(encoded),int(length),&width,&height,&channels)
           ||width<=0||height<=0||image.width!=unsigned(width)||image.height!=unsigned(height)
           ||image.levels.front().rmse>world_texture_blocks::base_rmse_max){
            // Source identity alone does not prove full resolution: reject a
            // valid-checksum cache with resized geometry or weaker admission.
            stats.rejected_cache++;return 0;
        }
        GLuint texture=0;glGenTextures(1,&texture);if(!texture){stats.upload_failures++;return 0;}
        const GLenum gpu_format=expected_format==world_texture_blocks::format_srgb_bc7?GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:GL_COMPRESSED_RGBA_BPTC_UNORM;
        bool success=true;std::uint64_t stored=0,reference=0;
        {
            UnpackScope unpack;glBindTexture(GL_TEXTURE_2D,texture);
            std::size_t first=0;while(max_edge&&first+1<image.levels.size()&&(image.levels[first].width>max_edge||image.levels[first].height>max_edge))++first;
            for(std::size_t i=first;i<image.levels.size();i++){
                const auto &level=image.levels[i];
                glCompressedTexImage2D(GL_TEXTURE_2D,GLint(i-first),gpu_format,
                    GLsizei(level.width),GLsizei(level.height),0,GLsizei(level.blocks.size()),level.blocks.data());
                // Failed uploads must not issue compressed-only queries: those
                // can queue a second error and poison the original RGBA fallback.
                if(glGetError()!=GL_NO_ERROR){success=false;break;}
                GLint compressed=0,bytes=0,format=0;
                glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(i-first),GL_TEXTURE_COMPRESSED,&compressed);
                if(glGetError()!=GL_NO_ERROR||!compressed){success=false;break;}
                glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(i-first),GL_TEXTURE_COMPRESSED_IMAGE_SIZE,&bytes);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(i-first),GL_TEXTURE_INTERNAL_FORMAT,&format);
                if(glGetError()!=GL_NO_ERROR||bytes<0||std::size_t(bytes)!=level.blocks.size()
                    ||GLenum(format)!=gpu_format){success=false;break;}
                stored+=level.blocks.size();reference+=std::uint64_t(level.width)*level.height*4;
            }
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,GLint(image.levels.size()-first-1));
        }
        if(!success){glDeleteTextures(1,&texture);stats.upload_failures++;return 0;}
        stats.cached_textures++;stats.reference_rgba_bytes+=reference;stats.block_bytes+=stored;
        return texture;
    }catch(...){stats.rejected_cache++;return 0;}
}
}
#endif

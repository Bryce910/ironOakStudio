#ifndef VALWOOD_WORLD_VILLAGE_TEXTURES_H
#define VALWOOD_WORLD_VILLAGE_TEXTURES_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "stb_image.h"
#include "world_village_ground.h"
#include "world_village_indirect.h"
#include <array>
#include <cstdio>
#include <cstring>

// Startup-only uploads must not inherit a caller's unpack PBO, row pitch,
// skips or alignment. Restore bindings and pixel-store state on every exit.
struct WorldVillageUnpackScope {
    GLint buffer=0,alignment=4,row=0,image=0,pixels=0,rows=0,images=0;
    GLint two=0,array=0,volume=0;
    WorldVillageUnpackScope() {
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&buffer);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&image);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&pixels);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&rows);
        glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&images);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&two);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&array);
        glGetIntegerv(GL_TEXTURE_BINDING_3D,&volume);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
    }
    ~WorldVillageUnpackScope() {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(buffer));glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,row);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,image);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,pixels);glPixelStorei(GL_UNPACK_SKIP_ROWS,rows);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES,images);
        glBindTexture(GL_TEXTURE_2D,GLuint(two));glBindTexture(GL_TEXTURE_2D_ARRAY,GLuint(array));
        glBindTexture(GL_TEXTURE_3D,GLuint(volume));
    }
};

struct WorldVillageTextures {
    GLuint litter=0,history=0,directional=0,up=0;
    bool forest_litter=false;
    std::array<float,3> heights={24,1.f/3,9};
    std::size_t directional_bytes=0,history_bytes=0;
    int directional_side=0,directional_slices=0;
    static void parameters(GLenum target,bool mips,bool repeat=false) {
        glTexParameteri(target,GL_TEXTURE_MIN_FILTER,mips?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
        glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(target,GL_TEXTURE_WRAP_S,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
        glTexParameteri(target,GL_TEXTURE_WRAP_T,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
        if(target==GL_TEXTURE_3D)glTexParameteri(target,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
        if(mips&&(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic"))) {
            GLfloat maximum=1;glGetFloatv(0x84FF,&maximum);glTexParameterf(target,0x84FE,std::min(maximum,4.f));
        }
    }
    // Same-sized, unchanged linear normal/ARM maps share one sampler. This
    // keeps the terrain within GL3.3's16 fragment-image-unit guarantee.
    bool load_litter(const char *normal,const char *arm,const char *forest_color=nullptr,
                     const char *forest_normal=nullptr,const char *forest_material=nullptr) {
        if(!normal||!arm)return false;
        stbi_set_flip_vertically_on_load(0);
        int w=0,h=0,c=0,aw=0,ah=0;auto *n=stbi_load(normal,&w,&h,&c,4);
        auto *a=stbi_load(arm,&aw,&ah,&c,4);
        if(!n||!a||w!=aw||h!=ah||w<1||h<1||w>4096||h>4096) {
            stbi_image_free(n);stbi_image_free(a);return false;
        }
        const bool extra=forest_color||forest_normal||forest_material;
        std::array<unsigned char*,3> forest={};
        if(extra) {
            const char *paths[]={forest_color,forest_normal,forest_material};
            bool valid=true;
            for(unsigned i=0;i<forest.size();++i){int fw=0,fh=0;if(paths[i])forest[i]=stbi_load(paths[i],&fw,&fh,&c,4);valid&=forest[i]&&fw==w&&fh==h;}
            if(!valid){for(auto *p:forest)stbi_image_free(p);stbi_image_free(n);stbi_image_free(a);return false;}
        }
        GLuint next=0;
        {
            WorldVillageUnpackScope scope;
            glGenTextures(1,&next);glBindTexture(GL_TEXTURE_2D_ARRAY,next);
            glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,w,h,extra?5:2,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,0,w,h,1,GL_RGBA,GL_UNSIGNED_BYTE,n);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,1,w,h,1,GL_RGBA,GL_UNSIGNED_BYTE,a);
            for(unsigned i=0;i<forest.size();++i)if(forest[i])glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,GLint(i+2),w,h,1,GL_RGBA,GL_UNSIGNED_BYTE,forest[i]);
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);parameters(GL_TEXTURE_2D_ARRAY,true,true);
        }
        stbi_image_free(n);stbi_image_free(a);
        for(auto *p:forest)stbi_image_free(p);
        if(!next||glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&next);return false;}
        glDeleteTextures(1,&litter);litter=next;forest_litter=extra;return true;
    }
    bool load_history(const char *file=world_village_ground_file) {
        if(!file)return false;
        stbi_set_flip_vertically_on_load(0);
        int w=0,h=0,c=0;auto *data=stbi_load(file,&w,&h,&c,4);
        if(!data||w!=world_village_ground_side||h!=world_village_ground_side){stbi_image_free(data);return false;}
        GLuint next=0;
        {
            WorldVillageUnpackScope scope;
            glGenTextures(1,&next);glBindTexture(GL_TEXTURE_2D,next);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
            glGenerateMipmap(GL_TEXTURE_2D);parameters(GL_TEXTURE_2D,true);
        }
        stbi_image_free(data);
        if(!next||glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&next);return false;}
        glDeleteTextures(1,&history);history=next;history_bytes=0;
        for(;;){history_bytes+=std::size_t(w)*h*4;if(w==1&&h==1)break;w=std::max(w/2,1);h=std::max(h/2,1);}
        return true;
    }
    bool build_directional(const std::vector<float> &roofs,const WorldVillageSkyConfig &source) {
        if(roofs.empty())return false;
        for(float value:roofs)if(!std::isfinite(value))return false;
        WorldVillageIndirectConfig config;
        GLint limit=0;glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE,&limit);
        if(limit<129)return false;
        if(limit<257)config.columns=config.rows=129;
        const float tallest=*std::max_element(roofs.begin(),roofs.end());
        const double needed=std::ceil(std::max(double(tallest)-config.first_height,0.)/config.height_step)+1.;
        config.slices=std::size_t(std::clamp(needed,9.,33.));
        if(config.first_height+config.height_step*float(config.slices-1)<tallest)
            config.height_step=std::ceil((tallest-config.first_height)/float(config.slices-1));
        WorldVillageIndirectField field;
        if(!world_village_indirect_build(roofs.data(),roofs.size(),source,config,field))return false;
        GLuint next[2]={};
        {
            WorldVillageUnpackScope scope;glGenTextures(2,next);
            glBindTexture(GL_TEXTURE_3D,next[0]);
            glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA8,GLsizei(config.columns),GLsizei(config.rows),GLsizei(config.slices),0,GL_RGBA,GL_UNSIGNED_BYTE,field.horizontal_rgba.data());
            parameters(GL_TEXTURE_3D,false);
            glBindTexture(GL_TEXTURE_3D,next[1]);
            glTexImage3D(GL_TEXTURE_3D,0,GL_R8,GLsizei(config.columns),GLsizei(config.rows),GLsizei(config.slices),0,GL_RED,GL_UNSIGNED_BYTE,field.up_r.data());
            parameters(GL_TEXTURE_3D,false);
        }
        if(!next[0]||!next[1]||glGetError()!=GL_NO_ERROR){glDeleteTextures(2,next);return false;}
        GLuint old[]={directional,up};glDeleteTextures(2,old);directional=next[0];up=next[1];
        heights={config.first_height,1.f/config.height_step,float(config.slices)};
        directional_side=int(config.columns);directional_slices=int(config.slices);
        directional_bytes=field.horizontal_rgba.size()+field.up_r.size();return true;
    }
    void release() {
        GLuint all[]={litter,history,directional,up};glDeleteTextures(4,all);
        litter=history=directional=up=0;directional_bytes=history_bytes=0;forest_litter=false;
    }
};
#endif

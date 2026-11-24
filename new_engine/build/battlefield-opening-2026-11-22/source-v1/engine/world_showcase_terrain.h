#ifndef VALWOOD_WORLD_SHOWCASE_TERRAIN_H
#define VALWOOD_WORLD_SHOWCASE_TERRAIN_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "stb_image.h"
#include "engine/world_program_epoch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

/* Optional private-project terrain. Three arrays reuse units0/1/2 after the
 * showcase shader removes legacy terrain material sampling. Bind LAST before
 * terrain.draw in main/reflection/warm/probe views. No scene-global shaders or
 * imported architecture materials are changed. All creation is loading-only.
 * Manifest (paths relative to manifest directory, quoted as needed):
 * VWSPLAT1
 * bounds xmin zmin width depth
 * weights "alpha0.png" "alpha1.png" flip_rows_0_or_1
 * layer "base.png" "normal.png" "arm.png" scaleX scaleZ offsetX offsetZ normalScale
 * ... exactly8 layers in source alpha-channel order.
 * UV=(absoluteWorldXZ-bounds.xy)*scale+offset. Tile images have standard PNG
 * top rows and flip once during upload. Alpha files may choose their row order.
 */
struct WorldShowcaseTerrain {
    std::array<GLuint,3> textures={};
    GLuint holes=0;
    std::array<float,4> bounds={};
    std::array<std::array<float,4>,8> uv={};
    std::array<float,8> normal_scale={};
    std::uint64_t bytes=0;
    int side=0,alpha_side=0,hole_side=0;
    struct Binding {
        GLuint program=0;unsigned long long epoch=0;
        GLint ready=-1,albedo=-1,surface=-1,weights=-1,bounds=-1,uv=-1,normal=-1,snow=-1,holes=-1,holes_ready=-1;
    };
    std::array<Binding,8> bindings={};unsigned next_binding=0;
    WorldShowcaseTerrain()=default;
    WorldShowcaseTerrain(const WorldShowcaseTerrain&)=delete;
    WorldShowcaseTerrain& operator=(const WorldShowcaseTerrain&)=delete;
    void release(){
        glDeleteTextures(GLsizei(textures.size()),textures.data());textures={};glDeleteTextures(1,&holes);holes=0;bytes=0;side=alpha_side=hole_side=0;bindings={};next_binding=0;
    }
    bool ready() const{return textures[0]!=0;}
    bool load(const char *manifest,std::string &error){
        if(!manifest||!*manifest){error="Missing showcase terrain manifest path";return false;}
        std::ifstream input(manifest);if(!input){error="Cannot open showcase terrain manifest";return false;}
        std::string token;WorldShowcaseTerrain next;
        double xmin=0,zmin=0,width=0,depth=0;
        std::array<std::string,2> alpha;std::array<std::array<std::string,3>,8> files;
        int flip_alpha=0;
        if(!(input>>token)||token!="VWSPLAT1"||!(input>>token>>xmin>>zmin>>width>>depth)||token!="bounds"||
            !std::isfinite(xmin)||!std::isfinite(zmin)||!std::isfinite(width)||!std::isfinite(depth)||width<=0||depth<=0||
            std::abs(xmin)>1.e7||std::abs(zmin)>1.e7||width>1.e7||depth>1.e7){
            error="Invalid showcase terrain bounds/header";return false;
        }
        if(!(input>>token>>std::quoted(alpha[0])>>std::quoted(alpha[1])>>flip_alpha)||token!="weights"||flip_alpha<0||flip_alpha>1){
            error="Invalid showcase terrain weights";return false;
        }
        next.bounds={float(xmin),float(zmin),float(1/width),float(1/depth)};
        for(unsigned i=0;i<8;i++){
            auto &m=next.uv[i];
            if(!(input>>token>>std::quoted(files[i][0])>>std::quoted(files[i][1])>>std::quoted(files[i][2])>>m[0]>>m[1]>>m[2]>>m[3]>>next.normal_scale[i])||token!="layer"){
                error="Invalid showcase terrain layer record";return false;
            }
            for(float v:m)if(!std::isfinite(v)){error="Non-finite showcase terrain UV";return false;}
            if(m[0]<=0||m[1]>=0||!std::isfinite(next.normal_scale[i])||next.normal_scale[i]<0||next.normal_scale[i]>8){
                error="Invalid showcase terrain source UV/normal scale";return false;
            }
        }
        if(input>>token){error="Unexpected showcase terrain manifest data";return false;}
        const auto root=std::filesystem::path(manifest).parent_path();
        GLint previous_active=0,previous_unpack=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&previous_active);glGetIntegerv(GL_UNPACK_ALIGNMENT,&previous_unpack);
        glActiveTexture(GL_TEXTURE0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glGenTextures(3,next.textures.data());
        auto restore=[&](){glPixelStorei(GL_UNPACK_ALIGNMENT,previous_unpack);glActiveTexture(GLenum(previous_active));};
        auto image=[&](const std::string &file,bool flip,int expected,std::vector<unsigned char> &pixels,int &size)->bool{
            int w=0,h=0,c=0;auto *raw=stbi_load((root/file).string().c_str(),&w,&h,&c,4);
            if(!raw||w!=h||w<4||w>2048||(w&(w-1))||(expected&&w!=expected)){
                stbi_image_free(raw);error="Invalid showcase terrain texture: "+file;return false;
            }
            size=w;pixels.resize(std::size_t(w)*h*4);
            for(int y=0;y<h;y++)std::copy_n(raw+std::size_t(flip?h-1-y:y)*w*4,std::size_t(w)*4,pixels.data()+std::size_t(y)*w*4);
            stbi_image_free(raw);return true;
        };
        auto storage=[&](GLuint texture,int image_side,int layers,GLint format,bool repeat){
            glBindTexture(GL_TEXTURE_2D_ARRAY,texture);glTexImage3D(GL_TEXTURE_2D_ARRAY,0,format,image_side,image_side,layers,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
            if(repeat&&(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic"))){
                GLfloat maximum=1;glGetFloatv(0x84FF,&maximum);glTexParameterf(GL_TEXTURE_2D_ARRAY,0x84FE,std::min(maximum,8.f));
            }
            for(int n=image_side;n;n/=2)next.bytes+=std::uint64_t(n)*n*layers*4;
        };
        std::vector<unsigned char> pixels;bool ok=true;
        stbi_set_flip_vertically_on_load(0);
        for(unsigned i=0;i<8&&ok;i++)for(unsigned kind=0;kind<3&&ok;kind++){
            int image_side=0;ok=image(files[i][kind],true,next.side,pixels,image_side);if(!ok)break;
            if(!next.side){next.side=image_side;storage(next.textures[0],next.side,8,GL_SRGB8_ALPHA8,true);storage(next.textures[1],next.side,16,GL_RGBA8,true);}
            glBindTexture(GL_TEXTURE_2D_ARRAY,next.textures[kind?1:0]);
            const int layer=kind?int(i*2+kind-1):int(i);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,layer,next.side,next.side,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        }
        for(unsigned i=0;i<2&&ok;i++){
            int image_side=0;ok=image(alpha[i],flip_alpha!=0,next.alpha_side,pixels,image_side);if(!ok)break;
            if(!next.alpha_side){next.alpha_side=image_side;storage(next.textures[2],next.alpha_side,2,GL_RGBA8,false);}
            glBindTexture(GL_TEXTURE_2D_ARRAY,next.textures[2]);glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,int(i),next.alpha_side,next.alpha_side,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        }
        if(ok)for(GLuint texture:next.textures){glBindTexture(GL_TEXTURE_2D_ARRAY,texture);glGenerateMipmap(GL_TEXTURE_2D_ARRAY);}
        const auto hole_file=root/"holes.png";
        if(ok&&std::filesystem::exists(hole_file)){
            int image_side=0;ok=image("holes.png",true,0,pixels,image_side);
            if(ok){
                next.hole_side=image_side;std::vector<unsigned char> mask(std::size_t(image_side)*image_side);
                for(std::size_t i=0;i<mask.size();i++)mask[i]=pixels[i*4];
                glGenTextures(1,&next.holes);glBindTexture(GL_TEXTURE_2D,next.holes);
                glTexImage2D(GL_TEXTURE_2D,0,GL_R8,image_side,image_side,0,GL_RED,GL_UNSIGNED_BYTE,mask.data());
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                next.bytes+=mask.size();
            }
        }
        if(glGetError()!=GL_NO_ERROR){ok=false;error="OpenGL showcase terrain array upload failed";}
        restore();
        if(!ok){next.release();return false;}
        release();textures=next.textures;next.textures={};holes=next.holes;next.holes=0;bounds=next.bounds;uv=next.uv;normal_scale=next.normal_scale;side=next.side;alpha_side=next.alpha_side;hole_side=next.hole_side;bytes=next.bytes;
        return true;
    }
    void bind(GLuint program){
        if(!program)return;
        Binding *binding=nullptr;
        for(auto &entry:bindings)if(entry.program==program&&entry.epoch==world_program_epoch){binding=&entry;break;}
        if(!binding){
            binding=&bindings[next_binding++%bindings.size()];*binding={};binding->program=program;binding->epoch=world_program_epoch;
            binding->ready=glGetUniformLocation(program,"uShowcaseReady");binding->albedo=glGetUniformLocation(program,"uShowcaseAlbedo");
            binding->surface=glGetUniformLocation(program,"uShowcaseSurface");binding->weights=glGetUniformLocation(program,"uShowcaseWeights");
            binding->bounds=glGetUniformLocation(program,"uShowcaseBounds");binding->uv=glGetUniformLocation(program,"uShowcaseUV[0]");
            binding->normal=glGetUniformLocation(program,"uShowcaseNormalScale[0]");binding->snow=glGetUniformLocation(program,"uSnowImprints");
            binding->holes=glGetUniformLocation(program,"uShowcaseHoles");binding->holes_ready=glGetUniformLocation(program,"uShowcaseHolesReady");
        }
        if(binding->ready<0)return;
        glUniform1i(binding->ready,ready());glUniform1i(binding->albedo,0);glUniform1i(binding->surface,1);glUniform1i(binding->weights,2);
        glUniform1i(binding->holes,3);glUniform1i(binding->holes_ready,holes!=0);
        glUniform1i(binding->snow,16); // Preserve existing snow field's unit; avoid array/2D sampler aliasing during warm views.
        if(ready()){
            glUniform4fv(binding->bounds,1,bounds.data());glUniform4fv(binding->uv,8,uv[0].data());glUniform1fv(binding->normal,8,normal_scale.data());
            for(unsigned i=0;i<3;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D_ARRAY,textures[i]);}
            if(holes){glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,holes);}
        }
        glActiveTexture(GL_TEXTURE0);
    }
};

#endif

#ifndef VALWOOD_WORLD_CITY_SKY_H
#define VALWOOD_WORLD_CITY_SKY_H
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL2/SDL_opengl.h>
#include <cstdint>
#include "stb_image.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
namespace world_city_sky {
constexpr double pi=3.14159265358979323846;
using V=std::array<double,3>;using RGB=std::array<float,3>;
struct Config{std::string panorama;unsigned side=256;double brightness=1.5,contrast=2.5,turns=-.1687459945678711;};
inline int read(const char *path,Config &out,std::string &error){
    std::ifstream file(path);if(!file){if(!std::filesystem::exists(path))return 0;error="Cannot open city sky manifest";return -1;}
    std::string line;if(!std::getline(file,line)||line!="VALWOOD_CITY_SKY 1"){error="Unsupported city sky manifest";return -1;}
    Config next;unsigned seen=0,rows=0;
    while(std::getline(file,line)){
        if(++rows>16||line.size()>1024){error="Oversized city sky manifest";return -1;}
        std::istringstream in(line);std::string kind,extra;in>>kind;if(kind.empty()||kind[0]=='#')continue;unsigned bit=0;bool okay=false;
        if(kind=="panorama"){bit=1;okay=bool(in>>std::quoted(next.panorama))&&next.panorama.size()<=512&&!next.panorama.empty()&&next.panorama.find('\0')==std::string::npos;}
        else if(kind=="material"){bit=2;okay=bool(in>>next.brightness>>next.contrast>>next.turns)&&std::isfinite(next.brightness)&&std::isfinite(next.contrast)&&std::isfinite(next.turns)&&next.brightness>=0&&next.brightness<=8&&next.contrast>=.1&&next.contrast<=8&&std::abs(next.turns)<=8;}
        else if(kind=="cube"){bit=4;okay=bool(in>>next.side)&&next.side>=32&&next.side<=512&&!(next.side&(next.side-1));}
        if(!okay||(seen&bit)||(in>>extra)){error="Invalid/duplicate city sky record: "+kind;return -1;}seen|=bit;
    }
    if(!file.eof()||seen!=7){error="Incomplete city sky manifest";return -1;}out=next;error.clear();return 1;
}
inline V face(unsigned f,double u,double v){
    const V all[]={{1,-v,-u},{-1,-v,u},{u,1,v},{u,-1,-v},{u,-v,1},{-u,-v,-1}};auto d=all[f];double n=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);for(auto &a:d)a/=n;return d;
}
// UE4 TextureCompressorModule FImageViewLongLat + TransformSideToWorldSpace:
// after native->UE(X,-Z,Y) and the authored +UEZ Rodrigues rotation,
// u=.5+atan2(UE_X,-UE_Y)/(2pi), v=acos(UE_Z)/pi. TextureSource rows stay top-down.
inline std::array<double,2> uv(V d,double turns){
    const double a=turns*2*pi,c=std::cos(a),s=std::sin(a);
    const double x=c*d[0]+s*d[2],y=s*d[0]-c*d[2];
    return {.5+std::atan2(x,-y)/(2*pi),std::acos(std::clamp(d[1],-1.,1.))/pi};
}
inline RGB lookup(const float *pixels,int w,int h,V d,const Config &config){
    auto coord=uv(d,config.turns);double x=coord[0]*w,y=coord[1]*h;int ix=int(std::floor(x)),iy=int(std::floor(y));double fx=x-ix,fy=y-iy;RGB rgb={};
    for(int j=0;j<2;j++)for(int i=0;i<2;i++){
        int px=(ix+i)%w;if(px<0)px+=w;int py=std::clamp(iy+j,0,h-1);double weight=(i?fx:1-fx)*(j?fy:1-fy);
        for(int channel=0;channel<3;channel++)rgb[channel]+=float(pixels[(size_t(py)*w+px)*3+channel]*weight);
    }
    for(auto &channel:rgb)channel=float(std::pow(std::max(double(channel),0.),config.contrast)*config.brightness);
    return rgb;
}
}
// A visible-only raw cube: no diffuse SH, GGX filtering or BRDF LUT is created.
// Creation/release are explicit loading boundaries with an active GL context.
struct WorldCitySky {
    GLuint texture=0;std::uint64_t bytes=0;world_city_sky::Config config;
    void release(){glDeleteTextures(1,&texture);texture=0;bytes=0;}
    bool ready()const{return texture!=0;}
    int load(const char *manifest,std::string &error){
        world_city_sky::Config next;int status=world_city_sky::read(manifest,next,error);if(status!=1)return status;
        std::error_code ec;auto encoded=std::filesystem::file_size(next.panorama,ec);
        if(ec||encoded>64ull*1024*1024){error="City sky HDR missing or exceeds64MiB";return -1;}
        int w=0,h=0,c=0;if(!stbi_is_hdr(next.panorama.c_str())||!stbi_info(next.panorama.c_str(),&w,&h,&c)||w!=h*2||w<2||h<2||w>4096||h>2048){error="City sky requires a2:1 Radiance HDR up to4096x2048";return -1;}
        std::unique_ptr<float,decltype(&stbi_image_free)> pixels(stbi_loadf(next.panorama.c_str(),&w,&h,&c,3),stbi_image_free);
        if(!pixels){error="City sky HDR decode failed";return -1;}
        for(size_t i=0;i<size_t(w)*h*3;i++)if(!std::isfinite(pixels.get()[i])||pixels.get()[i]<0||std::pow(double(pixels.get()[i]),next.contrast)*next.brightness>60000){error="Invalid/overflowing city sky radiance";return -1;}
        struct State{
            GLint active=0,cube=0,pbo=0,align=0,row=0,image=0,skipx=0,skipy=0,skipz=0;
            State(){glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&cube);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&pbo);glGetIntegerv(GL_UNPACK_ALIGNMENT,&align);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&image);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipx);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipy);glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&skipz);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);for(GLenum p:{GL_UNPACK_ROW_LENGTH,GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_IMAGES})glPixelStorei(p,0);}
            ~State(){glBindTexture(GL_TEXTURE_CUBE_MAP,cube);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);glPixelStorei(GL_UNPACK_ALIGNMENT,align);glPixelStorei(GL_UNPACK_ROW_LENGTH,row);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,image);glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipx);glPixelStorei(GL_UNPACK_SKIP_ROWS,skipy);glPixelStorei(GL_UNPACK_SKIP_IMAGES,skipz);glActiveTexture(active);}
        }state;
        GLuint result=0;glGenTextures(1,&result);glBindTexture(GL_TEXTURE_CUBE_MAP,result);std::vector<float> buffer(size_t(next.side)*next.side*3);
        for(unsigned f=0;f<6;f++){
            for(unsigned y=0;y<next.side;y++)for(unsigned x=0;x<next.side;x++){
                auto rgb=world_city_sky::lookup(pixels.get(),w,h,world_city_sky::face(f,2.*(x+.5)/next.side-1,2.*(y+.5)/next.side-1),next);std::copy(rgb.begin(),rgb.end(),buffer.begin()+(size_t(y)*next.side+x)*3);
            }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,0,GL_RGB16F,int(next.side),int(next.side),0,GL_RGB,GL_FLOAT,buffer.data());
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAX_LEVEL,0);
        for(GLenum axis:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})glTexParameteri(GL_TEXTURE_CUBE_MAP,axis,GL_CLAMP_TO_EDGE);
        if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&result);error="City sky cube upload failed";return -1;}
        release();texture=result;config=next;bytes=std::uint64_t(next.side)*next.side*6*6;return 1;
    }
};
#endif

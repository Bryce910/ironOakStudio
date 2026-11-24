#ifndef VALWOOD_WORLD_COLOR_GRADE_H
#define VALWOOD_WORLD_COLOR_GRADE_H

#include <algorithm>
#include <cstdio>
#include <vector>
#include <SDL2/SDL_opengl.h>
#ifndef STBI_INCLUDE_STB_IMAGE_H
#include "stb_image.h"
#endif

// The purchased strip stores display-referred RGB, not light or albedo. Expand
// its red-fastest, blue-sliced layout without filtering or color conversion.
// The post pass samples it only after its single linear-to-sRGB conversion.
struct WorldColorGrade {
    GLuint texture = 0;
    static constexpr int edge = 16;
    bool load(const char *path) {
        if(texture)return true;
        int width=0,height=0,channels=0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char *strip=stbi_load(path,&width,&height,&channels,4);
        if(!strip||width!=edge*edge||height!=edge) {
            std::fprintf(stderr,"[WorldGrade] Expected256x16 source strip: %s\n",path);
            stbi_image_free(strip);return false;
        }
        std::vector<unsigned char> cube(edge*edge*edge*4);
        for(int blue=0;blue<edge;blue++)for(int green=0;green<edge;green++)for(int red=0;red<edge;red++) {
            const size_t source=(size_t(green)*width+blue*edge+red)*4;
            const size_t target=((size_t(blue)*edge+green)*edge+red)*4;
            std::copy_n(strip+source,4,cube.data()+target);
        }
        stbi_image_free(strip);
        GLint previous=0,alignment=0,row=0,image=0,pbo=0,skip_pixels=0,skip_rows=0,skip_images=0;
        glGetIntegerv(GL_TEXTURE_BINDING_3D,&previous);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&pbo);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);
        glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&image);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixels);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_rows);glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&skip_images);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_3D,texture);
        // Half-float storage keeps filtering smooth rather than quantizing
        // each LUT interpolation back to8-bit display steps.
        glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA16F,edge,edge,edge,0,GL_RGBA,GL_UNSIGNED_BYTE,cube.data());
        glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
        GLint uploaded_width=0;glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_WIDTH,&uploaded_width);
        if(uploaded_width!=edge){glDeleteTextures(1,&texture);texture=0;}
        glBindTexture(GL_TEXTURE_3D,GLuint(previous));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(pbo));
        glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,row);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,image);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixels);glPixelStorei(GL_UNPACK_SKIP_ROWS,skip_rows);glPixelStorei(GL_UNPACK_SKIP_IMAGES,skip_images);
        return texture!=0;
    }
    void release() {glDeleteTextures(1,&texture);texture=0;}
};
#endif

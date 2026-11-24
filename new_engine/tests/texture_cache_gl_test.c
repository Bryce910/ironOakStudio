/* Compile this source twice: once normally, once with
   -DTEXTURE_CACHE_GL_OTHER_TU to model independent scene-loader translation
   units sharing the single linked engine/texture_cache.c implementation. */
#define GL_GLEXT_PROTOTYPES
#ifndef TEXTURE_CACHE_GL_OTHER_TU
#define STB_IMAGE_IMPLEMENTATION
#endif
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/texture_cache.h"
#include "tests/fixtures/texture_cache_fixture.h"
#include <assert.h>

static unsigned upload(const void *bytes,size_t length,const TextureCachePolicy *policy){
    return policy->srgb?GLH_texture_mem_srgb(bytes,(int)length):GLH_texture_mem(bytes,(int)length);
}
static void destroy(unsigned texture){glDeleteTextures(1,&texture);}
static unsigned fixture_acquire(bool srgb){
    const TextureCachePolicy policy={.srgb=srgb,.min_filter=GL_LINEAR_MIPMAP_LINEAR,
        .mag_filter=GL_LINEAR,.wrap_s=GL_CLAMP_TO_EDGE,.wrap_t=GL_CLAMP_TO_EDGE,.mip_policy=1};
    return texture_cache_acquire(texture_fixture_png,sizeof(texture_fixture_png),&policy,upload,destroy);
}

#ifdef TEXTURE_CACHE_GL_OTHER_TU
unsigned texture_cache_gl_other_acquire(bool srgb){return fixture_acquire(srgb);}
bool texture_cache_gl_other_retain(unsigned texture){return texture_cache_retain(texture);}
bool texture_cache_gl_other_release(unsigned texture){return texture_cache_release(texture);}
#else
extern unsigned texture_cache_gl_other_acquire(bool srgb);
extern bool texture_cache_gl_other_retain(unsigned texture);
extern bool texture_cache_gl_other_release(unsigned texture);

static void check_pixels(unsigned texture,bool srgb){
    assert(glIsTexture(texture));glBindTexture(GL_TEXTURE_2D,texture);
    GLint width=0,height=0,format=0,wrap=0,filter=0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
    assert(width==TEXTURE_FIXTURE_WIDTH&&height==TEXTURE_FIXTURE_HEIGHT);
    assert(srgb?format==GL_SRGB8_ALPHA8:(format==GL_RGBA8||format==GL_RGBA));
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&wrap);
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&filter);
    assert(wrap==GL_CLAMP_TO_EDGE&&filter==GL_LINEAR_MIPMAP_LINEAR);
    unsigned char pixels[sizeof(texture_fixture_pixels)];
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    assert(!memcmp(pixels,texture_fixture_pixels,sizeof(pixels)));
}

int main(void){
    if(!SDL_getenv("VALWOOD_TEST_NATIVE_GL")) SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Texture cache GL validation",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    printf("Texture cache renderer: %s\n",glGetString(GL_RENDERER));

    unsigned a=fixture_acquire(true),b=texture_cache_gl_other_acquire(true);
    assert(a&&a==b&&texture_cache_gl_other_retain(a));
    TextureCacheStats stats=texture_cache_stats();assert(stats.entries==1&&stats.references==3);
    check_pixels(a,true);
    unsigned linear=texture_cache_gl_other_acquire(false);
    assert(linear&&linear!=a);check_pixels(linear,false);
    assert(texture_cache_trim_source_pages());
    unsigned after_trim=texture_cache_gl_other_acquire(true);
    assert(after_trim==a&&texture_cache_release(after_trim));
    check_pixels(a,true);check_pixels(linear,false);
    assert(texture_cache_release(a)&&glIsTexture(a));
    assert(texture_cache_gl_other_release(b)&&glIsTexture(a));
    assert(texture_cache_release(a)&&!glIsTexture(a));
    assert(texture_cache_gl_other_release(linear)&&!glIsTexture(linear));

    /* Reverse which loader owns the first upload and the last release. */
    a=texture_cache_gl_other_acquire(true);b=fixture_acquire(true);
    assert(a&&a==b);check_pixels(a,true);
    assert(texture_cache_gl_other_release(a)&&glIsTexture(b));
    assert(texture_cache_release(b)&&!glIsTexture(b));
    stats=texture_cache_stats();assert(!stats.entries&&!stats.references&&!stats.source_bytes);
    assert(glGetError()==GL_NO_ERROR);texture_cache_shutdown();
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    puts("Texture cache GL: exact pixels/mips and shared IDs survive source-page trimming; full dimensions, color spaces, sampler state, cross-TU ownership and deletion passed.");
    return 0;
}
#endif

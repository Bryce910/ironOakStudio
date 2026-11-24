/* Actual production post-shader correctness, not an art or performance test.
 * Float targets preserve values for transfer-function checks. Software/offscreen
 * by default; VALWOOD_TEST_NATIVE_GL=1 preserves the caller's native driver. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_color_grade.h"

namespace {
constexpr int side=33; // Odd dimensions give an exact centre pixel at UV(.5,.5).
using Pixel=std::array<float,4>;

std::string source(const char *path) {
    std::ifstream file(path);assert(file);std::ostringstream text;text<<file.rdbuf();return text.str();
}
GLuint shader(GLenum type,const std::string &text) {
    GLuint id=glCreateShader(type);const char *bytes=text.c_str();
    glShaderSource(id,1,&bytes,nullptr);glCompileShader(id);
    GLint okay=0;glGetShaderiv(id,GL_COMPILE_STATUS,&okay);
    if(!okay){char log[8192];glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}
    assert(okay);return id;
}
GLuint program() {
    GLuint vertex=shader(GL_VERTEX_SHADER,source("shaders/world_post.vs.glsl"));
    GLuint fragment=shader(GL_FRAGMENT_SHADER,source("shaders/world_post.fs.glsl"));
    GLuint id=glCreateProgram();glAttachShader(id,vertex);glAttachShader(id,fragment);glLinkProgram(id);
    GLint okay=0;glGetProgramiv(id,GL_LINK_STATUS,&okay);
    if(!okay){char log[8192];glGetProgramInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}
    assert(okay);glDeleteShader(vertex);glDeleteShader(fragment);return id;
}
GLuint texture(int unit,GLenum format,const float *data) {
    GLuint id=0;glGenTextures(1,&id);glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,format==GL_RGBA?GL_RGBA32F:GL_R32F,1,1,0,format,GL_FLOAT,data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    return id;
}
struct Fixture {
    GLuint shader_program=0,vao=0,fbo=0,target=0,scene=0,depth=0,identity=0,bloom=0;
    Fixture() {
        shader_program=program();glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
        const Pixel empty={};scene=texture(0,GL_RGBA,empty.data());const float far_depth=1;
        depth=texture(1,GL_RED,&far_depth);
        bloom=texture(3,GL_RGBA,empty.data());
        glUseProgram(shader_program);glUniform1i(glGetUniformLocation(shader_program,"uScene"),0);
        glUniform1i(glGetUniformLocation(shader_program,"uDepth"),1);
        glUniform1i(glGetUniformLocation(shader_program,"uVillageGrade"),2);
        glUniform1i(glGetUniformLocation(shader_program,"uBloom"),3);
        glUniform1f(glGetUniformLocation(shader_program,"uBloomStrength"),0);
        std::vector<unsigned char> cube(16*16*16*4);
        for(int b=0;b<16;b++)for(int g=0;g<16;g++)for(int r=0;r<16;r++) {
            const size_t i=((b*16+g)*16+r)*4;
            cube[i]=r*17;cube[i+1]=g*17;cube[i+2]=b*17;cube[i+3]=255;
        }
        glGenTextures(1,&identity);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_3D,identity);
        glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA16F,16,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,cube.data());
        glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
        glUniform1f(glGetUniformLocation(shader_program,"uNear"),.4f);
        glUniform1f(glGetUniformLocation(shader_program,"uFar"),15000.f);
        glUniform2f(glGetUniformLocation(shader_program,"uResolution"),side,side);
        glViewport(0,0,side,side);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_FRAMEBUFFER_SRGB);
    }
    ~Fixture() {
        glDeleteTextures(1,&scene);glDeleteTextures(1,&depth);glDeleteTextures(1,&target);glDeleteTextures(1,&identity);glDeleteTextures(1,&bloom);
        glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);glDeleteProgram(shader_program);
    }
    void grade(GLuint map,float strength) {
        glUseProgram(shader_program);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_3D,map);
        glUniform1f(glGetUniformLocation(shader_program,"uVillageGradeWeight"),strength);
    }
    std::vector<Pixel> render(Pixel input,float exposure=1,bool temporal=false,Pixel highlight={0,0,0,0},float bloom_strength=0) {
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,scene);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_FLOAT,input.data());
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,bloom);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_FLOAT,highlight.data());
        glUseProgram(shader_program);glUniform1f(glGetUniformLocation(shader_program,"uExposure"),exposure);
        glUniform1i(glGetUniformLocation(shader_program,"uTemporalActive"),temporal?1:0);
        glUniform1f(glGetUniformLocation(shader_program,"uBloomStrength"),bloom_strength);
        glDrawArrays(GL_TRIANGLES,0,3);std::vector<Pixel> pixels(side*side);
        glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,pixels.data());
        for(const auto &pixel:pixels) {
            for(float value:pixel)assert(std::isfinite(value));
            for(int channel=0;channel<3;++channel)assert(pixel[channel]>=0&&pixel[channel]<=1.000001f);
            assert(pixel[3]==1);
        }
        assert(glGetError()==GL_NO_ERROR);return pixels;
    }
};
Pixel center(const std::vector<Pixel> &pixels){return pixels[(side/2)*side+side/2];}
float energy(const Pixel &pixel){return pixel[0]+pixel[1]+pixel[2];}
void black(const std::vector<Pixel> &pixels){for(const auto &pixel:pixels)assert(energy(pixel)==0);}
float expected_neutral(double x) {
    // Published ACES fitted neutral response followed by IEC sRGB encoding.
    // Uniform .18 has no edge/bloom contribution and the centre has no vignette.
    const double linear=(x*(2.51*x+.03))/(x*(2.43*x+.59)+.14);
    return float(linear<=.0031308?12.92*linear:1.055*std::pow(linear,1./2.4)-.055);
}
Pixel expected_composite(Pixel input,Pixel bloom,double strength,double exposure) {
    // Independent CPU reference for a uniform scene at the centre pixel:
    // add scene-linear highlight, expose once, tone-map, then the existing
    // mild saturation transform and a single exact sRGB transfer.
    std::array<double,3> mapped={};
    for(int c=0;c<3;c++) {
        const double x=(std::max(double(input[c]),0.)+std::max(double(bloom[c]),0.)*strength)*std::max(exposure,0.);
        mapped[c]=std::clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);
    }
    const double luminance=mapped[0]*.2126+mapped[1]*.7152+mapped[2]*.0722;
    Pixel result={0,0,0,1};
    for(int c=0;c<3;c++) {
        const double linear=mapped[c]*.98+luminance*.02;
        result[c]=float(linear<=.0031308?12.92*linear:1.055*std::pow(linear,1./2.4)-.055);
    }
    return result;
}
}

int main(int argc,char **argv) {
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Production post correctness",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Post renderer: %s\n",glGetString(GL_RENDERER));
    {
        Fixture test;
        black(test.render({0,0,0,1}));black(test.render({-1,-.2f,-.01f,1}));
        black(test.render({.5f,.5f,.5f,1},0));black(test.render({.5f,.5f,.5f,1},-1));
        const float levels[]={0,.000001f,.0001f,.001f,.005f,.02f,.06f,.18f,.5f,1,2,4,8,16};
        float previous=-1;
        for(float value:levels) {
            Pixel pixel=center(test.render({value,value,value,1}));
            assert(pixel[0]>=previous-1e-6f);
            assert(std::abs(pixel[0]-pixel[1])<2e-6f&&std::abs(pixel[1]-pixel[2])<2e-6f);
            previous=pixel[0];
        }
        const Pixel low=center(test.render({1,1,1,1})),middle=center(test.render({2,2,2,1})),high=center(test.render({4,4,4,1}));
        assert(middle[0]>low[0]+.01f&&high[0]>middle[0]+.01f);
        const Pixel dim=center(test.render({.18f,.18f,.18f,1},.5f));
        const auto normal=test.render({.18f,.18f,.18f,1});const Pixel grey=center(normal);
        const Pixel bright=center(test.render({.18f,.18f,.18f,1},2));
        assert(dim[0]+.05f<grey[0]&&grey[0]+.05f<bright[0]);
        assert(std::abs(grey[0]-expected_neutral(.18))<3e-5f);
        // A near-black neutral also exercises sRGB's linear toe. A pure
        // power approximation can look close at grey while crushing this.
        const Pixel toe=center(test.render({.001f,.001f,.001f,1}));
        assert(std::abs(toe[0]-expected_neutral(.001))<3e-6f);
        for(const Pixel input:{Pixel{.18f,.18f,.18f,1},Pixel{.7f,.2f,.03f,1},Pixel{8,.01f,.3f,1},Pixel{.001f,.001f,.001f,1}}) {
            const auto disabled=test.render(input,1,false),enabled=test.render(input,1,true);
            for(size_t i=0;i<disabled.size();++i)for(int channel=0;channel<4;++channel)
                assert(std::abs(disabled[i][channel]-enabled[i][channel])<2e-6f);
        }
        // Bloom is a restrained HDR addition, not a display-space overlay or
        // second exposure. Non-unit exposures distinguish those wrong orders.
        constexpr float bloom_strength=.035f;
        const Pixel neutral_scene={.18f,.18f,.18f,1},neutral_glow={2,2,2,0};
        for(float exposure:{.55f,1.f,1.28f,1.6f}) {
            const auto reference=expected_composite(neutral_scene,neutral_glow,bloom_strength,exposure);
            const auto actual=center(test.render(neutral_scene,exposure,false,neutral_glow,bloom_strength));
            for(int c=0;c<3;c++)assert(std::abs(actual[c]-reference[c])<3e-5f);
            assert(actual[0]>center(test.render(neutral_scene,exposure))[0]+.01f);
        }
        const Pixel colored_scene={.12f,.15f,.18f,1},colored_glow={4,.7f,.05f,0};
        const auto colored_reference=expected_composite(colored_scene,colored_glow,bloom_strength,1.3f);
        const auto colored_actual=center(test.render(colored_scene,1.3f,false,colored_glow,bloom_strength));
        for(int c=0;c<3;c++)assert(std::abs(colored_actual[c]-colored_reference[c])<3e-5f);
        for(const Pixel input:{Pixel{.18f,.18f,.18f,1},Pixel{.7f,.2f,.03f,1},Pixel{8,.01f,.3f,1},Pixel{.001f,.001f,.001f,1}}) {
            const auto reference=test.render(input,1.28f);
            // Every pixel is exact: disabled bloom ignores even a bright map;
            // an enabled black map preserves the same chromatic source image.
            assert(reference==test.render(input,1.28f,false,colored_glow,0));
            assert(reference==test.render(input,1.28f,false,{0,0,0,0},bloom_strength));
            assert(reference==test.render(input,1.28f,false,{-2,-1,-.5f,0},bloom_strength));
        }
        black(test.render({0,0,0,1},0,false,{8,4,2,0},bloom_strength));
        std::puts("Post HDR bloom: scene-linear addition before single exposure/tonemap/sRGB, non-unit exposures, chromatic reference, exact disabled/black/negative-map paths PASS");
        // Restrained, symmetric vignette must not bury peripheral detail.
        float darkest=1;
        for(const auto &pixel:normal)darkest=std::min(darkest,pixel[0]/grey[0]);
        assert(darkest>.94f&&darkest<.995f);
        assert(std::abs(normal.front()[0]-normal.back()[0])<2e-6f);
        // Identity/endpoints verify red/green/blue axes and half-texel addressing.
        // The actual purchased strip also checks upload orientation and that its
        // display values are not accidentally decoded/encoded as albedo.
        const std::array<Pixel,7> grade_inputs={Pixel{0,0,0,1},Pixel{.001f,.02f,.06f,1},Pixel{.18f,.18f,.18f,1},Pixel{.7f,.2f,.03f,1},Pixel{8,.01f,.3f,1},Pixel{.02f,.8f,.1f,1},Pixel{20,20,20,1}};
        for(const auto &input:grade_inputs) {
            test.grade(test.identity,0);const auto reference=center(test.render(input));
            test.grade(test.identity,1);const auto identity=center(test.render(input));
            // Half-float source storage/filtering permits less than one sRGB code value.
            for(int c=0;c<3;c++)assert(std::abs(reference[c]-identity[c])<8e-4f);
        }
        WorldColorGrade source_grade;
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_3D,test.identity);
        GLuint unpack=0;glGenBuffers(1,&unpack);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);glBufferData(GL_PIXEL_UNPACK_BUFFER,64,nullptr,GL_STREAM_DRAW);
        glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,5);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,2);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,3);glPixelStorei(GL_UNPACK_SKIP_ROWS,4);glPixelStorei(GL_UNPACK_SKIP_IMAGES,5);
        assert(source_grade.load("projects/valwood_world/ground/village_grade.png"));
        GLint state=0;glGetIntegerv(GL_TEXTURE_BINDING_3D,&state);assert(GLuint(state)==test.identity);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&state);assert(state==8);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&state);assert(state==5);
        glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&state);assert(state==2);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&state);assert(GLuint(state)==unpack);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&state);assert(state==3);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&state);assert(state==4);glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&state);assert(state==5);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glDeleteBuffers(1,&unpack);
        glPixelStorei(GL_UNPACK_ALIGNMENT,4);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
        int sw=0,sh=0,sc=0;unsigned char *strip=stbi_load("projects/valwood_world/ground/village_grade.png",&sw,&sh,&sc,4);assert(strip&&sw==256&&sh==16);
        auto lookup=[&](const Pixel &rgb,int c) {
            int lo[3],hi[3];float fraction[3];
            for(int k=0;k<3;k++){float x=std::clamp(rgb[k],0.f,1.f)*15;lo[k]=int(std::floor(x));hi[k]=std::min(lo[k]+1,15);fraction[k]=x-lo[k];}
            float value=0;
            for(int b=0;b<2;b++)for(int g=0;g<2;g++)for(int r=0;r<2;r++) {
                const int red=r?hi[0]:lo[0],green=g?hi[1]:lo[1],blue=b?hi[2]:lo[2];
                const float w=(r?fraction[0]:1-fraction[0])*(g?fraction[1]:1-fraction[1])*(b?fraction[2]:1-fraction[2]);
                value+=w*strip[(green*256+blue*16+red)*4+c]/255.f;
            }
            return value;
        };
        bool changes_colour=false;
        for(const auto &input:grade_inputs) {
            test.grade(test.identity,0);const auto ungraded=test.render(input);
            test.grade(source_grade.texture,0);const auto disabled=test.render(input);
            assert(ungraded==disabled);
            test.grade(source_grade.texture,0);const Pixel neutral=center(test.render(input));
            test.grade(source_grade.texture,1);const Pixel full=center(test.render(input));
            test.grade(source_grade.texture,.72f);const Pixel mixed=center(test.render(input));
            for(int c=0;c<3;c++) {
                const float expected=lookup(neutral,c);
                assert(std::abs(full[c]-expected)<8e-4f);
                assert(std::abs(mixed[c]-(neutral[c]*.28f+expected*.72f))<8e-4f);
                changes_colour|=std::abs(full[c]-neutral[c])>.03f;
            }
        }
        assert(changes_colour);stbi_image_free(strip);
        test.grade(test.identity,0);source_grade.release();
        std::puts("Post source grade: identity/axes/endpoints, actual-strip trilinear reference, exact disabled path, strength, upload state and ownership PASS");
        std::printf("Post: neutral .18 encoded %.7f reference %.7f; exposure %.5f < %.5f < %.5f; corner/centre %.5f\n",
            grey[0],expected_neutral(.18),dim[0],grey[0],bright[0],darkest);
    }
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::puts("world_post_gl_test: PASS (production shader; black/negative/exposure/HDR/transfer/temporal/vignette/bloom)");
    return 0;
}

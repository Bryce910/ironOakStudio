/* New village startup texture ownership, exact pixels and hostile unpack state.
 * Software correctness by default; native opt-in is never an FPS measurement. */
#define GL_GLEXT_PROTOTYPES
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_village_textures.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {
using State=std::array<GLint,10>;
State state() {
    State out={};const GLenum names[]={GL_PIXEL_UNPACK_BUFFER_BINDING,GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,
        GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_IMAGES,
        GL_TEXTURE_BINDING_2D,GL_TEXTURE_BINDING_2D_ARRAY,GL_TEXTURE_BINDING_3D};
    for(std::size_t i=0;i<out.size();i++)glGetIntegerv(names[i],&out[i]);
    return out;
}
std::vector<unsigned char> image(const char *file,int &w,int &h) {
    int c=0;auto *p=stbi_load(file,&w,&h,&c,4);assert(p);
    std::vector<unsigned char> out(p,p+std::size_t(w)*h*4);stbi_image_free(p);return out;
}
GLuint shader(GLenum type,const char *path) {
    std::ifstream file(path);assert(file);std::ostringstream buffer;buffer<<file.rdbuf();auto source=buffer.str();
    const char *p=source.c_str();GLuint s=glCreateShader(type);glShaderSource(s,1,&p,nullptr);glCompileShader(s);
    GLint okay=0;glGetShaderiv(s,GL_COMPILE_STATUS,&okay);
    if(!okay){char log[4096]={};glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);return s;
}
void check_samplers() {
    GLuint vs=shader(GL_VERTEX_SHADER,"shaders/world_terrain.vs.glsl"),fs=shader(GL_FRAGMENT_SHADER,"shaders/world_terrain.fs.glsl");
    GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);
    GLint okay=0,uniforms=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);assert(okay);
    glGetProgramiv(p,GL_ACTIVE_UNIFORMS,&uniforms);int samplers=0;
    for(int i=0;i<uniforms;i++) {
        char name[256];GLsizei length=0;GLint size=0;GLenum type=0;
        glGetActiveUniform(p,GLuint(i),sizeof(name),&length,&size,&type,name);
        if(type==GL_SAMPLER_2D||type==GL_SAMPLER_2D_ARRAY)samplers+=size;
    }
    assert(samplers==16);glDeleteProgram(p);glDeleteShader(vs);glDeleteShader(fs);
    std::puts("PASS terrain sampler budget: actual linked shader uses16 samplers; source litter maps share one array");
}
}
int main(int argc,char **argv) {
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Village material resources",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Village texture renderer: %s\n",glGetString(GL_RENDERER));
    check_samplers();glPixelStorei(GL_PACK_ALIGNMENT,1);
    GLuint sentinel[3],pbo;glGenTextures(3,sentinel);glBindTexture(GL_TEXTURE_2D,sentinel[0]);
    glBindTexture(GL_TEXTURE_2D_ARRAY,sentinel[1]);glBindTexture(GL_TEXTURE_3D,sentinel[2]);
    glGenBuffers(1,&pbo);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);glBufferData(GL_PIXEL_UNPACK_BUFFER,1,nullptr,GL_STATIC_DRAW);
    glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,17);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,19);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,2);glPixelStorei(GL_UNPACK_SKIP_ROWS,3);glPixelStorei(GL_UNPACK_SKIP_IMAGES,4);
    const auto original=state();WorldVillageTextures textures;
    const char *normal="projects/valwood_world/ground/terrain_dirt_nor.png",*arm="projects/valwood_world/ground/terrain_dirt_rgh.png";
    assert(textures.load_litter(normal,arm)&&state()==original);
    int w=0,h=0,aw=0,ah=0;auto n=image(normal,w,h),a=image(arm,aw,ah);assert(w==aw&&h==ah);
    std::vector<unsigned char> actual(std::size_t(w)*h*4*2);glBindTexture(GL_TEXTURE_2D_ARRAY,textures.litter);
    glGetTexImage(GL_TEXTURE_2D_ARRAY,0,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
    assert(!std::memcmp(actual.data(),n.data(),n.size())&&!std::memcmp(actual.data()+n.size(),a.data(),a.size()));
    GLint filter=0;glGetTexParameteriv(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,&filter);assert(filter==GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D_ARRAY,sentinel[1]);assert(state()==original);
    std::puts("PASS litter: every normal/ARM byte and layer exact; same dimensions, linear data, mip filtering, hostile unpack/PBO state restored");
    assert(textures.load_litter(normal,arm,arm,normal,arm)&&textures.forest_litter&&state()==original);
    actual.resize(n.size()*5);glBindTexture(GL_TEXTURE_2D_ARRAY,textures.litter);
    GLint layers=0;glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,0,GL_TEXTURE_DEPTH,&layers);assert(layers==5);
    glGetTexImage(GL_TEXTURE_2D_ARRAY,0,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
    const std::vector<unsigned char>* expected[]={&n,&a,&a,&n,&a};
    for(unsigned layer=0;layer<5;layer++)assert(!std::memcmp(actual.data()+layer*n.size(),expected[layer]->data(),n.size()));
    glBindTexture(GL_TEXTURE_2D_ARRAY,sentinel[1]);const auto forest_id=textures.litter;
    assert(!textures.load_litter(normal,arm,arm,nullptr,arm)&&textures.litter==forest_id&&textures.forest_litter&&state()==original);
    assert(!textures.load_litter(normal,arm,arm,normal,"missing.png")&&textures.litter==forest_id&&textures.forest_litter&&state()==original);
    assert(textures.load_litter(normal,arm)&&!textures.forest_litter&&state()==original);
    std::puts("PASS optional forest maps: five exact layers, transactional missing/partial input, legacy reload, unchanged unpack state");
    const char *history="projects/valwood_world/ground/village_ground_history.png";
    assert(textures.load_history(history)&&state()==original);auto mask=image(history,w,h);
    actual.resize(mask.size());glBindTexture(GL_TEXTURE_2D,textures.history);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());assert(actual==mask);
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&filter);assert(filter==GL_LINEAR_MIPMAP_LINEAR);
    GLint last_width=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,11,GL_TEXTURE_WIDTH,&last_width);assert(last_width==1);
    glBindTexture(GL_TEXTURE_2D,sentinel[0]);assert(state()==original);
    const auto previous_history=textures.history,previous_litter=textures.litter;
    assert(!textures.load_history(normal)&&!textures.load_history(nullptr)&&!textures.load_litter(nullptr,arm));
    assert(textures.history==previous_history&&textures.litter==previous_litter&&state()==original);
    std::puts("PASS history:2049 square linearRGBA orientation/weights exact, full mip chain, transactional invalid input and unpack state preserved");
    WorldVillageSkyConfig config;config.columns=config.rows=513;
    std::vector<float> roofs(513*513,24.f);assert(textures.build_directional(roofs,config)&&state()==original);
    const auto nodes=std::size_t(textures.directional_side)*textures.directional_side*textures.directional_slices;
    actual.resize(nodes);glBindTexture(GL_TEXTURE_3D,textures.up);glGetTexImage(GL_TEXTURE_3D,0,GL_RED,GL_UNSIGNED_BYTE,actual.data());
    for(auto p:actual)assert(p==255);
    glBindTexture(GL_TEXTURE_3D,textures.directional);actual.resize(nodes*4);glGetTexImage(GL_TEXTURE_3D,0,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
    for(auto p:actual)assert(p==255);
    glBindTexture(GL_TEXTURE_3D,sentinel[2]);assert(state()==original);
    auto old_directional=textures.directional;roofs[0]=NAN;assert(!textures.build_directional(roofs,config)&&state()==original&&textures.directional==old_directional);
    std::puts("PASS directional volumes: correctly aligned257-wideR8 andRGBA8, open-sky bytes, bounded height metadata, invalid-input ownership and GL state");
    GLuint owned[]={textures.litter,textures.history,textures.directional,textures.up};textures.release();
    for(auto id:owned)assert(!glIsTexture(id));
    assert(state()==original&&glGetError()==GL_NO_ERROR);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glDeleteBuffers(1,&pbo);glDeleteTextures(3,sentinel);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();std::puts("world_village_textures_gl_test: PASS");
}

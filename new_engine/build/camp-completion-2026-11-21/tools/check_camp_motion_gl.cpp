#define GL_GLEXT_PROTOTYPES
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "gl_helpers.h"
#include "engine/world_animation.h"
#include "engine/world_animation_render.h"
#include "engine/world_program_epoch.h"
#include "engine/world_trail_camp.h"
#include "engine/world_camp_presentation.h"
#include "engine/world_camp_skeletal_private.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>
#include <algorithm>
extern "C" {
#include "engine/texture_cache.h"
}
namespace a=world_animation;namespace ar=world_animation_render;
std::string read(const char*p){std::ifstream f(p);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
a::Vec3 point(const a::Asset&asset,const a::Pose&p,unsigned n){a::Vec3 v,normal;std::string e;assert(a::deform_vertex(asset.mesh.vertices[n],p,v,normal,e));return a::transform_point(p.global[asset.skeleton.mesh_node],v);}
int main(int argc,char**argv){assert(argc==4);assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto*w=SDL_CreateWindow("camp-motion-test",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);auto context=SDL_GL_CreateContext(w);assert(context);
const auto vs=read(argv[1]);const char*fs="#version 330 core\nin vec2 vUV;in vec4 vPreviousClip,vStaticClip;out vec4 color;void main(){color=vec4(vUV,0,1);}";GLuint program=GLH_program(vs.c_str(),fs);assert(program);const char*varyings[]={"gl_Position","vPreviousClip","vStaticClip"};glTransformFeedbackVaryings(program,3,varyings,GL_INTERLEAVED_ATTRIBS);glLinkProgram(program);GLint linked;glGetProgramiv(program,GL_LINK_STATUS,&linked);assert(linked);glUseProgram(program);a::Mat4 identity;for(const char*name:{"uVP","uPrevVP"})glUniformMatrix4fv(glGetUniformLocation(program,name),1,GL_FALSE,identity.m.data());glUniform1i(glGetUniformLocation(program,"uPrevSkinPalette"),18);std::string error;
for(unsigned file=2;file<unsigned(argc);++file){a::Asset asset;assert(a::load_glb(argv[file],asset,error));auto*r=ar::create(asset,1,error,96ull*1024*1024);if(!r){std::fprintf(stderr,"%s\n",error.c_str());return 1;}assert(ar::warm(r,program,error));a::Pose previous,current;assert(a::sample(asset.skeleton,asset.clips[0],2,true,previous,error));assert(a::sample(asset.skeleton,asset.clips[2],3,true,current,error));ar::Instance instance;auto bindings=ar::capture_bindings();assert(ar::upload(r,&previous,&instance,1,bindings,error));assert(ar::upload(r,&current,&instance,1,bindings,error));const auto palette=ar::previous_palette(r);assert(palette);glActiveTexture(GL_TEXTURE18);glBindTexture(GL_TEXTURE_BUFFER,palette);glBindSampler(18,0);glActiveTexture(GL_TEXTURE0);glUniform1i(glGetUniformLocation(program,"uHasPrevPalette"),1);
std::vector<std::array<float,12>> result(asset.mesh.indices.size());GLuint buffer;glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,result.size()*sizeof(result[0]),nullptr,GL_DYNAMIC_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_TRIANGLES);bindings=ar::capture_bindings();assert(ar::draw(r,program,bindings,error));glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,result.size()*sizeof(result[0]),result.data());assert(glGetError()==GL_NO_ERROR);double max_error=0,max_motion=0;for(unsigned n=0;n<result.size();++n){const auto c=point(asset,current,asset.mesh.indices[n]),p=point(asset,previous,asset.mesh.indices[n]);const float cc[]={c.x,c.y,c.z,1},pp[]={p.x,p.y,p.z,1};for(unsigned k=0;k<4;++k){max_error=std::max({max_error,std::abs(double(result[n][k]-cc[k])),std::abs(double(result[n][4+k]-pp[k])),std::abs(double(result[n][8+k]-cc[k]))});max_motion=std::max(max_motion,std::abs(double(result[n][k]-result[n][4+k])));}}std::printf("%s: indexed_vertices=%zu current/previous/static CPU/GPU maximum_error=%.9f actual_pose_motion=%.6f renderer=%s\n",argv[file],result.size(),max_error,max_motion,glGetString(GL_RENDERER));assert(max_error<.00002&&max_motion>.10);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);glActiveTexture(GL_TEXTURE18);glBindTexture(GL_TEXTURE_BUFFER,0);glActiveTexture(GL_TEXTURE0);ar::destroy(r);assert(texture_cache_stats().references==0);}
const auto directory=std::filesystem::path(argv[2]).parent_path().string();setenv("VALWOOD_CAMP_SKELETAL_DIRECTORY",directory.c_str(),1);
{auto bank=WorldCampSkeletal::load();assert(bank&&bank->enabled&&bank->warm(program));
 std::array<WorldTrailCamper,4> people;world_camp_presentation::State acting;acting.actors[0].speaking=true;acting.actors[0].nearby=true;acting.actors[1].nearby=false;
 for(unsigned frame=0;frame<180;++frame)assert(bank->update(people,acting,double(frame)/60));
 assert(bank->uploads==540&&bank->actors[0]->weights[2]>.85f&&bank->actors[1]->weights[3]>.67f);
 for(const auto&p:bank->actors)assert(p->asset.mesh.vertices.empty()&&p->asset.images.empty()&&world_animation_render::stats(p->gpu).instances==1);
 const auto uploads=bank->uploads;assert(bank->update(people,acting,179./60));assert(bank->uploads==uploads);
 std::printf("actual camp bank: three appearances, 180 updates, 540 atomic GPU pose commits, talking/warming states, unchanged-time no-upload, released CPU mesh/images, cpu_bytes=%llu gpu_geometry_bytes=%llu PASS\n",(unsigned long long)bank->cpu_bytes,(unsigned long long)bank->gpu_bytes);
}
assert(texture_cache_stats().references==0);
glDeleteProgram(program);SDL_GL_DeleteContext(context);SDL_DestroyWindow(w);SDL_Quit();}

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

a::Vec3 point(const a::Asset&asset,const a::Pose&p,unsigned n){a::Vec3 v,normal; a::SkinVertex vertex;std::string error;assert(a::morph_vertex(asset,n,p,vertex,error));assert(a::deform_vertex(vertex,p,v,normal,error));return a::transform_point(p.global[asset.skeleton.mesh_node],v);}
int main(int argc,char**argv){
 assert(argc==2);assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto*w=SDL_CreateWindow("morph-motion-test",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);auto context=SDL_GL_CreateContext(w);assert(context);
 auto vs=read(argv[1]);const char*fs="#version 330 core\nin vec2 vUV;in vec4 vPreviousClip,vStaticClip;out vec4 color;void main(){color=vec4(vUV,0,1);}";GLuint program=GLH_program(vs.c_str(),fs);assert(program);const char*varyings[]={"gl_Position","vPreviousClip","vStaticClip"};glTransformFeedbackVaryings(program,3,varyings,GL_INTERLEAVED_ATTRIBS);glLinkProgram(program);GLint linked;glGetProgramiv(program,GL_LINK_STATUS,&linked);assert(linked);glUseProgram(program);a::Mat4 identity;for(auto name:{"uVP","uPrevVP"})glUniformMatrix4fv(glGetUniformLocation(program,name),1,GL_FALSE,identity.m.data());glUniform1i(glGetUniformLocation(program,"uPrevSkinPalette"),18);
 a::Asset asset;asset.source_path="mode1-regression";asset.skeleton.joints.resize(2);asset.skeleton.joints[0].name="joint";asset.skeleton.joints[1].name="mesh";asset.skeleton.palette={0};asset.skeleton.inverse_bind.resize(1);asset.skeleton.mesh_node=1;asset.mesh.vertices.resize(3);asset.mesh.vertices[0].position={-.3f,0,0};asset.mesh.vertices[1].position={.3f,0,0};asset.mesh.vertices[2].position={0,.5f,0};asset.mesh.indices={0,1,2};asset.mesh.primitives.push_back({0,3,-1});asset.morph_targets.push_back({"face",{{0,{.12f,.03f,0},{},{}}}});
 std::string error;assert(a::validate(asset,error));auto*r=ar::create(asset,1,error);if(!r){std::fprintf(stderr,"%s\n",error.c_str());return 1;}assert(ar::stats(r).morph_mode==1);assert(ar::warm(r,program,error));assert(ar::previous_palette(r)==0&&ar::previous_palette_offset(r)==0);
 GLuint buffer;glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,3*12*sizeof(float),nullptr,GL_DYNAMIC_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
 a::Pose previous,current;assert(a::rest_pose(asset.skeleton,previous,error));ar::Instance instance;previous.morph_weights[0]=.2f;assert(ar::upload(r,&previous,&instance,1,ar::capture_bindings(),error));assert(ar::previous_palette(r)==0&&ar::previous_palette_offset(r)==0);
 double maximum_error=0,maximum_motion=0;bool positive=false,negative=false;
 for(unsigned frame=0;frame<8;++frame){assert(a::rest_pose(asset.skeleton,current,error));current.local[0].translation={frame%2?.17f:-.09f,.03f*float(frame%3),0};current.morph_weights[0]=frame%2?.85f:.35f;assert(a::evaluate(asset.skeleton,current,error));assert(ar::upload(r,&current,&instance,1,ar::capture_bindings(),error));auto palette=ar::previous_palette(r);auto offset=ar::previous_palette_offset(r);assert(palette&&offset!=0);positive|=offset>0;negative|=offset<0;glActiveTexture(GL_TEXTURE18);glBindTexture(GL_TEXTURE_BUFFER,palette);glBindSampler(18,0);glActiveTexture(GL_TEXTURE0);glUniform1i(glGetUniformLocation(program,"uHasPrevPalette"),1);glUniform1i(glGetUniformLocation(program,"uPrevPaletteOffset"),offset);
  std::array<std::array<float,12>,3> result;glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_TRIANGLES);assert(ar::draw(r,program,ar::capture_bindings(),error));glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result.data());assert(glGetError()==GL_NO_ERROR);
  double frame_error=0;for(unsigned n=0;n<3;++n){auto c=point(asset,current,n),p=point(asset,previous,n);float cc[]={c.x,c.y,c.z,1},pp[]={p.x,p.y,p.z,1};for(unsigned k=0;k<4;++k){frame_error=std::max({frame_error,std::abs(double(result[n][k]-cc[k])),std::abs(double(result[n][4+k]-pp[k])),std::abs(double(result[n][8+k]-cc[k]))});maximum_motion=std::max(maximum_motion,std::abs(double(result[n][k]-result[n][4+k])));}}maximum_error=std::max(maximum_error,frame_error);assert(frame_error<.00002);std::printf("frame=%u previous_bank_offset=%d CPU_GPU_error=%.9f current_weight=%.2f previous_weight=%.2f PASS\n",frame,offset,frame_error,current.morph_weights[0],previous.morph_weights[0]);previous=current;
 }
 assert(positive&&negative&&maximum_motion>.2);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);glActiveTexture(GL_TEXTURE18);glBindTexture(GL_TEXTURE_BUFFER,0);glActiveTexture(GL_TEXTURE0);ar::destroy(r);assert(texture_cache_stats().references==0);std::printf("mode1 shared-buffer bank alternation and independent facial previous weights: maximum_error=%.9f motion=%.6f renderer=%s PASS\n",maximum_error,maximum_motion,glGetString(GL_RENDERER));glDeleteProgram(program);SDL_GL_DeleteContext(context);SDL_DestroyWindow(w);SDL_Quit();}

#define GL_GLEXT_PROTOTYPES
#include "engine/world_environment.h"
#include "engine/world_local_probes.h"
#include "engine/world_cubemap_filter.h"
#include "stb_image.h"
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>
namespace {
constexpr double pi=3.14159265358979323846;
struct Vec {double x,y,z;};
Vec normal(Vec a){double n=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return {a.x/n,a.y/n,a.z/n};}
Vec face_direction(unsigned face,double u,double v){const Vec a[]={{1,-v,-u},{-1,-v,u},{u,1,v},{u,-1,-v},{u,-v,1},{-u,-v,-1}};return normal(a[face]);}
std::array<float,3> panorama(const float *pixels,int width,int height,Vec d){
 double x=(std::atan2(d.z,d.x)/(2*pi)+.5)*width-.5,y=std::acos(std::clamp(d.y,-1.,1.))/pi*height-.5;int ix=int(std::floor(x)),iy=int(std::floor(y));float fx=float(x-ix),fy=float(y-iy);std::array<float,3> result={};
 for(int j=0;j<2;j++)for(int i=0;i<2;i++){int px=(ix+i)%width;if(px<0)px+=width;int py=std::clamp(iy+j,0,height-1);float weight=(i?fx:1-fx)*(j?fy:1-fy);for(int c=0;c<3;c++)result[c]+=pixels[(size_t(py)*width+px)*3+c]*weight;}
 return result;
}
struct State {
 GLint draw=0,read=0,viewport[4]={},active=0,program=0,vao=0,unpack=0,align=0,row=0,image=0,px=0,py=0,pz=0,cube=0,texture=0,sampler=0;GLboolean depth=false,blend=false,cull=false,scissor=false,seamless=false,mask=true,color[4]={};
 State(){glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);depth=glIsEnabled(GL_DEPTH_TEST);blend=glIsEnabled(GL_BLEND);cull=glIsEnabled(GL_CULL_FACE);scissor=glIsEnabled(GL_SCISSOR_TEST);seamless=glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glGetBooleanv(GL_COLOR_WRITEMASK,color);
 glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);glGetIntegerv(GL_UNPACK_ALIGNMENT,&align);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&image);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&px);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&py);glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&pz);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);for(GLenum k:{GL_UNPACK_ROW_LENGTH,GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_IMAGES})glPixelStorei(k,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&cube);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegeri_v(GL_SAMPLER_BINDING,0,&sampler);}
 ~State(){glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glUseProgram(program);glBindVertexArray(vao);auto enable=[](GLenum k,GLboolean on){if(on)glEnable(k);else glDisable(k);};enable(GL_DEPTH_TEST,depth);enable(GL_BLEND,blend);enable(GL_CULL_FACE,cull);enable(GL_SCISSOR_TEST,scissor);enable(GL_TEXTURE_CUBE_MAP_SEAMLESS,seamless);glDepthMask(mask);glColorMask(color[0],color[1],color[2],color[3]);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);glPixelStorei(GL_UNPACK_ALIGNMENT,align);glPixelStorei(GL_UNPACK_ROW_LENGTH,row);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,image);glPixelStorei(GL_UNPACK_SKIP_PIXELS,px);glPixelStorei(GL_UNPACK_SKIP_ROWS,py);glPixelStorei(GL_UNPACK_SKIP_IMAGES,pz);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_CUBE_MAP,cube);glBindTexture(GL_TEXTURE_2D,texture);glBindSampler(0,sampler);glActiveTexture(active);}
};
GLuint program(){return world_cubemap_filter::program();}
double radical(unsigned b){b=(b<<16)|(b>>16);b=((b&0x55555555u)<<1)|((b&0xaaaaaaaau)>>1);b=((b&0x33333333u)<<2)|((b&0xccccccccu)>>2);b=((b&0x0f0f0f0fu)<<4)|((b&0xf0f0f0f0u)>>4);b=((b&0x00ff00ffu)<<8)|((b&0xff00ff00u)>>8);return b*2.3283064365386963e-10;}
std::vector<float> brdf_lut(){constexpr unsigned side=128,samples=128;std::vector<float> values(side*side*2);for(unsigned y=0;y<side;y++)for(unsigned x=0;x<side;x++){double nv=(x+.5)/side,rough=(y+.5)/side,a=rough*rough,a2=a*a;Vec V{std::sqrt(1-nv*nv),0,nv};double A=0,B=0;auto g1=[&](double n){return 2*n/(n+std::sqrt(a2+(1-a2)*n*n));};for(unsigned i=0;i<samples;i++){double phi=2*pi*i/samples,ct=std::sqrt((1-radical(i))/(1+(a2-1)*radical(i))),st=std::sqrt(std::max(0.,1-ct*ct));Vec H{std::cos(phi)*st,std::sin(phi)*st,ct};double vh=std::max(0.,V.x*H.x+V.z*H.z),nl=2*vh*H.z-V.z;if(nl>0){double visibility=g1(nv)*g1(nl)*vh/std::max(H.z*nv,1e-9),f=std::pow(1-vh,5);A+=(1-f)*visibility;B+=f*visibility;}}double energy=std::max(double(samples),A+B);values[(y*side+x)*2]=float(A/energy);values[(y*side+x)*2+1]=float(B/energy);}return values;}
struct Locations {unsigned program=0;GLint cube=-1,brdf=-1,sh=-1,weight=-1,rotation=-1,intensity=-1,mip=-1,sky_only=-1;};
}
struct WorldEnvironment {bool sky_only=false;WorldEnvironmentStats stats;std::array<Locations,8> locations={};unsigned next=0;};
WorldEnvironment *world_environment_create(const char *path,std::string &error,unsigned side){
 error.clear();if(!path||!side||side>256||side<32||(side&(side-1))){error="Invalid environment path/cube size";return nullptr;}
 try{if(std::filesystem::file_size(path)>64u*1024u*1024u){error="HDR source exceeds64 MiB";return nullptr;}}catch(const std::exception &e){error=e.what();return nullptr;}
 int width=0,height=0,channels=0;if(!stbi_is_hdr(path)||!stbi_info(path,&width,&height,&channels)||width<2||height<2||width>4096||height>2048||width!=height*2){error="Use a2:1 Radiance HDR panorama, up to4096x2048";return nullptr;}
 std::unique_ptr<float,decltype(&stbi_image_free)> pixels(stbi_loadf(path,&width,&height,&channels,3),stbi_image_free);if(!pixels){error="Cannot decode HDR panorama";return nullptr;}
 for(size_t i=0;i<size_t(width)*height*3;i++)if(!std::isfinite(pixels.get()[i])||pixels.get()[i]<0||pixels.get()[i]>60000){error="HDR radiance is non-finite or exceeds60000";return nullptr;}
 try {
 std::unique_ptr<WorldEnvironment,decltype(&world_environment_free)> e(new WorldEnvironment,world_environment_free);State state;e->sky_only=std::filesystem::canonical(path).filename().string().find("puresky")!=std::string::npos;e->stats.side=side;for(unsigned n=side;n;n/=2)e->stats.levels++;
 glGenTextures(1,&e->stats.raw);glBindTexture(GL_TEXTURE_CUBE_MAP,e->stats.raw);std::vector<float> face(size_t(side)*side*3);double sh[9][3]={};
 for(unsigned f=0;f<6;f++){for(unsigned y=0;y<side;y++)for(unsigned x=0;x<side;x++){auto value=panorama(pixels.get(),width,height,face_direction(f,2.*(x+.5)/side-1,2.*(y+.5)/side-1));std::copy(value.begin(),value.end(),face.begin()+(size_t(y)*side+x)*3);}glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,0,GL_RGB16F,side,side,0,GL_RGB,GL_FLOAT,face.data());
  std::vector<float> diffuse(64*64*3);for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){auto value=panorama(pixels.get(),width,height,face_direction(f,2.*(x+.5)/64-1,2.*(y+.5)/64-1));std::copy(value.begin(),value.end(),diffuse.begin()+(y*64+x)*3);}if(!world_local_probe_add_face(f,64,diffuse.data(),diffuse.size(),sh)){error="Cannot integrate HDR irradiance";world_environment_free(e.release());return nullptr;}}
 pixels.reset();for(unsigned i=0;i<9;i++)for(unsigned c=0;c<3;c++)e->stats.sh[i][c]=float(sh[i][c]);
 auto parameters=[](){glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);for(GLenum a:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})glTexParameteri(GL_TEXTURE_CUBE_MAP,a,GL_CLAMP_TO_EDGE);};parameters();glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
 glGenTextures(1,&e->stats.filtered);glBindTexture(GL_TEXTURE_CUBE_MAP,e->stats.filtered);for(unsigned level=0,n=side;n;n/=2,level++)for(unsigned f=0;f<6;f++)glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,level,GL_RGB16F,n,n,0,GL_RGB,GL_FLOAT,nullptr);parameters();
 GLuint filter=program(),fbo=0,vao=0;bool ok=filter!=0;glGenFramebuffers(1,&fbo);glGenVertexArrays(1,&vao);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glBindVertexArray(vao);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glUseProgram(filter);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_CUBE_MAP,e->stats.raw);glBindSampler(0,0);
 if(!filter){glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);error="Cannot compile HDR filter";return nullptr;}
 const GLint source=glGetUniformLocation(filter,"uSource"),side_location=glGetUniformLocation(filter,"uSide"),face_location=glGetUniformLocation(filter,"uFace"),roughness=glGetUniformLocation(filter,"uRough"),source_side=glGetUniformLocation(filter,"uSourceSide");glUniform1i(source,0);glUniform1f(source_side,float(side));
 for(unsigned level=0,n=side;ok&&n;n/=2,level++){glViewport(0,0,n,n);glUniform1i(side_location,int(n));glUniform1f(roughness,float(level)/float(e->stats.levels-1));for(unsigned f=0;f<6;f++){glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,e->stats.filtered,level);glDrawBuffer(GL_COLOR_ATTACHMENT0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){ok=false;break;}glUniform1i(face_location,int(f));glDrawArrays(GL_TRIANGLES,0,3);}}
 glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);if(filter)glDeleteProgram(filter);
 auto lut=brdf_lut();glGenTextures(1,&e->stats.brdf);glBindTexture(GL_TEXTURE_2D,e->stats.brdf);glTexImage2D(GL_TEXTURE_2D,0,GL_RG16F,128,128,0,GL_RG,GL_FLOAT,lut.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 for(unsigned n=side;n;n/=2)e->stats.bytes+=std::uint64_t(n)*n*6*6*2;
 e->stats.bytes+=128*128*4;
 if(!ok||glGetError()!=GL_NO_ERROR){error="HDR cube/BRDF preparation failed";world_environment_free(e.release());return nullptr;}return e.release();
 }catch(const std::exception &exception){error=exception.what();return nullptr;}
}
WorldEnvironmentStats world_environment_stats(const WorldEnvironment *e){return e?e->stats:WorldEnvironmentStats{};}
void world_environment_bind(WorldEnvironment *e,unsigned program,float rotation,float intensity,float strength){
 // Uniform lookup for a disabled environment is handled by the caller's
 // program cache/default initializers; enabled programs retain fixed locations.
 if(!e||!program)return;
 Locations *locations=nullptr;for(auto &v:e->locations)if(v.program==program){locations=&v;break;}if(!locations){locations=&e->locations[e->next++%e->locations.size()];*locations={};locations->program=program;locations->cube=glGetUniformLocation(program,"uEnvironmentCube");locations->brdf=glGetUniformLocation(program,"uEnvironmentBRDF");locations->sh=glGetUniformLocation(program,"uEnvironmentSH[0]");locations->weight=glGetUniformLocation(program,"uEnvironmentStrength");locations->rotation=glGetUniformLocation(program,"uEnvironmentRotation");locations->intensity=glGetUniformLocation(program,"uEnvironmentIntensity");locations->mip=glGetUniformLocation(program,"uEnvironmentMip");locations->sky_only=glGetUniformLocation(program,"uEnvironmentSkyOnly");}
 const auto &u=*locations;const float angle=rotation*.01745329252f;glUniform1f(u.weight,strength);glUniform2f(u.rotation,std::cos(angle),std::sin(angle));glUniform1f(u.intensity,intensity);glUniform1f(u.mip,float(e->stats.levels-1));glUniform1i(u.sky_only,e->sky_only?1:0);glUniform3fv(u.sh,9,e->stats.sh[0]);
 if(u.cube>=0){glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);glActiveTexture(GL_TEXTURE15);glBindSampler(15,0);glBindTexture(GL_TEXTURE_CUBE_MAP,e->stats.filtered);glUniform1i(u.cube,15);}if(u.brdf>=0){glActiveTexture(GL_TEXTURE6);glBindSampler(6,0);glBindTexture(GL_TEXTURE_2D,e->stats.brdf);glUniform1i(u.brdf,6);}glActiveTexture(GL_TEXTURE0);
}
void world_environment_free(WorldEnvironment *e){if(!e)return;glDeleteTextures(1,&e->stats.raw);glDeleteTextures(1,&e->stats.filtered);glDeleteTextures(1,&e->stats.brdf);delete e;}

void world_environment_invalidate_programs(WorldEnvironment *e){if(e){e->locations={};e->next=0;}}

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
static GLuint shader(GLenum type,const std::string&s){GLuint p=glCreateShader(type);const char*t=s.c_str();glShaderSource(p,1,&t,nullptr);glCompileShader(p);GLint ok;glGetShaderiv(p,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(p,sizeof log,nullptr,log);std::puts(log);}assert(ok);return p;}
static double smooth(double lo,double hi,double x){double t=std::clamp((x-lo)/(hi-lo),0.,1.);return t*t*(3-2*t);}
int main(int argc,char**argv){
 assert(argc==2);if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
 assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 auto*w=SDL_CreateWindow("Canopy field parity",0,0,8,8,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);auto context=SDL_GL_CreateContext(w);assert(context);
 GLuint vao,fbo,color,field;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
 glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,8,8,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
 std::vector<unsigned char> pixels(8*8*6*4,255);for(unsigned i=8*8*5*4;i<pixels.size();i+=4){pixels[i]=128;pixels[i+1]=40;pixels[i+2]=80;}
 glGenTextures(1,&field);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,field);glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,8,8,6,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glViewport(0,0,8,8);
 unsigned checked=0;
 for(const char*name:{"world_terrain.fs.glsl","world_foliage.fs.glsl"}){
  std::ifstream file(std::string(argv[1])+"/shaders/"+name);assert(file);std::stringstream stream;stream<<file.rdbuf();auto text=stream.str();auto start=text.find("float forest_sky_access(");assert(start!=std::string::npos);auto end=text.find("\n}",start);assert(end!=std::string::npos);auto body=text.substr(start,end+2-start);
  // Compile the full production fragments with both optional gather paths.
  // The application normally compiles gather out, unlike a default fixture.
  for(bool scalar:{false,true}){auto full=text;if(scalar)full.insert(full.find('\n')+1,"#define VALWOOD_QA_DISABLE_TEXTURE_GATHER 1\n");auto compiled=shader(GL_FRAGMENT_SHADER,full);glDeleteShader(compiled);}
  const char*sampler=std::string(name).find("terrain")!=std::string::npos?"uLandscape":"uVillageSkyMask";
  auto vs=shader(GL_VERTEX_SHADER,"#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}");
  auto fs=shader(GL_FRAGMENT_SHADER,std::string("#version 330 core\nuniform bool uForestShelterEnabled,uForestGroundReady;float forest_outcrop=0.;uniform vec4 uForestShelterBounds;uniform vec2 uForestShelterHeights;uniform sampler2DArray ")+sampler+";uniform vec3 tWorld;out vec4 color;\n"+body+"\nvoid main(){float access=forest_sky_access(tWorld);color=vec4(access,forest_outcrop,0,1);}");
  GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);glUseProgram(p);
  glUniform1i(glGetUniformLocation(p,sampler),0);glUniform4f(glGetUniformLocation(p,"uForestShelterBounds"),1000,900,.01f,.01f);glUniform2f(glGetUniformLocation(p,"uForestShelterHeights"),112,96);
  for(int enabled:{0,1})for(int ready:{0,1})for(float x:{999.f,1000.f,1003.f,1006.f,1050.f,1097.f,1100.f,1101.f})for(float y:{120.f,130.f,134.f,142.f,160.f}){
   glUniform1i(glGetUniformLocation(p,"uForestShelterEnabled"),enabled);glUniform1i(glGetUniformLocation(p,"uForestGroundReady"),ready);glUniform3f(glGetUniformLocation(p,"tWorld"),x,y,950);glDrawArrays(GL_TRIANGLES,0,3);float result[4];glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,result);
   double expected=1;if(enabled&&x>=1000&&x<=1100){const double ground=112+40./255*96,crown=112+80./255*96;const double recovery=smooth(ground+1.5,std::max(ground+3,crown),y),domain=smooth(0,6,std::min(x-1000.f,1100.f-x));expected=1+((128./255+(1-128./255)*recovery)-1)*domain;}
   assert(std::isfinite(result[0])&&std::abs(result[0]-expected)<.00001);
   const bool terrain=std::string(name).find("terrain")!=std::string::npos;
   const double exposure=terrain&&ready&&enabled&&x>=1000&&x<=1100?smooth(0,6,std::min(x-1000.f,1100.f-x)):0;
   assert(std::abs(result[1]-exposure)<.00001);checked++;
  }
  glDeleteProgram(p);glDeleteShader(vs);glDeleteShader(fs);
  if(std::string(name)=="world_foliage.fs.glsl"){
   // Exercise the actual main() composition statements, not just the field
   // helper: directional village lighting previously replaced forest shade
   // on opaque rocks and characters. Leaves/cards use the scalar sky path.
   const auto a=text.find("    vec3 unoccludedSky="),b=text.find("    float density=",a);
   const auto c=text.find("    if(uVillageDirectionalEnabled&&uVegetation==0&&!uGrass)"),d=text.find("    vec3 V=",c);
   assert(a!=std::string::npos&&b!=std::string::npos&&c!=std::string::npos&&d!=std::string::npos);
   auto v=shader(GL_VERTEX_SHADER,"#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}");
   auto f=shader(GL_FRAGMENT_SHADER,std::string("#version 330 core\nuniform bool uForestShelterEnabled,uVillageDirectionalEnabled,uGrass;uniform int uVegetation;uniform vec4 uForestShelterBounds;uniform vec2 uForestShelterHeights;uniform sampler2DArray uVillageSkyMask;uniform vec3 tWorld;out vec4 color;\n")+body+
    "\nvec3 village_directional_sky(vec3 N){return vec3(.3,.4,.5);}\nvoid main(){vec3 skyLight=vec3(.2,.4,.8),vPos=tWorld,geometricN=vec3(0,1,0);vec2 uWorldAnchor=vec2(0),villageSky=vec2(.6,0);\n"+text.substr(a,b-a)+text.substr(c,d-c)+"color=vec4(skyLight,1);}");
   GLuint q=glCreateProgram();glAttachShader(q,v);glAttachShader(q,f);glLinkProgram(q);glGetProgramiv(q,GL_LINK_STATUS,&ok);assert(ok);glUseProgram(q);
   glUniform1i(glGetUniformLocation(q,"uVillageSkyMask"),0);glUniform4f(glGetUniformLocation(q,"uForestShelterBounds"),1000,900,.01f,.01f);glUniform2f(glGetUniformLocation(q,"uForestShelterHeights"),112,96);
   for(int enabled:{0,1})for(int directional:{0,1})for(int material:{0,1,4})for(int grass:{0,1})for(float x:{999.f,1003.f,1050.f}){
    glUniform1i(glGetUniformLocation(q,"uForestShelterEnabled"),enabled);glUniform1i(glGetUniformLocation(q,"uVillageDirectionalEnabled"),directional);glUniform1i(glGetUniformLocation(q,"uVegetation"),material);glUniform1i(glGetUniformLocation(q,"uGrass"),grass);glUniform3f(glGetUniformLocation(q,"tWorld"),x,120,950);
    glDrawArrays(GL_TRIANGLES,0,3);float result[4];glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,result);
    double shelter=1;if(enabled&&x>=1000)shelter=1+(128./255-1)*smooth(0,6,std::min(x-1000.f,1100.f-x));
    for(unsigned channel=0;channel<3;channel++){
     const double sky=channel==0?.2:channel==1?.4:.8;
     const double village=directional&&material==0&&!grass?.3+channel*.1:.6;
     assert(std::abs(result[channel]-sky*village*shelter)<.00001);
    }
    checked++;
   }
   glDeleteProgram(q);glDeleteShader(v);glDeleteShader(f);
  }
 }
 assert(glGetError()==GL_NO_ERROR);glDeleteTextures(1,&field);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);SDL_GL_DeleteContext(context);SDL_DestroyWindow(w);SDL_Quit();std::printf("PASS actual production canopy GLSL: %u terrain/foliage cases; correct shared layer, exterior identity,6m edge feather,ground/crown recovery and disabled identity\n",checked);
}

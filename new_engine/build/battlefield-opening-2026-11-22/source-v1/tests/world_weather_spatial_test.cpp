// CPU-only: actual production uniform packet and support classifiers. GL entry
// points are recording stubs; no SDL init, context, window or renderer is used.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <array>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cmath>
std::map<int,std::vector<float>> uniform_values;
unsigned program_switches=0;
extern "C" void APIENTRY glUniform1i(GLint p,GLint v){uniform_values[p]={float(v)};}
extern "C" void APIENTRY glUniform4f(GLint p,GLfloat a,GLfloat b,GLfloat c,GLfloat d){uniform_values[p]={a,b,c,d};}
extern "C" void APIENTRY glUniform4fv(GLint p,GLsizei n,const GLfloat*v){uniform_values[p]=std::vector<float>(v,v+4*n);}
extern "C" void APIENTRY glUseProgram(GLuint){++program_switches;}
#include "engine/world_weather.cpp"
float world_atlas_height(double,double){return 0;}
unsigned checks=0;
void require(bool ok,const char*label){++checks;if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 using namespace world_weather_authoring;
 const WorldWeatherAuthoredUniforms u{1,2,3,4,5};PackedFrame empty;
 require(world_weather_bind_authored(u,empty)&&uniform_values[1][0]==0&&uniform_values[2][0]==0,"zero authored packet retains legacy branch");
 Global global;global.enabled=true;global.profile.rain={.8f,.8f};global.profile.cloud={.9f,.9f};global.profile.fog={.7f,.7f};
 std::array<BoundArea,16> areas;for(unsigned i=0;i<areas.size();++i){areas[i].id=i+10;areas[i].area.x=double(i)*100;areas[i].area.z=double(i)*17;areas[i].area.inner_radius=5;areas[i].area.radius=20;areas[i].area.priority=int(16-i);areas[i].area.profile.rain={float(i)/16,float(i)/16};areas[i].area.profile.fog={.3f,.3f};}
 Sample legacy;legacy.rain=.1f;legacy.cloud=.2f;legacy.fog=.1f;PackedFrame packed;
 require(pack(global,areas.data(),areas.size(),legacy,12,packed),"maximum sixteen-area packet packs");
 require(world_weather_bind_authored(u,packed)&&uniform_values[1][0]==17&&uniform_values[2][0]==16,"maximum packet counts include global baseline");
 require(uniform_values[3]==std::vector<float>({.8f,.9f,.7f,1}),"global sentinel and channels upload exactly");
 require(uniform_values[4].size()==64&&uniform_values[5].size()==64,"array uploads are tightly packed rather than struct-strided");
 for(unsigned i=0;i<16;++i)for(unsigned j=0;j<4;++j){require(uniform_values[4][4*i+j]==packed.areas[i].bounds[j],"every bound row matches stable sorted packet");require(uniform_values[5][4*i+j]==(j==3?packed.areas[i].effects[3]:packed.areas[i].channels[j]),"GPU channel W carries transient influence, not seconds");}
 require(program_switches==0,"uniform binder does not switch the current program");
 auto invalid=packed;invalid.count=17;require(!world_weather_bind_authored(u,invalid)&&uniform_values[1][0]==0,"oversized packet fails closed");
 invalid=packed;invalid.authored_count=3;require(!world_weather_bind_authored(u,invalid),"inconsistent authored count rejected");
 invalid=packed;invalid.areas[0].effects[3]=1.2f;require(!world_weather_bind_authored(u,invalid),"invalid influence rejected");
 invalid=packed;invalid.areas[0].channels[0]=NAN;require(!world_weather_bind_authored(u,invalid),"nonfinite channels rejected");
 WorldWeatherFrame frame;require(!has_rain(frame)&&!has_fog(frame),"legacy dry frame has no weather work");frame.intensity=.4f;require(has_rain(frame),"legacy rain classifier unchanged");frame.fog_strength=.2f;require(has_fog(frame),"legacy fog classifier unchanged");
 Global inherited;areas[0].area.x=2000;areas[0].area.z=0;areas[0].area.profile.rain={1,1};areas[0].area.profile.fog={1,1};require(pack(inherited,areas.data(),1,legacy,0,frame.authored),"inherited global and local area pack");frame.intensity=frame.fog_strength=0;
 require(!has_rain(frame)&&!has_fog(frame),"remote local areas do not activate existing render targets");frame.camera[0]=1950;require(has_rain(frame)&&has_fog(frame),"rain forty-metre support sees an approaching area");frame.fog_enabled=false;require(!has_fog(frame),"authored mist obeys graphics fog setting");
 frame.authored.areas[0].effects[3]=0;require(!has_rain(frame),"zero-influence area cannot activate rain");
 WorldWeatherFrame clear;clear.intensity=.48f;clear.fog_strength=.62f;Global clear_global;clear_global.enabled=true;clear_global.profile.rain_enabled=false;clear_global.profile.fog={0,0};require(pack(clear_global,nullptr,0,legacy,0,clear.authored),"authored clear packet packs");require(!world_weather_active(clear),"authored Clear legitimately suppresses legacy rain and fog");clear.authored={};require(world_weather_active(clear),"public eligibility preserves legacy positive rain/fog");
 PackedArea edge;edge.bounds={0,0,5,5};require(area_weight(edge,4.99,0)==1&&area_weight(edge,5,0)==0,"equal inner/outer is a finite hard boundary");
 world_weather_wetness::Packet wet;wet.enabled=true;wet.count=32;wet.global={.7f,.4f};for(unsigned i=0;i<32;++i){wet.layers[i].bounds={float(i),float(i*2),2,9};wet.layers[i].channels={float(i)/32,.2f,0,1};}
 const WorldWeatherWetnessUniforms wu{11,12,13,14,15};require(world_weather_bind_wetness(wu,wet)&&uniform_values[11][0]==1&&uniform_values[12][0]==32,"maximum wet packet actual binder");require(uniform_values[14].size()==128&&uniform_values[15].size()==128,"wet arrays tightly packed");for(unsigned i=0;i<32;++i)for(unsigned k=0;k<4;++k){require(uniform_values[14][i*4+k]==wet.layers[i].bounds[k],"wet bounds exact");require(uniform_values[15][i*4+k]==wet.layers[i].channels[k],"wet channels exact");}auto invalid_wet=wet;invalid_wet.count=33;require(!world_weather_bind_wetness(wu,invalid_wet)&&uniform_values[11][0]==0,"invalid wet packet disabled");require(program_switches==0,"wet binder keeps current program");
 WorldWeather cached;WorldWeatherFrame cache_frame;cache_frame.intensity=1;cache_frame.surface_height=[](double,double,void*){return 2.f;};cache_frame.cover_height=[](double,double,void*){return 2.f;};cache_frame.contact_surface=[](double x,double z,WorldRainContact&c,void*){c.material=x<0?WorldRainMaterial::Water:z<0?WorldRainMaterial::Stone:WorldRainMaterial::Mud;c.foliage_height=x>0&&z>0?5.f:-10000.f;return true;};
 for(unsigned i=0;i<9;++i){auto before=cached.stats.contact_queries;world_weather_update(&cached,cache_frame);require(cached.stats.contact_queries-before<=520,"material query budget bounded520");}
 require(cached.ready&&cached.stats.contact_queries==65*65&&cached.stats.height_queries==65*65,"complete actual CPU cache generation");for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x){unsigned offset=(z*65+x)*4;float wx=-96+3*float(x),wz=-96+3*float(z);require(cached.heights[offset+2]==float(wx<0?1:wz<0?2:3),"categorical material channels preserved");require(cached.heights[offset+3]==(wx>0&&wz>0?5.f:-10000.f),"only callback true foliage support used");}
 std::array<WorldWeatherDrip,64>drips{};cache_frame.drips=drips.data();cache_frame.drip_count=64;require(camera_valid(cache_frame),"64 geometry drip anchors valid");cache_frame.drip_count=65;require(!camera_valid(cache_frame),"oversized drip array rejected");cache_frame.drip_count=64;drips[0].exposure=NAN;require(!camera_valid(cache_frame),"nonfinite drip exposure rejected");
 if(argc==2){const std::string root=argv[1];for(const char*name:{"world_weather_particles.vs.glsl","world_weather_fog.fs.glsl","world_weather_compose.fs.glsl","world_terrain.fs.glsl","world_foliage.fs.glsl","world_water.fs.glsl"}){const std::string path=root+"/"+name;char*expanded=glsl_load(path.c_str());require(expanded!=nullptr,"actual production shader include loader reads source");std::string shader_source(expanded);std::free(expanded);require(shader_source.find("#pragma include")==std::string::npos,"shared include fully expands without a GPU");if(std::string(name)!="world_weather_compose.fs.glsl")require(shader_source.find("vec3 authoredWeather(")!=std::string::npos,"each spatial receiver gets the exact shared sampler");}}
 std::cout<<"PASS "<<checks<<" actual weather uniform/support/include checks; no GPU\n";
}
